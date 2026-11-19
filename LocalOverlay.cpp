#include "pch.h"
#pragma pack(push, 8)
#include "LocalOverlay.h"
#include "OverlayRibbonRender.h"
#include <vector>
#include <memory>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>
#include <algorithm>
#include <set>
#include <map>
#include <atlconv.h>
#include "cpl_string.h"
#include <gdiplus.h>
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"
#include "Overlay/ObjectLine.h"
#include "Overlay/ObjectRect.h"
#include "Overlay/ObjectEllipse.h"
#include "Overlay/ObjectLineDraw.h"
#include "Overlay/ObjectRectDraw.h"
#include "Overlay/ObjectEllipseDraw.h"
#include "cpl_minixml.h"
#pragma comment(lib, "gdiplus.lib")

namespace
{
// Stable clipboard payload; no pointers or STL objects cross the clipboard.
struct OverlayClipboardObject
{
    unsigned int version;
    int group;
    int type;
    double x1, y1, x2, y2;
    CLocalOverlay::Style style;
    CLocalOverlay::Geometry geometry;
};
std::string GeometryText(const CLocalOverlay::Geometry &geometry)
{
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::setprecision(17);
    for (int n = 0; n < geometry.count; ++n)
        text << geometry.x[n] << " " << geometry.y[n] << " ";
    return text.str();
}
UINT OverlayClipboardFormat()
{
    return RegisterClipboardFormat(_T("C4IMap.OverlayObject.GeoDCWorld.v4"));
}
struct OverlayCoordinateRestore
{
  public:
    CCoordMgr *value;
    ~OverlayCoordinateRestore()
    {
        OVERLAY::Point::g_pCoord = value;
    }
};
} // namespace

struct CLocalOverlay::Impl
{
  public:
    struct Item
    {
      public:
        int group = 0;
        int layer = 0;
        int type = Line;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        Style style;
        Geometry geometry;
    };

    struct Layer
    {
      public:
        int id;
        CString name;
        bool visible = true;
    };

    struct Snapshot
    {
      public:
        int layer;
        std::vector<Item> items;
    };

    std::vector<Layer> layers;
    int active = 0, next = 1;
    std::vector<Item> items;
    std::vector<Item> undone;
    Item pending, original;
    int selected = -1, handle = -1;
    std::set<int> selection;
    int nextGroup = 1;
    DWORD clipboardSequence = 0;
    unsigned int pasteCount = 0;
    bool editing = false;

    std::vector<Snapshot> history, future;
    Snapshot Capture()
    {
        Snapshot s;
        s.layer = active;
        for (auto &i : items)
            if (i.layer == active)
                s.items.push_back(i);
        return s;
    }
    void Remember()
    {
        history.push_back(Capture());
        // Bound snapshot memory when many detailed paths are selected.
        size_t bytes = 0;
        for (const auto &snapshot : history)
            bytes += snapshot.items.size() * sizeof(Item);
        while (history.size() > 1 && (history.size() > 64 || bytes > 128 * 1024 * 1024))
        {
            bytes -= history.front().items.size() * sizeof(Item);
            history.erase(history.begin());
        }

        future.erase(std::remove_if(future.begin(), future.end(),
                                    [&](const Snapshot &s) { return s.layer == active; }),
                     future.end());
    }

    void Restore(const Snapshot &s)
    {
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [&](const Item &i) { return i.layer == s.layer; }),
                    items.end());
        items.insert(items.end(), s.items.begin(), s.items.end());
        selected = -1;
        selection.clear();
    }

    static bool Screen(const Item &i, CCoordMgr *c, double &x1, double &y1, double &x2, double &y2)
    {
        c->WorldXY2ScreenXY(i.x1, i.y1, &x1, &y1);
        c->WorldXY2ScreenXY(i.x2, i.y2, &x2, &y2);
        return std::isfinite(x1) && std::isfinite(y1) && std::isfinite(x2) && std::isfinite(y2) &&
               fabs(x1) < 1e7 && fabs(y1) < 1e7 && fabs(x2) < 1e7 && fabs(y2) < 1e7;
    }

    static CPoint Handle(int n, double l, double t, double r, double b)
    {
        const double xs[] = {l, (l + r) / 2, r, r, r, (l + r) / 2, l, l};
        const double ys[] = {t, t, t, (t + b) / 2, b, b, b, (t + b) / 2};
        return CPoint((int)xs[n], (int)ys[n]);
    }

    int tool = Pan;
    bool drawing = false;
    bool modified = false;
    CPoint start;
    Style style;
    // The owning C4IMap application manages the GDI+ lifetime.
};

CLocalOverlay::CLocalOverlay() : p(new Impl)
{
}
CLocalOverlay::~CLocalOverlay()
{
    delete p;
}

void CLocalOverlay::SetTool(int tool)
{
    Cancel();
    p->selected = -1;
    p->selection.clear();
    p->tool = tool >= Pan && tool <= ClosedSpline ? tool : Pan;
}

int CLocalOverlay::GetTool() const
{
    return p->tool;
}

bool CLocalOverlay::IsDrawing() const
{
    return p->drawing;
}

void CLocalOverlay::Cancel()
{
    p->drawing = false;
    p->editing = false;
}

size_t CLocalOverlay::Count() const
{
    return p->items.size();
}

COLORREF CLocalOverlay::Color() const
{
    return GetStyle().lineColor;
}

void CLocalOverlay::SetColor(COLORREF c)
{
    auto style = GetStyle();
    style.lineColor = c;
    style.lineEnabled = true;
    SetStyle(style);
}

void CLocalOverlay::Undo()
{
    Cancel();
    auto it = std::find_if(p->history.rbegin(), p->history.rend(),
                           [&](const Impl::Snapshot &s) { return s.layer == p->active; });
    if (it != p->history.rend())
    {
        auto state = *it;
        p->history.erase(std::next(it).base());
        p->future.push_back(p->Capture());
        p->Restore(state);
        p->modified = true;
    }
    else
    {
        auto i = std::find_if(p->items.rbegin(), p->items.rend(),
                              [&](const Impl::Item &i) { return i.layer == p->active; });
        if (i != p->items.rend())
        {
            p->future.push_back(p->Capture());
            p->items.erase(std::next(i).base());
            p->selected = -1;
            p->selection.clear();
            p->modified = true;
        }
    }
}

bool CLocalOverlay::CanRedo() const
{
    return std::any_of(p->future.begin(), p->future.end(),
                       [&](const Impl::Snapshot &s) { return s.layer == p->active; });
}

void CLocalOverlay::Redo()
{
    Cancel();
    auto it = std::find_if(p->future.rbegin(), p->future.rend(),
                           [&](const Impl::Snapshot &s) { return s.layer == p->active; });
    if (it != p->future.rend())
    {
        auto state = *it;
        p->future.erase(std::next(it).base());
        p->history.push_back(p->Capture());
        p->Restore(state);
        p->modified = true;
    }
}

