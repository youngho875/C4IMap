#pragma once
#include <afxwin.h>
#include <map>
#include <string>
#include <utility>
#include <vector>
#pragma pack(push, 8)
class CAppPaths
{
  public:
    CAppPaths(const std::string &file, const std::string &executableDirectory);
    std::string Get(const std::string &key) const;
    std::string Resolve(const std::string &value) const;
    std::string Reference(const std::string &value) const;
    const std::string &File() const;

  private:
    std::string file, executableDirectory;
    std::map<std::string, std::string> values;
    std::vector<std::pair<std::string, std::string>> mounts;
    std::string Expand(const std::string &value, unsigned depth) const;
};
#pragma pack(pop)
CAppPaths &AppPaths();
std::string AppPath(const char *key);
CString AppPathText(const char *key);
std::wstring AppPathWide(const char *key);
std::string ResolveAppPath(const std::string &value);
