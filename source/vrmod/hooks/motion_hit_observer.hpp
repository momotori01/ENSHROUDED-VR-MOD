#pragma once
// Read-only native Impact VM scope and HitEvent queue observation. No damage or
// hit result is fabricated; callbacks outside the exact local scope are ignored.
namespace motionHitObserver {
using Run=uintptr_t(__fastcall*)(void*,uint32_t,void*);
using Node=void*(__fastcall*)(void*,void*,void*);
inline Run originalRun=nullptr;inline Node originalEach=nullptr,originalDamage=nullptr;
inline std::atomic<bool> accepting{false};inline bool enabled=false;
inline std::atomic<unsigned> callbacks{0},runs{0},eachCalls{0},damageCalls{0},failures{0};
struct Owner{uintptr_t query=0,world=0;uint32_t entity=0,root=0,item=0;std::array<uint8_t,24> event{};std::array<uint8_t,20> impact{};};
inline thread_local Owner current;
struct Guard{Guard(){++callbacks;}~Guard(){--callbacks;}};
struct Queue{uintptr_t address=0,data=0;uint32_t count=0;std::array<uint8_t,48> header{};};
inline Queue queue(uintptr_t scope){Queue q;q.address=read<uintptr_t>(scope+0x448);if(!q.address)return q;q.header=read<std::array<uint8_t,48>>(q.address);std::memcpy(&q.data,q.header.data()+0x18,8);std::memcpy(&q.count,q.header.data()+0x28,4);if(q.count>256||(!q.data&&q.count))throw std::runtime_error("HitEvent queue outside diagnostic bounds");return q;}
inline Owner owner(void* scope){
 Owner o;const auto q=reinterpret_cast<uintptr_t>(scope);const auto slots=read<std::array<uintptr_t,152>>(q);if(!slots[0]||!slots[9]||!slots[10]||!slots[13]||!slots[17])return o;
 const auto root=rootCamera::snapshot();if(!root.entity||read<uint32_t>(slots[9]+4)!=root.entity)return o;
 uint32_t seq=read<uint32_t>(slots[13]);if(!seq)return o;
 const auto state=read<uintptr_t>(slots[0]),world=read<uintptr_t>(state+0x30);const auto actor=actionSequenceObserver::forWorld(world,root.entity);if(!actor.active||actor.sequence!=seq)return o;
 const auto index=read<uint32_t>(slots[0]+8);if(!index||index>0x1000000)throw std::runtime_error("owned Impact VM entity index");
 o.query=q;o.world=world;o.root=root.entity;o.item=read<uint32_t>(slots[10]+8);o.entity=read<uint32_t>(read<uintptr_t>(state+0x828)+uintptr_t(index-1)*16);o.event=read<std::array<uint8_t,24>>(slots[13]);o.impact=read<std::array<uint8_t,20>>(slots[17]);
 if(!world||o.item!=actor.usedItem||read<uint32_t>(slots[9])!=root.entity)return {};return o;
}
inline std::string labels(const Owner& o){return "\"world\":"+hex(o.world)+",\"entity\":"+std::to_string(o.entity)+",\"root\":"+std::to_string(o.root)+",\"item\":"+std::to_string(o.item)+",\"scope\":"+hex(o.query)+",\"event_bytes\":"+jsonString(nativeImpactComponents::asHex(o.event))+",\"impact_bytes\":"+jsonString(nativeImpactComponents::asHex(o.impact));}
inline void fail(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("motion_hit_observer_failure","\"reason\":"+jsonString(e.what()));}
#include "motion_health_observer.hpp"
__declspec(noinline) uintptr_t __fastcall run(void* registry,uint32_t id,void* scope){
 Guard guard;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;Owner observed;Queue before;
 {const ObserverLastError preserve;if(accepting.load()&&caller==0x3ef6d3&&runs.load()<128)try{observed=owner(scope);if(observed.query){before=queue(observed.query);}}catch(const std::exception& e){fail(e);observed={};}}
 const auto previous=current;if(observed.query)current=observed;
 struct Restore{Owner old;~Restore(){current=old;}}restore{previous};
 const auto result=originalRun(registry,id,scope);const ObserverLastError preserve;
 if(observed.query)try{
  const auto after=queue(observed.query);const auto again=owner(scope);const bool stable=again.world==observed.world&&again.entity==observed.entity&&again.root==observed.root&&again.item==observed.item&&again.event==observed.event&&again.impact==observed.impact;
  if(stable)health::remember(observed,id);
  logFile->event("motion_owned_vm",labels(observed)+",\"sample\":"+std::to_string(++runs)+",\"instance_id\":"+std::to_string(id)+",\"queue_before\":"+jsonString(nativeImpactComponents::asHex(before.header))+",\"queue_after\":"+jsonString(nativeImpactComponents::asHex(after.header))+",\"count_before\":"+std::to_string(before.count)+",\"count_after\":"+std::to_string(after.count)+",\"owner_stable\":"+(stable?"true":"false")+",\"caller\":\"0x3ef6d3\",\"read_only\":true");
  if(!stable)throw std::runtime_error("owned Impact VM identity changed");
 }catch(const std::exception& e){fail(e);}return result;
}
__declspec(noinline) void* __fastcall each(void* output,void* arguments,void* vm){
 Guard guard;const auto o=current;bool observed=false;uint32_t index=0;uintptr_t indexPointer=0;Queue q;std::array<uint8_t,232> hit{};
 {const ObserverLastError preserve;if(accepting.load()&&o.query&&eachCalls.load()<256)try{
  if(read<uintptr_t>(reinterpret_cast<uintptr_t>(vm)+0x30)==o.query){q=queue(o.query);const auto args=read<uintptr_t>(reinterpret_cast<uintptr_t>(arguments));const auto slot=read<uint32_t>(args+23*4);
   if(slot){const auto table=read<uintptr_t>(reinterpret_cast<uintptr_t>(vm));indexPointer=read<uintptr_t>(reinterpret_cast<uintptr_t>(vm)+0x10)+read<uint16_t>(table+(slot&0xffff)*24+14);index=read<uint32_t>(indexPointer);if(index<q.count)hit=read<std::array<uint8_t,232>>(q.data+uintptr_t(index)*232);observed=true;}
  }
 }catch(const std::exception& e){fail(e);}}
 auto* result=originalEach(output,arguments,vm);const ObserverLastError preserve;
 if(observed)try{logFile->event("motion_owned_hit_iteration",labels(o)+",\"sample\":"+std::to_string(++eachCalls)+",\"queue_count\":"+std::to_string(q.count)+",\"index_before\":"+std::to_string(index)+",\"index_after\":"+std::to_string(read<uint32_t>(indexPointer))+",\"hit_bytes\":"+jsonString(index<q.count?nativeImpactComponents::asHex(hit):std::string{})+",\"result_word\":"+std::to_string(read<uint16_t>(reinterpret_cast<uintptr_t>(output)))+",\"return_matches_output\":"+(result==output?"true":"false")+",\"scope_matches\":true,\"read_only\":true");}catch(const std::exception& e){fail(e);}return result;
}
__declspec(noinline) void* __fastcall damage(void* output,void* arguments,void* vm){
 Guard guard;const auto o=current;bool observed=false;{const ObserverLastError preserve;if(accepting.load()&&o.query&&damageCalls.load()<64)try{observed=read<uintptr_t>(reinterpret_cast<uintptr_t>(vm)+0x30)==o.query;}catch(const std::exception& e){fail(e);}}
 auto* result=originalDamage(output,arguments,vm);const ObserverLastError preserve;
 if(observed)try{logFile->event("motion_owned_damage_callback",labels(o)+",\"sample\":"+std::to_string(++damageCalls)+",\"result_word\":"+std::to_string(read<uint16_t>(reinterpret_cast<uintptr_t>(output)))+",\"return_matches_output\":"+(result==output?"true":"false")+",\"scope_matches\":true,\"read_only\":true,\"damage_amount_unverified\":true");}catch(const std::exception& e){fail(e);}return result;
}
struct Hook{uintptr_t rva;const char* pattern;void* callback;void** original;bool on=false;};
inline std::array<Hook,6> hooks{{
 {0x1228490,"40 53 41 56 48 83 ec 48 4d 8b f0 4c 8b d1 e8 5d",reinterpret_cast<void*>(&run),reinterpret_cast<void**>(&originalRun)},
 {0x35a830,"4c 89 44 24 18 53 57 41 54 41 56 41 57 48 81 ec",reinterpret_cast<void*>(&each),reinterpret_cast<void**>(&originalEach)},
 {0x3d50d0,"48 8b c4 4c 89 40 18 48 89 48 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8d a8 08 f9 ff ff 48 81",reinterpret_cast<void*>(&damage),reinterpret_cast<void**>(&originalDamage)},
 {0x3e8ec0,"48 8b c4 4c 89 40 18 55 53 56 57 41 54 41 55",reinterpret_cast<void*>(&health::commit),reinterpret_cast<void**>(&health::originalCommit)},
 {0x3e8720,"48 89 5c 24 10 48 89 6c 24 18 48 89 74 24 20 57",reinterpret_cast<void*>(&health::delta),reinterpret_cast<void**>(&health::originalDelta)},
 {0x121e150,"89 54 24 10 53 48 83 ec 20 48 8b d9 48 8d 54 24 38 49 8d 48 68 e8 36",reinterpret_cast<void*>(&health::lookup),reinterpret_cast<void**>(&health::originalLookup)}
}};
inline void install(){{std::lock_guard<std::mutex> lock(health::mutex);health::witnesses.clear();}for(auto& h:hooks){const auto p=parsePattern(h.pattern);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+h.rva),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("Impact VM observer signature changed");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+h.rva),h.callback,h.original)!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+h.rva))!=MH_OK)throw std::runtime_error("Impact VM observer hook failed");h.on=true;enabled=true;}
 logFile->event("motion_health_observer_enabled","\"commit_rva\":\"0x3e8ec0\",\"delta_rva\":\"0x3e8720\",\"lookup_rva\":\"0x121e150\",\"health_key\":2394442133,\"health_schema_required\":true,\"witness_capacity\":16,\"witness_max_age_ms\":500,\"read_only\":true");
 accepting=true;logFile->event("motion_hit_observer_enabled","\"vm_rva\":\"0x1228490\",\"caller\":\"0x3ef6d3\",\"each_rva\":\"0x35a830\",\"damage_rva\":\"0x3d50d0\",\"hit_bytes\":232,\"read_only\":true");}
