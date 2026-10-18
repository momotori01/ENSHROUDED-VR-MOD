#include <evr/observation_budget.hpp>
#include <evr/motion_event_retime.hpp>
#include <evr/motion_collider_lease.hpp>
#include <iostream>
#include <thread>
#include <vector>
using namespace evr;
int main(){unsigned checks=0;try{
 auto check=[&](bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);};
 for(uint64_t limit:{0ull,1ull,3ull,64ull,256ull})for(bool witness:{false,true}){
  diagnostic::ObservationBudget b(limit,witness);uint64_t records=0,witnesses=0;
  motion::EventRetimeLedger<8> effects;motion::EventActionKey key{1,2,3,4,5,0,0};motion::EventIdentity id{6,{7}};
  unsigned append=0,suppress=0;
  for(uint64_t i=0;i<1000;i++){
   const auto o=b.observe();check(o.ordinal==i+1,"record sequence is monotonic");check(!(o.record&&o.witness),"streams exclusive");records+=o.record;witnesses+=o.witness;
   // Processing is independent of evidence. Late repeated native windows must
   // remain suppressed even after the first log entry was the last one.
   const auto d=effects.consider(key,id,i==0,true,i==0?0:1,16);
   if(d==motion::EventRetime::Decision::Append){check(effects.commit(key,id),"first event commits");++append;}
   if(d==motion::EventRetime::Decision::Suppress)++suppress;
  }
  check(records==limit&&witnesses==(witness?1000-limit:0),"exact evidence allocation");check(append==1&&suppress==999,"no event replay after recording exhaustion");
  bool rejected=false;try{b.configure(2,false);}catch(const std::logic_error&){rejected=true;}check(rejected,"cannot reset live counter");
 }
 // The first invalid pose must cancel after logging has ended, and later
 // reacquisition must not revive that owned native collider. Separate actions
 // remain independent. These are value-only invariants, not native callbacks.
 for(uint64_t limit:{0ull,1ull,2ull,64ull,256ull})for(bool witness:{false,true}){
  diagnostic::ObservationBudget budget(limit,witness);motion::ColliderLeaseLedger<8> leases;
  unsigned track=0,cancel=0,recorded=0;uint64_t nativeId=0;
  for(unsigned action=0;action<2;++action){
   motion::ColliderKey key{10,uint32_t(20+action),{30}};nativeId=0;
   for(unsigned i=0;i<400;++i){
    const auto observation=budget.observe();recorded+=observation.record;
    const auto decision=leases.decide(key,i!=300,nativeId,true);
    check(decision==(i<300?motion::ColliderDecision::Track:motion::ColliderDecision::Cancel),"cancellation after log exhaustion remains latched");
    track+=decision==motion::ColliderDecision::Track;cancel+=decision==motion::ColliderDecision::Cancel;
    nativeId=decision==motion::ColliderDecision::Track?uint64_t(100+action):0;
    check(leases.returned(key,nativeId),"every owned native scope returns after log exhaustion");
   }
  }
  check(track==600&&cancel==200&&recorded==limit,"independent actions preserve cancellation under every log budget");
 }
 diagnostic::ObservationBudget shared(7,true);std::atomic<unsigned> records{0},witnesses{0},invalid{0};std::vector<std::thread> workers;
 for(unsigned t=0;t<8;t++)workers.emplace_back([&]{for(unsigned i=0;i<10000;i++){auto r=shared.observe();records+=r.record;witnesses+=r.witness;if(!r.ordinal||r.record==r.witness)++invalid;}});
 for(auto& t:workers)t.join();check(shared.seen()==80000&&records==7&&witnesses==79993&&!invalid,"parallel callbacks cannot exceed logging quota");
 std::cout<<"PASS "<<checks<<" observation budget/retiming/cancellation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
