#include "pch.h"
#include "TerrainTileLayer.h"
#include "MapBuildOptions.h"
void CTerrainTileLayer::Trace(const char *text)
{
    WCHAR path[32768] = {0};
    if (!GetEnvironmentVariableW(L"C4_TERRAIN_TRACE", path, 32768))
        return;
    FILE *file = nullptr;
    if (_wfopen_s(&file, path, L"ab") == 0 && file)
    {
        fprintf(file, "%s\n", text);
        fclose(file);
    }
}

void CTerrainTileLayer::Ensure()
{
    if (server ? !C4MAP_ENABLE_TERRAIN_SERVER : !C4MAP_ENABLE_TERRAIN_LOCAL)
        return;
    auto location =
        BaseMapConfig().Value(server ? "TerrainServer.LayerJson" : "TerrainLocal.LayerJson");
    if (location.empty())
        return;
    if (attempted)
        return;
    attempted = true;
    WCHAR exe[32768] = {0};
    GetModuleFileNameW(nullptr, exe, 32768);
    CStringW path(exe);
    path = path.Left(path.ReverseFind(L'\\') + 1) + L"TerrainTiles.dll";
    dll = LoadLibraryW(path);
    if (!dll)
    {
        OutputDebugString(_T("TerrainTiles.dll could not be loaded\n"));
        return;
    }
#define TERRAIN_BIND(member, name)                                                                 \
    member = reinterpret_cast<decltype(member)>(GetProcAddress(dll, name));                        \
    if (!member)                                                                                   \
        return;
    TERRAIN_BIND(getError, "TerrainTilesGetError")
    TERRAIN_BIND(create, "TerrainTilesCreate")
    TERRAIN_BIND(destroy, "TerrainTilesDestroy")
    TERRAIN_BIND(status, "TerrainTilesGetStatus")
    TERRAIN_BIND(requestView, "TerrainTilesRequestView")
    TERRAIN_BIND(copyFrame, "TerrainTilesCopyFrame")
    TERRAIN_BIND(requestSample, "TerrainTilesRequestSample")
    TERRAIN_BIND(getSample, "TerrainTilesGetSample")
#undef TERRAIN_BIND
    handle = create(location.c_str(), server ? 1 : 0);
    Trace(handle ? "create OK" : "create failed");
}

CTerrainTileLayer::CTerrainTileLayer(int item, bool remote) : index(item), server(remote)
{
}

CTerrainTileLayer::~CTerrainTileLayer()
{
    if (handle)
        destroy(handle);
    if (dll)
        FreeLibrary(dll);
}

bool CTerrainTileLayer::Poll()
{
    if (!handle)
        return false;

    TerrainTilesStatus s{};
    s.size = sizeof(s);
    if (!status(handle, &s))
        return false;

    char error[512] = {0};
    getError(handle, error, sizeof(error));
    if (lastError != error)
    {
        lastError = error;
        OutputDebugStringA(error);
        Trace(error);
        WCHAR exe[32768] = {0};
        GetModuleFileNameW(nullptr, exe, 32768);
        CStringW log(exe);
        log = log.Left(log.ReverseFind(L'\\') + 1) + L"terrain-tiles.log";
        FILE *f = nullptr;
        if (_wfopen_s(&f, log, L"ab") == 0 && f)
        {
            fprintf(f, "%s: %s\n", server ? "server" : "local", error);
            fclose(f);
        }
    }
    bool changed = s.frameRevision != lastFrame || s.sampleRevision != lastSample;
    static unsigned seen = ~0u;
    if (seen != s.files || changed)
    {
        char text[256];
        sprintf_s(text, "index=%d files=%u frame=%llu sample=%llu", s.indexing, s.files,
                  s.frameRevision, s.sampleRevision);
        Trace(text);
        seen = s.files;
    }
    if (lastFrame != s.frameRevision)
        imageTicket = 0;
    lastFrame = s.frameRevision;
    lastSample = s.sampleRevision;
    return changed;
}

CString CTerrainTileLayer::Alt(double lon, double lat)
{
    Ensure();
    if (!handle)
        return _T(" Alt: N/A");
    sampleTicket = requestSample(handle, lon, lat);
    double metres = 0;
    int result = getSample(handle, sampleTicket, &metres);
    CString text;
    if (result == 1)
        text.Format(_T(" Alt: %.1f m"), metres);
    else if (result == 0)
        text = _T(" Alt: ...");
    else
        text = _T(" Alt: N/A");

    return text;
}

