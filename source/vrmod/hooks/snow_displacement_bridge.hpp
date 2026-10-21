// Footprints and trails in snow, in both eyes (2026-10-20, from the list of what was known to be the owner's eye only:
// docs/analysis/vfx_stereo_20261013/re_water_per_view.md 6.9, STATUS 10-13 "the same shape elsewhere: snow").
// Every View with a snow displacement state runs the stage after the grass's (grass_movement_bridge.hpp): June
// render_snow_displacement 0x149280 called at 0xfe29b for View+0x9f0; in this build 0x1710c0 called at 0x13171d for
// View+0xf80, right after the grass's 0xc22d0 at 0x1316f7 for View+0xf78, the frame's list record the third argument
// as there (rbp+0x9910 beside the grass's rbp+0x9ea0, copied the same way at 0x13111d..0x13113d: a dword counter id at
// +0, the buffer's 16 bytes at +8, the offset and the size from +0x18). The bodies that press the snow are written for
// the View that owns the effects only; the other eye's list stays empty, so its snow was never pressed (guess from the
// code, not seen: no snow at the places the Mock can reach). Here, as for the grass, the other eye takes the owner's
// list for the length of its call and keeps its own textures. EVR_SNOW_DISPLACEMENT=0 leaves the bridge out.
namespace snowDisplacementBridge {
using Stage=void(__fastcall*)(void*,void*,void*);
inline Stage original=nullptr;inline bool enabled=false;inline std::atomic<bool> failed{false};
constexpr uintptr_t rva=0x1711d0,callerRva=0x131822;
constexpr const char* pattern="48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 78 FE FF FF 48 81 EC 88 02";
using Fields=grassMovementBridge::Fields;
struct Source {uint64_t serial=0;Fields fields;bool valid=false;};
inline thread_local Source source;
inline std::atomic<uint64_t> ownerCalls{0},eyeCalls{0},lent{0},without{0};
inline void fail(const char* reason) noexcept {
    if(failed.exchange(true))return;
    try{logFile->event("snow_displacement_bridge_failure","\"reason\":"+jsonString(reason)+",\"eyes_keep_their_own_lists\":true");}catch(...){}
}
inline void report(const grassMovementBridge::Role& role,const Fields& own,bool lend,uint64_t number){
    if(number>4&&number%1800)return;
    std::ostringstream o;o<<"\"serial\":"<<role.serial<<",\"eye\":"<<role.eye<<",\"owner_of_the_effects\":"<<(role.owner?"true":"false")<<",\"calls\":"<<number
        <<",\"counter\":"<<own.counter<<",\"list\":"<<jsonString(nativeImpactComponents::asHex(own.list))<<",\"offset\":"<<own.offset<<",\"bytes\":"<<own.bytes
        <<",\"takes_the_owner_s_list\":"<<(lend?"true":"false")<<",\"lent\":"<<lent.load()<<",\"without_the_owner_s_record\":"<<without.load();
    logFile->event("snow_displacement_bridge",o.str());
}
__declspec(noinline) void __fastcall invoke(void* graph,void* state,void* frame){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto* role=grassMovementBridge::current;
    if(!role||caller!=baseAddress+callerRva||failed.load()||!frame){original(graph,state,frame);return;}
    auto* const p=reinterpret_cast<uint8_t*>(frame);Fields own;
    std::memcpy(&own.counter,p,4);std::memcpy(own.list.data(),p+8,16);std::memcpy(&own.offset,p+0x18,4);std::memcpy(&own.bytes,p+0x20,8);
    if(role->owner){
        source={role->serial,own,true};
        try{report(*role,own,false,++ownerCalls);}catch(...){}
        original(graph,state,frame);return;
    }
    // The owner's list of this frame, in this graph (a buffer's handle begins with its graph), of the same size.
    const bool lend=source.valid&&source.serial==role->serial&&!std::memcmp(source.fields.list.data(),own.list.data(),8)&&source.fields.bytes==own.bytes&&own.bytes>0;
    const auto number=++eyeCalls;
    if(lend)++lent;else ++without;
    try{report(*role,own,lend,number);}catch(...){}
    if(!lend){original(graph,state,frame);return;}
    std::memcpy(p,&source.fields.counter,4);std::memcpy(p+8,source.fields.list.data(),16);std::memcpy(p+0x18,&source.fields.offset,4);
    original(graph,state,frame);
    std::memcpy(p,&own.counter,4);std::memcpy(p+8,own.list.data(),16);std::memcpy(p+0x18,&own.offset,4);
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SNOW_DISPLACEMENT",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='0'){logFile->event("snow_displacement_bridge_off","\"reason\":\"EVR_SNOW_DISPLACEMENT=0\"");return;}
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)
            throw std::runtime_error("snow displacement stage signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("snow displacement stage hook failed");
        enabled=true;
        logFile->event("snow_displacement_bridge_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern)+",\"lends\":\"the owner's list of snow displacement bodies and its counter, for the call\"");
    }catch(const std::exception& e){fail(e.what());}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("snow_displacement_bridge_removed","\"status\":"+std::to_string(status)+",\"owner_calls\":"+std::to_string(ownerCalls.load())+",\"eye_calls\":"+std::to_string(eyeCalls.load())+
        ",\"lent\":"+std::to_string(lent.load())+",\"without_the_owner_s_record\":"+std::to_string(without.load())+",\"failed\":"+(failed.load()?"true":"false"));}catch(...){}
}
}
