#pragma once
#include <string>
// Convert the application persistence schema to the XMLControl ovd/ovi/ovs structure.
// Values not defined by XMLControl use explicit extension attributes (version 1).
namespace OverlayXmlFormat
{
std::string EscapeAttributeWhitespace(const std::string& xml);
bool Encode(const std::string& legacy, std::string& xml);
bool Decode(const std::string& xml, std::string& legacy);
}
