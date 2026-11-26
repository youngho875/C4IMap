#include "pch.h"
#include "C4IMapViewHeader.h"
#include "OverlayStyleDialog.h"
#include "OverlayRibbonRender.h"
#include "OverlayTextRibbonEdit.h"

BOOL CC4IMapView::OnOverlayStyleCommand(UINT id)
{
    if(id==IDM_POPOVERLAY_PROPERTY && m_militaryOverlay.Selected()){OnMilitaryEdit();return TRUE;}
    if (m_localOverlay.IsDrawing())
        return TRUE;
    auto style = m_localOverlay.GetStyle();
    auto frame = get_mainfrm();
    if (!frame)
        return TRUE;
    bool dialog = false;
    if (id == ID_MENU_OVERLAY_LINE_COLOR || id == ID_MENU_OVERLAY_FILL_COLOR ||
        id == ID_MENU_OVERLAY_PATTERN_COLOR)
    {
        auto button = DYNAMIC_DOWNCAST(CMFCRibbonColorButton, frame->m_wndRibbonBar.FindByID(id));
        if (!button)
            return TRUE;
        COLORREF color = button->GetColor();
        if (id == ID_MENU_OVERLAY_LINE_COLOR)
        {
            style.lineEnabled = color != (COLORREF)-1;
            if (style.lineEnabled)
                style.lineColor = color;
        }
        else if (id == ID_MENU_OVERLAY_FILL_COLOR)
        {
            style.fillEnabled = color != (COLORREF)-1;
            if (style.fillEnabled)
                style.fillColor = color;
        }
        else
            style.patternColor = color == (COLORREF)-1 ? RGB(0, 0, 0) : color;
    }
    else if (id >= ID_MENU_OVERLAY_LINE_WIDTH_0 && id <= ID_MENU_OVERLAY_LINE_WIDTH_10)
    {
        style.width = id - ID_MENU_OVERLAY_LINE_WIDTH_0;
        style.lineEnabled = style.width > 0;
    }
    else if (id >= ID_MENU_OVERLAY_LINE_STYLE_0 && id <= ID_MENU_OVERLAY_LINE_STYLE_4)
        style.dash = id - ID_MENU_OVERLAY_LINE_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_FILL_TYPE_0 && id <= ID_MENU_OVERLAY_FILL_TYPE_4)
    {
        int type = id - ID_MENU_OVERLAY_FILL_TYPE_0;
        style.fillEnabled = type != 0;
        if (type)
            style.fillType = type;
    }
    else if (id >= ID_MENU_OVERLAY_HATCH_STYLE_0 && id <= ID_MENU_OVERLAY_HATCH_STYLE_52)
    {
        style.hatch = id - ID_MENU_OVERLAY_HATCH_STYLE_0;
        style.fillType = 2;
        style.fillEnabled = true;
    }
    else if (id == ID_MENU_OVERLAY_FILL_USE)
        style.fillEnabled = !style.fillEnabled;
    else if (id == ID_MENU_OVERLAY_LINE_WIDTH_COLOR)
    {
        const double widths[] = {0.75, 1, 1.5, 2.25, 3, 4.5, 6};
        int index = frame->m_pRibbonDrawShapeLineWidth->GetSelectedItem();
        if (index < 0 || index >= 7)
            return TRUE;
        style.width = widths[index];
        style.lineEnabled = true;
    }
    else if (id == ID_MENU_OVERLAY_LINE_DESH_COLOR)
    {
        int index = frame->m_pRibbonDrawShapeLineDesh->GetSelectedItem();
        if (index < 0 || index > 4)
            return TRUE;
        style.dash = index;
    }
    else if (id >= ID_MENU_OVERLAY_ARROW_STYLE_0 && id <= ID_MENU_OVERLAY_ARROW_STYLE_35)
        style.startArrow = id - ID_MENU_OVERLAY_ARROW_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_ENDARROW_STYLE_0 && id <= ID_MENU_OVERLAY_ENDARROW_STYLE_35)
        style.endArrow = id - ID_MENU_OVERLAY_ENDARROW_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_LINECAP_STYLE_0 && id <= ID_MENU_OVERLAY_LINECAP_STYLE_2)
        style.lineCap = id - ID_MENU_OVERLAY_LINECAP_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_LINEJOIN_STYLE_0 && id <= ID_MENU_OVERLAY_LINEJOIN_STYLE_3)
        style.lineJoin = id - ID_MENU_OVERLAY_LINEJOIN_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_LINEALIGNMENT_STYLE_0 &&
             id <= ID_MENU_OVERLAY_LINEALIGNMENT_STYLE_1)
        style.lineAlignment = id - ID_MENU_OVERLAY_LINEALIGNMENT_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_LINESTRIPE_STYLE_0 && id <= ID_MENU_OVERLAY_LINESTRIPE_STYLE_4)
        style.lineStripe = id - ID_MENU_OVERLAY_LINESTRIPE_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_GARDIENT_LINERMODE_0 &&
             id <= ID_MENU_OVERLAY_GARDIENT_LINERMODE_3)
        style.gradientMode = id - ID_MENU_OVERLAY_GARDIENT_LINERMODE_0;
    else if (id == 61050)
    {
        CFileDialog file(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                         _T("OLE 문서|*.*||"), this);
        if (file.DoModal() != IDOK)
            return TRUE;
        CString path, error;
        if (!OverlayRibbonRender::ImportOlePreview(file.GetPathName(), path, error))
        {
            AfxMessageBox(error);
            return TRUE;
        }
        wcsncpy_s(style.imagePath, CStringW(path), _TRUNCATE);
        style.fillType = 5;
        style.fillEnabled = true;
    }
    else if (id >= 61017 && id <= 61021)
    {
        style.fillEnabled = true;
        style.fillType = id == 61017 ? 1 : 4;
        if (id != 61017)
            style.gradientMode = id - 61018;
    }
    else if (id >= 61030 && id <= 61041)
    {
        int index = id - 61030;
        const COLORREF colors[] = {RGB(40, 110, 200), RGB(220, 70, 40), RGB(50, 150, 80),
                                   RGB(140, 70, 180)};
        style.lineColor = colors[index % 4];
        style.fillColor = colors[index % 4];
        style.lineEnabled = true;
        style.width = 2;
        style.dash = 0;
        style.fillEnabled = index >= 4;
        style.fillType = index >= 8 ? 4 : 1;
        style.fillAlpha = index < 8 ? 96 : 255;
        style.gradientColor = RGB(255, 255, 255);
    }
    else if (id >= 61011 && id <= 61016)
    {
        if (m_localOverlay.Arrange(id - 61004, GP_COORDMGR))
            Invalidate(FALSE);
        return TRUE;
    }
    else if (id >= 61007 && id <= 61010)
    {
        if (id == 61010)
            m_localOverlay.SelectAll();
        else if (!m_localOverlay.GroupSelection(id == 61008))
            AfxMessageBox(_T("Ctrl+클릭으로 두 개 이상의 도형을 선택하세요."));
        Invalidate(FALSE);
        return TRUE;
    }
    else if (id >= 61000 && id <= 61006)
    {
        if (m_localOverlay.Arrange(id - 61000, GP_COORDMGR))
            Invalidate(FALSE);
        return TRUE;
    }
    else if (id == ID_MENU_OVERLAY_GARDIENT_COLOR)
        style.gradientColor = frame->m_pRibbonDrawShapeGradientColor->GetColor() == (COLORREF)-1
                                  ? RGB(0, 0, 0)
                                  : frame->m_pRibbonDrawShapeGradientColor->GetColor();
    else if (id == ID_MENU_OVERLAY_GARDIENT_BLENDPOS)
    {
        // A numeric ribbon edit commits with Enter.
        auto edit = DYNAMIC_DOWNCAST(CMFCRibbonEdit, frame->m_wndRibbonBar.FindByID(id));
        if (edit)
            style.gradientPosition = _ttoi(edit->GetEditText());
    }
    else if (id == ID_MENU_OVERLAY_TEXT_STRING)
    {
        auto edit = DYNAMIC_DOWNCAST(COverlayTextRibbonEdit,
                                    frame->m_pRibbonDrawShapeTextString);
        if (edit)
            wcsncpy_s(style.text, edit->CommittedText(), _TRUNCATE);
        else
            wcsncpy_s(style.text, CA2W(frame->m_pRibbonDrawShapeTextString->GetEditText(),
                                     CP_ACP), _TRUNCATE);
    }
    else if (id == ID_MENU_OVERLAY_TEXT_KIND)
        wcsncpy_s(style.font, CA2W(frame->m_pRibbonDrawShapeTextKind->GetEditText(), CP_ACP),
                  _TRUNCATE);
    else if (id == ID_MENU_OVERLAY_TEXT_SIZE)
        style.fontSize = _ttoi(frame->m_pRibbonDrawShapeTextSize->GetEditText());
    else if (id == ID_MENU_OVERLAY_TEXT_LINEINTERVAL)
        style.lineInterval = _ttoi(frame->m_pRibbonDrawShapeTextLineInterval->GetEditText());
    else if (id == ID_MENU_OVERLAY_TEXT_COLOR)
        style.textColor = frame->m_pRibbonDrawShapeTextColor->GetColor() == (COLORREF)-1
                              ? RGB(0, 0, 0)
                              : frame->m_pRibbonDrawShapeTextColor->GetColor();
    else if (id >= ID_MENU_OVERLAY_TEXT_BOLD && id <= ID_MENU_OVERLAY_TEXT_STRIKEOUT)
        style.fontStyle ^= (1 << (id - ID_MENU_OVERLAY_TEXT_BOLD));
    else if (id >= ID_MENU_OVERLAY_TEXT_ALIGNLEFT && id <= ID_MENU_OVERLAY_TEXT_ALIGNRIGHT)
        style.textAlign = id - ID_MENU_OVERLAY_TEXT_ALIGNLEFT;
    else if (id >= ID_MENU_OVERLAY_TEXT_ALIGNTOP && id <= ID_MENU_OVERLAY_TEXT_ALIGNBOTTOM)
        style.textAlignY = id - ID_MENU_OVERLAY_TEXT_ALIGNTOP;
    else if (id == ID_MENU_OVERLAY_TEXT_VERTICAL)
        style.vertical = !style.vertical;
    else if (id == ID_MENU_OVERLAY_TEXT_RIGHTTOLEFT)
        style.rightToLeft = !style.rightToLeft;
    else if (id == ID_MENU_OVERLAY_TEXT_AUTOBOX)
        style.autoTextBox = !style.autoTextBox;
    else if (id == ID_MENU_OVERLAY_FILL_IMAGE || id == ID_RIBBON_OVERLAY_TEXTURE_ETC_GALLERY ||
             id == ID_RIBBON_OVERLAY_TEXTURE_GALLERY)
    {
        CFileDialog file(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                         _T("이미지 (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp|모든 파일|*.*||"), this);
        if (file.DoModal() != IDOK)
            return TRUE;
        CStringW path(file.GetPathName());
        Gdiplus::Image image(path);
        if (image.GetLastStatus() != Gdiplus::Ok)
        {
            AfxMessageBox(_T("이미지를 읽을 수 없습니다."));
            return TRUE;
        }
        wcsncpy_s(style.imagePath, path, _TRUNCATE);
        style.fillType = id == ID_MENU_OVERLAY_FILL_IMAGE ? 5 : 6;
        style.fillEnabled = true;
    }
    else if (id == ID_RIBBON_OVERLAY_QUICKSTYLE_GALLERY ||
             id == ID_RIBBON_OVERLAY_PATTERN_GALLERY ||
             id == ID_RIBBON_OVERLAY_GRADATION_GALLERY)
    {
        return TRUE; // Menu containers; child commands apply the chosen value.
    }
    else if (id == ID_RIBBON_OVERLAY_GRADATION_ETC_GALLERY)
    {
        style.fillType = 4;
        style.fillEnabled = true;
        dialog = true;
    }
    else
        dialog = true;
    if (dialog)
    {
        COverlayStyleDialog editor(style, m_localOverlay.HasSelection(), this);
        if (editor.Show() != IDOK)
            return TRUE;
        style = editor.Style();
    }
    if (!CLocalOverlay::ValidStyle(style))
    {
        AfxMessageBox(_T("입력값 범위를 확인하세요. 글크기 1~300, 줄간격 10~1000, 그라데이션 위치 ")
                      _T("1~99입니다."));
        return TRUE;
    }
    const auto previous = m_localOverlay.GetStyle();
    const bool textProperty = id == ID_MENU_OVERLAY_TEXT_STRING ||
        id == ID_MENU_OVERLAY_TEXT_KIND || id == ID_MENU_OVERLAY_TEXT_SIZE ||
        id == ID_MENU_OVERLAY_TEXT_LINEINTERVAL;
    if (textProperty && wcscmp(previous.text, style.text) == 0 &&
        wcscmp(previous.font, style.font) == 0 && previous.fontSize == style.fontSize &&
        previous.lineInterval == style.lineInterval)
        return TRUE;
    if (m_localOverlay.SetStyle(style))
    {
        Invalidate(FALSE);
        if (textProperty)
            UpdateWindow();
    }
    return TRUE;
}
void CC4IMapView::OnUpdateOverlayStyle(CCmdUI *ui)
{
    ui->Enable(!m_localOverlay.IsDrawing() &&
               (ui->m_nID < 61000 || ui->m_nID > 61006 || m_localOverlay.HasSelection()));
    auto s = m_localOverlay.GetStyle();
    auto frame = get_mainfrm();
    if (frame)
    {
        auto edit = DYNAMIC_DOWNCAST(CMFCRibbonEdit, frame->m_wndRibbonBar.FindByID(ui->m_nID));
        // IsFocused describes the ribbon element, not its native RichEdit child.
        // Never synchronize model text into an editor while the user is typing
        // or composing IME text, including editors hosted in a ribbon popup.
        CWnd* focus = CWnd::GetFocus();
        CMFCPopupMenu* popup = CMFCPopupMenu::GetActiveMenu();
        const bool editingRibbon = focus &&
            (frame->m_wndRibbonBar.IsChild(focus) || (popup && popup->IsChild(focus)));
        // Spin controls still post their MFC command; preserve its pending value.
        MSG pendingMessage = {};
        const bool pendingCommand = ::PeekMessage(&pendingMessage, frame->GetSafeHwnd(),
            WM_COMMAND, WM_COMMAND, PM_NOREMOVE) != FALSE;
        if (edit && !pendingCommand && !editingRibbon && !edit->HasFocus() &&
            !edit->IsFocused() && !edit->IsDroppedDown())
        {
            CString value;
            bool update = true;
            switch (ui->m_nID)
            {
            case ID_MENU_OVERLAY_TEXT_STRING:
                value = CW2A(s.text, CP_ACP);
                break;
            case ID_MENU_OVERLAY_TEXT_KIND:
                value = CW2A(s.font, CP_ACP);
                break;
            case ID_MENU_OVERLAY_TEXT_SIZE:
                value.Format(_T("%d"), s.fontSize);
                break;
            case ID_MENU_OVERLAY_TEXT_LINEINTERVAL:
                value.Format(_T("%d"), s.lineInterval);
                break;
            case ID_MENU_OVERLAY_GARDIENT_BLENDPOS:
                value.Format(_T("%d"), s.gradientPosition);
                break;
            default:
                update = false;
                break;
            }
            if (update && edit->GetEditText() != value)
                edit->SetEditText(value);
        }
    }

    if (ui->m_nID == ID_MENU_OVERLAY_FILL_USE)
        ui->SetCheck(s.fillEnabled);
    else if (ui->m_nID >= ID_MENU_OVERLAY_LINE_WIDTH_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_LINE_WIDTH_10)
        ui->SetCheck(s.width == ui->m_nID - ID_MENU_OVERLAY_LINE_WIDTH_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_LINE_STYLE_0 && ui->m_nID <= ID_MENU_OVERLAY_LINE_STYLE_4)
        ui->SetCheck(s.dash == ui->m_nID - ID_MENU_OVERLAY_LINE_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_FILL_TYPE_0 && ui->m_nID <= ID_MENU_OVERLAY_FILL_TYPE_4)
        ui->SetCheck((s.fillEnabled ? s.fillType : 0) == ui->m_nID - ID_MENU_OVERLAY_FILL_TYPE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_HATCH_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_HATCH_STYLE_52)
        ui->SetCheck(s.fillEnabled && s.fillType == 2 &&
                     s.hatch == ui->m_nID - ID_MENU_OVERLAY_HATCH_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_ARROW_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_ARROW_STYLE_35)
        ui->SetCheck(s.startArrow == ui->m_nID - ID_MENU_OVERLAY_ARROW_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_ENDARROW_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_ENDARROW_STYLE_35)
        ui->SetCheck(s.endArrow == ui->m_nID - ID_MENU_OVERLAY_ENDARROW_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_LINECAP_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_LINECAP_STYLE_2)
        ui->SetCheck(s.lineCap == ui->m_nID - ID_MENU_OVERLAY_LINECAP_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_LINEJOIN_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_LINEJOIN_STYLE_3)
        ui->SetCheck(s.lineJoin == ui->m_nID - ID_MENU_OVERLAY_LINEJOIN_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_LINEALIGNMENT_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_LINEALIGNMENT_STYLE_1)
        ui->SetCheck(s.lineAlignment == ui->m_nID - ID_MENU_OVERLAY_LINEALIGNMENT_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_LINESTRIPE_STYLE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_LINESTRIPE_STYLE_4)
        ui->SetCheck(s.lineStripe == ui->m_nID - ID_MENU_OVERLAY_LINESTRIPE_STYLE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_GARDIENT_LINERMODE_0 &&
             ui->m_nID <= ID_MENU_OVERLAY_GARDIENT_LINERMODE_3)
        ui->SetCheck(s.gradientMode == ui->m_nID - ID_MENU_OVERLAY_GARDIENT_LINERMODE_0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_TEXT_BOLD && ui->m_nID <= ID_MENU_OVERLAY_TEXT_STRIKEOUT)
        ui->SetCheck((s.fontStyle & (1 << (ui->m_nID - ID_MENU_OVERLAY_TEXT_BOLD))) != 0);
    else if (ui->m_nID >= ID_MENU_OVERLAY_TEXT_ALIGNLEFT &&
             ui->m_nID <= ID_MENU_OVERLAY_TEXT_ALIGNRIGHT)
        ui->SetCheck(s.textAlign == ui->m_nID - ID_MENU_OVERLAY_TEXT_ALIGNLEFT);
    else if (ui->m_nID >= ID_MENU_OVERLAY_TEXT_ALIGNTOP &&
             ui->m_nID <= ID_MENU_OVERLAY_TEXT_ALIGNBOTTOM)
        ui->SetCheck(s.textAlignY == ui->m_nID - ID_MENU_OVERLAY_TEXT_ALIGNTOP);
    else if (ui->m_nID == ID_MENU_OVERLAY_TEXT_VERTICAL)
        ui->SetCheck(s.vertical);
    else if (ui->m_nID == ID_MENU_OVERLAY_TEXT_RIGHTTOLEFT)
        ui->SetCheck(s.rightToLeft);
    else if (ui->m_nID == ID_MENU_OVERLAY_TEXT_AUTOBOX)
        ui->SetCheck(s.autoTextBox);
}
