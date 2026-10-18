#include <evr/melee_session_fixture.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned count=0;try{
 auto check=[&](bool v,const char* s){++count;if(!v)throw std::runtime_error(s);};
 const auto data=std::filesystem::path(__FILE__).parent_path().parent_path()/"test_env";
 std::ifstream profile(data/"right-hand-speed-profile.txt");std::string magic;uint32_t item=0,type=0,slot=0,dummy=0;profile>>magic>>item>>type>>slot;
 for(unsigned i=0;i<8;i++)profile>>dummy;Weapon w;w.twoHanded=true;w.offHandTolerance=.035f;Vec3 scale;
 auto& p=w.gripToModel.position;auto& q=w.gripToModel.orientation;auto& off=w.offHandGripInModel;
 profile>>p.x>>p.y>>p.z>>q.x>>q.y>>q.z>>q.w>>scale.x>>scale.y>>scale.z>>off.x>>off.y>>off.z;
 check(bool(profile)&&magic=="EVR_GRIP_PREVIEW_1","measured profile");
 using Fixture=game::MeleeSessionFixture;Fixture fixture(w,2,false);HandFrame hands;hands.focused=true;hands.sessionGeneration=hands.originGeneration=1;
 game::MotionCombatObservation native;native.entity=1;native.item=native.usedItem=item;native.binding=77;native.world=9;native.valid=native.alive=native.ready=native.canPrimary=true;
 std::string line;std::ifstream file(data/"melee-repeat-hands.csv");check(bool(file),"archived measured repeat CSV");
 unsigned requests=0,acks=0,retired=0,spam=0;uint64_t live=0;Fixture::Result result;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::stringstream stream(line);std::vector<double> v;std::string part;
  while(std::getline(stream,part,','))v.push_back(std::stod(part));check(v.size()==29,"CSV width");const auto f=uint64_t(v[0]);if(f<364||f>700)continue;
  hands.frame=f;hands.predictedTime=f*11111111;
  for(unsigned h=0;h<2;h++){const unsigned k=1+13*h;auto& s=hands.hands[h];s.gripActive=s.grabActive=true;s.grab=v[k+10]!=0;s.gripFlags=15;s.profile=1;
   s.grip.position={float(v[k]),float(v[k+1]),float(v[k+2])};s.grip.orientation=euler(float(v[k+3])*pi/180,float(v[k+4])*pi/180,float(v[k+5])*pi/180);}
  native.active=(f>=524&&f<551)||(f>=604&&f<631);
  if(native.active){native.sequence=f<600?9:10;native.triggerTime=f<600?100:200;native.trigger=1;native.length=1000000000;native.currentTime=(f-(f<600?524:604))*30000000;}
  const auto now=1000+(f-364)*20;native.nativeMs=now;result=fixture.update(now,true,true,hands,now,native);const auto c=fixture.lastCommand;
  check(result.phase!=Fixture::Phase::Aborted,"measured sequence never aborts");
  if(c.issued){requests++;check((f==520||f==600)&&c.issued==requests&&!live&&!native.active,"only new allowed physical swing issues");live=c.issued;}
  if(c.accepted){acks++;check((f==524||f==604)&&c.accepted==acks&&live==acks,"each native ack has its own request");}
  if(c.retired){retired++;check((f==551||f==631)&&c.retired==retired&&live==retired&&!c.current&&!c.revoked,"each native end retires authority");live=0;}
  if(532<=f&&f<=537){spam++;check(result.gesture.speed>3&&native.active&&!c.issued&&!result.pulseLevel&&c.current==1&&result.gesture.blocked==Block::Cooldown,"fast recovery motion refused with no queue");}
  if(538<=f&&f<600)check(!c.issued&&!result.pulseLevel,"no deferred attack after recovery");
  if(428<=f&&f<=630){w.binding=77;check(twoHandSupportValid(hands,w),"both measured grips stay supported");}
  const auto duplicate=fixture.update(now+1,true,true,hands,now,native);const auto event=fixture.lastCommand;
  check(!event.issued&&!event.accepted&&!event.retired,"no duplicate callback events");
  check(duplicate.phase!=Fixture::Phase::Aborted,"duplicate hand callback safe");
  if(f>=631)check(result.phase==Fixture::Phase::Complete&&!result.pulseLevel&&result.completed==2&&!live,"finite completion neutral; unlimited coordinator remains separate");
 }
 check(requests==2&&acks==2&&retired==2&&spam==6,"exact finite repeat lifecycle");
 std::cout<<"PASS "<<count<<" measured repeat/native-model checks; not a real-game result\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<count<<": "<<e.what()<<'\n';return 1;}}
