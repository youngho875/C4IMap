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
        Pan = 0,
        Line = 1,
        Arrow = 2,
        Rectangle = 3,
        Ellipse = 4,
        RoundRect = 5,
        Triangle = 6,
        RightTriangle = 7,
        Parallelogram = 8,
        Trapezoid = 9,
        Diamond = 10,
        Pentagon = 11,
        Hexagon = 12,
        Arc = 13,
        Pie = 14,
        Chord = 15,
        Elbow = 16,
        Curve = 17,
        TextBox = 18,
        Polyline = 19,
        Polygon = 20,
        Spline = 21,
        ClosedSpline = 22
    };

    struct Geometry
    {
        int count = 0;
        double x[512] = {};
        double y[512] = {};
    };

    struct LayerInfo
    {
        int id;
        CString name;
        size_t count;
        bool visible;
    };

    struct Style
    {
      public:
        COLORREF lineColor = RGB(255, 60, 40);
        COLORREF fillColor = RGB(255, 60, 40);
        COLORREF patternColor = RGB(255, 255, 255);
        double width = 3.0;
        int lineAlpha = 255;
        int fillAlpha = 255;
        int dash = 0;
        int fillType = 1;
        int hatch = 0;
        int rotation = 0;
        int flipX = 0;
        int flipY = 0;
        int startArrow = 0;
        int endArrow = 0;
        int lineCap = 0;
        int lineJoin = 0;
        int lineAlignment = 0;
        int lineStripe = 0;
        int gradientMode = 0;
        int gradientPosition = 50;
        int gradientColor = 16777215;
        int textColor = 0;
        int fontSize = 10;
        int fontStyle = 0;
        int textAlign = 0;
        int textAlignY = 0;
        int lineInterval = 100;
        int vertical = 0;
        int rightToLeft = 0;
        int autoTextBox = 0;
        wchar_t text[1024] = {};
        wchar_t font[64] = L"Gulim";
        wchar_t imagePath[1024] = {};
        bool lineEnabled = true;
        bool fillEnabled = false;
    };

    void SelectAll();
    bool GroupSelection(bool ungroup);
    bool Arrange(int operation, CCoordMgr *coord);
    Style GetStyle() const;
    bool SetStyle(const Style &style);
    static bool ValidStyle(const Style &style);

    CLocalOverlay();
    ~CLocalOverlay();

    int AddLayer(const CString &name = CString());
    bool SelectLayer(int id);
    int ActiveLayer() const;
    bool SetLayerVisible(int id, bool visible);
    bool RenameLayer(int id, const CString &name);
    std::vector<LayerInfo> Layers() const;
    size_t ActiveCount() const;

    void SetTool(int tool);
    int GetTool() const;
    bool Begin(CPoint point, CCoordMgr *coord);
    bool Move(CPoint point, CCoordMgr *coord);
    bool End(CPoint point, CCoordMgr *coord);
    void Cancel();
    bool IsDrawing() const;
    void Draw(CDC *dc, CCoordMgr *coord);
    bool HasSelection() const;
    bool DeleteSelected();
    bool CopySelected(HWND owner) const;
    bool CutSelected(HWND owner);
    bool Paste(HWND owner, CCoordMgr *coord);
    static bool CanPaste();
    bool CanUndo() const;
    void Undo();
    void Redo();
    bool CanRedo() const;
    bool IsModified() const;
    void Clear();
    size_t Count() const;
    COLORREF Color() const;
    void SetColor(COLORREF color);
    bool Save(LPCTSTR path, CString &error) const;
    bool Load(LPCTSTR path, CString &error);

  private:
    struct Impl;
    Impl *p;
    CLocalOverlay(const CLocalOverlay &) = delete;
    CLocalOverlay &operator=(const CLocalOverlay &) = delete;
};
#pragma pack(pop)
