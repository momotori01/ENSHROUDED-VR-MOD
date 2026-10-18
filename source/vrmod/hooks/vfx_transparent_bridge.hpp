// Same-frame transparent particle output. Simulation and source particle
// storage remain untouched. Rendering is a separate, explicit opt-in.
namespace vfxTransparent {
using Handle=std::array<uint8_t,16>;
using Writeback=void(__fastcall*)(void*,void*,void*,void*);
inline Writeback original=nullptr;
inline bool requested=false,enabled=false,failed=false;
inline bool render=false;
// Experiment (EVR_VFX_TRANSPARENT_BORROW=1 in the game's environment, no launch flag): the eye does not run its own
// writeback but draws the owner's transparent list as it is - the shared target buffer still holds it, and the eye's
// View takes the owner's counter for the rest of its frame (after f7c50 has reset the View's own counters, as with
// the lights). No shift between the eyes, so everything in it sits at the owner's place. For one question: are the
// effects that only the owner's eye shows (aim circle of the pickaxe, the blue area of building mode) instances that
// the VFX update itself appends to the owner's list, which the eye's writeback of the CPU emitters does not copy?
inline int borrowExperiment=-1;
// 1.0 (Hotfix 1.0.0.1, 2026-10-16): the writeback the eye ran again (June 0x188320) is inlined into the VFX stage
// (0x197f90) and cannot be called. The eye borrows the owner's list instead - the experiment above, now the only way:
// for its frame the eye's View takes the owner's transparent counter (View+0xb60, June +0x8d0; the stage builds the
// writeback's arguments from it, 0x130f31) and its three draw branches are taken. The resource getter (0x19bd10) takes
// the renderer's +0x16460 now, not the VfxView, so it needs no stand-in. The list sits where the owner's camera put it
// (the eyes' distance apart), as the experiment said.
constexpr bool borrowOnly=true;constexpr uintptr_t counterAt=0xb60;
inline std::atomic<unsigned> borrowed{0},skipped{0};
// A frame the borrowing does not fit (no owner's list, the counters equal) is left as the game's own - recorded, not failed.
inline void skip(const char* reason) noexcept {if(skipped.fetch_add(1)<8)try{logFile->event("vfx_transparent_borrow_skipped","\"reason\":"+jsonString(reason));}catch(...){}}
constexpr uintptr_t rva=0x1883c0,callerRva=0x188778;
constexpr const char* pattern="48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57";
struct Source {
    PairResources* pair=nullptr;uint64_t serial=0;unsigned calls=0;
    uintptr_t view=0,renderer=0,graphContext=0,counterPool=0;
    uintptr_t ownerView=0; // View that ran VFX this frame: the original View, or eye 0 on desktop-skip frames
    Handle target{},instances{},sort{};uint32_t counter=0,counterOffset=0,capacity=0;
    WorldPositionQ32 position{};
    std::array<uint8_t,64> programs{};
    std::array<uint8_t,0x128> parameters{};
    bool ready=false;
};
inline thread_local Source source;
inline thread_local bool observingOriginal=false;
inline void fail(const char* reason) noexcept;
struct Eye {
    PairResources* pair=nullptr;unsigned eye=0,writes=0,allocations=0,counterBindings=0,gates=0,graphicsGates=0,resourceGets=0,bindingGates=0,bindingResourceGets=0;
    Vec3 delta{};uint32_t counter=0,offset=0;bool writing=false,readyBefore=false;
    uintptr_t borrowedView=0;uint32_t ownCounter=0;   // experiment: the View whose counter is the owner's for this frame
    std::array<unsigned,3> reached{};                 // how often each branch was reached in this eye frame (taken or not)
};
// 1.0 (2026-10-16 evening): the owner's eye records three particle passes through the graphics branch 0x137e75 (RenderDoc
// game-20261016-192957: 6883 and 6930 cleared, 6967 loaded - the third one paints the flame of the held staff, 6974), the
// June build two. With the eye's branch taken at most twice the eye had the first two only, and no flame
// (EVR_VFX_GRAPHICS_GATES=2: as before).
inline const unsigned graphicsGateLimit=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_GRAPHICS_GATES",v,sizeof v);
    const auto k=n&&n<sizeof v?unsigned(std::strtoul(v,nullptr,10)):0u;return k>=1&&k<=8?k:3u;}();
