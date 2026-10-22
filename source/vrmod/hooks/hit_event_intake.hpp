// The client's intake of replicated ECS events, read only (2026-10-22, the plan "the guest shows its own hits at once":
// docs/analysis/online_responsiveness_20261022/README.md, the path in docs/analysis/hit_event_client_20261022/README.md).
// A host's server sends the events its client programs read (keen::ecs::HitEvent among them) as they are; the guest's
// frame update hands each one to 0x958130 (its only caller 0x9cfc1e): (world rcx, event type index edx, a pointer to the
// event's pointer r8). It counts the type (root+0xe15190 + index*32) and appends a copy to the two queues of the type
// (world+0xeb9e58 / +0xeb9e80 + index*72, 0x958200), whose element size is at +0x14 of the type's queue. The client's
// sounds, sparks and blood (0x2f7040), the damage numbers and health bars (0x317e80) read the HitEvents from there.
// Here: each event of 248 bytes (keen::ecs::HitEvent) is recorded with the time it came - who hit (rootSourceId), what
// (targetId), the health change, the flags, the materials, the place, the reaction - to measure how late a guest's own hits
// come back and to learn what a hit the guest makes itself must carry. Nothing is changed but a blow a guest guarded (guard
// below). EVR_HIT_EVENT_OBSERVE=0: off.
namespace hitEventIntake {
using Intake=uint8_t(__fastcall*)(void*,uint32_t,const void* const*);
inline Intake original=nullptr;inline bool enabled=false;inline std::atomic<bool> failed{false};
constexpr uintptr_t rva=0x958130,callerRva=0x9cfc23,queueRead=0x958187,queueA=0xeb9e58,queueStride=72,elementSizeAt=0x14;
constexpr const char* pattern="48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 39 48 8B D9 8B CA 49 8B E8";
constexpr uint32_t hitEventBytes=248;
inline std::atomic<uint64_t> calls{0},hitEvents{0};inline std::atomic<unsigned> logged{0};
inline std::mutex typesMutex;inline std::map<uint32_t,uint32_t> typeSizes;   // the sizes seen per type index (told once each)
inline void fail(const char* reason) noexcept {
    if(failed.exchange(true))return;
    try{logFile->event("hit_event_intake_failure","\"reason\":"+jsonString(reason)+",\"events_pass_unchanged\":true");}catch(...){}
}
inline void record(void* world,uint32_t type,const void* const* data){
    const auto queues=read<uintptr_t>(reinterpret_cast<uintptr_t>(world)+queueA);if(!queues)return;
    const auto size=read<uint32_t>(queues+uintptr_t(type)*queueStride+elementSizeAt);
    {std::lock_guard<std::mutex> lock(typesMutex);
     if(typeSizes.size()<256&&typeSizes.emplace(type,size).second)logFile->event("hit_event_intake_type","\"type\":"+std::to_string(type)+",\"bytes\":"+std::to_string(size));}
    if(size!=hitEventBytes||!data||!*data)return;
    const auto e=read<std::array<uint8_t,hitEventBytes>>(reinterpret_cast<uintptr_t>(*data));
    const auto u32=[&](size_t at){uint32_t v=0;std::memcpy(&v,e.data()+at,4);return v;};
    const auto i32=[&](size_t at){int32_t v=0;std::memcpy(&v,e.data()+at,4);return v;};
    const auto f32=[&](size_t at){float v=0;std::memcpy(&v,e.data()+at,4);return v;};
    const auto u16=[&](size_t at){uint16_t v=0;std::memcpy(&v,e.data()+at,2);return v;};
    const auto n=++hitEvents;if(logged.load()>=600&&n%50)return;++logged;
    std::ostringstream o;o<<std::setprecision(6)<<"\"type\":"<<type<<",\"n\":"<<n<<",\"root_source\":"<<u32(144)<<",\"source\":"<<u32(164)<<",\"target\":"<<u32(148)
        <<",\"root_target\":"<<u32(160)<<",\"health_change\":"<<i32(188)<<",\"stamina_change\":"<<i32(192)<<",\"flags\":"<<u32(152)
        <<",\"material\":["<<u32(100)<<','<<u32(104)<<"],\"position\":["<<f32(108)<<','<<f32(112)<<','<<f32(116)<<"],\"normal\":["<<f32(120)<<','<<f32(124)<<','<<f32(128)
        <<"],\"reaction\":"<<u16(224)<<",\"push_back\":"<<u16(226)<<",\"impact\":"<<u32(212)<<",\"weapon_category\":"<<u32(216)<<",\"damage_types\":"<<u16(220)
        <<",\"attack_category\":"<<unsigned(e[223])<<",\"hit_counter\":"<<u16(208)<<",\"weapon_pide\":"<<u32(172)<<",\"head\":\"";
    for(size_t i=0;i<8;i++)o<<"0123456789abcdef"[e[i]>>4]<<"0123456789abcdef"[e[i]&15];
    o<<"\",\"world\":"<<hex(reinterpret_cast<uintptr_t>(world))<<",\"ms\":"<<GetTickCount64();
    logFile->event("hit_event_received",o.str());
}
// The guest's guard (2026-10-22, user: "decide the guard on the guest and send only whether it held"; on the friend's server
// a guest's guard was judged by the host toward the character's old facing). A blow on this player (the HitEvent's target
// within 1.2 m of the player across and 2.5 m up and down, a loss of health, neither blocked nor parried by the host) that
// comes while the guard is held: shown as blocked at once (WasBlocked, no health change, no reaction or push: the client
// plays the block's feedback, no damage number, no stagger) and sent to the host (online_sync.hpp sendGuards), whose mod
// gives the health back (live_melee_targets.hpp undoDamage). First form: every blow while guarding counts; the blow's
// direction and the camera's turn are logged (guest_guard_decided) to bring in the guard's cone once it is checked. Only as
// a guest (liveTargets::guestWorld). EVR_GUEST_GUARD=0: off.
inline const bool guestGuardWanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_GUEST_GUARD",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::array<uint8_t,hitEventBytes> guarded{};inline std::atomic<unsigned> guards{0};
inline bool guard(void* world,uint32_t type,const void* const* data,const void*& replacement){
    if(!guestGuardWanted||!liveTargets::guestWorld||!controllerUiFixture::liveGuardHeld.load()||!data||!*data)return false;
    const auto queues=read<uintptr_t>(reinterpret_cast<uintptr_t>(world)+queueA);if(!queues||read<uint32_t>(queues+uintptr_t(type)*queueStride+elementSizeAt)!=hitEventBytes)return false;
    auto e=read<std::array<uint8_t,hitEventBytes>>(reinterpret_cast<uintptr_t>(*data));
    uint32_t flags=0;int32_t health=0;uint32_t attacker=0;std::memcpy(&flags,e.data()+152,4);std::memcpy(&health,e.data()+188,4);std::memcpy(&attacker,e.data()+144,4);
    if(health>=0||(flags&3)||!attacker)return false;
    rootCamera::Sample own;{std::lock_guard<std::mutex> lock(rootCamera::mutex);own=rootCamera::latest;}
    if(!own.number||GetTickCount64()-own.milliseconds>1000)return false;
    std::array<int64_t,3> target{};std::memcpy(target.data(),e.data()+8,24);
    const double q=1.0/4294967296.0,dx=double(target[0]-own.stable.position.x)*q,dy=double(target[1]-own.stable.position.y)*q,dz=double(target[2]-own.stable.position.z)*q;
    if(dx*dx+dz*dz>1.2*1.2||std::abs(dy)>2.5)return false;
    guarded=e;flags|=1u;const int32_t zero=0;const uint16_t none=0;std::memcpy(guarded.data()+152,&flags,4);std::memcpy(guarded.data()+188,&zero,4);
    std::memcpy(guarded.data()+224,&none,2);std::memcpy(guarded.data()+226,&none,2);replacement=guarded.data();
    {std::lock_guard<std::mutex> lock(controllerUiFixture::guardDecidedMutex);auto now=GetTickCount64();if(now<=controllerUiFixture::guardDecidedMs)now=controllerUiFixture::guardDecidedMs+1;
     controllerUiFixture::guardDecidedMs=now;controllerUiFixture::guardDecidedAt={own.stable.position.x,own.stable.position.y,own.stable.position.z};}
    float dir[3]{};std::memcpy(dir,e.data()+132,12);const auto o=own.unshaken.orientation;
    if(guards.fetch_add(1)<300){std::ostringstream s;s<<std::setprecision(5)<<"\"attacker\":"<<attacker<<",\"health_change\":"<<health<<",\"flags\":"<<(flags&~1u)<<",\"hit_direction\":["<<dir[0]<<','<<dir[1]<<','<<dir[2]
        <<"],\"camera_orientation\":["<<o.x<<','<<o.y<<','<<o.z<<','<<o.w<<"],\"target_from_player_m\":["<<dx<<','<<dy<<','<<dz<<"],\"ms\":"<<GetTickCount64();
        logFile->event("guest_guard_decided",s.str());}
    return true;
}
__declspec(noinline) uint8_t __fastcall invoke(void* world,uint32_t type,const void* const* data){
    ++calls;
    if(!failed.load()&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+callerRva){
        try{record(world,type,data);}catch(const std::exception& e){fail(e.what());}catch(...){fail("unknown");}
        const void* replacement=nullptr;
        try{if(guard(world,type,data,replacement)&&replacement)return original(world,type,&replacement);}catch(...){}}
    return original(world,type,data);
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_HIT_EVENT_OBSERVE",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='0'){logFile->event("hit_event_intake_off","\"reason\":\"EVR_HIT_EVENT_OBSERVE=0\"");return;}
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||
           uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site||read<uint32_t>(baseAddress+queueRead+3)!=queueA)
            throw std::runtime_error("event intake signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("event intake hook failed");
        enabled=true;
        logFile->event("hit_event_intake_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"guest_guard\":"+(guestGuardWanted?"true":"false")+",\"hit_event_bytes\":"+std::to_string(hitEventBytes));
    }catch(const std::exception& e){fail(e.what());}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("hit_event_intake_removed","\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(calls.load())+",\"hit_events\":"+std::to_string(hitEvents.load())+
        ",\"failed\":"+(failed.load()?"true":"false"));}catch(...){}
}
}
