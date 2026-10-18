#include <evr/motion_attack_fixture.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool v){++n;if(!v)throw std::runtime_error("right-hand sequence invariant "+std::to_string(n));};
 const auto data=std::filesystem::path(__FILE__).parent_path().parent_path()/"test_env";
 std::ifstream profile(data/"right-hand-speed-profile.txt");std::string magic;uint32_t item=0,type=0,slot=0,dummy=0;profile>>magic>>item>>type>>slot;
 for(unsigned i=0;i<8;i++)profile>>dummy;Weapon w;w.twoHanded=true;w.offHandTolerance=.035f;Vec3 scale;
 auto& p=w.gripToModel.position;auto& q=w.gripToModel.orientation;auto& off=w.offHandGripInModel;
 profile>>p.x>>p.y>>p.z>>q.x>>q.y>>q.z>>q.w>>scale.x>>scale.y>>scale.z>>off.x>>off.y>>off.z;
 check(bool(profile)&&magic=="EVR_GRIP_PREVIEW_1");
 using Fixture=game::MotionAttackFixture;Fixture fixture{{2,.3f,.6f,2,100000000,12,30}};fixture.setTwoHandWeapon(w,item);
 HandFrame hands;hands.focused=true;hands.sessionGeneration=hands.originGeneration=1;
 game::MotionCombatObservation native;native.entity=1;native.item=native.usedItem=item;native.binding=77;native.valid=native.alive=native.ready=true;
 Fixture::Result result;unsigned requests=0,leftFrames=0,rightFrames=0;std::string line;std::ifstream file(data/"right-hand-speed-hands.csv");check(bool(file));
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::stringstream stream(line);std::vector<double> v;std::string part;
  while(std::getline(stream,part,','))v.push_back(std::stod(part));check(v.size()==29);const auto f=uint64_t(v[0]);if(f<364||f>600)continue;
  hands.frame=f;hands.predictedTime=f*11111111;
  for(unsigned h=0;h<2;h++){const unsigned k=1+13*h;auto& s=hands.hands[h];s.gripActive=s.grabActive=true;s.grab=v[k+10]!=0;s.gripFlags=15;s.profile=1;
   s.grip.position={float(v[k]),float(v[k+1]),float(v[k+2])};s.grip.orientation=euler(float(v[k+3])*pi/180,float(v[k+4])*pi/180,float(v[k+5])*pi/180);}
  if(f>=521&&f<560){native.active=true;native.sequence=9;native.triggerTime=100;native.length=1000000000;native.currentTime=(f-521)*20000000;}
  else native.active=false;
  const uint64_t now=1000+(f-364)*20;result=fixture.update(now,true,true,hands,now,native);
  check(result.phase!=Fixture::Phase::Aborted);
  if(result.gesture.request){++requests;check(f==520&&result.gesture.request==1&&length(result.gesture.controllerVelocity[0])==0&&result.gesture.speed>3);}
  if(500<=f&&f<=512){++leftFrames;check(result.gesture.blocked==Block::None&&result.gesture.speed==0&&!result.request&&!result.pulseLevel);if(f!=506)check(length(result.gesture.controllerVelocity[0])>3);}
  if(520<=f&&f<=525){++rightFrames;check(length(result.gesture.controllerVelocity[0])==0&&result.gesture.speed>3);}
 }
 check(requests==1&&leftFrames==13&&rightFrames==6&&result.phase==Fixture::Phase::Complete);
 std::cout<<"PASS "<<n<<" right-hand-only archived sequence checks; no native-game claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
