#include "pch.h"
#pragma pack(push,8)
#include "MapSources.h"
#include "GeoDC/Coord.h"
#include "GeoDC/CoordMgr.h"
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <gdal_alg.h>
#include <ogrsf_frmts.h>
#include <cpl_minixml.h>
#include <cpl_string.h>
#include <atlimage.h>
#include <shlobj.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <set>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <functional>
#include "VectorTileStyle.h"
#include "RemoteMapIdentify.h"

#pragma comment(lib,"msimg32.lib")

namespace 
{
std::string SU(const CString& s)
{
    CStringW w(CT2W(s, CP_ACP));
    int n=WideCharToMultiByte(CP_UTF8,0,w,-1,0,0,0,0);
    std::string r(n,0);
    WideCharToMultiByte(CP_UTF8,0,w,-1,&r[0],n,0,0);
    r.pop_back();
    return r;
}

CString ST(const std::string& s)
{
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,0,0);
    CStringW w;
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,w.GetBuffer(n),n);
    w.ReleaseBuffer();
    return CString(CW2T(w, CP_ACP));
}

std::string SV(CPLXMLNode* n,const char* key,const char* def="")
{
    return CPLGetXMLValue(n,key,def);
}

std::string SX(const std::string& s)
{
    char* p=CPLEscapeString(s.c_str(),-1,CPLES_XML);
    std::string r=p?p:"";
    CPLFree(p);
    return r;
}

std::string Lower(std::string s)
{
    for(auto& c:s)
        c=(char)tolower((unsigned char)c);
    return s;
}

std::string ReadUtf8(const CString& path)
{
    CFile f(path,CFile::modeRead|CFile::shareDenyWrite);
    auto n=f.GetLength();
    if(n>8*1024*1024)
        throw std::runtime_error("Catalog too large");
    std::string s((size_t)n,0);
    if(n)
        f.Read(&s[0],(UINT)n);
    return s;
}

