#include "pch.h"
#include "MilitaryOverlay.h"
#include "C4IMap.h"
#include "LocalOverlay.h"
#include "MapDecoration.h"
#include "GeoDC/CoordMgr.h"
#pragma pack(push, 8)
#include "ICOPS_WarSym/IWarSymbol.h"
#include "ICOPS_WarSym/IWarSymPropertyEditor.h"
#include "ICOPS_Framework/IMapProjection.h"
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

struct MilitaryPoint { double lon=0, lat=0; };
struct MilitaryItem
{
    ICOPS::IWarSymbolPtr symbol;
    std::vector<MilitaryPoint> points;
    int layer=0;
};
struct CMilitaryOverlay::Impl
{
    std::vector<std::unique_ptr<MilitaryItem>> items;
    std::unique_ptr<MilitaryItem> adding;
    ICOPS::IMapProjectionPtr pixels;
    CString lastCode;
    int selected=-1, handle=-1;
    bool dragging=false, consumed=false;
    mutable bool modified=false;
    MilitaryPoint last;
    std::vector<MilitaryPoint> original;
    bool Visible(int layer, const CLocalOverlay& overlay) const;
    bool Prepare(MilitaryItem& item, CCoordMgr* coord, std::vector<Gdiplus::PointF>& pts);
    bool Ready();
};
bool CMilitaryOverlay::Impl::Ready()
{
    auto app=static_cast<CC4IMapApp*>(AfxGetApp());
    if(!app || !app->m_pAppCxt || !app->m_piSymFac) return false;
    if(!pixels)
    {
        if(!app->m_pAppCxt->CreateObject(_T("ICOPS_Framework"),_T(".dll"),
            _T("CMapProjectionNone"),_T("IMapProjection"),(void**)&pixels)) return false;
        ICOPS::SGeoCoord2 center={}; ICOPS::SOrthoCoord2 origin={};
        pixels->SetCenter(&center); pixels->SetViewOrigin(&origin);
        pixels->SetMapScale(1); pixels->SetZoomFactor(1);
    }
    return true;
}
bool CMilitaryOverlay::Impl::Visible(int layer,const CLocalOverlay& overlay) const
{
    for(const auto& l:overlay.Layers()) if(l.id==layer) return l.visible;
    return false;
}
bool CMilitaryOverlay::Impl::Prepare(MilitaryItem& item,CCoordMgr* coord,std::vector<Gdiplus::PointF>& pts)
{
    if(!coord || !Ready() || item.points.empty()) return false;
    pts.clear();
    for(const auto& gp:item.points)
    {
        double x,y;
        coord->WorldLonLat2ScreenXY(gp.lon,gp.lat,&x,&y,coord->m_fCenterX,coord->m_fCenterY,coord->m_fScale);
        if(!std::isfinite(x)||!std::isfinite(y)) return false;
        pts.emplace_back((float)x,(float)y);
    }
    std::vector<ICOPS::SGeoCoord2> coords(pts.size());
    pixels->Inverse(pts.data(),coords.data(),(INT_PTR)pts.size());
    item.symbol->SetCoordinates(pixels,coords.data(),(INT_PTR)coords.size());
    return true;
}
CMilitaryOverlay::CMilitaryOverlay():p(new Impl){}
CMilitaryOverlay::~CMilitaryOverlay(){delete p;}
bool CMilitaryOverlay::Choose(CWnd* owner,int layer)
{
    Cancel();
    if(!p->Ready()){AfxMessageBox(_T("군대부호 SDK가 초기화되지 않았습니다."));return false;}
    auto app=static_cast<CC4IMapApp*>(AfxGetApp());
    ICOPS::IWarSymPropertyEditorPtr editor;
    if(!app->m_piSymFac->CreatePropertyEditor(&editor) || !editor)
    {AfxMessageBox(_T("군대부호 선택창을 생성할 수 없습니다. ICOPS UI DLL과 설정을 확인하세요."));return false;}
    editor->SetWarSymFactory(app->m_piSymFac);
    auto item=std::make_unique<MilitaryItem>();
    if(!editor->OpenSymbolSelector(p->lastCode,owner,&item->symbol) || !item->symbol) return false;
    p->lastCode=item->symbol->GetSymbolCode();
    item->layer=layer; p->selected=-1; p->adding=std::move(item);
    return true;
}
bool CMilitaryOverlay::Edit(CWnd* owner)
{
    if(!Selected() || !p->Ready()) return false;
    ICOPS::IWarSymPropertyEditorPtr editor;
    auto app=static_cast<CC4IMapApp*>(AfxGetApp());
    if(!app->m_piSymFac->CreatePropertyEditor(&editor)||!editor) return false;
    editor->SetWarSymFactory(app->m_piSymFac);
    editor->SetTargetSymbol(p->items[p->selected]->symbol);
    const bool ok=!!editor->OpenEditor(_T("군대부호 속성 편집"),owner);
    p->modified=true;
    p->items[p->selected]->symbol->InvalidateSymbolImage();
    return ok;
}
void CMilitaryOverlay::Draw(CDC* dc,CCoordMgr* coord,const CLocalOverlay& layers)
{
    if(!dc || !coord || (p->items.empty()&&!p->adding)) return;
    if(Selected() && !p->Visible(p->items[p->selected]->layer,layers)) Cancel();
    Gdiplus::Graphics g(dc->GetSafeHdc());
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(255,0,120,255),1);
    Gdiplus::SolidBrush white(Gdiplus::Color::White);
    auto draw=[&](MilitaryItem& item,bool selected)
    {
        if(!p->Visible(item.layer,layers)) return;
        std::vector<Gdiplus::PointF> pts;
        if(!p->Prepare(item,coord,pts)) return;
        const INT_PTR required=item.symbol->CountAnchorPoints(0,(INT_PTR)pts.size());
        if((required>0 && pts.size()>=(size_t)required)||(required<0 && pts.size()>=(size_t)-required))
            item.symbol->Draw(&g,pts.data(),(INT_PTR)pts.size(),1.0);
        else if(pts.size()>1) g.DrawLines(&pen,pts.data(),(INT)pts.size());
        if(selected)
        {
            Gdiplus::RectF box;
            if(item.symbol->GetBoundingRect(&g,p->pixels,1.0,&box,TRUE)) g.DrawRectangle(&pen,box);
            for(auto pt:pts){g.FillRectangle(&white,pt.X-4,pt.Y-4,8.0f,8.0f);g.DrawRectangle(&pen,pt.X-4,pt.Y-4,8.0f,8.0f);}
        }
    };
    for(size_t i=0;i<p->items.size();++i) draw(*p->items[i],(int)i==p->selected);
    if(p->adding) draw(*p->adding,true);
}
bool CMilitaryOverlay::Down(CPoint pt,CCoordMgr* coord,const CLocalOverlay& layers)
{
    MilitaryPoint gp;
    if(!MapScreenLonLat(coord,pt.x,pt.y,gp.lon,gp.lat)) return false;
    p->consumed=false;
    if(p->adding)
    {
        p->consumed=true;
        if(!p->Visible(p->adding->layer,layers)){Cancel();return true;}
        p->adding->points.push_back(gp);
        auto count=p->adding->symbol->CountAnchorPoints(0,(INT_PTR)p->adding->points.size());
        if(count>0 && p->adding->points.size()>=(size_t)count) Finish();
        return true;
    }
    CClientDC dc(AfxGetMainWnd());Gdiplus::Graphics g(dc.GetSafeHdc());
    auto hit=[&](int i)->bool
    {
        auto& item=*p->items[i];
        if(!p->Visible(item.layer,layers))return false;
        std::vector<Gdiplus::PointF> pts;
        if(!p->Prepare(item,coord,pts))return false;
        p->handle=-1;
        if(i==p->selected)for(size_t k=0;k<pts.size();++k)
            if(std::hypot(pts[k].X-pt.x,pts[k].Y-pt.y)<=8){p->handle=(int)k;break;}
        Gdiplus::RectF box;
        if(p->handle<0 && (!item.symbol->GetBoundingRect(&g,p->pixels,1.0,&box,TRUE)||!box.Contains((float)pt.x,(float)pt.y)))return false;
        p->selected=i;p->last=gp;p->original=item.points;p->dragging=true;return true;
    };
    if(Selected() && hit(p->selected))return true;
    for(int i=(int)p->items.size()-1;i>=0;--i)if(hit(i))return true;
    p->selected=-1;return false;
}
bool CMilitaryOverlay::Move(CPoint pt,CCoordMgr* coord)
{
    if(!p->dragging||!Selected())return false;
    MilitaryPoint gp;
    if(!MapScreenLonLat(coord,pt.x,pt.y,gp.lon,gp.lat))return true;
    auto& points=p->items[p->selected]->points;
    if(p->handle>=0)points[p->handle]=gp;
    else
    {
        double dx=std::remainder(gp.lon-p->last.lon,360.0),dy=gp.lat-p->last.lat;
        auto moved=points;
        for(auto& v:moved){v.lon=std::remainder(v.lon+dx,360.0);v.lat+=dy;if(v.lat < -90 || v.lat >90)return true;}
        points.swap(moved);
    }
    p->modified=true;
    p->last=gp;return true;
}
bool CMilitaryOverlay::Up(){bool active=p->dragging||p->consumed;p->consumed=false;p->dragging=false;p->original.clear();return active;}
bool CMilitaryOverlay::Finish()
{
    if(!p->adding)return false;
    const auto n=p->adding->symbol->CountAnchorPoints(0,(INT_PTR)p->adding->points.size());
    if(n==0 || p->adding->points.size()<(size_t)std::abs(n))return false;
    p->modified=true;p->items.push_back(std::move(p->adding));p->selected=(int)p->items.size()-1;return true;
}
void CMilitaryOverlay::Cancel()
{
    if(p->dragging&&Selected())p->items[p->selected]->points=p->original;
    p->adding.reset();p->dragging=false;p->original.clear();p->selected=-1;
}
bool CMilitaryOverlay::Selected()const{return p->selected>=0&&p->selected<(int)p->items.size();}
bool CMilitaryOverlay::Delete(){if(!Selected())return false;p->modified=true;p->items.erase(p->items.begin()+p->selected);Cancel();return true;}
bool CMilitaryOverlay::Modified()const{return p->modified;}
void CMilitaryOverlay::Clear(){Cancel();p->items.clear();p->modified=false;}
size_t CMilitaryOverlay::Count(int layer)const{size_t n=0;for(const auto& i:p->items)if(i->layer==layer)++n;return n;}
bool CMilitaryOverlay::Save(const CString& path,CString& error)const
{
    try{
        CFile f(path+_T(".warsym"),CFile::modeCreate|CFile::modeWrite);CArchive ar(&f,CArchive::store);
        ar<<(DWORD)0x57535931<<(DWORD)p->items.size();
        for(const auto& i:p->items){ar<<i->layer<<i->symbol->GetSymbolCode()<<i->symbol->GetAssembleStream()<<(DWORD)i->points.size();for(auto v:i->points)ar<<v.lon<<v.lat;}
        ar.Close();f.Close();p->modified=false;return true;
    }catch(CException* e){e->Delete();error=_T("군대부호 저장 실패 (.warsym)");return false;}
}
bool CMilitaryOverlay::Load(const CString& path,CString& error)
{
    if(GetFileAttributes(path+_T(".warsym"))==INVALID_FILE_ATTRIBUTES){Clear();return true;}
    try{
        if(!p->Ready())AfxThrowArchiveException(CArchiveException::badSchema);
        CFile f(path+_T(".warsym"),CFile::modeRead);CArchive ar(&f,CArchive::load);
        DWORD magic,count;ar>>magic>>count;if(magic!=0x57535931||count>100000)AfxThrowArchiveException(CArchiveException::badSchema);
        std::vector<std::unique_ptr<MilitaryItem>> loaded;
        auto app=static_cast<CC4IMapApp*>(AfxGetApp());
        for(DWORD k=0;k<count;++k){auto i=std::make_unique<MilitaryItem>();CString code,style;DWORD n;ar>>i->layer>>code>>style>>n;
            if(n>100000||!app->m_piSymFac->CreateSymbol(code,&i->symbol)||!i->symbol)AfxThrowArchiveException(CArchiveException::badSchema);
            i->symbol->SetAssembleStream(style);i->points.resize(n);for(auto& v:i->points){ar>>v.lon>>v.lat;if(!std::isfinite(v.lon)||!std::isfinite(v.lat)||fabs(v.lat)>90)AfxThrowArchiveException(CArchiveException::badSchema);}loaded.push_back(std::move(i));}
        Cancel();p->items.swap(loaded);p->modified=false;return true;
    }catch(CException* e){e->Delete();error=_T("군대부호 불러오기 실패 (.warsym)");return false;}
}
#pragma pack(pop)

#pragma pack(push,8)
#include "cpl_json.h"
std::string CMilitaryOverlay::SyncObjects(CCoordMgr*) const
{
    CPLJSONArray objects;
    size_t index=0;
    for(const auto& item:p->items)
    {
        CPLJSONObject object;object.Add("id",std::to_string(index++));object.Add("layer",item->layer);
        CStringW code(item->symbol->GetSymbolCode()),assembly(item->symbol->GetAssembleStream());
        object.Add("code",std::string(CW2A(code,CP_UTF8)));
        object.Add("assembly",std::string(CW2A(assembly,CP_UTF8)));
        CPLJSONArray points;
        for(const auto& point:item->points){CPLJSONArray pair;pair.Add(point.lon);pair.Add(point.lat);points.Add(pair);}
        object.Add("points",points);objects.Add(object);
    }
    return objects.Format(CPLJSONObject::PrettyFormat::Plain);
}

#pragma pack(pop)
