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
struct Types {uintptr_t world=0,registry=0;int health=-1,current=-1,actor=-1,selected=-1,relevant=-1,networkHealth=-1,render=-1,networkActor=-1,player=-1,enemy=-1;
 uint16_t healthBytes=68,selectedBytes=0,relevantBytes=0,renderBytes=0,playerBytes=0;bool usable=false;};
struct Layout {bool health=false,current=false,actor=false,selected=false,relevant=false,networkHealth=false,render=false,player=false,enemy=false;std::array<uint64_t,10> bits{};
 uint16_t healthOffset=0,currentOffset=0,selectedOffset=0,relevantOffset=0,networkHealthOffset=0,renderOffset=0,playerOffset=0;};
// Fallen players near the local one (2026-10-20, both players: "I cannot revive the other" - the revive is the game's
// interaction held for reviveDuration, and the right B of the mod only taps it, a longer hold opening the quick wheel).
struct Downed {uint32_t entity=0;std::array<int64_t,3> position{};uint64_t seen=0;};
inline std::vector<Downed> downed;
// What a full pass of the map found (once in a while): which components the entities near the player have.
struct Census {unsigned entities=0,health=0,networkHealth=0,current=0,render=0,player=0,downed=0;
 unsigned all=0,allHealth=0,allNetworkHealth=0,allCurrent=0,allRender=0,allPlayers=0,allEnemies=0;};   // all: anywhere in the map