void AtomicWrite(const CString& path,const std::string& bytes)
{
    CString tmp=path+_T(".tmp");
    CFile f(tmp,CFile::modeCreate|CFile::modeWrite|CFile::shareExclusive);
    f.Write(bytes.data(),(UINT)bytes.size());
    f.Flush();
    f.Close();
    if(!MoveFileEx(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    {
        DeleteFile(tmp);
        throw std::runtime_error("Cannot replace XML file");
    }
}

using Dataset=std::unique_ptr<GDALDataset,void(*)(GDALDataset*)>;
Dataset DS(GDALDataset* p)
{
    return Dataset(p,[](GDALDataset* d){if(d)GDALClose(d);});
}

struct VectorFeature { std::shared_ptr<OGRGeometry> geometry;std::string title;std::vector<std::string> fields,values; };
struct Tile 
{
    std::unique_ptr<CImage> displayImage; // Created on UI thread, reused for subsequent paints.
    size_t sourceId = 0;
    unsigned long long lastUse=0;
    std::vector<vtstyle::ScreenSymbol> symbols;
    std::vector<VectorFeature> features;
    double west,north,east,south;
    std::vector<unsigned char> bgra;
};

struct TileKey 
{
    int z,x,y;
    bool operator<(const TileKey& b)const
    {
        return std::tie(z,x,y)<std::tie(b.z,b.x,b.y);
    }
};

struct Request 
{
    int z=0; double styleScale=0;
    std::vector<TileKey> tiles;std::vector<size_t> sources;
    bool operator==(const Request& b)const
    {
        return z==b.z&&styleScale==b.styleScale&&sources==b.sources&&tiles.size()==b.tiles.size()&& std::equal(tiles.begin(),tiles.end(),b.tiles.begin(),[](const TileKey&a,const TileKey&b){return a.z==b.z&&a.x==b.x&&a.y==b.y;});
    }
};

struct RasterFile 
{
    std::string path;
    double west=-180,south=-90,east=180,north=90;
};

bool Allowed(const MapSourceLayer& l)
{
    if(l.type=="VectorTile")return C4MAP_ENABLE_LOCAL && C4MAP_SHAPE_MODE!=1;
    if(l.type=="Raster"||l.type=="CIB"){
        const int mode=l.type=="Raster"?C4MAP_RASTER_MODE:C4MAP_CIB_MODE;
        const bool xyz=l.tileScheme=="XYZ";
        if((mode==1&&xyz)||(mode==2&&!xyz))return false;
    }
    return (C4MAP_ENABLE_LOCAL&&(l.type=="Raster"||l.type=="CIB"))||(C4MAP_ENABLE_OGC&&(l.type=="WMS"||l.type=="WFS"||l.type=="WMTS"));
}

std::string Number(double v)
{
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s<<std::setprecision(17)<<v;
    return s.str();
}

std::string Connection(const MapSourceLayer& l)
{
#if C4MAP_ENABLE_OGC
    if(l.type=="WMS")
        return "<GDAL_WMS><Service name=\"WMS\"><Version>"+SX(l.version)+"</Version><ServerUrl>"+SX(l.endpoint)+"</ServerUrl><SRS>EPSG:4326</SRS><CRS>EPSG:4326</CRS><BBoxOrder>"+(l.version=="1.3.0"?std::string("yxYX"):std::string("xyXY"))+"</BBoxOrder><Layers>"+SX(l.layer)+"</Layers><Styles>"+SX(l.style)+"</Styles><ImageFormat>"+SX(l.format)+"</ImageFormat><Transparent>TRUE</Transparent></Service><DataWindow><UpperLeftX>-180</UpperLeftX><UpperLeftY>90</UpperLeftY><LowerRightX>180</LowerRightX><LowerRightY>-90</LowerRightY><SizeX>268435456</SizeX><SizeY>134217728</SizeY></DataWindow><Projection>EPSG:4326</Projection><BandsCount>4</BandsCount><BlockSizeX>256</BlockSizeX><BlockSizeY>256</BlockSizeY><OverviewCount>20</OverviewCount><Timeout>8</Timeout><MaxConnections>4</MaxConnections><Cache><Expires>3600</Expires><MaxSize>268435456</MaxSize></Cache></GDAL_WMS>";
    if(l.type=="WMTS")
        return "<GDAL_WMTS><GetCapabilitiesUrl>"+SX(l.endpoint)+"</GetCapabilitiesUrl><Layer>"+SX(l.layer)+"</Layer><Style>"+SX(l.style)+"</Style><TileMatrixSet>"+SX(l.matrix)+"</TileMatrixSet><Cache><Expires>3600</Expires><MaxSize>268435456</MaxSize></Cache></GDAL_WMTS>";
#endif
    return l.path;
}

}
struct CMapSources::Impl 
{
    RemoteMapIdentify identify;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool stop=false, pending=false, busy=false, changed=false;
    std::atomic<unsigned> revision{0},ticket{0};
    unsigned dataRevision=0;
    unsigned activeTicket=0;std::vector<MapSourceLayer> layers;
    Request request;
    CString xml,status;
    std::vector<std::shared_ptr<Tile>> ready;
    std::map<std::tuple<size_t,int,int,int>,std::shared_ptr<Tile>> cache;
    // Worker-only cache: retain parsed MVT datasets across adjacent render cells.
    struct VectorDataset
    {
        Dataset dataset;
        unsigned long long lastUse = 0;
    };
    std::map<std::string, VectorDataset> vectorDatasets;
    unsigned long long vectorDatasetClock = 0;

    GDALDataset* OpenVectorTile(const CString& path)
    {
        const std::string key = SU(path);
        auto found = vectorDatasets.find(key);
        if (found != vectorDatasets.end())
        {
            found->second.lastUse = ++vectorDatasetClock;
            return found->second.dataset.get();
        }

        // The catalog already identifies this file as MVT. Bypass header-only
        // format probing, which can throw caught GPBException on valid tiles.
        // The MVT decoder still validates the complete tile when opening it.
        const std::string connection = "MVT:" + key;
        const char* drivers[] = { "MVT", nullptr };
        auto dataset = DS(static_cast<GDALDataset*>(GDALOpenEx(
            connection.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
            drivers, nullptr, nullptr)));
        if (!dataset)
            return nullptr;

        // Bound open files and decoded dataset retention independently of image tiles.
        if (vectorDatasets.size() >= 32)
        {
            auto oldest = std::min_element(vectorDatasets.begin(), vectorDatasets.end(),
                [](const auto& a, const auto& b)
                {
                    return a.second.lastUse < b.second.lastUse;
                });
            vectorDatasets.erase(oldest);
        }

        auto result = dataset.get();
        vectorDatasets.emplace(key, VectorDataset{ std::move(dataset), ++vectorDatasetClock });
        return result;
    }

    double renderingStyleScale=0;
    unsigned long long cacheClock=0;
    struct XyzSet{CString root;std::set<int> levels;};
    std::map<size_t,std::vector<XyzSet>> xyzIndexes;
    std::map<size_t,std::vector<RasterFile>> indexes;
    std::map<size_t,Dataset> remote;
    // Owned exclusively by the render worker; never shared with identify requests.
    std::map<std::string,Dataset> wfsConnections;
    std::map<std::string,std::unique_ptr<vtstyle::Style>> vectorStyles;
    void Note(const CString& s)
    {
        std::lock_guard<std::mutex> lock(mutex);
        status=s;
        changed=true;
    }

    Impl()
    {
        worker=std::thread([this]{Run();});
    }
    ~Impl()
    {
        {
            std::lock_guard<std::mutex> l(mutex);
            stop=true;++revision;cv.notify_all();
        }
        worker.join();
    }
    bool Cancel(unsigned rev)
    {
        return revision.load()!=rev||ticket.load()!=activeTicket;
    }
    void Scan(const MapSourceLayer& source,const CString& folder,std::vector<RasterFile>& result,unsigned rev,std::set<std::string>& visited)
    {
        if(Cancel(rev))
            return;
        DWORD attr=GetFileAttributes(folder);
        if(attr==INVALID_FILE_ATTRIBUTES)
            return;
        if(attr&FILE_ATTRIBUTE_DIRECTORY)
        {
            if(attr&FILE_ATTRIBUTE_REPARSE_POINT)
                return;
            CFileFind find;
            BOOL more=find.FindFile(folder+_T("/*"));
            while(more&&!Cancel(rev))
            {
                more=find.FindNextFile();
                if(find.IsDots())
                    continue;
                if(find.IsDirectory())
                {
                    if(source.recursive)
                        Scan(source,find.GetFilePath(),result,rev,visited);
                }
                else 
                    Scan(source,find.GetFilePath(),result,rev,visited);
            }
            return;
        }

        std::string path=SU(folder),ext=Lower(path.substr(path.find_last_of('.')+1));
        if(!source.extensions.empty()&&(";"+Lower(source.extensions)+";").find(";"+ext+";")==std::string::npos)
            return;
        if(!visited.insert(path).second)
            return;
        auto ds=DS((GDALDataset*)GDALOpenEx(path.c_str(),GDAL_OF_RASTER|GDAL_OF_READONLY,nullptr,nullptr,nullptr));
        if(!ds||!ds->GetRasterCount())
            return;
        double gt[6];
        if(ds->GetGeoTransform(gt)!=CE_None)
            return;
        OGRSpatialReference input,target;
        target.SetWellKnownGeogCS("WGS84");
        target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if(input.SetFromUserInput(ds->GetProjectionRef())!=OGRERR_NONE)
        {
            if(source.srs.empty()||input.SetFromUserInput(source.srs.c_str())!=OGRERR_NONE)
                return;
        }
        input.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        auto tr=OGRCreateCoordinateTransformation(&input,&target);
        if(!tr)
            return;
        RasterFile f;
        f.path=path;
        f.west=f.south=1e30;
        f.east=f.north=-1e30;
        for(int iy=0;iy<3;++iy)
            for(int ix=0;ix<3;++ix)
            {
                double px=ds->GetRasterXSize()*ix/2.0, py=ds->GetRasterYSize()*iy/2.0, x=gt[0]+px*gt[1]+py*gt[2],y=gt[3]+px*gt[4]+py*gt[5];
                if(tr->Transform(1,&x,&y))
                {
                    f.west=(std::min)(f.west,x);
                    f.east=(std::max)(f.east,x);
                    f.south=(std::min)(f.south,y);
                    f.north=(std::max)(f.north,y);
                }
            }

        OCTDestroyCoordinateTransformation(tr);
        if(f.west<=f.east&&f.south<=f.north)
            result.push_back(f);
    }
    static int Progress(double,const char*,void* ptr)
    {
        auto pair=(std::pair<Impl*,unsigned>*)ptr;
        return !pair->first->Cancel(pair->second);
    }

    void FindXyz(const CString& root,std::vector<XyzSet>& sets,unsigned rev,bool recursive){
        if(Cancel(rev))return;
        DWORD attributes=GetFileAttributes(root);
        if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return;
        CFileFind find;BOOL more=find.FindFile(root+_T("/*"));XyzSet set;set.root=root;std::vector<CString> children;
        while(more&&!Cancel(rev)){more=find.FindNextFile();if(find.IsDots()||!find.IsDirectory())continue;
            CString name=find.GetFileName();bool digits=!name.IsEmpty();for(int i=0;i<name.GetLength();++i)if(name[i]<_T('0')||name[i]>_T('9'))digits=false;
            int z=_ttoi(name);if(digits&&name.GetLength()<=2&&z>=0&&z<=24)set.levels.insert(z);else children.push_back(find.GetFilePath());
        }
        if(!set.levels.empty()){sets.push_back(set);return;}
        if(recursive)for(const auto& child:children)FindXyz(child,sets,rev,true);
    }
    void Mvt(Tile& tile,const CString& path,const MapSourceLayer& source,unsigned rev){
        auto ds = OpenVectorTile(path);
        if(!ds){Note(_T("MVT read failed: ")+path);return;}
        OGRSpatialReference wgs;wgs.SetWellKnownGeogCS("WGS84");wgs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        auto image=DS(GetGDALDriverManager()->GetDriverByName("MEM")->Create("",256,256,4,GDT_Byte,nullptr));if(!image)return;
        double gt[6]={tile.west,(tile.east-tile.west)/256,0,tile.north,0,(tile.south-tile.north)/256};image->SetGeoTransform(gt);image->SetSpatialRef(&wgs);
        for(int b=1;b<=4;++b)image->GetRasterBand(b)->Fill(0);
        vtstyle::Style* style=nullptr;
        if(!source.styleSld.empty()){auto& cached=vectorStyles[source.styleSld];if(!cached)cached=std::make_unique<vtstyle::Style>(source.styleSld);if(!cached->Valid()){Note(_T("Invalid SLD: ")+ST(source.styleSld));return;}style=cached.get();}
        std::unique_ptr<Gdiplus::Bitmap> bitmap;std::unique_ptr<Gdiplus::Graphics> graphics;
        if(style){bitmap=std::make_unique<Gdiplus::Bitmap>(256,256,PixelFormat32bppARGB);graphics=std::make_unique<Gdiplus::Graphics>(bitmap.get());graphics->Clear(Gdiplus::Color(0,0,0,0));graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);}
        double denominator=renderingStyleScale>0?renderingStyleScale:111320*cos((tile.north+tile.south)*0.5*3.141592653589793/180)*(tile.east-tile.west)/256*96/0.0254;
        for(int li=0;li<ds->GetLayerCount()&&!Cancel(rev);++li){auto layer=ds->GetLayer(li);if(!layer||!layer->GetSpatialRef()||(!source.layer.empty()&&source.layer!=layer->GetName()))continue;
            layer->ResetReading();
            auto sr=std::unique_ptr<OGRSpatialReference>(layer->GetSpatialRef()->Clone());sr->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            auto ct=std::unique_ptr<OGRCoordinateTransformation,decltype(&OCTDestroyCoordinateTransformation)>(OGRCreateCoordinateTransformation(sr.get(),&wgs),OCTDestroyCoordinateTransformation);if(!ct)continue;
            unsigned hash=0;for(const char* s=layer->GetName();*s;++s)hash=hash*31+(unsigned char)*s;
            int bands[]={1,2,3,4};double burn[]={double(70+hash%150),double(70+(hash/151)%150),double(70+(hash/22801)%150),210};char* opts[]={(char*)"ALL_TOUCHED=TRUE",nullptr};
            while(!Cancel(rev)){auto f=std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)>(layer->GetNextFeature(),OGRFeature::DestroyFeature);if(!f)break;
                if(tile.features.size()>=10000){Note(_T("MVT: tile feature limit 10000; zoom in."));break;}
                if(!f->GetGeometryRef())continue;auto g=std::shared_ptr<OGRGeometry>(f->GetGeometryRef()->clone());if(g->transform(ct.get())!=OGRERR_NONE)continue;
                OGREnvelope e;g->getEnvelope(&e);if(e.MaxX<tile.west||e.MinX>tile.east||e.MaxY<tile.south||e.MinY>tile.north)continue;
                if(style){if(!style->Draw(*graphics,f.get(),g.get(),layer->GetName(),denominator,tile.west,tile.north,tile.east,tile.south,&tile.symbols))continue;}else{OGRGeometryH h=g.get();if(GDALRasterizeGeometries(image.get(),4,bands,1,&h,nullptr,nullptr,burn,opts,nullptr,nullptr)!=CE_None)continue;}
                VectorFeature hit;hit.geometry=g;hit.title=source.name+" / "+layer->GetName();hit.fields.push_back("FID");hit.values.push_back(std::to_string(f->GetFID()));
                for(int j=0;j<f->GetFieldCount();++j){hit.fields.push_back(f->GetFieldDefnRef(j)->GetNameRef());hit.values.push_back(f->IsFieldSetAndNotNull(j)?f->GetFieldAsString(j):"NULL");}
                tile.features.push_back(std::move(hit));
            }
        }
        if(style&&!Cancel(rev)){graphics.reset();Gdiplus::BitmapData data;Gdiplus::Rect area(0,0,256,256);if(bitmap->LockBits(&area,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)==Gdiplus::Ok){std::vector<unsigned char> rgba(256*256*4);for(int y=0;y<256;++y){auto row=(unsigned char*)data.Scan0+y*data.Stride;for(int x=0;x<256;++x){auto d=&rgba[(y*256+x)*4];d[0]=row[x*4+2];d[1]=row[x*4+1];d[2]=row[x*4];d[3]=row[x*4+3];}}bitmap->UnlockBits(&data);int bands[]={1,2,3,4};if(image->RasterIO(GF_Write,0,0,256,256,rgba.data(),256,256,GDT_Byte,4,bands,4,1024,1,nullptr)!=CE_None)Note(_T("SLD image write failed"));}}
        if(!Cancel(rev))Blend(tile,image.get());
    }
    void Xyz(Tile& tile,size_t id,const MapSourceLayer& source,unsigned rev){
        if(!xyzIndexes.count(id)){
            std::vector<XyzSet> sets;FindXyz(ST(source.path),sets,rev,source.recursive);
            if(Cancel(rev))return;xyzIndexes[id]=std::move(sets);
            if(xyzIndexes[id].empty())Note(ST(source.name)+_T(": XYZ 줌 폴더가 없습니다."));
        }
        const double pi=3.141592653589793,world=20037508.342789244;
        double north=(std::min)(85.0511287798066,tile.north),south=(std::max)(-85.0511287798066,tile.south);
        if(north<=south)return;
        double latitude=(north+south)*0.5*pi/180;
        int wanted=(int)ceil(log2(360.0/(tile.east-tile.west)*(std::max)(0.02,cos(latitude))));
        for(const auto& set:xyzIndexes[id]){
            auto level=set.levels.lower_bound(wanted);if(level==set.levels.end())level=std::prev(set.levels.end());
            int z=*level,n=1<<z;double span=2*world/n;
            auto row=[&](double lat){double r=lat*pi/180;return (1-log(tan(r)+1/cos(r))/pi)*0.5*n;};
            int x0=(std::max)(0,(int)floor((tile.west+180)/360*n)),x1=(std::min)(n-1,(int)ceil((tile.east+180)/360*n)-1);
            int y0=(std::max)(0,(int)floor(row(north))),y1=(std::min)(n-1,(int)ceil(row(south))-1);
            // A sparse high-zoom-only pyramid must not block the worker indefinitely.
            if((long long)(x1-x0+1)*(y1-y0+1)>4096){Note(ST(source.name)+_T(": 저해상도 줌 타일을 추가하거나 확대하세요."));continue;}
            for(int y=y0;y<=y1&&!Cancel(rev);++y)for(int x=x0;x<=x1&&!Cancel(rev);++x){
                CString base;base.Format(_T("%s/%d/%d/%d"),set.root.GetString(),z,x,y);
                if(source.type=="VectorTile"){CString path=base+_T(".pbf");if(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES)path=base+_T(".mvt");if(GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES)Mvt(tile,path,source,rev);continue;}
                CString path;for(auto ext:{_T(".png"),_T(".jpg"),_T(".jpeg")}){CString candidate=base+ext;if(GetFileAttributes(candidate)!=INVALID_FILE_ATTRIBUTES){path=candidate;break;}}
                if(path.IsEmpty())continue;
                auto input=DS((GDALDataset*)GDALOpenEx(SU(path).c_str(),GDAL_OF_RASTER|GDAL_OF_READONLY,nullptr,nullptr,nullptr));if(!input)continue;
                std::vector<std::string> args={"-of","VRT","-a_srs","EPSG:3857","-a_ullr",Number(-world+x*span),Number(world-y*span),Number(-world+(x+1)*span),Number(world-(y+1)*span)};
                std::vector<char*> argv;for(auto&arg:args)argv.push_back(&arg[0]);argv.push_back(nullptr);
                auto options=GDALTranslateOptionsNew(argv.data(),nullptr);
                auto referenced=DS((GDALDataset*)GDALTranslate("",input.get(),options,nullptr));GDALTranslateOptionsFree(options);
                if(referenced)Warp(tile,referenced.get(),source,rev);
            }
        }
    }
    void Blend(Tile& tile,GDALDataset* image)
    {
        int n=image->GetRasterCount();
        if(n<1)
            return;
        std::vector<unsigned char> rgba(256*256*4,255);
        auto band=image->GetRasterBand(1);
        auto ct=band->GetColorTable();
        if(n==1  ||n==2)
        {
            std::vector<unsigned char> grey(256*256);
            if(band->RasterIO(GF_Read,0,0,256,256,grey.data(),256,256,GDT_Byte,0,0)!=CE_None)
                return;
            for(size_t i=0;i<grey.size();++i)
            {
                auto color=ct?ct->GetColorEntry(grey[i]):nullptr;
                rgba[i*4]=color?color->c1:grey[i];
                rgba[i*4+1]=color?color->c2:grey[i];
                rgba[i*4+2]=color?color->c3:grey[i];
                rgba[i*4+3]=color?color->c4:255;
            }
            if(n==2&&image->GetRasterBand(2)->RasterIO(GF_Read,0,0,256,256,rgba.data()+3,256,256,GDT_Byte,4,256*4)!=CE_None)
                return;
        }
        else
        {
            int bands[4]={1,2,3,4};
            if(image->RasterIO(GF_Read,0,0,256,256,rgba.data(),256,256,GDT_Byte,(std::min)(4,n),bands,4,256*4,1)!=CE_None)
                return;
        }
        for(size_t i=0; i<256*256; ++i)
        {
            unsigned a=rgba[i*4+3],inv=255-a;
            for(int j=0; j<3; ++j)
                tile.bgra[i*4+j]=(unsigned char)((rgba[i*4+2-j]*a+tile.bgra[i*4+j]*inv)/255);
            tile.bgra[i*4+3]=(unsigned char)(a+tile.bgra[i*4+3]*inv/255);
        }
    }
    void Warp(Tile& tile,GDALDataset* input,const MapSourceLayer& source,unsigned rev)
    {
        std::vector<std::string> args={"-of","MEM","-t_srs","EPSG:4326","-te",Number(tile.west),Number(tile.south),
            Number(tile.east),Number(tile.north),"-ts","256","256","-r","near","-dstalpha","-wo","INIT_DEST=NO_DATA","-wm","32"};
        if(!strlen(input->GetProjectionRef())&&!source.srs.empty())
        {
            args.push_back("-s_srs");
            args.push_back(source.srs);
        }
        char** options=nullptr;
        for(const auto& a:args)
            options=CSLAddString(options,a.c_str());
        auto opts=GDALWarpAppOptionsNew(options,nullptr);
        CSLDestroy(options);
        std::pair<Impl*,unsigned> callback{this,rev};
        GDALWarpAppOptionsSetProgress(opts,Progress,&callback);
        GDALDatasetH handle=input;
        int usage=0;auto warped=DS((GDALDataset*)GDALWarp("",nullptr,1,&handle,opts,&usage));
        GDALWarpAppOptionsFree(opts);
        if(warped&&!Cancel(rev))Blend(tile,warped.get());
        else if(!warped)
            Note(_T("타일 좌표 변환 실패: ")+ST(CPLGetLastErrorMsg()));
    }
    void Wfs(Tile& tile,const MapSourceLayer& source,unsigned rev)
    {
#if C4MAP_ENABLE_OGC
        auto& ds = wfsConnections.try_emplace(source.endpoint, DS(nullptr)).first->second;
        if (!ds)
            ds = DS(static_cast<GDALDataset*>(GDALOpenEx(
                ("WFS:" + source.endpoint).c_str(),
                GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)));
        if(!ds)
        {
            Note(_T("WFS 연결 실패: 서버/레이어 설정을 확인하세요."));
            return;
        }
        auto layer=ds->GetLayerByName(source.layer.c_str());
        if(!layer)
        {
            Note(_T("WFS 레이어를 찾을 수 없습니다."));
            return;
        }
        OGRSpatialReference wgs;
        wgs.SetWellKnownGeogCS("WGS84");
        wgs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        auto sr=layer->GetSpatialRef();
        if(!sr)
        {
            Note(_T("WFS 좌표계가 없습니다."));
            return;
        }
        auto native=std::unique_ptr<OGRSpatialReference>(sr->Clone());
        native->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        auto forward=OGRCreateCoordinateTransformation(native.get(),&wgs);
        auto inverse=OGRCreateCoordinateTransformation(&wgs,native.get());
        if(!forward||!inverse)
        {
            if(forward)
                OCTDestroyCoordinateTransformation(forward);
            if(inverse)
                OCTDestroyCoordinateTransformation(inverse);
            return;
        }
        double xs[4]={tile.west,tile.east,tile.east,tile.west},ys[4]={tile.south,tile.south,tile.north,tile.north};
        if(!inverse->Transform(4,xs,ys))
        {
            OCTDestroyCoordinateTransformation(forward);
            OCTDestroyCoordinateTransformation(inverse);
            return;
        }
        layer->SetSpatialFilterRect(*std::min_element(xs,xs+4),*std::min_element(ys,ys+4),*std::max_element(xs,xs+4),*std::max_element(ys,ys+4));
        auto image=DS(GetGDALDriverManager()->GetDriverByName("MEM")->Create("",256,256,4,GDT_Byte,nullptr));
        double gt[6]={tile.west,(tile.east-tile.west)/256,0,tile.north,0,(tile.south-tile.north)/256};
        image->SetGeoTransform(gt);
        image->SetProjection(SRS_WKT_WGS84_LAT_LONG);
        for(int i=1;i<=4;++i)
            image->GetRasterBand(i)->Fill(0);
        int bands[4]={1,2,3,4};
        std::vector<OGRGeometryH> geometries;
        std::vector<double> burns;
        char* opts[]={(char*)"ALL_TOUCHED=TRUE",nullptr};
        int count=0;
        layer->ResetReading();
        while(!Cancel(rev))
        {
            auto feature=std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)>(layer->GetNextFeature(),OGRFeature::DestroyFeature);
            if(!feature)
                break;
            if(++count>10000)
            {
                Note(_T("WFS 타일 객체 수 10,000개 제한: 확대해서 확인하세요."));
                break;
            }
            auto g=feature->GetGeometryRef();
            if(!g)
                continue;
            auto copy=std::unique_ptr<OGRGeometry>(g->clone());
            if(copy->transform(forward)!=OGRERR_NONE)
                continue;
            geometries.push_back(copy.get());
            burns.insert(burns.end(), {40,190,255,255});
            VectorFeature hit;
            hit.geometry=std::shared_ptr<OGRGeometry>(copy.release());
            hit.title=source.name+" / "+source.layer;
            for(int i=0;i<feature->GetFieldCount();++i)
            {
                hit.fields.push_back(feature->GetFieldDefnRef(i)->GetNameRef());
                hit.values.push_back(feature->IsFieldSetAndNotNull(i)?feature->GetFieldAsString(i):"NULL");
            }
            tile.features.push_back(std::move(hit));
        }
        OCTDestroyCoordinateTransformation(forward);
        OCTDestroyCoordinateTransformation(inverse);
        if (!Cancel(rev) && !geometries.empty())
        {
            std::pair<Impl*,unsigned> callback{this, rev};
            GDALRasterizeGeometries(image.get(), 4, bands,
                static_cast<int>(geometries.size()), geometries.data(),
                nullptr, nullptr, burns.data(), opts, Progress, &callback);
        }
        if (!Cancel(rev))
            Blend(tile, image.get());
