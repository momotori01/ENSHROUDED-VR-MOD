#pragma once
// Bounded evidence and optional value-only process broker publisher. Never writes ECS.
namespace worldLifecycleObserver {
using Create=void*(__fastcall*)(void*);
// Destructor RAX is opaque to known callers; forwarding it preserves the ABI.
using One=uintptr_t(__fastcall*)(void*);
using Two=uintptr_t(__fastcall*)(void*,void*);
inline Create originalCreate=nullptr;
inline One originalRoot=nullptr,originalStorage=nullptr,originalPreclose=nullptr;
inline bool precloseRequested=false,precloseAdmission=false;inline std::atomic<unsigned> precloseCalls{0};
inline Two originalCleanup=nullptr;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},failures{0};
inline std::atomic<uint64_t> calls{0};
inline thread_local uint64_t parentCall=0;
inline bool inputExitRequested=false;inline std::atomic<bool> inputExitReturned{false};inline std::atomic<unsigned> inputExitCalls{0};
inline std::atomic<unsigned> inputExitCompletion{0}; // 0 waiting, 1 claimed, 2 PASS, 3 FAIL
inline void finishInputExit() noexcept;
inline bool epochShadow=false;
inline std::mutex epochMutex;
inline motion::WorldLifetimes<8> epochs;
inline std::map<uintptr_t,motion::WorldToken> epochByWorld;
inline unsigned epochCreated=0,epochClosedRoots=0;
inline bool reentryTrial=false,reentryGapObserved=false;
inline bool brokerRequested=false;
inline epoch::Call brokerCall=nullptr;
inline epoch::Response provider;
inline std::map<uintptr_t,epoch::Response> brokerWorlds;
inline std::array<epoch::Response,3> firstGeneration{};
inline epoch::Response broker(epoch::Request r){
 epoch::Response out;if(!brokerCall)throw std::runtime_error("World broker export unavailable");
 const auto code=brokerCall(&r,&out);if(code||out.status!=code||out.size!=sizeof(out)||out.version!=epoch::abiVersion)throw std::runtime_error("World broker operation failed: "+std::to_string(r.operation)+"/"+std::to_string(code));return out;
}
inline std::string brokerFields(const epoch::Response& r){
 return "\"provider\":"+std::to_string(r.provider)+",\"serial\":"+std::to_string(r.serial)+",\"slot\":"+std::to_string(r.slot)+",\"world\":"+hex(r.world)+",\"registry\":"+hex(r.registry)+",\"owner_root\":"+hex(r.root)+",\"kind\":"+std::to_string(r.kind)+",\"phase\":"+std::to_string(r.phase)+",\"pins\":"+std::to_string(r.pins)+",\"handle\":"+std::to_string(r.handle)+",\"flags\":"+std::to_string(r.flags)+",\"worlds\":"+std::to_string(r.worlds)+",\"closures\":"+std::to_string(r.closures)+",\"leases\":"+std::to_string(r.leases);
}
inline bool brokerDrained(){if(!brokerRequested)return true;try{const auto s=broker(epoch::request(epoch::Operation::Stats));return s.flags==epoch::flagActive&&s.provider==provider.provider&&!s.worlds&&!s.closures&&!s.leases;}catch(...){return false;}}
inline bool exitReady(){std::lock_guard<std::mutex> lock(epochMutex);return (!inputExitRequested||inputExitReturned.load())&&epochShadow&&epochCreated==(reentryTrial?6u:3u)&&epochClosedRoots==(reentryTrial?4u:2u)&&(!reentryTrial||reentryGapObserved)&&epochs.size()==0&&!epochs.faulted()&&brokerDrained();}
inline std::string tokenFields(motion::WorldToken token){return "\"epoch_slot\":"+std::to_string(token.slot)+",\"epoch_serial\":"+std::to_string(token.serial);}

