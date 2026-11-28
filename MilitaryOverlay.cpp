#include "pch.h"
#include "MilitaryOverlay.h"
#include "MilitaryOverlayState.h"
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

bool CMilitaryOverlay::Impl::Ready()
{
    auto app=static_cast<CC4IMapApp*>(AfxGetApp());
    if(!app || !app->m_pAppCxt || !app->m_piSymFac) 
        return false;
    if(!pixels)
    {
        if(!app->m_pAppCxt->CreateObject(_T("ICOPS_Framework"),_T(".dll"),
            _T("CMapProjectionNone"),_T("IMapProjection"),(void**)&pixels))
            return false;
        ICOPS::SGeoCoord2 center={};
        ICOPS::SOrthoCoord2 origin={};
        pixels->SetCenter(&center); 
        pixels->SetViewOrigin(&origin);
        pixels->SetMapScale(1); 
        pixels->SetZoomFactor(1);
    }
    return true;
}

bool CMilitaryOverlay::Impl::Visible(int layer,const CLocalOverlay& overlay) const
{
    for(const auto& l:overlay.Layers()) 
        if(l.id==layer)
            return l.visible;
    return false;
}

bool CMilitaryOverlay::Impl::Prepare(MilitaryItem& item,CCoordMgr* coord,std::vector<Gdiplus::PointF>& pts)
{
    if(!coord || !Ready() || item.points.empty()) 
        return false;
    pts.clear();
    for(const auto& gp:item.points)
    {
        double x,y;
        coord->WorldLonLat2ScreenXY(gp.lon,gp.lat,&x,&y,coord->m_fCenterX,coord->m_fCenterY,coord->m_fScale);
        if(!std::isfinite(x)||!std::isfinite(y))
            return false;
        pts.emplace_back((float)x,(float)y);
    }
    std::vector<ICOPS::SGeoCoord2> coords(pts.size());
    pixels->Inverse(pts.data(),coords.data(),(INT_PTR)pts.size());
    item.SetCoordinates(pixels,coords.data(),(INT_PTR)coords.size());
    return true;
}

CMilitaryOverlay::CMilitaryOverlay():p(new Impl){}

CMilitaryOverlay::~CMilitaryOverlay(){delete p;}

// History contains serialized SDK properties and geographic coordinates, never
// SDK pointers or screen-projected anchor coordinates. A drag is one edit.
bool CMilitaryOverlay::Impl::Capture(MilitaryHistoryState& state)
{
    try
    {
        CMemFile file;
        CArchive archive(&file, CArchive::store);
        archive << nextGroup << (DWORD)items.size();
        for (auto& item : items)
        {
            item->SerializeSymbol(archive);
            archive << (DWORD)item->points.size();
            for (const auto& point : item->points)
                archive << point.lon << point.lat;
        }
        archive.Close();
        state.data.resize((size_t)file.GetLength());
        file.SeekToBegin();
        if (!state.data.empty())
            file.Read(state.data.data(), (UINT)state.data.size());
        state.selection = selection;
        state.primary = selected;
        return true;
    }
    catch (CException* error) { error->Delete(); }
    return false;
}

bool CMilitaryOverlay::Impl::Restore(const MilitaryHistoryState& state)
{
    try
    {
        CMemFile file(const_cast<BYTE*>(state.data.data()), (UINT)state.data.size());
        CArchive archive(&file, CArchive::load);
        int next;
        DWORD count;
        archive >> next >> count;
        std::vector<std::unique_ptr<MilitaryItem>> restored;
        for (DWORD index = 0; index < count; ++index)
        {
            auto item = std::make_unique<MilitaryItem>();
            item->SerializeSymbol(archive);
            DWORD countPoints;
            archive >> countPoints;
            item->points.resize(countPoints);
            for (auto& point : item->points)
                archive >> point.lon >> point.lat;
            restored.push_back(std::move(item));
        }
        archive.Close();
        items.swap(restored);
        nextGroup = next;
        selection = state.selection;
        selected = state.primary;
        boxes.clear();
        modified = true;
        return true;
    }
    catch (CException* error) { error->Delete(); }
    return false;
}

bool CMilitaryOverlay::Impl::BeginChange()
{
    pendingHistory = Capture(pending);
    return pendingHistory;
}

