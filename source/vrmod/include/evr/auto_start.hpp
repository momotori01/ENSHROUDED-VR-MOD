#pragma once
// The start by itself (user, 2026-10-14: "started from the Steam launcher as usual, the game starts with the settings
// of last time", "the package works by dropping its contents into the game's folder", "VR is off by default and
// switched on in the exe", "the other VR players' movements are seen with VR on or off").
// The loader library (loader/loader.cpp, dinput8.dll beside the game) does at the game's start what the development
// scripts did around it (launch_diagnostics.ps1, run_play_session.ps1, run_render_resource_probe.py). The parts of it
// that need no running game are kept here so they are tested: the environment of a VR start, the game's own render
// settings for VR (changed and put back), the files of a session that depend on the settings, and the reading of the
// unused proof's log.
#include <evr/vr_settings.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace evr::autostart {
// ---- the game's enshrouded_local.json: "name": "value", changed only where it stands exactly once ----
inline bool field(const std::string& text,const std::string& name,size_t& begin,size_t& end){
    const std::string key="\""+name+"\"";size_t found=0;
    for(size_t at=text.find(key);at!=std::string::npos;at=text.find(key,at+1)){
        size_t i=at+key.size();
        while(i<text.size()&&(text[i]==' '||text[i]=='\t'))++i;
        if(i>=text.size()||text[i]!=':')continue;
        ++i;while(i<text.size()&&(text[i]==' '||text[i]=='\t'||text[i]=='\r'||text[i]=='\n'))++i;
        if(i>=text.size()||text[i]!='"')continue;
        const auto close=text.find('"',i+1);if(close==std::string::npos)continue;
        ++found;begin=i+1;end=close;
    }
    return found==1;
}
// The same for a value of true or false without quotes ("enableHdr": true).
inline bool flag(const std::string& text,const std::string& name,size_t& begin,size_t& end){
    const std::string key="\""+name+"\"";size_t found=0;
    for(size_t at=text.find(key);at!=std::string::npos;at=text.find(key,at+1)){
        size_t i=at+key.size();
        while(i<text.size()&&(text[i]==' '||text[i]=='\t'))++i;
        if(i>=text.size()||text[i]!=':')continue;
        ++i;while(i<text.size()&&(text[i]==' '||text[i]=='\t'||text[i]=='\r'||text[i]=='\n'))++i;
        for(const char* v:{"true","false"}){const std::string w=v;
            if(text.compare(i,w.size(),w)==0&&(i+w.size()>=text.size()||!std::isalnum(static_cast<unsigned char>(text[i+w.size()])))){++found;begin=i;end=i+w.size();break;}}
    }
    return found==1;
}
inline bool word(const std::string& s){if(s.empty()||s.size()>40)return false;for(const char c:s)if(!std::isalnum(static_cast<unsigned char>(c)))return false;return true;}
struct Change {std::string name,original,applied;};
struct RenderRecord {std::vector<Change> changes;};
inline std::string recordText(const RenderRecord& r){std::string out="EVR_RENDER_SETTINGS_1\r\n";for(const auto& c:r.changes)out+=c.name+" "+c.original+" "+c.applied+"\r\n";return out;}
inline bool parseRecord(const std::string& text,RenderRecord& out){
    out.changes.clear();const auto all=settings::lines(text);
    if(all.empty()||settings::trim(all[0].text)!="EVR_RENDER_SETTINGS_1")return false;
    for(size_t i=1;i<all.size();i++){const auto t=settings::trim(all[i].text);if(t.empty())continue;
        const auto a=t.find(' '),b=t.rfind(' ');if(a==std::string::npos||a==b)return false;
        Change c{t.substr(0,a),t.substr(a+1,b-a-1),t.substr(b+1)};
        if(!word(c.name)||!word(c.original)||!word(c.applied))return false;
        out.changes.push_back(c);}
    return true;
}
// A VR start's own changes to the game's settings (2026-10-25, user: "the recommended button is there - everything but HDR
// the player switches themselves"; a player: "it keeps resetting my indirect lighting to Quality though I set High
// Quality"): only HDR is turned off - on, the game's final picture is not the 8-bit RGBA the eyes are taken from and VR
// stops (2026-10-23). And the upscaler (forceDlss, the loader always - user, 2026-10-25: "nothing better than FSR 3 and
// DLSS? then set that by itself; Quality, Performance and so on stay free"): antiAliasing Dlss (a DLAA stays; a card
// without DLSS gets FSR 3 from the game itself, "Skipping DLSS on non-Nvidia GPU", with the player's fsr3 preset). The
// vertical sync goes off with it. The DLSS / FSR 3 mode and GI stay the player's own; the recommended-graphics button sets them
// (include/evr/recommended_graphics.hpp). Before 2026-10-25 GI was set High too (giQuality - the loader gives none now).
// The loader gives no mode (AfwDlss is not set any more, 2026-10-25); the alternate eyes' warp takes the depth and the
// motion from DLSS's own evaluation (hooks/dlss_eval_observer.hpp) - DLSS is set for every VR start anyway.
// What a VR start writes is recorded and put back at the next start without VR. An earlier start's record that was not
// put back yet: its fields that still hold what it wrote go back to the player's own first (as at a start without VR -
// an older version's DLSS and GI), HDR keeps that record's original; what the player changed since stays.
inline std::string fsr3Preset(const std::string& dlssMode){
    if(dlssMode=="Dlaa")return "Native";if(dlssMode=="Balanced")return "Balance";
    if(dlssMode=="Quality"||dlssMode=="Performance"||dlssMode=="UltraPerformance")return dlssMode;
    return "";
}
inline bool applyVr(const std::string& text,const std::string& mode,const RenderRecord* earlier,std::string& out,RenderRecord& record,std::string& reason,const std::string& giQuality="",bool forceDlss=false){
    out=text;record.changes.clear();
    size_t b=0,e=0;
    if(earlier)for(const auto& c:earlier->changes){
        if(c.name=="enableHdr"||c.applied==c.original||!word(c.original))continue;
        if(field(out,c.name,b,e)&&out.substr(b,e-b)==c.applied)out=out.substr(0,b)+c.original+out.substr(e);}
    const auto original=[&](const std::string& name,const std::string& now){
        if(earlier)for(const auto& c:earlier->changes)if(c.name==name&&c.applied==now)return c.original;
        return now;};
    const auto put=[&](const std::string& name,const std::string& value){
        if(!field(out,name,b,e)){reason="the game's setting '"+name+"' was not found exactly once";return false;}
        const auto now=out.substr(b,e-b);if(!word(now)){reason="the game's setting '"+name+"' has an unexpected value";return false;}
        record.changes.push_back({name,original(name,now),value});out=out.substr(0,b)+value+out.substr(e);return true;};
    if(forceDlss||!mode.empty()){
        if(!field(out,"dlss",b,e)){reason="the game's setting 'dlss' was not found exactly once";return false;}
        const auto dlssNow=out.substr(b,e-b);
        // No mode: the game's own mode stays, and the player's DLAA with it.
        size_t ab=0,ae=0;const bool dlaaNow=field(out,"antiAliasing",ab,ae)&&out.substr(ab,ae-ab)=="Dlaa";
        if(!put("antiAliasing",mode=="Dlaa"||(mode.empty()&&dlaaNow)?"Dlaa":"Dlss"))return false;
        // DLAA belongs to antiAliasing; the upscaling mode the player chose stays for when they go back to DLSS.
        if(!put("dlss",mode=="Dlaa"||mode.empty()?dlssNow:mode))return false;
        // A graphics card without DLSS (2026-10-20, a guest's report: AMD Radeon RX 9070 XT, "Skipping DLSS on non-Nvidia
        // GPU") draws with FSR 3 and the player's fsr3 preset instead. The same scale for FSR 3 (keen::pc::RenderFsr3PresetSetting:
        // UltraPerformance, Performance, Balance, Quality, Native - "Balance", not "Balanced"); DLAA is FSR 3's Native.
        if(field(out,"fsr3",b,e)){const auto fsr=fsr3Preset(mode);if(!fsr.empty()&&!put("fsr3",fsr))return false;}
    }
    // HDR (2026-10-23, a player's report, Apple Vision Pro through SteamVR, the game's HDR on): the game's final picture
    // is then not the 8-bit RGBA the mod takes the eyes from ("unsupported calibration target"), and the VR session
    // stopped 10 s into the world. A VR start turns the game's HDR off; put back at the next start without VR. A file
    // without the field, or with HDR off, is left. (A second VR start before it was put back keeps the first start's original.)
    if(flag(out,"enableHdr",b,e)){const auto now=out.substr(b,e-b),was=original("enableHdr",now);
        if(now=="true"||was=="true"){record.changes.push_back({"enableHdr",was,"false"});out=out.substr(0,b)+"false"+out.substr(e);}}
    // Vertical sync (2026-10-25, user: "what is surely right may be set - DLSS, HDR off"): off with the upscaler. In VR the
    // headset's frame timing is the mod's (xrWaitFrame); the monitor's sync could only hold the frames to the monitor's rate
    // (a 60 Hz monitor under a 90 Hz headset). Recorded and put back at the next start without VR, as HDR.
    if((forceDlss||!mode.empty())&&flag(out,"vsync",b,e)){const auto now=out.substr(b,e-b),was=original("vsync",now);
        if(now=="true"||was=="true"){record.changes.push_back({"vsync",was,"false"});out=out.substr(0,b)+"false"+out.substr(e);}}
    if(giQuality.empty()||giQuality=="Keep")return true;
    if(!word(giQuality)){reason="the GI quality '"+giQuality+"' is not a word";return false;}
    return put("dynamicGi",giQuality);
}
// Only a field that still holds what the VR start wrote goes back; anything the player changed in the game stays.
inline std::string restore(const std::string& text,const RenderRecord& record,std::vector<std::string>* restored=nullptr){
    auto out=text;
    for(const auto& c:record.changes){size_t b=0,e=0;
        if(!(field(out,c.name,b,e)||flag(out,c.name,b,e))||out.substr(b,e-b)!=c.applied||c.applied==c.original)continue;
        out=out.substr(0,b)+c.original+out.substr(e);if(restored)restored->push_back(c.name);}
    return out;
}
// ---- the environment of the game's process for a VR start ----
struct Paths {std::wstring game,mod,bin,data,log;};          // no separator at the end
struct Shaders {std::wstring shadowReceiver,giProbe;};       // SHA-256 of data\shadow_receiver.spv / data\gi_probe.spv; empty: not there
using Environment=std::vector<std::pair<std::wstring,std::wstring>>;   // an empty value takes the variable away
// What launch_diagnostics.ps1 sets for run_play_session.ps1 -RealVr -LiveInput -UiQuad -SymmetricFov with the shadow
// receiver and GI probe shaders, the transparent effects and the body clip; the logs are held small for play.
// EVR_SCREEN_XR (user, 2026-10-14: "from the start, consistently operable in VR is a must"): the session of the eyes
// is made at the game's first presents and the game's picture is shown on a plate in the headset until a world's eyes
// are drawn (layer/screen_xr.hpp); it is made with the eye size of the settings, which the world's session then keeps.
inline Environment vrEnvironment(const Paths& p,const Shaders& s,bool online,double renderScale=.8,bool alternateEyes=false,int eyeWarp=0,int afwPacing=0){
    const auto scaleText=[&]{const auto t=settings::text(renderScale);return std::wstring(t.begin(),t.end());}();
    Environment e{
        {L"EVR_SCREEN_XR",L"1"},{L"EVR_SCREEN_XR_SCALE",scaleText},
        // The first-person height that settled in an earlier session (hooks/root_camera_bridge.hpp): the first pair of
        // eyes does not wait half a minute for the game's idle camera again.
        {L"EVR_KNOWN_HEIGHT_FILE",p.mod+L"\\run\\known-height.txt"},
        // The lens' visible area of this headset, kept from an earlier start when the runtime gives it only after the
        // first frame (include/evr/openxr_client.hpp fovCache): the picture fits the lens from the next start.
        {L"EVR_FOV_CACHE_FILE",p.mod+L"\\run\\fov-cache.txt"},
        {L"EVR_LAYER_OUTPUT",p.log},{L"VK_LAYER_PATH",p.bin},{L"VK_INSTANCE_LAYERS",L"VK_LAYER_EVR_diagnostic"},
        {L"EVR_PREPARE_OPENXR",L"1"},{L"EVR_PREPARE_LOADER",p.bin+L"\\openxr_loader.dll"},{L"EVR_PREPARE_FIXTURE",L"0"},{L"EVR_PREPARE_FAIL_DEVICE",L"0"},
        {L"EVR_PREPARE_SESSION_FIXTURE",L"0"},{L"EVR_PREPARE_GAME_SESSION",L"0"},{L"EVR_LIVE_NATIVE_XR",L"1"},
        {L"EVR_NATIVE_MOTION_HANDS",L"1"},{L"EVR_HANDS_ONLY",L"1"},{L"EVR_GRIP_TRIM",L"4.47,-59.27,3.61"},
        {L"EVR_SYMMETRIC_FOV",L"1"},{L"EVR_XR_ASYNC_COMPLETION",L"1"},{L"EVR_DESKTOP_MIRROR",L"1"},{L"EVR_FOV_CROP",L"1"},
        {L"EVR_BODY_CLIP_SHADER",L"1"},{L"EVR_BODY_CLIP_PREPARE",L"1"},{L"EVR_HEAD_VISIBILITY_SHADER",L"1"},{L"EVR_VFX_TRANSPARENT_SHADER",L"1"},
        {L"EVR_UI_QUAD",L"1"},{L"EVR_UI_CAPTURE",L"1"},{L"EVR_UI_PIPELINE_PROBE",L"1"},
        // 2026-10-17: a UI frame with an unreviewed draw is shown without that draw (ui_draw_capture.hpp partial(); headset
        // report 21, "the menu cannot be seen") - until then "0" here left the whole UI of such a frame out of the headset.
        {L"EVR_UI_PARTIAL",L"1"},
        {L"EVR_RENDER_CPU_ARENA_MIB",L"1024"},
        {L"EVR_EYE_LUMA_EVERY",L"0"},{L"EVR_GPU_TIMING_EVERY",L"0"},{L"EVR_EYE_IMAGE_SWEEP",L"0"},{L"EVR_QUEUE_TRACE_LIVE",L"0"},{L"EVR_LOG_BUFFERED",L"0"},
        {L"EVR_LOG_NAME_CAP",L"300"},{L"EVR_PLAY_LOG",L"1"},
        {L"EVR_ONLINE_SYNC",online?L"1":L""},
        {L"EVR_ALTERNATE_EYES",alternateEyes?L"1":L""},
        {L"EVR_EYE_WARP",alternateEyes&&eyeWarp==2?L"2":alternateEyes&&eyeWarp==1?L"1":L""},
        // the alternate eyes' pacing (vr_settings.hpp afwPacing; include/evr/afw_pacing.hpp), changed in the game's VR settings too
        {L"EVR_AFW_PACING",alternateEyes&&afwPacing>=1&&afwPacing<=2?std::to_wstring(afwPacing):L""},
        // Only the tests' runtime reads these; nothing of an earlier shell may reach a play session.
        {L"EVR_MOCK_HAND_SEQUENCE",L""},{L"EVR_MOCK_NAV_FILE",L""},{L"EVR_MOCK_NAV_HAND_INPUT",L""},{L"EVR_MOCK_VISIBILITY_MASK",L""},{L"EVR_FOV_CROP_MARGIN_DEG",L""},{L"EVR_CAPTURE_SHADERS",L""},
    };
    if(!s.shadowReceiver.empty()){e.push_back({L"EVR_SHADOW_RECEIVER_SHADER",p.data+L"\\shadow_receiver.spv"});e.push_back({L"EVR_SHADOW_RECEIVER_SHADER_SHA256",s.shadowReceiver});}
    else{e.push_back({L"EVR_SHADOW_RECEIVER_SHADER",L""});e.push_back({L"EVR_SHADOW_RECEIVER_SHADER_SHA256",L""});}
    if(!s.giProbe.empty()){e.push_back({L"EVR_GI_PROBE_SHADER",p.data+L"\\gi_probe.spv"});e.push_back({L"EVR_GI_PROBE_SHADER_SHA256",s.giProbe});}
    else{e.push_back({L"EVR_GI_PROBE_SHADER",L""});e.push_back({L"EVR_GI_PROBE_SHADER_SHA256",L""});}
    return e;
}
// A pad style (2026-10-25): the motion controllers' settings do nothing (the native body is shown but the head and the upper
// body before the eyes: hooks/head_visibility_bridge.hpp, the hands-only shader kept).
inline Environment padEnvironment(){return {{L"EVR_NATIVE_MOTION_HANDS",L""},{L"EVR_ROOM_SNEAK",L"0"},{L"EVR_ARM_SWIM",L"0"}};}
// [Environment] of EnshroudedVR.ini, for a test or for looking into a problem: NAME=value on top of the list above
// (NAME= takes the variable away). Only the mod's own switches and the OpenXR runtime can be named.
// %GAME%, %MOD% and %LOG% stand for the folders.
inline bool overridable(const std::string& name){return name.rfind("EVR_",0)==0||name=="XR_RUNTIME_JSON";}
inline std::wstring expanded(std::wstring value,const Paths& p){
    for(const auto& pair:{std::pair<std::wstring,std::wstring>{L"%GAME%",p.game},{L"%MOD%",p.mod},{L"%LOG%",p.log}})
        for(auto at=value.find(pair.first);at!=std::wstring::npos;at=value.find(pair.first,at+pair.second.size()))value.replace(at,pair.first.size(),pair.second);
    return value;
}
// ---- the files of one VR session that follow the settings (the others come from the package's profile) ----
// The pad styles (vr_settings.hpp Style, 2026-10-25) add world-pad-mode.txt: first or third person, the pitch lock and the tilt.
inline std::map<std::string,std::string> sessionFiles(const settings::Values& v,const std::string& scene,const std::string& view){
    std::map<std::string,std::string> files{{"world-resource-proof.txt","EVR_WORLD_RESOURCE_1 "+scene+" "+view+"\n"},
            {"live-xr-config.txt","EVR_LIVE_XR_1 3000 "+settings::text(v.renderScale)+"\n"},
            {"world-vr-sharpness.txt","EVR_VR_SHARPNESS_1 "+settings::text(v.sharpness)+"\n"},
            {"world-session.txt","EVR_WORLD_SESSION_1 0\n"}};   // no minute limit: the session ends with the world
    if(settings::padStyle(v))files["world-pad-mode.txt"]=std::string("EVR_PAD_MODE_1 ")+(v.style==settings::Style::PadThird?"third":"first")+
        " "+(v.padPitchLock?"1":"0")+" "+std::to_string(v.padTilt)+"\n";
    return files;
}
// The package's profile files that belong to the motion controllers (2026-10-25): left out of a pad style's session. They
// make the mod's own gamepad from the controllers and hide the real one (controller-ui-fixture.txt), show and drive the
// tracked hands and the weapons in them, attack by the swing, guard by the pose, draw the bow, take the holsters and point at
// the menus - several of them refuse a session without the controllers' input. world-hands-only.txt stays: in first person
// with the pad it hides the head and the upper body before the eyes (hooks/head_visibility_bridge.hpp).
inline bool motionProfileFile(const std::string& name){
    static const char* const motion[]{"controller-ui-fixture.txt","world-motion-hands.txt","world-motion-visual.txt","world-native-grip-observe.txt",
        "world-weapon-mount.txt","world-two-hand-wield.txt","world-body-ik.txt","world-live-melee.txt","world-live-melee-contact.txt","world-live-guard.txt",
        "world-quick-slot-motion.txt","world-live-held-items.txt","world-menu-pointer.txt","world-bow-draw.txt","world-bow-tracked-aim.txt",
        "world-bow-physical-cancel.txt","world-bow-physical-pose.txt","world-bow-physical-release.txt","world-bow-spawn-observe.txt","world-bow-animation.txt",
        "world-ranged-animation.txt","world-ranged-aim.txt","world-shield-mount.txt","world-hand-curl.txt","hand-curl-closed-pose.bin"};
    for(const auto* m:motion)if(settings::sameText(name,m))return true;
    return false;
}
// world-found.txt of EVR_AwaitWorld: EVR_WORLD_FOUND_1 0x<scene> 0x<view>
inline bool worldFound(const std::string& text,std::string& scene,std::string& view){
    const auto t=settings::trim(text);const auto a=t.find(' '),b=t.rfind(' ');
    if(a==std::string::npos||a==b||t.substr(0,a)!="EVR_WORLD_FOUND_1")return false;
    scene=t.substr(a+1,b-a-1);view=t.substr(b+1);
    const auto address=[](const std::string& s){if(s.size()<4||s.size()>18||s[0]!='0'||s[1]!='x')return false;for(size_t i=2;i<s.size();i++)if(!std::isxdigit(static_cast<unsigned char>(s[i]))||std::isupper(static_cast<unsigned char>(s[i])))return false;return true;};
    return address(scene)&&address(view);
}
inline size_t count(const std::string& text,const std::string& part){size_t n=0;for(auto at=text.find(part);at!=std::string::npos;at=text.find(part,at+part.size()))++n;return n;}
// The log of the unused proof (EVR_ProbeWorldViewResolution with the culling history and the water pools), read as
// run_render_resource_probe.py reads it before it lets the eyes start: two unused Views of this world made and
// destroyed with nothing of the original changed, two histories and two pools made and released, the hook taken off.
inline bool proofAccepted(const std::string& log,const std::string& scene,const std::string& view,std::string& reason){
    const auto event=[&](const char* name){return count(log,std::string("\"event\":\"")+name+"\"");};
    if(count(log,"\"event\":\"native_view_probe_complete\",\"two_views_destroyed\":true,\"original_view_unchanged\":true,\"scene_time_unchanged\":true,\"back_pointer_restored\":true")!=1)
        reason="the two unused views were not made and destroyed cleanly";
    else if(event("native_view_created")!=2||count(log,"\"original_view\":\""+view+"\",\"scene\":\""+scene+"\"")!=2)reason="the unused views were not made for the world that was found";
    else if(count(log,"\"event\":\"unused_world_resource_diagnostic\",\"stage\":2")!=1)reason="the proof did not run as the world resolution stage";
    else if(event("culling_history_created")!=2||event("culling_history_released")!=2)reason="the two visibility histories were not made and released";
    else if(event("water_feedback_pool_created")!=2||event("water_feedback_pool_released")!=2||event("water_feedback_failure"))reason="the two water pools were not made and released";
    else if(event("view_resource_hook_complete")!=1||count(log,"\"event\":\"view_resource_hook_complete\",\"original_bytes_restored\":true,\"trampoline_retained\":true,\"result\":0")!=1)reason="the proof's hook was not taken off cleanly";
    else return true;
    return false;
}
// The layer's log (vulkan.jsonl) once the game has made its Vulkan device: is OpenXR prepared for the eyes?
// 1 yes, 0 no (reason says why, in the layer's words), -1 not known yet (the device is not made).
// Game start 20261014-110508 (a runtime that is not there): "preflight_fallback ... xrCreateInstance XrResult=-51",
// the game went on without VR, and the eyes' start failed only a minute later, in the world.
inline int xrPrepared(const std::string& log,std::string& reason){
    const std::string fallback="\"event\":\"preflight_fallback\",";
    if(const auto at=log.find(fallback);at!=std::string::npos){
        auto detail=log.substr(at+fallback.size(),log.find('\n',at)==std::string::npos?std::string::npos:log.find('\n',at)-at-fallback.size());
        detail=settings::trim(detail);if(!detail.empty()&&detail.back()=='}')detail.pop_back();
        std::string plain;for(const char c:detail)if(c!='"')plain+=c;
        reason="OpenXR could not be prepared ("+plain+")";return 0;}
    if(log.find("\"event\":\"preflight_disabled\"")!=std::string::npos){reason="the mod's Vulkan layer does not know this version of the game";return 0;}
    if(log.find("\"event\":\"preflight_runtime\"")!=std::string::npos&&log.find("\"event\":\"preflight_gpu_validation_deferred\"")!=std::string::npos)return 1;
    return -1;
}
} // namespace evr::autostart
