#include <evr/bow_physical_trial.hpp>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool x){++n;if(!x)throw std::runtime_error(std::to_string(n));};
 HandFrame f;f.focused=true;f.sessionGeneration=f.originGeneration=1;
 for(auto& h:f.hands){h.gripActive=h.triggerActive=true;h.gripFlags=15;h.profile=1;}
 BowPhysicalGeometry g;g.binding=77;BowPhysicalTrial t;LivePadState raw;raw.rightTrigger=255;raw.buttons=LivePadMapper::X|LivePadMapper::A;
 auto step=[&](float trigger,Vec3 local,bool allowed=true){++f.frame;f.predictedTime+=11111111;g.frame=f.frame;
  f.hands[1].trigger=trigger;f.hands[1].grip.position=g.model.position+rotate(g.model.orientation,local);return t.update(raw,f,g,allowed);};
 const Vec3 rest=BowPhysicalTrial::restString;
 ck(!step(1,rest).leftTrigger&&t.phase()==BowPhysicalTrial::Waiting);
 ck(!step(0,rest).leftTrigger&&t.phase()==BowPhysicalTrial::Armed);
 ck(!step(1,rest+Vec3{.5f,0,0}).leftTrigger&&t.phase()==BowPhysicalTrial::Waiting);
 ck(!step(1,rest).leftTrigger);step(0,rest);
 auto v=step(1,rest);ck(v.leftTrigger==255&&!v.rightTrigger&&v.buttons==LivePadMapper::A);
 v=step(1,rest+Vec3{0,0,.35f});ck(v.leftTrigger==255&&std::abs(t.metrics().pull-.35f)<1e-6f);
 ck(t.metrics().lateral<1e-6f);step(1,rest);v=step(0,rest);ck(!v.leftTrigger&&!v.rightTrigger&&t.phase()==BowPhysicalTrial::Cancelled);
 ck(!step(0,rest).leftTrigger&&!step(1,rest).leftTrigger);
 // The result must be unchanged under a common rigid movement and model scale.
 for(float yaw:{0.f,.7f,-1.57f,3.f}){
  t={};g.model={euler(yaw,.4f,-.2f),{3,2,-5}};g.scale={1.1f,1.2f,.8f};const Vec3 r{0,rest.y*g.scale.y,rest.z*g.scale.z};
  step(0,r);ck(step(1,r).leftTrigger==255);step(1,r+Vec3{0,0,.35f});ck(std::abs(t.metrics().pull-.35f)<2e-6f&&t.metrics().lateral<2e-6f);
 }
 // Every scope loss or implausible motion cancels without any shoot target.
 for(unsigned kind=0;kind<14;kind++){
  t={};g={};g.binding=77;f.focused=true;f.sessionGeneration=f.originGeneration=1;
  for(auto& h:f.hands){h.gripActive=h.triggerActive=true;h.gripFlags=15;h.profile=1;}
  step(0,rest);ck(step(1,rest).leftTrigger==255);
  if(kind==0)f.focused=false;if(kind==1)f.hands[0].gripFlags=0;if(kind==2)f.hands[1].gripFlags=0;
  if(kind==3)++f.sessionGeneration;if(kind==4)++f.originGeneration;if(kind==5)g.binding=88;
  if(kind==6)++f.hands[1].profile;if(kind==7)f.predictedTime+=10000000001ll;
  if(kind==8)f.predictedTime-=1000000000ll;if(kind==9)f.hands[1].triggerActive=false;
  if(kind==10)g.scale.z=std::numeric_limits<float>::quiet_NaN();if(kind==11)g.model.orientation.x=2;
  v=step(1,kind==12?rest+Vec3{.36f,0,0}:rest,kind!=13);
  ck(t.phase()==BowPhysicalTrial::Aborted&&!v.leftTrigger&&!v.rightTrigger&&!(v.buttons&LivePadMapper::X));
 }
 t={};g={};g.binding=77;f.focused=true;for(auto& h:f.hands){h.gripFlags=15;h.triggerActive=true;}
 step(0,rest);step(1,rest);++f.frame;v=t.update(raw,f,g,true);ck(t.phase()==BowPhysicalTrial::Aborted&&!v.leftTrigger);
 std::cout<<n<<" PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