struct Hook {uintptr_t rva;const char* pattern;void* target;void** original;bool created=false;};
struct Sample {uintptr_t root=0,world=0,registry=0,task=0,prediction=0;uint64_t queued=0,completed=0;uint32_t running=0,tick=0,counter=0,count=0;uint8_t kind=255;std::array<uint8_t,16> header{};};
inline Sample snapshot(uintptr_t world,uintptr_t root=0){
 Sample s;s.root=root;s.world=world;s.header=read<std::array<uint8_t,16>>(world);
 std::memcpy(&s.registry,s.header.data(),8);s.kind=s.header[8];
 if(!s.registry||s.kind>2)throw std::runtime_error("World lifecycle header invalid");
 s.task=read<uintptr_t>(s.registry+0xcc38e0);
 if(s.task){s.running=read<uint32_t>(s.task+0x1c8);s.queued=read<uint64_t>(s.task+0x1a8);s.completed=read<uint64_t>(s.task+0x1c0);}
 s.tick=read<uint32_t>(world+0x504);s.counter=read<uint32_t>(world+0x4c0);s.count=read<uint32_t>(world+0x19c);
 if(root&&s.kind==2){const auto container=read<uintptr_t>(root+0x10bbc08);if(container)s.prediction=container+0x118;}
 return s;
}
inline std::string fields(const Sample& s){
 std::ostringstream o;o<<"\"root\":"<<hex(s.root)<<",\"world\":"<<hex(s.world)<<",\"registry\":"<<hex(s.registry)<<",\"task_list\":"<<hex(s.task)<<",\"prediction_world\":"<<hex(s.prediction)<<",\"kind\":"<<unsigned(s.kind)<<",\"tick\":"<<s.tick<<",\"counter\":"<<s.counter<<",\"entity_count\":"<<s.count<<",\"running\":"<<s.running<<",\"queued\":"<<s.queued<<",\"completed\":"<<s.completed<<",\"header_hex\":\"";
 for(auto b:s.header)o<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);
 o<<"\",\"quiescence_proven\":false";return o.str();
}
inline void failure() noexcept {++failures;if(brokerCall&&provider.provider){auto q=epoch::request(epoch::Operation::Fault,provider);epoch::Response out;brokerCall(&q,&out);}}
// Optional native renderer progress observer. Mutually exclusive with the XR
// render probe: it forwards exact arguments and never creates or frees resources.
namespace progress {
inline bool requested=false;inline Render originalRender=nullptr;
using Owner=void(__fastcall*)(void*);inline Owner originalOwner=nullptr;
using Arm=uint32_t(*)(uint32_t);inline Arm armLayer=nullptr;
inline std::atomic<uint64_t> deadline{0},renders{0},returned{0},lastView{0},lastScene{0};
inline std::atomic<unsigned> inRender{0},owners{0};
inline bool recording(){auto d=deadline.load();return d&&GetTickCount64()<d;}
inline std::string counts(){return "\"render_calls\":"+std::to_string(renders.load())+",\"render_returns\":"+std::to_string(returned.load())+",\"render_active\":"+std::to_string(inRender.load())+",\"last_view\":"+hex(lastView.load())+",\"last_scene\":"+hex(lastScene.load());}
inline void mark(const char* phase,unsigned call){
 if(!requested)return;
 if(!deadline.load()){
  if(!armLayer||armLayer(5000))throw std::runtime_error("exit progress layer arm rejected");
  deadline=GetTickCount64()+5000;
  logFile->event("world_exit_progress_armed","\"duration_ms\":5000,\"read_only\":true");
 }
 logFile->event("world_exit_progress_boundary","\"phase\":"+jsonString(phase)+",\"preclose_call\":"+std::to_string(call)+","+counts());
}
__declspec(noinline) void* __fastcall render(void* out,void* context,void* view,void* parameters,void* target){
 ++callbacks;++inRender;const auto call=++renders;const auto start=GetTickCount64();
 const bool entered=recording();uintptr_t scene=0;uint64_t serial=0;
 {const ObserverLastError preserve;try{
  scene=read<uintptr_t>(reinterpret_cast<uintptr_t>(view)+8);
  serial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
  lastView=reinterpret_cast<uintptr_t>(view);lastScene=scene;
  if(entered)logFile->event("world_exit_render_begin","\"render_call\":"+std::to_string(call)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"scene\":"+hex(scene)+",\"view\":"+hex(reinterpret_cast<uintptr_t>(view))+",\"serial\":"+std::to_string(serial));
 }catch(...){failure();}}
 auto* result=originalRender(out,context,view,parameters,target);
 {const ObserverLastError preserve;try{
  if(entered||recording())logFile->event("world_exit_render_end","\"render_call\":"+std::to_string(call)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"scene\":"+hex(scene)+",\"view\":"+hex(reinterpret_cast<uintptr_t>(view))+",\"serial\":"+std::to_string(serial)+",\"entered_during_window\":"+(entered?"true":"false")+",\"elapsed_ms\":"+std::to_string(GetTickCount64()-start)+",\"arguments_unchanged\":true,\"return_unchanged\":true");
 }catch(...){failure();}}
 ++returned;--inRender;--callbacks;return result;
}
__declspec(noinline) void __fastcall owner(void* address){
 ++callbacks;const auto call=++owners;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 std::string saved;
 {const ObserverLastError preserve;try{
  if(call>64)throw std::runtime_error("exit owner observation cap");
  const auto object=read<uintptr_t>(reinterpret_cast<uintptr_t>(address));
  const auto scene=object?read<uintptr_t>(object+0x70):0,view=object?read<uintptr_t>(object+0x78):0;
  saved="\"owner_call\":"+std::to_string(call)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"caller\":"+hex(caller)+",\"address\":"+hex(reinterpret_cast<uintptr_t>(address))+",\"owner\":"+hex(object)+",\"scene\":"+hex(scene)+",\"view\":"+hex(view);
  logFile->event("world_exit_owner_begin",saved+","+counts()+",\"read_only\":true");
 }catch(...){failure();}}
 originalOwner(address);
 {const ObserverLastError preserve;try{if(!saved.empty())logFile->event("world_exit_owner_end",saved+",\"arguments_unchanged\":true,\"reads_destroyed_memory\":false");}catch(...){failure();}}
 --callbacks;
}
}
using InputBackendDestroy=void(__fastcall*)(void*,void*);
inline InputBackendDestroy originalInputBackendDestroy=nullptr;
__declspec(noinline) void __fastcall inputBackendDestroy(void* allocator,void* backend){
 ++callbacks;const auto number=++inputExitCalls;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 std::string saved;
 {const ObserverLastError preserve;try{
  if(number!=1||caller!=0xf99fd4)throw std::runtime_error("unexpected input backend destruction scope");
  const auto object=reinterpret_cast<uintptr_t>(backend),manager=read<uintptr_t>(object);
  if(!manager||read<uintptr_t>(manager+0x48)!=object)throw std::runtime_error("input backend has different native manager owner");
  std::ostringstream s;s<<"\"call\":"<<number<<",\"caller\":"<<hex(caller)<<",\"thread\":"<<GetCurrentThreadId()<<",\"allocator\":"<<hex(reinterpret_cast<uintptr_t>(allocator))<<",\"backend\":"<<hex(object)<<",\"manager\":"<<hex(manager)<<",\"module\":"<<hex(read<uintptr_t>(object+8))<<",\"api\":["<<hex(read<uintptr_t>(object+0x10))<<','<<hex(read<uintptr_t>(object+0x18))<<','<<hex(read<uintptr_t>(object+0x20))<<"],\"connected\":[";
  for(unsigned i=0;i<4;++i){if(i)s<<',';s<<unsigned(read<uint8_t>(object+0x50+i*0x40));}s<<"],\"read_only\":true";saved=s.str();
  logFile->event("input_backend_exit_begin",saved);
 }catch(...){failure();}}
 originalInputBackendDestroy(allocator,backend);
 {const ObserverLastError preserve;try{if(!saved.empty())logFile->event("input_backend_exit_end",saved+",\"arguments_unchanged\":true,\"reads_destroyed_memory\":false");}catch(...){failure();}}
 // Native call and all native-memory access have ended. The trampoline and
 // this module stay pinned. Finalize synchronously: process exit can terminate
 // the diagnostic worker before its next scheduled wakeup.
 inputExitReturned=true;--callbacks;finishInputExit();
}
struct Call {
 uint64_t id=0,parent=0;uintptr_t caller=0;const char* operation;Sample before{};bool valid=false;motion::WorldLifetimes<8>::Batch epochBatch;epoch::Response brokerClosure;
 Call(const char* name,uintptr_t call):caller(call),operation(name){
  ++callbacks;parent=parentCall;
  if(accepting.load()){id=++calls;if(id>128){failure();id=0;}else parentCall=id;}
 }
 ~Call(){parentCall=parent;--callbacks;}
 std::string common() const {return "\"call_id\":"+std::to_string(id)+",\"parent_call_id\":"+std::to_string(parent)+",\"operation\":"+jsonString(operation)+",\"caller\":"+hex(caller)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"milliseconds\":"+std::to_string(GetTickCount64())+",\"read_only\":true";}
 void publishEpoch(const Sample& s,uintptr_t root){
  std::lock_guard<std::mutex> lock(epochMutex);
  if(reentryTrial&&epochCreated>=3&&!reentryGapObserved)throw std::runtime_error("second World generation before first drained gap");
  auto token=epochs.created({s.world,s.registry,root,s.kind});
  if(!token||!epochByWorld.emplace(s.world,token).second)throw std::runtime_error("World epoch shadow creation conflict");++epochCreated;
  logFile->event("world_epoch_created",common()+","+tokenFields(token)+","+fields(s)+",\"owner_root\":"+hex(root)+",\"shadow_only\":true");
  if(brokerRequested){auto q=epoch::request(epoch::Operation::Create,provider);q.world=s.world;q.registry=s.registry;q.root=root;q.kind=s.kind;
   auto created=broker(q);if(!epoch::admitsNew(created)||!brokerWorlds.emplace(s.world,created).second)throw std::runtime_error("World broker creation mismatch");
   logFile->event("world_broker_created",common()+","+brokerFields(created)+",\"gameplay_connected\":false");
   if(reentryTrial){
    if(epochCreated<=3)firstGeneration[epochCreated-1]=created;
    else for(const auto& old:firstGeneration){
     const auto request=epoch::request(epoch::Operation::Read,old);epoch::Response rejected;
     const auto code=brokerCall(&request,&rejected);
     if(code!=uint32_t(epoch::Status::WrongToken)||rejected.status!=code||rejected.version!=epoch::abiVersion||rejected.size!=sizeof(rejected))throw std::runtime_error("old World token survived reentry");
     logFile->event("world_reentry_stale_token",common()+",\"old_provider\":"+std::to_string(old.provider)+",\"old_serial\":"+std::to_string(old.serial)+",\"old_slot\":"+std::to_string(old.slot)+",\"old_world\":"+hex(old.world)+",\"new_provider\":"+std::to_string(created.provider)+",\"new_serial\":"+std::to_string(created.serial)+",\"new_world\":"+hex(created.world)+",\"status\":"+std::to_string(code)+",\"reads_native_memory\":false,\"serials_reset\":false");
    }
   }
  }
 }
 void beginEpoch(){
  std::lock_guard<std::mutex> lock(epochMutex);auto i=epochByWorld.find(before.world);
  if(i==epochByWorld.end())throw std::runtime_error("World epoch shadow missed native creation");
  const auto token=i->second;const auto state=epochs.snapshot(token);
  if(state.identity.registry!=before.registry||state.identity.kind!=before.kind)throw std::runtime_error("World epoch shadow identity changed");
  if(brokerRequested){
   auto i=brokerWorlds.find(before.world);if(i==brokerWorlds.end()||i->second.registry!=before.registry||i->second.kind!=before.kind)throw std::runtime_error("World broker missed native identity");
   if(before.root){
    brokerClosure=broker(epoch::request(epoch::Operation::BeginRootClose,i->second));
    for(const auto& member:brokerWorlds)if(member.second.root==before.root){auto value=broker(epoch::request(epoch::Operation::Read,member.second));
     if(value.phase!=uint32_t(epoch::Phase::Closing)||!value.pins||epoch::admitsNew(value))throw std::runtime_error("World broker root invalidation mismatch");
     logFile->event("world_broker_closing",common()+","+brokerFields(value)+",\"close_handle\":"+std::to_string(brokerClosure.handle));}
   }else{auto value=broker(epoch::request(epoch::Operation::Read,i->second));if(value.phase!=uint32_t(epoch::Phase::Closing)||!value.pins)throw std::runtime_error("native child used live broker value");
    logFile->event("world_broker_child",common()+","+brokerFields(value));}
  }
  if(before.root){
   if(state.identity.root!=before.root||!epochs.alive(token))throw std::runtime_error("World epoch root not alive");
   // A pin represents this real MOD callback spanning the native destructor.
   for(const auto& entry:epochByWorld){auto member=epochs.snapshot(entry.second);if(member.identity.root==before.root&&!epochs.pin(entry.second))throw std::runtime_error("World epoch pin refused");}
   epochBatch=epochs.beginRootClose(before.root);if(epochBatch.count!=(before.kind==2?2u:1u))throw std::runtime_error("World epoch root membership incomplete");
   for(size_t j=0;j<epochBatch.count;++j){auto t=epochBatch.tokens[j];auto member=epochs.snapshot(t);if(epochs.alive(t)||member.pins!=1)throw std::runtime_error("World epoch failed to invalidate");
    logFile->event("world_epoch_closing",common()+","+tokenFields(t)+",\"world\":"+hex(member.identity.world)+",\"owner_root\":"+hex(before.root)+",\"kind\":"+std::to_string(member.identity.kind)+",\"pins\":1,\"alive\":false,\"shadow_only\":true");}
  }else{
   if(state.phase!=motion::WorldPhase::Closing||state.pins!=1)throw std::runtime_error("native cleanup consumed a live or unpinned shadow epoch");
   logFile->event("world_epoch_child",common()+","+tokenFields(token)+",\"world\":"+hex(before.world)+",\"kind\":"+std::to_string(before.kind)+",\"alive\":false,\"shadow_only\":true");
  }
 }
 void endEpoch(){
  std::lock_guard<std::mutex> lock(epochMutex);
  for(size_t j=0;j<epochBatch.count;++j){const auto t=epochBatch.tokens[j];auto member=epochs.snapshot(t);
   if(!epochs.finishClose(t)||epochs.reclaim(t)||!epochs.unpin(t)||!epochs.reclaim(t))throw std::runtime_error("World epoch shadow pin/drain failed");
   auto i=epochByWorld.find(member.identity.world);if(i==epochByWorld.end()||!(i->second==t))throw std::runtime_error("World epoch shadow map changed");epochByWorld.erase(i);
   logFile->event("world_epoch_retired",common()+","+tokenFields(t)+",\"world\":"+hex(member.identity.world)+",\"owner_root\":"+hex(member.identity.root)+",\"kind\":"+std::to_string(member.identity.kind)+",\"slots_remaining\":"+std::to_string(epochs.size())+",\"native_returned\":true,\"pin_blocked_early_reclaim\":true,\"shadow_only\":true");
  }
  if(brokerRequested&&brokerClosure.handle){auto value=broker(epoch::request(epoch::Operation::EndRootClose,brokerClosure));
   logFile->event("world_broker_root_ended",common()+","+brokerFields(value)+",\"native_returned\":true");
   for(auto i=brokerWorlds.begin();i!=brokerWorlds.end();)if(i->second.root==brokerClosure.root)i=brokerWorlds.erase(i);else ++i;
  }
  if(epochBatch.count)++epochClosedRoots;
  if(reentryTrial&&epochClosedRoots==2&&epochCreated==3){
   if(epochs.size()||epochs.faulted()||!epochByWorld.empty()||!brokerWorlds.empty()||!brokerDrained())throw std::runtime_error("first World generation did not drain before reentry");
   reentryGapObserved=true;
   logFile->event("world_reentry_gap",common()+",\"created\":3,\"closed_roots\":2,\"epochs_remaining\":0,\"broker_drained\":true,\"provider_retained\":true,\"serials_reset\":false,\"native_roots_returned\":true,\"claims_VR_reentry\":false");
  }
 }
 void begin(uintptr_t world,uintptr_t root,uintptr_t arg0,uintptr_t arg1=0) noexcept {
  if(!id)return;const ObserverLastError preserve;
  try{if(world){before=snapshot(world,root);valid=true;}
   logFile->event("world_lifecycle_begin",common()+",\"arg0\":"+hex(arg0)+",\"arg1\":"+hex(arg1)+",\"snapshot_valid\":"+(valid?"true":"false")+(valid?","+fields(before):""));
   if(epochShadow&&valid)beginEpoch();
  }catch(...){failure();}
 }
 void end(uintptr_t result,uintptr_t createdRoot=0) noexcept {
  if(!id)return;const ObserverLastError preserve;
  try{
   // Destruction completion uses ONLY the saved value snapshot, never freed memory.
   auto s=before;bool snapshotValid=valid;
   if(createdRoot){s=snapshot(createdRoot+0xcc4218,createdRoot);snapshotValid=true;}
   logFile->event("world_lifecycle_end",common()+",\"native_result\":"+hex(result)+",\"snapshot_valid\":"+(snapshotValid?"true":"false")+(snapshotValid?","+fields(s):"")+",\"arguments_unchanged\":true,\"return_unchanged\":true,\"reads_destroyed_memory\":false");
   if(epochShadow&&createdRoot)publishEpoch(s,createdRoot);
   if(createdRoot&&s.prediction){const auto p=snapshot(s.prediction);logFile->event("world_lifecycle_prediction",common()+","+fields(p));if(epochShadow)publishEpoch(p,createdRoot);}
   if(epochShadow&&epochBatch.count)endEpoch();
  }catch(...){failure();}
 }
};
__declspec(noinline) void* __fastcall create(void* config){
 Call c("create",reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress);c.begin(0,0,reinterpret_cast<uintptr_t>(config));
 auto* result=originalCreate(config);c.end(reinterpret_cast<uintptr_t>(result),reinterpret_cast<uintptr_t>(result));return result;
}
__declspec(noinline) uintptr_t __fastcall destroyRoot(void* root){
 const auto address=reinterpret_cast<uintptr_t>(root);Call c("root_destroy",reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress);c.begin(address+0xcc4218,address,address);
 const auto result=originalRoot(root);c.end(result);return result;
}
__declspec(noinline) uintptr_t __fastcall cleanup(void* allocator,void* world){
 Call c("entity_cleanup",reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress);c.begin(reinterpret_cast<uintptr_t>(world),0,reinterpret_cast<uintptr_t>(allocator),reinterpret_cast<uintptr_t>(world));
 const auto result=originalCleanup(allocator,world);c.end(result);return result;
}
__declspec(noinline) uintptr_t __fastcall storage(void* world){
 const auto address=reinterpret_cast<uintptr_t>(world);Call c("storage_destroy",reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress);c.begin(address,0,address);
 const auto result=originalStorage(world);c.end(result);return result;
}

