#include "pch.h"
#include "MapRibbonTools.h"
#include "GeoDC/CoordMgr.h"
#pragma pack(push, 8)
#include <ogrsf_frmts.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <thread>

CString MapSearchUtf8(const char* text)
{
    if (!text) return CString();
    UINT cp = CP_UTF8;
    int count = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (!count) { cp = 949; count = MultiByteToWideChar(cp, 0, text, -1, nullptr, 0); }
    CStringW wide;
    MultiByteToWideChar(cp, 0, text, -1, wide.GetBuffer(count + 1), count + 1);
    wide.ReleaseBuffer();
    return CString(CW2A(wide, CP_ACP));
}

struct CMapSearchJob::State
{
    std::atomic<bool> cancelled{false}, busy{false};
    std::mutex mutex;
    bool ready = false;
    MapSearchReply reply;
};
CMapSearchJob::CMapSearchJob() : m_state(std::make_shared<State>()) {}
CMapSearchJob::~CMapSearchJob() { Cancel(); }
void CMapSearchJob::Cancel() { m_state->cancelled = true; m_state = std::make_shared<State>(); }
bool CMapSearchJob::Busy() const { return m_state->busy; }

bool CMapSearchJob::Intersects(const MapSearchFeature& feature, const MapSearchQuery& q)
{
    if (!q.spatial) return true;
    CRect r = q.area; r.NormalizeRect();
    const double rx = (std::max)(1., r.Width() / 2.), ry = (std::max)(1., r.Height() / 2.);
    const double cx = (r.left + r.right) / 2., cy = (r.top + r.bottom) / 2.;
    auto inside = [&](CPoint p) {
        if (!q.ellipse) return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
        return pow((p.x-cx)/rx,2) + pow((p.y-cy)/ry,2) <= 1;
    };
    for (auto p : feature.screen) if (inside(p)) return true;
    bool containsCenter = false;
    size_t offset = 0;
    for (int count : feature.parts)
    {
        if (count < 0 || offset + count > feature.screen.size()) break;
        for (int i = 0; i < count && feature.kind != 0; ++i)
        {
            if (i+1 == count && feature.kind != 2) break;
            CPoint a = feature.screen[offset+i], b = feature.screen[offset+(i+1)%count];
            double dx = b.x-a.x, dy = b.y-a.y;
            if (q.ellipse)
            {
                double x=(a.x-cx)/rx, y=(a.y-cy)/ry, u=dx/rx, v=dy/ry;
                double length=u*u+v*v, t=length?-(x*u+y*v)/length:0;
                t=(std::max)(0.,(std::min)(1.,t));
                if (pow(x+t*u,2)+pow(y+t*v,2)<=1) return true;
            }
            else
            {
                double lo=0, hi=1;
                auto clip=[&](double p,double v){if(p==0)return v>=0;double t=v/p;if(p<0)lo=(std::max)(lo,t);else hi=(std::min)(hi,t);return lo<=hi;};
                if (clip(-dx,a.x-r.left)&&clip(dx,r.right-a.x)&&clip(-dy,a.y-r.top)&&clip(dy,r.bottom-a.y))return true;
            }
            if (feature.kind == 2 && ((a.y>cy)!=(b.y>cy)) && cx < a.x+(cy-a.y)*dx/dy)
                containsCenter=!containsCenter;
        }
        offset += count;
    }
    return containsCenter;
}

void CMapSearchJob::Start(std::vector<MapSearchFeature> features, const MapSearchQuery& query)
{
    Cancel();
    auto state=m_state; state->busy=true;
    std::thread([state,features=std::move(features),query]() mutable {
        MapSearchReply reply;
        try
        {
            using Dataset=std::shared_ptr<GDALDataset>;
            std::map<CString,Dataset> datasets;
            std::set<CString> seen;
            CString needle=query.text; needle.MakeLower();
            unsigned failures=0;
            for(auto& feature:features)
            {
                if(state->cancelled) return;
                if(!query.layer.IsEmpty()&&feature.layer!=query.layer)continue;
                if(!Intersects(feature,query))continue;
                if(!feature.path.IsEmpty())
                {
                    if(!datasets.count(feature.path))
                    {
                        CStringW wide(CA2W(feature.path,CP_ACP));
                        auto raw=static_cast<GDALDataset*>(GDALOpenEx(CW2A(wide,CP_UTF8),GDAL_OF_VECTOR|GDAL_OF_READONLY,nullptr,nullptr,nullptr));
                        datasets[feature.path]=Dataset(raw,[](GDALDataset* d){if(d)GDALClose(d);});
                    }
                    auto ds=datasets[feature.path];
                    auto layer=ds?ds->GetLayer(0):nullptr;
                    std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)> f(layer?layer->GetFeature(feature.fid):nullptr,OGRFeature::DestroyFeature);
                    if(!f){++failures;continue;}
                    feature.fields.clear();feature.values.clear();
                    for(int i=0;i<f->GetFieldCount();++i)
                    {
                        feature.fields.push_back(MapSearchUtf8(f->GetFieldDefnRef(i)->GetNameRef()));
                        feature.values.push_back(f->IsFieldSetAndNotNull(i)?MapSearchUtf8(f->GetFieldAsString(i)):_T("(NULL)"));
                    }
                }
                bool match=needle.IsEmpty();
                for(size_t i=0;!match&&i<feature.values.size();++i)
                {
                    if(!query.field.IsEmpty()&&(i>=feature.fields.size()||feature.fields[i]!=query.field))continue;
                    CString value=feature.values[i];value.MakeLower();match=value.Find(needle)>=0;
                }
                if(!match)continue;
                CString key=feature.layer+_T("|")+feature.path+_T("|")+feature.identity;
                if(feature.identity.IsEmpty()) { for(auto& v:feature.values)key+=_T("|")+v; }
                // Tile fragments with a stable feature identity are reported once.
                if(!feature.identity.IsEmpty()&&!seen.insert(key).second)continue;
                reply.features.push_back(std::move(feature));
                if(reply.features.size()>=5000)break;
            }
            reply.message.Format(_T("현재 화면 검색: %u건%s (입력 객체 %u개, 읽기 실패 %u개). 결과를 두 번 클릭하면 이동합니다."),
                (unsigned)reply.features.size(),reply.features.size()>=5000?_T(" - 표시 상한 5,000건"):_T(""),(unsigned)features.size(),failures);
        }
        catch(const std::exception& e){reply.message=_T("검색 실패: ")+MapSearchUtf8(e.what());}
        if(features.size()>=20000)reply.message+=_T(" [입력 20,000개 한도: 확대하거나 레이어를 나누어 검색하세요.]");
        if(state->cancelled)return;
        std::lock_guard<std::mutex> lock(state->mutex);state->reply=std::move(reply);state->ready=true;state->busy=false;
    }).detach();
}
bool CMapSearchJob::Poll(MapSearchReply& reply)
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    if(!m_state->ready)return false;
    reply=std::move(m_state->reply);m_state->ready=false;return true;
}

