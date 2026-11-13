#pragma once
#include <memory>
#include <vector>
#include <string>
class CCoordMgr;
#pragma pack(push, 8)
struct AnalysisPoint
{
    double lon = 0;
    double lat = 0;
};
struct AnalysisProfileSample
{
    double distance = 0;
    double height = 0;
};
struct AnalysisSettings
{
    double range = 1000;
    double observerHeight = 20;
    double interval = 200;
    int width = 1;
    COLORREF color = RGB(255, 0, 0);
    std::string source;
    int sourceKind = 0; // 0 = DEM / Terrain-RGB, 1 = local terrain, 2 = server
};
class CMapAnalysis
{
public:
    enum Mode { None, Distance, Area, Profile, Viewshed, Bearing };
    CMapAnalysis();
    ~CMapAnalysis();
    void Start(Mode mode, const AnalysisSettings& settings);
    void Clear();
    void Cancel();
    bool Active() const;
    bool Busy() const;
    bool Click(CPoint point, CCoordMgr* coord);
    bool Move(CPoint point, CCoordMgr* coord);
    bool Finish();
    bool Place(AnalysisPoint point);
    bool Poll();
    bool TakeProfile(std::vector<AnalysisProfileSample>& samples);
    bool TakeError(CString& error);
    void Draw(CDC* dc, CCoordMgr* coord, CRect rect);
    static double Length(AnalysisPoint a, AnalysisPoint b, double* bearing = nullptr);
    static double PolygonArea(const std::vector<AnalysisPoint>& points);
    static AnalysisPoint Destination(AnalysisPoint start, double bearing, double metres);
private:
    struct Impl;
    std::unique_ptr<Impl> p;
    CMapAnalysis(const CMapAnalysis&) = delete;
    CMapAnalysis& operator=(const CMapAnalysis&) = delete;
};
#pragma pack(pop)
