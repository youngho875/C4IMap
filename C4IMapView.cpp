// 이 MFC 샘플 소스 코드는 MFC Microsoft Office Fluent 사용자 인터페이스("Fluent UI")를
// 사용하는 방법을 보여 주며, MFC C++ 라이브러리 소프트웨어에 포함된
// Microsoft Foundation Classes Reference 및 관련 전자 문서에 대해
// 추가적으로 제공되는 내용입니다.
// Fluent UI를 복사, 사용 또는 배포하는 데 대한 사용 약관은 별도로 제공됩니다.
// Fluent UI 라이선싱 프로그램에 대한 자세한 내용은
// https://go.microsoft.com/fwlink/?LinkId=238214.
//
// Copyright (C) Microsoft Corporation
// All rights reserved.

// C4IMapView.cpp: CC4IMapView 클래스의 구현
//

#include "pch.h"
#include "DisplayProjection.h"
#include "TerrainProfileDialog.h"
#include "TerrainLayer.h"
#include "TerrainTileLayer.h"
//#include "framework.h"
// SHARED_HANDLERS는 미리 보기, 축소판 그림 및 검색 필터 처리기를 구현하는 ATL 프로젝트에서 정의할 수 있으며
// 해당 프로젝트와 문서 코드를 공유하도록 해 줍니다.
//#ifndef SHARED_HANDLERS
//#include "C4IMap.h"
//#endif

#include "C4IMapViewHeader.h"
#include "C4IMapView.h"

//#include "GlobalInfo.h"
#include "../inc/Useful.h"
#include "GPoint.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CC4IMapView

IMPLEMENT_DYNCREATE(CC4IMapView, CView)

