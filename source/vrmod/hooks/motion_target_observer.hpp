#pragma once
// Explicit read-only target inventory. Never selects, moves toward, or attacks
// candidates. Type IDs come from the exact native conclusion task query.
namespace motionTargets {
using Prepare=uintptr_t(__fastcall*)(void*,void*,uint32_t);
inline Prepare original=nullptr;inline bool enabled=false;
inline std::atomic<bool> accepting{false};inline std::atomic<unsigned> callbacks{0},samples{0},failures{0};
constexpr uintptr_t rva=0x9bab60,callerRva=0x3ed931;
constexpr const char* pattern="48 89 5c 24 10 55 56 57 41 54 41 55 41 56 41 57 48 83 ec 60";
struct Candidate{double distance=0;std::string fields;};
struct Scan{uintptr_t bitmap=0,keys=0,values=0;uint64_t capacity=0,cursor=0;unsigned health=0,current=0,found=0,skipped=0;nativeAttributes::Attribute healthAttribute;nativeImpactComponents::ComponentType healthType;uint64_t started=0;std::vector<Candidate> nearest;bool complete=false;};
inline std::mutex mutex;inline std::map<uintptr_t,Scan> scans;
struct Guard{Guard(){++callbacks;}~Guard(){--callbacks;}};
inline void capture(void* context,void* query,uint32_t bytes){
 if(bytes!=136)throw std::runtime_error("target query size differs");
 const auto slots=read<std::array<uintptr_t,17>>(reinterpret_cast<uintptr_t>(query));
 if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[11])throw std::runtime_error("target query Health lookup absent");
 const auto state=read<uintptr_t>(slots[0]);const auto world=read<uintptr_t>(state+0x30);if(!world)return;
 rootCamera::Sample root;{std::lock_guard<std::mutex> l(rootCamera::mutex);root=rootCamera::latest;}
 if(!root.entity||GetTickCount64()-root.milliseconds>1000)return;
 const auto actor=actionSequenceObserver::forWorld(world,root.entity);if(!actor.entity||GetTickCount64()-actor.milliseconds>1000)return;
 nativeImpactComponents::Binding binding;{std::lock_guard<std::mutex> l(nativeImpactComponents::mutex);const auto i=nativeImpactComponents::bindings.find(world);if(i==nativeImpactComponents::bindings.end()||i->second.current<0)return;binding=i->second;}
 const unsigned health=read<uint16_t>(slots[11]);if(health>=nativeImpactComponents::componentCapacity)throw std::runtime_error("target Health type outside bounds");
 std::lock_guard<std::mutex> lock(mutex);if(!scans.count(world)&&scans.size()>=4)return;auto& scan=scans[world];if(scan.complete)return;
 const auto map=world+0x148,bitmap=read<uintptr_t>(map+8),keys=read<uintptr_t>(map+0x28),values=read<uintptr_t>(map+0x40);const auto capacity=read<uint64_t>(map+0x10);
 if(!bitmap||!keys||!values||!capacity||capacity>0x100000||(capacity&(capacity-1)))throw std::runtime_error("target World map bounds");
 if(!scan.capacity){
  scan.healthType=nativeImpactComponents::componentType(world,health);scan.healthAttribute=nativeAttributes::lookup(slots[3],0x8eb84995);
  if(scan.healthType.qualifiedName!="keen::ecs::Health"||scan.healthType.bytes!=68||scan.healthAttribute.name!="Health")throw std::runtime_error("target Health runtime identity differs");
  scan.bitmap=bitmap;scan.keys=keys;scan.values=values;scan.capacity=capacity;scan.health=health;scan.current=unsigned(binding.current);scan.started=GetTickCount64();
  logFile->event("motion_target_health_schema","\"world\":"+hex(world)+",\"registry\":"+hex(slots[3])+",\"component_id\":"+std::to_string(health)+",\"component_bytes\":"+std::to_string(scan.healthType.bytes)+",\"component_name\":"+jsonString(scan.healthType.qualifiedName)+",\"component_type_bytes\":"+jsonString(nativeImpactComponents::asHex(scan.healthType.record))+",\"attribute_name\":"+jsonString(scan.healthAttribute.name)+",\"attribute_metadata\":"+jsonString(nativeImpactComponents::asHex(scan.healthAttribute.bytes))+",\"read_only\":true");
  logFile->event("motion_target_registry","\"world\":"+hex(world)+",\"local_entity\":"+std::to_string(root.entity)+",\"health_type\":"+std::to_string(health)+",\"current_type\":"+std::to_string(binding.current)+",\"started_ms\":"+std::to_string(scan.started)+",\"capacity\":"+std::to_string(capacity)+",\"query_bytes\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,136>>(reinterpret_cast<uintptr_t>(query))))+",\"caller\":\"0x3ed931\",\"read_only\":true");
 }
 if(scan.bitmap!=bitmap||scan.keys!=keys||scan.values!=values||scan.capacity!=capacity||scan.health!=health||scan.current!=unsigned(binding.current)){scan.complete=true;logFile->event("motion_target_scan_invalidated","\"world\":"+hex(world)+",\"reason\":\"map or learned types changed\"");return;}
 const auto end=std::min(capacity,scan.cursor+64);const auto occupied=read<uint64_t>(bitmap+(scan.cursor/64)*8);
 for(auto at=scan.cursor;at<end;++at){
  if(!(occupied&(1ull<<(at%64))))continue;
  try{
   const auto id=read<uint32_t>(keys+at*4);if(!id)continue;
   const auto e=nativeImpactComponents::entityAtWorld(world,id);if(e.record!=read<uintptr_t>(values+at*8))throw std::runtime_error("candidate map changed");
   const auto hp=nativeImpactComponents::component(e,health,68),pose=nativeImpactComponents::component(e,unsigned(binding.current),56);if(!hp||!pose)continue;
   const auto h=read<std::array<uint8_t,68>>(hp);const auto t=read<game::RootTransform>(pose);uint32_t offset=0,count=0;std::memcpy(&offset,h.data()+8,4);std::memcpy(&count,h.data()+12,4);
   if(!(h[16]&1)||offset!=36||!count||count>8)continue;
   std::array<uint8_t,20> prefix{};std::memcpy(prefix.data(),h.data(),20);
   const evr::motion::HealthSchema schema{prefix,scan.healthAttribute.bytes,scan.healthType.qualifiedName,scan.healthAttribute.name,health,scan.healthType.bytes};
   if(!evr::motion::verifiedHealthSchema(schema))continue;
   uint16_t healthIndex=0;std::memcpy(&healthIndex,scan.healthAttribute.bytes.data()+12,2);int32_t healthValue=0;std::memcpy(&healthValue,h.data()+offset+healthIndex*4,4);

   const Vec3 delta{float(relativeQ32(t.position.x,root.stable.position.x)),float(relativeQ32(t.position.y,root.stable.position.y)),float(relativeQ32(t.position.z,root.stable.position.z))};if(!finite(delta))continue;
   const double distance=std::sqrt(double(delta.x)*delta.x+double(delta.y)*delta.y+double(delta.z)*delta.z);if(distance>250)continue;
   auto farthest=std::max_element(scan.nearest.begin(),scan.nearest.end(),[](const auto& a,const auto& b){return a.distance<b.distance;});
   if(scan.nearest.size()==32&&distance>=farthest->distance)continue;
   // All source bytes must still match. Concurrently changing rows are skipped,
   // not turned into guessed target geometry or a false observation failure.
   const auto again=read<game::RootTransform>(pose);if(h!=read<std::array<uint8_t,68>>(hp)||std::memcmp(&t,&again,56))continue;
   std::array<uint8_t,56> transformBytes{};std::memcpy(transformBytes.data(),&t,56);
   if(read<uint32_t>(keys+at*4)!=id||read<uintptr_t>(values+at*8)!=e.record)continue;
   if(read<uintptr_t>(e.record+0x18)!=e.layout||read<uintptr_t>(e.record+0x20)!=e.data||read<uint32_t>(e.record+0x30)!=e.row){++scan.skipped;continue;}
   std::ostringstream o;o<<std::setprecision(12)<<"\"world\":"<<hex(world)<<",\"entity\":"<<id<<",\"local_entity\":"<<root.entity<<",\"health_type\":"<<health<<",\"current_type\":"<<binding.current<<",\"health_pointer\":"<<hex(hp)<<",\"transform_pointer\":"<<hex(pose)<<",\"health_bytes\":"<<jsonString(nativeImpactComponents::asHex(h))<<",\"transform_bytes\":"<<jsonString(nativeImpactComponents::asHex(transformBytes))<<",\"root_q32\":["<<root.stable.position.x<<','<<root.stable.position.y<<','<<root.stable.position.z<<"],\"relative_m\":["<<delta.x<<','<<delta.y<<','<<delta.z<<"],\"distance_m\":"<<distance<<",\"observed_ms\":"<<GetTickCount64()<<",\"has_actor\":"<<((binding.actor>=0&&nativeImpactComponents::component(e,unsigned(binding.actor),binding.actorStride))?"true":"false")<<",\"health_index_unresolved\":false,\"health_index\":"<<healthIndex<<",\"health_value\":"<<healthValue<<",\"health_schema_verified\":true,\"hostility_unclassified\":true,\"stable_read\":true,\"read_only\":true";
   if(scan.nearest.size()==32)*farthest={distance,o.str()};else scan.nearest.push_back({distance,o.str()});++scan.found;
  }catch(const std::exception&){++scan.skipped;}
 }
 scan.cursor=end;if(end==capacity){scan.complete=true;std::sort(scan.nearest.begin(),scan.nearest.end(),[](const auto& a,const auto& b){return a.distance<b.distance;});for(const auto& c:scan.nearest)logFile->event("motion_target_candidate","\"sample\":"+std::to_string(++samples)+","+c.fields);logFile->event("motion_target_scan_complete","\"world\":"+hex(world)+",\"buckets\":"+std::to_string(capacity)+",\"candidates\":"+std::to_string(scan.nearest.size())+",\"nearest_limit\":32,\"selection_updates\":"+std::to_string(scan.found)+",\"finished_ms\":"+std::to_string(GetTickCount64())+",\"observation_span_ms\":"+std::to_string(GetTickCount64()-scan.started)+",\"skipped_races\":"+std::to_string(scan.skipped)+",\"read_only\":true");}
}
__declspec(noinline) uintptr_t __fastcall prepare(void* context,void* query,uint32_t bytes){
 Guard guard;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;const auto result=original(context,query,bytes);const ObserverLastError preserve;
 if(accepting.load()&&caller==callerRva)try{capture(context,query,bytes);}catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("motion_target_failure","\"reason\":"+jsonString(e.what()));}return result;
}
inline void install(){const auto p=parsePattern(pattern);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("target query signature differs");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&prepare),reinterpret_cast<void**>(&original))!=MH_OK)throw std::runtime_error("target query hook create failed");
 if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("target query hook enable failed");enabled=true;accepting=true;logFile->event("motion_target_enabled","\"read_only\":true,\"maximum_candidates_per_world\":32,\"maximum_distance_m\":250,\"no_input_fixture\":true,\"health_schema_required\":true");}
inline void disable(){accepting=false;if(!enabled)return;if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("target observer disable failed");
 const auto until=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));const auto p=parsePattern(pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),p.size(),p)==std::vector<size_t>{0};
 enabled=false;logFile->event("motion_target_removed","\"restored\":"+std::string(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load())+",\"samples\":"+std::to_string(samples.load())+",\"failures\":"+std::to_string(failures.load()));if(!restored||callbacks.load()||failures.load())throw std::runtime_error("target inventory failed");}
}