void CTerrainTileLayer::Draw(CDC *dc, CCoordMgr *mgr, CRect rect)
{
    if (!BaseMapVisible(index) || !dc || !mgr || !mgr->m_bFlat || mgr->m_fScale <= 0 ||
        rect.Width() <= 0 || rect.Height() <= 0)
        return;

    Ensure();
    if (!handle)
        return;

    double west = 127.031 + (rect.left - mgr->m_fCenterX) / (47.5 * mgr->m_fScale),
           east = 127.031 + (rect.right - mgr->m_fCenterX) / (47.5 * mgr->m_fScale);
    double north = 37.0861 - (rect.top - mgr->m_fCenterY) / (60 * mgr->m_fScale),
           south = 37.0861 - (rect.bottom - mgr->m_fCenterY) / (60 * mgr->m_fScale);
    if (south >= 90 || north <= -90)
        return;

    north = (std::min)(90.0, north);
    south = (std::max)(-90.0, south);
    rect.top = (int)ceil(mgr->m_fCenterY - (north - 37.0861) * 60 * mgr->m_fScale);
    rect.bottom = (int)floor(mgr->m_fCenterY - (south - 37.0861) * 60 * mgr->m_fScale);
    if (rect.Height() <= 0)
        return;

    // Cap the CPU elevation surface; stretch only its display image.
    double ratio = (std::min)(1.0, 1024.0 / (std::max)(rect.Width(), rect.Height()));
    TerrainTilesView view{west,
                          south,
                          east,
                          north,
                          (std::max)(1, (int)(rect.Width() * ratio)),
                          (std::max)(1, (int)(rect.Height() * ratio))};
    auto previous = frameTicket;
    frameTicket = requestView(handle, &view);
    if (previous != frameTicket)
    {
        char text[256];
        sprintf_s(text, "view ticket=%llu %.10f %.10f %.10f %.10f %d %d", frameTicket, west, south,
                  east, north, view.width, view.height);
        Trace(text);
    }
    if (!frameTicket)
        return;

    if (imageTicket != frameTicket)
    {
        pixels.resize((size_t)view.width * view.height * 4);
        if (copyFrame(handle, frameTicket, pixels.data(), pixels.size()) != 1)
            return;

        image.Destroy();
        if (!image.Create(view.width, view.height, 32, CImage::createAlphaChannel))
            return;

        for (int y = 0; y < view.height; ++y)
            memcpy(image.GetPixelAddress(0, y), pixels.data() + (size_t)y * view.width * 4,
                   (size_t)view.width * 4);
        imageTicket = frameTicket;
        size_t count = 0;
        for (size_t i = 3; i < pixels.size(); i += 4)
            count += pixels[i] != 0;

        char text[256];
        sprintf_s(text, "image ticket=%llu alpha=%zu rect=%d,%d,%d,%d", frameTicket, count,
                  rect.left, rect.top, rect.right, rect.bottom);
        Trace(text);
    }
    if (!image.IsNull())
        image.AlphaBlend(dc->GetSafeHdc(), rect.left, rect.top, rect.Width(), rect.Height(), 0, 0,
                         image.GetWidth(), image.GetHeight());
}

CTerrainTileLayer &TerrainLocalLayer()
{
    static CTerrainTileLayer layer(5, false);
    return layer;
}

CTerrainTileLayer &TerrainServerLayer()
{
    static CTerrainTileLayer layer(6, true);
    return layer;
}

CString TerrainAltitude(double lon, double lat)
{
    auto selection = BaseMapConfig().Value("AltitudeSource", "auto");
    if (selection == "terrain-server")
        return TerrainServerLayer().Alt(lon, lat);
    if (selection == "terrain-local")
        return TerrainLocalLayer().Alt(lon, lat);
    if (selection == "dted")
        return TerrainLayer().Alt(lon, lat);
    if (selection != "auto")
        return _T(" Alt: N/A (source)");
#if C4MAP_ENABLE_TERRAIN_SERVER
    auto url = BaseMapConfig().Value("TerrainServer.LayerJson");
    if (url.find_first_not_of(" \t\r\n") != std::string::npos)
        return TerrainServerLayer().Alt(lon, lat);
#endif
#if C4MAP_ENABLE_TERRAIN_LOCAL
    CString path = BaseMapPath("TerrainLocal.LayerJson", "");
    DWORD attributes = GetFileAttributes(path);
    if (!path.IsEmpty() && attributes != INVALID_FILE_ATTRIBUTES)
    {
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (path.Right(1) != _T("/") && path.Right(1) != _T("\\"))
                path += _T("/");
            path += _T("layer.json");
        }
        if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
            return TerrainLocalLayer().Alt(lon, lat);
    }
#endif
    return TerrainLayer().Alt(lon, lat);
}
