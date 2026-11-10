#pragma once
#include "BaseMapOptions.h"
#include "GeoDC/CoordMgr.h"
#include "TerrainLayer.h"
#include "TerrainTiles.h"
#include <atlimage.h>
#include <vector>
#pragma pack(push, 8)
class CTerrainTileLayer
{
private:
    int index;
    bool server;
    HMODULE dll = nullptr;
    TerrainTilesHandle handle = nullptr;
    bool attempted = false;
    decltype(&TerrainTilesCreate) create = nullptr;
    decltype(&TerrainTilesDestroy) destroy = nullptr;
    decltype(&TerrainTilesGetStatus) status = nullptr;
    decltype(&TerrainTilesRequestView) requestView = nullptr;
    decltype(&TerrainTilesCopyFrame) copyFrame = nullptr;
    decltype(&TerrainTilesRequestSample) requestSample = nullptr;
    decltype(&TerrainTilesGetSample) getSample = nullptr;
    std::string lastError;
    decltype(&TerrainTilesGetError) getError = nullptr;
    uint64_t frameTicket = 0, imageTicket = 0, sampleTicket = 0, lastFrame = 0, lastSample = 0;
    CImage image;
    std::vector<unsigned char> pixels;
    static void Trace(const char *text);

    void Ensure();

public:
    CTerrainTileLayer(int item, bool remote);
    ~CTerrainTileLayer();

    bool Poll();

    CString Alt(double lon, double lat);

    void Draw(CDC *dc, CCoordMgr *mgr, CRect rect);
};
#pragma pack(pop)
CTerrainTileLayer &TerrainLocalLayer();

CTerrainTileLayer &TerrainServerLayer();

// Display checkboxes only control drawing. Altitude lookup has its own source.
CString TerrainAltitude(double lon, double lat);
