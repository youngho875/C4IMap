#include "pch.h"
#include "GridSettingsDialog.h"
#include "DisplayProjection.h"
#include <afxdlgs.h>

namespace
{
constexpr UINT ColorButton = 51401;
const int TmCodes[] = {5186, 5185, 5187, 5188, 5179, 5174, 5173, 5175, 5176, 5177};
const TCHAR *TmNames[] = {_T("Korea 2000 중부원점 2010 (EPSG:5186)"),
                          _T("Korea 2000 서부원점 2010 (EPSG:5185)"),
                          _T("Korea 2000 동부원점 2010 (EPSG:5187)"),
                          _T("Korea 2000 동해원점 2010 (EPSG:5188)"),
                          _T("Korea 2000 통합좌표계 / UTM-K (EPSG:5179)"),
                          _T("Korean 1985 수정 중부원점 (EPSG:5174)"),
                          _T("Korean 1985 수정 서부원점 (EPSG:5173)"),
                          _T("Korean 1985 수정 중부원점 제주 (EPSG:5175)"),
                          _T("Korean 1985 수정 동부원점 (EPSG:5176)"),
                          _T("Korean 1985 수정 동해원점 (EPSG:5177)")};
} // namespace

BEGIN_MESSAGE_MAP(CGridSettingsDialog, CModelessDialog)
ON_BN_CLICKED(ColorButton, &CGridSettingsDialog::OnColor)
ON_CBN_SELCHANGE(51406, &CGridSettingsDialog::OnProjectionChanged)
END_MESSAGE_MAP()

CGridSettingsDialog::CGridSettingsDialog(const CMapGridSettings &settings, CWnd *parent)
    : CModelessDialog((UINT)0, parent), value(settings)
{
}

bool CGridSettingsDialog::Show(std::function<void(CGridSettingsDialog&)> accepted)
{
    EmptyTemplate layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    layout.dialog.cx = 370;
    layout.dialog.cy = 365;
    return OpenTemplate(&layout.dialog, m_pParentWnd, [this, accepted]() { if (accepted) accepted(*this); });
}

CMapGridSettings CGridSettingsDialog::Settings() const
{
    return value;
}

BOOL CGridSettingsDialog::OnInitDialog()
{
    CModelessDialog::OnInitDialog();
    SetWindowText(_T("그리드 기준 좌표계 설정"));
    auto box = [&](int l, int t, int r, int b) {
        CRect rc(l, t, r, b);
        MapDialogRect(&rc);
        return rc;
    };
    const DWORD child = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    enabled.Create(_T("지도에 그리드 표시"), child | BS_AUTOCHECKBOX, box(12, 10, 340, 28), this,
                   51402);
    enabled.SetCheck(value.visible ? BST_CHECKED : BST_UNCHECKED);
    labels[0].Create(_T("그리드 기준 투영"), WS_CHILD | WS_VISIBLE, box(12, 36, 165, 53), this);
    projection.Create(child | CBS_DROPDOWNLIST, box(170, 33, 355, 220), this, 51406);
    for (const auto name : {_T("지도 투영과 동일"), _T("Geodetic (경위도)"), _T("UTM"),
                            _T("Mercator"), _T("Lambert Conformal Conic"), _T("Stereographic"),
                            _T("Equidistant Cylindrical"), _T("TM")})
        projection.AddString(name);
    projection.SetCurSel(value.projection + 1);
    labels[1].Create(_T("투영 그리드 간격 (m)"), WS_CHILD | WS_VISIBLE, box(12, 65, 165, 83), this);
    labels[2].Create(_T("경위도 그리드 간격 (°)"), WS_CHILD | WS_VISIBLE, box(12, 91, 165, 109),
                     this);
    metres.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(170, 63, 355, 83), this, 51403);
    degrees.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(170, 89, 355, 109), this, 51404);
    CString text;
    text.Format(_T("%.10g"), value.metres);
    metres.SetWindowText(text);
    text.Format(_T("%.10g"), value.degrees);
    degrees.SetWindowText(text);
    color.Create(_T(""), child, box(12, 117, 355, 138), this, ColorButton);
    labels[3].Create(_T("그리드용 TM 기준 (그리드 투영이 TM일 때 사용)"), WS_CHILD | WS_VISIBLE,
                     box(12, 146, 355, 162), this);
    gridTm.Create(child | CBS_DROPDOWNLIST | WS_VSCROLL, box(12, 164, 355, 345), this, 51407);
    labels[4].Create(_T("지도용 TM 기준 (지도 투영이 TM일 때 사용)"), WS_CHILD | WS_VISIBLE,
                     box(12, 192, 355, 208), this);
    tm.Create(child | CBS_DROPDOWNLIST | WS_VSCROLL, box(12, 210, 355, 390), this, 51405);
    for (int i = 0; i < _countof(TmCodes); ++i)
    {
        const int mapIndex = tm.AddString(TmNames[i]);
        tm.SetItemData(mapIndex, TmCodes[i]);
        if (TmCodes[i] == value.tmEpsg)
            tm.SetCurSel(mapIndex);
        const int gridIndex = gridTm.AddString(TmNames[i]);
        gridTm.SetItemData(gridIndex, TmCodes[i]);
        if (TmCodes[i] == value.gridTmEpsg)
            gridTm.SetCurSel(gridIndex);
    }
    labels[5].Create(_T("기준 경도 / 위도 (UTM 존·LCC·Stereo 원점)"), WS_CHILD | WS_VISIBLE,
                     box(12, 238, 355, 254), this);
    longitude.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(12, 256, 177, 276), this, 51408);
    latitude.Create(child | WS_BORDER | ES_AUTOHSCROLL, box(190, 256, 355, 276), this, 51409);
    text.Format(_T("%.10g"), value.originLongitude);
    longitude.SetWindowText(text);
    text.Format(_T("%.10g"), value.originLatitude);
    latitude.SetWindowText(text);
    labels[6].Create(_T(""), WS_CHILD | WS_VISIBLE, box(12, 287, 355, 303), this);
    labels[7].Create(_T("조밀한 격자는 지정 간격의 배수로 간략히 표시합니다."),
                     WS_CHILD | WS_VISIBLE, box(12, 308, 355, 324), this);
    ok.Create(_T("확인"), child | BS_DEFPUSHBUTTON, box(225, 339, 285, 359), this, IDOK);
    cancel.Create(_T("취소"), child, box(295, 339, 355, 359), this, IDCANCEL);
    OnProjectionChanged();
    for (CWnd *w = GetWindow(GW_CHILD); w; w = w->GetNextWindow())
        w->SetFont(GetFont());
    UpdateColor();
    return TRUE;
}

