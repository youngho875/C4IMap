#pragma once
#include "BaseMapOptions.h"
#include "GeoDC/CoordMgr.h"
#include "TerrainElevation.h"
#include <atlimage.h>
#include <vector>
#pragma pack(push, 8)
class CTerrainLayer
{
private:
    HMODULE dll = nullptr;
    TerrainHandle handle = nullptr;
    bool attempted = false;
    decltype(&TerrainCreate) create = nullptr;
    decltype(&TerrainDestroy) destroy = nullptr;
    decltype(&TerrainGetStatus) status = nullptr;
    decltype(&TerrainRequestView) requestView = nullptr;
    decltype(&TerrainCopyFrame) copyFrame = nullptr;
    decltype(&TerrainRequestSample) requestSample = nullptr;
    decltype(&TerrainGetSample) getSample = nullptr;
    uint64_t frameTicket = 0, imageTicket = 0, sampleTicket = 0, lastFrame = 0, lastSample = 0;
    CImage image;
    std::vector<unsigned char> pixels;
    static void Trace(const char *text);
    void Ensure();

public:
    ~CTerrainLayer();
    bool Poll();

    CString Alt(double lon, double lat);
    void Draw(CDC *dc, CCoordMgr *mgr, CRect rect);
};
#pragma pack(pop)
CTerrainLayer &TerrainLayer();
