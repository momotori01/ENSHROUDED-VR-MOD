#pragma once
// The fog dragon's camera kept off in VR (user, 2026-10-24 14:40: "during the dragon fight the camera is forced - VR must not take
// it; the forced lock-on is fine, the camera's effect is not"). Game data (docs/STATUS.md 10-24 14:30): of the templates with
// health only Enemy_Boss_Fog_Dragon has keen::ecs::CombatLifetimeCameraOverride; its CameraStatesOverrideConfig replaces the
// lock-on cameras' settings (DefaultTarget, SprintingTarget, HipfireStaffChannelingTarget: distance 10..16 m, fov 50, 3 m up,
// looking at the primary target, pitch -70..+5) while the dragon is alive and in combat. The dragon's HardLockTarget (the forced
// lock-on) is another component and stays.
// The component's system (0x2b0e3c, build 26082781) calls 0xe5efb0 - the camera controller's "apply state overrides": it copies
// each override into the controller's per-camera table (+0x2558, 0x6e0 a camera), marks them (+0x16198) and the entity
// (+0x161a0) - and stores its result (1 applied, 0 not) in the component's wasApplied (+0x10); with wasApplied 0 the removal
// (0xe98fb0) is never called. Both functions have no other caller. While the VR session runs, this detour answers 0 and applies
// nothing: the system tries again on its next tick and gets 0 again, the game's lock-on cameras keep their own settings.
// EVR_COMBAT_CAMERA_OVERRIDE=1 in the game's environment: the game's override as before.
namespace combatCameraOverride {
constexpr uintptr_t applyRva=0xe5efb0,callerRva=0x2b108b;
constexpr const char* applyPattern="4C 8B DC 53 56 57 48 81 EC 30 07 00 00 41 8B D8 49 8B F9 44 8B 42 04 48 8B F1 45 85 C0 0F 84";
using Apply=bool(__fastcall*)(void*,const void*,uint32_t,void*);
inline Apply original=nullptr;inline bool enabled=false;
inline std::atomic<uint64_t> blocked{0};
inline bool wanted(){char v[8]{};const auto n=GetEnvironmentVariableA("EVR_COMBAT_CAMERA_OVERRIDE",v,sizeof v);return !(n==1&&v[0]=='1');}
inline bool __fastcall apply(void* controller,const void* config,uint32_t entity,void* extra){
    const auto n=++blocked;
    if(n==1||n%1800==0)try{
        const uint32_t count=config?read<uint32_t>(reinterpret_cast<uintptr_t>(config)+4):0;
        logFile->event("combat_camera_override_blocked","\"calls\":"+std::to_string(n)+",\"entity\":"+std::to_string(entity)+",\"cameras\":"+std::to_string(count)+",\"applied\":false");
    }catch(...){}
    return false;
}
inline void install(){
    if(enabled||!wanted())return;
    const auto bytes=parsePattern(applyPattern);const auto target=baseAddress+applyRva,call=baseAddress+callerRva;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
        throw std::runtime_error("combat camera override signature/caller mismatch");
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&apply),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("combat camera override hook failed");
    enabled=true;
    logFile->event("combat_camera_override_hook_enabled","\"rva\":"+hex(applyRva)+",\"caller_rva\":"+hex(callerRva)+",\"forced_lock_on_kept\":true");
}
inline void uninstall() noexcept {
    if(!enabled)return;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+applyRva));
    if(status==MH_OK){MH_RemoveHook(reinterpret_cast<void*>(baseAddress+applyRva));enabled=false;}
    try{logFile->event("combat_camera_override_hook_removed","\"status\":"+std::to_string(int(status))+",\"blocked\":"+std::to_string(blocked.load()));}catch(...){}
}
}