// Observe native entity deletion; optional admission affects only MOD value state.
__declspec(noinline) uintptr_t __fastcall preclose(void* root){
 ++callbacks;struct Done{~Done(){--callbacks;}} done;
 const auto call=accepting.load()?++precloseCalls:0;
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 const auto address=reinterpret_cast<uintptr_t>(root);
 Sample before{},prediction{};epoch::Response state;bool valid=false;
 auto common=[&](){return "\"preclose_call\":"+std::to_string(call)+",\"caller\":"+hex(caller)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"read_only\":true";};
 if(call){const ObserverLastError preserve;try{
  if(call>64)throw std::runtime_error("preclose observation budget exhausted");
  progress::mark("before_preclose",call);
  before=snapshot(address+0xcc4218,address);if(before.prediction)prediction=snapshot(before.prediction);
  {std::lock_guard<std::mutex> lock(epochMutex);auto i=brokerWorlds.find(before.world);if(i==brokerWorlds.end())throw std::runtime_error("preclose has no startup identity");state=broker(epoch::request(epoch::Operation::Read,i->second));}
  valid=true;logFile->event("world_preclose_begin",common()+","+fields(before)+",\"prediction_count\":"+std::to_string(prediction.count)+",\"broker\":{"+brokerFields(state)+"}");
  if(precloseAdmission){std::lock_guard<std::mutex> lock(epochMutex);
   const auto i=brokerWorlds.find(before.world);if(i==brokerWorlds.end())throw std::runtime_error("preclose admission owner vanished");
   const auto closed=broker(epoch::request(epoch::Operation::CloseAdmission,i->second));
   if(closed.phase!=uint32_t(epoch::Phase::Alive)||closed.flags!=(epoch::flagActive|epoch::flagAdmissionClosed)||epoch::admitsNew(closed))throw std::runtime_error("preclose did not stop new admission");
   for(const auto& member:brokerWorlds)if(member.second.root==before.root){const auto value=broker(epoch::request(epoch::Operation::Read,member.second));
    if(value.flags!=(epoch::flagActive|epoch::flagAdmissionClosed)||epoch::admitsNew(value))throw std::runtime_error("preclose owner membership not closed");
    logFile->event("world_preclose_admission_closed",common()+","+brokerFields(value)+",\"changes_mod_admission\":true,\"writes_gameplay\":false,\"native_operation_not_started\":true");}
  }

 }catch(...){failure();}}
 const auto result=originalPreclose(root);
 if(call&&valid){const ObserverLastError preserve;try{
  progress::mark("after_preclose",call);
  // Static complete CFG returns a completion bool; it never frees its root.
  const auto after=snapshot(address+0xcc4218,address);const auto pred=after.prediction?snapshot(after.prediction):Sample{};
  logFile->event("world_preclose_end",common()+","+fields(after)+",\"native_result\":"+hex(result)+",\"completed\":"+((result&255)?"true":"false")+",\"before_count\":"+std::to_string(before.count)+",\"prediction_count\":"+std::to_string(pred.count)+",\"before_prediction_count\":"+std::to_string(prediction.count)+",\"arguments_unchanged\":true,\"return_unchanged\":true,\"root_not_freed_by_function\":true");
 }catch(...){failure();}}
 return result;
}
inline std::array<Hook,8> hooks{{
 {0x977f00,"40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 B8 FE FF FF",reinterpret_cast<void*>(&create),reinterpret_cast<void**>(&originalCreate)},
 {0x996430,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 40 80 39 03 48",reinterpret_cast<void*>(&destroyRoot),reinterpret_cast<void**>(&originalRoot)},
 {0x997d20,"48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC C8 00",reinterpret_cast<void*>(&cleanup),reinterpret_cast<void**>(&originalCleanup)},
 {0x952c60,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20",reinterpret_cast<void*>(&storage),reinterpret_cast<void**>(&originalStorage)},
 {0x9953e0,"48 89 5c 24 10 48 89 74 24 18 55 57 41 54 41 55 41 56 48 8d ac 24",reinterpret_cast<void*>(&preclose),reinterpret_cast<void**>(&originalPreclose)},
 {0xf99970,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 33 ED 48 8B DA 48 8B F1",reinterpret_cast<void*>(&inputBackendDestroy),reinterpret_cast<void**>(&originalInputBackendDestroy)},
 {renderRva,renderPattern,reinterpret_cast<void*>(&progress::render),reinterpret_cast<void**>(&progress::originalRender)},
 {0xf39d0,"40 53 48 83 EC 20 48 8B 19 48 85 DB 0F 84 4D 01",reinterpret_cast<void*>(&progress::owner),reinterpret_cast<void**>(&progress::originalOwner)}
}};
inline bool selected(const Hook& h){if(h.rva==0xf99970)return inputExitRequested;if(h.rva==renderRva||h.rva==0xf39d0)return progress::requested;return h.rva!=0x9953e0||precloseRequested;}
inline void install(){
 for(auto& h:hooks)if(selected(h)){const auto b=parsePattern(h.pattern);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+h.rva),b.size(),b)!=std::vector<size_t>{0})throw std::runtime_error("World lifecycle signature differs");}
 for(auto& h:hooks)if(selected(h)){if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+h.rva),h.target,h.original)!=MH_OK)throw std::runtime_error("World lifecycle create hook failed");h.created=true;}
 for(auto& h:hooks)if(selected(h)&&MH_QueueEnableHook(reinterpret_cast<void*>(baseAddress+h.rva))!=MH_OK)throw std::runtime_error("World lifecycle queue hook failed");
 if(epochShadow)logFile->event("world_epoch_shadow_enabled","\"adoption_sealed\":true,\"gameplay_connected\":false,\"capacity\":8");
 if(brokerRequested){auto module=GetModuleHandleW(L"evr_diagnostic.dll");brokerCall=module?reinterpret_cast<epoch::Call>(GetProcAddress(module,"evrWorldEpochCall")):nullptr;
  provider=broker(epoch::request(epoch::Operation::Register));if(provider.flags!=epoch::flagActive||!provider.provider||provider.worlds)throw std::runtime_error("World broker was not clean at startup");
  logFile->event("world_broker_enabled",brokerFields(provider)+",\"single_process_provider\":true,\"gameplay_connected\":false");}
 if(progress::requested){const auto m=GetModuleHandleW(L"evr_diagnostic.dll");progress::armLayer=m?reinterpret_cast<progress::Arm>(GetProcAddress(m,"evrArmExitProgress")):nullptr;if(!progress::armLayer)throw std::runtime_error("exit progress export missing");}
 accepting=true;if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("World lifecycle enable failed");
 for(auto& h:hooks)if(selected(h))logFile->event(h.rva==0xf99970?"input_backend_exit_hook":h.rva==0x9953e0?"world_preclose_hook":((h.rva==renderRva||h.rva==0xf39d0)?"world_exit_progress_hook":"world_lifecycle_hook"),"\"rva\":"+hex(h.rva)+",\"pattern\":"+jsonString(h.pattern)+",\"read_only\":true");
}
inline void disable(){
 accepting=false;bool disabled=true;
 for(auto& h:hooks)if(h.created){const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+h.rva));if(status!=MH_OK&&status!=MH_ERROR_DISABLED)disabled=false;}
 const auto deadline=GetTickCount64()+2000;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 bool restored=true;for(auto& h:hooks)if(selected(h)){const auto b=parsePattern(h.pattern);restored=restored&&scan(reinterpret_cast<const uint8_t*>(baseAddress+h.rva),b.size(),b)==std::vector<size_t>{0};}
 if(progress::requested)logFile->event("world_exit_progress_removed",progress::counts()+",\"owner_calls\":"+std::to_string(progress::owners.load())+",\"restored\":"+(restored?"true":"false"));
 logFile->event("world_lifecycle_removed","\"calls\":"+std::to_string(calls.load())+",\"callbacks\":"+std::to_string(callbacks.load())+",\"failures\":"+std::to_string(failures.load())+",\"disabled\":"+(disabled?"true":"false")+",\"restored\":"+(restored?"true":"false")+",\"trampolines_retained\":true");
 if(brokerRequested&&provider.provider){
  if(disabled&&restored&&!callbacks.load()&&!failures.load()&&brokerDrained()){
   auto stopped=broker(epoch::request(epoch::Operation::Stop,provider));logFile->event("world_broker_stopped",brokerFields(stopped));
  }else{failure();logFile->event("world_broker_faulted","\"storage_retained\":true,\"new_work_disabled\":true");}
 }
 if(!disabled||!restored||callbacks.load()||failures.load())throw std::runtime_error("World lifecycle evidence/cleanup failed; storage retained");
}
inline void finishInputExit() noexcept {
 unsigned expected=0;if(!inputExitCompletion.compare_exchange_strong(expected,1))return;
 bool cleanupAttempted=false;
 try{
  if(!exitReady()||callbacks.load())throw std::runtime_error("input owner exit did not follow drained native Worlds");
  cleanupAttempted=true;disable();
  logFile->event("world_lifecycle_observation_complete","\"result\":0,\"native_input_owner_returned\":true,\"synchronous_exit_finalization\":true,\"thread\":"+std::to_string(GetCurrentThreadId())+",\"code_and_trampolines_retained\":true");
  inputExitCompletion=2;
 }catch(const std::exception& e){
  if(!cleanupAttempted)try{disable();}catch(...){}
  try{logFile->event("world_lifecycle_observation_failure","\"reason\":"+jsonString(e.what())+",\"synchronous_exit_finalization\":true");}catch(...){}
  inputExitCompletion=3;
 }
}
}