#endif
    }
    std::shared_ptr<Tile> Render(size_t id,const MapSourceLayer& source,TileKey key,unsigned rev)
    {
        auto tile=std::make_shared<Tile>();
        tile->sourceId = id;
        double step=180.0/(1<<key.z);
        tile->west=-180+key.x*step;
        tile->east=tile->west+step;
        tile->north=90-key.y*step;
        tile->south=tile->north-step;
        tile->bgra.assign(256*256*4,0);
        if(source.type=="VectorTile"){Xyz(*tile,id,source,rev);return tile;}
        if(source.type=="WFS")
        {
            Wfs(*tile,source,rev);
            return tile;
        }
        if(source.type=="WMS"||source.type=="WMTS")
        {
#if C4MAP_ENABLE_OGC
            if(!remote.count(id))
            {
                auto ds=DS((GDALDataset*)GDALOpenEx(Connection(source).c_str(),GDAL_OF_RASTER|GDAL_OF_READONLY,nullptr,nullptr,nullptr));
                if(!ds)
                {
                    Note(_T("지도 서비스 연결 실패: 주소/레이어/행렬 설정을 확인하세요."));
                    return tile;
                }
                remote.emplace(id,std::move(ds));
            }
            Warp(*tile,remote.at(id).get(),source,rev);
#endif
            return tile;
        }
#if C4MAP_ENABLE_LOCAL
        if(source.tileScheme=="XYZ"){Xyz(*tile,id,source,rev);return tile;}
        if(!indexes.count(id))
        {
            Note(ST(source.name)+_T(": 하위 폴더 지도 인덱스 작성 중"));
            std::vector<RasterFile> files;
            std::set<std::string> visited;
            Scan(source,ST(source.path),files,rev,visited);
            if(Cancel(rev))
                return tile;
            indexes[id]=std::move(files);
            if(indexes[id].empty())
                Note(ST(source.name)+_T(": 읽을 수 있는 지리참조 영상이 없습니다."));
        }
        for(const auto& file:indexes[id])
        {
            if(Cancel(rev))
                break;
            if(file.east<tile->west||file.west>tile->east||file.north<tile->south||file.south>tile->north)
                continue;
            auto ds=DS((GDALDataset*)GDALOpenEx(file.path.c_str(),GDAL_OF_RASTER|GDAL_OF_READONLY,nullptr,nullptr,nullptr));
            if(ds)
                Warp(*tile,ds.get(),source,rev);
        }
#endif
        return tile;
    }
    void Run()
    {
        CPLSetThreadLocalConfigOption("GDAL_HTTP_TIMEOUT","8");
        CPLSetThreadLocalConfigOption("GDAL_HTTP_CONNECTTIMEOUT","5");
        CPLSetThreadLocalConfigOption("GDAL_HTTP_MAX_RETRY","0");
        CPLSetThreadLocalConfigOption("GDAL_MAX_CONNECTIONS","4");
        CPLSetThreadLocalConfigOption("OGR_WFS_PAGING_ALLOWED","ON");
        CPLSetThreadLocalConfigOption("OGR_WFS_PAGE_SIZE","1000");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        unsigned generation=~0u;
        for(;;)
        {
            Request r;std::vector<MapSourceLayer> sources;
            unsigned rev,dataRev;
            {
                std::unique_lock<std::mutex> l(mutex);
                cv.wait(l,[&]{return stop||pending;});
                if(stop)
                    break;
                // Coalesce rapid pan/zoom updates before issuing network requests.
                auto observed = ticket.load();
                while (!stop && cv.wait_for(l, std::chrono::milliseconds(80),
                    [&] { return stop || ticket.load() != observed; }))
                    observed = ticket.load();
                if (stop)
                    break;
                pending=false;
                busy=true;
                r=request;
                sources=layers;
                rev=revision;
                dataRev=dataRevision;
                activeTicket=ticket;
            }
            if(dataRev!=generation)
            {
                cache.clear();
                vectorDatasets.clear();
                indexes.clear();xyzIndexes.clear();vectorStyles.clear();
                remote.clear();
                wfsConnections.clear();
                generation=dataRev;
            }
            if(r.styleScale > 0 && renderingStyleScale!=r.styleScale){for(auto it=cache.begin();it!=cache.end();){auto id=std::get<0>(it->first);if(id<sources.size()&&!sources[id].styleSld.empty())it=cache.erase(it);else ++it;}renderingStyleScale=r.styleScale;}
            const size_t imageCacheLimit = (std::min)(size_t(1024),
                (std::max)(size_t(256), r.tiles.size() * r.sources.size()));
            // Work from the center out without changing request identity/order.
            auto orderedTiles = r.tiles;
            if (!orderedTiles.empty())
            {
                auto center = orderedTiles[orderedTiles.size() / 2];
                center.x = (orderedTiles.front().x + orderedTiles.back().x) / 2;
                center.y = (orderedTiles.front().y + orderedTiles.back().y) / 2;
                std::stable_sort(orderedTiles.begin(), orderedTiles.end(),
                    [center](const TileKey& a, const TileKey& b)
                    {
                        auto distance = [center](const TileKey& key)
                        {
                            const double dx = double(key.x) - center.x;
                            const double dy = double(key.y) - center.y;
                            return dx * dx + dy * dy;
                        };
                        return distance(a) < distance(b);
                    });
            }
            auto lastPublish = std::chrono::steady_clock::now();
            std::vector<std::shared_ptr<Tile>> result;
            std::set<size_t> completedSources;
            for(auto id:r.sources)
            {
                if(id>=sources.size())
                    continue;
                for(auto key:orderedTiles)
                {
                    if(Cancel(rev)) break;
                    auto k=std::make_tuple(id,key.z,key.x,key.y);
                    auto found=cache.find(k);
                    std::shared_ptr<Tile> tile=found==cache.end()?Render(id,sources[id],key,rev):found->second;
                    if (Cancel(rev))
                        break;

                    // Empty vector cells (including symbol-only cells) need no bitmap.
                    // Keep their hit geometry and screen symbols, but release pixel memory.
                    if (found == cache.end() && sources[id].type == "VectorTile")
                    {
                        bool hasPixels = false;
                        for (size_t alpha = 3; alpha < tile->bgra.size(); alpha += 4)
                        {
                            if (tile->bgra[alpha] != 0)
                            {
                                hasPixels = true;
                                break;
                            }
                        }
                        if (!hasPixels)
                            std::vector<unsigned char>().swap(tile->bgra);
                    }

                    tile->lastUse=++cacheClock;
                    if(found==cache.end()){
                        if(cache.size()>=imageCacheLimit){
                            auto oldest=cache.begin();
                            for(auto it=cache.begin();it!=cache.end();++it)
                                if(it->second->lastUse<oldest->second->lastUse)oldest=it;
                            cache.erase(oldest);
                        }
                        cache[k]=tile;
                    }
                    result.push_back(tile);
                    const auto now = std::chrono::steady_clock::now();
                    if (now - lastPublish >= std::chrono::milliseconds(100))
                    {
                        // Keep old coverage until its replacement is ready. Always
                        // composite in layer order, even while a layer is incomplete.
                        std::lock_guard<std::mutex> lock(mutex);
                        if (!Cancel(rev) && request == r)
                        {
                            using Region = std::tuple<size_t,double,double,double,double>;
                            auto region = [](const std::shared_ptr<Tile>& t)
                            {
                                return Region(t->sourceId,t->west,t->north,t->east,t->south);
                            };
                            std::set<Region> replaced;
                            for (const auto& t : result)
                                replaced.insert(region(t));
                            std::vector<std::shared_ptr<Tile>> progressive;
                            for (auto sourceId : r.sources)
                            {
                                for (const auto& t : ready)
                                    if (t->sourceId == sourceId && !replaced.count(region(t)))
                                        progressive.push_back(t);
                                for (const auto& t : result)
                                    if (t->sourceId == sourceId)
                                        progressive.push_back(t);
                            }
                            ready.swap(progressive);
                            changed = true;
                        }
                        lastPublish = now;
                    }
                }
                if (Cancel(rev))
                    break;

                // Publish complete layers while preserving the old image of layers
                // still being prepared. This keeps layer order and avoids blank flashes.
                completedSources.insert(id);
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (!Cancel(rev) && request == r)
                    {
                        std::vector<std::shared_ptr<Tile>> progressive;
                        for (auto sourceId : r.sources)
                        {
                            const auto& candidates = completedSources.count(sourceId) ? result : ready;
                            for (const auto& tile : candidates)
                            {
                                if (tile->sourceId == sourceId)
                                    progressive.push_back(tile);
                            }
                        }
                        ready.swap(progressive);
                        changed = true;
                    }
                }
            }
            {
                std::lock_guard<std::mutex> l(mutex);
                if(!Cancel(rev)&&request==r)
                {
                    ready.swap(result);
                    changed=true;
                }
                busy=false;
                cv.notify_all();
            }
        }
        vectorDatasets.clear();
        wfsConnections.clear();
        remote.clear();vectorStyles.clear();CPLPopErrorHandler();
    }
};

