#include "MapViewSync.h"
#include <cmath>
#include <algorithm>

struct CMapViewSync::Shared
{
    uint32_t magic;
    uint32_t version;
    uint32_t enabled;
    uint32_t owner;
    uint64_t epoch;
    uint64_t serial;
    uint64_t sequence[2];
    MapViewSyncState state[2];
};

#ifdef MAP_VIEW_SYNC_TEST
#define SYNC_MUTEX L"Local\\C4IMap.Globe3D.Sync.Test.Mutex"
#define SYNC_DATA L"Local\\C4IMap.Globe3D.Sync.Test.Data"
#else
#define SYNC_MUTEX L"Local\\C4IMap.Globe3D.Sync.v1.Mutex"
#define SYNC_DATA L"Local\\C4IMap.Globe3D.Sync.v1.Data"
#endif

CMapViewSync::CMapViewSync(unsigned role) : role_(role ? 1 : 0) {}
CMapViewSync::~CMapViewSync()
{
    if (role_ == 0) Stop();
    if (shared_) UnmapViewOfFile(shared_);
    if (mapping_) CloseHandle(mapping_);
    if (mutex_) CloseHandle(mutex_);
}
bool CMapViewSync::Open()
{
    if (shared_) return true;
    if (!mutex_) mutex_ = CreateMutexW(nullptr, FALSE, SYNC_MUTEX);
    if (!mutex_) return false;
    if (!mapping_) mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, sizeof(Shared), SYNC_DATA);
    if (!mapping_) return false;
    shared_ = static_cast<Shared*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    return shared_ != nullptr;
}
bool CMapViewSync::Lock()
{
    if (!Open()) return false;
    const DWORD result = WaitForSingleObject(mutex_, 0);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) return false;
    if (shared_->magic != 0x43473453 || shared_->version != 1)
    {
        ZeroMemory(shared_, sizeof(Shared));
        shared_->version = 1;
        shared_->magic = 0x43473453;
    }
    return true;
}
void CMapViewSync::Unlock() { ReleaseMutex(mutex_); }
bool CMapViewSync::Valid(const MapViewSyncState& s)
{
    return std::isfinite(s.longitude) && std::isfinite(s.latitude) &&
        std::isfinite(s.metresPerPixel) && fabs(s.longitude) <= 180 &&
        fabs(s.latitude) <= 90 && s.metresPerPixel > 0 && s.metresPerPixel < 1e9;
}
bool CMapViewSync::Same(const MapViewSyncState& a, const MapViewSyncState& b)
{
    return fabs(std::remainder(a.longitude-b.longitude, 360.0)) < 1e-8 &&
        fabs(a.latitude-b.latitude) < 1e-8 &&
        fabs(a.metresPerPixel-b.metresPerPixel) <= (std::max)(a.metresPerPixel,b.metresPerPixel)*1e-6 &&
        a.configuredLayers == b.configuredLayers && a.visibleLayers == b.visibleLayers;
}
bool CMapViewSync::Start(const MapViewSyncState& state)
{
    if (role_ != 0 || !Valid(state) || !Lock()) return false;
    shared_->owner = GetCurrentProcessId();
    shared_->enabled = 1;
    shared_->sequence[1] = 0;
    epoch_ = ++shared_->epoch;
    shared_->state[0] = state;
    shared_->sequence[0] = seen_ = ++shared_->serial;
    previous_ = state;
    primed_ = true;
    Unlock();
    return true;
}
void CMapViewSync::Stop()
{
    if (role_ != 0 || !shared_ || !Lock()) return;
    if (shared_->owner == GetCurrentProcessId())
    {
        shared_->enabled = 0;
        ++shared_->epoch;
    }
    Unlock();
}
bool CMapViewSync::IsEnabled()
{
    if (!Lock()) return false;
    if (shared_->enabled && shared_->owner != GetCurrentProcessId())
    {
        HANDLE owner = OpenProcess(SYNCHRONIZE, FALSE, shared_->owner);
        if (owner)
        {
            if (WaitForSingleObject(owner, 0) == WAIT_OBJECT_0) shared_->enabled = 0;
            CloseHandle(owner);
        }
        else if (GetLastError() == ERROR_INVALID_PARAMETER) shared_->enabled = 0;
    }
    const bool enabled = shared_->enabled != 0;
    Unlock();
    return enabled;
}
bool CMapViewSync::Tick(const MapViewSyncState& current, MapViewSyncState& incoming)
{
    if (!Valid(current) || !IsEnabled() || !Lock()) return false;
    if (!shared_->enabled) { Unlock(); primed_ = false; return false; }
    const bool initial = !primed_ || epoch_ != shared_->epoch;
    if (initial)
    {
        epoch_ = shared_->epoch;
        seen_ = 0;
        previous_ = current;
        primed_ = true;
    }
    if (!initial && !Same(current, previous_))
    {
        shared_->state[role_] = current;
        shared_->sequence[role_] = seen_ = ++shared_->serial;
        previous_ = current;
        Unlock();
        return false;
    }
    const unsigned peer = 1-role_;
    const bool apply = shared_->sequence[peer] > seen_ && Valid(shared_->state[peer]);
    if (apply)
    {
        incoming = shared_->state[peer];
        seen_ = shared_->sequence[peer];
        previous_ = incoming;
    }
    Unlock();
    return apply;
}
void CMapViewSync::Applied(const MapViewSyncState& state) { previous_ = state; }
