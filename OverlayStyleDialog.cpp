#include "pch.h"
#include "OverlayStyleDialog.h"
#include <afxdlgs.h>
#include <cmath>
namespace
{
enum
{
    LineColor = 51001,
    FillColor,
    PatternColor
};
struct DialogLayout
{
    DLGTEMPLATE dialog;
    WORD menu = 0, windowClass = 0, title = 0;
};
} // namespace
BEGIN_MESSAGE_MAP(COverlayStyleDialog, CModelessDialog)
ON_CONTROL_RANGE(BN_CLICKED, LineColor, PatternColor, &COverlayStyleDialog::OnColor)
END_MESSAGE_MAP()
COverlayStyleDialog::COverlayStyleDialog(const CLocalOverlay::Style &style, bool selected,
                                         CWnd *parent)
    : CModelessDialog((UINT)0, parent), value(style), selectedObject(selected)
{
}
bool COverlayStyleDialog::Show(std::function<void(COverlayStyleDialog&)> accepted)
{
    DialogLayout layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    layout.dialog.cx = 330;
    layout.dialog.cy = 245;
    return OpenTemplate(&layout.dialog, m_pParentWnd, [this, accepted]() { if (accepted) accepted(*this); });
}
CLocalOverlay::Style COverlayStyleDialog::Style() const
{
    return value;
}
BOOL COverlayStyleDialog::OnInitDialog()
{
    CModelessDialog::OnInitDialog();
    SetWindowText(_T("도형 속성 - 확인 시 현재 선택에 적용"));
    auto box = [&](int l, int t, int r, int b) {
        CRect rc(l, t, r, b);
        MapDialogRect(&rc);
        return rc;
    };
    const DWORD child = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    lineCheck.Create(_T("윤곽선 표시"), child | BS_AUTOCHECKBOX, box(12, 10, 150, 25), this, 51010);
    fillCheck.Create(_T("채우기 사용 (사각형·타원)"), child | BS_AUTOCHECKBOX, box(12, 35, 250, 50),
                     this, 51011);
    lineCheck.SetCheck(value.lineEnabled);
    fillCheck.SetCheck(value.fillEnabled);
    lineColor.Create(_T(""), child, box(12, 60, 108, 80), this, LineColor);
    fillColor.Create(_T(""), child, box(114, 60, 210, 80), this, FillColor);
    patternColor.Create(_T(""), child, box(216, 60, 318, 80), this, PatternColor);
    const TCHAR *text[] = {_T("선 굵기 (0~100 px)"),   _T("선 투명도 (0~100%)"),
                           _T("채움 투명도 (0~100%)"), _T("선 종류"),
                           _T("채우기 종류"),          _T("0%: 불투명 / 100%: 완전 투명")};
    for (int i = 0; i < 5; ++i)
        labels[i].Create(text[i], WS_CHILD | WS_VISIBLE, box(12, 90 + i * 23, 160, 105 + i * 23),
                         this);
    labels[5].Create(text[5], WS_CHILD | WS_VISIBLE, box(12, 206, 318, 220), this);
    width.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(170, 88, 310, 104), this, 51012);
    lineOpacity.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(170, 111, 310, 127), this, 51013);
    fillOpacity.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(170, 134, 310, 150), this, 51014);
    CString s;
    s.Format(_T("%.2f"), value.width);
    width.SetWindowText(s);
    SetDlgItemInt(51013, (int)lround((255 - value.lineAlpha) * 100.0 / 255));
    SetDlgItemInt(51014, (int)lround((255 - value.fillAlpha) * 100.0 / 255));
    dash.Create(child | CBS_DROPDOWNLIST, box(170, 157, 310, 240), this, 51015);
    for (auto name : {_T("실선"), _T("파선"), _T("점선"), _T("일점쇄선"), _T("이점쇄선")})
        dash.AddString(name);
    dash.SetCurSel(value.dash);
    fillType.Create(child | CBS_DROPDOWNLIST, box(170, 180, 310, 260), this, 51016);
    for (auto name : {_T("단색"), _T("패턴"), _T("중심 그라데이션"), _T("선형 그라데이션"),
                      _T("그림"), _T("질감")})
        fillType.AddString(name);
    fillType.SetCurSel(value.fillType - 1);
    ok.Create(_T("확인"), child | BS_DEFPUSHBUTTON, box(192, 222, 250, 240), this, IDOK);
    cancel.Create(_T("취소"), child, box(258, 222, 318, 240), this, IDCANCEL);
    for (CWnd *w = GetWindow(GW_CHILD); w; w = w->GetNextWindow())
        w->SetFont(GetFont());
    UpdateColors();
    return TRUE;
}
void COverlayStyleDialog::UpdateColors()
{
    auto label = [](CButton &button, const TCHAR *title, COLORREF color) {
        CString s;
        s.Format(_T("%s #%02X%02X%02X"), title, GetRValue(color), GetGValue(color),
                 GetBValue(color));
        button.SetWindowText(s);
    };
    label(lineColor, _T("선"), value.lineColor);
    label(fillColor, _T("채움"), value.fillColor);
    label(patternColor, _T("패턴"), value.patternColor);
}
void COverlayStyleDialog::OnColor(UINT id)
{
    COLORREF &color = id == LineColor ? value.lineColor
                                      : (id == FillColor ? value.fillColor : value.patternColor);
    ModelessDialogs::Color(this, color, [this, id](COLORREF selected) {
        COLORREF& target = id == LineColor ? value.lineColor :
            (id == FillColor ? value.fillColor : value.patternColor);
        target = selected;
        UpdateColors();
    });
}
void COverlayStyleDialog::OnOK()
{
    auto number = [&](CEdit &edit, double &out) {
        CString s;
        edit.GetWindowText(s);
        s.Trim();
        TCHAR *end = nullptr;
        out = _tcstod(s, &end);
        return !s.IsEmpty() && end && !*end && std::isfinite(out);
    };
    double w, a, b;
    if (!number(width, w) || !number(lineOpacity, a) || !number(fillOpacity, b) || w < 0 ||
        w > 100 || a < 0 || a > 100 || b < 0 || b > 100)
    {
        AfxMessageBox(_T("선 굵기와 투명도는 0~100 범위의 숫자로 입력하세요."));
        return;
    }
    value.width = w;
    value.lineAlpha = (int)lround(255 * (1 - a / 100));
    value.fillAlpha = (int)lround(255 * (1 - b / 100));
    value.lineEnabled = lineCheck.GetCheck() == BST_CHECKED;
    value.fillEnabled = fillCheck.GetCheck() == BST_CHECKED;
    value.dash = dash.GetCurSel();
    value.fillType = fillType.GetCurSel() + 1;
    if (value.fillEnabled && value.fillType >= 5 && !value.imagePath[0])
    {
        AfxMessageBox(_T("도형 채우기의 그림 또는 질감 메뉴에서 이미지 파일을 먼저 선택하세요."));
        return;
    }
    if (!CLocalOverlay::ValidStyle(value))
        return;
    CModelessDialog::OnOK();
}