CMapSources& MapSources(){static CMapSources source;return source;}
CMapSources::CMapSources():p(new Impl){GDALAllRegister();}
CMapSources::~CMapSources(){delete p;}

std::vector<MapSourceLayer> CMapSources::Layers()const
{
    std::lock_guard<std::mutex> l(p->mutex);
    return p->layers;
}

CString CMapSources::Path()const
{
    std::lock_guard<std::mutex> l(p->mutex);
    return p->xml;
}

CString CMapSources::Status()const
{
    std::lock_guard<std::mutex> l(p->mutex);
    return p->status;
}

void CMapSources::SetVisible(size_t i, bool visible)
{
    std::lock_guard<std::mutex> lock(p->mutex);
    if (i >= p->layers.size() || p->layers[i].visual == visible)
        return;

    p->identify.Cancel();
    p->layers[i].visual = visible;
    ++p->revision; // Cancel only work belonging to the previous selection.
    if (!visible)
    {
        // Keep every other layer's prepared image and hit-test geometry intact.
        p->ready.erase(std::remove_if(p->ready.begin(), p->ready.end(),
            [i](const std::shared_ptr<Tile>& tile)
            {
                return tile->sourceId == i;
            }), p->ready.end());
    }
    p->request = Request();
    p->changed = true;
}

