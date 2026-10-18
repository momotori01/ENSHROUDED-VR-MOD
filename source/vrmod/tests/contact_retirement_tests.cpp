#include <evr/motion_contact_retirement.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){unsigned checks=0;try{
 auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 EventAuthority a{{11,12,13,14,15,0,0},{16},true};ColliderKey k{11,20,{}};
 std::memcpy(k.event.data(),&a.action.sequence,4);std::memcpy(k.event.data()+8,a.guid.data(),16);
 ContactBoundary b{11,21,22,12,100,200,0,a.action,true};
 for(unsigned mutation=0;mutation<19;++mutation){
  ContactBoundaryContinuity continuity;ContactRetirementGate gate;check(gate.initialize(a,k),"verified identity");
  check(continuity.observe(b)&&!gate.ready(b,false),"active never releases");
  auto idle=b;idle.tick++;idle.active=false;idle.action={};bool destroyed=true;
  switch(mutation){
   case 0:idle.running=1;break;case 1:idle.queued=1;break;case 2:idle.completed=1;break;
   case 3:idle.commandBytes=1;break;case 4:idle.commandCount=1;break;case 5:idle.selectors=1;break;case 6:idle.queries=1;break;
   case 7:idle.world++;break;case 8:idle.actor++;break;case 9:idle.registry++;break;case 10:idle.taskList++;break;
   case 11:idle.worldKind=2;break;case 12:idle.tick=99;break;case 13:idle.entityCounter=199;break;
   case 14:idle.tick=100;break;case 15:destroyed=false;break;case 16:idle.action.item=14;break;case 17:idle.action.trigger=15;break;case 18:idle.action.sequence=13;break;
  }
  check(!(continuity.observe(idle)&&gate.ready(idle,destroyed)),"incomplete or foreign fence cannot release");
  if(continuity.faulted())check(!continuity.observe(b),"continuity fault never self-recovers");
 }
 ContactRetirementGate unseen;check(unseen.initialize(a,k)&&!unseen.commit(),"init alone cannot commit");auto idle=b;idle.tick++;idle.active=false;idle.action={};check(!unseen.ready(idle,true),"idle alone insufficient");
 auto bad=k;bad.event[8]^=1;ContactRetirementGate wrong;check(!wrong.initialize(a,bad),"mismatched Impact authority");
 ContactRetirementGate changed;check(changed.initialize(a,k)&&!changed.ready(b,false),"active baseline");auto foreign=b;foreign.action.trigger++;check(!changed.ready(foreign,true)&&changed.faulted()&&!changed.ready(idle,true),"different active action latches fault");
 // Reuse exactly one slot for many actions, preserving late selection and
 // cancellation until the actual lifetime gate authorizes retirement.
 EventRetimeLedger<8> events;ColliderLeaseLedger<8> colliders;ContactBoundaryContinuity continuity;
 check(!events.configureLimit(0)&&!colliders.configureLimit(9),"invalid admission limits");check(events.configureLimit(1)&&colliders.configureLimit(1),"one-slot trial");
 for(unsigned n=0;n<10000;++n){
  a.action.trigger=15+n;k.entity=20+n;b.tick=100+4*n;b.entityCounter=200+n;b.action=a.action;
  EventIdentity id{24,a.guid};ContactRetirementGate gate;check(gate.initialize(a,k),"unique action authority");
  check(events.consider(a.action,id,true,true,0,16)==EventRetime::Decision::Append&&events.commit(a.action,id),"reused slot admits one event");
  check(colliders.decide(k,true,0,true)==ColliderDecision::Track&&colliders.returned(k,900+n),"new collider acquired");
  check(events.size()==1&&colliders.size()==1,"one occupied slot");
  check(continuity.observe(b)&&!gate.ready(b,false),"quiet active fence retains both");
  // Native Impact can vanish before its authored event becomes due.
  check(!gate.ready(b,true)&&events.consider(a.action,id,false,false,1,16)==EventRetime::Decision::Suppress,"destroyed collider still suppresses late Impact");
  check(colliders.decide(k,false,900+n,true)==ColliderDecision::Cancel&&colliders.returned(k,0),"cancellation latches");
  check(colliders.decide(k,true,0,true)==ColliderDecision::Cancel,"regrip never reopens owned collider");
  auto next=a.action;next.trigger+=1;check(!events.commit(next,id),"full single slot rejects another event");
  auto other=k;other.entity+=1;check(colliders.decide(other,true,0,true)==ColliderDecision::Ignore,"full single slot rejects another collider");
  idle=b;idle.tick++;idle.active=false;idle.action={};check(continuity.observe(idle)&&gate.ready(idle,true),"later cleared empty native boundary authorizes");
  check(events.retireAfterQuiescence(a.action)&&colliders.retireAfterQuiescence(k)&&gate.commit(),"exact pair retires once");
  check(events.size()==0&&colliders.size()==0&&!events.retireAfterQuiescence(a.action)&&!colliders.retireAfterQuiescence(k)&&!gate.ready(idle,true)&&!gate.commit(),"no duplicate retirement");
  check(!events.configureLimit(2)&&!colliders.configureLimit(2),"live limits cannot be reset after emptying");
 }
 // Pre-spawn cancellation owns only an event entry. It must not be mistaken
 // for the absence of an unobserved collider or an ordinary appended Impact.
 EventRetimeLedger<8> blocked;check(blocked.configureLimit(1),"one event-only slot");
 for(unsigned n=0;n<10000;++n){
  a.action.trigger=30000+n;b.action=a.action;b.tick=50000+4*n;b.entityCounter=40000+n;
  EventIdentity id{24,a.guid};ContactRetirementGate gate;
  check(!gate.initializeBlocked(blocked,a.action,id),"unowned event is not cancelled");
  check(blocked.block(a.action,id)&&blocked.blocked(a.action),"exact blocked event owned");
  auto foreign=id;foreign.guid[0]^=1;check(!gate.initializeBlocked(blocked,a.action,foreign),"wrong blocked Impact identity");
  check(gate.initializeBlocked(blocked,a.action,id)&&!gate.ready(b,true),"blocked active action retained");
  idle=b;idle.tick++;idle.active=false;idle.action={};
  check(!gate.ready(idle,true),"no late suppression evidence retains event-only history");
  check(blocked.consider(a.action,id,true,true,0,16)==EventRetime::Decision::Ignore,"regrip does not append blocked event");
  check(blocked.consider(a.action,id,false,false,1,16)==EventRetime::Decision::Suppress&&gate.suppressed(a.action,id),"real authored occurrence remains consumed");
  check(!gate.ready(b,true)&&!gate.commit(),"suppression alone cannot retire active action");
  check(gate.ready(idle,true)&&blocked.retireAfterQuiescence(a.action)&&gate.commit(),"cleared quiet boundary retires blocked event slot");
  check(blocked.size()==0&&!gate.ready(idle,true)&&!gate.commit(),"blocked cancellation retires once");
 }
 a.action.trigger++;EventIdentity id{24,a.guid};check(blocked.commit(a.action,id),"regular append baseline");ContactRetirementGate appended;
 check(!appended.initializeBlocked(blocked,a.action,id),"an appended event never gets event-only proof");check(blocked.retireAfterQuiescence(a.action),"test adapter cleanup");
 a.action.trigger++;b.action=a.action;ContactRetirementGate escaped;
 check(blocked.block(a.action,id)&&escaped.initializeBlocked(blocked,a.action,id)&&!escaped.ready(b,true)&&escaped.suppressed(a.action,id),"escaped-spawn baseline");
 auto other=a.action;other.entity++;check(!escaped.impactCreated(other),"foreign creation cannot alter owned gate");
 idle=b;idle.tick++;idle.active=false;idle.action={};check(escaped.ready(idle,true),"candidate before unexpected owned creation");
 check(escaped.impactCreated(a.action)&&escaped.faulted()&&!escaped.commit()&&!escaped.ready(idle,true)&&blocked.owns(a.action),"unexpected native spawn invalidates even an outstanding permit");
 std::cout<<"PASS "<<checks<<" quiescent lifetime/one-slot reuse checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