bool CLocalOverlay::IsModified() const
{
    return p->modified;
}

void CLocalOverlay::Clear()
{
    Cancel();
    p->selected = -1;
    p->selection.clear();
    p->history.clear();
    p->future.clear();
    p->undone.clear();
    p->items.clear();
    p->layers.clear();
    p->active = 0;
    p->next = 1;
    p->modified = false;
    p->tool = Pan;
}

bool CLocalOverlay::Begin(CPoint pt, CCoordMgr *c)
{
    if (!c)
        return false;

    if (p->tool == Pan)
    {
        auto layer = std::find_if(p->layers.begin(), p->layers.end(), [&](const Impl::Layer &l) {
            return l.id == p->active && l.visible;
        });
        if (layer == p->layers.end())
        {
            p->selected = -1;
            p->selection.clear();
            return false;
        }
        p->handle = -1;
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && p->selected >= 0 &&
            p->selected < (int)p->items.size())
        {
            double x1, y1, x2, y2;
            if (Impl::Screen(p->items[p->selected], c, x1, y1, x2, y2))
            {
                for (int h = 0; h < 8; ++h)
                {
                    auto q = Impl::Handle(h, (std::min)(x1, x2), (std::min)(y1, y2),
                                          (std::max)(x1, x2), (std::max)(y1, y2));
                    if (abs(pt.x - q.x) <= 6 && abs(pt.y - q.y) <= 6)
                    {
                        p->handle = h;
                        break;
                    }
                }
            }
        }
        if (p->handle < 0)
        {
            int oldSelected = p->selected;
            auto oldSelection = p->selection;
            p->selected = -1;
            for (int i = (int)p->items.size() - 1; i >= 0; --i)
            {
                auto &item = p->items[i];
                if (item.layer != p->active)
                    continue;
                double a, b, d, e;
                if (!Impl::Screen(item, c, a, b, d, e))
                    continue;
                CPoint testPoint = pt;
                if (item.style.rotation || item.style.flipX || item.style.flipY)
                {
                    double cx = (a + d) / 2, cy = (b + e) / 2, u = pt.x - cx, v = pt.y - cy;
                    for (int k = 0; k < item.style.rotation; ++k)
                    {
                        double old = u;
                        u = v;
                        v = -old;
                    }
                    if (item.style.flipX)
                        u = -u;
                    if (item.style.flipY)
                        v = -v;
                    testPoint = CPoint((int)lround(cx + u), (int)lround(cy + v));
                }
                bool hit = false;
                if (item.type == Line || item.type == Arrow)
                {
                    double dx = d - a, dy = e - b, len = dx * dx + dy * dy;
                    double t = len > 0 ? (std::max)(0.0, (std::min)(1.0, ((testPoint.x - a) * dx +
                                                                          (testPoint.y - b) * dy) /
                                                                             len))
                                       : 0;
                    hit = hypot(testPoint.x - a - t * dx, testPoint.y - b - t * dy) <= 7;
                }
                else if (item.type == Ellipse)
                {
                    double rx = fabs(d - a) / 2, ry = fabs(e - b) / 2;
                    if (rx > 0 && ry > 0)
                        hit = pow((testPoint.x - (a + d) / 2) / (rx + 6), 2) +
                                  pow((testPoint.y - (b + e) / 2) / (ry + 6), 2) <=
                              1;
                }
                else
                    hit = testPoint.x >= (std::min)(a, d) - 6 &&
                          testPoint.x <= (std::max)(a, d) + 6 &&
                          testPoint.y >= (std::min)(b, e) - 6 &&
                          testPoint.y <= (std::max)(b, e) + 6;
                if (hit)
                {
                    p->selected = i;
                    break;
                }
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
            {
                p->selection = oldSelection;
                if (p->selected >= 0)
                {
                    if (p->selection.count(p->selected))
                        p->selection.erase(p->selected);
                    else
                        p->selection.insert(p->selected);
                }
                p->selected = p->selection.empty() ? -1 : *p->selection.rbegin();
                if (p->selected >= 0)
                {
                    p->original = p->pending = p->items[p->selected];
                    p->start = pt;
                    p->editing = p->drawing = true;
                }
                return true;
            }
            if (p->selected >= 0 && oldSelection.count(p->selected))
                p->selection = oldSelection;
            else
                p->selection.clear();
        }
        if (p->selected < 0)
        {
            p->selection.clear();
            return false;
        }
        p->selection.insert(p->selected);
        if (p->items[p->selected].group)
            for (int n = 0; n < (int)p->items.size(); ++n)
                if (p->items[n].layer == p->active &&
                    p->items[n].group == p->items[p->selected].group)
                    p->selection.insert(n);
        p->original = p->pending = p->items[p->selected];
        p->start = pt;
        p->editing = p->drawing = true;
        return true;
    }
    p->selected = -1;
    p->selection.clear();
    if (!p->active)
        AddLayer();
    auto layer = std::find_if(p->layers.begin(), p->layers.end(),
                              [&](const Impl::Layer &l) { return l.id == p->active; });
    if (layer == p->layers.end() || !layer->visible)
        return false;
    p->pending = Impl::Item();
    p->pending.layer = p->active;
    p->pending.type = p->tool;
    p->pending.style = p->style;
    if (p->tool == Arrow && !p->pending.style.endArrow)
        p->pending.style.endArrow = 1;
    c->ScreenXY2WorldXY(pt.x, pt.y, &p->pending.x1, &p->pending.y1);
    p->pending.x2 = p->pending.x1;
    p->pending.y2 = p->pending.y1;
    if (p->tool >= Polyline)
    {
        p->pending.geometry.count = 1;
        p->pending.geometry.x[0] = p->pending.x1;
        p->pending.geometry.y[0] = p->pending.y1;
    }
    p->start = pt;
    p->drawing = true;
    return true;
}

