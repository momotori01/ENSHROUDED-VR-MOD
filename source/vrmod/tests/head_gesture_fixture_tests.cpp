#include <evr/motion_attack_fixture.hpp>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
using namespace evr;using namespace evr::motion;
using Fixture=game::MotionAttackFixture;using Phase=Fixture::Phase;
struct Case{
 Fixture gate{{2,.3f,.6f,2,100000000,12,30}};HandFrame hand{};HeadGestureResult gesture{};
 game::MotionCombatObservation native{};Fixture::Result result{};uint64_t now=1000;
 Case(){gate.requireHeadGesture();hand.focused=true;hand.sessionGeneration=hand.originGeneration=1;
  for(auto& h:hand.hands){h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;h.profile=1;}
  native.entity=1;native.item=native.usedItem=20;native.binding=77;native.valid=native.alive=native.ready=true;
 }
 void step(float dx=0,unsigned fault=0){now+=10;hand.frame++;hand.predictedTime+=10000000;
  hand.hands[1].grip.position.x+=dx;gesture.frame=hand.frame;gesture.time=hand.predictedTime;
  gesture.session=hand.sessionGeneration;gesture.origin=hand.originGeneration;gesture.valid=true;
  if(fault==1)gesture.frame--;if(fault==2)gesture.time--;if(fault==3)gesture.session++;
  if(fault==4)gesture.origin++;if(fault==5)gesture.valid=false;
  result=gate.update(now,true,true,hand,now,native,fault==6?nullptr:&gesture);
 }
 void settle(){for(unsigned i=0;i<60;i++)step();}
};
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("native gesture fixture invariant "+std::to_string(n));};
 for(unsigned fault=1;fault<=6;fault++){
  Case c;c.settle();c.step(.04f,fault);check(c.result.gesture.blocked==Block::UiOrEvent&&!c.result.request&&!c.result.pulseLevel);
  for(unsigned i=0;i<8;i++){c.step(.04f);check(!c.result.request&&!c.result.pulseLevel);}
  for(unsigned i=0;i<4;i++)c.step();c.step(.04f);check(c.result.request==1&&c.result.pulseLevel==255);
 }
 {Case c;c.settle();c.gesture.resetSwing=true;c.step(.04f);
  check(!c.result.request&&c.result.gesture.blocked==Block::UiOrEvent);
  c.gesture.active=true;c.gesture.resetSwing=false;
  for(unsigned i=0;i<100;i++){c.step(i%2?.04f:-.04f);check(!c.result.request&&!c.result.pulseLevel);}
  c.gesture.active=false;c.gesture.resetSwing=true;c.step(.04f);check(!c.result.request);
  c.gesture.resetSwing=false;for(unsigned i=0;i<8;i++){c.step(.04f);check(!c.result.request);}
  for(unsigned i=0;i<4;i++)c.step();c.step(.04f);check(c.result.request==1&&c.result.pulseLevel==255);
 }
 for(bool accepted:{false,true})for(unsigned fault=0;fault<=6;fault++){
  Case c;c.settle();c.step(.04f);check(c.result.request==1);
  if(accepted){c.native.active=true;c.native.sequence=9;c.native.triggerTime=100;c.native.length=1000;c.native.currentTime=10;c.step();check(c.result.accepted);}
  const auto before=c.native;c.gesture.active=true;c.step(0,fault);
  check(c.result.phase==Phase::Aborted&&c.result.pulseLevel==0&&std::memcmp(&before,&c.native,sizeof(before))==0);
  c.gesture.active=false;for(unsigned i=0;i<4;i++)c.step();c.step(.04f);
  check(c.result.phase==Phase::Aborted&&!c.result.pulseLevel); // A cancelled diagnostic never resumes the old ticket.
 }
 // A frontend callback can reuse the same renderer frame. A missing or
 // invalidated policy must still revoke pending authority without new motion.
 for(bool accepted:{false,true})for(bool missing:{false,true}){
  Case c;c.settle();c.step(.04f);if(accepted){c.native.active=true;c.native.sequence=9;c.native.triggerTime=100;c.native.length=1000;c.native.currentTime=10;c.step();}
  const auto before=c.native;c.gesture.active=true;
  c.result=c.gate.update(c.now,true,true,c.hand,c.now,c.native,missing?nullptr:&c.gesture);
  check(c.result.phase==Phase::Aborted&&!c.result.pulseLevel&&std::memcmp(&before,&c.native,sizeof(before))==0);
 }
 {Case c;c.settle();bool rejected=false;try{c.gate.requireHeadGesture();}catch(...){rejected=true;}check(rejected);}
 // Replay the real archived CSV through HeadGesture and the native-input
 // state machine. Rendering/runtime scheduling remains a separate live test.
 {
  std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path()/"test_env/head-combat-hands.csv");check(bool(file));
  Fixture fixture{{2,.3f,.6f,2,100000000,12,30}};fixture.requireHeadGesture();
  HeadGesture mapper;HandFrame hands;hands.focused=true;hands.sessionGeneration=hands.originGeneration=1;
  game::MotionCombatObservation native;native.entity=1;native.item=native.usedItem=20;native.binding=77;native.valid=native.alive=native.ready=true;
  Fixture::Result result;unsigned requests=0,entries=0,exits=0;std::string line;
  while(std::getline(file,line)){
   if(line.empty()||line[0]=='#')continue;std::stringstream stream(line);std::vector<double> values;std::string part;
   while(std::getline(stream,part,','))values.push_back(std::stod(part));check(values.size()==29);
   const auto f=uint64_t(values[0]);if(f<364||f>600)continue;
   hands.frame=f;hands.predictedTime=f*11111111;
   for(unsigned h=0;h<2;h++){const unsigned k=1+13*h;auto& v=hands.hands[h];v.gripActive=v.grabActive=v.grab=true;v.gripFlags=15;v.profile=1;
    v.grip.position={float(values[k]),float(values[k+1]),float(values[k+2])};v.grip.orientation=euler(float(values[k+3])*pi/180,float(values[k+4])*pi/180,float(values[k+5])*pi/180);}
   GestureHead head{identityPose(),15,f,1,1,hands.predictedTime};head.pose.position={0,1.65f,0};const auto g=mapper.update(hands,head,{});
   if(g.entered){++entries;check(f==448);}if(g.left){++exits;check(f==481);}
   if(f>=521&&f<560){native.active=true;native.sequence=9;native.triggerTime=100;native.length=1000000000;native.currentTime=(f-521)*20000000;}
   else native.active=false;
   const uint64_t now=1000+(f-364)*20;result=fixture.update(now,true,true,hands,now,native,&g);
   if(result.gesture.request){++requests;check(f==520&&result.gesture.request==1);}
   if(440<=f&&f<=481)check(result.gesture.blocked==Block::UiOrEvent&&!result.request&&!result.pulseLevel);
   if(482<=f&&f<=519)check(!result.request&&!result.pulseLevel);
  }
  check(requests==1&&entries==1&&exits==1&&result.phase==Phase::Complete);
 }
 std::cout<<"PASS "<<n<<" native-fixture gesture policy checks; no live-game claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