BEGIN_MESSAGE_MAP(CC4IMapView, CView)
    ON_COMMAND_EX(ID_ANALYSIS_VISIBLE_INPUT, &CC4IMapView::OnAnalysisInputCommand)
    ON_UPDATE_COMMAND_UI(ID_ANALYSIS_VISIBLE_INPUT, &CC4IMapView::OnUpdateAnalysis)
    ON_COMMAND_RANGE(ID_MENU_ANALYSIS_CLEAR, ID_MENU_ANALYSIS_RADIUS_INPUT, &CC4IMapView::OnAnalysisCommand)
    ON_UPDATE_COMMAND_UI_RANGE(ID_MENU_ANALYSIS_CLEAR, ID_MENU_ANALYSIS_RADIUS_INPUT, &CC4IMapView::OnUpdateAnalysis)
    ON_COMMAND_RANGE(ID_MENU_OVERLAY_PASTE, ID_MENU_OVERLAY_COPY, &CC4IMapView::OnOverlayEdit)
    ON_COMMAND_EX(ID_MENU_OVERLAY_DELETE, &CC4IMapView::OnOverlayDeleteCommand)
    ON_UPDATE_COMMAND_UI(ID_MENU_OVERLAY_DELETE, &CC4IMapView::OnUpdateOverlayEdit)
    ON_UPDATE_COMMAND_UI_RANGE(ID_MENU_OVERLAY_PASTE, ID_MENU_OVERLAY_COPY, &CC4IMapView::OnUpdateOverlayEdit)

    ON_COMMAND(ID_MENU_MAP_ROTATE, &CC4IMapView::OnMapRotate)
    ON_UPDATE_COMMAND_UI(ID_MENU_MAP_ROTATE, &CC4IMapView::OnUpdateMapRotate)
    ON_WM_ERASEBKGND()
    ON_UPDATE_COMMAND_UI(ID_MENU_PROJECTION, &CC4IMapView::OnUpdateDisplayProjection)
    ON_COMMAND_RANGE(ID_MENU_PROJECTION_GEO, ID_MENU_PROJECTION_EQU, &CC4IMapView::OnDisplayProjection)
    ON_UPDATE_COMMAND_UI_RANGE(ID_MENU_PROJECTION_GEO, ID_MENU_PROJECTION_EQU, &CC4IMapView::OnUpdateDisplayProjection)
    ON_COMMAND(ID_MAP_SOURCES_XML, &CC4IMapView::OnMapSourcesXml)
    ON_WM_TIMER()
    ON_COMMAND(ID_MENU_OVERLAY_NEW, &CC4IMapView::OnOverlayLayerNew)
    ON_COMMAND(ID_MENU_OVERLAY_OPEN, &CC4IMapView::OnOverlayLayerOpen)
    ON_COMMAND(ID_MENU_OVERLAY_SAVE, &CC4IMapView::OnOverlayLayerSave)
    ON_COMMAND_RANGE(ID_C4OVER_PAN, ID_C4OVER_REDO, &CC4IMapView::OnLocalOverlayCommand)
    ON_UPDATE_COMMAND_UI_RANGE(ID_C4OVER_PAN, ID_C4OVER_REDO, &CC4IMapView::OnUpdateLocalOverlay)
    ON_WM_CAPTURECHANGED()
    ON_COMMAND(ID_LOCAL_SHP_SETTINGS,&CC4IMapView::OnLocalSettings)
    ON_COMMAND(ID_LOCAL_SHP_IDENTIFY,&CC4IMapView::OnLocalIdentify)
    ON_COMMAND(ID_LOCAL_SHP_TOGGLE,&CC4IMapView::OnLocalToggle)
	// 표준 인쇄 명령입니다.
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CC4IMapView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_SETCURSOR()
	ON_MESSAGE(WM_OPTION_CHANGED, &CC4IMapView::OnOptionChange)
	ON_MESSAGE(WM_MOVE_WITH_COORD, &CC4IMapView::move_with_coord)
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	//ON_COMMAND(IDC_COMBO, &CC4IMapView::OnComboChangeScale)
	//ON_COMMAND(IDC_COMBO1, &CC4IMapView::OnComboCenterChange)
	ON_COMMAND(IDC_CHECK_SATELLITE, &CC4IMapView::OnCheckSatellite)
	ON_UPDATE_COMMAND_UI(IDC_CHECK_SATELLITE, &CC4IMapView::OnUpdateCheckSatellite)
	ON_COMMAND(ID_BTN_MAPGENERATER, &CC4IMapView::OnBtnMapgenerater)
	ON_COMMAND(IDC_CHECK_RASTER, &CC4IMapView::OnCheckRaster)
	ON_COMMAND(IDC_CHECK_SHAPE, &CC4IMapView::OnCheckShape)
	ON_UPDATE_COMMAND_UI(IDC_CHECK_RASTER, &CC4IMapView::OnUpdateCheckRaster)
	ON_UPDATE_COMMAND_UI(IDC_CHECK_SHAPE, &CC4IMapView::OnUpdateCheckShape)
	ON_COMMAND(IDC_CHECK_DTED, &CC4IMapView::OnCheckDted)
	ON_UPDATE_COMMAND_UI(IDC_CHECK_DTED, &CC4IMapView::OnUpdateCheckDted)

	// 지도 핸들링
	ON_COMMAND(ID_MENU_MOUSE_BASE, &CC4IMapView::OnMenuMouseBase)
	ON_UPDATE_COMMAND_UI(ID_MENU_MOUSE_BASE, &CC4IMapView::OnUpdateMenuMouseBase)
	ON_COMMAND(ID_MENU_MOUSE_ZOOMIN, &CC4IMapView::OnMenuMouseZoomIn)
	ON_UPDATE_COMMAND_UI(ID_MENU_MOUSE_ZOOMIN, &CC4IMapView::OnUpdateMenuMouseZoomIn)
	ON_COMMAND(ID_MENU_MOUSE_ZOOMOUT, &CC4IMapView::OnMenuMouseZoomOut)
	ON_UPDATE_COMMAND_UI(ID_MENU_MOUSE_ZOOMOUT, &CC4IMapView::OnUpdateMenuMouseZoomOut)
	ON_COMMAND(ID_MENU_MOUSE_MOVE, &CC4IMapView::OnMenuMousePanning)
	ON_UPDATE_COMMAND_UI(ID_MENU_MOUSE_MOVE, &CC4IMapView::OnUpdateMenuMousePanning)

	//ON_COMMAND(ID_MENU_OVERLAY_NEW, OnOverlayNew)
	//ON_UPDATE_COMMAND_UI(ID_MENU_OVERLAY_NEW, OnUpdateOverlayNew)
	ON_COMMAND(ID_RIBBON_OVERLAY_DRAW_GALLERY, OnOverlayDraw)
	//ON_UPDATE_COMMAND_UI(ID_RIBBON_OVERLAY_DRAW_GALLERY, OnUpdateOverlayDraw)

END_MESSAGE_MAP()

// CC4IMapView 생성/소멸

