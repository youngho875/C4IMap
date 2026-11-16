#pragma once
#include "MapBuildOptions.h"
#include "AppPaths.h"
#include <string>

namespace MapSourceConfig
{
    inline const wchar_t* FileName()
    {
#if C4MAP_BUILD_MODE == 1
        return L"local-sources.xml";
#elif C4MAP_BUILD_MODE == 2
        return L"tile-sources.xml";
#else
        return L"ogc-sources.xml";
#endif
    }

    template<class Exists>
    inline std::wstring Select(std::wstring directory,Exists exists)
    {
        if(!directory.empty()&&directory.back()!=L'/'&&directory.back()!=L'\\')
            directory+=L'/';
        #if C4MAP_BUILD_MODE == 1
        auto selected=AppPathWide("LocalSourcesXml");
#elif C4MAP_BUILD_MODE == 2
        auto selected=AppPathWide("TileSourcesXml");
#else
        auto selected=AppPathWide("OgcSourcesXml");
#endif
        // Never hide an invalid mode file by silently loading another config.
        if(exists(selected))return selected;
        auto legacy=AppPathWide("LegacySourcesXml");
        if(exists(legacy))return legacy;
        return selected;
    }
}