void CMilitaryOverlay::Impl::CommitChange()
{
    if (!pendingHistory) return;
    MilitaryHistoryState after;
    if (Capture(after) && after.data != pending.data)
    {
        undo.push_back(std::move(pending));
        // Bound history memory growth for long editing sessions.
        if (undo.size() > 50) undo.erase(undo.begin());
        redo.clear();
    }
    pendingHistory = false;
    pending = MilitaryHistoryState();
}

void CMilitaryOverlay::Impl::ResetHistory()
{
    undo.clear();
    redo.clear();
    pending = MilitaryHistoryState();
    pendingHistory = false;
}

bool CMilitaryOverlay::CanUndo() const { return !p->undo.empty(); }
bool CMilitaryOverlay::CanRedo() const { return !p->redo.empty(); }

bool CMilitaryOverlay::Undo()
{
    CancelGesture();
    p->adding.reset();
    if (!CanUndo()) return false;
    MilitaryHistoryState current;
    if (!p->Capture(current) || !p->Restore(p->undo.back())) return false;
    p->redo.push_back(std::move(current));
    p->undo.pop_back();
    p->selectionMode = true;
    return true;
}

bool CMilitaryOverlay::Redo()
{
    CancelGesture();
    p->adding.reset();
    if (!CanRedo()) return false;
    MilitaryHistoryState current;
    if (!p->Capture(current) || !p->Restore(p->redo.back())) return false;
    p->undo.push_back(std::move(current));
    p->redo.pop_back();
    p->selectionMode = true;
    return true;
}

bool CMilitaryOverlay::Choose(CWnd* owner,int layer)
{
    Cancel();
    if(!p->Ready())
    {
        AfxMessageBox(_T("군대부호 SDK가 초기화되지 않았습니다."));
        return false;
    }
    auto app=static_cast<CC4IMapApp*>(AfxGetApp());
    ICOPS::IWarSymPropertyEditorPtr editor;
    if(!app->m_piSymFac->CreatePropertyEditor(&editor) || !editor)
    {
        AfxMessageBox(_T("군대부호 선택창을 생성할 수 없습니다. ICOPS UI DLL과 설정을 확인하세요."));
        return false;
    }
    editor->SetWarSymFactory(app->m_piSymFac);
    auto item=std::make_unique<MilitaryItem>();
    if(!editor->OpenSymbolSelector(p->lastCode,owner,&item->m_piSym) || !item->m_piSym) 
        return false;
    p->lastCode=item->m_piSym->GetSymbolCode();
    item->layer=layer; 
    p->selected=-1;
    p->selectionMode=true;
    p->adding=std::move(item);
    return true;
}

bool CMilitaryOverlay::Edit(CWnd* owner)
{
    if(SelectionCount()!=1 || !p->Ready()) 
        return false;
    ICOPS::IWarSymPropertyEditorPtr editor;
    auto app=static_cast<CC4IMapApp*>(AfxGetApp());
    if(!app->m_piSymFac->CreatePropertyEditor(&editor)||!editor)
        return false;
    editor->SetWarSymFactory(app->m_piSymFac);
    if (!p->BeginChange()) return false;
    editor->SetTargetSymbol(p->items[p->selected]->m_piSym);
    const bool ok=!!editor->OpenEditor(_T("군대부호 속성 편집"),owner);
    p->modified=true;
    p->items[p->selected]->m_piSym->InvalidateSymbolImage();
    p->CommitChange();
    return ok;
}

