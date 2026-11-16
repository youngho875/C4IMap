#include "pch.h"
#pragma pack(push, 8)
#include "LocalOverlay.h"
#include <vector>
#include <memory>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>
#include <algorithm>
#include <set>
#include <atlconv.h>
#include "cpl_string.h"
#include <gdiplus.h>
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"
#include "OverlaySDK/LibSrc/Overlay/ObjectLine.h"
#include "OverlaySDK/LibSrc/Overlay/ObjectRect.h"
#include "OverlaySDK/LibSrc/Overlay/ObjectEllipse.h"
#include "OverlaySDK/LibSrc/Overlay/ObjectLineDraw.h"
#include "OverlaySDK/LibSrc/Overlay/ObjectRectDraw.h"
#include "OverlaySDK/LibSrc/Overlay/ObjectEllipseDraw.h"
#include "cpl_minixml.h"
#pragma comment(lib, "gdiplus.lib")

namespace
{
// Stable clipboard payload; no pointers or STL objects cross the clipboard.
struct OverlayClipboardObject
{
    unsigned int version;
    int type;
    double x1, y1, x2, y2;
    CLocalOverlay::Style style;
};
UINT OverlayClipboardFormat()
{
    return RegisterClipboardFormat(_T("C4IMap.OverlayObject.GeoDCWorld.v2"));
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
        int layer = 0;
        int type = Line;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        Style style;
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
    p->tool = tool >= Pan && tool <= Ellipse ? tool : Pan;
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
        auto layer = std::find_if(p->layers.begin(), p->layers.end(), [&](const Impl::Layer &l)
                                  { return l.id == p->active && l.visible; });
        if (layer == p->layers.end())
        {
            p->selected = -1;
            return false;
        }
        p->handle = -1;
        if (p->selected >= 0 && p->selected < (int)p->items.size())
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
            p->selected = -1;
            for (int i = (int)p->items.size() - 1; i >= 0; --i)
            {
                auto &item = p->items[i];
                if (item.layer != p->active)
                    continue;
                double a, b, d, e;
                if (!Impl::Screen(item, c, a, b, d, e))
                    continue;
                bool hit = false;
                if (item.type == Line || item.type == Arrow)
                {
                    double dx = d - a, dy = e - b, len = dx * dx + dy * dy;
                    double t =
                        len > 0
                            ? (std::max)(0.0,
                                         (std::min)(1.0, ((pt.x - a) * dx + (pt.y - b) * dy) / len))
                            : 0;
                    hit = hypot(pt.x - a - t * dx, pt.y - b - t * dy) <= 7;
                }
                else if (item.type == Ellipse)
                {
                    double rx = fabs(d - a) / 2, ry = fabs(e - b) / 2;
                    if (rx > 0 && ry > 0)
                        hit = pow((pt.x - (a + d) / 2) / (rx + 6), 2) +
                                  pow((pt.y - (b + e) / 2) / (ry + 6), 2) <=
                              1;
                }
                else
                    hit = pt.x >= (std::min)(a, d) - 6 && pt.x <= (std::max)(a, d) + 6 &&
                          pt.y >= (std::min)(b, e) - 6 && pt.y <= (std::max)(b, e) + 6;
                if (hit)
                {
                    p->selected = i;
                    break;
                }
            }
        }
        if (p->selected < 0)
            return false;
        p->original = p->pending = p->items[p->selected];
        p->start = pt;
        p->editing = p->drawing = true;
        return true;
    }
    p->selected = -1;
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
    c->ScreenXY2WorldXY(pt.x, pt.y, &p->pending.x1, &p->pending.y1);
    p->pending.x2 = p->pending.x1;
    p->pending.y2 = p->pending.y1;
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
        c->ScreenXY2WorldXY((int)x1, (int)y1, &candidate.x1, &candidate.y1);
        c->ScreenXY2WorldXY((int)x2, (int)y2, &candidate.x2, &candidate.y2);
        if (std::isfinite(candidate.x1) && std::isfinite(candidate.y1) &&
            std::isfinite(candidate.x2) && std::isfinite(candidate.y2))
            p->pending = candidate;
        return true;
    }
    c->ScreenXY2WorldXY(pt.x, pt.y, &p->pending.x2, &p->pending.y2);
    return true;
}

