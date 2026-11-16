#pragma once
#include <afxwin.h>
#include <vector>
class CCoordMgr;
class CDisplayProjection;

#pragma pack(push, 8)
struct CMapGridSettings
{
  public:
    bool visible = false;
    double metres = 10000;
    double degrees = 0.1;
    COLORREF color = RGB(128, 160, 192);
    int tmEpsg = 5186;
    int projection = -1; // -1 follows the map; 0..6 match the projection menu.
    int gridTmEpsg = 5186;
    double originLatitude = 38;
    double originLongitude = 127;

    bool Valid() const;
    bool ConfigureProjection(CDisplayProjection &target) const;
    void Load();
    bool Save() const;
};
#pragma pack(pop)

bool IsSupportedTmEpsg(int epsg);
CMapGridSettings &MapGridSettings();
std::vector<std::vector<CPoint>> BuildMapGrid(CCoordMgr *mgr, const CRect &viewport,
                                              const CMapGridSettings &settings);
void DrawMapGrid(CDC *dc, CCoordMgr *mgr, const CRect &viewport);