bool CLocalOverlay::Move(CPoint pt, CCoordMgr *c)
{
    if (!c || !p->drawing)
        return false;
    if (p->editing)
    {
        double x1, y1, x2, y2;
        if (!Impl::Screen(p->original, c, x1, y1, x2, y2))
            return true;
        double dx = pt.x - p->start.x, dy = pt.y - p->start.y;
        if (p->handle < 0)
        {
            x1 += dx;
            x2 += dx;
            y1 += dy;
            y2 += dy;
        }
        else
        {
            double l = (std::min)(x1, x2), r = (std::max)(x1, x2), t = (std::min)(y1, y2),
                   b = (std::max)(y1, y2);
            double nl = l, nr = r, nt = t, nb = b;
            int h = p->handle;
            if (h == 0 || h == 6 || h == 7)
                nl = (std::min)(r - 2, l + dx);
            if (h == 2 || h == 3 || h == 4)
                nr = (std::max)(l + 2, r + dx);
            if (h == 0 || h == 1 || h == 2)
                nt = (std::min)(b - 2, t + dy);
            if (h == 4 || h == 5 || h == 6)
                nb = (std::max)(t + 2, b + dy);
            bool firstLeft = x1 <= x2;
            x1 = firstLeft ? nl : nr;
            x2 = firstLeft ? nr : nl;
            bool firstTop = y1 <= y2;
            y1 = firstTop ? nt : nb;
            y2 = firstTop ? nb : nt;
        }
        Impl::Item candidate = p->pending;
        if (candidate.geometry.count)
        {
            double a, b, d, e;
            Impl::Screen(p->original, c, a, b, d, e);
            for (int n = 0; n < candidate.geometry.count; ++n)
            {
                double u, v;
                c->WorldXY2ScreenXY(p->original.geometry.x[n], p->original.geometry.y[n], &u, &v);
                u = fabs(d - a) < 1e-6 ? u + x1 - a : x1 + (u - a) * (x2 - x1) / (d - a);
                v = fabs(e - b) < 1e-6 ? v + y1 - b : y1 + (v - b) * (y2 - y1) / (e - b);
                c->ScreenXY2WorldXY(u, v, &candidate.geometry.x[n], &candidate.geometry.y[n]);
            }
        }

        c->ScreenXY2WorldXY((int)x1, (int)y1, &candidate.x1, &candidate.y1);
        c->ScreenXY2WorldXY((int)x2, (int)y2, &candidate.x2, &candidate.y2);
        if (std::isfinite(candidate.x1) && std::isfinite(candidate.y1) &&
            std::isfinite(candidate.x2) && std::isfinite(candidate.y2))
            p->pending = candidate;
        return true;
    }
    if (p->tool >= Polyline)
    {
        auto &geometry = p->pending.geometry;
        double sx, sy;
        c->WorldXY2ScreenXY(geometry.x[geometry.count - 1], geometry.y[geometry.count - 1], &sx,
                            &sy);
        if (hypot(pt.x - sx, pt.y - sy) >= 2)
        {
            int n = (std::min)(geometry.count, 511);
            c->ScreenXY2WorldXY(pt.x, pt.y, &geometry.x[n], &geometry.y[n]);
            if (geometry.count < 512)
                ++geometry.count;
            double left = 1e30, top = 1e30, right = -1e30, bottom = -1e30;
            for (int j = 0; j < geometry.count; ++j)
            {
                c->WorldXY2ScreenXY(geometry.x[j], geometry.y[j], &sx, &sy);
                left = (std::min)(left, sx);
                right = (std::max)(right, sx);
                top = (std::min)(top, sy);
                bottom = (std::max)(bottom, sy);
            }
            c->ScreenXY2WorldXY(left, top, &p->pending.x1, &p->pending.y1);
            c->ScreenXY2WorldXY(right, bottom, &p->pending.x2, &p->pending.y2);
        }
        return true;
    }
    c->ScreenXY2WorldXY(pt.x, pt.y, &p->pending.x2, &p->pending.y2);
    return true;
}

bool CLocalOverlay::End(CPoint pt, CCoordMgr *c)
{
    if (!Move(pt, c))
        return false;
    if (abs(pt.x - p->start.x) + abs(pt.y - p->start.y) >= 4 ||
        (!p->editing && p->pending.geometry.count >= 3))
    {
        if (std::isfinite(p->pending.x1) && std::isfinite(p->pending.y1) &&
            std::isfinite(p->pending.x2) && std::isfinite(p->pending.y2))
        {
            p->Remember();
            if (p->editing)
            {
                double ax, ay, bx, by, nx, ny, mx, my;
                Impl::Screen(p->original, c, ax, ay, bx, by);
                Impl::Screen(p->pending, c, nx, ny, mx, my);
                for (int index : p->selection)
                    if (index != p->selected && index >= 0 && index < (int)p->items.size())
                    {
                        auto &other = p->items[index];
                        double u, v, w, z;
                        if (!Impl::Screen(other, c, u, v, w, z))
                            continue;
                        auto transform = [&](double &x, double &y) {
                            x = p->handle < 0 || fabs(bx - ax) < 1e-6
                                    ? x + nx - ax
                                    : nx + (x - ax) * (mx - nx) / (bx - ax);
                            y = p->handle < 0 || fabs(by - ay) < 1e-6
                                    ? y + ny - ay
                                    : ny + (y - ay) * (my - ny) / (by - ay);
                        };
                        for (int n = 0; n < other.geometry.count; ++n)
                        {
                            double gx, gy;
                            c->WorldXY2ScreenXY(other.geometry.x[n], other.geometry.y[n], &gx, &gy);
                            transform(gx, gy);
                            c->ScreenXY2WorldXY(gx, gy, &other.geometry.x[n], &other.geometry.y[n]);
                        }
                        transform(u, v);
                        transform(w, z);
                        c->ScreenXY2WorldXY(u, v, &other.x1, &other.y1);
                        c->ScreenXY2WorldXY(w, z, &other.x2, &other.y2);
                    }
                p->items[p->selected] = p->pending;
            }
            else
            {
                p->items.push_back(p->pending);
                p->selected = (int)p->items.size() - 1;
                p->selection = {p->selected};
            }
            p->modified = true;
        }
    }
    p->drawing = p->editing = false;
    return true;
}

void CLocalOverlay::Draw(CDC *dc, CCoordMgr *c)
{
    if (!dc || !c || !dc->GetSafeHdc())
        return;
    Gdiplus::Graphics g(dc->GetSafeHdc());
    if (g.GetLastStatus() != Gdiplus::Ok)
        return;
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    // C4IMap owns projection. Supply screen coordinates to SDK objects without
    // its global Point coordinate manager (which uses a different projection).
    auto previous = OVERLAY::Point::g_pCoord;
    OVERLAY::Point::g_pCoord = NULL;
    OverlayCoordinateRestore restore{previous};

    auto draw = [&](const Impl::Item &item) {
        double x1, y1, x2, y2;
        c->WorldXY2ScreenXY(item.x1, item.y1, &x1, &y1);
        c->WorldXY2ScreenXY(item.x2, item.y2, &x2, &y2);
        if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2))
            return;

        std::vector<Gdiplus::PointF> points;
        for (int n = 0; n < item.geometry.count; ++n)
        {
            double sx, sy;
            c->WorldXY2ScreenXY(item.geometry.x[n], item.geometry.y[n], &sx, &sy);
            if (std::isfinite(sx) && std::isfinite(sy))
                points.push_back({(float)sx, (float)sy});
        }
        OverlayRibbonRender::Draw(g, item.type, x1, y1, x2, y2, item.style, points);
    };
    for (const auto &layer : p->layers)
        if (layer.visible)
            for (size_t index = 0; index < p->items.size(); ++index)
            {
                auto &item = p->items[index];
                if (item.layer == layer.id && !(p->editing && (int)index == p->selected))
                    draw(item);
            }
    if (p->drawing)
        draw(p->pending);
    for (int index : p->selection)
        if (index != p->selected && index >= 0 && index < (int)p->items.size())
        {
            double a, b, d, e;
            if (Impl::Screen(p->items[index], c, a, b, d, e))
            {
                Gdiplus::Pen edge(Gdiplus::Color(255, 0, 180, 255), 1.f);
                edge.SetDashStyle(Gdiplus::DashStyleDash);
                g.DrawRectangle(&edge, (float)(std::min)(a, d), (float)(std::min)(b, e),
                                (float)(std::max)(1., fabs(d - a)),
                                (float)(std::max)(1., fabs(e - b)));
            }
        }
    if (p->selected >= 0 && p->selected < (int)p->items.size())
    {
        const auto &selected = p->editing ? p->pending : p->items[p->selected];
        bool visible = false;
        for (auto &l : p->layers)
            if (l.id == selected.layer && l.visible && l.id == p->active)
                visible = true;
        double x1, y1, x2, y2;
        if (visible && Impl::Screen(selected, c, x1, y1, x2, y2))
        {
            float l = (float)(std::min)(x1, x2), t = (float)(std::min)(y1, y2),
                  r = (float)(std::max)(x1, x2), b = (float)(std::max)(y1, y2);
            Gdiplus::Pen border(Gdiplus::Color(255, 0, 180, 255), 1);
            border.SetDashStyle(Gdiplus::DashStyleDash);
            g.DrawRectangle(&border, l, t, (std::max)(1.f, r - l), (std::max)(1.f, b - t));
            Gdiplus::SolidBrush fill(Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::Pen edge(Gdiplus::Color(255, 0, 100, 220), 1);
            for (int h = 0; h < 8; ++h)
            {
                auto q = Impl::Handle(h, l, t, r, b);
                g.FillRectangle(&fill, q.x - 4, q.y - 4, 8, 8);
                g.DrawRectangle(&edge, q.x - 4, q.y - 4, 8, 8);
            }
        }
    }
}