bool CLocalOverlay::End(CPoint pt, CCoordMgr *c)
{
    if (!Move(pt, c))
        return false;
    if (abs(pt.x - p->start.x) + abs(pt.y - p->start.y) >= 4)
    {
        if (std::isfinite(p->pending.x1) && std::isfinite(p->pending.y1) &&
            std::isfinite(p->pending.x2) && std::isfinite(p->pending.y2))
        {
            p->Remember();
            if (p->editing)
                p->items[p->selected] = p->pending;
            else
            {
                p->items.push_back(p->pending);
                p->selected = (int)p->items.size() - 1;
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

    auto draw = [&](const Impl::Item &item)
    {
        double x1, y1, x2, y2;
        c->WorldXY2ScreenXY(item.x1, item.y1, &x1, &y1);
        c->WorldXY2ScreenXY(item.x2, item.y2, &x2, &y2);
        if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2))
            return;

        std::unique_ptr<OVERLAY::OBJECT> object;
        std::unique_ptr<OVERLAY::CObjectDraw> renderer;
        if (item.type == Rectangle)
        {
            object.reset(new OVERLAY::ObjectRect);
            renderer.reset(new OVERLAY::CObjectRectDraw);
        }
        else if (item.type == Ellipse)
        {
            object.reset(new OVERLAY::ObjectEllipse);
            renderer.reset(new OVERLAY::CObjectEllipseDraw);
        }
        else
        {
            object.reset(new OVERLAY::ObjectLine);
            renderer.reset(new OVERLAY::CObjectLineDraw);
        }
        if (item.type == Rectangle || item.type == Ellipse)
        {
            if (x1 > x2)
                std::swap(x1, x2);
            if (y1 > y2)
                std::swap(y1, y2);
        }
        object->pt[0].SetXY(x1, y1);
        object->pt[1].SetXY(x2, y2);
        const auto& style = item.style;
        object->Property.LineColor = Gdiplus::Color((BYTE)style.lineAlpha,
            GetRValue(style.lineColor), GetGValue(style.lineColor), GetBValue(style.lineColor)).GetValue();
        object->Property.dLineWidth = style.width;
        object->Property.eLineStyle = static_cast<decltype(object->Property.eLineStyle)>(style.dash);
        object->Property.bIsLine = style.lineEnabled && style.width > 0;
        object->Property.bIsFill = FALSE;
        if (style.fillEnabled && (item.type == Rectangle || item.type == Ellipse))
        {
            Gdiplus::GraphicsPath path;
            Gdiplus::RectF box((float)x1, (float)y1, (float)(x2-x1), (float)(y2-y1));
            if (box.Width > 0 && box.Height > 0)
            {
                if (item.type == Rectangle) path.AddRectangle(box);
                else path.AddEllipse(box);
                Gdiplus::Color fill((BYTE)style.fillAlpha, GetRValue(style.fillColor), GetGValue(style.fillColor), GetBValue(style.fillColor));
                Gdiplus::Color other((BYTE)style.fillAlpha, GetRValue(style.patternColor), GetGValue(style.patternColor), GetBValue(style.patternColor));
                if (style.fillType == 2)
                {
                    Gdiplus::HatchBrush brush((Gdiplus::HatchStyle)style.hatch, other, fill);
                    g.FillPath(&brush, &path);
                }
                else if (style.fillType == 3)
                {
                    Gdiplus::PathGradientBrush brush(&path);
                    brush.SetCenterColor(other); INT count = 1;
                    brush.SetSurroundColors(&fill, &count);
                    g.FillPath(&brush, &path);
                }
                else if (style.fillType == 4)
                {
                    Gdiplus::LinearGradientBrush brush(box, fill, other, Gdiplus::LinearGradientModeHorizontal);
                    g.FillPath(&brush, &path);
                }
                else
                {
                    Gdiplus::SolidBrush brush(fill);
                    g.FillPath(&brush, &path);
                }
            }
        }
        if (item.type == Arrow)
            object->Property.nEndLineArrowStyle = 4;
        if (object->Property.bIsLine)
            renderer->Draw(object.get(), &g);
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
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<C4IMapOverlay version=\"3\" "
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
                    << "\" x2=\"" << i.x2 << "\" y2=\"" << i.y2 << "\" color=\"" << i.style.lineColor
                    << "\" lineColor=\"" << i.style.lineColor
                    << "\" fillColor=\"" << i.style.fillColor
                    << "\" patternColor=\"" << i.style.patternColor
                    << "\" width=\"" << i.style.width
                    << "\" lineAlpha=\"" << i.style.lineAlpha
                    << "\" fillAlpha=\"" << i.style.fillAlpha
                    << "\" dash=\"" << i.style.dash
                    << "\" fillType=\"" << i.style.fillType
                    << "\" hatch=\"" << i.style.hatch
                    << "\" lineEnabled=\"" << i.style.lineEnabled
                    << "\" fillEnabled=\"" << i.style.fillEnabled
                    << "\"/>\n";
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
    auto fail = [&]()
    {
        error = _T("Unsupported or invalid C4IMap Overlay XML.");
        return false;
    };
    if (!root || strcmp(CPLGetXMLValue(root, "coordinates", ""), "GeoDC-world"))
        return fail();
    std::string version = CPLGetXMLValue(root, "version", "");
    if (version != "1" && version != "2" && version != "3")
        return fail();
    std::vector<Impl::Item> items;
    std::vector<Impl::Layer> layers;
    int next = 1, active = 0;
    auto number = [](CPLXMLNode *n, const char *key, double &v)
    {
        const char *s = CPLGetXMLValue(n, key, "");
        std::istringstream in(s);
        in.imbue(std::locale::classic());
        return bool(in >> v) && in.peek() == EOF && std::isfinite(v);
    };
    auto objects = [&](CPLXMLNode *parent, int layer)
    {
        for (auto n = parent->psChild; n; n = n->psNext)
        {
            if (n->eType != CXT_Element)
                continue;
            if (strcmp(n->pszValue, "Object"))
                return false;
            Impl::Item i;
            i.layer = layer;
            double type, color;
            if (!number(n, "type", type) || type < Line || type > Ellipse || type != floor(type) ||
                !number(n, "color", color) || color < 0 || color > 0xffffff ||
                color != floor(color) || !number(n, "x1", i.x1) || !number(n, "y1", i.y1) ||
                !number(n, "x2", i.x2) || !number(n, "y2", i.y2))
                return false;

            i.type = (int)type;
            i.style.lineColor = (COLORREF)color;
            i.style.fillColor = (COLORREF)color;
            if (version == "3")
            {
                double lineColor;
                if (!number(n, "lineColor", lineColor) || lineColor != floor(lineColor) || lineColor < 0 || lineColor > 16777215) return false;
                i.style.lineColor = static_cast<COLORREF>(lineColor);
                double fillColor;
                if (!number(n, "fillColor", fillColor) || fillColor != floor(fillColor) || fillColor < 0 || fillColor > 16777215) return false;
                i.style.fillColor = static_cast<COLORREF>(fillColor);
                double patternColor;
                if (!number(n, "patternColor", patternColor) || patternColor != floor(patternColor) || patternColor < 0 || patternColor > 16777215) return false;
                i.style.patternColor = static_cast<COLORREF>(patternColor);
                double width;
                if (!number(n, "width", width)) return false;
                i.style.width = static_cast<double>(width);
                double lineAlpha;
                if (!number(n, "lineAlpha", lineAlpha) || lineAlpha != floor(lineAlpha) || lineAlpha < 0 || lineAlpha > 255) return false;
                i.style.lineAlpha = static_cast<int>(lineAlpha);
                double fillAlpha;
                if (!number(n, "fillAlpha", fillAlpha) || fillAlpha != floor(fillAlpha) || fillAlpha < 0 || fillAlpha > 255) return false;
                i.style.fillAlpha = static_cast<int>(fillAlpha);
                double dash;
                if (!number(n, "dash", dash) || dash != floor(dash) || dash < 0 || dash > 4) return false;
                i.style.dash = static_cast<int>(dash);
                double fillType;
                if (!number(n, "fillType", fillType) || fillType != floor(fillType) || fillType < 0 || fillType > 4) return false;
                i.style.fillType = static_cast<int>(fillType);
                double hatch;
                if (!number(n, "hatch", hatch) || hatch != floor(hatch) || hatch < 0 || hatch > 52) return false;
                i.style.hatch = static_cast<int>(hatch);
                double lineEnabled;
                if (!number(n, "lineEnabled", lineEnabled) || lineEnabled != floor(lineEnabled) || (lineEnabled != 0 && lineEnabled != 1)) return false;
                i.style.lineEnabled = static_cast<bool>(lineEnabled);
                double fillEnabled;
                if (!number(n, "fillEnabled", fillEnabled) || fillEnabled != floor(fillEnabled) || (fillEnabled != 0 && fillEnabled != 1)) return false;
                i.style.fillEnabled = static_cast<bool>(fillEnabled);
                if (!ValidStyle(i.style)) return false;
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
    p->history.clear();
    p->future.clear();
    p->undone.clear();
    p->items.swap(items);
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

bool CLocalOverlay::ValidStyle(const Style& style)
{
    return style.lineColor <= 0xffffff && style.fillColor <= 0xffffff && style.patternColor <= 0xffffff &&
        std::isfinite(style.width) && style.width >= 0 && style.width <= 100 &&
        style.lineAlpha >= 0 && style.lineAlpha <= 255 && style.fillAlpha >= 0 && style.fillAlpha <= 255 &&
        style.dash >= 0 && style.dash <= 4 && style.fillType >= 1 && style.fillType <= 4 &&
        style.hatch >= 0 && style.hatch <= 52;
}

bool CLocalOverlay::SetStyle(const Style& style)
{
    if (!ValidStyle(style) || p->drawing)
        return false;
    if (HasSelection())
    {
        p->Remember();
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
    const auto& item = p->items[p->selected];
    for (const auto& layer : p->layers)
        if (layer.id == item.layer && layer.id == p->active && layer.visible)
            return true;
    return false;
}

bool CLocalOverlay::DeleteSelected()
{
    if (!HasSelection())
        return false;
    p->Remember();
    p->items.erase(p->items.begin() + p->selected);
    p->selected = -1;
    p->modified = true;
    return true;
}

bool CLocalOverlay::CopySelected(HWND owner) const
{
    if (!HasSelection())
        return false;
    const auto& item = p->items[p->selected];
    OverlayClipboardObject data{};
    data.version = 2;
    data.type = item.type;
    data.x1 = item.x1; data.y1 = item.y1;
    data.x2 = item.x2; data.y2 = item.y2;
    data.style = item.style;
    UINT format = OverlayClipboardFormat();
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(data));
    if (!memory)
        return false;
    void* bytes = GlobalLock(memory);
    if (!bytes)
    {
        GlobalFree(memory);
        return false;
    }
    memcpy(bytes, &data, sizeof(data));
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

bool CLocalOverlay::Paste(HWND owner, CCoordMgr* coord)
{
    if (!coord || p->drawing || !CanPaste() || !::OpenClipboard(owner))
        return false;
    OverlayClipboardObject data{};
    HANDLE memory = GetClipboardData(OverlayClipboardFormat());
    bool valid = memory && GlobalSize(memory) == sizeof(data);
    if (valid)
    {
        void* bytes = GlobalLock(memory);
        valid = bytes != nullptr;
        if (bytes)
        {
            memcpy(&data, bytes, sizeof(data));
            GlobalUnlock(memory);
        }
    }
    ::CloseClipboard();
    if (!valid || data.version != 2 || data.type < Line || data.type > Ellipse ||
        !std::isfinite(data.x1) || !std::isfinite(data.y1) ||
        !std::isfinite(data.x2) || !std::isfinite(data.y2))
        return false;
    for (const auto& layer : p->layers)
        if (layer.id == p->active && !layer.visible)
            return false;
    Impl::Item item;
    item.type = data.type;
    if (!ValidStyle(data.style))
        return false;
    item.style = data.style;
    item.x1 = data.x1; item.y1 = data.y1;
    item.x2 = data.x2; item.y2 = data.y2;
    double x1, y1, x2, y2;
    if (!Impl::Screen(item, coord, x1, y1, x2, y2))
        return false;
    DWORD sequence = GetClipboardSequenceNumber();
    unsigned int count = sequence == p->clipboardSequence ? p->pasteCount + 1 : 1;
    double offset = 24.0 * (1 + (count - 1) % 10);
    coord->ScreenXY2WorldXY(static_cast<int>(x1 + offset), static_cast<int>(y1 + offset), &item.x1, &item.y1);
    coord->ScreenXY2WorldXY(static_cast<int>(x2 + offset), static_cast<int>(y2 + offset), &item.x2, &item.y2);
    if (!std::isfinite(item.x1) || !std::isfinite(item.y1) || !std::isfinite(item.x2) || !std::isfinite(item.y2))
        return false;
    if (!p->active)
        AddLayer();
    item.layer = p->active;
    p->Remember();
    p->items.push_back(item);
    p->clipboardSequence = sequence;
    p->pasteCount = count;
    p->selected = static_cast<int>(p->items.size()) - 1;
    p->tool = Pan;
    p->modified = true;
    return true;
}

bool CLocalOverlay::CanUndo() const
{
    for (const auto& snapshot : p->history)
        if (snapshot.layer == p->active)
            return true;
    return false;
}
