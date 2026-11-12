#include "pch.h"
#include "resource.h"
#include "C4IMap.h"
#include "MainFrm.h"
#include "C4IMapDoc.h"
#include "C4IMapView.h"
#include "LayerView.h"



#define COPLAYER_NAME 0
#define COPLAYER_ITEMSIZE 1
#define COPLAYER_TYPE 2

int _listCtrl_AddItems(CListCtrl* plistCtrl, VEC_STR& items);
int _listCtrl_AddItems(CListCtrl* plistCtrl, VEC_STR& items)
{
	CHeaderCtrl* pHeader = plistCtrl->GetHeaderCtrl();
	int nCol = pHeader->GetItemCount();

	if (nCol > 0)
	{
		LVCOLUMN lvCol;
		lvCol.mask = LVCF_WIDTH;


		int itemSize = (int)items.size();
		int nRow;
		if (itemSize < nCol)
			nRow = 1;
		else
			nRow = itemSize / nCol;

		int sequence = 0;
		int currRow = plistCtrl->GetItemCount();
		for (VEC_STR_iter iter = items.begin(); iter != items.end(); iter++, sequence++)
		{
			if (iter == items.begin())
			{
				currRow = plistCtrl->InsertItem(LVIF_TEXT, currRow, (*iter), 0, 0, 0, NULL);
			}
			else
				plistCtrl->SetItemText(currRow, sequence, (*iter));
		}
		return currRow;
	}
	return -1;
}

class CLayerViewMenuButton : public CMFCToolBarMenuButton
{
	friend class CLayerView;

	DECLARE_SERIAL(CLayerViewMenuButton)

public:
	CLayerViewMenuButton(HMENU hMenu = NULL) : CMFCToolBarMenuButton((UINT)-1, hMenu, -1)
	{
	}

	virtual void OnDraw(CDC* pDC, const CRect& rect, CMFCToolBarImages* pImages, BOOL bHorz = TRUE,
		BOOL bCustomizeMode = FALSE, BOOL bHighlight = FALSE, BOOL bDrawBorder = TRUE, BOOL bGrayDisabledButtons = TRUE)
	{
		pImages = CMFCToolBar::GetImages();

		CAfxDrawState ds;
		pImages->PrepareDrawImage(ds);

		CMFCToolBarMenuButton::OnDraw(pDC, rect, pImages, bHorz, bCustomizeMode, bHighlight, bDrawBorder, bGrayDisabledButtons);

		pImages->EndDrawImage(ds);
	}
};

IMPLEMENT_SERIAL(CLayerViewMenuButton, CMFCToolBarMenuButton, 1)

//////////////////////////////////////////////////////////////////////
// 생성/소멸
//////////////////////////////////////////////////////////////////////

CLayerView::CLayerView()
{
	m_nCurrSort = ID_SORTING_GROUPBYTYPE_COP;
	//m_pMsgBaseList = NULL;
}

CLayerView::~CLayerView()
{
}

BEGIN_MESSAGE_MAP(CLayerView, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_COP_ADD_MEMBER_FUNCTION, OnCOPAddMemberFunction)
	ON_COMMAND(ID_COP_ADD_MEMBER_VARIABLE, OnCOPAddMemberVariable)
	ON_COMMAND(ID_COP_DEFINITION, OnCOPDefinition)
	ON_COMMAND(ID_COP_PROPERTIES, OnCOPProperties)
	ON_COMMAND(ID_NEW_FOLDER_COP, OnNewFolder_COP)
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
	ON_COMMAND_RANGE(ID_SORTING_GROUPBYTYPE_COP, ID_SORTING_SORTBYACCESS_COP, OnSort)
	ON_UPDATE_COMMAND_UI_RANGE(ID_SORTING_GROUPBYTYPE_COP, ID_SORTING_SORTBYACCESS_COP, OnUpdateSort)

	ON_NOTIFY(LVN_ITEMCHANGED, IDC_COP_LISTCTRL_LAYER, &CLayerView::OnLvnItemchangedCopLayer)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_COP_LISTCTRL_LAYER, &CLayerView::OnOverlayLabelEdited)
	ON_NOTIFY(NM_DBLCLK, IDC_COP_LISTCTRL_LAYER, &CLayerView::OnNMDblclkListDebugDlg)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CClassView 메시지 처리기

int CLayerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// 뷰를 만듭니다.
	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_EDITLABELS | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	if (!m_wndCOPView.Create(dwViewStyle, rectDummy, this, IDC_COP_LISTCTRL_LAYER))
	{
		TRACE0("항목 뷰를 만들지 못했습니다.\n");
		return -1;      // 만들지 못했습니다.
	}

	DWORD dwExStyle = m_wndCOPView.GetExtendedStyle();
	m_wndCOPView.SetExtendedStyle(dwExStyle | LVS_EX_CHECKBOXES | LVS_EX_BORDERSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER /*| LVS_OWNERDATA*/);

	m_wndCOPView.InsertColumn(COPLAYER_NAME, _T("레이어 명칭"), LVCFMT_LEFT, 160);
	m_wndCOPView.InsertColumn(COPLAYER_ITEMSIZE, _T("도형 개수"), LVCFMT_CENTER, 80);
	m_wndCOPView.InsertColumn(COPLAYER_TYPE, _T("레이어 형식"), LVCFMT_LEFT, 100);
	//m_wndCOPView.EnableGroupView(TRUE);
	m_wndCOPView.ModifyStyle(LVS_TYPEMASK, LVS_REPORT);

	// 이미지를 로드합니다.
	m_wndCOPToolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_SORT_COP);
	m_wndCOPToolBar.LoadToolBar(IDR_SORT_COP, 0, 0, TRUE /* 잠금 */);

	OnChangeVisualStyle();

	m_wndCOPToolBar.SetPaneStyle(m_wndCOPToolBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
	m_wndCOPToolBar.SetPaneStyle(m_wndCOPToolBar.GetPaneStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));

	m_wndCOPToolBar.SetOwner(this);

	// 모든 명령은 부모 프레임이 아닌 이 컨트롤을 통해 라우팅됩니다.
	m_wndCOPToolBar.SetRouteCommandsViaFrame(FALSE);

	CMenu menuSort;
	menuSort.LoadMenu(IDR_POPUP_SORT_COP);

	m_wndCOPToolBar.ReplaceButton(ID_SORT_MENU_COP, CLayerViewMenuButton(menuSort.GetSubMenu(0)->GetSafeHmenu()));

	CLayerViewMenuButton* pButton = DYNAMIC_DOWNCAST(CLayerViewMenuButton, m_wndCOPToolBar.GetButton(0));

	if (pButton != NULL)
	{
		pButton->m_bText = FALSE;
		pButton->m_bImage = TRUE;
		pButton->SetImage(GetCmdMgr()->GetCmdImage(m_nCurrSort));
		pButton->SetMessageWnd(this);
	}

	// 정적 트리 뷰 데이터를 더미 코드로 채웁니다.
	FillClassView();
#ifdef _DEBUG
	TRACE("[CREATE VIEW] CLayerView OnCreate\n");
#endif // _DEBUG
	return 0;
}

void CLayerView::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CLayerView::FillClassView()
{

}

void CLayerView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	HINSTANCE hOrgInstance = AfxGetResourceHandle();
	AfxSetResourceHandle(AfxGetInstanceHandle());

	CListCtrl* pWndList = (CListCtrl*)&m_wndCOPView;
	ASSERT_VALID(pWndList);

	if (pWnd != pWndList)
	{
		CDockablePane::OnContextMenu(pWnd, point);
		AfxSetResourceHandle(hOrgInstance);
		return;
	}

	if (point != CPoint(-1, -1))
	{
		// 클릭한 항목을 선택합니다.
		CPoint ptList = point;
		pWndList->ScreenToClient(&ptList);

		UINT flags = 0;
		int nHit = pWndList->HitTest(ptList, &flags);
		if (nHit != -1)
		{
			pWndList->SetHotItem(nHit);
		}
	}

	pWndList->SetFocus();
	CMenu menu;
	if (menu.LoadMenu(IDR_POPUP_SORT_COP))
	{
		CMenu* pSumMenu = menu.GetSubMenu(0);

		if (AfxGetMainWnd()->IsKindOf(RUNTIME_CLASS(CFrameWndEx)))
		{
			CMFCPopupMenu* pPopupMenu = new CMFCPopupMenu;

			if (!pPopupMenu->Create(this, point.x, point.y, (HMENU)pSumMenu->m_hMenu, FALSE, TRUE))
				return;

			((CFrameWndEx*)AfxGetMainWnd())->OnShowPopupMenu(pPopupMenu);
			UpdateDialogControls(this, FALSE);
		}
	}
	AfxSetResourceHandle(hOrgInstance);
}

