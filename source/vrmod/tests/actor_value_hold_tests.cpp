#include <evr/actor_value_hold.hpp>
#include <evr/world_end_boundary.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr;using namespace evr::motion;
int main(){unsigned checks=0;try{
 auto ck=[&](bool v){++checks;if(!v)throw std::runtime_error("hold and retirement boundary");};
 ActorValueHoldBoundary hold;hold.stable=hold.admissionStopped=true;hold.destructors=1;ck(hold.permitsHold());
 for(unsigned n=0;n<12;++n){auto bad=hold;switch(n){
 case 0:bad.stable=false;break;case 1:bad.admissionStopped=false;break;
 case 2:bad.before[0]=bad.after[0]=1;break;case 3:bad.before[1]=bad.after[1]=1;break;case 4:bad.before[2]=bad.after[2]=1;break;
 case 5:bad.after[0]=1;break;case 6:bad.after[1]=1;break;case 7:bad.selectors=1;break;
 case 8:bad.queries=1;break;case 9:bad.lookups=1;break;case 10:bad.flushes=1;break;case 11:bad.destructors=0;break;}
 ck(!bad.permitsHold());}
 WorldEndBoundary end;end.observed.status=uint32_t(epoch::Status::Ok);end.observed.provider=1;end.observed.serial=2;end.observed.world=3;end.observed.registry=4;end.observed.root=5;
 end.observed.phase=uint32_t(epoch::Phase::Ended);end.observed.pins=1;end.observed.flags=epoch::flagActive;end.leaseHandle=6;
 end.owner=worldStamp(end.observed);end.consumersStopped=true;ck(end.permits(end.owner));
 for(unsigned n=1;n<=128;++n){hold.destructors=n;end.destructors=n;ck(hold.permitsHold());ck(!end.permits(end.owner));}
 end.destructors=0;ck(end.permits(end.owner));end.consumersStopped=false;ck(!end.permits(end.owner));
 std::cout<<"PASS "<<checks<<" provisional hold versus final global drain checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
