#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif
namespace evr::fovfit {
// The headset-view fit ([Picture] FitView; 2026-10-25, a player with ALVR through SteamVR: cross-eyed; with the symmetric field
// of view off, no longer cross-eyed but flickering - docs/analysis/alvr_fov_20261025.md). The eyes are drawn with the
// symmetric field of view as always; what goes to the headset is the part of that picture the runtime's own field of view
// covers, stretched to the eye image, submitted with the runtime's field of view - the image is then the runtime's own
// field of view, however the runtime or the streamer behind it reads the field of view it is given.
struct Angles {float left=0,right=0,up=0,down=0;};
struct Rect {int32_t x0=0,y0=0,x1=0,y1=0;bool valid()const{return x1>x0&&y1>y0;}};
// The part of a picture drawn with the field of view `drawn` (width x height pixels, x to the right, y down) that the field of
// view `shown` covers. A picture is linear in the tangents of its angles; a part outside the picture is cut at its edge.
inline Rect part(Angles drawn,Angles shown,uint32_t width,uint32_t height){
    const auto t=[](float a){return std::tan(a);};
    const float l=t(drawn.left),r=t(drawn.right),u=t(drawn.up),d=t(drawn.down);
    if(!(r>l)||!(u>d)||!width||!height||!std::isfinite(l+r+u+d))return {};
    const auto x=[&](float a){return int32_t(std::lround(std::clamp((t(a)-l)/(r-l),0.f,1.f)*float(width)));};
    const auto y=[&](float a){return int32_t(std::lround(std::clamp((u-t(a))/(u-d),0.f,1.f)*float(height)));};
    return {x(shown.left),y(shown.up),x(shown.right),y(shown.down)};
}
// Auto, the first sign: the headset's system name (xrGetSystemProperties) says ALVR, whatever the case. (ALVR can call its
// headset by another headset's name - not seen yet.)
inline bool alvrName(const std::string& name){
    std::string low=name;for(char& c:low)c=char(std::tolower(static_cast<unsigned char>(c)));
    return low.find("alvr")!=std::string::npos;
}
#ifdef _WIN32
// Auto, the second sign: ALVR's SteamVR driver (driver_alvr_server.dll) loaded in SteamVR's server (vrserver.exe, a process of
// the same user). 1 a process of that name has the module, 0 it runs without it, -1 no process of that name, -2 not readable.
inline int processHasModule(const wchar_t* process,const wchar_t* module){
    const HANDLE all=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(all==INVALID_HANDLE_VALUE)return -2;
    PROCESSENTRY32W p{};p.dwSize=sizeof p;int result=-1;
    if(Process32FirstW(all,&p))do{
        if(_wcsicmp(p.szExeFile,process))continue;
        if(result==-1)result=-2;
        HANDLE mods=INVALID_HANDLE_VALUE;
        for(int attempt=0;attempt<3&&mods==INVALID_HANDLE_VALUE;++attempt){   // ERROR_BAD_LENGTH while the process loads modules
            mods=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,p.th32ProcessID);if(mods==INVALID_HANDLE_VALUE&&GetLastError()!=ERROR_BAD_LENGTH)break;}
        if(mods==INVALID_HANDLE_VALUE)continue;
        MODULEENTRY32W m{};m.dwSize=sizeof m;bool found=false;
        if(Module32FirstW(mods,&m))do{if(!_wcsicmp(m.szModule,module)){found=true;break;}}while(Module32NextW(mods,&m));
        CloseHandle(mods);
        if(found){result=1;break;}
        result=0;
    }while(Process32NextW(all,&p));
    CloseHandle(all);return result;
}
#endif
} // namespace evr::fovfit
