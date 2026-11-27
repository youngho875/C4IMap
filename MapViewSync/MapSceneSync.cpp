#include "MapSceneSync.h"
#include <windows.h>

#include <vector>

std::wstring CMapSceneSync::FileName()
{
    wchar_t temp[32768]={};
    if(!GetTempPathW(32768,temp))return {};
    DWORD session=0;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&session))return {};
    std::wstring folder=std::wstring(temp)+L"C4IMap-Globe3D";
    if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return {};
    folder+=L"\\"+std::to_wstring(session);
    if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return {};
    return folder+L"\\scene-v1.json";
}
void CMapSceneSync::ForcePublish(){previous_.clear();}
bool CMapSceneSync::Publish(const std::string& json,std::string& error)
{
    if(json==previous_)return true;
    const auto path=FileName();
    if(path.empty()){error="Cannot create scene exchange folder";return false;}
    const auto temp=path+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error="Cannot write scene snapshot";return false;}
    DWORD done=0;
    const bool written=json.size()<=64*1024*1024 && WriteFile(file,json.data(),(DWORD)json.size(),&done,nullptr) && done==json.size();
    CloseHandle(file);
    if(!written || !MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    {DeleteFileW(temp.c_str());error="Cannot commit scene snapshot";return false;}
    previous_=json;return true;
}
bool CMapSceneSync::ReadChanged(std::string& json,std::string& error)
{
    const auto path=FileName();
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    FILETIME modified={};LARGE_INTEGER size={};
    GetFileTime(file,nullptr,nullptr,&modified);GetFileSizeEx(file,&size);
    const uint64_t stamp=(uint64_t(modified.dwHighDateTime)<<32)|modified.dwLowDateTime;
    if(stamp==stamp_){CloseHandle(file);return false;}
    if(size.QuadPart<2 || size.QuadPart>64*1024*1024){CloseHandle(file);error="Invalid scene size";return false;}
    std::string text((size_t)size.QuadPart,'\0');DWORD got=0;
    const bool ok=ReadFile(file,&text[0],(DWORD)text.size(),&got,nullptr)&&got==text.size();
    CloseHandle(file);
    if(!ok){error="Cannot read complete scene snapshot";return false;}
    stamp_=stamp;
    if(text==previous_)return false;
    previous_=text;json=std::move(text);return true;
}
