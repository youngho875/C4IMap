#pragma once
#pragma pack(push, 8)
#include <windows.h>
#include <cstdint>

struct MapViewSyncState
{
    double longitude = 0;
    double latitude = 0;
    double metresPerPixel = 1;
    uint32_t configuredLayers = 0;
    uint32_t visibleLayers = 0;
};

// One channel per Windows logon session. Role 0 controls start/stop; role 1 follows it.
class CMapViewSync
{
public:
    explicit CMapViewSync(unsigned role);
    ~CMapViewSync();
    bool Start(const MapViewSyncState& state);
    void Stop();
    bool IsEnabled();
    bool Tick(const MapViewSyncState& current, MapViewSyncState& incoming);
    void Applied(const MapViewSyncState& actual);
private:
    struct Shared;
    bool Open();
    bool Lock();
    void Unlock();
    static bool Valid(const MapViewSyncState& state);
    static bool Same(const MapViewSyncState& a, const MapViewSyncState& b);
    unsigned role_;
    HANDLE mapping_ = nullptr;
    HANDLE mutex_ = nullptr;
    Shared* shared_ = nullptr;
    uint64_t seen_ = 0;
    uint64_t epoch_ = 0;
    bool primed_ = false;
    MapViewSyncState previous_;
    CMapViewSync(const CMapViewSync&) = delete;
    CMapViewSync& operator=(const CMapViewSync&) = delete;
};

#ifdef _WIN64
static_assert(sizeof(CMapViewSync)==88,"MapViewSync packing mismatch");
#endif
#pragma pack(pop)
