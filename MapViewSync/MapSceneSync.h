#pragma once
#pragma pack(push,8)
#include <string>
#include <cstdint>

// Scene payloads contain source paths, styles and geographic objects, never a captured map image.
class CMapSceneSync
{
public:
    bool Publish(const std::string& json, std::string& error);
    bool ReadChanged(std::string& json, std::string& error);
    void ForcePublish();
private:
    static std::wstring FileName();
    std::string previous_;
    uint64_t stamp_ = 0;
};

#pragma pack(pop)
