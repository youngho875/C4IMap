#pragma once
#include <string>
#include <afxwin.h>
class CCoordMgr;
class CLocalOverlay;
#pragma pack(push, 8)
class CMilitaryOverlay
{
public:
    CMilitaryOverlay();
    ~CMilitaryOverlay();
    bool Choose(CWnd* owner, int layer);
    bool Edit(CWnd* owner);
    void Draw(CDC* dc, CCoordMgr* coord, const CLocalOverlay& layers);
    bool Down(CPoint point, CCoordMgr* coord, const CLocalOverlay& layers, UINT flags = 0);
    bool Move(CPoint point, CCoordMgr* coord);
    bool Up();
    bool Finish();
    void Cancel();
    bool Selected() const;
    size_t SelectionCount() const;
    void SelectMode();
    bool SelectionMode() const;
    void CancelGesture();
    bool GroupSelection(bool ungroup);
    bool CanGroup() const;
    bool CanUngroup() const;
    void SelectAll(const CLocalOverlay& layers);
    bool Undo();
    bool Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    bool Modified() const;
    void Clear();
    bool Delete();
    size_t Count(int layer) const;
    std::string SyncObjects(CCoordMgr* coord) const;
    bool Save(const CString& path, CString& error) const;
    bool Load(const CString& path, CString& error);
private:
    friend class MilitaryOverlayTests;
    struct Impl;
    Impl* p;
    CMilitaryOverlay(const CMilitaryOverlay&) = delete;
    CMilitaryOverlay& operator=(const CMilitaryOverlay&) = delete;
};
#pragma pack(pop)