bool CMilitaryOverlay::Impl::Bounds(MilitaryItem& item, Gdiplus::Graphics& graphics,
    const std::vector<Gdiplus::PointF>& points, Gdiplus::RectF& rectangle)
{
    if(points.empty()) return false;
    const auto state = graphics.Save();
    Gdiplus::RectF sdkBox;
    const bool measured = item.GetBoundingRect(&graphics,pixels,1.0,&sdkBox,TRUE) != FALSE;
    graphics.Restore(state);
    if(measured && std::isfinite(sdkBox.X) && std::isfinite(sdkBox.Y) &&
        std::isfinite(sdkBox.Width) && std::isfinite(sdkBox.Height) &&
        sdkBox.Width>=0 && sdkBox.Height>=0 && sdkBox.Width+sdkBox.Height>0)
    {
        rectangle=sdkBox;
        if(rectangle.Width<8){rectangle.X-=(8-rectangle.Width)/2;rectangle.Width=8;}
        if(rectangle.Height<8){rectangle.Y-=(8-rectangle.Height)/2;rectangle.Height=8;}
        return true;
    }
    // Some SDK symbols have no cached device bounds until first rendering.
    float left=points[0].X,right=left,top=points[0].Y,bottom=top;
    for(const auto& point:points)
    {
        left=std::min(left,point.X);right=std::max(right,point.X);
        top=std::min(top,point.Y);bottom=std::max(bottom,point.Y);
    }
    const float size=item.m_piSym->GetSymbolSize();
    const float padding=std::isfinite(size)?std::max(12.f,std::min(size*.75f,256.f)):16.f;
    rectangle=Gdiplus::RectF(left-padding,top-padding,right-left+2*padding,bottom-top+2*padding);
    return true;
}

void CMilitaryOverlay::Draw(CDC* dc,CCoordMgr* coord,const CLocalOverlay& layers)
{
    if(!dc || !coord || (p->items.empty()&&!p->adding)) return;
    for(auto it=p->selection.begin();it!=p->selection.end();)
    {
        if(!p->Visible(p->items[*it]->layer,layers)) it=p->selection.erase(it);
        else ++it;
    }
    p->UpdatePrimary();
    Gdiplus::Graphics graphics(dc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(255,0,120,255),1);
    auto draw=[&](MilitaryItem& item)
    {
        if(!p->Visible(item.layer,layers)) return;
        std::vector<Gdiplus::PointF> points;
        if(!p->Prepare(item,coord,points)) return;
        const auto state=graphics.Save();
        const INT_PTR required=item.m_piSym->CountAnchorPoints(0,(INT_PTR)points.size());
        if((required>0 && points.size()>=(size_t)required)||(required<0 && points.size()>=(size_t)-required))
            item.Draw(&graphics,points.data(),(INT_PTR)points.size(),1.0);
        else if(points.size()>1)
            graphics.DrawLines(&pen,points.data(),(INT)points.size());
        graphics.Restore(state);
    };
    for(auto& item:p->items) draw(*item);
    if(p->adding) draw(*p->adding);
}

void CMilitaryOverlay::DrawSelection(CDC* dc,CCoordMgr* coord,const CLocalOverlay& layers)
{
    if(!dc || !coord) return;
    Gdiplus::Graphics graphics(dc->GetSafeHdc());
    graphics.SetPageUnit(Gdiplus::UnitPixel);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    Gdiplus::Pen halo(Gdiplus::Color(255,255,255,255),4.f);
    Gdiplus::Pen outline(Gdiplus::Color(255,0,100,240),2.f);
    Gdiplus::SolidBrush white(Gdiplus::Color(255,255,255,255));
    Gdiplus::RectF combined;
    bool hasCombined=false;
    auto selection=[&](MilitaryItem& item,bool handles)
    {
        if(!p->Visible(item.layer,layers)) return;
        std::vector<Gdiplus::PointF> points;
        if(!p->Prepare(item,coord,points)) return;
        Gdiplus::RectF box;
        if(!p->Bounds(item,graphics,points,box)) return;
        box.Inflate(4.f,4.f);
        graphics.DrawRectangle(&halo,box);
        graphics.DrawRectangle(&outline,box);
        if(hasCombined){Gdiplus::RectF united;Gdiplus::RectF::Union(united,combined,box);combined=united;}
        else {combined=box;hasCombined=true;}
        if(handles)
            for(const auto& point:points)
            {
                graphics.FillRectangle(&white,point.X-4,point.Y-4,8.f,8.f);
                graphics.DrawRectangle(&outline,point.X-4,point.Y-4,8.f,8.f);
            }
    };
    for(int index:p->selection)
        if(index>=0 && (size_t)index<p->items.size())
            selection(*p->items[index],p->selection.size()==1 && p->items[index]->group==0);
    if(p->adding) selection(*p->adding,true);
    if(hasCombined && p->selection.size()>1)
    {
        combined.Inflate(5.f,5.f);
        graphics.DrawRectangle(&halo,combined);
        outline.SetDashStyle(Gdiplus::DashStyleDash);
        graphics.DrawRectangle(&outline,combined);
    }
    if(p->boxing)
    {
        CRect rectangle(p->boxStart,p->boxEnd);rectangle.NormalizeRect();
        Gdiplus::SolidBrush fill(Gdiplus::Color(35,0,120,255));
        outline.SetDashStyle(Gdiplus::DashStyleDash);
        graphics.FillRectangle(&fill,rectangle.left,rectangle.top,rectangle.Width(),rectangle.Height());
        graphics.DrawRectangle(&outline,rectangle.left,rectangle.top,rectangle.Width(),rectangle.Height());
    }
}

