// Diagnostic, read-only (EVR_SHADOW_HISTORY_PROBE=1 in the game's environment; no launch flag): does the sun shadow's
// temporal filter get its history in the eyes?
// User, 2026-10-13: "shadows that flicker fast, mostly at their edges, on the grass and on the hands - can that be
// fixed?" 32 consecutive frames with a still head (Mock run game-20261013-200531, 10504..10535): on ground that does
// not move, the dappled shadow of a tree is a new dotted pattern in every frame - 12 steps of 255 rms from frame to
// frame, successive changes correlated -0.43 (-0.5 is noise without any memory), the same in both eyes.
// The shadow mask ("shadow_mask", f7227.. of the gbuffer function, which calls 13da20 at f7556) takes noisy samples
// of the shadow maps and blends 95 % of the last frame's result in (the shader the layer replaces for the receiver
// plane keeps that: output/shadow-receiver-live-20261009/shadow_receiver.comp). It does not when its constant _m9 is
// set, and 13da20 sets that (13dbdd..13dc0f) to what f92a30 answers for the history picture's handle - the third
// argument +10, which the caller takes from the View: View+5c0 + 16 * (1 - View+5f0), the picture of the other
// ping-pong turn. f92a30: "the resource this handle names has no content" (byte +2 of its record is 0).
// Here: for every View the stage runs for, that answer and the two handles, at the first calls and whenever they
// change. Nothing is written; f92a30 is the game's own read, done once more.
namespace shadowHistoryProbe {
using Pass=void*(__fastcall*)(void*,void*,void*,void*);
using NoContent=bool(__fastcall*)(const void*);
inline Pass original=nullptr;inline bool enabled=false;
constexpr uintptr_t rva=0x164970,callerRva=0xf75fb,noContentRva=0x1014910;
constexpr const char* pattern="48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC";
constexpr const char* noContentPattern="48 89 5C 24 08 57 48 83 EC 50 8B 41 08 48 8B D9 48 8B 39 0F";
struct Seen {unsigned calls=0,resets=0,lines=0;int last=-1;std::array<uint8_t,16> history{};unsigned historyChanges=0;};
inline thread_local std::array<Seen,3> seen;   // the original View, the left eye, the right eye
// The View the stage runs for (set by the policy around each View's frame), for a look at what the caller chooses
// from: its flags (+10), the byte +44, the ping-pong turn (+5f0) and the pictures it takes turns with.
inline thread_local uintptr_t view=0;
struct ViewScope {uintptr_t previous=view;explicit ViewScope(uintptr_t v){view=v;}~ViewScope(){view=previous;}ViewScope(const ViewScope&)=delete;ViewScope& operator=(const ViewScope&)=delete;};
inline std::atomic<unsigned> total{0},totalResets{0};
__declspec(noinline) void* __fastcall pass(void* graph,void* viewPart,void* parameters,void* fourth){
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+callerRva){
        const ObserverLastError preserve;
        try{
            const auto p=reinterpret_cast<uintptr_t>(parameters);
            const auto* role=grassMovementBridge::current;const int eye=role?role->eye:-2;
            if(eye>=-1&&eye<=1){
                auto& s=seen[size_t(eye+1)];
                const bool reset=reinterpret_cast<NoContent>(baseAddress+noContentRva)(reinterpret_cast<const void*>(p+0x10));
                const auto output=read<std::array<uint8_t,16>>(p),history=read<std::array<uint8_t,16>>(p+0x10);
                ++s.calls;++total;if(reset){++s.resets;++totalResets;}
                // The two pictures take turns: the history of this frame is the output of the one before.
                const bool changed=s.calls>2&&history!=s.history&&s.calls%2==1;if(changed)++s.historyChanges;
                if(s.calls%2==1)s.history=history;
                if((s.calls<=8||int(reset)!=s.last||s.calls%1800==0)&&s.lines<120){++s.lines;
                    const auto bytes=[](const std::array<uint8_t,16>& a){static const char* digits="0123456789abcdef";std::string t;for(auto b:a){t+=digits[b>>4];t+=digits[b&15];}return t;};
                    logFile->event("shadow_history_probe","\"serial\":"+std::to_string(role->serial)+",\"eye\":"+std::to_string(eye)+",\"call\":"+std::to_string(s.calls)+
                        ",\"history_has_no_content\":"+(reset?"true":"false")+",\"calls_without_history\":"+std::to_string(s.resets)+
                        ",\"output\":"+jsonString(bytes(output))+",\"history\":"+jsonString(bytes(history))+",\"history_handle_changes\":"+std::to_string(s.historyChanges)+",\"read_only\":true");}
                if(view&&(s.calls==5||s.calls==6||s.calls==601||s.calls==602)){
                    const auto dump=[](uintptr_t at,size_t n){static const char* digits="0123456789abcdef";std::string t;for(size_t i=0;i<n;++i){const auto b=read<uint8_t>(at+i);t+=digits[b>>4];t+=digits[b&15];}return t;};
                    logFile->event("shadow_history_view","\"eye\":"+std::to_string(eye)+",\"call\":"+std::to_string(s.calls)+",\"flags\":"+hex(read<uint32_t>(view+0x10))+",\"byte_44\":"+std::to_string(read<uint8_t>(view+0x44))+
                        ",\"turn\":"+std::to_string(read<uint32_t>(view+0x5f0))+",\"view_b0\":"+jsonString(dump(view+0xb0,32))+",\"view_3c8\":"+jsonString(dump(view+0x3c8,0xa0))+",\"view_5c0\":"+jsonString(dump(view+0x5c0,32))+
                        ",\"parameters\":"+jsonString(dump(p,0xe0))+",\"read_only\":true");}
                s.last=int(reset);
            }
        }catch(...){}
    }
    return original(graph,viewPart,parameters,fourth);
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SHADOW_HISTORY_PROBE",value,sizeof value);
        if(!(n&&n<sizeof value&&value[0]=='1'))return;
        const auto bytes=parsePattern(pattern),check=parsePattern(noContentPattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||
           scan(reinterpret_cast<const uint8_t*>(baseAddress+noContentRva),check.size(),check)!=std::vector<size_t>{0}||
           read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)
            throw std::runtime_error("shadow mask pass signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&pass),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("shadow mask pass hook failed");
        enabled=true;
        logFile->event("shadow_history_probe_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"read_only\":true");
    }catch(const std::exception& e){try{logFile->event("shadow_history_probe_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("shadow_history_probe_removed","\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(total.load())+",\"calls_without_history\":"+std::to_string(totalResets.load()));}catch(...){}
}
}
