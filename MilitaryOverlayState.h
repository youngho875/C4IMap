#pragma once
#include "MilitaryOverlay.h"
#pragma pack(push, 8)
#include "CTest_OverItem.h"
#include "ICOPS_Framework/IMapProjection.h"
#include <memory>
#include <vector>
#include <set>
#include <map>

#pragma pack(8)

struct MilitaryPoint
{
    double lon = 0;
    double lat = 0;
};

// The SDK adapter owns symbol properties, drawing, editing and serialization.
class MilitaryItem : public CTest_OverItem
{
public:
    std::vector<MilitaryPoint> points;
};

struct MilitaryHitBox
{
    int index = -1;
    Gdiplus::RectF bounds;
};

struct MilitaryHistoryState
{
    std::vector<BYTE> data;
    std::set<int> selection;
    int primary = -1;
};

struct CMilitaryOverlay::Impl
{
    std::vector<std::unique_ptr<MilitaryItem>> items;
    std::unique_ptr<MilitaryItem> adding;
    ICOPS::IMapProjectionPtr pixels;
    CString lastCode;
    std::vector<MilitaryHistoryState> undo;
    std::vector<MilitaryHistoryState> redo;
    MilitaryHistoryState pending;
    bool pendingHistory = false;
    bool Capture(MilitaryHistoryState& state);
    bool Restore(const MilitaryHistoryState& state);
    bool BeginChange();
    void CommitChange();
    void ResetHistory();
    std::set<int> selection;
    std::set<int> selectionBefore;
    std::map<int, std::vector<MilitaryPoint>> original;
    std::vector<MilitaryHitBox> boxes;
    int selected = -1;
    int handle = -1;
    int nextGroup = 1;
    bool selectionMode = false;
    bool dragging = false;
    bool boxing = false;
    bool consumed = false;
    bool modifiedBefore = false;
    mutable bool modified = false;
    CPoint boxStart;
    CPoint boxEnd;
    MilitaryPoint last;

    bool Visible(int layer, const CLocalOverlay& overlay) const;
    bool Prepare(MilitaryItem& item, CCoordMgr* coord, std::vector<Gdiplus::PointF>& points);
    bool Ready();
    void SelectUnit(int index, bool select);
    void UpdatePrimary();
};
#pragma pack(pop)
