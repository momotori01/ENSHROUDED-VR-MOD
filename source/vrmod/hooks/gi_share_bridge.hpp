// GI sharing between the eyes (world-gi-share.txt, 2026-10-06).
// The probe GI of a View lives in its GI object (View+950, 0x1ce0 bytes, feature case 7). Per frame,
// f7c50 runs 73020 (dynamic_gi_prepare: probe update, ray shooting; also flips the ping-pong index +1c8)
// and 77c80 (dynamic_gi: hit shading, probe blending/convergence, and the screen-space 6ac40 rasterize /
// reproject / prefilter of irradiance). Probe data is world-space; only six handles of the object are
// screen-sized (current/history irradiance at +228/+238/+600/+610/+620/+630).
// While the right eye records, its View+950 points to the left eye's GI object with the right eye's own
// screen handles swapped in, 73020 is skipped and the world-space stages called from 77c80 are skipped, so
// the probes are updated once per frame and the right eye only rasterizes its own screen-space GI.
extern "C" { void* evr_gi_trampolines[24]{}; }
extern "C" void evr_gi_stub0();extern "C" void evr_gi_stub1();extern "C" void evr_gi_stub2();extern "C" void evr_gi_stub3();
extern "C" void evr_gi_stub4();extern "C" void evr_gi_stub5();extern "C" void evr_gi_stub6();extern "C" void evr_gi_stub7();
extern "C" void evr_gi_stub8();extern "C" void evr_gi_stub9();extern "C" void evr_gi_stub10();extern "C" void evr_gi_stub11();
extern "C" void evr_gi_stub12();extern "C" void evr_gi_stub13();extern "C" void evr_gi_stub14();extern "C" void evr_gi_stub15();
extern "C" void evr_gi_stub16();extern "C" void evr_gi_stub17();extern "C" void evr_gi_stub18();extern "C" void evr_gi_stub19();
extern "C" void evr_gi_stub20();extern "C" void evr_gi_stub21();extern "C" void evr_gi_stub22();extern "C" void evr_gi_stub23();
namespace giShare {
// callers: none = skipped when called from dynamic GI (77c80); otherwise only these return addresses are skipped.
struct Target{uintptr_t rva;const char* pattern;std::array<uintptr_t,2> callers{};int object=-1;bool enabled=false;}; // object: borrowedObjects index
inline std::array<Target,21> targets{{
    {0x9ea50,"40 55 53 56 41 56 41 57 48 8D AC 24 90 F1",{0x136476}}, // dynamic_gi_prepare (skipped only from f7c50 102c2b)
    {0x977d0,"40 55 53 56 57 41 54 41 55 41 56 41 57 48"}, // shade hits (terrain/building/sky/sdf)
    {0x8f2d0,"48 89 5C 24 20 55 56 57 41 54 41 55 41 56"}, // blend world cache radiance
    {0x9d310,"4C 89 4C 24 20 55 53 56 57 41 54 41 55 41"}, // blend world cache SH irradiance
    {0x8eeb0,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56"}, // blend radiance
    {0x61fa0,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56"}, // blend distance probes
    {0x8dba0,"48 8B C4 4C 89 48 20 4C 89 40 18 55 53 56"}, // blend neighbour radiance
    {0x8d8a0,"48 89 5C 24 20 55 56 57 41 54 41 55 41 56"}, // blend irradiance
    {0x9cc60,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56"}, // update probe border
    {0x9cfc0,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56"}, // update probe irradiance border
    {0x94c30,"48 8B C4 4C 89 40 18 48 89 50 10 55 53 56"}, // prefilter radiance
    {0xa1f60,"48 8B C4 48 89 58 20 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 C8 FA FF FF 48 81 EC 00 06 00 00 0F 10 15 ? ? ? ? 4C 8B 3A 4C 8B F2 0F"}, // player-centred distance and material data
    {0x588b0,"48 8B C4 48 89 58 18 48 89 50 10 55 56 57"}, // ambient probe visibility
    {0x8b1c0,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D0 FD"}, // fill requested ambient probes
    {0x8aa00,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D0 FD"}, // fill primary / world-cache ambient probes
    {0x854a0,"4C 89 4C 24 20 4C 89 44 24 18 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D8 FA FF FF"}, // converge ambient probes
    {0xca330,"48 8B C4 4C 89 40 18 48 89 48 08 55 53 56",{0x1367f6},0}, // render_many_lights_shadows (View+9a0), from f7c50 102ff5
    {0x7db40,"48 89 74 24 18 57 48 81 EC 80 00 00 00 80 BA 88 00 00",{0x12aba3,0x12abd1},1}, // heightmap frame reset (View+9f8/+a00)
    {0x7df50,"48 83 EC 18 F2 0F 10 02 0F 57 E4 F3 0F 10",{0x1322a5,0x1322d9},1}, // heightmap centre and frame counter from the camera
    {0x50d30,"40 55 53 56 57 41 54 41 56 41 57 48 8D AC",{0x1334ad,0x1334ad},1}, // dynamic_heightmap_fill
    // 20: the screen-space GI of a View (rasterize / reproject / prefilter), from 77c80 at 797a7 / 79d9f (its result is
    // not used there). Skipped for the right eye in the borrow-screen form below only.
    {0x95500,"48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 58 F4 FF FF",{0xa6380,0xa66a1}},
}};
constexpr uintptr_t dynamicLo=0xa43a0,dynamicHi=0xa6f3d;
// Other per-View world-space objects the right eye borrows whole (their updates are skipped targets above):
// View+9a0 many-lights shadow atlas (0xf8 bytes; its per-View light-list write 9a430 still runs for each eye),
// View+9f8/+a00 dynamic heightmaps (0x98 bytes each; the readers 512e0/51370 still run for each eye).
constexpr std::array<std::pair<uint32_t,uint32_t>,3> borrowedObjects{{{0x9a0,0xf8},{0x9f8,0x98},{0xa00,0x98}}};
constexpr std::array<uint32_t,6> screenFields{0x228,0x238,0x600,0x610,0x620,0x630};
constexpr uint32_t objectBytes=0x1ce0;
constexpr uint32_t warmupPairs=120;
inline bool requested=false,enabled=false,disabled=false,checked=false;
// Diagnostic (2026-10-09): stages whose bit is set still run in the right eye (on the shared object). The shared GI
// darkened the right eye's ground with head motion; the view-driven ambient-probe stages (bits 12..15) are suspects.
inline uint32_t keepMask=0;
// Borrowed View objects available in both eyes (decided on the first shared frame). Heightmaps are borrowed as a pair.
inline std::array<bool,3> borrowActive{};
inline thread_local bool active=false;
// 2026-10-15: the right eye runs the GI prepare (73020) too, with its own camera. The GI object keeps per-View camera
// state (dump game-20261015-032232: the world-to-clip matrix +40, the camera position +114 and its previous +120, the
// inverse matrix +160 differ between the eyes; everything else but the resource handles is the same), and 73020 writes
// them from f7c50's per-View parameters and uploads them for the GPU. Skipping 73020 left the right eye's screen GI
// (6ac40) with the left eye's camera (headset run 1 and Mock 2026-10-09: the right eye's ground dark and mottled with
// head motion; the look-round comparison 2026-10-14: 1.67 -> 7.49). Now: before the right eye's 73020 the object's
// +30..+1d0 are saved and the current camera position +114 is set to the right eye's previous one (73020 copies it to
// +120), 73020 runs (its ping-pong flip is undone at the next gated stage), and when the eye's frame is over the saved
// state comes back for the left eye's next frame. The world-space stages of 77c80 stay skipped. EVR_GI_SHARE_PREPARE=0:
// the form before (73020 skipped).
inline const bool rightPrepare=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GI_SHARE_PREPARE",v,sizeof v);return !(n==1&&v[0]=='0');}();
struct RightCamera {bool valid=false;uint64_t frame=0;std::array<uint8_t,12> previousPosition{};};
inline RightCamera rightCamera;
inline std::atomic<uint64_t> prepared{0},parityRestored{0};
constexpr uint32_t stateBegin=0x30,stateEnd=0x1d0;   // the per-frame camera/time/ping-pong state 73020 writes
// The borrow-screen form (2026-10-15, after the shared-object forms failed: own GI per eye 1.93 / 2.44 of left-right
// difference on the room route, shared 4.36 / 4.84, shared with every stage run in the right eye too 3.59 / 3.88).
// Each eye keeps its own GI object and probes, but the right eye does not draw its screen-space GI (6ac40 skipped):
// during its frame its object's six screen fields and ping-pong index are the left eye's, so its lighting reads the
// irradiance the left eye rasterized this frame. The indirect light is low-frequency and prefiltered; the parallax
// between the eyes moves it by a few pixels at a distance (more on the hands), which the eyes then share instead of
// each converging on its own. EVR_GI_BORROW_SCREEN=1 turns it on (not with --world-gi-share).
inline const bool borrowScreen=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GI_BORROW_SCREEN",v,sizeof v);return n==1&&v[0]=='1';}();
inline uint64_t borrowFrames=0;inline bool borrowChecked=false;
inline std::array<std::atomic<uint64_t>,21> skipped{};
inline uint64_t frames=0;
inline void disable(const std::string& reason) noexcept {
    if(disabled)return;disabled=true;
    try{logFile->event("gi_share_disabled","\"reason\":"+jsonString(reason)+",\"frames\":"+std::to_string(frames));}catch(...){}
}
using Handle=std::array<uint8_t,16>;
// Texture extent of a frame graph handle {graph, index}; {0,0} when not a handle of this graph.
inline std::pair<uint32_t,uint32_t> extent(uintptr_t graph,const Handle& h){
    uintptr_t g{};std::memcpy(&g,h.data(),8);uint16_t index{};std::memcpy(&index,h.data()+8,2);
    if(g!=graph||index>=read<uint64_t>(graph+0x88))return {0,0};
    const auto d=read<uintptr_t>(graph+0x80)+uintptr_t(index)*48;
    return {read<uint32_t>(d+4),read<uint32_t>(d+8)};
}
// One-time check of both GI objects: the six screen fields are screen-sized textures in both eyes, and no
// other field holds a pair of screen-sized textures (which would be per-eye state this bridge does not swap).
inline void verify(uintptr_t graph,uintptr_t gi0,uintptr_t gi1){
    const auto w=extent(graph,read<Handle>(gi0+screenFields[0]));
    if(!w.first||!w.second)throw std::runtime_error("screen irradiance handle not in the frame graph");
    const auto screen=[&](std::pair<uint32_t,uint32_t> e){return e==w||e==std::pair<uint32_t,uint32_t>{(w.first+1)/2,w.second};};
    for(auto f:screenFields){
        const auto a=extent(graph,read<Handle>(gi0+f)),b=extent(graph,read<Handle>(gi1+f));
        if(a!=b||!screen(a))throw std::runtime_error("screen field "+hex(f)+" is not a screen-sized texture pair");
    }
    for(uint32_t o=0;o+16<=objectBytes;o+=8){
        if(std::find(screenFields.begin(),screenFields.end(),o)!=screenFields.end())continue;
        const auto a=extent(graph,read<Handle>(gi0+o)),b=extent(graph,read<Handle>(gi1+o));
        if(a.first&&a==b&&screen(a)&&read<Handle>(gi0+o)!=read<Handle>(gi1+o))
            throw std::runtime_error("unswapped per-eye screen resource at "+hex(o));
    }
    logFile->event("gi_share_verified","\"screen\":["+std::to_string(w.first)+","+std::to_string(w.second)+"],\"fields\":6");
}
// Borrowed objects must hold no per-eye screen-sized resources.
inline void verifyBorrowed(uintptr_t graph,uintptr_t gi0,uintptr_t a0,uintptr_t a1,uint32_t bytes,uint32_t viewOffset){
    const auto w=extent(graph,read<Handle>(gi0+screenFields[0]));
    const auto screen=[&](std::pair<uint32_t,uint32_t> e){return e==w||e==std::pair<uint32_t,uint32_t>{(w.first+1)/2,w.second};};
    for(uint32_t o=0;o+16<=bytes;o+=8){
        const auto a=extent(graph,read<Handle>(a0+o)),b=extent(graph,read<Handle>(a1+o));
        if(a.first&&a==b&&screen(a)&&read<Handle>(a0+o)!=read<Handle>(a1+o))
            throw std::runtime_error("borrowed View"+hex(viewOffset)+" object holds a per-eye screen resource at "+hex(o));
    }
    logFile->event("gi_share_borrow_verified","\"view_offset\":"+hex(viewOffset)+",\"bytes\":"+std::to_string(bytes));
}
struct Scope {
    uintptr_t eyeView=0,shared=0,own=0;std::array<Handle,6> saved{};std::array<uintptr_t,borrowedObjects.size()> ownBorrowed{};bool on=false;
    std::array<uint8_t,stateEnd-stateBegin> savedState{};bool stateSaved=false,parityBack=false;uint64_t frame=0;
    Scope(uintptr_t graph,uintptr_t leftView,uintptr_t rightView,bool use){
        if(!use||!enabled||disabled)return;
        try{
            const auto gi0=read<uintptr_t>(leftView+0x950),gi1=read<uintptr_t>(rightView+0x950);
            if(!gi0||!gi1||gi0==gi1)throw std::runtime_error("eye GI objects missing or already shared");
            std::array<uintptr_t,borrowedObjects.size()> left{};
            for(size_t i=0;i<borrowedObjects.size();++i){
                left[i]=read<uintptr_t>(leftView+borrowedObjects[i].first);ownBorrowed[i]=read<uintptr_t>(rightView+borrowedObjects[i].first);}
            if(!checked){verify(graph,gi0,gi1);
                for(size_t i=0;i<borrowedObjects.size();++i){
                    borrowActive[i]=left[i]&&ownBorrowed[i]&&left[i]!=ownBorrowed[i];
                    if(borrowActive[i])verifyBorrowed(graph,gi0,left[i],ownBorrowed[i],borrowedObjects[i].second,borrowedObjects[i].first);
                    else logFile->event("gi_share_borrow_unavailable","\"view_offset\":"+hex(borrowedObjects[i].first));}
                borrowActive[1]=borrowActive[2]=borrowActive[1]&&borrowActive[2];
                checked=true;}
            for(size_t i=0;i<borrowedObjects.size();++i)
                if(borrowActive[i]&&(!left[i]||!ownBorrowed[i]||left[i]==ownBorrowed[i]))throw std::runtime_error("eye View"+hex(borrowedObjects[i].first)+" object changed");
            for(size_t i=0;i<screenFields.size();++i)saved[i]=read<Handle>(gi0+screenFields[i]);
            for(size_t i=0;i<screenFields.size();++i){const auto mine=read<Handle>(gi1+screenFields[i]);
                std::memcpy(reinterpret_cast<void*>(gi0+screenFields[i]),mine.data(),16);}
            *reinterpret_cast<uintptr_t*>(rightView+0x950)=gi0;
            for(size_t i=0;i<borrowedObjects.size();++i)if(borrowActive[i])*reinterpret_cast<uintptr_t*>(rightView+borrowedObjects[i].first)=left[i];
            eyeView=rightView;shared=gi0;own=gi1;active=true;on=true;++frames;frame=frames;current=this;
        }catch(const std::exception& e){disable(e.what());}
    }
    ~Scope(){
        if(!on)return;
        active=false;current=nullptr;
        if(stateSaved){   // the right eye's camera position of this frame is its previous one next frame; the left eye's state comes back
            std::memcpy(rightCamera.previousPosition.data(),reinterpret_cast<const void*>(shared+0x114),12);rightCamera.valid=true;rightCamera.frame=frame;
            std::memcpy(reinterpret_cast<void*>(shared+stateBegin),savedState.data(),savedState.size());}
        *reinterpret_cast<uintptr_t*>(eyeView+0x950)=own;
        for(size_t i=0;i<borrowedObjects.size();++i)if(borrowActive[i])*reinterpret_cast<uintptr_t*>(eyeView+borrowedObjects[i].first)=ownBorrowed[i];
        for(size_t i=0;i<screenFields.size();++i)std::memcpy(reinterpret_cast<void*>(shared+screenFields[i]),saved[i].data(),16);
    }
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    static inline thread_local Scope* current=nullptr;
};
struct BorrowScope {
    uintptr_t rightGi=0;std::array<Handle,6> saved{};uint32_t savedParity=0;bool on=false;
    static inline thread_local bool active=false;
    BorrowScope(uintptr_t graph,uintptr_t leftView,uintptr_t rightView,bool use){
        if(!use||!enabled||disabled)return;
        try{
            const auto gi0=read<uintptr_t>(leftView+0x950),gi1=read<uintptr_t>(rightView+0x950);
            if(!gi0||!gi1||gi0==gi1)throw std::runtime_error("eye GI objects missing or already shared");
            if(!borrowChecked){verify(graph,gi0,gi1);borrowChecked=true;}
            for(size_t i=0;i<screenFields.size();++i)saved[i]=read<Handle>(gi1+screenFields[i]);
            savedParity=read<uint32_t>(gi1+0x1c8);
            for(size_t i=0;i<screenFields.size();++i){const auto theirs=read<Handle>(gi0+screenFields[i]);std::memcpy(reinterpret_cast<void*>(gi1+screenFields[i]),theirs.data(),16);}
            *reinterpret_cast<uint32_t*>(gi1+0x1c8)=read<uint32_t>(gi0+0x1c8);
            rightGi=gi1;active=true;on=true;++borrowFrames;
        }catch(const std::exception& e){disable(e.what());}
    }
    ~BorrowScope(){
        if(!on)return;
        active=false;
        for(size_t i=0;i<screenFields.size();++i)std::memcpy(reinterpret_cast<void*>(rightGi+screenFields[i]),saved[i].data(),16);
        *reinterpret_cast<uint32_t*>(rightGi+0x1c8)=savedParity;
    }
    BorrowScope(const BorrowScope&)=delete;BorrowScope& operator=(const BorrowScope&)=delete;
};
inline void install(){
    for(const auto& t:targets)for(auto c:t.callers)if(c){const auto call=baseAddress+c-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+t.rva)
            throw std::runtime_error("world share caller changed for "+hex(t.rva));}
    void(*stubs[24])()={evr_gi_stub0,evr_gi_stub1,evr_gi_stub2,evr_gi_stub3,evr_gi_stub4,evr_gi_stub5,evr_gi_stub6,evr_gi_stub7,
        evr_gi_stub8,evr_gi_stub9,evr_gi_stub10,evr_gi_stub11,evr_gi_stub12,evr_gi_stub13,evr_gi_stub14,evr_gi_stub15,
        evr_gi_stub16,evr_gi_stub17,evr_gi_stub18,evr_gi_stub19,evr_gi_stub20,evr_gi_stub21,evr_gi_stub22,evr_gi_stub23};
    static_assert(std::tuple_size<decltype(targets)>::value<=24,"one assembly stub per target");
    for(size_t i=0;i<targets.size();++i){
        auto& t=targets[i];const auto bytes=parsePattern(t.pattern);const auto site=baseAddress+t.rva;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("GI stage signature changed at "+hex(t.rva));
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(stubs[i]),&evr_gi_trampolines[i])!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("GI stage hook failed at "+hex(t.rva));
        t.enabled=true;
    }
    enabled=true;
    logFile->event("gi_share_hooks_enabled","\"stages\":"+std::to_string(targets.size())+",\"dynamic_gi\":["+hex(dynamicLo)+","+hex(dynamicHi)+"],\"borrowed_view_objects\":[\"0x9a0\",\"0x9f8\",\"0xa00\"],\"borrow_screen\":"+(borrowScreen?"true":"false"));
}
inline void uninstall() noexcept {
    for(auto it=targets.rbegin();it!=targets.rend();++it)if(it->enabled){
        const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+it->rva));if(status==MH_OK)it->enabled=false;}
    if(!enabled)return;enabled=false;
    try{std::ostringstream f;f<<"\"frames\":"<<frames<<",\"skipped\":[";for(size_t i=0;i<skipped.size();++i)f<<(i?",":"")<<skipped[i].load();f<<']';
        logFile->event("gi_share_hooks_removed",f.str());}catch(...){}
}
}
extern "C" bool evr_gi_should_skip(uint32_t index,uintptr_t returnAddress,uintptr_t object,uintptr_t parameters) noexcept {
    if(index>=giShare::targets.size())return false;
    if(index==20){   // the screen-space GI: left out for the right eye of the borrow-screen form only
        if(!giShare::BorrowScope::active)return false;
        const auto r=returnAddress-baseAddress;const bool skip=r==giShare::targets[20].callers[0]||r==giShare::targets[20].callers[1];
        if(skip)giShare::skipped[20].fetch_add(1,std::memory_order_relaxed);
        return skip;
    }
    if(!giShare::active)return false;
    auto* scope=giShare::Scope::current;
    if(scope&&scope->stateSaved&&!scope->parityBack&&index!=0){   // the first gated stage after the right eye's 73020: its ping-pong flip undone
        try{std::memcpy(reinterpret_cast<void*>(scope->shared+0x1c8),scope->savedState.data()+(0x1c8-giShare::stateBegin),4);}catch(...){}
        scope->parityBack=true;giShare::parityRestored.fetch_add(1,std::memory_order_relaxed);}
    if(index==0&&(giShare::rightPrepare||(giShare::keepMask&1u))&&scope&&!scope->stateSaved&&object==scope->shared&&returnAddress-baseAddress==giShare::targets[0].callers[0]){
        try{
            std::memcpy(scope->savedState.data(),reinterpret_cast<const void*>(object+giShare::stateBegin),scope->savedState.size());scope->stateSaved=true;
            const bool continued=giShare::rightCamera.valid&&giShare::rightCamera.frame+1==scope->frame;
            std::memcpy(reinterpret_cast<void*>(object+0x114),continued?giShare::rightCamera.previousPosition.data():reinterpret_cast<const uint8_t*>(parameters+0xf8),12);
            const auto n=giShare::prepared.fetch_add(1,std::memory_order_relaxed)+1;
            if(n<=3||n%600==0)logFile->event("gi_share_right_prepare","\"frames\":"+std::to_string(giShare::frames)+",\"prepared\":"+std::to_string(n)+",\"previous_continued\":"+(continued?"true":"false")+",\"parity_restored\":"+std::to_string(giShare::parityRestored.load()));
        }catch(...){scope->stateSaved=false;}
        if(scope->stateSaved)return false;   // 73020 runs for the right eye on the shared object, with the right eye's parameters
    }
    if((giShare::keepMask>>index)&1u)return false;
    if(giShare::targets[index].object>=0&&!giShare::borrowActive[size_t(giShare::targets[index].object)])return false;
    const auto r=returnAddress-baseAddress;const auto& callers=giShare::targets[index].callers;
    const bool skip=callers[0]?(r==callers[0]||(callers[1]&&r==callers[1])):(r>=giShare::dynamicLo&&r<giShare::dynamicHi);
    if(skip)giShare::skipped[index].fetch_add(1,std::memory_order_relaxed);
    return skip;
}
