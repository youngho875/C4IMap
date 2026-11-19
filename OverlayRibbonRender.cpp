#include "pch.h"
#include "OverlayRibbonRender.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <atlconv.h>
#include "cpl_string.h"
namespace
{
Gdiplus::Color Color(int rgb, int alpha)
{
    return Gdiplus::Color((BYTE)alpha, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}
void Arrow(Gdiplus::Graphics &g, int kind, float x, float y, float angle,
           const CLocalOverlay::Style &style)
{
    if (!kind)
        return;
    auto state = g.Save();
    g.TranslateTransform(x, y);
    g.RotateTransform(angle);
    const float size = (std::max)(6.f, (float)style.width * 3);
    Gdiplus::Pen pen(Color(style.lineColor, style.lineAlpha), (float)style.width);
    Gdiplus::SolidBrush brush(Color(style.lineColor, style.lineAlpha));
    Gdiplus::GraphicsPath path;
    bool filled = false;
    if (kind >= 30)
    {
        int base = kind <= 32 ? kind - 29 : kind - 29;
        Arrow(g, base, 0, 0, 0, style);
        Arrow(g, base, -size, 0, 0, style);
    }
    else if (kind == 25 || kind == 26)
    {
        path.AddEllipse(-size, -size / 2, size, size);
        filled = kind == 26;
    }
    else if (kind == 23 || kind == 24)
    {
        path.AddRectangle(Gdiplus::RectF(-size, -size / 2, size, size));
        filled = kind == 24;
    }
    else if (kind == 27 || kind == 28)
    {
        Gdiplus::PointF points[] = {
            {0, 0}, {-size / 2, -size / 2}, {-size, 0}, {-size / 2, size / 2}};
        path.AddPolygon(points, 4);
        filled = kind == 28;
    }
    else if (kind == 14 || kind == 15 || kind == 16 || kind == 29)
    {
        if (kind == 14 || kind == 16)
            path.AddLine(-size, -size / 2, 0.f, size / 2);
        if (kind == 15 || kind == 16)
        {
            path.StartFigure();
            path.AddLine(-size, size / 2, 0.f, -size / 2);
        }
        if (kind == 29)
            path.AddLine(-size / 2, -size / 2, -size / 2, size / 2);
    }
    else
    {
        bool left = true, right = true, tail = kind >= 4 && kind <= 13;
        if (kind == 2 || kind == 5 || kind == 8 || kind == 10 || kind == 12 || kind == 19 ||
            kind == 20)
            right = false;
        if (kind == 3 || kind == 6 || kind == 9 || kind == 11 || kind == 13 || kind == 21 ||
            kind == 22)
            left = false;
        filled = (kind >= 7 && kind <= 9) || kind == 12 || kind == 13 || kind == 18 || kind == 20 ||
                 kind == 22;
        float tip = tail ? -size : 0, back = tail ? 0 : -size;
        if (left)
            path.AddLine(back, -size / 2, tip, 0.f);
        if (right)
            path.AddLine(tip, 0.f, back, size / 2);
        if (filled || (kind >= 17 && kind <= 22))
        {
            path.AddLine(back, right ? size / 2 : 0, back, left ? -size / 2 : 0);
            path.CloseFigure();
        }
        if (kind >= 10 && kind <= 13)
        {
            path.StartFigure();
            path.AddLine(-size, 0.f, 0.f, 0.f);
        }
    }
    if (filled)
        g.FillPath(&brush, &path);
    else
        g.DrawPath(&pen, &path);
    g.Restore(state);
}
void Text(Gdiplus::Graphics &g, const Gdiplus::RectF &box, const CLocalOverlay::Style &s)
{
    if (!s.text[0])
        return;
    Gdiplus::FontFamily family(s.font);
    const Gdiplus::FontFamily *use =
        family.GetLastStatus() == Gdiplus::Ok ? &family : Gdiplus::FontFamily::GenericSansSerif();
    Gdiplus::Font font(use, (float)s.fontSize, s.fontStyle, Gdiplus::UnitPoint);
    Gdiplus::StringFormat format;
    format.SetAlignment((Gdiplus::StringAlignment)s.textAlign);
    format.SetLineAlignment((Gdiplus::StringAlignment)s.textAlignY);
    int flags = Gdiplus::StringFormatFlagsNoClip;
    if (s.vertical)
        flags |= Gdiplus::StringFormatFlagsDirectionVertical;
    if (s.rightToLeft)
        flags |= Gdiplus::StringFormatFlagsDirectionRightToLeft;
    if (s.autoTextBox)
        flags |= Gdiplus::StringFormatFlagsNoWrap;
    format.SetFormatFlags(flags);
    Gdiplus::SolidBrush brush(Color(s.textColor, 255));
    std::vector<std::wstring> lines;
    std::wstring input = s.text, line;
    Gdiplus::StringFormat measureFormat(Gdiplus::StringFormat::GenericTypographic());
    measureFormat.SetFormatFlags(Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    float limit = s.vertical ? box.Height : box.Width;
    for (wchar_t ch : input)
    {
        if (ch == L'\r')
            continue;
        if (ch == L'\n')
        {
            lines.push_back(line);
            line.clear();
            continue;
        }
        std::wstring candidate = line + ch;
        Gdiplus::RectF measured;
        g.MeasureString(candidate.c_str(), (INT)candidate.size(), &font, Gdiplus::PointF(0, 0),
                        &measureFormat, &measured);
        if (!s.autoTextBox && !line.empty() && measured.Width > limit)
        {
            lines.push_back(line);
            line.clear();
        }
        line += ch;
    }
    lines.push_back(line);
    float glyphHeight = font.GetHeight(&g), advance = glyphHeight * s.lineInterval / 100.f;
    float total = glyphHeight + (lines.size() - 1) * advance;
    float available = s.vertical ? box.Width : box.Height;
    float offset = s.textAlignY == 1   ? (available - total) / 2
                   : s.textAlignY == 2 ? available - total
                                       : 0;
    format.SetLineAlignment(Gdiplus::StringAlignmentNear);
    format.SetFormatFlags(flags | Gdiplus::StringFormatFlagsNoWrap);
    auto state = g.Save();
    if (!s.autoTextBox)
        g.SetClip(box, Gdiplus::CombineModeIntersect);
    for (size_t n = 0; n < lines.size(); ++n)
    {
        Gdiplus::RectF layout = box;
        if (s.vertical)
        {
            layout.X += offset + n * advance;
            layout.Width = glyphHeight * 1.5f;
        }
        else
        {
            layout.Y += offset + n * advance;
            layout.Height = glyphHeight * 1.5f;
        }
        g.DrawString(lines[n].c_str(), (INT)lines[n].size(), &font, layout, &format, &brush);
    }
    g.Restore(state);
}
} // namespace
std::string OverlayRibbonRender::Escape(const wchar_t *value)
{
    CW2A utf8(value, CP_UTF8);
    char *escaped = CPLEscapeString(utf8, -1, CPLES_XML);
    std::string result = escaped ? escaped : "";
    CPLFree(escaped);
    return result;
}
void OverlayRibbonRender::Draw(Gdiplus::Graphics &g, int type, double x1, double y1, double x2,
                               double y2, const CLocalOverlay::Style &s,
                               const std::vector<Gdiplus::PointF> &points)
{
    const auto saved = g.Save();
    const float cx = (float)(x1 + x2) / 2, cy = (float)(y1 + y2) / 2;
    g.TranslateTransform(cx, cy);
    g.RotateTransform((float)s.rotation * 90);
    g.ScaleTransform(s.flipX ? -1.f : 1.f, s.flipY ? -1.f : 1.f);
    g.TranslateTransform(-cx, -cy);
    const bool closed = type >= CLocalOverlay::Rectangle && type != CLocalOverlay::Arc &&
                        type != CLocalOverlay::Elbow && type != CLocalOverlay::Curve &&
                        type != CLocalOverlay::Polyline && type != CLocalOverlay::Spline;
    Gdiplus::RectF box((float)(std::min)(x1, x2), (float)(std::min)(y1, y2),
                       (float)(std::max)(1., fabs(x2 - x1)), (float)(std::max)(1., fabs(y2 - y1)));
    Gdiplus::GraphicsPath path;
    const float l = box.X, t = box.Y, w = box.Width, h = box.Height;
    auto polygon = [&](std::initializer_list<Gdiplus::PointF> points) {
        path.AddPolygon(points.begin(), (INT)points.size());
    };
    if (type >= CLocalOverlay::Polyline && points.size() >= 2)
    {
        if (type == CLocalOverlay::ClosedSpline && points.size() >= 3)
            path.AddClosedCurve(points.data(), (INT)points.size());
        else if (type == CLocalOverlay::Spline)
            path.AddCurve(points.data(), (INT)points.size());
        else if (type == CLocalOverlay::Polygon && points.size() >= 3)
            path.AddPolygon(points.data(), (INT)points.size());
        else
            path.AddLines(points.data(), (INT)points.size());
    }
    else if (type == CLocalOverlay::Rectangle || type == CLocalOverlay::TextBox)
        path.AddRectangle(box);
    else if (type == CLocalOverlay::Ellipse)
        path.AddEllipse(box);
    else if (type == CLocalOverlay::RoundRect)
    {
        float d = (std::min)(w, h) / 3;
        path.AddArc(l, t, d, d, 180.f, 90.f);
        path.AddArc(l + w - d, t, d, d, 270.f, 90.f);
        path.AddArc(l + w - d, t + h - d, d, d, 0.f, 90.f);
        path.AddArc(l, t + h - d, d, d, 90.f, 90.f);
        path.CloseFigure();
    }
    else if (type == CLocalOverlay::Triangle)
        polygon({{l + w / 2, t}, {l + w, t + h}, {l, t + h}});
    else if (type == CLocalOverlay::RightTriangle)
        polygon({{l, t}, {l + w, t + h}, {l, t + h}});
    else if (type == CLocalOverlay::Parallelogram)
        polygon({{l + w / 4, t}, {l + w, t}, {l + w * 3 / 4, t + h}, {l, t + h}});
    else if (type == CLocalOverlay::Trapezoid)
        polygon({{l + w / 4, t}, {l + w * 3 / 4, t}, {l + w, t + h}, {l, t + h}});
    else if (type == CLocalOverlay::Diamond)
        polygon({{l + w / 2, t}, {l + w, t + h / 2}, {l + w / 2, t + h}, {l, t + h / 2}});
    else if (type == CLocalOverlay::Pentagon || type == CLocalOverlay::Hexagon)
    {
        Gdiplus::PointF points[6];
        int count = type == CLocalOverlay::Pentagon ? 5 : 6;
        for (int n = 0; n < count; ++n)
        {
            double a = 6.283185307179586 * n / count - 1.570796326794897;
            points[n] = {l + w / 2 + w / 2 * (float)cos(a), t + h / 2 + h / 2 * (float)sin(a)};
        }
        path.AddPolygon(points, count);
    }
    else if (type == CLocalOverlay::Arc)
        path.AddArc(box, 0.f, 180.f);
    else if (type == CLocalOverlay::Pie)
        path.AddPie(l, t, w, h, 0.f, 270.f);
    else if (type == CLocalOverlay::Chord)
    {
        path.AddArc(box, 0.f, 180.f);
        path.CloseFigure();
    }
    else if (type == CLocalOverlay::Elbow)
    {
        Gdiplus::PointF points[] = {{(float)x1, (float)y1},
                                    {(float)(x1 + x2) / 2, (float)y1},
                                    {(float)(x1 + x2) / 2, (float)y2},
                                    {(float)x2, (float)y2}};
        path.AddLines(points, 4);
    }
    else if (type == CLocalOverlay::Curve)
        path.AddBezier((float)x1, (float)y1, (float)(x1 + x2) / 2, (float)y1, (float)(x1 + x2) / 2,
                       (float)y2, (float)x2, (float)y2);
    else
        path.AddLine((float)x1, (float)y1, (float)x2, (float)y2);
    auto fill = Color(s.fillColor, s.fillAlpha), pattern = Color(s.patternColor, s.fillAlpha);
    auto gradient = Color(s.gradientColor, s.fillAlpha);
    if (closed && s.fillEnabled)
    {
        if (s.fillType == 2)
        {
            Gdiplus::HatchBrush brush((Gdiplus::HatchStyle)s.hatch, pattern, fill);
            g.FillPath(&brush, &path);
        }
        else if (s.fillType == 3)
        {
            Gdiplus::PathGradientBrush brush(&path);
            brush.SetCenterColor(gradient);
            INT count = 1;
            brush.SetSurroundColors(&fill, &count);
            float focus = s.gradientPosition / 100.f;
            brush.SetFocusScales(focus, focus);
            g.FillPath(&brush, &path);
        }
        else if (s.fillType == 4)
        {
            Gdiplus::LinearGradientBrush brush(box, fill, gradient,
                                               (Gdiplus::LinearGradientMode)s.gradientMode);
            float positions[] = {0, s.gradientPosition / 100.f, 1};
            float factors[] = {0, .5f, 1};
            brush.SetBlend(factors, positions, 3);
            g.FillPath(&brush, &path);
        }
        else if (s.fillType >= 5 && s.imagePath[0])
        {
            Gdiplus::Image image(s.imagePath);
            if (image.GetLastStatus() == Gdiplus::Ok)
            {
                Gdiplus::ColorMatrix matrix = {
                    1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, s.fillAlpha / 255.f,
                    0, 0, 0, 0, 0, 1};
                Gdiplus::ImageAttributes attrs;
                attrs.SetColorMatrix(&matrix);
                auto state = g.Save();
                g.SetClip(&path, Gdiplus::CombineModeIntersect);
                if (s.fillType == 5)
                    g.DrawImage(&image, box, 0, 0, (float)image.GetWidth(),
                                (float)image.GetHeight(), Gdiplus::UnitPixel, &attrs);
                else
                {
                    Gdiplus::TextureBrush brush(
                        &image, Gdiplus::Rect(0, 0, image.GetWidth(), image.GetHeight()), &attrs);
                    brush.SetWrapMode(Gdiplus::WrapModeTile);
                    g.FillPath(&brush, &path);
                }
                g.Restore(state);
            }
        }
        else
        {
            Gdiplus::SolidBrush brush(fill);
            g.FillPath(&brush, &path);
        }
    }
    if (s.lineEnabled && s.width > 0)
    {
        Gdiplus::Pen pen(Color(s.lineColor, s.lineAlpha), (float)s.width);
        pen.SetDashStyle((Gdiplus::DashStyle)s.dash);
        const Gdiplus::DashCap caps[] = {Gdiplus::DashCapFlat, Gdiplus::DashCapRound,
                                         Gdiplus::DashCapTriangle};
        pen.SetDashCap(caps[s.lineCap]);
        pen.SetStartCap(s.lineCap == 1   ? Gdiplus::LineCapRound
                        : s.lineCap == 2 ? Gdiplus::LineCapTriangle
                                         : Gdiplus::LineCapFlat);
        pen.SetEndCap(pen.GetStartCap());
        pen.SetLineJoin((Gdiplus::LineJoin)s.lineJoin);
        if (closed && s.lineAlignment)
            pen.SetAlignment(Gdiplus::PenAlignmentInset);
        else if (s.lineStripe)
        {
            const float compound[][6] = {{0, .3f, .7f, 1, 0, 0},
                                         {0, .15f, .45f, 1, 0, 0},
                                         {0, .55f, .85f, 1, 0, 0},
                                         {0, .2f, .4f, .6f, .8f, 1}};
            pen.SetCompoundArray(compound[s.lineStripe - 1], s.lineStripe == 4 ? 6 : 4);
        }
        g.DrawPath(&pen, &path);
        if (!closed && path.GetPointCount() >= 2)
        {
            std::vector<Gdiplus::PointF> endpoints(path.GetPointCount());
            path.GetPathPoints(endpoints.data(), (INT)endpoints.size());
            const auto first = endpoints.front(), second = endpoints[1], last = endpoints.back(),
                       before = endpoints[endpoints.size() - 2];
            float startAngle =
                (float)(atan2(first.Y - second.Y, first.X - second.X) * 180 / 3.141592653589793);
            float endAngle =
                (float)(atan2(last.Y - before.Y, last.X - before.X) * 180 / 3.141592653589793);
            Arrow(g, s.startArrow, first.X, first.Y, startAngle, s);
            Arrow(g, s.endArrow, last.X, last.Y, endAngle, s);
        }
    }
    Text(g, box, s);
    g.Restore(saved);
}

#include <ole2.h>
#include <shlobj.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

bool OverlayRibbonRender::ImportOlePreview(const CString &source, CString &pngPath, CString &error)
{
    HRESULT initialized = OleInitialize(nullptr);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
    {
        error = _T("OLE 초기화에 실패했습니다.");
        return false;
    }
    ILockBytes *bytes = nullptr;
    IStorage *storage = nullptr;
    IOleObject *object = nullptr;
    HRESULT result = CreateILockBytesOnHGlobal(nullptr, TRUE, &bytes);
    if (SUCCEEDED(result))
        result = StgCreateDocfileOnILockBytes(
            bytes, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &storage);
    CStringW file(source);
    if (SUCCEEDED(result))
        result = OleCreateFromFile(CLSID_NULL, file, IID_IOleObject, OLERENDER_DRAW, nullptr,
                                   nullptr, storage, (void **)&object);
    bool saved = false;
    if (SUCCEEDED(result))
    {
        SIZEL extent = {16000, 12000};
        object->GetExtent(DVASPECT_CONTENT, &extent);
        double ratio = extent.cy > 0 ? (double)extent.cx / extent.cy : 4.0 / 3;
        ratio = (std::max)(.05, (std::min)(20., ratio));
        int width = ratio >= 1 ? 1600 : (int)(1600 * ratio),
            height = ratio >= 1 ? (int)(1600 / ratio) : 1600;
        Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics graphics(&bitmap);
            graphics.Clear(Gdiplus::Color::White);
            HDC dc = graphics.GetHDC();
            RECT bounds = {0, 0, width, height};
            result = OleDraw(object, DVASPECT_CONTENT, dc, &bounds);
            graphics.ReleaseHDC(dc);
        }
        if (SUCCEEDED(result))
        {
            wchar_t folder[MAX_PATH] = {};
            result = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr,
                                      SHGFP_TYPE_CURRENT, folder);
            if (SUCCEEDED(result))
            {
                CStringW directory(folder);
                directory += L"\\C4IMap\\OverlayPreviews";
                int created = SHCreateDirectoryExW(nullptr, directory, nullptr);
                if (created == ERROR_SUCCESS || created == ERROR_ALREADY_EXISTS ||
                    created == ERROR_FILE_EXISTS)
                {
                    GUID guid;
                    CoCreateGuid(&guid);
                    wchar_t id[64];
                    StringFromGUID2(guid, id, 64);
                    CStringW destination = directory + L"\\" + id + L".png";
                    UINT count = 0, size = 0;
                    Gdiplus::GetImageEncodersSize(&count, &size);
                    std::vector<BYTE> buffer(size);
                    if (size &&
                        Gdiplus::GetImageEncoders(
                            count, size, (Gdiplus::ImageCodecInfo *)buffer.data()) == Gdiplus::Ok)
                    {
                        auto codecs = (Gdiplus::ImageCodecInfo *)buffer.data();
                        for (UINT n = 0; n < count; ++n)
                            if (wcscmp(codecs[n].MimeType, L"image/png") == 0)
                            {
                                saved = bitmap.Save(destination, &codecs[n].Clsid) == Gdiplus::Ok;
                                if (saved)
                                    pngPath = destination;
                                break;
                            }
                    }
                }
            }
        }
    }
    if (object)
    {
        object->Close(OLECLOSE_NOSAVE);
        object->Release();
    }
    if (storage)
        storage->Release();
    if (bytes)
        bytes->Release();
    if (SUCCEEDED(initialized))
        OleUninitialize();
    if (!saved)
        error.Format(_T("OLE 미리보기를 만들 수 없습니다 (0x%08X). 해당 문서의 OLE 프로그램이 ")
                     _T("설치되어 있는지 확인하세요."),
                     (unsigned int)result);
    return saved;
}