// A World without anything that has a health or is an enemy (2026-10-20, Mock game-20261020-021225: a game's client World -
// what a guest has - holds 2455 entities, the 230 near the player the same as the server's 230, but none with Health,
// NetworkHealth or Destructible: only models, colliders and the like). There no target can be known, and the gate is let
// go - a stroke attacks as before the gate (2026-10-11) - instead of never attacking (the guest of 20261020-002320).
inline bool blind=false;inline unsigned blindLogs=0;
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
struct Contact {bool reached=true,gated=false;uint32_t entity=0;float distance=0,nearest=-1;unsigned cached=0;bool ahead=false;};
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
inline uint64_t cursor=0,lastTick=0,lastLog=0,fullScans=0;inline std::atomic<unsigned> ticks{0},failures{0},adopted{0},dropped{0};
// 4096 of the map's 131072 slots a tick: the whole map about twice a second. A blade reaches 2.3 m from the player and
// a target counts from 1.3 m, so 10 m leaves a running creature a second to be noticed. Measured cost: none visible
// in the frame times (game191128, 2048 slots: median 27.0 ms with the list, 26.9 ms without).
constexpr unsigned slotsPerTick=4096,maximumCached=64;
constexpr double keepMetres=10,dropMetres=12;
// Creatures are kept further out: the aim mark of a bow, staff or wand ends on the creature its line passes through
// (aimReach below; include/evr/aim_mark.hpp markReach). The nearest 64 stay, as before.
constexpr double aimKeepMetres=60,aimDropMetres=66;
inline bool block(uintptr_t address,void* out,size_t bytes){
 SIZE_T count=0;return address&&bytes&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&count)&&count==bytes;
}
inline double metres(const std::array<int64_t,3>& a,const std::array<int64_t,3>& b){
 constexpr double q=1.0/4294967296.0;const double x=double(a[0]-b[0])*q,y=double(a[1]-b[1])*q,z=double(a[2]-b[2])*q;return std::sqrt(x*x+y*y+z*z);
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
  if(!text||length<16||length>40)continue;
  char name[41]{};if(!block(text,name,size_t(length)))continue;const std::string_view n(name,size_t(length));
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
  l.enemy=has(types.enemy);l.bits=bits;
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
  rootCamera::Sample root;if(!rootCamera::trySnapshot(root)||root.entity!=local)return;
  const std::array<int64_t,3> origin{root.stable.position.x,root.stable.position.y,root.stable.position.z};
  const auto now=GetTickCount64();std::lock_guard<std::mutex> lock(mutex);++ticks;
  if(types.world!=world||types.registry!=read<uintptr_t>(world)){layouts.clear();cache.clear();cursor=0;learn(world);censusLogs=0;census={};typeCounts={};typePasses=0;}
  if(!types.usable)return;
  // The targets already known: where they are now, or forget them.
  for(auto i=cache.begin();i!=cache.end();){
   const auto r=record(i->record);game::RootTransform t{};
   bool keep=r.valid&&r.layout==i->layout&&r.data==i->data&&r.row==i->row&&now-i->seen<=4000&&block(i->pose,&t,sizeof(t));
   if(keep){i->target.position={t.position.x,t.position.y,t.position.z};keep=metres(i->target.position,origin)<=(i->target.character?aimDropMetres:dropMetres);}
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
     ",\"players\":"+std::to_string(census.player)+",\"fallen_players\":"+std::to_string(census.downed)+",\"cached\":"+std::to_string(cache.size())+
     ",\"map_entities\":"+std::to_string(census.all)+",\"map_health\":"+std::to_string(census.allHealth)+",\"map_network_health\":"+std::to_string(census.allNetworkHealth)+
     ",\"map_current_transform\":"+std::to_string(census.allCurrent)+",\"map_render_transform\":"+std::to_string(census.allRender)+",\"map_players\":"+std::to_string(census.allPlayers));}
   {const bool now_blind=census.all>=64&&!census.allHealth&&!census.allNetworkHealth&&!census.allEnemies;
    if(now_blind!=blind&&blindLogs++<50)logFile->event("live_targets_blind","\"world\":"+hex(world)+",\"blind\":"+(now_blind?"true":"false")+",\"entities\":"+std::to_string(census.all)+",\"gate\":\""+(now_blind?"off_strokes_attack":"on")+"\"");
    blind=now_blind;}
   census={};remoteMelee::publishPlayers(playersSeen);playersSeen.clear();
   if(typePasses<2)tellTypes(world);typeCounts={};typeCountsNear={};}
  // Fallen players not seen again for a while are forgotten.
  downed.erase(std::remove_if(downed.begin(),downed.end(),[&](const Downed& d){return now<d.seen||now-d.seen>1500;}),downed.end());
  const uint64_t first=cursor&~63ull,count=std::min<uint64_t>(slotsPerTick,capacity-first);
  std::vector<uint64_t> occupied(size_t((count+63)/64));std::vector<uint32_t> ids(static_cast<size_t>(count));std::vector<uintptr_t> rows(static_cast<size_t>(count));
  std::array<uint8_t,0x58> again{};
  const bool whole=block(bitmap+(first/64)*8,occupied.data(),occupied.size()*8)&&block(keys+first*4,ids.data(),ids.size()*4)&&block(values+first*8,rows.data(),rows.size()*8)&&
   block(world+0x148,again.data(),again.size())&&again==map;
  cursor=first+count;
  if(whole)for(uint64_t at=0;at<count;++at){
   if(!((occupied[size_t(at/64)]>>(at%64))&1))continue;
   const auto id=ids[size_t(at)];if(id&&id==local&&rows[size_t(at)])localRow=rows[size_t(at)];
   if(!id||id==local||!rows[size_t(at)])continue;
   const auto r=record(rows[size_t(at)]);if(!r.valid)continue;
   const auto l=layout(r.layout);
   if(typePasses<2)for(unsigned b=0;b<640;++b)if((l.bits[b/64]>>(b%64))&1)++typeCounts[b];
   ++census.all;if(l.health)++census.allHealth;if(l.networkHealth)++census.allNetworkHealth;if(l.current)++census.allCurrent;if(l.render)++census.allRender;if(l.player)++census.allPlayers;if(l.enemy)++census.allEnemies;
   if(!(l.health||l.networkHealth||l.enemy)||!(l.current||l.render)){
    if(typePasses<2&&(l.current||l.render)){game::RootTransform t{};const auto spot=l.current?r.data+l.currentOffset+uintptr_t(r.row)*56:r.data+l.renderOffset+uintptr_t(r.row)*types.renderBytes;
     if(block(spot,&t,sizeof t)&&metres({t.position.x,t.position.y,t.position.z},origin)<=aimKeepMetres)for(unsigned b=0;b<640;++b)if((l.bits[b/64]>>(b%64))&1)++typeCountsNear[b];}
    continue;}
   const auto pose=l.current?r.data+l.currentOffset+uintptr_t(r.row)*56:r.data+l.renderOffset+uintptr_t(r.row)*types.renderBytes;
   game::RootTransform t{};if(!block(pose,&t,sizeof(t)))continue;
   const std::array<int64_t,3> position{t.position.x,t.position.y,t.position.z};
   const double away=metres(position,origin);
   if(away<=aimKeepMetres){++census.entities;if(l.health)++census.health;if(l.networkHealth)++census.networkHealth;if(l.current)++census.current;if(l.render)++census.render;if(l.player)++census.player;}
   if(l.player&&playersSeen.size()<8)playersSeen.push_back({id,WorldPositionQ32{position[0],position[1],position[2]},now});
   if(typePasses<2&&away<=aimKeepMetres)for(unsigned b=0;b<640;++b)if((l.bits[b/64]>>(b%64))&1)++typeCountsNear[b];
   int32_t health=0;
   if(!l.health&&!l.networkHealth)health=1;   // NetworkEnemy alone (a guest's World without either health): taken as alive
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
   if(away>(l.actor?aimKeepMetres:keepMetres))continue;
   const auto same=record(rows[size_t(at)]);if(!same.valid||same.layout!=r.layout||same.data!=r.data||same.row!=r.row)continue;
   auto known=std::find_if(cache.begin(),cache.end(),[&](const Cached& c){return c.target.entity==id;});
   if(known==cache.end()){
    if(cache.size()>=maximumCached){   // keep the nearest
     auto farthest=std::max_element(cache.begin(),cache.end(),[&](const Cached& a,const Cached& b){return metres(a.target.position,origin)<metres(b.target.position,origin);});
     if(metres(farthest->target.position,origin)<=metres(position,origin))continue;
     cache.erase(farthest);++dropped;
    }
    cache.push_back({});known=cache.end()-1;++adopted;
   }
   known->target.entity=id;known->target.position=position;known->target.character=l.actor;
   known->record=rows[size_t(at)];known->layout=r.layout;known->data=r.data;known->row=r.row;known->pose=pose;known->seen=now;
  }
  // The local player's combat target (slot 0 of its SelectedTargets), once a tick; and its hard-lock candidate (slot 13).
  if(types.selected>=0&&localRow){
   const auto r=record(localRow);CombatTarget c,h;
   if(r.valid){const auto l=layout(r.layout);
    if(l.selected){
     auto readSlot=[&](size_t slot,CombatTarget& t){std::array<uint8_t,targetInfoBytes> info{};
      if(!block(r.data+l.selectedOffset+uintptr_t(r.row)*types.selectedBytes+slot*targetInfoBytes,info.data(),info.size()))return;
      t.valid=true;std::memcpy(t.position.data(),info.data(),24);std::memcpy(t.offset.data(),info.data()+0x38,40);std::memcpy(&t.entity,info.data()+0x64,4);
      std::memcpy(&t.counter,info.data()+0x80,2);t.flags=info[0x82];t.ms=now;};
     readSlot(0,c);if(types.selectedBytes>=(hardLockCandidateSlot+1)*targetInfoBytes)readSlot(hardLockCandidateSlot,h);
     CombatTarget pick;double pickMetres=40.;
     if(l.relevant){std::array<uint8_t,relevantCountAt+1> enemies{};
      if(block(r.data+l.relevantOffset+uintptr_t(r.row)*types.relevantBytes,enemies.data(),enemies.size()))
       for(unsigned k=0;k<std::min<unsigned>(enemies[relevantCountAt],8);++k){const auto* e=enemies.data()+k*relevantTargetBytes;uint32_t who=0;std::memcpy(&who,e,4);
        for(auto* t:{&c,&h}){if(!t->valid||!t->entity||who!=t->entity||t->relevant)continue;
         t->relevant=true;std::memcpy(t->relevantPosition.data(),e+8,24);std::memcpy(&t->height,e+0x48,4);t->boxValid=e[0x4c]!=0;t->current=e[0x4f]!=0;}
        if(!who)continue;
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
  lastTick=now;
  if(now-lastLog>=5000){lastLog=now;
   std::ostringstream o;o<<std::setprecision(4)<<"\"cached\":"<<cache.size()<<",\"full_scans\":"<<fullScans<<",\"ticks\":"<<ticks.load()<<",\"map_capacity\":"<<capacity<<",\"adopted\":"<<adopted.load()<<",\"dropped\":"<<dropped.load()<<",\"failures\":"<<failures.load()<<",\"nearest\":[";
   std::vector<const Cached*> order;for(const auto& c:cache)order.push_back(&c);
   std::sort(order.begin(),order.end(),[&](const Cached* a,const Cached* b){return metres(a->target.position,origin)<metres(b->target.position,origin);});
   for(size_t i=0;i<order.size()&&i<6;++i)o<<(i?",":"")<<"{\"entity\":"<<order[i]->target.entity<<",\"metres\":"<<metres(order[i]->target.position,origin)<<",\"character\":"<<(order[i]->target.character?"true":"false")<<'}';
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
 for(const auto& c:cache){
  float distance=evr::motion::bladeDistance(weapon.bladeFrom,weapon.bladeTo,c.target,reach);bool touches=evr::motion::bladeNear(weapon.bladeFrom,weapon.bladeTo,c.target,reach),later=false;
  if(!touches&&seen.ahead&&evr::motion::bladeNear(seen.aheadFrom,seen.aheadTo,c.target,reach)){touches=later=true;distance=evr::motion::bladeDistance(seen.aheadFrom,seen.aheadTo,c.target,reach);}
  if(touches&&distance<best){best=distance;out.reached=true;out.entity=c.target.entity;out.distance=distance;out.ahead=later;}
  if(std::isfinite(distance)&&(out.nearest<0||distance<out.nearest))out.nearest=distance;
 }
 return out;
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
inline bool entityPosition(uint32_t entity,std::array<int64_t,3>& out,uint64_t now){
 std::lock_guard<std::mutex> lock(mutex);
 for(const auto& c:cache)if(entity&&c.target.entity==entity&&now>=c.seen&&now-c.seen<=4000){out=c.target.position;return true;}
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
inline void summary(){
 if(!requested)return;std::lock_guard<std::mutex> lock(mutex);
 logFile->event("live_targets_removed","\"ticks\":"+std::to_string(ticks.load())+",\"full_scans\":"+std::to_string(fullScans)+",\"adopted\":"+std::to_string(adopted.load())+",\"dropped\":"+std::to_string(dropped.load())+
  ",\"cached\":"+std::to_string(cache.size())+",\"failures\":"+std::to_string(failures.load())+",\"read_only\":true,\"game_functions_called\":0");
}
}
