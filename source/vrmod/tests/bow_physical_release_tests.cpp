#include <evr/bow_physical_release.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 HandFrame f;BowPhysicalGeometry g;BowPhysicalRelease t;uint64_t now=1000;LivePadState raw;raw.buttons=LivePadMapper::X;
 auto reset=[&](){t={};f={};g={};g.binding=77;f.focused=true;f.sessionGeneration=f.originGeneration=1;for(auto& h:f.hands){h.gripActive=h.triggerActive=true;h.gripFlags=15;h.profile=1;}now=1000;};
 auto step=[&](float trigger,float pull,uint64_t delta=20,bool allowed=true){++f.frame;g.frame=f.frame;f.predictedTime+=11111111;now+=delta;f.hands[1].trigger=trigger;f.hands[1].grip.position=BowPhysicalTrial::restString+Vec3{0,0,pull};return t.update(raw,f,g,allowed,now);};
 reset();step(0,0);ck(step(1,0).leftTrigger==255);ck(!step(1,.35f,500).rightTrigger);auto p=step(0,.35f);ck(t.phase()==5&&p.leftTrigger==255&&p.rightTrigger==255&&!(p.buttons&LivePadMapper::X));
 ck(step(0,.35f,59).rightTrigger==255);p=step(0,.35f,1);ck(!p.rightTrigger&&p.leftTrigger==255);p=step(0,.35f,440);ck(!p.rightTrigger&&!p.leftTrigger&&t.phase()==6);ck(!step(1,.35f).rightTrigger);
 for(float pull:{0.f,.05f,.099f}){reset();step(0,0);step(1,0);step(1,pull,500);p=step(0,pull);ck(t.phase()==3&&!p.rightTrigger&&!p.leftTrigger);}
 reset();step(0,0);step(1,0);p=step(0,.35f,20);ck(t.phase()==3&&!p.rightTrigger);
 for(unsigned phase=0;phase<2;phase++)for(unsigned bad=0;bad<8;bad++){
  reset();step(0,0);step(1,0);step(1,.35f,500);if(phase)step(0,.35f);
  if(bad==0)f.focused=false;if(bad==1)f.hands[1].gripFlags=0;if(bad==2)++f.sessionGeneration;if(bad==3)++f.originGeneration;if(bad==4)g.binding=88;if(bad==5)++f.hands[0].profile;if(bad==6)f.predictedTime=-100000000;
  p=step(0,.35f,20,bad!=7);ck(t.phase()==4&&!p.rightTrigger&&!p.leftTrigger);ck(!step(1,.35f).rightTrigger);
 }
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