CC4IMapView::CC4IMapView()
//	: m_Thread(NULL)
{
	// TODO: 여기에 생성 코드를 추가합니다.
	OriginX = 0;
	OriginY = 0;
	m_recSave.SetRectEmpty();
	m_pMouseState = MOUSE_DEFAULT;
	m_pSearchOption = SEARCH_NONE;
	m_pMouseStateCoordOld = MOUSE_DEFAULT;
	//m_generateDlg = NULL;

	m_pCoordMgr = GP_COORDMGR;
	m_Coord.initialize_adaptation_data();

	m_dcMain = NULL;
	m_dcMainTemp = NULL;
	m_dcMapLayer = NULL;
	m_dcShpLayer = NULL;
	m_dcDTEDLayer = NULL;
	m_dcRasterLayer = NULL;
	//m_dcSymLayer = NULL;

	m_pOldBitmap4MainTemp = NULL;
	m_pOldBitmap4Map = NULL;
	m_pOldBitmap4Shape = NULL;
	m_pOldBitmap4DTED = NULL;
	m_pOldBitmap4Raster = NULL;
	m_pOldBitmap4Sym = NULL;

	m_brush_10_10_10.CreateSolidBrush(RGB(10, 10, 10));

	m_fPrevScale = 0.0;

	m_bRaster = FALSE;
	m_bsatellite = FALSE;
	m_bShape = FALSE;
	m_bDted = FALSE;
	m_bDCInitialized = FALSE;
	m_bMapMove = FALSE;
	m_bPanning = FALSE;
	mb_moving = FALSE;
	m_bOnZoom = FALSE;
	m_bIsDrawSituationInProcess = FALSE;

	//m_pMapConfigMgr = NULL;
	//m_pMapDataMgr = NULL;
	//m_pMapDisplayMgr = NULL;

	// 투명도
	m_pOverlayThread = NULL;
	m_pDlgOverlayControler = NULL;
	m_pDlgObjectList = NULL;
	m_pDlgOverlayProperty = NULL;
	m_pDlgOverlayTextEdit = NULL;
	m_pDlgOverlayEditObject = NULL;
	m_pDlgOverlayEditObjectPoint = NULL;

	m_nOverlyaEditObjectPointX = 0;
	m_nOverlyaEditObjectPointY = 0;

	//m_TmsMgr = NULL;
	mHeights = NULL;

}

CC4IMapView::~CC4IMapView()
{
    if (BufferDC.GetSafeHdc() && m_oldBufferBitmap)
        ::SelectObject(BufferDC.GetSafeHdc(), m_oldBufferBitmap);
    bmpBuffer.DeleteObject();
    BufferDC.DeleteDC();
	if (m_dcMainTemp && m_pOldBitmap4MainTemp)
		::SelectObject(m_dcMainTemp->GetSafeHdc(), m_pOldBitmap4MainTemp);
	if (m_dcMapLayer && m_pOldBitmap4Map)
		::SelectObject(m_dcMapLayer->GetSafeHdc(), m_pOldBitmap4Map);
	if (m_dcShpLayer && m_pOldBitmap4Shape)
		::SelectObject(m_dcShpLayer->GetSafeHdc(), m_pOldBitmap4Shape);
	if (m_dcDTEDLayer && m_pOldBitmap4DTED)
		::SelectObject(m_dcDTEDLayer->GetSafeHdc(), m_pOldBitmap4DTED);
	if (m_dcRasterLayer && m_pOldBitmap4Raster)
		::SelectObject(m_dcRasterLayer->GetSafeHdc(), m_pOldBitmap4Raster);
	//m_dcSymLayer->SelectObject(m_pOldBitmap4Sym);

	m_pBitmap4MainTemp.DeleteObject();
	m_pBitmap4Map.DeleteObject();
	m_pBitmap4Shape.DeleteObject();
	m_pBitmap4DTED.DeleteObject();
	m_pBitmap4Raster.DeleteObject();
	//m_pBitmap4Sym.DeleteObject();

	//m_dcSymLayer->DeleteDC();
	if (m_dcRasterLayer) m_dcRasterLayer->DeleteDC();
	if (m_dcDTEDLayer) m_dcDTEDLayer->DeleteDC();
	if (m_dcShpLayer) m_dcShpLayer->DeleteDC();
	if (m_dcMapLayer) m_dcMapLayer->DeleteDC();
	if (m_dcMainTemp) m_dcMainTemp->DeleteDC();

	if (m_dcMain && m_dcMain->GetSafeHdc())
		::ReleaseDC(this->GetSafeHwnd(), m_dcMain->Detach());
	//m_pOverlayThread->End();

	//SAFE_DELETE(m_dcSymLayer);
	SAFE_DELETE(m_dcRasterLayer);
	SAFE_DELETE(m_dcDTEDLayer);
	SAFE_DELETE(m_dcShpLayer);
	SAFE_DELETE(m_dcMapLayer);
	SAFE_DELETE(m_dcMainTemp);
	SAFE_DELETE(m_dcMain);

	CCoordMgr::ReleaseInst();

	//if (m_pMapDataMgr)
	//{
	//	delete m_pMapDataMgr;
	//	m_pMapDataMgr = NULL;
	//}

	//if (m_pMapDisplayMgr)
	//{
	//	delete m_pMapDisplayMgr;
	//	m_pMapDisplayMgr = NULL;
	//}

	//if (m_TmsMgr)
	//{
	//	delete m_TmsMgr;
	//	m_TmsMgr = NULL;

	//}
	//if (m_pMapConfigMgr)
	//{
	//	delete m_pMapConfigMgr;
	//	m_pMapConfigMgr = NULL;
	//}

	//SAFE_DELETE(m_pOverlayThread);
	//SAFE_DELETE(m_pDlgOverlayControler);
	//SAFE_DELETE(m_pDlgObjectList);
	//SAFE_DELETE(m_pDlgOverlayProperty);
	//SAFE_DELETE(m_pDlgOverlayTextEdit);
	//SAFE_DELETE(m_pDlgOverlayEditObject);
	//SAFE_DELETE(m_pDlgOverlayEditObjectPoint);


}

