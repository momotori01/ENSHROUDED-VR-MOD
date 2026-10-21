namespace stableFrustumBridge {
using Build=bool(__fastcall*)(void*,const void*,const void*);
inline Build original=nullptr;
inline bool enabled=false;
constexpr uintptr_t rva=0xf4fe00;
constexpr std::array<uintptr_t,2> callers{0x12b694,0x12b6ae};   // 1.0 (June 0xf9226, 0xf9240: not remapped by the matcher)
constexpr const char* pattern="48 89 5C 24 08 48 89 74 24 10 57 48 81 EC A0 00 00 00 0F 57 C0 0F 57 C9 48 8B DA 0F 29 44 24 20 48 8B";
inline bool observeEyes=false,stabilizeEyes=false;
struct Context{uint64_t serial=0,frame=0;unsigned calls=0,mask=0;bool failed=false;int eye=-1;bool apply=true;};
inline thread_local Context* current=nullptr;
// VFX owner eye on desktop-skip frames (2026-10-06): the VFX planes builder (caller f9240) is run a second time
// with the owner descriptor widened to both eyes (vfx_culling_union.hpp). The native output is untouched; only
// vfxCulling passes the widened planes to the VFX camera stage 179a80.
struct VfxWiden{game::ProjectionDescriptor eye,wide;game::TangentBounds bounds{};std::array<uint8_t,0x60> planes{};
    unsigned builds=0,applied=0;bool failed=false;std::string reason;};
inline thread_local VfxWiden* vfxWiden=nullptr;
struct VfxScope{VfxWiden* previous=vfxWiden;explicit VfxScope(VfxWiden* value){vfxWiden=value;}~VfxScope(){vfxWiden=previous;}};
struct Scope{
    Context* previous=current;
    explicit Scope(Context* value){current=value;}
    ~Scope(){current=previous;}
};
bool __fastcall invoke(void* output,const void* input,const void* descriptor){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    const bool nativeOkay=original(output,input,descriptor);
    if(vfxWiden&&caller==callers[1]){
        auto& w=*vfxWiden;
        try{
            if(!nativeOkay||w.builds||read<game::ProjectionDescriptor>(reinterpret_cast<uintptr_t>(descriptor)).bytes!=w.eye.bytes)
                throw std::runtime_error("VFX owner frustum descriptor differs");
            alignas(16) std::array<uint8_t,0x100> planes{};
            if(!original(planes.data(),input,w.wide.bytes.data()))throw std::runtime_error("native builder rejected widened descriptor");
            std::memcpy(w.planes.data(),planes.data(),w.planes.size());++w.builds;
        }catch(const std::exception& e){w.failed=true;w.reason=e.what();}
    }
    if(!current||(caller!=callers[0]&&caller!=callers[1]))return nativeOkay;
    auto& c=*current;++c.calls;
    try{
        const unsigned bit=caller==callers[0]?1:2;
        if(c.mask&bit)throw std::runtime_error("duplicate shared frustum builder");
        c.mask|=bit;
        const auto m=read<game::MatrixRows>(reinterpret_cast<uintptr_t>(input));
        const auto d=read<game::ProjectionDescriptor>(reinterpret_cast<uintptr_t>(descriptor));
        if(!nativeOkay||!eyeProjection||d.bytes!=eyeProjection->bytes)
            throw std::runtime_error("native shared frustum descriptor mismatch");
        const auto planes=game::stableFrustum(m,d);
        if(c.eye>=0){
            if(c.frame<=450||c.frame%30==0){
                const auto native=read<game::FrustumPlanes>(reinterpret_cast<uintptr_t>(output));
                std::ostringstream f;f<<std::setprecision(9)<<"\"frame\":"<<c.frame<<",\"serial\":"<<c.serial<<",\"eye\":"<<c.eye
                    <<",\"caller\":"<<hex(caller)<<",\"applied\":"<<(c.apply?"true":"false")<<",\"matrix\":[";
                for(unsigned i=0;i<16;i++){if(i)f<<',';f<<((i==3||i==7||i==11)?0:(i==15?1:m.values[i]));}f<<']';
                std::ostringstream descriptorHex;descriptorHex<<std::hex<<std::setfill('0');for(auto b:d.bytes)descriptorHex<<std::setw(2)<<unsigned(b);f<<",\"descriptor\":"<<jsonString(descriptorHex.str());
                for(auto entry:{std::pair<const char*,const game::FrustumPlanes*>{"native_planes",&native},{"stable_planes",&planes}}){
                    f<<",\""<<entry.first<<"\":[";for(unsigned i=0;i<6;i++){if(i)f<<',';f<<'[';
                        for(unsigned j=0;j<4;j++){if(j)f<<',';f<<(*entry.second)[i][j];}f<<']';}f<<']';}
                logFile->event("eye_frustum_planes",f.str());
            }
            if(c.apply)std::memcpy(output,planes.data(),sizeof(planes));
            return nativeOkay;
        }
        std::ostringstream fields;fields<<std::setprecision(9)<<"\"serial\":"<<c.serial<<",\"frame\":"<<c.frame
            <<",\"caller\":"<<hex(caller)<<",\"planes\":[";
        for(size_t i=0;i<planes.size();i++){
            if(i)fields<<',';fields<<'[';
            for(size_t k=0;k<4;k++){if(k)fields<<',';fields<<planes[i][k];}fields<<']';
        }
        fields<<']';
        logFile->event("shared_stable_frustum",fields.str());
        std::memcpy(output,planes.data(),sizeof(planes));
    }catch(const std::exception& error){c.failed=true;
        try{logFile->event("shared_stable_frustum_failure","\"serial\":"+std::to_string(c.serial)+",\"reason\":"+jsonString(error.what()));}catch(...){}
    }
    return nativeOkay;
}
inline void install(){
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0})
        throw std::runtime_error("frustum builder signature mismatch");
    for(auto caller:callers){const auto call=baseAddress+caller-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
            throw std::runtime_error("frustum builder callsite mismatch");
    }
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("stable frustum hook failed");
    enabled=true;
    logFile->event("shared_stable_frustum_hook_enabled","\"rva\":"+hex(rva)+",\"pattern\":"+jsonString(pattern));
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("shared_stable_frustum_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
}
}

