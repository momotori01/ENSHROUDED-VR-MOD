#pragma once
// The mod's own stamina ring (2026-10-26, user: "cutting the UI will always go wrong somewhere - take stamina out of the UI
// and draw what is needed in the reticle's layer yourself; the game shows it, so the value is there"). The game's own
// floating ring is held off in VR (floating_stamina.hpp); the layer draws a ring of its own into each eye where VHVR has its
// stamina (layer/native_xr.hpp evrSetStamina). The value, each frame:
//   solo and host: the own character's keen::ecs::Stamina of this World (now, the most; live_melee_targets.hpp localStamina)
//   a guest: the stamina the host's mod sends four times a second (online_sync.hpp, kind 9 - the guest's World has none of
//     its player's, 10-23), with its most since 2026-10-26
// Off since 2026-10-26 14:00 (the game's own ring again, floating_stamina.hpp): EVR_OWN_STAMINA_RING=1 (with EVR_FLOATING_STAMINA_OFF=1).
namespace ownStaminaRing {
inline const bool wanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_OWN_STAMINA_RING",v,sizeof v);return n==1&&v[0]=='1';}();
using SetFn=void(*)(int,int);inline SetFn set=nullptr;inline bool looked=false;inline unsigned logs=0;inline int lastSource=-1;
inline void frame() noexcept {
    if(!wanted)return;
    if(!looked){looked=true;if(const auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))set=reinterpret_cast<SetFn>(GetProcAddress(layer,"evrSetStamina"));}
    if(!set)return;
    const auto now=GetTickCount64();int value=-1,most=-1,source=0;
    const auto at=liveTargets::localStaminaAt.load();
    if(at&&now>=at&&now-at<=500){value=liveTargets::localStaminaNow.load();most=liveTargets::localStaminaMax.load();source=1;}
    else{const auto h=controllerUiFixture::hostStamina.load();const auto t=h>>16;
        if(h&&now>=t&&now-t<=1500){value=int(h&0xffff);most=controllerUiFixture::hostStaminaMost.load();source=2;}}
    if(source!=lastSource&&logs<40){++logs;lastSource=source;
        try{logFile->event("own_stamina_source","\"source\":\""+std::string(source==1?"own_world":source==2?"host":"none")+"\",\"value\":"+std::to_string(value)+",\"most\":"+std::to_string(most));}catch(...){}}
    if(value>=0&&most>0&&value<=most)set(value,most);
}
}