void CMapSources::ResetCache()
{
    p->identify.Cancel();
    std::lock_guard<std::mutex> l(p->mutex);
    ++p->revision;
    ++p->dataRevision;
    p->request=Request();
    p->ready.clear();
    p->changed=true;
}

bool CMapSources::Poll()
{
    std::lock_guard<std::mutex> l(p->mutex);
    bool v=p->changed;p->changed=false;
    return v;
}

bool CMapSources::WaitForIdle(unsigned ms)
{
    std::unique_lock<std::mutex> l(p->mutex);
    return p->cv.wait_for(l,std::chrono::milliseconds(ms),[&]{return !p->busy&&!p->pending;});
}

bool CMapSources::Load(const CString& xml,CString& error)
{
    p->identify.Cancel();
    try{
        auto bytes=ReadUtf8(xml);
        std::unique_ptr<CPLXMLNode,decltype(&CPLDestroyXMLNode)> doc(CPLParseXMLString(bytes.c_str()),CPLDestroyXMLNode);
        CPLXMLNode* root=nullptr;
        for(auto n=doc.get();n;n=n->psNext)
            if(n->eType==CXT_Element&&!strcmp(n->pszValue,"MapSources"))
                root=n;
        if(!root||SV(root,"version")!="1")
            throw std::runtime_error("MapSources version=1 XML required");
        std::set<std::string> ids;std::vector<MapSourceLayer> layers;
        for(auto n=root->psChild;n;n=n->psNext)
        {
            if(n->eType!=CXT_Element)
                continue;
            if(strcmp(n->pszValue,"Layer"))
                throw std::runtime_error("Unknown catalog element");
            MapSourceLayer s;
            s.id=SV(n,"id");
            s.name=SV(n,"name");
            s.type=SV(n,"type");
            s.family=SV(n,"family");s.category=SV(n,"Category");
            s.path=SV(n,"Path");
            s.styleSld=SV(n,"StyleSLD");s.localStyleXml=SV(n,"LocalStyleXML");s.localLayerName=SV(n,"LocalLayerName");
            s.tileScheme=SV(n,"TileScheme");
            if(!s.tileScheme.empty()&&s.tileScheme!="XYZ")throw std::runtime_error("TileScheme must be XYZ or empty");
            s.extensions=Lower(SV(n,"Extensions","tif;tiff;img;vrt;tl2;tc2;i42;i21;i22;i41;ntf;nitf"));
            s.identifyType=SV(n,"IdentifyType");
            s.identifyEndpoint=SV(n,"IdentifyEndpoint");
            s.identifyLayer=SV(n,"IdentifyLayer");
            if(!s.identifyType.empty() && s.identifyType!="WMS" && s.identifyType!="WFS")
                throw std::runtime_error("IdentifyType must be WMS or WFS");
            if(!s.identifyEndpoint.empty() && s.identifyEndpoint.rfind("http://",0)!=0 && s.identifyEndpoint.rfind("https://",0)!=0)
                throw std::runtime_error("IdentifyEndpoint must be HTTP(S)");
            s.endpoint=SV(n,"Endpoint");
            s.layer=SV(n,"LayerName");
            s.matrix=SV(n,"TileMatrixSet");
            s.style=SV(n,"Style");
            s.format=SV(n,"Format","image/png");
            s.version=SV(n,"Version","1.1.1");
            s.srs=SV(n,"SRS","");
            auto number=[&](const char* key)
            {
                std::string v=SV(n,key,"0");
                std::istringstream in(v);
                in.imbue(std::locale::classic());
                double d;
                if(!(in>>d)||in.peek()!=EOF||!std::isfinite(d)||d<0)
                    throw std::runtime_error("Invalid scale");
                return d;
            };
            s.scale=number("Scale");
            s.minScale=number("MinScale");
            s.maxScale=number("MaxScale");
            if(s.maxScale&&s.maxScale<s.minScale)
                throw std::runtime_error("Scale range reversed");
            auto boolean=[&](const char* key,const char* def)
            {
                auto v=Lower(SV(n,key,def));
                if(v!="true"&&v!="false")
                    throw std::runtime_error("Expected true or false");
                return v=="true";
            };
            s.visual=boolean("visual","false");
            s.recursive=boolean("Recursive","true");
            if(s.id.empty() || s.name.empty())
                throw std::runtime_error("Layer IDs and names must be nonempty");
            if(!ids.insert(s.id).second){
                const auto original=s.id;
                for(size_t suffix=2;;++suffix){
                    auto candidate=original+"__duplicate_"+std::to_string(suffix);
                    bool reserved=false;
                    for(auto sibling=n->psNext;sibling;sibling=sibling->psNext)
                        if(sibling->eType==CXT_Element && SV(sibling,"id")==candidate){reserved=true;break;}
                    if(!reserved && ids.insert(candidate).second){s.id=candidate;break;}
                }
            }
            if(s.type!="Raster" && s.type!="CIB" && s.type!="WMS" && s.type!="WFS" && s.type!="WMTS" && s.type!="VectorTile")
                throw std::runtime_error("Unknown layer type");
            if((s.type=="Raster" || s.type=="CIB" || s.type=="VectorTile") && s.path.empty())
                throw std::runtime_error("Local layer Path required");
            if(s.type=="Raster" || s.type=="CIB" || s.type=="VectorTile")
            {
                if(s.path.find(':')==std::string::npos&&s.path.substr(0,2)!="\\\\")
                {
                    auto base=xml.Left(xml.ReverseFind('\\')+1);
                    if(base.IsEmpty())
                        base=xml.Left(xml.ReverseFind('/')+1);
                    s.path=SU(base)+s.path;
                }
            }
            else if(s.visual && (s.endpoint.empty() || s.layer.empty()))
                throw std::runtime_error("Enabled service needs Endpoint and LayerName");
            if(!s.endpoint.empty() && s.endpoint.rfind("http://",0)!=0 && s.endpoint.rfind("https://",0)!=0)
                throw std::runtime_error("Service endpoint must be HTTP(S)");
            layers.push_back(s);
            if(layers.size()>512)
                throw std::runtime_error("Too many layers");
        }
        std::lock_guard<std::mutex> l(p->mutex);
        ++p->revision;
        ++p->dataRevision;
        p->layers.swap(layers);
        p->xml=xml;
        p->request=Request();
        p->ready.clear();
        p->status=_T("XML 지도 정의를 불러왔습니다.");
        p->changed=true;
        return true;
    }
    catch(CFileException* e)
    {
        TCHAR text[512];
        e->GetErrorMessage(text,512);
        error=text;
        e->Delete();
        return false;
    }
    catch(const std::exception& e)
    {
        error=ST(e.what());
        return false;
    }
}