void CMilitaryOverlay::Impl::UpdatePrimary()
{
    if(!selection.count(selected))
        selected=selection.empty() ? -1 : *selection.rbegin();
}

void CMilitaryOverlay::Impl::SelectUnit(int index,bool select)
{
    const auto& item=*items[index];
    for(size_t i=0;i<items.size();++i)
    {
        if((int)i!=index && (item.group==0 || items[i]->group!=item.group))
            continue;
        if(select) selection.insert((int)i);
        else selection.erase((int)i);
    }
    UpdatePrimary();
}

bool CMilitaryOverlay::Down(CPoint point,CCoordMgr* coord,const CLocalOverlay& layers,UINT flags)
{
    MilitaryPoint location;
    if(!MapScreenLonLat(coord,point.x,point.y,location.lon,location.lat))
        return false;
    p->consumed=false;
    if(p->adding)
    {
        p->consumed=true;
        if(!p->Visible(p->adding->layer,layers))
        {
            Cancel();
            return true;
        }
        p->adding->points.push_back(location);
        const auto count=p->adding->m_piSym->CountAnchorPoints(0,(INT_PTR)p->adding->points.size());
        if(count>0 && p->adding->points.size()>=(size_t)count)
            Finish();
        return true;
    }

    const bool control=(flags & MK_CONTROL)!=0;
    const bool additive=control || (flags & MK_SHIFT)!=0;
    p->selectionBefore=p->selection;
    p->boxes.clear();
    p->handle=-1;
    CClientDC dc(AfxGetMainWnd());
    Gdiplus::Graphics graphics(dc.GetSafeHdc());
    int hit=-1;
    for(int i=(int)p->items.size()-1;i>=0;--i)
    {
        auto& item=*p->items[i];
        if(!p->Visible(item.layer,layers))
            continue;
        std::vector<Gdiplus::PointF> points;
        if(!p->Prepare(item,coord,points))
            continue;
        Gdiplus::RectF box;
        if(!p->Bounds(item,graphics,points,box))
            continue;
        // Include a small hit tolerance even for thin/point symbols.
        box.Inflate(5.f,5.f);
        p->boxes.push_back({i,box});
        if(hit<0 && box.Contains((float)point.x,(float)point.y))
        {
            hit=i;
            if(p->selection.size()==1 && p->selection.count(i) && item.group==0 && !control)
                for(size_t k=0;k<points.size();++k)
                    if(std::hypot(points[k].X-point.x,points[k].Y-point.y)<=8)
                    {
                        p->handle=(int)k;
                        break;
                    }
        }
    }
    if(hit>=0)
    {
        p->selectionMode=true;
        p->consumed=true;
        if(control)
        {
            p->SelectUnit(hit,p->selection.count(hit)==0);
            return true;
        }
        if(!p->selection.count(hit))
        {
            p->selection.clear();
            p->SelectUnit(hit,true);
        }
        p->selected=hit;
        p->last=location;
        p->original.clear();
        for(int index:p->selection)
            p->original[index]=p->items[index]->points;
        if (!p->BeginChange()) return true;
        p->modifiedBefore=p->modified;
        p->dragging=true;
        return true;
    }
    if(!p->selectionMode && !control)
    {
        p->selection.clear();p->UpdatePrimary();
        return false;
    }
    p->boxing=true;
    p->consumed=true;
    p->boxStart=p->boxEnd=point;
    if(!additive) p->selection.clear();
    p->UpdatePrimary();
    return true;
}

