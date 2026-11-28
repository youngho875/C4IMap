#include "pch.h"
#include "ModelessDialog.h"
#include <memory>
#include "C4IMapViewHeader.h"
#include "MapRibbonTools.h"
#include <set>
#include <algorithm>
#include <cmath>

void CC4IMapView::RefreshMapSearchControls()
{
    auto frame=get_mainfrm();
    if(!frame||!m_bDCInitialized)
        return;
    std::vector<MapSearchFeature> snapshot;
    if(C4MAP_SHAPE_MODE!=2)
        frame->m_indexMap.SearchSnapshot(m_pCoordMgr,snapshot,true);
    MapSources().SearchSnapshot(m_pCoordMgr,snapshot,true);
    CString selected=frame->m_pRibbonSearchAreaTarget->GetCurSel()>0?frame->m_pRibbonSearchAreaTarget->GetEditText():CString();
    std::set<CString> layers;
    for(const auto& feature:snapshot)
        layers.insert(feature.layer);
    std::vector<CString> names(layers.begin(),layers.end());
    auto target=frame->m_pRibbonSearchAreaTarget;
    if(names!=m_searchLayerNames)
    {
        target->RemoveAllItems();
        target->AddItem(_T("현재 화면 전체"));
        for(const auto& name:names)target->AddItem(name);
        if(selected.IsEmpty()||!target->SelectItem(selected))
            target->SelectItem(0);
        m_searchLayerNames=names;
    }
    selected=target->GetCurSel()>0?target->GetEditText():CString();
    std::set<CString> fields;
    for(const auto& feature:snapshot)
        if(selected.IsEmpty()||feature.layer==selected)
            for(const auto& field:feature.fields)
                fields.insert(field);

    std::vector<CString> fieldNames(fields.begin(),fields.end());
    auto field=frame->m_pRibbonSearchMapField;
    if(fieldNames!=m_searchFieldNames||field->GetCount()==0)
    {
        CString previous=field->GetCurSel()>0?field->GetEditText():CString();
        field->RemoveAllItems();
        field->AddItem(_T("모든 속성"));
        for(const auto& name:fieldNames)
            field->AddItem(name);
        if(previous.IsEmpty()||!field->SelectItem(previous))
            field->SelectItem(0);
        m_searchFieldNames=fieldNames;
    }
    if(target->GetCount()==0)
    {
        target->AddItem(_T("현재 화면 전체"));
        target->SelectItem(0);
    }
}

void CC4IMapView::StartMapSearch(bool spatial,CRect area,bool click)
{
    auto frame=get_mainfrm();
    std::vector<MapSearchFeature> features;
    if(C4MAP_SHAPE_MODE!=2)
        frame->m_indexMap.SearchSnapshot(m_pCoordMgr,features);
    MapSources().SearchSnapshot(m_pCoordMgr,features);
    MapSearchQuery query;
    if(frame->m_pRibbonSearchAreaTarget->GetCurSel()>0)
        query.layer=frame->m_pRibbonSearchAreaTarget->GetEditText();
    if(frame->m_pRibbonSearchMapField->GetCurSel()>0)
        query.field=frame->m_pRibbonSearchMapField->GetEditText();
    query.text=frame->m_pRibbonSearchMapInput->GetEditText();
    query.text.Trim();
    query.spatial=spatial;
    query.area=area;
    query.ellipse=!click&&frame->m_pRibbonSearchAreaType->GetCurSel()==1;
    if(click)
        query.text.Empty();
    if(click&&query.layer.IsEmpty())
    {
        bool found=false;
        for(const auto& f:features)
            if(CMapSearchJob::Intersects(f,query))
            {
                found=true;
                break;
            }
        if(!found)
        {
            ShowLocalFeature(area.CenterPoint());
            return;
        }
    }
    MapSearchReply status;
    status.message=features.empty()?_T("현재 화면에 검색 가능한 벡터 객체가 없습니다. 지도가 로딩된 뒤 다시 검색하세요. WMS/WMTS 영상은 ‘클릭’으로 서버 속성을 조회하세요."):
        _T("현재 화면의 벡터 객체 검색 중... (최대 입력 20,000개 / 결과 5,000건)");
    frame->m_wndOutput.ShowMapSearch(status);
    m_searchResults.clear();
    Invalidate(FALSE);

    if(features.empty())
    {
        m_searchJob.Cancel();
        return;
    }
    m_searchJob.Start(std::move(features),query);
}

