#pragma once
#include <afxwin.h>
#include <array>

// Allocate the complete frame before replacing any selected bitmap.
// On allocation failure all existing buffers remain usable.
inline bool ResizeViewBitmapBuffers(CDC& reference, int width, int height,
    const std::array<CDC*, 6>& contexts,
    const std::array<CBitmap*, 6>& bitmaps)
{
    if (width <= 0 || height <= 0)
        return false;

    bool sameSize = true;
    for (auto bitmap : bitmaps)
    {
        BITMAP info = {};
        if (!bitmap->GetSafeHandle() || !bitmap->GetBitmap(&info) ||
            info.bmWidth != width || info.bmHeight != height)
            sameSize = false;
    }
    if (sameSize)
        return true;

    std::array<CBitmap, 6> replacements;
    for (auto& bitmap : replacements)
    {
        if (!bitmap.CreateCompatibleBitmap(&reference, width, height))
            return false;
    }

    std::array<HGDIOBJ, 6> previous = {};
    for (size_t i = 0; i < contexts.size(); ++i)
    {
        previous[i] = ::SelectObject(contexts[i]->GetSafeHdc(),
            replacements[i].GetSafeHandle());
        if (!previous[i] || previous[i] == HGDI_ERROR)
        {
            for (size_t j = 0; j < i; ++j)
                ::SelectObject(contexts[j]->GetSafeHdc(), previous[j]);
            return false;
        }
    }

    for (size_t i = 0; i < bitmaps.size(); ++i)
    {
        bitmaps[i]->DeleteObject();
        bitmaps[i]->Attach(replacements[i].Detach());
        // Intermediate map layers use black as the TransparentBlt color key.
        const COLORREF background = (i >= 1 && i <= 4)
            ? RGB(0, 0, 0) : RGB(10, 10, 10);
        contexts[i]->FillSolidRect(0, 0, width, height, background);
    }
    return true;
}
