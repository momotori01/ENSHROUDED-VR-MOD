#pragma once
#include <evr/mod_update.hpp>
#include <winhttp.h>
#pragma comment(lib,"winhttp.lib")
// Whether a newer mod is out (user, 2026-10-19: "a notice at the main menu and in the start menu when the mod has an
// update"; include/evr/mod_update.hpp). Once per game start, on a thread of its own, when the plate of a VR start is
// first made (screen_xr.hpp) or the start menu's panel first asks (evrModUpdateState): the installed version from <game>\EnshroudedVR\PACKAGE_MANIFEST.json, the newest release
// from https://api.github.com/repos/<repository>/releases/latest - the same look-up as "Enshrouded VR setting.exe" does
// at its start (package/Update-EnshroudedVR.ps1 -CheckOnly). Nothing is downloaded or changed here; a failure (no
// network, no manifest: a development start) shows nothing. evrModUpdateState: 0 unknown, 1 up to date, 2 an update.
// EVR_UPDATE_NOTICE=0: no look-up; =1: the notice shown without one (Mock trials); =2: the same, without the blink (pictures).
namespace modUpdate {
inline std::atomic<int> state{0};inline std::atomic<bool> started{false};
inline std::string readFile(const std::filesystem::path& path,size_t limit){
    std::ifstream f(path,std::ios::binary);if(!f)return {};std::string s(limit,'\0');f.read(s.data(),std::streamsize(limit));s.resize(size_t(f.gcount()));return s;
}
// GET https://api.github.com<path>: the body (up to 512 KB), or empty with the reason.
inline std::string get(const std::wstring& path,std::string& why){
    std::string body;HINTERNET session=nullptr,connection=nullptr,request=nullptr;
    session=WinHttpOpen(L"EnshroudedVR-UpdateNotice",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session){session=WinHttpOpen(L"EnshroudedVR-UpdateNotice",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);}
    if(session){
        WinHttpSetTimeouts(session,5000,5000,8000,8000);
        connection=WinHttpConnect(session,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0);
        if(connection)request=WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
        const wchar_t* headers=L"Accept: application/vnd.github+json\r\n";
        if(request&&WinHttpSendRequest(request,headers,DWORD(-1L),WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(request,nullptr)){
            DWORD status=0,size=sizeof status;
            WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);
            if(status==200){
                for(;;){DWORD n=0;if(!WinHttpQueryDataAvailable(request,&n)||!n)break;
                    const size_t at=body.size();if(at+n>512*1024){why="answer too long";body.clear();break;}
                    body.resize(at+n);DWORD read=0;if(!WinHttpReadData(request,body.data()+at,n,&read)){why="read failed";body.clear();break;}body.resize(at+read);}
            }else why="http "+std::to_string(status);
        }else why="no answer (error "+std::to_string(GetLastError())+")";
    }else why="no session";
    if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);if(session)WinHttpCloseHandle(session);
    return body;
}
inline void start() noexcept {
    if(started.exchange(true))return;
    try{
        const char* f=std::getenv("EVR_UPDATE_NOTICE");
        if(f&&!std::strcmp(f,"0")){note("mod_update_check","\"skipped\":\"EVR_UPDATE_NOTICE=0\"");return;}
        if(f&&(!std::strcmp(f,"1")||!std::strcmp(f,"2"))){const auto s=f[0]=='2'?evr::modupdate::State::AvailableSteady:evr::modupdate::State::Available;
            state=int(s);note("mod_update_check","\"state\":"+std::to_string(int(s))+",\"test\":true");return;}
        wchar_t exe[MAX_PATH*2]{};const DWORD n=GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));if(!n||n>=std::size(exe))return;
        const auto manifest=std::filesystem::path(exe).parent_path()/L"EnshroudedVR"/L"PACKAGE_MANIFEST.json";
        std::thread([manifest]{
            try{
                const auto installed=evr::modupdate::installed(readFile(manifest,1024*1024));
                if(!installed.version){note("mod_update_check","\"state\":0,\"reason\":\"no installed version\",\"manifest\":"+jsonString(manifest.string()));return;}
                std::string why;const auto path=L"/repos/"+std::wstring(installed.repository.begin(),installed.repository.end())+L"/releases/latest";
                const auto body=get(path,why);
                const auto result=body.empty()?evr::modupdate::State::Unknown:evr::modupdate::compare(installed,body);
                state=int(result);
                const auto tag=body.empty()?std::optional<std::string>{}:evr::modupdate::stringField(body,"tag_name");
                note("mod_update_check","\"state\":"+std::to_string(int(result))+",\"installed\":"+jsonString(evr::modupdate::text(*installed.version))+",\"latest_tag\":"+(tag?jsonString(*tag):std::string("null"))+
                    ",\"repository\":"+jsonString(installed.repository)+(why.empty()?std::string():",\"reason\":"+jsonString(why)));
            }catch(...){}
        }).detach();
    }catch(...){}
}
}
// Asked by the start menu's panel (hooks/bug_report.hpp) too: a start without the plate (EVR_SCREEN_XR off, a development
// start) looks it up from there (Mock game-20261019-174418: no plate, no look-up, no notice).
extern "C" __declspec(dllexport) int evrModUpdateState() noexcept {modUpdate::start();return modUpdate::state.load();}