inline thread_local Eye* current=nullptr;
struct EyeScope {
    Eye value;Eye* previous=current;
    // The VFX owner eye of a desktop-skip frame renders its own VFX natively; it is not bridged.
    bool owner=false;
    EyeScope(PairResources& pair,unsigned eye){value.pair=&pair;value.eye=eye;owner=worldSharedStages::vfxOwnerEye==int(eye);
        current=render&&!owner?&value:nullptr;
        if(render&&!owner)value.readyBefore=read<uint8_t>(pair.created[eye]+layout::viewReady)!=0;}
    ~EyeScope() noexcept {
        current=previous;if(!render||owner)return;
        if(value.borrowedView)std::memcpy(reinterpret_cast<void*>(value.borrowedView+counterAt),&value.ownCounter,4);
        try{
            if(borrowOnly){
                if(value.writes>1||value.gates>1||value.bindingGates>1||value.graphicsGates>graphicsGateLimit)skip("VFX transparent borrow count mismatch");
            }else{
            if(value.writes&&(value.writes!=1||value.allocations!=1||value.counterBindings!=1||value.gates!=value.resourceGets))
                fail("VFX transparent stage count mismatch");
            if(value.readyBefore&&read<uint8_t>(value.pair->created[value.eye]+layout::viewReady)&&
               (value.writes!=1||value.gates!=1||value.resourceGets!=1||value.bindingGates!=1||value.bindingResourceGets!=1||!value.graphicsGates||value.graphicsGates>2))fail("Ready eye omitted transparent VFX stage");
            }
            static std::atomic<unsigned> completions{0};const auto line=completions.fetch_add(1);
            if(line<8||line%600==0)logFile->event("vfx_transparent_eye_complete","\"serial\":"+std::to_string(value.pair->serial)+
                ",\"eye\":"+std::to_string(value.eye)+",\"writebacks\":"+std::to_string(value.writes)+
                ",\"allocations\":"+std::to_string(value.allocations)+",\"counter_bindings\":"+std::to_string(value.counterBindings)+
                ",\"draw_gates\":"+std::to_string(value.gates)+",\"resource_gets\":"+std::to_string(value.resourceGets)+
                ",\"graphics_gates\":"+std::to_string(value.graphicsGates)+",\"reached_draw_graphics_binding\":["+std::to_string(value.reached[0])+","+
                std::to_string(value.reached[1])+","+std::to_string(value.reached[2])+"]"+
                ",\"binding_gates\":"+std::to_string(value.bindingGates)+",\"binding_resource_gets\":"+std::to_string(value.bindingResourceGets)+
                ",\"private_vfx_view_still_null\":"+(read<uintptr_t>(value.pair->created[value.eye]+layout::viewVfx)?"false":"true"));
        }catch(...){fail("VFX eye completion observation failed");}
    }
};
using Allocate=void*(__fastcall*)(void*,void*,uint8_t,uint64_t,const void*);
using Descriptor=void*(__fastcall*)(void*,void*);
inline Allocate originalAllocate=nullptr;
inline Descriptor originalCounter=nullptr,originalResource=nullptr;
struct Hook{uintptr_t rva,caller;const char* pattern;bool enabled=false;};
constexpr uintptr_t bindingResourceCaller=0x141a8c;
inline std::array<Hook,6> hooks{{
    {0x4e5f0,0x188486,"4C 89 4C 24 20 53 48 81 EC A0 00 00 00"},
    {0xa9a10,0x1885b6,"8B 42 50 0F 10 42 28 C1 E0 02 89 41 24 48 8B C1 C6 01 08 0F"},
    {0x19bd50,0xf3e05,"40 53 48 83 EC 40 48 8B D9 48 81 C2 20 06 00 00 48 8D 4C 24 20 E8 ? ? ? ? C6 03 08"},
    // 1.0: the draw (View in rbx), "forward_transparent_vfx" (r13) and the bindings' branch (r13) on the VfxView
    {0x137eb5,0,"0F 84 ? ? ? ? 48 8B 7C 24 58 48 8D 8D B0 AB"},
    {0x127b2b,0,"0F 84 ? ? ? ? 48 8D 05 ? ? ? ? 33 D2 41"},
    {0x141a47,0,"0F 84 ? ? ? ? 0F 10 05 ? ? ? ? 48 8D 8D"}
}};
inline void fail(const char* reason) noexcept {
    failed=true;try{logFile->event("vfx_transparent_failure","\"reason\":"+jsonString(reason));}catch(...){}
}
inline uint16_t handleIndex(const Handle& h){uint16_t n{};std::memcpy(&n,h.data()+8,2);return n;}
inline void validateHandle(const PairResources& pair,const Handle& h,uint64_t minimum){
    uintptr_t graph{};std::memcpy(&graph,h.data(),8);const auto i=handleIndex(h);
    if(graph!=pair.graph||i>=read<uint64_t>(graph+0x118))throw std::runtime_error("VFX buffer graph/index mismatch");
    const auto object=read<uintptr_t>(read<uintptr_t>(graph+0x110)+uintptr_t(i)*8);
    if(!object)throw std::runtime_error("VFX buffer object missing");
    const auto descriptor=read<uintptr_t>(graph+0x128)+uintptr_t(i)*16;
    if(read<uint64_t>(descriptor)<minimum||read<uint64_t>(object+8)<minimum||!read<uintptr_t>(object+0x18))
        throw std::runtime_error("VFX buffer extent/Vulkan object mismatch");
}
inline uint32_t counterOffset(uintptr_t pool,uint32_t id){
    // 7b780 has no failure return. Validate its generation before reading the
    // exact same record; this observer never calls it with a stale handle.
    const auto index=id&0xffffu,generation=id>>16;
    const auto slots=read<uint64_t>(pool+0x50),records=read<uintptr_t>(pool+0x48);
    if(!id||!generation||!records||!slots||slots>65535||index>=slots)
        throw std::runtime_error("VFX counter pool bounds");
    const auto record=records+uintptr_t(index)*64;
    if(read<uint16_t>(record+0x38)!=generation-1)throw std::runtime_error("VFX counter generation mismatch");
    const auto offset=read<uint32_t>(record+0x10);
    if(offset%4||offset/4>=slots)throw std::runtime_error("VFX counter byte offset bounds");
    return offset;
}
struct FrameScope {
    bool previous=observingOriginal;
    FrameScope(PairResources* pair,uint64_t serial,void* parameters,bool use,uintptr_t ownerView){
        observingOriginal=requested&&use&&pair;
        if(!observingOriginal)return;
        source={};source.pair=pair;source.serial=serial;source.ownerView=ownerView;
        source.position=read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(parameters)+0x38);
    }
    ~FrameScope(){observingOriginal=previous;}
};
// Another writeback's payload, made longer the same way (the decals: vfx_decal_rebase.hpp). The caller arms it around
// its own call of the game's pass; `caller` is the return address of that pass's allocation. Where the allocation is
// not the expected one nothing is changed and `used` stays 0 - the pass then runs as the game's own.
struct ExtraPayload {uintptr_t caller=0;std::array<uint32_t,5> words{};unsigned used=0;};
inline thread_local ExtraPayload* extraPayload=nullptr;
void* __fastcall allocate(void* graphContext,void* output,uint8_t alignment,uint64_t size,const void* options){
    if(extraPayload&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+extraPayload->caller){
        auto& extra=*extraPayload;
        if(size!=4||alignment!=4||extra.used)return originalAllocate(graphContext,output,alignment,size,options);
        auto result=originalAllocate(graphContext,output,alignment,sizeof(extra.words),options);
        const auto address=reinterpret_cast<uintptr_t>(output),mapped=read<uintptr_t>(address+0x20);
        // The game writes the first word (the counter's byte offset) itself, right after this returns.
        if(mapped&&read<uint64_t>(address+0x18)==sizeof(extra.words)){std::memcpy(reinterpret_cast<void*>(mapped),extra.words.data(),sizeof(extra.words));++extra.used;}
        return result;
    }
    if(!current||!current->writing||reinterpret_cast<uintptr_t>(_ReturnAddress())!=baseAddress+hooks[0].caller)
        return originalAllocate(graphContext,output,alignment,size,options);
    auto& c=*current;
    if(size!=4||alignment!=4||c.allocations)throw std::runtime_error("VFX payload allocation contract changed");
    auto result=originalAllocate(graphContext,output,alignment,28,options);
    const auto address=reinterpret_cast<uintptr_t>(output),mapped=read<uintptr_t>(address+0x20);
    if(!mapped||read<uint64_t>(address+0x18)!=28)throw std::runtime_error("VFX extended allocation failed");
    std::array<uint32_t,7> payload{c.offset,0x45565254u,source.counterOffset,source.capacity,0,0,0};
    std::memcpy(payload.data()+4,&c.delta,sizeof(c.delta));
    std::memcpy(reinterpret_cast<void*>(mapped),payload.data(),sizeof(payload));++c.allocations;
    logFile->event("vfx_transparent_payload","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+
        ",\"bytes\":28,\"destination_counter_offset\":"+std::to_string(c.offset)+",\"source_counter_offset\":"+std::to_string(source.counterOffset)+
        ",\"capacity\":"+std::to_string(source.capacity)+",\"buffer_index\":"+std::to_string(handleIndex(read<Handle>(address)))+
        ",\"buffer_offset\":"+std::to_string(read<uint32_t>(address+0x10)));
    return result;
}
void* __fastcall getCounter(void* output,void* pool){
    if(!current||!current->writing||reinterpret_cast<uintptr_t>(_ReturnAddress())!=baseAddress+hooks[1].caller)
        return originalCounter(output,pool);
    auto& c=*current;
    if(reinterpret_cast<uintptr_t>(pool)!=source.counterPool||c.counterBindings)
        throw std::runtime_error("VFX counter binding scope mismatch");
    auto result=originalCounter(output,pool);const auto address=reinterpret_cast<uintptr_t>(output);
    auto expected=read<std::array<uint8_t,40>>(address);
    if(expected[0]!=8||read<uint64_t>(address+0x18)!=0||read<uint32_t>(address+0x20)!=1||
       read<uint32_t>(address+0x24)!=4*read<uint64_t>(source.counterPool+0x50))
        throw std::runtime_error("VFX counter descriptor layout changed");
    // Native 7b6b0 is the same pool's RW byte-address descriptor (tag 10).
    // This informs native resource transitions; changing SPIR-V alone is insufficient.
    reinterpret_cast<Descriptor>(baseAddress+0xa9a40)(output,pool);expected[0]=10;
    if(read<std::array<uint8_t,40>>(address)!=expected)throw std::runtime_error("VFX writable descriptor changed resource/range");
    ++c.counterBindings;return result;
}
void* __fastcall getResource(void* output,void* view){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool binding=caller==baseAddress+bindingResourceCaller;
    if(!current||(!binding&&caller!=baseAddress+hooks[2].caller))
        return originalResource(output,view);
    auto& c=*current;
    if(view||!source.ready||source.pair!=c.pair||source.serial!=c.pair->serial||
       (binding?(c.bindingGates!=1||c.bindingResourceGets!=0):(!c.gates||!c.writes||c.resourceGets!=0))||
       source.view!=read<uintptr_t>(c.pair->originalView+0x738))
        throw std::runtime_error("VFX draw resource getter scope mismatch");
    // This getter only reads renderer+9890 and creates a native descriptor.
    // No VfxView pointer is written to the eye; simulation remains absent.
    const auto handle=read<Handle>(source.renderer+0x9890);validateHandle(*c.pair,handle,1);
    auto result=originalResource(output,reinterpret_cast<void*>(source.view));
    const auto address=reinterpret_cast<uintptr_t>(output);
    if(read<uint8_t>(address)!=8||read<Handle>(address+8)!=handle||read<Handle>(source.renderer+0x9890)!=handle)
        throw std::runtime_error("VFX draw descriptor source changed");
    if(binding)++c.bindingResourceGets;else ++c.resourceGets;return result;
}
inline void onGate(uintptr_t rsp,unsigned gate) noexcept {
    if(!current||failed)return;const ObserverLastError preserve;auto& c=*current;
    if(borrowOnly)try{
        // 1.0: the View is rbx at the draw's branch, r13 at the other two (vfx_gate_stub.asm's saved registers).
        const auto view=read<uintptr_t>(rsp-(gate==0?40:112));const auto flags=read<uint64_t>(rsp-8);
        if(gate>2||view!=c.pair->created[c.eye]||read<uintptr_t>(view+layout::viewVfx)){skip("a draw branch of another View");return;}
        if(!(flags&0x40))return;                                      // not the branch past the draw: nothing to do
        ++c.reached[gate];
        // The bindings' branch comes before the borrowing (the lights' culling) in the eye's frame: it is taken when the
        // owner has a VfxView; the draws only with the owner's list borrowed. Each once (the forward pass twice at most).
        if(gate==2){const auto ownerView=worldSharedStages::vfxOwnerEye>=0?c.pair->created[size_t(worldSharedStages::vfxOwnerEye)]:c.pair->originalView;
            if(c.bindingGates||(worldSharedStages::vfxOwnerEye<0&&!read<uintptr_t>(ownerView+layout::viewVfx))||!read<uint32_t>(ownerView+counterAt))return;}
        else if(!c.borrowedView||(gate==1?c.graphicsGates>=graphicsGateLimit:c.gates))return;
        const auto changed=flags&~uint64_t(0x40);std::memcpy(reinterpret_cast<void*>(rsp-8),&changed,8);
        // Diagnostic (2026-10-16 evening): the particles of the borrowed list sit where the owner's camera put them (RenderDoc
        // game-20261016-193923: the same instance positions in both eyes), and the particle passes' own constant block
        // (2320 bytes, its camera-relative view-projection at c135) is the eye's - so the eye shows them shifted by the
        // eyes' distance. Where the View keeps that matrix: 4x4 floats whose w column is (0,0,near,0) and third row (0,0,0,near).
        if(gate==1&&!c.graphicsGates){static std::atomic<unsigned> searches{0};if(searches.fetch_add(1)<3){std::string found;unsigned n=0;
            for(uintptr_t o=0;o+64<=0x2000&&n<8;o+=4){const auto m=read<std::array<float,16>>(view+o);
                if(m[3]==0.f&&m[7]==0.f&&m[15]==0.f&&m[8]==0.f&&m[9]==0.f&&std::abs(m[10])<1e-6f&&m[11]>1e-4f&&m[11]<1.f&&std::abs(m[0])>0.1f&&std::abs(m[5])>0.1f){
                    ++n;found+=std::string(found.empty()?"":",")+"{\"at\":"+hex(o)+",\"m\":[";
                    for(size_t k=0;k<16;++k)found+=(k?",":"")+std::to_string(m[k]);found+="]}";}}
            logFile->event("vfx_particle_matrix_search","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+",\"found\":["+found+"]");}}
        if(gate==2)++c.bindingGates;else if(gate==1)++c.graphicsGates;else ++c.gates;
        static std::atomic<unsigned> lines{0};
        if(lines.fetch_add(1)<12)logFile->event("vfx_transparent_draw_gate","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+
            ",\"rva\":"+hex(hooks[3+gate].rva)+",\"borrowed_list\":true,\"only_saved_zf_changed\":true");
        return;
    }catch(const std::exception& e){fail(e.what());return;}
    try{
        const auto view=read<uintptr_t>(rsp-(gate?128:104));const auto flags=read<uint64_t>(rsp-8);
        if(gate>2||view!=c.pair->created[c.eye]||read<uintptr_t>(view+0x738)||!(flags&0x40)||
           !source.ready||source.pair!=c.pair||source.serial!=c.pair->serial||
           source.view!=read<uintptr_t>(c.pair->originalView+0x738))
            throw std::runtime_error("VFX transparent draw gate scope mismatch");
        if(gate==2){
            // Native 10aaf0 initializes common+48 before the eye writeback.
            if(c.bindingGates||read<uintptr_t>(rsp-120))throw std::runtime_error("VFX binding initialization scope mismatch");
        }else{
            if(c.writes!=1||c.bindingGates!=1||c.bindingResourceGets!=1||
               (!gate?c.gates!=0:(c.gates!=1||c.graphicsGates>=2)))
                throw std::runtime_error("VFX transparent stage ordering mismatch");
            if(gate==1){
                const auto parameters=read<uintptr_t>(rsp-112);
                const auto common=parameters?read<uintptr_t>(parameters+0x70):0;
                const auto globalSet=common?read<uintptr_t>(common+0x3ca0):0;
                const auto vfxSet=common?read<uintptr_t>(common+0x48):0;
                const auto eyeSet=parameters?read<uintptr_t>(parameters+0x80):0;
                logFile->event("vfx_transparent_graphics_bindings","\"serial\":"+std::to_string(c.pair->serial)+
                    ",\"eye\":"+std::to_string(c.eye)+",\"global_set\":"+hex(globalSet)+
                    ",\"vfx_set\":"+hex(vfxSet)+",\"eye_set\":"+hex(eyeSet));
                // Keep the native JE taken if a required descriptor is absent.
                if(!globalSet||!vfxSet||!eyeSet)throw std::runtime_error("VFX graphics descriptor missing");
            }
        }
        const auto changed=flags&~uint64_t(0x40);
        std::memcpy(reinterpret_cast<void*>(rsp-8),&changed,8);
        if(gate==2)++c.bindingGates;else if(gate==1)++c.graphicsGates;else ++c.gates;
        logFile->event("vfx_transparent_draw_gate","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+
            ",\"rva\":"+hex(hooks[3+gate].rva)+",\"only_saved_zf_changed\":true,\"view_pointer_unchanged\":true");
    }catch(const std::exception& e){fail(e.what());}
}
void __fastcall invoke(void* graphContext,void* view,void* programs,void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    original(graphContext,view,programs,parameters);
    const ObserverLastError preserve;
    if(!observingOriginal||caller!=baseAddress+callerRva)return;
    try{
        auto& s=source;auto& pair=*s.pair;++s.calls;
        if(s.calls!=1||GetCurrentThreadId()!=pair.renderThread)throw std::runtime_error("VFX original writeback scope/thread mismatch");
        s.view=reinterpret_cast<uintptr_t>(view);s.graphContext=reinterpret_cast<uintptr_t>(graphContext);
        s.renderer=read<uintptr_t>(s.view+0x58);s.counterPool=read<uintptr_t>(s.renderer+0x48);
        if(s.view!=read<uintptr_t>(pair.originalView+0x738)||!s.renderer||!s.counterPool)
            throw std::runtime_error("VFX source View ownership mismatch");
        s.programs=read<std::array<uint8_t,64>>(reinterpret_cast<uintptr_t>(programs));
        s.parameters=read<std::array<uint8_t,0x128>>(reinterpret_cast<uintptr_t>(parameters));
        const auto p=reinterpret_cast<uintptr_t>(s.parameters.data());
        s.target=read<Handle>(p+0x110);s.capacity=read<uint32_t>(p+0x120);s.counter=read<uint32_t>(p+0x124);
        s.instances=read<Handle>(s.renderer+0x1b8);s.sort=read<Handle>(s.renderer+0x1c8);
        const auto gameRenderer=read<uintptr_t>(pair.scene+8);
        if(!s.capacity||s.capacity>65536||s.capacity!=read<uint32_t>(gameRenderer+0x144b0)||
           s.target!=read<Handle>(gameRenderer+0x144a0)||!s.ownerView||s.counter!=read<uint32_t>(s.ownerView+0x8d0)||
           s.counterPool!=read<uintptr_t>(gameRenderer+0x90))
            throw std::runtime_error("VFX transparent target/counter ownership mismatch");
        validateHandle(pair,s.target,uint64_t(s.capacity)*180);
        validateHandle(pair,s.instances,uint64_t(s.capacity)*180);
        validateHandle(pair,s.sort,uint64_t(s.capacity)*16);
        if(s.target==s.instances)throw std::runtime_error("VFX source and target alias");
        s.counterOffset=counterOffset(s.counterPool,s.counter);s.ready=true;
        std::ostringstream out;out<<"\"serial\":"<<s.serial<<",\"calls\":"<<s.calls<<",\"vfx_view\":"<<hex(s.view)
            <<",\"vfx_renderer\":"<<hex(s.renderer)<<",\"graph_context\":"<<hex(s.graphContext)
            <<",\"counter_pool\":"<<hex(s.counterPool)<<",\"counter\":"<<s.counter<<",\"counter_offset\":"<<s.counterOffset
            <<",\"capacity\":"<<s.capacity<<",\"target_index\":"<<handleIndex(s.target)
            <<",\"source_index\":"<<handleIndex(s.instances)<<",\"sort_index\":"<<handleIndex(s.sort)
            <<",\"position_q32\":["<<s.position.x<<','<<s.position.y<<','<<s.position.z<<"],\"read_only\":true";
        logFile->event("vfx_transparent_source",out.str());
    }catch(const std::exception& e){fail(e.what());}
}
inline void observeEye(void* graphContext,PairResources& pair,unsigned eye) noexcept {
    if(!requested||failed||worldSharedStages::vfxOwnerEye==int(eye))return;
    const ObserverLastError preserve;
    if(borrowOnly){
        if(!render||!current||current->pair!=&pair||current->eye!=eye||current->writes)return;
        try{
            const auto view=pair.created[eye];
            const auto ownerView=worldSharedStages::vfxOwnerEye>=0?pair.created[size_t(worldSharedStages::vfxOwnerEye)]:pair.originalView;
            const auto own=read<uint32_t>(view+counterAt),owner=read<uint32_t>(ownerView+counterAt);
            // The owner eye of a desktop-skip frame has the VfxView only for its own frame: its counter is what counts.
            if(read<uintptr_t>(view+layout::viewVfx)||(worldSharedStages::vfxOwnerEye<0&&!read<uintptr_t>(ownerView+layout::viewVfx))||!own||!owner||own==owner)
                {const std::string why="the counters or VfxViews differ: own "+std::to_string(own)+" owner "+std::to_string(owner)+" eye_vfx "+hex(read<uintptr_t>(view+layout::viewVfx))+
                    " owner_vfx "+hex(read<uintptr_t>(ownerView+layout::viewVfx))+" owner_eye "+std::to_string(worldSharedStages::vfxOwnerEye)+" eye "+std::to_string(eye);
                 skip(why.c_str());return;}
            auto& c=*current;c.borrowedView=view;c.ownCounter=own;std::memcpy(reinterpret_cast<void*>(view+counterAt),&owner,4);
            c.allocations=c.counterBindings=1;++c.writes;
            const auto n=borrowed.fetch_add(1)+1;
            if(n<=4||n%1200==0)logFile->event("vfx_transparent_borrowed","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(eye)+
                ",\"count\":"+std::to_string(n)+",\"own_counter\":"+std::to_string(own)+",\"owner_counter\":"+std::to_string(owner)+",\"owner_view\":"+hex(ownerView));
        }catch(const std::exception& e){fail(e.what());}
        return;
    }
    try{
        auto& s=source;
        if(eye>1||s.pair!=&pair||s.serial!=pair.serial||!s.ready||s.calls!=1||GetCurrentThreadId()!=pair.renderThread)
            throw std::runtime_error("VFX eye lacks same-serial source writeback");
        const auto view=pair.created[eye];const auto counter=read<uint32_t>(view+0x8d0);
        const auto offset=counterOffset(s.counterPool,counter);
        if(read<uintptr_t>(view+0x738)||counter==s.counter||offset==s.counterOffset||
           s.instances!=read<Handle>(s.renderer+0x1b8)||s.sort!=read<Handle>(s.renderer+0x1c8)||
           s.target!=read<Handle>(read<uintptr_t>(pair.scene+8)+0x144a0)||
           s.counterOffset!=counterOffset(s.counterPool,s.counter))
            throw std::runtime_error("VFX source changed or eye resource alias");
        const auto position=read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(pair.parameters[eye].data())+0x38);
        constexpr double q=4294967296.;
        // Subtract in double to avoid signed integer overflow; nearby game Q32
        // origins fit its exact integer precision in the current coordinate range.
        const Vec3 delta{float((double(s.position.x)-double(position.x))/q),
                         float((double(s.position.y)-double(position.y))/q),
                         float((double(s.position.z)-double(position.z))/q)};
        if(!finite(delta))throw std::runtime_error("VFX nonfinite rebase delta");
        std::ostringstream out;out<<std::setprecision(9)<<"\"serial\":"<<s.serial<<",\"eye\":"<<eye
            <<",\"counter\":"<<counter<<",\"counter_offset\":"<<offset<<",\"source_counter_offset\":"<<s.counterOffset
            <<",\"graph_context\":"<<hex(reinterpret_cast<uintptr_t>(graphContext))
            <<",\"source_graph_context\":"<<hex(s.graphContext)<<",\"delta\":["<<delta.x<<','<<delta.y<<','<<delta.z
            <<"],\"position_q32\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"read_only\":true";
        logFile->event("vfx_transparent_eye_observed",out.str());
        if(render){
            if(!current||current->pair!=&pair||current->eye!=eye||current->writes)
                throw std::runtime_error("VFX extended writeback outside private eye scope");
            auto& c=*current;c.counter=counter;c.offset=offset;c.delta=delta;
            if(borrowExperiment<0){char value[4]{};const auto n=GetEnvironmentVariableA("EVR_VFX_TRANSPARENT_BORROW",value,sizeof value);borrowExperiment=n==1&&value[0]=='1';
                if(borrowExperiment)logFile->event("vfx_transparent_borrow_experiment","\"diagnostic\":true,\"own_writeback\":false,\"shift_between_eyes\":false");}
            if(borrowExperiment==1){
                c.borrowedView=view;c.ownCounter=counter;std::memcpy(reinterpret_cast<void*>(view+0x8d0),&s.counter,4);
                c.allocations=c.counterBindings=1;++c.writes;return;}
            auto parameters=s.parameters;std::memcpy(parameters.data()+0x124,&counter,4);
            struct WriteScope{Eye& c;WriteScope(Eye& e):c(e){c.writing=true;}~WriteScope(){c.writing=false;}} writing(c);
            original(graphContext,reinterpret_cast<void*>(s.view),s.programs.data(),parameters.data());
            if(c.allocations!=1||c.counterBindings!=1)throw std::runtime_error("VFX extended bindings were not created");
            ++c.writes;
        }
    }catch(const std::exception& e){fail(e.what());}
}
// 1.0 (2026-10-16 evening, user: "the flame was a little to the right; the light of the sword and of the trowel is off to
// the right as much"): the particles of the borrowed list sit relative to the owner's camera - one VFX simulation a frame
// (events 1841..3750 write them), and RenderDoc game-20261016-193923 shows the same instance positions in both eyes
// (0.28 0.03 0.68 m) - while the particle passes draw them with the eye's own camera-relative view-projection (c135 of
// their 2320-byte constants; their eye cameras 0.065 m apart). So the eye shows them where the owner's eye saw them.
// The 2320-byte constants of the particle passes are put together on the stack of the render function (rbp+0x3fd0) and
// copied to the GPU at 0x14025b..0x140268 (0x49a70 allocates 0x910 bytes, 0x13aa750 copies them). c135..c138 (rbp+0x4840)
// are the transpose of camera+0x520 (camera = r14, column-major; camera+0x620 when bit 21 of View+0x10 is set) -
// 0x140111..0x14019c. Here, just before the copy, the eye that borrows gets the shift (owner's camera - eye's camera,
// world metres: the matrix takes camera-relative world positions) added to the w of those four rows - the copy then
// carries it to the GPU, nothing of the game is changed.
// Earlier tries (2026-10-16, both drew the right eye's flame 50 px right of the staff's tip as before - Mock do / ds):
// 1) 0x124490's block+0x140 (a projection without the view; RenderDoc dp showed c135..c138 w still 0); 2) camera+0x520
// itself while 0x124490 ran (the log showed the right matrix shifted, but these constants are made outside that call).
// EVR_VFX_PARTICLE_SHIFT=0: as before.
extern "C" {inline void* evr_vfx_constants_trampoline=nullptr;void evr_vfx_constants_stub();}
constexpr uintptr_t constantsRva=0x14029b;
constexpr const char* constantsPattern="48 8D 95 D0 3F 00 00 41 B8 10 09 00 00 E8";
constexpr size_t constantsRows=0x4840,cameraMatrix=0x520,cameraMatrixFlagged=0x620;
inline bool constantsEnabled=false;
inline WorldPositionQ32 originalCamera{};   // the original View's camera of this frame (view_resource_probe.cpp): the owner on a refresh frame
inline const bool particleShift=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_PARTICLE_SHIFT",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<unsigned> shiftLines{0},shiftMisses{0},shiftFormMisses{0},shiftOtherViews{0};
// The stub's rsp is the game's rsp at the site; the saved registers below it (vfx_gate_stub.asm): rbp -48, r13 -112, r14 -120.
inline void onParticleConstants(uintptr_t rsp) noexcept {
    if(!particleShift||!current)return;const ObserverLastError preserve;
    try{
        auto& c=*current;const auto rbp=read<uintptr_t>(rsp-48),view=read<uintptr_t>(rsp-112),camera=read<uintptr_t>(rsp-120);
        if(c.eye<0||c.eye>1||view!=c.pair->created[size_t(c.eye)]){shiftOtherViews.fetch_add(1);return;}
        const auto owner=worldSharedStages::vfxOwnerEye;
        if(owner==c.eye)return;   // the eye that made the list: its own camera
        const auto eyePosition=read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(c.pair->parameters[size_t(c.eye)].data())+0x38);
        const auto ownerPosition=owner>=0&&owner<=1?read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(c.pair->parameters[size_t(owner)].data())+0x38):originalCamera;
        constexpr double q=4294967296.;
        const double dx=(double(ownerPosition.x)-double(eyePosition.x))/q,dy=(double(ownerPosition.y)-double(eyePosition.y))/q,dz=(double(ownerPosition.z)-double(eyePosition.z))/q;
        if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(dz)||dx*dx+dy*dy+dz*dz>2500.)
            throw std::runtime_error("particle shift out of range: owner "+std::to_string(owner)+" eye "+std::to_string(c.eye)+" shift "+std::to_string(dx)+" "+std::to_string(dy)+" "+std::to_string(dz));
        // The rows must be the camera's matrix transposed (the form this was read from); anything else is left alone.
        const auto rows=read<std::array<float,16>>(rbp+constantsRows);
        const auto source=read<std::array<float,16>>(camera+((read<uint32_t>(view+0x10)>>21)&1u?cameraMatrixFlagged:cameraMatrix));
        bool same=true;for(size_t r=0;r<4&&same;++r)for(size_t k=0;k<4&&same;++k)same=rows[r*4+k]==source[k*4+r];
        auto floats=[](const std::array<float,16>& m){std::string s;for(size_t i=0;i<16;i++)s+=(i?",":"")+std::to_string(m[i]);return s;};
        if(!same){if(shiftFormMisses.fetch_add(1)<4)logFile->event("vfx_particle_shift_miss","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+
            ",\"reason\":\"rows are not the camera's matrix\",\"rows\":["+floats(rows)+"],\"camera_matrix\":["+floats(source)+"]");return;}
        auto m=rows;for(size_t r=0;r<4;++r)m[r*4+3]+=float(m[r*4]*dx+m[r*4+1]*dy+m[r*4+2]*dz);
        std::memcpy(reinterpret_cast<void*>(rbp+constantsRows),m.data(),64);
        const auto n=shiftLines.fetch_add(1);
        if(n<4||n%1800==0)logFile->event("vfx_particle_shift","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+",\"owner_eye\":"+std::to_string(owner)+
            ",\"shift_m\":["+std::to_string(dx)+","+std::to_string(dy)+","+std::to_string(dz)+"],\"rows\":["+floats(rows)+"],\"shifted_w\":["+std::to_string(m[3])+","+std::to_string(m[7])+","+std::to_string(m[11])+","+std::to_string(m[15])+
            "],\"constants\":"+std::to_string(n+1)+",\"other_views\":"+std::to_string(shiftOtherViews.load()));
    }catch(const std::exception& e){if(shiftMisses.fetch_add(1)<4)try{logFile->event("vfx_particle_shift_miss","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void install(){
    if(borrowOnly){
        if(!render){logFile->event("vfx_transparent_observer_skipped","\"reason\":\"1.0: only the borrowing draw is ported\"");return;}
        if(particleShift){const auto p=parsePattern(constantsPattern);const auto site=baseAddress+constantsRva;
            if(scan(reinterpret_cast<const uint8_t*>(site),p.size(),p)!=std::vector<size_t>{0})logFile->event("vfx_particle_shift_skipped","\"reason\":\"signature\"");
            else if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&evr_vfx_constants_stub),&evr_vfx_constants_trampoline)!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
                logFile->event("vfx_particle_shift_skipped","\"reason\":\"hook\"");
            else{constantsEnabled=true;logFile->event("vfx_particle_shift_enabled","\"rva\":"+hex(constantsRva));}}
        for(unsigned i=3;i<hooks.size();i++){
            auto& h=hooks[i];const auto p=parsePattern(h.pattern);const auto site=baseAddress+h.rva;
            if(scan(reinterpret_cast<const uint8_t*>(site),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("VFX rendering hook signature changed");
            void* callbacks[]{reinterpret_cast<void*>(&evr_vfx_gate_stub),reinterpret_cast<void*>(&evr_vfx_draw_stub),reinterpret_cast<void*>(&evr_vfx_bindings_stub)};
            void** trampolines[]{&evr_vfx_gate_trampoline,&evr_vfx_draw_trampoline,&evr_vfx_bindings_trampoline};
            if(MH_CreateHook(reinterpret_cast<void*>(site),callbacks[i-3],trampolines[i-3])!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
                throw std::runtime_error("VFX rendering hook failed");
            h.enabled=true;logFile->event("vfx_transparent_render_hook_enabled","\"rva\":"+hex(h.rva)+",\"pattern\":"+jsonString(h.pattern)+",\"borrowing\":true");
        }
        return;
    }
    if(render){
        using Status=uint32_t(*)();const auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        const auto status=module?reinterpret_cast<Status>(GetProcAddress(module,"evrVfxTransparentShaderStatus")):nullptr;
        if(!status||!status())throw std::runtime_error("VFX transparent shader not prepared at process launch");
        const auto rw=parsePattern("8B 42 50 0F 10 42 28 C1 E0 02 89 41 24 48 8B C1 C6 01 0A");
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+0xa9a40),rw.size(),rw)!=std::vector<size_t>{0})
            throw std::runtime_error("VFX writable pool descriptor signature changed");
        const auto bindingCall=baseAddress+bindingResourceCaller-5;
        if(read<uint8_t>(bindingCall)!=0xe8||uintptr_t(int64_t(bindingCall+5)+read<int32_t>(bindingCall+1))!=baseAddress+hooks[2].rva)
            throw std::runtime_error("VFX binding resource getter caller changed");
        for(unsigned i=0;i<hooks.size();i++){
            auto& h=hooks[i];const auto p=parsePattern(h.pattern);const auto site=baseAddress+h.rva;
            if(scan(reinterpret_cast<const uint8_t*>(site),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("VFX rendering hook signature changed");
            if(h.caller){const auto call=baseAddress+h.caller-5;
                if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)
                    throw std::runtime_error("VFX rendering hook caller changed");}
            void* callbacks[]{reinterpret_cast<void*>(&allocate),reinterpret_cast<void*>(&getCounter),reinterpret_cast<void*>(&getResource),reinterpret_cast<void*>(&evr_vfx_gate_stub),reinterpret_cast<void*>(&evr_vfx_draw_stub),reinterpret_cast<void*>(&evr_vfx_bindings_stub)};
            void** trampolines[]{reinterpret_cast<void**>(&originalAllocate),reinterpret_cast<void**>(&originalCounter),reinterpret_cast<void**>(&originalResource),&evr_vfx_gate_trampoline,&evr_vfx_draw_trampoline,&evr_vfx_bindings_trampoline};
            if(MH_CreateHook(reinterpret_cast<void*>(site),callbacks[i],trampolines[i])!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
                throw std::runtime_error("VFX rendering hook failed");
            h.enabled=true;logFile->event("vfx_transparent_render_hook_enabled","\"rva\":"+hex(h.rva)+",\"caller\":"+hex(h.caller)+",\"pattern\":"+jsonString(h.pattern));
        }
    }
    const auto bytes=parsePattern(pattern);const auto matches=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes);
    const auto call=baseAddress+callerRva-5;
    if(matches!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)
        throw std::runtime_error("VFX writeback signature/caller changed");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("VFX writeback observer hook failed");
    enabled=true;logFile->event("vfx_transparent_observer_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern));
}
inline void disable() noexcept {
    if(constantsEnabled){const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+constantsRva));if(status==MH_OK)constantsEnabled=false;
        try{logFile->event("vfx_particle_shift_removed","\"status\":"+std::to_string(status)+",\"shifted\":"+std::to_string(shiftLines.load())+",\"other_views\":"+std::to_string(shiftOtherViews.load()));}catch(...){}}
    for(auto it=hooks.rbegin();it!=hooks.rend();++it)if(it->enabled){
        const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+it->rva));if(status==MH_OK)it->enabled=false;
        try{logFile->event("vfx_transparent_render_hook_removed","\"rva\":"+hex(it->rva)+",\"status\":"+std::to_string(status));}catch(...){}
    }
    if(!enabled)return;const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("vfx_transparent_observer_removed","\"status\":"+std::to_string(status)+",\"failed\":"+(failed?"true":"false"));}catch(...){}
}
}
