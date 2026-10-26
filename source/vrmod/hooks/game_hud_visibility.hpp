#pragma once
// The HUD's auto-hide by the game itself (2026-10-26, user: "not cutting - can't the UI side hide it, never handing it to the UI?
// Cutting always goes wrong somewhere: the compass sticks out a little below the cut and stays near the Shroud"). The game's own
// HUD visibility settings - the gameplay settings struct in the frontend at +0x34068 (floating_stamina.hpp), its reflection field
// table at rva 0x176f3f0: +0x29 playerVitalsVisibility, +0x2a actionBarVisibility, +0x2b compassVisibility, +0x2c
// questTrackerVisibility (keen::g38_options::common::user::HudElementVisibility: Permanent 0, Contextual 1, Never 2) - are set to
// Contextual on the play screen while the UI auto-hide is asked: the game fades those parts itself while nothing changes, and the
// layer's fade by regions of the UI picture (include/evr/hud_fade.hpp) is not used (uiHudBridge sends 2). Only the copy in memory:
// on any other screen (the settings menu among them, where the game would save it) and when the world is left the player's own
// values go back. Off by default (see requested): the layer's fade by regions.
namespace gameHudVisibility {
constexpr uintptr_t settingsOffset=0x34068;constexpr std::array<uintptr_t,4> fields{0x29,0x2a,0x2b,0x2c};
constexpr const char* names[4]{"player_vitals","action_bar","compass","quest_tracker"};
// Off by default (2026-10-26 device run, user: "a UI where some parts fade and some do not - worse"): the game's Contextual mode
// fades its parts on its own rules, unlike the layer's one fade of the whole HUD. EVR_GAME_HUD_CONTEXTUAL=1 turns it on to try.
inline const bool requested=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_GAME_HUD_CONTEXTUAL",v,sizeof v);return n==1&&v[0]=='1';}();
inline bool wanted=false;   // the UI auto-hide asked for (view_resource_probe.cpp, world-ui-auto-hide.txt)
inline std::array<int,4> original{-1,-1,-1,-1};inline uintptr_t heldIn=0;inline unsigned logs=0;
inline bool inUse(){return requested&&wanted;}
inline bool plausible(uintptr_t s){
    if(!floatingStamina::plausible(s))return false;
    for(const auto f:fields)if(read<uint8_t>(s+f)>2)return false;
    return true;
}
inline void putBack(const char* why){
    if(!heldIn)return;
    try{const auto s=heldIn+settingsOffset;
        if(plausible(s)){std::string values;
            for(size_t i=0;i<fields.size();++i)if(original[i]>=0){*reinterpret_cast<volatile uint8_t*>(s+fields[i])=uint8_t(original[i]);values+=(values.empty()?"":",")+std::to_string(original[i]);}
            if(logs<200){++logs;logFile->event("game_hud_visibility_put_back","\"values\":["+values+"],\"why\":"+jsonString(why));}}
    }catch(...){}
    original={-1,-1,-1,-1};heldIn=0;
}
// Diagnostic: EVR_GAME_SETTING_POKE=<offset hex>:<value>/... - other bytes of the settings struct set on the play screen the same
// way (put back with the rest), e.g. 2f:1 hideActionHints (does it take the aim point's ammunition list away?).
inline std::vector<std::pair<uintptr_t,uint8_t>> pokes=[]{std::vector<std::pair<uintptr_t,uint8_t>> v;char t[256]{};const auto n=GetEnvironmentVariableA("EVR_GAME_SETTING_POKE",t,sizeof t);
    if(n&&n<sizeof t){std::string s(t);size_t p=0;while(p<s.size()){auto e=s.find('/',p);if(e==std::string::npos)e=s.size();const auto item=s.substr(p,e-p);const auto c=item.find(':');
        if(c!=std::string::npos){const auto o=std::strtoul(item.substr(0,c).c_str(),nullptr,16);if(o>=0x24&&o<=0x47)v.push_back({uintptr_t(o),uint8_t(std::strtoul(item.substr(c+1).c_str(),nullptr,10))});}p=e+1;}}return v;}();
inline std::vector<int> pokedOriginal;
// From the UI observer's gate (input_ui_observer.hpp), on the game's thread, with the frontend and its HUD screen.
inline void apply(uintptr_t frontend,uint8_t screen){
    if(!pokes.empty()&&frontend){const auto s=frontend+settingsOffset;
        if(screen==1&&floatingStamina::plausible(s)){if(pokedOriginal.empty()){for(const auto& [o,v]:pokes)pokedOriginal.push_back(read<uint8_t>(s+o));
                logFile->event("game_setting_poke","\"count\":"+std::to_string(pokes.size()));}
            for(const auto& [o,v]:pokes)*reinterpret_cast<volatile uint8_t*>(s+o)=v;}
        else if(!pokedOriginal.empty()&&floatingStamina::plausible(s)){for(size_t i=0;i<pokes.size();++i)*reinterpret_cast<volatile uint8_t*>(s+pokes[i].first)=uint8_t(pokedOriginal[i]);pokedOriginal.clear();}}
    if(!inUse()||!frontend)return;
    if(screen!=1){putBack("not the play screen");return;}
    const auto s=frontend+settingsOffset;
    if(!plausible(s)){if(logs<5){++logs;logFile->event("game_hud_visibility_skipped","\"reason\":\"the settings struct does not look right\"");}return;}
    if(heldIn!=frontend){if(heldIn)putBack("another frontend");
        for(size_t i=0;i<fields.size();++i)original[i]=read<uint8_t>(s+fields[i]);heldIn=frontend;
        if(logs<200){++logs;std::string own;for(size_t i=0;i<fields.size();++i)own+=std::string(i?",":"")+"\""+names[i]+"\":"+std::to_string(original[i]);
            logFile->event("game_hud_visibility_contextual","\"players_own\":{"+own+"}");}}
    for(size_t i=0;i<fields.size();++i){auto* v=reinterpret_cast<volatile uint8_t*>(s+fields[i]);
        // the player's own Never stays Never
        if(original[i]==0&&*v!=1)*v=1;}
}
}
