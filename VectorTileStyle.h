#pragma once
#include <algorithm>
#include <cmath>
#include <cpl_minixml.h>
#include <functional>
#include <gdiplus.h>
#include <map>
#include <memory>
#include <ogrsf_frmts.h>
#include <sstream>
#include <vector>
#pragma comment(lib, "gdiplus.lib")
namespace vtstyle
{
std::string Tag(const char *s);
const CPLXMLNode *Child(const CPLXMLNode *n, const char *name);
std::string Text(const CPLXMLNode *n);
std::string Value(const CPLXMLNode *n, const char *key);
double Num(std::string s, double d = 0);
std::wstring Wide(const std::string &s);
std::string Param(const CPLXMLNode *n, const char *key, const char *fallback = "");
Gdiplus::Color Color(const std::string &s, double opacity = 1);
std::string Expr(const CPLXMLNode *n, OGRFeature *f);
bool Filter(const CPLXMLNode *n, OGRFeature *f);
struct ScreenSymbol
{
public:
    std::string path;
    double lon, lat;
    float width, height;
};
class Style
{
private:
    std::unique_ptr<CPLXMLNode, decltype(&CPLDestroyXMLNode)> xml{nullptr, CPLDestroyXMLNode};
    std::string root;
    std::map<std::string, std::unique_ptr<Gdiplus::Image>> images;
    Gdiplus::Image *Image(const CPLXMLNode *graphic);
    void Path(Gdiplus::GraphicsPath &path, OGRGeometry *geo, double w, double n, double sx,
              double sy);

public:
    explicit Style(const std::string &file);
    bool Valid() const;
    bool Draw(Gdiplus::Graphics &graphics, OGRFeature *feature, OGRGeometry *geo,
              const std::string &layer, double scale, double west, double north, double east,
              double south, std::vector<ScreenSymbol> *symbols = nullptr);
};
} // namespace vtstyle