bool CMilitaryOverlay::Move(CPoint point,CCoordMgr* coord)
{
    if(p->boxing)
    {
        p->boxEnd=point;
        return true;
    }
    if(!p->dragging || !Selected())
        return false;
    MilitaryPoint location;
    if(!MapScreenLonLat(coord,point.x,point.y,location.lon,location.lat))
        return true;
    if(location.lon==p->last.lon && location.lat==p->last.lat)
        return true;
    if(p->handle>=0)
        p->items[p->selected]->points[p->handle]=location;
    else
    {
        const double dx=std::remainder(location.lon-p->last.lon,360.0);
        const double dy=location.lat-p->last.lat;
        // Validate the entire group before changing any member.
        for(int index:p->selection)
            for(const auto& vertex:p->items[index]->points)
                if(vertex.lat+dy < -90 || vertex.lat+dy > 90)
                    return true;
        for(int index:p->selection)
            for(auto& vertex:p->items[index]->points)
            {
                vertex.lon=std::remainder(vertex.lon+dx,360.0);
                vertex.lat+=dy;
            }
    }
    p->modified=true;
    p->last=location;
    return true;
}

bool CMilitaryOverlay::Up()
{
    const bool active=p->dragging || p->boxing || p->consumed;
    if(p->boxing)
    {
        CRect rectangle(p->boxStart,p->boxEnd);
        rectangle.NormalizeRect();
        if(rectangle.Width()>=3 || rectangle.Height()>=3)
        {
            Gdiplus::RectF area((float)rectangle.left,(float)rectangle.top,
                (float)std::max(1,rectangle.Width()),(float)std::max(1,rectangle.Height()));
            for(const auto& box:p->boxes)
                if(area.IntersectsWith(box.bounds))
                    p->SelectUnit(box.index,true);
        }
    }
    if (p->dragging) p->CommitChange();
    p->consumed=false;
    p->dragging=false;
    p->boxing=false;
    p->original.clear();
    p->selectionBefore.clear();
    p->boxes.clear();
    p->UpdatePrimary();
    return active;
}

bool CMilitaryOverlay::Finish()
{
    if(!p->adding)
        return false;
    const auto n=p->adding->m_piSym->CountAnchorPoints(0,(INT_PTR)p->adding->points.size());
    if(n==0 || p->adding->points.size()<(size_t)std::abs(n))
        return false;
    if (!p->BeginChange()) return false;
    p->modified=true;
    p->items.push_back(std::move(p->adding));
    p->selected=(int)p->items.size()-1;
    p->selection={p->selected};
    p->selectionMode=true;
    p->CommitChange();
    return true;
}

void CMilitaryOverlay::CancelGesture()
{
    p->pendingHistory=false;
    p->pending=MilitaryHistoryState();
    if(p->dragging)
    {
        for(const auto& entry:p->original)
            p->items[entry.first]->points=entry.second;
        p->modified=p->modifiedBefore;
    }
    if(p->boxing)
        p->selection=p->selectionBefore;
    p->dragging=p->boxing=p->consumed=false;
    p->original.clear();p->boxes.clear();p->selectionBefore.clear();
    p->UpdatePrimary();
}

void CMilitaryOverlay::Cancel()
{
    CancelGesture();
    p->adding.reset();
    p->selection.clear();p->selected=-1;
    p->selectionMode=false;
}

void CMilitaryOverlay::SelectMode()
{
    CancelGesture();
    p->adding.reset();
    p->selectionMode=true;
}

bool CMilitaryOverlay::SelectionMode() const { return p->selectionMode; }
bool CMilitaryOverlay::Selected() const { return !p->selection.empty(); }
size_t CMilitaryOverlay::SelectionCount() const { return p->selection.size(); }

void CMilitaryOverlay::SelectAll(const CLocalOverlay& layers)
{
    SelectMode();
    p->selection.clear();
    for(size_t i=0;i<p->items.size();++i)
        if(p->Visible(p->items[i]->layer,layers) &&
            (!layers.ActiveLayer() || p->items[i]->layer==layers.ActiveLayer()))
            p->selection.insert((int)i);
    p->UpdatePrimary();
}

