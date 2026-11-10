#include "pch.h"
#include "MapDecoration.h"
#include "BaseMapOptions.h"
#include "MainFrm.h"
#include "MapBuildOptions.h"
MapCompassImage::MapCompassImage()
{
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&token_, &startup, nullptr) != Gdiplus::Ok)
        return;
    HMODULE module = AfxGetResourceHandle();
    HRSRC resource = FindResource(module, _T("MAP_COMPASS_JPEG"), RT_RCDATA);
    if (!resource)
        return;
    DWORD bytes = SizeofResource(module, resource);
    const void *data = LockResource(LoadResource(module, resource));
    if (!data || !bytes)
        return;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory)
        return;
    void *target = GlobalLock(memory);
    if (!target)
    {
        GlobalFree(memory);
        return;
    }
    memcpy(target, data, bytes);
    GlobalUnlock(memory);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)))
    {
        GlobalFree(memory);
        return;
    }
    {
        Gdiplus::Bitmap source(stream);
        if (source.GetLastStatus() == Gdiplus::Ok)
            image_ =
                source.Clone(0, 0, source.GetWidth(), source.GetHeight(), PixelFormat32bppARGB);
    }
    stream->Release();
    if (!image_)
        return;
    const int width = image_->GetWidth(), height = image_->GetHeight();
    Gdiplus::Rect all(0, 0, width, height);
    Gdiplus::BitmapData pixels = {};
    if (image_->LockBits(&all, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
                         PixelFormat32bppARGB, &pixels) != Gdiplus::Ok)
    {
        delete image_;
        image_ = nullptr;
        return;
    }
    int left = width, top = height, right = -1, bottom = -1;
    for (int y = 0; y < height; ++y)
    {
        BYTE *row = static_cast<BYTE *>(pixels.Scan0) + y * pixels.Stride;
        for (int x = 0; x < width; ++x)
        {
            BYTE *c = row + x * 4;
            int white = (std::min)((int)c[0], (std::min)((int)c[1], (int)c[2]));
            // Remove the white matte, including near-white JPEG noise.
            int alpha = white >= 245 ? 0 : 255 - white;
            for (int channel = 0; channel < 3; ++channel)
                c[channel] = alpha ? BYTE((c[channel] - white) * 255 / alpha) : 0;
            c[3] = BYTE(alpha);
            if (alpha > 20)
            {
                left = (std::min)(left, x);
                right = (std::max)(right, x);
                top = (std::min)(top, y);
                bottom = (std::max)(bottom, y);
            }
        }
    }
    image_->UnlockBits(&pixels);
    crop_ = right >= left ? Gdiplus::Rect(left, top, right - left + 1, bottom - top + 1) : all;
}

MapCompassImage::~MapCompassImage()
{
    delete image_;
    if (token_)
        Gdiplus::GdiplusShutdown(token_);
}

void MapCompassImage::Draw(HDC dc, float cx, float cy, double nx, double ny)
{
    if (!image_)
        return;
    Gdiplus::Graphics graphics(dc);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.TranslateTransform(cx, cy);
    graphics.RotateTransform(float(atan2(nx, -ny) * 180.0 / 3.141592653589793));
    float factor = 88.0f / (std::max)(crop_.Width, crop_.Height);
    float w = crop_.Width * factor, h = crop_.Height * factor;
    graphics.DrawImage(image_, Gdiplus::RectF(-w / 2, -h / 2, w, h), float(crop_.X), float(crop_.Y),
                       float(crop_.Width), float(crop_.Height), Gdiplus::UnitPixel);
}

bool MapScreenLonLat(CCoordMgr *mgr, double x, double y, double &lon, double &lat)
{
    if (!mgr || mgr->m_fScale <= 0)
        return false;
    if (mgr->m_bFlat)
    {
        lon = 127.031 + (x - mgr->m_fCenterX) / (47.5 * mgr->m_fScale);
        lat = 37.0861 - (y - mgr->m_fCenterY) / (60.0 * mgr->m_fScale);
    }
    else
    {
        double wx, wy;
        mgr->ScreenXY2WorldXY((int)x, (int)y, &wx, &wy);
        CCoord c;
        c.initialize_adaptation_data();
        c.x_y_to_degrees_lat_long(wx, wy, &lat, &lon);
    }
    return std::isfinite(lon) && std::isfinite(lat) && fabs(lat) <= 90;
}

