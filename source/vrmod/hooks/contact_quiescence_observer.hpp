#pragma once
// Observe the exact post-command-flush boundary after native task completion.
// Default is evidence only. Explicit finite recycling touches only MOD ledgers.
namespace contactQuiescenceObserver {
using Flush=void(__fastcall*)(void*);
inline Flush original=nullptr;
inline bool enabled=false;inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},seen{0},samples{0},omitted{0},failures{0};
inline std::mutex mutex;
struct Last {uintptr_t world=0;uint32_t actor=0,sequence=0,item=0;int64_t trigger=0;bool active=false;unsigned destroyed=0;bool quiet=false;};
inline Last previous;
inline bool retirementRequested=false;
inline motion::ContactBoundaryContinuity continuity;
// Gate state lives in the serial-protected reusable watch pool.
inline unsigned retired=0,expectedActions=2;
// Explicit finite bridge trial: keep one already quiescent real action until
// its native World ends. This is not an attack-in-flight exit claim.
inline bool terminalRequested=false,ownerHandoffRequested=false;inline std::atomic<bool> terminalHeld{false};
inline unsigned terminalRetired=0;
inline bool interruptedRequested=false;inline std::atomic<bool> interruptedHeld{false};

// Read-only observation at the actual native Actor deletion entry. Native
// owner arguments authorize this snapshot, never a stored Actor pointer. Zero
// counts here are evidence, NOT a lock, pin, drain or permission to free a scope.
inline bool observeActorOwnerBoundary(const contactLifetimeObserver::ActorOwnerEvidence& e){
 if(!e.beforeValid||!e.watch.used)throw std::runtime_error("Actor boundary lacks native owner");
 const auto world=e.watch.key.world;
 const auto registry=read<uintptr_t>(world),tasks=read<uintptr_t>(registry+0xcc38e0);
 if(!tasks)throw std::runtime_error("Actor owner task list missing");
 auto taskCounts=[&]{return std::array<uint64_t,3>{read<uint32_t>(tasks+0x1c8),read<uint64_t>(tasks+0x1a8),read<uint64_t>(tasks+0x1c0)};};
 const auto before=taskCounts();
 const auto selectors=motionEventSelection::callbacks.load(),queries=motionColliderApply::outstanding.load(),lookups=motionColliderApply::lookupCallbacks.load(),flushes=callbacks.load();
 const auto destructors=contactLifetimeObserver::callbacks.load();
 const auto after=taskCounts();
 const bool stable=before==after&&registry==read<uintptr_t>(world)&&tasks==read<uintptr_t>(registry+0xcc38e0);
 const bool stopped=combatEpochGate::actorStopped();
 std::ostringstream o;o<<e.fields<<",\"task_list\":"<<hex(tasks)<<",\"task_counts_before\":["<<before[0]<<','<<before[1]<<','<<before[2]<<"],\"task_counts_after\":["<<after[0]<<','<<after[1]<<','<<after[2]<<"]"
 <<",\"selector_callbacks\":"<<selectors<<",\"query_scopes\":"<<queries<<",\"lookup_callbacks\":"<<lookups<<",\"flush_callbacks\":"<<flushes<<",\"destructor_callbacks\":"<<destructors
 <<",\"value_hold_policy\":2,\"global_drain_required_before_retire\":true,\"stable\":"<<(stable?"true":"false")<<",\"admission_stopped\":"<<(stopped?"true":"false")<<",\"before_original\":true,\"waited_for_worker\":false,\"claims_native_drain\":false,\"writes_gameplay\":false";
 logFile->event("contact_actor_native_boundary",o.str());
 return motion::ActorValueHoldBoundary{before,after,stable,stopped,selectors,queries,lookups,flushes,destructors}.permitsHold();
}
// Only the real native Actor destructor returning with exact entity absence can
// replace the normal-idle proof in this explicit interrupted trial. This does
// not retire values: exact server Ended and all consumer drains are still needed.
inline void observeInterruptedActorReturn(const contactLifetimeObserver::ActorOwnerEvidence& e){
 if(!interruptedRequested)return;
 if(interruptedHeld.load()||terminalHeld.load()||!e.beforeValid||!e.activeActor||!e.canHoldValues||!(e.nativeAction==e.watch.authority.action)||!combatEpochGate::actorStopped())throw std::runtime_error("interrupted Actor return lacks active exact native owner proof");
 std::scoped_lock locks(motionEventSelection::retimeMutex,motionColliderApply::mutex,contactLifetimeObserver::mutex);
 const auto current=contactLifetimeObserver::lifetimes.snapshot(e.watch.token);
 if(!current.used||!(current.authority.action==e.nativeAction)||!(current.collider==e.watch.key)||current.destroying||!current.ended||motionEventSelection::callbacks.load()||motionColliderApply::outstanding.load()||motionColliderApply::lookupCallbacks.load()||callbacks.load()||!motionEventSelection::retimeLedger.owns(e.nativeAction)||!motionColliderApply::lease.owns(e.watch.key)||motionEventSelection::appended!=1||motionEventSelection::suppressed!=1)throw std::runtime_error("interrupted trial requires restored scopes and completed Impact/suppression before Actor return");
 motionEventSelection::restrictToRetained(e.nativeAction);
 logFile->event("contact_interrupted_actor_held","\"watch_slot\":"+std::to_string(e.watch.token.slot)+",\"watch_serial\":"+std::to_string(e.watch.token.serial)+",\"world\":"+hex(e.watch.key.world)+",\"actor\":"+std::to_string(e.nativeAction.entity)+",\"entity\":"+std::to_string(e.watch.key.entity)+",\"owner_call\":"+std::to_string(e.ownerCall)+",\"sequence\":"+std::to_string(e.nativeAction.sequence)+",\"item\":"+std::to_string(e.nativeAction.item)+",\"trigger\":"+std::to_string(e.nativeAction.trigger)+",\"native_actor_returned_absent\":true,\"actor_was_active\":true,\"Impact_ended\":true,\"late_event_suppressed\":true,\"normal_idle_observed\":false,\"value_ledgers_retained\":true,\"native_memory_pinned\":false"+combatEpochGate::stampFields(e.nativeAction.epoch));
 interruptedHeld.store(true,std::memory_order_release);
}

