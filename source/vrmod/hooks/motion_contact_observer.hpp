// Read-only observation of registered native Impact VM callbacks. ABI and
// argument roles: docs/analysis/motion_impact_callbacks/{362280,35c3d0,358350}.asm.
// Never changes VM values, queries, hit records, costs, or actor state.
namespace motionContactObserver {
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},samples{0},failures{0},calls{0};
inline std::array<std::atomic<unsigned>,3> baselineSamples{},actionSamples{};
using Callback=void*(__fastcall*)(void*,void*,void*);
struct Hook {const char* name;uintptr_t rva;const char* pattern;Callback original=nullptr;bool enabled=false;};
inline std::array<Hook,3> hooks{{
 {"startQuery",0x3e5c50,"4C 89 44 24 18 53 55 57 41 54 41 55 41 57 48 83 EC"},
 {"GetSlotTransform",0x35c8ec,"40 53 56 41 54 41 55 41 56 48 81 EC A0 00 00 00"},
 {"fetchQueryResult",0x3da960,"4C 89 44 24 18 53 55 57 41 54 41 55 41 57 48 81 EC 88 00 00 00 4C"}
}};
inline constexpr std::array<std::array<unsigned,8>,3> sizes{{{{4,2,1,56,4,0,0,0}},{{56,12,1,4,0,0,0,0}},{{4,12,12,1,1,1,4,4}}}};
inline constexpr unsigned counts[]{5,4,8};
struct Argument {uint32_t id=0;uint16_t offset=0;uintptr_t pointer=0;std::array<uint8_t,24> descriptor{};std::string bytes;};
struct Snapshot {uint32_t scopeEntity=0;uintptr_t args=0,table=0,data=0,scope=0;std::array<Argument,8> values{};};
inline std::string bytesAt(uintptr_t ptr,size_t count){
 if(count>64)throw std::runtime_error("contact read exceeds bounded value");
 std::array<uint8_t,64> bytes{};SIZE_T done=0;
 if(!ptr||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(ptr),bytes.data(),count,&done)||done!=count)throw std::runtime_error("contact argument unreadable");
 const char* digits="0123456789abcdef";std::string result;result.reserve(count*2);
 for(size_t i=0;i<count;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;
}
inline Snapshot snapshot(unsigned kind,void* arguments,void* vm){
 Snapshot s;const auto v=reinterpret_cast<uintptr_t>(vm);
 s.args=read<uintptr_t>(reinterpret_cast<uintptr_t>(arguments));s.table=read<uintptr_t>(v);s.data=read<uintptr_t>(v+0x10);s.scope=read<uintptr_t>(v+0x30);
 // Exact read-only equivalent of native 8d3490, called by startQuery.
 const auto task=read<uintptr_t>(s.scope);const auto index=read<uint32_t>(task+8);
 if(index>0x1000000)throw std::runtime_error("contact task entity index outside diagnostic bound");
 if(index){const auto state=read<uintptr_t>(task);s.scopeEntity=read<uint32_t>(read<uintptr_t>(state+0x828)+uintptr_t(index-1)*16);}
 for(unsigned i=0;i<counts[kind];++i){auto& a=s.values[i];a.id=read<uint32_t>(s.args+i*4);if(!a.id)continue;
  // Native code uses the low 16-bit slot and its 16-bit byte offset.
  a.descriptor=read<std::array<uint8_t,24>>(s.table+uintptr_t(a.id&0xffff)*24);
  std::memcpy(&a.offset,a.descriptor.data()+14,2);a.pointer=s.data+a.offset;a.bytes=bytesAt(a.pointer,sizes[kind][i]);
 }return s;
}
inline void append(std::ostringstream& o,const Snapshot& a,const Snapshot& b,unsigned k){
 o<<",\"argument_storage_stable\":"<<((a.args==b.args&&a.table==b.table&&a.data==b.data&&a.scope==b.scope)?"true":"false")<<",\"scope\":"<<hex(a.scope)<<",\"scope_entity\":"<<a.scopeEntity<<",\"scope_entity_stable\":"<<(a.scopeEntity==b.scopeEntity?"true":"false")<<",\"vm_table\":"<<hex(a.table)<<",\"vm_data\":"<<hex(a.data)<<",\"argument_ids\":"<<hex(a.args)<<",\"arguments\":[";
 for(unsigned i=0;i<counts[k];++i){if(i)o<<',';const auto& v=a.values[i];const auto& w=b.values[i];
  o<<"{\"index\":"<<i<<",\"id\":"<<v.id<<",\"offset\":"<<v.offset<<",\"address\":"<<hex(v.pointer)
   <<",\"descriptor_stable\":"<<((v.id==w.id&&v.pointer==w.pointer&&v.descriptor==w.descriptor)?"true":"false")
   <<",\"before\":"<<jsonString(v.bytes)<<",\"after\":"<<jsonString(w.bytes)<<'}';
 }o<<']';
}
template<unsigned K> __declspec(noinline) void* __fastcall observe(void* output,void* arguments,void* vm){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());callbacks.fetch_add(1);calls.fetch_add(1);
 struct Leave{~Leave(){callbacks.fetch_sub(1);}}leave;
 Snapshot before;bool captured=false;actionSequenceObserver::Status actor;
 {const ObserverLastError error;
  try{if(accepting.load()&&actionSequenceObserver::recording.load()){
   {std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);actor=actionSequenceObserver::latest;}
   const auto now=GetTickCount64();const bool action=actor.active&&actor.entity&&actor.milliseconds<=now&&now-actor.milliseconds<=200;
   auto& quota=action?actionSamples[K]:baselineSamples[K];const unsigned limit=action?512u:16u;
   if(quota.fetch_add(1)<limit){before=snapshot(K,arguments,vm);captured=true;}
  }}catch(const std::exception& e){if(failures.fetch_add(1)<3)try{logFile->event("motion_contact_failure","\"phase\":\"before\",\"reason\":"+jsonString(e.what()));}catch(...){} }
 }
 auto* result=hooks[K].original(output,arguments,vm);
 {const ObserverLastError error;
  if(captured)try{
   const auto after=snapshot(K,arguments,vm);const auto number=samples.fetch_add(1)+1;
   std::ostringstream o;o<<"\"sample\":"<<number<<",\"name\":"<<jsonString(hooks[K].name)<<",\"rva\":"<<hex(hooks[K].rva)
    <<",\"caller_rva\":"<<hex(caller-baseAddress)<<",\"thread\":"<<GetCurrentThreadId()<<",\"milliseconds\":"<<GetTickCount64()
    <<",\"return_matches_output\":"<<(result==output?"true":"false")<<",\"result_word\":"<<read<uint16_t>(reinterpret_cast<uintptr_t>(output))
    <<",\"latest_actor_entity\":"<<actor.entity<<",\"latest_actor_item\":"<<actor.usedItem<<",\"latest_actor_active\":"<<(actor.active?"true":"false")
    <<",\"latest_actor_ms\":"<<actor.milliseconds<<",\"latest_actor_trigger\":"<<actor.triggerTime<<",\"read_only\":true,\"owner_proven\":false";
   append(o,before,after,K);logFile->event("motion_contact_sample",o.str());
  }catch(const std::exception& e){if(failures.fetch_add(1)<3)try{logFile->event("motion_contact_failure","\"phase\":\"after\",\"reason\":"+jsonString(e.what()));}catch(...){} }
 }return result;
}
inline void install(){
 std::array<Callback,3> wrappers{{observe<0>,observe<1>,observe<2>}};
 for(unsigned i=0;i<hooks.size();++i){auto& h=hooks[i];const auto target=reinterpret_cast<void*>(baseAddress+h.rva);const auto pattern=parsePattern(h.pattern);
  if(scan(static_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0})throw std::runtime_error("contact callback signature differs");
  if(MH_CreateHook(target,reinterpret_cast<void*>(wrappers[i]),reinterpret_cast<void**>(&h.original))!=MH_OK)throw std::runtime_error("contact callback create failed");
  if(MH_EnableHook(target)!=MH_OK)throw std::runtime_error("contact callback enable failed");h.enabled=true;
  logFile->event("motion_contact_enabled","\"name\":"+jsonString(h.name)+",\"rva\":"+hex(h.rva)+",\"pattern\":"+jsonString(h.pattern)+",\"read_only\":true");
 }accepting=true;
}
inline void disable(){
 accepting=false;bool any=false;for(auto& h:hooks)if(h.enabled){any=true;if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+h.rva))!=MH_OK)throw std::runtime_error("contact callback disable failed");h.enabled=false;}
 if(!any)return;const auto deadline=GetTickCount64()+1000;
 while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 bool restored=true;for(const auto& h:hooks){if(!h.original)continue;const auto p=parsePattern(h.pattern);restored=restored&&scan(reinterpret_cast<const uint8_t*>(baseAddress+h.rva),p.size(),p)==std::vector<size_t>{0};}
 logFile->event("motion_contact_removed","\"original_bytes_restored\":"+std::string(restored?"true":"false")+",\"remaining_callbacks\":"+std::to_string(callbacks.load())+",\"samples\":"+std::to_string(samples.load())+",\"read_failures\":"+std::to_string(failures.load())+",\"calls\":"+std::to_string(calls.load()));
 if(!restored||callbacks.load())throw std::runtime_error("contact callback restoration incomplete");
}
}
