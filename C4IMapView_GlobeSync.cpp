#include "pch.h"
#include "C4IMapViewHeader.h"
#include "MapDecoration.h"
#include "BaseMapOptions.h"
#include <algorithm>
#include <cmath>
#pragma pack(push,8)
#include "cpl_json.h"
#include "AppPaths.h"

bool CC4IMapView::CaptureGlobeSyncState(MapViewSyncState& state)
{
    if (!m_bDCInitialized || !m_pCoordMgr) return false;
    CRect client; GetClientRect(&client);
    if (client.Width() < 2 || client.Height() < 2) return false;
    const auto center = client.CenterPoint();
    double lon2, lat2;
    if (!MapScreenLonLat(m_pCoordMgr,center.x,center.y,state.longitude,state.latitude) ||
        !MapScreenLonLat(m_pCoordMgr,center.x,center.y+1,lon2,lat2)) return false;
    state.longitude = std::remainder(state.longitude,360.0);
    state.metresPerPixel = MapDistanceMetres(state.longitude,state.latitude,lon2,lat2);
    state.configuredLayers = state.visibleLayers = 0;
    for (int i=0;i<3;++i)
    {
        if (BaseMapConfigured(i)) state.configuredLayers |= 1u<<i;
        if (BaseMapVisible(i)) state.visibleLayers |= 1u<<i;
    }
    return std::isfinite(state.metresPerPixel) && state.metresPerPixel>0;
}
void CC4IMapView::OnGlobeSyncStart()
{
    m_globeScene.ForcePublish();
    PublishGlobeScene();
    MapViewSyncState state;
    if (!CaptureGlobeSyncState(state) || !m_globeSync.Start(state))
        AfxMessageBox(_T("Globe3D 동기화를 시작하지 못했습니다. 지도 준비 후 다시 시도하세요."));
}
void CC4IMapView::OnGlobeSyncStop() { m_globeSync.Stop(); }
void CC4IMapView::OnUpdateGlobeSync(CCmdUI* ui)
{
    const bool enabled=m_globeSync.IsEnabled();
    ui->Enable(m_bDCInitialized && (ui->m_nID==ID_GLOBE_SYNC_START ? !enabled : enabled));
    if(ui->m_nID==ID_GLOBE_SYNC_START) ui->SetCheck(enabled);
}
void CC4IMapView::SyncGlobeView()
{
    if(m_globeSync.IsEnabled() && GetTickCount64()-m_globeSceneTick>=500){PublishGlobeScene();m_globeSceneTick=GetTickCount64();}
    MapViewSyncState current,incoming;
    if(!CaptureGlobeSyncState(current) || !m_globeSync.Tick(current,incoming)) return;
    CRect client;GetClientRect(&client);const auto center=client.CenterPoint();
    const double oldX=m_pCoordMgr->m_fCenterX,oldY=m_pCoordMgr->m_fCenterY,oldScale=m_pCoordMgr->m_fScale;
    double x,y;
    m_pCoordMgr->WorldLonLat2ScreenXY(incoming.longitude,incoming.latitude,&x,&y,
        oldX,oldY,oldScale);
    if(std::isfinite(x)&&std::isfinite(y))
    {
        m_pCoordMgr->m_fCenterX+=center.x-x;
        m_pCoordMgr->m_fCenterY+=center.y-y;
        for(int i=0;i<4;++i)
        {
            MapViewSyncState measured;
            if(!CaptureGlobeSyncState(measured)) break;
            const double before=m_pCoordMgr->m_fScale;
            const double after=(std::max)(1e-6,(std::min)(2500.0,before*measured.metresPerPixel/incoming.metresPerPixel));
            const double ratio=after/before;
            m_pCoordMgr->m_fCenterX=center.x-(center.x-m_pCoordMgr->m_fCenterX)*ratio;
            m_pCoordMgr->m_fCenterY=center.y-(center.y-m_pCoordMgr->m_fCenterY)*ratio;
            m_pCoordMgr->m_fScale=after;
            if(fabs(ratio-1)<1e-6)break;
        }
        // Keep the user-selected scale limits without echoing a clamped remote view.
        SetNavigationScale(NavigationDenominator());
    }
    bool changed=false;
    for(int i=0;i<3;++i)
        if((incoming.configuredLayers&(1u<<i))&&BaseMapConfigured(i))
        {
            const bool visible=(incoming.visibleLayers&(1u<<i))!=0;
            if(BaseMapVisible(i)!=visible){SetBaseMapVisible(i,visible);changed=true;}
        }
    if(changed)get_mainfrm()->m_wndMapObjectManager.RefreshLocalShapeTree();
    m_mapFrameCache.InvalidateFrom();
    if(CaptureGlobeSyncState(current))m_globeSync.Applied(current);
    Invalidate(FALSE);
}

