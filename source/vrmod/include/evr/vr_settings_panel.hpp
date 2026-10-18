#pragma once
// The VR settings in the game's start menu (user, 2026-10-18 after the headset run of that morning: "the crouch's
// threshold should be changeable: add a VR settings item to the menu, where real crouching is switched on and off and
// its threshold changed, even during the game"; then "the crouch depth set freely with a slider"). A button beside the VR
// bug report's (hooks/bug_report.hpp draws both on the mod's own picture over the start menu and takes the menu
// pointer's presses); pressed, the place under the buttons shows the settings instead of the bug report's note. A change
// applies at once and is written into EnshroudedVR.ini ([Body] RealCrouch, CrouchDrop - include/evr/vr_settings.hpp),
// which the loader gives to the next start (EVR_ROOM_SNEAK, EVR_CROUCH_DROP).
// (From 11:00 to 12:40 the same day it also switched off, for comparing, what the right eye takes from the left eye's
// drawing - the shadow map, the indirect light, the fog. The user after trying it: "not borrowing did not solve the
// problems; the not-borrowing setting can go" - removed.)
#include <evr/vr_settings.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
namespace evr::vrpanel {
// The crouch's threshold: how far the headset comes down, in percent of the standing eye height (the user's "30 %
// down"; include/evr/room_sneak.hpp enters under 1 - drop and leaves over that x 1.05). 10 .. 50, any whole percent.
struct Body {bool realCrouch=true;int crouchDrop=30;};
constexpr int dropLow=10,dropHigh=50;
inline int clampDrop(int drop){return std::clamp(drop,dropLow,dropHigh);}
inline float enterRatio(int drop){return 1.f-float(clampDrop(drop))/100.f;}
// Both values into the settings file's text (every other line stays).
inline std::string store(const std::string& ini,const Body& b){
    return settings::set(settings::set(ini,"Body","RealCrouch",b.realCrouch?"1":"0"),"Body","CrouchDrop",std::to_string(clampDrop(b.crouchDrop)));
}
// What a place on the picture is, and what pressing it does.
enum class Hit {None,BugReport,Settings,CrouchOn,CrouchOff,DropSlider};
inline const char* name(Hit h){
    switch(h){case Hit::BugReport:return "bug_report";case Hit::Settings:return "settings";case Hit::CrouchOn:return "crouch_on";case Hit::CrouchOff:return "crouch_off";
        case Hit::DropSlider:return "slider";default:return "none";}
}
inline Hit hitNamed(const std::string& s){
    for(const auto h:{Hit::BugReport,Hit::Settings,Hit::CrouchOn,Hit::CrouchOff,Hit::DropSlider})if(s==name(h))return h;
    return Hit::None;
}
inline Body apply(Body b,Hit h){
    if(h==Hit::CrouchOn)b.realCrouch=true;else if(h==Hit::CrouchOff)b.realCrouch=false;
    return b;
}
// The picture's places, in its own pixels (1200 x 450, drawn at about the UI's pixel size of 1920 x 1080): the two
// buttons on top, the settings' rows under them while they are open.
struct Box {Hit hit;int x0,y0,x1,y1;bool contains(int x,int y)const{return x>=x0&&x<x1&&y>=y0&&y<y1;}};
constexpr int width=1200,height=450,buttonHeight=84;
constexpr Box bugButton{Hit::BugReport,0,0,640,buttonHeight},settingsButton{Hit::Settings,660,0,980,buttonHeight};
constexpr Box crouchOn{Hit::CrouchOn,700,104,850,170},crouchOff{Hit::CrouchOff,870,104,1020,170};
constexpr Box dropSlider{Hit::DropSlider,700,184,1120,250};
constexpr int noteTop=270;
// The slider: the press (and the trigger held while the hand moves) sets the depth where the pointer is on its track;
// the ends of the box past the track give 10 % and 50 %.
constexpr int trackX0=dropSlider.x0+28,trackX1=dropSlider.x1-28;
inline int sliderDrop(int x){
    const float t=std::clamp(float(x-trackX0)/float(trackX1-trackX0),0.f,1.f);
    return clampDrop(int(std::lround(float(dropLow)+t*float(dropHigh-dropLow))));
}
inline int sliderX(int drop){return trackX0+int(std::lround(float(clampDrop(drop)-dropLow)/float(dropHigh-dropLow)*float(trackX1-trackX0)));}
inline std::vector<Box> boxes(bool open){
    std::vector<Box> out{bugButton,settingsButton};
    if(open)for(const auto& b:{crouchOn,crouchOff,dropSlider})out.push_back(b);
    return out;
}
inline Hit hitAt(int x,int y,bool open){for(const auto& b:boxes(open))if(b.contains(x,y))return b.hit;return Hit::None;}
// The texts, in Steam's game language (English for any other). A row's label is its heading, a line break, and a smaller
// explanation (two blocks: a heading and its note are drawn apart, each fitting its own height).
struct Texts {const wchar_t* button;const wchar_t* crouch;const wchar_t* on;const wchar_t* off;const wchar_t* drop;const wchar_t* note;const wchar_t* unsaved;};
inline Texts texts(const std::string& language){
    if(language=="japanese")return {L"VR設定",L"リアルしゃがみ\n頭を下げると隠密になります",L"入",L"切",L"しゃがむ深さ：%d%%\n立った時の目の高さから何%%下がったら隠密か",
        L"しゃがみの設定はすぐに反映され、次の起動にも残ります。\n立った時の目の高さ＝最後に視点をリセットした時の高さです。",
        L"（開発用の起動：この回だけ反映、保存はしません）"};
    return {L"VR settings",L"Real crouch\nLower your head to sneak",L"On",L"Off",L"Crouch depth: %d%%\nHow far below your standing eye height",
        L"Crouch settings apply at once and are kept for the next start.\nStanding eye height = the height at your last view reset.",
        L"(development start: applies now, not saved)"};
}
// Presses for a Mock trial (EVR_VR_SETTINGS_PRESSES): "frame:name;frame:name", in the order of their frames; the slider
// takes its value as "frame:slider=40".
struct TestPress {unsigned long long frame;Hit hit;int value=-1;};
inline std::vector<TestPress> testPresses(const std::string& text){
    std::vector<TestPress> out;size_t at=0;
    while(at<text.size()){
        const auto end=std::min(text.find(';',at),text.size());const auto item=settings::trim(text.substr(at,end-at));at=end+1;
        const auto colon=item.find(':');if(colon==std::string::npos)continue;
        char* stop=nullptr;const auto frame=std::strtoull(item.c_str(),&stop,10);
        auto what=settings::trim(item.substr(colon+1));int value=-1;
        if(const auto eq=what.find('=');eq!=std::string::npos){char* e=nullptr;const auto v=std::strtol(what.c_str()+eq+1,&e,10);if(e&&!*e&&e!=what.c_str()+eq+1)value=int(v);what=what.substr(0,eq);}
        const auto hit=hitNamed(what);
        if(stop==item.c_str()+colon&&frame&&hit!=Hit::None&&(hit!=Hit::DropSlider||value>=0))out.push_back({frame,hit,value});
    }
    std::stable_sort(out.begin(),out.end(),[](const TestPress& a,const TestPress& b){return a.frame<b.frame;});
    return out;
}
}
