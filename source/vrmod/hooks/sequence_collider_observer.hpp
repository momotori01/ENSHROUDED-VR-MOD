// Read-only sequence_event_collider query after native create/update/remove.
// Exact callsite 36be7e -> 8d5ca0, query 0x50 at caller RSP+0xb0.
#include "sequence_asset_observer.hpp"
#include <evr/observation_budget.hpp>
#include "native_impact_components.hpp"
#include "sequence_collider_identity.hpp"
#include "motion_collider_shadow.hpp"
#include "motion_collider_apply.hpp"
// Other players' melee hits on this host (2026-10-20; user 2026-10-19: "the guest finds the contact on its PC, the
// host's mod makes the game's own hit" - docs/analysis/online_sync_20261013/guest_melee_20261019.md). The host's server
// sweeps every attack collider here, the guests' too; a guest's collider follows the guest's character's animation on
// the server, not the guest's VR weapon. A collider whose action sequence is the running sequence of another player's
// Actor (the attacker - not a nearby enemy's swing) is swept with that player's weapon as their mod sends it (the held
// item's pose in the hand, online_sync.hpp publishItems: the weapon this host draws in their hand), when one arrived in
// the last half second and their character stands within 4 m of where that weapon was sent from (2026-10-21: a quarter
// second and 2.5 m left 27 of 40 sweeps of the guest's strokes without the weapon in the session of 10-21 00:25; the
// log now names the nearest weapon's age and distance when none is taken). Everything after
// (damage, stamina, the reaction) stays the game's. Without a sent weapon (a player without the mod, a desktop player)
// the game's own sweep stays. EVR_REMOTE_MELEE=0: off.
namespace remoteMelee {
struct Weapon {uint32_t character=0;WorldPositionQ32 root{};std::array<uint8_t,40> transform{};uint64_t ms=0;};
struct Player {uint32_t entity=0;WorldPositionQ32 at{};uint64_t ms=0;};
inline const bool enabled=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_MELEE",v,sizeof v);return !(n==1&&v[0]=='0');}();
// EVR_REMOTE_MELEE_SELF=1 (a test for one player, with EVR_ONLINE_SYNC=1 EVR_SYNC_SELF=1): the own strokes take this way -
// the own character as "another player" and the own weapon as it came back through Steam.
inline const bool self=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_MELEE_SELF",v,sizeof v);return n==1&&v[0]=='1';}();
inline std::mutex mutex;inline std::array<Weapon,8> weapons{};inline std::array<Player,8> players{};
inline std::atomic<uint64_t> weaponMs{0};inline std::atomic<unsigned> applied{0},noWeapon{0},checked{0};inline std::atomic<uint64_t> serial{0};
// online_sync.hpp: the main hand's weapon of a driven character (world position Q32 and orientation, as drawn here).
inline void publishWeapon(uint32_t character,const WorldPositionQ32& root,const std::array<uint8_t,40>& transform,uint64_t now){
 if(!enabled)return;std::lock_guard<std::mutex> lock(mutex);
 Weapon* slot=nullptr;for(auto& w:weapons)if(w.character==character){slot=&w;break;}
 if(!slot){slot=&weapons[0];for(auto& w:weapons)if(w.ms<slot->ms)slot=&w;}
 *slot={character,root,transform,now};weaponMs=now;
}
// live_melee_targets.hpp: the other players of the World the local player's Actor ticks in, after each pass of its map.
// The hits the other players' mods decided (online_sync.hpp, sync::hitAction; user 2026-10-21 03:00: "take the hit from
// the guest as decided and compute what follows - then it always hits"): where the touched thing stood on the guest's PC.
// While that player's attack collider runs here, it is laid on the host's thing nearest to that place (a creature for a
// creature, within 2.5 m; liveTargets::nearestTo), a few sweeps long, so that the game's own hit is made on it; the
// sweep with the sent weapon (below) stays for what no hit came for. EVR_REMOTE_HITS=0: only the sweep.
struct Hit {uint64_t from=0,ms=0;std::array<int64_t,3> at{};bool character=false;unsigned sweeps=0;bool used=true;};
inline std::array<Hit,8> hits{};inline unsigned confirmedLogs=0;
inline const bool hitsWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_HITS",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline bool (*nearestTarget)(const std::array<int64_t,3>&,bool,float,std::array<int64_t,3>&,uint32_t&,float&)=nullptr;
inline constexpr uint64_t hitMs=800;inline constexpr unsigned hitSweeps=4;
inline void publishHit(uint64_t from,const std::array<int64_t,3>& at,bool character,uint64_t now){
 if(!enabled||!hitsWanted)return;std::lock_guard<std::mutex> lock(mutex);
 Hit* slot=&hits[0];for(auto& h:hits)if(h.ms<slot->ms)slot=&h;*slot={from,now,at,character,0,false};
}
inline void publishPlayers(const std::vector<Player>& list){
 std::lock_guard<std::mutex> lock(mutex);players={};for(size_t i=0;i<list.size()&&i<players.size();++i)players[i]=list[i];
}
struct Types {uintptr_t registry=0;int actor=-1;unsigned actorBytes=0;};inline Types types;
inline void learn(uintptr_t world){
 const auto registry=read<uintptr_t>(world);if(types.registry==registry)return;types={};types.registry=registry;
 for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){try{const auto t=nativeImpactComponents::componentType(world,i);
   if(t.qualifiedName=="keen::ecs::Actor"&&t.bytes>=0x40){types.actor=int(i);types.actorBytes=t.bytes;break;}}catch(...){break;}}
 logFile->event("remote_melee_types","\"world\":"+hex(world)+",\"actor\":"+std::to_string(types.actor)+",\"actor_bytes\":"+std::to_string(types.actorBytes));
}
inline double metres(const WorldPositionQ32& a,const WorldPositionQ32& b){const double x=relativeQ32(a.x,b.x),y=relativeQ32(a.y,b.y),z=relativeQ32(a.z,b.z);return std::sqrt(x*x+y*y+z*z);}
// sequence_collider_observer.hpp ready(): a collider that is not the local player's.
inline void collider(uintptr_t state,uint32_t entity,void* query,uint32_t sequence,uint32_t localActor,unsigned sample,const Player* own=nullptr) noexcept {
 try{
  const auto now=GetTickCount64();const auto last=weaponMs.load();
  if(!enabled||!sequence||!last||now<last||now-last>500)return;
  const auto world=read<uintptr_t>(state+0x30);if(!world)return;
  std::array<Player,8> list;std::array<Weapon,8> sent;{std::lock_guard<std::mutex> lock(mutex);list=players;sent=weapons;}
  if(own)list[list.size()-1]=*own;
  learn(world);if(types.actor<0)return;
  for(const auto& p:list){
   if(!p.entity||p.entity==localActor||now<p.ms||now-p.ms>3000)continue;
   uint32_t running=0;
   try{const auto e=nativeImpactComponents::entityAtWorld(world,p.entity);const auto a=nativeImpactComponents::component(e,unsigned(types.actor),types.actorBytes);if(!a)continue;running=read<uint32_t>(a);}catch(...){continue;}
   if(running!=sequence)continue;
   // A hit that player's mod decided: the collider on the thing it touched (the oldest such hit first).
   if(nearestTarget){Hit hit;bool have=false;
    {std::lock_guard<std::mutex> lock(mutex);Hit* oldest=nullptr;
     for(auto& h:hits){if(h.used)continue;if(now<h.ms||now-h.ms>hitMs){h.used=true;continue;}
      if(metres(WorldPositionQ32{h.at[0],h.at[1],h.at[2]},p.at)>8)continue;if(!oldest||h.ms<oldest->ms)oldest=&h;}
     if(oldest){if(++oldest->sweeps>=hitSweeps)oldest->used=true;hit=*oldest;have=true;}}
    std::array<int64_t,3> target{};uint32_t thing=0;float away=-1;
    if(have&&nearestTarget(hit.at,hit.character,2.5f,target,thing,away)){
     // the collider's grip at the thing's middle (a creature about 0.9 m up, a thing 0.4 m), along the sent weapon
     const int64_t up=int64_t((hit.character?.9:.4)*4294967296.);const WorldPositionQ32 q{target[0],target[1]+up,target[2]};
     Quat o{0,0,0,1};for(const auto& w:sent)if(w.ms&&now>=w.ms&&now-w.ms<=1000&&metres(w.root,p.at)<4){std::memcpy(&o,w.transform.data()+24,16);o=normalized(o);break;}
     motion::ColliderPose pose;pose.valid=true;pose.frame=1;pose.serial=++serial;pose.predictedTime=1;pose.milliseconds=now;
     std::memcpy(pose.transform.data(),&q,24);std::memcpy(pose.transform.data()+24,&o,16);
     motionColliderApply::begin(world,entity,query,sample,pose,false,true);
     if(confirmedLogs++<300)logFile->event("remote_melee_confirmed","\"attacker\":"+std::to_string(p.entity)+",\"collider\":"+std::to_string(entity)+",\"sequence\":"+std::to_string(sequence)+
      ",\"target\":"+std::to_string(thing)+",\"character\":"+(hit.character?"true":"false")+",\"hit_age_ms\":"+std::to_string(now-hit.ms)+",\"guest_to_host_metres\":"+std::to_string(away)+",\"sweep\":"+std::to_string(hit.sweeps));
     return;}
    if(have&&confirmedLogs++<300)logFile->event("remote_melee_hit_unmatched","\"attacker\":"+std::to_string(p.entity)+",\"character\":"+(hit.character?"true":"false")+",\"hit_age_ms\":"+std::to_string(now-hit.ms));}
   ++checked;const Weapon* best=nullptr;double bestMetres=4;uint64_t nearestAge=UINT64_MAX;double nearestMetres=-1;
   for(const auto& w:sent){if(!w.ms||now<w.ms)continue;const double m=metres(w.root,p.at);
    if(nearestMetres<0||m<nearestMetres){nearestMetres=m;nearestAge=now-w.ms;}
    if(now-w.ms>500)continue;if(m<bestMetres){bestMetres=m;best=&w;}}
   if(!best){if(noWeapon.fetch_add(1)<200)logFile->event("remote_melee_no_weapon","\"entity\":"+std::to_string(p.entity)+",\"collider\":"+std::to_string(entity)+",\"sequence\":"+std::to_string(sequence)+
    ",\"nearest_age_ms\":"+std::to_string(nearestAge)+",\"nearest_metres\":"+std::to_string(nearestMetres));return;}
   motion::ColliderPose pose;pose.valid=true;pose.frame=1;pose.serial=++serial;pose.predictedTime=1;pose.milliseconds=now;
   std::memcpy(pose.transform.data(),best->transform.data(),40);
   motionColliderApply::begin(world,entity,query,sample,pose,false,true);
   if(applied.fetch_add(1)<300)logFile->event("remote_melee_collider","\"attacker\":"+std::to_string(p.entity)+",\"collider\":"+std::to_string(entity)+",\"sequence\":"+std::to_string(sequence)+
    ",\"character\":"+std::to_string(best->character)+",\"weapon_age_ms\":"+std::to_string(now-best->ms)+",\"attacker_to_sent_metres\":"+std::to_string(bestMetres));
   return;
  }
 }catch(...){}
}
}
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
  if((!actor.active||!actor.sequence||sequence!=actor.sequence)&&!motionColliderApply::owns(read<uintptr_t>(state+0x30),entity,event)){
   if(juneShape)remoteMelee::collider(state,entity,query,sequence,actor.entity,0);   // another player's stroke (2026-10-20)
   return;}
  if(remoteMelee::self&&juneShape){const auto r=rootCamera::snapshot();
   const remoteMelee::Player own{actor.entity,WorldPositionQ32{r.stable.position.x,r.stable.position.y,r.stable.position.z},GetTickCount64()};
   remoteMelee::collider(state,entity,query,sequence,0,0,&own);return;}
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
