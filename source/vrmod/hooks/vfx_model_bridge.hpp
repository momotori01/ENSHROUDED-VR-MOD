// VFX models in the eye that does not own the VFX (opt-in).
// The aim circle of the pickaxe and the blue area of building mode showed in the VFX owner's eye only (headset run
// of 2026-10-11). They are models that the VFX puts into a View: leaving the stage f7c50 names "vfx_model_culling"
// (170d60) out takes the circle away from the owner's eye too (Mock run game131039), while the decals, the inject
// and transparent writebacks and each single CPU draw recording do not (vfx_cpu_draw_probe.hpp).
// For a View, f7c50 runs these steps, every one only when the View has a VfxView (View+738):
//   fcf3f  "vfx_model_culling_prepare": 179a80 fills the buffers of the View's model instances (f7c50's rbp+a900,
//          which is rbp+a870 +90: the fields 10aaf0 reads at +f0 .. +120)
//   10aaf0 (called at fd121): two bindings of those buffers, 80 and 44 bytes a record (11100e, 111170)
//   feb74  "vfx_model_culling": 170d60 culls the VfxView's models with the View's camera
//   188730 "vfx_cpu_drawcalls": the draws, recorded in the visibility prepass (101144, pass 0), the shadow maps
//          (1017c9, pass 2) and - in f6830, the gbuffer - at f70be (pass 1)
// Culling alone in the eye (Mock run game131556) showed nothing: the draws were still left out. Here the private
// eye takes all seven branches: the saved zero flag of each is cleared, and where the VfxView is an argument - null
// for the eye - it is the Scene's VfxView. No VfxView pointer is written to the eye; the simulation is not run again;
// the models are culled and placed with the eye's own camera.
// 1.0 (Hotfix 1.0.0.1, 2026-10-16; the stage names are gone from the exe, matched by the June order of the View's render
// 0x129e80 and the uses of View+0x9a8): prepare 0x19f7e0 (gate 0x1300f1, View in rax), bindings 0x13d7d0 (called at
// 0x1303c0; gates 0x143743 / 0x143b4b, View in r13), culling 0x197f90 (gate 0x1321b3 - now a short je after
// "mov rdx,[View+0x9a8]; test rdx,rdx", the VfxView stays in rdx up to the call at 0x13220a), draws 0x1af7a0 (arguments
// as June's 188730: pass 0 at 0x13493b gate 0x13489f, pass 2 at 0x134f5d gate 0x134ec3, both View in rbx; pass 1 in the
// gbuffer 0x128b30 at 0x129564, gate 0x1293f6 after "mov rcx,[r13+0x9a8]; test rcx,rcx", View in r13). Until this was
// matched the bridge was skipped ("signature mismatch: culling") and the right eye had no VFX models (headset run of
// 2026-10-16: the light of the staff in the left eye only).
// The eighth branch (2026-10-16, Hotfix 1.0.0.1): the VFX writeback 0x1aed30 (June 188660: the effects' lights, decals and
// the fog's instances written into the View's own lists, from the block the render builds at rbp+0x2ec0 with this View's
// descriptors, counters and camera), behind "mov rcx,[View+0x9a8]; test rcx,rcx; je" at 0x131801 (View in rax), called at
// 0x131811. Borrowing the owner's lists (shared_point_lights.hpp) no longer reached the eye's lighting in 1.0: the deferred
// lighting bound the eye's own, empty light list (RenderDoc game-20261016-160355 and -162332, light buffer 309 / 437, decals
// 310 / 438) - the room lit by the staff in the owner's eye, dark in the other (user: "only inside my own buildings"). The
// idea: the eye writes its own lists, as the owner does, from the owner's VfxView. Opt-in only (EVR_EYE_VFX_WRITEBACK=1,
// see eyeWriteback below: it crashed).
namespace vfxModelBridge {
using Prepare=void*(__fastcall*)(void*,void*,void*,void*);
using Cull=void*(__fastcall*)(void*,void*,void*,void*);
using Record=void(__fastcall*)(void*,void*,void*,uint32_t,void*);
using Writeback=void(__fastcall*)(void*,void*,void*,void*);
inline Prepare originalPrepare=nullptr;inline Cull originalCull=nullptr;inline Record originalRecord=nullptr;inline Writeback originalWriteback=nullptr;
// Opt-in (EVR_EYE_VFX_WRITEBACK=1): the first trial crashed in the eye's frame (Mock game-20261016-163556, dump
// enshrouded_20261016_073734: 0xa9ae9 reading null+0x10 - 0x1aed30 at 0x1aedf9 takes [VFX renderer+0x158], which is empty
// by the time the eye is drawn; the owner's VFX stage holds it for its own frame only).
inline const bool eyeWriteback=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_VFX_WRITEBACK",v,sizeof v);return n==1&&v[0]=='1';}();
inline bool requested=false,installed=false;
inline std::atomic<unsigned> failures{0},scopes{0};
// The branches. viewAt: where vfx_gate_stub.asm saved the register that holds the View, below the stack pointer
// of the hooked instruction (rax: 16, rbx: 40, r13: 112; June r12: 104, r15: 128, rsi: 56). needs: the step that has
// to have run in this eye.
enum Step:unsigned {Culling,PrepareStep,BindInstances,BindMaterials,DrawPrepass,DrawShadow,DrawGbuffer,WritebackStep,StepCount};
struct Gate {uintptr_t rva;const char* pattern;unsigned viewAt;const char* name;bool ownFrame;bool enabled=false;};
inline std::array<Gate,StepCount> gates{{
    {0x1322f3,"74 5A 48 8B 85 C8 B6 02 00",16,"culling",true},
    {0x130231,"0F 84 03 01 00 00 48 8D 8D 50 74 00 00",16,"prepare",true},
    {0x143883,"0F 84 E6 03 00 00 44 8B 0D",112,"bind_instances",false},
    {0x143c8b,"0F 84 C3 01 00 00 44 8B 0D",112,"bind_materials",false},
    {0x1349df,"0F 84 9B 00 00 00 48 8B 54 24 58",40,"draw_prepass",true},
    {0x135003,"0F 84 99 00 00 00 49 8B D5 48 8D 8D E0 DD 00 00",40,"draw_shadow",true},
    {0x129526,"0F 84 6D 01 00 00 41 0F 10 86 68 6D 01 00",112,"draw_gbuffer",false},
    {0x131941,"74 13 4C 8D 85 C0 2E 00 00",16,"writeback",true}}};
struct Call {uintptr_t rva;const char* pattern;bool enabled=false;};
inline std::array<Call,4> calls{{
    {0x19f970,"4C 8B DC 49 89 5B 08 49 89 6B 10 49 89 73 18 49 89 7B 20 41 56 48 81 EC C0 00 00 00"},
    {0x198110,"4C 8B DC 55 57 41 54 41 57 49 8D AB 48 FC FF FF 48 81 EC 98 04 00 00"},
    {0x1af930,"4C 8B DC 55 56 41 54 41 55 41 56 41 57 49 8D AB 98 FC FF FF 48 81 EC 38"},
    {0x1aeec0,"48 8B C4 55 53 56 41 56 48 8D A8 F8 FE FF FF 48 81 EC E8 01 00 00"}}};
constexpr uintptr_t prepareCaller=0x13033a,cullCaller=0x13234f,writebackCaller=0x131956;constexpr std::array<uintptr_t,3> recordCallers{0x134a80,0x1350a2,0x129699};
struct Eye {PairResources* pair=nullptr;int eye=-1;std::array<unsigned,StepCount> taken{};unsigned prepares=0,culls=0,records=0,writebacks=0,held=0;uintptr_t vfx=0;};
inline thread_local Eye* current=nullptr;
inline void fail(const char* reason) noexcept {
    if(failures.fetch_add(1)==0)try{logFile->event("vfx_model_bridge_failure","\"reason\":"+jsonString(reason));}catch(...){}
}
struct EyeScope {
    Eye value;Eye* previous=current;
    EyeScope(PairResources& pair,unsigned eye){
        value.pair=&pair;value.eye=int(eye);
        current=requested&&installed&&!failures.load()&&worldSharedStages::vfxOwnerEye!=int(eye)?&value:nullptr;}
    ~EyeScope() noexcept {
        const bool active=current==&value;current=previous;if(!active)return;
        const auto& t=value.taken;
        if(t[PrepareStep]!=value.prepares||t[Culling]!=value.culls||t[DrawPrepass]+t[DrawShadow]+t[DrawGbuffer]!=value.records||t[WritebackStep]!=value.writebacks)fail("an eye took a VFX model branch without its call");
        const auto n=scopes.fetch_add(1);
        if(n<4||n%2000==0)try{std::string taken;for(unsigned i=0;i<StepCount;++i)taken+=std::string(i?",":"")+std::to_string(t[i]);
            logFile->event("vfx_model_bridge_eye","\"serial\":"+std::to_string(value.pair->serial)+",\"eye\":"+std::to_string(value.eye)+",\"branches\":["+taken+
            "],\"prepares\":"+std::to_string(value.prepares)+",\"culls\":"+std::to_string(value.culls)+",\"draw_records\":"+std::to_string(value.records)+",\"writebacks\":"+std::to_string(value.writebacks)+
            ",\"left_native\":"+std::to_string(value.held)+",\"vfx_view\":"+hex(value.vfx));}catch(...){}
    }
    EyeScope(const EyeScope&)=delete;EyeScope& operator=(const EyeScope&)=delete;
};
// rsp: the stack pointer at the hooked branch (vfx_gate_stub.asm); the saved flags lie at rsp-8.
inline void onGate(uintptr_t rsp,unsigned step) noexcept {
    if(!current||step>=StepCount)return;const ObserverLastError preserve;auto& c=*current;
    try{
        const auto flags=read<uint64_t>(rsp-8);
        if(!(flags&0x40))return;                                   // a View with its own VfxView: the game's own way
        auto& pair=*c.pair;const auto& g=gates[step];const auto view=read<uintptr_t>(rsp-g.viewAt);
        if(view!=pair.created[c.eye]){if(g.ownFrame)throw std::runtime_error("VFX model branch of another View inside the eye's frame");++c.held;return;}
        // 1.0: the VfxView lives inside the VFX scene (layout::vfxSceneView; before, the scene pointed at it from +0xe0).
        const auto vfx=read<uintptr_t>(pair.originalView+layout::viewVfx),vfxScene=read<uintptr_t>(pair.scene+layout::sceneVfx);
        if(read<uintptr_t>(view+layout::viewVfx)||GetCurrentThreadId()!=pair.renderThread||!vfx||!vfxScene||vfxScene+layout::vfxSceneView!=vfx)
            throw std::runtime_error("VFX model branch outside the private eye");
        // Each step once, and only on what the steps before it have built in this eye.
        const bool ready=step==PrepareStep?true:step==Culling||step==BindInstances||step==BindMaterials||step==WritebackStep?c.prepares==1:c.culls==1;
        if(c.taken[step]||!ready){++c.held;return;}
        const auto changed=flags&~uint64_t(0x40);std::memcpy(reinterpret_cast<void*>(rsp-8),&changed,8);
        c.vfx=vfx;++c.taken[step];
    }catch(const std::exception& e){fail(e.what());}
}
// 179a80 is already hooked for the widened VFX planes of the owner (stable_frustum_bridge.hpp vfxCulling); that hook
// asks here for the eye's VfxView. Only without it does this bridge hook the stage itself.
inline void* prepareView(){
    if(!current)return nullptr;auto& c=*current;
    if(c.taken[PrepareStep]==1&&!c.prepares&&c.vfx){++c.prepares;return reinterpret_cast<void*>(c.vfx);}
    fail("VFX model prepare reached without the branch of this eye");return nullptr;
}
void* __fastcall prepare(void* output,void* vfxView,void* parameters,void* fourth){
    if(!vfxView&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+prepareCaller){
        vfxView=prepareView();if(!vfxView)return nullptr;
    }
    return originalPrepare(output,vfxView,parameters,fourth);
}
// The VfxView right after each eye's culling, for two frames (2026-10-17: which of its fields differ between the owner's own
// culling and the other eye's bridged one - the owner eye drew none of the models after its lighting). EVR_VFX_CULL_DUMP=1.
inline const bool cullDump=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_CULL_DUMP",v,sizeof v);return n==1&&v[0]=='1';}();
inline std::atomic<unsigned> cullDumps{0};inline std::atomic<uint64_t> cullCalls{0};
inline void dumpCull(void* vfxView,void* parameters) noexcept {
    if(!cullDump||!vfxView)return;const auto n=++cullCalls;if(n<1200||cullDumps.load()>=4)return;
    try{const auto* lights=sharedPointLights::current;const int eye=lights&&lights->eye>=0?lights->eye:-1;if(eye<0)return;
        if(cullDumps.fetch_add(1)>=4)return;
        const auto bytes=read<std::array<uint8_t,0x800>>(reinterpret_cast<uintptr_t>(vfxView));std::string s;static const char* d="0123456789abcdef";
        for(auto b:bytes){s+=d[b>>4];s+=d[b&15];}
        logFile->event("vfx_model_cull_dump","\"eye\":"+std::to_string(eye)+",\"serial\":"+std::to_string(lights->pair?lights->pair->serial:0)+",\"vfx_view\":"+hex(reinterpret_cast<uintptr_t>(vfxView))+",\"bytes\":\""+s+"\"");
    }catch(...){}
}
void* __fastcall cull(void* graph,void* vfxView,void* parameters,void* fourth){
    if(current&&!vfxView&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+cullCaller){
        auto& c=*current;
        if(c.taken[Culling]==1&&!c.culls&&c.vfx){++c.culls;auto* r=originalCull(graph,reinterpret_cast<void*>(c.vfx),parameters,fourth);dumpCull(reinterpret_cast<void*>(c.vfx),parameters);return r;}
        fail("VFX model culling reached without the branch of this eye");return nullptr;   // as for a View without a VfxView: nothing culled
    }
    auto* r=originalCull(graph,vfxView,parameters,fourth);dumpCull(vfxView,parameters);return r;
}
// Census of the draw records (2026-10-17, headset reports 4, 15 and 19: the stars, the tool's area and the building's blue
// area in one eye only; RenderDoc game-20261017-034858: the eye that owns the VFX recorded none of the VFX models' draws
// after the lighting - the emissive draw and the area's volume draw - the other eye recorded both): per eye (the owner's
// native calls included) and pass, how often 0x1af7e0 is called and with a VfxView, on record every 600 frames.
inline std::array<std::array<std::atomic<unsigned>,8>,3> recordCensus{};inline std::array<std::array<std::atomic<unsigned>,8>,3> recordCensusView{};
inline std::atomic<unsigned> recordCensusLines{0};
inline void censusRecord(uint32_t pass,bool view,uintptr_t caller) noexcept {
    const auto* lights=sharedPointLights::current;const int eye=lights&&lights->eye>=0?lights->eye:-1;const size_t e=eye<0?2:size_t(eye>1?2:eye);const size_t p=pass<7?pass:7;
    ++recordCensus[e][p];if(view)++recordCensusView[e][p];
    static std::atomic<uint64_t> calls{0};const auto n=++calls;
    if(n%2400==0&&recordCensusLines.fetch_add(1)<12)try{std::string s;
        // [calls, with a VfxView] pairs (2026-10-17: was "calls/with" unquoted, which broke the readers of the whole log)
        for(size_t i=0;i<3;++i){s+=std::string(i?",":"")+"[";for(size_t k=0;k<8;++k)s+=std::string(k?",":"")+"["+std::to_string(recordCensus[i][k].load())+","+std::to_string(recordCensusView[i][k].load())+"]";s+="]";}
        logFile->event("vfx_model_record_census","\"calls_by_eye_and_pass\":["+s+"],\"rows\":\"eye 0, eye 1, original View; [calls, with a VfxView]\",\"last_caller\":"+hex(caller-baseAddress));}catch(...){}
}
void __fastcall record(void* vfxView,void* graph,void* parameters,uint32_t pass,void* extra){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    censusRecord(pass,vfxView!=nullptr||(current&&current->vfx),caller);
    if(current&&!vfxView&&(caller==baseAddress+recordCallers[0]||caller==baseAddress+recordCallers[1]||caller==baseAddress+recordCallers[2])){
        auto& c=*current;
        if(c.culls==1&&c.vfx&&c.records<3){++c.records;
            // The CPU-built instances (fishing line, grappling rope) of this eye: vfx_spring_rebase.hpp.
            const vfxSpringRebase::EyeDraw springDraw(c.vfx,pass,c.pair->serial,c.eye);
            originalRecord(reinterpret_cast<void*>(c.vfx),graph,parameters,pass,extra);return;}
        fail("VFX model draw reached without the culling of this eye");return;
    }
    originalRecord(vfxView,graph,parameters,pass,extra);
}
// The eye's own lists of the effects (the eighth branch): 0x1aed30 with the owner's VfxView, into this eye's lists.
void __fastcall writeback(void* vfxView,void* graph,void* block,void* fourth){
    if(current&&!vfxView&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+writebackCaller){
        auto& c=*current;
        if(c.taken[WritebackStep]==1&&!c.writebacks&&c.vfx){++c.writebacks;originalWriteback(reinterpret_cast<void*>(c.vfx),graph,block,fourth);return;}
        fail("VFX writeback reached without the branch of this eye");return;
    }
    originalWriteback(vfxView,graph,block,fourth);
}
// For shared_point_lights.hpp: this eye has written its own lights, decals and fog instances this frame, so it culls
// and lights them as its own instead of borrowing the owner's.
inline bool wroteLists(){return current&&current->writebacks==1;}
inline bool writesLists(){return current!=nullptr;}   // set for this eye's frame; the writeback comes later in it
// For culling_history_bridge.hpp: this eye frame makes its own generated submodels, so it starts from the game's reset.
inline bool regenerates(){return current!=nullptr;}
inline void install(void* const* stubs,void** trampolines){
    if(!requested)return;
    // Without the eye's writeback (EVR_EYE_VFX_WRITEBACK=0) its branch and call are left as the game's.
    const auto used=[](size_t gate){return gate!=WritebackStep||eyeWriteback;};
    for(size_t i=0;i<gates.size();++i){if(!used(i))continue;const auto& g=gates[i];const auto bytes=parsePattern(g.pattern);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+g.rva),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error(std::string("VFX model branch signature mismatch: ")+g.name);}
    const bool sharedPrepare=vfxCulling::enabled;   // then 179a80 already begins with that hook's jump
    const size_t callCount=eyeWriteback?calls.size():calls.size()-1;
    for(size_t i=sharedPrepare?1:0;i<callCount;++i){const auto bytes=parsePattern(calls[i].pattern);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+calls[i].rva),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("VFX model call signature mismatch");}
    auto calledFrom=[](uintptr_t after,uintptr_t target){const auto site=baseAddress+after-5;return read<uint8_t>(site)==0xe8&&uintptr_t(int64_t(site+5)+read<int32_t>(site+1))==baseAddress+target;};
    if(!calledFrom(prepareCaller,calls[0].rva)||!calledFrom(cullCaller,calls[1].rva)||!calledFrom(recordCallers[0],calls[2].rva)||
       !calledFrom(recordCallers[1],calls[2].rva)||!calledFrom(recordCallers[2],calls[2].rva)||(eyeWriteback&&!calledFrom(writebackCaller,calls[3].rva)))
        throw std::runtime_error("VFX model caller mismatch");
    void* callbacks[]{reinterpret_cast<void*>(&prepare),reinterpret_cast<void*>(&cull),reinterpret_cast<void*>(&record),reinterpret_cast<void*>(&writeback)};
    void** originals[]{reinterpret_cast<void**>(&originalPrepare),reinterpret_cast<void**>(&originalCull),reinterpret_cast<void**>(&originalRecord),reinterpret_cast<void**>(&originalWriteback)};
    if(sharedPrepare)vfxCulling::eyeVfxView=&prepareView;
    for(size_t i=sharedPrepare?1:0;i<callCount;++i){
        if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+calls[i].rva),callbacks[i],originals[i])!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(baseAddress+calls[i].rva))!=MH_OK)throw std::runtime_error("VFX model call hook failed");
        calls[i].enabled=true;}
    for(size_t i=0;i<gates.size();++i){if(!used(i))continue;
        if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+gates[i].rva),stubs[i],&trampolines[i])!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(baseAddress+gates[i].rva))!=MH_OK)throw std::runtime_error(std::string("VFX model branch hook failed: ")+gates[i].name);
        gates[i].enabled=true;}
    installed=true;cullingHistory::eyeRegeneratesModels=&regenerates;
    if(eyeWriteback){sharedPointLights::eyeWroteLists=&wroteLists;sharedPointLights::eyeWritesLists=&writesLists;}
    logFile->event("vfx_model_bridge_enabled","\"branches\":"+std::to_string(eyeWriteback?8:7)+",\"calls\":[\"0x19f970\",\"0x198110\",\"0x1af930\""+std::string(eyeWriteback?",\"0x1aeec0\"":"")+
        "],\"eye_writes_its_own_vfx_lists\":"+(eyeWriteback?"true":"false")+",\"prepare_through_vfx_culling_hook\":"+std::string(sharedPrepare?"true":"false")+",\"only_saved_zf_and_one_argument_changed\":true");
}
inline void disable() noexcept {
    installed=false;vfxCulling::eyeVfxView=nullptr;cullingHistory::eyeRegeneratesModels=nullptr;sharedPointLights::eyeWroteLists=nullptr;sharedPointLights::eyeWritesLists=nullptr;
    for(auto it=gates.rbegin();it!=gates.rend();++it)if(it->enabled){const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+it->rva));if(status==MH_OK)it->enabled=false;}
    bool any=false;
    for(auto& k:calls)if(k.enabled){any=true;const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+k.rva));if(status==MH_OK)k.enabled=false;}
    if(any)try{logFile->event("vfx_model_bridge_removed","\"eye_frames\":"+std::to_string(scopes.load())+",\"failures\":"+std::to_string(failures.load()));}catch(...){}
}
}
