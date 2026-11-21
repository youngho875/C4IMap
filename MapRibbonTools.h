#pragma once
#pragma pack(push, 8)
#include <afxwin.h>
#include <atlimage.h>
#include <memory>
#include <vector>

class CCoordMgr;
constexpr UINT WM_MAP_SEARCH_MOVE = WM_APP + 183;

struct MapSearchPoint { double x = 0, y = 0; };
struct MapSearchFeature
{
    CString layer, path, identity;
    long long fid = -1;
    int kind = 0;
    std::vector<CString> fields, values;
    std::vector<CPoint> screen;
    std::vector<MapSearchPoint> world;
    std::vector<int> parts;
};
struct MapSearchQuery
{
    CString layer, field, text;
    bool spatial = false, ellipse = false;
    CRect area;
};
struct MapSearchReply
{
    std::vector<MapSearchFeature> features;
    CString message;
};

class CMapSearchJob
{
public:
    CMapSearchJob();
    ~CMapSearchJob();
    void Start(std::vector<MapSearchFeature> features, const MapSearchQuery& query);
    void Cancel();
    bool Poll(MapSearchReply& reply);
    bool Busy() const;
    static bool Intersects(const MapSearchFeature& feature, const MapSearchQuery& query);
private:
    struct State;
    std::shared_ptr<State> m_state;
};

class CMapImageAdjustment
{
public:
    static void MakeTable(int brightness, int contrast, BYTE table[256]);
    bool Apply(CDC* dc, int width, int height, int brightness, int contrast);
    static bool Capture(CDC* dc, int width, int height, CImage& image);
    static bool CopyToClipboard(HWND owner, CImage& image);
private:
    CImage m_image;
};

CString MapSearchUtf8(const char* text);
void MapSearchDraw(CDC* dc, CCoordMgr* coord,
    const std::vector<MapSearchFeature>& features, COLORREF color);
#pragma pack(pop)