// VFX camera stage 179a80 (called from f7c50 at fd055 with the VFX planes copied from the f9240 output). On VFX
// owner frames the copy it receives carries the widened planes, so VFX light/particle culling covers both eyes.
namespace vfxCulling {
using Camera=void(__fastcall*)(void*,void*,const void*);
inline Camera original=nullptr;
inline bool enabled=false;
// The private eye's own model culling (vfx_model_bridge.hpp) reaches this stage with no VfxView; it supplies the
// Scene's, or null when it is not that eye's turn (then nothing is prepared, as for a View without a VfxView).
inline void* (*eyeVfxView)()=nullptr;
// 1.0 (Hotfix 1.0.0.1): the stage is 0x19f7e0, called once from the View's render at 0x1301f5 with the View's VfxView
// (+0x9a8); it reads the six planes and the float at +0x60 as June's 0x179a80 (called at 0xfd055 with View+0x738).
constexpr uintptr_t rva=0x19f970,callerRva=0x13033a;
constexpr const char* pattern="4C 8B DC 49 89 5B 08 49 89 6B 10 49 89 73 18 49 89 7B 20 41 56 48 81 EC C0 00 00 00";
void __fastcall invoke(void* output,void* vfxView,const void* camera){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    if(!vfxView&&caller==callerRva){vfxView=eyeVfxView?eyeVfxView():nullptr;if(!vfxView)return;original(output,vfxView,camera);return;}
    auto* w=stableFrustumBridge::vfxWiden;
    if(!w||caller!=callerRva||w->failed||w->builds!=1||w->applied){original(output,vfxView,camera);return;}
    // 19f7e0 (June 179a80) reads six planes (0x60) and one float at +0x60 from this argument; pass a widened copy.
    // 1.0 also reads a byte at +0x70 (2026-10-18, 0x19f983: when it is set, a second part of the GPU buffer +0x480 is
    // handed out into the output's +0x20..+0x3f): the copy was 0x70 bytes, so that byte came from the stack beyond it.
    alignas(16) std::array<uint8_t,0x78> copy{};
    std::memcpy(copy.data(),camera,copy.size());
    std::memcpy(copy.data(),w->planes.data(),w->planes.size());
    // The first calls on record: the game's six planes and the widened ones (2026-10-17: the owner eye drew none of the VFX
    // models after its lighting - RenderDoc game-20261017-034858 - while the other eye, culled without the widening, did).
    {static std::atomic<unsigned> told{0};if(told.fetch_add(1)<3)try{std::array<float,28> a{},b{};std::memcpy(a.data(),camera,0x70);std::memcpy(b.data(),copy.data(),0x70);
        std::string sa,sb;for(size_t i=0;i<28;++i){sa+=(i?",":"")+std::to_string(a[i]);sb+=(i?",":"")+std::to_string(b[i]);}
        logFile->event("vfx_culling_planes","\"game\":["+sa+"],\"widened\":["+sb+"],\"flag_70\":"+std::to_string(unsigned(copy[0x70])));}catch(...){}}
    ++w->applied;
    original(output,vfxView,copy.data());
}
inline void install(){
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva,call=baseAddress+callerRva-5;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
        throw std::runtime_error("VFX camera stage signature/caller mismatch");
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("VFX camera stage hook failed");
    enabled=true;
    logFile->event("vfx_culling_hook_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern));
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("vfx_culling_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
}
}
