#include <evr/motion_attack_fixture.hpp>
#include <iostream>
using namespace evr;using namespace evr::game;
unsigned count=0;void check(bool ok){++count;if(!ok)throw std::runtime_error("motion attack invariant "+std::to_string(count));}
struct Trial {
 MotionAttackFixture trial{{2.f,.3f,.6f,2,100000000,12.f,30.f}};motion::HandFrame f{};MotionCombatObservation o{};uint64_t now=1000;
 bool connected=true,ui=true;MotionAttackFixture::Result r;
 Trial(){f.focused=true;f.sessionGeneration=f.originGeneration=1;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;
  auto& h=f.hands[1];h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;h.profile=1;}
 void step(float dx=0){now+=10;f.frame++;f.predictedTime+=10000000;f.hands[1].grip.position.x+=dx;r=trial.update(now,connected,ui,f,now,o);}
 void arm(){for(unsigned i=0;i<55;i++)step();check(r.phase==MotionAttackFixture::Phase::WaitSwing&&r.pulseLevel==0);}
 void swing(){step(.04f);check(r.phase==MotionAttackFixture::Phase::Press&&r.pulseLevel==255&&r.request==1);}
 void native(){o.active=true;o.usedItem=o.item;o.sequence=10;o.triggerTime=100;o.length=1000;o.currentTime=10;step();check(r.accepted);}
};
int main(){try{
 {Trial t;t.arm();for(int i=0;i<200;i++)t.step();check(t.r.request==0);t.swing();
  // Same XR sample with another frontend callback cannot become another attack.
  auto duplicate=t.trial.update(t.now+1,true,true,t.f,t.now,t.o);check(duplicate.request==1&&duplicate.pulseLevel==255);
  t.native();double previous=1000;for(int i=1;i<99;i++){t.o.playedTime=i*10;t.o.currentTime=(i+1)*10;t.step(i%2?.04f:-.04f);check(t.r.request==1&&t.r.remaining<=previous);previous=t.r.remaining;}
  t.o.active=false;for(int i=0;i<30;i++)t.step();check(t.r.phase==MotionAttackFixture::Phase::Complete&&t.r.pulseLevel==0&&t.r.accepted);
  for(int i=0;i<50;i++){t.step(.04f);check(t.r.phase==MotionAttackFixture::Phase::Complete&&t.r.request==1&&t.r.pulseLevel==0);}}
 for(unsigned fault=0;fault<13;fault++){
  Trial t;t.arm();t.swing();switch(fault){case 0:t.ui=false;break;case 1:t.connected=false;break;case 2:t.o.binding++;break;case 3:t.o.alive=false;break;case 4:t.o.ready=false;break;case 5:t.f.focused=false;break;case 6:t.f.hands[1].grab=false;break;
   case 7:t.f.originGeneration++;break;case 8:t.f.sessionGeneration++;break;case 9:t.f.hands[1].grip.position.x+=1;break;case 10:t.o.active=true;t.o.usedItem=999;break;case 11:t.now+=30100;break;case 12:t.o.entity++;break;}
  t.step();check(t.r.phase==MotionAttackFixture::Phase::Aborted&&t.r.pulseLevel==0);
 }
 {Trial t;t.arm();t.swing();t.native();t.o.sequence++;t.step();check(t.r.phase==MotionAttackFixture::Phase::Aborted);}
 {Trial t;t.arm();t.swing();auto r=t.trial.update(t.now+201,true,true,t.f,t.now,t.o);check(r.phase==MotionAttackFixture::Phase::Aborted&&r.pulseLevel==0);}
 {Trial t;t.arm();t.o.active=true;t.step();check(t.r.phase==MotionAttackFixture::Phase::Aborted);}
 {Trial t;t.arm();t.swing();for(int i=0;i<3100;i++)t.step();check(t.r.phase==MotionAttackFixture::Phase::Aborted&&t.r.pulseLevel==0&&!t.r.accepted);}
 std::cout<<"PASS "<<count<<" native motion attack protocol checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
