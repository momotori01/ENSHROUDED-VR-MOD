#include <evr/bow_tracked_shot.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool x){++n;if(!x)throw std::runtime_error(std::to_string(n));};
 HandFrame f;BowPhysicalGeometry g;BowTrackedShot t;BowTrackedShotValue out;
 auto reset=[&](){f={};g={};t={};f.frame=g.frame=720;f.sessionGeneration=f.originGeneration=1;f.predictedTime=100000000;f.focused=true;
  for(auto& h:f.hands){h.gripActive=true;h.gripFlags=15;h.profile=1;}f.hands[1].grip.position={.03f,.06f,.59f};g.binding=77;g.sourceWorld=123;g.entity=1;g.worldPosition={150000000000ll,160000000000ll,170000000000ll};};
 reset();ck(t.arm(f,g,1000));ck(!t.arm(f,g,1000));f.frame=g.frame=723;ck(t.consume(f,g,1050,1,out));ck(out.frame==720&&out.geometry.binding==77);ck(!t.consume(f,g,1051,1,out));ck(!t.arm(f,g,1060));
 for(unsigned bad=0;bad<14;bad++){reset();ck(t.arm(f,g,1000));uint64_t now=1020;uint32_t entity=1;
  if(bad==0)f.focused=false;if(bad==1)f.hands[1].gripFlags=0;if(bad==2)f.hands[0].gripFlags=0;if(bad==3)++f.sessionGeneration;if(bad==4)++f.originGeneration;
  if(bad==5)++f.hands[0].profile;if(bad==6)++f.hands[1].profile;if(bad==7)++g.binding;if(bad==8)++g.sourceWorld;if(bad==9)++entity;if(bad==10)now=1201;if(bad==11)now=999;if(bad==12)f.frame=g.frame=729;if(bad==13)--f.predictedTime;
  ck(!t.consume(f,g,now,entity,out));ck(!t.consume(f,g,1020,1,out));
 }
 reset();ck(t.arm(f,g,1000));t.invalidate();ck(!t.consume(f,g,1020,1,out));
 reset();g.entity=0;ck(!t.arm(f,g,1000));reset();f.hands[1].grip.position={0,0,-.5f};ck(!t.arm(f,g,1000));
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
