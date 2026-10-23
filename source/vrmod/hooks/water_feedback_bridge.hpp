// Opt-in native water feedback aggregation. The original pool and shared
// simulation cadence stay native; only private-eye copy destinations change.
namespace waterFeedback {
struct Result {uintptr_t data=0;uint64_t bytes=0,serial=0;std::array<uint64_t,2> context{};};
static_assert(sizeof(Result)==40);
using Take=bool(__fastcall*)(Result*,void*);
inline Take originalTake=nullptr;
inline bool enabled=false;
inline unsigned staleDropped=0;   // produced serials whose result never came (take(): the renderer was rebuilt on that frame)
inline thread_local WaterFeedbackState* current=nullptr;
struct Scope {WaterFeedbackState* previous=current;explicit Scope(WaterFeedbackState* p){current=p;}~Scope(){current=previous;}};
inline void fail(WaterFeedbackState& state,const char* reason) noexcept {
    state.failed=true;
    resultCode=1;
    try{logFile->event("water_feedback_failure","\"reason\":"+jsonString(reason));}catch(...){}
}
inline water::Packet packet(const Result& result,uint32_t nodes,const std::filesystem::path& path={}){
    if(result.bytes!=water::packetBytes||result.context!=std::array<uint64_t,2>{})throw std::runtime_error("unexpected native water feedback envelope/context");
    std::vector<uint8_t> bytes(water::packetBytes);SIZE_T copied=0;
    if(!result.data||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(result.data),bytes.data(),bytes.size(),&copied)||copied!=bytes.size())
        throw std::runtime_error("unreadable completed native water feedback");
    if(!path.empty()) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(!file)throw std::runtime_error("water packet evidence write failed");
    }
    return water::decode(bytes.data(),bytes.size(),result.serial,nodes);
}
inline uintptr_t route(uintptr_t source,uintptr_t caller) noexcept {
    auto* state=current;const auto* stage=worldSharedStages::current;
    // The shared-update owner (original View, or eye 0 on desktop-skip frames) keeps the native pool.
    if(!privateWaterFeedback||!state||!stage||stage->eye<0||stage->eye==worldSharedStages::ownerEye||caller!=0x1fada2)return source;
    try {
        if(state->failed||stage->eye>1||source!=state->source||stage->shared!=state->owner||GetCurrentThreadId()!=state->thread)
            throw std::runtime_error("water copy scope/owner changed");
        auto& previous=state->lastProduced[size_t(stage->eye)];
        if(!stage->serial||previous>=stage->serial)throw std::runtime_error("duplicate/out-of-order private water producer");
        const auto selected=state->pools[size_t(stage->eye)];
        if(!selected)throw std::runtime_error("private water pool unavailable");
        previous=stage->serial;if(!state->firstSerial)state->firstSerial=stage->serial;
        auto& mask=state->producedEyes[stage->serial];
        if(mask&(1u<<stage->eye))throw std::runtime_error("duplicate water eye production ledger entry");
        mask|=uint8_t(1u<<stage->eye);
        if(state->producedEyes.size()>16)throw std::runtime_error("water production ledger exceeded bounded native readback delay");
        logFile->event("water_feedback_routed","\"eye\":"+std::to_string(stage->eye)+",\"serial\":"+std::to_string(stage->serial)+
            ",\"source_pool\":"+hex(source)+",\"private_pool\":"+hex(selected));
        return selected;
    }catch(const std::exception& e){fail(*state,e.what());return source;}
}
bool __fastcall take(Result* output,void* pool){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    const bool available=originalTake(output,pool);
    auto* state=current;const auto* stage=worldSharedStages::current;
    if(!available||!state||!stage||stage->eye!=worldSharedStages::ownerEye||caller!=0x1fe227||reinterpret_cast<uintptr_t>(pool)!=state->source||!state->firstSerial||output->serial<state->firstSerial||state->failed)
        return available;
    // Results come in the order of their serials, two frames after they were produced. A produced serial that is well
    // behind the result that just came will not come any more: the game drops a frame's readback when it rebuilds
    // its renderer after a change of the graphics settings. Mock run game-20261013-032305: the frame after each of
    // fifteen changes (shadows, contact shadows, indirect lighting, reflections, water, fog, SSAO) stayed in the
    // ledger, the sixteenth entry ended the session ("water production ledger exceeded bounded native readback
    // delay"). Such an entry is taken off the ledger; nothing is read for it.
    {constexpr uint64_t staleAfter=8;
     for(auto it=state->producedEyes.begin();it!=state->producedEyes.end();){
        if(it->first+staleAfter<output->serial){
            if(staleDropped++<64)try{logFile->event("water_feedback_stale_dropped","\"serial\":"+std::to_string(it->first)+",\"result_serial\":"+std::to_string(output->serial)+
                ",\"produced_eye_mask\":"+std::to_string(it->second)+",\"ledger_before\":"+std::to_string(state->producedEyes.size())+",\"nothing_read_for_it\":true");}catch(...){}
            it=state->producedEyes.erase(it);
        }else ++it;
     }}
    // Original rendering continues after the bounded eye producer stops. Its
    // later packets have no private counterpart and must remain native.
    if(output->serial>std::max(state->lastProduced[0],state->lastProduced[1]))return available;
    try {
        if(stage->shared!=state->owner||GetCurrentThreadId()!=state->thread||output->serial<=state->lastMerged)
            throw std::runtime_error("water consumer owner/thread/serial changed");
        const auto production=state->producedEyes.find(output->serial);
        if(production==state->producedEyes.end())return available;
        const auto producedMask=production->second;
        // Private Views can become ready on different warmup frames. A view
        // that did not record a copy has no result to wait for. Consume only
        // the actually produced counterpart, preserving the native packet.
        if(producedMask!=3){
            for(unsigned eye=0;eye<2;eye++)if(producedMask&(1u<<eye)){
                Result result;
                if(!originalTake(&result,reinterpret_cast<void*>(state->pools[eye]))||result.serial!=output->serial||result.bytes!=water::packetBytes)
                    throw std::runtime_error("partial warmup water result missing or different serial");
            }
            logFile->event("water_feedback_partial_warmup","\"serial\":"+std::to_string(output->serial)+",\"current_serial\":"+std::to_string(stage->serial)+
                ",\"produced_eye_mask\":"+std::to_string(producedMask)+",\"original_packet_unchanged\":true,\"produced_packets_drained\":true");
            state->producedEyes.erase(production);return available;
        }
        const auto nodes=read<uint32_t>(state->owner+0x100458);
        const bool save=state->packetCaptures<2;if(save)++state->packetCaptures;
        auto path=[&](unsigned view){return save?calibrationDirectory/"water-packets"/("serial-"+std::to_string(output->serial)+"-view-"+std::to_string(view)+".bin"):std::filesystem::path{};};
        std::array<water::Packet,3> packets;packets[0]=packet(*output,nodes,path(0));
        for(size_t eye=0;eye<2;eye++) {
            Result result;
            if(!originalTake(&result,reinterpret_cast<void*>(state->pools[eye])))throw std::runtime_error("private water result not complete with original frame");
            packets[eye+1]=packet(result,nodes,path(unsigned(eye+1)));
        }
        auto merged=water::merge(packets);
        std::ostringstream f;f<<"\"serial\":"<<merged.serial<<",\"current_serial\":"<<stage->serial<<",\"source_counts\":[";
        for(size_t n=0;n<3;n++){if(n)f<<',';f<<'['<<packets[n].rawCounts[0]<<','<<packets[n].rawCounts[1]<<','<<packets[n].rawCounts[2]<<']';}
        f<<"],\"merged_counts\":["<<merged.counts[0]<<','<<merged.counts[1]<<','<<merged.counts[2]<<"],\"omitted_splits\":"<<merged.omittedSplits
         <<",\"same_serial\":true,\"supplied_to_native\":true,\"packet_evidence\":"<<(save?"true":"false");
        if(save){std::ofstream file(calibrationDirectory/"water-packets"/("serial-"+std::to_string(merged.serial)+"-merged.bin"),std::ios::binary);
            file.write(reinterpret_cast<const char*>(merged.bytes.data()),merged.bytes.size());if(!file)throw std::runtime_error("merged water packet evidence write failed");}
        logFile->event("water_feedback_merged",f.str());
        state->merged=std::move(merged);state->lastMerged=state->merged.serial;
        state->producedEyes.erase(production);
        // df4f70's native caller finishes reading/sorting before the next frame.
        // Keep the replacement packet owned by the Scene-lifetime state.
        output->data=reinterpret_cast<uintptr_t>(state->merged.bytes.data());
    }catch(const std::exception& e){fail(*state,e.what());} // Original result stays intact on refusal.
    return available;
}
inline void validate(uintptr_t rva,const char* signature){
    const auto p=parsePattern(signature);
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),p.size(),p)!=std::vector<size_t>{0})
        throw std::runtime_error("water feedback native signature changed");
}
inline void create(WaterFeedbackState& state,uintptr_t scene){
    state.owner=read<uintptr_t>(scene+0x9d8);state.thread=GetCurrentThreadId();
    state.source=read<uintptr_t>(state.owner+0x130);
    if(!state.source)throw std::runtime_error("original water feedback pool absent");
    const auto allocator=read<uintptr_t>(state.source),backend=read<uintptr_t>(state.source+0x10),graphics=read<uintptr_t>(state.source+8);
    const auto count=read<uint64_t>(state.source+0x28);
    if(count<1||count>8||count!=read<uint32_t>(graphics+0x440))throw std::runtime_error("native water pool capacity changed");
    const auto entries=read<uintptr_t>(state.source+0x20);
    for(size_t n=0;n<count;n++)if(read<uint64_t>(entries+n*0x40+0x38)!=water::packetBytes)throw std::runtime_error("native water packet size changed");
    struct Descriptor{uint64_t kind=0;uintptr_t backend=0;uint64_t bytes=water::packetBytes;} descriptor;
    descriptor.backend=backend;
    using Factory=void*(__fastcall*)(void*,const void*);
    for(size_t eye=0;eye<2;eye++) {
        const auto pool=reinterpret_cast<uintptr_t>(reinterpret_cast<Factory>(baseAddress+0xfc1550)(reinterpret_cast<void*>(allocator),&descriptor));
        if(!pool||pool==state.source||(eye&&pool==state.pools[0]))throw std::runtime_error("water feedback factory returned invalid ownership");
        state.pools[eye]=pool;
        if(read<uintptr_t>(pool)!=allocator||read<uintptr_t>(pool+8)!=graphics||read<uintptr_t>(pool+0x10)!=backend||read<uint64_t>(pool+0x28)!=count||read<uint64_t>(pool+0x278))
            throw std::runtime_error("private water pool metadata differs");
        logFile->event("water_feedback_pool_created","\"eye\":"+std::to_string(eye)+",\"pool\":"+hex(pool)+
            ",\"source_pool\":"+hex(state.source)+",\"capacity\":"+std::to_string(count)+",\"packet_bytes\":"+std::to_string(water::packetBytes));
    }
}
// leftWhileDrawing (the start by itself, 2026-10-14): the player left the world - quit, or back to the menu - while the
// eyes were still drawn, so the copies of the last frames have not come back yet. As with a stale copy, such a pool is
// left to the process's end and the rest of the disposal goes on (game start 20261014-103108: the disposal stopped
// here with "water Scene cleanup failed" and the bodies, hands and hooks after it were not taken back).
inline void release(WaterFeedbackState& state,bool leftWhileDrawing=false) noexcept {
    for(size_t eye=0;eye<2;eye++)if(state.pools[eye])try {
        const auto pool=state.pools[eye],count=read<uint64_t>(pool+0x28);unsigned drained=0;
        if(count<1||count>8)throw std::runtime_error("water pool disposal capacity changed");
        Result result;
        while(drained<count&&originalTake(&result,reinterpret_cast<void*>(pool)))++drained;
        if(const auto pending=read<uint64_t>(pool+0x278)){
            // A copy whose result never came (a frame on which the game rebuilt its renderer, see take()) stays pending
            // in the eye's pool for good. A pool with pending entries cannot be destroyed; it is left to the process's
            // end, and the rest of the disposal goes on. Without such a frame this is still a failure.
            if(!staleDropped&&!leftWhileDrawing)throw std::runtime_error("water pool still has pending native GPU entries; retained until process exit");
            logFile->event("water_feedback_pool_retained","\"eye\":"+std::to_string(eye)+",\"pool\":"+hex(pool)+",\"pending\":"+std::to_string(pending)+",\"drained\":"+std::to_string(drained)+
                ",\"stale_frames\":"+std::to_string(staleDropped)+",\"world_left_while_drawing\":"+(leftWhileDrawing?"true":"false")+",\"left_to_process_exit\":true");
            continue;
        }
        reinterpret_cast<void(__fastcall*)(uintptr_t*)>(baseAddress+0xfcff30)(&state.pools[eye]);
        if(state.pools[eye])throw std::runtime_error("native water pool destructor did not clear ownership");
        logFile->event("water_feedback_pool_released","\"eye\":"+std::to_string(eye)+",\"pool\":"+hex(pool)+",\"drained\":"+std::to_string(drained));
    }catch(const std::exception& e){fail(state,e.what());}
}
inline std::unique_ptr<WaterFeedbackState> deferred;
inline uint64_t deferredAt=0,deferredPolls=0;
inline void retirementSnapshot(const WaterFeedbackState& state,const char* phase){
 for(unsigned eye=0;eye<2;++eye)if(const auto pool=state.pools[eye]){
  const auto graphics=read<uintptr_t>(pool+8),entries=read<uintptr_t>(pool+0x20),count=read<uint64_t>(pool+0x28);
  if(!count||count>8)throw std::runtime_error("retirement snapshot capacity differs");
  std::ostringstream o;o<<"\"phase\":"<<jsonString(phase)<<",\"poll\":"<<deferredPolls<<",\"eye\":"<<eye<<",\"pool\":"<<hex(pool)<<",\"thread\":"<<GetCurrentThreadId()<<",\"pending\":"<<read<uint64_t>(pool+0x278)<<",\"completed\":"<<read<uint64_t>(graphics+0x550)<<",\"submitted\":"<<read<uint64_t>(graphics+0x548)<<",\"slots\":[";
  for(uint64_t n=0;n<count;++n){if(n)o<<',';o<<'['<<read<uint64_t>(entries+n*0x40+0x10)<<','<<read<uint64_t>(entries+n*0x40+0x28)<<']';}o<<']';
  logFile->event("water_deferred_snapshot",o.str());
 }
}
inline void discardCompleted(WaterFeedbackState& state){
 for(unsigned eye=0;eye<2;++eye)if(const auto pool=state.pools[eye]){
  const auto graphics=read<uintptr_t>(pool+8),entries=read<uintptr_t>(pool+0x20);
  water::RetirementEvidence e;e.ownerStopped=true;e.sceneReturned=activeXrExitOwnerReturned.load();e.sameThread=state.thread==GetCurrentThreadId();
  e.capacity=read<uint64_t>(pool+0x28);e.pending=read<uint64_t>(pool+0x278);e.completed=read<uint64_t>(graphics+0x550);
  if(!e.capacity||e.capacity>8||e.pending>e.capacity)throw std::runtime_error("private readback retirement layout changed");
  for(size_t i=0;i<e.capacity;++i)e.slots[i]={read<uint64_t>(entries+i*0x40+0x10),read<uint64_t>(entries+i*0x40+0x28)};
  if(!water::permitsPrivateDiscard(e))continue;
  // df4f70 calls e0d620 on every Take; repeating Take can clear an already-ready
  // second slot without decrementing pending. At Scene end no consumer remains.
  // Use the native destructor's supported unread-result path after real native
  // completion. It logs the unread count and releases ownership normally.
  // Do not modify pool bookkeeping or global graphics completion to hide it.
  logFile->event("water_private_feedback_discard","\"eye\":"+std::to_string(eye)+",\"pool\":"+hex(pool)+",\"unread_packets\":"+std::to_string(e.pending)+",\"completed\":"+std::to_string(e.completed)+",\"scene_returned\":true,\"native_take_calls\":0,\"native_serials_modified\":false");
  reinterpret_cast<void(__fastcall*)(uintptr_t*)>(baseAddress+0xfcff30)(&state.pools[eye]);
  if(state.pools[eye])throw std::runtime_error("native detached pool destructor did not clear ownership");
  logFile->event("water_feedback_pool_released","\"eye\":"+std::to_string(eye)+",\"pool\":"+hex(pool)+",\"drained\":0,\"discarded_packets\":"+std::to_string(e.pending)+",\"after_native_completion\":true");
 }
}
inline uint32_t pollDeferred(void* context,uint32_t deviceEnding){
 const RenderCallbackScope scope;const ObserverLastError preserve;
 if(!deferred||context!=deferred.get()||GetCurrentThreadId()!=deferred->thread)return 2;
 ++deferredPolls;
 try{
  if(deviceEnding||GetTickCount64()-deferredAt>2000)throw std::runtime_error("water deferred retirement did not complete before bounded device lifetime");
  if(!activeXrExitOwnerReturned.load())return 0;
  if(deferredPolls<=8)retirementSnapshot(*deferred,"later_present");
  discardCompleted(*deferred);
  if(deferred->failed)throw std::runtime_error("water deferred native drain failed");
  if(deferred->pools[0]||deferred->pools[1])return 0;
  logFile->event("water_deferred_retired","\"polls\":"+std::to_string(deferredPolls)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"elapsed_ms\":"+std::to_string(GetTickCount64()-deferredAt)+",\"scene_accessed\":false,\"native_serials_modified\":false,\"pools_remaining\":0");
  deferred.reset();activeXrExitReadbackRetained=false;done.store(true,std::memory_order_release);return 1;
 }catch(const std::exception& e){
  fail(*deferred,e.what());logFile->event("water_deferred_retained_on_failure","\"pools_retained\":true,\"scene_accessed\":false");
  done.store(true,std::memory_order_release);return 2;
 }
}
inline void defer(std::unique_ptr<WaterFeedbackState> state){
 if(deferred||!state||state->failed||state->thread!=GetCurrentThreadId())throw std::runtime_error("invalid detached readback ownership transfer");
 deferred=std::move(state);activeXrExitReadbackRetained=true;deferredAt=GetTickCount64();retirementSnapshot(*deferred,"scene_detached");
 const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");const auto install=layer?reinterpret_cast<diagnostic::RegisterPresentRetirement>(GetProcAddress(layer,"evrRegisterPresentRetirement")):nullptr;
 diagnostic::PresentRetirementRequest request;request.thread=GetCurrentThreadId();request.callback=&pollDeferred;request.context=deferred.get();
 if(!install||install(&request))throw std::runtime_error("present readback retirement registration failed; ownership retained");
 logFile->event("water_deferred_ownership","\"scene_accessed_after_return\":false,\"owner_count\":1,\"world_accessed\":false");
}
inline void install(){
    // 1.0 (2026-10-16): the water's producer side was rebuilt (the WaterBrickTree; the June cull_water_bricks 0x1ccac0 has
    // no counterpart yet) and the owner create() reads (scene+0x9d8 +0x130 ...) is not located: the private feedback stays
    // off - both eyes use the scene's own water feedback - until it is ported. Mock game-20261016-040701 read garbage here.
    throw std::runtime_error("private water feedback is not ported to 1.0 (WaterBrickTree)");
    validate(0xfc1550,"48 89 6C 24 18 56 41 56 41 57 48 81 EC 80 00 00 00");
    validate(0xfcff30,"4C 8B DC 49 89 5B 18 57 48 81 EC 90 00 00 00");
    validate(0xfe6d80,"48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 81 EC 80 00 00 00 48 83 BA 78");
    const auto call=baseAddress+0x1fe222;   // 1.0: call 0xfe39c0 (return 0x1fdd77)
    if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+0xfe6d80)
        throw std::runtime_error("water feedback consumer caller changed");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+0xfe6d80),reinterpret_cast<void*>(&take),reinterpret_cast<void**>(&originalTake))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+0xfe6d80))!=MH_OK)throw std::runtime_error("water feedback consumer hook failed");
    enabled=true;logFile->event("water_feedback_hook_enabled","\"rva\":\"0xfe6d80\",\"policy\":\"same_serial_union_refine_intersection_coarsen_retire\"");
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto result=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0xfe6d80));if(result==MH_OK)enabled=false;
    try{logFile->event("water_feedback_hook_removed","\"status\":"+std::to_string(result));}catch(...){}
}