bool CMapSources::Save(const CString& xml,const std::vector<MapSourceLayer>& layers,CString& error)
{
    try
    {
        std::string s="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<MapSources version=\"1\">\n";
        for(const auto& l:layers)
        {
            s+="  <Layer id=\""+SX(l.id)+"\" name=\""+SX(l.name)+"\" type=\""+SX(l.type)+"\" family=\""+SX(l.family)+"\">\n";
            auto add=[&](const char* n,const std::string& v)
            {
                s+="    <"+std::string(n)+">"+SX(v)+"</"+n+">\n";
            };
            add("visual",l.visual?"true":"false");
            add("Path",l.path);
            if(!l.tileScheme.empty())add("TileScheme",l.tileScheme);
            if(!l.styleSld.empty())add("StyleSLD",l.styleSld);
            if(!l.category.empty())add("Category",l.category);
            if(!l.localStyleXml.empty())add("LocalStyleXML",l.localStyleXml);
            if(!l.localLayerName.empty())add("LocalLayerName",l.localLayerName);
            add("Extensions",l.extensions);
            add("Recursive",l.recursive?"true":"false");
            add("Scale",Number(l.scale));
            add("MinScale",Number(l.minScale));
            add("MaxScale",Number(l.maxScale));
            add("SRS",l.srs);
            add("IdentifyType",l.identifyType);
            add("IdentifyEndpoint",l.identifyEndpoint);
            add("IdentifyLayer",l.identifyLayer);
            add("Endpoint",l.endpoint);
            add("LayerName",l.layer);
            add("Version",l.version);
            add("Style",l.style);
            add("Format",l.format);
            add("TileMatrixSet",l.matrix);s+="  </Layer>\n";}s+="</MapSources>\n";
            AtomicWrite(xml,s);
            return true;
    }
    catch(CFileException* e)
    {
        TCHAR text[512];
        e->GetErrorMessage(text,512);
        error=text;
        e->Delete();
        return false;
    }
    catch(const std::exception& e)
    {
        error=ST(e.what());
        return false;
    }
}

bool CMapSources::Identify(CPoint point,CCoordMgr* c,CString& title,std::vector<CString>& fields,std::vector<CString>& values){
    p->identify.Cancel();
    if(!c||c->m_fScale<=0)return false;
    auto lonlat=[&](int x,int y,double& lon,double& lat){if(c->m_bFlat){lon=127.031+(x-c->m_fCenterX)/(47.5*c->m_fScale);lat=37.0861+(c->m_fCenterY-y)/(60*c->m_fScale);}else{double wx,wy;c->ScreenXY2WorldXY(x,y,&wx,&wy,c->m_fCenterX,c->m_fCenterY,c->m_fScale);CCoord conv;conv.x_y_to_degrees_lat_long(wx,wy,&lat,&lon);}};
    double lon,lat;lonlat(point.x,point.y,lon,lat);if(!std::isfinite(lon)||!std::isfinite(lat))return false;lon=fmod(lon+540,360)-180;
    double dx=0,dy=0;for(auto offset:{CPoint(7,0),CPoint(-7,0),CPoint(0,7),CPoint(0,-7)}){double a,b;lonlat(point.x+offset.x,point.y+offset.y,a,b);dx=(std::max)(dx,fabs(remainder(a-lon,360)));dy=(std::max)(dy,fabs(b-lat));}
    OGRLinearRing ring;ring.addPoint(lon-dx,lat-dy);ring.addPoint(lon+dx,lat-dy);ring.addPoint(lon+dx,lat+dy);ring.addPoint(lon-dx,lat+dy);ring.addPoint(lon-dx,lat-dy);OGRPolygon box;box.addRing(&ring);
    std::vector<std::shared_ptr<Tile>> ready;{std::lock_guard<std::mutex> lock(p->mutex);ready=p->ready;}
    for(auto t=ready.rbegin();t!=ready.rend();++t){if(lon+dx<(*t)->west||lon-dx>(*t)->east||lat+dy<(*t)->south||lat-dy>(*t)->north)continue;
        for(auto f=(*t)->features.rbegin();f!=(*t)->features.rend();++f)if(f->geometry->Intersects(&box)){title=ST(f->title);fields.clear();values.clear();for(auto& v:f->fields)fields.push_back(ST(v));for(auto& v:f->values)values.push_back(ST(v));return true;}}
#if C4MAP_ENABLE_OGC
    std::vector<MapSourceLayer> candidates;
    {
        std::lock_guard<std::mutex> lock(p->mutex);
        // Query only layers currently eligible for rendering, including scale limits.
        for(auto id:p->request.sources)
        {
            if(id>=p->layers.size())continue;
            auto source=p->layers[id];
            if(!source.visual || !Allowed(source))continue;
            if(source.type=="WMS" || source.type=="WFS" ||
                (source.type=="WMTS" && !source.identifyType.empty() && !source.identifyEndpoint.empty()))
                candidates.push_back(std::move(source));
        }
    }
    if(!candidates.empty())
        p->identify.Start(std::move(candidates),lon-dx,lat-dy,lon+dx,lat+dy);
#endif
    return false;
}
bool CMapSources::IdentifyPending() const { return p->identify.Pending(); }
void CMapSources::CancelIdentify() { p->identify.Cancel(); }
bool CMapSources::PollIdentify(CString& title,std::vector<CString>& fields,std::vector<CString>& values)
{
    RemoteMapHit hit;
    if(!p->identify.Poll(hit))return false;
    title=ST(hit.title);fields.clear();values.clear();
    for(const auto& field:hit.fields)fields.push_back(ST(field));
    for(const auto& value:hit.values)values.push_back(ST(value));
    return true;
}
bool CMapSources::Enabled(const MapSourceLayer& l){return Allowed(l);}

