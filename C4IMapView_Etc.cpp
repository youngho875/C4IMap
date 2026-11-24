#include "pch.h"
#include "C4IMapViewHeader.h"
#include "TerrainLayer.h"
#include "TerrainTileLayer.h"
#include <cmath>
#include <memory>



void CC4IMapView::ScreenCaptureRouting()
{
	/*
	HWND hWindow;
	CScrCapture scrCapture;
	hWindow = this->GetSafeHwnd();
	scrCapture.CaptureAs(hWindow);
		*/

	//scrCap.SetWindow(this->GetSafeHwnd());
	//scrCap.Capture();
	return;//return 0;
}

//void CC4IMapView::OnComboChangeScale()
//{
//	// TODO: 여기에 명령 처리기 코드를 추가합니다.
//	CMainFrame* pFrame;
//
//	pFrame = (CMainFrame*)AfxGetMainWnd();
//	int nSel = pFrame->m_ctrlCombo.GetCurSel();
//	if (nSel == 0) ChangeScale(1.2f);
//	else if (nSel == 1)	ChangeScale(1.0f);
//	else if (nSel == 2) ChangeScale(2.0f);
//	else if (nSel == 3) ChangeScale(4.0f);
//	else if (nSel == 4) ChangeScale(8.0f);
//	else if (nSel == 5) ChangeScale(16.0f);
//	else if (nSel == 6) ChangeScale(50.0f);
//	else if (nSel == 7) ChangeScale(100.0f);
//	else if (nSel == 8) ChangeScale(200.0f);
//
//}


//void CC4IMapView::OnComboCenterChange()
//{
//	// TODO: 여기에 명령 처리기 코드를 추가합니다.
//	CMainFrame* pFrame;
//	pFrame = (CMainFrame*)AfxGetMainWnd();
//
//	int numSel = pFrame->m_ctrlCombo1.GetCurSel();
	//if (numSel == 1)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT1);
	//else if (numSel == 2)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT3);
	//else if (numSel == 3)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT5);
	//else if (numSel == 4)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT8);
	//else if (numSel == 5)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT10);
	//else if (numSel == 6)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT11);
	//else if (numSel == 7)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT15);
	//else if (numSel == 8)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT16);
	//else if (numSel == 9)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT17);
	//else if (numSel == 10)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT18);
	//else if (numSel == 11)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT19);
	//else if (numSel == 12)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT20);
	//else if (numSel == 13)	OnMenuCenterUnit(ID_MENU_CENTER_UNIT38);

	//else;
//}


void CC4IMapView::ChangeScale(float fScale)
{
	// TODO: Add your command handler code here
	// mbc 7.20  배율에 따른 지도 교체
	m_fPrevScale = m_pCoordMgr->m_fScale;

	// jykim 08.04
	double	fXCoord, fYCoord;
	m_pCoordMgr->ScreenXY2WorldXY(m_pCoordMgr->m_nDisplayViewWidth / 2, m_pCoordMgr->m_nDisplayViewHeight / 2, &fXCoord, &fYCoord);

	//	zDelta/abs(zDelta)는 부호를 얻어오기 위한 것. 	
	m_pCoordMgr->m_fScale = fScale;

	// jykim 08.04
	long nCenterX, nCenterY;
	m_pCoordMgr->WorldXY2ScreenXY(fXCoord, fYCoord, &nCenterX, &nCenterY);
	m_pCoordMgr->SetNewCenter(nCenterX, nCenterY);


	// mbc 7.20  배율에 따른 지도 교체 
	//if (m_bRaster != 0) {
	//	if (m_fPrevScale < 2.3 && m_pCoordMgr->m_fScale >= 2.3)
	//	{
	//		m_bRaster = FALSE;
	//		OnShowRaster();
	//	}
	//	else if (m_fPrevScale >= 2.3 && m_pCoordMgr->m_fScale < 10.0)
	//	{
	//		m_bRaster = FALSE;
	//		OnShowRaster();
	//	}
	//	else if (m_fPrevScale < 10.0 && m_pCoordMgr->m_fScale >= 10.0)
	//	{
	//		m_bRaster = FALSE;
	//		OnShowRaster();
	//	}
	//}
	//		else if(m_bRaster != 0 && m_fPrevScale >= 20.0 && m_pCoordMgr->m_fScale < 20.0)
	//		{
	//			m_bRaster = FALSE;
	//			OnShowRaster();
	//		}

	// mbc 11.28 상태바
	CString sStatus;
	sStatus.Format("배율:%f       ", m_pCoordMgr->m_fScale);

	CMainFrame* pFrame;
	pFrame = (CMainFrame*)AfxGetMainWnd();
	//pFrame->m_wndStatusBar.SetPaneText(5, sStatus);

	//m_pOverlayThread->RePosition();
	//if (m_pDlgOverlayControler->IsWindowVisible() == TRUE)
	//	m_pDlgOverlayControler->UpDateGridDraw();
	//	DrawSituation(TRUE, TRUE, TRUE, TRUE); 

}

// 상태바 처리
// youngho
void CC4IMapView::SetStatusBar1Txt(CString strMsg)
{

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if (pFrame)
	{
		int nElement = pFrame->m_wndStatusBar.GetCount();
		if (nElement == 0) return;

		CMFCRibbonBaseElement* pStatusElement = pFrame->m_wndStatusBar.GetElement(nElement - 1);
		if (pStatusElement)
		{
			pStatusElement->SetText(strMsg);
			pStatusElement->Redraw();
		}
	}
}
void CC4IMapView::SetStatusBar2Txt(CString strMsg)
{
	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if (pFrame)
	{
		int nElement = pFrame->m_wndStatusBar.GetExCount();
		if (nElement == 0) return;

		CMFCRibbonBaseElement* pStatusElement = pFrame->m_wndStatusBar.GetExElement(nElement - 1);
		if (pStatusElement)
		{
			pStatusElement->SetText(strMsg);
			pStatusElement->Redraw();
		}
	}
}

