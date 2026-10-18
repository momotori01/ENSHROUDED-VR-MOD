#pragma once
// Bounded read-only reconstruction of native 8d3630/8c93e0/8c7340.
// Component IDs are learned from exact task-query pointers, never hardcoded.
namespace nativeImpactComponents {
// PE reflection keen::ecs::EcsComponentsMask: uint64[10],80bytes.
// Live registry capacity640/count598 independently matches this upper bound.
// Do not interpret storage beyond the reflected mask as additional type IDs.
inline constexpr unsigned componentCapacity=640;
inline bool requested=false;
struct Entity {uintptr_t world=0,record=0,layout=0,data=0;uint32_t row=0;};
struct Binding {int hierarchy=-1,item=-1,current=-1,slot=-1,serverSlot=-1,actor=-1,render=-1;unsigned actorStride=0;};
inline std::mutex mutex;inline std::map<uintptr_t,Binding> bindings;
inline uint32_t hashEntity(uint32_t x){x=((x>>16)^x)*0x45d9f3bu;x=((x>>16)^x)*0x45d9f3bu;return (x>>16)^x;}
inline Entity entityAtWorld(uintptr_t world,uint32_t id){
 Entity e;e.world=world;if(!e.world||!id)throw std::runtime_error("component lookup world/entity absent");
 const auto map=e.world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keys=read<uintptr_t>(map+0x28),values=read<uintptr_t>(map+0x40);const auto count=read<uint32_t>(map+0x54);
 if(!bitmap||!keys||!values||!capacity||capacity>0x100000||(capacity&(capacity-1))||!count||count>capacity)throw std::runtime_error("entity map bounds");
 auto bucket=uint64_t(hashEntity(id))&(capacity-1);
 for(uint64_t n=0;n<capacity;++n){if(!(read<uint64_t>(bitmap+8*(bucket/64))&(1ull<<(bucket%64))))break;
  if(read<uint32_t>(keys+bucket*4)==id){e.record=read<uintptr_t>(values+bucket*8);break;}bucket=(bucket+1)&(capacity-1);
 }
 if(!e.record)throw std::runtime_error("current query entity absent from world map");
 if(bitmap!=read<uintptr_t>(map+8)||capacity!=read<uint64_t>(map+0x10)||keys!=read<uintptr_t>(map+0x28)||values!=read<uintptr_t>(map+0x40)||read<uint32_t>(keys+bucket*4)!=id||read<uintptr_t>(values+bucket*8)!=e.record)throw std::runtime_error("entity lookup changed");
 e.layout=read<uintptr_t>(e.record+0x18);e.data=read<uintptr_t>(e.record+0x20);e.row=read<uint32_t>(e.record+0x30);
 if(!e.layout||!e.data||e.row>0x1000000)throw std::runtime_error("entity component layout bounds");return e;
}
inline Entity entityAt(uintptr_t taskState,uint32_t id){return entityAtWorld(read<uintptr_t>(taskState+0x30),id);}
inline uintptr_t component(const Entity& e,unsigned id,unsigned bytes){
 if(id>=componentCapacity)throw std::runtime_error("component id outside diagnostic search bound");
 if(!(read<uint64_t>(e.layout+8*(id/64))&(1ull<<(id%64))))return 0;
 const auto stride=read<uint16_t>(e.layout+0xa84+id*2),offset=read<uint16_t>(e.layout+0x84+id*2);
 if(stride!=bytes)return 0;return e.data+offset+uintptr_t(e.row)*stride;
}
inline int identify(const Entity& e,uintptr_t expected,unsigned bytes){
 if(!expected)return -1;int found=-1;
 const auto bits=read<std::array<uint64_t,10>>(e.layout);
 for(unsigned i=0;i<componentCapacity;++i){if(!(bits[i/64]&(1ull<<(i%64))))continue;if(component(e,i,bytes)==expected){if(found!=-1)throw std::runtime_error("ambiguous component query pointer");found=int(i);}}
 if(found<0)throw std::runtime_error("component query pointer not in bounded native layout");return found;
}
// Runtime ECS registry evidence: World[0],256byte records,+10 qualified name.
// Static8964e0 plus two independent live query captures establish this layout.
struct ComponentType {unsigned bytes=0;std::string qualifiedName;std::array<uint8_t,64> record{};};
inline ComponentType componentType(uintptr_t world,unsigned id){
 if(id>=componentCapacity)throw std::runtime_error("type id outside reflected mask");
 const auto registry=read<uintptr_t>(world);const auto header=read<std::array<uintptr_t,4>>(registry);
 if(!header[1]||!header[2]||header[2]>header[3]||header[3]>componentCapacity||id>=header[2])throw std::runtime_error("runtime type registry bounds");
 const auto at=header[1]+uintptr_t(id)*256;const auto raw=read<std::array<uint8_t,64>>(at);
 uintptr_t text=0,length=0;uint16_t size=0;std::memcpy(&text,raw.data()+16,8);std::memcpy(&length,raw.data()+24,8);std::memcpy(&size,raw.data()+56,2);
 if(!text||!length||length>256)throw std::runtime_error("runtime type name bounds");
 ComponentType result;result.bytes=size;result.record=raw;result.qualifiedName.reserve(length);
 for(size_t i=0;i<length;++i)result.qualifiedName.push_back(char(read<uint8_t>(text+i)));
 for(size_t i=0;i<length;++i)if(result.qualifiedName[i]!=char(read<uint8_t>(text+i)))throw std::runtime_error("runtime type name changed");
 if(registry!=read<uintptr_t>(world)||header!=read<std::array<uintptr_t,4>>(registry)||raw!=read<std::array<uint8_t,64>>(at))throw std::runtime_error("runtime type registry changed");
 return result;
}
// Diagnostic (2026-10-16): every component type of a World, once, as [id, name, size] - after the 1.0 update several
// observers found their types by June's sizes and stayed idle; this table is what a new build has.
inline void dumpTypes(uintptr_t world) noexcept {
 static std::mutex m;static std::vector<uintptr_t> done;{std::lock_guard<std::mutex> lock(m);if(!world||done.size()>8||std::find(done.begin(),done.end(),world)!=done.end())return;done.push_back(world);}
 try{std::string out;unsigned count=0;
  for(unsigned i=0;i<componentCapacity;++i){try{const auto t=componentType(world,i);
    out+=std::string(count++?",":"")+"["+std::to_string(i)+","+jsonString(t.qualifiedName)+","+std::to_string(t.bytes)+"]";}catch(const std::exception&){break;}}
  logFile->event("ecs_component_types","\"world\":"+hex(world)+",\"count\":"+std::to_string(count)+",\"types\":["+out+"]");
 }catch(...){}
}
// A task's +30 world lookup capability can be null. Resolve task query
// ownership by exact component storage identity against already observed Worlds.
// The local player's component in each World, as the last full resolve saw it (2026-10-19, server load): resolveWorld is
// asked for every actor of every World each tick (motion_event_selection_observer.hpp select, input_actor_observer.hpp),
// and each full answer reads the World's entity map and the entity's layout through ReadProcessMemory - with the 1.0
// actor sizes up to 640 components a call for a World where the pointer is not the player's. The game's server thread
// spent 27 % of its time there (Mock game-20261019-012818, hooks/thread_stack_sampler.hpp), more with more actors (a
// friend joined: the server thread at 92-97 % of a core, "[server] Bad Performance", the hits and guards late).
// For knownMs after a full answer, a pointer that is none of the player's components is answered "no World" at once.
struct Known {uint32_t entity=0;std::array<uintptr_t,4> pointers{};uint64_t ms=0;};
inline std::map<std::pair<uintptr_t,unsigned>,Known> known;   // (World, bytes) -> the player's component there (0: none); guarded by mutex
inline constexpr uint64_t knownMs=250;inline std::atomic<uint64_t> fastAnswers{0},fullAnswers{0};
inline void remember(uintptr_t world,unsigned bytes,uint32_t entity,std::array<uintptr_t,4> pointers){
 std::lock_guard<std::mutex> lock(mutex);known[{world,bytes}]=Known{entity,pointers,GetTickCount64()};}
inline std::atomic<uint64_t> unknownWorlds{0},mineAnswers{0};
// Whether any World has the player's Actor bound (a host: the server's World does; a client never - its own Actor is not a
// component of a World this process runs, 2026-10-19 friend's log: only the render binding, action_sequence world 0).
inline bool actorBound(){std::lock_guard<std::mutex> lock(mutex);for(const auto& b:bindings)if(b.second.actor>=0)return true;return false;}
// EVR_FORCE_CLIENT_TICKS=1 (test, 2026-10-19): a single player's updates for the player (targets, action bar, equipment, the
// fog timer) run the way a client's do - from the camera's World (inputActor::clientWorldTicks) - and not from the server's
// World the player's Actor is bound to. The guest's throwing and bow (headset 01:09, the friend: no action bar seen at all,
// so no throwable in the hand) depend on that path, which a single player never takes otherwise.
inline const bool forceClientTicks=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FORCE_CLIENT_TICKS",v,sizeof v);return n==1&&v[0]=='1';}();
// When those client updates last ran (GetTickCount64, inputActor final actor query): this player is a client while it
// is recent - no World of this process has the player's Actor (or EVR_FORCE_CLIENT_TICKS=1).
inline std::atomic<uint64_t> clientTickMs{0};
inline bool clientNow(uint64_t now){const auto at=clientTickMs.load();return at&&now>=at&&now-at<=1000;}
inline uintptr_t resolveWorld(uint32_t entity,uintptr_t pointer,unsigned bytes) noexcept {
 if(!requested||!entity||!pointer||(bytes!=176&&bytes!=3104))return 0;
 try{
  {const auto now=GetTickCount64();std::lock_guard<std::mutex> lock(mutex);
   if(!bindings.empty()&&bindings.size()<=8){bool fresh=true,mine=false;
    for(const auto& b:bindings){const auto k=known.find({b.first,bytes});
     if(k==known.end()||k->second.entity!=entity||now<k->second.ms||now-k->second.ms>knownMs){fresh=false;++unknownWorlds;break;}
     for(const auto p:k->second.pointers)if(p&&p==pointer)mine=true;}
    if(fresh&&!mine){++fastAnswers;return 0;}if(fresh)++mineAnswers;}}
  if(const auto n=++fullAnswers;n<=3||n%2000==0)try{logFile->event("resolve_world_stats","\"full\":"+std::to_string(n)+",\"fast\":"+std::to_string(fastAnswers.load())+
   ",\"bytes\":"+std::to_string(bytes)+",\"worlds\":"+std::to_string(bindings.size())+",\"known\":"+std::to_string(known.size())+",\"unknown_worlds\":"+std::to_string(unknownWorlds.load())+",\"mine\":"+std::to_string(mineAnswers.load()));}catch(...){}
  std::vector<std::pair<uintptr_t,Binding>> candidates;{std::lock_guard<std::mutex> lock(mutex);for(const auto& b:bindings)candidates.push_back(b);}
  if(candidates.size()>8)return 0;uintptr_t found=0;
  for(const auto& entry:candidates){try{
   Entity e;try{e=entityAtWorld(entry.first,entity);}catch(...){remember(entry.first,bytes,entity,{});continue;}   // the player is not in this World
   int id=bytes==176?entry.second.render:entry.second.actor;unsigned stride=bytes==176?176:entry.second.actorStride;
   if(id<0){
    // Native Actor(3600) and ClientActor(3128) inherit BaseActor(3104)
    // at offset zero, verified through PE reflection metadata parent pointers.
    if(bytes==176){try{id=identify(e,pointer,176);stride=176;}catch(...){continue;}}
    else{for(unsigned size:std::array<unsigned,3>{3104,3128,3600}){try{const auto candidate=identify(e,pointer,size);if(id>=0)return 0;id=candidate;stride=size;}catch(...){}}
     // 1.0 (2026-10-16): the Actor types are not June's sizes any more, and no Actor was ever bound - every user of the
     // world (the targets of the live melee, the action bar, the equipment, the fog timer, motion_event_selection) stayed
     // idle on 1.0 (headset run 20261016-134128). The stride is the entity's own (its layout +0xa84); a component of at
     // least BaseActor's 3104 bytes that starts at the pointer.
     if(id<0){const auto bits=read<std::array<uint64_t,10>>(e.layout);unsigned actorSized=0;std::array<uintptr_t,4> sized{};
      for(unsigned i=0;i<componentCapacity;++i){if(!(bits[i/64]&(1ull<<(i%64))))continue;
       const auto s=read<uint16_t>(e.layout+0xa84+i*2);if(s<3104)continue;
       const auto c=component(e,i,s);if(actorSized<sized.size())sized[actorSized]=c;++actorSized;
       if(c==pointer){if(id>=0)return 0;id=int(i);stride=s;}}
      // Not this pointer: the player's actor here, if any, is one of its actor-sized components (none: no actor here).
      if(id<0&&actorSized<=sized.size())remember(e.world,bytes,entity,sized);}
     if(id<0)continue;}
   }
   {const auto mine=component(e,id,stride);remember(e.world,bytes,entity,{mine});if(mine!=pointer)continue;}
   if(found&&found!=e.world)return 0;found=e.world;
   {std::lock_guard<std::mutex> lock(mutex);auto& b=bindings[e.world];auto& dest=bytes==176?b.render:b.actor;if(dest<0){dest=id;if(bytes==3104)b.actorStride=stride;logFile->event("impact_query_world_binding","\"world\":"+hex(e.world)+",\"entity\":"+std::to_string(entity)+",\"component_id\":"+std::to_string(id)+",\"bytes\":"+std::to_string(bytes)+",\"component_stride\":"+std::to_string(stride)+",\"query_pointer_match\":true");}}
  }catch(...){} }
  return found;
 }catch(...){return 0;}
}
inline void learn(uintptr_t state,uint32_t entity,uintptr_t hierarchy,uintptr_t item){
 const auto e=entityAt(state,entity);std::lock_guard<std::mutex> lock(mutex);auto& b=bindings[e.world];const auto before=b;
 if(hierarchy){if(b.hierarchy<0)b.hierarchy=identify(e,hierarchy,16);else if(component(e,b.hierarchy,16)!=hierarchy)throw std::runtime_error("hierarchy query differs from native component lookup");}
 if(item){if(b.item<0)b.item=identify(e,item,44);else if(component(e,b.item,44)!=item)throw std::runtime_error("item query differs from native component lookup");}
 if(before.hierarchy!=b.hierarchy||before.item!=b.item){std::ostringstream o;o<<"\"world\":"<<hex(e.world)<<",\"entity\":"<<entity<<",\"hierarchy_component_id\":"<<b.hierarchy<<",\"item_component_id\":"<<b.item<<",\"query_pointer_match\":true,\"read_only\":true";logFile->event("impact_component_binding",o.str());}
}
template<size_t N> inline std::string asHex(const std::array<uint8_t,N>& b){const char* d="0123456789abcdef";std::string s;for(auto v:b){s+=d[v>>4];s+=d[v&15];}return s;}
inline void learnCurrent(uintptr_t state,uint32_t entity,uintptr_t current){
 const auto e=entityAt(state,entity);std::lock_guard<std::mutex> lock(mutex);auto& b=bindings[e.world];
 if(b.current<0){b.current=identify(e,current,56);logFile->event("impact_transform_component_binding","\"world\":"+hex(e.world)+",\"entity\":"+std::to_string(entity)+",\"current_component_id\":"+std::to_string(b.current)+",\"query_pointer_match\":true,\"read_only\":true");}
 else if(component(e,b.current,56)!=current)throw std::runtime_error("current transform query differs from native lookup");
}
inline std::array<uint8_t,56> currentTransform(uintptr_t state,uint32_t entity){
 const auto e=entityAt(state,entity);Binding b;{std::lock_guard<std::mutex> lock(mutex);const auto i=bindings.find(e.world);if(i!=bindings.end())b=i->second;}
 if(b.current<0)throw std::runtime_error("current transform type not learned");const auto p=component(e,b.current,56);if(!p)throw std::runtime_error("current transform absent");
 const auto r=read<std::array<uint8_t,56>>(p);if(r!=read<std::array<uint8_t,56>>(p))throw std::runtime_error("current transform changed while reading");return r;
}
inline void learnSlot(uintptr_t state,uint32_t entity,uintptr_t slot){
 if(!requested)return;
 const auto e=entityAt(state,entity);std::lock_guard<std::mutex> lock(mutex);auto& b=bindings[e.world];
 if(b.slot<0){b.slot=identify(e,slot,48);logFile->event("impact_slot_component_binding","\"world\":"+hex(e.world)+",\"entity\":"+std::to_string(entity)+",\"slot_component_id\":"+std::to_string(b.slot)+",\"query_pointer_match\":true,\"read_only\":true");}
 else if(component(e,b.slot,48)!=slot)throw std::runtime_error("slot attachment query differs from native lookup");
}
inline void appendSlot(std::ostringstream& o,uintptr_t state,uint32_t entity,const char* field){
 const auto e=entityAt(state,entity);Binding b;{std::lock_guard<std::mutex> lock(mutex);const auto i=bindings.find(e.world);if(i!=bindings.end())b=i->second;}
 const auto p=b.slot<0?0:component(e,b.slot,48);o<<",\""<<field<<"\":{\"world\":"<<hex(e.world)<<",\"component_id\":"<<b.slot<<",\"present\":"<<(p?"true":"false");
 if(p){const auto r=read<std::array<uint8_t,48>>(p);o<<",\"bytes\":"<<jsonString(asHex(r))<<",\"stable\":"<<(r==read<std::array<uint8_t,48>>(p)?"true":"false");}o<<'}';
}
inline void learnServerSlot(uintptr_t state,uint32_t entity,uintptr_t slot){
 const auto e=entityAt(state,entity);std::lock_guard<std::mutex> lock(mutex);auto& b=bindings[e.world];
 if(b.serverSlot<0){b.serverSlot=identify(e,slot,112);logFile->event("impact_server_slot_binding","\"world\":"+hex(e.world)+",\"entity\":"+std::to_string(entity)+",\"component_id\":"+std::to_string(b.serverSlot)+",\"query_pointer_match\":true");}
 else if(component(e,b.serverSlot,112)!=slot)throw std::runtime_error("server slot query differs from lookup");
}
inline void appendServerSlot(std::ostringstream& o,uintptr_t state,uint32_t entity){
 const auto e=entityAt(state,entity);Binding b;{std::lock_guard<std::mutex> lock(mutex);const auto i=bindings.find(e.world);if(i!=bindings.end())b=i->second;}
 const auto p=b.serverSlot<0?0:component(e,b.serverSlot,112);o<<",\"impact_server_slot\":{\"world\":"<<hex(e.world)<<",\"component_id\":"<<b.serverSlot<<",\"present\":"<<(p?"true":"false");
 if(p){const auto r=read<std::array<uint8_t,112>>(p);o<<",\"bytes\":"<<jsonString(asHex(r))<<",\"stable\":"<<(r==read<std::array<uint8_t,112>>(p)?"true":"false");}o<<'}';
}
struct Owner {uintptr_t world=0;Binding binding{};bool ready=false,stable=false;std::array<uint8_t,16> hierarchy{};std::array<uint8_t,44> item{};};
inline Owner owner(uintptr_t state,uint32_t entity){
 const auto e=entityAt(state,entity);Owner o;o.world=e.world;{std::lock_guard<std::mutex> lock(mutex);const auto i=bindings.find(e.world);if(i!=bindings.end())o.binding=i->second;}
 const auto b=o.binding;const auto h=b.hierarchy<0?0:component(e,b.hierarchy,16),i=b.item<0?0:component(e,b.item,44);
 if(h&&i){o.ready=true;o.hierarchy=read<std::array<uint8_t,16>>(h);o.item=read<std::array<uint8_t,44>>(i);o.stable=o.hierarchy==read<std::array<uint8_t,16>>(h)&&o.item==read<std::array<uint8_t,44>>(i)&&component(e,b.hierarchy,16)==h&&component(e,b.item,44)==i;}return o;
}
inline void append(std::ostringstream& o,uintptr_t state,uint32_t entity){
 const auto e=entityAt(state,entity);Binding b;{std::lock_guard<std::mutex> lock(mutex);const auto i=bindings.find(e.world);if(i!=bindings.end())b=i->second;}
 o<<",\"owner_lookup\":{\"world\":"<<hex(e.world)<<",\"hierarchy_component_id\":"<<b.hierarchy<<",\"item_component_id\":"<<b.item;
 const auto h=b.hierarchy<0?0:component(e,b.hierarchy,16),i=b.item<0?0:component(e,b.item,44);
 o<<",\"ready\":"<<((h&&i)?"true":"false");
 if(h&&i){const auto hb=read<std::array<uint8_t,16>>(h);const auto ib=read<std::array<uint8_t,44>>(i);const bool stable=hb==read<std::array<uint8_t,16>>(h)&&ib==read<std::array<uint8_t,44>>(i)&&component(e,b.hierarchy,16)==h&&component(e,b.item,44)==i;
  o<<",\"hierarchy_bytes\":"<<jsonString(asHex(hb))<<",\"item_config_bytes\":"<<jsonString(asHex(ib))<<",\"stable\":"<<(stable?"true":"false");}
 o<<'}';
}
}
