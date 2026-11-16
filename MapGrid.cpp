#include "pch.h"
#include "MapGrid.h"
#include "BaseMapOptions.h"
#include "DisplayProjection.h"
#include <limits>
#include <sstream>
#include <iomanip>
#include <array>

namespace
{
const char *ProjectionNames[] = {
    "SameAsMap", "Geodetic", "UTM", "Mercator", "LCC", "Stereographic", "EquidistantCylindrical",
    "TM"};
CDisplayProjection *GridProjection(const CMapGridSettings &settings)
{
    if (settings.projection == -1)
        return &DisplayProjection();
    static CDisplayProjection projection;
    static std::array<double, 4> previous{};
    static bool ready = false;
    const std::array<double, 4> key = {double(settings.projection), double(settings.gridTmEpsg),
                                       settings.originLatitude, settings.originLongitude};
    if (!ready || key != previous)
    {
        ready = settings.ConfigureProjection(projection);
        if (!ready)
            return nullptr;
        previous = key;
    }
    return &projection;
}
double Number(const std::string &text, double fallback)
{
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    double value;
    return (in >> value) && in.peek() == EOF && std::isfinite(value) ? value : fallback;
}
std::string Text(double value)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(15) << value;
    return out.str();
}
} // namespace

bool IsSupportedTmEpsg(int epsg)
{
    return (epsg >= 5185 && epsg <= 5188) || (epsg >= 5173 && epsg <= 5177) || epsg == 5179;
}

bool CMapGridSettings::Valid() const
{
    return std::isfinite(metres) && metres >= 0.1 && metres <= 10000000 && std::isfinite(degrees) &&
           degrees >= 0.000001 && degrees <= 180 && color <= 0xffffff &&
           IsSupportedTmEpsg(tmEpsg) && IsSupportedTmEpsg(gridTmEpsg) && projection >= -1 &&
           projection <= 6 && std::isfinite(originLatitude) && std::isfinite(originLongitude) &&
           fabs(originLatitude) < 89.999 && fabs(originLongitude) <= 180;
}

void CMapGridSettings::Load()
{
    auto &config = BaseMapConfig();
    visible = config.Flag("Grid.Visible", false);
    metres = Number(config.Value("Grid.MetreInterval"), 10000);
    degrees = Number(config.Value("Grid.DegreeInterval"), 0.1);
    if (metres < 0.1 || metres > 10000000)
        metres = 10000;
    if (degrees < 0.000001 || degrees > 180)
        degrees = 0.1;
    const auto rgb = config.Value("Grid.Color", "#80A0C0");
    unsigned int hex = 0;
    if (rgb.size() == 7 && rgb[0] == '#' &&
        rgb.find_first_not_of("0123456789abcdefABCDEF", 1) == std::string::npos)
    {
        std::istringstream in(rgb.substr(1));
        in >> std::hex >> hex;
        color = RGB((hex >> 16) & 255, (hex >> 8) & 255, hex & 255);
    }
    const double epsg = Number(config.Value("Projection.TMEpsg"), 5186);
    tmEpsg = epsg >= 5173 && epsg <= 5188 && epsg == floor(epsg) && IsSupportedTmEpsg((int)epsg)
                 ? (int)epsg
                 : 5186;
    const auto name = config.Value("Grid.Projection", "SameAsMap");
    projection = -1;
    for (int i = 0; i < 8; ++i)
        if (name == ProjectionNames[i])
            projection = i - 1;
    const double gridEpsg = Number(config.Value("Grid.TMEpsg"), 5186);
    gridTmEpsg = gridEpsg >= 5173 && gridEpsg <= 5188 && floor(gridEpsg) == gridEpsg &&
                         IsSupportedTmEpsg((int)gridEpsg)
                     ? (int)gridEpsg
                     : 5186;
    originLatitude = Number(config.Value("Grid.OriginLatitude"), 38);
    originLongitude = Number(config.Value("Grid.OriginLongitude"), 127);
    if (fabs(originLatitude) >= 89.999)
        originLatitude = 38;
    if (fabs(originLongitude) > 180)
        originLongitude = 127;
}

