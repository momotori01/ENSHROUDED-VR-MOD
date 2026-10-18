#include <evr/retained_event_scope.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool v){++n;if(!v)throw std::runtime_error("retained event scope "+std::to_string(n));};
 RetainedEventScope s;s.actorArgument=0x1234;s.registryArgument=40;s.action.world=10;s.action.entity=1;s.action.item=2;s.action.sequence=3;s.action.trigger=4;
 s.action.epoch.provider=1;s.action.epoch.serial=2;s.action.epoch.world=10;s.action.epoch.registry=20;s.action.epoch.root=30;s.action.epoch.slot=0;s.action.epoch.kind=0;
 check(s.valid());check(s.acceptsCall(0x1234,40,s.action.epoch));check(s.matchesAction(s.action,1));
 check(!s.acceptsCall(0x1235,40,s.action.epoch));check(!s.acceptsCall(0x1234,41,s.action.epoch));check(!s.acceptsCall(0x1234,40,{}));
 for(unsigned i=0;i<7;++i){auto e=s.action.epoch;switch(i){case 0:++e.provider;break;case 1:++e.serial;break;case 2:++e.slot;break;case 3:++e.world;break;case 4:++e.registry;break;case 5:++e.root;break;case 6:e.kind=1;break;}check(!s.acceptsCall(0x1234,40,e));}
 for(unsigned i=0;i<8;++i){auto a=s.action;uint32_t trigger=1;switch(i){case 0:++a.world;break;case 1:++a.entity;break;case 2:++a.sequence;break;case 3:++a.item;break;case 4:++a.trigger;break;case 5:++a.call;break;case 6:++a.sub;break;case 7:trigger=2;break;}check(!s.matchesAction(a,trigger));}
 // Same native address reused in a subsequent World cannot authorize the old
 // action, even when the action's numeric entity/sequence/trigger coincide.
 for(unsigned i=1;i<=10000;++i){auto e=s.action.epoch;e.serial+=i;check(!s.acceptsCall(s.actorArgument,s.registryArgument,e));}
 // A sampler that would read inaccessible native state is never invoked for
 // foreign identities. Repeated matching calls never cache a successful epoch.
 unsigned sampled=0;auto current=s.action.epoch;
 auto sample=[&](uintptr_t world){++sampled;check(world==s.action.world);return current;};
 for(unsigned i=1;i<=10000;++i){
  check(!s.acceptsSampledCall(s.actorArgument+i,s.registryArgument,sample));
  check(!s.acceptsSampledCall(s.actorArgument,s.registryArgument+i,sample));
 }
 check(sampled==0);
 check(s.acceptsSampledCall(s.actorArgument,s.registryArgument,sample));check(sampled==1);
 ++current.serial;check(!s.acceptsSampledCall(s.actorArgument,s.registryArgument,sample));check(sampled==2);
 current={};check(!s.acceptsSampledCall(s.actorArgument,s.registryArgument,sample));check(sampled==3);
 current=s.action.epoch;check(s.acceptsSampledCall(s.actorArgument,s.registryArgument,sample));check(sampled==4);
 auto invalid=s;invalid.action.epoch={};check(!invalid.acceptsSampledCall(s.actorArgument,s.registryArgument,sample));check(sampled==4);
 bool threw=false;try{s.acceptsSampledCall(s.actorArgument,s.registryArgument,[](uintptr_t)->WorldStamp{throw std::runtime_error("sample unavailable");});}catch(...){threw=true;}check(threw);
 std::cout<<"PASS "<<n<<" retained native-argument identity checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