void CMapImageAdjustment::MakeTable(int brightness,int contrast,BYTE table[256])
{
    brightness=(std::max)(-100,(std::min)(100,brightness));
    contrast=(std::max)(-100,(std::min)(99,contrast));
    const double gain=(100.+contrast)/(100.-contrast);
    for(int i=0;i<256;++i)table[i]=(BYTE)(std::max)(0.,(std::min)(255.,std::round((i-127.5)*gain+127.5+brightness*2.55)));
}
bool CMapImageAdjustment::Capture(CDC* dc,int width,int height,CImage& image)
{
    if(!dc||width<=0||height<=0)return false;
    if(!image.IsNull()&&(image.GetWidth()!=width||image.GetHeight()!=height))image.Destroy();
    if(image.IsNull()&&!image.Create(width,-height,32))return false;
    HDC target=image.GetDC();BOOL ok=::BitBlt(target,0,0,width,height,dc->GetSafeHdc(),0,0,SRCCOPY);image.ReleaseDC();return ok!=FALSE;
}
bool CMapImageAdjustment::Apply(CDC* dc,int width,int height,int brightness,int contrast)
{
    if(brightness==0&&contrast==0)return true;
    if(!Capture(dc,width,height,m_image))return false;
    BYTE table[256];MakeTable(brightness,contrast,table);
    for(int y=0;y<height;++y){auto row=(BYTE*)m_image.GetPixelAddress(0,y);for(int x=0;x<width;++x)for(int c=0;c<3;++c)row[x*4+c]=table[row[x*4+c]];}
    HDC source=m_image.GetDC();BOOL ok=::BitBlt(dc->GetSafeHdc(),0,0,width,height,source,0,0,SRCCOPY);m_image.ReleaseDC();return ok!=FALSE;
}
bool CMapImageAdjustment::CopyToClipboard(HWND owner,CImage& image)
{
    if(image.IsNull())return false;
    const size_t stride=(size_t)image.GetWidth()*4, bytes=stride*image.GetHeight();
    if(bytes>SIZE_MAX-sizeof(BITMAPINFOHEADER))return false;
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,sizeof(BITMAPINFOHEADER)+bytes);if(!data)return false;
    auto header=(BITMAPINFOHEADER*)GlobalLock(data);if(!header){GlobalFree(data);return false;}
    *header={};header->biSize=sizeof(*header);header->biWidth=image.GetWidth();header->biHeight=image.GetHeight();header->biPlanes=1;header->biBitCount=32;header->biCompression=BI_RGB;
    auto pixels=(BYTE*)(header+1);
    for(int y=0;y<image.GetHeight();++y)memcpy(pixels+(image.GetHeight()-1-y)*stride,image.GetPixelAddress(0,y),stride);
    GlobalUnlock(data);
    if(!OpenClipboard(owner)){GlobalFree(data);return false;}
    bool ok=false;
    if(EmptyClipboard())ok=SetClipboardData(CF_DIB,data)!=nullptr;
    CloseClipboard();if(!ok)GlobalFree(data);return ok;
}
void MapSearchDraw(CDC* dc,CCoordMgr* coord,const std::vector<MapSearchFeature>& features,COLORREF color)
{
    if(!dc||!coord)return;
    CPen pen(PS_SOLID,3,color);auto old=dc->SelectObject(&pen);auto brush=dc->SelectStockObject(NULL_BRUSH);
    for(const auto& f:features)
    {
        size_t offset=0;
        for(int count:f.parts)
        {
            if(count<0||offset+count>f.world.size())break;
            std::vector<POINT> points;
            for(int i=0;i<count;++i){double x,y;auto p=f.world[offset+i];coord->WorldXY2ScreenXY(p.x,p.y,&x,&y);if(std::isfinite(x)&&std::isfinite(y))points.push_back({(LONG)std::clamp(x,-1e7,1e7),(LONG)std::clamp(y,-1e7,1e7)});}
            if(f.kind==0){for(auto p:points)dc->Ellipse(p.x-6,p.y-6,p.x+7,p.y+7);}
            else if(points.size()>1){if(f.kind==2)points.push_back(points.front());dc->Polyline(points.data(),(int)points.size());}
            offset+=count;
        }
    }
    dc->SelectObject(brush);dc->SelectObject(old);
}
#pragma pack(pop)
