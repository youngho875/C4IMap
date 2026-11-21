#include "pch.h"
#include "MapNavigation.h"
#include <algorithm>
#include <cmath>

bool CMapNavigationHistory::Equal(const MapNavigationView& a,const MapNavigationView& b)
{
    return fabs(remainder(a.longitude-b.longitude,360.))<1e-8&&fabs(a.latitude-b.latitude)<1e-8&&
        fabs(a.denominator-b.denominator)<(std::max)(0.1,a.denominator*1e-7)&&
        fabs(a.angle-b.angle)<1e-7&&a.projection==b.projection&&a.tmEpsg==b.tmEpsg;
}
void CMapNavigationHistory::Record(const MapNavigationView& view)
{
    if(!std::isfinite(view.longitude)||!std::isfinite(view.latitude)||!std::isfinite(view.denominator)||view.denominator<=0)return;
    if(!m_hasInitial){m_initial=view;m_hasInitial=true;}
    if(!m_views.empty()&&Equal(m_views[m_index],view))return;
    if(!m_views.empty())m_views.resize(m_index+1);
    m_views.push_back(view);
    if(m_views.size()>100)m_views.erase(m_views.begin());
    m_index=m_views.size()-1;
}
bool CMapNavigationHistory::CanPrevious() const{return !m_views.empty()&&m_index>0;}
bool CMapNavigationHistory::CanNext() const{return !m_views.empty()&&m_index+1<m_views.size();}
bool CMapNavigationHistory::Previous(MapNavigationView& view){if(!CanPrevious())return false;view=m_views[--m_index];return true;}
bool CMapNavigationHistory::Next(MapNavigationView& view){if(!CanNext())return false;view=m_views[++m_index];return true;}
bool CMapNavigationHistory::Initial(MapNavigationView& view) const{if(!m_hasInitial)return false;view=m_initial;return true;}
bool CMapNavigationHistory::ParseNumber(CString text,double& value,bool scale)
{
    text.Trim();text.Replace(_T(","),_T(""));
    if(scale&&text.Left(2)==_T("1:"))text=text.Mid(2);
    text.Trim();if(text.IsEmpty())return false;
    TCHAR* end=nullptr;value=_tcstod(text,&end);
    return end&&*end==0&&std::isfinite(value)&&value>0;
}
CMapCoordinateDialog::CMapCoordinateDialog(double longitude,double latitude,CWnd* parent)
    : CDialogEx((UINT)0,parent),m_longitude(longitude),m_latitude(latitude) {}
INT_PTR CMapCoordinateDialog::Show()
{
    WORD storage[32]={};auto dialog=reinterpret_cast<DLGTEMPLATE*>(storage);
    dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;dialog->cx=230;dialog->cy=90;
    if(!InitModalIndirect(dialog))return IDCANCEL;return DoModal();
}
BOOL CMapCoordinateDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();SetWindowText(_T("좌표 이동 - WGS84 십진도"));
    CRect r;GetClientRect(&r);int width=r.Width();
    m_lonLabel.Create(_T("경도 (-180 ~ 180)"),WS_CHILD|WS_VISIBLE,CRect(12,14,165,36),this);
    m_latLabel.Create(_T("위도 (-90 ~ 90)"),WS_CHILD|WS_VISIBLE,CRect(12,48,165,70),this);
    m_lonEdit.Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,CRect(170,12,width-12,37),this,7101);
    m_latEdit.Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,CRect(170,46,width-12,71),this,7102);
    m_ok.Create(_T("이동"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,CRect(width-180,85,width-100,113),this,IDOK);
    m_cancel.Create(_T("취소"),WS_CHILD|WS_VISIBLE|WS_TABSTOP,CRect(width-92,85,width-12,113),this,IDCANCEL);
    CString text;text.Format(_T("%.8f"),m_longitude);m_lonEdit.SetWindowText(text);text.Format(_T("%.8f"),m_latitude);m_latEdit.SetWindowText(text);
    return TRUE;
}
void CMapCoordinateDialog::OnOK()
{
    auto parse=[](CEdit& edit,double& number){CString text;edit.GetWindowText(text);text.Trim();TCHAR* end=nullptr;number=_tcstod(text,&end);return !text.IsEmpty()&&end&&*end==0&&std::isfinite(number);};
    double lon,lat;
    if(!parse(m_lonEdit,lon)||!parse(m_latEdit,lat)||lon<-180||lon>180||lat<-90||lat>90)
    {AfxMessageBox(_T("경도 -180~180, 위도 -90~90 범위의 십진도를 입력하세요."));return;}
    m_longitude=lon;m_latitude=lat;CDialogEx::OnOK();
}
double CMapCoordinateDialog::Longitude() const{return m_longitude;}
double CMapCoordinateDialog::Latitude() const{return m_latitude;}

void CMapNavigationHistory::SynchronizeCurrent(const MapNavigationView& view)
{
    if(!m_views.empty())m_views[m_index]=view;
}
