#include "pch.h"
#include "AppPaths.h"
#include "BaseMapOptions.h"
#include "MapBuildOptions.h"
BaseMapConfiguration::BaseMapConfiguration()
{
    CStringW path(AppPathWide("BaseMapXml").c_str());
    file = AppPath("BaseMapXml");
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        xml = CPLParseXMLFile(file.c_str());

    if (xml && !CPLGetXMLNode(xml, "=BaseMap"))
    {
        CPLDestroyXMLNode(xml);
        xml = nullptr;
    }
    const bool createDefaults = GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES;
    if (!xml && createDefaults)
    {
        xml = CPLParseXMLString(
            "<BaseMap version=\"1\"><SmallScale><Path>${SmallMap}</Path><LabelPath>${SmallNames}</LabelPath><visual>true</visual></SmallScale>"
            "<World><Path>${WorldMap}</Path><BoundaryPath>${WorldBoundary}</BoundaryPath><LabelPath>${WorldNames}</LabelPath><CityPath>${WorldCities}</CityPath><visual>true</visual></World>"
            "<NaturalEarth><Path>${NaturalEarth}</Path><visual>true</visual></NaturalEarth><Names><visual>true</visual></Names>"
            "<ScaleStatus>true</ScaleStatus><ScaleBar>true</ScaleBar><Compass>true</Compass></BaseMap>");
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
            Save();
    }
    auto root = CPLGetXMLNode(xml, "=BaseMap");
    if (createDefaults && root && !CPLGetXMLNode(root, "Elevation"))
    {
        CPLSetXMLValue(root, "Elevation.Path", "${ElevationRoot}");
        CPLSetXMLValue(root, "Elevation.visual", "false");
        Save();
    }
    if (createDefaults && root && !CPLGetXMLNode(root, "TerrainLocal"))
    {
        CPLSetXMLValue(root, "TerrainLocal.LayerJson", "${TerrainLocalJson}");
        CPLSetXMLValue(root, "TerrainLocal.visual", "false");
        Save();
    }
    if (createDefaults && root && !CPLGetXMLNode(root, "TerrainServer"))
    {
        CPLSetXMLValue(root, "TerrainServer.LayerJson", "");
        CPLSetXMLValue(root, "TerrainServer.visual", "false");
        Save();
    }
    if (createDefaults && root && !CPLGetXMLNode(root, "AltitudeSource"))
    {
        CPLSetXMLValue(root, "AltitudeSource", "auto");
        Save();
    }
}

BaseMapConfiguration::~BaseMapConfiguration()
{
    CPLDestroyXMLNode(xml);
}

std::string BaseMapConfiguration::Value(const char *key, const char *fallback) const
{
    return ResolveAppPath(CPLGetXMLValue(CPLGetXMLNode(xml, "=BaseMap"), key, fallback));
}

bool BaseMapConfiguration::Flag(const char *key, bool defaultVisible) const
{
    auto value = Value(key, defaultVisible ? "true" : "false");
    return value == "true" || value == "1" || value == "TRUE";
}

void BaseMapConfiguration::Save()
{
    if (!CPLSerializeXMLTreeToFile(xml, file.c_str()))
        OutputDebugString(_T("C4IMap: base-map.xml save failed\n"));
}

BaseMapConfiguration &BaseMapConfig()
{
    static BaseMapConfiguration config;
    return config;
}

bool BaseMapConfigured(int index)
{
    static const char *sections[] = {"SmallScale", "World",        "NaturalEarth", "Names",
                                     "Elevation",  "TerrainLocal", "TerrainServer"};
    if (index < 0 || index >= 7)
        return false;
    auto root = CPLGetXMLNode(BaseMapConfig().xml, "=BaseMap");
    auto node = root ? CPLGetXMLNode(root, sections[index]) : nullptr;
    return node && node->eType == CXT_Element;
}

bool BaseMapVisible(int index)
{
    static const char *keys[] = {"SmallScale.visual",   "World.visual",     "NaturalEarth.visual",
                                 "Names.visual",        "Elevation.visual", "TerrainLocal.visual",
                                 "TerrainServer.visual"};
    return BaseMapConfigured(index) &&
           (index < 4    ? C4MAP_ENABLE_BASEMAP
            : index == 5 ? C4MAP_ENABLE_TERRAIN_LOCAL
            : index == 6 ? C4MAP_ENABLE_TERRAIN_SERVER
                         : C4MAP_ENABLE_LOCAL) &&
           BaseMapConfig().Flag(keys[index], index < 4);
}

void SetBaseMapVisible(int index, bool visible)
{
    static const char *keys[] = {"SmallScale.visual",   "World.visual",     "NaturalEarth.visual",
                                 "Names.visual",        "Elevation.visual", "TerrainLocal.visual",
                                 "TerrainServer.visual"};
    if (BaseMapConfigured(index))
    {
        CPLSetXMLValue(CPLGetXMLNode(BaseMapConfig().xml, "=BaseMap"), keys[index],
                       visible ? "true" : "false");
        BaseMapConfig().Save();
    }
}

CString BaseMapPath(const char *key, const char *fallback)
{
    auto path = BaseMapConfig().Value(key, fallback);
    CA2W wide(path.c_str(), CP_UTF8);
    return CString(CW2T(wide, CP_ACP));
}


bool MapAutoScaleEnabled()
{
    return BaseMapConfig().Flag("AutoScale", true);
}

bool SetMapAutoScaleEnabled(bool enabled)
{
    auto& config = BaseMapConfig();
    auto root = CPLGetXMLNode(config.xml, "=BaseMap");
    if (!root)
        return false;
    const std::string previous = config.Value("AutoScale", "true");
    CPLSetXMLValue(root, "AutoScale", enabled ? "true" : "false");
    if (CPLSerializeXMLTreeToFile(config.xml, config.file.c_str()))
        return true;
    CPLSetXMLValue(root, "AutoScale", previous.c_str());
    return false;
}
