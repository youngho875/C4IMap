#include "pch.h"
#include "AppPaths.h"
#include "cpl_minixml.h"
#include <algorithm>
#include <atlconv.h>
#include <cctype>
#include <filesystem>
#include <memory>
#include <stdexcept>
namespace
{
std::string Slashes(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}
bool IsUrl(const std::string &value)
{
    return value.find("://") != std::string::npos || value.rfind("/vsi", 0) == 0;
}
std::string Absolute(const std::string &value, const std::string &base)
{
    if (value.empty() || IsUrl(value))
        return value;
    auto path = std::filesystem::u8path(Slashes(value));
    if (!path.is_absolute())
        path = std::filesystem::u8path(base) / path;
    return path.lexically_normal().generic_u8string();
}
bool Prefix(const std::string &value, const std::string &prefix)
{
    if (value.size() < prefix.size())
        return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (tolower((unsigned char)value[i]) != tolower((unsigned char)prefix[i]))
            return false;
    return value.size() == prefix.size() || value[prefix.size()] == '/';
}
} // namespace
CAppPaths::CAppPaths(const std::string &name, const std::string &executable)
    : file(name), executableDirectory(Slashes(executable))
{
    // Compatibility defaults live only here. Deployment overrides live in
    // c4imap-paths.xml.
    values = {{"MapRoot", "D:/map"},
              {"WebcopRoot", "D:/webcop"},
              {"RasterRoot", "${MapRoot}/raster"},
              {"CibRoot", "${MapRoot}/satellite"},
              {"TileRoot", "${MapRoot}/tileData"},
              {"ShapeRoot", "${MapRoot}/shp"},
              {"StyleRoot", "${MapRoot}/styles"},
              {"GssRoot", "${StyleRoot}/GSS"},
              {"SymbolRoot", "${StyleRoot}/symbols"},
              {"StyleCatalog", "${StyleRoot}/mapmake-style-catalog.xml"},
              {"ProjRoot", "${StyleRoot}/proj"},
              {"SmallMap", "${MapRoot}/small_scale_map/small_map.shp"},
              {"SmallNames", "${MapRoot}/small_scale_map/name_map.shp"},
              {"WorldMap", "${MapRoot}/world_map/Continents.shp"},
              {"WorldBoundary", "${MapRoot}/world_map/national_boundary_polygon.shp"},
              {"WorldNames", "${MapRoot}/world_map/placenam.shp"},
              {"WorldCities", "${MapRoot}/world_map/World_city_pt.shp"},
              {"NaturalEarth", "${WebcopRoot}/NaturalEarthII"},
              {"ElevationRoot", "${MapRoot}/strm"},
              {"TerrainLocalJson", "${MapRoot}/terrain/layer.json"},
              {"TerrainElevationDll", "${ExeDir}/TerrainElevation.dll"},
              {"TerrainTilesDll", "${ExeDir}/TerrainTiles.dll"},
              {"BaseMapXml", "${ExeDir}/base-map.xml"},
              {"LocalSourcesXml", "${ExeDir}/local-sources.xml"},
              {"TileSourcesXml", "${ExeDir}/tile-sources.xml"},
              {"OgcSourcesXml", "${ExeDir}/ogc-sources.xml"},
              {"LegacySourcesXml", "${ExeDir}/map-sources.xml"}};
    mounts = {{"D:/map", "${MapRoot}"}, {"D:/webcop", "${WebcopRoot}"}};
    if (!file.empty())
    {
        std::unique_ptr<CPLXMLNode, decltype(&CPLDestroyXMLNode)> xml(CPLParseXMLFile(file.c_str()),
                                                                      CPLDestroyXMLNode);
        auto root = xml ? CPLGetXMLNode(xml.get(), "=C4IMapPaths") : nullptr;
        if (!root)
            throw std::runtime_error("Invalid c4imap-paths.xml: " + file);
        for (auto node = root->psChild; node; node = node->psNext)
        {
            if (node->eType != CXT_Element)
                continue;
            if (!strcmp(node->pszValue, "Path"))
            {
                std::string key = CPLGetXMLValue(node, "name", ""),
                            value = CPLGetXMLValue(node, "value", "");
                if (key.empty() || value.empty() || key == "ExeDir" || key == "ConfigDir")
                    throw std::runtime_error("Invalid Path entry: " + file);
                values[key] = value;
            }
            else if (!strcmp(node->pszValue, "Mount"))
            {
                std::string from = Slashes(CPLGetXMLValue(node, "from", "")),
                            to = CPLGetXMLValue(node, "to", "");
                while (from.size() > 3 && from.back() == '/')
                    from.pop_back();
                if (from.empty() || to.empty())
                    throw std::runtime_error("Invalid Mount entry: " + file);
                mounts.erase(std::remove_if(mounts.begin(), mounts.end(),
                                            [&](const auto &m) {
                                                return _stricmp(m.first.c_str(), from.c_str()) == 0;
                                            }),
                             mounts.end());
                mounts.emplace_back(from, to);
            }
            else
                throw std::runtime_error("Unknown path configuration element: " +
                                         std::string(node->pszValue));
        }
    }
    std::stable_sort(mounts.begin(), mounts.end(),
                     [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
    for (const auto &value : values)
        Get(value.first);
    for (const auto &mount : mounts)
        Expand(mount.second, 0);
}
std::string CAppPaths::Expand(const std::string &input, unsigned depth) const
{
    if (depth > 16)
        throw std::runtime_error("Circular path variable in " + file);
    std::string value = input;
    size_t start = 0;
    while ((start = value.find("${", start)) != std::string::npos)
    {
        auto end = value.find('}', start + 2);
        if (end == std::string::npos)
            throw std::runtime_error("Unclosed path variable");
        auto key = value.substr(start + 2, end - start - 2);
        std::string replacement;
        if (key == "ExeDir")
            replacement = executableDirectory;
        else if (key == "ConfigDir")
            replacement = file.empty()
                              ? executableDirectory
                              : std::filesystem::u8path(file).parent_path().generic_u8string();
        else
        {
            auto found = values.find(key);
            if (found == values.end())
                throw std::runtime_error("Unknown path variable: " + key);
            replacement = Expand(found->second, depth + 1);
        }
        value.replace(start, end - start + 1, replacement);
        start += replacement.size();
    }
    return value;
}
std::string CAppPaths::Get(const std::string &key) const
{
    auto found = values.find(key);
    if (found == values.end())
        throw std::runtime_error("Unknown path key: " + key);
    return Absolute(Expand(found->second, 0), executableDirectory);
}
std::string CAppPaths::Resolve(const std::string &input) const
{
    if (input.empty())
        return input;
    auto value = Expand(input, 0);
    if (IsUrl(value))
        return value;
    auto normalized = Slashes(value);
    for (const auto &mount : mounts)
        if (Prefix(normalized, mount.first))
            return Absolute(Expand(mount.second, 0) + normalized.substr(mount.first.size()),
                            executableDirectory);
    // Keep ordinary XML-relative paths relative to their own XML file.
    return input.find("${") != std::string::npos ? Absolute(value, executableDirectory) : value;
}
std::string CAppPaths::Reference(const std::string &input) const
{
    if (input.empty() || IsUrl(input) || input.find("${") != std::string::npos)
        return input;
    const auto normalized = Slashes(input);
    std::string bestKey, bestPath;
    for (const auto &entry : values)
    {
        const auto path = Slashes(Get(entry.first));
        if (!path.empty() && !IsUrl(path) && path.size() > bestPath.size() &&
            Prefix(normalized, path))
        {
            bestKey = entry.first;
            bestPath = path;
        }
    }
    return bestKey.empty() ? input : "${" + bestKey + "}" + normalized.substr(bestPath.size());
}
const std::string &CAppPaths::File() const
{
    return file;
}
CAppPaths &AppPaths()
{
    static CAppPaths paths = []() {
        wchar_t exe[32768] = {};
        GetModuleFileNameW(nullptr, exe, 32768);
        auto directory = std::filesystem::path(exe).parent_path();
        auto parent = directory;
        std::string file;
        while (!parent.empty())
        {
            auto candidate = parent / L"c4imap-paths.xml";
            if (std::filesystem::exists(candidate))
            {
                file = candidate.generic_u8string();
                break;
            }
            auto next = parent.parent_path();
            if (next == parent)
                break;
            parent = next;
        }
        return CAppPaths(file, directory.generic_u8string());
    }();
    return paths;
}
std::string AppPath(const char *key)
{
    return AppPaths().Get(key);
}
std::wstring AppPathWide(const char *key)
{
    auto value = AppPath(key);
    return std::wstring(CA2W(value.c_str(), CP_UTF8));
}
CString AppPathText(const char *key)
{
    return CString(AppPathWide(key).c_str());
}
std::string ResolveAppPath(const std::string &value)
{
    return AppPaths().Resolve(value);
}
