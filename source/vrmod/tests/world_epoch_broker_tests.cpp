#include <evr/world_epoch_broker.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::epoch;
int main(){unsigned n=0;try{
 auto ck=[&](bool v,const char* why){++n;if(!v)throw std::runtime_error(why);};
 Broker<2,2,1> b;auto reg=b.call(request(Operation::Register));
 auto call=[&](Request r,Status expected=Status::Ok){auto s=b.call(r);ck(s.status==uint32_t(expected),"expected bounded broker status");return s;};
 auto create=[&](uint64_t world,uint64_t root,Status expected=Status::Ok){auto r=request(Operation::Create,reg);r.world=world;r.registry=root+1;r.root=root;r.kind=0;return call(r,expected);};
 auto a=create(100,200),d=create(300,400);create(500,600,Status::Capacity);
 ck(admitsNew(call(request(Operation::Read,a))),"capacity refusal leaves existing generation alive");
 auto p=call(request(Operation::Acquire,a)),p2=call(request(Operation::Acquire,d));call(request(Operation::Acquire,a),Status::Capacity);
 auto c=call(request(Operation::BeginRootClose,a));call(request(Operation::BeginRootClose,d),Status::Capacity);
 ck(admitsNew(call(request(Operation::Read,d))),"closure capacity rejection does not partially pin next root");
 auto invalid=request(Operation::EndRootClose,c);invalid.root++;call(invalid,Status::WrongToken);
 call(request(Operation::EndRootClose,c));create(500,600,Status::Capacity);
 auto end=call(request(Operation::Read,a));ck(end.phase==uint32_t(Phase::Ended)&&end.pins==1,"ended leased value consumes slot until exact release");
 call(request(Operation::Release,p));call(request(Operation::Release,p),Status::Unknown);
 auto fresh=create(100,200);ck(fresh.serial!=a.serial&&fresh.slot==a.slot,"freed slot gets a new serial for identical native addresses");
 call(request(Operation::Acquire,a),Status::WrongToken);
 call(request(Operation::Release,p2));auto closing=call(request(Operation::BeginRootClose,fresh));
 auto fault=call(request(Operation::Fault,reg));ck((fault.flags&flagFaulted)&&!(fault.flags&flagActive),"explicit fault disables future admission");
 call(request(Operation::Acquire,d),Status::Unavailable);ck(!admitsNew(call(request(Operation::Read,d))),"read after fault cannot authorize work even if phase remains Alive");
 call(request(Operation::EndRootClose,closing));ck(call(request(Operation::Stats)).closures==0,"saved native root close may drain after fault");
 call(request(Operation::Stop,reg),Status::Busy);call(request(Operation::Register),Status::Duplicate);
 Broker<> malformed;auto m=malformed.call(request(Operation::Register));auto q=request(Operation::Create,m);q.world=100;q.registry=200;q.root=300;q.kind=2;auto client=malformed.call(q);
 auto partial=malformed.call(request(Operation::BeginRootClose,client));ck(partial.status==uint32_t(Status::Fault)&&partial.flags==flagFaulted,"missing prediction coverage latches fault before native close");
 q=request(Operation::Create,m);q.world=400;q.registry=200;q.root=300;q.kind=1;ck(malformed.call(q).status==uint32_t(Status::Unavailable),"late missing prediction cannot repair failed coverage by resurrection");
 auto bad=request(Operation::Stats);bad.size--;ck(b.call(bad).status==uint32_t(Status::AbiMismatch),"invalid ABI size refused");bad=request(Operation::Stats);bad.operation=999;ck(b.call(bad).status==uint32_t(Status::Invalid),"unknown operation refused");
 std::cout<<"PASS "<<n<<" World broker bounded capacity, fault, and late-drain checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<n<<": "<<e.what()<<'\n';return 1;}}
