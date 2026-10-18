#include <evr/motion_world_retirement.hpp>
#include <evr/combat_epoch_client.hpp>
#include <evr/world_epoch_broker.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr;using namespace evr::motion;
namespace {
epoch::Broker<>* broker=nullptr;bool badRead=false;
uint32_t dispatch(const epoch::Request* q,epoch::Response* out){
 *out=broker->call(*q);
 if(badRead&&q->operation==uint32_t(epoch::Operation::Read)&&q->kind==0)out->status=uint32_t(epoch::Status::WrongToken);
 return out->status;
}
EventAuthority authority(WorldStamp s){return {{s.world,12,13,14,15,0,0,s},{16},true};}
ColliderKey collider(const EventAuthority& a){
 ColliderKey k{a.action.world,20,{},a.action.epoch};
 std::memcpy(k.event.data(),&a.action.sequence,4);std::memcpy(k.event.data()+8,a.guid.data(),16);return k;
}
}
int main(){unsigned checks=0;try{
 auto ck=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 epoch::Broker<> book;broker=&book;auto provider=book.call(epoch::request(epoch::Operation::Register));
 auto call=[&](epoch::Operation op,const epoch::Response& r){return book.call(epoch::request(op,r));};
 auto create=[&](uint64_t w,uint64_t registry,uint64_t root,uint32_t kind){auto q=epoch::request(epoch::Operation::Create,provider);q.world=w;q.registry=registry;q.root=root;q.kind=kind;auto r=book.call(q);ck(epoch::admitsNew(r),"create original or reused native address");return r;};
 auto begin=[&](const epoch::Response& r){auto c=call(epoch::Operation::BeginRootClose,r);ck(c.status==0,"begin native destruction");return c;};
 auto end=[&](const epoch::Response& c){ck(call(epoch::Operation::EndRootClose,c).status==0,"original destructor returned");};
 ContactLifetimePool<1> pool;EventRetimeLedger<1> events;ColliderLeaseLedger<1> colliders;
 WorldEndBoundary previous;ContactLifetimeToken previousToken;
 for(unsigned cycle=0;cycle<10000;++cycle){
  auto server=create(100,101,102,0),client=create(200,201,202,2);create(300,201,202,1);
  CombatEpochClient owner;ck(owner.open(dispatch,client,server),"retain exact independent World leases");
  ck(permitsActorFence(owner.worlds().contact,owner.sample().contact),"live server permits native Actor fence");
  const auto a=authority(owner.worlds().contact);const auto k=collider(a);const EventIdentity id{24,a.guid};const bool blocked=cycle%2;
  ck(events.commit(a.action,id,blocked),"commit authored event before destruction");
  auto token=blocked?pool.acquireBlocked(events,a.action,id):pool.acquire(a,k);
  ck(bool(token),"retain serial-protected action values");
  if(!blocked)ck(colliders.decide(k,true,0,true)==ColliderDecision::Track&&colliders.returned(k,40),"native collider lease");
  WorldEndBoundary proof;
  ck(!owner.endedContact(proof)&&!proof.owner.valid(),"live root cannot be a terminal proof");
  const auto untouched=[&]{return pool.snapshot(token).used&&events.owns(a.action)&&(blocked||colliders.owns(k));};
  const auto attempt=[&](const WorldEndBoundary& b){return retireContactAfterWorldEnd(pool,token,events,colliders,b);};
  ck(attempt(proof)==WorldRetirementResult::InvalidBoundary&&untouched(),"no proof causes zero erases");
  if(previousToken){
   ck(attempt(previous)==WorldRetirementResult::InvalidBoundary&&untouched(),"ended previous generation cannot erase reused raw addresses");
   ck(retireContactAfterWorldEnd(pool,previousToken,events,colliders,previous)==WorldRetirementResult::StaleToken&&untouched(),"old callback token cannot erase new slot");
  }
  ck(call(epoch::Operation::CloseAdmission,client).status==0,"stop only input admission");
  ck(!owner.endedContact(proof)&&owner.sample().suppresses(a.action.epoch,true),"client closure preserves consumed server event");
  end(begin(client));
  ck(permitsActorFence(a.action.epoch,owner.sample().contact),"input World end still permits live server idle retirement");
  ck(!owner.endedContact(proof)&&owner.sample().suppresses(a.action.epoch,true),"ended client is not ended server");
  if(cycle%3==0){ck(call(epoch::Operation::CloseAdmission,server).status==0,"server preclose optional before destructor");ck(!permitsActorFence(a.action.epoch,owner.sample().contact),"server preclose prevents Actor reads during entity deletion");}
  auto closing=begin(server);
  ck(!permitsActorFence(a.action.epoch,owner.sample().contact),"closing server cannot authorize Actor fence reads");
  ck(!owner.endedContact(proof)&&owner.sample().suppresses(a.action.epoch,true),"closing server retains suppression");
  ck(events.consider(a.action,id,false,false,0,16)==EventRetime::Decision::Suppress,"no timeout or client exit replays consumed Impact");
  end(closing);
  ck(owner.endedContact(proof)&&!proof.consumersStopped&&!proof.permits(a.action.epoch),"fresh ended metadata does not invent callback quiescence");
  ck(attempt(proof)==WorldRetirementResult::InvalidBoundary&&untouched(),"stopped admission and callback drain required separately");
  proof.consumersStopped=true;
  ck(!permitsActorFence(a.action.epoch,proof.observed),"terminal retirement never authorizes Actor reads");
  ck(proof.permits(a.action.epoch),"exact stopped old server permits values-only retirement");
  if(cycle==0){
   for(unsigned field=0;field<25;++field){auto bad=proof;
    switch(field){
     case 0:bad.observed.size--;break;case 1:bad.observed.version++;break;
     case 2:bad.observed.status=uint32_t(epoch::Status::Unavailable);break;
     case 3:bad.observed.phase=uint32_t(epoch::Phase::Alive);break;
     case 4:bad.observed.phase=uint32_t(epoch::Phase::Closing);break;
     case 5:bad.observed.phase=99;break;case 6:bad.observed.provider++;break;
     case 7:bad.observed.serial++;break;case 8:bad.observed.world++;break;
     case 9:bad.observed.registry++;break;case 10:bad.observed.root++;break;
     case 11:bad.observed.slot++;break;case 12:bad.observed.kind=2;break;
     case 13:bad.observed.pins=0;break;case 14:bad.observed.flags|=epoch::flagFaulted;break;
     case 15:bad.observed.flags=0;break;case 16:bad.observed.flags|=8;break;
     case 17:bad.leaseHandle=0;break;case 18:bad.observed.handle=1;break;
     case 19:bad.owner.serial++;break;case 20:bad.consumersStopped=false;break;
     case 21:bad.selectors=1;break;case 22:bad.queries=1;break;
     case 23:bad.destructors=1;break;case 24:bad.flushes=1;break;
    }
    ck(attempt(bad)==WorldRetirementResult::InvalidBoundary&&untouched(),"invalid ABI/identity/phase/lease/quiescence cannot mutate any ledger");
   }
   // Disagreement across the MOD ledgers fails before the first erase.
   ColliderLeaseLedger<1> missingCollider;
   ck(retireContactAfterWorldEnd(pool,token,events,missingCollider,proof)==WorldRetirementResult::OwnershipMismatch&&untouched(),"missing collider ownership preserves event/watch");
   EventRetimeLedger<1> missingEvent;
   ck(retireContactAfterWorldEnd(pool,token,missingEvent,colliders,proof)==WorldRetirementResult::OwnershipMismatch&&untouched(),"missing event ownership preserves collider/watch");
   ck(pool.beginDestroy(token,k)&&attempt(proof)==WorldRetirementResult::OwnershipMismatch&&untouched(),"unfinished observer destruction cannot be bypassed");
   ck(pool.finishDestroy(token,k,true),"finish exact observer scope");
   ContactLifetimePool<1> faulty;EventRetimeLedger<1> blockedEvents;ColliderLeaseLedger<1> noColliders;
   ck(blockedEvents.block(a.action,id),"blocked fault fixture event");const auto t=faulty.acquireBlocked(blockedEvents,a.action,id);
   ck(t&&faulty.unexpectedImpact(t,a.action),"unexpected native Impact invalidates blocked-only proof");
   ck(retireContactAfterWorldEnd(faulty,t,blockedEvents,noColliders,proof)==WorldRetirementResult::Faulted&&faulty.size()==1&&blockedEvents.size()==1,"World end does not hide prior lifetime failure");
   auto legacy=a;legacy.action.epoch={};auto oldKey=collider(legacy);ContactLifetimePool<1> legacyPool;EventRetimeLedger<1> legacyEvents;ColliderLeaseLedger<1> legacyColliders;
   ck(legacyEvents.commit(legacy.action,id),"legacy fixture");auto lt=legacyPool.acquire(legacy,oldKey);
   ck(lt&&retireContactAfterWorldEnd(legacyPool,lt,legacyEvents,legacyColliders,proof)==WorldRetirementResult::InvalidBoundary&&legacyEvents.size()==1,"raw legacy address is never a World end proof");
  }
  // Native addresses are reused while the OLD value leases still exist.
  auto newServer=create(100,101,102,0),newClient=create(200,201,202,2);create(300,201,202,1);
  ck(worldStamp(newServer).serial!=a.action.epoch.serial&&owner.endedContact(proof),"fresh broker Read still resolves exact retained old generation");proof.consumersStopped=true;
  auto replacement=proof;replacement.observed=call(epoch::Operation::Read,newServer);
  ck(attempt(replacement)==WorldRetirementResult::InvalidBoundary&&untouched(),"replacement live World at same address cannot supply terminal evidence");
  ck(attempt(proof)==WorldRetirementResult::Retired,"terminal retirement without Actor idle or individual native destruction");
  ck(!pool.size()&&!events.size()&&!colliders.size()&&attempt(proof)==WorldRetirementResult::StaleToken,"all owned values erased once without replay or leakage");
  previous=proof;previousToken=token;
  ck(owner.release()&&!owner.endedContact(proof),"released lease can no longer supply terminal evidence");
  end(begin(newClient));end(begin(newServer));
  ck(call(epoch::Operation::Stats,provider).worlds==0&&call(epoch::Operation::Stats,provider).leases==0,"all generations and value handles reclaimable");
 }
 // Faulted consumer cannot turn an apparently healthy terminal Read into proof.
 auto s=create(100,101,102,0),c=create(200,201,202,2);create(300,201,202,1);CombatEpochClient failed;ck(failed.open(dispatch,c,s),"fault fixture open");
 badRead=true;failed.sample();badRead=false;end(begin(c));end(begin(s));WorldEndBoundary invalid;
 ck(!failed.endedContact(invalid)&&failed.faulted()&&failed.release(),"sticky transport fault retains evidence until explicit cleanup");
 ck(call(epoch::Operation::Stop,provider).status==0,"provider drains after repeated generation reuse");
 std::cout<<"PASS "<<checks<<" exact World end retirement checks (CPU broker/value ledgers only)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
