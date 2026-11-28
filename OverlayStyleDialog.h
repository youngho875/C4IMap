#pragma once
#include "ModelessDialog.h"
#include <afxdialogex.h>
#include "LocalOverlay.h"
class COverlayStyleDialog : public CModelessDialog
{
public:
    COverlayStyleDialog(const CLocalOverlay::Style& style, bool selected, CWnd* parent);
    bool Show(std::function<void(COverlayStyleDialog&)> accepted);
    CLocalOverlay::Style Style() const;
protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnColor(UINT id);
    DECLARE_MESSAGE_MAP()
private:
    CLocalOverlay::Style value;
    bool selectedObject;
    CButton lineCheck, fillCheck, lineColor, fillColor, patternColor, ok, cancel;
    CEdit width, lineOpacity, fillOpacity;
    CComboBox dash, fillType;
    CStatic labels[6];
    void UpdateColors();
};