inline void disable(){accepting=false;if(!enabled)return;for(auto& h:hooks)if(h.on){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+h.rva))!=MH_OK)throw std::runtime_error("Impact VM observer disable failed");h.on=false;}
 const auto end=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));bool restored=true;for(const auto& h:hooks){const auto p=parsePattern(h.pattern);restored&=scan(reinterpret_cast<const uint8_t*>(baseAddress+h.rva),p.size(),p)==std::vector<size_t>{0};}
 logFile->event("motion_health_observer_removed","\"commits\":"+std::to_string(health::commits.load())+",\"deltas\":"+std::to_string(health::deltas.load())+",\"unqualified\":"+std::to_string(health::unqualified.load()));
 {std::lock_guard<std::mutex> lock(health::mutex);health::witnesses.clear();}
 logFile->event("motion_health_witness_removed","\"records\":"+std::to_string(health::recorded.load())+",\"cleared\":true");
 enabled=false;logFile->event("motion_hit_observer_removed",std::string("\"restored\":")+(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load())+",\"owned_vm_calls\":"+std::to_string(runs.load())+",\"hit_iterations\":"+std::to_string(eachCalls.load())+",\"damage_callbacks\":"+std::to_string(damageCalls.load())+",\"failures\":"+std::to_string(failures.load()));
 // Live play may end without one attack Impact (tools have none): no owned run is not a failure there.
 if(!restored||callbacks.load()||failures.load()||(!runs.load()&&!motionColliderChannel::live&&!(controllerUiFixture::expectedMotionCancellation()&&motionColliderChannel::eventBlocked.load())))throw std::runtime_error("owned Impact VM observation failed");}
}
