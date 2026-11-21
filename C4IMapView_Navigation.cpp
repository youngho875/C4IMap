#include "pch.h"
#include "C4IMapViewHeader.h"
#include "DisplayProjection.h"
#include "MapDecoration.h"
#include <algorithm>
#include <cmath>

double CC4IMapView::NavigationDenominator()
{
    if(!m_pCoordMgr||!m_bDCInitialized)return 0;
    CRect rect;GetClientRect(&rect);if(rect.IsRectEmpty())return 0;
    auto center=rect.CenterPoint();double lon,lat,lon2,lat2;
    if(!MapScreenLonLat(m_pCoordMgr,center.x,center.y,lon,lat)||!MapScreenLonLat(m_pCoordMgr,center.x+1,center.y,lon2,lat2))return 0;
    CClientDC dc(this);return MapDistanceMetres(lon,lat,lon2,lat2)*dc.GetDeviceCaps(LOGPIXELSX)/0.0254;
}
void CC4IMapView::SetNavigationScale(double target)
{
    target=(std::max)(m_navigationMin,(std::min)(m_navigationMax,target));
    CRect rect;GetClientRect(&rect);auto center=rect.CenterPoint();
    for(int i=0;i<6;++i)
    {
        double current=NavigationDenominator(),old=m_pCoordMgr->m_fScale;
        if(!(current>0)||!(old>0)||fabs(current-target)<target*1e-6)break;
        double scale=(std::max)(1e-6,(std::min)(2500.,old*current/target));
        if(fabs(scale-old)<old*1e-12)break;
        const double ratio=scale/old;
        m_pCoordMgr->m_fCenterX=center.x-(center.x-m_pCoordMgr->m_fCenterX)*ratio;
        m_pCoordMgr->m_fCenterY=center.y-(center.y-m_pCoordMgr->m_fCenterY)*ratio;
        m_pCoordMgr->m_fScale=scale;
    }
}
void CC4IMapView::CaptureNavigation()
{
    if(!m_bDCInitialized||m_rotationDragging||m_bPanning||mb_moving)return;
    CRect rect;GetClientRect(&rect);MapNavigationView view;
    if(!MapScreenLonLat(m_pCoordMgr,rect.CenterPoint().x,rect.CenterPoint().y,view.longitude,view.latitude))return;
    view.denominator=NavigationDenominator();view.projection=DisplayProjection().Kind();view.tmEpsg=DisplayProjection().TmEpsg();view.angle=DisplayProjection().Angle();
    if(m_navigationRestoring){m_navigationHistory.SynchronizeCurrent(view);m_navigationRestoring=false;}
    else m_navigationHistory.Record(view);
}
bool CC4IMapView::RestoreNavigation(const MapNavigationView& view,bool historyStep)
{
    if(!DisplayProjection().Set(m_pCoordMgr,view.projection,view.tmEpsg)||!DisplayProjection().SetAngle(m_pCoordMgr,view.angle))return false;
    double x,y;m_pCoordMgr->WorldLonLat2ScreenXY(view.longitude,view.latitude,&x,&y,m_pCoordMgr->m_fCenterX,m_pCoordMgr->m_fCenterY,m_pCoordMgr->m_fScale);
    if(!std::isfinite(x)||!std::isfinite(y))return false;
    CRect rect;GetClientRect(&rect);m_pCoordMgr->m_fCenterX+=rect.CenterPoint().x-x;m_pCoordMgr->m_fCenterY+=rect.CenterPoint().y-y;
    SetNavigationScale(view.denominator);m_navigationRestoring=historyStep;
    m_mapFrameCache.InvalidateFrom();Invalidate(FALSE);return true;
}
void CC4IMapView::OnNavigationCommand(UINT id)
{
    if(!m_bDCInitialized)return;
    CaptureNavigation();auto frame=get_mainfrm();
    if(id==ID_MENU_MAP_UNDO||id==ID_MENU_MAP_REDO||id==ID_MENU_MAP_INIT)
    {
        MapNavigationView view;bool found=id==ID_MENU_MAP_UNDO?m_navigationHistory.Previous(view):(id==ID_MENU_MAP_REDO?m_navigationHistory.Next(view):m_navigationHistory.Initial(view));
        if(found&&!RestoreNavigation(view,id!=ID_MENU_MAP_INIT))AfxMessageBox(_T("현재 투영에서 해당 화면을 복원할 수 없습니다."));return;
    }
    if(id==ID_MENU_COORDINATE_MOVE)
    {
        CRect rect;GetClientRect(&rect);MapNavigationView view;
        if(!MapScreenLonLat(m_pCoordMgr,rect.CenterPoint().x,rect.CenterPoint().y,view.longitude,view.latitude))return;
        CMapCoordinateDialog dialog(view.longitude,view.latitude,this);if(dialog.Show()!=IDOK)return;
        view.longitude=dialog.Longitude();view.latitude=dialog.Latitude();view.denominator=NavigationDenominator();view.projection=DisplayProjection().Kind();view.tmEpsg=DisplayProjection().TmEpsg();view.angle=DisplayProjection().Angle();
        if(!RestoreNavigation(view,false))AfxMessageBox(_T("입력 좌표를 현재 투영으로 표시할 수 없습니다."));return;
    }
    auto edit=DYNAMIC_DOWNCAST(CMFCRibbonEdit,frame->m_wndRibbonBar.FindByID(id));if(!edit)return;
    double value;
    if(!CMapNavigationHistory::ParseNumber(edit->GetEditText(),value,id!=ID_MENU_MAP_SCALE_ZOOM))
    {AfxMessageBox(_T("양수를 입력하세요. 축척은 1:100000 또는 100000 형식입니다."));return;}
    if(id==ID_MENU_MAP_SCALE_ZOOM)
    {
        if(value<=1||value>10){AfxMessageBox(_T("스크롤 배율은 1 초과, 10 이하로 입력하세요. 기본값은 1.5입니다."));return;}
        m_navigationScroll=value;return;
    }
    if(value<1||value>1e10){AfxMessageBox(_T("축척 분모는 1~10,000,000,000 범위입니다."));return;}
    if(id==ID_MENU_MAP_SCALE_MIN){if(value>=m_navigationMax){AfxMessageBox(_T("최소 분모는 최대 분모보다 작아야 합니다."));return;}m_navigationMin=value;}
    if(id==ID_MENU_MAP_SCALE_MAX){if(value<=m_navigationMin){AfxMessageBox(_T("최대 분모는 최소 분모보다 커야 합니다."));return;}m_navigationMax=value;}
    SetNavigationScale(id==ID_MENU_MAP_SCALE_CURRENT?value:NavigationDenominator());
    m_mapFrameCache.InvalidateFrom();Invalidate(FALSE);
}
void CC4IMapView::OnUpdateNavigation(CCmdUI* ui)
{
    ui->Enable(m_bDCInitialized);
    if(ui->m_nID==ID_MENU_MAP_UNDO){ui->Enable(m_bDCInitialized&&m_navigationHistory.CanPrevious());return;}
    if(ui->m_nID==ID_MENU_MAP_REDO){ui->Enable(m_bDCInitialized&&m_navigationHistory.CanNext());return;}
    auto frame=get_mainfrm();if(!frame)return;
    auto edit=DYNAMIC_DOWNCAST(CMFCRibbonEdit,frame->m_wndRibbonBar.FindByID(ui->m_nID));if(!edit)return;
    CWnd* focus=CWnd::GetFocus();MSG pending={};
    if(edit->HasFocus()||edit->IsFocused()||edit->IsDroppedDown()||CMFCPopupMenu::GetActiveMenu()||
       (focus&&frame->m_wndRibbonBar.IsChild(focus))||PeekMessage(&pending,frame->GetSafeHwnd(),WM_COMMAND,WM_COMMAND,PM_NOREMOVE))return;
    double value=ui->m_nID==ID_MENU_MAP_SCALE_MIN?m_navigationMin:ui->m_nID==ID_MENU_MAP_SCALE_MAX?m_navigationMax:ui->m_nID==ID_MENU_MAP_SCALE_ZOOM?m_navigationScroll:NavigationDenominator();
    CString text;text.Format(ui->m_nID==ID_MENU_MAP_SCALE_ZOOM?_T("%.3g"):_T("%.0f"),value);
    if(edit->GetEditText()!=text)edit->SetEditText(text);
}

void CC4IMapView::OnNavigationCoordinate(){OnNavigationCommand(ID_MENU_COORDINATE_MOVE);}
