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
// comes while the guard is held, by the game's own rule of a block (user 14:00: "is the guard invincible then? that is not
// allowed" - 0.9.8 took every blow while guarding): the player's Actor is in its guard state (Blocking, 2 - the game's own
// rules for when a guard holds: its start, a stagger, no stamina), the attacker stands within 80 degrees of the guard's front
// (the game's hit angle test 0x21a4f0, called by applyDamage at 0x3d6e92 with the Blocking state: (hit angle + pi) in
// [0, 80] or [280, 360] degrees; the angle between the target's front and the way from the attacker, 0x2190e0, level) - the
// front the character is given while guarding (input_head_idle.hpp: the back of the shield hand, a two-hand weapon from the
// head to the hands) - and stamina is left. Then it is shown as blocked at once (WasBlocked, no health change, no reaction
// or push: the client plays the block's feedback, no damage number, no stagger) and sent to the host (online_sync.hpp
// sendGuards), whose mod gives the health back and takes the block's stamina (live_melee_targets.hpp guardGiveBack). Only as
// a guest (liveTargets::guestWorld). EVR_GUEST_GUARD=0: off. Not known here: an attack the game makes unblockable.
inline const bool guestGuardWanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_GUEST_GUARD",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::array<uint8_t,hitEventBytes> guarded{};inline std::atomic<unsigned> guards{0},refusals{0};
struct StaminaType {uintptr_t world=0;int id=-2;};inline StaminaType staminaType;   // keen::ecs::NetworkStamina (4 bytes: stamina, max) of a World
inline int networkStamina(uintptr_t world,uint32_t entity){
    if(staminaType.world!=world||staminaType.id==-2){staminaType={world,-1};
        for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){try{const auto t=nativeImpactComponents::componentType(world,i);
            if(t.qualifiedName=="keen::ecs::NetworkStamina"&&t.bytes==4){staminaType.id=int(i);break;}}catch(...){break;}}}
    if(staminaType.id<0)return -1;
    try{const auto c=nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(world,entity),unsigned(staminaType.id),4);return c?int(read<uint16_t>(c)):-1;}catch(...){return -1;}
}
inline void refuse(const char* why,uint32_t attacker,int32_t health,double angle,int stamina,const char* front){
    if(refusals.fetch_add(1)>=300)return;
    std::ostringstream s;s<<std::setprecision(4)<<"\"reason\":\""<<why<<"\",\"attacker\":"<<attacker<<",\"health_change\":"<<health<<",\"angle_deg\":"<<angle*57.29578<<",\"stamina\":"<<stamina<<",\"front\":\""<<front<<"\",\"ms\":"<<GetTickCount64();
    logFile->event("guest_guard_refused",s.str());
}
inline bool guard(void* world,uint32_t type,const void* const* data,const void*& replacement){
    if(!guestGuardWanted||!liveTargets::guestWorld||!controllerUiFixture::liveGuardHeld.load()||!data||!*data)return false;
    const auto queues=read<uintptr_t>(reinterpret_cast<uintptr_t>(world)+queueA);if(!queues||read<uint32_t>(queues+uintptr_t(type)*queueStride+elementSizeAt)!=hitEventBytes)return false;
    auto e=read<std::array<uint8_t,hitEventBytes>>(reinterpret_cast<uintptr_t>(*data));
    uint32_t flags=0;int32_t health=0;uint32_t attacker=0,source=0;std::memcpy(&flags,e.data()+152,4);std::memcpy(&health,e.data()+188,4);std::memcpy(&attacker,e.data()+144,4);std::memcpy(&source,e.data()+164,4);
    if(health>=0||(flags&3)||!attacker)return false;
    rootCamera::Sample own;{std::lock_guard<std::mutex> lock(rootCamera::mutex);own=rootCamera::latest;}
    const auto now=GetTickCount64();
    if(!own.number||now-own.milliseconds>1000)return false;
    std::array<int64_t,3> target{};std::memcpy(target.data(),e.data()+8,24);
    const double q=1.0/4294967296.0,dx=double(target[0]-own.stable.position.x)*q,dy=double(target[1]-own.stable.position.y)*q,dz=double(target[2]-own.stable.position.z)*q;
    if(dx*dx+dz*dz>1.2*1.2||std::abs(dy)>2.5)return false;
    // the game's guard state of this player's Actor (input_actor_observer.hpp), when it is known
    {const auto ms=controllerUiFixture::playerActorStateMs.load();
     const auto blocking=controllerUiFixture::blockingMs.load();   // within 150 ms (a guard held drops it for a tick or two: guest log 20261022-123336)
     if(ms&&now>=ms&&now-ms<=300&&!(blocking&&now>=blocking&&now-blocking<=150)){refuse("not_in_the_guard_state",attacker,health,-1,-1,"");return false;}}
    // the guard's front: as sent to the game while guarding, else the camera's
    Vec3 front{};const char* frontFrom="guard";
    {std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);const auto ms=controllerUiFixture::guardWorldForwardMs;if(ms&&now>=ms&&now-ms<=300)front=controllerUiFixture::guardWorldForward;}
    if(front.x==0&&front.z==0){frontFrom="camera";auto f=rotate(normalized(own.unshaken.orientation),Vec3{0,0,1});const float l=std::sqrt(f.x*f.x+f.z*f.z);if(l>1e-4f&&std::isfinite(l))front={f.x/l,0,f.z/l};}
    if(front.x==0&&front.z==0){refuse("front_unknown",attacker,health,-1,-1,frontFrom);return false;}
    // where the attacker stands (the list of targets of this PC; the shooter of a shot)
    std::array<int64_t,3> from{};bool found=false;for(const auto id:{attacker,source})if(id&&liveTargets::entityPosition(id,from,now)){found=true;break;}
    if(!found){refuse("attacker_unknown",attacker,health,-1,-1,frontFrom);return false;}
    const double ax=double(from[0]-own.stable.position.x)*q,az=double(from[2]-own.stable.position.z)*q,al=std::sqrt(ax*ax+az*az);
    const double angle=evr::guard::angleFromFront(front.x,front.z,ax,az);
    int stamina=networkStamina(reinterpret_cast<uintptr_t>(world),own.entity);
    // A guest's game holds no stamina of its player (2026-10-23: only ClientPlayerStaminaHints): the value the host's mod
    // sent for this player (online_sync.hpp kind 9), when it is at most 1.5 s old.
    if(stamina<0){const auto h=controllerUiFixture::hostStamina.load();const auto at=h>>16;if(h&&now>=at&&now-at<=1500)stamina=int(h&0xffff);}
    if(angle>evr::guard::halfAngle){refuse("outside_the_guard",attacker,health,angle,stamina,frontFrom);return false;}
    if(stamina==0){refuse("no_stamina",attacker,health,angle,stamina,frontFrom);return false;}
    guarded=e;flags|=1u;const int32_t zero=0;const uint16_t none=0;std::memcpy(guarded.data()+152,&flags,4);std::memcpy(guarded.data()+188,&zero,4);
    std::memcpy(guarded.data()+224,&none,2);std::memcpy(guarded.data()+226,&none,2);replacement=guarded.data();
    {std::lock_guard<std::mutex> lock(controllerUiFixture::guardDecidedMutex);auto at=now;if(at<=controllerUiFixture::guardDecidedMs)at=controllerUiFixture::guardDecidedMs+1;
     controllerUiFixture::guardDecidedMs=at;controllerUiFixture::guardDecidedAt={own.stable.position.x,own.stable.position.y,own.stable.position.z};}
    if(guards.fetch_add(1)<300){std::ostringstream s;s<<std::setprecision(5)<<"\"attacker\":"<<attacker<<",\"health_change\":"<<health<<",\"flags\":"<<(flags&~1u)<<",\"angle_deg\":"<<angle*57.29578
        <<",\"front\":\""<<frontFrom<<"\",\"front_dir\":["<<front.x<<','<<front.z<<"],\"attacker_m\":"<<al<<",\"stamina\":"<<stamina<<",\"target_from_player_m\":["<<dx<<','<<dy<<','<<dz<<"],\"ms\":"<<now;
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
