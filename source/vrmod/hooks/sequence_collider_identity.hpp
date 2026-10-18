// Identity-copy rehearsal of the exact stack-query redirection boundary.
// No hand pose is applied here; native collision coordinates stay identical.
namespace sequenceColliderIdentity {
inline bool requested=false;inline std::mutex gate;inline bool accepting=true;
inline std::atomic<unsigned> outstanding{0},opened{0},closed{0},failures{0};
inline thread_local game::ScopedQueryTransforms borrow;
inline thread_local std::array<uint8_t,56> sourceCurrent;
inline thread_local std::array<uint8_t,72> sourcePrevious;
inline thread_local uint32_t sourceEntity=0;
inline void begin(uintptr_t state,uint32_t entity,void* query,const actionSequenceObserver::Status& actor,uint32_t sequence){
 if(!requested)return;std::lock_guard<std::mutex> lock(gate);if(!accepting)return;
 if(borrow.active())throw std::runtime_error("nested collider identity borrow");
 const auto now=GetTickCount64();if(!actor.active||actor.sequence!=sequence||!actor.entity||!actor.usedItem||now<actor.milliseconds||now-actor.milliseconds>200)return;
 const auto owner=nativeImpactComponents::owner(state,entity);if(!owner.ready||!owner.stable||owner.world!=actor.world)return;
 uint32_t root=0,item=0,pide=0,weapon=0;std::memcpy(&root,owner.hierarchy.data()+4,4);std::memcpy(&weapon,owner.hierarchy.data()+8,4);std::memcpy(&item,owner.item.data()+8,4);std::memcpy(&pide,owner.item.data()+12,4);
 if(root!=actor.entity||entity==root||item!=actor.usedItem||!pide||pide!=weapon)return;
 auto& q=*reinterpret_cast<game::ScopedQueryTransforms::Query*>(query);
 sourceCurrent=read<std::array<uint8_t,56>>(q[1]);sourcePrevious=q[3]?read<std::array<uint8_t,72>>(q[3]):std::array<uint8_t,72>{};sourceEntity=entity;
 borrow.open(q,sourceCurrent,sourcePrevious);++outstanding;++opened;
 logFile->event("sequence_collider_identity_begin","\"entity\":"+std::to_string(entity)+",\"sequence\":"+std::to_string(sequence)+",\"root\":"+std::to_string(root)+",\"item\":"+std::to_string(item)+",\"current_bytes\":"+jsonString(nativeImpactComponents::asHex(sourceCurrent))+",\"last_present\":"+(borrow.original()[3]?"true":"false")+",\"ecs_written\":false");
}
inline void finish(void* query) noexcept {
 if(!borrow.active())return;
 game::QueryReturnCompletion completion(outstanding,closed);
 try{
 // Always return borrowed entries, even after accepting=false or log errors.
 const auto q=borrow.original();auto& actual=*reinterpret_cast<game::ScopedQueryTransforms::Query*>(query);
 bool unchanged=false;try{unchanged=sourceCurrent==read<std::array<uint8_t,56>>(q[1])&&(!q[3]||sourcePrevious==read<std::array<uint8_t,72>>(q[3]));}catch(...){}
 const auto restored=borrow.close(actual);completion.markReturned();
 if(!restored||!unchanged)++failures;
 logFile->event("sequence_collider_identity_end","\"entity\":"+std::to_string(sourceEntity)+",\"query_restored\":"+(restored?"true":"false")+",\"original_components_unchanged\":"+(unchanged?"true":"false")+",\"ecs_written\":false");
 }catch(...){++failures;}
}
inline void drain(){
 if(!requested)return;{std::lock_guard<std::mutex> lock(gate);accepting=false;}
 const auto deadline=GetTickCount64()+2000;while(outstanding.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 logFile->event("sequence_collider_identity_drained","\"outstanding\":"+std::to_string(outstanding.load())+",\"opened\":"+std::to_string(opened.load())+",\"closed\":"+std::to_string(closed.load())+",\"failures\":"+std::to_string(failures.load()));
 if(outstanding.load())throw std::runtime_error("collider query borrowing not drained; iterator hook retained");
}
}