bool CLocalOverlay::Save(LPCTSTR path, CString &error) const
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<C4IMapOverlay version=\"4\" "
           "coordinates=\"GeoDC-world\" active=\""
        << p->active << "\">\n";
    for (const auto &layer : p->layers)
    {
        CStringW wide(layer.name);
        CW2A utf8(wide, CP_UTF8);
        char *escaped = CPLEscapeString(utf8, -1, CPLES_XML);
        out << "  <Layer id=\"" << layer.id << "\" name=\"" << escaped << "\" visible=\""
            << (layer.visible ? 1 : 0) << "\">\n";
        CPLFree(escaped);
        for (const auto &i : p->items)
            if (i.layer == layer.id)
                out << "    <Object type=\"" << i.type << "\" x1=\"" << i.x1 << "\" y1=\"" << i.y1
                    << "\" x2=\"" << i.x2 << "\" y2=\"" << i.y2 << "\" color=\""
                    << i.style.lineColor << "\" rotation=\"" << i.style.rotation << "\" flipX=\""
                    << i.style.flipX << "\" flipY=\"" << i.style.flipY << "\" points=\""
                    << GeometryText(i.geometry) << "\" group=\"" << i.group << "\" lineColor=\""
                    << i.style.lineColor << "\" fillColor=\"" << i.style.fillColor
                    << "\" patternColor=\"" << i.style.patternColor << "\" width=\""
                    << i.style.width << "\" lineAlpha=\"" << i.style.lineAlpha << "\" fillAlpha=\""
                    << i.style.fillAlpha << "\" dash=\"" << i.style.dash << "\" fillType=\""
                    << i.style.fillType << "\" hatch=\"" << i.style.hatch << "\" lineEnabled=\""
                    << i.style.lineEnabled << "\" fillEnabled=\"" << i.style.fillEnabled
                    << "\" startArrow=\"" << i.style.startArrow << "\" endArrow=\""
                    << i.style.endArrow << "\" lineCap=\"" << i.style.lineCap << "\" lineJoin=\""
                    << i.style.lineJoin << "\" lineAlignment=\"" << i.style.lineAlignment
                    << "\" lineStripe=\"" << i.style.lineStripe << "\" gradientMode=\""
                    << i.style.gradientMode << "\" gradientPosition=\"" << i.style.gradientPosition
                    << "\" gradientColor=\"" << i.style.gradientColor << "\" textColor=\""
                    << i.style.textColor << "\" fontSize=\"" << i.style.fontSize
                    << "\" fontStyle=\"" << i.style.fontStyle << "\" textAlign=\""
                    << i.style.textAlign << "\" textAlignY=\"" << i.style.textAlignY
                    << "\" lineInterval=\"" << i.style.lineInterval << "\" vertical=\""
                    << i.style.vertical << "\" rightToLeft=\"" << i.style.rightToLeft
                    << "\" autoTextBox=\"" << i.style.autoTextBox << "\" text=\""
                    << OverlayRibbonRender::Escape(i.style.text) << "\" font=\""
                    << OverlayRibbonRender::Escape(i.style.font) << "\" imagePath=\""
                    << OverlayRibbonRender::Escape(i.style.imagePath) << "\"/>\n";
        out << "  </Layer>\n";
    }
    out << "</C4IMapOverlay>\n";
    CString temp(path);
    temp += _T(".tmp");
    try
    {
        CFile file(temp, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive);
        auto s = out.str();
        file.Write(s.data(), (UINT)s.size());
        file.Flush();
        file.Close();
    }
    catch (CFileException *e)
    {
        TCHAR msg[512];
        e->GetErrorMessage(msg, 512);
        error = msg;
        e->Delete();
        DeleteFile(temp);
        return false;
    }
    if (!MoveFileEx(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        error = _T("Overlay XML replacement failed.");
        DeleteFile(temp);
        return false;
    }
    p->modified = false;
    return true;
}
bool CLocalOverlay::Load(LPCTSTR path, CString &error)
{
    std::string bytes;
    try
    {
        CFile file(path, CFile::modeRead | CFile::shareDenyWrite);
        auto len = file.GetLength();
        if (len == 0 || len > 8 * 1024 * 1024)
        {
            error = _T("Overlay XML is too large.");
            return false;
        }
        bytes.resize((size_t)len);
        if (file.Read(&bytes[0], (UINT)len) != len)
        {
            error = _T("Overlay XML read failed.");
            return false;
        }
    }
    catch (CFileException *e)
    {
        TCHAR msg[512];
        e->GetErrorMessage(msg, 512);
        error = msg;
        e->Delete();
        return false;
    }
    std::unique_ptr<CPLXMLNode, decltype(&CPLDestroyXMLNode)> tree(CPLParseXMLString(bytes.c_str()),
                                                                   CPLDestroyXMLNode);
    CPLXMLNode *root = nullptr;
    for (auto n = tree.get(); n; n = n->psNext)
        if (n->eType == CXT_Element && strcmp(n->pszValue, "C4IMapOverlay") == 0)
            root = n;
    auto fail = [&]() {
        error = _T("Unsupported or invalid C4IMap Overlay XML.");
        return false;
    };
    if (!root || strcmp(CPLGetXMLValue(root, "coordinates", ""), "GeoDC-world"))
        return fail();
    std::string version = CPLGetXMLValue(root, "version", "");
    if (version != "1" && version != "2" && version != "3" && version != "4")
        return fail();
    std::vector<Impl::Item> items;
    std::vector<Impl::Layer> layers;
    int next = 1, active = 0;
    auto number = [](CPLXMLNode *n, const char *key, double &v) {
        const char *s = CPLGetXMLValue(n, key, "");
        std::istringstream in(s);
        in.imbue(std::locale::classic());
        return bool(in >> v) && in.peek() == EOF && std::isfinite(v);
    };
    auto objects = [&](CPLXMLNode *parent, int layer) {
        for (auto n = parent->psChild; n; n = n->psNext)
        {
            if (n->eType != CXT_Element)
                continue;
            if (strcmp(n->pszValue, "Object"))
                return false;
            Impl::Item i;
            i.layer = layer;
            double type, color;
            if (!number(n, "type", type) || type < Line || type > ClosedSpline ||
                type != floor(type) || !number(n, "color", color) || color < 0 ||
                color > 0xffffff || color != floor(color) || !number(n, "x1", i.x1) ||
                !number(n, "y1", i.y1) || !number(n, "x2", i.x2) || !number(n, "y2", i.y2))
                return false;

            i.type = (int)type;
            if (i.type == Arrow && version != "4")
                i.style.endArrow = 1;
            i.style.lineColor = (COLORREF)color;
            i.style.fillColor = (COLORREF)color;
            if (version == "3" || version == "4")
            {
                double lineColor;
                if (!number(n, "lineColor", lineColor) || lineColor != floor(lineColor) ||
                    lineColor < 0 || lineColor > 16777215)
                    return false;
                i.style.lineColor = static_cast<COLORREF>(lineColor);
                double fillColor;
                if (!number(n, "fillColor", fillColor) || fillColor != floor(fillColor) ||
                    fillColor < 0 || fillColor > 16777215)
                    return false;
                i.style.fillColor = static_cast<COLORREF>(fillColor);
                double patternColor;
                if (!number(n, "patternColor", patternColor) ||
                    patternColor != floor(patternColor) || patternColor < 0 ||
                    patternColor > 16777215)
                    return false;
                i.style.patternColor = static_cast<COLORREF>(patternColor);
                if (version == "3")
                    i.style.gradientColor = i.style.patternColor;
                double width;
                if (!number(n, "width", width))
                    return false;
                i.style.width = static_cast<double>(width);
                double lineAlpha;
                if (!number(n, "lineAlpha", lineAlpha) || lineAlpha != floor(lineAlpha) ||
                    lineAlpha < 0 || lineAlpha > 255)
                    return false;
                i.style.lineAlpha = static_cast<int>(lineAlpha);
                double fillAlpha;
                if (!number(n, "fillAlpha", fillAlpha) || fillAlpha != floor(fillAlpha) ||
                    fillAlpha < 0 || fillAlpha > 255)
                    return false;
                i.style.fillAlpha = static_cast<int>(fillAlpha);
                double dash;
                if (!number(n, "dash", dash) || dash != floor(dash) || dash < 0 || dash > 4)
                    return false;
                i.style.dash = static_cast<int>(dash);
                double fillType;
                if (!number(n, "fillType", fillType) || fillType != floor(fillType) ||
                    fillType < 0 || fillType > 6)
                    return false;
                i.style.fillType = static_cast<int>(fillType);
                double hatch;
                if (!number(n, "hatch", hatch) || hatch != floor(hatch) || hatch < 0 || hatch > 52)
                    return false;
                i.style.hatch = static_cast<int>(hatch);
                double lineEnabled;
                if (!number(n, "lineEnabled", lineEnabled) || lineEnabled != floor(lineEnabled) ||
                    (lineEnabled != 0 && lineEnabled != 1))
                    return false;
                i.style.lineEnabled = static_cast<bool>(lineEnabled);
                double fillEnabled;
                if (!number(n, "fillEnabled", fillEnabled) || fillEnabled != floor(fillEnabled) ||
                    (fillEnabled != 0 && fillEnabled != 1))
                    return false;
                i.style.fillEnabled = static_cast<bool>(fillEnabled);
                if (version == "4")
                {
                    for (auto entry : {std::make_pair("rotation", &i.style.rotation),
                                       std::make_pair("flipX", &i.style.flipX),
                                       std::make_pair("flipY", &i.style.flipY)})
                    {
                        double value;
                        if (!number(n, entry.first, value) || value < 0 || value > 3 ||
                            value != floor(value))
                            return false;
                        *entry.second = (int)value;
                    }
                    double group;
                    if (!number(n, "group", group) || group < 0 || group > 1000000 ||
                        group != floor(group))
                        return false;
                    i.group = (int)group;
                    std::istringstream points(CPLGetXMLValue(n, "points", ""));
                    points.imbue(std::locale::classic());
                    while (points >> std::ws && !points.eof())
                    {
                        int index = i.geometry.count;
                        if (index >= 512)
                            return false;
                        if (!(points >> i.geometry.x[index] >> i.geometry.y[index]) ||
                            !std::isfinite(i.geometry.x[index]) ||
                            !std::isfinite(i.geometry.y[index]))
                            return false;
                        ++i.geometry.count;
                    }
                    if (i.type >= Polyline && i.geometry.count < 2)
                        return false;
                    double startArrow;
                    if (!number(n, "startArrow", startArrow) || startArrow != floor(startArrow) ||
                        startArrow < 0 || startArrow > 35)
                        return false;
                    i.style.startArrow = static_cast<int>(startArrow);
                    double endArrow;
                    if (!number(n, "endArrow", endArrow) || endArrow != floor(endArrow) ||
                        endArrow < 0 || endArrow > 35)
                        return false;
                    i.style.endArrow = static_cast<int>(endArrow);
                    double lineCap;
                    if (!number(n, "lineCap", lineCap) || lineCap != floor(lineCap) ||
                        lineCap < 0 || lineCap > 2)
                        return false;
                    i.style.lineCap = static_cast<int>(lineCap);
                    double lineJoin;
                    if (!number(n, "lineJoin", lineJoin) || lineJoin != floor(lineJoin) ||
                        lineJoin < 0 || lineJoin > 3)
                        return false;
                    i.style.lineJoin = static_cast<int>(lineJoin);
                    double lineAlignment;
                    if (!number(n, "lineAlignment", lineAlignment) ||
                        lineAlignment != floor(lineAlignment) || lineAlignment < 0 ||
                        lineAlignment > 1)
                        return false;
                    i.style.lineAlignment = static_cast<int>(lineAlignment);
                    double lineStripe;
                    if (!number(n, "lineStripe", lineStripe) || lineStripe != floor(lineStripe) ||
                        lineStripe < 0 || lineStripe > 4)
                        return false;
                    i.style.lineStripe = static_cast<int>(lineStripe);
                    double gradientMode;
                    if (!number(n, "gradientMode", gradientMode) ||
                        gradientMode != floor(gradientMode) || gradientMode < 0 || gradientMode > 3)
                        return false;
                    i.style.gradientMode = static_cast<int>(gradientMode);
                    double gradientPosition;
                    if (!number(n, "gradientPosition", gradientPosition) ||
                        gradientPosition != floor(gradientPosition) || gradientPosition < 1 ||
                        gradientPosition > 99)
                        return false;
                    i.style.gradientPosition = static_cast<int>(gradientPosition);
                    double gradientColor;
                    if (!number(n, "gradientColor", gradientColor) ||
                        gradientColor != floor(gradientColor) || gradientColor < 0 ||
                        gradientColor > 16777215)
                        return false;
                    i.style.gradientColor = static_cast<int>(gradientColor);
                    double textColor;
                    if (!number(n, "textColor", textColor) || textColor != floor(textColor) ||
                        textColor < 0 || textColor > 16777215)
                        return false;
                    i.style.textColor = static_cast<int>(textColor);
                    double fontSize;
                    if (!number(n, "fontSize", fontSize) || fontSize != floor(fontSize) ||
                        fontSize < 1 || fontSize > 300)
                        return false;
                    i.style.fontSize = static_cast<int>(fontSize);
                    double fontStyle;
                    if (!number(n, "fontStyle", fontStyle) || fontStyle != floor(fontStyle) ||
                        fontStyle < 0 || fontStyle > 15)
                        return false;
                    i.style.fontStyle = static_cast<int>(fontStyle);
                    double textAlign;
                    if (!number(n, "textAlign", textAlign) || textAlign != floor(textAlign) ||
                        textAlign < 0 || textAlign > 2)
                        return false;
                    i.style.textAlign = static_cast<int>(textAlign);
                    double textAlignY;
                    if (!number(n, "textAlignY", textAlignY) || textAlignY != floor(textAlignY) ||
                        textAlignY < 0 || textAlignY > 2)
                        return false;
                    i.style.textAlignY = static_cast<int>(textAlignY);
                    double lineInterval;
                    if (!number(n, "lineInterval", lineInterval) ||
                        lineInterval != floor(lineInterval) || lineInterval < 10 ||
                        lineInterval > 1000)
                        return false;
                    i.style.lineInterval = static_cast<int>(lineInterval);
                    double vertical;
                    if (!number(n, "vertical", vertical) || vertical != floor(vertical) ||
                        vertical < 0 || vertical > 1)
                        return false;
                    i.style.vertical = static_cast<int>(vertical);
                    double rightToLeft;
                    if (!number(n, "rightToLeft", rightToLeft) ||
                        rightToLeft != floor(rightToLeft) || rightToLeft < 0 || rightToLeft > 1)
                        return false;
                    i.style.rightToLeft = static_cast<int>(rightToLeft);
                    double autoTextBox;
                    if (!number(n, "autoTextBox", autoTextBox) ||
                        autoTextBox != floor(autoTextBox) || autoTextBox < 0 || autoTextBox > 1)
                        return false;
                    i.style.autoTextBox = static_cast<int>(autoTextBox);
                    CA2W textWide(CPLGetXMLValue(n, "text", ""), CP_UTF8);
                    if (wcslen(textWide) >= 1024)
                        return false;
                    wcscpy_s(i.style.text, textWide);
                    CA2W fontWide(CPLGetXMLValue(n, "font", ""), CP_UTF8);
                    if (wcslen(fontWide) >= 64)
                        return false;
                    wcscpy_s(i.style.font, fontWide);
                    CA2W imagePathWide(CPLGetXMLValue(n, "imagePath", ""), CP_UTF8);
                    if (wcslen(imagePathWide) >= 1024)
                        return false;
                    wcscpy_s(i.style.imagePath, imagePathWide);
                }
                if (!ValidStyle(i.style))
                    return false;
            }
            items.push_back(i);
            if (items.size() > 10000)
                return false;
        }
        return true;
    };
    if (version == "1")
    {
        layers.push_back({1, _T("투명도 1"), true});
        active = 1;
        next = 2;
        if (!objects(root, 1))
            return fail();
    }
    else
    {
        std::set<int> ids;
        for (auto n = root->psChild; n; n = n->psNext)
        {
            if (n->eType != CXT_Element)
                continue;
            if (strcmp(n->pszValue, "Layer"))
                return fail();
            double id, visible;
            if (!number(n, "id", id) || id < 1 || id > 1000000 || id != floor(id) ||
                !ids.insert((int)id).second || !number(n, "visible", visible) ||
                (visible != 0 && visible != 1))
                return fail();

            CA2W wide(CPLGetXMLValue(n, "name", ""), CP_UTF8);
            CString name(wide);
            name.Trim();
            if (name.IsEmpty() || name.GetLength() > 128)
                return fail();
            layers.push_back({(int)id, name, visible != 0});
            if (layers.size() > 1000 || !objects(n, (int)id))
                return fail();
            next = (std::max)(next, (int)id + 1);
        }
        double selected;
        if (!number(root, "active", selected) || selected != floor(selected) || selected < 0 ||
            selected > 1000000)
            return fail();
        active = (int)selected;
        if ((active && !ids.count(active)) || (!active && !layers.empty()))
            return fail();
    }
    Cancel();
    p->selected = -1;
    p->selection.clear();
    p->history.clear();
    p->future.clear();
    p->undone.clear();
    p->items.swap(items);
    p->nextGroup = 1;
    for (const auto &item : p->items)
        p->nextGroup = (std::max)(p->nextGroup, item.group + 1);
    p->layers.swap(layers);
    p->active = active;
    p->next = next;
    p->modified = false;
    return true;
}

