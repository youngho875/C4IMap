#pragma once
#include "MapAnalysis.h"
#pragma pack(push, 8)
class CAnalysisLocationDialog : public CDialog
{
public:
    explicit CAnalysisLocationDialog(CWnd* parent);
    INT_PTR Show();
    AnalysisPoint Location() const;
protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
private:
    CEdit longitude;
    CEdit latitude;
    CStatic longitudeLabel;
    CStatic latitudeLabel;
    CButton ok;
    CButton cancel;
    AnalysisPoint location;
};
#pragma pack(pop)