void CGridSettingsDialog::UpdateColor()
{
    CString text;
    text.Format(_T("그리드 색 선택   #%02X%02X%02X"), GetRValue(value.color),
                GetGValue(value.color), GetBValue(value.color));
    color.SetWindowText(text);
}

void CGridSettingsDialog::OnColor()
{
    ModelessDialogs::Color(this, value.color, [this](COLORREF color) {
        value.color = color;
        UpdateColor();
    });
}

void CGridSettingsDialog::OnOK()
{
    auto parse = [](CEdit &edit, double &number) {
        CString text;
        edit.GetWindowText(text);
        text.Trim();
        TCHAR *end = nullptr;
        number = _tcstod(text, &end);
        return !text.IsEmpty() && end && !*end && std::isfinite(number);
    };
    CMapGridSettings next = value;
    if (!parse(metres, next.metres) || !parse(degrees, next.degrees) || tm.GetCurSel() == CB_ERR ||
        gridTm.GetCurSel() == CB_ERR || projection.GetCurSel() == CB_ERR ||
        !parse(latitude, next.originLatitude) || !parse(longitude, next.originLongitude))
    {
        AfxMessageBox(_T("간격과 기준 경위도를 숫자로 입력하고 그리드 투영을 선택하세요."));
        return;
    }
    next.tmEpsg = (int)tm.GetItemData(tm.GetCurSel());
    next.gridTmEpsg = (int)gridTm.GetItemData(gridTm.GetCurSel());
    next.projection = projection.GetCurSel() - 1;
    next.visible = enabled.GetCheck() == BST_CHECKED;
    if (!next.Valid())
    {
        AfxMessageBox(_T("미터 간격: 0.1~10,000,000 m\n경위도 간격: 0.000001~180°\n기준 경도: -180~180°, 기준 위도: -89.999~89.999° 미만"));
        return;
    }
    CDisplayProjection check;
    if (next.projection >= 0 && !next.ConfigureProjection(check))
    {
        AfxMessageBox(_T("선택한 투영에 사용할 수 없는 기준 경위도입니다. UTM/TM은 남위 80~북위 ")
                      _T("84도에서 사용하세요."));
        return;
    }
    value = next;
    CModelessDialog::OnOK();
}

void CGridSettingsDialog::OnProjectionChanged()
{
    const int selected = projection.GetCurSel() - 1;
    const int effective = selected < 0 ? DisplayProjection().Kind() : selected;
    metres.EnableWindow(effective != 0);
    degrees.EnableWindow(effective == 0);
    gridTm.EnableWindow(selected == 6);
    const bool origin = selected == 1 || selected == 3 || selected == 4;
    latitude.EnableWindow(origin);
    longitude.EnableWindow(origin);
    labels[6].SetWindowText(effective == 0 ? _T("그리드 간격: 도(°) / 지도와 별도로 선택할 수 있습니다.")
                                           : _T("그리드 간격: 미터(m) / 지도와 별도로 선택할 수 있습니다."));
}
