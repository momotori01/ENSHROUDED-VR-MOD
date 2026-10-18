#include <evr/world_epoch_broker.hpp>
#include <evr/combat_epoch_client.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
using namespace evr;using namespace evr::epoch;
using Store=Broker<>;Store* store=nullptr;
uint32_t route(const Request* q,Response* r){*r=store->call(*q);return r->status;}
unsigned checks=0;void ck(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
Response create(const Response& provider,uint64_t world,uint64_t registry,uint64_t root,uint32_t kind){auto q=request(Operation::Create,provider);q.world=world;q.registry=registry;q.root=root;q.kind=kind;auto r=store->call(q);ck(admitsNew(r),"fresh native creation");return r;}
Response op(Operation action,const Response& r){return store->call(request(action,r));}
int main(){try{
 Store book;store=&book;auto provider=book.call(request(Operation::Register));
 for(unsigned cycle=0;cycle<2000;++cycle){
  const auto server=create(provider,100,101,102,0),client=create(provider,200,201,202,2),prediction=create(provider,203,201,202,1);
  motion::CombatEpochClient consumer;ck(consumer.open(route,client,server),"bind fresh client/server values");const auto saved=consumer.worlds();
  ck(op(Operation::CloseAdmission,prediction).status==uint32_t(Status::Invalid),"prediction cannot stop owner root independently");
  auto wrong=client;wrong.serial++;ck(op(Operation::CloseAdmission,wrong).status==uint32_t(Status::WrongToken),"wrong generation cannot stop live root");
  const bool clientFirst=cycle%2==0;const auto first=clientFirst?client:server;
  auto shut=op(Operation::CloseAdmission,first);ck(shut.status==0&&shut.phase==1&&shut.flags==(flagActive|flagAdmissionClosed)&&!admitsNew(shut),"preclose stops admission while root value stays alive");
  ck(op(Operation::CloseAdmission,first).flags==shut.flags,"native retry is idempotent");
  ck(op(Operation::Acquire,first).status==uint32_t(Status::Closing),"new lease acquisition rejected while deleting entities");
  auto sample=consumer.sample();ck(sample.inputValid&&sample.contactValid&&!sample.newCombat&&!consumer.faulted(),"admission closure is deliberate, not ABI/read fault");
  ck(sample.suppresses(saved.contact,true)&&!sample.suppresses(saved.contact,false),"only previously consumed server event remains suppressible");
  ck(admitsNew(op(Operation::Read,clientFirst?server:client)),"unrelated root remains independently alive");
  ck(admitsNew(op(Operation::Read,prediction))==!clientFirst,"prediction admission follows its owner root only");
  std::atomic<bool> bad{false};std::thread reader([&]{for(unsigned i=0;i<100;++i)if(admitsNew(op(Operation::Read,first)))bad=true;});
  for(unsigned i=0;i<10;++i)ck(op(Operation::CloseAdmission,first).status==0,"repeated teardown polls do not allocate close handles");reader.join();ck(!bad,"post-close fresh reads never grant new admission");
  auto closeFirst=op(Operation::BeginRootClose,first);ck(closeFirst.status==0&&closeFirst.phase==2&&closeFirst.handle,"destructor still acquires its separate close scope");
  ck(op(Operation::EndRootClose,closeFirst).status==0,"destructor completion is distinct from early admission stop");
  sample=consumer.sample();ck(!sample.newCombat&&sample.suppresses(saved.contact,true)==clientFirst,"server end stops suppression; client end preserves server history");
  const auto second=clientFirst?server:client;ck(op(Operation::CloseAdmission,second).status==0,"stop the other owner separately");auto closeSecond=op(Operation::BeginRootClose,second);ck(closeSecond.status==0&&op(Operation::EndRootClose,closeSecond).status==0,"finish remaining native root");
  ck(consumer.release()&&consumer.retained()==0,"drained consumer releases both exact handles");auto stats=op(Operation::Stats,provider);ck(stats.worlds==stats.leases&&stats.leases==stats.closures&&stats.worlds==0,"all values reclaimed");
  ck(op(Operation::CloseAdmission,first).status==uint32_t(Status::WrongToken),"old same-address generation cannot suspend replacement");
 }
 ck(op(Operation::Stop,provider).status==0,"provider drains normally");
 std::cout<<"PASS "<<checks<<" early admission checks / 2000 same-address cycles; CPU values only, no ECS exclusion claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<checks<<" "<<e.what()<<'\n';return 1;}}