// viewer 시작
BOOL CC4IMapView::BeginView()
{
	//m_pathConfig.SetWork(FILEPATH_CONFIG);

	// 투명도 관련 대화상자 생성
	//m_pDlgOverlayControler = new CDlgOverlayControler(this);
	//if (m_pDlgOverlayControler != NULL)
	//{
	//	// 투명도
	//	m_pDlgOverlayControler->Create(CDlgOverlayControler::IDD, this);
	//}
	 
	//m_pDlgObjectList = new CDlgObjectList(this);
	//if (m_pDlgObjectList != NULL)
	{
		//m_pDlgObjectList->Create(CDlgObjectList::IDD, this);
 		//m_pOverlayThread = new COverlayThread(this);
		//m_pDlgOverlayControler->BindOverlay(m_pOverlayThread, m_pDlgObjectList);
		//m_pOverlayThread->SetViewPointer(this);
	}
/*
	m_pDlgOverlayProperty = new CDlgOverlayProperty(this, m_pOverlayThread->GetOverlayView());
	if (m_pDlgOverlayProperty != NULL)
		m_pDlgOverlayProperty->Create(CDlgOverlayProperty::IDD, this);

	m_pDlgOverlayTextEdit = new CDlgOverlayTextEdit(this);
	if (m_pDlgOverlayTextEdit != NULL)
	{
		m_pDlgOverlayTextEdit->Create(CDlgOverlayTextEdit::IDD, this);
		m_pDlgOverlayTextEdit->BindOverlayView(m_pOverlayThread->GetOverlayView());
	}

	m_pDlgOverlayEditObject = new CDlgOverlayEditObject(this);
	if (m_pDlgOverlayEditObject != NULL)
		m_pDlgOverlayEditObject->Create(CDlgOverlayEditObject::IDD, this);

	m_pDlgOverlayEditObjectPoint = new CDlgOverlayEditObjectPoint(this);
	if (m_pDlgOverlayEditObjectPoint != NULL)
		m_pDlgOverlayEditObjectPoint->Create(CDlgOverlayEditObjectPoint::IDD, this);
*/
	//m_adjObj = CObjectProp::GetInst();
	//m_adjObj->SetWnd(this);

	//m_TmsMgr = new CTmsMgr();

#if C4MAP_ENABLE_BASEMAP
	m_TmsMgr.Initalize();
#endif
    SetTimer(ID_MAP_SOURCES_XML,150,nullptr);

	return TRUE;
}

BOOL CC4IMapView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: CREATESTRUCT cs를 수정하여 여기에서
	//  Window 클래스 또는 스타일을 수정합니다.

	return CView::PreCreateWindow(cs);
}

// CC4IMapView 그리기

void CC4IMapView::OnDraw(CDC* pDC)
{
	CC4IMapDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: 여기에 원시 데이터에 대한 그리기 코드를 추가합니다.

}


// CC4IMapView 인쇄


void CC4IMapView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

BOOL CC4IMapView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// 기본적인 준비
	return DoPreparePrinting(pInfo);
}

void CC4IMapView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 인쇄하기 전에 추가 초기화 작업을 추가합니다.
}

void CC4IMapView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 인쇄 후 정리 작업을 추가합니다.
}


void CC4IMapView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CC4IMapView 진단

#ifdef _DEBUG
void CC4IMapView::AssertValid() const
{
	CView::AssertValid();
}

void CC4IMapView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CC4IMapDoc* CC4IMapView::GetDocument() const // 디버그되지 않은 버전은 인라인으로 지정됩니다.
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CC4IMapDoc)));
	return (CC4IMapDoc*)m_pDocument;
}
#endif //_DEBUG


// CC4IMapView 메시지 처리기

