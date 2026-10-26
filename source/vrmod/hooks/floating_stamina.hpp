#pragma once
// The game's floating stamina ring in VR (2026-10-25, user: "the stamina gauge goes in and out between the white dot and
// the UI - can the stamina gauge and the staff's spell choice be moved one by one, and hidden from the UI one by one?";
// and "the game's own UI settings show and hide many items - isn't that a help?").
// The ring is the game's "show stamina as floating" (user_settings.json gameplay.showStaminaAsFloating): drawn beside the
// character, so in first person it wanders over the HUD (bug reports 200115 / 200206: x 569..906, y 493..718 of 1920x1080)
// across the edge of the aim cluster (aim_mark.hpp clusterRect, x 760..1560 y 370..710) - in and out of the white dot.
// With the setting off the game shows stamina as a third bar under health and mana, top left (Mock 20261025-202648).
// Where the setting is (found 2026-10-25): the gameplay settings struct by the executable's reflection (field table at
// rva 0x176f4e0: +0x04 cameraZoomFactor, +0x1c hudAspectRatioWidth, +0x20 textScale, +0x26 showStaminaAsFloating,
// +0x27 showManaAsFloating, +0x28 hideHUD) lies in the frontend at +0x34068 (Mock 20261025-201918, memory copy of the UI).
// So in motion VR and the first-person pad style it is held off: only the copy in memory, only on the play screen (HUD
// screen 1). On any other screen (the settings menu among them, where the game would save it), in the third-person pad
// style (the game's own camera, the ring where the game means it) and when the world is left, the player's own value is
// put back - their user_settings.json keeps it. Off since 2026-10-26 14:00 (user after the headset: "is this stamina gauge your own?
// it is drawn differently from the game's - use the game's; it may follow the reticle as before"; the HUD's auto-hide also stopped:
// the game then showed a third bar with the vitals, and each change of stamina showed the HUD again): EVR_FLOATING_STAMINA_OFF=1.
namespace floatingStamina {
constexpr uintptr_t settingsOffset=0x34068,staminaFloating=0x26;
inline const bool requested=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_FLOATING_STAMINA_OFF",v,sizeof v);return n==1&&v[0]=='1';}();
inline int original=-1;            // the player's own value while it is held off
inline uintptr_t heldIn=0;         // the frontend it is held off in
inline unsigned logs=0;
// the layer's evrStaminaStrip: "the play screen, the game's ring held off" - the layer draws its own stamina ring then
using StripFn=void(*)();inline StripFn strip=nullptr;inline bool stripLooked=false;
inline bool plausible(uintptr_t s){
    const float zoom=read<float>(s+0x04),text=read<float>(s+0x20);const auto hud=read<uint32_t>(s+0x1c);
    if(!(zoom>0.05f&&zoom<5.f&&text>0.3f&&text<4.f&&hud>=4&&hud<=64))return false;
    for(uintptr_t i=0x24;i<=0x28;++i)if(read<uint8_t>(s+i)>1)return false;
    return true;
}
inline void putBack(const char* why){
    if(original<0||!heldIn)return;
    try{const auto s=heldIn+settingsOffset;
        if(plausible(s)){*reinterpret_cast<volatile uint8_t*>(s+staminaFloating)=uint8_t(original);
            if(logs<200){++logs;logFile->event("floating_stamina_put_back","\"value\":"+std::to_string(original)+",\"why\":"+jsonString(why));}}
    }catch(...){}
    original=-1;heldIn=0;
}
// From the UI observer's gate (input_ui_observer.hpp), on the game's thread, with the frontend and its HUD screen.
inline void apply(uintptr_t frontend,uint8_t screen){
    if(!requested||!frontend)return;
    const bool hold=screen==1&&game::padStyle.load()!=2;
    if(!hold){putBack(screen==1?"third-person pad style":"not the play screen");return;}
    const auto s=frontend+settingsOffset;
    if(!plausible(s)){if(logs<5){++logs;logFile->event("floating_stamina_skipped","\"reason\":\"the settings struct does not look right\",\"screen\":"+std::to_string(unsigned(screen)));}return;}
    auto* flag=reinterpret_cast<volatile uint8_t*>(s+staminaFloating);
    if(original<0||heldIn!=frontend){if(original>=0)putBack("another frontend");original=*flag;heldIn=frontend;
        if(logs<200){++logs;logFile->event("floating_stamina_off","\"players_own\":"+std::to_string(original));}}
    if(*flag)*flag=0;
    if(!stripLooked){stripLooked=true;if(const auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))strip=reinterpret_cast<StripFn>(GetProcAddress(layer,"evrStaminaStrip"));}
    if(strip)strip();
}
}
