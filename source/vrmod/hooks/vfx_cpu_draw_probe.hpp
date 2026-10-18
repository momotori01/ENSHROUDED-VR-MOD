// Diagnostic: which of the VFX draws recorded by CPU draw calls (188730 "vfx_cpu_drawcalls") shows an effect.
// f7c50 calls it for a View with a VfxView at 1011df (pass 0) and 101864 (pass 2), and its 'gbuffer' pass (f6830,
// called at 102060) at f7157 (pass 1); a private eye without a VfxView skips all three. EVR_VFX_CPU_SKIP in the
// game's environment is a mask of the calls to leave out for every View: 1 the first, 2 the second, 4 the one of
// the gbuffer - what then disappears from the VFX owner's eye is drawn there. No launch flag sets it.
// Mock runs game123827 (1) and game124130 (2): the aim circle of the pickaxe stays with either left out.
namespace vfxCpuDrawProbe {
using Record=void(__fastcall*)(void*,void*,void*,uint32_t,void*);
inline Record original=nullptr;inline bool enabled=false;inline unsigned skip=0;
inline std::array<std::atomic<unsigned>,3> calls{},skipped{};
constexpr uintptr_t rva=0x1af7e0;constexpr std::array<uintptr_t,3> callers{0x101284,0x101909,0xf71fc};
constexpr const char* pattern="48 8B C4 55 56 57 41 54 41 55 41 56 48 8D A8 A8 FC FF FF";
void __fastcall record(void* view,void* graph,void* parameters,uint32_t pass,void* extra){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const unsigned index=caller==baseAddress+callers[0]?0u:caller==baseAddress+callers[1]?1u:caller==baseAddress+callers[2]?2u:3u;
    if(index<3){
        if(calls[index].fetch_add(1)==0)try{logFile->event("vfx_cpu_draw_call","\"site\":"+std::to_string(index)+",\"pass\":"+std::to_string(pass)+",\"skipped\":"+((skip>>index)&1u?"true":"false"));}catch(...){}
        if((skip>>index)&1u){++skipped[index];return;}
    }
    original(view,graph,parameters,pass,extra);
}
// And for 170d60, the stage f7c50 names "vfx_model_culling" (fed4e, only for a View with a VfxView):
// EVR_VFX_MODEL_CULL_SKIP=1 leaves it out. Its results start as zeroes in f7c50, as for a View without a VfxView.
namespace modelCulling {
using Cull=void*(__fastcall*)(void*,void*,void*,void*);
inline Cull original=nullptr;inline bool enabled=false;inline std::atomic<unsigned> skipped{0};
constexpr uintptr_t rva=0x170e00,caller=0x1334ad;
constexpr const char* pattern="48 8B C4 48 89 50 10 55 56 41 55 41 56 48 8D A8 B8 FB FF FF";
void* __fastcall cull(void* a,void* b,void* c,void* d){
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+caller){++skipped;return nullptr;}
    return original(a,b,c,d);
}
inline void install(){
    char value[4]{};const auto n=GetEnvironmentVariableA("EVR_VFX_MODEL_CULL_SKIP",value,sizeof value);
    if(n!=1||value[0]!='1')return;
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva,site=baseAddress+caller-5;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(site)!=0xe8||
       uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=target)throw std::runtime_error("VFX model culling signature/caller mismatch");
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&cull),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("VFX model culling probe hook failed");
    enabled=true;logFile->event("vfx_model_culling_probe_enabled","\"diagnostic\":true,\"skipped\":true");
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("vfx_model_culling_probe_removed","\"status\":"+std::to_string(status)+",\"skipped\":"+std::to_string(skipped.load()));}catch(...){}
}
}
namespace writeback {inline void install();}
inline void install(){
    writeback::install();modelCulling::install();
    char value[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_CPU_SKIP",value,sizeof value);
    if(!n||n>=sizeof value||value[0]<'1'||value[0]>'7'||value[1])return;
    skip=unsigned(value[0]-'0');
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("VFX CPU draw signature mismatch");
    for(const auto after:callers){const auto site=baseAddress+after-5;
        if(read<uint8_t>(site)!=0xe8||uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=target)throw std::runtime_error("VFX CPU draw caller mismatch");}
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&record),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("VFX CPU draw probe hook failed");
    enabled=true;logFile->event("vfx_cpu_draw_probe_enabled","\"diagnostic\":true,\"skip_mask\":"+std::to_string(skip)+",\"rva\":"+hex(rva));
}
// The same question for two of the three writebacks of 188660 (f7c50 at fe25b, only for a View with a VfxView):
// EVR_VFX_WRITEBACK_SKIP is a mask - 1 leaves out 187d10 "vfx_write_decals_sort_data_back", 2 leaves out 188030
// "vfx_write_inject_instances_back", 4 leaves out 188320 "vfx_write_transparent_instances_back". That one is the
// source of vfx_transparent_bridge.hpp, whose strict checks end the session when it is missing (run game125735), so
// bit 4 only works without the bridge (launch with -NoVfxParticles).
// Mock runs game124932 (1) and game125233 (2): the aim circle stays without the decal and without the inject writeback.
// Mock run game124539 (EVR_VFX_CPU_SKIP=4): the aim circle of the pickaxe stays without any of the three CPU draws.
namespace writeback {
using Write=void(__fastcall*)(void*,void*,void*,void*);
struct Target {uintptr_t rva,caller;const char* pattern;const char* name;Write original=nullptr;bool enabled=false;std::atomic<unsigned> calls{0},skipped{0};};
inline std::array<Target,3> targets{{
    {0x1aeaf0,0x1887b0,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24","decals_sort_data"},
    {0x1880d0,0x18878b,"48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55","inject_instances"},
    {0x1883c0,0x188778,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57","transparent_instances"}}};
inline unsigned skip=0;
template<size_t I> void __fastcall write(void* graph,void* view,void* programs,void* parameters){
    auto& t=targets[I];
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+t.caller){
        if(t.calls.fetch_add(1)==0)try{logFile->event("vfx_writeback_call","\"stage\":"+jsonString(t.name)+",\"skipped\":"+((skip>>I)&1u?"true":"false"));}catch(...){}
        if((skip>>I)&1u){++t.skipped;return;}
    }
    t.original(graph,view,programs,parameters);
}
inline void install(){
    char value[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_WRITEBACK_SKIP",value,sizeof value);
    if(!n||n>=sizeof value||value[0]<'1'||value[0]>'7'||value[1])return;
    skip=unsigned(value[0]-'0');
    if((skip&4u)&&vfxTransparent::requested)throw std::runtime_error("EVR_VFX_WRITEBACK_SKIP bit 4 needs the transparent bridge off (-NoVfxParticles)");
    void* callbacks[]{reinterpret_cast<void*>(&write<0>),reinterpret_cast<void*>(&write<1>),reinterpret_cast<void*>(&write<2>)};
    for(size_t i=0;i<targets.size();++i){auto& t=targets[i];
        if(i==2&&!(skip&4u))continue;   // 188320 is the bridge's own hook otherwise
        const auto bytes=parsePattern(t.pattern);const auto target=baseAddress+t.rva,site=baseAddress+t.caller-5;
        if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(site)!=0xe8||
           uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=target)throw std::runtime_error("VFX writeback signature/caller mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(target),callbacks[i],reinterpret_cast<void**>(&t.original))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("VFX writeback probe hook failed");
        t.enabled=true;}
    logFile->event("vfx_writeback_probe_enabled","\"diagnostic\":true,\"skip_mask\":"+std::to_string(skip)+",\"transparent_bridge\":"+(vfxTransparent::requested?"true":"false"));
}
inline void disable() noexcept {
    for(auto& t:targets)if(t.enabled){
        const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+t.rva));if(status==MH_OK)t.enabled=false;
        try{logFile->event("vfx_writeback_probe_removed","\"stage\":"+jsonString(t.name)+",\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(t.calls.load())+",\"skipped\":"+std::to_string(t.skipped.load()));}catch(...){}
    }
}
}
inline void disable() noexcept {
    writeback::disable();modelCulling::disable();
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("vfx_cpu_draw_probe_removed","\"status\":"+std::to_string(status)+",\"calls\":["+std::to_string(calls[0].load())+","+std::to_string(calls[1].load())+","+std::to_string(calls[2].load())+
        "],\"skipped\":["+std::to_string(skipped[0].load())+","+std::to_string(skipped[1].load())+","+std::to_string(skipped[2].load())+"]");}catch(...){}
}
}
