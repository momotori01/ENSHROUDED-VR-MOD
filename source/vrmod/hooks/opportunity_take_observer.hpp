#pragma once
// The take of a follow-up slot (1d9e30: the input's trigger kind, [context+48]+20, against the six FollowUp slots of
// the Actor at [context+38] - the first slot of that kind; 1d8fd0 is the search), observed for the airborne attacks
// (2026-10-15 afternoon, user: "swing up in the air for the rising attack, down for the crash-down"). The Actor that
// holds the slots is not one of the instances the event selection sees (Mock game-20261015-132051: their slots were
// empty while the jump's slash-up came), so it is taken from here: its address is kept for the shaping of the slots
// before the next airborne press (motion_event_selection_observer.hpp shapeAirborneOpportunities, opportunityHolder),
// and the holder with its slots goes on record before the take, the first 80 calls (opportunity_take). Nothing is
// written here. Opt-in (EVR_OPPORTUNITY_TAKE_OBSERVER=1): a diagnostic on a hot path (about 1000 calls a second). In 1.0
// it finds nothing: the take (0x2145a0) looks its Actor and input up by component ([rcx+0x98], 0x9c5df0) instead of
// reading them at context+0x38 / +0x48 (Mock game-20261017-092452: 144477 calls, no Primary take seen). The holder is
// found by its components instead (motion_event_selection_observer.hpp, findOpportunityHolder).
namespace opportunityTakeObserver {
using Take=uint64_t(__fastcall*)(void*,void*,void*,void*);
inline Take original=nullptr;inline bool enabled=false;
constexpr uintptr_t rva=0x2145a0;
constexpr const char* pattern="48 89 54 24 10 55 56 41 55 48 8D AC 24 90 FE FF FF 48 81 EC 70 02 00 00 48 8B F1 48 8B 89 98 00 00 00";
inline std::atomic<unsigned> logged{0},calls{0},inside{0};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
uint64_t __fastcall take(void* context,void* input,void* third,void* fourth){
    const Inside counted;++calls;std::string rows;
    try{
        const auto c=reinterpret_cast<uintptr_t>(context);const auto holder=read<uintptr_t>(c+0x38);const auto in=read<uintptr_t>(c+0x48);
        const uint16_t trigger=in?read<uint16_t>(in+0x20):0;
        if(holder&&trigger==1){   // the Primary takes only (Mock game-20261015-141243 / 141636: the other actors take kinds 0 and 23 all the time)
            if(trigger==1){motionEventSelection::opportunityHolder::actor=holder;motionEventSelection::opportunityHolder::seen=GetTickCount64();}
            if(logged.load()<80){
                std::ostringstream o;o<<"\"holder\":"<<hex(holder)<<",\"flags\":"<<unsigned(read<uint8_t>(holder+0x1b1))<<",\"trigger\":"<<trigger<<",\"slots\":[";
                for(unsigned i=0;i<6;++i){const auto slot=holder+0x1d8+i*0x1a8;
                    o<<(i?",":"")<<"{\"trigger\":"<<read<uint16_t>(slot+0x18)<<",\"sequence\":"<<read<uint32_t>(slot+0x170)<<",\"skill\":"<<read<uint32_t>(slot+0x190)<<",\"used\":"<<unsigned(read<uint8_t>(slot+0x1a4))<<"}";}
                o<<"]";rows=o.str();
            }
        }
    }catch(...){rows.clear();}
    const auto result=original(context,input,third,fourth);
    if(!rows.empty()&&logged.load()<80){const ObserverLastError preserve;
        try{++logged;logFile->event("opportunity_take",rows+",\"frame\":"+std::to_string(liveFrame.frame)+",\"result\":"+std::to_string(result&0xff)+",\"read_only\":true");}catch(...){}}
    return result;
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_OPPORTUNITY_TAKE_OBSERVER",value,sizeof value);
        if(!(n==1&&value[0]=='1'))return;
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("opportunity take signature changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&take),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
            throw std::runtime_error("opportunity take hook failed");
        enabled=true;logFile->event("opportunity_take_observer_enabled","\"rva\":"+hex(rva)+",\"read_only\":true");
    }catch(const std::exception& e){try{logFile->event("opportunity_take_observer_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void disable() noexcept {
    if(!enabled)return;enabled=false;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("opportunity_take_observer_removed","\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(calls.load()));}catch(...){}
}
}
