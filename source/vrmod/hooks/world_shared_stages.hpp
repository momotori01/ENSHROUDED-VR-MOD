// Two shared world updates enqueue brick/water work and mutate Scene+9d8.
// Their f7c50 callers ignore RAX. Geometry/shader sampling stages remain per-eye.
namespace worldSharedStages {
using Stage=void (__fastcall*)(void*,void*,void*);
struct Entry {uintptr_t rva,caller;const char* pattern;Stage original=nullptr;bool enabled=false;};
inline std::array<Entry,2> entries{{
    {0x1fdf60,0x12c112,"4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08"},
    {0x201c20,0x12c24d,"48 8B C4 48 89 50 10 55 53 56 41 54"}
}};
struct Completed {uintptr_t object=0;uint64_t serial=0;bool valid=false;};
inline thread_local std::array<Completed,2> completed{};
inline bool once=false;
// View that executes the shared updates of a frame: -1 = the original View; 0 = left eye on frames where
// the original View's world render is skipped (desktop skip, 2026-10-06), and since 2026-10-17 also on a refresh frame
// (the far terrain's LOD changed shape with the desktop camera's bricks, headset report 10). Other Views skip with proof;
// the refresh frame's original View, drawn before the eye, skips the terrain bricks without one (deferred).
inline thread_local int ownerEye=-1;
// View that runs the effects (VFX) of a frame - their simulation, lights, decals, fog instances, particles, models -
// which the other eye borrows: -1 = the original View. The same eye as ownerEye on a desktop-skip frame, and since
// 2026-10-15 also on a desktop-refresh frame (one frame in thirty), where the original View is drawn first, without
// its VfxView, for the shared updates only. Before, both eyes borrowed the original View's lights on that frame, and
// with them its point-light shadow atlas, which is drawn on that frame only (headset run 9, user: "outdoors, many places
// flash as black shadows about twice a second"; "the lights flicker, the lantern of the next room for some runs now").
inline thread_local int vfxOwnerEye=-1;
// Diagnostic only (2026-10-16, the grass that grows inside the player's buildings in the non-owner eye):
// EVR_EYE_RUNS_SHARED=<mask> lets the other eye run these shared updates as the game does instead of skipping them -
// 1: 0x1fdc70, 2: 0x201930, 4: the terrain bricks 0x5a8c0 (which can overflow the bricks' arena while loading).
inline const unsigned eyeRunsShared=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_RUNS_SHARED",v,sizeof v);return n&&n<sizeof v?unsigned(std::strtoul(v,nullptr,10)):0u;}();
struct Context {uintptr_t shared=0,terrain=0;uint64_t serial=0;int eye=-1;std::array<unsigned,2> calls{},skipped{};bool rejected=false;
    unsigned terrainCalls=0,terrainSkipped=0;bool terrainUnproven=false;};
inline thread_local Context* current=nullptr;
// 1.0 (2026-10-16, the headset crashed each time on entering the world, package-20261016-124304): the terrain bricks'
// update 0x5a8c0 (scene+0xa80; called from each View's world render at 0x12c189 with the graph, the bricks and that View's
// camera) requests the bricks around the camera and takes the output of each from the bricks' frame arena
// ([bricks+0x10300], 0x100b720: 8 MB, 2000 bytes a brick, null when full), which the job (0x58b00 -> 0x58540 -> 0x1094ee0)
// writes without a check. Run for the original View and both eyes, the first frames after loading took three Views' worth
// and the job wrote through null (enshrouded.exe+0x13aa839; dumps enshrouded_20261016_034944 / _035041 / _041052 /
// _041809). Ordinary Mock runs reach the eyes only after the world has settled, with nothing left to request. Like the
// water updates above, it runs for the shared-update owner only; an eye skips it when the owner ran it on the same
// serial, and otherwise runs it as the game does. The next call, 0x57080 (the bricks' atlas imported into the View's
// graph, for its own terrain), stays per View.
namespace terrain {
constexpr uintptr_t rva=0x5a8f0,caller=0x12c1ce;
constexpr const char* pattern="4C 89 44 24 18 48 89 4C 24 08 55 53 56 57 41 55 41 56 41 57 48 8D AC 24 60 FD FD FF B8 A0 03 02 00";
inline Stage original=nullptr;inline bool enabled=false;
inline thread_local Completed completed{};
inline std::atomic<uint64_t> ran{0},skipped{0},unproven{0},deferred{0},eyeOwn{0},eyeHeld{0};
// The other eye's own update (2026-10-18; headset reports of 15:00 "the terrain differs between the eyes; fields show it most"
// and the user's field save). Skipped, the other eye drew the terrain without what this update gives the View that calls it:
// on tilled soil the left eye showed the clods and stones in relief, the right eye nearly flat (Mock game-20261018-154607:
// one-eye blocks over the field 45 % against 7 % elsewhere, after the right eye was resampled to the left's view). With the
// right eye running it itself (Mock game-20261018-160647) both eyes show the relief, one-eye blocks 5 % over the field too,
// pairs per second 39.6 against 40.9. So the other eye runs it itself - as the game does for every View - on frames where
// the left eye owns the shared updates (the original View does not run its world: two Views' worth, not three) and the
// world has settled (eyeOwnAllowed, view_resource_probe.cpp: 600 frames after the eyes began and after any jump of the
// camera of 20 m). Run for three Views while the world loaded, the update's jobs overran the bricks' frame arena and wrote
// through null (10-16 dumps; EVR_EYE_RUNS_SHARED=4 in Mock game-20261018-155050). EVR_EYE_TERRAIN=0: skipped as before.
// Off by default since 18:00 the same day: Mock control runs showed no effect (field runs game-20261018-171150 with the
// other eye's own update and -173218 without it: one-eye blocks over the field 5 %, at its end 12-28 % in both); the 45 % of
// the first runs came with pictures taken every 20 frames from frame 500 (game-20261018-154607 without, -172753 with: 45 %
// both). EVR_EYE_TERRAIN=1: on (and the layer enlarges the arena). Not safe: with it on, Mock game-20261018-192327 ended
// in an access violation right after the other eye's second own update (frame 274; two runs before had not). (Those
// "one-eye blocks" turned out to be a measuring fault; the headset's flat soil in the right eye, report 1 of 18:23, was
// not repeated in any Mock run, so whether this would help is not known.)
inline const bool eyeOwnWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_TERRAIN",v,sizeof v);return n==1&&v[0]=='1';}();
inline bool (*eyeOwnAllowed)()=nullptr;
// The bricks' frame arena (2026-10-18). With the other eye running its own update, Mock game-20261018-161211 crashed 1.5 s
// after it began, while running: the brick job (0x5875b -> 0x1095935) copied a brick's output to the null that the arena gave
// when full (0x13ab449). The arena is one GPU buffer, [bricks+0x10300], made with the brick system (0x4f4fc) at 0x4f845
// "mov qword ptr [rbp-0x19], 0x7d0000" - 4096 bricks of 2000 bytes a frame; its allocator (0x100bd60) bumps an offset and
// gives null past [buffer+0x70]. Made three times as large (Views' worth for the original View and both eyes) before the
// world is made - from the probe's first stage, before the world loads (view_resource_probe.cpp, after MH_Initialize; Mock
// game-20261018-161836: patched in the world stage, the world's arena had been made 67 s before at its native size); the other
// eye runs its own update only when the arena of this world is that large (roomy).
namespace arena {
constexpr uintptr_t site=0x4f845;constexpr uint32_t nativeBytes=0x7d0000,bytes=0x1770000;
inline bool patched=false;inline std::atomic<int> roomy{-1};   // -1 not read yet for this bricks object
inline std::atomic<uintptr_t> readFor{0};
inline void install(){
    static const bool wanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_TERRAIN_ARENA",v,sizeof v);return !(n==1&&v[0]=='0');}();
    if(!wanted||!eyeOwnWanted){logFile->event("terrain_arena_left","\"reason\":\"switched off\"");return;}
    const auto at=reinterpret_cast<uint8_t*>(baseAddress+site);const uint8_t expected[8]{0x48,0xc7,0x45,0xe7,0x00,0x00,0x7d,0x00};
    uint8_t enlarged[8];std::memcpy(enlarged,expected,4);std::memcpy(enlarged+4,&bytes,4);
    if(!std::memcmp(at,enlarged,8)){patched=true;logFile->event("terrain_arena_enlarged","\"already\":true");return;}   // the earlier stage did it
    if(std::memcmp(at,expected,8)){logFile->event("terrain_arena_left","\"reason\":\"the creation's bytes differ\"");return;}
    DWORD old=0;if(!VirtualProtect(at+4,4,PAGE_EXECUTE_READWRITE,&old)){logFile->event("terrain_arena_left","\"reason\":\"protect\"");return;}
    std::memcpy(at+4,&bytes,4);DWORD ignored=0;VirtualProtect(at+4,4,old,&ignored);FlushInstructionCache(GetCurrentProcess(),at,8);
    patched=true;logFile->event("terrain_arena_enlarged","\"site\":"+hex(site)+",\"native_bytes\":"+std::to_string(nativeBytes)+",\"bytes\":"+std::to_string(bytes));
}
// Whether this world's arena is the enlarged one: its capacity (+0x70 of the buffer), read once per bricks object.
inline bool large(uintptr_t bricks){
    if(readFor.load()!=bricks){int r=0;uint64_t capacity=0;
        try{const auto buffer=read<uintptr_t>(bricks+0x10300);if(buffer){capacity=read<uint64_t>(buffer+0x70);r=capacity>=uint64_t(bytes)?1:0;}}catch(...){r=0;}
        roomy=r;readFor=bricks;
        logFile->event("terrain_arena_capacity","\"bricks\":"+hex(bricks)+",\"capacity\":"+std::to_string(capacity)+",\"enlarged\":"+(r?"true":"false"));}
    return roomy.load()==1;
}
}
// The brick system's step (0x532b0, called by this update once, at 0x5acb3): "a call takes the finished batch and starts the
// next one" - the brick jobs that fill the frame arena. Run for both eyes, the system stepped twice a frame and a batch's jobs
// wrote to an arena already closed (Mock game-20261018-162655: the same crash with the arena three times as large -
// 0x100bd60 also gives null when the frame's base is gone). So the other eye's own update leaves the step out and takes the
// owner's result of this frame instead (the status byte at +0 and the word at +4 that 0x532b0 returns; the update reads the
// byte at 0x5acb8 for one branch, 0x5af3a); it then builds only its own View's terrain, from the bricks as the owner left them.
namespace step {
constexpr uintptr_t rva=0x532b0,callReturn=0x5acb8;
constexpr const char* pattern="4C 89 44 24 18 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 C8 48 81 EC 38 01 00 00";
using Fn=void*(__fastcall*)(void*,void*,void*);
inline Fn original=nullptr;inline bool enabled=false;
inline thread_local int role=0;   // 1: the owner's update (its result is kept), 2: the other eye's own update (the step is left out)
inline std::atomic<uint64_t> keptSerial{0},lent{0},kept{0};inline std::atomic<uint32_t> keptStatus{0},keptWord{0};
// EVR_BRICK_STEP_LOG=1 (diagnostic, 2026-10-19; buildings seen from above while falling turn see-through for a few
// seconds, fewer since the headset ran at 36 pairs a second instead of 25): what each step returns and what the bricks'
// frame arena holds around it, 30 steps a line - does the brick system run at a limit while the view falls? Pass-through.
inline const bool logWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_BRICK_STEP_LOG",v,sizeof v);return n==1&&v[0]=='1';}();
struct Sample {uint64_t frame=0;uint32_t status=0,word=0,calls=0;std::array<uint64_t,6> before{},after{};};
inline std::array<Sample,30> samples{};inline size_t sampleCount=0;inline uint64_t lines=0;
inline void record(void* bricks,void* result,const std::array<uint64_t,6>& before,uint64_t frame){
    Sample s;s.frame=frame;s.before=before;
    if(result){s.status=*static_cast<uint8_t*>(result);std::memcpy(&s.word,static_cast<uint8_t*>(result)+4,4);}
    try{const auto buffer=read<uintptr_t>(reinterpret_cast<uintptr_t>(bricks)+0x10300);if(buffer)s.after=read<std::array<uint64_t,6>>(buffer+0x58);}catch(...){}
    samples[sampleCount++]=s;
    if(sampleCount<samples.size())return;
    sampleCount=0;
    std::ostringstream f;f<<"\"line\":"<<++lines<<",\"frames\":["<<samples.front().frame<<','<<samples.back().frame<<"],\"steps\":[";
    for(size_t i=0;i<samples.size();++i){const auto& x=samples[i];if(i)f<<',';
        f<<'['<<x.status<<','<<x.word;for(auto v:x.before)f<<','<<v;for(auto v:x.after)f<<','<<v;f<<']';}
    f<<"],\"arena_words_at\":\"buffer+0x58..+0x80 before, then after\"";
    try{logFile->event("brick_step_stats",f.str());}catch(...){}
}
inline void* __fastcall invoke(void* out,void* bricks,void* third){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(caller==baseAddress+callReturn&&role==2&&current&&keptSerial.load()==current->serial){
        auto* o=static_cast<uint8_t*>(out);o[0]=uint8_t(keptStatus.load());const uint32_t w=keptWord.load();std::memcpy(o+4,&w,4);++lent;return out;}
    std::array<uint64_t,6> before{};
    if(logWanted&&caller==baseAddress+callReturn)try{const auto buffer=read<uintptr_t>(reinterpret_cast<uintptr_t>(bricks)+0x10300);if(buffer)before=read<std::array<uint64_t,6>>(buffer+0x58);}catch(...){}
    void* r=original(out,bricks,third);
    if(logWanted&&caller==baseAddress+callReturn)record(bricks,r,before,liveFrame.frame);
    if(caller==baseAddress+callReturn&&role==1&&current&&r){
        uint32_t w=0;std::memcpy(&w,static_cast<uint8_t*>(r)+4,4);keptStatus=*static_cast<uint8_t*>(r);keptWord=w;keptSerial=current->serial;++kept;}
    return r;
}
struct Role {int previous=role;explicit Role(int r){role=r;}~Role(){role=previous;}};
}
inline void __fastcall invoke(void* graph,void* bricks,void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto address=reinterpret_cast<uintptr_t>(bricks);
    const bool observed=current&&current->terrain&&address==current->terrain&&caller==baseAddress+terrain::caller;
    if(observed){
        ++current->terrainCalls;
        bool eyeRunsOwn=false;
        if(current->eye>=0&&current->eye!=ownerEye&&eyeOwnWanted&&ownerEye==0&&current->eye==1&&once){
            // the owner ran it (and stepped the bricks) on this serial, this world's arena is the large one, the world has settled
            eyeRunsOwn=step::enabled&&completed.valid&&completed.object==address&&completed.serial==current->serial&&step::keptSerial.load()==current->serial&&
                arena::large(address)&&eyeOwnAllowed&&eyeOwnAllowed();
            if(!eyeRunsOwn)++eyeHeld;
        }
        if(eyeRunsOwn){const auto k=eyeOwn.fetch_add(1);if(k<3||k%54000==0)logFile->event("terrain_bricks_eye_runs_own","\"eye\":"+std::to_string(current->eye)+",\"frame\":"+std::to_string(liveFrame.frame)+
            ",\"runs\":"+std::to_string(k+1)+",\"held\":"+std::to_string(eyeHeld.load())+",\"steps_lent\":"+std::to_string(step::lent.load())+",\"steps_kept\":"+std::to_string(step::kept.load()));
            const step::Role role(2);original(graph,bricks,parameters);++ran;return;}
        else if(current->eye!=ownerEye&&once&&!(eyeRunsShared&4)){
            if(completed.valid&&completed.object==address&&completed.serial==current->serial){++current->terrainSkipped;++skipped;return;}
            // The refresh frame's original View, when an eye owns the shared updates of that frame (refresh to eye, see
            // ownerEye): the eye, drawn next, requests the bricks around its own camera; the original View's camera
            // (the desktop camera, 1.6 m away) would choose other LODs for one frame in thirty.
            if(current->eye<0&&ownerEye>=0){++current->terrainSkipped;++deferred;return;}
            current->terrainUnproven=true;++unproven;   // the owner did not run it on this serial: the game's own path
        }
    }
    {const step::Role role(observed&&current->eye==ownerEye?1:0);original(graph,bricks,parameters);}++ran;
    if(observed&&current->eye==ownerEye)completed={address,current->serial,true};
}
}
struct Scope {Context* previous=current;explicit Scope(Context* value){current=value;}~Scope(){current=previous;}};
// The terrain's fine displacement radius of the other eye (2026-10-18 21:20; headset reports of the field: the soil's relief
// differs between the eyes - flat in the right eye, worse since 15:40). The headset capture game-20261018-195604 (RenderDoc,
// the VR bug report pressed in front of the soil mound): both eyes draw the mound with the same tessellated terrain draws
// and shaders; their per-View constants (480 bytes) differ (besides the cameras) in one float at 0x114 - left eye 0.0299,
// right eye 0.677. The domain shader adds the fine displacement where (distance from the camera * that)^2 < 1: within 33.5 m
// in the left eye, 1.5 m in the right. Every Mock capture had the same value in both eyes (also after the user's walk to the
// field, game-20261018-203225: 35.4 m), so where the right eye's comes from on the headset is not known; the copies 0x83e10
// and 0x83f30 carry other constants (Mock game-20261018-205612). Those constants are written into a block of the GPU's
// frame buffer taken by 0x49aa0 (the terrain's constants allocator; callers 0x4a303 .. 0x84efa) and filled while the View
// is drawn: the blocks of 480 bytes taken during each eye's drawing are kept, and when the right eye has been drawn, its
// blocks holding 5, 1 / radius, 1023 at 0x110 take the left eye's 1 / radius of the frame (before the GPU reads them).
// EVR_TERRAIN_RADIUS_SHARE=0: as before (the difference only recorded).
namespace detailRadius {
constexpr uintptr_t rva=0x49aa0;
constexpr const char* pattern="4C 89 44 24 18 53 48 81 EC A0 00 00 00 41 0F 10 01 48 8D 44 24 30 48 8B DA";
using Fn=void*(__fastcall*)(void*,void*,uint64_t,void*);
inline Fn original=nullptr;inline bool enabled=false;
inline const bool share=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_TERRAIN_RADIUS_SHARE",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::mutex mutex;inline uint64_t blockSerial=0;inline std::array<std::vector<uintptr_t>,2> blocks,callers;   // callers: the return address into the game (2026-10-19: which function fills the radius)
inline std::atomic<uint64_t> allocations{0},recorded{0},unscoped{0},seen{0},differed{0},lent{0},frames{0};
inline std::atomic<uint64_t> lastSeenSerial{0},lastDiffSerial{0};inline std::atomic<float> lastLeft{0.f},lastRight{0.f};   // for the View dumps (diagnostic)
inline void* __fastcall invoke(void* allocator,void* out,uint64_t size,void* options){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    void* result=original(allocator,out,size,options);
    if(size==0x1e0&&out){++allocations;
        if(current&&(current->eye==0||current->eye==1)){uintptr_t block=0;std::memcpy(&block,static_cast<uint8_t*>(out)+0x20,8);
            if(block){std::lock_guard<std::mutex> lock(mutex);
                if(blockSerial!=current->serial){blockSerial=current->serial;for(int k=0;k<2;k++){blocks[k].clear();callers[k].clear();}}
                if(blocks[size_t(current->eye)].size()<64){blocks[size_t(current->eye)].push_back(block);callers[size_t(current->eye)].push_back(caller);++recorded;}}}
        else ++unscoped;}
    return result;
}
// Where the radius comes from (2026-10-19 02:30, read in the code): the constants' builder 0x13d810 takes 1 / (r + 0.01) with r
// from the getter 0x1de3d0, "movss xmm0, [rcx+0xc444]; ret", of the View's own object at View+0x978 (each View - the
// original and both eyes - has its own: capture-view dumps of game-20261019-010227). The object's update 0x1e2d20 (called
// by each View's world render at 0x12a3ba) moves +0xc440 to +0xc444, and during the frame +0xc440 becomes the least of a
// distance over its items (0x1e3f63 minss) - the radius stops where the View's detail is not ready. The right eye's object
// does not request its bricks itself (the shared update 0x5a8c0 is the left eye's), so its radius is short and jumps
// (headset 2026-10-18: left 33.5 m, right 1.5 m; 2026-10-19: right 2-28 m against left 10-34 m). The getter is also read
// at 0x131bbd (another terrain pass of the View render). Here the right eye's reads get the left eye's value of the same
// frame, before any constant is built - the patch of the blocks after the eye (finishEye) then finds nothing to lend.
namespace getter {
constexpr uintptr_t rva=0x1de3d0;constexpr const char* pattern="F3 0F 10 81 44 C4 00 00 C3";
using Fn=float(__fastcall*)(const void*);inline Fn original=nullptr;inline bool enabled=false;
inline std::atomic<uintptr_t> leftObject{0};inline std::atomic<uint64_t> leftSerial{0},own{0},lentCalls{0},lastLentSerial{0};
inline float __fastcall invoke(const void* object){
    const float value=original(object);
    if(!share||!current)return value;
    if(current->eye==0){leftObject.store(reinterpret_cast<uintptr_t>(object));leftSerial.store(current->serial);++own;return value;}
    if(current->eye==1&&leftSerial.load()==current->serial){const auto left=leftObject.load();
        if(left&&left!=reinterpret_cast<uintptr_t>(object)){const float l=*reinterpret_cast<const volatile float*>(left+0xc444);
            if(std::isfinite(l)&&l>0.f){++lentCalls;lastLentSerial.store(current->serial);return l;}}}
    return value;
}
}
// The View streaming object's update 0x1e2d20 (rcx the object, called at 0x12a3ba by each View's world render: "mov rcx,
// [rdi+0x978]; call"). It takes the View's camera, rebuilds the object's item lists and may start the object's job (+0x368
// set: the later updates leave early until it is done). While a refresh's original View draws with the left eye's object
// (view_resource_probe.cpp StreamingLend), skip says to leave the object as the eyes made it (2026-10-19 experiment,
// EVR_SKIP_ORIGINAL_STREAMING_UPDATE=1: the right eye lost bricks on every refresh frame, Mock game-20261019-061432).
namespace streamingUpdate {
constexpr uintptr_t rva=0x1e2d20,callReturn=0x12a3bf;constexpr const char* pattern="48 89 5C 24 10 4C 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57";
using Fn=void(__fastcall*)(void*,void*,void*);inline Fn original=nullptr;inline bool enabled=false;
inline const bool wanted=[]{char v[4]{};return GetEnvironmentVariableA("EVR_SKIP_ORIGINAL_STREAMING_UPDATE",v,sizeof v)==1&&v[0]=='1';}();
inline thread_local bool skip=false;inline std::atomic<uint64_t> skipped{0},passed{0};
inline void __fastcall invoke(void* object,void* a,void* b){if(skip){++skipped;return;}++passed;original(object,a,b);}
}
// 5, 1 / radius, 1023 at 0x110 of a block (the GPU's write-combined memory: read once, 12 bytes)
inline bool radiusOf(uintptr_t block,float& inv){uint32_t u[3]{};SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(block+0x110),u,12,&got)||got!=12)return false;
    std::memcpy(&inv,&u[1],4);return u[0]==5&&u[2]==1023&&std::isfinite(inv)&&inv>0.f&&inv<100.f;}