BOOL CC4IMapView::OnEraseBkgnd(CDC*)
{
    // DrawSituation presents a complete buffered frame; keep the last frame while rendering.
    return TRUE;
}

void CC4IMapView::OnPaint()
{
	CPaintDC dc(this); // device context for painting
					   // TODO: 여기에 메시지 처리기 코드를 추가합니다.
					   // 그리기 메시지에 대해서는 CView::OnPaint()을(를) 호출하지 마십시오.

	DrawSituation(FALSE, TRUE, TRUE, TRUE);

	//if (!m_bDCInitialized)
	//{
	//	DrawSituation(FALSE, TRUE, TRUE, TRUE);
	//	m_bDCInitialized = TRUE;
	//}
	//DrawSituation(FALSE, FALSE, FALSE, FALSE);
}

void CC4IMapView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo)
{
	// TODO: 여기에 특수화된 코드를 추가 및/또는 기본 클래스를 호출합니다.
	pDC->SetMapMode(MM_ANISOTROPIC);

	CView::OnPrepareDC(pDC, pInfo);
}

BOOL CC4IMapView::HitTest(double fXCoord, double fYCoord, double fCenterX, double fCenterY, double fScale)
{
	if (HitestDisplayPoint(fXCoord, fYCoord, fCenterX, fCenterY, fScale)) 
		return TRUE;
	//if (HitestSymbol(fXCoord, fYCoord, fCenterX, fCenterY, fScale)) return TRUE;
	//if (HitestSpecialPoint(fXCoord, fYCoord, fCenterX, fCenterY, fScale)) return TRUE;
	//if (HitestTrack(fXCoord, fYCoord, fCenterX, fCenterY, fScale)) return TRUE;


	m_nHookType = HOOKED_GROUND;

	return FALSE;
}

CMainFrame* CC4IMapView::get_mainfrm()
{
	return (CMainFrame*)AfxGetMainWnd();
}


void CC4IMapView::ZoomAtPoint(CPoint point, bool zoomIn)
{
    if (!m_pCoordMgr || !m_bDCInitialized)
        return;
    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty() || !client.PtInRect(point))
        return;

    const double oldScale = m_pCoordMgr->m_fScale;
    if (!(oldScale > 0.0) || oldScale > 1.0e12)
        return;
    double newScale = zoomIn ? oldScale * 1.5 : oldScale / 1.5;
    newScale = (std::max)(0.000001, (std::min)(2500.0, newScale));
    const double ratio = newScale / oldScale;
    const CPoint center = client.CenterPoint();

    // Screen = origin + projected coordinate * scale (also after rotation).
    // Move the selected map point to the viewport center at the new scale.
    m_fPrevScale = oldScale;
    m_pCoordMgr->m_fCenterX = center.x - (point.x - m_pCoordMgr->m_fCenterX) * ratio;
    m_pCoordMgr->m_fCenterY = center.y - (point.y - m_pCoordMgr->m_fCenterY) * ratio;
    m_pCoordMgr->m_fScale = newScale;
    Invalidate(FALSE);
}

void CC4IMapView::ZoomIn()
{
    CRect client;
    GetClientRect(&client);
    ZoomAtPoint(client.CenterPoint(), true);
}


void CC4IMapView::ZoomOut()
{
    CRect client;
    GetClientRect(&client);
    ZoomAtPoint(client.CenterPoint(), false);
}

void CC4IMapView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (nType == SIZE_MINIMIZED || cx <= 0 || cy <= 0 || m_pCoordMgr == nullptr)
        return;

    const int previousWidth = m_pCoordMgr->m_nDisplayViewWidth;
    const int previousHeight = m_pCoordMgr->m_nDisplayViewHeight;
    GetWindowRect(&m_recSave);
    const CRect client(0, 0, cx, cy);

    double centerX, centerY;
    m_pCoordMgr->GetDisplayCenter(&centerX, &centerY);
    if (m_bDCInitialized)
    {
        m_pCoordMgr->m_fCenterX = centerX + (cx - previousWidth) * 0.5;
        m_pCoordMgr->m_fCenterY = centerY + (cy - previousHeight) * 0.5;
    }

    m_pCoordMgr->m_nDisplayViewWidth = cx;
    m_pCoordMgr->m_nDisplayViewHeight = cy;
    m_pCoordMgr->m_fHorVerRatio = static_cast<double>(cx) / cy;
    m_pCoordMgr->InitDisplayParameter(client);

    if (!m_bDCInitialized)
        return;

    ReInitializeDCs();
    m_mapFrameCache.InvalidateFrom();

    // Schedule a buffered repaint after layout completes. Do not steal focus
    // from docked controls or paint synchronously inside WM_SIZE.
    Invalidate(FALSE);
}


