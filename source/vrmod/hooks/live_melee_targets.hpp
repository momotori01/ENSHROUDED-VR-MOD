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
struct Types {uintptr_t world=0,registry=0;int health=-1,current=-1,actor=-1;uint16_t healthBytes=68;bool usable=false;};
struct Layout {bool health=false,current=false,actor=false;uint16_t healthOffset=0,currentOffset=0;};
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
  if(!text||length<16||length>27)continue;
  char name[28]{};if(!block(text,name,size_t(length)))continue;const std::string_view n(name,size_t(length));
  if(n=="keen::ecs::Health"&&size>=40&&size<=256){types.health=int(id);types.healthBytes=size;}
  else if(n=="keen::ecs::CurrentTransform"&&size==56)types.current=int(id);
  else if(n=="keen::ecs::Actor")types.actor=int(id);
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
 types.registry=registry;types.usable=types.health>=0&&types.current>=0;
 logFile->event("live_targets_types","\"world\":"+hex(world)+",\"types\":"+std::to_string(header[2])+",\"health\":"+std::to_string(types.health)+",\"health_bytes\":"+std::to_string(types.healthBytes)+",\"current_transform\":"+std::to_string(types.current)+
  ",\"actor\":"+std::to_string(types.actor)+",\"usable\":"+(types.usable?"true":"false")+",\"read_only\":true");
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
  l.actor=has(types.actor);
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
  if(types.world!=world||types.registry!=read<uintptr_t>(world)){layouts.clear();cache.clear();cursor=0;learn(world);}
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
  if(cursor>=capacity){cursor=0;++fullScans;layouts.clear();}
  const uint64_t first=cursor&~63ull,count=std::min<uint64_t>(slotsPerTick,capacity-first);
  std::vector<uint64_t> occupied(size_t((count+63)/64));std::vector<uint32_t> ids(static_cast<size_t>(count));std::vector<uintptr_t> rows(static_cast<size_t>(count));
  std::array<uint8_t,0x58> again{};
  const bool whole=block(bitmap+(first/64)*8,occupied.data(),occupied.size()*8)&&block(keys+first*4,ids.data(),ids.size()*4)&&block(values+first*8,rows.data(),rows.size()*8)&&
   block(world+0x148,again.data(),again.size())&&again==map;
  cursor=first+count;
  if(whole)for(uint64_t at=0;at<count;++at){
   if(!((occupied[size_t(at/64)]>>(at%64))&1))continue;
   const auto id=ids[size_t(at)];if(!id||id==local||!rows[size_t(at)])continue;
   const auto r=record(rows[size_t(at)]);if(!r.valid)continue;
   const auto l=layout(r.layout);if(!l.health||!l.current)continue;
   const auto hp=r.data+l.healthOffset+uintptr_t(r.row)*types.healthBytes,pose=r.data+l.currentOffset+uintptr_t(r.row)*56;
   std::array<uint8_t,256> h{};game::RootTransform t{};if(!block(hp,h.data(),types.healthBytes)||!block(pose,&t,sizeof(t)))continue;
   {static std::atomic<unsigned> shown{0};if(types.healthBytes!=68&&shown.fetch_add(1)<6){const char* d="0123456789abcdef";std::string s;
     for(unsigned k=0;k<types.healthBytes;++k){s+=d[h[k]>>4];s+=d[h[k]&15];}
     logFile->event("live_targets_health_sample","\"entity\":"+std::to_string(id)+",\"bytes\":"+std::to_string(types.healthBytes)+",\"health\":\""+s+"\"");}}
   int32_t health=0;
   if(types.healthBytes==68){
    uint32_t offset=0,values4=0;std::memcpy(&offset,h.data()+8,4);std::memcpy(&values4,h.data()+12,4);
    if(!(h[16]&1)||offset!=36||!values4||values4>8)continue;                // the Health layout that motion_target_observer verified
    std::memcpy(&health,h.data()+offset,4);
   }else if(types.healthBytes==40){
    // 1.0: a u32 hash, a byte count of values (8), two bytes, then eight int32 values from +8 (live_targets_health_sample,
    // Mock 20261016-141748: a living creature 20 at +8, +16 and +36, a dead or empty row all zero). The first is taken.
    if(!h[4]||h[4]>8)continue;std::memcpy(&health,h.data()+8,4);
   }else continue;
   if(health<=0)continue;            // dead or destroyed
   const std::array<int64_t,3> position{t.position.x,t.position.y,t.position.z};
   if(metres(position,origin)>(l.actor?aimKeepMetres:keepMetres))continue;
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
 if(!requested||!types.usable||!lastTick||now<lastTick||now-lastTick>1000||!weapon.blade)return out;
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
