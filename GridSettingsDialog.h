#pragma once
#include <afxdialogex.h>
#include "MapGrid.h"

class CGridSettingsDialog : public CDialogEx
{
  public:
    CGridSettingsDialog(const CMapGridSettings &settings, CWnd *parent);
    INT_PTR Show();
    CMapGridSettings Settings() const;

  protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnColor();
    afx_msg void OnProjectionChanged();
    DECLARE_MESSAGE_MAP()

  private:
    struct EmptyTemplate
    {
        DLGTEMPLATE dialog;
        WORD menu = 0, windowClass = 0, title = 0;
    };
    CMapGridSettings value;
    CButton enabled, color, ok, cancel;
    CEdit metres, degrees, latitude, longitude;
    CComboBox tm, gridTm, projection;
    CStatic labels[9];
    void UpdateColor();
};
