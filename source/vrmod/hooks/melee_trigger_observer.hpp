// The game's melee attack trigger (275110, from 2856d7), observed for the local player (2026-10-15, user: "there is a
// jump attack skill - attacking in the air falls on the enemy with a shock wave; in VR I want both the ordinary attack
// in the air and the jump attack: the heavy stroke in the air for the jump attack"). Read in the executable:
//   - no attack while jumpCount (DynamicLocomotion +cd) says a double jump and airborneMeleeAttacksCount (PlayerState
//     +58) is already 1 (27537f..2753a2): one attack a double jump; the count goes up at the end (275ae8) under the
//     same double-jump condition;
//   - after the sequence is asked for (1db700 at 2758ad), the jump type of the attack's jump is written to the input
//     ([row+a8]+60, 2759fc..275a21): JumpType::Attack (14) while jumpCount <= 1, else none - the leap of the jump
//     attack, when the sequence has one (keen::actor::JumpEvent).
// Here: what came in (the request's 20 bytes, jump type and count, the airborne attack count) and what the trigger
// left (the count, the jump type it asked for), the first 300 calls. Nothing is written. EVR_MELEE_TRIGGER_OBSERVER=0
// leaves it out. What the skill changes - which sequence the trigger picks in the air and by which input - is read
// from these records of a headset run with the skill (docs/analysis/airborne_skills_20261015.md).
namespace meleeTriggerObserver {
using Trigger=uint64_t(__fastcall*)(void*,void*,void*);
inline Trigger original=nullptr;inline bool enabled=false;
constexpr uintptr_t rva=0x2c5320,callerRva=0x2d9de8;
constexpr const char* pattern="48 8B C4 4C 89 40 18 48 89 48 08 55 53 56 57 41 54 41 55 41 57 48 8D A8 E8 FE FF FF 48 81 EC E0 01 00 00 8B 79";
inline std::atomic<unsigned> logged{0},calls{0},inside{0};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
inline std::string hexBytes(const uint8_t* p,size_t n){static const char* digits="0123456789abcdef";std::string s;for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;}
uint64_t __fastcall trigger(void* request,void* row,void* third){
    const Inside counted;++calls;
    const auto r=reinterpret_cast<uintptr_t>(row);
    bool local=false;uintptr_t loco=0,state=0,input=0;uint8_t jumpType=0,jumpCount=0,airBefore=0;std::array<uint8_t,20> req{};
    try{const auto actor=read<uintptr_t>(r+0x98);local=actor&&actor==controllerUiFixture::playerBaseActor.load();
        if(local){loco=read<uintptr_t>(r+0x118);state=read<uintptr_t>(r+0x138);input=read<uintptr_t>(r+0xa8);req=read<std::array<uint8_t,20>>(reinterpret_cast<uintptr_t>(request));
            if(loco){jumpType=read<uint8_t>(loco+0xcc);jumpCount=read<uint8_t>(loco+0xcd);}if(state)airBefore=read<uint8_t>(state+0x58);}
    }catch(...){local=false;}
    const auto result=original(request,row,third);
    if(local&&logged.load()<300){const ObserverLastError preserve;
        try{uint8_t airAfter=state?read<uint8_t>(state+0x58):0,asked=input?read<uint8_t>(input+0x60):0;uint16_t sequence=0;
            try{sequence=read<uint16_t>(read<uintptr_t>(r+0x98)+0x18);}catch(...){}
            ++logged;logFile->event("melee_trigger","\"frame\":"+std::to_string(liveFrame.frame)+",\"request\":\""+hexBytes(req.data(),req.size())+"\",\"jump_type\":"+std::to_string(jumpType)+
                ",\"jump_count\":"+std::to_string(jumpCount)+",\"airborne_attacks_before\":"+std::to_string(airBefore)+",\"airborne_attacks_after\":"+std::to_string(airAfter)+
                ",\"asked_jump_type\":"+std::to_string(asked)+",\"actor_sequence\":"+std::to_string(sequence)+",\"result\":"+std::to_string(result&0xff)+",\"read_only\":true");
        }catch(...){}
    }
    return result;
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_TRIGGER_OBSERVER",value,sizeof value);
        if(n==1&&value[0]=='0')return;
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("melee trigger signature changed");
        const auto call=baseAddress+callerRva-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)throw std::runtime_error("melee trigger caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&trigger),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
            throw std::runtime_error("melee trigger hook failed");
        enabled=true;logFile->event("melee_trigger_observer_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"read_only\":true");
    }catch(const std::exception& e){try{logFile->event("melee_trigger_observer_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void disable() noexcept {
    if(!enabled)return;enabled=false;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("melee_trigger_observer_removed","\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(calls.load()));}catch(...){}
}
}
