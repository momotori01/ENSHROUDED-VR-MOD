#pragma once
// The parts of "Enshrouded VR setting.exe" that need no window, kept here so they can be tested: EnshroudedVR.ini
// read and changed line by line - every comment and every other line stays as it is - and the values the loader
// (loader/loader.cpp, at the game's start) takes from it, held to what the mod accepts.
// User, 2026-10-14: "make a VRsetting.exe as EDF6VR has; VR mode is switched on and off there." / "VR is off by
// default, switched on in the exe" / "the other VR players' movements are seen with VR on or off".
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
namespace evr::settings {
inline std::string trim(const std::string& s){
    const auto first=s.find_first_not_of(" \t\r\n");if(first==std::string::npos)return {};
    return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
inline bool sameText(const std::string& a,const std::string& b){
    return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](char x,char y){return std::tolower(static_cast<unsigned char>(x))==std::tolower(static_cast<unsigned char>(y));});
}
struct Line {std::string text,end;};          // a line and its own end ("\r\n", "\n" or none)
inline std::vector<Line> lines(const std::string& bytes){
    std::vector<Line> out;size_t at=0;
    while(at<bytes.size()){const auto end=bytes.find('\n',at);const size_t stop=end==std::string::npos?bytes.size():end;
        size_t text=stop;if(text>at&&bytes[text-1]=='\r')--text;
        out.push_back({bytes.substr(at,text-at),end==std::string::npos?std::string():bytes.substr(text,end+1-text)});
        at=end==std::string::npos?bytes.size():end+1;}
    return out;
}
inline std::string sectionOf(const std::string& line){const auto t=trim(line);return t.size()>2&&t.front()=='['&&t.back()==']'?trim(t.substr(1,t.size()-2)):std::string();}
// key=value of a line that is not a comment; false for anything else.
inline bool keyValue(const std::string& line,std::string& key,std::string& value){
    const auto t=trim(line);if(t.empty()||t[0]==';'||t[0]=='#'||t[0]=='[')return false;
    const auto eq=t.find('=');if(eq==std::string::npos)return false;
    key=trim(t.substr(0,eq));value=trim(t.substr(eq+1));
    const auto comment=value.find(';');if(comment!=std::string::npos)value=trim(value.substr(0,comment));
    return !key.empty();
}
inline std::string get(const std::string& ini,const std::string& section,const std::string& key,const std::string& fallback={}){
    std::string current,k,v;
    for(const auto& line:lines(ini)){const auto s=sectionOf(line.text);if(!s.empty()){current=s;continue;}
        if(sameText(current,section)&&keyValue(line.text,k,v)&&sameText(k,key))return v;}
    return fallback;
}
// The value on its own line is replaced (what stands before the '=' and a comment behind the value stay); a key or a
// section that is not there is added at the section's end / the file's end. Nothing else changes.
inline std::string set(const std::string& ini,const std::string& section,const std::string& key,const std::string& value){
    auto all=lines(ini);std::string current,k,v;int sectionEnd=-1;bool inSection=false;
    const std::string eol=ini.find('\n')==std::string::npos||ini.find("\r\n")!=std::string::npos?"\r\n":"\n";   // a file without any line end: as Windows writes them
    for(size_t i=0;i<all.size();i++){const auto s=sectionOf(all[i].text);
        if(!s.empty()){current=s;inSection=sameText(current,section);if(inSection)sectionEnd=int(i);continue;}
        if(!inSection)continue;
        if(!trim(all[i].text).empty())sectionEnd=int(i);
        if(keyValue(all[i].text,k,v)&&sameText(k,key)){
            const auto eq=all[i].text.find('=');const auto rest=all[i].text.substr(eq+1);const auto comment=rest.find(';');
            // The blanks between the value and its comment stay as they were (a value of the same length: the same bytes).
            std::string tail;if(comment!=std::string::npos){const auto before=rest.substr(0,comment);const auto last=before.find_last_not_of(" \t");
                tail=before.substr(last==std::string::npos?0:last+1)+rest.substr(comment);if(tail.front()==';')tail="   "+tail;}
            all[i].text=all[i].text.substr(0,eq+1)+value+tail;
            std::string out;for(const auto& line:all)out+=line.text+line.end;return out;}
    }
    std::string out;
    if(sectionEnd<0){for(const auto& line:all)out+=line.text+line.end;
        if(!out.empty()&&out.back()!='\n')out+=eol;
        return out+"["+section+"]"+eol+key+"="+value+eol;}
    for(size_t i=0;i<all.size();i++){out+=all[i].text+(all[i].end.empty()&&int(i)==sectionEnd?eol:all[i].end);if(int(i)==sectionEnd)out+=key+"="+value+eol;}
    return out;
}
// Every key=value of a section, in the file's order.
inline std::vector<std::pair<std::string,std::string>> entries(const std::string& ini,const std::string& section){
    std::vector<std::pair<std::string,std::string>> out;std::string current,k,v;
    for(const auto& line:lines(ini)){const auto s=sectionOf(line.text);if(!s.empty()){current=s;continue;}
        if(sameText(current,section)&&keyValue(line.text,k,v))out.emplace_back(k,v);}
    return out;
}
// What the loader takes. Anything unreadable or out of range gives the value it came with.
// online: the other players' VR bodies ([Online] Sync; on unless it is switched off for looking into a problem).
// keepLogs: how many of the newest log folders stay ([Log] Keep; 0 = all).
// alternateEyes: one eye drawn a frame, the other shows its last picture ([Picture] AlternateEyes; 2026-10-14, user:
// "try the PureDark way"; hooks/view_resource_probe.cpp alternateEyes). Off unless switched on.
// giQuality: the game's dynamicGi while in VR ([Picture] GiQuality; 2026-10-15). Each eye computes its own indirect
// light and the two converge on their own; on the room route of the self-tests (run_route_check.ps1) High left the
// eyes about 40 % less apart than Performance (1.05 / 1.66 against 1.93 / 2.44), in the corners and under the bed most of
// all (user: "the brightness differs, the bed and the window by it"). Keep = the game's own setting.
// eyeWarp: with alternateEyes, 1 = the eye left out shows its last picture warped to the frame's pose by its depth,
// 2 = it is made from the other eye's picture of the frame, its own last picture only in the holes ([Picture] EyeWarp;
// 2026-10-15, PureDark's Alternate Frame Warping, include/evr/eye_warp.hpp). 0 unless set.
// realCrouch / crouchDrop: sneaking by crouching and how far the headset comes down for it, in percent of the standing
// eye height ([Body] RealCrouch, CrouchDrop; 2026-10-18, user: "the crouch's threshold should be changeable - a VR
// settings item in the menu where real crouching is switched on and off and the threshold changed, even during the
// game"). The game's own menu writes them (hooks/bug_report.hpp, include/evr/vr_settings_panel.hpp); on, 30 % unless set.
// swingSpeed: the speed a stroke needs to attack, in percent of the mod's own (include/evr/live_melee.hpp swingSpeed;
// [Melee] SwingSpeed, 50..150 in steps of 5; 2026-10-20, user: "the threshold a normal attack needs - changeable with a
// slider, in the game and in the exe"). armSwim: swimming by the arms' strokes, as VHVR's gestured locomotion
// ([Body] ArmSwim; 2026-10-20, user: "add swimming by hand - VHVR had it; switchable in the settings"). On unless set.
// hitStop: the hit stop of a melee stroke (include/evr/hit_stop.hpp; [Melee] HitStop; 2026-10-20, user: "make the hit stop
// switchable in the game's settings"). On unless set.
// uiAutoHide: the HUD fades out five seconds after its last change (hooks: uiHudBridge, layer hud_fade.hpp; [Picture]
// UiAutoHide; 2026-10-23, user: "make the always-shown UI switchable in the settings" - a player found the HUD coming and
// going hard to play with). On unless set; 0 = the HUD always shown.
// afwRenderScale / afwDlss / afwPacing (2026-10-23; user, after the headset's frames.csv: "the AFW shakes" - at 75 Hz the
// alternate eyes took 19 ms, the runtime going between 75 and 37.5 frames a second): with alternateEyes, the picture size
// and the DLSS mode of the VR start ([Picture] AfwRenderScale, AfwDlss; 0 / empty = RenderScale, the game's own mode) and how the
// frames are paced ([Picture] AfwPacing): 0 every refresh the frame can make (the runtime fills the misses), 1 a steady
// two refreshes a frame (half the headset's rate, no switching - room for a larger picture), 2 the mod's own warp of the
// last pictures for every refresh the game misses (include/evr/afw_pacing.hpp).
// style (2026-10-25, user: "the play mode switchable: the full VR of now, the camera only in first person with the Xbox pad
// played as usual, the third person view with the pad, and the ordinary desktop mode"): with vr, Motion = the motion
// controllers (all of the mod), PadFirst = the headset's view in first person and the game's own gamepad play (no hand
// tracking; the head-bob removal of the first person stays, user), PadThird = the game's own third-person camera in the
// headset, played with the pad ([Mode] Style). padPitchLock: in PadThird the view keeps level, the game camera's pitch
// (the right stick's up and down) leaves it ([Pad] PitchLock; user: "switchable in the game and in the exe").
// padTilt: the pad modes' view tilted down by this many degrees, 0 .. 90, for playing lying down ([Pad] Tilt; user: "at 90,
// lying on your back the camera faces forward"; "only for lazing with the controller - not needed in the full VR").
// fitView (2026-10-25, a player with ALVR through SteamVR: cross-eyed; docs/analysis/alvr_fov_20261025.md): the picture handed
// to the headset is cut to the headset's own field of view and stretched to its image, and submitted with that field of view
// (include/evr/openxr_client.hpp runtimeFovFit), for a runtime that shows the image as its own field of view whatever field of
// view it is given. Three choices (2026-10-25, user: "Auto, the present one, and one for ALVR or Meta when cross-eyed"; the
// item is "View fit", not a runtime): Auto - the mod fits only when it finds ALVR (the headset's name, or ALVR's driver in
// SteamVR's server: include/evr/fov_fit.hpp); Normal - the picture as always; Fit - always fitted ([Picture] FitView; the
// earlier forms' ALVR / On read as Fit, SteamVR / Off as Normal).
enum class FitView {Auto,Normal,Fit};
inline const std::vector<std::string>& fitViewNames(){static const std::vector<std::string> names{"Auto","Normal","Fit"};return names;}
enum class Style {Motion,PadFirst,PadThird};
inline const std::vector<std::string>& styleNames(){static const std::vector<std::string> names{"Motion","PadFirst","PadThird"};return names;}
constexpr int padTiltLow=0,padTiltHigh=90;
struct Values {bool vr=false,online=true;double renderScale=.8;std::string dlss;double sharpness=.2;int keepLogs=8;bool alternateEyes=false;std::string giQuality="High";int eyeWarp=0;
    bool realCrouch=true;int crouchDrop=30;int swingSpeed=100;bool armSwim=true;bool hitStop=true;bool uiAutoHide=true;
    double afwRenderScale=0;std::string afwDlss;int afwPacing=0;
    Style style=Style::Motion;bool padPitchLock=true;int padTilt=0;FitView fitView=FitView::Auto;};