int CLocalOverlay::AddLayer(const CString &requested)
{
    Cancel();
    CString name = requested;
    name.Trim();
    if (name.IsEmpty())
        name.Format(_T("투명도 %d"), p->next);
    if (name.GetLength() > 128 || p->layers.size() >= 1000)
        return 0;
    int id = p->next++;
    p->layers.push_back({id, name, true});
    p->selected = -1;
    p->selection.clear();
    p->active = id;
    p->modified = true;
    return id;
}

bool CLocalOverlay::SelectLayer(int id)
{
    for (const auto &layer : p->layers)
        if (layer.id == id)
        {
            Cancel();
            p->selected = -1;
            p->selection.clear();
            p->active = id;
            return true;
        }
    return false;
}

int CLocalOverlay::ActiveLayer() const
{
    return p->active;
}

bool CLocalOverlay::SetLayerVisible(int id, bool visible)
{
    for (auto &layer : p->layers)
        if (layer.id == id)
        {
            Cancel();
            layer.visible = visible;
            if (!visible && layer.id == p->active)
            {
                p->selected = -1;
                p->selection.clear();
            }
            p->modified = true;
            return true;
        }
    return false;
}

bool CLocalOverlay::RenameLayer(int id, const CString &requested)
{
    CString name = requested;
    name.Trim();
    if (name.IsEmpty() || name.GetLength() > 128)
        return false;
    for (auto &layer : p->layers)
        if (layer.id == id)
        {
            layer.name = name;
            p->modified = true;
            return true;
        }
    return false;
}

