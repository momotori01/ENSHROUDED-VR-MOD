#pragma once
// The entities near the local player that have Health, for the contact gate of live melee (include/evr/live_targets.hpp:
// VHVR attacks only when the weapon enters a target). Read only: the World's entity map and two components of each
// entity are read with ReadProcessMemory, a slice of the map on every tick of the local player's Actor, and kept in a
// small list. Nothing is selected, moved or attacked here, and no game function is called.
//
// Layouts are the ones native_impact_components.hpp and motion_target_observer.hpp established (World+0x148 entity
// map; record +0x18 layout, +0x20 data, +0x30 row; layout: component mask, offsets at +0x84, strides at +0xa84).
// Component ids are found by their runtime names, never hardcoded. A row that changes while it is read is skipped.
// When the list cannot be kept (unknown types, no tick for a second), contact() answers "reached": melee then works as it
// did without the gate, which is better than no attack at all.
namespace liveTargets {
inline bool requested=false;
// healthBytes: the Health component's size in this build (68 in June; 1.0 found no Health of 68 bytes and the list stayed
// empty, live_targets_types health -1 in the Mock auto start 20261016-141318).
// A guest's World (2026-10-20, the guest's report of the online session 20261020-002320: live_targets_state cached 0 for the
// whole half hour, 4968 full scans, no live_targets_health_sample, so no attack was ever pressed - "my melee never hits"):
// a joined game's entities carry the replicated keen::ecs::NetworkHealth (health, healthMax: uint32, 8 bytes) instead of
// the server's Health, and RenderTransform (its WorldTransform first, as CurrentTransform's) may stand for the position.
// Both are taken where the server's components are missing; the host and a game of one keep the server's.
// Players are the entities with keen::ecs::NetworkPlayerAttributes; one at no health is a fallen player a teammate revives
// (its revivieData +0x28: NetworkReviveData state, duration, left; reviveDuration +0x44) - see downedNear below.
struct Types {uintptr_t world=0,registry=0;int model=-1;uint16_t modelBytes=0;int health=-1,current=-1,actor=-1,selected=-1,relevant=-1,networkHealth=-1,render=-1,networkActor=-1,player=-1,enemy=-1,destructible=-1,clientCollider=-1,feedbackMaterial=-1,velocity=-1,gravity=-1,stamina=-1,blockValue=-1,staminaDamageMod=-1;
 uint16_t healthBytes=68,selectedBytes=0,relevantBytes=0,renderBytes=0,playerBytes=0;bool usable=false;};
struct Layout {bool health=false,current=false,actor=false,selected=false,relevant=false,networkHealth=false,render=false,player=false,enemy=false,destructible=false,clientThing=false;std::array<uint64_t,10> bits{};
 uint16_t healthOffset=0,currentOffset=0,selectedOffset=0,relevantOffset=0,networkHealthOffset=0,renderOffset=0,playerOffset=0;};
// Fallen players near the local one (2026-10-20, both players: "I cannot revive the other" - the revive is the game's
// interaction held for reviveDuration, and the right B of the mod only taps it, a longer hold opening the quick wheel).
struct Downed {uint32_t entity=0;std::array<int64_t,3> position{};uint64_t seen=0;};
inline std::vector<Downed> downed;
// What a full pass of the map found (once in a while): which components the entities near the player have.
struct Census {unsigned entities=0,health=0,networkHealth=0,current=0,render=0,player=0,downed=0;
 unsigned objects=0,objectsWithoutPose=0;double nearestObject=-1;   // destructible things (2026-10-20): within 60 m; how many lack a pose this list reads
 unsigned all=0,allHealth=0,allNetworkHealth=0,allCurrent=0,allRender=0,allPlayers=0,allEnemies=0;};   // all: anywhere in the map
// A World without anything that has a health or is an enemy (2026-10-20, Mock game-20261020-021225: a game's client World -
// what a guest has - holds 2455 entities, the 230 near the player the same as the server's 230, but none with Health,
// NetworkHealth or Destructible: only models, colliders and the like). There no target can be known, and the gate is let
// go - a stroke attacks as before the gate (2026-10-11) - instead of never attacking (the guest of 20261020-002320).
inline bool blind=false;inline unsigned blindLogs=0;
// A guest's World (2026-10-21, user after the online session of 10-21 00:25: "as a guest the sword does not hit trees and
// pots"; the guest's log 20261021-002429 world-4: no entity with Destructible or Health, 13 with NetworkHealth - the
// server's components stay on the host): the things with a collider of the guest's own and a collision feedback
// material (keen::ecs::StaticClientCollider + CollisionFeedbackMaterialComponent - pots, trees, rocks, walls alike; the
// guest cannot tell which break) are taken as things there, so that a stroke at them attacks on the guest at once and the
// host decides what it does (user 01:55: "decide all of it on the guest; the host processes it later").
// EVR_GUEST_THINGS=0: as before.
inline bool guestWorld=false;inline unsigned guestLogs=0;
inline const bool guestThingsWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUEST_THINGS",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline Census census;inline unsigned censusLogs=0;
// Which components the entities of this World have (the first two passes of each World): the 40 most common, by name,
// and those of the entities near the player - what a guest's World holds, to find its creatures by (2026-10-20).
inline std::array<unsigned,640> typeCounts{},typeCountsNear{};inline unsigned typePasses=0;
inline void tellTypes(uintptr_t world){
 if(typePasses>=2)return;++typePasses;
 const auto top=[&](const std::array<unsigned,640>& counts){std::vector<std::pair<unsigned,unsigned>> order;for(unsigned i=0;i<counts.size();++i)if(counts[i])order.push_back({counts[i],i});
  std::sort(order.rbegin(),order.rend());std::string out;
  for(size_t k=0;k<order.size()&&k<40;++k){std::string name="?";try{name=nativeImpactComponents::componentType(world,order[k].second).qualifiedName;if(name.rfind("keen::ecs::",0)==0)name=name.substr(11);}catch(...){}
   out+=std::string(k?",":"")+"["+jsonString(name)+","+std::to_string(order[k].first)+"]";}
  return out;};
 try{logFile->event("live_targets_world_types","\"world\":"+hex(world)+",\"pass\":"+std::to_string(typePasses)+",\"all\":["+top(typeCounts)+"],\"within_60m\":["+top(typeCountsNear)+"]");}catch(...){}
}
// The other players of this World (2026-10-20): for the sweep of their strokes on this host (sequence_collider_observer.hpp
// remoteMelee), handed over after each pass of the map.
inline std::vector<remoteMelee::Player> playersSeen;
// The local player's combat target (2026-10-19, user: "I cannot tell what is locked on - its health bar is on the HUD, not
// above the enemy's head"): keen::ecs::SelectedTargets of the player (16 TargetInfo of 136 bytes, the layout of June and of
// 1.0.0.1 alike - tools/reflection_struct_diff.py), slot 0 = TargetType Combat; TargetInfo: the target's transform +0 (its
// position, Q32, first), the targeting offset +0x38, its entity +0x64, a change counter +0x80, LockedTargetFlags +0x82
// (bit 3 SupportsHardLock). Read with the rest of the list on the local Actor's tick, read only.
// The same target among the player's PlayerRelevantEnemies (8 PlayerRelevantTarget of 80 bytes, the count +0x280; entity +0,
// its transform +8 (position Q32 first), boundingBoxHeight +0x48, validBoundingBox +0x4c, isCurrentTarget +0x4f): how tall it is.
struct CombatTarget {bool valid=false;uint32_t entity=0;uint8_t flags=0;uint16_t counter=0;std::array<int64_t,3> position{};std::array<float,10> offset{};uint64_t ms=0;
 bool relevant=false,boxValid=false,current=false;float height=0;std::array<int64_t,3> relevantPosition{};};
constexpr size_t relevantTargetBytes=0x50,relevantCountAt=0x280;
inline CombatTarget combat;inline uintptr_t localRow=0;constexpr size_t targetInfoBytes=0x88;
// Slot 13 of the same SelectedTargets, keen::ecs::TargetType HardLockCandidate (1.0 reflection, 2026-10-19): the enemy a
// lock would take - the game marks it, and shows its health bar, without any lock or fight (headset run 20261019-185327:
// no combat target at all, the bar of a far enemy unaware in a cave in the HUD after a catch). Read the same way.
constexpr size_t hardLockCandidateSlot=13;inline CombatTarget candidate;
// The player's relevant enemies (PlayerRelevantEnemies, up to 8: the enemies that know of the player) - whose health bars
// the game shows with neither a combat target nor a candidate (headset run 20261019-193946, report 194241: a far enemy's
// bar in the HUD while fishing, nothing selected). The one marked as the current target first, else the nearest within
// 40 m, for the health bar's part (target_mark.hpp).
inline CombatTarget relevantChoice;
struct Cached {evr::motion::LiveTarget target;uintptr_t record=0,layout=0,data=0,pose=0;uint32_t row=0;uint64_t seen=0;};
// The creatures the game itself names this tick (2026-10-20, user: "the first stroke after walking up often misses - with
// ordinary enemies too"): the local player's combat target and hard-lock candidate (SelectedTargets) and its relevant
// enemies (PlayerRelevantEnemies, up to 8), where they stand. Headset run 20261020-112023: the combat target 1.2..2.3 m
// away while the list above was still empty (targets_known 0) - strokes taken for air until the pass of the map reached
// the creature 1..1.6 s later (a pass took 4.3 s there). contact() takes these as characters too, at once.
struct Quick {uint32_t entity=0;std::array<int64_t,3> position{};};
inline std::array<Quick,10> quick{};inline size_t quickCount=0;inline uint64_t quickTick=0;inline std::atomic<unsigned> quickReached{0};
struct Contact {bool reached=true,gated=false;uint32_t entity=0;float distance=0,nearest=-1;unsigned cached=0;bool ahead=false,quick=false;};
// The blade as it was at the last look, a hand each, for where it will be `leadMs` later (live_targets.hpp bladeAhead:
// the game needs about that long from the stroke to the hit volume). EVR_MELEE_LEAD_MS in the game's environment sets
// another time, 0 none (no launch flag). 130 since the headset run 20261014-124515 (user: "the hit comes late - make
// it earlier"): there the press reached the action's first tick after 79..141 ms, 110 in the middle, and the hit of
// the sword lasts 150 ms from that tick, so the press may come a little before the blade arrives. 150 since 2026-10-15
// (headset run 10: the action's first tick 72..141 ms after the press, 97 in the middle; with the lower speed asked of
// a stroke that is still coming (live_melee.hpp approachFactor) the press comes that much before the blade arrives).
inline unsigned leadMs=150;
struct BladeSeen {uint64_t binding=0;std::array<int64_t,3> from{},to{},aheadFrom{},aheadTo{};std::chrono::steady_clock::time_point at{};bool ahead=false,configured=false;};
inline std::array<BladeSeen,2> blades; // guarded by mutex
inline std::mutex mutex;inline std::vector<Cached> cache;inline Types types;inline std::map<uintptr_t,Layout> layouts;
// The health of a kept creature now, read from its row as the pass reads it (-1: not kept, no Health, or not readable).
// For the record of what a stroke did (controller_ui_fixture.hpp live_melee_outcome, 2026-10-20).
inline bool block(uintptr_t address,void* out,size_t bytes);
// (below, at undoDamage: the own player's place and the other players, kept by tick)
inline uint32_t estimateLocal=0;   // the local player's entity (estimateStroke)
inline std::array<int64_t,3> lastOrigin{};inline bool lastOriginKnown=false;inline std::vector<std::pair<uint32_t,std::array<int64_t,3>>> otherPlayers;
inline void sampleOtherPlayers(uint64_t now);
// A guest's arrows on this host (2026-10-21; user 04:30: "the host needs only the hit's damage"). The guest flies its
// own arrow on its PC and decides where it hits (motion_arrow_visual.hpp); its hit comes here (online_sync.hpp,
// sync::hitAction with the arrow's flag). This host's server made that guest's arrow from the guest's input as well
// (bow_spawn_observer.hpp, remoteSpawnAt): in the 1.5 s after such a spawn the entities that come new into this World
// with a Velocity and a Gravity are taken as those arrows; when the guest's hit comes, the newest of them still flying
// is put a metre before the host's thing nearest to the hit (a creature 1.0 m up, a thing 0.4 m) and sent at it, so that
// the game's own hit of that arrow - the damage - is made there. Under mutex. EVR_REMOTE_ARROW_STEER=0: off.
// Parked (user 2026-10-21 04:00: "the host draws what it got; it returns only what comes after the hit - the flying
// arrow is never returned; then the guest has nothing to hide"): the host's server sends every arrow it makes to
// everyone, the shooter too, so a guest's arrow is put out of sight at once - 300 m above where it was made, standing
// still, its gravity off - and only the guest's hit sends it on, to the thing it hit. Without a hit for 3 s it is let
// fall from far below the world (gone, not lying at the guest's feet).
// Only one thing a shot, and only one that flies along the guest's release - its velocity within about 25 degrees of
// the aim, and no farther from where the aim started than it could have come since (4 m + its speed for the time since
// the spawn and 50 ms; 6 m when it does not move yet) - when it is first seen (2026-10-21 05:40, review: every new
// flying thing of the 0.6 s was parked and later dropped - an enemy's arrow, or anything thrown out at a kill, would
// have been lost with it). A second arrow of the same shot flies as made.
struct RemoteArrow {uint32_t entity=0;uint64_t ms=0;bool parked=false;};
struct ShotHit {std::array<int64_t,3> at{};bool character=false,used=true;uint64_t ms=0;uint32_t entity=0;};
inline std::array<RemoteArrow,8> remoteArrows{};inline std::array<ShotHit,4> shotHits{};
inline std::unordered_map<uint32_t,uint8_t> arrowKnown;inline bool arrowPrimed=false;inline uint64_t arrowScanAt=0;inline unsigned arrowLogs=0;
inline std::vector<uint32_t> arrowKeys;inline std::vector<uint64_t> arrowBits;inline uint64_t arrowParkedFor=0;
inline const bool steerWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_ARROW_STEER",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline void publishShotHit(const std::array<int64_t,3>& at,bool character,uint32_t entity,uint64_t now);
inline void steerArrows(uintptr_t world,uint64_t now);
inline void thingOutcomes(uint64_t now);
inline void estimateProbe(uintptr_t world,uint32_t local,uint64_t now);
inline void forgetWorldRecords(uintptr_t world);
inline void weaponProbe(uintptr_t world,uint32_t local,const char* why);
inline Layout layout(uintptr_t address);
inline int32_t healthNow(uint32_t entity){
 std::lock_guard<std::mutex> lock(mutex);
 const auto c=std::find_if(cache.begin(),cache.end(),[&](const Cached& x){return x.target.entity==entity;});
 if(c==cache.end())return -1;
 // layout(), not the kept layouts alone: those are emptied at each pass of the map (2026-10-20 17:15, headset run
 // 20261020-165058: 70 of 132 hit stops came from "no health" turning into a health at a pass, not from a hit).
 const auto l=layout(c->layout);if(!l.health)return -1;
 const auto bytes=types.healthBytes;if(bytes!=68&&bytes!=40)return -1;
 std::array<uint8_t,256> h{};if(!block(c->data+l.healthOffset+uintptr_t(c->row)*bytes,h.data(),bytes))return -1;
 int32_t health=-1;
 if(bytes==68){uint32_t offset=0;std::memcpy(&offset,h.data()+8,4);if(!(h[16]&1)||offset!=36)return -1;std::memcpy(&health,h.data()+offset,4);}
 else{if(!h[4]||h[4]>8)return -1;std::memcpy(&health,h.data()+8,4);}
 return health;
}
inline uint64_t cursor=0,lastTick=0,lastLog=0,fullScans=0;inline std::atomic<unsigned> ticks{0},failures{0},adopted{0},dropped{0};
// 2026-10-20 15:30: a whole pass of the 131072 slots took 2.5 s (Mock) to 4.3 s (headset run 20261020-112023) at 60 ticks
// a second, where 4096 slots a tick make it 0.5 s (and the client World of 65536 slots took 16 ticks, as made). What
// each tick did since the last live_targets_state: slots read, ticks that read none and why, the time the ticks took.
inline uint64_t slotsRead=0,scanTicks=0,noRoot=0,relearns=0,wholeFailed=0;inline double tickSeconds=0,tickSecondsMax=0;
// 4096 of the map's 131072 slots a tick: the whole map about twice a second. A blade reaches 2.3 m from the player and
// a target counts from 1.3 m, so 10 m leaves a running creature a second to be noticed. Measured cost: none visible
// in the frame times (game191128, 2048 slots: median 27.0 ms with the list, 26.9 ms without).
constexpr unsigned slotsPerTick=4096,maximumCached=64;
constexpr double keepMetres=10,dropMetres=12;
// Destructible things without a health (tables, pots, crates; 2026-10-20: 490 within 60 m of the home, 12 of them with
// Health - live_targets_world_types of the server's World, Mock 20261020-083517): within reach only, and a few, so that
// they never push the creatures out of the list.
// EVR_MELEE_OBJECT_METRES=<1..15>: another distance (a test where nothing breakable stands within 4 m).
inline const double objectKeepMetres=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_OBJECT_METRES",v,sizeof v);const double m=n&&n<sizeof v?std::atof(v):4.;return m>=1&&m<=15?m:4.;}();constexpr unsigned maximumObjects=24;
// Creatures are kept further out: the aim mark of a bow, staff or wand ends on the creature its line passes through
// (aimReach below; include/evr/aim_mark.hpp markReach). The nearest 64 stay, as before.
constexpr double aimKeepMetres=60,aimDropMetres=66;
inline bool block(uintptr_t address,void* out,size_t bytes){
 SIZE_T count=0;return address&&bytes&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&count)&&count==bytes;
}
inline double metres(const std::array<int64_t,3>& a,const std::array<int64_t,3>& b){
 constexpr double q=1.0/4294967296.0;const double x=double(a[0]-b[0])*q,y=double(a[1]-b[1])*q,z=double(a[2]-b[2])*q;return std::sqrt(x*x+y*y+z*z);
}
// The distance of a place to the nearest player - the own one (`own` metres away) or another (2026-10-22: a host kept
// only the things within 4 m of its own player, and a guest's touch of a pot by the guest found none). Under mutex.
inline double nearestPlayerMetres(const std::array<int64_t,3>& at,double own){
 if(guestWorld)return own;   // a guest touches only what is near itself (review W10: the things by others took its 24 places)
 for(const auto& [id,p]:otherPlayers){(void)id;own=std::min(own,metres(at,p));}return own;
}
// The ids of Health, CurrentTransform and Actor in this World's component registry (256 byte records, name at +16).
inline void learn(uintptr_t world){
 types={};types.world=world;
 const auto registry=read<uintptr_t>(world);const auto header=read<std::array<uintptr_t,4>>(registry);
 if(!header[1]||!header[2]||header[2]>header[3]||header[3]>nativeImpactComponents::componentCapacity)throw std::runtime_error("live target type registry bounds");
 std::vector<uint8_t> records(size_t(header[2])*256);
 if(!block(header[1],records.data(),records.size()))throw std::runtime_error("live target type registry unreadable");
 for(unsigned id=0;id<header[2];++id){
  const auto* raw=records.data()+size_t(id)*256;uintptr_t text=0,length=0;uint16_t size=0;std::memcpy(&text,raw+16,8);std::memcpy(&length,raw+24,8);std::memcpy(&size,raw+56,2);
  // up to 64 characters (2026-10-22: keen::ecs::CollisionFeedbackMaterialComponent has 45, and with a limit of 40 it was
  // never found - a guest's pots and trees were never things, guest log 20261022-000930: objects 0 all session)
  if(!text||length<16||length>64)continue;
  char name[65]{};if(!block(text,name,size_t(length)))continue;const std::string_view n(name,size_t(length));
  if(n=="keen::ecs::Health"&&size>=40&&size<=256){types.health=int(id);types.healthBytes=size;}
  else if(n=="keen::ecs::CurrentTransform"&&size==56)types.current=int(id);
  else if(n=="keen::ecs::Actor")types.actor=int(id);
  else if(n=="keen::ecs::SelectedTargets"&&size>=16*targetInfoBytes&&size<=0x1000){types.selected=int(id);types.selectedBytes=size;}
  else if(n=="keen::ecs::PlayerRelevantEnemies"&&size>relevantCountAt&&size<=0x400){types.relevant=int(id);types.relevantBytes=size;}
  else if(n=="keen::ecs::NetworkHealth"&&size==8)types.networkHealth=int(id);
  else if(n=="keen::ecs::RenderTransform"&&size>=56&&size<=512){types.render=int(id);types.renderBytes=size;}
  else if(n=="keen::ecs::NetworkActor")types.networkActor=int(id);
  else if(n=="keen::ecs::NetworkPlayerAttributes"&&size>=0x34&&size<=0x400){types.player=int(id);types.playerBytes=size;}
  else if(n=="keen::ecs::NetworkEnemy")types.enemy=int(id);
  else if(n=="keen::ecs::Destructible")types.destructible=int(id);
  else if(n=="keen::ecs::StaticClientCollider")types.clientCollider=int(id);
  else if(n=="keen::ecs::CollisionFeedbackMaterialComponent")types.feedbackMaterial=int(id);
  else if(n=="keen::ecs::Velocity"&&size==12)types.velocity=int(id);
  else if(n=="keen::ecs::Gravity"&&size==20)types.gravity=int(id);
  else if(n=="keen::ecs::Stamina"&&size==40)types.stamina=int(id);
  else if(n=="keen::ecs::Block"&&size==12)types.blockValue=int(id);
  else if(n=="keen::ecs::StaminaDamageMod"&&size==24)types.staminaDamageMod=int(id);
  else if(n=="keen::ecs::ModelComponent"&&size>=4&&size<=64){types.model=int(id);types.modelBytes=size;}
 }
 if(types.health<0){   // which types of this build look like it (bounded; names up to 64 characters)
  std::string seen;unsigned shown=0;
  for(unsigned id=0;id<header[2]&&shown<12;++id){
   const auto* raw=records.data()+size_t(id)*256;uintptr_t text=0,length=0;uint16_t size=0;std::memcpy(&text,raw+16,8);std::memcpy(&length,raw+24,8);std::memcpy(&size,raw+56,2);
   if(!text||!length||length>64)continue;char name[65]{};if(!block(text,name,size_t(length)))continue;
   if(std::string_view(name,size_t(length)).find("ealth")==std::string_view::npos)continue;
   seen+=std::string(shown++?",":"")+"{\"id\":"+std::to_string(id)+",\"name\":"+jsonString(std::string(name,size_t(length)))+",\"size\":"+std::to_string(size)+"}";}
  logFile->event("live_targets_health_candidates","\"types\":["+seen+"]");
 }
 types.registry=registry;types.usable=(types.health>=0||types.networkHealth>=0)&&(types.current>=0||types.render>=0);
 logFile->event("live_targets_types","\"world\":"+hex(world)+",\"types\":"+std::to_string(header[2])+",\"health\":"+std::to_string(types.health)+",\"health_bytes\":"+std::to_string(types.healthBytes)+",\"current_transform\":"+std::to_string(types.current)+
  ",\"actor\":"+std::to_string(types.actor)+",\"selected_targets\":"+std::to_string(types.selected)+",\"selected_targets_bytes\":"+std::to_string(types.selectedBytes)+",\"relevant_enemies\":"+std::to_string(types.relevant)+",\"network_health\":"+std::to_string(types.networkHealth)+",\"render_transform\":"+std::to_string(types.render)+
  ",\"player_attributes\":"+std::to_string(types.player)+",\"usable\":"+(types.usable?"true":"false")+",\"read_only\":true");
}
inline Layout layout(uintptr_t address){
 const auto known=layouts.find(address);if(known!=layouts.end())return known->second;
 if(layouts.size()>=2048)layouts.clear();
 Layout l;std::array<uint64_t,10> bits{};
 if(block(address,bits.data(),sizeof(bits))){
  const auto has=[&](int id){return id>=0&&((bits[size_t(id)/64]>>(unsigned(id)%64))&1);};
  const auto place=[&](int id,uint16_t bytes,uint16_t& offset){uint16_t stride=0;return block(address+0xa84+uintptr_t(id)*2,&stride,2)&&stride==bytes&&block(address+0x84+uintptr_t(id)*2,&offset,2);};
  l.health=has(types.health)&&place(types.health,types.healthBytes,l.healthOffset);
  l.current=has(types.current)&&place(types.current,56,l.currentOffset);
  l.actor=has(types.actor)||(!l.health&&has(types.networkActor));   // the replicated actor only where the server's Health is missing (a guest's World)
  l.networkHealth=has(types.networkHealth)&&place(types.networkHealth,8,l.networkHealthOffset);
  l.render=has(types.render)&&place(types.render,types.renderBytes,l.renderOffset);
  l.player=types.player>=0&&has(types.player)&&place(types.player,types.playerBytes,l.playerOffset);
  l.enemy=has(types.enemy);l.destructible=has(types.destructible);l.bits=bits;
  l.clientThing=has(types.clientCollider)&&has(types.feedbackMaterial);
  l.selected=types.selected>=0&&has(types.selected)&&place(types.selected,types.selectedBytes,l.selectedOffset);
  l.relevant=types.relevant>=0&&has(types.relevant)&&place(types.relevant,types.relevantBytes,l.relevantOffset);
 }
 layouts[address]=l;return l;
}
struct Record {uintptr_t layout=0,data=0;uint32_t row=0;bool valid=false;};
inline Record record(uintptr_t address){
 std::array<uint8_t,0x38> raw{};Record r;if(!block(address,raw.data(),raw.size()))return r;
 std::memcpy(&r.layout,raw.data()+0x18,8);std::memcpy(&r.data,raw.data()+0x20,8);std::memcpy(&r.row,raw.data()+0x30,4);
 r.valid=r.layout&&r.data&&r.row<=0x1000000;return r;
}
// Once per tick of the local player's Actor, on the game's simulation thread.
inline void tick(uintptr_t world,uint32_t local){
 if(!requested||!world||!local)return;
 try{
  rootCamera::Sample root;if(!rootCamera::trySnapshot(root)||root.entity!=local){++noRoot;return;}
  const std::array<int64_t,3> origin{root.stable.position.x,root.stable.position.y,root.stable.position.z};
  const auto now=GetTickCount64();std::lock_guard<std::mutex> lock(mutex);++ticks;
  const auto started=std::chrono::steady_clock::now();
  struct Timed{std::chrono::steady_clock::time_point at;~Timed(){const double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-at).count();tickSeconds+=s;tickSecondsMax=std::max(tickSecondsMax,s);}}timed{started};
  if(types.world!=world||types.registry!=read<uintptr_t>(world)){++relearns;layouts.clear();cache.clear();cursor=0;guestWorld=false;
   forgetWorldRecords(world);   // (review W11: no writes to an old World's ids)
   learn(world);censusLogs=0;census={};typeCounts={};typePasses=0;}
  if(!types.usable)return;
  lastOrigin=origin;lastOriginKnown=true;estimateLocal=local;estimateProbe(world,local,now);sampleOtherPlayers(now);steerArrows(world,now);thingOutcomes(now);
  // The targets already known: where they are now, or forget them.
  for(auto i=cache.begin();i!=cache.end();){
   const auto r=record(i->record);game::RootTransform t{};
   bool keep=r.valid&&r.layout==i->layout&&r.data==i->data&&r.row==i->row&&now-i->seen<=4000&&block(i->pose,&t,sizeof(t));
   if(keep){i->target.position={t.position.x,t.position.y,t.position.z};keep=nearestPlayerMetres(i->target.position,metres(i->target.position,origin))<=(i->target.character?aimDropMetres:i->target.object?objectKeepMetres+1:dropMetres);}
   if(keep)++i;else{i=cache.erase(i);++dropped;}
  }
  // A slice of the World's entity map.
  std::array<uint8_t,0x58> map{};if(!block(world+0x148,map.data(),map.size()))throw std::runtime_error("live target entity map unreadable");
  uintptr_t bitmap=0,keys=0,values=0;uint64_t capacity=0;std::memcpy(&bitmap,map.data()+8,8);std::memcpy(&capacity,map.data()+0x10,8);std::memcpy(&keys,map.data()+0x28,8);std::memcpy(&values,map.data()+0x40,8);
  if(!bitmap||!keys||!values||!capacity||capacity>0x100000||(capacity&(capacity-1)))throw std::runtime_error("live target entity map bounds");
  if(cursor>=capacity){cursor=0;++fullScans;layouts.clear();
   if(censusLogs<4||fullScans%500==0){++censusLogs;
    logFile->event("live_targets_census","\"full_scans\":"+std::to_string(fullScans)+",\"entities_within_60m\":"+std::to_string(census.entities)+",\"health\":"+std::to_string(census.health)+
     ",\"network_health\":"+std::to_string(census.networkHealth)+",\"current_transform\":"+std::to_string(census.current)+",\"render_transform\":"+std::to_string(census.render)+
     ",\"objects\":"+std::to_string(census.objects)+",\"objects_without_pose\":"+std::to_string(census.objectsWithoutPose)+",\"nearest_object_m\":"+std::to_string(census.nearestObject)+
     ",\"players\":"+std::to_string(census.player)+",\"fallen_players\":"+std::to_string(census.downed)+",\"cached\":"+std::to_string(cache.size())+
     ",\"map_entities\":"+std::to_string(census.all)+",\"map_health\":"+std::to_string(census.allHealth)+",\"map_network_health\":"+std::to_string(census.allNetworkHealth)+
     ",\"map_current_transform\":"+std::to_string(census.allCurrent)+",\"map_render_transform\":"+std::to_string(census.allRender)+",\"map_players\":"+std::to_string(census.allPlayers));}
   {const bool now_blind=census.all>=64&&!census.allHealth&&!census.allNetworkHealth&&!census.allEnemies;
    if(now_blind!=blind&&blindLogs++<50)logFile->event("live_targets_blind","\"world\":"+hex(world)+",\"blind\":"+(now_blind?"true":"false")+",\"entities\":"+std::to_string(census.all)+",\"gate\":\""+(now_blind?"off_strokes_attack":"on")+"\"");
    blind=now_blind;
    // kept while the World has no Health (2026-10-22, guest log 20261022-000930: with no creature about the count of
    // NetworkHealth fell to 0 and the guest's pots and trees stopped being things)
    const bool nowGuest=guestThingsWanted&&census.all>=64&&!census.allHealth&&(census.allNetworkHealth>0||guestWorld);
    if(nowGuest!=guestWorld&&guestLogs++<50)logFile->event("live_targets_guest_world","\"world\":"+hex(world)+",\"guest\":"+(nowGuest?"true":"false")+",\"network_health\":"+std::to_string(census.allNetworkHealth)+
     ",\"client_colliders_are_things\":"+(nowGuest&&types.clientCollider>=0&&types.feedbackMaterial>=0?"true":"false"));
    if(nowGuest&&!guestWorld)weaponProbe(world,local,"guest_world");
    guestWorld=nowGuest;}
   otherPlayers.clear();for(const auto& p:playersSeen)if(p.entity!=local)otherPlayers.push_back({p.entity,{p.at.x,p.at.y,p.at.z}});
   census={};remoteMelee::publishPlayers(playersSeen);playersSeen.clear();
   if(typePasses<2)tellTypes(world);typeCounts={};typeCountsNear={};}
  // Fallen players not seen again for a while are forgotten.
  downed.erase(std::remove_if(downed.begin(),downed.end(),[&](const Downed& d){return now<d.seen||now-d.seen>1500;}),downed.end());
  // 2026-10-20 16:40: the server World's map holds 1048576 slots (Mock game-20261020-163718; 131072 is the client World's
  // of the menu), so 4096 a tick took 256 ticks, 4.3 s, for a pass - a creature that came near was not known for seconds
  // (the first stroke at it taken for air). A tick of 4096 slots took 0.07 ms (mean, the same Mock): a pass now takes
  // about 48 ticks (0.8 s) whatever the map's size, 4096..32768 slots a tick.
  const uint64_t perTick=std::clamp<uint64_t>(capacity/48,slotsPerTick,32768);
  const uint64_t first=cursor&~63ull,count=std::min<uint64_t>(perTick,capacity-first);
  std::vector<uint64_t> occupied(size_t((count+63)/64));std::vector<uint32_t> ids(static_cast<size_t>(count));std::vector<uintptr_t> rows(static_cast<size_t>(count));
  std::array<uint8_t,0x58> again{};
  const bool whole=block(bitmap+(first/64)*8,occupied.data(),occupied.size()*8)&&block(keys+first*4,ids.data(),ids.size()*4)&&block(values+first*8,rows.data(),rows.size()*8)&&
   block(world+0x148,again.data(),again.size())&&again==map;
  cursor=first+count;slotsRead+=count;++scanTicks;if(!whole)++wholeFailed;
  if(whole)for(uint64_t at=0;at<count;++at){
   if(!((occupied[size_t(at/64)]>>(at%64))&1))continue;
   const auto id=ids[size_t(at)];if(id&&id==local&&rows[size_t(at)])localRow=rows[size_t(at)];
   if(!id||id==local||!rows[size_t(at)])continue;
   const auto r=record(rows[size_t(at)]);if(!r.valid)continue;
   const auto l=layout(r.layout);
   if(typePasses<2)for(unsigned b=0;b<640;++b)if((l.bits[b/64]>>(b%64))&1)++typeCounts[b];
   ++census.all;if(l.health)++census.allHealth;if(l.networkHealth)++census.allNetworkHealth;if(l.current)++census.allCurrent;if(l.render)++census.allRender;if(l.player)++census.allPlayers;if(l.enemy)++census.allEnemies;
   const bool object=(l.destructible||(guestWorld&&l.clientThing))&&!(l.health||l.networkHealth||l.enemy||l.actor);
   if(object&&!(l.current||l.render))++census.objectsWithoutPose;
   if(!(l.health||l.networkHealth||l.enemy||object)||!(l.current||l.render)){
    if(typePasses<2&&(l.current||l.render)){game::RootTransform t{};const auto spot=l.current?r.data+l.currentOffset+uintptr_t(r.row)*56:r.data+l.renderOffset+uintptr_t(r.row)*types.renderBytes;
     if(block(spot,&t,sizeof t)&&metres({t.position.x,t.position.y,t.position.z},origin)<=aimKeepMetres)for(unsigned b=0;b<640;++b)if((l.bits[b/64]>>(b%64))&1)++typeCountsNear[b];}
    continue;}
   const auto pose=l.current?r.data+l.currentOffset+uintptr_t(r.row)*56:r.data+l.renderOffset+uintptr_t(r.row)*types.renderBytes;
   game::RootTransform t{};if(!block(pose,&t,sizeof(t)))continue;
   const std::array<int64_t,3> position{t.position.x,t.position.y,t.position.z};
   const double away=metres(position,origin);
   if(object&&away<=aimKeepMetres){++census.objects;if(census.nearestObject<0||away<census.nearestObject)census.nearestObject=away;}
   if(away<=aimKeepMetres){++census.entities;if(l.health)++census.health;if(l.networkHealth)++census.networkHealth;if(l.current)++census.current;if(l.render)++census.render;if(l.player)++census.player;}
   if(l.player&&playersSeen.size()<8)playersSeen.push_back({id,WorldPositionQ32{position[0],position[1],position[2]},now});
   if(typePasses<2&&away<=aimKeepMetres)for(unsigned b=0;b<640;++b)if((l.bits[b/64]>>(b%64))&1)++typeCountsNear[b];
   int32_t health=0;
   if(!l.health&&!l.networkHealth)health=1;   // NetworkEnemy alone (a guest's World without either health), or a destructible thing: taken as there
   else if(!l.health){   // a guest's World: the replicated health
    uint32_t value[2]{};if(!block(r.data+l.networkHealthOffset+uintptr_t(r.row)*8,value,sizeof value)||!value[1]||value[0]>value[1])continue;
    health=int32_t(std::min<uint32_t>(value[0],0x7fffffff));
   }else{
   const auto hp=r.data+l.healthOffset+uintptr_t(r.row)*types.healthBytes;
   std::array<uint8_t,256> h{};if(!block(hp,h.data(),types.healthBytes))continue;
   {static std::atomic<unsigned> shown{0};if(types.healthBytes!=68&&shown.fetch_add(1)<6){const char* d="0123456789abcdef";std::string s;
     for(unsigned k=0;k<types.healthBytes;++k){s+=d[h[k]>>4];s+=d[h[k]&15];}
     logFile->event("live_targets_health_sample","\"entity\":"+std::to_string(id)+",\"bytes\":"+std::to_string(types.healthBytes)+",\"health\":\""+s+"\"");}}
   if(types.healthBytes==68){
    uint32_t offset=0,values4=0;std::memcpy(&offset,h.data()+8,4);std::memcpy(&values4,h.data()+12,4);
    if(!(h[16]&1)||offset!=36||!values4||values4>8)continue;                // the Health layout that motion_target_observer verified
    std::memcpy(&health,h.data()+offset,4);
   }else if(types.healthBytes==40){
    // 1.0: a u32 hash, a byte count of values (8), two bytes, then eight int32 values from +8 (live_targets_health_sample,
    // Mock 20261016-141748: a living creature 20 at +8, +16 and +36, a dead or empty row all zero). The first is taken.
    if(!h[4]||h[4]>8)continue;std::memcpy(&health,h.data()+8,4);
   }else continue;
   }
   if(health<=0){                    // dead or destroyed; a player at no health is fallen and can be revived
    if(l.player&&away<=6){++census.downed;
     auto d=std::find_if(downed.begin(),downed.end(),[&](const Downed& x){return x.entity==id;});
     if(d==downed.end()){if(downed.size()>=8)continue;downed.push_back({});d=downed.end()-1;
      static std::atomic<unsigned> told{0};if(told.fetch_add(1)<100)logFile->event("fallen_player_seen","\"entity\":"+std::to_string(id)+",\"metres\":"+std::to_string(away)+",\"guest_world\":"+(l.health?"false":"true"));}
     d->entity=id;d->position=position;d->seen=now;}
    continue;}
   // Players are no targets (2026-10-22, the friend's host log: guest hits laid on the guest itself and on the host)
   if(l.player)continue;
   if(nearestPlayerMetres(position,away)>(l.actor?aimKeepMetres:object?objectKeepMetres:keepMetres))continue;
   const auto same=record(rows[size_t(at)]);if(!same.valid||same.layout!=r.layout||same.data!=r.data||same.row!=r.row)continue;
   auto known=std::find_if(cache.begin(),cache.end(),[&](const Cached& c){return c.target.entity==id;});
   if(known==cache.end()){
    if(object){   // a few things at most: the nearest of them
     const auto objects=unsigned(std::count_if(cache.begin(),cache.end(),[](const Cached& c){return c.target.object;}));
     if(objects>=maximumObjects){auto farthest=cache.end();double farMetres=-1;
      for(auto it=cache.begin();it!=cache.end();++it)if(it->target.object){const double m=nearestPlayerMetres(it->target.position,metres(it->target.position,origin));if(m>farMetres){farMetres=m;farthest=it;}}
      if(farthest==cache.end()||farMetres<=nearestPlayerMetres(position,away))continue;cache.erase(farthest);++dropped;}
    }
    if(cache.size()>=maximumCached){   // keep the nearest
     auto farthest=std::max_element(cache.begin(),cache.end(),[&](const Cached& a,const Cached& b){return metres(a.target.position,origin)<metres(b.target.position,origin);});
     if(metres(farthest->target.position,origin)<=metres(position,origin))continue;
     cache.erase(farthest);++dropped;
    }
    cache.push_back({});known=cache.end()-1;++adopted;
   }
   known->target.entity=id;known->target.position=position;known->target.character=l.actor;known->target.object=object;
   known->record=rows[size_t(at)];known->layout=r.layout;known->data=r.data;known->row=r.row;known->pose=pose;known->seen=now;
  }
  // The local player's combat target (slot 0 of its SelectedTargets), once a tick; and its hard-lock candidate (slot 13).
  std::array<Quick,8> relevantSeen{};size_t relevantCount=0;bool selectedRead=false;
  if(types.selected>=0&&localRow){
   const auto r=record(localRow);CombatTarget c,h;
   if(r.valid){const auto l=layout(r.layout);
    if(l.selected){
     auto readSlot=[&](size_t slot,CombatTarget& t){std::array<uint8_t,targetInfoBytes> info{};
      if(!block(r.data+l.selectedOffset+uintptr_t(r.row)*types.selectedBytes+slot*targetInfoBytes,info.data(),info.size()))return;
      t.valid=true;std::memcpy(t.position.data(),info.data(),24);std::memcpy(t.offset.data(),info.data()+0x38,40);std::memcpy(&t.entity,info.data()+0x64,4);
      std::memcpy(&t.counter,info.data()+0x80,2);t.flags=info[0x82];t.ms=now;};
     readSlot(0,c);if(types.selectedBytes>=(hardLockCandidateSlot+1)*targetInfoBytes)readSlot(hardLockCandidateSlot,h);selectedRead=true;
     CombatTarget pick;double pickMetres=40.;
     if(l.relevant){std::array<uint8_t,relevantCountAt+1> enemies{};
      if(block(r.data+l.relevantOffset+uintptr_t(r.row)*types.relevantBytes,enemies.data(),enemies.size()))
       for(unsigned k=0;k<std::min<unsigned>(enemies[relevantCountAt],8);++k){const auto* e=enemies.data()+k*relevantTargetBytes;uint32_t who=0;std::memcpy(&who,e,4);
        for(auto* t:{&c,&h}){if(!t->valid||!t->entity||who!=t->entity||t->relevant)continue;
         t->relevant=true;std::memcpy(t->relevantPosition.data(),e+8,24);std::memcpy(&t->height,e+0x48,4);t->boxValid=e[0x4c]!=0;t->current=e[0x4f]!=0;}
        if(!who)continue;
        if(relevantCount<relevantSeen.size()){Quick s;s.entity=who;std::memcpy(s.position.data(),e+8,24);relevantSeen[relevantCount++]=s;}
        CombatTarget q;q.valid=true;q.entity=who;q.ms=now;q.relevant=true;std::memcpy(q.relevantPosition.data(),e+8,24);q.position=q.relevantPosition;
        std::memcpy(&q.height,e+0x48,4);q.boxValid=e[0x4c]!=0;q.current=e[0x4f]!=0;
        const double m=metres(q.position,origin);
        if((q.current&&!pick.current)||(q.current==pick.current&&m<pickMetres)){pick=q;pickMetres=m;}}}
     if(pick.entity!=relevantChoice.entity){static std::atomic<unsigned> toldRelevant{0};
      if(toldRelevant.fetch_add(1)<400)logFile->event("relevant_enemy_choice","\"entity\":"+std::to_string(pick.entity)+",\"current\":"+(pick.current?"true":"false")+",\"metres\":"+std::to_string(pick.entity?pickMetres:-1.));}
     relevantChoice=pick;}}
   else localRow=0;
   if(h.entity!=candidate.entity){static std::atomic<unsigned> toldCandidate{0};
    if(toldCandidate.fetch_add(1)<400)logFile->event("hard_lock_candidate","\"entity\":"+std::to_string(h.entity)+",\"metres\":"+std::to_string(h.valid&&h.entity?metres(h.position,origin):-1.)+",\"relevant\":"+(h.relevant?"true":"false"));}
   candidate=h;
   if(c.entity!=combat.entity||c.flags!=combat.flags||c.valid!=combat.valid){
    static std::atomic<unsigned> told{0};
    if(told.fetch_add(1)<400){std::ostringstream o;o<<std::setprecision(5)<<"\"valid\":"<<(c.valid?"true":"false")<<",\"entity\":"<<c.entity<<",\"flags\":"<<unsigned(c.flags)<<",\"counter\":"<<c.counter
      <<",\"metres\":"<<(c.valid&&c.entity?metres(c.position,origin):-1.)<<",\"up_metres\":"<<(c.valid&&c.entity?double(c.position[1]-origin[1])/4294967296.0:0.)<<",\"relevant\":"<<(c.relevant?"true":"false")
      <<",\"box_height\":"<<c.height<<",\"box_valid\":"<<(c.boxValid?"true":"false")<<",\"current\":"<<(c.current?"true":"false")
      <<",\"relevant_up_metres\":"<<(c.relevant?double(c.relevantPosition[1]-origin[1])/4294967296.0:0.)<<",\"offset\":[";
     for(size_t k=0;k<c.offset.size();++k)o<<(k?",":"")<<c.offset[k];
     o<<"]";logFile->event("combat_target",o.str());}}
   combat=c;
  }
  // The creatures named this tick (Quick above): the combat target, the candidate, the relevant enemies, within 15 m.
  {quickCount=0;
   const auto add=[&](uint32_t entity,const std::array<int64_t,3>& position){
    if(!entity||entity==local||quickCount>=quick.size()||metres(position,origin)>15.)return;
    for(size_t k=0;k<quickCount;++k)if(quick[k].entity==entity)return;
    quick[quickCount].entity=entity;quick[quickCount].position=position;++quickCount;};
   if(selectedRead){if(combat.valid)add(combat.entity,combat.position);if(candidate.valid)add(candidate.entity,candidate.position);}
   for(size_t k=0;k<relevantCount;++k)add(relevantSeen[k].entity,relevantSeen[k].position);
   quickTick=now;}
  lastTick=now;
  if(now-lastLog>=5000){lastLog=now;
   std::ostringstream o;o<<std::setprecision(4)<<"\"cached\":"<<cache.size()<<",\"full_scans\":"<<fullScans<<",\"ticks\":"<<ticks.load()<<",\"map_capacity\":"<<capacity<<",\"adopted\":"<<adopted.load()<<",\"dropped\":"<<dropped.load()<<",\"failures\":"<<failures.load()<<",\"quick\":"<<quickCount<<",\"quick_reached\":"<<quickReached.load()<<",\"slots_read\":"<<slotsRead<<",\"scan_ticks\":"<<scanTicks<<",\"no_root\":"<<noRoot<<",\"relearns\":"<<relearns
     <<",\"whole_failed\":"<<wholeFailed<<",\"tick_ms_mean\":"<<(scanTicks?tickSeconds*1000/double(scanTicks):0.)<<",\"tick_ms_max\":"<<tickSecondsMax*1000<<",\"nearest\":[";
   slotsRead=scanTicks=noRoot=relearns=wholeFailed=0;tickSeconds=tickSecondsMax=0;
   std::vector<const Cached*> order;for(const auto& c:cache)order.push_back(&c);
   std::sort(order.begin(),order.end(),[&](const Cached* a,const Cached* b){return metres(a->target.position,origin)<metres(b->target.position,origin);});
   for(size_t i=0;i<order.size()&&i<6;++i)o<<(i?",":"")<<"{\"entity\":"<<order[i]->target.entity<<",\"metres\":"<<metres(order[i]->target.position,origin)<<",\"character\":"<<(order[i]->target.character?"true":"false")<<(order[i]->target.object?",\"object\":true":"")<<'}';
   o<<"]";logFile->event("live_targets_state",o.str());}
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("live_targets_failure","\"reason\":"+jsonString(e.what()));}
}
// Any thread: is the blade of this weapon at a known target? Without a usable, recent list the answer is "reached".
inline Contact contact(const evr::motion::LiveMeleeWeapon& weapon,uint64_t now){
 Contact out;std::lock_guard<std::mutex> lock(mutex);out.cached=unsigned(cache.size());
 if(!requested||!types.usable||!lastTick||now<lastTick||now-lastTick>1000||!weapon.blade||blind)return out;
 out.gated=true;out.reached=false;float best=std::numeric_limits<float>::infinity();const evr::motion::LiveTargetReach reach;
 auto& seen=blades[weapon.hand?1:0];const auto time=std::chrono::steady_clock::now();
 {static bool read=false;if(!read){read=true;char value[16]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_LEAD_MS",value,sizeof value);if(n&&n<sizeof value)leadMs=std::min(250u,unsigned(std::strtoul(value,nullptr,10)));
   try{logFile->event("live_targets_lead","\"lead_ms\":"+std::to_string(leadMs)+",\"maximum_metres\":0.8");}catch(...){}}}
 if(seen.binding!=weapon.binding){seen={};seen.binding=weapon.binding;seen.from=weapon.bladeFrom;seen.to=weapon.bladeTo;seen.at=time;}
 else if(seen.from!=weapon.bladeFrom||seen.to!=weapon.bladeTo){
  const double elapsed=std::chrono::duration<double>(time-seen.at).count();
  seen.aheadFrom=evr::motion::bladeAhead(weapon.bladeFrom,seen.from,elapsed,leadMs*.001);seen.aheadTo=evr::motion::bladeAhead(weapon.bladeTo,seen.to,elapsed,leadMs*.001);
  seen.ahead=leadMs&&(seen.aheadFrom!=weapon.bladeFrom||seen.aheadTo!=weapon.bladeTo);seen.from=weapon.bladeFrom;seen.to=weapon.bladeTo;seen.at=time;
 }else if(std::chrono::duration<double>(time-seen.at).count()>.12)seen.ahead=false;   // the blade has not moved for a while
 const auto test=[&](const evr::motion::LiveTarget& target){
  float distance=evr::motion::bladeDistance(weapon.bladeFrom,weapon.bladeTo,target,reach);bool touches=evr::motion::bladeNear(weapon.bladeFrom,weapon.bladeTo,target,reach),later=false;
  if(!touches&&seen.ahead&&evr::motion::bladeNear(seen.aheadFrom,seen.aheadTo,target,reach)){touches=later=true;distance=evr::motion::bladeDistance(seen.aheadFrom,seen.aheadTo,target,reach);}
  if(touches&&distance<best){best=distance;out.reached=true;out.entity=target.entity;out.distance=distance;out.ahead=later;return true;}
  if(std::isfinite(distance)&&(out.nearest<0||distance<out.nearest))out.nearest=distance;
  return false;};
 for(const auto& c:cache)test(c.target);
 // The creatures the game names this tick that the list does not hold yet (Quick above).
 if(quickTick&&now>=quickTick&&now-quickTick<=1000)for(size_t k=0;k<quickCount;++k){
  const auto& q=quick[k];if(std::any_of(cache.begin(),cache.end(),[&](const Cached& c){return c.target.entity==q.entity;}))continue;
  evr::motion::LiveTarget t;t.entity=q.entity;t.position=q.position;t.character=true;
  if(test(t)){out.quick=true;++quickReached;}
 }
 return out;
}
// Any thread, for the hit stop (2026-10-20, controller_ui_fixture.hpp hitWatch): the kept things and the creatures the game
// names this tick (Quick) whose axis is within `radius` m of the blade: the blade's distance to the axis now, the radius of
// the body there - where the blade touches it - and the state (stateNow below; -1 for a named creature not kept yet).
// The bodies (2026-10-20 17:20, user: "the stop comes after the blade has passed through the enemy - stop it the moment it
// touches the model, and let the hit effect come while it stands"; the game's hit came 109 ms after the press, median of
// 27 in the headset run 20261020-165058): a creature 0.5 m from its axis (the strokes the game took were 0.26..0.60 m from
// it, headset run 20261014-124515), a thing without health (a pot) 0.4, anything else (a tree) 0.55.
struct NearBlade {uint32_t entity=0;float distance=0,body=0,reach=0;int32_t state=-1;bool character=false;};
inline int32_t stateLocked(const Cached& c);
inline size_t nearBlade(const evr::motion::LiveMeleeWeapon& weapon,float radius,NearBlade* out,size_t maximum,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);size_t n=0;
 if(!requested||!types.usable||!weapon.blade)return 0;const evr::motion::LiveTargetReach reach;
 const auto add=[&](const evr::motion::LiveTarget& t,int32_t state){
  if(n>=maximum)return;const float d=evr::motion::bladeDistance(weapon.bladeFrom,weapon.bladeTo,t,reach);if(!std::isfinite(d)||d>radius)return;
  out[n]={t.entity,d,t.character?.5f:t.object?.4f:.55f,t.character?reach.characterStrike:t.object?reach.objectRadius:reach.otherRadius,state,t.character};++n;};
 for(const auto& c:cache)add(c.target,stateLocked(c));
 if(quickTick&&now>=quickTick&&now-quickTick<=1000)for(size_t k=0;k<quickCount;++k){
  const auto& q=quick[k];if(std::any_of(cache.begin(),cache.end(),[&](const Cached& c){return c.target.entity==q.entity;}))continue;
  evr::motion::LiveTarget t;t.entity=q.entity;t.position=q.position;t.character=true;add(t,-1);}
 return n;
}
// The state of a kept thing: its health, -1 kept without a readable health (a pot, a tree not yet struck), -2 not kept
// (gone: broken, or its components changed - a tree gets its Health when first struck).
inline int32_t stateLocked(const Cached& c){
 const auto l=layout(c.layout);
 // A guest's World (online): the replicated keen::ecs::NetworkHealth (health, healthMax) where the server's Health is
 // missing (as the list takes it in tick() above) - so that a guest's hit stop sees a health drop too (2026-10-20 18:10,
 // user: "make it work in an online session, on the guest's side as well").
 if(!l.health&&l.networkHealth){uint32_t value[2]{};if(!block(c.data+l.networkHealthOffset+uintptr_t(c.row)*8,value,sizeof value)||!value[1]||value[0]>value[1])return -1;
  return int32_t(std::min<uint32_t>(value[0],0x7fffffff));}
 if(!l.health)return -1;   // (healthNow above: not the kept layouts alone)
 const auto bytes=types.healthBytes;if(bytes!=68&&bytes!=40)return -1;
 std::array<uint8_t,256> h{};if(!block(c.data+l.healthOffset+uintptr_t(c.row)*bytes,h.data(),bytes))return -1;
 int32_t health=-1;
 if(bytes==68){uint32_t offset=0;std::memcpy(&offset,h.data()+8,4);if(!(h[16]&1)||offset!=36)return -1;std::memcpy(&health,h.data()+offset,4);}
 else{if(!h[4]||h[4]>8)return -1;std::memcpy(&health,h.data()+8,4);}
 return std::max(health,0);
}
/// The health a guest sees drops at the touch (2026-10-21, user: "the guest keeps it lowered; the host processes it later
// - what matters most is that everyone plays with the feel of solo"). On a guest's World the enemy's health is the
// replicated keen::ecs::NetworkHealth {health, healthMax}, which the host's server writes when its hit has been made
// (the session of 10-21 00:25: about half a second after the press). At the blade's touch of a creature (the hit stop's
// watch, controller_ui_fixture.hpp) the guest's copy is lowered by what the stroke takes.
// 2026-10-22 (the online session of 10-22 00:10, guest log world-2-vr: every predicted value was put back 15..32 ms
// later by the host's copy from before the hit - "host" equal to "before" - and a weapon's first stroke lowered nothing):
// - Kept: until the host's value drops below what it was at the touch, the predicted value is written again each frame,
//   for 0.7 s at most (user: "the host's answer comes 0.3 s later - keep it 0.7 s on the guest"); without a drop by then
//   it goes back (the host's stroke missed).
// - Computed, from the first stroke (user 10-22: "learning is a breeding ground of exceptions and a bad feel - compute it
//   from the weapon and my own state"): include/evr/damage_calc.hpp with the weapon's item (the registry: its damage
//   distribution +0x684, weapon category +0xd0, perks +0x658), its instance (keen::ecs::ItemState +0xc baseDamageUi,
//   PerkContainerNew +1 unlockedPerks - both on a guest's World too), the own keen::ecs::NetworkPlayerAttributes (strength,
//   damage modifiers, crit), the running attack's modifier (the action sequence -> include/evr/damage_tables.hpp attacks)
//   and the creature's susceptibility by its render model (damage_tables.hpp enemies). The crit is its expected value.
//   No stroke without an estimate is predicted (the host's value comes as before). On a host or in a game of one the
//   estimate is only told against the health the stroke really took (damage_estimate_check), to see it hold.
// - Down to 0 (user 10-22: "kill it on the guest too - a rare revival is better than every enemy dying late"): a creature
//   whose host value has not followed in 0.7 s comes back.
// Strokes on one creature in a row each take their own damage; each drop of the host's value settles the oldest.
// EVR_GUEST_PREDICT_HEALTH=0: off.
struct Pending {uint64_t ms=0;float damage=0;uint32_t item=0,sequence=0;float mod=1;};
// lastStale: when the host's copy from before the strokes came last (it overwrote the prediction). 2026-10-22 review W1: a
// host value equal to the prediction - every kill, 0 and 0 - cannot be told by its change; when the old copy has stopped
// coming for 0.2 s by the end of the 0.7 s, the prediction is taken as the host's answer and stays.
struct Predicted {uint32_t entity=0,host=0,written=0,max=0;std::array<Pending,4> strokes{};size_t count=0;uint64_t ms=0,lastStale=0;bool open=false;};
inline std::array<Predicted,8> predicted{};inline unsigned predictLogs=0;
constexpr uint64_t predictHoldMs=700;
inline const bool predictWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUEST_PREDICT_HEALTH",v,sizeof v);return !(n==1&&v[0]=='0');}();
// The types of a World this estimate reads (the client's World for the render model on a host). Under mutex.
struct EstimateTypes {uintptr_t world=0,registry=0;int selection=-1,inventory=-1,itemState=-1,perks=-1,player=-1,model=-1;unsigned playerBytes=0,modelBytes=0;uint64_t used=0;};
inline std::array<EstimateTypes,2> estimateTypes{};inline uint64_t estimateTypesUse=0;
inline const EstimateTypes& estimateTypesOf(uintptr_t world){
 // the World's own slot, else the one used longest ago (2026-10-22 review W2: the server's and the client's World took one
 // slot in turn after a World change, and each stroke read every type name again)
 size_t at=estimateTypes[0].world==world?0:estimateTypes[1].world==world?1:(estimateTypes[0].used<=estimateTypes[1].used?0:1);
 auto& t=estimateTypes[at];t.used=++estimateTypesUse;
 const auto registry=read<uintptr_t>(world);if(t.world==world&&t.registry==registry)return t;
 t={};t.world=world;t.registry=registry;t.used=estimateTypesUse;
 for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){nativeImpactComponents::ComponentType c;try{c=nativeImpactComponents::componentType(world,i);}catch(const std::exception&){break;}
  const auto& n=c.qualifiedName;
  if(n=="keen::ecs::SlotSelection"&&c.bytes==216)t.selection=int(i);else if(n=="keen::ecs::Inventory"&&c.bytes==96)t.inventory=int(i);
  else if(n=="keen::ecs::ItemState"&&c.bytes==20)t.itemState=int(i);else if(n=="keen::ecs::PerkContainerNew"&&c.bytes==2)t.perks=int(i);
  else if(n=="keen::ecs::NetworkPlayerAttributes"&&c.bytes>=240&&c.bytes<=0x400){t.player=int(i);t.playerBytes=c.bytes;}
  else if(n=="keen::ecs::ModelComponent"&&c.bytes>=4&&c.bytes<=64){t.model=int(i);t.modelBytes=c.bytes;}}
 return t;
}
struct Estimate {bool valid=false;int32_t loss=0;std::string why="";evr::damage::HitInputs in{};uint32_t item=0,sequence=0,instance=0;bool attackKnown=false,enemyKnown=false,categoryKnown=false;uint8_t ambiguity=0;unsigned perksActive=0;};
inline std::string estimateJson(const Estimate& e){
 std::ostringstream o;o<<std::setprecision(4)<<"\"valid\":"<<(e.valid?"true":"false")<<",\"loss\":"<<e.loss<<",\"why\":"<<jsonString(e.why)<<",\"item\":"<<e.item<<",\"instance\":"<<e.instance
  <<",\"item_damage\":"<<e.in.itemDamage<<",\"distribution\":["<<e.in.distribution[0]<<','<<e.in.distribution[1]<<','<<e.in.distribution[2]<<','<<e.in.distribution[3]<<','<<e.in.distribution[4]<<','<<e.in.distribution[5]<<','<<e.in.distribution[6]<<','<<e.in.distribution[8]
  <<"],\"perks\":"<<e.perksActive<<",\"strength\":"<<e.in.strength<<",\"category\":"<<e.in.categoryMod<<(e.categoryKnown?"":",\"category_unknown\":true")<<",\"attack\":"<<e.in.attackMod<<",\"sequence\":"<<e.sequence<<(e.attackKnown?"":",\"attack_unknown\":true")
  <<",\"damage_mod\":"<<e.in.damageMod<<",\"melee_mod\":"<<e.in.meleeDamageMod<<",\"against\":"<<e.in.against<<",\"crit\":["<<e.in.critChance<<','<<e.in.critDamageMod<<"],\"susceptibility\":["<<e.in.susceptibility[0]<<','<<e.in.susceptibility[1]<<','<<e.in.susceptibility[2]<<','<<e.in.susceptibility[3]<<"]"
  <<(e.enemyKnown?"":",\"enemy_unknown\":true")<<",\"ambiguity\":"<<unsigned(e.ambiguity);
 return o.str();
}
// The guest's weapon instance (2026-10-22, the friend's server, guest log 20261022-123336: every estimate stopped at
// "weapon instance not found" - the slot's stack holds the sword but its instance is 0 in the guest's World). Three times
// at most (the guest World seen, an estimate without the instance): the SlotSelection, each slot row's Inventory (96 bytes)
// and every ItemState of this World with its entity (weapon_probe), to find where the guest has the weapon's own values.
inline unsigned weaponProbes=0;
inline void weaponProbe(uintptr_t world,uint32_t local,const char* why){
 if(weaponProbes>=3||!world||!local)return;++weaponProbes;
 try{
  const auto& t=estimateTypesOf(world);
  std::ostringstream o;o<<"\"why\":"<<jsonString(why)<<",\"world\":"<<hex(world)<<",\"local\":"<<local<<",\"types\":["<<t.selection<<','<<t.inventory<<','<<t.itemState<<']';
  if(t.selection>=0&&t.inventory>=0){
   const auto player=nativeImpactComponents::entityAtWorld(world,local);
   const auto sel=nativeImpactComponents::component(player,unsigned(t.selection),216);
   if(sel){const auto raw=read<std::array<uint8_t,216>>(sel);o<<",\"selection\":"<<jsonString(nativeImpactComponents::asHex(raw));
    std::vector<uint32_t> rows;for(unsigned slot=0;slot<24;++slot){uint32_t row=0;std::memcpy(&row,raw.data()+8*slot,4);if(row)rows.push_back(row);}
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    o<<",\"rows\":[";bool first=true;
    for(const auto r:rows){o<<(first?"":",")<<"{\"row\":"<<r;first=false;
     try{const auto e=nativeImpactComponents::entityAtWorld(world,r);const auto inv=nativeImpactComponents::component(e,unsigned(t.inventory),96);
      if(inv)o<<",\"inventory\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,96>>(inv)));}catch(...){o<<",\"unreadable\":true";}
     o<<'}';}
    o<<']';}}
  if(t.itemState>=0){
   const auto map=world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keyArray=read<uintptr_t>(map+0x28),values=read<uintptr_t>(map+0x40);
   unsigned found=0;o<<",\"item_states\":[";
   if(bitmap&&keyArray&&values&&capacity&&capacity<=0x100000&&!(capacity&(capacity-1))){
    std::vector<uint64_t> bits(static_cast<size_t>((capacity+63)/64));std::vector<uint32_t> keys(static_cast<size_t>(capacity));SIZE_T got=0;
    if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(bitmap),bits.data(),bits.size()*8,&got)&&got==bits.size()*8&&
       ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(keyArray),keys.data(),keys.size()*4,&got)&&got==keys.size()*4)
     for(uint64_t i=0;i<capacity;++i){if(!(bits[size_t(i/64)]>>(i%64)&1))continue;
      try{nativeImpactComponents::Entity e;e.world=world;e.record=read<uintptr_t>(values+i*8);if(!e.record)continue;e.layout=read<uintptr_t>(e.record+0x18);e.data=read<uintptr_t>(e.record+0x20);e.row=read<uint32_t>(e.record+0x30);
       if(!e.layout||!e.data)continue;const auto b=read<std::array<uint64_t,10>>(e.layout);const auto id=unsigned(t.itemState);if(!(b[id/64]>>(id%64)&1))continue;
       const auto c=nativeImpactComponents::component(e,id,20);if(!c)continue;
       if(found<60)o<<(found?",":"")<<"{\"entity\":"<<keys[size_t(i)]<<",\"bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,20>>(c)))<<'}';++found;}catch(...){}}}
   o<<"],\"item_states_found\":"<<found;}
  logFile->event("weapon_probe",o.str());
 }catch(const std::exception& e){logFile->event("weapon_probe_failed","\"reason\":"+jsonString(e.what()));}
}
inline uint32_t (*runningSequence)()=nullptr;   // the local player's running action (actionSequenceObserver), set by view_resource_probe.cpp
// Under mutex. `world`: the World of the local player (the server's in a game of one); `client`: the World with the render models.
inline Estimate estimateStroke(uintptr_t world,uintptr_t client,uint32_t local,uint32_t entity,uint64_t binding){
 Estimate e;e.item=uint32_t(binding>>32);if(!world||!local||!entity||!e.item){e.why="no world";return e;}
 try{
  const auto& t=estimateTypesOf(world);
  // the weapon's item in the registry
  const auto registry=attachmentObserver::itemRegistry.load();if(!registry){e.why="no item registry";return e;}
  auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,e.item);
  if(found.status!=game::ItemLookupStatus::Found||!found.pointer){e.why="item not found";return e;}
  std::array<float,9> dist{};std::array<uint8_t,16> category{};std::array<uint32_t,5> perkIds{};
  if(!inputRanged::readItemBytes(found.pointer+0x684,dist.data(),sizeof dist)||!inputRanged::readItemBytes(found.pointer+0xd0,category.data(),16)||
     !inputRanged::readItemBytes(found.pointer+0x658,perkIds.data(),sizeof perkIds)){e.why="item unreadable";return e;}
  for(auto& d:dist)if(!std::isfinite(d)||d<0||d>4)d=0;
  // the instance worn: the selected slot of the player's SlotSelection, or any slot holding the item
  if(t.selection<0||t.inventory<0||t.itemState<0){e.why="no item types";return e;}
  const auto player=nativeImpactComponents::entityAtWorld(world,local);
  const auto sel=nativeImpactComponents::component(player,unsigned(t.selection),216);if(!sel){e.why="no slot selection";return e;}
  const auto raw=read<std::array<uint8_t,216>>(sel);
  const auto stackOf=[&](unsigned slot,uint32_t& item,uint32_t& instance){uint32_t row=0;std::memcpy(&row,raw.data()+8*slot,4);const uint8_t index=raw[8*slot+4];item=instance=0;
   if(!row||index>=8)return false;try{const auto r=nativeImpactComponents::entityAtWorld(world,row);const auto inv=nativeImpactComponents::component(r,unsigned(t.inventory),96);if(!inv)return false;
    const auto s=read<std::array<uint32_t,3>>(inv+12*index);item=s[0];instance=s[2];return true;}catch(...){return false;}};
  {uint32_t item=0,instance=0;const unsigned selected=raw[0xc0];if(selected<20&&stackOf(selected,item,instance)&&item==e.item)e.instance=instance;
   for(unsigned slot=0;slot<20&&!e.instance;++slot)if(stackOf(slot,item,instance)&&item==e.item&&instance)e.instance=instance;
   // A guest (2026-10-22, weapon_probe in game-20261022-130133 on the local dedicated server): no slot points at the sword's
   // stack - it lies in the action bar's row (5673, the third of its 8 stacks, instance 5661, ItemState {row, index, item,
   // baseDamage 37}). Every stack of every row the slots name is looked through for the item.
   if(!e.instance){std::vector<uint32_t> rows;for(unsigned slot=0;slot<24;++slot){uint32_t row=0;std::memcpy(&row,raw.data()+8*slot,4);if(row)rows.push_back(row);}
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    for(const auto r:rows){if(e.instance)break;try{const auto re=nativeImpactComponents::entityAtWorld(world,r);const auto inv=nativeImpactComponents::component(re,unsigned(t.inventory),96);if(!inv)continue;
      const auto stacks=read<std::array<uint32_t,24>>(inv);for(unsigned k=0;k<8;++k)if(stacks[3*k]==e.item&&stacks[3*k+2]){e.instance=stacks[3*k+2];break;}}catch(...){}}}}
  if(!e.instance){weaponProbe(world,local,"estimate");e.why="weapon instance not found";return e;}
  const auto inst=nativeImpactComponents::entityAtWorld(world,e.instance);
  const auto state=nativeImpactComponents::component(inst,unsigned(t.itemState),20);if(!state){e.why="no item state";return e;}
  const auto baseDamage=read<uint16_t>(state+0xc);if(!baseDamage){e.why="no base damage";return e;}
  e.in.itemDamage=float(baseDamage);
  if(t.perks>=0){const auto p=nativeImpactComponents::component(inst,unsigned(t.perks),2);if(p){e.perksActive=std::min<unsigned>(read<uint8_t>(p+1),5);
   for(unsigned k=0;k<e.perksActive;++k)if(const auto* perk=evr::damage::perk(perkIds[k]))dist[perk->type]+=perk->add;}}
  e.in.distribution=dist;
  // the own attributes
  if(t.player<0){e.why="no player attributes";return e;}
  const auto npa=nativeImpactComponents::component(player,unsigned(t.player),t.playerBytes);if(!npa){e.why="no player attributes";return e;}
  std::array<uint8_t,256> a{};if(!block(npa,a.data(),std::min<size_t>(t.playerBytes,a.size()))){e.why="attributes unreadable";return e;}
  const auto u16=[&](unsigned off){uint16_t v=0;std::memcpy(&v,a.data()+off,2);return float(v)/100.f;};
  const auto f32=[&](unsigned off){float v=0;std::memcpy(&v,a.data()+off,4);return std::isfinite(v)?v:0.f;};
  e.in.strength=float(a[evr::damage::npa_strength]);e.in.damageMod=u16(evr::damage::npa_damageMod);e.in.meleeDamageMod=u16(evr::damage::npa_meleeDamageMod);
  e.in.meleeTypeMod={u16(evr::damage::npa_meleeCut),u16(evr::damage::npa_meleePierce),u16(evr::damage::npa_meleeBlunt)};
  e.in.critChance=f32(evr::damage::npa_critChance)+f32(evr::damage::npa_critChanceMelee);e.in.critDamageMod=u16(evr::damage::npa_critDamageMod);
  {const int field=evr::damage::categoryField(category);e.categoryKnown=field>=0;e.in.categoryMod=field>=0?u16(unsigned(field)):1.f;}
  // the running attack
  e.sequence=runningSequence?runningSequence():0;e.attackKnown=e.sequence&&evr::damage::attackKnown(e.sequence);e.in.attackMod=evr::damage::attackMod(e.sequence);
  // the creature by its render model (the client's World)
  e.in.against=1;
  if(client){const auto& ct=estimateTypesOf(client);
   if(ct.model>=0&&motionVisual::scene)try{const auto ce=nativeImpactComponents::entityAtWorld(client,entity);const auto mc=nativeImpactComponents::component(ce,unsigned(ct.model),ct.modelBytes);
    const auto handle=mc?read<uint32_t>(mc):0u;const auto object=handle?motionVisual::currentObject(handle):0;const auto res=object?read<uintptr_t>(object+layout::modelResource):0;
    if(res){const auto guid=read<std::array<uint32_t,4>>(res+0x18);
     if(const auto* en=evr::damage::enemy(guid)){e.enemyKnown=true;e.ambiguity=en->ambiguity;e.in.susceptibility=en->susceptibility;
      if(en->againstClass>=0)e.in.against+=u16(unsigned(en->againstClass))-1.f;
      if(en->flying)e.in.against+=u16(evr::damage::npa_againstFlying)-1.f;
      if(en->againstFaction>=0)e.in.against+=u16(unsigned(en->againstFaction))-1.f;}}}catch(...){}}
  e.in.melee=true;e.in.crit=-1;
  e.loss=evr::damage::hitLoss(e.in).loss;e.valid=e.loss>0;if(!e.valid)e.why="no damage";
 }catch(const std::exception& x){e.why=x.what();}
 return e;
}
// The own strokes' estimates on a host or in a game of one, told against the health they took (learnHit).
struct HostEstimate {uint32_t entity=0;uint64_t ms=0;Estimate estimate;};
inline std::array<HostEstimate,8> hostEstimates{};inline unsigned estimateLogs=0;
inline uintptr_t clientWorldNow(){rootCamera::Sample root;try{return rootCamera::trySnapshot(root)?root.world:0;}catch(...){return 0;}}   // (review S2: no exception out)
// A host's or a lone player's own stroke took this much from a creature (the hit stop's watch, controller_ui_fixture.hpp).
inline void learnHit(uint32_t entity,uint64_t binding,int32_t before,int32_t after){
 std::lock_guard<std::mutex> lock(mutex);if(!predictWanted||guestWorld||before<=after||after<0)return;(void)binding;
 for(auto& h:hostEstimates)if(h.entity==entity&&h.ms&&GetTickCount64()-h.ms<=1500){
  const uint32_t seqNow=runningSequence?runningSequence():0;
  if(estimateLogs++<400)logFile->event("damage_estimate_check","\"entity\":"+std::to_string(entity)+",\"actual\":"+std::to_string(before-after)+",\"estimated\":"+std::to_string(h.estimate.loss)+
   ",\"before\":"+std::to_string(before)+",\"after_ms\":"+std::to_string(GetTickCount64()-h.ms)+",\"sequence_at_drop\":"+std::to_string(seqNow)+",\"attack_at_drop\":"+std::to_string(evr::damage::attackMod(seqNow))+","+estimateJson(h.estimate));
  h.entity=0;return;}
}
// A diagnostic (EVR_DAMAGE_ESTIMATE_PROBE=1): every 5 s, the estimate of a stroke of the selected item at the nearest
// creature kept, without any stroke - whether every read of estimateStroke works in this game (damage_estimate_probe).
inline const bool estimateProbeWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_DAMAGE_ESTIMATE_PROBE",v,sizeof v);return n==1&&v[0]=='1';}();
inline uint64_t estimateProbeAt=0;
inline void estimateProbe(uintptr_t world,uint32_t local,uint64_t now){   // under mutex, on the World's tick
 if(!estimateProbeWanted||now-estimateProbeAt<5000)return;estimateProbeAt=now;
 const Cached* nearest=nullptr;double best=1e9;for(const auto& c:cache)if(c.target.character){const double m=metres(c.target.position,lastOrigin);if(m<best){best=m;nearest=&c;}}
 uint32_t item=0;
 try{const auto& t=estimateTypesOf(world);const auto player=nativeImpactComponents::entityAtWorld(world,local);
  const auto sel=t.selection>=0?nativeImpactComponents::component(player,unsigned(t.selection),216):0;
  // the first worn item whose instance has a damage (the selected one may be no weapon in a test)
  if(sel){const auto raw=read<std::array<uint8_t,216>>(sel);
   for(unsigned slot=0;slot<20&&!item;++slot){uint32_t row=0;std::memcpy(&row,raw.data()+8*slot,4);const uint8_t index=raw[8*slot+4];if(!row||index>=8||t.inventory<0||t.itemState<0)continue;
    try{const auto r=nativeImpactComponents::entityAtWorld(world,row);const auto inv=nativeImpactComponents::component(r,unsigned(t.inventory),96);if(!inv)continue;
     const auto stack=read<std::array<uint32_t,3>>(inv+12*index);if(!stack[2])continue;const auto ie=nativeImpactComponents::entityAtWorld(world,stack[2]);
     const auto st=nativeImpactComponents::component(ie,unsigned(t.itemState),20);if(st&&read<uint16_t>(st+0xc))item=stack[0];}catch(...){}}}}catch(...){}
 const auto client=guestWorld?world:clientWorldNow();
 const auto e=estimateStroke(world,client,local,nearest?nearest->target.entity:0,uint64_t(item)<<32);
 if(estimateLogs++<400)logFile->event("damage_estimate_probe","\"creature\":"+std::to_string(nearest?nearest->target.entity:0)+",\"metres\":"+std::to_string(nearest?best:-1.)+",\"guest\":"+(guestWorld?"true":"false")+","+estimateJson(e));
}
inline uintptr_t networkHealthAt(uint32_t entity){   // under mutex
 if(!types.world||types.networkHealth<0)return 0;
 try{return nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(types.world,entity),unsigned(types.networkHealth),8);}catch(...){return 0;}
}
inline void predictHit(uint32_t entity,uint64_t key,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);if(!predictWanted||!entity||!types.world||!lastOriginKnown)return;
 const uint32_t local=estimateLocal;
 if(!guestWorld){   // a host or a game of one: the estimate only, told against the health the stroke takes
  const auto e=estimateStroke(types.world,clientWorldNow(),local,entity,key);
  HostEstimate* slot=&hostEstimates[0];for(auto& h:hostEstimates)if(!h.entity){slot=&h;break;}else if(h.ms<slot->ms)slot=&h;*slot={entity,now,e};
  if(!e.valid&&estimateLogs++<400)logFile->event("damage_estimate_failed","\"entity\":"+std::to_string(entity)+","+estimateJson(e));
  return;}
 const auto at=networkHealthAt(entity);uint32_t value[2]{};if(!at||!block(at,value,sizeof value)||!value[1]||!value[0]||value[0]>value[1])return;
 const auto e=estimateStroke(types.world,types.world,local,entity,key);
 if(!e.valid){if(predictLogs++<300)logFile->event("guest_health_not_predicted","\"entity\":"+std::to_string(entity)+","+estimateJson(e));return;}
 const float damage=float(e.loss);const uint32_t item=e.item;
 Predicted* slot=nullptr;for(auto& p:predicted)if(p.open&&p.entity==entity){slot=&p;break;}
 if(!slot){for(auto& p:predicted)if(!p.open&&(!slot||p.ms<slot->ms))slot=&p;if(!slot){slot=&predicted[0];for(auto& p:predicted)if(p.ms<slot->ms)slot=&p;}
  *slot={};slot->entity=entity;slot->host=value[0];slot->written=value[0];slot->open=true;}
 if(slot->count>=slot->strokes.size()){std::move(slot->strokes.begin()+1,slot->strokes.end(),slot->strokes.begin());--slot->count;}
 slot->strokes[slot->count++]={now,damage,item,e.sequence,e.in.attackMod};slot->ms=now;slot->max=value[1];
 float owed=0;for(size_t k=0;k<slot->count;++k)owed+=slot->strokes[k].damage;
 const uint32_t shown=uint32_t(std::max<int64_t>(0,int64_t(slot->host)-int64_t(owed+.5f)));SIZE_T n=0;
 if(shown!=value[0]&&WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&shown,4,&n))slot->written=shown;
 if(predictLogs++<300)logFile->event("guest_health_predicted","\"entity\":"+std::to_string(entity)+",\"before\":"+std::to_string(value[0])+",\"written\":"+std::to_string(slot->written)+
  ",\"max\":"+std::to_string(value[1])+",\"strokes\":"+std::to_string(slot->count)+","+estimateJson(e));
}
// Each frame: the host's value of a creature with strokes still owed. A drop settles the oldest stroke; the host's copy
// from before is overwritten again; a stroke not followed by a drop in 0.7 s is given up.
inline void settlePredictions(uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);
 for(auto& p:predicted){if(!p.open)continue;
  const auto at=networkHealthAt(p.entity);uint32_t value[2]{};
  if(!at||!block(at,value,sizeof value)||!value[1]){p.open=false;continue;}
  if(value[0]!=p.written){
   if(value[0]<p.host&&p.count){   // the host's hit came
    const float drop=float(p.host-value[0]);
    if(predictLogs++<300)logFile->event("guest_health_host","\"entity\":"+std::to_string(p.entity)+",\"host_before\":"+std::to_string(p.host)+",\"shown\":"+std::to_string(p.written)+
     ",\"host\":"+std::to_string(value[0])+",\"host_drop\":"+std::to_string(drop)+",\"predicted_damage\":"+std::to_string(p.strokes[0].damage)+",\"after_ms\":"+std::to_string(now-p.strokes[0].ms)+",\"strokes\":"+std::to_string(p.count));
    // the strokes this drop covers: the oldest, and the next ones while the drop holds them too
    float left=drop;size_t settled=0;while(settled<p.count&&(settled==0||left>=p.strokes[settled].damage*.6f)){left-=p.strokes[settled].damage;++settled;}
    std::move(p.strokes.begin()+settled,p.strokes.begin()+p.count,p.strokes.begin());p.count-=settled;}
   else if(value[0]>=p.host)p.lastStale=now;   // the host's copy from before (or a heal)
   p.host=value[0];}
  // the stroke's own action, when it starts after the touch (review W4: the touch comes before it): its attack modifier
  {const uint32_t seq=runningSequence?runningSequence():0;
   if(seq&&evr::damage::attackKnown(seq))for(size_t k=0;k<p.count;++k){auto& s=p.strokes[k];
    if(now-s.ms<=300&&s.sequence!=seq&&s.mod>0){const float mod=evr::damage::attackMod(seq);
     if(predictLogs++<300)logFile->event("guest_health_attack_changed","\"entity\":"+std::to_string(p.entity)+",\"sequence\":"+std::to_string(seq)+",\"mod\":"+std::to_string(s.mod)+",\"to\":"+std::to_string(mod)+",\"damage\":"+std::to_string(s.damage));
     s.damage*=mod/s.mod;s.mod=mod;s.sequence=seq;}}}
  // strokes the host did not follow
  if(p.count&&now-p.strokes[0].ms>predictHoldMs&&value[0]==p.written&&(!p.lastStale||now-p.lastStale>=200)){
   if(predictLogs++<300)logFile->event("guest_health_kept","\"entity\":"+std::to_string(p.entity)+",\"shown\":"+std::to_string(p.written)+",\"stale_ms_ago\":"+std::to_string(p.lastStale?now-p.lastStale:0)+",\"strokes\":"+std::to_string(p.count));
   p.count=0;p.host=value[0];}
  while(p.count&&now-p.strokes[0].ms>predictHoldMs){
   if(predictLogs++<300)logFile->event("guest_health_unconfirmed","\"entity\":"+std::to_string(p.entity)+",\"host\":"+std::to_string(p.host)+",\"predicted_damage\":"+std::to_string(p.strokes[0].damage)+",\"revived\":"+(p.written==0&&p.host>0?"true":"false"));
   std::move(p.strokes.begin()+1,p.strokes.begin()+p.count,p.strokes.begin());--p.count;}
  if(!p.count){p.open=false;if(value[0]!=p.host){SIZE_T n=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&p.host,4,&n);}continue;}
  float owed=0;for(size_t k=0;k<p.count;++k)owed+=p.strokes[k].damage;
  const uint32_t shown=uint32_t(std::max<int64_t>(0,int64_t(p.host)-int64_t(owed+.5f)));
  if(shown!=value[0]){SIZE_T n=0;if(WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&shown,4,&n))p.written=shown;}else p.written=shown;}
}
// What became of the things a guest's blade touched (2026-10-22, a diagnostic for the guest's own break of pots: a guest's
// World does not tell a pot from a wall - both are a StaticClientCollider with a CollisionFeedbackMaterialComponent): the
// thing's components and its render model (the Scene handle in ModelComponent, the model resource's GUID +0x18 and number)
// at the touch, and 1.5 s later whether it is still in the World (the host broke it, or not). guest_thing_outcome; 300
// records at the most. Under mutex.
// The guest's own break (user 2026-10-22: "a pot breaks for certain - break it on the guest; the host's word comes 0.3 s
// later, so keep it 0.7 s and nothing differs to the eye"): a touched thing whose model is a breakable one
// (include/evr/breakable_models.hpp, from the game data: docs/analysis/breakable_props_20261022) is taken out of the eyes
// at once (head_visibility_bridge.hpp) until the host's break takes it out of the World, for 0.7 s at the most - then it
// shows again (the host did not break it). The host's shards and sound come with its break. Only when one stroke is sure
// to break it: the model's verdict 0 or 1 (2 only without ClientInteractionOffer / PickupItem - a pickup shares the
// model), a material of wood, stone or metal, and the weapon's damage of that material - the item's u32 at +0x6a8 / +0x6ac
// / +0x6b0 (its damage distribution +0x684, the woodDamage tenth; docs/analysis/damage_formula_20261022 1.2) - not
// below the model's largest maxHealth (a guess of the data study: a stroke takes the weapon's value of the material;
// guest_break_judge tells each touch). EVR_GUEST_BREAK=0: off.
struct ModelInfo {uint32_t handle=0;std::array<uint32_t,4> guid{};uint16_t number=0;bool valid=false;};
struct ThingTouch {uint32_t entity=0;uint64_t ms=0;std::string components;ModelInfo model;};
inline std::array<ThingTouch,8> thingTouches{};inline unsigned thingLogs=0;
struct HiddenThing {uint32_t entity=0,handle=0;uint64_t ms=0;};
inline std::array<HiddenThing,4> hiddenThings{};inline unsigned breakLogs=0;
constexpr uint64_t guestBreakHoldMs=700;
inline const bool guestBreakWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUEST_BREAK",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline ModelInfo modelOf(const nativeImpactComponents::Entity& e){   // under mutex
 ModelInfo m;if(types.model<0||types.modelBytes<4||!motionVisual::scene)return m;
 try{const auto c=nativeImpactComponents::component(e,unsigned(types.model),types.modelBytes);if(!c)return m;m.handle=read<uint32_t>(c);
  const auto object=m.handle?motionVisual::currentObject(m.handle):0;if(!object)return m;const auto resource=read<uintptr_t>(object+layout::modelResource);if(!resource)return m;
  m.guid=read<std::array<uint32_t,4>>(resource+0x18);m.number=read<uint16_t>(resource+layout::resourceModel);m.valid=true;}catch(...){}
 return m;
}
inline std::string modelJson(const ModelInfo& m){
 return "{\"handle\":"+std::to_string(m.handle)+",\"valid\":"+(m.valid?"true":"false")+",\"number\":"+std::to_string(m.number)+",\"guid\":["+std::to_string(m.guid[0])+","+std::to_string(m.guid[1])+","+std::to_string(m.guid[2])+","+std::to_string(m.guid[3])+"]}";
}
// The weapon's damage of a material (1 wood, 2 stone, 3 metal) from its item in the registry; -1 unknown.
inline int64_t materialDamage(uint32_t item,unsigned material){
 const auto registry=attachmentObserver::itemRegistry.load();if(!item||!registry||material<1||material>3)return -1;
 try{auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,item);if(found.status!=game::ItemLookupStatus::Found||!found.pointer)return -1;
  uint32_t v=0;if(!inputRanged::readItemBytes(found.pointer+0x6a8+4*(material-1),&v,4))return -1;return v<100000?int64_t(v):-1;}catch(...){return -1;}
}
inline void thingTouched(uint32_t entity,uint64_t binding,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);if(!guestWorld||!entity||!types.world)return;
 for(const auto& t:thingTouches)if(t.entity==entity)return;
 nativeImpactComponents::Entity e;std::array<uint64_t,10> b{};
 try{e=nativeImpactComponents::entityAtWorld(types.world,entity);b=read<std::array<uint64_t,10>>(e.layout);}catch(...){return;}
 const auto model=modelOf(e);
 // the guest's own break
 if(guestBreakWanted&&model.valid){
  if(const auto* known=evr::breakable::find(model.guid)){
   const auto hasComponent=[&](const char* name){for(unsigned t=0;t<640&&t<nativeImpactComponents::componentCapacity;++t)if((b[t/64]>>(t%64))&1){
     try{if(nativeImpactComponents::componentType(types.world,t).qualifiedName==name)return true;}catch(...){}}return false;};
   const bool pickup=known->verdict==2&&(hasComponent("keen::ecs::ClientInteractionOffer")||hasComponent("keen::ecs::PickupItem"));
   const auto damage=materialDamage(uint32_t(binding>>32),known->material);
   const bool breaks=known->verdict<=2&&!pickup&&known->material>=1&&known->material<=3&&damage>=int64_t(known->healthMax);
   if(breaks){HiddenThing* slot=&hiddenThings[0];for(auto& h:hiddenThings)if(!h.entity){slot=&h;break;}else if(h.ms<slot->ms)slot=&h;*slot={entity,model.handle,now};}
   if(breakLogs++<300)logFile->event("guest_break_judge","\"entity\":"+std::to_string(entity)+",\"model\":"+modelJson(model)+",\"health_max\":"+std::to_string(known->healthMax)+
    ",\"material\":"+std::to_string(known->material)+",\"verdict\":"+std::to_string(known->verdict)+",\"pickup\":"+(pickup?"true":"false")+
    ",\"item\":"+std::to_string(uint32_t(binding>>32))+",\"weapon_damage\":"+std::to_string(damage)+",\"broken_here\":"+(breaks?"true":"false"));}}
 if(thingLogs>=300)return;
 ThingTouch* slot=&thingTouches[0];for(auto& t:thingTouches)if(!t.entity){slot=&t;break;}else if(t.ms<slot->ms)slot=&t;
 std::string names;int count=0;
 for(unsigned t=0;t<640&&t<nativeImpactComponents::componentCapacity&&count<40;++t)if((b[t/64]>>(t%64))&1){
  try{auto name=nativeImpactComponents::componentType(types.world,t).qualifiedName;if(name.rfind("keen::ecs::",0)==0)name=name.substr(11);names+=(count?",":"")+jsonString(name);++count;}catch(...){}}
 *slot={entity,now,names,model};
}
inline void thingOutcomes(uint64_t now){   // under mutex, on the World's tick
 for(auto& t:thingTouches){if(!t.entity||now-t.ms<1500)continue;
  bool there=true;try{nativeImpactComponents::entityAtWorld(types.world,t.entity);}catch(...){there=false;}
  if(thingLogs++<300)logFile->event("guest_thing_outcome","\"entity\":"+std::to_string(t.entity)+",\"gone_after_1500_ms\":"+(there?"false":"true")+",\"model\":"+modelJson(t.model)+
   ",\"breakable_model\":"+(t.model.valid&&evr::breakable::find(t.model.guid)?"true":"false")+",\"components\":["+t.components+"]");
  t.entity=0;}
 for(auto& h:hiddenThings){if(!h.entity)continue;
  bool there=true;try{nativeImpactComponents::entityAtWorld(types.world,h.entity);}catch(...){there=false;}
  if(!there||now-h.ms>guestBreakHoldMs){
   if(breakLogs++<300)logFile->event(there?"guest_break_undone":"guest_break_confirmed","\"entity\":"+std::to_string(h.entity)+",\"after_ms\":"+std::to_string(now-h.ms));
   h.entity=0;}}
}
// Any thread (the eyes' culling, head_visibility_bridge.hpp): the Scene handles of the things broken on this PC still
// waiting for the host's break.
inline size_t hiddenThingHandles(uint32_t* out,size_t maximum,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);size_t n=0;
 for(const auto& h:hiddenThings)if(h.entity&&now>=h.ms&&now-h.ms<=guestBreakHoldMs&&n<maximum)out[n++]=h.handle;
 return n;
}
inline int32_t stateNow(uint32_t entity){
 std::lock_guard<std::mutex> lock(mutex);
 const auto c=std::find_if(cache.begin(),cache.end(),[&](const Cached& x){return x.target.entity==entity;});
 return c==cache.end()?-2:stateLocked(*c);
}
// Any thread: a fallen player within `within` metres of the local player's origin, seen in the last second and a half.
inline bool downedNear(double within,uint64_t now,uint32_t& who,double& away){
 std::lock_guard<std::mutex> lock(mutex);who=0;away=-1;
 rootCamera::Sample root;if(!rootCamera::trySnapshot(root))return false;
 const std::array<int64_t,3> origin{root.stable.position.x,root.stable.position.y,root.stable.position.z};
 for(const auto& d:downed){if(now<d.seen||now-d.seen>1500)continue;const double m=metres(d.position,origin);
  if(m<=within&&(!who||m<away)){who=d.entity;away=m;}}
 return who!=0;
}
// Any thread: the local player's combat target (slot 0 of its SelectedTargets), if read within the last half second.
inline bool combatTarget(CombatTarget& out,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);
 if(!combat.valid||!combat.entity||now<combat.ms||now-combat.ms>500)return false;
 out=combat;return true;
}
// Any thread: the enemy whose health bar the game shows - the combat target, else the hard-lock candidate - if read within
// the last half second (target_mark.hpp). candidateOnly: true when it is the candidate.
inline bool hudTarget(CombatTarget& out,bool& candidateOnly,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);candidateOnly=false;
 if(combat.valid&&combat.entity&&now>=combat.ms&&now-combat.ms<=500){out=combat;return true;}
 if(candidate.valid&&candidate.entity&&now>=candidate.ms&&now-candidate.ms<=500){out=candidate;candidateOnly=true;return true;}
 if(relevantChoice.valid&&relevantChoice.entity&&now>=relevantChoice.ms&&now-relevantChoice.ms<=500){out=relevantChoice;candidateOnly=true;return true;}   // 19:55
 return false;
}
// Any thread: where a creature among the kept ones stands now (its origin, Q32), if it is kept (2026-10-19: the health
// bar follows its enemy for a while after the game's combat target is lost - target_mark.hpp).
// The creature nearest to the own player within some metres (a guest's parry, online_sync.hpp sendParries).
inline bool creatureNear(float within,std::array<int64_t,3>& out,uint32_t& entity){
 std::lock_guard<std::mutex> lock(mutex);if(!lastOriginKnown)return false;double best=within;bool found=false;
 for(const auto& c:cache){if(!c.target.character)continue;const double d=metres(c.target.position,lastOrigin);if(d<best){best=d;out=c.target.position;entity=c.target.entity;found=true;}}
 return found;
}
// The other players' health on this host, each tick (a guest's parry gives back the damage of the moments before it:
// the host is ahead of the guest by the way there and back, so the blow the guest parried may have landed here already).
// EVR_PARRY_GIVE_BACK=0: not given back.
struct PlayerHealth {uint32_t entity=0;std::array<int64_t,3> at{};std::array<std::pair<uint64_t,int32_t>,32> history{};size_t next=0;};
inline std::array<PlayerHealth,8> playerHealth{};
inline const bool giveBackWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_PARRY_GIVE_BACK",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline uintptr_t healthValueAt(uint32_t entity){   // under mutex: the address of the current health in Health, or 0
 if(!types.world||types.health<0)return 0;const auto bytes=types.healthBytes;if(bytes!=68&&bytes!=40)return 0;
 try{const auto h=nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(types.world,entity),unsigned(types.health),bytes);if(!h)return 0;
  std::array<uint8_t,68> b{};if(!block(h,b.data(),bytes))return 0;
  if(bytes==68){uint32_t offset=0;std::memcpy(&offset,b.data()+8,4);return (b[16]&1)&&offset==36?h+36:0;}
  return b[4]&&b[4]<=8?h+8:0;}catch(...){return 0;}
}
inline void sampleOtherPlayers(uint64_t now){   // under mutex
 if(!giveBackWanted)return;
 for(const auto& [id,at]:otherPlayers){PlayerHealth* slot=nullptr;for(auto& p:playerHealth)if(p.entity==id){slot=&p;break;}
  if(!slot)for(auto& p:playerHealth)if(!p.entity){slot=&p;*slot={};slot->entity=id;break;}
  if(!slot)continue;slot->at=at;const auto v=healthValueAt(id);int32_t value=-1;if(!v||!block(v,&value,4))continue;
  slot->history[slot->next++%slot->history.size()]={now,value};}
}
inline bool undoDamage(const std::array<int64_t,3>& enemy,uint64_t windowMs){
 std::lock_guard<std::mutex> lock(mutex);if(!giveBackWanted)return false;const auto now=GetTickCount64();
 PlayerHealth* who=nullptr;double best=8;
 for(auto& p:playerHealth)if(p.entity){const double d=metres(p.at,enemy);if(d<best){best=d;who=&p;}}
 if(!who)return false;const auto v=healthValueAt(who->entity);int32_t current=-1;if(!v||!block(v,&current,4)||current<=0)return false;
 int32_t highest=current;for(const auto& [ms,value]:who->history)if(ms&&now>=ms&&now-ms<=windowMs)highest=std::max(highest,value);
 const bool back=highest>current;if(back){SIZE_T n=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(v),&highest,4,&n);}
 if(predictLogs++<300)logFile->event("parry_damage_given_back","\"player\":"+std::to_string(who->entity)+",\"health\":"+std::to_string(current)+",\"to\":"+std::to_string(highest)+",\"window_ms\":"+std::to_string(windowMs));
 return back;
}
// A guest's guard (2026-10-22, user: "is the guard invincible then? that is not allowed"): the guest decided the blow was blocked
// (hit_event_intake.hpp guard: the game's guard state, the attacker within 80 degrees of the guard's front, stamina left), and
// here the host's game does what its own block does (applyDamage 0x3d50d0, 0x3d6e42..0x3d702a): no health lost - the health of
// the last windowMs given back - and stamina taken instead, round(10/50 x damage x StaminaDamageMod - Block) at the least 0
// (0xe618d0; the balancing's 10 and 50 at +0x4c / +0x3c). Block and StaminaDamageMod: the guest's own attribute components
// here (the melee, ranged and magic additions of StaminaDamageMod and the attack's own stamina factor are not read: 1). With
// no stamina left the guest's game ends its guard by itself. EVR_GUARD_STAMINA=0: the health only (0.9.8).
inline const bool guardStaminaWanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_GUARD_STAMINA",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline uintptr_t storageValueAt(uint32_t entity,int type,unsigned bytes){   // under mutex: an attribute's value (sint32 storage +8, its count at +4)
 if(!types.world||type<0)return 0;
 try{const auto c=nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(types.world,entity),unsigned(type),bytes);if(!c)return 0;
  std::array<uint8_t,8> head{};if(!block(c,head.data(),head.size())||!head[4]||head[4]>8)return 0;return c+8;}catch(...){return 0;}
}
inline bool guardGiveBack(const std::array<int64_t,3>& guest,uint64_t windowMs){
 std::lock_guard<std::mutex> lock(mutex);if(!giveBackWanted)return false;const auto now=GetTickCount64();
 PlayerHealth* who=nullptr;double best=8;
 for(auto& p:playerHealth)if(p.entity){const double d=metres(p.at,guest);if(d<best){best=d;who=&p;}}
 if(!who)return false;const auto v=healthValueAt(who->entity);int32_t current=-1;if(!v||!block(v,&current,4)||current<=0)return false;
 int32_t highest=current;for(const auto& [ms,value]:who->history)if(ms&&now>=ms&&now-ms<=windowMs)highest=std::max(highest,value);
 const int32_t drop=highest-current;if(drop<=0)return false;
 int32_t blockValue=0;float mod=1;int32_t stamina=-1,after=-1,cost=0;
 if(guardStaminaWanted){
  if(const auto b=storageValueAt(who->entity,types.blockValue,12)){int32_t x=0;if(block(b,&x,4)&&x>=0&&x<100000)blockValue=x;}
  if(types.world&&types.staminaDamageMod>=0)try{const auto c=nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(types.world,who->entity),unsigned(types.staminaDamageMod),24);
   float x=0;if(c&&block(c+8,&x,4)&&std::isfinite(x)&&x>0&&x<10)mod=x;}catch(...){}
  cost=evr::guard::staminaCost(drop,mod,blockValue);
  if(const auto st=storageValueAt(who->entity,types.stamina,40);st&&block(st,&stamina,4)&&stamina>=0){
   after=std::max(0,stamina-cost);if(after!=stamina){SIZE_T n=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(st),&after,4,&n);}}}
 SIZE_T n=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(v),&highest,4,&n);
 if(predictLogs++<300)logFile->event("guard_given_back","\"player\":"+std::to_string(who->entity)+",\"health\":"+std::to_string(current)+",\"to\":"+std::to_string(highest)+",\"window_ms\":"+std::to_string(windowMs)+
  ",\"block\":"+std::to_string(blockValue)+",\"stamina_damage_mod\":"+std::to_string(mod)+",\"stamina_cost\":"+std::to_string(cost)+",\"stamina\":"+std::to_string(stamina)+",\"stamina_after\":"+std::to_string(after));
 return true;
}
// The first kept thing a flying shot's path meets (motion_arrow_visual.hpp, a guest's own arrow flown on its PC): the
// segment from..to against each creature's and thing's upright body (creature 0.5 m, thing 0.4 m); where on the
// segment (0..1), the thing's place and whether it is a creature.
inline bool segmentHit(const std::array<int64_t,3>& from,const std::array<int64_t,3>& to,float& along,std::array<int64_t,3>& at,bool& character,uint32_t& entity){
 std::lock_guard<std::mutex> lock(mutex);const evr::motion::LiveTargetReach reach;float best=2;
 for(const auto& c:cache){const float r=c.target.character?.5f:c.target.object?.4f:.55f;
  const float d=evr::motion::bladeDistance(from,to,c.target,reach);if(!std::isfinite(d)||d>r)continue;
  // where along the path: the nearest approach to the thing's axis, from the path's start
  const double q=1.0/4294967296.0;const Vec3 a{float(double(from[0]-c.target.position[0])*q),float(double(from[1]-c.target.position[1])*q),float(double(from[2]-c.target.position[2])*q)};
  const Vec3 b{float(double(to[0]-c.target.position[0])*q),float(double(to[1]-c.target.position[1])*q),float(double(to[2]-c.target.position[2])*q)};
  const Vec3 u=b-a;const float uu=dot(u,u);const float s=uu>1e-9f?std::clamp(-(a.x*u.x+a.z*u.z)/std::max(u.x*u.x+u.z*u.z,1e-9f),0.f,1.f):0.f;
  if(s<best){best=s;at=c.target.position;character=c.target.character;entity=c.target.entity;}}
 if(best>1)return false;along=best;return true;
}
inline void publishShotHit(const std::array<int64_t,3>& at,bool character,uint32_t entity,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);if(!steerWanted)return;ShotHit* slot=&shotHits[0];for(auto& h:shotHits)if(h.ms<slot->ms)slot=&h;*slot={at,character,false,now,entity};
}
inline void steerArrowsBody(uintptr_t world,uint64_t now);
inline void steerArrows(uintptr_t world,uint64_t now){try{steerArrowsBody(world,now);}catch(...){}}   // (review M5: a failed read ends the arrows' part only, not the tick)
inline void steerArrowsBody(uintptr_t world,uint64_t now){   // under mutex, on the World's tick
 if(!steerWanted||guestWorld||types.velocity<0||types.gravity<0||types.current<0)return;
 const auto spawned=controllerUiFixture::remoteSpawnAt.load();const bool watching=spawned&&now>=spawned&&now-spawned<=1500;
 // the new flying things after another player's projectile was made here
 // the map's ids are kept known while other players are here (every 0.2 s), so that the arrow made after a guest's shot
 // is the new one; in the 0.6 s after such a shot every tick (it goes out of sight before it is seen)
 if((watching&&now-spawned<=600)||(!otherPlayers.empty()&&now-arrowScanAt>=200)){arrowScanAt=now;
  const auto map=world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keyArray=read<uintptr_t>(map+0x28),values=read<uintptr_t>(map+0x40);
  if(bitmap&&keyArray&&values&&capacity&&capacity<=0x100000&&!(capacity&(capacity-1))){
   arrowKeys.resize(size_t(capacity));arrowBits.resize(size_t((capacity+63)/64));
   if(block(bitmap,arrowBits.data(),arrowBits.size()*8)&&block(keyArray,arrowKeys.data(),arrowKeys.size()*4)){
    std::unordered_map<uint32_t,uint8_t> seen;seen.reserve(arrowKnown.size()+64);
    for(uint64_t i=0;i<capacity;++i){if(!(arrowBits[size_t(i/64)]>>(i%64)&1))continue;const auto id=arrowKeys[size_t(i)];seen.emplace(id,uint8_t(1));
     if(!arrowPrimed||arrowKnown.count(id))continue;
     uintptr_t record=0;if(!block(values+i*8,&record,8)||!record)continue;uintptr_t lay=0;if(!block(record+0x18,&lay,8)||!lay)continue;
     std::array<uint64_t,10> b{};if(!block(lay,b.data(),sizeof b))continue;
     const auto bit=[&](int t){return t>=0&&(b[unsigned(t)/64]>>(unsigned(t)%64)&1);};
     if(watching&&now-spawned<=600&&arrowParkedFor!=spawned&&bit(types.velocity)&&bit(types.gravity)&&!bit(types.actor)){
      // out of sight at once - when it is that shot's arrow: near where the guest's release aimed from, the first one
      bool parked=false,aligned=true;double fromRelease=-1,reach=6;float speedSeen=-1;try{const auto e=nativeImpactComponents::entityAtWorld(world,id);
       const auto vc=nativeImpactComponents::component(e,unsigned(types.velocity),12),gc=nativeImpactComponents::component(e,unsigned(types.gravity),20),cc=nativeImpactComponents::component(e,unsigned(types.current),56);
       if(cc){const auto at=read<WorldPositionQ32>(cc);fromRelease=metres({at.x,at.y,at.z},{controllerUiFixture::remoteSpawnOrigin[0].load(),controllerUiFixture::remoteSpawnOrigin[1].load(),controllerUiFixture::remoteSpawnOrigin[2].load()});}
       if(vc){const Vec3 v=read<Vec3>(vc);speedSeen=length(v);   // the game's speed of the arrow as made (90 m/s for the bow measured on 10-21; the guest's own flight uses it)
        if(speedSeen>1){const Vec3 aim{controllerUiFixture::remoteSpawnDirection[0].load(),controllerUiFixture::remoteSpawnDirection[1].load(),controllerUiFixture::remoteSpawnDirection[2].load()};
         aligned=length(aim)<.5f||dot(v*(1/speedSeen),aim*(1/length(aim)))>=.9f;reach=std::min(40.,4+double(speedSeen)*double(now-spawned+50)/1000.);}}
       if(vc&&gc&&cc&&aligned&&fromRelease>=0&&fromRelease<=reach){auto p=read<WorldPositionQ32>(cc);p.y+=int64_t(300.0*4294967296.);const Vec3 zero{};const uint8_t off=0;SIZE_T n=0;
        parked=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(cc),&p,24,&n)&&WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(vc),&zero,12,&n)&&
         WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(gc+16),&off,1,&n);}}catch(...){}
      if(parked){arrowParkedFor=spawned;RemoteArrow* slot=&remoteArrows[0];for(auto& a:remoteArrows)if(a.ms<slot->ms)slot=&a;*slot={id,now,true};}
      if(arrowLogs++<300)logFile->event("remote_arrow_seen","\"entity\":"+std::to_string(id)+",\"since_spawn_ms\":"+std::to_string(now-spawned)+",\"from_release_m\":"+std::to_string(fromRelease)+",\"speed_seen\":"+std::to_string(speedSeen)+",\"reach_m\":"+std::to_string(reach)+",\"aligned\":"+(aligned?"true":"false")+",\"parked\":"+(parked?"true":"false"));}}
    arrowKnown.swap(seen);arrowPrimed=true;}}}
 else if(otherPlayers.empty()&&arrowPrimed){arrowKnown.clear();arrowPrimed=false;}
 // parked arrows no hit came for: away below the world
 for(auto& a:remoteArrows)if(a.entity&&a.parked&&now-a.ms>3000){
  try{const auto e=nativeImpactComponents::entityAtWorld(world,a.entity);
   const auto vc=nativeImpactComponents::component(e,unsigned(types.velocity),12),gc=nativeImpactComponents::component(e,unsigned(types.gravity),20),cc=nativeImpactComponents::component(e,unsigned(types.current),56);
   if(vc&&gc&&cc){auto p=read<WorldPositionQ32>(cc);p.y-=int64_t(2000.0*4294967296.);const Vec3 down{0,-50,0};const uint8_t on=1;SIZE_T n=0;
    WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(cc),&p,24,&n);WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(vc),&down,12,&n);WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(gc+16),&on,1,&n);}}catch(...){}
  if(arrowLogs++<300)logFile->event("remote_arrow_dropped","\"entity\":"+std::to_string(a.entity));a.entity=0;a.parked=false;}
 for(auto& h:shotHits){if(h.used)continue;if(now<h.ms||now-h.ms>1500){h.used=true;if(arrowLogs++<300)logFile->event("remote_arrow_unmatched","\"character\":"+std::string(h.character?"true":"false"));continue;}
  RemoteArrow* arrow=nullptr;for(auto& a:remoteArrows)if(a.entity&&now-a.ms<=3000&&(!arrow||a.ms>arrow->ms))arrow=&a;if(!arrow)continue;
  try{const auto e=nativeImpactComponents::entityAtWorld(world,arrow->entity);
   const auto vc=nativeImpactComponents::component(e,unsigned(types.velocity),12),cc=nativeImpactComponents::component(e,unsigned(types.current),56);
   if(!vc||!cc){arrow->entity=0;continue;}
   // the host's thing nearest to the guest's hit
   std::array<int64_t,3> target=h.at;double best=3;bool byEntity=false;
   for(const auto& c:cache)if(h.entity&&c.target.entity==h.entity&&c.target.character==h.character&&metres(c.target.position,h.at)<=6.){best=metres(c.target.position,h.at);target=c.target.position;byEntity=true;break;}
   if(!byEntity)for(const auto& c:cache)if(c.target.character==h.character){const double m=metres(c.target.position,h.at);if(m<best){best=m;target=c.target.position;}}
   target[1]+=int64_t((h.character?1.0:.4)*4294967296.);
   const auto p=read<WorldPositionQ32>(cc);Vec3 dir{float(double(target[0]-p.x)/4294967296.),float(double(target[1]-p.y)/4294967296.),float(double(target[2]-p.z)/4294967296.)};
   const float distance=length(dir);if(!(distance>1e-3f)){h.used=true;continue;}dir=dir*(1/distance);
   const Vec3 v=read<Vec3>(vc);const float speed=std::max(length(v),30.f);
   const WorldPositionQ32 place{target[0]-int64_t(double(dir.x)*4294967296.),target[1]-int64_t(double(dir.y)*4294967296.),target[2]-int64_t(double(dir.z)*4294967296.)};
   const Vec3 nv=dir*speed;SIZE_T n=0;
   const bool wrote=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(cc),&place,24,&n)&&WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(vc),&nv,12,&n);
   arrow->parked=false;
   if(arrowLogs++<300)logFile->event("remote_arrow_steered","\"entity\":"+std::to_string(arrow->entity)+",\"character\":"+(h.character?"true":"false")+",\"hit_age_ms\":"+std::to_string(now-h.ms)+
    ",\"guest_to_host_m\":"+std::to_string(best<3||byEntity?best:-1.)+",\"by_entity\":"+(byEntity?"true":"false")+",\"moved_m\":"+std::to_string(distance)+",\"speed\":"+std::to_string(speed)+",\"written\":"+(wrote?"true":"false"));
   h.used=true;arrow->entity=0;
  }catch(...){arrow->entity=0;}
 }
}
// The host's thing nearest to where a guest's stroke touched one (remoteMelee): a creature for a creature's touch.
inline bool nearestTo(const std::array<int64_t,3>& at,bool character,uint32_t wanted,float within,std::array<int64_t,3>& out,uint32_t& entity,float& away){
 std::lock_guard<std::mutex> lock(mutex);float best=within;bool found=false;
 // the same id within 6 m only (a guest's own things - its colliders of pots and walls - may have ids of their own)
 if(wanted)for(const auto& c:cache)if(c.target.entity==wanted&&c.target.character==character&&metres(c.target.position,at)<=(character?6.:1.5)){out=c.target.position;entity=wanted;away=float(metres(c.target.position,at));return true;}
 for(const auto& c:cache){if(c.target.character!=character)continue;const float d=float(metres(c.target.position,at));
  if(d<best){best=d;out=c.target.position;entity=c.target.entity;found=true;}}
 if(found)away=best;return found;
}
inline bool entityPosition(uint32_t entity,std::array<int64_t,3>& out,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);
 for(const auto& c:cache)if(entity&&c.target.entity==entity&&now>=c.seen&&now-c.seen<=4000){out=c.target.position;return true;}
 // a creature the game names this tick before the pass of the map has kept it (review W7: its touch was never sent)
 if(entity&&quickTick&&now>=quickTick&&now-quickTick<=1000)for(size_t k=0;k<quickCount;++k)if(quick[k].entity==entity){out=quick[k].position;return true;}
 return false;
}
// Any thread: how far along this line (the game's world; direction a unit vector) the aim mark sits - the creature in
// the way, the level ground of the player's feet, or 128 m. by: 0 far, 1 ground, 2 creature.
inline float aimReach(WorldPositionQ32 origin,Vec3 direction,uint64_t now,int& by){
 by=0;float groundY=std::nanf("");
 rootCamera::Sample root;
 if(rootCamera::trySnapshot(root)&&root.number&&now>=root.milliseconds&&now-root.milliseconds<=1000)groundY=float(relativeQ32(root.stable.position.y,origin.y));
 std::array<evr::aim::MarkBody,maximumCached> bodies{};size_t count=0;const evr::motion::LiveTargetReach reach;
 {std::lock_guard<std::mutex> lock(mutex);
  if(requested&&types.usable&&lastTick&&now>=lastTick&&now-lastTick<=1000)for(const auto& c:cache){
   if(!c.target.character||count>=bodies.size())continue;
   bodies[count++]={{float(relativeQ32(c.target.position[0],origin.x)),float(relativeQ32(c.target.position[1],origin.y)),float(relativeQ32(c.target.position[2],origin.z))},reach.characterRadius,reach.height};}}
 return evr::aim::markReach({true,{0,0,0},direction},groundY,bodies.data(),count,&by);
}
// A new World: the records of strokes, broken things, estimates and parked arrows of the old one go (their ids are another World's).
// And the estimate's types of the new World are read now, while it loads, not at the first stroke (a few tens of ms).
inline void forgetWorldRecords(uintptr_t world){predicted={};hiddenThings={};thingTouches={};hostEstimates={};remoteArrows={};
 if(predictWanted)try{estimateTypesOf(world);}catch(...){}}
inline void summary(){
 if(!requested)return;std::lock_guard<std::mutex> lock(mutex);
 logFile->event("live_targets_removed","\"ticks\":"+std::to_string(ticks.load())+",\"full_scans\":"+std::to_string(fullScans)+",\"adopted\":"+std::to_string(adopted.load())+",\"dropped\":"+std::to_string(dropped.load())+
  ",\"cached\":"+std::to_string(cache.size())+",\"failures\":"+std::to_string(failures.load())+",\"read_only\":true,\"game_functions_called\":0");
}
}
