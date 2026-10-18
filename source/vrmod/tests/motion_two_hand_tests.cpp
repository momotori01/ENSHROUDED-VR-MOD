#include <evr/motion_attack_fixture.hpp>
#include <iostream>
#include <cstring>
using namespace evr;using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool v){++n;if(!v)throw std::runtime_error("two hand attack invariant "+std::to_string(n));};
 using Fixture=game::MotionAttackFixture;using Phase=Fixture::Phase;
 struct Case{
  Fixture fixture{{2,.3f,.6f,2,100000000,12,30}};
  HandFrame f{};game::MotionCombatObservation o{};uint64_t now=1000;Fixture::Result result;
  Case(){Weapon w;w.twoHanded=true;w.gripToModel.position={0,0,.63f};w.offHandTolerance=.035f;fixture.setTwoHandWeapon(w,20);
   f.focused=true;f.sessionGeneration=f.originGeneration=1;
   for(auto& h:f.hands){h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;h.profile=1;}
   f.hands[0].grip.position={0,0,.63f};o.entity=1;o.item=o.usedItem=20;o.binding=55;o.valid=o.alive=o.ready=true;
  }
  void step(float dx=0){now+=10;f.frame++;f.predictedTime+=10000000;for(auto& h:f.hands)h.grip.position.x+=dx;result=fixture.update(now,true,true,f,now,o);}
  void settle(){for(unsigned i=0;i<55;i++)step();}
  void accept(){o.active=true;o.sequence=10;o.triggerTime=100;o.length=1000;o.currentTime=10;step();}
 };
 Case a;a.f.hands[0].grab=false;a.settle();
 for(unsigned i=0;i<80;i++){a.step(i%2?.04f:-.04f);check(a.result.pulseLevel==0&&!a.result.gesture.request&&a.result.phase==Phase::WaitSwing);}
 a.f.hands[0].grab=true;a.f.hands[0].grip.position.z=1;
 for(unsigned i=0;i<80;i++){a.step(i%2?.04f:-.04f);check(a.result.pulseLevel==0&&!a.result.gesture.request&&a.result.phase==Phase::WaitSwing);}
 a.f.hands[0].grip.position.z=.63f;
 a.step(.04f);check(a.result.pulseLevel==0);for(unsigned i=0;i<4;i++)a.step();a.step(.04f);check(a.result.gesture.request==1&&a.result.pulseLevel==255);
 a.accept();check(a.result.accepted&&a.result.phase!=Phase::Aborted&&a.result.gesture.haptic[0]>0&&a.result.gesture.haptic[1]>0);
 for(unsigned i=1;i<70;i++){a.o.currentTime=i*10;a.o.playedTime=a.o.currentTime-1;a.step(i%2?.04f:-.04f);check(!a.result.gesture.request&&a.result.request==1);}
 a.o.active=false;for(unsigned i=0;i<30;i++)a.step();check(a.result.phase==Phase::Complete&&a.result.completed==1);
 for(unsigned k=0;k<5;k++)for(bool accepted:{false,true}){
  Case b;b.settle();b.step(.04f);check(b.result.request==1);if(accepted)b.accept();const auto nativeBefore=b.o;
  if(k==0)b.f.hands[0].grab=false;
  if(k==1)b.f.hands[0].gripFlags=0;
  if(k==2)b.f.hands[0].grip.position.z=1;
  if(k==3)b.f.hands[0].grabActive=false;
  if(k==4)b.f.hands[0].grip.orientation={0,0,0,0}; // Off-hand rotation is not needed for the support axis.
  if(k==4){b.f.hands[0].profile++;b.step();}else b.result=b.fixture.update(b.now,true,true,b.f,b.now,b.o);
  check(b.result.phase==Phase::Aborted&&b.result.pulseLevel==0&&std::memcmp(&b.o,&nativeBefore,sizeof(b.o))==0);
 }
 Case wrong;wrong.o.item=21;wrong.step();check(wrong.result.phase==Phase::Aborted);
 Case config;config.settle();Weapon w;w.twoHanded=true;w.offHandTolerance=.035f;bool rejected=false;try{config.fixture.setTwoHandWeapon(w,20);}catch(...){rejected=true;}check(rejected);
 for(float bad:{0.f,-.1f,INFINITY,NAN}){Fixture f{{2,.3f,.6f,2,100000000,12,30}};w.offHandTolerance=bad;rejected=false;try{f.setTwoHandWeapon(w,20);}catch(...){rejected=true;}check(rejected);}
 for(float span:{0.f,.06f}){Fixture f{{2,.3f,.6f,2,100000000,12,30}};w.offHandTolerance=.035f;w.gripToModel.position={0,0,span};rejected=false;try{f.setTwoHandWeapon(w,20);}catch(...){rejected=true;}check(rejected);}
 // The same support guard is used at the physics/event consumer, including
 // an unchanged frame after a grip loss; no dependency on frontend scheduling.
 Case supportCase;Weapon support;support.twoHanded=true;support.binding=55;support.gripToModel.position={0,0,.63f};support.offHandTolerance=.035f;
 check(twoHandSupportValid(supportCase.f,support));
 for(unsigned k=0;k<12;k++){auto f=supportCase.f;auto w=support;
  switch(k){case 0:f.hands[0].grab=false;break;case 1:f.hands[1].grab=false;break;
   case 2:f.hands[0].gripFlags=0;break;case 3:f.hands[1].gripFlags=0;break;
   case 4:f.hands[0].grip.position.z=1;break;case 5:f.focused=false;break;
   case 6:f.hands[0].grip.position.x=NAN;break;case 7:f.hands[0].grip.orientation={0,0,0,0};break;
   case 8:f.hands[1].grip.orientation={0,0,0,0};break;case 9:w.binding=0;break;
   case 10:w.offHandTolerance=NAN;break;case 11:f.hands[0].grabActive=false;break;}
  check(!twoHandSupportValid(f,w));
 }
 for(float angle:{0.f,1.f,2.f,3.14159265f}){auto f=supportCase.f;const Quat q{0,std::sin(angle/2),0,std::cos(angle/2)};
  for(auto& h:f.hands){h.grip.position=rotate(q,h.grip.position)+Vec3{2,3,4};h.grip.orientation=q;}check(twoHandSupportValid(f,support));}
 std::cout<<"PASS "<<n<<" two-hand native-gate tests\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