std::vector<CLocalOverlay::LayerInfo> CLocalOverlay::Layers() const
{
    std::vector<LayerInfo> result;
    for (const auto &layer : p->layers)
    {
        size_t count = 0;
        for (const auto &i : p->items)
            if (i.layer == layer.id)
                ++count;
        result.push_back({layer.id, layer.name, count, layer.visible});
    }
    return result;
}

size_t CLocalOverlay::ActiveCount() const
{
    size_t count = 0;
    for (const auto &i : p->items)
        if (i.layer == p->active)
            ++count;

    return count;
}

CLocalOverlay::Style CLocalOverlay::GetStyle() const
{
    return HasSelection() ? p->items[p->selected].style : p->style;
}

bool CLocalOverlay::ValidStyle(const Style &style)
{
    return style.rotation >= 0 && style.rotation <= 3 && style.flipX >= 0 && style.flipX <= 1 &&
           style.flipY >= 0 && style.flipY <= 1 && style.lineColor <= 0xffffff &&
           style.fillColor <= 0xffffff && style.patternColor <= 0xffffff &&
           std::isfinite(style.width) && style.width >= 0 && style.width <= 100 &&
           style.lineAlpha >= 0 && style.lineAlpha <= 255 && style.fillAlpha >= 0 &&
           style.fillAlpha <= 255 && style.dash >= 0 && style.dash <= 4 && style.fillType >= 1 &&
           style.fillType <= 6 && style.hatch >= 0 && style.hatch <= 52 && style.startArrow >= 0 &&
           style.startArrow <= 35 && style.endArrow >= 0 && style.endArrow <= 35 &&
           style.lineCap >= 0 && style.lineCap <= 2 && style.lineJoin >= 0 && style.lineJoin <= 3 &&
           style.lineAlignment >= 0 && style.lineAlignment <= 1 && style.lineStripe >= 0 &&
           style.lineStripe <= 4 && style.gradientMode >= 0 && style.gradientMode <= 3 &&
           style.gradientPosition >= 1 && style.gradientPosition <= 99 &&
           style.gradientColor >= 0 && style.gradientColor <= 16777215 && style.textColor >= 0 &&
           style.textColor <= 16777215 && style.fontSize >= 1 && style.fontSize <= 300 &&
           style.fontStyle >= 0 && style.fontStyle <= 15 && style.textAlign >= 0 &&
           style.textAlign <= 2 && style.textAlignY >= 0 && style.textAlignY <= 2 &&
           style.lineInterval >= 10 && style.lineInterval <= 1000 && style.vertical >= 0 &&
           style.vertical <= 1 && style.rightToLeft >= 0 && style.rightToLeft <= 1 &&
           style.autoTextBox >= 0 && style.autoTextBox <= 1 && wmemchr(style.text, 0, 1024) &&
           wmemchr(style.font, 0, 64) && wmemchr(style.imagePath, 0, 1024);
}

