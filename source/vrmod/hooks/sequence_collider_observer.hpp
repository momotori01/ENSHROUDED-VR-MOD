// Read-only sequence_event_collider query after native create/update/remove.
// Exact callsite 36be7e -> 8d5ca0, query 0x50 at caller RSP+0xb0.
#include "sequence_asset_observer.hpp"
#include <evr/observation_budget.hpp>
#include "native_impact_components.hpp"
#include "sequence_collider_identity.hpp"
#include "motion_collider_shadow.hpp"
#include "motion_collider_apply.hpp"
namespace sequenceColliderObserver {
inline std::mutex quotaMutex;inline std::map<std::pair<uint32_t,bool>,unsigned> quotas;
inline bool requested=false;
inline std::atomic<unsigned> samples{0},foreignSamples{0},localSamples{0},calls{0},failures{0};
inline thread_local unsigned currentReadySample=0;
constexpr uintptr_t callerRva=0x3f02dd;
inline actionSequenceObserver::Status qualifiedActor(uintptr_t state,const actionSequenceObserver::Status& fallback){
 const auto world=read<uintptr_t>(state+0x30);if(!world||!fallback.entity)return {};
 return actionSequenceObserver::forWorld(world,fallback.entity);
}
template<size_t N> std::string bytesHex(const std::array<uint8_t,N>& b){
 const char* d="0123456789abcdef";std::string s;s.reserve(N*2);for(auto v:b){s+=d[v>>4];s+=d[v&15];}return s;
}
inline void capture(uintptr_t sp,void* context,void* query,uint32_t bytes){
 ++calls;if(!actionSequenceObserver::recording.load()||samples.load()>=1040)return;
 try{
  if(bytes!=0x50||reinterpret_cast<uintptr_t>(query)!=sp+0xb0)throw std::runtime_error("sequence collider query ABI differs");
  const auto slots=read<std::array<uintptr_t,10>>(reinterpret_cast<uintptr_t>(query));
  if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[1]||!slots[4]||!slots[5]||!slots[7]||!slots[8])throw std::runtime_error("sequence collider slots missing");
  const auto index=read<uint32_t>(slots[0]+8),parent=slots[2]?read<uint32_t>(slots[2]):0;
  if(!index||index>0x1000000)throw std::runtime_error("sequence collider entity index invalid");
  const auto state=read<uintptr_t>(slots[0]);const auto entity=read<uint32_t>(read<uintptr_t>(state+0x828)+uintptr_t(index-1)*16);
  actionSequenceObserver::Status actor;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);actor=actionSequenceObserver::latest;}
  actor=qualifiedActor(state,actor);
  sequenceAssetObserver::capture(slots[7],actor);
  const bool local=parent&&parent==actor.entity;
  {std::lock_guard<std::mutex> guard(quotaMutex);auto& n=quotas[{entity,actor.active}];if(n++>=(actor.active?128u:2u))return;}
  if(local)++localSamples;else ++foreignSamples;
  const auto event=read<std::array<uint8_t,24>>(slots[4]);const auto collider=read<std::array<uint8_t,24>>(slots[5]);
  const auto transform=read<std::array<uint8_t,56>>(slots[1]);
  const auto physicsCurrent=read<std::array<uint8_t,32>>(sp+0x70); // Last create/update native argument, consumed before this boundary.
  const auto last=slots[3]?read<std::array<uint8_t,72>>(slots[3]):std::array<uint8_t,72>{};
  const auto link=slots[2]?read<std::array<uint8_t,8>>(slots[2]):std::array<uint8_t,8>{};
  const bool stable=slots==read<std::array<uintptr_t,10>>(reinterpret_cast<uintptr_t>(query))&&event==read<std::array<uint8_t,24>>(slots[4])&&collider==read<std::array<uint8_t,24>>(slots[5])&&transform==read<std::array<uint8_t,56>>(slots[1]);
  uint32_t sequence=0;uint64_t colliderId=0;std::memcpy(&sequence,event.data(),4);std::memcpy(&colliderId,collider.data()+8,8);
  std::ostringstream o;o<<"\"ready_sample\":"<<currentReadySample<<",\"sample\":"<<++samples<<",\"entity\":"<<entity<<",\"parent_entity\":"<<parent<<",\"local_parent\":"<<(local?"true":"false")
   <<",\"sequence\":"<<sequence<<",\"collider_id\":"<<colliderId<<",\"event_bytes\":"<<jsonString(bytesHex(event))
   <<",\"collider_bytes\":"<<jsonString(bytesHex(collider))<<",\"transform_bytes\":"<<jsonString(bytesHex(transform))
   <<",\"physics_updated\":"<<(motionColliderApply::cancelScope?"false":"true")<<",\"physics_current_bytes\":"<<jsonString(bytesHex(physicsCurrent))<<",\"physics_previous_bytes\":"<<jsonString(bytesHex(read<std::array<uint8_t,32>>(sp+0x90)))<<",\"physics_argument_address\":"<<hex(sp+0x70)
   <<",\"last_transform_bytes\":"<<jsonString(bytesHex(last))<<",\"last_transform_present\":"<<(slots[3]?"true":"false")
   <<",\"despawn_link_bytes\":"<<jsonString(bytesHex(link))<<",\"stable_read\":"<<(stable?"true":"false")
   <<",\"query\":"<<hex(reinterpret_cast<uintptr_t>(query))<<",\"state\":"<<hex(state)<<",\"thread\":"<<GetCurrentThreadId()<<",\"milliseconds\":"<<GetTickCount64()
   <<",\"latest_actor_entity\":"<<actor.entity<<",\"latest_actor_sequence\":"<<actor.sequence<<",\"latest_actor_item\":"<<actor.usedItem
   <<",\"latest_actor_active\":"<<(actor.active?"true":"false")<<",\"latest_actor_ms\":"<<actor.milliseconds<<",\"read_only\":true";
  nativeImpactComponents::append(o,state,entity);
  if(actor.active&&actor.sequence==sequence){
   const auto visual=attachmentObserver::meleeAppearance(actor.entity,GetTickCount64());
   if(visual.valid&&visual.item==actor.usedItem){o<<",\"native_weapon_entity\":"<<visual.child<<",\"native_weapon_item\":"<<visual.item;o<<",\"native_weapon_world\":"<<hex(visual.world)<<",\"native_weapon_slot_bytes\":"<<jsonString(bytesHex(visual.slotBytes))<<",\"native_weapon_slot_ms\":"<<visual.milliseconds<<",\"latest_actor_world\":"<<hex(actor.world);nativeImpactComponents::appendServerSlot(o,state,entity);}
  }
  logFile->event("sequence_collider_sample",o.str());
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("sequence_collider_failure","\"reason\":"+jsonString(e.what()));}
}
// Called immediately after successful native iterator advance, before create/update.
inline std::atomic<unsigned> readySamples{0},readyRecorded{0},readyWitnesses{0},readyOmitted{0};
inline evr::diagnostic::ObservationBudget readyRecordingBudget{64};
inline void ready(uintptr_t sp,void* context,void* query,uint32_t bytes,bool juneShape=false){
 currentReadySample=0;
 if(!actionSequenceObserver::recording.load())return; // Fixture arming, not a sample quota.
 try{
  if(!juneShape&&(bytes!=0x50||reinterpret_cast<uintptr_t>(query)!=sp+0xb0))throw std::runtime_error("collider ready query ABI differs");
  const auto slots=read<std::array<uintptr_t,10>>(reinterpret_cast<uintptr_t>(query));if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[4]||!slots[5])throw std::runtime_error("collider ready query absent");
  actionSequenceObserver::Status actor;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);actor=actionSequenceObserver::latest;}
  const auto event=read<std::array<uint8_t,24>>(slots[4]);uint32_t sequence=0;std::memcpy(&sequence,event.data(),4);
  const auto state=read<uintptr_t>(slots[0]);const auto index=read<uint32_t>(slots[0]+8);if(!index||index>0x1000000)throw std::runtime_error("collider ready entity bounds");
  const auto entity=read<uint32_t>(read<uintptr_t>(state+0x828)+uintptr_t(index-1)*16);const auto id=read<uint64_t>(slots[5]+8);
  blockedActionLifetime::collider(read<uintptr_t>(state+0x30),entity,event);
  actor=qualifiedActor(state,actor);
  nativeImpactComponents::learnCurrent(state,entity,slots[1]);
  if((!actor.active||!actor.sequence||sequence!=actor.sequence)&&!motionColliderApply::owns(read<uintptr_t>(state+0x30),entity,event))return;
  const auto observation=readyRecordingBudget.observe();++readySamples;currentReadySample=static_cast<unsigned>(observation.ordinal);
  if(observation.record||observation.witness)try{
  std::ostringstream o;o<<"\"sample\":"<<currentReadySample<<",\"entity\":"<<entity<<",\"sequence\":"<<sequence<<",\"event_bytes\":"<<jsonString(bytesHex(event))<<",\"collider_id_before_native\":"<<id<<",\"thread\":"<<GetCurrentThreadId()<<",\"milliseconds\":"<<GetTickCount64()<<",\"latest_actor_entity\":"<<actor.entity<<",\"latest_actor_item\":"<<actor.usedItem<<",\"read_only\":true";
  nativeImpactComponents::append(o,state,entity);logFile->event(observation.record?"sequence_collider_ready_sample":"sequence_collider_ready_budget_witness",o.str());
  if(observation.record)++readyRecorded;else ++readyWitnesses;
  }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("sequence_collider_recording_failure","\"reason\":"+jsonString(e.what()));}
  else ++readyOmitted;
  // Owned query borrowing/cancellation and the outer finish remain unconditional.
  motionColliderShadow::inspect(state,entity,query,actor,currentReadySample);
  motionColliderApply::begin(read<uintptr_t>(state+0x30),entity,query,currentReadySample,motionColliderShadow::acceptedPose,motionColliderShadow::ownedIntent);
  sequenceColliderIdentity::begin(state,entity,query,actor,sequence);
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("sequence_collider_failure","\"reason\":"+jsonString(e.what()));}
}
// 1.0 (2026-10-17, headset reports 13, 14 and 17: "attacks do not hit", "the felling axe does not cut the tree"): the attack
// colliders are swept in 0x32c230 - per collider entity, from its LastTransform to its CurrentTransform, every shape of the
// action event's collider (0x999d70 per shape). Its 0x38-byte query at the caller's rsp+0x80 (made at 0x32c246): the context
// 0, CurrentTransform 2, LastTransform 3, SequenceEventCollider 4 (24 bytes, the action's sequence first, the event's guid
// at +8 - what June's event slot held), the events' registry 5. The advances return at 0x32c271 (the first) and 0x32c45e
// (the loop's). June's place 0x36be7e, moved by the rematch to 0x3f02dd, is never reached in 1.0: Mock game-20261017-031508,
// eight strokes, sequence_collider calls 0, motion_collider applied 0 - the collider stayed where the game's (withheld)
// animation put the weapon. The census (EVR_LAYOUT_DUMP=2, game-20261017-045053): this query, entity 6764 from 115 ms after
// the first press. Here June's ten-slot shape is copied from it (current 1, previous 3, event 4, no collider id: 5 points
// at zeros), handed to ready() / the finishes as before, and the two transform slots the borrowing put in the copy are
// written into the game's query - for this one iteration; the next advance puts the originals back.
namespace colliderSweep {
constexpr uintptr_t firstReturn=0x32c271,nextReturn=0x32c45e;constexpr uint32_t queryBytes=0x38;constexpr uintptr_t queryAt=0x80;
inline const std::array<uint8_t,16> noCollider{};
inline thread_local std::array<uintptr_t,10> shape{};inline thread_local uintptr_t* game=nullptr;inline thread_local bool open=false;
inline std::atomic<unsigned> iterations{0},misplaced{0},worldless{0};
// This task's state has no world (+0x30 is null: Mock game-20261017-045725, "component lookup world/entity absent"), which
// the code after ready() takes from it. The world is the one of the worlds the local player's actions were seen in where
// the entity exists and its CurrentTransform is the query's; the rest of the code gets a small copy of the state with that
// world (+0x30) and the state's own entity and tick tables (+0x828, +0x818), and a context that points at it.
inline thread_local std::array<uint8_t,0x840> state{};inline thread_local std::array<uintptr_t,2> context{};
inline uintptr_t worldOf(uint32_t entity,uintptr_t current){
 std::vector<uintptr_t> worlds;
 {std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);if(actionSequenceObserver::latest.world)worlds.push_back(actionSequenceObserver::latest.world);
  for(const auto& [key,status]:actionSequenceObserver::qualified)if(key.first&&std::find(worlds.begin(),worlds.end(),key.first)==worlds.end())worlds.push_back(key.first);}
 for(const auto w:worlds)try{const auto e=nativeImpactComponents::entityAtWorld(w,entity);if(nativeImpactComponents::identify(e,current,56)>=0)return w;}catch(...){}
 return 0;
}
inline bool site(uintptr_t caller){return caller==baseAddress+firstReturn||caller==baseAddress+nextReturn;}
// Before the next advance (or the last one, which ends the loop): the borrowed slots go back to the game's own.
inline void close(void* query) noexcept {
 if(!open)return;open=false;
 auto* q=reinterpret_cast<uintptr_t*>(query);if(q!=game)return;
 shape[1]=q[2];shape[3]=q[3];
 motionColliderApply::finish(shape.data());sequenceColliderIdentity::finish(shape.data());
 q[2]=shape[1];q[3]=shape[3];game=nullptr;
}
// After an advance that found a collider entity.
inline void opened(uintptr_t sp,void* nativeContext,void* query,uint32_t bytes) noexcept {
 try{
  if(bytes!=queryBytes||reinterpret_cast<uintptr_t>(query)!=sp+queryAt){if(misplaced.fetch_add(1)<3)logFile->event("collider_sweep_query_differs","\"bytes\":"+std::to_string(bytes)+",\"query_minus_rsp\":"+hex(reinterpret_cast<uintptr_t>(query)-sp));return;}
  auto* q=reinterpret_cast<uintptr_t*>(query);
  if(q[0]!=reinterpret_cast<uintptr_t>(nativeContext)||!q[2]||!q[4])return;
  const auto real=read<uintptr_t>(q[0]);const auto index=read<uint32_t>(q[0]+8);const auto entities=read<uintptr_t>(real+0x828);
  if(!real||!index||index>0x1000000||!entities)return;
  const auto entity=read<uint32_t>(entities+uintptr_t(index-1)*16);
  uintptr_t world=read<uintptr_t>(real+0x30);if(!world)world=worldOf(entity,q[2]);
  if(!world){if(worldless.fetch_add(1)<3)logFile->event("collider_sweep_world_unknown","\"entity\":"+std::to_string(entity));return;}
  state.fill(0);std::memcpy(state.data()+0x30,&world,8);
  {const auto ticks=read<uintptr_t>(real+0x818);std::memcpy(state.data()+0x818,&ticks,8);std::memcpy(state.data()+0x828,&entities,8);}
  context={reinterpret_cast<uintptr_t>(state.data()),uintptr_t(index)};
  shape={reinterpret_cast<uintptr_t>(context.data()),q[2],0,q[3],q[4],reinterpret_cast<uintptr_t>(noCollider.data()),0,0,0,0};game=q;open=true;
  const auto n=iterations.fetch_add(1);
  if(n<3)logFile->event("collider_sweep_iteration","\"count\":"+std::to_string(n+1)+",\"entity\":"+std::to_string(entity)+",\"world\":"+hex(world)+",\"task_world\":"+hex(read<uintptr_t>(real+0x30))+",\"sequence\":"+std::to_string(read<uint32_t>(q[4]))+",\"june_shape\":true");
  motionColliderShadow::fromSweep=true;ready(sp,context.data(),shape.data(),0x50,true);motionColliderShadow::fromSweep=false;
  q[2]=shape[1];q[3]=shape[3];
 }catch(...){}
}
}
inline void serverSlot(uintptr_t sp,void* context,void* query,uint32_t bytes){
 try{
  if(bytes!=0x30||reinterpret_cast<uintptr_t>(query)!=sp+0x48)throw std::runtime_error("server slot query ABI");
  const auto slots=read<std::array<uintptr_t,6>>(reinterpret_cast<uintptr_t>(query));if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[1])throw std::runtime_error("server slot query absent");
  const auto local=rootCamera::snapshot();if(!local.entity||read<uint32_t>(slots[1]+44)!=local.entity)return;
  const auto state=read<uintptr_t>(slots[0]);const auto index=read<uint32_t>(slots[0]+8);if(!index||index>0x1000000)throw std::runtime_error("server slot entity bounds");
  const auto entity=read<uint32_t>(read<uintptr_t>(state+0x828)+uintptr_t(index-1)*16);nativeImpactComponents::learnServerSlot(state,entity,slots[1]);
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("sequence_collider_failure","\"reason\":"+jsonString(e.what()));}
}
inline void removed(bool restored,unsigned callbacks){if(requested)logFile->event("collider_ready_recording_summary","\"processed\":"+std::to_string(readySamples.load())+",\"recorded\":"+std::to_string(readyRecorded.load())+",\"witness\":"+std::to_string(readyWitnesses.load())+",\"omitted\":"+std::to_string(readyOmitted.load())+",\"limit\":"+std::to_string(readyRecordingBudget.limit()));if(requested)logFile->event("sequence_collider_removed","\"restored\":"+std::string(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks)+",\"samples\":"+std::to_string(samples.load())+",\"calls\":"+std::to_string(calls.load())+",\"failures\":"+std::to_string(failures.load()));}
}
