#include "pch.h"
#include "VectorTileStyle.h"
#include "MapBuildOptions.h"

namespace
{
class GdiPlusThreadSession
{
public:
    GdiPlusThreadSession();
    ~GdiPlusThreadSession();

private:
    ULONG_PTR token_ = 0;
};
GdiPlusThreadSession::GdiPlusThreadSession()
{
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&token_, &input, nullptr);
}
GdiPlusThreadSession::~GdiPlusThreadSession()
{
    if (token_)
        Gdiplus::GdiplusShutdown(token_);
}
} // namespace
namespace vtstyle
{
std::string Tag(const char *s)
{
    std::string v = s ? s : "";
    auto p = v.find(':');
    return p == v.npos ? v : v.substr(p + 1);
}

const CPLXMLNode *Child(const CPLXMLNode *n, const char *name)
{
    for (auto c = n ? n->psChild : nullptr; c; c = c->psNext)
        if (c->eType == CXT_Element && Tag(c->pszValue) == name)
            return c;
    return nullptr;
}

std::string Text(const CPLXMLNode *n)
{
    for (auto c = n ? n->psChild : nullptr; c; c = c->psNext)
        if (c->eType == CXT_Text)
            return c->pszValue;
    return {};
}

std::string Value(const CPLXMLNode *n, const char *key)
{
    return Text(Child(n, key));
}

double Num(std::string s, double d)
{
    if (s.empty())
        return d;
    char *end = nullptr;
    double v = strtod(s.c_str(), &end);
    return end && !*end && std::isfinite(v) ? v : d;
}

std::wstring Wide(const std::string &s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    w.resize(n ? n - 1 : 0);
    return w;
}

std::string Param(const CPLXMLNode *n, const char *key, const char *fallback)
{
    for (auto c = n ? n->psChild : nullptr; c; c = c->psNext)
        if (c->eType == CXT_Element &&
            (Tag(c->pszValue) == "CssParameter" || Tag(c->pszValue) == "SvgParameter") &&
            std::string(CPLGetXMLValue(c, "name", "")) == key)
            return Text(c);
    return fallback;
}

Gdiplus::Color Color(const std::string &s, double opacity)
{
    unsigned r = 40, g = 110, b = 190;
    if (s.size() == 7 && s[0] == '#')
    {
        unsigned v = (unsigned)strtoul(s.c_str() + 1, nullptr, 16);
        r = v >> 16;
        g = (v >> 8) & 255;
        b = v & 255;
    }
    return Gdiplus::Color((BYTE)(255 * (std::max)(0.0, (std::min)(1.0, opacity))), (BYTE)r, (BYTE)g,
                          (BYTE)b);
}

std::string Expr(const CPLXMLNode *n, OGRFeature *f)
{
    if (!n)
        return {};
    return Tag(n->pszValue) == "PropertyName" ? f->GetFieldAsString(Text(n).c_str()) : Text(n);
}

bool Filter(const CPLXMLNode *n, OGRFeature *f)
{
    if (!n)
        return true;
    auto name = Tag(n->pszValue);
    std::vector<const CPLXMLNode *> children;
    for (auto c = n->psChild; c; c = c->psNext)
        if (c->eType == CXT_Element)
            children.push_back(c);
    if (name == "Filter")
        return children.size() == 1 && Filter(children[0], f);
    if (name == "And" || name == "Or")
    {
        bool result = name == "And";
        for (auto c : children)
        {
            if (name == "And")
                result = result && Filter(c, f);
            else
                result = result || Filter(c, f);
        }
        return result;
    }
    if (name == "Not")
        return children.size() == 1 && !Filter(children[0], f);
    if (name == "PropertyIsNull")
    {
        int i = f->GetFieldIndex(Value(n, "PropertyName").c_str());
        return i < 0 || !f->IsFieldSetAndNotNull(i);
    }
    if (children.size() != 2)
        return false;
    auto a = Expr(children[0], f), b = Expr(children[1], f);
    if (name == "PropertyIsEqualTo")
        return a == b || (std::isfinite(Num(a, NAN)) && Num(a, NAN) == Num(b, NAN));
    if (name == "PropertyIsNotEqualTo")
        return !(a == b || (std::isfinite(Num(a, NAN)) && Num(a, NAN) == Num(b, NAN)));
    double x = Num(a, NAN), y = Num(b, NAN);
    if (name == "PropertyIsLessThan")
        return x < y;
    if (name == "PropertyIsLessThanOrEqualTo")
        return x <= y;
    if (name == "PropertyIsGreaterThan")
        return x > y;
    if (name == "PropertyIsGreaterThanOrEqualTo")
        return x >= y;
    return false;
}

Gdiplus::Image *Style::Image(const CPLXMLNode *graphic)
{
    auto resource = Child(Child(graphic, "ExternalGraphic"), "OnlineResource");
    if (!resource)
        return nullptr;
    std::string href;
    for (auto a = resource->psChild; a; a = a->psNext)
        if (a->eType == CXT_Attribute && Tag(a->pszValue) == "href")
            href = Text(a);
    if (href.empty())
        return nullptr;
    auto path = href;
    if (path.find(':') == path.npos && path[0] != '/')
        path = root + "/" + path;
    auto it = images.find(path);
    if (it == images.end())
        it = images
                 .emplace(path, std::unique_ptr<Gdiplus::Image>(
                                    Gdiplus::Image::FromFile(Wide(path).c_str())))
                 .first;
    return it->second && it->second->GetLastStatus() == Gdiplus::Ok ? it->second.get() : nullptr;
}

void Style::Path(Gdiplus::GraphicsPath &path, OGRGeometry *geo, double w, double n, double sx,
                 double sy)
{
    auto t = wkbFlatten(geo->getGeometryType());
    if (t == wkbMultiPolygon || t == wkbMultiLineString || t == wkbMultiPoint ||
        t == wkbGeometryCollection)
    {
        auto col = geo->toGeometryCollection();
        for (int i = 0; i < col->getNumGeometries(); ++i)
            Path(path, col->getGeometryRef(i), w, n, sx, sy);
    }
    else if (t == wkbPolygon)
    {
        auto p = geo->toPolygon();
        if (p->getExteriorRing())
        {
            Path(path, p->getExteriorRing(), w, n, sx, sy);
            path.CloseFigure();
        }
        for (int i = 0; i < p->getNumInteriorRings(); ++i)
        {
            Path(path, p->getInteriorRing(i), w, n, sx, sy);
            path.CloseFigure();
        }
    }
    else if (t == wkbLineString || t == wkbLinearRing)
    {
        auto l = geo->toLineString();
        std::vector<Gdiplus::PointF> points;
        for (int i = 0; i < l->getNumPoints(); ++i)
            points.emplace_back((float)((l->getX(i) - w) * sx), (float)((n - l->getY(i)) * sy));
        if (points.size() > 1)
        {
            path.StartFigure();
            path.AddLines(points.data(), (INT)points.size());
        }
    }
}

Style::Style(const std::string &file) : root(file.substr(0, file.find_last_of("/\\")))
{
    static thread_local GdiPlusThreadSession session;
    xml.reset(CPLParseXMLFile(file.c_str()));
}

bool Style::Valid() const
{
    return xml != nullptr;
}

bool Style::Draw(Gdiplus::Graphics &graphics, OGRFeature *feature, OGRGeometry *geo,
                 const std::string &layer, double scale, double west, double north, double east,
                 double south, std::vector<ScreenSymbol> *symbols)
{
    double sx = 256 / (east - west), sy = 256 / (north - south);
    bool painted = false;
    std::function<void(const CPLXMLNode *)> visit;
    visit = [&](const CPLXMLNode *node)
    {
        for (auto q = node; q; q = q->psNext)
        {
            if (q->eType != CXT_Element)
                continue;
            if (Tag(q->pszValue) == "NamedLayer")
            {
                if (Value(q, "Name") == layer)
                    visit(q->psChild);
                continue;
            }
            if (Tag(q->pszValue) != "Rule")
            {
                visit(q->psChild);
                continue;
            }
            auto min = Num(Value(q, "MinScaleDenominator")),
                 max = Num(Value(q, "MaxScaleDenominator"));
            if ((min && scale < min) || (max && scale > max) ||
                !Filter(Child(q, "Filter"), feature))
                continue;
            for (auto symbol = q->psChild; symbol; symbol = symbol->psNext)
            {
                if (symbol->eType != CXT_Element)
                    continue;
                auto type = Tag(symbol->pszValue);
                if (type == "LineSymbolizer" || type == "PolygonSymbolizer")
                {
                    Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
                    Path(path, geo, west, north, sx, sy);
                    auto fill = Child(symbol, "Fill"), stroke = Child(symbol, "Stroke");
                    if (type == "PolygonSymbolizer" && fill)
                    {
                        auto image = Image(Child(Child(fill, "GraphicFill"), "Graphic"));
                        if (image)
                        {
                            Gdiplus::TextureBrush brush(image, Gdiplus::WrapModeTile);
                            graphics.FillPath(&brush, &path);
                        }
                        else
                        {
                            Gdiplus::SolidBrush brush(
                                Color(Param(fill, "fill", "#2870be"),
                                      Num(Param(fill, "fill-opacity", "1"), 1)));
                            graphics.FillPath(&brush, &path);
                        }
                    }
                    if (stroke)
                    {
                        auto width = (float)Num(Param(stroke, "stroke-width", "1"), 1);
                        Gdiplus::Pen pen(Color(Param(stroke, "stroke", "#2870be"),
                                               Num(Param(stroke, "stroke-opacity", "1"), 1)),
                                         width);
                        std::istringstream in(Param(stroke, "stroke-dasharray"));
                        std::vector<float> dash;
                        float d;
                        while (in >> d)
                            if (d > 0)
                                dash.push_back(d / (std::max)(1.0f, width));
                        if (!dash.empty())
                            pen.SetDashPattern(dash.data(), (INT)dash.size());
                        graphics.DrawPath(&pen, &path);
                        auto graphic = Child(Child(stroke, "GraphicStroke"), "Graphic");
                        auto pattern = Image(graphic);
                        if (pattern)
                        {
                            path.Flatten();
                            int count = path.GetPointCount();
                            std::vector<Gdiplus::PointF> points(count);
                            std::vector<BYTE> types(count);
                            path.GetPathPoints(points.data(), count);
                            path.GetPathTypes(types.data(), count);
                            float size = (float)Num(Value(graphic, "Size"), pattern->GetWidth());
                            size = (std::max)(2.0f, (std::min)(128.0f, size));
                            int drawn = 0;
                            for (int k = 1; k < count && drawn < 4096; ++k)
                            {
                                if ((types[k] & 7) == Gdiplus::PathPointTypeStart)
                                    continue;
                                float dx = points[k].X - points[k - 1].X,
                                      dy = points[k].Y - points[k - 1].Y,
                                      len = sqrtf(dx * dx + dy * dy);
                                for (float d = size / 2; d < len && drawn < 4096; d += size)
                                {
                                    auto saved = graphics.Save();
                                    graphics.TranslateTransform(points[k - 1].X + dx * d / len,
                                                                points[k - 1].Y + dy * d / len);
                                    graphics.RotateTransform(atan2f(dy, dx) * 180 / 3.14159265f);
                                    graphics.DrawImage(pattern, -size / 2, -size / 2, size, size);
                                    graphics.Restore(saved);
                                    ++drawn;
                                }
                            }
                        }
                    }
                    painted = true;
                }
                else if (type == "PointSymbolizer")
                {
                    auto graphic = Child(symbol, "Graphic");
                    auto image = Image(graphic);
                    float size = (float)Num(Value(graphic, "Size"), image ? image->GetHeight() : 8);
                    size = (std::max)(1.0f, (std::min)(128.0f, size));
                    auto mark = Child(graphic, "Mark");
                    Gdiplus::SolidBrush brush(Color(Param(Child(mark, "Fill"), "fill", "#2870be")));
                    Gdiplus::Pen pen(Color(Param(Child(mark, "Stroke"), "stroke", "#000000")), 1);
                    std::function<void(OGRGeometry *)> points = [&](OGRGeometry *g)
                    {
                        auto t = wkbFlatten(g->getGeometryType());
                        if (t == wkbMultiPoint)
                        {
                            auto col = g->toGeometryCollection();
                            for (int i = 0; i < col->getNumGeometries(); ++i)
                                points(col->getGeometryRef(i));
                            return;
                        }
                        OGREnvelope e;
                        g->getEnvelope(&e);
                        float x = (float)(((e.MinX + e.MaxX) / 2 - west) * sx),
                              y = (float)((north - (e.MinY + e.MaxY) / 2) * sy);
                        if (image)
                        {
                            float width = size * image->GetWidth() / (float)image->GetHeight();
                            if (symbols)
                            {
                                for (auto &entry : images)
                                    if (entry.second.get() == image)
                                    {
                                        symbols->push_back({entry.first, (e.MinX + e.MaxX) / 2,
                                                            (e.MinY + e.MaxY) / 2, width, size});
                                        break;
                                    }
                            }
                            else
                                graphics.DrawImage(image, x - width / 2, y - size / 2, width, size);
                        }
                        else if (Value(mark, "WellKnownName") == "square")
                        {
                            graphics.FillRectangle(&brush, x - size / 2, y - size / 2, size, size);
                            graphics.DrawRectangle(&pen, x - size / 2, y - size / 2, size, size);
                        }
                        else
                        {
                            graphics.FillEllipse(&brush, x - size / 2, y - size / 2, size, size);
                            graphics.DrawEllipse(&pen, x - size / 2, y - size / 2, size, size);
                        }
                    };
                    points(geo);
                    painted = true;
                }
                else if (type == "TextSymbolizer")
                {
                    auto label = Child(symbol, "Label");
                    auto expression = Child(label, "PropertyName");
                    auto text = expression ? feature->GetFieldAsString(Text(expression).c_str())
                                           : Text(label);
                    if (std::string(text).empty())
                        continue;
                    auto font = Child(symbol, "Font");
                    auto family = Wide(Param(font, "font-family", "Arial"));
                    Gdiplus::Font f(family.c_str(), (float)Num(Param(font, "font-size", "10"), 10),
                                    Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                    Gdiplus::SolidBrush brush(
                        Color(Param(Child(symbol, "Fill"), "fill", "#000000")));
                    OGREnvelope e;
                    geo->getEnvelope(&e);
                    auto value = Wide(text);
                    Gdiplus::PointF at((float)(((e.MinX + e.MaxX) / 2 - west) * sx),
                                       (float)((north - (e.MinY + e.MaxY) / 2) * sy));
                    auto placement = Child(Child(symbol, "LabelPlacement"), "PointPlacement");
                    auto anchor = Child(placement, "AnchorPoint"),
                         displacement = Child(placement, "Displacement");
                    Gdiplus::RectF bounds;
                    graphics.MeasureString(value.c_str(), (INT)value.size(), &f, at, &bounds);
                    at.X += (float)(Num(Value(displacement, "DisplacementX")) -
                                    bounds.Width * Num(Value(anchor, "AnchorPointX"), 0.5));
                    at.Y -= (float)(Num(Value(displacement, "DisplacementY")) +
                                    bounds.Height * Num(Value(anchor, "AnchorPointY"), 0.5));
                    graphics.DrawString(value.c_str(), (INT)value.size(), &f, at, &brush);
                    painted = true;
                }
            }
        }
    };
    visit(xml.get());
    return painted;
}
} // namespace vtstyle
