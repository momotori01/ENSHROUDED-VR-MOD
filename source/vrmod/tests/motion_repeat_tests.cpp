#include <evr/motion_attack_fixture.hpp>
#include <evr/motion_event_retime.hpp>
#include <evr/motion_collider_lease.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("repeat attack invariant "+std::to_string(n));};
 EventActionKey a{100,1,10,20,30,0,0},b=a;b.trigger++;
 EventIdentity first{1000,{1}},second{1000,{1}};EventRetimeLedger<2> events;using D=EventRetime::Decision;
 check(events.commit(a,first));check(events.consider(b,second,true,true,0,16)==D::Append);check(events.commit(b,second));
 for(unsigned i=0;i<1000;++i){check(events.consider(a,first,false,false,1,16)==D::Suppress);check(events.consider(b,second,false,false,1,16)==D::Suppress);check(events.consider(a,first,true,true,0,16)==D::Ignore);}
 auto third=b;third.trigger++;check(events.consider(third,second,true,true,0,16)==D::Ignore);check(!events.commit(third,second));
 auto unrelated=first;unrelated.guid[1]++;check(events.consider(a,unrelated,false,false,1,16)==D::Ignore);
 EventRetimeLedger<2> blocked;check(blocked.block(a,first));check(blocked.commit(b,second));check(blocked.consider(a,first,false,false,1,16)==D::Suppress);
 ColliderLeaseLedger<2> colliders;ColliderKey ca{100,7,{1}},cb{100,8,{1}};
 check(colliders.decide(ca,true,0,true)==ColliderDecision::Track);check(colliders.returned(ca,100));
 check(colliders.decide(cb,true,22,true)==ColliderDecision::Ignore);check(!colliders.owns(cb));
 check(colliders.decide(cb,true,0,true)==ColliderDecision::Track);check(colliders.returned(cb,200));
 check(colliders.decide(ca,false,100)==ColliderDecision::Cancel);check(colliders.returned(ca,0));
 for(unsigned i=0;i<1000;++i){check(colliders.decide(ca,true,0,true)==ColliderDecision::Cancel);check(colliders.decide(cb,true,200,true)==ColliderDecision::Track);}
 auto cc=cb;cc.world++;check(colliders.decide(cc,true,0,true)==ColliderDecision::Ignore);check(!colliders.returned(cc,200));
 game::MotionAttackFixture fixture{{2.f,.3f,.6f,2,100000000,12.f,30.f}};fixture.setAttackLimit(2);
 HandFrame f{};f.focused=true;f.sessionGeneration=f.originGeneration=1;auto& h=f.hands[1];h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;h.profile=1;
 game::MotionCombatObservation o{};o.entity=1;o.item=o.usedItem=20;o.binding=55;o.valid=o.alive=o.ready=true;
 uint64_t now=1000;game::MotionAttackFixture::Result r;
 auto step=[&](float dx=0){now+=10;f.frame++;f.predictedTime+=10000000;h.grip.position.x+=dx;r=fixture.update(now,true,true,f,now,o);};
 for(unsigned i=0;i<55;i++)step();check(r.phase==game::MotionAttackFixture::Phase::WaitSwing);
 for(unsigned attack=1;attack<=2;++attack){
  step(.04f);check(r.gesture.request==attack&&r.request==attack&&r.pulseLevel==255);
  o.active=true;o.sequence=10;o.triggerTime=100*attack;o.length=1000;o.currentTime=10;o.playedTime=0;step();check(r.accepted);
  for(unsigned i=1;i<99;i++){o.playedTime=i*10;o.currentTime=(i+1)*10;step(i%2?.04f:-.04f);check(!r.gesture.request&&r.request==attack);}
  o.active=false;for(unsigned i=0;i<30;i++)step();check(r.completed==attack&&r.pulseLevel==0);
  check(r.phase==(attack==1?game::MotionAttackFixture::Phase::WaitSwing:game::MotionAttackFixture::Phase::Complete));
 }
 for(unsigned i=0;i<40;++i){step(.04f);check(!r.gesture.request&&r.completed==2&&r.pulseLevel==0);}
 std::cout<<"PASS "<<n<<" repeated action/retime/lease checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
