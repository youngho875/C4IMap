#include "pch.h"
#include "MapFrameCache.h"
#include "MapBuildOptions.h"
void CMapFrameCache::Surface::Clear()
{
    if (dc.GetSafeHdc() && original)
        ::SelectObject(dc.GetSafeHdc(), original);
    original = nullptr;
    bitmap.DeleteObject();
    dc.DeleteDC();
    valid = false;
}

CMapFrameCache::Surface::~Surface()
{
    Clear();
}

void CMapFrameCache::InvalidateFrom(int first)
{
    for (int i = first; i < 4; ++i)
        surfaces[i].valid = false;
}

void CMapFrameCache::Prepare(int w, int h, const std::array<double, 12> &next)
{
    if (w != width || h != height)
    {
        for (auto &s : surfaces)
            s.Clear();
        width = w;
        height = h;
    }
    if (!keyed || key != next)
    {
        InvalidateFrom();
        key = next;
        keyed = true;
    }
}

bool CMapFrameCache::Restore(int group, CDC *target)
{
    auto &s = surfaces[group];
    if (!s.valid)
        return false;
    if (!target->BitBlt(0, 0, width, height, &s.dc, 0, 0, SRCCOPY))
    {
        InvalidateFrom(group);
        return false;
    }
    ++hits[group];

    return true;
}

void CMapFrameCache::Store(int group, CDC *source)
{
    auto &s = surfaces[group];
    ++renders[group];
    if (width <= 0 || height <= 0)
        return;
    if (!s.dc.GetSafeHdc())
    {
        if (!s.dc.CreateCompatibleDC(source) ||
            !s.bitmap.CreateCompatibleBitmap(source, width, height))
        {
            s.Clear();
            return;
        }
        s.original = ::SelectObject(s.dc.GetSafeHdc(), s.bitmap.GetSafeHandle());
        if (!s.original || s.original == HGDI_ERROR)
        {
            s.original = nullptr;
            s.Clear();
            return;
        }
    }
    s.valid = s.dc.BitBlt(0, 0, width, height, source, 0, 0, SRCCOPY) != FALSE;
}