void CC4IMapView::OnSetFocus(CWnd* pOldWnd)
{
	CView::OnSetFocus(pOldWnd);

	// TODO: 여기에 메시지 처리기 코드를 추가합니다.
}

void CC4IMapView::DrawZoomRect(CDC* pDC, CRect& pRect)
{
	CPen pen, * pOldPen;
	pen.CreatePen(PS_DOT, 0, RGB(0, 0, 0));

	pOldPen = pDC->SelectObject(&pen);
	pDC->SetROP2(R2_XORPEN);
	pDC->SelectStockObject(NULL_BRUSH);
	pDC->Rectangle(&pRect);

	if (abs(pRect.Width()) > 15 && abs(pRect.Height()) > 15) 
	{
		pDC->MoveTo(pRect.CenterPoint().x - 15, pRect.CenterPoint().y);
		pDC->LineTo(pRect.CenterPoint().x + 15, pRect.CenterPoint().y);
		pDC->MoveTo(pRect.CenterPoint().x, pRect.CenterPoint().y - 15);
		pDC->LineTo(pRect.CenterPoint().x, pRect.CenterPoint().y + 15);
	}

	pDC->SetROP2(R2_COPYPEN);
	pDC->SelectObject(pOldPen);
}

LRESULT CC4IMapView::OnOptionChange(WPARAM pText, LPARAM nFlag)
{
	return 0;
}

void CC4IMapView::OnBtnMapgenerater()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
	/*
	m_generateDlg = new CMapGeneratorDlg(this);
	if (m_generateDlg != NULL)
	{
		m_generateDlg->Create(IDD_MAP_GENERATOR, this);
		m_generateDlg->ShowWindow(true);
	}
	*/
}


void CC4IMapView::OnLocalSettings()
{
	if(!C4MAP_ENABLE_LOCAL)
		return;
	
	get_mainfrm()->m_indexMap.ConfigureLocal(this);
	get_mainfrm()->m_wndMapObjectManager.RefreshLocalShapeTree();
	DrawSituation(TRUE,TRUE,TRUE,TRUE);
}

void CC4IMapView::OnLocalToggle()
{
	get_mainfrm()->m_indexMap.ToggleLocal();
	DrawSituation(TRUE,TRUE,TRUE,TRUE);
}

void CC4IMapView::OnLocalIdentify()
{
	ShowLocalFeature(m_localClick);
}

void CC4IMapView::ShowLocalFeature(CPoint point)
{
    CString title;std::vector<CString> fields,values;
    auto frame=get_mainfrm();
    MapSources().CancelIdentify();
    frame->m_wndMapProperties.ClearPropList();
    if((C4MAP_SHAPE_MODE!=2 && frame->m_indexMap.IdentifyLocal(point,title,fields,values)) || 
		MapSources().Identify(point,m_pCoordMgr,title,fields,values))
	{
        frame->m_wndMapProperties.SetSearchFeatureInfo(title,fields,values);
        frame->m_wndMapProperties.ShowPane(TRUE,FALSE,TRUE);
    }
	else 
	{
		frame->m_wndMapProperties.SetSearchFeatureInfo(MapSources().IdentifyPending()?_T("서버 속성 조회 중..."):_T("선택된 객체 없음"),fields,values);
	}
    DrawSituation(TRUE,TRUE,TRUE,TRUE);
}

void CC4IMapView::OnLocalOverlayCommand(UINT id)
{
    SetMouseState(MOUSE_DEFAULT);
    m_analysis.Cancel();
    m_localOverlay.Cancel();
    if(GetCapture()==this)
		ReleaseCapture();

    m_bPanning=FALSE;
	m_bMapMove=FALSE;
	m_bOnZoom=FALSE;
	mb_moving=FALSE;

    if(id>=ID_C4OVER_PAN && id<=ID_C4OVER_ELLIPSE)
		m_localOverlay.SetTool(id-ID_C4OVER_PAN);
    else if(id==ID_C4OVER_COLOR)
	{
		CColorDialog dlg(m_localOverlay.Color(),CC_FULLOPEN,this);
		if(dlg.DoModal()==IDOK)
			m_localOverlay.SetColor(dlg.GetColor());
	}
    else if(id==ID_C4OVER_UNDO)
		m_localOverlay.Undo();
    else if(id==ID_C4OVER_REDO) 
		m_localOverlay.Redo();
    else if(id==ID_C4OVER_SAVE || id==ID_C4OVER_LOAD)
	{
        BOOL load=id==ID_C4OVER_LOAD;
        CFileDialog dlg(load,_T("xml"),_T("overlay.xml"),OFN_HIDEREADONLY|(load?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT),_T("C4IMap Overlay XML (*.xml)|*.xml||"),this);
        if(dlg.DoModal()==IDOK)
		{
            if(load && !m_localOverlay.Layers().empty() && AfxMessageBox(_T("현재 Overlay 도형을 선택한 파일로 교체하시겠습니까? \
				저장하지 않은 도형은 사라집니다."),MB_YESNO|MB_ICONQUESTION)!=IDYES)
				return;
            CString error;
			bool ok=load?m_localOverlay.Load(dlg.GetPathName(),error):m_localOverlay.Save(dlg.GetPathName(),error);
            if(!ok)
				AfxMessageBox(error,MB_ICONERROR);
        }
    }
    RefreshOverlayLayers();
    Invalidate(FALSE);
}

