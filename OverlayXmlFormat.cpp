#include "pch.h"
#include "OverlayXmlFormat.h"
#include "cpl_minixml.h"
#include "cpl_conv.h"
#include <memory>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <locale>
#include <algorithm>

namespace
{
using Tree = std::unique_ptr<CPLXMLNode, decltype(&CPLDestroyXMLNode)>;
struct AttributeMapping
{
    const char* oldName;
    const char* tag;
    const char* name;
};
const AttributeMapping mappings[] = {
    {"x1", "geo", "x1"},
    {"y1", "geo", "y1"},
    {"x2", "geo", "x2"},
    {"y2", "geo", "y2"},
    {"points", "geo", "pt"},
    {"flipX", "geo", "flipX"},
    {"flipY", "geo", "flipY"},
    {"lineColor", "lp", "lc"},
    {"width", "lp", "lw"},
    {"lineAlpha", "lp", "alpha"},
    {"lineEnabled", "lp", "enabled"},
    {"lineCap", "lp", "cap"},
    {"lineJoin", "lp", "lj"},
    {"lineAlignment", "lp", "la"},
    {"lineStripe", "lp", "stripe"},
    {"fillColor", "fp", "fc"},
    {"fillType", "fp", "ft"},
    {"fillAlpha", "fp", "alpha"},
    {"fillEnabled", "fp", "enabled"},
    {"hatch", "hb", "hs"},
    {"patternColor", "hb", "fg"},
    {"gradientMode", "lgb", "lgm"},
    {"gradientPosition", "lgb", "bp"},
    {"gradientColor", "lgb", "ic"},
    {"startArrow", "lcs", "cs"},
    {"endArrow", "lce", "cs"},
    {"dash", "ds", "ds"},
    {"rotation", "srt", "rot"},
    {"text", "tp", "tx"},
    {"font", "tp", "fn"},
    {"fontSize", "tp", "fs"},
    {"textColor", "tp", "tc"},
    {"fontStyle", "tp", "style"},
    {"textAlign", "tp", "ha"},
    {"textAlignY", "tp", "va"},
    {"lineInterval", "tp", "lineInterval"},
    {"vertical", "tp", "vertical"},
    {"rightToLeft", "tp", "rightToLeft"},
    {"autoTextBox", "tp", "autoTextBox"},
    {"imagePath", "image", "src"},
};
const char* types[] = {"NULL_TYPE", "LINE", "LINE", "RECT", "ELLIPSE", "ROUND_RECT",
    "TRIANGLE", "TRIANGLE", "POLYGON", "POLYGON", "DIAMOND", "PENTAGON", "SIX_ANGLE",
    "ARC", "PIE", "ARC", "LINKED_LINE", "CURVE", "TEXT", "POLYLINE", "POLYGON",
    "CURVE", "CURVE"};
CPLXMLNode* Root(CPLXMLNode* node, const char* name)
{
    for (; node; node = node->psNext)
        if (node->eType == CXT_Element && !strcmp(node->pszValue, name)) return node;
    return nullptr;
}
void Attr(CPLXMLNode* node, const char* name, const char* value)
{
    CPLSetXMLValue(node, (std::string("#") + name).c_str(), value);
}
void Copy(CPLXMLNode* from, const char* key, CPLXMLNode* to, const char* name)
{
    const char* value = CPLGetXMLValue(from, key, nullptr);
    if (value) Attr(to, name, value);
}
std::string Serialize(CPLXMLNode* node)
{
    char* text = CPLSerializeXMLTree(node);
    std::string value = text ? text : "";
    CPLFree(text);
    return OverlayXmlFormat::EscapeAttributeWhitespace(value);
}
}

