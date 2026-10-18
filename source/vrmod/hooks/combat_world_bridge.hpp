#pragma once
// Finite read-only shadow. Leases retain broker values, never native ECS memory.
// Does not change input pulses, event decisions, colliders or GPU resources.
namespace combatWorldBridge {
enum class Stage:size_t {Input,Pose,Event,Collider,Flush,Count};
inline constexpr const char* names[]{"input","pose","event","collider","flush"};
inline std::atomic<bool> enabled{false};
inline std::mutex mutex;
inline epoch::Call provider=nullptr;
inline std::array<epoch::Response,2> leases{};
inline motion::CombatWorlds identity{};
inline std::array<uint64_t,size_t(Stage::Count)> seen{},logged{};
inline uint64_t failures=0;
inline std::string fields(const epoch::Response& r){
 std::ostringstream s;s<<"\"provider\":"<<r.provider<<",\"serial\":"<<r.serial<<",\"slot\":"<<r.slot<<",\"world\":"<<hex(r.world)<<",\"registry\":"<<hex(r.registry)<<",\"owner_root\":"<<hex(r.root)<<",\"kind\":"<<r.kind<<",\"phase\":"<<r.phase<<",\"pins\":"<<r.pins<<",\"handle\":"<<r.handle<<",\"flags\":"<<r.flags;return s.str();
}
inline epoch::Response call(const epoch::Request& q){
 epoch::Response r;const auto saved=GetLastError();const auto status=provider(&q,&r);
 if(GetLastError()!=saved||status||r.status!=status||r.size!=sizeof(r)||r.version!=epoch::abiVersion)throw std::runtime_error("combat World broker ABI/status mismatch");return r;
}
inline void install(const std::filesystem::path& path){
 const auto file=path/"world-combat-epoch-shadow.txt";if(!std::filesystem::exists(file))return;
 std::lock_guard<std::mutex> lock(mutex);if(enabled||provider)throw std::runtime_error("combat World shadow already initialized");
 std::ifstream f(file);std::string magic;uint64_t id=0;f>>magic>>id;
 if(!f||magic!="EVR_COMBAT_EPOCH_SHADOW_1"||!id)throw std::runtime_error("invalid combat World shadow policy");
 std::array<epoch::Response,2> expected{};
 for(size_t i=0;i<2;++i){auto& r=expected[i];r.provider=id;f>>r.world>>r.registry>>r.root>>r.kind>>r.slot>>r.serial;
  r.status=0;r.flags=epoch::flagActive;r.phase=uint32_t(epoch::Phase::Alive);
  if(!f||!r.world||!r.registry||!r.root||!r.serial||r.slot>=8||r.kind!=(i==0?2u:0u))throw std::runtime_error("invalid ordered combat World identity");}
 f>>std::ws;identity=motion::combatWorlds(expected[0],expected[1]);
 if(!f.eof()||!identity.valid())throw std::runtime_error("invalid client/server World pair");
 const auto module=GetModuleHandleW(L"evr_diagnostic.dll");provider=module?reinterpret_cast<epoch::Call>(GetProcAddress(module,"evrWorldEpochCall")):nullptr;
 if(!provider)throw std::runtime_error("persistent World provider missing");
 try{
  for(size_t i=0;i<2;++i){auto resolved=call(epoch::request(epoch::Operation::Resolve,expected[i]));
   if(!epoch::admitsNew(resolved)||!(motion::worldStamp(resolved)==motion::worldStamp(expected[i])))throw std::runtime_error("published combat World changed before acquisition");
   leases[i]=call(epoch::request(epoch::Operation::Acquire,resolved));if(!leases[i].handle)throw std::runtime_error("combat World lease handle missing");
   logFile->event("combat_epoch_pinned",fields(leases[i])+",\"shadow_only\":true");
  }
  const auto client=call(epoch::request(epoch::Operation::Read,leases[0]));const auto server=call(epoch::request(epoch::Operation::Read,leases[1]));
  if(!motion::permitsNewCombat(identity,client,server))throw std::runtime_error("combat Worlds closed during acquisition");
  enabled=true;logFile->event("combat_epoch_shadow_enabled","\"roles\":2,\"stages\":5,\"shadow_only\":true,\"ecs_pinned\":false,\"atomic_native_permission\":false");
 }catch(...){for(auto& r:leases)if(r.handle){auto q=epoch::request(epoch::Operation::Release,r);epoch::Response out;provider(&q,&out);r={};}throw;}
}
inline std::string header(uintptr_t world,const motion::WorldStamp& expected){
 if(world!=expected.world)throw std::runtime_error("observed World does not match saved role");
 const auto bytes=read<std::array<uint8_t,16>>(world);uint64_t registry=0;std::memcpy(&registry,bytes.data(),8);
 if(registry!=expected.registry||bytes[8]!=expected.kind)throw std::runtime_error("native World header disagrees with generation role");
 std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto b:bytes)s<<std::setw(2)<<unsigned(b);return s.str();
}
inline void observe(Stage stage,uintptr_t clientWorld,uintptr_t serverWorld)noexcept{
 if(!enabled.load())return;
 try{
  const ObserverLastError preserve;std::lock_guard<std::mutex> lock(mutex);if(!enabled)return;
  const auto index=size_t(stage);if(index>=seen.size())throw std::runtime_error("invalid combat shadow stage");++seen[index];
  const auto client=call(epoch::request(epoch::Operation::Read,leases[0]));const auto server=call(epoch::request(epoch::Operation::Read,leases[1]));
  if(!motion::permitsNewCombat(identity,client,server))throw std::runtime_error("combat World no longer admits new work");
  // Only read the World supplied by this native boundary. Before/after value
  // checks are observations, not an atomic permission for subsequent ECS access.
  const auto ch=clientWorld?header(clientWorld,identity.input):std::string{};
  const auto sh=serverWorld?header(serverWorld,identity.contact):std::string{};
  const auto ca=call(epoch::request(epoch::Operation::Read,leases[0]));const auto sa=call(epoch::request(epoch::Operation::Read,leases[1]));
  if(!motion::permitsNewCombat(identity,ca,sa))throw std::runtime_error("combat World changed during shadow sample");
  if(logged[index]<32){++logged[index];std::ostringstream s;s<<"\"stage\":"<<jsonString(names[index])<<",\"sample\":"<<seen[index]<<",\"client\":{"<<fields(ca)<<"},\"server\":{"<<fields(sa)<<"},\"observed_client\":"<<hex(clientWorld)<<",\"observed_server\":"<<hex(serverWorld)<<",\"client_header\":"<<jsonString(ch)<<",\"server_header\":"<<jsonString(sh)<<",\"thread\":"<<GetCurrentThreadId()<<",\"would_permit\":true,\"shadow_only\":true";logFile->event("combat_epoch_shadow",s.str());}
 }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);if(++failures<=8)try{logFile->event("combat_epoch_shadow_failure","\"stage\":"+jsonString(size_t(stage)<seen.size()?names[size_t(stage)]:"unknown")+",\"reason\":"+jsonString(e.what()));}catch(...){} }
}
// Only after successful stop/drain of all relevant native hooks. On exceptional
// cleanup retain values conservatively; the publisher then reports failure.
inline void finish(){
 std::lock_guard<std::mutex> lock(mutex);if(!enabled.exchange(false))return;
 for(auto& r:leases)if(r.handle){const auto out=call(epoch::request(epoch::Operation::Release,r));logFile->event("combat_epoch_released",fields(out)+",\"released_handle\":"+std::to_string(r.handle));r={};}
 std::ostringstream s;s<<"\"failures\":"<<failures<<",\"shadow_only\":true,\"leases_remaining\":0,\"stages\":{";
 bool complete=failures==0;for(size_t i=0;i<seen.size();++i){if(i)s<<',';s<<jsonString(names[i])<<':'<<seen[i];complete&=seen[i]>0;}s<<'}';logFile->event("combat_epoch_shadow_complete",s.str());
 if(!complete)throw std::runtime_error("combat World role shadow incomplete");
}
}
