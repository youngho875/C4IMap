#include "pch.h"
#include "C4IMapViewHeader.h"




void CC4IMapView::OnUpdateCheckSatellite(CCmdUI* pCmdUI)
{
	// TODO: 여기에 명령 업데이트 UI 처리기 코드를 추가합니다.
	pCmdUI->SetCheck(m_bsatellite);
}

void CC4IMapView::OnCheckSatellite()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
	m_bsatellite = !m_bsatellite;
	Invalidate(false);
/*
	CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK_SATELLITE);
	if (pCheck != nullptr)
	{
		BOOL bChecked = pCheck->GetCheck();
		if (bChecked == TRUE)
		{
			m_bsatellite = TRUE;
		}
		else
		{
			m_bsatellite = FALSE;
		}
	}

	Invalidate();
*/
}


void CC4IMapView::OnCheckRaster()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
	m_bRaster = !m_bRaster;
	Invalidate(false);
}

void CC4IMapView::OnUpdateCheckRaster(CCmdUI* pCmdUI)
{
	// TODO: 여기에 명령 업데이트 UI 처리기 코드를 추가합니다.
	pCmdUI->SetCheck(m_bRaster);

}

void CC4IMapView::OnCheckShape()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
	m_bShape = !m_bShape;
	Invalidate(false);
}

void CC4IMapView::OnUpdateCheckShape(CCmdUI* pCmdUI)
{
	// TODO: 여기에 명령 업데이트 UI 처리기 코드를 추가합니다.
	pCmdUI->SetCheck(m_bShape);
}

void CC4IMapView::OnCheckDted()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
	m_bDted = !m_bDted;
	Invalidate(false);
}


void CC4IMapView::OnUpdateCheckDted(CCmdUI* pCmdUI)
{
	// TODO: 여기에 명령 업데이트 UI 처리기 코드를 추가합니다.
	pCmdUI->SetCheck(m_bDted);
}


void CC4IMapView::OnMenuMouseBase()
{
	SetMouseState(MOUSE_DEFAULT);
	//SetMouseState(MOUSE_COPLAY);
}

void CC4IMapView::OnUpdateMenuMouseBase(CCmdUI* pCmdUI)
{
	// 지도 정상 로드 시 able, 아닐 경우 disable
	//if (IsEnableMap())
	//{
	//	// 마우스가 상황/투명도/지도 기본 상태가 아닐 경우 able, 상태라면 disable
		if (GetMouseState() == MOUSE_DEFAULT || GetMouseState() == MOUSE_OVERLAY)
			pCmdUI->Enable(FALSE);
		else
			pCmdUI->Enable(TRUE);
	//}
	//else
	//	pCmdUI->Enable(FALSE);
}
// 화면 범주 - 마우스 확대
void CC4IMapView::OnMenuMouseZoomIn()
{
    if (!m_pCoordMgr || !m_bDCInitialized)
        return;
    if (m_analysis.Active())
        m_analysis.Cancel();
    m_localOverlay.SetTool(CLocalOverlay::Pan);
    m_bPanning = m_bOnZoom = m_bMapMove = mb_moving = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();
    SetMouseState(MOUSE_ZOOMIN);
    SetFocus();
}

void CC4IMapView::OnUpdateMenuMouseZoomIn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_pCoordMgr != nullptr && m_bDCInitialized);
    pCmdUI->SetCheck(GetMouseState() == MOUSE_ZOOMIN);
}

// 화면 범주 - 마우스 축소
void CC4IMapView::OnMenuMouseZoomOut()
{
    if (!m_pCoordMgr || !m_bDCInitialized)
        return;
    if (m_analysis.Active())
        m_analysis.Cancel();
    m_localOverlay.SetTool(CLocalOverlay::Pan);
    m_bPanning = m_bOnZoom = m_bMapMove = mb_moving = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();
    SetMouseState(MOUSE_ZOOMOUT);
    SetFocus();
}

void CC4IMapView::OnUpdateMenuMouseZoomOut(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_pCoordMgr != nullptr && m_bDCInitialized);
    pCmdUI->SetCheck(GetMouseState() == MOUSE_ZOOMOUT);
}

// 화면 범주 - 마우스 이동
void CC4IMapView::OnMenuMousePanning()
{
	SetMouseState(MOUSE_PANNING);
}

void CC4IMapView::OnUpdateMenuMousePanning(CCmdUI* pCmdUI)
{
	// 지도 정상 로드 시 able, 아닐 경우 disable
	//if (IsEnableMap())
	//{
	//	// 마우스가 이동 상태 일 경우 disable, 아닐 경우 enable
		if (GetMouseState() == MOUSE_PANNING)
			pCmdUI->Enable(FALSE);
		else
			pCmdUI->Enable(TRUE);
	//}
	//else
	//	pCmdUI->Enable(FALSE);
}
/*
void CC4IMapView::OnOverlayDraw()
{
	//m_pDlgOverlayControler->ShowWindow(SW_SHOW);

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if (pFrame && m_pDlgOverlayControler != NULL)
	{
		int nGrpOffset = pFrame->m_pRibbonDrawGallery->GetGroupOffset();
		CString strGrpNm = pFrame->m_pRibbonDrawGallery->GetGroupName(nGrpOffset);
		int nPaletteID = pFrame->m_pRibbonDrawGallery->GetPaletteID();
		int nSelectItem = pFrame->m_pRibbonDrawGallery->GetSelectedItem();
		int nSelectRow = pFrame->m_pRibbonDrawGallery->GetIconsInRow();
		int nLastSelectItem = 0; 
		CString strDrawOrder = _T("");

		CString strCommandNm = pFrame->GetOverlayDrawItem(nSelectItem);

		TRACE("OnOverlayDraw %d, %d, %d [%s]\n", nPaletteID, nSelectItem, nLastSelectItem, strCommandNm);
	}
}
*/
/*
void CC4IMapView::OnUpdateOverlayDraw(CCmdUI* pCmdUI) 
{
	if(m_pDlgOverlayControler != NULL)
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

void CC4IMapView::OnOverlayNew()
{
	//OvlMessageProc(MAPX_OVER_NEWCRT, NULL, NULL, NULL);
	SetMouseState(MOUSE_OVERLAY);
}

void CC4IMapView::OnUpdateOverlayNew(CCmdUI* pCmdUI)
{
	if (m_pDlgOverlayControler != NULL)
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}
*/