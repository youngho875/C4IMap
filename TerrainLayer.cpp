#include "pch.h"
#include "TerrainLayer.h"
#include "MapBuildOptions.h"
void CTerrainLayer::Trace(const char *text)
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

void CTerrainLayer::Ensure()
{
#if C4MAP_ENABLE_LOCAL
    if (attempted)
        return;
    attempted = true;
    WCHAR exe[32768] = {0};
    GetModuleFileNameW(nullptr, exe, 32768);
    CStringW path(exe);
    path = path.Left(path.ReverseFind(L'\\') + 1) + L"TerrainElevation.dll";
    dll = LoadLibraryW(path);
    if (!dll)
    {
        OutputDebugString(_T("TerrainElevation.dll could not be loaded\n"));
        return;
    }
#define TERRAIN_BIND(member, name)                                                                 \
    member = reinterpret_cast<decltype(member)>(GetProcAddress(dll, name));                        \
    if (!member)                                                                                   \
        return;
    TERRAIN_BIND(create, "TerrainCreate")
    TERRAIN_BIND(destroy, "TerrainDestroy")
    TERRAIN_BIND(status, "TerrainGetStatus") TERRAIN_BIND(requestView, "TerrainRequestView")
        TERRAIN_BIND(copyFrame, "TerrainCopyFrame")
            TERRAIN_BIND(requestSample, "TerrainRequestSample")
                TERRAIN_BIND(getSample, "TerrainGetSample")
#undef TERRAIN_BIND
                    handle = create(BaseMapConfig().Value("Elevation.Path", "D:/map/strm").c_str());
    Trace(handle ? "create OK" : "create failed");
#endif
}

CTerrainLayer::~CTerrainLayer()
{
    if (handle)
        destroy(handle);
    if (dll)
        FreeLibrary(dll);
}

bool CTerrainLayer::Poll()
{
    Ensure();
    if (!handle)
        return false;
    TerrainStatus s{};
    s.size = sizeof(s);
    if (!status(handle, &s))
        return false;
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
    lastFrame = s.frameRevision;
    lastSample = s.sampleRevision;
    return changed;
}

CString CTerrainLayer::Alt(double lon, double lat)
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

void CTerrainLayer::Draw(CDC *dc, CCoordMgr *mgr, CRect rect)
{
    if (!BaseMapVisible(4) || !dc || !mgr || !mgr->m_bFlat || mgr->m_fScale <= 0 ||
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
    TerrainView view{west,
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

CTerrainLayer &TerrainLayer()
{
    static CTerrainLayer layer;
    return layer;
}
