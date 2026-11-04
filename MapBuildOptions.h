#pragma once
// The only user-facing map build option:
// 1 = original local SHP/raster/CIB files
// 2 = local vector/raster/CIB tiles
// 3 = network WMS/WFS/WMTS and terrain server
#ifndef C4MAP_BUILD_MODE
#define C4MAP_BUILD_MODE 2
#endif
#if C4MAP_BUILD_MODE < 1 || C4MAP_BUILD_MODE > 3
#error C4MAP_BUILD_MODE must be 1 (original), 2 (tiles), or 3 (network).
#endif

// Derived implementation switches. Legacy project definitions cannot override
// the selected mode. Configure C4MAP_BUILD_MODE only.
#ifdef C4MAP_ENABLE_LOCAL
#undef C4MAP_ENABLE_LOCAL
#endif
#ifdef C4MAP_ENABLE_OGC
#undef C4MAP_ENABLE_OGC
#endif
#ifdef C4MAP_ENABLE_TERRAIN_LOCAL
#undef C4MAP_ENABLE_TERRAIN_LOCAL
#endif
#ifdef C4MAP_ENABLE_TERRAIN_SERVER
#undef C4MAP_ENABLE_TERRAIN_SERVER
#endif
#ifdef C4MAP_RASTER_MODE
#undef C4MAP_RASTER_MODE
#endif
#ifdef C4MAP_CIB_MODE
#undef C4MAP_CIB_MODE
#endif
#ifdef C4MAP_SHAPE_MODE
#undef C4MAP_SHAPE_MODE
#endif
#ifdef C4MAP_TILE_TREE_ONLY
#undef C4MAP_TILE_TREE_ONLY
#endif
#ifdef C4MAP_ENABLE_BASEMAP
#undef C4MAP_ENABLE_BASEMAP
#endif

#define C4MAP_ENABLE_BASEMAP 1
#define C4MAP_ENABLE_LOCAL (C4MAP_BUILD_MODE != 3)
#define C4MAP_ENABLE_OGC (C4MAP_BUILD_MODE == 3)
#define C4MAP_ENABLE_TERRAIN_LOCAL C4MAP_ENABLE_LOCAL
#define C4MAP_ENABLE_TERRAIN_SERVER C4MAP_ENABLE_OGC
#define C4MAP_TILE_TREE_ONLY (C4MAP_BUILD_MODE == 2)
#if C4MAP_BUILD_MODE == 1
#define C4MAP_SHAPE_MODE 1
#define C4MAP_RASTER_MODE 1
#define C4MAP_CIB_MODE 1
#else
#define C4MAP_SHAPE_MODE 2
#define C4MAP_RASTER_MODE 2
#define C4MAP_CIB_MODE 2
#endif
