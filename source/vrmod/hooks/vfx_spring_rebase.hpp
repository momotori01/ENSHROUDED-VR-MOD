// The fishing line and the rope of the grappling hook in the eye that does not own the VFX.
// User, 2026-10-13: "the string of the cast fishing rod and the string of the grappling hook look odd - is it drawn
// differently?" Measured in both eyes' pictures: the line and the rope stand at the SAME place of the screen in both
// eyes (left minus right 0..+7 pixels along the whole rope where the hand beside it has +106; Mock runs
// game-20261013-164738, game-20261013-185120 frames 41011 and 40951): no depth, and off the hand by the eyes' distance.
// What they are (read from the executable; scratchpad report re_vfx_cpu_instances.md of that day):
// keen::VfxSpringResource effects. Once a frame, for the View that owns the VfxView, the VFX update (17dc90, called by
// f7c50 at fe210) lets 173fa0 make one keen::VfxCpuDrawcallInstanceData (80 bytes: position relative to THAT View's
// camera, scale, orientation, and the same of the frame before) per segment into vfxView+648 (84 bytes a record: a
// sort key, then the 80; their number at vfxView+2c7e0, at most 0x800), 169980 makes the draw records of the three
// passes (vfxView+2a648 + pass * b28: +0 the array, +8 its length; a record is 44 bytes, +0 first instance, +4
// instance count), and at 17edf1 f93200 hands out the frame's part (0x28000 bytes = 0x800 instances) of the GPU buffer
// vfxView+2c7e8, into which the loop at 17ee20 copies the first n. 188730 ("vfx_cpu_drawcalls") binds that part and
// draws each record with its first instance. The other eye (vfx_model_bridge.hpp) draws the same records from the same
// part, so with the owner's camera-relative positions.
// Here: the part has room for 0x800 instances and all of it is transferred. After f93200 has handed it out, a copy of
// the n instances is written behind them for each eye that does not own the VFX, with (owner's camera - that eye's
// camera) added to both positions - the same shift the transparent particles get (vfx_transparent_bridge.hpp). While
// that eye records its draws (vfxModelBridge::record), the first instance of every draw record of the pass is moved
// to the copy, and back afterwards. Nothing is allocated, no native call is added; the owner's draws are untouched.
// Only when the copies fit (n * (1 + eyes) <= 0x800) and were written in this frame before the eye draws.
// EVR_VFX_SPRING_REBASE=0 in the game's environment leaves it out.
// 1.0 (Hotfix 1.0.0.1, 2026-10-16): the VfxView keeps the instances behind a pointer (+0x6b8, 84 bytes a record, the
// 80 at +4), their number at +0x768, the size of the part at +0x6c0 (no longer a fixed 0x800; the part is one of a
// ring, chosen by the frame number in 0x19ecd0 and again in the draws 0x1af7a0), the GPU buffer at +0x6c8 and the
// three passes' draw records at +0x6d8 + pass * 0x28 (+0 the array, +8 its length; a record is still 44 bytes). The
// part is handed out by the same 0x1014aa0, now called at 0x19ee09 from 0x19ecd0 (called by the VFX update 0x1a1db0).
namespace vfxSpringRebase {
using Upload=void*(__fastcall*)(void*,void*,void*,void*);
inline Upload original=nullptr;inline bool enabled=false;inline std::atomic<bool> failed{false};
constexpr uintptr_t rva=0x1017d40,callerRva=0x19ef9e;
constexpr const char* pattern="48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 81 EC 90 00 00 00 49 8B E9 49 8B";
constexpr const char* callerPattern="44 39 83 68 07 00 00";   // 19ee29: cmp dword ptr [rbx+768], r8d (the copy loop's count)
constexpr uintptr_t callerPatternAt=0x19efb9;
constexpr size_t instanceBytes=80,sourceStride=0x54,sourceInstance=4,recordBytes=0x2c;
constexpr uintptr_t countAt=0x768,capacityAt=0x6c0,bufferAt=0x6c8,sourceAt=0x6b8,listsAt=0x6d8,listStride=0x28;
struct Written {uint64_t serial=0;uintptr_t vfx=0;uint32_t count=0;std::array<uint32_t,2> base{};std::array<bool,2> has{};};
// Both eyes' cameras of the frame, published by the policy before the first eye is drawn. While the owner's eye is
// drawn, pair.parameters of the other eye still hold the frame before: on the grappling hook's pull that was 8 to 12 cm
// of the player's own travel in the shift, and the right eye's rope stood off the hand (Mock session
// game-20261014-004300, frame 22181; the standing fishing cast had hidden it).
struct FrameEyes {uint64_t serial=0;std::array<WorldPositionQ32,2> position{};bool valid=false;};
inline thread_local FrameEyes frameEyes;
inline std::atomic<uint64_t> fromFrameEyes{0},fromParameters{0};
inline thread_local Written written;
inline std::atomic<uint64_t> uploads{0},copied{0},withoutRoom{0},shiftedDraws{0},lines{0};
inline void fail(const char* reason) noexcept {
    if(failed.exchange(true))return;
    try{logFile->event("vfx_spring_rebase_failure","\"reason\":"+jsonString(reason));}catch(...){}
}
__declspec(noinline) void* __fastcall upload(void* out,void* system,void* range,void* flags){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    void* result=original(out,system,range,flags);
    if(caller!=baseAddress+callerRva||failed.load())return result;
    const ObserverLastError preserve;
    try{
        const auto& s=vfxTransparent::source;
        if(!vfxTransparent::observingOriginal||!s.pair||s.serial!=s.pair->serial||GetCurrentThreadId()!=s.pair->renderThread)return result;
        auto& pair=*s.pair;
        const auto vfxScene=read<uintptr_t>(pair.scene+layout::sceneVfx);const auto vfx=vfxScene?vfxScene+layout::vfxSceneView:0;   // 1.0: the VfxView inside the VFX scene
        if(!vfx||!result)return result;
        const auto r=reinterpret_cast<uintptr_t>(range);
        const auto slots=read<uint32_t>(vfx+capacityAt);
        if(!slots||slots>0x10000)throw std::runtime_error("the VfxView's instance part size is not usable");
        if(read<std::array<uint8_t,16>>(r)!=read<std::array<uint8_t,16>>(vfx+bufferAt)||read<uint64_t>(r+0x18)!=uint64_t(slots)*instanceBytes)
            throw std::runtime_error("the instance part handed out is not the VfxView's");
        const auto dest=read<uintptr_t>(reinterpret_cast<uintptr_t>(result));const auto n=read<uint32_t>(vfx+countAt);
        written={};written.serial=pair.serial;written.vfx=vfx;written.count=n;++uploads;
        const auto source=read<uintptr_t>(vfx+sourceAt);
        if(!dest||!n||!source)return result;
        if(n>slots)throw std::runtime_error("more instances than the part holds");
        unsigned index=0;constexpr double q=4294967296.;std::array<Vec3,2> deltas{};
        for(unsigned eye=0;eye<2;++eye){
            if(int(eye)==worldSharedStages::vfxOwnerEye)continue;
            const uint32_t base=n*(1+index);++index;
            if(uint64_t(base)+n>slots){++withoutRoom;continue;}
            const bool published=frameEyes.valid&&frameEyes.serial==pair.serial;if(published)++fromFrameEyes;else ++fromParameters;
            const auto position=published?frameEyes.position[eye]:read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(pair.parameters[eye].data())+0x38);
            const Vec3 delta{float((double(s.position.x)-double(position.x))/q),float((double(s.position.y)-double(position.y))/q),float((double(s.position.z)-double(position.z))/q)};
            if(!finite(delta)||length(delta)>1000)throw std::runtime_error("the shift between the owner's camera and the eye's is not usable");
            for(uint32_t i=0;i<n;++i){
                std::array<float,20> instance;std::memcpy(instance.data(),reinterpret_cast<const void*>(source+sourceInstance+size_t(i)*sourceStride),instanceBytes);
                instance[0]+=delta.x;instance[1]+=delta.y;instance[2]+=delta.z;       // transform.cameraRelativePosition
                instance[10]+=delta.x;instance[11]+=delta.y;instance[12]+=delta.z;    // prevTransform.cameraRelativePosition
                std::memcpy(reinterpret_cast<void*>(dest+(size_t(base)+i)*instanceBytes),instance.data(),instanceBytes);
            }
            written.base[eye]=base;written.has[eye]=true;deltas[eye]=delta;++copied;
        }
        if(lines.fetch_add(1)<12||lines.load()%3000==0){
            std::ostringstream o;o<<std::setprecision(6)<<"\"serial\":"<<pair.serial<<",\"owner_eye\":"<<worldSharedStages::vfxOwnerEye<<",\"instances\":"<<n
                <<",\"copy_for_eye\":["<<(written.has[0]?"true":"false")<<','<<(written.has[1]?"true":"false")<<"],\"first_instance_of_copy\":["<<written.base[0]<<','<<written.base[1]
                <<"],\"shift\":[["<<deltas[0].x<<','<<deltas[0].y<<','<<deltas[0].z<<"],["<<deltas[1].x<<','<<deltas[1].y<<','<<deltas[1].z<<"]],\"frames_without_room\":"<<withoutRoom.load()
                <<",\"eye_cameras_of_this_frame\":"<<(frameEyes.valid&&frameEyes.serial==pair.serial?"true":"false");
            logFile->event("vfx_spring_rebase",o.str());}
    }catch(const std::exception& e){fail(e.what());}
    return result;
}
// Around the eye's call of 188730: the first instance of each draw record of this pass points at the eye's copy.
struct EyeDraw {
    uintptr_t records=0;uint64_t count=0;uint32_t base=0;
    EyeDraw(uintptr_t vfx,uint32_t pass,uint64_t serial,int eye) noexcept {
        if(!enabled||failed.load()||eye<0||eye>1||pass>2||written.serial!=serial||written.vfx!=vfx||!written.has[size_t(eye)])return;
        try{
            const auto list=vfx+listsAt+uintptr_t(pass)*listStride;const auto data=read<uintptr_t>(list);const auto n=read<uint64_t>(list+8);
            if(!data||!n)return;
            if(n>4096)throw std::runtime_error("draw record list too long");
            for(uint64_t i=0;i<n;++i){const auto first=read<uint32_t>(data+i*recordBytes),instances=read<uint32_t>(data+i*recordBytes+4);
                if(uint64_t(first)+instances>written.count)throw std::runtime_error("a draw record names instances past the frame's");}
            base=written.base[size_t(eye)];
            for(uint64_t i=0;i<n;++i){auto first=read<uint32_t>(data+i*recordBytes)+base;std::memcpy(reinterpret_cast<void*>(data+i*recordBytes),&first,4);}
            records=data;count=n;++shiftedDraws;
        }catch(const std::exception& e){records=0;fail(e.what());}
    }
    ~EyeDraw() noexcept {
        if(!records)return;
        for(uint64_t i=0;i<count;++i){uint32_t first;std::memcpy(&first,reinterpret_cast<const void*>(records+i*recordBytes),4);first-=base;std::memcpy(reinterpret_cast<void*>(records+i*recordBytes),&first,4);}
    }
    EyeDraw(const EyeDraw&)=delete;EyeDraw& operator=(const EyeDraw&)=delete;
};
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_SPRING_REBASE",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='0'){logFile->event("vfx_spring_rebase_off","\"reason\":\"EVR_VFX_SPRING_REBASE=0\"");return;}
        const auto bytes=parsePattern(pattern),after=parsePattern(callerPattern);const auto site=baseAddress+rva,call=baseAddress+callerRva-5;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site||
           scan(reinterpret_cast<const uint8_t*>(baseAddress+callerPatternAt),after.size(),after)!=std::vector<size_t>{0}||
           read<uint32_t>(baseAddress+0x19ef29+2)!=capacityAt||read<uint32_t>(baseAddress+0x1af95b+4)!=listsAt+8||read<uint32_t>(baseAddress+0x1af963+4)!=listsAt)
            throw std::runtime_error("spring instance upload signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&upload),reinterpret_cast<void**>(&original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("spring instance upload hook failed");
        enabled=true;
        logFile->event("vfx_spring_rebase_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"copies_behind_the_frame_s_instances\":true,\"first_instance_moved_for_the_eye_s_draws\":true");
    }catch(const std::exception& e){fail(e.what());}
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("vfx_spring_rebase_removed","\"status\":"+std::to_string(status)+",\"uploads\":"+std::to_string(uploads.load())+",\"copies\":"+std::to_string(copied.load())+
        ",\"eye_draw_calls_moved\":"+std::to_string(shiftedDraws.load())+",\"frames_without_room\":"+std::to_string(withoutRoom.load())+
        ",\"copies_from_this_frame_s_cameras\":"+std::to_string(fromFrameEyes.load())+",\"copies_from_the_eye_s_last_parameters\":"+std::to_string(fromParameters.load())+",\"failed\":"+(failed.load()?"true":"false"));}catch(...){}
}
}