void CC4IMapView::PublishGlobeScene()
{
    if(!m_bDCInitialized || !m_pCoordMgr)return;
    CPLJSONObject root;
    root.Add("version",1);root.Add("owner",(int)GetCurrentProcessId());
    root.Add("autoScale",MapAutoScaleEnabled());root.Add("buildMode",C4MAP_BUILD_MODE);
    root.Add("baseMapXml",BaseMapConfig().file);
    CPLJSONArray sources;
    for(const auto& source:MapSources().Layers())
    {
        CPLJSONObject entry;
        entry.Add("identifyType",source.identifyType);
        entry.Add("identifyEndpoint",source.identifyEndpoint);
        entry.Add("identifyLayer",source.identifyLayer);
        entry.Add("category",source.category);
        entry.Add("tileScheme",source.tileScheme);
        entry.Add("styleSld",source.styleSld);
        entry.Add("localStyleXml",source.localStyleXml);
        entry.Add("localLayerName",source.localLayerName);
        entry.Add("id",source.id);
        entry.Add("name",source.name);
        entry.Add("type",source.type);
        entry.Add("family",source.family);
        entry.Add("path",source.path);
        entry.Add("extensions",source.extensions);
        entry.Add("endpoint",source.endpoint);
        entry.Add("layer",source.layer);
        entry.Add("matrix",source.matrix);
        entry.Add("style",source.style);
        entry.Add("format",source.format);
        entry.Add("version",source.version);
        entry.Add("srs",source.srs);
        entry.Add("scale",source.scale);
        entry.Add("minScale",source.minScale);
        entry.Add("maxScale",source.maxScale);
        entry.Add("recursive",source.recursive);
        entry.Add("visual",source.visual && CMapSources::Enabled(source));
        sources.Add(entry);
    }
    root.Add("sources",sources);
    CPLJSONArray bases;
    const char* names[]={"SmallScale","World","NaturalEarth"};
    for(int i=0;i<3;++i)
    {
        if(!BaseMapConfigured(i))continue;
        CPLJSONObject layer;layer.Add("id",names[i]);layer.Add("visual",BaseMapVisible(i));
        for(const char* field:{"Path","BoundaryPath","LabelPath","CityPath"})
        {
            std::string key=std::string(names[i])+"."+field;
            CStringW value(BaseMapPath(key.c_str(),""));
            layer.Add(field,std::string(CW2A(value,CP_UTF8)));
        }
        bases.Add(layer);
    }
    root.Add("bases",bases);
    CPLJSONArray layers;
    for(const auto& layer:m_localOverlay.Layers())
    {
        CPLJSONObject entry;entry.Add("id",layer.id);entry.Add("visible",layer.visible);
        CStringW name(layer.name);entry.Add("name",std::string(CW2A(name,CP_UTF8)));layers.Add(entry);
    }
    root.Add("overlayLayers",layers);
    CPLJSONDocument shapes,symbols;
    if(!shapes.LoadMemory(m_localOverlay.SyncObjects(m_pCoordMgr)) ||
       !symbols.LoadMemory(m_militaryOverlay.SyncObjects(m_pCoordMgr)))return;
    root.Add("overlays",shapes.GetRoot());root.Add("military",symbols.GetRoot());
    // Let the receiving renderer resolve the same centrally configured paths.
    root.Add("gssRoot",AppPath("GssRoot"));
    std::string error;
    if(!m_globeScene.Publish(root.Format(CPLJSONObject::PrettyFormat::Plain),error))
        OutputDebugStringA(("Globe scene: "+error+"\n").c_str());
}

#pragma pack(pop)