bool OverlayXmlFormat::Encode(const std::string& legacy, std::string& xml)
{
    Tree source(CPLParseXMLString(legacy.c_str()), CPLDestroyXMLNode);
    auto old = Root(source.get(), "C4IMapOverlay");
    if (!old) return false;
    Tree result(CPLCreateXMLNode(nullptr, CXT_Element, "ovd"), CPLDestroyXMLNode);
    auto root = result.get();
    Attr(root, "format", "C4IMap-XMLControl");
    Attr(root, "version", "1");
    Attr(root, "coordinates", "GeoDC-world");
    Copy(old, "active", root, "active");
    size_t objectId = 0;
    for (auto layer = old->psChild; layer; layer = layer->psNext)
    {
        if (layer->eType != CXT_Element) continue;
        auto info = CPLCreateXMLNode(root, CXT_Element, "ovi");
        Copy(layer, "id", info, "gid");
        Copy(layer, "name", info, "nm");
        Copy(layer, "visible", info, "sh");
        for (auto item = layer->psChild; item; item = item->psNext)
        {
            if (item->eType != CXT_Element) continue;
            auto shape = CPLCreateXMLNode(info, CXT_Element, "ovs");
            Attr(shape, "oid", std::to_string(++objectId).c_str());
            Copy(layer, "id", shape, "gid");
            Copy(item, "type", shape, "type");
            Copy(item, "group", shape, "group");
            const int type = atoi(CPLGetXMLValue(item, "type", "0"));
            Attr(shape, "nm", type >= 0 && type < 23 ? types[type] : "NULL_TYPE");
            for (const auto& mapping : mappings)
            {
                auto child = CPLGetXMLNode(shape, mapping.tag);
                if (!child) child = CPLCreateXMLNode(shape, CXT_Element, mapping.tag);
                Copy(item, mapping.oldName, child, mapping.name);
                if (!strcmp(mapping.oldName, "points"))
                {
                    std::istringstream input(CPLGetXMLValue(item, "points", ""));
                    input.imbue(std::locale::classic());
                    std::ostringstream points;
                    points.imbue(std::locale::classic());
                    points << std::setprecision(17);
                    double x, y;
                    bool first = true;
                    while (input >> x >> y)
                    {
                        if (!first) points << '|';
                        points << x << ',' << y;
                        first = false;
                    }
                    Attr(child, "vertices", first ? "0" : "1");
                    if (first)
                        points << CPLGetXMLValue(item, "x1", "0") << ',' << CPLGetXMLValue(item, "y1", "0")
                               << '|' << CPLGetXMLValue(item, "x2", "0") << ',' << CPLGetXMLValue(item, "y2", "0");
                    Attr(child, "pt", points.str().c_str());
                    Attr(child, "ct", "0");
                }
            }
        }
    }
    xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + Serialize(root);
    return true;
}

bool OverlayXmlFormat::Decode(const std::string& xml, std::string& legacy)
{
    Tree source(CPLParseXMLString(xml.c_str()), CPLDestroyXMLNode);
    auto root = Root(source.get(), "ovd");
    if (!root) { legacy = xml; return true; } // Existing version 1..4 files.
    if (strcmp(CPLGetXMLValue(root, "format", ""), "C4IMap-XMLControl") ||
        strcmp(CPLGetXMLValue(root, "version", ""), "1") ||
        strcmp(CPLGetXMLValue(root, "coordinates", ""), "GeoDC-world")) return false;
    Tree result(CPLCreateXMLNode(nullptr, CXT_Element, "C4IMapOverlay"), CPLDestroyXMLNode);
    auto old = result.get();
    Attr(old, "version", "4");
    Attr(old, "coordinates", "GeoDC-world");
    Copy(root, "active", old, "active");
    for (auto info = root->psChild; info; info = info->psNext)
    {
        if (info->eType != CXT_Element) continue;
        if (!strcmp(info->pszValue, "iov")) continue; // SDK military symbols.
        if (strcmp(info->pszValue, "ovi")) return false;
        auto layer = CPLCreateXMLNode(old, CXT_Element, "Layer");
        Copy(info, "gid", layer, "id");
        Copy(info, "nm", layer, "name");
        Copy(info, "sh", layer, "visible");
        for (auto shape = info->psChild; shape; shape = shape->psNext)
        {
            if (shape->eType != CXT_Element) continue;
            if (strcmp(shape->pszValue, "ovs")) return false;
            auto item = CPLCreateXMLNode(layer, CXT_Element, "Object");
            Copy(shape, "type", item, "type");
            Copy(shape, "group", item, "group");
            for (const auto& mapping : mappings)
            {
                auto child = CPLGetXMLNode(shape, mapping.tag);
                if (child) Copy(child, mapping.name, item, mapping.oldName);
                if (child && !strcmp(mapping.oldName, "points"))
                {
                    std::string points = CPLGetXMLValue(child, "pt", "");
                    std::replace(points.begin(), points.end(), ',', ' ');
                    std::replace(points.begin(), points.end(), '|', ' ');
                    Attr(item, "points", !strcmp(CPLGetXMLValue(child, "vertices", "1"), "0") ? "" : points.c_str());
                }
            }
            Copy(item, "lineColor", item, "color");
        }
    }
    legacy = Serialize(old);
    return true;
}

std::string OverlayXmlFormat::EscapeAttributeWhitespace(const std::string& xml)
{
    std::string result;
    result.reserve(xml.size());
    char quote = 0;
    bool tag = false;
    for (char c : xml)
    {
        if (!quote && c == '<') tag = true;
        if (tag && (c == '\"' || c == '\''))
        {
            if (!quote) quote = c;
            else if (quote == c) quote = 0;
        }
        if (!quote && c == '>') tag = false;
        if (quote && c == '\n') result += "&#10;";
        else if (quote && c == '\r') result += "&#13;";
        else if (quote && c == '\t') result += "&#9;";
        else result += c;
    }
    return result;
}