void CC4IMapView::OnMapRibbonCommand(UINT id)
{
    auto frame=get_mainfrm();
    if(!frame)
        return;
    switch(id)
    {
    case ID_MENU_BRIGHT_SLIDER:
        m_mapBrightness=frame->m_pRibbonMapBrightSlider->GetPos();
        break;
    case ID_MENU_CONTRAST_SLIDER:
        m_mapContrast=frame->m_pRibbonMapContrastSlider->GetPos();
        break;
    case ID_MENU_BRIGHT_EDIT:
    case ID_MENU_CONTRAST_EDIT:
    {
        CString value=id==ID_MENU_BRIGHT_EDIT?frame->m_pRibbonMapBrightEdit->GetEditText():frame->m_pRibbonMapContrastEdit->GetEditText();
        value.Trim();
        if(value.Right(1)==_T("%"))
            value.Delete(value.GetLength()-1);
        value.Trim();
        TCHAR* end=nullptr;
        long number=_tcstol(value,&end,10);
        if(value.IsEmpty()||!end||*end||number<-100||number>100)
        {
            AfxMessageBox(_T("-100부터 100까지 입력하세요."));
            return;
        }
        if(id==ID_MENU_BRIGHT_EDIT)
            m_mapBrightness=(int)number;
        else 
            m_mapContrast=(int)number;
        break;
    }
    case ID_MENU_BRICON_INIT:
        m_mapBrightness=m_mapContrast=0;
        break;
    case ID_MENU_SAVE_CLIPBOARD:
    case ID_MENU_SAVE_IMAGEFILE:
        CaptureMapImage(id==ID_MENU_SAVE_CLIPBOARD);
        return;
    case ID_MENU_SEARCH_CLEAR:
    {
        m_searchJob.Cancel();
        MapSources().CancelIdentify();
        m_searchMode=0;m_searchDragging=false;
        m_searchResults.clear();
        if(GetCapture()==this)
            ReleaseCapture();
        frame->m_pRibbonSearchMapInput->SetEditText(_T(""));
        MapSearchReply reply;
        reply.message=_T("검색 결과를 초기화했습니다.");
        frame->m_wndOutput.ShowMapSearch(reply);
        RefreshMapSearchControls();
        Invalidate(FALSE);
        return;
    }
    case ID_MENU_SEARCH_AREA_TARGET:
        RefreshMapSearchControls();
        return;
    case ID_MENU_SEARCH_AREA_COLOR:
        m_searchColor=frame->m_pRibbonSearchAreaColor->GetColor();
        if(m_searchColor==(COLORREF)-1)
            m_searchColor=RGB(255,0,0);
        Invalidate(FALSE);
        return;
    case ID_MENU_SEARCH_AREA:
    case ID_MENU_SEARCH_POINT:
        m_analysis.Cancel();
        m_localOverlay.Cancel();
        m_localOverlay.SetTool(CLocalOverlay::Pan);
        SetMouseState(MOUSE_DEFAULT);
        m_searchMode=id==ID_MENU_SEARCH_AREA?1:2;m_searchDragging=false;
        SetFocus();
        return;
    case ID_MENU_SEARCH_MAP:
    case ID_MENU_SEARCH_MAP_INPUT:
        StartMapSearch(false,CRect());
        return;
    default:
        return;
    }
    frame->m_pRibbonMapBrightSlider->SetPos(m_mapBrightness);
    frame->m_pRibbonMapContrastSlider->SetPos(m_mapContrast);
    CString value;
    value.Format(_T("%d%%"),m_mapBrightness);
    frame->m_pRibbonMapBrightEdit->SetEditText(value);
    value.Format(_T("%d%%"),m_mapContrast);
    frame->m_pRibbonMapContrastEdit->SetEditText(value);
    Invalidate(FALSE);
}

