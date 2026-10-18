#include <evr/combat_epoch_client.hpp>
#include <evr/world_epoch_broker.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr;using namespace evr::motion;
namespace {
epoch::Broker<>* active=nullptr;unsigned failAcquireKind=99,failRelease=0,readCount=0,closeOnRead=0;epoch::Response closeTarget{},closure{};bool corruptAcquireVersion=false;unsigned failReadKind=99;
uint32_t dispatch(const epoch::Request* q,epoch::Response* out){
 if(q->operation==uint32_t(epoch::Operation::Acquire)&&q->kind==failAcquireKind){*out={};out->status=uint32_t(epoch::Status::Capacity);return out->status;}
 if(q->operation==uint32_t(epoch::Operation::Release)&&failRelease){--failRelease;*out={};out->status=uint32_t(epoch::Status::Unavailable);return out->status;}
 if(q->operation==uint32_t(epoch::Operation::Read)&&++readCount==closeOnRead)closure=active->call(epoch::request(epoch::Operation::BeginRootClose,closeTarget));
 if(q->operation==uint32_t(epoch::Operation::Read)&&q->kind==failReadKind){*out={};out->status=uint32_t(epoch::Status::WrongToken);return out->status;}
 *out=active->call(*q);if(corruptAcquireVersion&&q->operation==uint32_t(epoch::Operation::Acquire))++out->version;return out->status;
}
}
int main(){unsigned checks=0;try{
 auto ck=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 epoch::Broker<> broker;active=&broker;auto provider=broker.call(epoch::request(epoch::Operation::Register));
 auto call=[&](epoch::Operation op,const epoch::Response& r){return broker.call(epoch::request(op,r));};
 auto create=[&](uint64_t world,uint64_t registry,uint64_t root,uint32_t kind){auto q=epoch::request(epoch::Operation::Create,provider);q.world=world;q.registry=registry;q.root=root;q.kind=kind;auto r=broker.call(q);ck(epoch::admitsNew(r),"create exact native identity");return r;};
 auto end=[&](const epoch::Response& r){const auto closing=call(epoch::Operation::BeginRootClose,r);ck(closing.status==0,"begin root close");ck(call(epoch::Operation::EndRootClose,closing).status==0,"native root ended");};
 for(unsigned cycle=0;cycle<3000;++cycle){
  auto server=create(100,101,102,0),client=create(200,201,202,2);create(300,201,202,1);
  CombatEpochClient old;ck(old.open(dispatch,client,server)&&old.retained()==2,"two independent leases");
  auto before=old.sample();const auto oldWorlds=old.worlds();ck(before.newCombat&&before.suppresses(oldWorlds.contact,true)&&!before.suppresses(oldWorlds.contact,false),"fresh pair permits new combat; suppression requires consumed event");
  end(client);auto after=old.sample();ck(!after.newCombat&&after.suppresses(oldWorlds.contact,true)&&old.retained()==2,"ended client retains old server event history and leases");
  auto nextClient=create(200,201,202,2);create(300,201,202,1);
  ck(!old.open(dispatch,nextClient,server)&&old.worlds()==oldWorlds&&!old.sample().newCombat,"never retag existing binding after identical-address reuse");
  CombatEpochClient stale;ck(!stale.open(dispatch,client,server)&&stale.retained()==0,"old published snapshot cannot acquire same-address replacement World");
  CombatEpochClient next;ck(next.open(dispatch,nextClient,server)&&next.sample().newCombat,"separate binding can use new native generation");
  auto closing=call(epoch::Operation::BeginRootClose,server);auto closed=old.sample();ck(!closed.newCombat&&closed.suppresses(oldWorlds.contact,true),"closing server retains consumed suppression");
  ck(call(epoch::Operation::EndRootClose,closing).status==0,"server native return");ck(!old.sample().suppresses(oldWorlds.contact,true)&&!next.sample().newCombat,"ended server loses all native authority");
  ck(old.release()&&old.retained()==0&&old.release()&&!old.sample().newCombat,"exact release idempotent locally without new native release");
  end(nextClient);ck(next.release()&&call(epoch::Operation::Stats,provider).worlds==0,"all old/new value leases drained");
 }
 for(unsigned role=0;role<2;++role){
  auto s=create(100,101,102,0),c=create(200,201,202,2);create(300,201,202,1);CombatEpochClient owner;ck(owner.open(dispatch,c,s),"admission cancellation owner");
  ck(owner.sample().state()==CombatEpochState::Open,"live exact roles classify open");
  ck(call(epoch::Operation::CloseAdmission,role?s:c).status==0,"close either role admission only");
  const auto stopped=owner.sample();ck(stopped.state()==CombatEpochState::Closed&&!stopped.newCombat&&stopped.suppresses(owner.worlds().contact,true),"admission closure distinguished from unknown; old consumed history retained");
  auto malformed=stopped;malformed.inputValid=false;ck(malformed.state()==CombatEpochState::Unavailable,"failed read cannot count as expected close");
  malformed=stopped;malformed.contact.flags|=epoch::flagFaulted;ck(malformed.state()==CombatEpochState::Unavailable,"provider fault not expected closure");
  end(c);end(s);ck(owner.sample().state()==CombatEpochState::Closed&&owner.release(),"ended exact generation remains classified until release");
 }
 auto server=create(100,101,102,0),client=create(200,201,202,2);create(300,201,202,1);
 CombatEpochClient cold;ck(cold.release()&&!cold.open(dispatch,client,server)&&!cold.retained(),"explicitly released cold owner cannot acquire later");
 failAcquireKind=0;CombatEpochClient partial;ck(!partial.open(dispatch,client,server)&&partial.faulted()&&!partial.retained()&&call(epoch::Operation::Stats,provider).leases==0,"partial pair acquisition releases client; no silent leak");failAcquireKind=99;
 ck(!partial.open(dispatch,client,server),"failed instance cannot adopt again");
 CombatEpochClient retry;ck(retry.open(dispatch,client,server),"retry scenario open");failRelease=1;ck(!retry.release()&&retry.retained()==1&&retry.faulted()&&!retry.sample().newCombat,"failed release retains exact handle and closes authority");ck(retry.release()&&retry.retained()==0,"retry exact failed handle drains values");
 corruptAcquireVersion=true;CombatEpochClient malformed;ck(!malformed.open(dispatch,client,server)&&malformed.faulted()&&!malformed.retained()&&call(epoch::Operation::Stats,provider).leases==0,"malformed successful acquire response still drains exact saved identity");corruptAcquireVersion=false;
 failAcquireKind=0;failRelease=1;CombatEpochClient partialRetry;ck(!partialRetry.open(dispatch,client,server)&&partialRetry.retained()==1&&partialRetry.faulted(),"partial initialization cleanup failure retains exact handle");failAcquireKind=99;ck(partialRetry.release()&&partialRetry.retained()==0,"partial cleanup can retry without rebinding");
 CombatEpochClient unreadable;ck(unreadable.open(dispatch,client,server),"read fault case open");failReadKind=2;auto badClient=unreadable.sample();ck(unreadable.faulted()&&!badClient.newCombat&&badClient.suppresses(worldStamp(server),true),"unknown client stops new requests but exact server history remains");failReadKind=99;ck(!unreadable.sample().newCombat,"read fault is sticky despite later healthy reads");failReadKind=0;ck(!unreadable.sample().suppresses(worldStamp(server),true),"unreadable server never authorizes suppression");failReadKind=99;ck(unreadable.release(),"faulted consumer exact release");
 CombatEpochClient interleaved;ck(interleaved.open(dispatch,client,server),"interleaved close open");readCount=0;closeOnRead=3;closeTarget=client;
 ck(!interleaved.sample().newCombat,"close between the two role reads cannot pass latest pair check");closeOnRead=0;ck(call(epoch::Operation::EndRootClose,closure).status==0,"interleaved native close finishes");
 ck(interleaved.sample().suppresses(worldStamp(server),true),"interleaved client end preserves consumed server history");
 call(epoch::Operation::Fault,provider);auto fault=interleaved.sample();ck(!fault.newCombat&&fault.suppresses(worldStamp(server),true),"provider fault stops new work but not exact owned suppression");
 ck(interleaved.release()&&call(epoch::Operation::Stats,provider).leases==0,"faulted provider still permits exact value drain");
 std::cout<<"PASS "<<checks<<" combat epoch client checks (CPU, no ECS lifetime proof)\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
