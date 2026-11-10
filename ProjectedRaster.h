#pragma once
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"
#include <algorithm>
#include <atlimage.h>
#include <cmath>
#include <functional>
#include <memory>
// Reproject existing geographic raster backends through the same inverse transform
// used by cursor/feature queries. Vector features continue to project individually.
void DrawProjectedRaster(CDC *dc, CCoordMgr *mgr, CRect rect,
                         const std::function<void(CDC *, CCoordMgr *, CRect)> &draw);