void CMapSources::Draw(CDC* dc,CCoordMgr* c)
{
    if(!dc||!c||!c->m_bFlat||c->m_fScale<=0)
        return;
    double lon0 = 127.031 - c->m_fCenterX / (47.5 * c->m_fScale);
    double lat0 = 37.0861 + c->m_fCenterY / (60 * c->m_fScale);
    double lon1 = lon0 + c->m_nDisplayViewWidth / (47.5 * c->m_fScale);
    double lat1 = lat0 - c->m_nDisplayViewHeight / (60 * c->m_fScale);

    Request r;
    r.z=(std::max)(0, (std::min)(20,(int)std::ceil(log2(180*60.0*c->m_fScale/256))));
    double step=180.0/(1<<r.z);
    int nx = 2 << r.z;
    int ny = 1 << r.z;
    int a = (int)floor((lon0 + 180) / step);
    int b = (int)floor((lon1 + 180) / step);
    int top = (std::max)(0, (int)floor((90 - lat0) / step));
    int bottom = (std::min)(ny - 1, (int)floor((90 - lat1) / step));
    for(int y=top; y<=bottom; ++y)
        for(int x=a; x<=b; ++x)
        {
            int xx=(x%nx+nx)%nx;
            r.tiles.push_back({r.z,xx,y});
        }
    // Exceptionally large viewports still render; normal viewports need far fewer cells.
    if (r.tiles.size() > 4096)
        return;
    double latitude = (std::max)(-85.0, (std::min)(85.0, (lat0 + lat1) / 2));
    double denominator = 111320 * cos(latitude * 3.141592653589793 / 180) / (47.5 * c->m_fScale) * dc->GetDeviceCaps(LOGPIXELSX) / 0.0254;
    std::vector<std::shared_ptr<Tile>> ready;
    {
        std::lock_guard<std::mutex> l(p->mutex);
        std::map<std::string,std::pair<double,size_t>> best;
        for(size_t i=0;i<p->layers.size();++i)
        {
            const auto& source=p->layers[i];
            if(!Allowed(source) || !source.visual || (source.minScale&&denominator<source.minScale)||
                (source.maxScale&&denominator>source.maxScale))
                continue;
            if(source.type=="Raster" && source.scale>0)
            {
                auto key=source.type+source.family;
                double score=fabs(log(denominator/source.scale));
                if(!best.count(key) || score<best[key].first)
                    best[key]={score,i};
            }
            else 
                r.sources.push_back(i);
        }
        for(const auto& v:best)
            r.sources.push_back(v.second.second);
        std::sort(r.sources.begin(),r.sources.end());
        for(auto id:r.sources)if(!p->layers[id].styleSld.empty()){r.styleScale=std::round(denominator);break;}
        // Layer count must not reduce vector geometry or raster resolution.
        // Keep the pixel-sized grid and bound retained caches instead.
        if(!(p->request==r))
        {
            // Preserve georeferenced tiles during pan/zoom until the replacement is ready.
            // Layer/scale selection changes must not keep now-hidden sources visible.
            if (p->request.sources != r.sources)
            {
                // Selection/scale changes remove only ineligible layers. Cached
                // tiles for the remaining layers stay visible while work runs.
                p->ready.erase(std::remove_if(p->ready.begin(), p->ready.end(),
                    [&](const std::shared_ptr<Tile>& tile)
                    {
                        return !std::binary_search(r.sources.begin(), r.sources.end(), tile->sourceId);
                    }), p->ready.end());
            }
            p->request=r;
            ++p->ticket;
            p->pending=true;
            p->cv.notify_one();
        }
        ready=p->ready;
    }
    for(const auto& tile:ready)
    {
        if (tile->bgra.empty())
            continue;

        if(!tile->displayImage){
            auto image=std::make_unique<CImage>();
            if(!image->Create(256,256,32,CImage::createAlphaChannel))continue;
            for(int y=0;y<256;++y)
                memcpy(image->GetPixelAddress(0,y),tile->bgra.data()+y*256*4,256*4);
            tile->displayImage=std::move(image);
        }
        CImage& img=*tile->displayImage;
        int first = (int)floor((lon0 - tile->east) / 360);
        int last = (int)ceil((lon1 - tile->west) / 360);
        for(int copy=first; copy<=last; ++copy)
        {
            double x0,y0,x1,y1;
            c->WorldLonLat2ScreenXY(tile->west+360*copy,tile->north,&x0,&y0,c->m_fCenterX,c->m_fCenterY,c->m_fScale);
            c->WorldLonLat2ScreenXY(tile->east+360*copy,tile->south,&x1,&y1,c->m_fCenterX,c->m_fCenterY,c->m_fScale);
            if(x1<0 || x0>c->m_nDisplayViewWidth)
                continue;
            img.AlphaBlend(dc->GetSafeHdc(),(int)floor(x0),(int)floor(y0),(int)ceil(x1)-(int)floor(x0),(int)ceil(y1)-(int)floor(y0),0,0,256,256);
        }
    }
    // GSS point symbols are composed after the map, at screen-pixel size.
    struct SymbolImages {
        ULONG_PTR token=0;std::map<std::string,std::unique_ptr<Gdiplus::Image>> images;
        SymbolImages(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}
        ~SymbolImages(){images.clear();if(token)Gdiplus::GdiplusShutdown(token);}
    };
    static thread_local SymbolImages symbolImages;
    Gdiplus::Graphics graphics(dc->GetSafeHdc());
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    std::set<std::tuple<std::string,long long,long long>> drawn;
    for(const auto& tile:ready)for(const auto& symbol:tile->symbols){
        if(!drawn.emplace(symbol.path,llround(symbol.lon*1e7),llround(symbol.lat*1e7)).second)continue;
        auto& image=symbolImages.images[symbol.path];
        if(!image)image.reset(Gdiplus::Image::FromFile(vtstyle::Wide(symbol.path).c_str()));
        if(!image||image->GetLastStatus()!=Gdiplus::Ok)continue;
        int first=(int)floor((lon0-symbol.lon)/360),last=(int)ceil((lon1-symbol.lon)/360);
        for(int copy=first;copy<=last;++copy){double x,y;
            c->WorldLonLat2ScreenXY(symbol.lon+360*copy,symbol.lat,&x,&y,c->m_fCenterX,c->m_fCenterY,c->m_fScale);
            if(x+symbol.width/2<0||y+symbol.height/2<0||x-symbol.width/2>c->m_nDisplayViewWidth||y-symbol.height/2>c->m_nDisplayViewHeight)continue;
            graphics.DrawImage(image.get(),(float)x-symbol.width/2,(float)y-symbol.height/2,symbol.width,symbol.height);
        }
    }

}
#pragma pack(pop)