bool CMapGridSettings::Save() const
{
    if (!Valid())
        return false;
    auto &config = BaseMapConfig();
    auto root = CPLGetXMLNode(config.xml, "=BaseMap");
    if (!root)
        return false;
    CPLSetXMLValue(root, "Grid.Visible", visible ? "true" : "false");
    CPLSetXMLValue(root, "Grid.MetreInterval", Text(metres).c_str());
    CPLSetXMLValue(root, "Grid.DegreeInterval", Text(degrees).c_str());
    char rgb[16];
    sprintf_s(rgb, "#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    CPLSetXMLValue(root, "Grid.Color", rgb);
    CPLSetXMLValue(root, "Grid.Projection", ProjectionNames[projection + 1]);
    CPLSetXMLValue(root, "Grid.TMEpsg", std::to_string(gridTmEpsg).c_str());
    CPLSetXMLValue(root, "Grid.OriginLatitude", Text(originLatitude).c_str());
    CPLSetXMLValue(root, "Grid.OriginLongitude", Text(originLongitude).c_str());
    CPLSetXMLValue(root, "Projection.TMEpsg", std::to_string(tmEpsg).c_str());
    return CPLSerializeXMLTreeToFile(config.xml, config.file.c_str()) != 0;
}

CMapGridSettings &MapGridSettings()
{
    static CMapGridSettings settings = [] {
        CMapGridSettings value;
        value.Load();
        return value;
    }();
    return settings;
}

std::vector<std::vector<CPoint>> BuildMapGrid(CCoordMgr *mgr, const CRect &viewport,
                                              const CMapGridSettings &settings)
{
    std::vector<std::vector<CPoint>> result;
    if (!mgr || !settings.visible || !settings.Valid() || viewport.IsRectEmpty())
        return result;
    auto selected = GridProjection(settings);
    if (!selected)
        return result;
    auto &projection = *selected;
    double xmin = DBL_MAX, ymin = DBL_MAX, xmax = -DBL_MAX, ymax = -DBL_MAX;
    // Include interior samples when viewport corners lie outside a projection's domain.
    for (int row = 0; row <= 8; ++row)
        for (int col = 0; col <= 8; ++col)
        {
            double wx, wy, x, y;
            mgr->ScreenXY2WorldXY(viewport.left + viewport.Width() * col / 8,
                                  viewport.top + viewport.Height() * row / 8, &wx, &wy);
            if (!projection.WorldToGrid(wx, wy, x, y))
                continue;
            xmin = (std::min)(xmin, x);
            xmax = (std::max)(xmax, x);
            ymin = (std::min)(ymin, y);
            ymax = (std::max)(ymax, y);
        }
    if (xmin == DBL_MAX || xmax <= xmin || ymax <= ymin)
        return result;
    double step = projection.Kind() == 0 ? settings.degrees : settings.metres;
    // Keep major lines on exact multiples of the user's interval at wide zooms.
    const double span = (std::max)(xmax - xmin, ymax - ymin);
    step *= (std::max)(1.0, ceil(span / step / 100.0));
    if (!std::isfinite(step) || step <= 0)
        return result;
    xmin -= step;
    xmax += step;
    ymin -= step;
    ymax += step;
    for (int axis = 0; axis < 2; ++axis)
    {
        const double first = ceil((axis ? ymin : xmin) / step) * step;
        const double last = axis ? ymax : xmax;
        for (int line = 0; line < 104; ++line)
        {
            const double fixed = first + line * step;
            if (fixed > last)
                break;
            std::vector<CPoint> points;
            auto flush = [&] {
                if (points.size() > 1)
                    result.push_back(points);
                points.clear();
            };
            for (int segment = 0; segment <= 16; ++segment)
            {
                const double moving = axis ? xmin + (xmax - xmin) * segment / 16
                                           : ymin + (ymax - ymin) * segment / 16;
                double wx, wy, sx, sy;
                if (!projection.GridToWorld(axis ? moving : fixed, axis ? fixed : moving, wx, wy))
                {
                    flush();
                    continue;
                }
                mgr->WorldXY2ScreenXY(wx, wy, &sx, &sy, mgr->m_fCenterX, mgr->m_fCenterY,
                                      mgr->m_fScale);
                if (!std::isfinite(sx) || !std::isfinite(sy) || fabs(sx) > 10000000 ||
                    fabs(sy) > 10000000)
                {
                    flush();
                    continue;
                }
                if (!points.empty() && hypot(sx - points.back().x, sy - points.back().y) >
                                           2.0 * hypot(viewport.Width(), viewport.Height()))
                    flush();
                points.emplace_back((int)lround(sx), (int)lround(sy));
            }
            flush();
        }
    }
    return result;
}

void DrawMapGrid(CDC *dc, CCoordMgr *mgr, const CRect &viewport)
{
    if (!dc || !dc->GetSafeHdc())
        return;
    const auto &settings = MapGridSettings();
    auto lines = BuildMapGrid(mgr, viewport, settings);
    if (lines.empty())
        return;
    const int saved = dc->SaveDC();
    if (!saved)
        return;
    dc->IntersectClipRect(viewport);
    CPen pen(PS_SOLID, 1, settings.color);
    auto old = dc->SelectObject(&pen);
    dc->SetROP2(R2_COPYPEN);
    for (const auto &line : lines)
        dc->Polyline(line.data(), (int)line.size());
    dc->SelectObject(old);
    dc->RestoreDC(saved);
}

bool CMapGridSettings::ConfigureProjection(CDisplayProjection &target) const
{
    const bool usesOrigin = projection == 1 || projection == 3 || projection == 4;
    return Valid() && target.ConfigureCoordinates(projection, usesOrigin ? originLatitude : 38,
                                                  usesOrigin ? originLongitude : 127, gridTmEpsg);
}
