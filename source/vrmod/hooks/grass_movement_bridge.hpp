// Grass that is pushed aside, in both eyes (user after the headset runs of 2026-10-13: "the weeds that sway after an
// arrow has passed do so in the left eye only - fix it").
// What pushes grass is an effect: keen::ecs::GrassEffector is a keen::ecs::VfxComponent (the system
// update_grass_effector_vfx), and the simulation of the effects writes keen::GrassMovementBody records {packed world
// position, outer radius, inner radius, strength} (24 bytes) into a list of the View it runs for
// (keen::VfxUpdateBuffer: grassMovementBodyCounterByteOffset, grassMovementBodyListCapacity). Every View then runs
// "render_grass_movement" (92870, called by f7c50 at fe27b for a View with a grass movement state at View+9e8): it
// takes the last texture of that View, the list and its counter, and writes the View's new texture
// (keen::GrassMovementUpdateParameters; the grass reads it as keen::GrassMovementData of the pass constants).
// The effects are simulated once a frame, for the View that owns the VfxView - the original View, or the left eye on
// the frames where the original View's world is left out. The other eye's list stays empty, so its grass never moved.
// The records are places in the world, so the other eye can take the owner's list as it is: where 92870 is called
// for a View that does not own the effects, the frame's record of the list (the third argument: +0 the counter's id,
// +8 the buffer, +18 its offset, +20 its size in bytes) is the owner's for the length of the call, and the eye's own
// again afterwards. The eye keeps its own textures and its own origin. Same graph and same frame only; without the
// owner's record of this frame (the owner is drawn after the eye) the eye's own - empty - list stays.
// EVR_GRASS_MOVEMENT=0 leaves the bridge out. EVR_GRASS_MOVEMENT_PAUSE="<from> <to>" (diagnostic) lends nothing during
// those XR frames: with a still head, the other eye's grass then stands up again and is pushed anew when the lending
// returns, while the owner's eye stays as it is - the proof in one trial (Mock run game-20261013-172656: over the
// change, the grass at the feet changed by 8.0 in the other eye and by 5.3 in the owner's, means of seven pictures).
namespace grassMovementBridge {
using Stage=void(__fastcall*)(void*,void*,void*);
inline Stage original=nullptr;inline bool enabled=false;inline std::atomic<bool> failed{false};
// 1.0 (Hotfix 1.0.0.1): the stage is 0xc22a0 (June 0x92870, the same prologue), called at 0x1316b7 for a View with
// a grass movement state at View+0xf78 (June +0x9e8), the frame's list record the third argument as before.
constexpr uintptr_t rva=0xc2350,callerRva=0x1317fc;
constexpr const char* pattern="48 89 5C 24 20 48 89 54 24 10 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D0 FE FF FF";
struct Fields {uint32_t counter=0;std::array<uint8_t,16> list{};uint32_t offset=0;uint64_t bytes=0;};
struct Role {uint64_t serial=0;int eye=-2;bool owner=false;};
inline thread_local const Role* current=nullptr;
struct EyeScope {
    Role role;const Role* previous=current;
    EyeScope(uint64_t serial,int eye,bool owner):role{serial,eye,owner}{current=&role;}
    ~EyeScope(){current=previous;}
    EyeScope(const EyeScope&)=delete;EyeScope& operator=(const EyeScope&)=delete;
};
struct Source {uint64_t serial=0;Fields fields;bool valid=false;};
inline thread_local Source source;
inline std::atomic<uint64_t> ownerCalls{0},eyeCalls{0},lent{0},without{0},paused{0};
inline uint64_t pauseFrom=0,pauseTo=0;
inline void fail(const char* reason) noexcept {
    if(failed.exchange(true))return;
    try{logFile->event("grass_movement_bridge_failure","\"reason\":"+jsonString(reason)+",\"eyes_keep_their_own_lists\":true");}catch(...){}
}
inline void report(const Role& role,const Fields& own,uintptr_t state,bool lend,uint64_t number){
    if(number>4&&number%1800)return;
    uint32_t valid=0;try{valid=read<uint32_t>(state+0x40);}catch(...){}
    std::ostringstream o;o<<"\"serial\":"<<role.serial<<",\"eye\":"<<role.eye<<",\"owner_of_the_effects\":"<<(role.owner?"true":"false")<<",\"calls\":"<<number
        <<",\"counter\":"<<own.counter<<",\"list\":"<<jsonString(nativeImpactComponents::asHex(own.list))<<",\"offset\":"<<own.offset<<",\"bytes\":"<<own.bytes<<",\"bodies_capacity\":"<<own.bytes/24
        <<",\"frames_of_history\":"<<valid<<",\"takes_the_owner_s_list\":"<<(lend?"true":"false")
        <<",\"lent\":"<<lent.load()<<",\"without_the_owner_s_record\":"<<without.load();
    logFile->event("grass_movement_bridge",o.str());
}
__declspec(noinline) void __fastcall invoke(void* graph,void* state,void* frame){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const Role* role=current;
    if(!role||caller!=baseAddress+callerRva||failed.load()||!frame){original(graph,state,frame);return;}
    auto* const p=reinterpret_cast<uint8_t*>(frame);Fields own;
    std::memcpy(&own.counter,p,4);std::memcpy(own.list.data(),p+8,16);std::memcpy(&own.offset,p+0x18,4);std::memcpy(&own.bytes,p+0x20,8);
    if(role->owner){
        source={role->serial,own,true};
        try{report(*role,own,reinterpret_cast<uintptr_t>(state),false,++ownerCalls);}catch(...){}
        original(graph,state,frame);return;
    }
    // The owner's list of this frame, in this graph (a buffer's handle begins with its graph), of the same size.
    bool lend=source.valid&&source.serial==role->serial&&!std::memcmp(source.fields.list.data(),own.list.data(),8)&&source.fields.bytes==own.bytes&&own.bytes>=24;
    const auto number=++eyeCalls;
    if(lend&&liveFrame.frame>=pauseFrom&&liveFrame.frame<pauseTo){lend=false;++paused;}
    else if(lend)++lent;else ++without;
    try{report(*role,own,reinterpret_cast<uintptr_t>(state),lend,number);}catch(...){}
    if(!lend){original(graph,state,frame);return;}
    std::memcpy(p,&source.fields.counter,4);std::memcpy(p+8,source.fields.list.data(),16);std::memcpy(p+0x18,&source.fields.offset,4);
    original(graph,state,frame);
    std::memcpy(p,&own.counter,4);std::memcpy(p+8,own.list.data(),16);std::memcpy(p+0x18,&own.offset,4);
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_GRASS_MOVEMENT",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='0'){logFile->event("grass_movement_bridge_off","\"reason\":\"EVR_GRASS_MOVEMENT=0\"");return;}
        {char pause[48]{};const auto m=GetEnvironmentVariableA("EVR_GRASS_MOVEMENT_PAUSE",pause,sizeof pause);
            if(m&&m<sizeof pause){std::istringstream in(pause);uint64_t a=0,b=0;if(in>>a>>b&&b>a){pauseFrom=a;pauseTo=b;
                logFile->event("grass_movement_bridge_pause","\"diagnostic\":true,\"from_frame\":"+std::to_string(a)+",\"to_frame\":"+std::to_string(b));}}}
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)
            throw std::runtime_error("grass movement stage signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("grass movement stage hook failed");
        enabled=true;
        logFile->event("grass_movement_bridge_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern)+",\"lends\":\"the list of GrassMovementBody and its counter, for the call\"");
    }catch(const std::exception& e){fail(e.what());}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("grass_movement_bridge_removed","\"status\":"+std::to_string(status)+",\"owner_calls\":"+std::to_string(ownerCalls.load())+",\"eye_calls\":"+std::to_string(eyeCalls.load())+
        ",\"lent\":"+std::to_string(lent.load())+",\"without_the_owner_s_record\":"+std::to_string(without.load())+",\"paused_by_the_diagnostic\":"+std::to_string(paused.load())+",\"failed\":"+(failed.load()?"true":"false"));}catch(...){}
}
}