#pragma pack(push,8)
namespace 
{
class SourceDialog:public CDialog 
{
    CMapSources& owner;std::vector<MapSourceLayer> data;
    std::vector<std::unique_ptr<CWnd>> controls;
    CListBox list;
    CFont font;
    std::vector<size_t> rows;
    int selected=-1;

    enum
    {
        LIST=7101,
        OPEN=7102,
        SAVE=7103,
        APPLY=7104,
        ADD=7105,
        RELOAD=7106,
        REFRESH=7107
    };

    CString Value(int id)
    {
        CString s;
        GetDlgItemText(id,s);
        return s;
    }
    void Make(const char* type,const CString& text,int id,int x,int y,int w,int h,DWORD extra=0)
    {
        auto c=std::make_unique<CWnd>();
        c->CreateEx(0,CString(type),text,WS_CHILD|WS_VISIBLE|extra,CRect(x,y,x+w,y+h),this,id);
        c->SetFont(&font);
        controls.push_back(std::move(c));
    }
    void Fill()
    {
        list.ResetContent();rows.clear();
        for(size_t i=0;i<data.size();++i)
            if(Allowed(data[i]))
            {
                rows.push_back(i);
                list.AddString(ST(data[i].name+" ["+data[i].type+"]"));
            }
        if(!rows.empty())
        {
            list.SetCurSel(0);
            Select();
        }
    }

    void Select()
    {
        int row=list.GetCurSel();
        if(row<0||row>=(int)rows.size())
            return;
        selected=(int)rows[row];
        const auto& s=data[selected];
        std::vector<std::string> values={s.name,s.path,s.extensions,Number(s.scale),Number(s.minScale),Number(s.maxScale),
            s.endpoint,s.layer,s.matrix,s.style,s.version,s.srs};
        for(int i=0;i<(int)values.size();++i)
            SetDlgItemText(7200+i,ST(values[i]));
        CheckDlgButton(7250, s.visual?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(7251, s.recursive?BST_CHECKED:BST_UNCHECKED);
    }
    bool Apply()
    {
        if(selected<0)
            return true;
        auto s=data[selected];
        s.name=SU(Value(7200));
        s.path=SU(Value(7201));
        s.extensions=SU(Value(7202));
        s.endpoint=SU(Value(7206));
        s.layer=SU(Value(7207));
        s.matrix=SU(Value(7208));
        s.style=SU(Value(7209));
        s.version=SU(Value(7210));
        s.srs=SU(Value(7211));
        auto number=[&](int id,double& d)
        {
            std::istringstream in(SU(Value(id)));
            in.imbue(std::locale::classic());
            return bool(in>>d) && in.peek()==EOF && std::isfinite(d) && d>=0;
        };
        if(s.name.empty()||!number(7203,s.scale)||!number(7204,s.minScale)||!number(7205,s.maxScale)||(s.maxScale&&s.maxScale<s.minScale))
        {
            AfxMessageBox(_T("이름과 축척 값을 확인하세요. 제한 없음은 0입니다."));
            return false;
        }
        s.visual=IsDlgButtonChecked(7250)==BST_CHECKED;
        s.recursive=IsDlgButtonChecked(7251)==BST_CHECKED;
        if((s.type=="WMS"||s.type=="WFS"||s.type=="WMTS")&&s.visual&&(s.endpoint.empty()||s.layer.empty()))
        {
            AfxMessageBox(_T("표시할 서비스의 주소와 레이어를 입력하세요."));
            return false;
        }
        if((s.type=="Raster"||s.type=="CIB"||s.type=="VectorTile")&&s.path.empty())
        {
            AfxMessageBox(_T("데이터 폴더 또는 파일 경로를 입력하세요."));
            return false;
        }
        if(!s.endpoint.empty()&&s.endpoint.rfind("http://",0)!=0&&s.endpoint.rfind("https://",0)!=0)
        {
            AfxMessageBox(_T("서버 주소는 http:// 또는 https:// 로 시작해야 합니다."));
            return false;
        }
        data[selected]=s;
        int row=list.GetCurSel();
        list.DeleteString(row);
        list.InsertString(row,ST(s.name+" ["+s.type+"]"));
        list.SetCurSel(row);
        return true;
    }
    BOOL OnInitDialog()override
    {
        CDialog::OnInitDialog();
        SetWindowText(_T("XML 지도 소스 설정"));
        font.CreatePointFont(90,_T("맑은 고딕"));
        list.Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,CRect(10,10,240,485),this,LIST);
        list.SetFont(&font);
        const char* labels[]={"이름","데이터 경로","확장자 (; 구분)","대표 축척 분모","최소 축척 분모",
            "최대 축척 분모","서버 / Capabilities URL","서비스 레이어 이름","WMTS TileMatrixSet","스타일","WMS 버전","원본 좌표계 (선택)"};
        for(int i=0;i<12;++i)
        {
            int y=10+i*34;
            Make("STATIC",CString(labels[i]),-1,252,y,190,20);
            Make("EDIT",_T(""),7200+i,442,y,355,24,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        }
        Make("BUTTON",_T("기본 표시 (visual)"),7250,260,423,230,25,BS_AUTOCHECKBOX|WS_TABSTOP);
        Make("BUTTON",_T("하위 폴더 전체 탐색"),7251,510,423,245,25,BS_AUTOCHECKBOX|WS_TABSTOP);
        Make("BUTTON",_T("선택 항목 반영"),APPLY,255,460,155,28,WS_TABSTOP);
        Make("BUTTON",_T("XML 저장 및 적용"),SAVE,420,460,170,28,WS_TABSTOP);
        Make("BUTTON",_T("XML 불러오기"),OPEN,600,460,160,28,WS_TABSTOP);
        Make("BUTTON",_T("XML 다시 읽기"),RELOAD,10,502,155,28,WS_TABSTOP);
        Make("BUTTON",_T("타일 새로 고침"),REFRESH,175,502,155,28,WS_TABSTOP);
        Make("BUTTON",_T("닫기"),IDCANCEL,675,502,120,28,WS_TABSTOP);
        Make("STATIC",owner.Status(),7300,10,540,785,40);Fill();
        return TRUE;
    }

    BOOL OnCommand(WPARAM wp,LPARAM lp)override
    {
        int id=LOWORD(wp);
        if(id==LIST&&HIWORD(wp)==LBN_SELCHANGE)
        {
            Select();
            return TRUE;
        }
        if(id==APPLY)
        {
            Apply();
            return TRUE;
        }
        if(id==REFRESH)
        {
            owner.ResetCache();
            SetDlgItemText(7300,_T("타일 캐시를 초기화했습니다."));
            return TRUE;
        }
        if(id==SAVE)
        {
            if(!Apply())
                return TRUE;
            CString path=owner.Path();
            CFileDialog dlg(FALSE,_T("xml"),path,OFN_OVERWRITEPROMPT,_T("지도 정의 XML (*.xml)|*.xml||"),this);
            if(dlg.DoModal()==IDOK)
            {
                CString err;
                if(owner.Save(dlg.GetPathName(),data,err)&&owner.Load(dlg.GetPathName(),err))
                {
                    SetDlgItemText(7300,_T("XML 저장 및 적용 완료"));
                }
                else 
                    AfxMessageBox(err,MB_ICONERROR);
            }
            return TRUE;
        }
        if(id==OPEN||id==RELOAD)
        {
            CString path=owner.Path();
            if(id==OPEN)
            {
                CFileDialog dlg(TRUE,_T("xml"),path,OFN_FILEMUSTEXIST,_T("지도 정의 XML (*.xml)|*.xml||"),this);
                if(dlg.DoModal()!=IDOK)
                    return TRUE;
                path=dlg.GetPathName();
            }
            CString err;
            if(owner.Load(path,err))
            {
                data=owner.Layers();
                Fill();
                SetDlgItemText(7300,owner.Status());
            }
            else 
                AfxMessageBox(err,MB_ICONERROR);
            return TRUE;
        }
        return CDialog::OnCommand(wp,lp);
    }
public:SourceDialog(CMapSources& o):owner(o),data(o.Layers())
{
}
    INT_PTR Show(CWnd* parent)
    {
        struct Template
        {
            DLGTEMPLATE dlg;
            WORD menu,klass,title;
        } 
        t={};
        t.dlg.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;
        t.dlg.cx=540;
        t.dlg.cy=390;InitModalIndirect(&t.dlg,parent);
        return DoModal();
    }
};
}
void CMapSources::Configure(CWnd* parent)
{
    SourceDialog dlg(*this);
    dlg.Show(parent);
}
#pragma pack(pop)