bool CMilitaryOverlay::CanGroup() const
{
    if(p->selection.size()<2 || p->dragging || p->boxing || p->adding) return false;
    const auto& first=*p->items[*p->selection.begin()];
    bool different=first.group==0;
    for(int index:p->selection)
    {
        const auto& item=*p->items[index];
        if(item.layer!=first.layer) return false;
        different |= item.group!=first.group;
    }
    return different;
}

bool CMilitaryOverlay::CanUngroup() const
{
    if(p->dragging || p->boxing || p->adding) return false;
    for(int index:p->selection) if(p->items[index]->group>0) return true;
    return false;
}

bool CMilitaryOverlay::GroupSelection(bool ungroup)
{
    if(ungroup ? !CanUngroup() : !CanGroup()) return false;
    if (!ungroup && p->nextGroup==INT_MAX) return false;
    if (!p->BeginChange()) return false;
    if(ungroup)
    {
        std::set<int> groups;
        for(int index:p->selection) groups.insert(p->items[index]->group);
        groups.erase(0);
        for(auto& item:p->items) if(groups.count(item->group)) item->group=0;
        if(p->selected>=0) p->selection={p->selected};
    }
    else
    {
        if(p->nextGroup==INT_MAX) return false;
        const int group=p->nextGroup++;
        for(int index:p->selection) p->items[index]->group=group;
    }
    p->modified=true;
    p->CommitChange();
    return true;
}

bool CMilitaryOverlay::Delete()
{
    if(!Selected()) return false;
    if (!p->BeginChange()) return false;
    // Descending indices keep the remaining selections valid during erasure.
    for(auto index=p->selection.rbegin();index!=p->selection.rend();++index)
        p->items.erase(p->items.begin()+*index);
    p->selection.clear();p->selected=-1;
    p->original.clear();p->dragging=p->boxing=p->consumed=false;
    p->modified=true;
    p->CommitChange();
    return true;
}

bool CMilitaryOverlay::Modified()const{return p->modified;}
void CMilitaryOverlay::Clear()
{
    Cancel();
    p->items.clear();
    p->ResetHistory();
    p->nextGroup=1;
    p->modified=false;
}

size_t CMilitaryOverlay::Count(int layer)const
{
    size_t n=0;
    for(const auto& i:p->items)
        if(i->layer==layer)
            ++n;
    return n;
}

