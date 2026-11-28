#pragma once
#include "ModelessDialog.h"
#include "MapAnalysis.h"
#pragma pack(push, 8)
class CAnalysisLocationDialog : public CModelessDialog
{
public:
    explicit CAnalysisLocationDialog(CWnd* parent);
    bool Show(std::function<void(CAnalysisLocationDialog&)> accepted);
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
