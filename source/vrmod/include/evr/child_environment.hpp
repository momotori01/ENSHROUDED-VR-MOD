#pragma once
// The environment a program started from inside the game gets (2026-11-12, user: "EDF6VR did not start; the Steam this
// mod had opened was the cause - make it not affect other games").
// The loader sets the mod's variables in the game's own process (the Vulkan layer, OpenXR, EVR_*). A process the game
// starts inherits them: on 2026-11-11 the game, started after Steam had gone down in a RenderDoc test, started Steam
// again itself (the Steam API does that when Steam is not running), and that Steam handed the test's variables
// (the mock OpenXR runtime, the mod's and RenderDoc's Vulkan layers) to every game and to SteamVR it started until it
// was closed. SteamVR started by the game's OpenXR call would get them the same way.
// So a program started from the game gets the environment without the mod: every EVR_* variable left out, and each
// variable the loader changed as it was before (or left out if it was not there). The mod's own programs (in the
// game's folder, such as "Enshrouded VR setting.exe" for the bug report) get the environment as it is.
// Pure functions here (tests/child_environment_tests.cpp); the hook on the system's process creation is in loader.cpp.
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
namespace evr::childenv {
struct Original {std::wstring name;bool present=false;std::wstring value;};   // a variable before the loader changed it
inline bool sameName(const std::wstring& a,const std::wstring& b){
    return CompareStringOrdinal(a.c_str(),int(a.size()),b.c_str(),int(b.size()),TRUE)==CSTR_EQUAL;
}
// "NAME=value"; a name may begin with '=' (the drives' folders, "=C:=C:\...").
inline std::wstring nameOf(const std::wstring& entry){
    const auto at=entry.find(L'=',entry.empty()?0:1);return at==std::wstring::npos?entry:entry.substr(0,at);
}
inline bool modOwned(const std::wstring& name){
    return name.size()>=4&&CompareStringOrdinal(name.c_str(),4,L"EVR_",4,TRUE)==CSTR_EQUAL;
}
// The entries of an environment block ("A=1\0B=2\0\0"); limit: a block is never longer than 32767 entries' worth here.
inline std::vector<std::wstring> entries(const wchar_t* block){
    std::vector<std::wstring> out;if(!block)return out;
    for(const wchar_t* p=block;*p&&out.size()<65536;){const std::wstring e(p);out.push_back(e);p+=e.size()+1;}
    return out;
}
inline std::vector<std::wstring> entriesAnsi(const char* block){
    std::vector<std::wstring> out;if(!block)return out;
    for(const char* p=block;*p&&out.size()<65536;){const std::string e(p);p+=e.size()+1;
        const int n=MultiByteToWideChar(CP_ACP,0,e.data(),int(e.size()),nullptr,0);std::wstring w(size_t(n>0?n:0),L'\0');
        if(n>0)MultiByteToWideChar(CP_ACP,0,e.data(),int(e.size()),w.data(),n);out.push_back(w);}
    return out;
}
// The entries without the mod; *changed counts what was left out, put back or given its old value.
inline std::vector<std::wstring> cleaned(const std::vector<std::wstring>& in,const std::vector<Original>& originals,size_t* changed=nullptr){
    std::vector<std::wstring> out;std::vector<bool> seen(originals.size(),false);size_t count=0;
    for(const auto& e:in){const auto name=nameOf(e);
        if(name.empty()||name[0]==L'='){out.push_back(e);continue;}
        if(modOwned(name)){++count;continue;}
        bool handled=false;
        for(size_t i=0;i<originals.size();++i)if(sameName(originals[i].name,name)){seen[i]=true;handled=true;
            if(!originals[i].present){++count;break;}
            const auto before=name+L"="+originals[i].value;if(before!=e)++count;out.push_back(before);break;}
        if(!handled)out.push_back(e);}
    for(size_t i=0;i<originals.size();++i)if(!seen[i]&&originals[i].present&&!modOwned(originals[i].name)){out.push_back(originals[i].name+L"="+originals[i].value);++count;}
    if(changed)*changed=count;
    return out;
}
// A block for CreateProcess (CREATE_UNICODE_ENVIRONMENT): sorted by name without regard to case, as Windows keeps it.
inline std::vector<wchar_t> block(std::vector<std::wstring> list){
    std::stable_sort(list.begin(),list.end(),[](const std::wstring& a,const std::wstring& b){
        const auto x=nameOf(a),y=nameOf(b);return CompareStringOrdinal(x.c_str(),int(x.size()),y.c_str(),int(y.size()),TRUE)==CSTR_LESS_THAN;});
    std::vector<wchar_t> out;for(const auto& e:list){out.insert(out.end(),e.begin(),e.end());out.push_back(L'\0');}
    if(out.empty())out.push_back(L'\0');
    out.push_back(L'\0');return out;
}
// The program a CreateProcess call starts: the application name, or the command line's first word (quoted or not).
inline std::wstring programOf(const wchar_t* application,const wchar_t* commandLine){
    if(application&&*application)return application;
    if(!commandLine)return {};
    const wchar_t* p=commandLine;while(*p==L' '||*p==L'\t')++p;
    if(*p==L'"'){const wchar_t* end=wcschr(p+1,L'"');return end?std::wstring(p+1,end):std::wstring(p+1);}
    const wchar_t* end=p;while(*end&&*end!=L' '&&*end!=L'\t')++end;return std::wstring(p,end);
}
// Is path inside folder (no separator at its end)? Without regard to case, '/' taken as '\'.
inline bool insideFolder(std::wstring path,std::wstring folder){
    if(folder.empty()||path.size()<=folder.size())return false;
    std::replace(path.begin(),path.end(),L'/',L'\\');std::replace(folder.begin(),folder.end(),L'/',L'\\');
    if(folder.back()==L'\\')folder.pop_back();
    if(path[folder.size()]!=L'\\')return false;
    return CompareStringOrdinal(path.c_str(),int(folder.size()),folder.c_str(),int(folder.size()),TRUE)==CSTR_EQUAL;
}
// The file name of a path, for the log (no folders: they can hold the player's name).
inline std::wstring fileName(const std::wstring& path){const auto at=path.find_last_of(L"\\/");return at==std::wstring::npos?path:path.substr(at+1);}
} // namespace evr::childenv
