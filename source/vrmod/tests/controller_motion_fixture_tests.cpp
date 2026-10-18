#include <evr/controller_motion_fixture.hpp>
#include <iostream>
#include <vector>
using F=evr::game::ControllerMotionFixture;using P=F::Phase;using S=evr::game::MotionObservation;
int main(){unsigned passed=0,failed=0;auto check=[&](bool v,const char*n){std::cout<<(v?"PASS ":"FAIL ")<<n<<'\n';v?++passed:++failed;};
 for(bool twice:{false,true})for(uint64_t step:{10,100,180}){
  F f(twice);S s{1,4,0,10,0,true};uint64_t launched=0;uint16_t last=0;std::vector<uint16_t> presses;std::vector<uint32_t> captures;
  for(uint64_t t=0;t<10000;t+=step){
   if(launched){s.camera=t-launched<1500?43:4;s.y=s.camera==43?10.5:10;s.actorState=s.camera==43?((1ull<<1)|(1ull<<32)):1;}
   auto r=f.update(t,true,1,0,s);if(r.buttons&&r.buttons!=last)presses.push_back(r.buttons);last=r.buttons;if(r.captureTag)captures.push_back(r.captureTag);
   if(r.phase==P::PressFirst&&!launched)launched=t;
   if(r.phase==P::Complete)break;
  }
  check(f.phase()==P::Complete&&presses==std::vector<uint16_t>(twice?2:1,4096)&&captures==std::vector<uint32_t>{1,2},"single/double diagnostic only A and actual air/land captures across frame rates");
 }
 {F f;S s{1,4,0,10,0,true};f.update(0,true,1,0,s);f.update(500,true,1,0,s);s.y=10.5;s.actorState=(1ull<<1)|(1ull<<32);f.update(620,true,1,0,s);check(f.update(630,true,1,0,s).phase==P::Airborne,"default Camera4 can be a verified native airborne actor");}
 for(unsigned scenario=0;scenario<9;scenario++){
  F f(true);S s{1,4,0,10,0,true};f.update(100,true,1,0,s);
  if(scenario==0)s.entity=2;if(scenario==1)s.x=.3;if(scenario==2)s.y=15;if(scenario==3)s.valid=false;if(scenario==4)s.camera=31;
  auto r=f.update(scenario==5?99:scenario==6?13000:200,scenario!=7,scenario==8?4:1,0,s);
  check(r.phase==P::Aborted&&!r.buttons,"owner/motion bound/validity/camera/clock/deadline/disconnect/UI failure releases");
 }
 F noJump(true);S s{1,4,0,10,0,true};noJump.update(0,true,1,0,s);noJump.update(500,true,1,0,s);noJump.update(620,true,1,0,s);
 check(noJump.update(2500,true,1,0,s).phase==P::Aborted,"unobserved first jump cannot emit second jump");
 for(uint64_t step:{10,100,180}){
  F f(false,true);S s{1,4,0,10,0,true};uint64_t launch=0,glide=0;bool captured=false;std::vector<uint16_t> presses;uint16_t last=0;
  for(uint64_t t=0;t<10000;t+=step){
   if(launch){s.y=10.5;s.actorState=(1ull<<1)|(1ull<<32);}
   if(glide){s.camera=t-glide<500?25:4;s.actorState=t-glide<500?(1ull<<37):t-glide<800?9:1;s.x=3;s.y=t-glide<500?10.4:10;}
   auto r=f.update(t,true,1,0,s);if(r.buttons&&r.buttons!=last)presses.push_back(r.buttons);last=r.buttons;
   if(r.phase==P::PressFirst&&!launch)launch=t;if(r.phase==P::PressSecond&&!glide)glide=t;
   if(r.captureTag==1)captured=s.camera==25&&bool(s.actorState&(1ull<<37));if(r.phase==P::Complete)break;
  }
  check(f.phase()==P::Complete&&captured&&presses==std::vector<uint16_t>{4096,4096},"explicit glider mode follows native motion and captures confirmed Camera25 only");
 }
 {
  F f(false,true);S s{1,19,0,843.5,0,true};
  f.update(0,true,1,0,s);auto first=f.update(500,true,1,0,s);
  auto released=f.update(670,true,1,0,s);s.y=843.6574;s.actorState=(1ull<<1)|(1ull<<32);
  auto second=f.update(840,true,1,0,s);
  s.y=844.8;s.camera=25;s.actorState=1ull<<37;auto airborne=f.update(1010,true,1,0,s);
  s.y=843.470776;s.actorState=(1ull<<37)|1;auto contact=f.update(1180,true,1,0,s);
  s.y=843.13796;s.camera=4;s.actorState=9;auto roll=f.update(1350,true,1,0,s);
  s.y=843.5;s.actorState=1;auto settle=f.update(1520,true,1,0,s);
  s.y=843.7;s.actorState=1ull<<32;auto bounce=f.update(1690,true,1,0,s);
  s.y=843.5;s.actorState=1;auto restart=f.update(1860,true,1,0,s);
  auto land=f.update(2200,true,1,0,s);auto complete=f.update(2600,true,1,0,s);
  check(first.buttons==4096&&released.buttons==0&&second.buttons==4096&&second.phase==P::PressSecond,
        "slow GPU glide has two A edges separated by a released packet and native air proof");
  check(airborne.captureTag==1&&contact.phase==P::WaitLand&&roll.phase==P::WaitLand&&!contact.buttons&&!roll.buttons,
        "recorded floor contact plus hangglide then 0.36m landing roll cannot complete landing");
  check(settle.phase==P::ObserveLand&&bounce.phase==P::WaitLand&&restart.phase==P::ObserveLand&&land.captureTag==2&&complete.phase==P::Complete,
        "valid native bounce restarts continuous grounded settling without further input");
 }
 {
  F f(false,true);S s{1,4,0,10,0,true};f.update(0,true,1,0,s);f.update(500,true,1,0,s);f.update(670,true,1,0,s);
  s.actorState=1ull<<32;s.y=10.5;f.update(840,true,1,0,s);s.camera=25;s.actorState=1ull<<37;f.update(1010,true,1,0,s);
  s.camera=4;s.actorState=9;s.y=9.65;f.update(1180,true,1,0,s);auto r=f.update(12001,true,1,0,s);
  check(r.phase==P::Aborted&&!r.buttons,"unsettled landing retains global timeout and releases all input");
 }
 std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
