#pragma once
#include "MapAnalysis.h"
#include <array>
#pragma pack(push, 8)
class CAnalysisWorkspace
{
public:
    CAnalysisWorkspace();
    ~CAnalysisWorkspace();
    bool Start(CMapAnalysis::Mode mode, const AnalysisSettings& settings);
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
private:
    std::array<std::unique_ptr<CMapAnalysis>, 6> tools;
    CMapAnalysis::Mode current = CMapAnalysis::None;
    CMapAnalysis* Current() const;
};
#pragma pack(pop)
