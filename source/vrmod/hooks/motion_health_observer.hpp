#pragma once
// Included inside motionHitObserver. Read-only, bounded diagnostic joining the
// local native Impact instance to conclusion -> target Health -> attribute delta.
// No extra native lookup is called and no candidate/damage/attribute is written.
namespace health {
using Commit=uintptr_t(__fastcall*)(void*,void*,void*);
using Delta=uintptr_t(__fastcall*)(void*,void*,int32_t,uint32_t);
using Lookup=void*(__fastcall*)(void*,uint32_t,void*);
inline Commit originalCommit=nullptr;inline Delta originalDelta=nullptr;inline Lookup originalLookup=nullptr;
inline std::atomic<unsigned> commits{0},deltas{0},unqualified{0};
inline std::mutex mutex;
inline evr::motion::ImpactWitnessLedger<Owner,16> witnesses{500};
inline std::atomic<unsigned> recorded{0};
inline void remember(const Owner& o,uint32_t id){
 std::lock_guard<std::mutex> lock(mutex);
 const auto same=[](const Owner& a,const Owner& b){return a.world==b.world&&a.root==b.root&&a.entity==b.entity&&a.item==b.item&&a.event==b.event&&a.impact==b.impact;};
 const auto result=witnesses.remember({o.world,o.root,id},o,GetTickCount64(),same);
 if(result!=evr::motion::WitnessRecord::Stored)throw std::runtime_error("Impact witness collision, capacity or clock failure");
 ++recorded;
}
struct Scope{Owner owner;uint32_t target=0,impact=0,serial=0;uintptr_t registry=0;std::array<uint8_t,320> candidate{};};
inline thread_local Scope current;
struct Sample{uintptr_t container=0,registry=0,slot=0;uint32_t key=0,index=0,count=0;int component=-1;int32_t requested=0,before=0;std::array<uint8_t,20> header{};std::array<uint8_t,32> metadata{};std::array<uint8_t,64> componentRecord{};std::string attributeName,componentName;unsigned componentBytes=0;bool schemaValidated=false,resolved=false;};
inline thread_local Sample* pending=nullptr;
__declspec(noinline) uintptr_t __fastcall commit(void* query,void* candidate,void* context){
 Guard guard;Scope observed;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 {const ObserverLastError preserve;if(accepting.load()&&commits.load()<64&&(caller==0x3ee88e||caller==0x3eeb63||caller==0x3eed85||caller==0x3eed85))try{
  const auto p=reinterpret_cast<uintptr_t>(candidate),state=read<uintptr_t>(reinterpret_cast<uintptr_t>(context));
  const auto world=state?read<uintptr_t>(state+0x30):0;
  const auto root=read<uint32_t>(p+0x90),instance=read<uint32_t>(p+0xd4);
  std::optional<evr::motion::ImpactWitnessLedger<Owner,16>::Witness> witness;uint64_t now=0;
  {std::lock_guard<std::mutex> lock(mutex);now=GetTickCount64();witness=witnesses.find({world,root,instance},now);}
  if(witness){const auto& w=*witness;observed.owner=w.value;observed.impact=instance;observed.target=read<uint32_t>(p+0x94);observed.registry=read<uintptr_t>(reinterpret_cast<uintptr_t>(query)+0x18);observed.candidate=read<std::array<uint8_t,320>>(p);
   if(!observed.target||!observed.registry)throw std::runtime_error("owned conclusion has no target/attribute registry");
   observed.serial=++commits;logFile->event("motion_owned_hit_commit",labels(observed.owner)+",\"commit\":"+std::to_string(observed.serial)+",\"target\":"+std::to_string(observed.target)+",\"instance_id\":"+std::to_string(observed.impact)+",\"caller\":"+hex(caller)+",\"owner_age_ms\":"+std::to_string(now-w.last)+",\"candidate_bytes\":"+jsonString(nativeImpactComponents::asHex(observed.candidate))+",\"world_matches\":true,\"read_only\":true");
  }
 }catch(const std::exception& e){fail(e);observed={};}}
 const auto previous=current;current=observed;struct Restore{Scope old;~Restore(){current=old;}}restore{previous};
 const auto result=originalCommit(query,candidate,context);const ObserverLastError preserve;
 if(observed.serial)try{logFile->event("motion_owned_hit_commit_end","\"commit\":"+std::to_string(observed.serial)+",\"candidate_unchanged\":"+(read<std::array<uint8_t,320>>(reinterpret_cast<uintptr_t>(candidate))==observed.candidate?"true":"false")+",\"read_only\":true");}catch(const std::exception& e){fail(e);}return result;
}
__declspec(noinline) uintptr_t __fastcall delta(void* container,void* registry,int32_t amount,uint32_t key){
 Guard guard;Sample sample;bool observe=false;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 {const ObserverLastError preserve;observe=accepting.load()&&current.serial&&caller==0x3e9447&&key==0x8eb84995&&reinterpret_cast<uintptr_t>(registry)==current.registry&&deltas.load()<64;
  if(observe){sample.container=reinterpret_cast<uintptr_t>(container);sample.registry=reinterpret_cast<uintptr_t>(registry);sample.requested=amount;sample.key=key;}}
 auto* previous=pending;pending=observe?&sample:nullptr;struct Restore{Sample* old;~Restore(){pending=old;}}restore{previous};
 const auto result=originalDelta(container,registry,amount,key);const ObserverLastError preserve;
 if(observe)try{
  int32_t after=0;bool stable=false;if(sample.resolved){stable=read<std::array<uint8_t,20>>(sample.container)==sample.header;if(!stable)throw std::runtime_error("target Health header changed during delta");after=read<int32_t>(sample.slot);}
  const bool arithmetic=sample.resolved&&uint32_t(after)==uint32_t(sample.before)+uint32_t(amount);
  logFile->event("motion_owned_health_delta",labels(current.owner)+",\"sample\":"+std::to_string(++deltas)+",\"commit\":"+std::to_string(current.serial)+",\"target\":"+std::to_string(current.target)+",\"instance_id\":"+std::to_string(current.impact)+",\"container\":"+hex(sample.container)+",\"slot\":"+hex(sample.slot)+",\"component_id\":"+std::to_string(sample.component)+",\"key\":"+std::to_string(key)+",\"index\":"+std::to_string(sample.index)+",\"count\":"+std::to_string(sample.count)+",\"before\":"+std::to_string(sample.before)+",\"after\":"+std::to_string(after)+",\"requested_delta\":"+std::to_string(amount)+",\"resolved\":"+(sample.resolved?"true":"false")+",\"header_stable\":"+(stable?"true":"false")+",\"native_delta_exact\":"+(arithmetic?"true":"false")+",\"schema_validated\":"+(sample.schemaValidated?"true":"false")+",\"attribute_metadata\":"+jsonString(nativeImpactComponents::asHex(sample.metadata))+",\"header_bytes\":"+jsonString(nativeImpactComponents::asHex(sample.header))+",\"attribute_name\":"+jsonString(sample.attributeName)+",\"component_name\":"+jsonString(sample.componentName)+",\"component_type_bytes\":"+jsonString(nativeImpactComponents::asHex(sample.componentRecord))+",\"component_bytes\":"+std::to_string(sample.componentBytes)+",\"caller\":\"0x3e9447\",\"read_only\":true");
 }catch(const std::exception& e){fail(e);}return result;
}
__declspec(noinline) void* __fastcall lookup(void* output,uint32_t key,void* registry){
 Guard guard;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 auto* result=originalLookup(output,key,registry);const ObserverLastError preserve;
 if(accepting.load()&&pending&&current.serial&&caller==0x3e874d&&key==pending->key&&reinterpret_cast<uintptr_t>(registry)==pending->registry)try{
  auto& s=*pending;const auto p=reinterpret_cast<uintptr_t>(output);if(read<uint8_t>(p))return result;
  const auto meta=read<uintptr_t>(p+8);if(!meta||!s.container)return result;
  s.index=read<uint16_t>(meta+12);s.header=read<std::array<uint8_t,20>>(s.container);uint32_t offset=0;std::memcpy(&offset,s.header.data()+8,4);std::memcpy(&s.count,s.header.data()+12,4);
  if(!(s.header[16]&1))return result;
  // Health is reflection-verified as68bytes, inline int32 data[8] at+36.
  // Fail closed instead of trusting the native registry's unchecked index.
  if(s.count>8||s.index>=s.count||offset!=36)throw std::runtime_error("target Health attribute bounds differ from reflection");
  const auto entity=nativeImpactComponents::entityAtWorld(current.owner.world,current.target);s.component=nativeImpactComponents::identify(entity,s.container,68);
  const auto type=nativeImpactComponents::componentType(current.owner.world,unsigned(s.component));s.componentName=type.qualifiedName;s.componentBytes=type.bytes;s.componentRecord=type.record;
  s.metadata=read<std::array<uint8_t,32>>(meta);uintptr_t name=0,length=0;std::memcpy(&name,s.metadata.data()+16,8);std::memcpy(&length,s.metadata.data()+24,8);
  if(!name||length!=6)throw std::runtime_error("native Health metadata name bounds");
  const auto nameBytes=read<std::array<char,6>>(name);s.attributeName.assign(nameBytes.begin(),nameBytes.end());
  const evr::motion::HealthSchema schema{s.header,s.metadata,s.componentName,s.attributeName,unsigned(s.component),s.componentBytes};
  s.schemaValidated=evr::motion::verifiedHealthSchema(schema)&&s.metadata==read<std::array<uint8_t,32>>(meta)&&nameBytes==read<std::array<char,6>>(name);
  if(!s.schemaValidated)throw std::runtime_error("target component/attribute is not runtime-verified Health");
  s.slot=s.container+offset+uintptr_t(s.index)*4;s.before=read<int32_t>(s.slot);s.resolved=true;
 }catch(const std::exception& e){fail(e);}return result;
}
}
