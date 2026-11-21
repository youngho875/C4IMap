#pragma once
#include <afxwin.h>
#include <afxdialogex.h>
#pragma pack(push,8)
#include <vector>
struct MapNavigationView
{
    double longitude=0,latitude=0,denominator=0,angle=0;
    int projection=0,tmEpsg=5186;
};
class CMapNavigationHistory
{
public:
    void Record(const MapNavigationView& view);
    void SynchronizeCurrent(const MapNavigationView& view);
    bool Previous(MapNavigationView& view);
    bool Next(MapNavigationView& view);
    bool Initial(MapNavigationView& view) const;
    bool CanPrevious() const;
    bool CanNext() const;
    static bool ParseNumber(CString text,double& value,bool scale=false);
    static bool Equal(const MapNavigationView& a,const MapNavigationView& b);
private:
    std::vector<MapNavigationView> m_views;
    size_t m_index=0;
    MapNavigationView m_initial;
    bool m_hasInitial=false;
};
class CMapCoordinateDialog : public CDialogEx
{
public:
    CMapCoordinateDialog(double longitude,double latitude,CWnd* parent);
    INT_PTR Show();
    double Longitude() const;
    double Latitude() const;
protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
private:
    double m_longitude,m_latitude;
    CEdit m_lonEdit,m_latEdit;
    CStatic m_lonLabel,m_latLabel;
    CButton m_ok,m_cancel;
};
#pragma pack(pop)