double MapDistanceMetres(double lon1, double lat1, double lon2, double lat2)
{
    const double rad = 3.141592653589793 / 180;
    double dlat = (lat2 - lat1) * rad, dlon = std::remainder(lon2 - lon1, 360.0) * rad;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * rad) * cos(lat2 * rad) * sin(dlon / 2) * sin(dlon / 2);
    return 6371008.8 * 2 * asin(sqrt((std::max)(0.0, (std::min)(1.0, a))));
}

double MapNiceDistance(double maximum)
{
    if (!std::isfinite(maximum) || maximum <= 0)
        return 0;
    double power = pow(10.0, floor(log10(maximum))), fraction = maximum / power;
    return power * (fraction >= 5 ? 5 : fraction >= 2 ? 2 : 1);
}

void DrawMapDecoration(CDC *dc, CCoordMgr *mgr, CMainFrame *frame, const CRect &viewport)
{
    if (!dc || !mgr || viewport.Width() < 220 || viewport.Height() < 120)
        return;
    double lon, lat, lon2, lat2;
    double x = viewport.CenterPoint().x, y = viewport.CenterPoint().y;
    if (!MapScreenLonLat(mgr, x, y, lon, lat) || !MapScreenLonLat(mgr, x + 1, y, lon2, lat2))
        return;
    const double metresPerPixel = MapDistanceMetres(lon, lat, lon2, lat2);
    if (metresPerPixel <= 0)
        return;
    double denominator = metresPerPixel * dc->GetDeviceCaps(LOGPIXELSX) / 0.0254;
    CString number;
    number.Format(_T("%.0f"), denominator);
    for (int pos = number.GetLength() - 3; pos > 0; pos -= 3)
        number.Insert(pos, _T(','));
    CString status = BaseMapConfig().Flag("ScaleStatus") ? _T("Scale 1:") + number : CString();
    if (frame)
    {
        auto pane = frame->m_wndStatusBar.FindElement(59220);
        if (pane && CString(pane->GetText()) != status)
        {
            pane->SetText(status);
            frame->m_wndStatusBar.RecalcLayout();
            frame->m_wndStatusBar.RedrawWindow();
        }
    }
    int saved = dc->SaveDC();
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(RGB(20, 30, 40));
    CFont font;
    font.CreatePointFont(90, _T("Segoe UI"), dc);
    dc->SelectObject(&font);
    const int left = viewport.left + 18, bottom = viewport.bottom - 20;
    // Distance at the bar's latitude (projection scale varies with latitude).
    double blon, blat, elon, elat;
    if (BaseMapConfig().Flag("ScaleBar") && MapScreenLonLat(mgr, left, bottom, blon, blat) &&
        MapScreenLonLat(mgr, left + 1, bottom, elon, elat))
    {
        double unit = MapDistanceMetres(blon, blat, elon, elat);
        double distance = MapNiceDistance(unit * 140);
        if (unit > 0 && distance > 0)
        {
            int width = (int)std::lround(distance / unit);
            CString label;
            if (distance >= 1000)
                label.Format(_T("%g km"), distance / 1000);
            else
                label.Format(_T("%g m"), distance);
            dc->FillSolidRect(left - 8, bottom - 28, 180, 45, RGB(255, 255, 255));
            dc->TextOut(left, bottom - 25, _T("0"));
            dc->TextOut(left + width / 2, bottom - 25, label);
            dc->FillSolidRect(left, bottom - 5, width, 7, RGB(20, 30, 40));
            dc->FillSolidRect(left + width / 2, bottom - 4, width - width / 2 - 1, 5,
                              RGB(255, 255, 255));
        }
    }
    if (!BaseMapConfig().Flag("Compass"))
    {
        dc->RestoreDC(saved);
        return;
    }
    const int cx = viewport.right - 70, cy = viewport.top + 70;
    double nx = 0, ny = -1;
    if (!mgr->m_bFlat)
    {
        double northX, northY;
        mgr->WorldLonLat2ScreenXY(lon, (std::min)(89.999, lat + 0.1), &northX, &northY,
                                  mgr->m_fCenterX, mgr->m_fCenterY, mgr->m_fScale);
        nx = northX - x;
        ny = northY - y;
        double length = hypot(nx, ny);
        if (length < 1e-8)
        {
            nx = 0;
            ny = -1;
        }
        else
        {
            nx /= length;
            ny /= length;
        }
    }
    static MapCompassImage compass;
    compass.Draw(dc->GetSafeHdc(), float(cx), float(cy), nx, ny);
    dc->RestoreDC(saved);
}