enum class RetirementTrial {NormalRepeat,PostImpactCancel};
inline void configureRetirement(RetirementTrial trial=RetirementTrial::NormalRepeat){
 std::scoped_lock locks(motionEventSelection::retimeMutex,motionColliderApply::mutex,contactLifetimeObserver::mutex);
 if(!motionEventSelection::retimeLedger.configureLimit(1)||!motionColliderApply::lease.configureLimit(1)||!contactLifetimeObserver::lifetimes.configureLimit(1))throw std::runtime_error("one-slot retirement configured after ownership began");
 retirementRequested=true;expectedActions=trial==RetirementTrial::PostImpactCancel?1:2;
 logFile->event("contact_recycle_enabled",std::string("\"event_slots\":1,\"collider_slots\":1,\"watch_slots\":1,\"serial_tokens\":true,\"expected_actions\":")+std::to_string(expectedActions)+",\"post_Impact_cancel_trial\":"+(trial==RetirementTrial::PostImpactCancel?"true":"false")+",\"writes_gameplay\":false,\"changes_mod_ledgers\":true,\"finite_trial\":true");
}
inline void recycle(const motion::ContactBoundary& b,const std::array<contactLifetimeObserver::Watch,8>& owned){
 if(!retirementRequested)return;
 if(!continuity.observe(b))throw std::runtime_error("retirement World/task/tick/counter continuity changed");
 for(size_t i=0;i<owned.size();++i){const auto& w=owned[i];if(!w.used)continue;
  std::scoped_lock locks(motionEventSelection::retimeMutex,motionColliderApply::mutex,contactLifetimeObserver::mutex);
  auto& pool=contactLifetimeObserver::lifetimes;const auto current=pool.snapshot(w.token);
  if(!current.used||!(current.collider==w.key)||!(current.authority.action==w.authority.action))throw std::runtime_error("stale retirement watch snapshot");
  const bool ready=pool.ready(w.token,b);
  if(pool.faulted(w.token))throw std::runtime_error("retirement Actor lifetime discontinuity");
  if(!ready)continue;
  if(terminalRequested){
   if(interruptedRequested)throw std::runtime_error("interrupted Actor trial reached normal idle before exit");
   if(!terminalHeld){motionEventSelection::restrictToRetained(w.authority.action);terminalHeld=true;
    logFile->event("contact_terminal_retirement_held","\"watch_slot\":"+std::to_string(w.token.slot)+",\"watch_serial\":"+std::to_string(w.token.serial)+",\"world\":"+hex(w.key.world)+",\"actor\":"+std::to_string(w.authority.action.entity)+",\"entity\":"+std::to_string(w.key.entity)+",\"normal_idle_observed\":true,\"value_ledgers_retained\":true,\"native_memory_pinned\":false"+combatEpochGate::stampFields(w.key.epoch));
   }continue;
  }
  const auto present=contactLifetimeObserver::presence(b.world,w.key.entity);
  if(present.present||contactLifetimeObserver::callbacks.load()||motionEventSelection::callbacks.load()||motionColliderApply::outstanding.load()||!motionEventSelection::retimeLedger.owns(w.authority.action)||!motionColliderApply::lease.owns(w.key))throw std::runtime_error("retirement ownership/quiescence recheck failed");
  const auto beforeEvents=motionEventSelection::retimeLedger.size(),beforeColliders=motionColliderApply::lease.size();
  if(!motionEventSelection::retimeLedger.retireAfterQuiescence(w.authority.action)||!motionColliderApply::lease.retireAfterQuiescence(w.key)||!pool.retire(w.token))throw std::runtime_error("retirement exact pair commit failed");
  contactLifetimeObserver::watches[w.token.slot]={};
  ++retired;std::ostringstream o;o<<"\"watch_slot\":"<<w.token.slot<<",\"watch_serial\":"<<w.token.serial<<",\"watch_slots_remaining\":"<<pool.size()<<",\"world\":"<<hex(b.world)<<",\"actor\":"<<b.actor<<",\"entity\":"<<w.key.entity<<",\"sequence\":"<<w.authority.action.sequence<<",\"item\":"<<w.authority.action.item<<",\"trigger\":"<<w.authority.action.trigger<<",\"call\":"<<unsigned(w.authority.action.call)<<",\"sub\":"<<unsigned(w.authority.action.sub)<<",\"tick\":"<<b.tick<<",\"entity_counter\":"<<b.entityCounter<<",\"retired\":"<<retired<<",\"events_before\":"<<beforeEvents<<",\"colliders_before\":"<<beforeColliders<<",\"events_after\":"<<motionEventSelection::retimeLedger.size()<<",\"colliders_after\":"<<motionColliderApply::lease.size()<<",\"native_Impact_absent\":true,\"actor_cleared\":true,\"quiet\":true,\"writes_gameplay\":false,\"changes_mod_ledgers\":true";
  o<<combatEpochGate::stampFields(b.epoch)<<",\"watch_epoch_serial\":"<<w.key.epoch.serial;logFile->event("contact_recycled",o.str());
 }
}

