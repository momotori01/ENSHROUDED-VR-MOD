// The decals of the effects, for the eye that does not run the effects (2026-10-13; 1.0: 2026-10-17).
// The VFX update runs for one View a frame - the eye that owns the effects (or the original View) - and writes that
// View's decal list (keen::ForwardDecalData of 112 bytes) in that View's camera-relative space. The other eye borrowed
// that list as it was (shared_point_lights.hpp, -ShareDecals), so each mark sat displaced by the distance between the
// two cameras: headset run game-20261013-105615, "the scorch marks of the impacts are out of line between left and
// right"; again on 1.0 (headset report 1 of 2026-10-17). Shifting the eye's whole lighting instead would move its own
// sun shadows as well (user: "if it shifts, no - find another way; we fixed the decals in June, look at that").
// So, as in June, the eye gets a list of its own: the same pass once more into the eye's own buffer, with the payload
// that makes the layer's stand-in shader (shaders/vfx_decal_writeback.comp) move every record by
// delta = owner's camera position - this eye's. The count stays the owner's (the pass only reads the counter), so the
// eye's culling takes the owner's counter and its own buffer. Nothing of the owner's list or of the simulation is
// touched; without the stand-in shader, or after anything unexpected, the plain borrowing goes on as before.
// 1.0: the pass is no function of its own any more. 0x1aed70 (called at 0x131851 with the VFX stage's holder - its
// [[rcx]]+8 is the VFX object -, the graph context and the stage's parameters) writes four lists back one after
// another, each behind a test of the VFX object and a count in the parameters: +0x200 (particles; 0x1aeaf0), +0x240
// with parameters+0x248 (the decals: sort data VFX+0x258, records VFX+0x248, target descriptor parameters+0x1c8, the
// counter's id parameters+0x24c, the payload's allocation at 0x1af08a), +0x278 (a second list of 112-byte records into
// parameters+0x1f0, the same shader) and +0x2b0. The function reads the VFX object only for its values (+0x158, +0x180,
// +0x188 and each pass's resources) and hands it on to nothing but the first pass. The eye's call therefore gets a copy
// of the VFX object's first 0x400 bytes with the other three tests zeroed, and a copy of the parameters with the
// target's handle set to the eye's own buffer (View+0x580).
// EVR_DECAL_REBASE=0 leaves it off; EVR_DECAL_REBASE_FORM=2 takes the transform's fourth column as the decal's place
// instead (see the shader).
namespace vfxDecals {
using Handle=std::array<uint8_t,16>;
using Writeback=void(__fastcall*)(void*,void*,void*);
inline Writeback original=nullptr;
inline bool enabled=false,allocationHooked=false;inline std::atomic<bool> failed{false};inline uint32_t marker=evr::vfx::decalMarkerInto;
constexpr uintptr_t rva=0x1aed70,callerRva=0x131856,payloadCaller=0x1af08f;
constexpr const char* pattern="48 8B C4 55 53 56 41 56 48 8D A8 F8 FE FF FF 48 81 EC E8 01 00 00";
constexpr size_t parameterBytes=0x480,objectBytes=0x400,target=0x1c8,countAt=0x248,counterAt=0x24c;
constexpr std::array<size_t,3> otherPasses{0x200,0x278,0x2b0};constexpr size_t decalPass=0x240;
constexpr uintptr_t viewDecals=0x580,viewDecalCapacity=0xc8,viewDecalCounter=0xb70;   // shared_point_lights.hpp viewList
struct Source {PairResources* pair=nullptr;uint64_t serial=0;uintptr_t view=0;std::array<uint8_t,objectBytes> object{};std::array<uint8_t,parameterBytes> parameters{};bool ready=false;};
inline thread_local Source source;
inline std::atomic<uint64_t> sources{0},eyeWrites{0};
// For a test only (EVR_DECAL_REBASE_SHIFT="x y z", metres): added to delta, so the eye's marks stand that far from
// their place - a check that the eye shows its own list and which way the transform takes the shift.
inline Vec3 testShift{};
inline bool active(){return enabled&&!failed.load()&&!vfxTransparent::failed;}
// The owner's pass of this very frame is on record: the eye can have its own list.
inline bool sourceFor(const PairResources& pair){
    const auto& frame=vfxTransparent::source;
    return active()&&source.ready&&source.pair==&pair&&source.serial==pair.serial&&frame.pair==&pair&&frame.serial==pair.serial;
}
inline void fail(const char* reason) noexcept {
    if(!failed.exchange(true))try{logFile->event("vfx_decal_rebase_failure","\"reason\":"+jsonString(reason)+",\"falls_back_to\":\"the owner's list as it is\"");}catch(...){}
}
// The owner's call of this frame: what it was called with, for the eye's call.
void __fastcall invoke(void* holder,void* graphContext,void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    original(holder,graphContext,parameters);
    const ObserverLastError preserve;
    if(!vfxTransparent::observingOriginal||caller!=baseAddress+callerRva||failed.load())return;
    try{
        const auto& frame=vfxTransparent::source;   // the owner's frame, set where its VFX stages begin
        if(!frame.pair||GetCurrentThreadId()!=frame.pair->renderThread)throw std::runtime_error("decal writeback outside the owner's frame");
        const auto object=read<uintptr_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(holder))+8);
        if(!object)throw std::runtime_error("decal writeback without its VFX object");
        source.pair=frame.pair;source.serial=frame.serial;source.view=frame.ownerView;
        source.object=read<std::array<uint8_t,objectBytes>>(object);
        source.parameters=read<std::array<uint8_t,parameterBytes>>(reinterpret_cast<uintptr_t>(parameters));source.ready=true;
        const auto n=++sources;
        if(n<=3){const auto p=reinterpret_cast<uintptr_t>(source.parameters.data());const auto o=reinterpret_cast<uintptr_t>(source.object.data());
            logFile->event("vfx_decal_rebase_source","\"serial\":"+std::to_string(source.serial)+",\"owner_view\":"+hex(source.view)+",\"decal_pass\":"+std::to_string(read<uint32_t>(o+decalPass))+
                ",\"other_passes\":["+std::to_string(read<uint32_t>(o+otherPasses[0]))+","+std::to_string(read<uint32_t>(o+otherPasses[1]))+","+std::to_string(read<uint32_t>(o+otherPasses[2]))+"]"+
                ",\"target_tag\":"+std::to_string(read<uint8_t>(p+target))+",\"target_stride\":"+std::to_string(read<uint32_t>(p+target+0x20))+",\"target_count\":"+std::to_string(read<uint32_t>(p+target+0x24))+
                ",\"count\":"+std::to_string(read<uint32_t>(p+countAt))+",\"counter\":"+std::to_string(read<uint32_t>(p+counterAt))+
                ",\"owner_buffer_is_target\":"+(source.view&&read<Handle>(source.view+viewDecals)==read<Handle>(p+target+8)?"true":"false")+",\"read_only\":true");}
    }catch(const std::exception& e){fail(e.what());}
}
// The eye's own list for this frame. true: the eye's buffer (View+0x580) holds the owner's decals of this frame in the
// eye's own space, as many as the owner's counter says. Called where the eye's clustered culling begins.
inline bool writeEye(void* graphContext,PairResources& pair,unsigned eye,uintptr_t eyeView,uintptr_t sourceView) noexcept {
    if(!sourceFor(pair))return false;
    const ObserverLastError preserve;
    try{
        if(eye>1||GetCurrentThreadId()!=pair.renderThread||!eyeView||!sourceView||eyeView==sourceView)throw std::runtime_error("decal writeback outside the private eye");
        auto parameters=source.parameters;const auto p=reinterpret_cast<uintptr_t>(parameters.data());
        const auto own=read<Handle>(eyeView+viewDecals),owner=read<Handle>(sourceView+viewDecals);const auto capacity=read<uint32_t>(sourceView+viewDecalCapacity);
        // the descriptor's kind: 9 in June, 11 in 1.0 (Mock game-20261017-120552)
        const auto kind=read<uint8_t>(p+target);
        if((kind!=9&&kind!=11)||(source.view&&source.view!=sourceView)||read<Handle>(p+target+8)!=owner||read<uint64_t>(p+target+0x18)!=0||read<uint32_t>(p+target+0x20)!=112||read<uint32_t>(p+target+0x24)!=capacity||
           read<uint32_t>(p+counterAt)!=read<uint32_t>(sourceView+viewDecalCounter)||!capacity||capacity>65536||!read<uint32_t>(p+countAt)||
           read<uint32_t>(eyeView+viewDecalCapacity)!=capacity||own==owner||!read<uint64_t>(eyeView+viewDecals))
            throw std::runtime_error("decal writeback parameters differ from the Views");
        std::memcpy(parameters.data()+target+8,own.data(),16);
        // Only the decal pass: the VFX object's other three tests zeroed in a copy.
        auto object=source.object;const auto o=reinterpret_cast<uintptr_t>(object.data());
        if(!read<uint32_t>(o+decalPass))throw std::runtime_error("the owner's decal pass was closed");
        for(const auto at:otherPasses)std::memset(object.data()+at,0,4);
        const std::array<uintptr_t,2> inner{0,o};const uintptr_t holder=reinterpret_cast<uintptr_t>(inner.data());
        // As for the transparent particles: the owner's camera position - this eye's, in metres (Q32 positions).
        const auto from=vfxTransparent::source.position;const auto to=read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(pair.parameters[eye].data())+0x38);
        constexpr double q=4294967296.;
        const Vec3 delta{float((double(from.x)-double(to.x))/q),float((double(from.y)-double(to.y))/q),float((double(from.z)-double(to.z))/q)};
        if(!finite(delta)||std::abs(delta.x)>50||std::abs(delta.y)>50||std::abs(delta.z)>50)throw std::runtime_error("decal delta is not a distance between two eyes");
        const Vec3 moved{delta.x+testShift.x,delta.y+testShift.y,delta.z+testShift.z};
        vfxTransparent::ExtraPayload payload;payload.caller=payloadCaller;payload.words=evr::vfx::decalPayload({moved.x,moved.y,moved.z},marker);
        struct Arm {vfxTransparent::ExtraPayload* previous;explicit Arm(vfxTransparent::ExtraPayload& p):previous(vfxTransparent::extraPayload){vfxTransparent::extraPayload=&p;}
            ~Arm(){vfxTransparent::extraPayload=previous;}};
        {Arm arm(payload);original(const_cast<uintptr_t*>(&holder),graphContext,parameters.data());}
        // Without the longer payload the pass copied the owner's records as they are: the same picture as the borrowing.
        if(payload.used!=1)throw std::runtime_error("decal payload was not extended");
        const auto n=++eyeWrites;
        if(n<=4||n%1800==0){std::ostringstream s;s<<std::setprecision(7)<<"\"serial\":"<<pair.serial<<",\"eye\":"<<eye<<",\"writes\":"<<n<<",\"delta\":["<<delta.x<<','<<delta.y<<','<<delta.z
            <<"],\"form\":"<<(marker==evr::vfx::decalMarkerInto?"\"into_the_decal\"":"\"place_of_the_decal\"")<<",\"capacity\":"<<capacity<<",\"count_from\":\"the owner's counter\"";
            logFile->event("vfx_decal_rebase_eye",s.str());}
        return true;
    }catch(const std::exception& e){fail(e.what());return false;}
}
inline void install() noexcept {
    try{
        char value[8]{};auto n=GetEnvironmentVariableA("EVR_DECAL_REBASE",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='0'){logFile->event("vfx_decal_rebase_off","\"reason\":\"EVR_DECAL_REBASE=0\"");return;}
        n=GetEnvironmentVariableA("EVR_DECAL_REBASE_FORM",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='2')marker=evr::vfx::decalMarkerPlace;
        {char shift[64]{};const auto m=GetEnvironmentVariableA("EVR_DECAL_REBASE_SHIFT",shift,sizeof shift);
            if(m&&m<sizeof shift){std::istringstream in(shift);Vec3 s{};if(in>>s.x>>s.y>>s.z&&finite(s)&&std::abs(s.x)<=5&&std::abs(s.y)<=5&&std::abs(s.z)<=5){testShift=s;
                logFile->event("vfx_decal_rebase_test_shift","\"diagnostic\":true,\"shift\":["+std::to_string(s.x)+','+std::to_string(s.y)+','+std::to_string(s.z)+"]");}}}
        if(!vfxTransparent::render){logFile->event("vfx_decal_rebase_off","\"reason\":\"the transparent bridge of the eye is not in use\"");return;}
        using Status=uint32_t(*)();const auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        const auto status=module?reinterpret_cast<Status>(GetProcAddress(module,"evrVfxDecalShaderStatus")):nullptr;
        if(!status||!status()){logFile->event("vfx_decal_rebase_off","\"reason\":\"the stand-in decal shader was not prepared at process launch\"");return;}
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        const auto allocation=baseAddress+payloadCaller-5;auto& allocate=vfxTransparent::hooks[0];
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site||
           read<uint8_t>(allocation)!=0xe8||uintptr_t(int64_t(allocation+5)+read<int32_t>(allocation+1))!=baseAddress+allocate.rva)
            throw std::runtime_error("decal writeback signature/caller changed");
        // 1.0: the transparent bridge only borrows, so the payload's allocation (0x4e5f0) is hooked here; its detour
        // changes nothing but an armed call from the return address above.
        if(!allocate.enabled){const auto p=parsePattern(allocate.pattern);const auto at=baseAddress+allocate.rva;
            if(scan(reinterpret_cast<const uint8_t*>(at),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("payload allocation signature changed");
            if(MH_CreateHook(reinterpret_cast<void*>(at),reinterpret_cast<void*>(&vfxTransparent::allocate),reinterpret_cast<void**>(&vfxTransparent::originalAllocate))!=MH_OK||
               MH_EnableHook(reinterpret_cast<void*>(at))!=MH_OK)throw std::runtime_error("payload allocation hook failed");
            allocate.enabled=true;allocationHooked=true;}
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("decal writeback hook failed");
        enabled=true;
        logFile->event("vfx_decal_rebase_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"payload_caller\":"+hex(payloadCaller)+",\"shader_modules\":"+std::to_string(status())+
            ",\"allocation_hooked_here\":"+(allocationHooked?"true":"false")+",\"form\":"+(marker==evr::vfx::decalMarkerInto?"\"into_the_decal\"":"\"place_of_the_decal\""));
    }catch(const std::exception& e){fail(e.what());}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("vfx_decal_rebase_removed","\"status\":"+std::to_string(status)+",\"owner_calls\":"+std::to_string(sources.load())+",\"eye_writes\":"+std::to_string(eyeWrites.load())+",\"failed\":"+(failed.load()?"true":"false"));}catch(...){}
}
}