void CC4IMapView::OnUpdateMapRibbon(CCmdUI* ui)
{
    ui->Enable(m_bDCInitialized);
    if(ui->m_nID==ID_MENU_SEARCH_AREA)ui->SetCheck(m_searchMode==1);
    if(ui->m_nID==ID_MENU_SEARCH_POINT)ui->SetCheck(m_searchMode==2);
}

void CC4IMapView::CaptureMapImage(bool clipboard)
{
    CRect area;
    GetClientRect(&area);
    if(area.IsRectEmpty()||!m_bDCInitialized)
        return;
    DrawSituation(FALSE,TRUE,TRUE,CPoint(0,0));
    CImage image;
    if(!CMapImageAdjustment::Capture(&BufferDC,area.Width(),area.Height(),image))
    {
        AfxMessageBox(_T("지도 이미지 캡처에 실패했습니다."));
        return;
    }
    if(clipboard)
    {
        if(!CMapImageAdjustment::CopyToClipboard(GetSafeHwnd(),image))
            AfxMessageBox(_T("클립보드에 이미지를 복사하지 못했습니다."));
        return;
    }
    auto captured = std::make_shared<CImage>();
    captured->Attach(image.Detach());
    ModelessDialogs::File(this, false, _T("map.png"),
        _T("PNG 이미지 (*.png)|*.png|JPEG 이미지 (*.jpg)|*.jpg|BMP 이미지 (*.bmp)|*.bmp||"),
        [captured](CString path, UINT selected) {
            const GUID* format = selected == 2 ? &Gdiplus::ImageFormatJPEG :
                (selected == 3 ? &Gdiplus::ImageFormatBMP : &Gdiplus::ImageFormatPNG);
            if (FAILED(captured->Save(path, *format)))
                AfxMessageBox(_T("이미지 저장에 실패했습니다. 경로와 쓰기 권한을 확인하세요."));
        });
}

void CC4IMapView::DrawMapSearch(CDC* dc)
{
    MapSearchDraw(dc,m_pCoordMgr,m_searchResults,m_searchColor);
    if(m_searchDragging)
    {
        CRect rect(m_searchStart,m_searchEnd);
        rect.NormalizeRect();
        CPen pen(PS_DOT,1,m_searchColor);
        auto old=dc->SelectObject(&pen);
        auto brush=dc->SelectStockObject(NULL_BRUSH);
        if(get_mainfrm()->m_pRibbonSearchAreaType->GetCurSel()==1)
            dc->Ellipse(rect);
        else 
            dc->Rectangle(rect);
        dc->SelectObject(brush);
        dc->SelectObject(old);
    }
}

LRESULT CC4IMapView::OnMapSearchMove(WPARAM,LPARAM data)
{
    auto location=reinterpret_cast<const MapSearchPoint*>(data);
    if(!location)
        return 0;
    double x,y;
    m_pCoordMgr->WorldXY2ScreenXY(location->x,location->y,&x,&y);
    if(!std::isfinite(x)||!std::isfinite(y))
        return 0;
    CRect rect;GetClientRect(&rect);
    m_pCoordMgr->m_fCenterX+=rect.CenterPoint().x-x;m_pCoordMgr->m_fCenterY+=rect.CenterPoint().y-y;
    m_mapFrameCache.InvalidateFrom();
    Invalidate(FALSE);
    return 0;
}

void CC4IMapView::OutputMapIdentify(const CString& title,const std::vector<CString>& fields,const std::vector<CString>& values)
{
    MapSearchReply reply;
    reply.message=title;
    if(!fields.empty())
    {
        MapSearchFeature feature;
        feature.layer=title;
        feature.fields=fields;
        feature.values=values;
        feature.world.push_back(m_searchIdentifyLocation);
        reply.features.push_back(std::move(feature));
    }
    get_mainfrm()->m_wndOutput.ShowMapSearch(reply);
}