bool CLocalOverlay::SetStyle(const Style &style)
{
    if (!ValidStyle(style) || p->drawing)
        return false;
    if (HasSelection())
    {
        p->Remember();
        for (int index : p->selection)
            if (index >= 0 && index < (int)p->items.size())
                p->items[index].style = style;
        p->items[p->selected].style = style;
        p->modified = true;
    }
    p->style = style;
    return true;
}

#pragma pack(pop)

bool CLocalOverlay::HasSelection() const
{
    if (p->drawing || p->selected < 0 || p->selected >= static_cast<int>(p->items.size()))
        return false;
    const auto &item = p->items[p->selected];
    for (const auto &layer : p->layers)
        if (layer.id == item.layer && layer.id == p->active && layer.visible)
            return true;
    return false;
}

bool CLocalOverlay::DeleteSelected()
{
    if (!HasSelection())
        return false;
    p->Remember();
    p->selection.insert(p->selected);
    for (auto it = p->selection.rbegin(); it != p->selection.rend(); ++it)
        if (*it >= 0 && *it < (int)p->items.size())
            p->items.erase(p->items.begin() + *it);
    p->selected = -1;
    p->selection.clear();
    p->modified = true;
    return true;
}

bool CLocalOverlay::CopySelected(HWND owner) const
{
    if (!HasSelection())
        return false;
    std::vector<OverlayClipboardObject> objects;
    auto selection = p->selection;
    selection.insert(p->selected);
    for (int index : selection)
    {
        const auto &item = p->items[index];
        OverlayClipboardObject data{};
        data.version = 4;
        data.group = item.group;
        data.type = item.type;
        data.x1 = item.x1;
        data.y1 = item.y1;
        data.x2 = item.x2;
        data.y2 = item.y2;
        data.style = item.style;
        data.geometry = item.geometry;
        objects.push_back(data);
    }
    UINT format = OverlayClipboardFormat();
    size_t size = objects.size() * sizeof(OverlayClipboardObject);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory)
        return false;
    void *bytes = GlobalLock(memory);
    if (!bytes)
    {
        GlobalFree(memory);
        return false;
    }
    memcpy(bytes, objects.data(), size);
    GlobalUnlock(memory);
    bool copied = false;
    if (format && ::OpenClipboard(owner))
    {
        if (::EmptyClipboard())
            copied = ::SetClipboardData(format, memory) != nullptr;
        ::CloseClipboard();
    }
    if (!copied)
        GlobalFree(memory);
    return copied;
}

bool CLocalOverlay::CutSelected(HWND owner)
{
    // Do not remove the original if another application owns the clipboard.
    return CopySelected(owner) && DeleteSelected();
}

bool CLocalOverlay::CanPaste()
{
    UINT format = OverlayClipboardFormat();
    return format && IsClipboardFormatAvailable(format);
}

