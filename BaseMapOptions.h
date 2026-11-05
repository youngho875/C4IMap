#pragma once
#include "MapBuildOptions.h"
#include "cpl_minixml.h"
#include <atlconv.h>
#include <string>
#pragma pack(push,8)
struct BaseMapConfiguration
{
    CPLXMLNode* xml=nullptr;
    std::string file;

    BaseMapConfiguration()
    {
        WCHAR exe[32768]={0};
        GetModuleFileNameW(nullptr,exe,32768);
        CStringW path(exe);
        path=path.Left(path.ReverseFind(L'\\')+1)+L"base-map.xml";

        file=CW2A(path,CP_UTF8);
        if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)
            xml=CPLParseXMLFile(file.c_str());

        if(xml && !CPLGetXMLNode(xml,"=BaseMap"))
        {
            CPLDestroyXMLNode(xml);
            xml=nullptr;
        }
        const bool createDefaults = GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES;
        if(!xml && createDefaults)
        {
            xml=CPLParseXMLString("<BaseMap version=\"1\"><SmallScale><Path>D:/map/small_scale_map/small_map.shp</Path>" \
                "<LabelPath>D:/map/small_scale_map/name_map.shp</LabelPath><visual>true</visual></SmallScale>" \
                "<World><Path>D:/map/world_map/Continents.shp</Path><BoundaryPath>D:/map/world_map/national_boundary_polygon.shp</BoundaryPath>" \
                "<LabelPath>D:/map/world_map/placenam.shp</LabelPath><CityPath>D:/map/world_map/World_city_pt.shp</CityPath><visual>true</visual>" \
                "</World><NaturalEarth><Path>D:/webcop/NaturalEarthII</Path><visual>true</visual></NaturalEarth><Names><visual>true</visual></Names>" \
                "<ScaleStatus>true</ScaleStatus><ScaleBar>true</ScaleBar><Compass>true</Compass></BaseMap>");
            if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)
                Save();
        }
        auto root=CPLGetXMLNode(xml,"=BaseMap");
        if(createDefaults && root && !CPLGetXMLNode(root,"Elevation")){
            CPLSetXMLValue(root,"Elevation.Path","D:/map/strm");
            CPLSetXMLValue(root,"Elevation.visual","false");Save();
        }
        if(createDefaults && root && !CPLGetXMLNode(root,"TerrainLocal")){
            CPLSetXMLValue(root,"TerrainLocal.LayerJson","D:/map/terrain/layer.json");
            CPLSetXMLValue(root,"TerrainLocal.visual","false");Save();
        }
        if(createDefaults && root && !CPLGetXMLNode(root,"TerrainServer")){
            CPLSetXMLValue(root,"TerrainServer.LayerJson","");
            CPLSetXMLValue(root,"TerrainServer.visual","false");Save();
        }
        if(createDefaults && root && !CPLGetXMLNode(root,"AltitudeSource")){
            CPLSetXMLValue(root,"AltitudeSource","auto");Save();
        }
    }
    ~BaseMapConfiguration()
    {
        CPLDestroyXMLNode(xml);
    }

    std::string Value(const char* key,const char* fallback="") const
    {
        return CPLGetXMLValue(CPLGetXMLNode(xml,"=BaseMap"),key,fallback);
    }
    bool Flag(const char* key,bool defaultVisible=true) const
    {
        auto value=Value(key,defaultVisible?"true":"false");
        return value=="true"||value=="1"||value=="TRUE";
    }
    void Save()
    {
        if(!CPLSerializeXMLTreeToFile(xml,file.c_str()))
            OutputDebugString(_T("C4IMap: base-map.xml save failed\n"));
    }
};

#pragma pack(pop)
inline BaseMapConfiguration& BaseMapConfig()
{
    static BaseMapConfiguration config;
    return config;
}

// Missing or commented sections are not configured layers.
inline bool BaseMapConfigured(int index)
{
    static const char* sections[] = {
        "SmallScale", "World", "NaturalEarth", "Names",
        "Elevation", "TerrainLocal", "TerrainServer"
    };
    if (index < 0 || index >= 7)
        return false;
    auto root = CPLGetXMLNode(BaseMapConfig().xml, "=BaseMap");
    auto node = root ? CPLGetXMLNode(root, sections[index]) : nullptr;
    return node && node->eType == CXT_Element;
}

inline bool BaseMapVisible(int index)
{
    static const char* keys[]={"SmallScale.visual","World.visual","NaturalEarth.visual","Names.visual","Elevation.visual","TerrainLocal.visual","TerrainServer.visual"};
    return BaseMapConfigured(index) && (index<4?C4MAP_ENABLE_BASEMAP:index==5?C4MAP_ENABLE_TERRAIN_LOCAL:index==6?C4MAP_ENABLE_TERRAIN_SERVER:C4MAP_ENABLE_LOCAL) && BaseMapConfig().Flag(keys[index],index<4);
}

inline void SetBaseMapVisible(int index,bool visible)
{
    static const char* keys[]={"SmallScale.visual","World.visual","NaturalEarth.visual","Names.visual","Elevation.visual","TerrainLocal.visual","TerrainServer.visual"};
    if(BaseMapConfigured(index))
    {
        CPLSetXMLValue(CPLGetXMLNode(BaseMapConfig().xml,"=BaseMap"),keys[index],visible?"true":"false");
        BaseMapConfig().Save();
    }
}

inline CString BaseMapPath(const char* key,const char* fallback)
{
    auto path=BaseMapConfig().Value(key,fallback);
    CA2W wide(path.c_str(), CP_UTF8);
    return CString(CW2T(wide, CP_ACP));
}
