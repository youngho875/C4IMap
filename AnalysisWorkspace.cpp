#include "pch.h"
#pragma pack(push, 8)
#include "AnalysisWorkspace.h"
CAnalysisWorkspace::CAnalysisWorkspace()
{
    for (size_t i = 1; i < tools.size(); ++i)
        tools[i] = std::make_unique<CMapAnalysis>();
}
CAnalysisWorkspace::~CAnalysisWorkspace() = default;
CMapAnalysis* CAnalysisWorkspace::Current() const
{
    return current == CMapAnalysis::None ? nullptr : tools[current].get();
}
bool CAnalysisWorkspace::Start(CMapAnalysis::Mode mode, const AnalysisSettings& settings)
{
    if (mode <= CMapAnalysis::None || mode > CMapAnalysis::Bearing || tools[mode]->Busy())
        return false;
    // Leave completed results and background workers intact. Only the unfinished
    // input path is discarded when switching to another tool.
    if (current != mode && Current() && Current()->Active())
        Current()->Clear();
    current = mode;
    tools[mode]->Start(mode, settings);
    return true;
}
void CAnalysisWorkspace::Clear()
{
    for (auto& tool : tools)
        if (tool)
            tool->Clear();
    current = CMapAnalysis::None;
}
void CAnalysisWorkspace::Cancel()
{
    if (Current())
        Current()->Cancel();
}
bool CAnalysisWorkspace::Active() const
{
    return Current() && Current()->Active();
}
bool CAnalysisWorkspace::Busy() const
{
    return Current() && Current()->Busy();
}
bool CAnalysisWorkspace::Click(CPoint point, CCoordMgr* coord)
{
    return Current() && Current()->Click(point, coord);
}
bool CAnalysisWorkspace::Move(CPoint point, CCoordMgr* coord)
{
    return Current() && Current()->Move(point, coord);
}
bool CAnalysisWorkspace::Finish()
{
    return Current() && Current()->Finish();
}
bool CAnalysisWorkspace::Place(AnalysisPoint point)
{
    return Current() && Current()->Place(point);
}
bool CAnalysisWorkspace::Poll()
{
    bool changed = false;
    for (auto& tool : tools)
        if (tool)
            changed = tool->Poll() || changed;
    return changed;
}
bool CAnalysisWorkspace::TakeProfile(std::vector<AnalysisProfileSample>& samples)
{
    return tools[CMapAnalysis::Profile]->TakeProfile(samples);
}
bool CAnalysisWorkspace::TakeError(CString& error)
{
    for (auto& tool : tools)
        if (tool && tool->TakeError(error))
            return true;
    return false;
}
void CAnalysisWorkspace::Draw(CDC* dc, CCoordMgr* coord, CRect rect)
{
    const CMapAnalysis::Mode order[] = {CMapAnalysis::Viewshed, CMapAnalysis::Bearing,
        CMapAnalysis::Area, CMapAnalysis::Distance, CMapAnalysis::Profile};
    for (auto mode : order)
        tools[mode]->Draw(dc, coord, rect);
}
#pragma pack(pop)
