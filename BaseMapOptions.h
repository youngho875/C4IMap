#pragma once
#include "MapBuildOptions.h"
#include "cpl_minixml.h"
#include <atlconv.h>
#include <string>
#pragma pack(push, 8)
struct BaseMapConfiguration
{
public:
    CPLXMLNode *xml = nullptr;
    std::string file;

    BaseMapConfiguration();
    ~BaseMapConfiguration();

    std::string Value(const char *key, const char *fallback = "") const;
    bool Flag(const char *key, bool defaultVisible = true) const;
    void Save();
};

#pragma pack(pop)
BaseMapConfiguration &BaseMapConfig();

// Missing or commented sections are not configured layers.
bool BaseMapConfigured(int index);

bool BaseMapVisible(int index);

void SetBaseMapVisible(int index, bool visible);

CString BaseMapPath(const char *key, const char *fallback);
