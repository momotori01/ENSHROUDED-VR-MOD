#include <evr/motion_contact_lifetime.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){unsigned checks=0;try{
 auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 ContactLifetimePool<8> pool;EventRetimeLedger<8> events;ColliderLeaseLedger<8> leases;
 check(pool.configureLimit(1)&&events.configureLimit(1)&&leases.configureLimit(1),"all storage limited to one slot");
 EventAuthority a{{11,12,13,14,15,0,0},{16},true};EventIdentity id{24,a.guid};ColliderKey k{11,20,{}};
 std::memcpy(k.event.data(),&a.action.sequence,4);std::memcpy(k.event.data()+8,a.guid.data(),16);
 ContactBoundary b{11,21,22,12,100,200,0,a.action,true};ContactBoundaryContinuity continuity;
 ContactLifetimeToken old;
 // All native identity/address values intentionally repeat. Only this local
 // callback token changes. Native World-generation proof remains external.
 for(unsigned n=0;n<30000;++n){
  const bool blocked=n%3==1,cancelled=n%3==2;b.tick=100+4*n;
  check(events.commit(a.action,id,blocked),"next action event slot reusable");
  auto t=blocked?pool.acquireBlocked(events,a.action,id):pool.acquire(a,k);
  check(t&&t.slot==0&&t.serial==n+1&&pool.size()==1,"one watch slot, new serial every action");
  check(!pool.configureLimit(2),"live limit immutable even after retirement");
  check(!pool.acquire(a,k)&&!pool.acquireBlocked(events,a.action,id),"no duplicate or alias admission");
  if(old){
   check(!pool.snapshot(old).used&&!pool.beginDestroy(old,k)&&!pool.finishDestroy(old,k,true),"stale destroy callbacks cannot mutate new identical owner");
   check(!pool.suppressed(old,a.action,id)&&!pool.unexpectedImpact(old,a.action)&&!pool.ready(old,b)&&!pool.retire(old),"stale selector/fence/commit cannot affect current action");
  }
  check(continuity.observe(b)&&!pool.ready(t,b)&&!pool.retire(t),"active retained");
  auto idle=b;idle.tick++;idle.active=false;idle.action={};
  if(blocked){
   check(!pool.beginDestroy(t,k)&&!pool.finishDestroy(t,k,true),"blocked event cannot use collider proof");
   check(!pool.ready(t,idle),"blocked requires actual authored suppression");
   auto wrong=id;wrong.variant++;check(!pool.suppressed(t,a.action,wrong),"wrong event identity rejected");
   check(pool.suppressed(t,a.action,id),"late blocked suppression");
  }else{
   check(leases.decide(k,true,0,true)==ColliderDecision::Track&&leases.returned(k,100),"native collider lease");
   if(cancelled)check(leases.decide(k,false,100,true)==ColliderDecision::Cancel&&leases.returned(k,0)&&leases.decide(k,true,0,true)==ColliderDecision::Cancel,"post-spawn cancellation cannot reopen on regrip");
   check(!pool.ready(t,idle)&&!pool.finishDestroy(t,k,true),"idle and unsolicited destroy completion insufficient");
   auto wrong=k;wrong.entity++;check(!pool.beginDestroy(t,wrong),"foreign entity does not start destruction");
   check(pool.beginDestroy(t,k)&&!pool.beginDestroy(t,k)&&!pool.ready(t,idle),"in-flight destruction pins slot");
   check(pool.finishDestroy(t,k,true)&&!pool.finishDestroy(t,k,true),"exact destruction finishes once");
   check(!pool.suppressed(t,a.action,id)&&!pool.unexpectedImpact(t,a.action),"normal append cannot become blocked-event evidence");
  }
  check(events.consider(a.action,id,false,false,1,16)==EventRetime::Decision::Suppress,"history retained after native Impact end");
  auto busy=idle;busy.selectors=1;check(!pool.ready(t,busy)&&!pool.retire(t),"callback in flight prevents retirement");
  check(continuity.observe(idle)&&pool.ready(t,idle),"later native quiet cleared fence permits");
  check(events.retireAfterQuiescence(a.action)&&(blocked||leases.retireAfterQuiescence(k))&&pool.retire(t),"all owned storage released once");
  check(pool.size()==0&&events.size()==0&&leases.size()==0&&!pool.snapshot(t).used&&!pool.retire(t),"no leaked slot or duplicate commit");old=t;
 }
 check(events.block(a.action,id),"unexpected Impact test owns blocked event");auto t=pool.acquireBlocked(events,a.action,id);
 check(t&&!pool.ready(t,b)&&pool.suppressed(t,a.action,id),"blocked candidate baseline");auto idle=b;idle.tick++;idle.active=false;idle.action={};
 check(pool.ready(t,idle)&&pool.unexpectedImpact(t,a.action)&&pool.faulted(t)&&!pool.retire(t)&&!pool.ready(t,idle),"unexpected spawn cancels outstanding retirement permit");
 check(events.owns(a.action)&&pool.size()==1,"fault retains evidence rather than reusing storage");
 std::cout<<"PASS "<<checks<<" serial-safe mixed action lifetime checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
