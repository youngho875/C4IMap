#include "pch.h"
#pragma pack(push, 8)
#include "AnalysisLocationDialog.h"
#include <cmath>
namespace
{
#pragma pack(push, 2)
struct LocationDialogTemplate
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD windowClass;
    WORD title;
};
#pragma pack(pop)
} // namespace
CAnalysisLocationDialog::CAnalysisLocationDialog(CWnd *parent) : CDialog()
{
    m_pParentWnd = parent;
}
INT_PTR CAnalysisLocationDialog::Show()
{
    LocationDialogTemplate layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    layout.dialog.cx = 240;
    layout.dialog.cy = 115;
    if (!InitModalIndirect(&layout.dialog, m_pParentWnd))
        return IDCANCEL;
    return DoModal();
}
BOOL CAnalysisLocationDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetWindowText(_T("지점 입력 (WGS84 십진 도)"));
    CRect area;
    GetClientRect(&area);
    int width = area.Width();
    longitudeLabel.Create(_T("경도 (-180 ~ 180)"), WS_CHILD | WS_VISIBLE,
                          CRect(12, 15, width - 12, 35), this);
    longitude.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                     CRect(12, 38, width - 12, 62), this, 101);
    latitudeLabel.Create(_T("위도 (-90 ~ 90)"), WS_CHILD | WS_VISIBLE,
                         CRect(12, 70, width - 12, 90), this);
    latitude.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                    CRect(12, 93, width - 12, 117), this, 102);
    longitude.SetWindowText(_T("127.0"));
    latitude.SetWindowText(_T("37.0"));
    int bottom = area.Height() - 12;
    ok.Create(_T("확인"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
              CRect(width - 170, bottom - 28, width - 95, bottom), this, IDOK);
    cancel.Create(_T("취소"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                  CRect(width - 85, bottom - 28, width - 10, bottom), this, IDCANCEL);
    CenterWindow();
    return TRUE;
}
void CAnalysisLocationDialog::OnOK()
{
    CString lon, lat;
    longitude.GetWindowText(lon);
    latitude.GetWindowText(lat);
    lon.Trim();
    lat.Trim();
    TCHAR *lonEnd = nullptr;
    TCHAR *latEnd = nullptr;
    double x = _tcstod(lon, &lonEnd), y = _tcstod(lat, &latEnd);
    if (lon.IsEmpty() || lat.IsEmpty() || !lonEnd || *lonEnd || !latEnd || *latEnd ||
        !std::isfinite(x) || !std::isfinite(y) || fabs(x) > 180 || fabs(y) > 90)
    {
        AfxMessageBox(_T("유효한 경도와 위도를 입력하세요."));
        return;
    }
    location = {x, y};
    CDialog::OnOK();
}
AnalysisPoint CAnalysisLocationDialog::Location() const
{
    return location;
}

#pragma pack(pop)