inline bool padStyle(const Values& v){return v.style!=Style::Motion;}
constexpr int swingSpeedLow=50,swingSpeedHigh=150,swingSpeedStep=5;
inline int clampSwingSpeed(int percent){
    const int stepped=int(std::lround(double(percent)/swingSpeedStep))*swingSpeedStep;return std::clamp(stepped,swingSpeedLow,swingSpeedHigh);
}
inline const std::vector<std::string>& giModes(){static const std::vector<std::string> modes{"Keep","Performance","Balance","High","Ultra"};return modes;}
inline const std::vector<std::string>& dlssModes(){static const std::vector<std::string> modes{"Dlaa","UltraQuality","Quality","Balanced","Performance","UltraPerformance"};return modes;}
inline Values read(const std::string& ini){
    Values d,out;
    const auto flag=[&](const char* section,const char* key,bool fallback){const auto v=get(ini,section,key);return v=="1"?true:v=="0"?false:fallback;};
    const auto number=[&](const char* section,const char* key,double fallback,double low,double high){const auto v=get(ini,section,key);
        char* end=nullptr;const double x=std::strtod(v.c_str(),&end);return !v.empty()&&end&&!*end&&std::isfinite(x)&&x>=low&&x<=high?x:fallback;};
    out.vr=flag("Mode","VR",d.vr);out.online=flag("Online","Sync",d.online);out.alternateEyes=flag("Picture","AlternateEyes",d.alternateEyes);out.eyeWarp=int(number("Picture","EyeWarp",d.eyeWarp,0.,2.));
    out.renderScale=number("Picture","RenderScale",d.renderScale,.25,2.);out.sharpness=number("Picture","Sharpness",d.sharpness,0.,1.);
    out.keepLogs=int(number("Log","Keep",d.keepLogs,0.,100.));
    out.realCrouch=flag("Body","RealCrouch",d.realCrouch);out.crouchDrop=int(number("Body","CrouchDrop",d.crouchDrop,10.,50.));
    out.armSwim=flag("Body","ArmSwim",d.armSwim);out.uiAutoHide=flag("Picture","UiAutoHide",d.uiAutoHide);out.hitStop=flag("Melee","HitStop",d.hitStop);out.swingSpeed=clampSwingSpeed(int(number("Melee","SwingSpeed",d.swingSpeed,swingSpeedLow,swingSpeedHigh)));
    // [Picture] Dlss is no longer read (2026-10-23, user: "the setting exe's DLSS is the same as the game's - remove it"):
    // dlss stays empty, a VR start keeps the game's own DLSS / FSR 3 mode; only AFW's own mode (AfwDlss) is still set.
    const auto gi=get(ini,"Picture","GiQuality");for(const auto& known:giModes())if(sameText(known,gi))out.giQuality=known;
    out.afwRenderScale=number("Picture","AfwRenderScale",0.,.25,2.);if(get(ini,"Picture","AfwRenderScale")=="0")out.afwRenderScale=0;
    {const auto m=get(ini,"Picture","AfwDlss");for(const auto& known:dlssModes())if(sameText(known,m))out.afwDlss=known;}
    out.afwPacing=int(number("Picture","AfwPacing",d.afwPacing,0.,2.));
    {const auto st=get(ini,"Mode","Style");for(size_t i=0;i<styleNames().size();++i)if(sameText(styleNames()[i],st))out.style=Style(i);}
    out.padPitchLock=flag("Pad","PitchLock",d.padPitchLock);out.padTilt=int(std::lround(number("Pad","Tilt",d.padTilt,padTiltLow,padTiltHigh)));
    {const auto fv=get(ini,"Picture","FitView");
     if(sameText(fv,"Fit")||sameText(fv,"ALVR")||sameText(fv,"On"))out.fitView=FitView::Fit;
     else if(sameText(fv,"Normal")||sameText(fv,"SteamVR")||sameText(fv,"Off"))out.fitView=FitView::Normal;}
    return out;
}
// The values a VR start uses: with the alternate eyes their own picture size and DLSS mode, when set.
inline Values forStart(Values v){if(v.alternateEyes){if(v.afwRenderScale>0)v.renderScale=v.afwRenderScale;if(!v.afwDlss.empty())v.dlss=v.afwDlss;}return v;}
inline std::string text(double v){char b[32];std::snprintf(b,sizeof b,"%.2f",v);std::string s=b;while(s.size()>3&&s.back()=='0')s.pop_back();return s;}
} // namespace evr::settings