void CLayerView::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	int cyTlb = m_wndCOPToolBar.CalcFixedLayout(FALSE, TRUE).cy;

	m_wndCOPToolBar.SetWindowPos(NULL, rectClient.left, rectClient.top, rectClient.Width(), cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);
	m_wndCOPView.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + cyTlb + 1, rectClient.Width() - 2, rectClient.Height() - cyTlb - 2, SWP_NOACTIVATE | SWP_NOZORDER);

}

BOOL CLayerView::PreTranslateMessage(MSG* pMsg)
{
	return CDockablePane::PreTranslateMessage(pMsg);
}

void CLayerView::OnSort(UINT id)
{
	if (m_nCurrSort == id)
	{
		return;
	}

	m_nCurrSort = id;

	CLayerViewMenuButton* pButton = DYNAMIC_DOWNCAST(CLayerViewMenuButton, m_wndCOPToolBar.GetButton(0));

	if (pButton != NULL)
	{
		pButton->SetImage(GetCmdMgr()->GetCmdImage(id));
		m_wndCOPToolBar.Invalidate();
		m_wndCOPToolBar.UpdateWindow();
	}
}

void CLayerView::OnUpdateSort(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(pCmdUI->m_nID == m_nCurrSort);
}

void CLayerView::OnCOPAddMemberFunction()
{
	AfxMessageBox(_T("멤버 함수 추가..."));
}

void CLayerView::OnCOPAddMemberVariable()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
}

void CLayerView::OnCOPDefinition()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
}

void CLayerView::OnCOPProperties()
{
	// TODO: 여기에 명령 처리기 코드를 추가합니다.
}

void CLayerView::OnNewFolder_COP()
{
	AfxMessageBox(_T("새 폴더..."));
}

void CLayerView::OnPaint()
{
	CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

	CRect rectList;
	m_wndCOPView.GetWindowRect(rectList);
	ScreenToClient(rectList);

	rectList.InflateRect(1, 1);
	dc.Draw3dRect(rectList, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CLayerView::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);

	m_wndCOPView.SetFocus();
}

void CLayerView::OnChangeVisualStyle()
{
	m_COPViewImages.DeleteImageList();

	UINT uiBmpId = theApp.m_bHiColorIcons ? IDB_COP_VIEW_24 : IDB_COP_VIEW;

	CBitmap bmp;
	if (!bmp.LoadBitmap(uiBmpId))
	{
		TRACE(_T("비트맵을 로드할 수 없습니다. %x\n"), uiBmpId);
		ASSERT(FALSE);
		return;
	}

	BITMAP bmpObj;
	bmp.GetBitmap(&bmpObj);

	UINT nFlags = ILC_MASK;

	nFlags |= (theApp.m_bHiColorIcons) ? ILC_COLOR24 : ILC_COLOR4;

	m_COPViewImages.Create(16, bmpObj.bmHeight, nFlags, 0, 0);
	m_COPViewImages.Add(&bmp, RGB(255, 0, 0));

	m_wndCOPView.SetImageList(&m_COPViewImages, TVSIL_NORMAL);

	m_wndCOPToolBar.CleanUpLockedImages();
	m_wndCOPToolBar.LoadBitmap(theApp.m_bHiColorIcons ? IDB_SORT_COP_24 : IDR_SORT_COP, 0, 0, TRUE /* 잠금 */);
}

CListCtrl* CLayerView::GetView()
{
	return &m_wndCOPView;
}

//void CLayerView::SetMsgMgr(CMapXMsgProcBase_List* pMsgBaseList)
//{
//	m_pMsgBaseList = pMsgBaseList;
//}