// The scene's own water feedback, read only (2026-10-23, the user: "with AFW the water's reflection is the same in both
// eyes - does that help with the left/right difference of the main mode?"). Since 1.0 the eyes' private feedback is off
// (install above), so both eyes' water copies go to the scene's one pool, and its consumer - the shared update 0x1fe0c0,
// the owner's alone (worldSharedStages) - takes one result a frame. With both eyes drawn a frame that is two copies made and
// one taken; with one eye a frame (AFW) one and one. EVR_WATER_FEEDBACK_OBSERVE=1: each take's result (whether one was
// there, its serial against the frame's) and the pool's words around its capacity and pending count, as the June layout had
// them (+0x28, +0x278), read with checks; nothing is changed.
namespace waterObserve {
inline const bool observe=[]{char v[8]{};return GetEnvironmentVariableA("EVR_WATER_FEEDBACK_OBSERVE",v,sizeof v)==1&&v[0]=='1';}();
// The other eye running the first shared update itself (worldSharedStages EVR_EYE_RUNS_SHARED bit 1, 2026-10-23: the water's
// left/right difference of the main mode) takes the scene's water result a second time a frame: Mock game-20261023-134101 and
// -152432 ended in an access violation (enshrouded.exe+0x132eadf) as the session closed. That eye's take finds nothing now -
// the result stays the owner's, one a frame as the game has it.
inline bool guardOtherEye(){return (worldSharedStages::eyeRunsShared&1)!=0;}
inline bool wanted(){return observe||guardOtherEye();}
inline bool enabled=false;inline Take original=nullptr;
inline std::atomic<uint64_t> calls{0},available{0},lagSum{0},lagMax{0},noStage{0},guarded{0};
inline uintptr_t lastPool=0;
bool __fastcall take(Result* output,void* pool){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    if(guardOtherEye()&&caller==0x1fe227){const auto* stage=worldSharedStages::current;
        if(stage&&stage->eye>=0&&stage->eye!=worldSharedStages::ownerEye){const auto n=++guarded;
            if(n<=3||n%3600==0)try{logFile->event("water_feedback_other_eye_take_skipped","\"frame\":"+std::to_string(liveFrame.frame)+",\"eye\":"+std::to_string(stage->eye)+",\"times\":"+std::to_string(n));}catch(...){}
            return false;}}
    const bool got=original(output,pool);
    if(!observe)return got;
    try{
        const auto n=++calls;const auto* stage=worldSharedStages::current;
        uint64_t lag=0;
        if(got)++available;
        if(stage&&got&&stage->serial>=output->serial){lag=stage->serial-output->serial;lagSum+=lag;if(lag>lagMax)lagMax=lag;}
        if(!stage)++noStage;
        const auto p=reinterpret_cast<uintptr_t>(pool);
        if(n<=40||p!=lastPool||n%600==0){
            uint64_t capacity=0,pending=0;try{capacity=read<uint64_t>(p+0x28);pending=read<uint64_t>(p+0x278);}catch(...){}
            std::ostringstream o;o<<"\"call\":"<<n<<",\"frame\":"<<liveFrame.frame<<",\"eye_mask\":"<<liveFrame.eyeMask<<",\"caller\":"<<hex(caller)<<",\"pool\":"<<hex(p)
                <<",\"available\":"<<(got?"true":"false")<<",\"result_serial\":"<<(got?output->serial:0)<<",\"bytes\":"<<(got?output->bytes:0)
                <<",\"stage_eye\":"<<(stage?stage->eye:-9)<<",\"stage_serial\":"<<(stage?stage->serial:0)<<",\"owner_eye\":"<<worldSharedStages::ownerEye
                <<",\"lag\":"<<lag<<",\"pool_word_28\":"<<capacity<<",\"pool_word_278\":"<<pending
                <<",\"calls\":"<<n<<",\"available_calls\":"<<available.load()<<",\"lag_mean\":"<<(available.load()?double(lagSum.load())/double(available.load()):0.0)<<",\"lag_max\":"<<lagMax.load()<<",\"no_stage\":"<<noStage.load();
            logFile->event("water_feedback_observed",o.str());}
        lastPool=p;
    }catch(...){}
    return got;
}
inline void install(){
    const auto p=parsePattern("48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 81 EC 80 00 00 00 48 83 BA 78");
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+0xfe6d80),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("water feedback consumer signature changed");
    const auto call=baseAddress+0x1fe222;
    if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+0xfe6d80)throw std::runtime_error("water feedback consumer caller changed");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+0xfe6d80),reinterpret_cast<void*>(&take),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+0xfe6d80))!=MH_OK)throw std::runtime_error("water feedback observer hook failed");
    enabled=true;logFile->event("water_feedback_observer_enabled","\"rva\":\"0xfe6d80\",\"changes_native_results\":false");
}
inline void disable() noexcept {
    if(!enabled)return;const auto r=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0xfe6d80));if(r==MH_OK)enabled=false;
    try{logFile->event("water_feedback_observer_removed","\"status\":"+std::to_string(r)+",\"calls\":"+std::to_string(calls.load())+",\"available\":"+std::to_string(available.load())+
        ",\"lag_mean\":"+std::to_string(available.load()?double(lagSum.load())/double(available.load()):0.0)+",\"lag_max\":"+std::to_string(lagMax.load()));}catch(...){}
}
}
}
