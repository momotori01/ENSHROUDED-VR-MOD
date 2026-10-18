#pragma once
// Explicit finite enforcement adapter. Broker leases retain MOD values only;
// the original native callback remains responsible for ECS lifetime/scheduling.
namespace combatEpochGate {
inline std::atomic<bool> requested{false};inline std::mutex mutex;inline motion::CombatEpochClient client;
enum class Stage:size_t {Input,Poll,Ticket,Pose,Event,Append,Suppress,Collider,Flush,Scope,Count};
inline constexpr const char* names[]{"input","poll","ticket","pose","event","append","suppress","collider","flush","scope"};
inline std::array<uint64_t,size_t(Stage::Count)> checks{},denied{};inline bool initialized=false;
inline motion::ActorAdmission actorAdmission;
inline void install(const std::filesystem::path& path){
 const auto file=path/"world-combat-epoch-strict.txt";if(!std::filesystem::exists(file))return;
 std::lock_guard<std::mutex> lock(mutex);requested=true;
 std::ifstream f(file);std::string magic;uint64_t provider=0;f>>magic>>provider;
 std::array<epoch::Response,2> expected{};
 for(size_t i=0;i<2;++i){auto& r=expected[i];r.provider=provider;r.status=0;r.phase=r.flags=1;f>>r.world>>r.registry>>r.root>>r.kind>>r.slot>>r.serial;}
 f>>std::ws;if(!f.eof()||magic!="EVR_COMBAT_EPOCH_STRICT_1"||!provider||expected[0].kind!=2||expected[1].kind!=0||expected[0].slot>=8||expected[1].slot>=8)throw std::runtime_error("invalid strict combat epoch policy");
 const auto module=GetModuleHandleW(L"evr_diagnostic.dll");const auto fn=module?reinterpret_cast<epoch::Call>(GetProcAddress(module,"evrWorldEpochCall")):nullptr;
 if(!client.open(fn,expected[0],expected[1]))throw std::runtime_error("strict combat World lease acquisition failed");if(!actorAdmission.open(client.worlds()))throw std::runtime_error("Actor admission binding already used or invalid");initialized=true;
 const auto state=client.sample();const auto saved=client.leaseIdentities();
 for(size_t i=0;i<2;++i){auto r=i==0?state.input:state.contact;r.handle=saved[i].handle;logFile->event("combat_epoch_gate_pinned",combatWorldBridge::fields(r));}
 logFile->event("combat_epoch_gate_enabled","\"roles\":2,\"enforced\":true,\"ecs_pinned\":false,\"atomic_native_permission\":false");
}
template<class Decision> inline bool checked(Stage stage,uintptr_t input,uintptr_t contact,Decision decision){
 if(!requested)return true;const ObserverLastError preserve;std::lock_guard<std::mutex> lock(mutex);
 const auto index=size_t(stage);++checks[index];const auto sample=client.sample();const auto& worlds=client.worlds();
 const bool identity=worlds.valid()&&(!input||input==worlds.input.world)&&(!contact||contact==worlds.contact.world);
 const bool allowed=initialized&&identity&&decision(sample,worlds);if(!allowed)++denied[index];
 if(checks[index]<=24||(!allowed&&denied[index]<=8)){
  std::ostringstream s;s<<"\"stage\":"<<jsonString(names[index])<<",\"check\":"<<checks[index]<<",\"allowed\":"<<(allowed?"true":"false")<<",\"client\":{"<<combatWorldBridge::fields(sample.input)<<"},\"server\":{"<<combatWorldBridge::fields(sample.contact)<<"},\"observed_client\":"<<hex(input)<<",\"observed_server\":"<<hex(contact)<<",\"input_valid\":"<<(sample.inputValid?"true":"false")<<",\"contact_valid\":"<<(sample.contactValid?"true":"false")<<",\"thread\":"<<GetCurrentThreadId()<<",\"enforced\":true";logFile->event("combat_epoch_gate_check",s.str());
 }
 return allowed;
}
inline bool newAllowed(Stage stage,uintptr_t input=0,uintptr_t contact=0){return checked(stage,input,contact,[](const auto& s,const auto& w){return s.newCombat&&actorAdmission.permitsNew(w);});}
inline motion::CombatWorlds capture(Stage stage,uintptr_t input,uintptr_t contact=0){
 motion::CombatWorlds result;if(!requested)return result;
 checked(stage,input,contact,[&](const auto& s,const auto& w){if(!s.newCombat||!actorAdmission.permitsNew(w))return false;result=w;return true;});return result;
}
inline motion::WorldStamp serverStamp(uintptr_t world,Stage stage=Stage::Scope){
 motion::WorldStamp result;if(!requested)return result;
 checked(stage,0,world,[&](const auto& s,const auto& w){if(!s.suppresses(w.contact,true))return false;result=w.contact;return true;});return result;
}
inline motion::WorldStamp actorFenceStamp(uintptr_t world){
 motion::WorldStamp result;if(!requested)return result;
 checked(Stage::Flush,0,world,[&](const auto& s,const auto& w){
  if(!s.contactValid||s.consumerFault||!actorAdmission.permitsFence(w.contact)||!motion::permitsActorFence(w.contact,s.contact))return false;
  result=w.contact;return true;});return result;
}
// Registered from an owned Impact watch, never by polling a saved Actor.
inline void bindActor(const motion::WorldStamp& server,uint32_t entity){
 if(!requested)return;std::lock_guard<std::mutex> lock(mutex);
 if(!initialized||!actorAdmission.bind(server,entity))throw std::runtime_error("Actor admission owner differs or has already stopped");
}
inline void stopActor(const motion::WorldStamp& server,uint32_t entity,uint64_t ownerCall){
 if(!requested)return;const ObserverLastError preserve;std::lock_guard<std::mutex> lock(mutex);
 const auto sample=client.sample();
 if(!initialized||!sample.contactValid||!(motion::worldStamp(sample.contact)==server))throw std::runtime_error("Actor stop lost exact server generation");
 const auto result=actorAdmission.stop(server,entity,ownerCall);
 if(result==motion::ActorAdmission::Stop::Rejected)throw std::runtime_error("Actor stop does not identify the registered owner");
 if(result==motion::ActorAdmission::Stop::AlreadyStopped)return;
 std::ostringstream o;o<<"\"world\":"<<hex(server.world)<<",\"entity\":"<<entity<<",\"owner_call\":"<<ownerCall<<",\"epoch_provider\":"<<server.provider<<",\"epoch_serial\":"<<server.serial
 <<",\"epoch_registry\":"<<hex(server.registry)<<",\"epoch_root\":"<<hex(server.root)<<",\"epoch_kind\":"<<server.kind<<",\"thread\":"<<GetCurrentThreadId()
 <<",\"server_history_permitted\":"<<(sample.suppresses(server,true)?"true":"false")
 <<",\"input_gate_closed\":true,\"pose_gate_closed\":true,\"ticket_gate_closed\":true,\"actor_fence_closed\":true,\"native_original_called\":false,\"inflight_callbacks_drained\":false,\"ecs_pinned\":false,\"changes_mod_admission\":true,\"retires_ledger_entries\":false";
 logFile->event("combat_actor_admission_stopped",o.str());
}
inline motion::EventActionKey action(motion::EventActionKey key){
 if(!requested)return key;const auto stamp=serverStamp(key.world);if(!key.epoch.empty()&&!(key.epoch==stamp))return {};key.epoch=stamp;return key.epoch.valid()?key:motion::EventActionKey{};
}
inline motion::ColliderKey collider(motion::ColliderKey key){
 if(!requested)return key;const auto stamp=serverStamp(key.world);if(!key.epoch.empty()&&!(key.epoch==stamp))return {};key.epoch=stamp;return key.epoch.valid()?key:motion::ColliderKey{};
}
inline bool impact(const motion::NativeAttackTicket& ticket,const motion::EventActionKey& key,uint64_t now){
 if(!requested)return ticket.permits(key,now);
 return checked(Stage::Event,0,key.world,[&](const auto& s,const auto& w){return s.newCombat&&actorAdmission.permitsNew(w)&&motion::permitsNewImpact(ticket,key,now,s.input,s.contact);});
}
inline bool suppress(const motion::EventActionKey& key,bool consumed){
 if(!requested)return consumed;
 return checked(Stage::Suppress,0,key.world,[&](const auto& s,const auto&){return s.suppresses(key.epoch,consumed);});
}
inline void pose(motion::ColliderPose& value){
 if(!requested)return;
 const bool ok=checked(Stage::Pose,value.clientWorld,0,[&](const auto& s,const auto& w){value=motion::bindColliderWorlds(value,w,s.input,s.contact);return s.newCombat&&actorAdmission.permitsNew(w)&&value.valid;});
 if(!ok)value.valid=false;
}
inline const char* poseRejection(const motion::ColliderPose& value,const motion::ColliderEvidence& evidence,bool eitherHand=false){
 if(!requested)return motion::colliderPoseRejection(value,evidence,eitherHand);const char* reason="world_epoch";
 checked(Stage::Collider,evidence.clientWorld,evidence.impactWorld,[&](const auto& s,const auto& w){if(!actorAdmission.permitsNew(w))reason="actor_owner_stopped";else if(s.newCombat)reason=motion::scopedColliderPoseRejection(value,evidence,s.input,s.contact);return reason==nullptr;});return reason;
}
inline std::string stampFields(const motion::WorldStamp& stamp){
 if(!requested)return {};std::ostringstream s;s<<",\"epoch_provider\":"<<stamp.provider<<",\"epoch_serial\":"<<stamp.serial<<",\"epoch_kind\":"<<stamp.kind<<",\"epoch_registry\":"<<hex(stamp.registry)<<",\"epoch_root\":"<<hex(stamp.root);return s.str();
}
inline std::string contextFields(const motion::CombatWorlds& worlds){if(!requested)return {};return stampFields(worlds.contact)+",\"client_epoch_serial\":"+std::to_string(worlds.input.serial)+",\"client_epoch_world\":"+hex(worlds.input.world);}
inline bool actorStopped(){if(!requested)return false;std::lock_guard<std::mutex> lock(mutex);return actorAdmission.stopped();}
inline motion::CombatEpochState inputState(){
 if(!requested)return motion::CombatEpochState::Open;
 std::lock_guard<std::mutex> lock(mutex);if(!initialized)return motion::CombatEpochState::Unavailable;
 const auto state=client.sample().state();
 return state==motion::CombatEpochState::Open&&actorAdmission.stopped()?motion::CombatEpochState::Closed:state;
}
// Explicit finite TEST ONLY: close MOD admission at an observed post-flush
// boundary while native Worlds keep simulating. This does not destroy a World.
inline bool cancelTrial=false,cancelIssued=false;
inline void exerciseClose(uintptr_t world,uint32_t actor,uint32_t tick,unsigned applied,unsigned watches,bool active,bool quiet){
 if(!cancelTrial||cancelIssued||!active||!quiet||watches!=1||applied<2)return;
 std::lock_guard<std::mutex> lock(mutex);const auto before=client.sample();const auto saved=client.leaseIdentities();
 if(!initialized||!before.newCombat||before.contact.world!=world)throw std::runtime_error("epoch cancel trial lost exact live ownership");
 auto q=epoch::request(epoch::Operation::CloseAdmission,saved[0]);q.handle=0;
 const auto module=GetModuleHandleW(L"evr_diagnostic.dll");const auto fn=module?reinterpret_cast<epoch::Call>(GetProcAddress(module,"evrWorldEpochCall")):nullptr;
 epoch::Response closed;
 if(!fn||fn(&q,&closed)||closed.status||closed.size!=sizeof(closed)||closed.version!=epoch::abiVersion||!(motion::worldStamp(closed)==client.worlds().input))throw std::runtime_error("epoch cancel trial rejected");
 const auto after=client.sample();
 if(after.state()!=motion::CombatEpochState::Closed||after.input.phase!=1||after.contact.phase!=1||!after.suppresses(client.worlds().contact,true))throw std::runtime_error("epoch cancellation did not preserve live server history");
 cancelIssued=true;
 logFile->event("combat_epoch_admission_exercised","\"client\":{"+combatWorldBridge::fields(after.input)+"},\"server\":{"+combatWorldBridge::fields(after.contact)+"},\"actor\":"+std::to_string(actor)+",\"tick\":"+std::to_string(tick)+",\"applied_scopes\":"+std::to_string(applied)+",\"quiet\":true,\"active_action\":true,\"native_world_destroyed\":false,\"writes_gameplay\":false");
}
inline bool endedContact(motion::WorldEndBoundary& boundary){
 if(!requested)return false;std::lock_guard<std::mutex> lock(mutex);
 return initialized&&client.endedContact(boundary);
}
inline void finish(){
 if(!requested)return;std::lock_guard<std::mutex> lock(mutex);const auto saved=client.leaseIdentities();
 if(!client.release())throw std::runtime_error("strict World leases not drained after native callbacks");initialized=false;
 for(const auto& r:saved)if(r.handle)logFile->event("combat_epoch_gate_released",combatWorldBridge::fields(r)+",\"exact_release_succeeded\":true,\"saved_request_metadata\":true");
 std::ostringstream s;s<<"\"retained\":"<<client.retained()<<",\"faulted\":"<<(client.faulted()?"true":"false")<<",\"checks\":{";
 for(size_t i=0;i<checks.size();++i){if(i)s<<',';s<<jsonString(names[i])<<':'<<checks[i];}s<<"},\"denied\":{";
 for(size_t i=0;i<denied.size();++i){if(i)s<<',';s<<jsonString(names[i])<<':'<<denied[i];}s<<"},\"enforced\":true";logFile->event("combat_epoch_gate_complete",s.str());
}
}
