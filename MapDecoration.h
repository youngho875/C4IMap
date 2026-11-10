#pragma once
#include <cmath>
class CMainFrame;
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"

#pragma pack(push, 8)
#include <gdiplus.h>
#pragma pack(pop)
#pragma comment(lib, "gdiplus.lib")

// Decode once; embed res/compass.jpg so all deployment profiles carry the image.
class MapCompassImage
{
private:
    ULONG_PTR token_ = 0;
    Gdiplus::Bitmap *image_ = nullptr;
    Gdiplus::Rect crop_;

public:
    MapCompassImage();
    ~MapCompassImage();
    MapCompassImage(const MapCompassImage &) = delete;
    MapCompassImage &operator=(const MapCompassImage &) = delete;
    void Draw(HDC dc, float cx, float cy, double nx, double ny);
};

bool MapScreenLonLat(CCoordMgr *mgr, double x, double y, double &lon, double &lat);
double MapDistanceMetres(double lon1, double lat1, double lon2, double lat2);
double MapNiceDistance(double maximum);
void DrawMapDecoration(CDC *dc, CCoordMgr *mgr, CMainFrame *frame, const CRect &viewport);
