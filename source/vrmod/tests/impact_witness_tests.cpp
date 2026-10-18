#include <evr/impact_witness.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
struct Owner {uint32_t entity,item,event;uint64_t ephemeralQuery;};
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("Impact witness invariant");};
 auto same=[](const Owner& a,const Owner& b){return a.entity==b.entity&&a.item==b.item&&a.event==b.event;};
 ImpactWitnessLedger<Owner,3> ledger(500);ImpactWitnessKey a{100,1,10},b{100,1,11},foreign{200,1,10};
 Owner oa{20,30,40,1000},ob{21,31,41,2000},of{22,32,42,3000};
 check(!ledger.find(a,1));check(ledger.remember(a,oa,10,same)==WitnessRecord::Stored);
 check(ledger.remember(b,ob,11,same)==WitnessRecord::Stored);check(ledger.remember(foreign,of,12,same)==WitnessRecord::Stored);
 for(unsigned i=0;i<100;i++){
  check(ledger.find(a,100)->value.event==40);check(ledger.find(b,100)->value.event==41);check(ledger.find(foreign,100)->value.event==42);
 }
 check(!ledger.find({100,2,10},20));check(!ledger.find({100,1,12},20));check(!ledger.find(a,9));
 check(ledger.find(a,510).has_value());check(!ledger.find(a,511));
 check(ledger.remember({300,1,12},oa,20,same)==WitnessRecord::Full);check(ledger.find(b,20)->value.entity==21);
 auto updated=oa;updated.ephemeralQuery=5000;check(ledger.remember(a,updated,30,same)==WitnessRecord::Stored);check(ledger.find(a,30)->value.ephemeralQuery==5000);
 check(ledger.find(a,30)->first==10);check(ledger.find(a,30)->last==30);
 check(ledger.remember(a,oa,29,same)==WitnessRecord::ClockRegression);check(ledger.find(a,30)->value.ephemeralQuery==5000);
 auto collision=oa;collision.event++;check(ledger.remember(a,collision,31,same)==WitnessRecord::Conflict);check(!ledger.find(a,31));
 check(ledger.remember(a,oa,1000,same)==WitnessRecord::Conflict);check(!ledger.find(a,1000));
 check(ledger.remember({300,1,12},oa,1001,same)==WitnessRecord::Stored);check(ledger.find({300,1,12},1001)->value.event==40);check(!ledger.find(b,1001));
 check(ledger.remember({0,1,1},oa,1002,same)==WitnessRecord::Invalid);check(ledger.remember({1,0,1},oa,1002,same)==WitnessRecord::Invalid);check(ledger.remember({1,1,0},oa,1002,same)==WitnessRecord::Invalid);
 ledger.clear();check(!ledger.find(a,1002));check(ledger.remember(a,oa,1,same)==WitnessRecord::Stored);
 for(unsigned field=0;field<3;++field){ledger.clear();check(ledger.remember(a,oa,1,same)==WitnessRecord::Stored);auto q=oa;if(field==0)q.entity++;if(field==1)q.item++;if(field==2)q.event++;check(ledger.remember(a,q,2,same)==WitnessRecord::Conflict);check(!ledger.find(a,2));}
 ImpactWitnessLedger<Owner> disabled(0);check(disabled.remember(a,oa,1,same)==WitnessRecord::Invalid);
 std::cout<<"PASS "<<n<<" Impact witness overlap/expiry/isolation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
