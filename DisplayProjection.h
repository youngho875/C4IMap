#pragma once
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"
#include "ogr_spatialref.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#pragma pack(push, 8)
class CDisplayProjection
{
  private:
    struct DeleteTransform
    {
      public:
        void operator()(OGRCoordinateTransformation *p) const;
    };
    std::unique_ptr<OGRCoordinateTransformation, DeleteTransform> forward, inverse;
    CCoord coord;
    int kind = 0;
    int tmEpsg = 5186;
    double centralMeridian = 0, angle = 0;
    void RotateXY(double *x, double *y, double degrees);
    static bool Forward(void *context, double *x, double *y);
    static bool Inverse(void *context, double *x, double *y);

  public:
    int Kind() const;
    double Angle() const;
    bool SetAngle(CCoordMgr *mgr, double degrees);
    bool Set(CCoordMgr *mgr, int selected, int requestedTmEpsg = 0);
    int TmEpsg() const;
    bool ConfigureCoordinates(int selected, double latitude, double longitude, int requestedTmEpsg);
    bool WorldToGrid(double wx, double wy, double &x, double &y);
    bool GridToWorld(double x, double y, double &wx, double &wy);
};
CDisplayProjection &DisplayProjection();
#pragma pack(pop)