void CC4IMapView::UpdateStatusBar(CPoint point)
{
	CCoordMgr* CoordMgr = GP_COORDMGR;
	CCoord		Coord;
	GPoint		ptWorldPoint;

	char strUTM[20], strGeoref[10], strMGRS[20], strGARS[9];

	CoordMgr->ScreenXY2WorldXY(point.x, point.y, &ptWorldPoint.x, &ptWorldPoint.y, m_pCoordMgr->m_fCenterX, m_pCoordMgr->m_fCenterY, m_pCoordMgr->m_fScale);
	//Coord.x_y_to_degrees_lat_long_str(ptWorldPoint.x, ptWorldPoint.y, strLat, strLong);
	//Coord.x_y_to_degrees_lat_long(ptWorldPoint.x, ptWorldPoint.y, &dLat, &dLong);
	Coord.x_y_to_georef_str(ptWorldPoint.x, ptWorldPoint.y, strGeoref);
	Coord.x_y_to_utm_str(ptWorldPoint.x, ptWorldPoint.y, strMGRS);
	Coord.x_y_to__other_utm_str(ptWorldPoint.x, ptWorldPoint.y, strUTM);
	
	Coord.x_y_to_degrees_lat_long(ptWorldPoint.x, ptWorldPoint.y, &m_pStatusInfo.pGeo_y, &m_pStatusInfo.pGeo_x);

	CString strStatus = "";
	CString strGeo = "", strrMGR = "", strrUTM = "", strGeoRef = "", strrGARS = "", strHeight = "";
	CString strMGRSZone = _T("");

	int DoX, MinX;	double SecX;
	int DoY, MinY;  double SecY;
	
	Coord.DegreeToDmsLon(m_pStatusInfo.pGeo_x,  &DoX,  &MinX,  &SecX);
	Coord.DegreeToDmsLat(m_pStatusInfo.pGeo_y,  &DoY,  &MinY,  &SecY);

	strGeo.Format(" Lon:%3d° %2d'%7.3f\", Lat:%3d° %2d'%7.3f\" (X:%.6f, Y:%.6f) |", DoX, MinX, SecX, DoY, MinY, SecY, m_pStatusInfo.pGeo_x, m_pStatusInfo.pGeo_y);
	strStatus += strGeo;

	strrMGR.Format(" MGRS: %s |", strMGRS);
	strStatus += strrMGR;

	strrUTM.Format(" UTM: %s |", strUTM);
	strStatus += strrUTM;

	strGeoRef.Format(" GEOREF: %s |", strGeoref);
	strStatus += strGeoRef;

	Coord.degree_lat_lon_to_gars(m_pStatusInfo.pGeo_y, m_pStatusInfo.pGeo_x, 5, strGARS);
	strrGARS.Format("GARS: %s |", strGARS);
	strStatus += strrGARS;

    // The main map uses the geographic display projection; derive the sample
    // directly from it instead of reusing unrelated legacy DTED coordinates.
    double terrainLon=m_pStatusInfo.pGeo_x,terrainLat=m_pStatusInfo.pGeo_y;
    if(m_pCoordMgr->m_bFlat && m_pCoordMgr->m_fScale>0){
        terrainLon=127.031+(point.x-m_pCoordMgr->m_fCenterX)/(47.5*m_pCoordMgr->m_fScale);
        terrainLat=37.0861-(point.y-m_pCoordMgr->m_fCenterY)/(60.0*m_pCoordMgr->m_fScale);
    }
    strHeight=TerrainAltitude(terrainLon,terrainLat);
	strStatus += strHeight;

	m_pStatusInfo.strStatusCoord = strStatus;

	SetStatusBar1Txt(m_pStatusInfo.strStatusCoord);

	//if (m_pStatusInfo.strStatusCoord.IsEmpty())
	//	SetStatusBar2Txt(_T("Disconnected"));
	//else
	//	SetStatusBar2Txt(_T("Connected"));
}

LRESULT CC4IMapView::move_with_coord(WPARAM w, LPARAM l)
{
    // WM_MOVE_WITH_COORD transfers ownership: [latitude, longitude] in degrees.
    std::unique_ptr<CStringArray> values(reinterpret_cast<CStringArray*>(w));
    if (!values || values->GetSize() < 2 || !m_pCoordMgr)
        return FALSE;

    CString latitude = values->GetAt(0), longitude = values->GetAt(1);
    latitude.Trim();
    longitude.Trim();
    TCHAR* latEnd = nullptr;
    TCHAR* lonEnd = nullptr;
    const double lat = _tcstod(latitude, &latEnd);
    const double lon = _tcstod(longitude, &lonEnd);
    if (latitude.IsEmpty() || longitude.IsEmpty() || *latEnd || *lonEnd ||
        !std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 ||
        lon < -180 || lon > 180)
        return FALSE;

    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty())
        return FALSE;

    double screenX, screenY;
    m_pCoordMgr->WorldLonLat2ScreenXY(lon, lat, &screenX, &screenY,
        m_pCoordMgr->m_fCenterX, m_pCoordMgr->m_fCenterY, m_pCoordMgr->m_fScale);
    if (!std::isfinite(screenX) || !std::isfinite(screenY))
        return FALSE;

    // Preserve the main view's projection, rotation and scale.
    m_pCoordMgr->m_fCenterX += client.left + client.Width() / 2.0 - screenX;
    m_pCoordMgr->m_fCenterY += client.top + client.Height() / 2.0 - screenY;
    m_mapFrameCache.InvalidateFrom();
    Invalidate(FALSE);
    return TRUE;
}

