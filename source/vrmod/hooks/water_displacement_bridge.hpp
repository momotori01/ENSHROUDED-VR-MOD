// What disturbs the water's surface, in both eyes.
// User, 2026-10-13 (after the headset run of that day, then shown in a Mock session steered by keyboard and mouse,
// game-20261013-185120 frames 11146..11416 and 22441, 22561): pouring the watering can onto a pond, and swimming, the
// rings spreading on the water and the bright reflection on the disturbed surface are in the left eye only; the
// splashing drops are in both.
// Read from the executable (scratchpad report re_water_per_view.md of that day): the simulation of the effects writes
// keen::WaterDisplacementBody records {packed world position, least radius, greatest radius, depth} (24 bytes) into a
// list of the View it runs for. Every View with a water state (View+8c8, flag bit 24 of View+10) runs
// "render_water_surface" (1bfa60, called by f7c50 at 101d9c only); its stage "waves" (1c3d12..) reads the list
// straight from that state: the counter's id at state+1d8 (1c4398) and the list's buffer at state+1e0 (1c4579).
// The effects are simulated once a frame, for the View that owns the VfxView; the other eye's counter is set to zero
// every frame (fd4c3) and nothing writes its list, so its water never moved.
// The records are places in the world, so the other eye can read the owner's list as it is: for the length of its
// call of 1bfa60 the eye's state carries the owner's counter id and list handle, and its own again afterwards
// (the model: grass_movement_bridge.hpp, which this follows). The eye keeps its own wave textures and origin.
// Same frame, same counter manager (the state's first pointer), same graph of the handle only; when the owner is
// drawn after the eye, the eye's own - empty - list stays.
// Whether the rings are these bodies or the meshes drawn as water (keen::WaterMeshInstanceData, relative to the
// owner's camera: not lent here) is not known from the executable alone; EVR_WATER_DISPLACEMENT_PAUSE="<from> <to>"
// (diagnostic) lends nothing during those XR frames, to see the other eye's water calm down and move again.
// EVR_WATER_DISPLACEMENT=0 leaves the bridge out.
namespace waterDisplacementBridge {
using Stage=void(__fastcall*)(void*,void*,void*);
inline Stage original=nullptr;inline bool enabled=false;inline std::atomic<bool> failed{false};
// 1.0 (Hotfix 1.0.0.1): render_water_surface is 0x1ecd50, called at 0x135576 with the View's water state (+0xb48,
// June +0x8c8); its waves read the counter id at state+0x310 (0x1f0c28, June +0x1d8 at 1c4398) and the list at
// state+0x318 (0x1f0e2c, June +0x1e0 at 1c4579).
constexpr uintptr_t rva=0x1ed1a0,callerRva=0x1356bb,counterAt=0x310,listAt=0x318,counterRead=0x1f1078,listRead=0x1f127c;
constexpr const char* pattern="4C 89 44 24 18 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 C8 E7 FF FF B8 38 19 00 00";
struct Fields {uint32_t counter=0;std::array<uint8_t,16> list{};};
struct Source {uint64_t serial=0;uintptr_t state=0,parent=0;Fields fields;bool valid=false;};
inline thread_local Source source;
inline std::atomic<uint64_t> ownerCalls{0},eyeCalls{0},lent{0},without{0},paused{0};
inline uint64_t pauseFrom=0,pauseTo=0;
inline void fail(const char* reason) noexcept {
    if(failed.exchange(true))return;
    try{logFile->event("water_displacement_bridge_failure","\"reason\":"+jsonString(reason)+",\"eyes_keep_their_own_lists\":true");}catch(...){}
}
inline void report(const grassMovementBridge::Role& role,const Fields& own,bool lend,uint64_t number){
    if(number>4&&number%1800)return;
    std::ostringstream o;o<<"\"serial\":"<<role.serial<<",\"eye\":"<<role.eye<<",\"owner_of_the_effects\":"<<(role.owner?"true":"false")<<",\"calls\":"<<number
        <<",\"counter\":"<<own.counter<<",\"list\":"<<jsonString(nativeImpactComponents::asHex(own.list))<<",\"takes_the_owner_s_list\":"<<(lend?"true":"false")
        <<",\"lent\":"<<lent.load()<<",\"without_the_owner_s_record\":"<<without.load();
    logFile->event("water_displacement_bridge",o.str());
}
__declspec(noinline) void __fastcall invoke(void* graph,void* state,void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto* role=grassMovementBridge::current;
    if(!role||caller!=baseAddress+callerRva||failed.load()||!state){original(graph,state,parameters);return;}
    auto* const p=reinterpret_cast<uint8_t*>(state);Fields own;uintptr_t parent=0;
    std::memcpy(&parent,p,sizeof parent);std::memcpy(&own.counter,p+counterAt,4);std::memcpy(own.list.data(),p+listAt,16);
    if(role->owner){
        source={role->serial,reinterpret_cast<uintptr_t>(state),parent,own,true};
        try{report(*role,own,false,++ownerCalls);}catch(...){}
        original(graph,state,parameters);return;
    }
    // The owner's list of this frame: another state under the same counter manager, both counters made, the two
    // buffers different ones of the same graph (a buffer's handle begins with its graph).
    bool lend=source.valid&&source.serial==role->serial&&source.state!=reinterpret_cast<uintptr_t>(state)&&source.parent&&source.parent==parent&&
        source.fields.counter&&own.counter&&source.fields.counter!=own.counter&&
        !std::memcmp(source.fields.list.data(),own.list.data(),8)&&source.fields.list!=own.list;
    const auto number=++eyeCalls;
    if(lend&&liveFrame.frame>=pauseFrom&&liveFrame.frame<pauseTo){lend=false;++paused;}
    else if(lend)++lent;else ++without;
    try{report(*role,own,lend,number);}catch(...){}
    if(!lend){original(graph,state,parameters);return;}
    std::memcpy(p+counterAt,&source.fields.counter,4);std::memcpy(p+listAt,source.fields.list.data(),16);
    original(graph,state,parameters);
    std::memcpy(p+counterAt,&own.counter,4);std::memcpy(p+listAt,own.list.data(),16);
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_WATER_DISPLACEMENT",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='0'){logFile->event("water_displacement_bridge_off","\"reason\":\"EVR_WATER_DISPLACEMENT=0\"");return;}
        {char pause[48]{};const auto m=GetEnvironmentVariableA("EVR_WATER_DISPLACEMENT_PAUSE",pause,sizeof pause);
            if(m&&m<sizeof pause){std::istringstream in(pause);uint64_t a=0,b=0;if(in>>a>>b&&b>a){pauseFrom=a;pauseTo=b;
                logFile->event("water_displacement_bridge_pause","\"diagnostic\":true,\"from_frame\":"+std::to_string(a)+",\"to_frame\":"+std::to_string(b));}}}
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site||
           read<uint32_t>(baseAddress+counterRead+3)!=counterAt||read<uint32_t>(baseAddress+listRead+3)!=listAt)
            throw std::runtime_error("water surface stage signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("water surface stage hook failed");
        enabled=true;
        logFile->event("water_displacement_bridge_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"lends\":\"the list of WaterDisplacementBody and its counter, for the call\"");
    }catch(const std::exception& e){fail(e.what());}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("water_displacement_bridge_removed","\"status\":"+std::to_string(status)+",\"owner_calls\":"+std::to_string(ownerCalls.load())+",\"eye_calls\":"+std::to_string(eyeCalls.load())+
        ",\"lent\":"+std::to_string(lent.load())+",\"without_the_owner_s_record\":"+std::to_string(without.load())+",\"paused_by_the_diagnostic\":"+std::to_string(paused.load())+",\"failed\":"+(failed.load()?"true":"false"));}catch(...){}
}
}
