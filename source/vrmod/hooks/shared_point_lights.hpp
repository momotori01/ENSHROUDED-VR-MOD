// Opt-in causal experiment: consume the original view's same-frame GPU lights.
// No VFX simulation is repeated. Eye cluster/GI resources remain independent.
// This does not solve lights culled outside the original VFX camera frustum.
namespace sharedPointLights {
using Handle=std::array<uint8_t,16>;
using Cluster=void(__fastcall*)(void*,void*);
inline Cluster original=nullptr;
inline bool requested=false,enabled=false;
// The decals of the effects (the aim circle of the pickaxe or the watering can on the ground) reach a View the same
// way as the point lights: a buffer of the View (View+928, ForwardDecalData of 112 bytes, capacity View+80) and a
// counter (View+8e4), filled by 187d10 "vfx_write_decals_sort_data_back" - which f7c50 only runs for a View that owns
// a VfxView (188660 behind the test at fe240). The private eye never gets it, so its list stayed empty and the circle
// showed in the VFX owner's eye only (headset run of 2026-10-11, Mock game203702). Opt-in: the eye borrows the
// owner's list for its clustered culling and shading, exactly as it borrows the lights.
inline bool decalsRequested=false;inline std::atomic<unsigned> decalFailures{0},decalStates{0};
// The passes that draw the decals take their buffer earlier than the clustered culling: f7c50 calls 10aaf0 (the
// bindings of the shading passes) at fd121, and that builds the decal descriptor twice through eeb20 (10beb9,
// 10da0a) from View+928 - the eye's own, empty buffer. Borrowing at the culling alone therefore showed nothing in the
// eye (Mock run game120208: the lists were borrowed, the circle stayed out). So the descriptor itself is pointed at
// the owner's buffer where 10aaf0 builds it. The counter is left alone there: f7c50 resets the View's own counters
// after 10aaf0 (fd41d), and only the culling reads a counter.
// Result (Mock runs game120845 / game121245): both borrowings run on every frame of the eye without a failure, and
// the aim circle of the pickaxe still shows in the owner's eye only. So that circle is not one of these decals. The
// candidates left are the VFX meshes drawn by CPU draw calls: 10aaf0 builds their two instance bindings (80 and 44
// bytes a record) only for a View with a VfxView (111007, 111169), and f7c50 records their draws through 188730
// (1011d7 pass 0, 10185c pass 2) under the same test. Their transforms are camera-relative, so an eye would need its
// own shifted copy. This borrowing stays opt-in until an effect is found that it does bring into the other eye.
using DecalDescriptor=void*(__fastcall*)(void*,void*);
inline DecalDescriptor originalDecalDescriptor=nullptr;inline bool decalDescriptorEnabled=false;
constexpr uintptr_t decalDescriptorRva=0x11fc70;constexpr std::array<uintptr_t,2> decalDescriptorCallers{0x12ddc9,0x12ddc9};   // 1.0: built once, by the render itself before the shading bindings
constexpr const char* decalDescriptorPattern="48 83 EC 58 48 83 BA 80 05 00 00 00 74 41";
// The point lights have the same trap (2026-10-14, user: "the brightness of the walls differs between the eyes here
// and there - that is what feels wrong"; Mock self-test game-20261014-184902: a warm light on the logs in one eye
// only, the brighter eye changing from place to place). f7c50 hands View+918 to four users through eec60: 10aaf0
// (called at fd121) for the shading passes at 10d96f, the clustered culling at fe32b, f7210 (1023b9) and 104e32. The
// borrowing below swaps the View's buffer at the culling only, so the shading bindings, made before it, kept the
// eye's own buffer: the clusters held indices into the owner's list and the shading read them from another list -
// lights on the wrong walls, lights missing. Now the private eye's View holds the owner's buffer while 10aaf0 runs
// as well (called at fd121 only; the counter is not read there), so all four read one list.
// Not by a hook on eec60 itself: f7c50 calls eeb20 right after eec60 (fe32b, fe359) without loading rdx again - the
// game counts on that small function leaving rdx alone, and a hook does not (Mock run game-20261014-190519: the
// decal descriptor of the culling took code bytes, ACCESS_VIOLATION at f8e9b8). 10aaf0 is large; its caller loads
// its registers again after it.
using ShadingBindings=uint64_t(__fastcall*)(void*,void*,void*,void*,void*,void*,uint64_t,void*,uint64_t);
inline ShadingBindings originalShading=nullptr;inline bool shadingEnabled=false;
inline std::atomic<unsigned> lightDescriptorFailures{0},lightDescriptorStates{0};
constexpr uintptr_t shadingRva=0x13d810,shadingCaller=0x130405;
constexpr const char* shadingPattern="4C 89 44 24 18 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57";   // 1.0: 8 arguments (9 before); the 9-argument detour passes them on
constexpr uintptr_t rva=0x65200,callerRva=0x131a9d;
constexpr const char* pattern="48 8B C4 48 89 58 08 55 56 57 41 54 41 55 41 56 41 57";
// 1.0 (2026-10-16): the View's lists moved (lights 0x918 -> 0x570, decals 0x928 -> 0x580, fog instances 0x938 -> 0x590; a
// fourth list at 0x5a0), their capacities (0x7c / 0x80 / 0x84 -> 0xc4 / 0xc8 / 0xcc) and counter ids (0x8e0 / 0x8e4 / 0x8e8 ->
// 0xb6c / 0xb70 / 0xb74: the reset 107570 against d3bf0). The clustered culling 65170 (called at 1319f8) takes the descriptors at
// parameters+0x10 / +0x38 / +0x60 (0x18 / 0x40 / 0x68), the counters at +0xe0 / +0xe8 / +0xe4 (0x90 / 0x94 / 0x98) and the
// capacities at +0xf0 / +0xf4 / +0xf8 (0x9c / 0xa0 / 0xa4); parameters+8 is &View[0x6b0] (it was View[0xa30]). The many-lights
// shadow object View+0xb48 (0x8c8: 13d770 against 10aaf0; in 1.0 +0xb48 is the water's - see shareShadows), the CPU lights' count View+0xfa0 (0xa10).
// 1.0 (the arguments against the View, Mock game-20261016-104021): the View's four lists +0x570 lights, +0x580 decals,
// +0x590 a second list of 112-byte records, +0x5a0 the fog's VFX instances (June +0x938), their counters +0xb6c.. and
// capacities +0xc4.. in the same order; the culling's arguments take the counters as +0xe0 light, +0xe8 decal, +0xe4 the
// second list, +0xec fog, and the capacities in order from +0xf0.
namespace viewList {constexpr uintptr_t lights=0x570,decals=0x580,fog=0x5a0,lightCapacity=0xc4,decalCapacity=0xc8,fogCapacity=0xd0,
    lightCounter=0xb6c,decalCounter=0xb70,fogCounter=0xb78,second=0x590,secondCounter=0xb74,secondCapacity=0xcc,shadows=0xb48,cpuLights=0xfa0,parameterAnchor=0x6b0,
    anchorLights=0xc28,anchorDecals=0xca0;}   // the block at +0x6b0 keeps its own copies of the two buffers (see Context::anchor)
// 1.0 (Mock game-20261016-103430, the arguments as they are): five 40-byte descriptors from +0x10 - lights (52 bytes a
// record, June 64), decals (112), a second list of 112-byte records, the fog's VFX instances (32) and 256 records of 16.
constexpr uint32_t lightStride=52;
namespace cullArgument {constexpr uintptr_t second=0x60,secondCounter=0xe4,secondCapacity=0xf8,lights=0x10,decals=0x38,fog=0x88,lightCounter=0xe0,decalCounter=0xe8,fogCounter=0xec,
    lightCapacity=0xf0,decalCapacity=0xf4,fogCapacity=0xfc;}
struct Context {
    PairResources* pair=nullptr;int eye=-1;unsigned calls=0;
    uintptr_t view=0,sourceView=0;Handle owned{},source{};uint32_t ownedCounter=0,sourceCounter=0,capacity=0;
    bool borrowed=false,failed=false,parametersRestored=true;
    Handle ownedDecals{},sourceDecals{};uint32_t ownedDecalCounter=0,sourceDecalCounter=0,decalCapacity=0;bool decalsBorrowed=false;
    Handle ownedFog{},sourceFog{};uint32_t ownedFogCounter=0,sourceFogCounter=0,fogCapacity=0;bool fogBorrowed=false;   // the fog's VFX instances
    // 1.0: the second list of 112-byte records (View+0x590), borrowed like the lights (EVR_SHARE_SECOND_LIST=0: not).
    Handle ownedSecond{},sourceSecond{};uint32_t ownedSecondCounter=0,sourceSecondCounter=0;bool secondBorrowed=false;
    Handle bindingDecals{};unsigned decalDescriptors=0;   // the owner's buffer as handed to the shading passes, and how often
    Handle bindingLights{};unsigned lightDescriptors=0;   // the same for the point lights (shadingBindings below)
    // The eye's own list (vfx_decal_rebase.hpp): the shading passes were left with the eye's own buffer, which the
    // eye's writeback fills before the culling; only the counter is the owner's then.
    bool decalsOwnBuffer=false,decalsRebased=false;
    // 1.0 (2026-10-16): the eye's deferred lighting does not read View+0x570 / +0x580 but the copies of the two buffers in
    // the View's block at +0x6b0 (the culling's anchor, parameters+8): View+0xc28 and +0xca0, found by their values
    // (Mock game-20261016-164752, shared_light_handle_copies; the owner's View has them at the same places). With only
    // +0x570 borrowed, the eye lit with its own, empty list (RenderDoc game-20261016-162332: light buffer 309 in the owner's
    // eye, 437 in the other), so the room lit by the staff showed in one eye only (user, 2026-10-16: "only inside my own
    // buildings; outside it was normal"). From the shading bindings to the end of the eye's frame the copies are the owner's.
    uintptr_t anchorView=0;Handle anchorOwnLights{},anchorOwnDecals{};bool anchorLights=false,anchorDecals=false;
    // The same for the second list (View+0x590, 112-byte records): the eye's grass (the detail scatter) read its own,
    // empty copy against clusters made from the owner's list (RenderDoc game-20261016-173800: the scatter 12125 binds
    // buffer 437 with no records, the owner's 8566 binds 311 with 14) - grass inside the player's buildings in the
    // non-owner eye only. Its place is found by its value (shadingBindings).
    uintptr_t anchorSecondAt=0;Handle anchorOwnSecond{};bool anchorSecond=false;
    // The transparent effects' counter number (View+0xb60) is kept a second time in the View, and the particles' draw
    // arguments are made from that copy (dd, game-20261016-191851: +0xb60 lent from the bindings on, the eye's draws
    // still <0, 0>). Found by its value like the second list (shadingBindings).
    uintptr_t anchorCounterAt=0;uint32_t anchorOwnCounter=0;bool anchorCounter=false;
    // The render copies six counter numbers of the effects, View+0xb50..+0xb64 (0x130f01..0x130f71), into the block of
    // the particles' draws - one for each of the six [1,1,1] dispatches that make their arguments (RenderDoc
    // game-20261016-191223, 6517..6633 / 9882..9998). The bridge lends only +0xb60; the flame of the staff counts in
    // another one. The other five are lent here from the bindings to the end of the eye's frame.
    std::array<uint32_t,6> ownVfxCounters{};unsigned lentVfxCounters=0;uintptr_t vfxCounterView=0;
};
namespace vfxCounters {constexpr std::array<uintptr_t,6> at{0xb50,0xb54,0xb58,0xb5c,0xb60,0xb64};constexpr size_t bridged=4;}
inline thread_local Context* current=nullptr;
inline void fail(Context& c,const char* reason) noexcept {
    c.failed=true;
    try{logFile->event("shared_point_lights_failure","\"serial\":"+std::to_string(c.pair->serial)+
        ",\"eye\":"+std::to_string(c.eye)+",\"reason\":"+jsonString(reason));}catch(...){}
}
inline void validateHandle(const PairResources& pair,const Handle& h){
    const auto p=reinterpret_cast<uintptr_t>(h.data());
    const auto index=read<uint16_t>(p+8);
    // 1.0: the graph's buffer objects +0x268, their count +0x270 (+0x110 / +0x118; culling_history_bridge.hpp).
    if(read<uintptr_t>(p)!=pair.graph||index>=read<uint64_t>(pair.graph+0x270)||
       !read<uintptr_t>(read<uintptr_t>(pair.graph+0x268)+uintptr_t(index)*8))
        throw std::runtime_error("point light buffer outside the active graph");
}
// Diagnostic (EVR_LIGHT_LEND_LATE=1, 2026-10-16): the eye's deferred lighting bound the eye's own light list although the
// View held the owner's from the bindings to the end of the eye's render call. If the passes read the View's lists only
// when the graph is recorded, after that call, the View has to keep the owner's buffers until then: here it keeps the
// owner's light and decal buffers until the eye's next render call begins (the counters go back as before), and the record
// says whether the game had put a buffer of its own there by then.
inline const bool lendLate=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_LIGHT_LEND_LATE",v,sizeof v);return n==1&&v[0]=='1';}();
struct LentLate {uintptr_t view=0;Handle ownLights{},sourceLights{},ownDecals{},sourceDecals{};bool lights=false,decals=false;};
inline std::array<LentLate,2> lentLate{};inline std::atomic<uint64_t> lateRestored{0},lateReplaced{0};
struct Scope {
    Context& c;Context* previous=current;
    explicit Scope(Context& value):c(value){current=requested?&c:nullptr;
        if(requested&&lendLate&&c.eye>=0&&c.eye<=1&&lentLate[size_t(c.eye)].view)try{
            auto& l=lentLate[size_t(c.eye)];bool replaced=false;
            if(l.lights){if(read<Handle>(l.view+viewList::lights)==l.sourceLights)std::memcpy(reinterpret_cast<void*>(l.view+viewList::lights),l.ownLights.data(),16);else replaced=true;}
            if(l.decals){if(read<Handle>(l.view+viewList::decals)==l.sourceDecals)std::memcpy(reinterpret_cast<void*>(l.view+viewList::decals),l.ownDecals.data(),16);else replaced=true;}
            const auto n=replaced?++lateReplaced:++lateRestored;
            if(n<=3||n%1800==0)logFile->event("shared_point_lights_lend_late","\"eye\":"+std::to_string(c.eye)+",\"game_put_its_own_buffer\":"+(replaced?"true":"false")+
                ",\"restored\":"+std::to_string(lateRestored.load())+",\"replaced\":"+std::to_string(lateReplaced.load()));
            l={};
        }catch(...){}}
    ~Scope() noexcept {
        current=previous;
        if(!requested)return;
        // Restore ownership before leaving f7c50's caller, including exceptions.
        // These addresses belong to the retained private View, not game entities.
        if(c.anchorLights)std::memcpy(reinterpret_cast<void*>(c.anchorView+viewList::anchorLights),c.anchorOwnLights.data(),16);
        if(c.anchorDecals)std::memcpy(reinterpret_cast<void*>(c.anchorView+viewList::anchorDecals),c.anchorOwnDecals.data(),16);
        if(c.anchorSecond)std::memcpy(reinterpret_cast<void*>(c.anchorView+c.anchorSecondAt),c.anchorOwnSecond.data(),16);
        if(c.anchorCounter)std::memcpy(reinterpret_cast<void*>(c.anchorView+c.anchorCounterAt),&c.anchorOwnCounter,4);
        if(c.vfxCounterView)for(size_t i=0;i<vfxCounters::at.size();++i)if(c.lentVfxCounters&(1u<<i))
            std::memcpy(reinterpret_cast<void*>(c.vfxCounterView+vfxCounters::at[i]),&c.ownVfxCounters[i],4);
        const bool late=lendLate&&c.eye>=0&&c.eye<=1;
        if(late&&(c.borrowed||c.decalsBorrowed)){auto& l=lentLate[size_t(c.eye)];l.view=c.view;
            if(c.borrowed){l.lights=true;l.ownLights=c.owned;l.sourceLights=c.source;}
            if(c.decalsBorrowed){l.decals=true;l.ownDecals=c.ownedDecals;l.sourceDecals=c.sourceDecals;}}
        if(c.borrowed){
            if(!late)std::memcpy(reinterpret_cast<void*>(c.view+viewList::lights),c.owned.data(),16);
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::lightCounter),&c.ownedCounter,4);
        }
        if(c.decalsBorrowed){
            if(!late)std::memcpy(reinterpret_cast<void*>(c.view+viewList::decals),c.ownedDecals.data(),16);
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::decalCounter),&c.ownedDecalCounter,4);
        }
        if(c.fogBorrowed){
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::fog),c.ownedFog.data(),16);
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::fogCounter),&c.ownedFogCounter,4);
        }
        if(c.secondBorrowed){
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::second),c.ownedSecond.data(),16);
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::secondCounter),&c.ownedSecondCounter,4);
        }
        try{
            const bool restored=(!c.borrowed||((late||read<Handle>(c.view+viewList::lights)==c.owned)&&read<uint32_t>(c.view+viewList::lightCounter)==c.ownedCounter))&&
                (!c.decalsBorrowed||((late||read<Handle>(c.view+viewList::decals)==c.ownedDecals)&&read<uint32_t>(c.view+viewList::decalCounter)==c.ownedDecalCounter))&&
                (!c.fogBorrowed||(read<Handle>(c.view+viewList::fog)==c.ownedFog&&read<uint32_t>(c.view+viewList::fogCounter)==c.ownedFogCounter));
            const bool sourceUnchanged=(!c.borrowed||(read<Handle>(c.sourceView+viewList::lights)==c.source&&
                read<uint32_t>(c.sourceView+viewList::lightCounter)==c.sourceCounter))&&
                (!c.decalsBorrowed||(read<Handle>(c.sourceView+viewList::decals)==c.sourceDecals&&read<uint32_t>(c.sourceView+viewList::decalCounter)==c.sourceDecalCounter))&&
                (!c.fogBorrowed||(read<Handle>(c.sourceView+viewList::fog)==c.sourceFog&&read<uint32_t>(c.sourceView+viewList::fogCounter)==c.sourceFogCounter));
            if(!restored||!sourceUnchanged||!c.parametersRestored)fail(c,"light resource restoration failed");
            static std::atomic<unsigned> scopeLines{0};const auto line=scopeLines.fetch_add(1);
            if(line<8||line%600==0||!restored||!sourceUnchanged||!c.parametersRestored)logFile->event("shared_point_lights_scope","\"serial\":"+std::to_string(c.pair->serial)+
                ",\"eye\":"+std::to_string(c.eye)+",\"calls\":"+std::to_string(c.calls)+
                ",\"borrowed\":"+(c.borrowed?"true":"false")+",\"view_restored\":"+(restored?"true":"false")+
                ",\"source_unchanged\":"+(sourceUnchanged?"true":"false")+
                ",\"parameters_restored\":"+(c.parametersRestored?"true":"false")+
                ",\"failed\":"+(c.failed?"true":"false")+
                ",\"owned_counter\":"+std::to_string(c.ownedCounter)+",\"source_counter\":"+std::to_string(c.sourceCounter)+
                ",\"capacity\":"+std::to_string(c.capacity)+",\"light_descriptors\":"+std::to_string(c.lightDescriptors)+",\"decals_borrowed\":"+(c.decalsBorrowed?"true":"false")+",\"decal_descriptors\":"+std::to_string(c.decalDescriptors)+
                ",\"fog_instances_borrowed\":"+(c.fogBorrowed?"true":"false")+",\"second_list_borrowed\":"+(c.secondBorrowed?"true":"false"));
        }catch(...){fail(c,"light scope verification unreadable");}
    }
};
inline void decalFailure(const Context& c,const char* reason) noexcept {
    if(decalFailures.fetch_add(1)==0)try{logFile->event("shared_decals_failure","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+",\"reason\":"+jsonString(reason));}catch(...){}
}
void* __fastcall decalDescriptor(void* output,void* view){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto result=originalDecalDescriptor(output,view);
    if(!current||!decalsRequested||(caller!=baseAddress+decalDescriptorCallers[0]&&caller!=baseAddress+decalDescriptorCallers[1]))return result;
    auto& c=*current;
    if(c.eye<0||c.eye>1||c.eye==worldSharedStages::vfxOwnerEye)return result;
    try{
        auto& pair=*c.pair;const auto eyeView=pair.created[c.eye];
        const auto sourceView=worldSharedStages::vfxOwnerEye>=0?pair.created[size_t(worldSharedStages::vfxOwnerEye)]:pair.originalView;
        if(GetCurrentThreadId()!=pair.renderThread||reinterpret_cast<uintptr_t>(view)!=eyeView||!eyeView||!sourceView||eyeView==sourceView||read<uintptr_t>(eyeView+layout::viewVfx))
            throw std::runtime_error("decal descriptor outside the private eye");
        if(!read<uint64_t>(eyeView+viewList::decals)||!read<uint64_t>(sourceView+viewList::decals))return result;   // no buffer: nothing to borrow
        if(vfxDecals::sourceFor(pair)){c.decalsOwnBuffer=true;return result;}                   // the eye's own list will be written
        const auto own=read<Handle>(eyeView+viewList::decals),source=read<Handle>(sourceView+viewList::decals);validateHandle(pair,source);
        const auto address=reinterpret_cast<uintptr_t>(output);
        // tag +0, handle +8, byte offset +18, stride +20 (112), count +24 (the capacity)
        if(read<uint8_t>(address)!=9||read<Handle>(address+8)!=own||read<uint64_t>(address+0x18)!=0||read<uint32_t>(address+0x20)!=112||
           read<uint32_t>(address+0x24)!=read<uint32_t>(sourceView+viewList::decalCapacity)||own==source)
            throw std::runtime_error("decal descriptor layout or capacity differs");
        std::memcpy(reinterpret_cast<void*>(address+8),source.data(),16);c.bindingDecals=source;++c.decalDescriptors;
    }catch(const std::exception& error){decalFailure(c,error.what());}
    return result;
}
inline std::atomic<unsigned> fogFailures{0};
// EVR_EYE_LIST_FIX=0: neither the shading bindings nor the fog's instances take the owner's list (the state before
// 2026-10-14 evening), for comparisons only.
inline const bool eyeListFix=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_LIST_FIX",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline void fogFailure(const Context& c,const char* reason) noexcept {
    if(fogFailures.fetch_add(1)==0)try{logFile->event("shared_fog_instances_failure","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+",\"reason\":"+jsonString(reason));}catch(...){}
}
inline void lightDescriptorFailure(const Context& c,const char* reason) noexcept {
    if(lightDescriptorFailures.fetch_add(1)==0)try{logFile->event("shared_light_binding_failure","\"serial\":"+std::to_string(c.pair->serial)+",\"eye\":"+std::to_string(c.eye)+",\"reason\":"+jsonString(reason));}catch(...){}
}
inline const bool shareSecond=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SHARE_SECOND_LIST",v,sizeof v);return !(n==1&&v[0]=='0');}();
// 1.0 (Hotfix 1.0.0.1, 2026-10-16): the eye writes its own lists of the effects (vfx_model_bridge.hpp, the eighth branch:
// the VFX writeback 0x1aed30 with the owner's VfxView). The borrowing below never reached the eye's deferred lighting in
// 1.0 - it bound the eye's own, empty light list (RenderDoc game-20261016-160355 / -162332: light buffer 309 in the owner's
// eye, 437 in the other) - so the room lit by the staff showed in the owner's eye only. With its own lists the eye
// neither swaps its View at the bindings nor borrows at the culling. eyeWritesLists: the bridge is set for this eye's
// frame (the writeback comes after the bindings); eyeWroteLists: the writeback has run.
inline bool (*eyeWritesLists)()=nullptr;inline bool (*eyeWroteLists)()=nullptr;
inline std::atomic<uint64_t> ownListFrames{0};
uint64_t __fastcall shadingBindings(void* a,void* view,void* b,void* c4,void* e,void* f,uint64_t g,void* h,uint64_t i){
    struct Put {uintptr_t view=0;Handle own{};uintptr_t shadowView=0,ownShadows=0;uintptr_t secondView=0;Handle ownSecond{};
        ~Put() noexcept {if(view)std::memcpy(reinterpret_cast<void*>(view+viewList::lights),own.data(),16);if(shadowView)*reinterpret_cast<uintptr_t*>(shadowView+viewList::shadows)=ownShadows;
            if(secondView)std::memcpy(reinterpret_cast<void*>(secondView+viewList::second),ownSecond.data(),16);}} put;
    if(eyeListFix&&current&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+shadingCaller&&!(eyeWritesLists&&eyeWritesLists())){
        auto& c=*current;
        if(c.eye>=0&&c.eye<=1&&c.eye!=worldSharedStages::vfxOwnerEye)try{
            auto& pair=*c.pair;const auto eyeView=pair.created[c.eye];
            const auto sourceView=worldSharedStages::vfxOwnerEye>=0?pair.created[size_t(worldSharedStages::vfxOwnerEye)]:pair.originalView;
            if(GetCurrentThreadId()!=pair.renderThread||reinterpret_cast<uintptr_t>(view)!=eyeView||!eyeView||!sourceView||eyeView==sourceView||
               read<uintptr_t>(eyeView+8)!=pair.scene||read<uintptr_t>(sourceView+8)!=pair.scene||read<uintptr_t>(eyeView+layout::viewVfx)||!worldSharedStages::once)
                throw std::runtime_error("shading bindings outside the private eye");
            const auto shared=read<uintptr_t>(pair.scene+0xa88);
            for(const auto& proof:worldSharedStages::completed)
                if(!proof.valid||proof.object!=shared||proof.serial!=pair.serial)throw std::runtime_error("original frame updates not proven");
            // 1.0 (2026-10-16 evening): the transparent effects' counter (vfx_transparent_bridge.hpp, View+0xb60) from here on,
            // not only from the culling: the render builds the arguments of the particles' draws from it after the bindings
            // and before the culling (RenderDoc game-20261016-191223: the eye's 9882 read counter offset 0x2754 - its own,
            // empty one - the owner's 6517 0x27a4; the eye's draws 10223.. got <0, 0>) - the flame of the held staff, the
            // glowing motes and the beam over the tower in the owner's eye only (bug report 20261016-145228). The counters
            // were reset before the bindings (lendLists notes above); the bridge's scope puts the eye's own back at its end.
            // EVR_VFX_COUNTER_EARLY=0: at the culling only, as before.
            {static const bool early=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_COUNTER_EARLY",v,sizeof v);return !(n==1&&v[0]=='0');}();
             // Its copy (Context::anchorCounter): the place where the eye keeps its own counter number and the owner its
             // own, found by the values before the bridge lends +0xb60 (EVR_ANCHOR_COUNTER=0: left alone).
             static const bool anchorCounterSwap=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ANCHOR_COUNTER",v,sizeof v);return !(n==1&&v[0]=='0');}();
             constexpr uintptr_t vfxCounterAt=0xb60;
             const auto ownCounter=read<uint32_t>(eyeView+vfxCounterAt),ownerCounter=read<uint32_t>(sourceView+vfxCounterAt);
             if(anchorCounterSwap&&early&&!c.anchorCounter&&ownCounter&&ownerCounter&&ownCounter!=ownerCounter){
                 static std::atomic<uintptr_t> counterAt{0};static std::atomic<unsigned> searches{0};
                 if(!counterAt.load()&&searches.load()<64){
                     const auto n=++searches;std::string all,both;uintptr_t found=0;
                     for(uintptr_t o=0x400;o+4<=0x2000;o+=4)if(o!=vfxCounterAt&&read<uint32_t>(eyeView+o)==ownCounter){
                         all+=(all.empty()?"":",")+hex(o);if(read<uint32_t>(sourceView+o)==ownerCounter){both+=(both.empty()?"":",")+hex(o);if(!found)found=o;}}
                     if(found)counterAt=found;
                     if(found||n==1||n==64)logFile->event("shared_vfx_counter_copies","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                         ",\"search\":"+std::to_string(n)+",\"own\":"+std::to_string(ownCounter)+",\"owner\":"+std::to_string(ownerCounter)+
                         ",\"own_at\":["+all+"],\"owner_has_its_own_at\":["+both+"],\"lent_at\":"+(found?hex(found):std::string("null")));
                 }
                 const auto at=counterAt.load();
                 if(at&&read<uint32_t>(eyeView+at)==ownCounter&&read<uint32_t>(sourceView+at)==ownerCounter){
                     c.anchorView=eyeView;c.anchorCounterAt=at;c.anchorOwnCounter=ownCounter;c.anchorCounter=true;
                     std::memcpy(reinterpret_cast<void*>(eyeView+at),&ownerCounter,4);}
             }
             // The other five counters of the block (Context::ownVfxCounters; EVR_VFX_COUNTERS_ALL=0: only +0xb60, as before).
             static const bool allCounters=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_COUNTERS_ALL",v,sizeof v);return !(n==1&&v[0]=='0');}();
             if(early&&allCounters&&!c.vfxCounterView){std::string values;
                 for(size_t i=0;i<vfxCounters::at.size();++i){
                     const auto own=read<uint32_t>(eyeView+vfxCounters::at[i]),owner=read<uint32_t>(sourceView+vfxCounters::at[i]);
                     values+=std::string(i?",":"")+"["+std::to_string(own)+","+std::to_string(owner)+"]";
                     if(i==vfxCounters::bridged||!own||!owner||own==owner)continue;
                     c.ownVfxCounters[i]=own;c.lentVfxCounters|=1u<<i;std::memcpy(reinterpret_cast<void*>(eyeView+vfxCounters::at[i]),&owner,4);}
                 if(c.lentVfxCounters)c.vfxCounterView=eyeView;
                 static std::atomic<unsigned> told{0};
                 if(told.fetch_add(1)<4)logFile->event("shared_vfx_counters","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                     ",\"own_owner_b50_to_b64\":["+values+"],\"lent_mask\":"+std::to_string(c.lentVfxCounters)+",\"b60_by_the_bridge\":true");}
             if(early)vfxTransparent::observeEye(nullptr,pair,unsigned(c.eye));}
            // Diagnostic (2026-10-16): where else in the eye's View its own light and decal buffers are kept - the deferred
            // lighting bound the eye's own list although View+0x570 held the owner's until the next frame (EVR_LIGHT_LEND_LATE).
            {static std::atomic<unsigned> scans{0};if(scans.fetch_add(1)<4){
                const auto ownLights=read<Handle>(eyeView+viewList::lights),ownDecals=read<Handle>(eyeView+viewList::decals);std::string lights,decals;
                for(uintptr_t o=0;o+16<=0x2000;o+=8){const auto h=read<Handle>(eyeView+o);
                    if(h==ownLights)lights+=(lights.empty()?"":",")+hex(o);if(h==ownDecals)decals+=(decals.empty()?"":",")+hex(o);}
                std::string inSource;for(uintptr_t o=0;o+16<=0x2000;o+=8)if(read<Handle>(sourceView+o)==read<Handle>(sourceView+viewList::lights))inSource+=(inSource.empty()?"":",")+hex(o);
                logFile->event("shared_light_handle_copies","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+",\"light_handle_at\":["+lights+"],\"decal_handle_at\":["+decals+
                    "],\"owner_light_handle_at\":["+inSource+"]");}}
            if(read<uint64_t>(eyeView+viewList::lights)&&read<uint64_t>(sourceView+viewList::lights)&&!read<uint64_t>(eyeView+viewList::cpuLights)){
                const auto own=read<Handle>(eyeView+viewList::lights),source=read<Handle>(sourceView+viewList::lights);validateHandle(pair,own);validateHandle(pair,source);
                if(own==source||!read<uint32_t>(eyeView+viewList::lightCapacity)||read<uint32_t>(eyeView+viewList::lightCapacity)!=read<uint32_t>(sourceView+viewList::lightCapacity))
                    throw std::runtime_error("light buffers of the eye and the owner differ in kind");
                put.own=own;put.view=eyeView;
                static const bool anchorSwap=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ANCHOR_SWAP",v,sizeof v);return !(n==1&&v[0]=='0');}();
                if(anchorSwap&&!c.anchorLights&&read<Handle>(eyeView+viewList::anchorLights)==own&&read<Handle>(sourceView+viewList::anchorLights)==source){
                    c.anchorView=eyeView;c.anchorOwnLights=own;c.anchorLights=true;std::memcpy(reinterpret_cast<void*>(eyeView+viewList::anchorLights),source.data(),16);}
                // 1.0 (2026-10-17): the eye whose own decal list is written this frame (vfx_decal_rebase.hpp: the owner's
                // writeback of this frame is on record) keeps its own buffer here; the culling below writes it and takes
                // the owner's counter. (June set this where the decal descriptor was built; 1.0 builds it elsewhere.)
                if(decalsRequested&&vfxDecals::sourceFor(pair))c.decalsOwnBuffer=true;
                if(anchorSwap&&decalsRequested&&!c.decalsOwnBuffer&&!c.anchorDecals&&read<uint64_t>(eyeView+viewList::decals)&&read<uint64_t>(sourceView+viewList::decals)){
                    const auto ownDecals=read<Handle>(eyeView+viewList::decals),sourceDecals=read<Handle>(sourceView+viewList::decals);
                    if(ownDecals!=sourceDecals&&read<Handle>(eyeView+viewList::anchorDecals)==ownDecals&&read<Handle>(sourceView+viewList::anchorDecals)==sourceDecals){
                        validateHandle(pair,ownDecals);validateHandle(pair,sourceDecals);
                        c.anchorView=eyeView;c.anchorOwnDecals=ownDecals;c.anchorDecals=true;std::memcpy(reinterpret_cast<void*>(eyeView+viewList::anchorDecals),sourceDecals.data(),16);}}
                // The second list's copy (Context::anchorSecond): the place in the View where the eye keeps its own buffer
                // and the owner its own, found once by the values (EVR_ANCHOR_SECOND=0: left alone, for comparisons).
                static const bool anchorSecondSwap=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ANCHOR_SECOND",v,sizeof v);return !(n==1&&v[0]=='0');}();
                if(anchorSwap&&anchorSecondSwap&&shareSecond&&!c.anchorSecond&&read<uint64_t>(eyeView+viewList::second)&&read<uint64_t>(sourceView+viewList::second)){
                    const auto ownSecond=read<Handle>(eyeView+viewList::second),sourceSecond=read<Handle>(sourceView+viewList::second);
                    static std::atomic<uintptr_t> secondAt{0};static std::atomic<unsigned> searches{0};
                    if(ownSecond!=sourceSecond&&!secondAt.load()&&searches.load()<64){
                        const auto n=++searches;std::string all;uintptr_t found=0;
                        for(uintptr_t o=0x600;o+16<=0x2000;o+=8)if(o!=viewList::second&&read<Handle>(eyeView+o)==ownSecond){
                            all+=(all.empty()?"":",")+hex(o);if(!found&&read<Handle>(sourceView+o)==sourceSecond)found=o;}
                        if(found)secondAt=found;
                        if(found||n==1||n==64)logFile->event("shared_second_list_copies","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                            ",\"search\":"+std::to_string(n)+",\"own_handle_at\":["+all+"],\"lent_at\":"+(found?hex(found):std::string("null")));
                    }
                    // Record only (2026-10-16): whether the fog's VFX instances (View+0x5a0, borrowed at the culling) have such a
                    // copy as well - then the eye's fog would read its own list against the owner's clusters too.
                    {static std::atomic<unsigned> fogSearches{0};
                     if(read<uint64_t>(eyeView+viewList::fog)&&read<uint64_t>(sourceView+viewList::fog)&&fogSearches.fetch_add(1)<2){
                        const auto ownFog=read<Handle>(eyeView+viewList::fog),sourceFog=read<Handle>(sourceView+viewList::fog);std::string all,both;
                        for(uintptr_t o=0x600;o+16<=0x2000;o+=8)if(o!=viewList::fog&&read<Handle>(eyeView+o)==ownFog){
                            all+=(all.empty()?"":",")+hex(o);if(read<Handle>(sourceView+o)==sourceFog)both+=(both.empty()?"":",")+hex(o);}
                        logFile->event("shared_fog_list_copies","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                            ",\"differs\":"+(ownFog!=sourceFog?"true":"false")+",\"own_handle_at\":["+all+"],\"owner_has_its_own_at\":["+both+"]");}}
                    const auto at=secondAt.load();
                    if(ownSecond!=sourceSecond&&at&&read<Handle>(eyeView+at)==ownSecond&&read<Handle>(sourceView+at)==sourceSecond){
                        validateHandle(pair,ownSecond);validateHandle(pair,sourceSecond);
                        c.anchorView=eyeView;c.anchorSecondAt=at;c.anchorOwnSecond=ownSecond;c.anchorSecond=true;
                        std::memcpy(reinterpret_cast<void*>(eyeView+at),sourceSecond.data(),16);}
                }
                // The point lights' shadows (View+8c8, the many-lights shadow object - its atlas and parameters, read here at
                // 10c947): the owner's lights carry places in the owner's shadow atlas, so the shading reads that atlas too
                // (2026-10-15, user: "the brightness differs - the bed and the window by it"; Mock game-20261014-231512:
                // the lamp's light on the floor in the owner's eye only, the floor dark in the other). EVR_SHARE_LIGHT_SHADOWS=0
                // leaves the eye its own.
                // 1.0 (2026-10-17): View+0xb48 is not that object any more but the water's (water_displacement_bridge.hpp; the
                // half-resolution water surface, its depth cascades and the caustics texture). RenderDoc game-20261017-034858:
                // the non-owner eye's deferred lighting (event 10485) bound the owner's half-res water surface 14470/14465,
                // the 9-layer depth 14498 and the caustics 14492 - the four only reads across the eyes in the frame - and its
                // own (14679/14675/14701) went unread; with this off (game-20261017-041928) no read crosses the eyes. The
                // caustics drawn from the other eye's water surface: headset report 9 of 2026-10-17 (coloured glints by the
                // weapon in the right eye). Off by default in 1.0; EVR_SHARE_LIGHT_SHADOWS=1 lends it again.
                static const bool shareShadows=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SHARE_LIGHT_SHADOWS",v,sizeof v);return n==1&&v[0]=='1';}();
                const auto ownShadows=read<uintptr_t>(eyeView+viewList::shadows),sourceShadows=read<uintptr_t>(sourceView+viewList::shadows);
                if(shareShadows&&ownShadows&&sourceShadows&&ownShadows!=sourceShadows){put.shadowView=eyeView;put.ownShadows=ownShadows;*reinterpret_cast<uintptr_t*>(eyeView+viewList::shadows)=sourceShadows;}
                {static std::atomic<unsigned> told{0};if(told.fetch_add(1)<6)logFile->event("shared_light_shadow_object","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                    ",\"own\":"+hex(ownShadows)+",\"source\":"+hex(sourceShadows)+",\"swapped\":"+(put.shadowView?"true":"false")+",\"source_view\":"+hex(sourceView)+
                    ",\"owner_eye\":"+std::to_string(worldSharedStages::vfxOwnerEye));}
                std::memcpy(reinterpret_cast<void*>(eyeView+viewList::lights),source.data(),16);c.bindingLights=source;++c.lightDescriptors;
                // 1.0: the second list's buffer too, as for the lights.
                if(read<uint64_t>(eyeView+viewList::second)&&read<uint64_t>(sourceView+viewList::second)){
                    const auto ownSecond=read<Handle>(eyeView+viewList::second),sourceSecond=read<Handle>(sourceView+viewList::second);
                    if(ownSecond!=sourceSecond){validateHandle(pair,ownSecond);validateHandle(pair,sourceSecond);
                        put.secondView=eyeView;put.ownSecond=ownSecond;std::memcpy(reinterpret_cast<void*>(eyeView+viewList::second),sourceSecond.data(),16);}}
                if(lightDescriptorStates.fetch_add(1)<4)logFile->event("shared_light_binding","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                    ",\"capacity\":"+std::to_string(read<uint32_t>(eyeView+viewList::lightCapacity))+",\"shading_reads_owner_list\":true,\"anchor_lights\":"+(c.anchorLights?"true":"false")+",\"anchor_decals\":"+(c.anchorDecals?"true":"false")+",\"anchor_second\":"+(c.anchorSecond?hex(c.anchorSecondAt):std::string("false"))+",\"anchor_vfx_counter\":"+(c.anchorCounter?hex(c.anchorCounterAt):std::string("false")));
            }
        }catch(const std::exception& error){lightDescriptorFailure(c,error.what());}
    }
    return originalShading(a,view,b,c4,e,f,g,h,i);
}
void __fastcall invoke(void* graphContext,void* parameters){
    if(!current||reinterpret_cast<uintptr_t>(_ReturnAddress())!=baseAddress+callerRva){original(graphContext,parameters);return;}
    auto& c=*current;auto& pair=*c.pair;const auto p=reinterpret_cast<uintptr_t>(parameters);
    ++c.calls;
    // Desktop-skip frame: the VFX owner eye produced these lights itself; the other eye borrows from it.
    if(c.eye==worldSharedStages::vfxOwnerEye){original(graphContext,parameters);return;}
    c.sourceView=worldSharedStages::vfxOwnerEye>=0?pair.created[size_t(worldSharedStages::vfxOwnerEye)]:pair.originalView;
    vfxTransparent::observeEye(graphContext,pair,unsigned(c.eye));
    // The eye wrote its own lists this frame (vfx_model_bridge.hpp): it culls them as the game does.
    if(eyeWroteLists&&eyeWroteLists()){
        const auto n=++ownListFrames;
        if(n<=4||n%1800==0)try{logFile->event("shared_point_lights_own_lists","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
            ",\"frames\":"+std::to_string(n)+",\"borrowed\":false,\"written_by_the_eye_from_the_owner_s_vfx\":true");}catch(...){}
        original(graphContext,parameters);return;
    }
    // EVR_LIGHT_BORROW=0 (comparison only): the eye culls its own lists, everything else as before.
    {static const bool borrowLights=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_LIGHT_BORROW",v,sizeof v);return !(n==1&&v[0]=='0');}();
     if(!borrowLights){original(graphContext,parameters);return;}}
    try{
        if(c.calls!=1||c.eye<0||c.eye>1||GetCurrentThreadId()!=pair.renderThread||!worldSharedStages::once)
            throw std::runtime_error("point light scope/thread mismatch");
        c.view=pair.created[c.eye];
        if(!c.view||c.view==c.sourceView||read<uintptr_t>(c.view+8)!=pair.scene||
           read<uintptr_t>(c.sourceView+8)!=pair.scene||read<uintptr_t>(c.view+layout::viewVfx))
            throw std::runtime_error("point light source/private VFX ownership mismatch");
        const auto shared=read<uintptr_t>(pair.scene+0xa88);
        for(const auto& proof:worldSharedStages::completed)
            if(!proof.valid||proof.object!=shared||proof.serial!=pair.serial)
                throw std::runtime_error("original frame updates not proven");
        c.capacity=read<uint32_t>(c.view+viewList::lightCapacity);
        c.ownedCounter=read<uint32_t>(c.view+viewList::lightCounter);c.sourceCounter=read<uint32_t>(c.sourceView+viewList::lightCounter);
        c.owned=read<Handle>(c.view+viewList::lights);c.source=read<Handle>(c.sourceView+viewList::lights);
        validateHandle(pair,c.owned);validateHandle(pair,c.source);
        if(!c.capacity||c.capacity>65536||c.capacity!=read<uint32_t>(c.sourceView+viewList::lightCapacity)||
           !c.ownedCounter||!c.sourceCounter||c.ownedCounter==c.sourceCounter||c.owned==c.source)
            throw std::runtime_error("point light capacities or resource identities differ");
        // Do not silently discard private CPU-selected point lights in this
        // restricted VFX-light experiment. A combined light list needs more work.
        if(read<uint64_t>(c.view+viewList::cpuLights)!=0)throw std::runtime_error("private CPU lights require a merged list");
        // The shading passes must read the list that is culled here (lightDescriptor).
        if(c.lightDescriptors&&c.bindingLights!=c.source)throw std::runtime_error("light buffer of the shading passes differs from the culled one");
        // f7c50 builds a 40-byte descriptor at parameters+18 via eec60.
        // tag +18, handle +20, byte offset +30, stride +38, count +3c.
        const char* differs=read<uintptr_t>(p+8)!=c.view+viewList::parameterAnchor?"anchor":
           read<uint8_t>(p+cullArgument::lights)!=9?"tag":read<Handle>(p+cullArgument::lights+8)!=c.owned?"handle":
           read<uint64_t>(p+cullArgument::lights+0x18)!=0?"offset":read<uint32_t>(p+cullArgument::lights+0x20)!=lightStride?"stride":
           read<uint32_t>(p+cullArgument::lights+0x24)!=c.capacity?"count":read<uint32_t>(p+cullArgument::lightCounter)!=c.ownedCounter?"counter":
           read<uint32_t>(p+cullArgument::lightCapacity)!=c.capacity?"capacity":nullptr;
        {   // 1.0 (2026-10-16): the first time, the arguments as they are (0x110 bytes) and the View's lists, to place them.
            static std::atomic<bool> dumped{false};
            if(!dumped.exchange(true))try{const auto raw=read<std::array<uint8_t,0x110>>(p);std::string text;char b[4];
                for(auto v:raw){std::snprintf(b,sizeof b,"%02x",v);text+=b;}
                logFile->event("shared_point_lights_arguments","\"differs\":\""+std::string(differs?differs:"none")+"\",\"bytes\":\""+text+"\",\"view_anchor\":"+hex(c.view+viewList::parameterAnchor)+
                    ",\"capacity\":"+std::to_string(c.capacity)+",\"owned_counter\":"+std::to_string(c.ownedCounter)+",\"source_counter\":"+std::to_string(c.sourceCounter)+
                    ",\"view_lists\":\""+[&]{std::string s;char q[4];for(auto v:read<std::array<uint8_t,0x40>>(c.view+0x570)){std::snprintf(q,sizeof q,"%02x",v);s+=q;}return s;}()+
                    "\",\"view_counters\":\""+[&]{std::string s;char q[4];for(auto v:read<std::array<uint8_t,0x10>>(c.view+0xb6c)){std::snprintf(q,sizeof q,"%02x",v);s+=q;}return s;}()+
                    "\",\"view_capacities\":\""+[&]{std::string s;char q[4];for(auto v:read<std::array<uint8_t,0x10>>(c.view+0xc4)){std::snprintf(q,sizeof q,"%02x",v);s+=q;}return s;}()+"\"");}catch(...){}
        }
        if(differs)throw std::runtime_error(std::string("clustered light input layout changed (")+differs+")");
    }catch(const std::exception& error){fail(c,error.what());original(graphContext,parameters);return;}
    // The decals, by the same rules. A View without a decal buffer has nothing to borrow into; anything unexpected
    // leaves the decals alone and is put on record once, the lights go on.
    bool decals=false;
    if(decalsRequested){
        try{
            // (The VFX renderer's +1b0 / +1f0, which 188660 tests, are the capacities of the transparent and the decal
            // buffer - 65536 all the time, run game121245 - not this frame's counts. The counts exist on the GPU only.)
            c.decalCapacity=read<uint32_t>(c.view+viewList::decalCapacity);
            c.ownedDecalCounter=read<uint32_t>(c.view+viewList::decalCounter);c.sourceDecalCounter=read<uint32_t>(c.sourceView+viewList::decalCounter);
            c.ownedDecals=read<Handle>(c.view+viewList::decals);c.sourceDecals=read<Handle>(c.sourceView+viewList::decals);
            const bool present=read<uint64_t>(c.view+viewList::decals)!=0&&read<uint64_t>(c.sourceView+viewList::decals)!=0;
            if(present){
                validateHandle(pair,c.ownedDecals);validateHandle(pair,c.sourceDecals);
                if(!c.decalCapacity||c.decalCapacity>65536||c.decalCapacity!=read<uint32_t>(c.sourceView+viewList::decalCapacity)||
                   !c.ownedDecalCounter||!c.sourceDecalCounter||c.ownedDecalCounter==c.sourceDecalCounter||c.ownedDecals==c.sourceDecals)
                    throw std::runtime_error("decal capacities or resource identities differ");
                // f7c50 builds the 40-byte descriptor at parameters+40 via eeb20:
                // tag +40, handle +48, byte offset +58, stride +60 (112), count +64; counter +94, capacity +a0.
                if(read<uint8_t>(p+cullArgument::decals)!=9||read<Handle>(p+cullArgument::decals+8)!=c.ownedDecals||read<uint64_t>(p+cullArgument::decals+0x18)!=0||
                   read<uint32_t>(p+cullArgument::decals+0x20)!=112||read<uint32_t>(p+cullArgument::decals+0x24)!=c.decalCapacity||
                   read<uint32_t>(p+cullArgument::decalCounter)!=c.ownedDecalCounter||read<uint32_t>(p+cullArgument::decalCapacity)!=c.decalCapacity)
                    throw std::runtime_error("clustered decal input layout changed");
                if(c.decalDescriptors&&c.bindingDecals!=c.sourceDecals)throw std::runtime_error("decal buffer of the shading passes differs from the culled one");
                if(c.decalDescriptors&&c.decalsOwnBuffer)throw std::runtime_error("decal buffers of the shading passes are mixed");
                decals=true;
                if(c.decalsOwnBuffer){
                    // The shading passes read the eye's own buffer this frame. Without the eye's writeback it holds
                    // nothing of this frame, so the eye then shows no decals for the frame instead of old ones.
                    c.decalsRebased=vfxDecals::writeEye(graphContext,pair,unsigned(c.eye),c.view,c.sourceView);
                    if(!c.decalsRebased)decals=false;
                }
            }
            if(decalStates.fetch_add(1)<4)logFile->event("shared_decals_state","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
                ",\"buffers_present\":"+(present?"true":"false")+",\"borrow\":"+(decals?"true":"false")+",\"capacity\":"+std::to_string(c.decalCapacity)+
                ",\"owned_counter\":"+std::to_string(c.ownedDecalCounter)+",\"source_counter\":"+std::to_string(c.sourceDecalCounter)+",\"shading_descriptors\":"+std::to_string(c.decalDescriptors)+
                ",\"own_list_rebased\":"+(c.decalsRebased?"true":"false"));
        }catch(const std::exception& error){decals=false;decalFailure(c,error.what());}
    }
    // The third list of this culling: the fog's VFX instances (188030 "vfx_write_inject_instances_back", written for the
    // View with the VFX only): View+938, 32 bytes a record, counter id View+8e8, capacity View+84; descriptor at
    // parameters+68 (eeef0 at fe387), counter +98, capacity +a4. The eye did not borrow it, so the smoke and haze of
    // the effects were in the owner's fog only (2026-10-14, user: "the fog's thickness and the brightness differ between
    // the eyes"; Mock self-test game-20261014-191341: the warm haze of the room in one eye). The fog pass reads View+938 /
    // +8e8 itself after the culling (101c70), so the View keeps the owner's list until the scope ends, as with the
    // lights. The records are relative to the owner's camera: off by the eyes' distance, nothing in a fog.
    bool fog=false;
    try{
        if(eyeListFix&&read<uint64_t>(c.view+viewList::fog)&&read<uint64_t>(c.sourceView+viewList::fog)){
            c.fogCapacity=read<uint32_t>(c.view+viewList::fogCapacity);
            c.ownedFogCounter=read<uint32_t>(c.view+viewList::fogCounter);c.sourceFogCounter=read<uint32_t>(c.sourceView+viewList::fogCounter);
            c.ownedFog=read<Handle>(c.view+viewList::fog);c.sourceFog=read<Handle>(c.sourceView+viewList::fog);
            validateHandle(pair,c.ownedFog);validateHandle(pair,c.sourceFog);
            if(!c.fogCapacity||c.fogCapacity>65536||c.fogCapacity!=read<uint32_t>(c.sourceView+viewList::fogCapacity)||
               !c.ownedFogCounter||!c.sourceFogCounter||c.ownedFogCounter==c.sourceFogCounter||c.ownedFog==c.sourceFog)
                throw std::runtime_error("fog instance capacities or resource identities differ");
            if(read<uint8_t>(p+cullArgument::fog)!=9||read<Handle>(p+cullArgument::fog+8)!=c.ownedFog||read<uint64_t>(p+cullArgument::fog+0x18)!=0||read<uint32_t>(p+cullArgument::fog+0x20)!=32||
               read<uint32_t>(p+cullArgument::fog+0x24)!=c.fogCapacity||read<uint32_t>(p+cullArgument::fogCounter)!=c.ownedFogCounter||read<uint32_t>(p+cullArgument::fogCapacity)!=c.fogCapacity)
                throw std::runtime_error("clustered fog instance input layout changed");
            fog=true;
        }
    }catch(const std::exception& error){fog=false;fogFailure(c,error.what());}
    struct RestoreParameters {
        Context& c;uintptr_t p;
        ~RestoreParameters() noexcept {
            std::memcpy(reinterpret_cast<void*>(p+cullArgument::lights+8),c.owned.data(),16);
            std::memcpy(reinterpret_cast<void*>(p+cullArgument::lightCounter),&c.ownedCounter,4);
            if(c.decalsBorrowed){
                std::memcpy(reinterpret_cast<void*>(p+cullArgument::decals+8),c.ownedDecals.data(),16);
                std::memcpy(reinterpret_cast<void*>(p+cullArgument::decalCounter),&c.ownedDecalCounter,4);
            }
            if(c.fogBorrowed){
                std::memcpy(reinterpret_cast<void*>(p+cullArgument::fog+8),c.ownedFog.data(),16);
                std::memcpy(reinterpret_cast<void*>(p+cullArgument::fogCounter),&c.ownedFogCounter,4);
            }
            if(c.secondBorrowed){
                std::memcpy(reinterpret_cast<void*>(p+cullArgument::second+8),c.ownedSecond.data(),16);
                std::memcpy(reinterpret_cast<void*>(p+cullArgument::secondCounter),&c.ownedSecondCounter,4);
            }
            try{c.parametersRestored=read<Handle>(p+cullArgument::lights+8)==c.owned&&read<uint32_t>(p+cullArgument::lightCounter)==c.ownedCounter&&
                (!c.decalsBorrowed||(read<Handle>(p+cullArgument::decals+8)==c.ownedDecals&&read<uint32_t>(p+cullArgument::decalCounter)==c.ownedDecalCounter))&&
                (!c.fogBorrowed||(read<Handle>(p+cullArgument::fog+8)==c.ownedFog&&read<uint32_t>(p+cullArgument::fogCounter)==c.ownedFogCounter));}
            catch(...){c.parametersRestored=false;}
        }
    } restore{c,p};
    if(decals){
        c.decalsBorrowed=true;
        if(!c.decalsRebased){
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::decals),c.sourceDecals.data(),16);
            std::memcpy(reinterpret_cast<void*>(p+cullArgument::decals+8),c.sourceDecals.data(),16);
        }
        std::memcpy(reinterpret_cast<void*>(c.view+viewList::decalCounter),&c.sourceDecalCounter,4);
        std::memcpy(reinterpret_cast<void*>(p+cullArgument::decalCounter),&c.sourceDecalCounter,4);
    }
    if(fog){
        c.fogBorrowed=true;
        std::memcpy(reinterpret_cast<void*>(c.view+viewList::fog),c.sourceFog.data(),16);
        std::memcpy(reinterpret_cast<void*>(c.view+viewList::fogCounter),&c.sourceFogCounter,4);
        std::memcpy(reinterpret_cast<void*>(p+cullArgument::fog+8),c.sourceFog.data(),16);
        std::memcpy(reinterpret_cast<void*>(p+cullArgument::fogCounter),&c.sourceFogCounter,4);
    }
    {if(shareSecond&&eyeListFix)try{
        const auto own=read<Handle>(c.view+viewList::second),source=read<Handle>(c.sourceView+viewList::second);
        const auto ownCounter=read<uint32_t>(c.view+viewList::secondCounter),sourceCounter=read<uint32_t>(c.sourceView+viewList::secondCounter);
        const auto capacity=read<uint32_t>(c.view+viewList::secondCapacity);
        if(read<uint64_t>(c.view+viewList::second)&&read<uint64_t>(c.sourceView+viewList::second)&&own!=source&&ownCounter&&sourceCounter&&ownCounter!=sourceCounter&&
           capacity&&capacity==read<uint32_t>(c.sourceView+viewList::secondCapacity)&&read<uint8_t>(p+cullArgument::second)==9&&read<Handle>(p+cullArgument::second+8)==own&&
           read<uint32_t>(p+cullArgument::second+0x20)==112&&read<uint32_t>(p+cullArgument::secondCounter)==ownCounter&&read<uint32_t>(p+cullArgument::secondCapacity)==capacity){
            validateHandle(pair,own);validateHandle(pair,source);
            c.ownedSecond=own;c.sourceSecond=source;c.ownedSecondCounter=ownCounter;c.sourceSecondCounter=sourceCounter;c.secondBorrowed=true;
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::second),source.data(),16);
            std::memcpy(reinterpret_cast<void*>(c.view+viewList::secondCounter),&sourceCounter,4);
            std::memcpy(reinterpret_cast<void*>(p+cullArgument::second+8),source.data(),16);
            std::memcpy(reinterpret_cast<void*>(p+cullArgument::secondCounter),&sourceCounter,4);
        }else{static std::atomic<unsigned> told{0};if(told.fetch_add(1)<3)logFile->event("shared_second_list_skipped","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(c.eye)+
            ",\"own_counter\":"+std::to_string(ownCounter)+",\"source_counter\":"+std::to_string(sourceCounter)+",\"capacity\":"+std::to_string(capacity));}
     }catch(const std::exception& e){static std::atomic<unsigned> told{0};if(told.fetch_add(1)<3)try{logFile->event("shared_second_list_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}}
    c.borrowed=true;
    std::memcpy(reinterpret_cast<void*>(c.view+viewList::lights),c.source.data(),16);
    std::memcpy(reinterpret_cast<void*>(c.view+viewList::lightCounter),&c.sourceCounter,4);
    std::memcpy(reinterpret_cast<void*>(p+cullArgument::lights+8),c.source.data(),16);
    std::memcpy(reinterpret_cast<void*>(p+cullArgument::lightCounter),&c.sourceCounter,4);
    original(graphContext,parameters);
}
inline void install(){
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva,call=baseAddress+callerRva-5;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
        throw std::runtime_error("clustered lights signature/caller mismatch");
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&invoke),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("clustered lights hook failed");
    enabled=true;
    {
        const auto shadingBytes=parsePattern(shadingPattern);const auto shadingTarget=baseAddress+shadingRva,site=baseAddress+shadingCaller-5;
        if(scan(reinterpret_cast<const uint8_t*>(shadingTarget),shadingBytes.size(),shadingBytes)!=std::vector<size_t>{0}||
           read<uint8_t>(site)!=0xe8||uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=shadingTarget)throw std::runtime_error("shading bindings signature/caller mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(shadingTarget),reinterpret_cast<void*>(&shadingBindings),reinterpret_cast<void**>(&originalShading))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(shadingTarget))!=MH_OK)throw std::runtime_error("shading bindings hook failed");
        shadingEnabled=true;
        logFile->event("shared_light_binding_hook_enabled","\"rva\":"+hex(shadingRva)+",\"caller\":"+hex(shadingCaller)+",\"pattern\":"+jsonString(shadingPattern));
    }
    // 1.0 (2026-10-16): the decal descriptor 11fbd0 is a small leaf the render calls at 12dd24 and then uses r10 again (12dd34),
    // which a C++ detour does not keep: hooking it crashed the game (Mock run game-20261016-091438, ACCESS_VIOLATION in 65e70).
    // The decals' descriptor stays the game's; the culling still borrows the owner's decal list below.
    if(decalsRequested&&false){
        const auto decalBytes=parsePattern(decalDescriptorPattern);const auto decalTarget=baseAddress+decalDescriptorRva;
        if(scan(reinterpret_cast<const uint8_t*>(decalTarget),decalBytes.size(),decalBytes)!=std::vector<size_t>{0})throw std::runtime_error("decal descriptor signature mismatch");
        for(const auto after:decalDescriptorCallers){const auto site=baseAddress+after-5;
            if(read<uint8_t>(site)!=0xe8||uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=decalTarget)throw std::runtime_error("decal descriptor caller mismatch");}
        if(MH_CreateHook(reinterpret_cast<void*>(decalTarget),reinterpret_cast<void*>(&decalDescriptor),reinterpret_cast<void**>(&originalDecalDescriptor))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(decalTarget))!=MH_OK)throw std::runtime_error("decal descriptor hook failed");
        decalDescriptorEnabled=true;
        logFile->event("shared_decals_hook_enabled","\"rva\":"+hex(decalDescriptorRva)+",\"callers\":[\"0x13ea95\",\"0x1405ce\"],\"pattern\":"+jsonString(decalDescriptorPattern));
    }
    logFile->event("shared_point_lights_hook_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern));
}
inline void disable() noexcept {
    if(shadingEnabled){
        const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+shadingRva));if(status==MH_OK)shadingEnabled=false;
        try{logFile->event("shared_light_binding_hook_removed","\"status\":"+std::to_string(status)+",\"failures\":"+std::to_string(lightDescriptorFailures.load()));}catch(...){}
    }
    if(decalDescriptorEnabled){
        const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+decalDescriptorRva));if(status==MH_OK)decalDescriptorEnabled=false;
        try{logFile->event("shared_decals_hook_removed","\"status\":"+std::to_string(status)+",\"failures\":"+std::to_string(decalFailures.load()));}catch(...){}
    }
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("shared_point_lights_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
}
}