bool CLocalOverlay::Paste(HWND owner, CCoordMgr *coord)
{
    if (!coord || p->drawing || !CanPaste() || !::OpenClipboard(owner))
        return false;
    HANDLE memory = GetClipboardData(OverlayClipboardFormat());
    size_t size = memory ? GlobalSize(memory) : 0;
    std::vector<OverlayClipboardObject> objects;
    if (size && size % sizeof(OverlayClipboardObject) == 0 &&
        size / sizeof(OverlayClipboardObject) <= 10000)
    {
        void *bytes = GlobalLock(memory);
        if (bytes)
        {
            objects.resize(size / sizeof(OverlayClipboardObject));
            memcpy(objects.data(), bytes, size);
            GlobalUnlock(memory);
        }
    }
    ::CloseClipboard();
    if (objects.empty())
        return false;
    for (const auto &layer : p->layers)
        if (layer.id == p->active && !layer.visible)
            return false;
    std::vector<Impl::Item> items;
    DWORD sequence = GetClipboardSequenceNumber();
    unsigned int count = sequence == p->clipboardSequence ? p->pasteCount + 1 : 1;
    double offset = 24.0 * (1 + (count - 1) % 10);
    std::map<int, int> groups;
    for (const auto &data : objects)
    {
        if (data.version != 4 || data.type < Line || data.type > ClosedSpline ||
            !ValidStyle(data.style) || data.group < 0 || data.group > 1000000)
            return false;
        Impl::Item item;
        item.type = data.type;
        item.style = data.style;
        if (data.geometry.count < 0 || data.geometry.count > 512 ||
            (data.type >= Polyline && data.geometry.count < 2))
            return false;
        item.geometry = data.geometry;
        for (int n = 0; n < item.geometry.count; ++n)
        {
            double sx, sy;
            if (!std::isfinite(item.geometry.x[n]) || !std::isfinite(item.geometry.y[n]))
                return false;
            coord->WorldXY2ScreenXY(item.geometry.x[n], item.geometry.y[n], &sx, &sy);
            coord->ScreenXY2WorldXY(sx + offset, sy + offset, &item.geometry.x[n],
                                    &item.geometry.y[n]);
        }
        item.x1 = data.x1;
        item.y1 = data.y1;
        item.x2 = data.x2;
        item.y2 = data.y2;
        double a, b, d, e;
        if (!Impl::Screen(item, coord, a, b, d, e))
            return false;
        coord->ScreenXY2WorldXY(a + offset, b + offset, &item.x1, &item.y1);
        coord->ScreenXY2WorldXY(d + offset, e + offset, &item.x2, &item.y2);
        if (!std::isfinite(item.x1) || !std::isfinite(item.y1) || !std::isfinite(item.x2) ||
            !std::isfinite(item.y2))
            return false;
        if (data.group)
        {
            if (!groups.count(data.group))
                groups[data.group] = p->nextGroup + (int)groups.size();
            item.group = groups[data.group];
        }
        items.push_back(item);
    }
    if (!p->active)
        AddLayer();
    p->Remember();
    p->selection.clear();
    for (auto &item : items)
    {
        item.layer = p->active;
        p->selection.insert((int)p->items.size());
        p->items.push_back(item);
    }
    p->nextGroup += (int)groups.size();
    p->selected = (int)p->items.size() - 1;
    p->clipboardSequence = sequence;
    p->pasteCount = count;
    p->tool = Pan;
    p->modified = true;
    return true;
}

bool CLocalOverlay::CanUndo() const
{
    for (const auto &snapshot : p->history)
        if (snapshot.layer == p->active)
            return true;
    return false;
}

bool CLocalOverlay::Arrange(int operation, CCoordMgr *coord)
{
    if (!HasSelection() || !coord || operation < 0 || operation > 12)
        return false;
    p->Remember();
    auto index = p->selected;
    if (operation <= 3)
    {
        std::vector<int> slots;
        std::vector<std::pair<Impl::Item, bool>> entries;
        for (int n = 0; n < (int)p->items.size(); ++n)
            if (p->items[n].layer == p->active)
            {
                slots.push_back(n);
                entries.push_back({p->items[n], p->selection.count(n) != 0});
            }
        if (operation < 2)
            std::stable_partition(entries.begin(), entries.end(), [&](const auto &e) {
                return operation == 0 ? !e.second : e.second;
            });
        else if (operation == 2)
        {
            for (int n = (int)entries.size() - 2; n >= 0; --n)
                if (entries[n].second && !entries[n + 1].second)
                    std::swap(entries[n], entries[n + 1]);
        }
        else
        {
            for (size_t n = 1; n < entries.size(); ++n)
                if (entries[n].second && !entries[n - 1].second)
                    std::swap(entries[n], entries[n - 1]);
        }
        p->selection.clear();
        for (size_t n = 0; n < slots.size(); ++n)
        {
            p->items[slots[n]] = entries[n].first;
            if (entries[n].second)
                p->selection.insert(slots[n]);
        }
        p->selected = p->selection.empty() ? -1 : *p->selection.rbegin();
    }
    else if (operation >= 7)
    {
        double left = 1e30, top = 1e30, right = -1e30, bottom = -1e30;
        for (int n : p->selection)
        {
            double a, b, d, e;
            if (!Impl::Screen(p->items[n], coord, a, b, d, e))
                continue;
            left = (std::min)(left, (std::min)(a, d));
            right = (std::max)(right, (std::max)(a, d));
            top = (std::min)(top, (std::min)(b, e));
            bottom = (std::max)(bottom, (std::max)(b, e));
        }
        for (int n : p->selection)
        {
            auto &item = p->items[n];
            double a, b, d, e;
            if (!Impl::Screen(item, coord, a, b, d, e))
                continue;
            double dx = 0, dy = 0;
            switch (operation)
            {
            case 7:
                dx = left - (std::min)(a, d);
                break;
            case 8:
                dx = (left + right - a - d) / 2;
                break;
            case 9:
                dx = right - (std::max)(a, d);
                break;
            case 10:
                dy = top - (std::min)(b, e);
                break;
            case 11:
                dy = (top + bottom - b - e) / 2;
                break;
            case 12:
                dy = bottom - (std::max)(b, e);
                break;
            }
            for (int n = 0; n < item.geometry.count; ++n)
            {
                double sx, sy;
                coord->WorldXY2ScreenXY(item.geometry.x[n], item.geometry.y[n], &sx, &sy);
                coord->ScreenXY2WorldXY(sx + dx, sy + dy, &item.geometry.x[n], &item.geometry.y[n]);
            }
            coord->ScreenXY2WorldXY(a + dx, b + dy, &item.x1, &item.y1);
            coord->ScreenXY2WorldXY(d + dx, e + dy, &item.x2, &item.y2);
        }
    }
    else
    {
        for (int n : p->selection)
        {
            auto &style = p->items[n].style;
            if (operation == 4)
                style.flipX = !style.flipX;
            else if (operation == 5)
                style.flipY = !style.flipY;
            else
                style.rotation = (style.rotation + 1) % 4;
        }
    }

    p->modified = true;
    return true;
}

void CLocalOverlay::SelectAll()
{
    Cancel();
    p->selection.clear();
    p->selected = -1;
    bool visible = false;
    for (const auto &layer : p->layers)
        if (layer.id == p->active)
            visible = layer.visible;
    if (!visible)
        return;
    for (int n = 0; n < (int)p->items.size(); ++n)
        if (p->items[n].layer == p->active)
            p->selection.insert(n);
    p->selected = p->selection.empty() ? -1 : *p->selection.rbegin();
}
bool CLocalOverlay::GroupSelection(bool ungroup)
{
    if (!HasSelection() || (!ungroup && p->selection.size() < 2))
        return false;
    p->Remember();
    int group = ungroup ? 0 : p->nextGroup++;
    for (int index : p->selection)
        p->items[index].group = group;
    p->modified = true;
    return true;
}