void CC4IMapView::OnUpdateLocalOverlay(CCmdUI* ui)
{
    ui->Enable(ui->m_nID==ID_C4OVER_REDO ? m_localOverlay.CanRedo() : (ui->m_nID!=ID_C4OVER_UNDO || m_localOverlay.CanUndo()));
    if(ui->m_nID<=ID_C4OVER_ELLIPSE)
		ui->SetRadio(m_localOverlay.GetTool()==int(ui->m_nID-ID_C4OVER_PAN));
}

BOOL CC4IMapView::PreTranslateMessage(MSG* msg)
{
    if (msg->message == WM_KEYDOWN && msg->hwnd == GetSafeHwnd())
    {
        if (msg->wParam == VK_ESCAPE && (m_analysis.Active() || m_analysis.Busy()))
        {
            m_analysis.Cancel();
            Invalidate(FALSE);
            return TRUE;
        }
        if (msg->wParam == VK_RETURN && m_analysis.Finish())
        {
            Invalidate(FALSE);
            return TRUE;
        }
        bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        UINT command = 0;
        if (msg->wParam == VK_DELETE)
            command = ID_MENU_OVERLAY_DELETE;
        else if (control && msg->wParam == 'C')
            command = ID_MENU_OVERLAY_COPY;
        else if (control && msg->wParam == 'X')
            command = ID_MENU_OVERLAY_CUT;
        else if (control && msg->wParam == 'V')
            command = ID_MENU_OVERLAY_PASTE;
        else if (control && (msg->wParam == 'Z' || msg->wParam == 'Y'))
        {
            OnLocalOverlayCommand(msg->wParam == 'Z' ? ID_C4OVER_UNDO : ID_C4OVER_REDO);
            return TRUE;
        }
        if (command)
        {
            OnOverlayEdit(command);
            return TRUE;
        }
    }
    if(msg->message==WM_KEYDOWN && msg->wParam==VK_ESCAPE && 
		(m_localOverlay.IsDrawing()||m_localOverlay.GetTool()!=CLocalOverlay::Pan))
	{
        m_localOverlay.SetTool(CLocalOverlay::Pan);
		if(GetCapture()==this)
			ReleaseCapture();
		Invalidate(FALSE);
		return TRUE;
    }
    return CView::PreTranslateMessage(msg);
}

void CC4IMapView::OnCaptureChanged(CWnd* wnd)
{
    m_rotationDragging=false;
    if(m_localOverlay.IsDrawing())
	{
		m_localOverlay.Cancel();
		Invalidate(FALSE);
	}
    CView::OnCaptureChanged(wnd);
}

BOOL CC4IMapView::SaveLocalOverlayIfModified()
{
    if(!m_localOverlay.IsModified())
		return TRUE;
    int answer=AfxMessageBox(_T("Overlay 도형 변경 내용을 XML로 저장하시겠습니까?"),MB_YESNOCANCEL|MB_ICONQUESTION);
    if(answer==IDCANCEL)
		return FALSE;
    if(answer==IDNO)
		return TRUE;
    OnLocalOverlayCommand(ID_C4OVER_SAVE);
    return !m_localOverlay.IsModified();
}

void CC4IMapView::OnMapSourcesXml()
{
	MapSources().Configure(this);
    m_mapFrameCache.InvalidateFrom(CMapFrameCache::Sources);
	get_mainfrm()->m_wndMapObjectManager.RefreshLocalShapeTree();
	Invalidate(FALSE);
}

