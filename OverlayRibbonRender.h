#pragma once
#include <string>
#include <vector>
#include <gdiplus.h>
#include "LocalOverlay.h"
namespace OverlayRibbonRender
{
std::string Escape(const wchar_t *value);
void Draw(Gdiplus::Graphics &graphics, int type, double x1, double y1, double x2, double y2,
          const CLocalOverlay::Style &style, const std::vector<Gdiplus::PointF> &points = {});
} // namespace OverlayRibbonRender
// Render an installed OLE document server's preview to a persistent PNG.
namespace OverlayRibbonRender
{
bool ImportOlePreview(const CString &source, CString &pngPath, CString &error);
}
