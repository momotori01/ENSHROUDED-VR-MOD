#pragma once
// The in-game notice of a newer mod (user, 2026-10-19: "at the game's main menu at start, and where the bug report is in
// the left stick's menu, a notice when there is an update of the mod: white letters 'MOD Update', not standing out too
// much, a size that keeps the mood, blinking slowly so that it is seen - like the present 'VR Loading...'").
// The pure parts: the installed version and repository from the package's manifest (EnshroudedVR\PACKAGE_MANIFEST.json,
// tools/pack_release.ps1), the newest release's version from GitHub's answer (tag "EnshroudedVR-<x.y.z>", as
// package/Update-EnshroudedVR.ps1 reads it), their comparison, and the slow blink. The look-up itself is
// layer/mod_update_check.hpp; the notice is drawn by layer/screen_xr.hpp (the main menu's plate) and hooks/bug_report.hpp
// (the start menu's panel).
#include <array>
#include <cstdint>
#include <optional>
#include <string>
namespace evr::modupdate {
inline constexpr const char* noticeText="MOD Update";inline constexpr const wchar_t* wideNotice=L"MOD Update";
inline constexpr const char* defaultRepository="momotori01/ENSHROUDED-VR-MOD";
// The slow blink: 1.2 s shown, 0.8 s not ("VR Now Loading..." blinks 0.5 / 0.5).
inline constexpr uint64_t blinkPeriodMs=2000,blinkShownMs=1200;
inline bool blinkShown(uint64_t nowMs){return nowMs%blinkPeriodMs<blinkShownMs;}

using Version=std::array<int,3>;
// "x.y.z" (digits only, each part below 100000); anything else is no version.
inline std::optional<Version> parseVersion(const std::string& text){
    Version v{};size_t at=0;
    for(int part=0;part<3;++part){
        size_t digits=0;long long value=0;
        while(at<text.size()&&text[at]>='0'&&text[at]<='9'&&digits<6){value=value*10+(text[at]-'0');++at;++digits;}
        if(!digits||value>=100000)return std::nullopt;
        v[size_t(part)]=int(value);
        if(part<2){if(at>=text.size()||text[at]!='.')return std::nullopt;++at;}
    }
    if(at!=text.size())return std::nullopt;
    return v;
}
inline bool newer(const Version& latest,const Version& installed){return latest>installed;}
inline std::string text(const Version& v){return std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]);}
// The value of the first "key": "value" in a JSON text (strings without escapes: versions, tags, repositories).
inline std::optional<std::string> stringField(const std::string& json,const std::string& key){
    const std::string quoted="\""+key+"\"";size_t at=json.find(quoted);
    while(at!=std::string::npos){
        size_t p=at+quoted.size();
        while(p<json.size()&&(json[p]==' '||json[p]=='\t'||json[p]=='\r'||json[p]=='\n'))++p;
        if(p<json.size()&&json[p]==':'){
            ++p;while(p<json.size()&&(json[p]==' '||json[p]=='\t'||json[p]=='\r'||json[p]=='\n'))++p;
            if(p>=json.size()||json[p]!='"')return std::nullopt;
            const size_t end=json.find('"',p+1);if(end==std::string::npos)return std::nullopt;
            const auto value=json.substr(p+1,end-p-1);if(value.find('\\')!=std::string::npos)return std::nullopt;
            return value;
        }
        at=json.find(quoted,at+1);
    }
    return std::nullopt;
}
// The release's tag "EnshroudedVR-x.y.z" (the updater refuses any other name).
inline std::optional<Version> tagVersion(const std::string& tag){
    const std::string prefix="EnshroudedVR-";
    if(tag.rfind(prefix,0)!=0)return std::nullopt;
    return parseVersion(tag.substr(prefix.size()));
}
// "owner/name" of letters, digits, '_', '.', '-' (as the updater accepts it).
inline bool repositoryValid(const std::string& r){
    const auto slash=r.find('/');if(slash==std::string::npos||slash==0||slash+1>=r.size()||r.find('/',slash+1)!=std::string::npos||r.size()>200)return false;
    for(char c:r)if(!(c=='/'||c=='_'||c=='.'||c=='-'||(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')))return false;
    return true;
}
struct Installed {std::optional<Version> version;std::string repository=defaultRepository;};
inline Installed installed(const std::string& manifest){
    Installed i;
    if(const auto v=stringField(manifest,"version"))i.version=parseVersion(*v);
    if(const auto r=stringField(manifest,"repository");r&&repositoryValid(*r))i.repository=*r;
    return i;
}
// What the look-up concluded: Unknown (not looked up yet, failed, or no installed version), UpToDate, Available;
// AvailableSteady only for pictures of the tests (EVR_UPDATE_NOTICE=2): shown without the blink.
enum class State:int {Unknown=0,UpToDate=1,Available=2,AvailableSteady=3};
// Whether the notice is drawn now for a state (layer evrModUpdateState).
inline bool noticeShown(int state,uint64_t nowMs){return state==int(State::AvailableSteady)||(state==int(State::Available)&&blinkShown(nowMs));}
inline State compare(const Installed& i,const std::string& releaseJson){
    if(!i.version)return State::Unknown;
    const auto tag=stringField(releaseJson,"tag_name");if(!tag)return State::Unknown;
    const auto latest=tagVersion(*tag);if(!latest)return State::Unknown;
    return newer(*latest,*i.version)?State::Available:State::UpToDate;
}
}