void CC4IMapView::OnTimer(UINT_PTR id)
{
    if (id == ID_MAP_SOURCES_XML)
    {
        if (m_analysis.Poll())
            Invalidate(FALSE);
        std::vector<AnalysisProfileSample> profile;
        CString error;
        if (m_analysis.TakeProfile(profile))
        {
            if (!CTerrainProfileDialog::Open(profile, this))
                AfxMessageBox(_T("단면도 창을 만들지 못했습니다."));
        }
        else if (m_analysis.TakeError(error))
        {
            AfxMessageBox(error, MB_OK | MB_ICONEXCLAMATION);
        }
    }
    if(id== ID_MAP_SOURCES_XML)
    {
        CString title;std::vector<CString> fields,values;
        if(MapSources().PollIdentify(title,fields,values))
        {
            auto frame=get_mainfrm();
            frame->m_wndMapProperties.ClearPropList();
            frame->m_wndMapProperties.SetSearchFeatureInfo(title,fields,values);
            frame->m_wndMapProperties.ShowPane(TRUE,FALSE,TRUE);
        }
    }
    if(id== ID_MAP_SOURCES_XML && (TerrainLayer().Poll() | TerrainLocalLayer().Poll() | TerrainServerLayer().Poll()))
	{
		m_mapFrameCache.InvalidateFrom(CMapFrameCache::Elevation);
        CPoint cursor;GetCursorPos(&cursor);
		ScreenToClient(&cursor);
		CRect area;
		GetClientRect(&area);
		if(area.PtInRect(cursor))
			UpdateStatusBar(cursor);
		Invalidate(FALSE);
	}
	if(id== ID_MAP_SOURCES_XML && MapSources().Poll())
	{
		m_mapFrameCache.InvalidateFrom(CMapFrameCache::Sources);
        get_mainfrm()->SetMessageText(MapSources().Status());
		Invalidate(FALSE);
	}
	CView::OnTimer(id);
}

void CC4IMapView::RefreshOverlayLayers()
{
    auto frame=DYNAMIC_DOWNCAST(CMainFrame,AfxGetMainWnd());
	if(frame&&frame->m_wndCOPLayerView.GetSafeHwnd())
		frame->m_wndCOPLayerView.RefreshMsgList();
}
void CC4IMapView::OnOverlayLayerNew()
{
    m_localOverlay.Cancel();
	if(GetCapture()==this)
		ReleaseCapture();
    if(!m_localOverlay.AddLayer())
	{
		AfxMessageBox(_T("레이어를 추가할 수 없습니다."));
		return;
	}
    RefreshOverlayLayers();
	auto frame=DYNAMIC_DOWNCAST(CMainFrame,AfxGetMainWnd());
    if(frame)
		frame->m_wndCOPLayerView.ShowPane(TRUE,FALSE,TRUE);
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
	 auto frame=get_mainfrm();
	 if(!frame||!frame->m_pRibbonDrawGallery)
		 return;

	 int index=frame->m_pRibbonDrawGallery->GetSelectedItem();
	 CString name=frame->GetOverlayDrawItem(index);
	 UINT command=0;
	 if(name==_T("직선"))
		 command=ID_C4OVER_LINE;
	 else if(name==_T("화살표 직선"))
		 command=ID_C4OVER_ARROW;
	 else if(name==_T("사각형"))
		 command=ID_C4OVER_RECT;
	 else if(name==_T("타원")||name==_T("원"))
		 command=ID_C4OVER_ELLIPSE;
	 if(command)
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

void CC4IMapView::OnDisplayProjection(UINT id)
{
	 if(int(id-ID_MENU_PROJECTION_GEO)==DisplayProjection().Kind())
		 return;

	 m_localOverlay.Cancel();
	 if(GetCapture()==this)
		 ReleaseCapture();
	 if(!DisplayProjection().Set(m_pCoordMgr,int(id-ID_MENU_PROJECTION_GEO)))
	 {
		 AfxMessageBox(_T("현재 지도 중심에서 이 투영을 적용할 수 없습니다. UTM은 남위 80~북위 84도, \
				Mercator는 남북위 85도 안에서 선택하세요."));
		 return;
	 }

	 Invalidate(FALSE);
}

void CC4IMapView::OnUpdateDisplayProjection(CCmdUI* ui)
{
	ui->Enable(m_pCoordMgr!=nullptr);
	if(ui->m_nID!=ID_MENU_PROJECTION)
		ui->SetRadio(int(ui->m_nID-ID_MENU_PROJECTION_GEO)==DisplayProjection().Kind());
}

void CC4IMapView::OnMapRotate(){
 m_localOverlay.Cancel();
 if(DisplayProjection().SetAngle(m_pCoordMgr,0))Invalidate(FALSE);
 get_mainfrm()->SetMessageText(_T("휠 버튼을 누르고 좌우로 드래그하면 회전합니다. 회전 버튼은 북쪽 위로 초기화합니다."));
}
void CC4IMapView::OnUpdateMapRotate(CCmdUI* ui){ui->Enable(m_pCoordMgr!=nullptr);}
