#include "pch.h"
#include "C4IMapViewHeader.h"
#include "MapDecoration.h"
#include "BaseMapOptions.h"
#include <algorithm>
#include <cmath>

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
