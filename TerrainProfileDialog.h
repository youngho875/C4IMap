#pragma once
#include "MapAnalysis.h"
#pragma pack(push, 8)
class CTerrainProfileDialog : public CDialog
{
  public:
    CTerrainProfileDialog(const std::vector<AnalysisProfileSample> &samples, CWnd *parent);
    INT_PTR Show();
    static bool Open(const std::vector<AnalysisProfileSample>& samples, CWnd* parent);
    static void DrawChart(CDC *dc, CRect rect, const std::vector<AnalysisProfileSample> &samples,
                          int selected);

  protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    void OnCancel() override;
    void PostNcDestroy() override;
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    DECLARE_MESSAGE_MAP()
  private:
    std::vector<AnalysisProfileSample> data;
    int selected = -1;
    bool modeless = false;
    CButton closeButton;
};
#pragma pack(pop)
