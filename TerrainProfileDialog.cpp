#include "pch.h"
#pragma pack(push, 8)
#include "TerrainProfileDialog.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <limits>
#pragma comment(lib, "gdiplus.lib")
namespace
{
#pragma pack(push, 2)
struct ProfileDialogTemplate
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD windowClass;
    WORD title;
};
#pragma pack(pop)
CRect ProfilePlot(CRect rect)
{
    return CRect(rect.left + 85, rect.top + 90, rect.right - 35, rect.bottom - 80);
}
} // namespace
BEGIN_MESSAGE_MAP(CTerrainProfileDialog, CDialog)
ON_WM_PAINT()
ON_WM_SIZE()
ON_WM_MOUSEMOVE()
ON_WM_ERASEBKGND()
END_MESSAGE_MAP()
CTerrainProfileDialog::CTerrainProfileDialog(const std::vector<AnalysisProfileSample> &samples,
                                             CWnd *parent)
    : data(samples)
{
    m_pParentWnd = parent;
}
INT_PTR CTerrainProfileDialog::Show()
{
    ProfileDialogTemplate layout{};
    layout.dialog.style =
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | DS_MODALFRAME;
    layout.dialog.cx = 560;
    layout.dialog.cy = 310;
    if (!InitModalIndirect(&layout.dialog, m_pParentWnd))
        return IDCANCEL;
    return DoModal();
}
bool CTerrainProfileDialog::Open(const std::vector<AnalysisProfileSample>& samples, CWnd* parent)
{
    ProfileDialogTemplate layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
        WS_MAXIMIZEBOX | WS_CLIPCHILDREN | DS_MODALFRAME;
    layout.dialog.cx = 560;
    layout.dialog.cy = 310;
    auto* dialog = new CTerrainProfileDialog(samples, parent);
    // Do not auto-delete during a failed CreateIndirect call.
    if (!dialog->CreateIndirect(&layout.dialog, parent))
    {
        delete dialog;
        return false;
    }
    dialog->modeless = true;
    dialog->ShowWindow(SW_SHOWNOACTIVATE);
    return true;
}
void CTerrainProfileDialog::OnOK()
{
    if (modeless)
        DestroyWindow();
    else
        CDialog::OnOK();
}
void CTerrainProfileDialog::OnCancel()
{
    if (modeless)
        DestroyWindow();
    else
        CDialog::OnCancel();
}
void CTerrainProfileDialog::PostNcDestroy()
{
    bool release = modeless;
    CDialog::PostNcDestroy();
    if (release)
        delete this;
}
BOOL CTerrainProfileDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetWindowText(_T("지형 단면도 - 거리 / 높이"));
    CRect rect;
    GetClientRect(&rect);
    closeButton.Create(_T("닫기"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                       CRect(rect.right - 100, rect.bottom - 38, rect.right - 15, rect.bottom - 10),
                       this, IDOK);
    CenterWindow();
    return TRUE;
}
void CTerrainProfileDialog::OnPaint()
{
    CPaintDC screen(this);
    CRect rect;
    GetClientRect(&rect);
    CDC buffer;
    CBitmap bitmap;
    if (!buffer.CreateCompatibleDC(&screen) ||
        !bitmap.CreateCompatibleBitmap(&screen, rect.Width(), rect.Height()))
        return;
    CBitmap *old = buffer.SelectObject(&bitmap);
    DrawChart(&buffer, rect, data, selected);
    screen.BitBlt(0, 0, rect.Width(), rect.Height(), &buffer, 0, 0, SRCCOPY);
    buffer.SelectObject(old);
}
BOOL CTerrainProfileDialog::OnEraseBkgnd(CDC *)
{
    return TRUE;
}
void CTerrainProfileDialog::OnSize(UINT type, int cx, int cy)
{
    CDialog::OnSize(type, cx, cy);
    if (closeButton.GetSafeHwnd())
        closeButton.MoveWindow((std::max)(0, cx - 100), (std::max)(0, cy - 38), 85, 28);
    Invalidate(FALSE);
}
void CTerrainProfileDialog::OnMouseMove(UINT flags, CPoint point)
{
    CRect rect;
    GetClientRect(&rect);
    CRect plot = ProfilePlot(rect);
    int index = -1;
    if (!data.empty() && plot.Width() > 0 && plot.PtInRect(point))
    {
        double distance = data.back().distance * (point.x - plot.left) / plot.Width();
        auto found = std::lower_bound(data.begin(), data.end(), distance,
                                      [](const AnalysisProfileSample &sample, double value)
                                      { return sample.distance < value; });
        index = static_cast<int>(found - data.begin());
        if (index == static_cast<int>(data.size()))
            --index;
        if (index > 0 &&
            fabs(data[index - 1].distance - distance) < fabs(data[index].distance - distance))
            --index;
    }
    if (index != selected)
    {
        selected = index;
        Invalidate(FALSE);
    }
    CDialog::OnMouseMove(flags, point);
}
void CTerrainProfileDialog::DrawChart(CDC *dc, CRect rect,
                                      const std::vector<AnalysisProfileSample> &samples,
                                      int selected)
{
    int saved = dc->SaveDC();
    dc->FillSolidRect(rect, RGB(255, 255, 255));
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(RGB(30, 30, 30));
    CRect plot = ProfilePlot(rect);
    if (plot.Width() < 30 || plot.Height() < 30 || samples.empty())
    {
        dc->TextOut(12, 12, _T("표시할 단면도 데이터가 없습니다."));
        dc->RestoreDC(saved);
        return;
    }
    double low = std::numeric_limits<double>::infinity(), high = -low;
    int missing = 0;
    for (const auto &sample : samples)
    {
        if (std::isfinite(sample.height))
        {
            low = (std::min)(low, sample.height);
            high = (std::max)(high, sample.height);
        }
        else
            ++missing;
    }
    CString title;
    title.Format(_T("총 거리 %.1f m    최저 %.1f m    최고 %.1f m"), samples.back().distance,
                 std::isfinite(low) ? low : 0, std::isfinite(high) ? high : 0);
    if (!std::isfinite(low))
        title = _T("해당 경로의 고도 데이터가 없습니다.");
    dc->TextOut(18, 12, title);
    if (selected >= 0 && selected < static_cast<int>(samples.size()))
    {
        const auto &sample = samples[selected];
        if (std::isfinite(sample.height))
            title.Format(_T("선택 위치: 거리 %.1f m / 높이 %.1f m"), sample.distance,
                         sample.height);
        else
            title.Format(_T("선택 위치: 거리 %.1f m / 높이 없음"), sample.distance);
        dc->TextOut(18, 34, title);
    }
    dc->TextOut(8, plot.top - 22, _T("높이(m)"));
    dc->TextOut(plot.right - 60, plot.bottom + 28, _T("거리(m)"));
    if (!std::isfinite(low))
    {
        dc->RestoreDC(saved);
        return;
    }
    if (high - low < 1)
    {
        low -= 1;
        high += 1;
    }
    Gdiplus::Graphics graphics(dc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen grid(Gdiplus::Color(255, 220, 225, 230), 1);
    Gdiplus::Pen line(Gdiplus::Color(255, 10, 90, 190), 2);
    double length = (std::max)(1.0, samples.back().distance);
    for (int tick = 0; tick <= 5; ++tick)
    {
        int x = plot.left + plot.Width() * tick / 5;
        int y = plot.bottom - plot.Height() * tick / 5;
        graphics.DrawLine(&grid, x, plot.top, x, plot.bottom);
        graphics.DrawLine(&grid, plot.left, y, plot.right, y);
    }
    bool previousValid = false;
    Gdiplus::PointF previous;
    for (const auto &sample : samples)
    {
        if (!std::isfinite(sample.height))
        {
            previousValid = false;
            continue;
        }
        Gdiplus::PointF point(
            static_cast<float>(plot.left + sample.distance / length * plot.Width()),
            static_cast<float>(plot.bottom - (sample.height - low) / (high - low) * plot.Height()));
        if (previousValid)
            graphics.DrawLine(&line, previous, point);
        previous = point;
        previousValid = true;
    }
    if (selected >= 0 && selected < static_cast<int>(samples.size()))
    {
        int x = plot.left + static_cast<int>(samples[selected].distance / length * plot.Width());
        Gdiplus::Pen cursor(Gdiplus::Color(255, 170, 50, 20), 1);
        cursor.SetDashStyle(Gdiplus::DashStyleDash);
        graphics.DrawLine(&cursor, x, plot.top, x, plot.bottom);
    }
    graphics.Flush(Gdiplus::FlushIntentionSync);
    for (int tick = 0; tick <= 5; ++tick)
    {
        int x = plot.left + plot.Width() * tick / 5;
        int y = plot.bottom - plot.Height() * tick / 5;
        CString label;
        label.Format(_T("%.0f"), low + (high - low) * tick / 5);
        dc->TextOut(10, y - 8, label);
        label.Format(_T("%.0f"), samples.back().distance * tick / 5);
        dc->TextOut(x - dc->GetTextExtent(label).cx / 2, plot.bottom + 8, label);
    }
    if (missing)
    {
        CString text;
        text.Format(_T("빈 구간: 고도 없음 (%d개 표본)"), missing);
        dc->TextOut(plot.left, plot.bottom + 30, text);
    }
    dc->RestoreDC(saved);
}
#pragma pack(pop)
