#include "pch.h"
#include "ProjectedRaster.h"
#include "MapBuildOptions.h"
void DrawProjectedRaster(CDC *dc, CCoordMgr *mgr, CRect rect,
                         const std::function<void(CDC *, CCoordMgr *, CRect)> &draw)
{
    if (!mgr->HasDisplayTransform())
    {
        draw(dc, mgr, rect);
        return;
    }
    if (rect.Width() <= 0 || rect.Height() <= 0)
        return;
    CCoord coord;
    double wx, wy, clat, clon;
    mgr->ScreenXY2WorldXY(rect.CenterPoint().x, rect.CenterPoint().y, &wx, &wy);
    coord.x_y_to_degrees_lat_long(wx, wy, &clat, &clon);
    if (!std::isfinite(clat) || !std::isfinite(clon))
        return;
    auto geo = [&](int x, int y, double &lon, double &lat)
    {
        mgr->ScreenXY2WorldXY(x, y, &wx, &wy);
        if (!std::isfinite(wx) || !std::isfinite(wy))
            return false;
        coord.x_y_to_degrees_lat_long(wx, wy, &lat, &lon);
        lon = clon + std::remainder(lon - clon, 360.0);
        return std::isfinite(lon) && std::isfinite(lat) && fabs(lat) <= 90;
    };
    double west = 1e100, east = -1e100, south = 1e100, north = -1e100;
    for (int y = 0; y <= 16; ++y)
        for (int x = 0; x <= 16; ++x)
        {
            double lon, lat;
            if (geo(rect.left + x * rect.Width() / 16, rect.top + y * rect.Height() / 16, lon, lat))
            {
                west = (std::min)(west, lon);
                east = (std::max)(east, lon);
                south = (std::min)(south, lat);
                north = (std::max)(north, lat);
            }
        }
    if (!(east > west && north > south))
        return;
    double marginX = (east - west) * .03, marginY = (north - south) * .03;
    west -= marginX;
    east += marginX;
    south = (std::max)(-90.0, south - marginY);
    north = (std::min)(90.0, north + marginY);
    const int limit = 1024;
    double scale = (std::min)(limit / ((east - west) * 47.5), limit / ((north - south) * 60));
    int sw = (std::max)(1, (int)ceil((east - west) * 47.5 * scale)),
        sh = (std::max)(1, (int)ceil((north - south) * 60 * scale));
    std::unique_ptr<CCoordMgr> flat(CreateIndependentCoordMgr());
    flat->m_bFlat = true;
    flat->m_fScale = scale;
    flat->m_fCenterX = -(west - 127.031) * 47.5 * scale;
    flat->m_fCenterY = (north - 37.0861) * 60 * scale;
    flat->m_nDisplayViewWidth = sw;
    flat->m_nDisplayViewHeight = sh;
    CImage source;
    if (!source.Create(sw, sh, 32, CImage::createAlphaChannel))
        return;
    for (int y = 0; y < sh; ++y)
        memset(source.GetPixelAddress(0, y), 0, sw * 4);
    HDC h = source.GetDC();
    draw(CDC::FromHandle(h), flat.get(), CRect(0, 0, sw, sh));
    source.ReleaseDC();
    double ratio = (std::min)(1.0, 1024.0 / (std::max)(rect.Width(), rect.Height()));
    int dw = (std::max)(1, int(rect.Width() * ratio)),
        dh = (std::max)(1, int(rect.Height() * ratio));
    CImage image;
    if (!image.Create(dw, dh, 32, CImage::createAlphaChannel))
        return;
    for (int y = 0; y < dh; ++y)
    {
        auto row = (DWORD *)image.GetPixelAddress(0, y);
        for (int x = 0; x < dw; ++x)
        {
            row[x] = 0;
            double lon, lat;
            if (!geo(rect.left + (int)((x + .5) * rect.Width() / dw),
                     rect.top + (int)((y + .5) * rect.Height() / dh), lon, lat))
                continue;
            int ix = (int)floor((lon - west) * 47.5 * scale),
                iy = (int)floor((north - lat) * 60 * scale);
            if (ix >= 0 && ix < sw && iy >= 0 && iy < sh)
                row[x] = *(DWORD *)source.GetPixelAddress(ix, iy);
        }
    }
    image.AlphaBlend(dc->GetSafeHdc(), rect.left, rect.top, rect.Width(), rect.Height(), 0, 0, dw,
                     dh);
}
