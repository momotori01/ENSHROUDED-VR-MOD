#include <evr/motion_collider_lease.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("collider lease invariant");};
 ColliderKey key{100,1,{}};key.event[0]=20;
 ColliderLease lease;check(lease.decide(key,false,0)==ColliderDecision::Ignore);check(!lease.owns(key));
 check(lease.decide(key,true,12)==ColliderDecision::Ignore);check(!lease.owns(key));
 check(lease.decide(key,true,0)==ColliderDecision::Track);lease.returned(12);
 for(unsigned mutation=0;mutation<3;++mutation){auto foreign=key;if(mutation==0)foreign.world++;if(mutation==1)foreign.entity++;if(mutation==2)foreign.event[8]++;
  check(!lease.owns(foreign));check(lease.decide(foreign,false,12)==ColliderDecision::Ignore);check(!lease.cancelled());}
 for(unsigned i=0;i<1000;i++){check(lease.decide(key,true,12)==ColliderDecision::Track);lease.returned(12);}
 check(lease.decide(key,false,12)==ColliderDecision::Cancel);lease.returned(0);
 for(unsigned i=0;i<1000;i++){check(lease.decide(key,true,0)==ColliderDecision::Cancel);lease.returned(0);}
 ColliderLease changed;check(changed.decide(key,true,0)==ColliderDecision::Track);changed.returned(12);check(changed.decide(key,true,13)==ColliderDecision::Cancel);
 for(unsigned i=0;i<2;i++){auto invalid=key;if(i)invalid.world=0;else invalid.entity=0;ColliderLease empty;check(empty.decide(invalid,true,0)==ColliderDecision::Ignore);check(!empty.owns(invalid));}
 ColliderLease initialLost;check(initialLost.decide(key,false,0,true)==ColliderDecision::Cancel);check(initialLost.owns(key));check(initialLost.cancelled());initialLost.returned(0);
 for(unsigned i=0;i<1000;++i)check(initialLost.decide(key,true,0,true)==ColliderDecision::Cancel);
 auto foreign=key;foreign.entity++;check(initialLost.decide(foreign,false,0,true)==ColliderDecision::Ignore);
 ColliderLease existing;check(existing.decide(key,false,12,true)==ColliderDecision::Ignore);check(!existing.owns(key));
 std::cout<<"PASS "<<n<<" collider lease isolation/cancellation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
