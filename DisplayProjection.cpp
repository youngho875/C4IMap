#include "pch.h"
#include "DisplayProjection.h"
#include "MapBuildOptions.h"
void CDisplayProjection::DeleteTransform::operator()(OGRCoordinateTransformation *p) const
{
    if (p)
        OCTDestroyCoordinateTransformation(p);
}

void CDisplayProjection::RotateXY(double *x, double *y, double degrees)
{
    double a = degrees * 3.141592653589793 / 180, xx = *x, yy = *y;
    *x = cos(a) * xx + sin(a) * yy;
    *y = -sin(a) * xx + cos(a) * yy;
}

bool CDisplayProjection::Forward(void *context, double *x, double *y)
{
    auto &p = *static_cast<CDisplayProjection *>(context);
    double lat, lon;
    p.coord.x_y_to_degrees_lat_long(*x, *y, &lat, &lon);
    if (!std::isfinite(lat) || !std::isfinite(lon) || fabs(lat) >= 89.999)
        return false;
    if (p.kind == 1 &&
        (lat < -80 || lat > 84 || fabs(std::remainder(lon - p.centralMeridian, 360.0)) > 45))
        return false;
    *x = lon;
    *y = lat;
    if (p.kind)
    {
        if (!p.forward->Transform(1, x, y) || !std::isfinite(*x) || !std::isfinite(*y))
            return false;
        *x /= 1852;
        *y /= 1852;
    }
    else
    {
        *x = (lon - 127.031) * 47.5;
        *y = (lat - 37.0861) * 60;
    }
    p.RotateXY(x, y, p.angle);
    return true;
}

bool CDisplayProjection::Inverse(void *context, double *x, double *y)
{
    auto &p = *static_cast<CDisplayProjection *>(context);
    p.RotateXY(x, y, -p.angle);
    if (p.kind)
    {
        *x *= 1852;
        *y *= 1852;
        if (!p.inverse->Transform(1, x, y))
            return false;
    }
    else
    {
        *x = 127.031 + *x / 47.5;
        *y = 37.0861 + *y / 60;
    }
    if (!std::isfinite(*x) || !std::isfinite(*y) || fabs(*y) > 90)
        return false;
    double lon = std::remainder(*x, 360.0), lat = *y;
    p.coord.degrees_lat_long_to_x_y(lat, lon, x, y);
    return std::isfinite(*x) && std::isfinite(*y);
}

int CDisplayProjection::Kind() const
{
    return kind;
}

double CDisplayProjection::Angle() const
{
    return angle;
}

bool CDisplayProjection::SetAngle(CCoordMgr *mgr, double degrees)
{
    if (!mgr || !std::isfinite(degrees))
        return false;
    int cx = mgr->m_nDisplayViewWidth / 2, cy = mgr->m_nDisplayViewHeight / 2;
    double wx, wy;
    mgr->ScreenXY2WorldXY(cx, cy, &wx, &wy);
    if (!std::isfinite(wx) || !std::isfinite(wy))
        return false;
    angle = std::remainder(degrees, 360.0);
    mgr->m_bFlat = kind == 0 && fabs(angle) < 1e-9;
    mgr->SetDisplayTransform(mgr->m_bFlat ? nullptr : Forward, mgr->m_bFlat ? nullptr : Inverse,
                             this);
    double sx, sy;
    mgr->WorldXY2ScreenXY(wx, wy, &sx, &sy, 0, 0, mgr->m_fScale);
    mgr->m_fCenterX = cx - sx;
    mgr->m_fCenterY = cy - sy;
    return true;
}

bool CDisplayProjection::Set(CCoordMgr *mgr, int selected)
{
    if (!mgr || selected < 0 || selected > 5)
        return false;
    int cx = mgr->m_nDisplayViewWidth / 2, cy = mgr->m_nDisplayViewHeight / 2;
    double wx, wy, lat, lon;
    mgr->ScreenXY2WorldXY(cx, cy, &wx, &wy);
    coord.x_y_to_degrees_lat_long(wx, wy, &lat, &lon);
    if (!std::isfinite(lat) || !std::isfinite(lon))
        return false;
    std::unique_ptr<OGRCoordinateTransformation, DeleteTransform> f, b;
    if (selected)
    {
        OGRSpatialReference geo, dst;
        geo.SetWellKnownGeogCS("WGS84");
        geo.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        dst.SetWellKnownGeogCS("WGS84");
        if (selected == 1)
        {
            if (lat < -80 || lat > 84)
                return false;
            int zone = (std::max)(1, (std::min)(60, int(floor((lon + 180) / 6)) + 1));
            dst.SetUTM(zone, lat >= 0);
            centralMeridian = zone * 6 - 183;
        }
        if (selected == 2)
        {
            if (fabs(lat) > 85)
                return false;
            dst.SetMercator(0, 0, 1, 0, 0);
        }
        if (selected == 3)
            dst.SetLCC(lat >= 0 ? 30 : -30, lat >= 0 ? 60 : -60, lat, lon, 0, 0);
        if (selected == 4)
            dst.SetStereographic(lat, lon, 1, 0, 0);
        if (selected == 5)
            dst.SetEquirectangular2(0, 0, 0, 0, 0);
        dst.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        f.reset(OGRCreateCoordinateTransformation(&geo, &dst));
        b.reset(OGRCreateCoordinateTransformation(&dst, &geo));
        if (!f || !b)
            return false;
        double x = lon, y = lat;
        if (!f->Transform(1, &x, &y) || !std::isfinite(x) || !std::isfinite(y))
            return false;
    }
    mgr->SetDisplayTransform(nullptr, nullptr, nullptr);
    forward = std::move(f);
    inverse = std::move(b);
    kind = selected;
    mgr->m_bFlat = selected == 0 && fabs(angle) < 1e-9;
    if (!mgr->m_bFlat)
        mgr->SetDisplayTransform(Forward, Inverse, this);
    double sx, sy;
    mgr->WorldXY2ScreenXY(wx, wy, &sx, &sy, 0, 0, mgr->m_fScale);
    mgr->m_fCenterX = cx - sx;
    mgr->m_fCenterY = cy - sy;
    return true;
}

CDisplayProjection &DisplayProjection()
{
    static CDisplayProjection projection;
    return projection;
}