// After an eye has been drawn (hooks/view_resource_probe.cpp, calibration_eye_recorded).
inline void finishEye(int eye,uint64_t serial){
    if(!enabled||eye!=1)return;
    std::array<std::vector<uintptr_t>,2> b,c;{std::lock_guard<std::mutex> lock(mutex);if(blockSerial!=serial)return;b=blocks;c=callers;}
    const auto frame=++frames;
    // Which of the game's functions asked for the blocks that carry the radius, and for the others (each caller once).
    if(frame<=3000){static std::set<uintptr_t> logged,loggedRadius;for(int e=0;e<2;e++)for(size_t i=0;i<b[e].size()&&i<c[e].size();i++){
        float r=0;const bool radius=radiusOf(b[e][i],r);auto& set=radius?loggedRadius:logged;
        if(set.size()>=32||set.count(c[e][i]))continue;set.insert(c[e][i]);
        try{logFile->event("terrain_constants_caller","\"eye\":"+std::to_string(e)+",\"caller\":"+hex(c[e][i]-baseAddress)+",\"index\":"+std::to_string(i)+",\"blocks\":"+std::to_string(b[e].size())+",\"radius\":"+(radius?"true":"false")+",\"inv\":"+std::to_string(radius?r:0.f));}catch(...){}}}
    float left=0;bool have=false;for(const auto p:b[0])if(radiusOf(p,left)){have=true;break;}
    unsigned rightBlocks=0;float firstRight=0;
    if(have)for(const auto p:b[1]){float r=0;if(!radiusOf(p,r))continue;++seen;if(!rightBlocks)firstRight=r;++rightBlocks;
        lastSeenSerial=serial;if(std::fabs(left-r)>std::fabs(left)*1e-3f){lastDiffSerial=serial;lastLeft=left;lastRight=r;}
        if(std::fabs(left-r)>std::fabs(left)*1e-3f){const auto n=++differed;
            if(n<=12||n%900==0)try{logFile->event("terrain_detail_radius_differs","\"serial\":"+std::to_string(serial)+",\"left_inv\":"+std::to_string(left)+
                ",\"right_inv\":"+std::to_string(r)+",\"left_radius_m\":"+std::to_string(1.f/left)+",\"right_radius_m\":"+std::to_string(1.f/r)+
                ",\"times\":"+std::to_string(n)+",\"lent\":"+(share?"true":"false"));}catch(...){}
            if(share){*reinterpret_cast<volatile float*>(p+0x114)=left;++lent;}}
        // EVR_TERRAIN_RADIUS_TEST=<1 / radius> (diagnostic): every right eye block takes this value - a capture shows whether a
        // value written here reaches the GPU (2026-10-18: RenderDoc captures of this frame never showed the difference itself).
        static const float test=[]{char v[24]{};return GetEnvironmentVariableA("EVR_TERRAIN_RADIUS_TEST",v,sizeof v)?float(std::atof(v)):0.f;}();
        if(test>0.f)*reinterpret_cast<volatile float*>(p+0x114)=test;}
    if(frame<=3||frame%900==0)try{logFile->event("terrain_detail_radius_frame","\"serial\":"+std::to_string(serial)+",\"left_blocks\":"+std::to_string(b[0].size())+
        ",\"right_blocks\":"+std::to_string(b[1].size())+",\"left_inv\":"+std::to_string(have?left:0.f)+",\"right_inv\":"+std::to_string(firstRight)+",\"right_with_radius\":"+std::to_string(rightBlocks)+
        ",\"allocations\":"+std::to_string(allocations.load())+",\"unscoped\":"+std::to_string(unscoped.load())+",\"differed\":"+std::to_string(differed.load())+",\"lent\":"+std::to_string(lent.load())+",\"getter_own\":"+std::to_string(getter::own.load())+",\"getter_lent\":"+std::to_string(getter::lentCalls.load()));}catch(...){}
}
}
template<size_t Index> void __fastcall invoke(void* graph,void* object,void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto address=reinterpret_cast<uintptr_t>(object);auto& entry=entries[Index];
    const bool observed=current&&address==current->shared&&caller==baseAddress+entry.caller;
    if(observed){
        ++current->calls[Index];
        if(current->eye!=ownerEye&&once&&!(eyeRunsShared&(1u<<Index))){
            const auto& proof=completed[Index];
            if(!proof.valid||proof.object!=address||proof.serial!=current->serial)current->rejected=true;
            ++current->skipped[Index];return;
        }
    }
    entry.original(graph,object,parameters);
    if(observed&&current->eye==ownerEye)completed[Index]={address,current->serial,true};
}
inline void write(const Context& scope){
    for(size_t i=0;i<entries.size();++i)if(scope.calls[i])
        logFile->event("world_shared_render_stage","\"function\":"+hex(entries[i].rva)+",\"serial\":"+std::to_string(scope.serial)+
            ",\"eye\":"+std::to_string(scope.eye)+",\"object\":"+hex(scope.shared)+",\"calls\":"+std::to_string(scope.calls[i])+
            ",\"skipped\":"+std::to_string(scope.skipped[i])+",\"proof_rejected\":"+(scope.rejected?"true":"false"));
    // the terrain bricks: the first frames, then one in 600 and every eye that ran it without the owner's proof
    if(scope.terrainCalls){static std::atomic<uint64_t> written{0};const auto n=++written;
        if(n<=12||n%600==0||scope.terrainUnproven)
            logFile->event("world_shared_terrain_bricks","\"serial\":"+std::to_string(scope.serial)+",\"eye\":"+std::to_string(scope.eye)+
                ",\"object\":"+hex(scope.terrain)+",\"calls\":"+std::to_string(scope.terrainCalls)+",\"skipped\":"+std::to_string(scope.terrainSkipped)+
                ",\"unproven\":"+(scope.terrainUnproven?"true":"false")+",\"ran_total\":"+std::to_string(terrain::ran.load())+
                ",\"skipped_total\":"+std::to_string(terrain::skipped.load())+",\"unproven_total\":"+std::to_string(terrain::unproven.load()));}
}
inline void install(){
    const std::array<void*,2> hooks{reinterpret_cast<void*>(&invoke<0>),reinterpret_cast<void*>(&invoke<1>)};
    for(size_t i=0;i<entries.size();++i){auto& e=entries[i];const auto p=parsePattern(e.pattern);const auto target=baseAddress+e.rva,call=baseAddress+e.caller-5;
        if(scan(reinterpret_cast<const uint8_t*>(target),p.size(),p)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||
            uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)throw std::runtime_error("shared world stage signature/caller mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(target),hooks[i],reinterpret_cast<void**>(&e.original))!=MH_OK||
            MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("shared world stage hook failed");
        e.enabled=true;
    }
    {const auto p=parsePattern(terrain::pattern);const auto target=baseAddress+terrain::rva,call=baseAddress+terrain::caller-5;
        if(scan(reinterpret_cast<const uint8_t*>(target),p.size(),p)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||
            uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target||read<uint32_t>(call-7)!=0x80908b48u||read<uint32_t>(call-4)!=0xa80u)   // mov rdx,[rax+0xa80]
            throw std::runtime_error("shared terrain bricks update signature/caller mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&terrain::invoke),reinterpret_cast<void**>(&terrain::original))!=MH_OK||
            MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("shared terrain bricks update hook failed");
        terrain::enabled=true;
        terrain::arena::install();
        // the step: left out of the other eye's own update (optional - without it the other eye skips the update as before);
        // only with that update (EVR_EYE_TERRAIN=1) - otherwise it only passed calls through (2026-10-18 19:30)
        if(terrain::eyeOwnWanted||terrain::step::logWanted)try{const auto sp=parsePattern(terrain::step::pattern);const auto st=baseAddress+terrain::step::rva,sc=baseAddress+terrain::step::callReturn-5;
            if(scan(reinterpret_cast<const uint8_t*>(st),sp.size(),sp)!=std::vector<size_t>{0}||read<uint8_t>(sc)!=0xe8||uintptr_t(int64_t(sc+5)+read<int32_t>(sc+1))!=st)
                throw std::runtime_error("brick step signature/caller mismatch");
            if(MH_CreateHook(reinterpret_cast<void*>(st),reinterpret_cast<void*>(&terrain::step::invoke),reinterpret_cast<void**>(&terrain::step::original))!=MH_OK||
                MH_EnableHook(reinterpret_cast<void*>(st))!=MH_OK)throw std::runtime_error("brick step hook failed");
            terrain::step::enabled=true;logFile->event("terrain_brick_step_hook_enabled","\"rva\":"+hex(terrain::step::rva)+",\"caller_return\":"+hex(terrain::step::callReturn));}
        catch(const std::exception& e){logFile->event("terrain_brick_step_hook_skipped","\"reason\":"+jsonString(e.what()));}
        logFile->event("world_shared_terrain_bricks_hook_enabled","\"rva\":"+hex(terrain::rva)+",\"caller\":"+hex(terrain::caller)+",\"owner_only\":true");}
    if(eyeRunsShared)logFile->event("world_shared_eye_runs_diagnostic","\"mask\":"+std::to_string(eyeRunsShared)+",\"diagnostic\":true");
    // the terrain constants' allocator (optional: without it the right eye keeps its own fine displacement radius)
    try{const auto p=parsePattern(detailRadius::pattern);const auto target=baseAddress+detailRadius::rva;
        if(scan(reinterpret_cast<const uint8_t*>(target),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("terrain constants allocator signature mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&detailRadius::invoke),reinterpret_cast<void**>(&detailRadius::original))!=MH_OK||
            MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("terrain constants allocator hook failed");
        detailRadius::enabled=true;logFile->event("terrain_detail_radius_hook_enabled","\"rva\":"+hex(detailRadius::rva)+",\"share\":"+(detailRadius::share?"true":"false"));}
    catch(const std::exception& e){logFile->event("terrain_detail_radius_hook_skipped","\"reason\":"+jsonString(e.what()));}
    // the radius' getter (optional, with the share): the right eye reads the left eye's radius of the same frame
    if(detailRadius::share)try{const auto p=parsePattern(detailRadius::getter::pattern);const auto target=baseAddress+detailRadius::getter::rva;
        if(scan(reinterpret_cast<const uint8_t*>(target),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("terrain radius getter signature mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&detailRadius::getter::invoke),reinterpret_cast<void**>(&detailRadius::getter::original))!=MH_OK||
            MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("terrain radius getter hook failed");
        detailRadius::getter::enabled=true;logFile->event("terrain_radius_getter_hook_enabled","\"rva\":"+hex(detailRadius::getter::rva)+",\"right_eye_reads_the_left\":true");}
    catch(const std::exception& e){logFile->event("terrain_radius_getter_hook_skipped","\"reason\":"+jsonString(e.what()));}
    if(detailRadius::streamingUpdate::wanted)try{const auto p=parsePattern(detailRadius::streamingUpdate::pattern);const auto target=baseAddress+detailRadius::streamingUpdate::rva,call=baseAddress+detailRadius::streamingUpdate::callReturn-5;
        if(scan(reinterpret_cast<const uint8_t*>(target),p.size(),p)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target||
           read<uint32_t>(call-7)!=0x788f8b48u||read<uint16_t>(call-3)!=0x0009u)   // mov rcx,[rdi+0x978]
            throw std::runtime_error("streaming update signature/caller mismatch");
        if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&detailRadius::streamingUpdate::invoke),reinterpret_cast<void**>(&detailRadius::streamingUpdate::original))!=MH_OK||
            MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("streaming update hook failed");
        detailRadius::streamingUpdate::enabled=true;logFile->event("streaming_update_hook_enabled","\"rva\":"+hex(detailRadius::streamingUpdate::rva)+",\"original_view_skips_when_lent\":true");}
    catch(const std::exception& e){logFile->event("streaming_update_hook_skipped","\"reason\":"+jsonString(e.what()));}
}
inline void disable() noexcept {
    for(auto& e:entries)if(e.enabled){const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+e.rva));if(status==MH_OK)e.enabled=false;
        try{logFile->event("world_shared_stage_hook_removed","\"function\":"+hex(e.rva)+",\"status\":"+std::to_string(status));}catch(...){}
    }
    if(terrain::step::enabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+terrain::step::rva))==MH_OK)terrain::step::enabled=false;
    if(detailRadius::streamingUpdate::enabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+detailRadius::streamingUpdate::rva))==MH_OK){detailRadius::streamingUpdate::enabled=false;
        try{logFile->event("streaming_update_hook_removed","\"skipped\":"+std::to_string(detailRadius::streamingUpdate::skipped.load())+",\"passed\":"+std::to_string(detailRadius::streamingUpdate::passed.load()));}catch(...){}}
    if(detailRadius::getter::enabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+detailRadius::getter::rva))==MH_OK){detailRadius::getter::enabled=false;
        try{logFile->event("terrain_radius_getter_hook_removed","\"own\":"+std::to_string(detailRadius::getter::own.load())+",\"lent\":"+std::to_string(detailRadius::getter::lentCalls.load()));}catch(...){}}
    if(detailRadius::enabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+detailRadius::rva))==MH_OK){detailRadius::enabled=false;
        try{logFile->event("terrain_detail_radius_hook_removed","\"allocations\":"+std::to_string(detailRadius::allocations.load())+",\"seen\":"+std::to_string(detailRadius::seen.load())+",\"differed\":"+std::to_string(detailRadius::differed.load())+
            ",\"lent\":"+std::to_string(detailRadius::lent.load()));}catch(...){}}
    if(terrain::enabled){const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+terrain::rva));if(status==MH_OK)terrain::enabled=false;
        try{logFile->event("world_shared_terrain_bricks_hook_removed","\"function\":"+hex(terrain::rva)+",\"status\":"+std::to_string(status)+
            ",\"ran\":"+std::to_string(terrain::ran.load())+",\"skipped\":"+std::to_string(terrain::skipped.load())+",\"unproven\":"+std::to_string(terrain::unproven.load())+",\"deferred_to_the_eye\":"+std::to_string(terrain::deferred.load()));}catch(...){}
    }
}
}
