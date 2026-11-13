#include "pch.h"
#pragma pack(push, 8)
#include "MapAnalysis.h"
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"
#include "proj9/geodesic.h"
#include <gdiplus.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
#pragma comment(lib, "proj9.lib")
#pragma comment(lib, "gdiplus.lib")
namespace
{
constexpr double Pi = 3.14159265358979323846;
constexpr double EarthRadius = 6371008.8;
constexpr int Radials = 360;
constexpr int Steps = 128;
struct VisibleCell
{
    AnalysisPoint corners[4];
};
struct AnalysisResult
{
    std::vector<AnalysisProfileSample> profile;
    std::vector<VisibleCell> visible;
    std::vector<VisibleCell> hidden;
    std::vector<VisibleCell> unknown;
    bool complete = false;
    bool failed = false;
    CString text;
    double observerAltitude = 0;
};
class ElevationSampler
{
  public:
    explicit ElevationSampler(const AnalysisSettings &settings);
    ~ElevationSampler();
    double Sample(AnalysisPoint point, const std::atomic<bool> &cancel);

  private:
    HMODULE dll = nullptr;
    HANDLE wakeTimer = nullptr;
    void *handle = nullptr;
    void (*destroy)(void *) = nullptr;
    uint64_t (*request)(void *, double, double) = nullptr;
    int (*read)(void *, uint64_t, double *) = nullptr;
    int (*waitSample)(void*, uint64_t, double*, uint32_t) = nullptr;
};
ElevationSampler::ElevationSampler(const AnalysisSettings &settings)
{
    wchar_t exe[32768] = {};
    GetModuleFileNameW(nullptr, exe, 32768);
    std::wstring path(exe);
    path.resize(path.find_last_of(L"\\/") + 1);
    bool terrain = settings.sourceKind != 0;
    path += terrain ? L"TerrainTiles.dll" : L"TerrainElevation.dll";
    dll = LoadLibraryW(path.c_str());
    if (!dll)
        throw std::runtime_error("Cannot load elevation DLL");
    destroy = reinterpret_cast<decltype(destroy)>(
        GetProcAddress(dll, terrain ? "TerrainTilesDestroy" : "TerrainDestroy"));
    request = reinterpret_cast<decltype(request)>(
        GetProcAddress(dll, terrain ? "TerrainTilesRequestSample" : "TerrainRequestSample"));
    read = reinterpret_cast<decltype(read)>(
        GetProcAddress(dll, terrain ? "TerrainTilesGetSample" : "TerrainGetSample"));
    auto create = reinterpret_cast<void *(*)(const char *)>(GetProcAddress(dll, "TerrainCreate"));
    auto createTiles =
        reinterpret_cast<void *(*)(const char *, int)>(GetProcAddress(dll, "TerrainTilesCreate"));
    if (terrain)
        waitSample = reinterpret_cast<decltype(waitSample)>(GetProcAddress(dll, "TerrainTilesWaitSample"));
    if (destroy && request && read)
    {
        if (terrain && createTiles)
            handle = createTiles(settings.source.c_str(), settings.sourceKind == 2);
        else if (!terrain && create)
            handle = create(settings.source.c_str());
    }
    if (!handle)
    {
        FreeLibrary(dll);
        dll = nullptr;
        throw std::runtime_error("Cannot open elevation source");
    }
    // A high-resolution wait avoids Windows' coarse Sleep(1) interval for
    // tens of thousands of asynchronous elevation replies. No global timer change.
    wakeTimer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS);
}
ElevationSampler::~ElevationSampler()
{
    if (wakeTimer)
        CloseHandle(wakeTimer);
    if (handle)
        destroy(handle);
    if (dll)
        FreeLibrary(dll);
}
double ElevationSampler::Sample(AnalysisPoint point, const std::atomic<bool> &cancel)
{
    auto ticket = request(handle, point.lon, point.lat);
    if (!ticket)
        throw std::runtime_error("Invalid elevation request");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!cancel)
    {
        double height;
        int result = waitSample ? waitSample(handle, ticket, &height, 25) : read(handle, ticket, &height);
        if (result == 1)
            return std::isfinite(height) ? height : std::numeric_limits<double>::quiet_NaN();
        if (result == 2)
            return std::numeric_limits<double>::quiet_NaN();
        if (result < 0 || std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Elevation lookup failed or timed out");
        if (waitSample)
            continue;
        LARGE_INTEGER due;
        due.QuadPart = -10000; // Relative 1 ms in 100 ns units.
        if (wakeTimer && SetWaitableTimer(wakeTimer, &due, 0, nullptr, nullptr, FALSE))
            WaitForSingleObject(wakeTimer, 10);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Cancelled");
}
bool FromScreen(CPoint point, CCoordMgr *mgr, AnalysisPoint &geo)
{
    if (!mgr)
        return false;
    if (mgr->m_bFlat)
    {
        if (mgr->m_fScale <= 0)
            return false;
        geo.lon = 127.031 + (point.x - mgr->m_fCenterX) / (47.5 * mgr->m_fScale);
        geo.lat = 37.0861 - (point.y - mgr->m_fCenterY) / (60 * mgr->m_fScale);
    }
    else
    {
        double x, y;
        mgr->ScreenXY2WorldXY(point.x, point.y, &x, &y);
        CCoord c;
        c.x_y_to_degrees_lat_long(x, y, &geo.lat, &geo.lon);
    }
    return std::isfinite(geo.lon) && std::isfinite(geo.lat) && fabs(geo.lon) <= 180 &&
           fabs(geo.lat) <= 90;
}
bool ToScreen(AnalysisPoint geo, CCoordMgr *mgr, Gdiplus::PointF &result)
{
    double x, y;
    if (mgr->m_bFlat)
    {
        x = mgr->m_fCenterX + (geo.lon - 127.031) * 47.5 * mgr->m_fScale;
        y = mgr->m_fCenterY - (geo.lat - 37.0861) * 60 * mgr->m_fScale;
    }
    else
    {
        CCoord c;
        double wx, wy;
        c.degrees_lat_long_to_x_y(geo.lat, geo.lon, &wx, &wy);
        mgr->WorldXY2ScreenXY(wx, wy, &x, &y);
    }
    if (!std::isfinite(x) || !std::isfinite(y) || fabs(x) > 1e7 || fabs(y) > 1e7)
        return false;
    result = Gdiplus::PointF(static_cast<float>(x), static_cast<float>(y));
    return true;
}
} // namespace
struct CMapAnalysis::Impl
{
    Mode mode = None;
    AnalysisSettings settings;
    std::vector<AnalysisPoint> points;
    AnalysisPoint hover;
    bool hasHover = false;
    bool collecting = false;
    bool clearRequested = false;
    bool profileReady = false;
    bool errorReady = false;
    std::thread worker;
    std::atomic<bool> cancel{false}, busy{false}, ready{false};
    std::atomic<int> progress{0};
    int shownProgress = -1;
    std::mutex mutex;
    AnalysisResult result, pending;
    void Stop();
    void Compute();
};
void CMapAnalysis::Impl::Stop()
{
    cancel = true;
    if (worker.joinable())
        worker.join();
    busy = false;
    ready = false;
}
void CMapAnalysis::Impl::Compute()
{
    Stop();
    cancel = false;
    busy = true;
    progress = 0;
    result = AnalysisResult();
    auto path = points;
    auto options = settings;
    Mode operation = mode;
    worker = std::thread(
        [this, path, options, operation]()
        {
            AnalysisResult output;
            try
            {
                ElevationSampler sampler(options);
                if (operation == Profile)
                {
                    double total = 0;
                    std::vector<double> cumulative{0};
                    for (size_t i = 1; i < path.size(); ++i)
                    {
                        total += CMapAnalysis::Length(path[i - 1], path[i]);
                        cumulative.push_back(total);
                    }
                    int count =
                        (std::max)(2, (std::min)(2048, static_cast<int>(ceil(total / 30)) + 1));
                    size_t segment = 1;
                    int valid = 0;
                    for (int i = 0; i < count && !cancel; ++i)
                    {
                        double distance = total * i / (count - 1);
                        while (segment + 1 < path.size() && cumulative[segment] < distance)
                            ++segment;
                        double azimuth;
                        CMapAnalysis::Length(path[segment - 1], path[segment], &azimuth);
                        auto location = CMapAnalysis::Destination(
                            path[segment - 1], azimuth, distance - cumulative[segment - 1]);
                        double z = sampler.Sample(location, cancel);
                        valid += std::isfinite(z) ? 1 : 0;
                        output.profile.push_back({distance, z});
                        progress = (i + 1) * 100 / count;
                    }
                    output.text.Format(_T("단면도 %.1f m / 표본 %d개 / 고도 없음 %d개"), total,
                                       count, count - valid);
                }
                else
                {
                    auto origin = path.front();
                    double ground = sampler.Sample(origin, cancel);
                    if (!std::isfinite(ground))
                        throw std::runtime_error("Observer has no elevation data");
                    output.observerAltitude = ground + options.observerHeight;
                    int unknown = 0;
                    double step = options.range / Steps;
                    for (int ray = 0; ray < Radials && !cancel; ++ray)
                    {
                        double horizon = -std::numeric_limits<double>::infinity();
                        bool missing = false;
                        double bearing = ray * 360.0 / Radials;
                        for (int j = 1; j <= Steps && !cancel; ++j)
                        {
                            double distance = j * step;
                            double height = sampler.Sample(
                                CMapAnalysis::Destination(origin, bearing, distance), cancel);
                            VisibleCell cell;
                            double half = 180.0 / Radials;
                            cell.corners[0] =
                                CMapAnalysis::Destination(origin, bearing - half, (j - 1) * step);
                            cell.corners[1] =
                                CMapAnalysis::Destination(origin, bearing - half, distance);
                            cell.corners[2] =
                                CMapAnalysis::Destination(origin, bearing + half, distance);
                            cell.corners[3] =
                                CMapAnalysis::Destination(origin, bearing + half, (j - 1) * step);
                            if (!std::isfinite(height))
                                missing = true;
                            if (missing)
                            {
                                ++unknown;
                                output.unknown.push_back(cell);
                                continue;
                            }
                            double corrected =
                                height - (6.0 / 7.0) * distance * distance / (2 * EarthRadius);
                            double slope = (corrected - output.observerAltitude) / distance;
                            if (slope >= horizon)
                                output.visible.push_back(cell);
                            else
                                output.hidden.push_back(cell);
                            horizon = (std::max)(horizon, slope);
                        }
                        // Publish sectors as they finish instead of leaving the map empty
                        // until every elevation sample has been retrieved.
                        if ((ray + 1) % 12 == 0 && !cancel)
                        {
                            std::lock_guard<std::mutex> guard(mutex);
                            pending = output;
                            ready = true;
                        }
                        progress = (ray + 1) * 100 / Radials;
                    }
                    output.text.Format(_T("가시권: 관측고도 %.1f m (지면 %.1f + 높이 %.1f), 범위 ")
                                       _T("%.0f m / 표본 간격 %.1f m·1° / 미확인 %d개"),
                                       output.observerAltitude, ground, options.observerHeight,
                                       options.range, step, unknown);
                }
            }
            catch (const std::exception &error)
            {
                output = AnalysisResult();
                output.failed = true;
                output.text = _T("지형 분석 실패: ");
                output.text += CString(error.what());
            }
            if (!cancel)
            {
                std::lock_guard<std::mutex> guard(mutex);
                output.complete = true;
                pending = std::move(output);
                ready = true;
            }
            busy = false;
        });
}
CMapAnalysis::CMapAnalysis() : p(new Impl)
{
}
CMapAnalysis::~CMapAnalysis()
{
    p->Stop();
}
void CMapAnalysis::Clear()
{
    if (p->busy)
    {
        p->cancel = true;
        p->collecting = p->hasHover = false;
        p->clearRequested = true;
        return;
    }
    p->Stop();
    p->clearRequested = false;
    p->profileReady = p->errorReady = false;
    p->mode = None;
    p->collecting = p->hasHover = false;
    p->points.clear();
    p->result = AnalysisResult();
}
void CMapAnalysis::Cancel()
{
    // Cancellation is asynchronous; join only after the worker acknowledges it.
    p->cancel = true;
    p->collecting = p->hasHover = false;
}
void CMapAnalysis::Start(Mode mode, const AnalysisSettings &settings)
{
    Clear();
    p->settings = settings;
    p->mode = mode;
    p->collecting = true;
}
bool CMapAnalysis::Active() const
{
    return p->collecting;
}
bool CMapAnalysis::Busy() const
{
    return p->busy;
}
bool CMapAnalysis::Click(CPoint point, CCoordMgr *coord)
{
    if (!Active())
        return false;
    AnalysisPoint geo;
    if (FromScreen(point, coord, geo))
        Place(geo);
    return true;
}
bool CMapAnalysis::Place(AnalysisPoint point)
{
    if (!Active() || !std::isfinite(point.lon) || !std::isfinite(point.lat) ||
        fabs(point.lon) > 180 || fabs(point.lat) > 90)
        return false;
    if (p->points.empty() || Length(p->points.back(), point) > 0.01)
        p->points.push_back(point);
    if (p->mode == Viewshed || p->mode == Bearing)
        Finish();
    return true;
}
bool CMapAnalysis::Move(CPoint point, CCoordMgr *coord)
{
    if (!Active())
        return false;
    p->hasHover = FromScreen(point, coord, p->hover);
    return true;
}
bool CMapAnalysis::Finish()
{
    if (!Active())
        return false;
    size_t minimum = p->mode == Area ? 3 : ((p->mode == Distance || p->mode == Profile) ? 2 : 1);
    if (p->points.size() < minimum)
    {
        p->result.text.Format(_T("최소 %u개 지점을 선택하세요."), static_cast<unsigned>(minimum));
        return true;
    }
    p->collecting = p->hasHover = false;
    if (p->mode == Profile || p->mode == Viewshed)
        p->Compute();
    return true;
}
bool CMapAnalysis::Poll()
{
    bool changed = false;
    if (p->ready.exchange(false))
    {
        std::lock_guard<std::mutex> guard(p->mutex);
        p->result = std::move(p->pending);
        if (p->result.complete && !p->cancel)
        {
            p->profileReady = p->mode == Profile && !p->result.failed;
            p->errorReady = p->result.failed;
        }
        changed = true;
    }
    int progress = p->progress;
    if (p->busy && progress != p->shownProgress)
    {
        p->shownProgress = progress;
        changed = true;
    }
    if (!p->busy && p->worker.joinable())
    {
        p->worker.join();
        if (p->cancel)
            p->result.text = _T("지형 분석을 취소했습니다.");
        changed = true;
    }
    if (p->clearRequested && !p->busy)
    {
        Clear();
        changed = true;
    }
    return changed;
}
bool CMapAnalysis::TakeProfile(std::vector<AnalysisProfileSample> &samples)
{
    if (!p->profileReady)
        return false;
    p->profileReady = false;
    samples = p->result.profile;
    return true;
}
bool CMapAnalysis::TakeError(CString &error)
{
    if (!p->errorReady)
        return false;
    p->errorReady = false;
    error = p->result.text;
    return true;
}
double CMapAnalysis::Length(AnalysisPoint a, AnalysisPoint b, double *bearing)
{
    geod_geodesic geodesic;
    geod_init(&geodesic, 6378137.0, 1 / 298.257223563);
    double distance, azimuth;
    geod_inverse(&geodesic, a.lat, a.lon, b.lat, b.lon, &distance, &azimuth, nullptr);
    if (bearing)
        *bearing = fmod(azimuth + 360, 360);
    return distance;
}
double CMapAnalysis::PolygonArea(const std::vector<AnalysisPoint> &points)
{
    geod_geodesic geodesic;
    geod_init(&geodesic, 6378137.0, 1 / 298.257223563);
    geod_polygon polygon;
    geod_polygon_init(&polygon, 0);
    for (auto point : points)
        geod_polygon_addpoint(&geodesic, &polygon, point.lat, point.lon);
    double area;
    geod_polygon_compute(&geodesic, &polygon, 0, 1, &area, nullptr);
    return fabs(area);
}
AnalysisPoint CMapAnalysis::Destination(AnalysisPoint start, double bearing, double metres)
{
    geod_geodesic geodesic;
    geod_init(&geodesic, 6378137.0, 1 / 298.257223563);
    AnalysisPoint result;
    geod_direct(&geodesic, start.lat, start.lon, bearing, metres, &result.lat, &result.lon,
                nullptr);
    return result;
}
void CMapAnalysis::Draw(CDC *dc, CCoordMgr *coord, CRect rect)
{
    if (!dc || !coord || p->mode == None)
        return;

    const int saved = dc->SaveDC();
    dc->IntersectClipRect(rect);
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(RGB(20, 20, 20));
    Gdiplus::Graphics graphics(dc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    COLORREF color = p->settings.color;
    Gdiplus::Color ink(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(ink, static_cast<float>((std::max)(2, p->settings.width)));

    auto label = [&](CString text, Gdiplus::PointF point)
    {
        graphics.Flush(Gdiplus::FlushIntentionSync);
        CSize extent = dc->GetTextExtent(text);
        int x = (std::max<LONG>)(rect.left + 2,
                           (std::min<LONG>)(rect.right - extent.cx - 6, static_cast<int>(point.X)));
        int y = (std::max<LONG>)(rect.top + 2,
                           (std::min<LONG>)(rect.bottom - extent.cy - 6, static_cast<int>(point.Y)));
        dc->FillSolidRect(x - 2, y - 1, extent.cx + 4, extent.cy + 2, RGB(255, 255, 245));
        dc->TextOut(x, y, text);
    };
    auto marker = [&](AnalysisPoint location)
    {
        Gdiplus::PointF point;
        if (!ToScreen(location, coord, point))
            return;
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
        graphics.FillEllipse(&white, point.X - 4, point.Y - 4, 8.f, 8.f);
        graphics.DrawEllipse(&pen, point.X - 4, point.Y - 4, 8.f, 8.f);
    };

    COLORREF hiddenColor =
        GetRValue(color) > GetGValue(color) + 40 ? RGB(35, 60, 150) : RGB(170, 35, 35);
    auto drawCells = [&](const std::vector<VisibleCell> &cells, COLORREF rgb)
    {
        Gdiplus::GraphicsPath polygons(Gdiplus::FillModeWinding);
        for (const auto &cell : cells)
        {
            Gdiplus::PointF points[4];
            bool valid = true;
            for (int i = 0; i < 4; ++i)
                valid = ToScreen(cell.corners[i], coord, points[i]) && valid;
            if (!valid)
                continue;
            float left = points[0].X, right = left, top = points[0].Y, bottom = top;
            for (int i = 1; i < 4; ++i)
            {
                left = (std::min)(left, points[i].X);
                right = (std::max)(right, points[i].X);
                top = (std::min)(top, points[i].Y);
                bottom = (std::max)(bottom, points[i].Y);
            }
            if (right < rect.left || left > rect.right || bottom < rect.top || top > rect.bottom)
                continue;
            polygons.AddPolygon(points, 4);
        }
        Gdiplus::SolidBrush brush(
            Gdiplus::Color(120, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)));
        graphics.FillPath(&brush, &polygons);
    };
    if (p->mode == Viewshed)
    {
        drawCells(p->result.hidden, hiddenColor);
        drawCells(p->result.unknown, RGB(120, 120, 120));
        drawCells(p->result.visible, color);
    }

    auto path = p->points;
    if (Active() && p->hasHover && !path.empty() && p->mode != Viewshed && p->mode != Bearing)
        path.push_back(p->hover);

    std::vector<Gdiplus::PointF> polygon;
    for (auto location : path)
    {
        Gdiplus::PointF point;
        if (ToScreen(location, coord, point))
            polygon.push_back(point);
    }
    if (p->mode == Area && polygon.size() >= 3 && polygon.size() == path.size())
    {
        Gdiplus::SolidBrush fill(
            Gdiplus::Color(65, GetRValue(color), GetGValue(color), GetBValue(color)));
        graphics.FillPolygon(&fill, polygon.data(), static_cast<int>(polygon.size()));
        graphics.DrawPolygon(&pen, polygon.data(), static_cast<int>(polygon.size()));
    }

    double total = 0;
    for (size_t i = 1; i < path.size(); ++i)
    {
        Gdiplus::PointF a, b;
        double length = Length(path[i - 1], path[i]);
        total += length;
        if (ToScreen(path[i - 1], coord, a) && ToScreen(path[i], coord, b))
        {
            if (p->mode != Area || polygon.size() < 3)
                graphics.DrawLine(&pen, a, b);
            if (p->mode == Distance)
            {
                CString text;
                text.Format(_T("%.1f m"), length);
                label(text, {(a.X + b.X) / 2 + 6, (a.Y + b.Y) / 2 + 6});
            }
        }
    }
    for (auto point : p->points)
        marker(point);

    if (p->mode == Distance && path.size() >= 2)
    {
        Gdiplus::PointF point;
        if (ToScreen(path.back(), coord, point))
        {
            CString text;
            text.Format(_T("총 거리 %.1f m / %.3f km"), total, total / 1000);
            label(text, {point.X + 10, point.Y + 16});
        }
    }
    if (p->mode == Area && polygon.size() >= 3)
    {
        Gdiplus::PointF center(0, 0);
        for (auto point : polygon)
        {
            center.X += point.X;
            center.Y += point.Y;
        }
        center.X /= static_cast<float>(polygon.size());
        center.Y /= static_cast<float>(polygon.size());
        double area = PolygonArea(path);
        CString text;
        text.Format(_T("면적 %.1f m² / %.4f km²"), area, area / 1e6);
        center.X -= dc->GetTextExtent(text).cx / 2.f;
        label(text, center);
    }

    if ((p->mode == Bearing || p->mode == Viewshed) && !path.empty())
    {
        double interval = p->mode == Bearing ? p->settings.interval : p->settings.range;
        if (interval > 0 && p->settings.range > 0)
        {
            int rings = (std::min)(100, static_cast<int>(ceil(p->settings.range / interval)));
            for (int ring = 1; ring <= rings; ++ring)
            {
                double radius = (std::min)(ring * interval, p->settings.range);
                Gdiplus::PointF previous;
                bool previousValid = false;
                for (int angle = 0; angle <= 360; angle += 2)
                {
                    Gdiplus::PointF point;
                    bool valid = ToScreen(Destination(path.front(), angle, radius), coord, point);
                    if (valid && previousValid)
                        graphics.DrawLine(&pen, previous, point);
                    previous = point;
                    previousValid = valid;
                }
                Gdiplus::PointF point;
                if (ToScreen(Destination(path.front(), 90, radius), coord, point))
                {
                    CString text;
                    text.Format(_T("%.0f m"), radius);
                    label(text, {point.X + 4, point.Y + 18});
                }
            }
            if (p->mode == Bearing)
            {
                for (int angle = 0; angle < 360; angle += 30)
                {
                    Gdiplus::PointF a, b;
                    if (ToScreen(path.front(), coord, a) &&
                        ToScreen(Destination(path.front(), angle, p->settings.range), coord, b))
                    {
                        graphics.DrawLine(&pen, a, b);
                        CString text;
                        text.Format(_T("%03d°"), angle);
                        label(text, {b.X + 4, b.Y - 5});
                    }
                }
            }
        }
        marker(path.front());
        if (p->mode == Viewshed &&
            (!p->result.visible.empty() || !p->result.hidden.empty() || !p->result.unknown.empty()))
        {
            graphics.Flush(Gdiplus::FlushIntentionSync);
            int x = rect.left + 12, y = rect.bottom - 30;
            COLORREF colors[] = {color, hiddenColor, RGB(120, 120, 120)};
            LPCTSTR names[] = {_T("가시"), _T("비가시"), _T("고도 없음")};
            for (int i = 0; i < 3; ++i)
            {
                dc->FillSolidRect(x, y, 14, 14, colors[i]);
                label(names[i], {static_cast<float>(x + 18), static_cast<float>(y - 1)});
                x += 105;
            }
        }
    }
    graphics.Flush(Gdiplus::FlushIntentionSync);
    dc->RestoreDC(saved);
}
#pragma pack(pop)
