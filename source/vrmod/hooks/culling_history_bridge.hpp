// Native graph-owned visibility history. Shared model/mesh tables are borrowed.
// 1.0 (2026-10-16): the instance owner is scene+0x948 (+0x900; its layout is unchanged), the graph's buffer arrays
// moved +0x158 (objects +0x268, capacity +0x270, records +0x280, free count +0x2b0), the View's culling state +0x9c0 (+0x750).
// All changes are scoped to private culling descriptor outputs or owned buffers.
namespace cullingHistory {
using State=PairResources::CullingHistory;
inline bool requested=false;
inline bool fillOnly=false; // Diagnostic ablation: keep every native descriptor.
inline thread_local PairResources* current=nullptr;
inline thread_local int currentEye=-1;
inline thread_local uintptr_t prepareGraph=0;
inline thread_local unsigned prepareGets=0;
struct Scope {
    PairResources* previous=current;int previousEye=currentEye;
    Scope(PairResources* pair,int eye){current=pair;currentEye=eye;}
    ~Scope(){current=previous;currentEye=previousEye;}
};
using Get=void*(__fastcall*)(void*,void*);
using Prepare=void*(__fastcall*)(void*,void*,void*,void*);
using Upload=void(__fastcall*)(void*,void*,void*);
using ResetGenerated=void(__fastcall*)(void*,void*);
inline Get originalGet=nullptr;inline Prepare originalPrepare=nullptr;inline Upload originalUpload=nullptr;
inline ResetGenerated originalResetGenerated=nullptr;
// With vfx_model_bridge.hpp the eye that does not own the VFX culls the VFX models itself, which appends its own
// generated submodels. Then it must not also keep the owner's: both sets together overran what the manager holds
// at a change of weapon (Mock run game135651, "generated instance count owner differs" 19 frames after selecting
// the sword). Asked per eye frame; true: that eye resets the count as the game does and makes its own.
inline bool (*eyeRegeneratesModels)()=nullptr;
// 2026-10-13: that reset also takes away what the eye has from the owner besides the models - the flying rocks of
// an explosion are generated submodels of the owner's simulation, and with the reset they showed in the owner's
// eye only (headset run game-20261013-105615 "the debris shows in one eye only"; Mock runs game-20261013-131712
// with -ShareVfxModels: left eye only, game-20261013-133942 without: both eyes). So the eye that culls its own
// models keeps the owner's generated parts as well - no reset, its own are appended (Mock runs game-20261013-134620:
// the rocks in both eyes; game-20261013-134924: the pickaxe's aim circle once in each eye). Where the manager has
// made a new count buffer since the owner's reset, that frame is reset as before (nothing is borrowed from a buffer
// the owner did not write). EVR_VFX_MODEL_KEEP_GENERATED=0 goes back to the reset in every such frame.
inline int keepGenerated=-1;
inline std::atomic<unsigned> keptGenerated{0},keepFallbacks{0};
struct Hook {uintptr_t rva,caller;const char* pattern;bool enabled=false;};
inline std::array<Hook,4> hooks{{
    {0xaab60,0xad3db,"48 89 5C 24 08 57 48 83 EC 40 48 8B DA 48 8B F9 48 83"},
    {0xad0f0,0x12ede6,"4C 89 4C 24 20 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 68 F5 FE FF B8 98 0B 01 00 E8 ? ? ? ? 48 2B"},
    {0xaaed0,0x12a50f,"40 55 53 57 41 55 41 56 41 57 48 8D 6C 24 A8 48 81 EC"},
    {0xaacc0,0x12a523,"48 83 EC 38 48 83 C2 38 C7 44 24 20 00 00 00 00 41 B9 04 00 00 00 45 33 C0"}
}};
inline uintptr_t address(const std::array<uint8_t,16>& h){return read<uintptr_t>(reinterpret_cast<uintptr_t>(h.data()));}
inline uint16_t index(const std::array<uint8_t,16>& h){return read<uint16_t>(reinterpret_cast<uintptr_t>(h.data())+8);}
inline uintptr_t object(uintptr_t graph,const std::array<uint8_t,16>& h){
    if(address(h)!=graph||index(h)>=read<uint64_t>(graph+0x270))throw std::runtime_error("culling buffer outside native graph");
    return read<uintptr_t>(read<uintptr_t>(graph+0x268)+uintptr_t(index(h))*8);
}
inline void failure(State& s,const char* reason) noexcept {
    if(!s.failed)try{logFile->event("culling_history_failure","\"reason\":"+jsonString(reason));}catch(...){}
    s.failed=true;
}
inline void release(State& s){
    using Free=void(__fastcall*)(void*,void*);
    if(!s.graph)return;
    for(int eye=1;eye>=0;--eye)if(address(s.handles[eye])){
        const auto id=index(s.handles[eye]);object(s.graph,s.handles[eye]);
        reinterpret_cast<Free>(baseAddress+0x10144e0)(reinterpret_cast<void*>(s.graph),s.handles[eye].data());
        if(address(s.handles[eye]))throw std::runtime_error("native culling buffer handle not cleared");
        logFile->event("culling_history_released","\"eye\":"+std::to_string(eye)+",\"index\":"+std::to_string(id)+
            ",\"substitutions\":"+std::to_string(s.substitutions[eye])+",\"reset_commands\":"+std::to_string(s.resets[eye]));
    }
    if(read<std::array<uint8_t,16>>(s.manager+0x28)!=s.source)throw std::runtime_error("original culling buffer handle changed");
    if(s.generatedResets||s.generatedBorrows)
        logFile->event("generated_instance_count_restored","\"resets\":"+std::to_string(s.generatedResets)+
            ",\"borrows\":"+std::to_string(s.generatedBorrows)+",\"audited_eyes\":"+std::to_string(s.generatedAuditedEyes)+
            ",\"calls\":["+std::to_string(s.generatedCalls[0])+","+std::to_string(s.generatedCalls[1])+","+std::to_string(s.generatedCalls[2])+
            "],\"source_unchanged\":"+(read<std::array<uint8_t,16>>(s.manager+0x38)==s.generatedCountSource?"true":"false")+
            ",\"failed\":"+(s.failed?"true":"false"));
}
inline void create(State& s,uintptr_t scene){
    for(const auto& entry:std::array<std::pair<uintptr_t,const char*>,4>{{
        {0x100f700,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 40 49 8B E8 48 8D"},
        {0x10144e0,"48 89 74 24 18 48 89 7C 24 20 41 57 48 83 EC 20"},
        {0x1013460,"48 8B 02 44 0F B7 42 08 0F 10 02 4D 03 C0"},
        {0x7cb10,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 40 48 8B F2"}}}){
        const auto p=parsePattern(entry.second);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+entry.first),p.size(),p)!=std::vector<size_t>{0})
            throw std::runtime_error("native culling buffer API signature differs");
        logFile->event("culling_history_api_verified","\"rva\":"+hex(entry.first)+",\"pattern\":"+jsonString(entry.second));
    }
    using CreateBuffer=void*(__fastcall*)(void*,void*,const void*);
    s.manager=read<uintptr_t>(scene+0x948);s.thread=GetCurrentThreadId();
    if(!s.manager)throw std::runtime_error("native CPU instance owner missing");
    s.source=read<std::array<uint8_t,16>>(s.manager+0x28);s.graph=address(s.source);
    s.generatedCountSource=read<std::array<uint8_t,16>>(s.manager+0x38);
    object(s.graph,s.generatedCountSource);
    if(read<uint64_t>(read<uintptr_t>(s.graph+0x280)+uintptr_t(index(s.generatedCountSource))*16)!=4)
        throw std::runtime_error("native generated instance count size differs");
    s.slots=read<uint32_t>(s.manager+0xc0);
    if(!s.slots||s.slots>65536||!object(s.graph,s.source)||read<uint64_t>(s.graph+0x2b0)<4)
        throw std::runtime_error("unsupported native culling history capacity");
    const auto record=read<uintptr_t>(s.graph+0x280)+uintptr_t(index(s.source))*16;
    const auto bytes=read<uint64_t>(record);const auto sourceFlags=read<uint32_t>(record+8);
    if(bytes!=uint64_t(s.slots)*4||sourceFlags!=0x10)throw std::runtime_error("native visibility history descriptor changed");
    // The original history is shader-written only. Our graph fill also needs
    // transfer support. Take the flags of this owner's native instance buffer,
    // which 7cd84 initializes with the same 50860 graph fill API.
    const auto fillSource=read<std::array<uint8_t,16>>(s.manager+8);object(s.graph,fillSource);
    const auto flags=read<uint32_t>(read<uintptr_t>(s.graph+0x280)+uintptr_t(index(fillSource))*16+8);
    if(flags!=0x13||(flags&sourceFlags)!=sourceFlags)throw std::runtime_error("native graph-fill buffer usage differs");
    // +c is zero in native queued creation, not the uninitialized padding in
    // graph descriptor storage. f8dda0 -> dcad50 -> dcad80 allocates/binds memory.
    // f8dbd0 creates an UNBOUND buffer for a later allocation-group binding.
    std::array<uint8_t,32> descriptor{};std::memcpy(descriptor.data(),&bytes,8);std::memcpy(descriptor.data()+8,&flags,4);
    for(unsigned eye=0;eye<2;eye++){
        reinterpret_cast<CreateBuffer>(baseAddress+0x100f700)(s.handles[eye].data(),reinterpret_cast<void*>(s.graph),descriptor.data());
        const auto owned=object(s.graph,s.handles[eye]);
        if(!owned||owned==object(s.graph,s.source)||(eye&&owned==object(s.graph,s.handles[0])))throw std::runtime_error("private culling buffers alias");
        const auto allocation=read<uint32_t>(owned+0x14);
        if(!allocation)throw std::runtime_error("private culling buffer lacks native owned memory allocation");
        if(read<uint64_t>(owned+8)!=bytes||read<uint32_t>(owned+0x10)!=flags||!read<uintptr_t>(owned+0x18))
            throw std::runtime_error("native Vulkan buffer metadata differs");
        const auto d=read<uintptr_t>(s.graph+0x280)+uintptr_t(index(s.handles[eye]))*16;
        if(read<uint64_t>(d)!=bytes||read<uint32_t>(d+8)!=flags)throw std::runtime_error("private culling buffer descriptor differs");
        logFile->event("culling_history_created","\"eye\":"+std::to_string(eye)+",\"manager\":"+hex(s.manager)+",\"graph\":"+hex(s.graph)+
            ",\"index\":"+std::to_string(index(s.handles[eye]))+",\"source_index\":"+std::to_string(index(s.source))+
            ",\"object\":"+hex(owned)+",\"source_object\":"+hex(object(s.graph,s.source))+",\"bytes\":"+std::to_string(bytes)+",\"usage\":"+std::to_string(flags)+",\"source_usage\":"+std::to_string(sourceFlags)+",\"owned_allocation\":"+std::to_string(allocation)+
            ",\"vk_buffer\":"+hex(read<uintptr_t>(owned+0x18))+",\"source_vk_buffer\":"+hex(read<uintptr_t>(object(s.graph,s.source)+0x18))+
            ",\"memory_offset\":"+std::to_string(read<uint64_t>(owned+0x20)));
    }
}
inline void fill(State& s,unsigned eye,uintptr_t graph,uint32_t first,uint32_t count){
    if(!graph||!count||uint64_t(first)+count>s.slots)throw std::runtime_error("culling history reset outside allocation");
    using Fill=void(__fastcall*)(void*,const void*,uint64_t,uint64_t,uint32_t);
    reinterpret_cast<Fill>(baseAddress+0x7cb10)(reinterpret_cast<void*>(graph),s.handles[eye].data(),uint64_t(first)*4,uint64_t(count)*4,0xffffffffu);
    ++s.resets[eye];
}
void* __fastcall get(void* output,void* manager){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());auto result=originalGet(output,manager);const ObserverLastError preserve;
    if(!requested||!current||currentEye<0||!prepareGraph||caller!=baseAddress+hooks[0].caller)return result;
    auto& s=current->history;
    try{
        if(s.failed||currentEye>1||s.thread!=GetCurrentThreadId()||reinterpret_cast<uintptr_t>(manager)!=s.manager)
            throw std::runtime_error("private culling descriptor scope differs");
        const auto originalBytes=read<std::array<uint8_t,200>>(reinterpret_cast<uintptr_t>(output));
        if(read<std::array<uint8_t,16>>(reinterpret_cast<uintptr_t>(output)+0x80)!=s.source||
           read<uint64_t>(reinterpret_cast<uintptr_t>(output)+0x90)!=0||read<uint64_t>(reinterpret_cast<uintptr_t>(output)+0x98)!=uint64_t(s.slots)*4)
            throw std::runtime_error("native culling output is not expected visibility history");
        std::array<uint8_t,32> replacement{};
        reinterpret_cast<Get>(baseAddress+0x1013460)(replacement.data(),s.handles[currentEye].data());
        if(!s.initialized[currentEye]){fill(s,unsigned(currentEye),prepareGraph,0,s.slots);s.initialized[currentEye]=true;}
        if(fillOnly){++prepareGets;
            if(!s.fillOnlyObserved[currentEye]){s.fillOnlyObserved[currentEye]=true;
                logFile->event("culling_history_fill_only","\"eye\":"+std::to_string(currentEye)+",\"descriptor_unchanged\":true");}
            return result;}
        std::memcpy(static_cast<uint8_t*>(output)+0x80,replacement.data(),32);
        auto expected=originalBytes;std::memcpy(expected.data()+0x80,replacement.data(),32);
        if(read<std::array<uint8_t,200>>(reinterpret_cast<uintptr_t>(output))!=expected||read<std::array<uint8_t,16>>(s.manager+0x28)!=s.source)
            throw std::runtime_error("culling descriptor changed outside private history range");
        ++s.substitutions[currentEye];++prepareGets;
        if(s.substitutions[currentEye]<=3||current->pairedFrames+1>=calibrationFrames)
            logFile->event("culling_history_substituted","\"eye\":"+std::to_string(currentEye)+",\"serial\":"+std::to_string(current->serial)+
                ",\"index\":"+std::to_string(index(s.handles[currentEye]))+",\"source_unchanged\":true,\"other_descriptors_unchanged\":true");
    }catch(const std::exception& e){failure(s,e.what());}
    return result;
}
void* __fastcall prepare(void* output,void* graph,void* state,void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!requested||!current||currentEye<0||caller!=baseAddress+hooks[1].caller)return originalPrepare(output,graph,state,parameters);
    auto& s=current->history;const auto savedGraph=prepareGraph;const auto savedGets=prepareGets;
    prepareGraph=reinterpret_cast<uintptr_t>(graph);prepareGets=0;
    {const ObserverLastError preserve;try{if(currentEye>1||reinterpret_cast<uintptr_t>(state)!=read<uintptr_t>(current->created[currentEye]+0x9c0))failure(s,"culling state not private View state");}
    catch(const std::exception& e){failure(s,e.what());}}
    auto result=originalPrepare(output,graph,state,parameters);const ObserverLastError preserve;
    if(prepareGets!=1)failure(s,"culling resource substitution count differs");
    prepareGraph=savedGraph;prepareGets=savedGets;return result;
}
void __fastcall upload(void* graph,void* renderer,void* manager){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto* s=requested&&current&&reinterpret_cast<uintptr_t>(manager)==current->history.manager&&caller==baseAddress+hooks[2].caller?&current->history:nullptr;
    std::vector<culling::ResetRange> ranges;
    {const ObserverLastError preserve;if(s&&!s->failed)try{
        if(s->thread!=GetCurrentThreadId())throw std::runtime_error("culling upload thread differs");
        const auto n=read<uint64_t>(s->manager+0xa0),capacity=read<uint64_t>(s->manager+0xa8);
        if(n>65536||n>capacity||n!=read<uint64_t>(s->manager+0x78))throw std::runtime_error("native instance upload counts differ");
        std::vector<uint32_t> values(size_t(n),0);if(n){SIZE_T copied=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(read<uintptr_t>(s->manager+0x98)),values.data(),size_t(n)*4,&copied)||copied!=n*4)
                throw std::runtime_error("native instance updates unreadable");}
        ranges=culling::resetRanges(values,s->slots);
    }catch(const std::exception& e){failure(*s,e.what());}}
    originalUpload(graph,renderer,manager);const ObserverLastError preserve;
    if(s&&!s->failed)try{
        for(unsigned eye=0;eye<2;eye++)if(s->initialized[eye])for(const auto& r:ranges)fill(*s,eye,reinterpret_cast<uintptr_t>(graph),r.first,r.count);
        if(!ranges.empty())logFile->event("culling_history_instance_resets","\"serial\":"+std::to_string(worldSharedStages::current?worldSharedStages::current->serial:current->serial)+",\"source_eye\":"+std::to_string(currentEye)+
            ",\"ranges\":"+std::to_string(ranges.size())+",\"value\":4294967295");
    }catch(const std::exception& e){failure(*s,e.what());}
}
// GPU-generated submodels are appended by the shared VFX owner. The native
// 7caa0 call clears only their four-byte count; repeating it for a borrowing
// eye discards every generated part, even though its instance record remains.
// Keep the owner's reset, and require a same-frame/owner proof for borrowers.
void __fastcall resetGenerated(void* graph,void* manager){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!requested||!current||!worldSharedStages::current||!worldSharedStages::once||
       caller!=baseAddress+hooks[3].caller){originalResetGenerated(graph,manager);return;}
    auto& s=current->history;const auto& frame=*worldSharedStages::current;
    bool borrow=false,foreign=false;
    {const ObserverLastError preserve;try{
        if(s.failed)throw std::runtime_error("generated instance count owner differs: culling history already failed");
        // 2026-10-14: another manager, or this one on another thread, is not this pair's reset to judge. About one
        // fast travel in three did that where the journey begins (Mock sessions game-20261013-200531 and
        // game-20261014-004300: 3 s after the map opened), and the old check ended the whole session there. The game's
        // own reset runs for it untouched: nothing is borrowed, nothing is counted, and what it was is put on record.
        if(s.thread!=GetCurrentThreadId()||reinterpret_cast<uintptr_t>(manager)!=s.manager){
            foreign=true;static std::atomic<unsigned> lines{0};
            if(lines.fetch_add(1)<60){uintptr_t now=0;try{now=read<uintptr_t>(current->scene+0x948);}catch(...){}
                logFile->event("generated_instance_foreign_reset","\"serial\":"+std::to_string(frame.serial)+",\"eye\":"+std::to_string(currentEye)+",\"thread\":"+std::to_string(GetCurrentThreadId())+
                    ",\"pair_thread\":"+std::to_string(s.thread)+",\"manager\":"+hex(reinterpret_cast<uintptr_t>(manager))+",\"pair_manager\":"+hex(s.manager)+",\"scene_manager_now\":"+hex(now)+",\"native_reset\":true");}
        }
        if(!foreign){
        if(currentEye!=frame.eye||currentEye< -1||currentEye>1)throw std::runtime_error("generated instance count owner differs: eye of the frame");
        if(keepGenerated<0){char value[4]{};const auto n=GetEnvironmentVariableA("EVR_VFX_MODEL_KEEP_GENERATED",value,sizeof value);keepGenerated=!(n==1&&value[0]=='0');
            logFile->event("generated_instance_keep_policy",std::string("\"keep_the_owners_parts_in_an_eye_that_culls_its_own_models\":")+(keepGenerated?"true":"false"));}
        // The generated parts are appended by the View that runs the VFX: the VFX owner eye, which since 2026-10-15
        // is the left eye on a desktop-refresh frame as well (view_resource_probe.cpp vfxToEye) while the original
        // View keeps the other shared updates (ownerEye -1). Until 2026-10-15 07:30 the owner here was ownerEye, so on
        // a refresh frame the left eye borrowed the original View's reset and appended its parts to the list of the
        // frame before (headset run 10, user: "a light-only model a little to the right, like an afterimage, on the
        // weapon in the right hand and on roofs"). The original View, neither eye, resets its count natively on such a
        // frame without being the owner of the proof.
        const int owner=worldSharedStages::vfxOwnerEye>=0?worldSharedStages::vfxOwnerEye:worldSharedStages::ownerEye;
        const bool neitherEye=currentEye<0&&owner>=0;
        const bool regenerates=eyeRegeneratesModels&&eyeRegeneratesModels();
        const bool sourceChanged=read<std::array<uint8_t,16>>(s.manager+0x38)!=s.generatedCountSource;
        if(sourceChanged&&!(keepGenerated==1&&regenerates))throw std::runtime_error("generated instance count owner differs: the manager's count buffer changed");
        if(owner< -1||owner>1)throw std::runtime_error("generated instance frame owner differs");   // eye 1 owns the frames it is drawn alone
        borrow=!neitherEye&&currentEye!=owner&&(!regenerates||(keepGenerated==1&&!sourceChanged&&s.generatedResetValid&&s.generatedResetSerial==frame.serial&&s.generatedOwnerEye==owner));
        if(regenerates&&keepGenerated==1&&currentEye!=owner&&!neitherEye){
            if(borrow){if(keptGenerated.fetch_add(1)<3)logFile->event("generated_instance_kept","\"serial\":"+std::to_string(frame.serial)+",\"eye\":"+std::to_string(currentEye)+",\"the_eye_appends_its_own_models\":true");}
            else{if(sourceChanged){s.generatedCountSource=read<std::array<uint8_t,16>>(s.manager+0x38);}
                if(keepFallbacks.fetch_add(1)<20)logFile->event("generated_instance_keep_fallback","\"serial\":"+std::to_string(frame.serial)+",\"eye\":"+std::to_string(currentEye)+",\"count_buffer_changed\":"+(sourceChanged?"true":"false")+",\"reset_as_before\":true");}
        }
        // 1.0 (2026-10-16): the reset moved to the head of the View render (12a37e, before the world readiness check
        // 11dcd0; June f84be ran after it), so the first eye of a pair's first frame reaches it, and that frame's
        // original View was drawn before the pair began (Mock run game-20261016-060537). Nothing has been reset that
        // could be borrowed: that one frame resets as the game does, and is the proof for the other eye.
        if(borrow&&!s.generatedResetValid&&!s.generatedResets&&!s.generatedBorrows){borrow=false;
            logFile->event("generated_instance_first_frame_reset","\"serial\":"+std::to_string(frame.serial)+",\"eye\":"+std::to_string(currentEye)+",\"owner_eye\":"+std::to_string(owner)+",\"native_reset\":true");}
        if(borrow&&(!s.generatedResetValid||s.generatedResetSerial!=frame.serial||s.generatedOwnerEye!=owner))
            throw std::runtime_error("generated instance borrower lacks same-frame reset proof");
        if(!borrow&&!neitherEye){s.generatedResetSerial=frame.serial;s.generatedOwnerEye=owner;s.generatedResetValid=true;}
        ++s.generatedCalls[size_t(currentEye+1)];if(borrow)++s.generatedBorrows;else ++s.generatedResets;
        logFile->event("generated_instance_count_reset","\"serial\":"+std::to_string(frame.serial)+
            ",\"eye\":"+std::to_string(currentEye)+",\"owner_eye\":"+std::to_string(owner)+
            ",\"reset\":"+(borrow?"false":"true")+",\"source_unchanged\":true,\"count_bytes\":4");
        }
    }catch(const std::exception& e){failure(s,e.what());borrow=false;s.generatedResetValid=false;}}
    if(!borrow)originalResetGenerated(graph,manager);
}
inline void install(){
    const std::array<void*,4> functions{reinterpret_cast<void*>(&get),reinterpret_cast<void*>(&prepare),reinterpret_cast<void*>(&upload),reinterpret_cast<void*>(&resetGenerated)};
    const std::array<void**,4> originals{reinterpret_cast<void**>(&originalGet),reinterpret_cast<void**>(&originalPrepare),reinterpret_cast<void**>(&originalUpload),reinterpret_cast<void**>(&originalResetGenerated)};
    for(size_t i=0;i<hooks.size();i++){auto& h=hooks[i];const auto p=parsePattern(h.pattern);const auto call=baseAddress+h.caller-5,target=baseAddress+h.rva;
        if(scan(reinterpret_cast<const uint8_t*>(target),p.size(),p)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
            throw std::runtime_error("culling history hook signature/caller differs");
        if(MH_CreateHook(reinterpret_cast<void*>(target),functions[i],originals[i])!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)
            throw std::runtime_error("culling history hook creation failed");
        h.enabled=true;logFile->event("culling_history_hook_enabled","\"rva\":"+hex(h.rva)+",\"caller\":"+hex(h.caller)+",\"pattern\":"+jsonString(h.pattern));
    }
}
inline void disable(){
    for(auto& h:hooks)if(h.enabled){const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+h.rva));
        if(status!=MH_OK)throw std::runtime_error("culling history hook disable failed");h.enabled=false;
        const auto p=parsePattern(h.pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+h.rva),p.size(),p)==std::vector<size_t>{0};
        logFile->event("culling_history_hook_removed","\"rva\":"+hex(h.rva)+",\"original_bytes_restored\":"+(restored?"true":"false"));
        if(!restored)throw std::runtime_error("culling history hook bytes not restored");
    }
}
}
