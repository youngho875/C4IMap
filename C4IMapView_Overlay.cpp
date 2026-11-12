#include "pch.h"
#include "C4IMapView.h"
#include "C4IMapViewHeader.h"

CLocalOverlay& CC4IMapView::OverlayLayers()
{ 
	return m_localOverlay; 
}

void CC4IMapView::ResetLocalOverlay()
{
	m_localOverlay.Clear();
	RefreshOverlayLayers();
	Invalidate(FALSE);
}

void CC4IMapView::RefreshOverlayLayers()
{
	auto frame = DYNAMIC_DOWNCAST(CMainFrame, AfxGetMainWnd());
	if (frame && frame->m_wndCOPLayerView.GetSafeHwnd())
		frame->m_wndCOPLayerView.RefreshMsgList();
}

void CC4IMapView::OnOverlayLayerNew()
{
	m_localOverlay.Cancel();
	if (GetCapture() == this)
		ReleaseCapture();
	if (!m_localOverlay.AddLayer())
	{
		AfxMessageBox(_T("레이어를 추가할 수 없습니다."));
		return;
	}
	RefreshOverlayLayers();
	auto frame = DYNAMIC_DOWNCAST(CMainFrame, AfxGetMainWnd());
	if (frame)
		frame->m_wndCOPLayerView.ShowPane(TRUE, FALSE, TRUE);
	Invalidate(FALSE);
}

void CC4IMapView::OnOverlayLayerOpen()
{
	OnLocalOverlayCommand(ID_C4OVER_LOAD);
}

void CC4IMapView::OnOverlayLayerSave()
{
	OnLocalOverlayCommand(ID_C4OVER_SAVE);
}

void CC4IMapView::OnOverlayDraw()
{
	auto frame = get_mainfrm();
	if (!frame || !frame->m_pRibbonDrawGallery)
		return;

	int index = frame->m_pRibbonDrawGallery->GetSelectedItem();
	CString name = frame->GetOverlayDrawItem(index);
	UINT command = 0;
	if (name == _T("직선"))
		command = ID_C4OVER_LINE;
	else if (name == _T("화살표 직선"))
		command = ID_C4OVER_ARROW;
	else if (name == _T("사각형"))
		command = ID_C4OVER_RECT;
	else if (name == _T("타원") || name == _T("원"))
		command = ID_C4OVER_ELLIPSE;
	if (command)
	{
		OnLocalOverlayCommand(command);
		frame->AddOverlayRentlyDrawItem(index);
	}
	else
	{
		m_localOverlay.Cancel();
		AfxMessageBox(_T("이 도형은 아직 지원되지 않습니다. 도형그리기에서 선, 화살표, 사각형 또는 타원을 선택하세요."));
	}
}

void CC4IMapView::OnLocalOverlayCommand(UINT id)
{
	m_localOverlay.Cancel();
	if (GetCapture() == this)
		ReleaseCapture();

	m_bPanning = FALSE;
	m_bMapMove = FALSE;
	m_bOnZoom = FALSE;
	mb_moving = FALSE;

	if (id >= ID_C4OVER_PAN && id <= ID_C4OVER_ELLIPSE)
		m_localOverlay.SetTool(id - ID_C4OVER_PAN);
	else if (id == ID_C4OVER_COLOR)
	{
		CColorDialog dlg(m_localOverlay.Color(), CC_FULLOPEN, this);
		if (dlg.DoModal() == IDOK)
			m_localOverlay.SetColor(dlg.GetColor());
	}
	else if (id == ID_C4OVER_UNDO)
		m_localOverlay.Undo();
	else if (id == ID_C4OVER_REDO)
		m_localOverlay.Redo();
	else if (id == ID_C4OVER_SAVE || id == ID_C4OVER_LOAD)
	{
		BOOL load = id == ID_C4OVER_LOAD;
		CFileDialog dlg(load, _T("xml"), _T("overlay.xml"), OFN_HIDEREADONLY | (load ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT), _T("C4IMap Overlay XML (*.xml)|*.xml||"), this);
		if (dlg.DoModal() == IDOK)
		{
			if (load && !m_localOverlay.Layers().empty() && AfxMessageBox(_T("현재 Overlay 도형을 선택한 파일로 교체하시겠습니까? \
				저장하지 않은 도형은 사라집니다."), MB_YESNO | MB_ICONQUESTION) != IDYES)
				return;
			CString error;
			bool ok = load ? m_localOverlay.Load(dlg.GetPathName(), error) : m_localOverlay.Save(dlg.GetPathName(), error);
			if (!ok)
				AfxMessageBox(error, MB_ICONERROR);
		}
	}
	RefreshOverlayLayers();
	Invalidate(FALSE);
}

void CC4IMapView::OnUpdateLocalOverlay(CCmdUI* ui)
{
	ui->Enable(ui->m_nID == ID_C4OVER_REDO ? m_localOverlay.CanRedo() : (ui->m_nID != ID_C4OVER_UNDO || m_localOverlay.ActiveCount() > 0));
	if (ui->m_nID <= ID_C4OVER_ELLIPSE)
		ui->SetRadio(m_localOverlay.GetTool() == int(ui->m_nID - ID_C4OVER_PAN));
}