bool CMilitaryOverlay::Save(const CString& path,CString& error) const
{
    CString temporary;
    try
    {
        const CString target=path+_T(".warsym");
        CString folder=target.Left(target.ReverseFind(_T('\\'))+1);
        if(folder.IsEmpty()) folder=target.Left(target.ReverseFind(_T('/'))+1);
        if(folder.IsEmpty()) folder=_T(".");
        TCHAR name[MAX_PATH]={};
        if(!GetTempFileName(folder,_T("wsy"),0,name)) AfxThrowFileException(CFileException::genericException);
        temporary=name;
        CFile file(temporary,CFile::modeCreate|CFile::modeWrite|CFile::shareExclusive);
        CArchive archive(&file,CArchive::store);
        archive << (DWORD)0x57535932 << (DWORD)p->items.size();
        for(const auto& item:p->items)
        {
            item->SerializeSymbol(archive);
            archive << (DWORD)item->points.size();
            for(const auto& vertex:item->points) archive << vertex.lon << vertex.lat;
        }
        archive.Close();file.Flush();file.Close();
        if(!MoveFileEx(temporary,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            AfxThrowFileException(CFileException::genericException);
        p->modified=false;
        return true;
    }
    catch(CException* exception)
    {
        exception->Delete();
        if(!temporary.IsEmpty()) DeleteFile(temporary);
        error=_T("군대부호 저장 실패. 기존 파일은 유지됩니다 (.warsym).");
        return false;
    }
}

bool CMilitaryOverlay::Load(const CString& path,CString& error)
{
    if(GetFileAttributes(path+_T(".warsym"))==INVALID_FILE_ATTRIBUTES)
    {
        Clear();return true;
    }
    try
    {
        if(!p->Ready()) AfxThrowArchiveException(CArchiveException::badSchema);
        CFile file(path+_T(".warsym"),CFile::modeRead|CFile::shareDenyWrite);
        CArchive archive(&file,CArchive::load);
        DWORD magic,count;
        archive >> magic >> count;
        if((magic!=0x57535931 && magic!=0x57535932)||count>100000)
            AfxThrowArchiveException(CArchiveException::badSchema);
        std::vector<std::unique_ptr<MilitaryItem>> loaded;
        auto app=static_cast<CC4IMapApp*>(AfxGetApp());
        std::map<int,int> groupLayers;
        int nextGroup=1;
        for(DWORD index=0;index<count;++index)
        {
            auto item=std::make_unique<MilitaryItem>();
            DWORD points;
            if(magic==0x57535932)
                item->SerializeSymbol(archive);
            else
            {
                CString code,assembly;
                archive >> item->layer >> code >> assembly;
                if(!app->m_piSymFac->CreateSymbol(code,&item->m_piSym)||!item->m_piSym ||
                    !item->SetAssembleStream(assembly))
                    AfxThrowArchiveException(CArchiveException::badSchema);
            }
            archive >> points;
            if(points==0 || points>100000 || item->layer<0 || item->group<0 || item->group==INT_MAX)
                AfxThrowArchiveException(CArchiveException::badSchema);
            if(item->group>0)
            {
                if(groupLayers.count(item->group) && groupLayers[item->group]!=item->layer)
                    AfxThrowArchiveException(CArchiveException::badSchema);
                groupLayers[item->group]=item->layer;
                nextGroup=std::max(nextGroup,item->group+1);
            }
            item->points.resize(points);
            for(auto& vertex:item->points)
            {
                archive >> vertex.lon >> vertex.lat;
                if(!std::isfinite(vertex.lon)||!std::isfinite(vertex.lat)||
                    std::abs(vertex.lon)>180 || std::abs(vertex.lat)>90)
                    AfxThrowArchiveException(CArchiveException::badSchema);
            }
            loaded.push_back(std::move(item));
        }
        archive.Close();file.Close();
        Cancel();p->ResetHistory();p->items.swap(loaded);p->nextGroup=nextGroup;p->modified=false;
        return true;
    }
    catch(CException* exception)
    {
        exception->Delete();
        error=_T("군대부호 불러오기 실패. 파일 형식과 부호 데이터를 확인하세요 (.warsym).");
        return false;
    }
}
#pragma pack(pop)

#pragma pack(push,8)
#include "cpl_json.h"
#include "ICOPS_WarSym/ICOPS_WarSymCommon.h"
#pragma pack(8)
std::string CMilitaryOverlay::SyncObjects(CCoordMgr*) const
{
    CPLJSONArray objects;
    size_t index=0;
    for(const auto& item:p->items)
    {
        CPLJSONObject object;
        object.Add("id",std::to_string(index++));
        object.Add("layer",item->layer);
        object.Add("group",item->group);
        object.Add("symbolSize",(double)item->m_piSym->GetSymbolSize());
        object.Add("transparency",(int)item->m_piSym->GetTransparency());
        object.Add("rotation",(double)item->m_piSym->GetSymbolRotate());
        if (const auto mode=item->m_piSym->GetDrawMode())
        {
            object.Add("hasDrawMode",true);
            object.Add("bgWhite",mode->isBgWhite!=FALSE);
            object.Add("colorMode",mode->isColorMode!=FALSE);
            object.Add("fillFrame",mode->fillFrame!=FALSE);
            object.Add("showFrame",mode->showFrame!=FALSE);
            object.Add("showIcon",mode->showIcon!=FALSE);
            object.Add("civilian",mode->civ!=FALSE);
            object.Add("luminance",(int)mode->luminance);
            object.Add("displayType",(int)mode->dispType);
            object.Add("symbolState",(int)mode->symState);
        }
        CStringW code(item->m_piSym->GetSymbolCode()),assembly(item->m_piSym->GetAssembleStream());
        object.Add("code",std::string(CW2A(code,CP_UTF8)));
        object.Add("assembly",std::string(CW2A(assembly,CP_UTF8)));
        CPLJSONArray points;
        for(const auto& point:item->points)
        {
            CPLJSONArray pair;pair.Add(point.lon);pair.Add(point.lat);points.Add(pair);
        }
        object.Add("points",points);objects.Add(object);
    }
    return objects.Format(CPLJSONObject::PrettyFormat::Plain);
}

#pragma pack(pop)