void CLayerView::RefreshMsgList()
{
    if(!m_wndCOPView.GetSafeHwnd()||m_refreshingOverlay)
		return;
    auto frame=DYNAMIC_DOWNCAST(CMainFrame,AfxGetMainWnd());
    auto view=frame?DYNAMIC_DOWNCAST(CC4IMapView,frame->GetActiveView()):nullptr;
	if(!view)
		return;
    m_refreshingOverlay=true;
	m_wndCOPView.SetRedraw(FALSE);
	m_wndCOPView.DeleteAllItems();
    for(const auto& layer:view->OverlayLayers().Layers())
	{
        int row=m_wndCOPView.InsertItem(m_wndCOPView.GetItemCount(),layer.name);
		CString count;count.Format(_T("%llu"),(unsigned long long)layer.count);
        m_wndCOPView.SetItemText(row,COPLAYER_ITEMSIZE,count);
		m_wndCOPView.SetItemText(row,COPLAYER_TYPE,_T("투명도 (Overlay)"));
        m_wndCOPView.SetItemData(row,(DWORD_PTR)layer.id);
		m_wndCOPView.SetCheck(row,layer.visible);
        if(layer.id==view->OverlayLayers().ActiveLayer())
			m_wndCOPView.SetItemState(row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    m_wndCOPView.SetRedraw(TRUE);
	m_wndCOPView.Invalidate();
	m_refreshingOverlay=false;
}

void CLayerView::OnLvnItemchangedCopLayer(NMHDR* header,LRESULT* result)
{
    *result=0;
	if(m_refreshingOverlay)
		return;

	auto changed=(NMLISTVIEW*)header;
	if(changed->iItem<0)
		return;

    auto frame=DYNAMIC_DOWNCAST(CMainFrame,AfxGetMainWnd());
	auto view=frame?DYNAMIC_DOWNCAST(CC4IMapView,frame->GetActiveView()):nullptr;
	if(!view)
		return;

    int id=(int)m_wndCOPView.GetItemData(changed->iItem);
    if((changed->uNewState&LVIS_SELECTED) && !(changed->uOldState&LVIS_SELECTED))
		view->OverlayLayers().SelectLayer(id);
    UINT before = changed->uOldState&LVIS_STATEIMAGEMASK, after=changed->uNewState&LVIS_STATEIMAGEMASK;
    if(before && after && before != after)
		view->OverlayLayers().SetLayerVisible(id,m_wndCOPView.GetCheck(changed->iItem)!=FALSE);
    
	view->Invalidate(FALSE);
} 

void CLayerView::OnNMDblclkListDebugDlg(NMHDR* header,LRESULT* result)
{
	auto item=(NMITEMACTIVATE*)header;
	if(item->iItem>=0)
		m_wndCOPView.EditLabel(item->iItem);
	
	*result=0;
}

void CLayerView::OnOverlayLabelEdited(NMHDR* header,LRESULT* result)
{
    *result=FALSE;
	auto edit=(NMLVDISPINFO*)header;
	if(!edit->item.pszText || edit->item.iItem<0)
		return;
    auto frame=DYNAMIC_DOWNCAST(CMainFrame,AfxGetMainWnd());
	auto view=frame?DYNAMIC_DOWNCAST(CC4IMapView,frame->GetActiveView()):nullptr;
    if(view)
		*result=view->OverlayLayers().RenameLayer((int)m_wndCOPView.GetItemData(edit->item.iItem),edit->item.pszText)?TRUE:FALSE;
}

void CLayerView::msgBaseListPatrol(BOOL bPatrol)
{
/*
	if (m_pMsgBaseList)
	{
		CString strButtonNm;
		//GetDlgItemText(IDC_BTN_DRAWITEM_IMPO_PATRAL, strButtonNm); 
		if (bPatrol)
		{
			if (strButtonNm.CompareNoCase(_T("중요보기")) == 0)
				if (m_pMsgBaseList->copDrawItemImportentPatrol(TRUE))
				{
					//SetDlgItemText(IDC_BTN_DRAWITEM_IMPO_PATRAL, _T("순회중"));
				}
		}
		else
		{
			if (strButtonNm.CompareNoCase(_T("순회중")) == 0)
				if (!m_pMsgBaseList->copDrawItemImportentPatrol(FALSE))
				{
					//SetDlgItemText(IDC_BTN_DRAWITEM_IMPO_PATRAL, _T("중요보기"));
				}
		}
	}
*/
}
