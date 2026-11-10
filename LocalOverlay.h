#pragma once
#include <vector>
class CCoordMgr;
// Keep SDK/STL layout independent of legacy application packing.
#pragma pack(push, 8)
class CLocalOverlay 
{
public:
    enum Tool 
    {
        Pan=0, 
        Line=1, 
        Arrow=2, 
        Rectangle=3, 
        Ellipse=4 
    };

    struct LayerInfo 
    {
        int id; 
        CString name; 
        size_t count; 
        bool visible; 
    };

    CLocalOverlay();
    ~CLocalOverlay();

    int AddLayer(const CString& name=CString());
    bool SelectLayer(int id);
    int ActiveLayer() const;
    bool SetLayerVisible(int id,bool visible);
    bool RenameLayer(int id,const CString& name);
    std::vector<LayerInfo> Layers() const;
    size_t ActiveCount() const;

    void SetTool(int tool);
    int GetTool() const;
    bool Begin(CPoint point, CCoordMgr* coord);
    bool Move(CPoint point, CCoordMgr* coord);
    bool End(CPoint point, CCoordMgr* coord);
    void Cancel();
    bool IsDrawing() const;
    void Draw(CDC* dc, CCoordMgr* coord);
    void Undo();
    void Redo();
    bool CanRedo() const;
    bool IsModified() const;
    void Clear();
    size_t Count() const;
    COLORREF Color() const;
    void SetColor(COLORREF color);
    bool Save(LPCTSTR path, CString& error) const;
    bool Load(LPCTSTR path, CString& error);

private:
    struct Impl;
    Impl* p;
    CLocalOverlay(const CLocalOverlay&) = delete;
    CLocalOverlay& operator=(const CLocalOverlay&) = delete;
};
#pragma pack(pop)
