#pragma once
// Finite, read-only destruction evidence for already owned tracked Impacts.
// No ledger entry is retired here. Record addresses are NOT generation IDs.
namespace contactLifetimeObserver {
inline bool requested=false,enabled=false;
// Explicit terminal diagnostic only. Learn the Actor owner boundary from the
// actual native entity-destruction callback, never by polling saved pointers.
inline bool actorOwnerRequested=false;
inline std::atomic<unsigned> actorOwnerBegins{0},actorOwnerEnds{0},actorOwnerCalls{0},actorAbsentBegins{0},actorAbsentEnds{0};
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},failures{0},destroyed{0},watched{0};
struct Watch {motion::ColliderKey key{};motion::EventAuthority authority{};unsigned firstSample=0,lastSample=0,uses=0;bool used=false,destroying=false,ended=false;motion::ContactLifetimeToken token{};};
inline std::mutex mutex;inline std::array<Watch,8> watches{};
inline motion::ContactLifetimePool<8> lifetimes;
using Destroy=uint8_t(__fastcall*)(void*,uint32_t,uint8_t,uint8_t);
inline Destroy original=nullptr;
constexpr uintptr_t rva=0x996eb0;
constexpr const char* pattern="48 89 5C 24 18 48 89 74 24 20 89 54 24 10 55 57 41 55 41";
struct Presence {bool present=false;uintptr_t record=0;uint64_t capacity=0;uint32_t count=0,counter=0;uint8_t worldKind=0;};
inline Presence presence(uintptr_t world,uint32_t entity){
 const auto map=world+0x148;
 const auto header=read<std::array<uintptr_t,11>>(map);
 const auto bitmap=header[1],capacity=header[2],keys=header[5],values=header[8];uint32_t count=0;std::memcpy(&count,reinterpret_cast<const uint8_t*>(header.data())+0x54,4);
 if(!world||!entity||!bitmap||!keys||!values||!capacity||capacity>0x100000||(capacity&(capacity-1))||count>capacity)throw std::runtime_error("lifetime presence map bounds");
 Presence out;out.capacity=capacity;out.count=count;out.counter=read<uint32_t>(world+0x4c0);out.worldKind=read<uint8_t>(world+8);
 auto bucket=uint64_t(nativeImpactComponents::hashEntity(entity))&(capacity-1);bool stopped=false;
 for(uint64_t n=0;n<capacity;++n){
  const auto word=read<uint64_t>(bitmap+8*(bucket/64));const auto bit=1ull<<(bucket%64);
  if(!(word&bit)){if(word!=read<uint64_t>(bitmap+8*(bucket/64)))throw std::runtime_error("lifetime absence changed");stopped=true;break;}
  const auto key=read<uint32_t>(keys+bucket*4);
  if(key==entity){out.record=read<uintptr_t>(values+bucket*8);out.present=out.record!=0;
   if(key!=read<uint32_t>(keys+bucket*4)||out.record!=read<uintptr_t>(values+bucket*8)||word!=read<uint64_t>(bitmap+8*(bucket/64)))throw std::runtime_error("lifetime presence changed");stopped=true;break;}
  bucket=(bucket+1)&(capacity-1);
 }
 if(!stopped||header!=read<std::array<uintptr_t,11>>(map))throw std::runtime_error("lifetime map unstable");return out;
}
inline void track(const motion::ColliderKey& key,unsigned sample){
 if(!requested||!accepting.load())return;
 try{
  std::lock_guard<std::mutex> lock(mutex);
  for(auto& w:watches)if(w.used&&w.key.world==key.world&&w.key.entity==key.entity){
   if(!(w.key==key)||w.destroying||w.ended)throw std::runtime_error("owned Impact identity reused or updated during/after destruction");w.lastSample=sample;++w.uses;return;
  }
  const auto authority=motionColliderChannel::authorized();
  if(!authority.matches(key.world,authority.action.entity,authority.action.item,authority.action.trigger,key.event,key.epoch))throw std::runtime_error("lifetime watch lacks authored Impact authority");
  const auto state=presence(key.world,key.entity);const auto native=nativeImpactComponents::entityAtWorld(key.world,key.entity);
  if(!state.present||state.record!=native.record||read<uint32_t>(state.record+0x10)!=key.entity)throw std::runtime_error("lifetime initial entity record differs");
  combatEpochGate::bindActor(key.epoch,authority.action.entity);
  const auto token=lifetimes.acquire(authority,key);
  if(token){auto& w=watches[token.slot];if(w.used)throw std::runtime_error("lifetime pool/watch storage mismatch");w={key,authority,sample,sample,1,true,false,false,token};++watched;
   std::ostringstream o;o<<"\"watch_slot\":"<<token.slot<<",\"watch_serial\":"<<token.serial<<",\"world\":"<<hex(key.world)<<",\"entity\":"<<key.entity<<",\"event_bytes\":"<<jsonString(nativeImpactComponents::asHex(key.event))<<",\"record\":"<<hex(state.record)<<",\"counter\":"<<state.counter<<",\"world_kind\":"<<unsigned(state.worldKind)<<",\"ready_sample\":"<<sample<<",\"actor\":"<<authority.action.entity<<",\"sequence\":"<<authority.action.sequence<<",\"item\":"<<authority.action.item<<",\"trigger\":"<<authority.action.trigger<<",\"milliseconds\":"<<GetTickCount64()<<",\"thread\":"<<GetCurrentThreadId()<<",\"read_only\":true";
   o<<combatEpochGate::stampFields(key.epoch);logFile->event("contact_lifetime_watch",o.str());return;}
  throw std::runtime_error("finite lifetime evidence storage full");
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("contact_lifetime_failure","\"reason\":"+jsonString(e.what()));}
}
struct ActorOwnerEvidence {
 Watch watch{};Presence before{};bool matched=false,beforeValid=false,activeActor=false,canHoldValues=false;uint64_t ownerCall=0;motion::EventActionKey nativeAction{};
 std::string fields;
};
inline bool (*actorBoundaryObserver)(const ActorOwnerEvidence&)=nullptr;
inline void (*actorReturnObserver)(const ActorOwnerEvidence&)=nullptr;
inline ActorOwnerEvidence actorOwnerBegin(uintptr_t world,uint32_t entity,uintptr_t caller,uint8_t reason,uint8_t skipNotify){
 ActorOwnerEvidence out;if(!actorOwnerRequested)return out;
 bool candidate=false;{std::lock_guard<std::mutex> lock(mutex);for(const auto& w:watches)if(w.used&&w.key.world==world&&w.authority.action.entity==entity){candidate=true;break;}}
 if(!candidate)return out; // No broker request or native read for foreign entities.
 const auto epoch=combatEpochGate::serverStamp(world);
 {std::lock_guard<std::mutex> lock(mutex);
  for(const auto& w:watches)if(w.used&&w.key.world==world&&w.authority.action.entity==entity&&w.key.epoch==epoch){
   if(out.matched&&!(out.watch.authority.action==w.authority.action))throw std::runtime_error("ambiguous finite Actor owner watch");
   out.watch=w;out.matched=true;
  }
 }
 if(!out.matched)return out;
 out.before=presence(world,entity);uintptr_t actor=0;uint32_t sequence=0,item=0;int64_t trigger=0;bool activeActor=false;
 if(out.before.present){
  const auto native=nativeImpactComponents::entityAtWorld(world,entity);
  out.beforeValid=out.before.record==native.record&&read<uint32_t>(native.record+0x10)==entity;
  if(!out.beforeValid)throw std::runtime_error("Actor destructor entry has a different entity record");
  nativeImpactComponents::Binding binding;
  {std::lock_guard<std::mutex> lock(nativeImpactComponents::mutex);const auto i=nativeImpactComponents::bindings.find(world);if(i==nativeImpactComponents::bindings.end())throw std::runtime_error("Actor owner binding missing");binding=i->second;}
  if(binding.actor<0||binding.actorStride<3104)throw std::runtime_error("Actor owner component layout unlearned");
  actor=nativeImpactComponents::component(native,unsigned(binding.actor),binding.actorStride);
  if(!actor)throw std::runtime_error("Actor destructor entry has no Actor component");
  const auto bytes=read<std::array<uint8_t,3104>>(actor);
  std::memcpy(&sequence,bytes.data(),4);std::memcpy(&trigger,bytes.data()+8,8);std::memcpy(&item,bytes.data()+0x38,4);activeActor=(bytes[0x1b1]&1)!=0;out.activeActor=activeActor;out.nativeAction.world=world;out.nativeAction.entity=entity;out.nativeAction.sequence=sequence;out.nativeAction.item=item;out.nativeAction.trigger=trigger;out.nativeAction.call=bytes[20];out.nativeAction.sub=bytes[0x170];out.nativeAction.epoch=epoch;
 }
 out.ownerCall=++actorOwnerCalls;
 const auto& w=out.watch;std::ostringstream o;
 o<<"\"owner_call\":"<<out.ownerCall<<",\"watch_slot\":"<<w.token.slot<<",\"watch_serial\":"<<w.token.serial<<",\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"impact_entity\":"<<w.key.entity
 <<",\"caller\":"<<hex(caller)<<",\"reason\":"<<unsigned(reason)<<",\"skip_notify\":"<<unsigned(skipNotify)<<",\"record\":"<<hex(out.before.record)<<",\"actor_component\":"<<hex(actor)
 <<",\"present\":"<<(out.before.present?"true":"false")<<",\"sequence\":"<<sequence<<",\"item\":"<<item<<",\"trigger\":"<<trigger<<",\"active\":"<<(activeActor?"true":"false")
 <<",\"counter\":"<<out.before.counter<<",\"tick\":"<<read<uint32_t>(world+0x504)<<",\"world_kind\":"<<unsigned(out.before.worldKind)<<",\"thread\":"<<GetCurrentThreadId()
 <<",\"reads_native_callback_owner\":"<<(out.before.present?"true":"false")<<",\"read_only\":true"<<combatEpochGate::stampFields(epoch);
 out.fields=o.str();if(out.before.present)++actorOwnerBegins;else ++actorAbsentBegins;logFile->event(out.before.present?"contact_actor_owner_destroy_begin":"contact_actor_absent_destroy_begin",out.fields);return out;
}
inline void actorOwnerEnd(const ActorOwnerEvidence& e,uintptr_t world,uint32_t entity,uint8_t result){
 if(!e.matched)return;
 // Only the still-owned World map is inspected after the original call. The
 // Actor/record saved above is numeric evidence and is never dereferenced.
 const auto after=presence(world,entity);
 const bool complete=(e.beforeValid?result!=0:result==0)&&!after.present&&e.watch.key.epoch==combatEpochGate::serverStamp(world);
 if(!complete)throw std::runtime_error("Actor owner native destruction did not erase exact entity");
 if(e.beforeValid)++actorOwnerEnds;else ++actorAbsentEnds;logFile->event(e.beforeValid?"contact_actor_owner_destroy_end":"contact_actor_absent_destroy_end",e.fields+",\"native_result\":"+std::to_string(result)+",\"after_present\":false,\"after_record\":"+hex(after.record)+",\"after_counter\":"+std::to_string(after.counter)+",\"arguments_unchanged\":true,\"return_unchanged\":true,\"reads_destroyed_actor\":false,\"retires_ledger_entries\":false");
}
__declspec(noinline) uint8_t __fastcall destroy(void* worldArg,uint32_t entity,uint8_t reason,uint8_t skipNotify){
 const auto world=reinterpret_cast<uintptr_t>(worldArg),caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 struct Callback{Callback(){++callbacks;}~Callback(){--callbacks;}} callback;
 int index=-1;Watch watch;Presence before;bool beforeValid=false;ActorOwnerEvidence actorEvidence;
 {const ObserverLastError preserve;
  if(accepting.load())try{
   actorEvidence=actorOwnerBegin(world,entity,caller,reason,skipNotify);
   // Actual native deletion entry, before original can destroy the local Actor.
   // Stop new admission only; old owned suppression lives until server Ended.
   if(actorEvidence.beforeValid){
    combatEpochGate::stopActor(actorEvidence.watch.key.epoch,entity,actorEvidence.ownerCall);
    if(actorBoundaryObserver)actorEvidence.canHoldValues=actorBoundaryObserver(actorEvidence);
   }
   {std::lock_guard<std::mutex> lock(mutex);for(size_t i=0;i<watches.size();++i){auto& w=watches[i];if(w.used&&w.key.world==world&&w.key.entity==entity&&(!combatEpochGate::requested||w.key.epoch==combatEpochGate::serverStamp(world))){if(w.destroying||w.ended)throw std::runtime_error("duplicate watched destruction");if(!lifetimes.beginDestroy(w.token,w.key))throw std::runtime_error("stale lifetime destroy entry");w.destroying=true;watch=w;index=int(i);break;}}}
   if(index>=0){before=presence(world,entity);beforeValid=before.present&&read<uint32_t>(before.record+0x10)==entity;
    logFile->event("contact_lifetime_destroy_begin","\"watch_slot\":"+std::to_string(watch.token.slot)+",\"watch_serial\":"+std::to_string(watch.token.serial)+",\"world\":"+hex(world)+",\"entity\":"+std::to_string(entity)+",\"caller\":"+hex(caller)+",\"reason\":"+std::to_string(reason)+",\"skip_notify\":"+std::to_string(skipNotify)+",\"record\":"+hex(before.record)+",\"present\":"+(beforeValid?"true":"false")+",\"counter\":"+std::to_string(before.counter)+",\"last_ready_sample\":"+std::to_string(watch.lastSample)+",\"uses\":"+std::to_string(watch.uses)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"milliseconds\":"+std::to_string(GetTickCount64())+",\"read_only\":true");}
  }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("contact_lifetime_failure","\"reason\":"+jsonString(e.what()));}
 }
 // Preserve the native call, original arguments, and result even if evidence fails.
 const auto result=original(worldArg,entity,reason,skipNotify);const ObserverLastError preserve;
 try{actorOwnerEnd(actorEvidence,world,entity,result);if(actorEvidence.beforeValid&&actorReturnObserver)actorReturnObserver(actorEvidence);}catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("contact_lifetime_failure","\"reason\":"+jsonString(e.what()));}
 if(index>=0)try{
  const auto after=presence(world,entity);const bool complete=beforeValid&&result&&!after.present;
  {std::lock_guard<std::mutex> lock(mutex);auto& w=watches[size_t(index)];if(!w.used||!(w.token==watch.token)||!(w.key==watch.key)||!lifetimes.finishDestroy(watch.token,watch.key,complete))throw std::runtime_error("stale lifetime destroy completion");w.destroying=false;w.ended=complete;}
  if(complete)++destroyed;else ++failures;
  logFile->event("contact_lifetime_destroy_end","\"watch_slot\":"+std::to_string(watch.token.slot)+",\"watch_serial\":"+std::to_string(watch.token.serial)+",\"world\":"+hex(world)+",\"entity\":"+std::to_string(entity)+",\"native_result\":"+std::to_string(result)+",\"present\":"+(after.present?"true":"false")+",\"record\":"+hex(after.record)+",\"counter\":"+std::to_string(after.counter)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"milliseconds\":"+std::to_string(GetTickCount64())+",\"return_unchanged\":true,\"arguments_unchanged\":true,\"read_only\":true");
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("contact_lifetime_failure","\"reason\":"+jsonString(e.what()));}
 return result;
}
inline void install(){
 const auto bytes=parsePattern(pattern);
 if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("native entity destruction signature differs");
 for(const auto& pair:std::array<std::pair<uintptr_t,uintptr_t>,2>{{{0x8b9d4d,0x8d08e0},{0x8b9d71,0x8fd7b0}}})if(read<uint8_t>(baseAddress+pair.first)!=0xe8||uintptr_t(int64_t(pair.first+5)+read<int32_t>(baseAddress+pair.first+1))!=pair.second)throw std::runtime_error("native entity destructor/map erasure calls differ");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&destroy),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("entity lifetime hook failed");
 requested=enabled=true;accepting=true;logFile->event("contact_lifetime_enabled","\"rva\":\"0x996eb0\",\"pattern\":"+jsonString(pattern)+",\"read_only\":true,\"retires_ledger_entries\":false");
}
inline void disable(){
 accepting=false;if(!enabled)return;
 if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("entity lifetime disable failed");enabled=false;
 const auto deadline=GetTickCount64()+1000;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 const auto bytes=parsePattern(pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
 logFile->event("contact_lifetime_removed","\"watched\":"+std::to_string(watched.load())+",\"actor_owner_begins\":"+std::to_string(actorOwnerBegins.load())+",\"actor_owner_ends\":"+std::to_string(actorOwnerEnds.load())+",\"actor_absent_begins\":"+std::to_string(actorAbsentBegins.load())+",\"actor_absent_ends\":"+std::to_string(actorAbsentEnds.load())+",\"destroyed\":"+std::to_string(destroyed.load())+",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load())+",\"restored\":"+(restored?"true":"false")+",\"read_only\":true,\"retires_ledger_entries\":false");
 if(!restored||callbacks.load()||failures.load())throw std::runtime_error("entity lifetime evidence failed; hook restored if possible");
}
}
