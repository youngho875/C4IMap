#include "pch.h"
#pragma pack(push, 8)
#include "C4IMapViewHeader.h"
#include "C4IMapView.h"
#include "MainFrm.h"
#include "BaseMapOptions.h"
#include "AnalysisLocationDialog.h"
#include <cmath>
namespace
{
bool ReadAnalysisNumber(CMFCRibbonEdit *edit, double &value)
{
    if (!edit)
        return false;
    CString text = edit->GetEditText();
    text.Trim();
    TCHAR *end = nullptr;
    value = _tcstod(text, &end);
    return !text.IsEmpty() && end && !*end && std::isfinite(value);
}
void SelectAnalysisElevation(AnalysisSettings &settings)
{
    auto &config = BaseMapConfig();
    auto selection = config.Value("AltitudeSource", "auto");
    settings.source = config.Value("Elevation.Path", "D:/map/strm");
    if (selection == "terrain-server")
    {
        settings.sourceKind = 2;
        settings.source = config.Value("TerrainServer.LayerJson");
    }
    else if (selection == "terrain-local")
    {
        settings.sourceKind = 1;
        settings.source = config.Value("TerrainLocal.LayerJson");
    }
    else if (selection == "auto")
    {
#if C4MAP_ENABLE_TERRAIN_SERVER
        auto url = config.Value("TerrainServer.LayerJson");
        if (url.find_first_not_of(" \t\r\n") != std::string::npos)
        {
            settings.sourceKind = 2;
            settings.source = url;
            return;
        }
#endif
#if C4MAP_ENABLE_TERRAIN_LOCAL
        CString path = BaseMapPath("TerrainLocal.LayerJson", "");
        if (!path.IsEmpty() && GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        {
            settings.sourceKind = 1;
            settings.source = config.Value("TerrainLocal.LayerJson");
        }
#endif
    }
    else if (selection != "dted")
        settings.source.clear();
}
} // namespace
void CC4IMapView::OnAnalysisCommand(UINT id)
{
    if (id == ID_MENU_ANALYSIS_CLEAR)
    {
        m_analysis.Clear();
        Invalidate(FALSE);
        return;
    }
    CMapAnalysis::Mode mode;
    switch (id)
    {
    case ID_MENU_ANALYSIS_DISTANCE:
        mode = CMapAnalysis::Distance;
        break;
    case ID_MENU_ANALYSIS_AREA:
        mode = CMapAnalysis::Area;
        break;
    case ID_MENU_ANALYSIS_SECTION:
        mode = CMapAnalysis::Profile;
        break;
    case ID_MENU_ANALYSIS_VISIBLE_SELECT:
    case ID_ANALYSIS_VISIBLE_INPUT:
        mode = CMapAnalysis::Viewshed;
        break;
    case ID_MENU_ANALYSIS_RADIUS_SELECT:
    case ID_MENU_ANALYSIS_RADIUS_INPUT:
        mode = CMapAnalysis::Bearing;
        break;
    default:
        return; // Ribbon edit and color changes are read when starting an operation.
    }
    AnalysisSettings settings;
    auto frame = get_mainfrm();
    if (mode == CMapAnalysis::Viewshed)
    {
        if (!ReadAnalysisNumber(frame->m_pRibbonAnalysisVisibleRange, settings.range) ||
            !ReadAnalysisNumber(frame->m_pRibbonAnalysisVisibleHeight, settings.observerHeight) ||
            settings.range <= 0 || settings.range > 100000 || settings.observerHeight < 0 ||
            settings.observerHeight > 10000)
        {
            AfxMessageBox(
                _T("관측 범위는 0 초과 ~ 100,000 m, 관측 높이는 0 ~ 10,000 m로 입력하세요."));
            return;
        }
        settings.color = frame->m_pRibbonAnalysisVisibleColor->GetColor();
    }
    if (mode == CMapAnalysis::Bearing)
    {
        if (!ReadAnalysisNumber(frame->m_pRibbonRadiusRange, settings.range) ||
            !ReadAnalysisNumber(frame->m_pRibbonRadiusInterval, settings.interval) ||
            settings.range <= 0 || settings.range > 1000000 || settings.interval <= 0 ||
            ceil(settings.range / settings.interval) > 100)
        {
            AfxMessageBox(_T("반경은 0 초과 ~ 1,000,000 m, 간격은 양수이며 원의 개수는 100개 ")
                          _T("이하로 입력하세요."));
            return;
        }
        settings.color = frame->m_pRibbonAnalysisRadiusColor->GetColor();
        settings.width = frame->m_pRibbonAnalysisRadiusWidth->GetCurSel() + 1;
    }
    if (mode == CMapAnalysis::Profile || mode == CMapAnalysis::Viewshed)
    {
        SelectAnalysisElevation(settings);
        if (settings.source.empty())
        {
            AfxMessageBox(
                _T("base-map.xml의 AltitudeSource와 해당 고도 데이터 경로를 설정하세요."));
            return;
        }
    }
    if (settings.color == static_cast<COLORREF>(-1))
        settings.color = RGB(255, 0, 0);
    CAnalysisLocationDialog dialog(this);
    bool input = id == ID_MENU_ANALYSIS_RADIUS_INPUT || id == ID_ANALYSIS_VISIBLE_INPUT;
    if (input && dialog.Show() != IDOK)
        return;
    SetMouseState(MOUSE_DEFAULT);
    m_localOverlay.SetTool(CLocalOverlay::Pan);
    m_bPanning = FALSE;
    m_bOnZoom = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();
    if (!m_analysis.Start(mode, settings))
    {
        AfxMessageBox(_T("같은 종류의 분석이 아직 계산 중입니다. 다른 분석 도구는 사용할 수 있습니다."));
        return;
    }
    if (input)
        m_analysis.Place(dialog.Location());
    SetFocus();
    Invalidate(FALSE);
}
void CC4IMapView::OnUpdateAnalysis(CCmdUI *ui)
{
    ui->Enable(TRUE);
}
BOOL CC4IMapView::OnOverlayDeleteCommand(UINT id)
{
    OnOverlayEdit(id);
    return TRUE;
}
void CC4IMapView::OnOverlayEdit(UINT id)
{
    bool success = false;
    switch (id)
    {
    case ID_MENU_OVERLAY_DELETE:
        success = m_localOverlay.DeleteSelected();
        break;
    case ID_MENU_OVERLAY_COPY:
        success = m_localOverlay.CopySelected(GetSafeHwnd());
        break;
    case ID_MENU_OVERLAY_CUT:
        success = m_localOverlay.CutSelected(GetSafeHwnd());
        break;
    case ID_MENU_OVERLAY_PASTE:
        success = m_localOverlay.Paste(GetSafeHwnd(), m_pCoordMgr);
        break;
    }
    if (success)
    {
        RefreshOverlayLayers();
        Invalidate(FALSE);
    }
    else if (id == ID_MENU_OVERLAY_PASTE || m_localOverlay.HasSelection())
        AfxMessageBox(_T("도형 편집을 완료하지 못했습니다. 활성 레이어의 표시 여부와 클립보드 ")
                      _T("사용 상태를 확인하세요."));
}
void CC4IMapView::OnUpdateOverlayEdit(CCmdUI *ui)
{
    ui->Enable(ui->m_nID == ID_MENU_OVERLAY_PASTE ? CLocalOverlay::CanPaste()
                                                  : m_localOverlay.HasSelection());
}

BOOL CC4IMapView::OnAnalysisInputCommand(UINT id)
{
    OnAnalysisCommand(id);
    return TRUE;
}

#pragma pack(pop)