inline bool retiresAnyLedger(){return retirementRequested||blockedActionLifetime::requested;}
inline void configureBlockedRetirement(){
 std::lock_guard<std::mutex> lock(motionEventSelection::retimeMutex);
 if(retirementRequested||!motionEventSelection::retimeLedger.configureLimit(1))throw std::runtime_error("event-only retirement configured too late");
 blockedActionLifetime::requested=true;
 logFile->event("blocked_event_recycle_enabled","\"event_slots\":1,\"expected_actions\":1,\"expected_Impacts\":0,\"finite_trial\":true,\"writes_gameplay\":false,\"changes_mod_ledgers\":true");
}
inline void recycleBlocked(const motion::ContactBoundary& b){
 if(!blockedActionLifetime::requested)return;
 if(!continuity.observe(b))throw std::runtime_error("blocked event World/tick/counter continuity changed");
 std::scoped_lock locks(motionEventSelection::retimeMutex,blockedActionLifetime::mutex);
 auto& pool=blockedActionLifetime::lifetimes;const auto token=blockedActionLifetime::token;const auto& key=blockedActionLifetime::key;
 if(!blockedActionLifetime::registered)return;
 const bool ready=pool.ready(token,b);
 if(pool.faulted(token)||blockedActionLifetime::colliders||blockedActionLifetime::failures.load())throw std::runtime_error("blocked event lifetime evidence invalid");
 if(!ready)return;
 if(motionEventSelection::callbacks.load()||motionColliderApply::outstanding.load()||!motionEventSelection::retimeLedger.blocked(key))throw std::runtime_error("blocked event still in use or not blocked");
 const auto count=motionEventSelection::retimeLedger.size();
 if(!motionEventSelection::retimeLedger.retireAfterQuiescence(key)||!pool.retire(token))throw std::runtime_error("blocked event exact retirement failed");
 blockedActionLifetime::registered=false;++blockedActionLifetime::retired;
 std::ostringstream o;o<<"\"watch_slot\":"<<token.slot<<",\"watch_serial\":"<<token.serial<<",\"watch_slots_remaining\":"<<pool.size()<<",\"world\":"<<hex(b.world)<<",\"actor\":"<<b.actor<<",\"sequence\":"<<key.sequence<<",\"item\":"<<key.item<<",\"trigger\":"<<key.trigger<<",\"call\":"<<unsigned(key.call)<<",\"sub\":"<<unsigned(key.sub)<<",\"tick\":"<<b.tick<<",\"events_before\":"<<count<<",\"events_after\":"<<motionEventSelection::retimeLedger.size()<<",\"suppressions\":"<<blockedActionLifetime::suppressions<<",\"matching_colliders\":"<<blockedActionLifetime::colliders<<",\"actor_cleared\":true,\"quiet\":true,\"writes_gameplay\":false,\"changes_mod_ledgers\":true";
 logFile->event("blocked_event_recycled",o.str());
}
constexpr uintptr_t rva=0xa17490,callerRva=0xa2002f;
constexpr const char* pattern="40 53 41 54 48 83 ec 38 45 33 e4 48 8b d9 41 8b d4";
inline void observe(uintptr_t world){
 // This explicit one-action bridge has already observed its normal idle.
 // The player can disappear before server preclose; retain values, never poll
 // that completed Actor again. Other retirement modes continue normal fences.
 if(terminalRequested&&(terminalHeld.load()||interruptedHeld.load()))return;
 const auto epoch=combatEpochGate::actorFenceStamp(world);if(combatEpochGate::requested&&!epoch.valid())return;
 std::array<contactLifetimeObserver::Watch,8> owned;
 {std::lock_guard<std::mutex> lock(contactLifetimeObserver::mutex);owned=contactLifetimeObserver::watches;}
 uint32_t actor=0;unsigned watches=0,destroyed=0;
 for(const auto& w:owned)if(w.used&&w.key.world==world&&(!combatEpochGate::requested||w.key.epoch==epoch)){if(actor&&actor!=w.authority.action.entity)throw std::runtime_error("multiple local actors at finite fence");actor=w.authority.action.entity;++watches;destroyed+=w.ended;}
 const auto blocked=blockedActionLifetime::snapshot();
 if(blocked.valid()&&blocked.world==world&&(!combatEpochGate::requested||blocked.epoch==epoch)){if(actor&&actor!=blocked.entity)throw std::runtime_error("blocked/contact actors differ");actor=blocked.entity;}
 if(!actor)return;++seen;
 nativeImpactComponents::Binding binding;
 {std::lock_guard<std::mutex> lock(nativeImpactComponents::mutex);const auto i=nativeImpactComponents::bindings.find(world);if(i==nativeImpactComponents::bindings.end())throw std::runtime_error("fence World binding missing");binding=i->second;}
 const auto entity=nativeImpactComponents::entityAtWorld(world,actor);
 if(binding.actor<0||binding.actorStride<3104)throw std::runtime_error("fence Actor type not learned");
 const auto pointer=nativeImpactComponents::component(entity,unsigned(binding.actor),binding.actorStride);
 if(!pointer)throw std::runtime_error("fence Actor absent");
 const auto bytes=read<std::array<uint8_t,3104>>(pointer);uint32_t sequence=0,item=0;int64_t trigger=0;
 std::memcpy(&sequence,bytes.data(),4);std::memcpy(&trigger,bytes.data()+8,8);std::memcpy(&item,bytes.data()+0x38,4);const bool active=(bytes[0x1b1]&1)!=0;
 const auto registry=read<uintptr_t>(world);const auto taskList=read<uintptr_t>(registry+0xcc38e0);
 if(!taskList)throw std::runtime_error("fence native task list absent");
 const auto running=read<uint32_t>(taskList+0x1c8);const auto queued=read<uint64_t>(taskList+0x1a8),completed=read<uint64_t>(taskList+0x1c0);
 const auto queues=read<uintptr_t>(world+0x3d6020),count=read<uint64_t>(world+0x3d6028);
 if(!queues||!count||count>1024)throw std::runtime_error("fence command queue bounds");
 uint64_t commandBytes=0,commandCount=0;for(uint64_t i=0;i<count;++i){commandBytes+=read<uint32_t>(queues+i*0x80+0x18);commandCount+=read<uint32_t>(queues+i*0x80+0x1c);}
 const auto tick=read<uint32_t>(world+0x504),entityCounter=read<uint32_t>(world+0x4c0);const auto kind=read<uint8_t>(world+8);
 const bool stable=registry==read<uintptr_t>(world)&&bytes==read<std::array<uint8_t,3104>>(pointer)&&taskList==read<uintptr_t>(registry+0xcc38e0)&&queues==read<uintptr_t>(world+0x3d6020)&&count==read<uint64_t>(world+0x3d6028);
 if(!stable)throw std::runtime_error("fence inputs changed");
 const auto selectors=motionEventSelection::callbacks.load(),scopes=motionColliderApply::outstanding.load();
 const bool quiet=running==0&&queued==0&&completed==0&&commandBytes==0&&commandCount==0&&selectors==0&&scopes==0;
 std::lock_guard<std::mutex> lock(mutex);const bool changed=previous.world!=world||previous.actor!=actor||previous.sequence!=sequence||previous.item!=item||previous.trigger!=trigger||previous.active!=active||previous.destroyed!=destroyed||previous.quiet!=quiet;
 previous={world,actor,sequence,item,trigger,active,destroyed,quiet};
 if(active||changed){
 if(samples.load()>=1024){++omitted;}else{
 std::ostringstream o;o<<"\"sample\":"<<++samples<<",\"world\":"<<hex(world)<<",\"actor\":"<<actor<<",\"world_kind\":"<<unsigned(kind)<<",\"tick\":"<<tick<<",\"registry\":"<<hex(registry)<<",\"entity_counter\":"<<entityCounter<<",\"sequence\":"<<sequence<<",\"item\":"<<item<<",\"trigger\":"<<trigger<<",\"active\":"<<(active?"true":"false")<<",\"actor_bytes\":"<<jsonString(nativeImpactComponents::asHex(bytes))
 <<",\"task_list\":"<<hex(taskList)<<",\"running\":"<<running<<",\"queued\":"<<queued<<",\"completed_pending\":"<<completed<<",\"command_queues\":"<<count<<",\"command_bytes\":"<<commandBytes<<",\"command_count\":"<<commandCount<<",\"selector_callbacks\":"<<selectors<<",\"query_scopes\":"<<scopes<<",\"quiet\":"<<(quiet?"true":"false")<<",\"watched\":"<<watches<<",\"destroyed\":"<<destroyed<<",\"thread\":"<<GetCurrentThreadId()<<",\"milliseconds\":"<<GetTickCount64()<<",\"read_only\":true,\"retires_ledger_entries\":"<<(retiresAnyLedger()?"true":"false");
 o<<combatEpochGate::stampFields(epoch);logFile->event("contact_quiescence_sample",o.str());
 }}
 motion::ContactBoundary b;b.world=world;b.registry=registry;b.taskList=taskList;b.actor=actor;b.tick=tick;b.entityCounter=entityCounter;b.worldKind=kind;b.action=motionEventSelection::actionKey(bytes,world,actor);b.active=active;b.running=running;b.queued=queued;b.completed=completed;b.commandBytes=commandBytes;b.commandCount=commandCount;b.selectors=selectors;b.queries=scopes;
 b.epoch=epoch;
 combatWorldBridge::observe(combatWorldBridge::Stage::Flush,0,world);
 recycle(b,owned);recycleBlocked(b);
 combatEpochGate::exerciseClose(world,actor,tick,motionColliderApply::applied.load(),watches,active,quiet&&!destroyed);
}
__declspec(noinline) void __fastcall flush(void* world){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 struct Callback{Callback(){++callbacks;}~Callback(){--callbacks;}} callback;
 original(world);const ObserverLastError preserve;
 if(!accepting.load()||caller!=callerRva)return;
 try{observe(reinterpret_cast<uintptr_t>(world));}catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("contact_quiescence_failure","\"reason\":"+jsonString(e.what()));}
}
inline void install(){
 const auto bytes=parsePattern(pattern);const auto call=baseAddress+callerRva-5;
 if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)throw std::runtime_error("native flush signature/caller differs");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&flush),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("native flush observer hook failed");
 enabled=true;accepting=true;logFile->event("contact_quiescence_enabled","\"rva\":\"0xa17490\",\"caller\":\"0xa2002f\",\"pattern\":"+jsonString(pattern)+",\"read_only\":true,\"retires_ledger_entries\":"+(retiresAnyLedger()?"true":"false"));
}
inline bool stop(){
 accepting=false;if(!enabled)return false;
 if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("native flush observer disable failed");enabled=false;
 const auto deadline=GetTickCount64()+1000;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 const auto bytes=parsePattern(pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
 logFile->event("contact_quiescence_removed","\"seen\":"+std::to_string(seen.load())+",\"samples\":"+std::to_string(samples.load())+",\"omitted\":"+std::to_string(omitted.load())+",\"callbacks\":"+std::to_string(callbacks.load())+",\"failures\":"+std::to_string(failures.load())+",\"restored\":"+(restored?"true":"false")+",\"read_only\":true,\"retires_ledger_entries\":"+(retiresAnyLedger()?"true":"false"));
 if(!restored||callbacks.load()||failures.load())throw std::runtime_error("native flush evidence failed; hook restored if possible");
 return true;
}
inline void verifyRetirement(){
 if(retirementRequested){
  std::scoped_lock locks(motionEventSelection::retimeMutex,motionColliderApply::mutex,contactLifetimeObserver::mutex);
  const auto events=motionEventSelection::retimeLedger.size(),colliders=motionColliderApply::lease.size();
  logFile->event("contact_recycle_removed","\"retired\":"+std::to_string(retired)+",\"watch_slots_remaining\":"+std::to_string(contactLifetimeObserver::lifetimes.size())+",\"events_remaining\":"+std::to_string(events)+",\"colliders_remaining\":"+std::to_string(colliders)+",\"continuity_fault\":"+(continuity.faulted()?"true":"false")+",\"writes_gameplay\":false,\"changes_mod_ledgers\":true");
  if(retired!=expectedActions||events||colliders||contactLifetimeObserver::lifetimes.size()||continuity.faulted())throw std::runtime_error("one-slot lifetime recycling incomplete");
 }
 if(blockedActionLifetime::requested){
  std::scoped_lock locks(motionEventSelection::retimeMutex,blockedActionLifetime::mutex);
  const auto remaining=motionEventSelection::retimeLedger.size();
  const auto valid=!blockedActionLifetime::registered&&!blockedActionLifetime::lifetimes.size()&&blockedActionLifetime::registrations==1&&blockedActionLifetime::suppressions==1&&blockedActionLifetime::retired==1&&blockedActionLifetime::colliders==0&&!blockedActionLifetime::failures.load()&&!remaining&&!continuity.faulted();
  logFile->event("blocked_event_recycle_removed","\"registrations\":"+std::to_string(blockedActionLifetime::registrations)+",\"watch_slots_remaining\":"+std::to_string(blockedActionLifetime::lifetimes.size())+",\"suppressions\":"+std::to_string(blockedActionLifetime::suppressions)+",\"retired\":"+std::to_string(blockedActionLifetime::retired)+",\"matching_colliders\":"+std::to_string(blockedActionLifetime::colliders)+",\"failures\":"+std::to_string(blockedActionLifetime::failures.load())+",\"events_remaining\":"+std::to_string(remaining)+",\"continuity_fault\":"+(continuity.faulted()?"true":"false")+",\"writes_gameplay\":false,\"changes_mod_ledgers\":true");
  if(!valid)throw std::runtime_error("event-only lifetime retirement incomplete");
 }
 if(terminalRequested&&(terminalRetired!=1||!(interruptedRequested?interruptedHeld.load():terminalHeld.load())))throw std::runtime_error("terminal World transaction not observed");
}
inline void disable(){if(stop())verifyRetirement();}
// After the finite renderer finishes: input is restored, or explicit native
// owner handoff has stopped gameplay readers. Retain suppression until server Ended.
inline void finishTerminalWorld(){
 if(!terminalRequested)return;
 const bool inputRestored=controllerUiFixture::done.load()&&!controllerUiFixture::installed.load();
 const bool inputPending=ownerHandoffRequested&&controllerUiFixture::backendExitRequested&&controllerUiFixture::readersStopped.load()&&controllerUiFixture::inputModulePinned&&controllerUiFixture::installed.load()&&!controllerUiFixture::done.load()&&!inputUi::enabled&&!inputUi::callbacks.load();
 if(!(interruptedRequested?interruptedHeld.load():terminalHeld.load())||!(ownerHandoffRequested?inputPending:inputRestored)||controllerUiFixture::callbacks.load()||controllerUiFixture::failed.load())throw std::runtime_error("terminal trial needs proven input drain or explicit reader-stopped native owner handoff");
 logFile->event("contact_terminal_wait","\"input_restored\":"+std::string(inputRestored?"true":"false")+",\"input_owner_pending\":"+(inputPending?"true":"false")+",\"actor_idle_previously_observed\":"+std::string(interruptedRequested?"false":"true")+",\"actor_interrupted_end_observed\":"+(interruptedHeld.load()?"true":"false")+",\"suppression_retained\":true,\"requires_normal_world_exit\":true");
 motion::WorldEndBoundary boundary;
 const auto deadline=GetTickCount64()+120000;
 while(!combatEpochGate::endedContact(boundary)&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 if(!boundary.owner.valid())throw std::runtime_error("terminal World native root did not end within trial budget");
 logFile->event("contact_terminal_world_ended",combatWorldBridge::fields(boundary.observed)+",\"saved_lease\":"+std::to_string(boundary.leaseHandle)+",\"before_consumer_shutdown\":true,\"reads_ecs\":false");
 // These operations close admission AND restore hooks; zero counts alone are
 // insufficient. All allocations/code remain pinned while callbacks unwind.
 motionEventSelection::disable();motionColliderApply::drain();contactLifetimeObserver::disable();stop();
 {
  std::scoped_lock locks(motionEventSelection::retimeMutex,motionColliderApply::mutex,contactLifetimeObserver::mutex);
  if(!combatEpochGate::endedContact(boundary))throw std::runtime_error("terminal retained World proof changed after hook drain");
  boundary.consumersStopped=!motionEventSelection::enabled&&!motionEventSelection::accepting.load()&&!motionColliderApply::accepting&&!motionColliderApply::lookupEnabled&&!contactLifetimeObserver::enabled&&!contactLifetimeObserver::accepting.load()&&!enabled&&!accepting.load();
  boundary.selectors=motionEventSelection::callbacks.load();boundary.queries=motionColliderApply::outstanding.load()+motionColliderApply::lookupCallbacks.load();boundary.destructors=contactLifetimeObserver::callbacks.load();boundary.flushes=callbacks.load();
  if(motionEventSelection::failures.load()||motionColliderApply::failures.load()||contactLifetimeObserver::failures.load()||failures.load()||continuity.faulted()||!boundary.permits(boundary.owner))throw std::runtime_error("terminal World consumers not safely drained");
  auto& pool=contactLifetimeObserver::lifetimes;
  if(pool.size()!=1||motionEventSelection::retimeLedger.size()!=1||motionColliderApply::lease.size()!=1)throw std::runtime_error("terminal trial requires one real retained action");
  for(auto& watch:contactLifetimeObserver::watches)if(watch.used){
   const auto saved=watch;
   if(motion::retireContactAfterWorldEnd(pool,watch.token,motionEventSelection::retimeLedger,motionColliderApply::lease,boundary)!=motion::WorldRetirementResult::Retired)throw std::runtime_error("terminal World exact ledger transaction rejected");
   watch={};++retired;++terminalRetired;
   std::ostringstream o;o<<"\"watch_slot\":"<<saved.token.slot<<",\"watch_serial\":"<<saved.token.serial<<",\"world\":"<<hex(saved.key.world)<<",\"actor\":"<<saved.authority.action.entity<<",\"entity\":"<<saved.key.entity<<",\"sequence\":"<<saved.authority.action.sequence<<",\"item\":"<<saved.authority.action.item<<",\"trigger\":"<<saved.authority.action.trigger<<",\"call\":"<<unsigned(saved.authority.action.call)<<",\"sub\":"<<unsigned(saved.authority.action.sub)<<",\"events_after\":"<<motionEventSelection::retimeLedger.size()<<",\"colliders_after\":"<<motionColliderApply::lease.size()<<",\"watches_after\":"<<pool.size()<<",\"native_phase\":"<<boundary.observed.phase<<",\"lease_pins\":"<<boundary.observed.pins<<",\"consumer_hooks_stopped\":true,\"callbacks_zero\":true,\"reads_ecs\":false,\"writes_gameplay\":false,\"changes_mod_ledgers\":true"<<combatEpochGate::stampFields(saved.key.epoch);
   logFile->event("contact_terminal_recycled",o.str());
  }
 }
 verifyRetirement();
 logFile->event("contact_terminal_complete","\"retired\":"+std::to_string(terminalRetired)+",\"reads_destroyed_memory\":false,\"active_attack_exit_claim\":false");
}
}
