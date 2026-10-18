#include <evr/readback_retirement.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::water;
int main(){unsigned checks=0;auto ck=[&](bool ok){++checks;if(!ok)throw std::runtime_error("unsafe readback retirement");};try{
 RetirementEvidence e;e.ownerStopped=e.sceneReturned=e.sameThread=true;e.completed=100;e.pending=e.capacity=2;e.slots[0]={99,0};e.slots[1]={100,0};ck(permitsPrivateDiscard(e));
 for(unsigned bit=0;bit<3;++bit){auto b=e;(bit==0?b.ownerStopped:bit==1?b.sceneReturned:b.sameThread)=false;ck(!permitsPrivateDiscard(b));}
 for(auto bad:{0ull,9ull}){auto b=e;b.capacity=bad;ck(!permitsPrivateDiscard(b));}
 {auto b=e;b.pending=3;ck(!permitsPrivateDiscard(b));b=e;b.completed=0;ck(!permitsPrivateDiscard(b));}
 {auto b=e;b.slots={};ck(!permitsPrivateDiscard(b));b.pending=0;ck(permitsPrivateDiscard(b));}
 for(uint64_t tick=1;tick<2000;++tick){for(unsigned slot=0;slot<2;++slot){auto b=e;b.completed=tick;b.pending=1;b.slots={};b.slots[slot].inFlight=tick+1;ck(!permitsPrivateDiscard(b));b.slots[slot].inFlight=tick;ck(permitsPrivateDiscard(b));b.slots[slot]={0,tick+1};ck(!permitsPrivateDiscard(b));b.slots[slot].ready=tick;ck(permitsPrivateDiscard(b));}}
 std::cout<<"PASS "<<checks<<" private readback retirement checks\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
