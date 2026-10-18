#include <evr/glide_launch.hpp>
#include <iostream>
using namespace evr;using namespace evr::game;
int main(){unsigned failed=0,passed=0;auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';ok?++passed:++failed;};
 constexpr uint64_t active=(1ull<<37)|(1ull<<50);
 bool ok=true;unsigned n=0;
 for(float yaw:{-pi,-2.f,-.3f,0.f,.8f,pi})for(float pitch:{-.8f,0.f,.6f})for(float bank:{-.7f,0.f,.7f})for(float delta:{-1.5f,-.610865f,0.f,.610865f,1.5f}){
  const auto b=euler(yaw,pitch,bank),c=euler(yaw+delta,.5f,.3f);const auto r=glideLaunchRotation(b,c,active,1,0,true);
  const auto f=rotate(r.orientation,{0,0,1}),aim=rotate(c,{0,0,1}),before=rotate(b,{0,0,1});
  const auto x=rotate(r.orientation,{1,0,0}),y=rotate(r.orientation,{0,1,0});
  ok &= r.applied&&std::abs(std::remainder(std::atan2(f.x,f.z)-std::atan2(aim.x,aim.z),2*pi))<2e-5f&&std::abs(f.y-before.y)<2e-6f&&
   std::abs(x.y-rotate(b,{1,0,0}).y)<2e-6f&&std::abs(y.y-rotate(b,{0,1,0}).y)<2e-6f&&std::abs(dot(x,f))<2e-6f;
  ++n;
 }
 check(ok,"270 left/right/wraparound launches align horizontal heading and preserve pitch/bank");
 auto b=euler(pi,0,0),c=euler(pi+.610865f,0,0);auto r=glideLaunchRotation(b,c,active,1,0,true);
 check(r.applied&&length(rotate(r.orientation,{0,0,1})-Vec3{-.573576f,0,-.819152f})<2e-6f,"reproduced stationary body 180/head 215 launches at 215 degrees");
 check(!glideLaunchRotation(b,c,active,1,1,true).applied&&!glideLaunchRotation(b,c,active,1,16666667,true).applied,"later flight and head look do not re-steer the glider");
 check(!glideLaunchRotation(b,c,active,0,0,true).applied&&!glideLaunchRotation(b,c,active,1,0,false).applied,"inactive or stale/unqualified input keeps native orientation");
 check(!glideLaunchRotation(b,c,1ull<<50,1,0,true).applied&&!glideLaunchRotation(b,c,1ull<<37,1,0,true).applied&&!glideLaunchRotation(b,c,active|(1ull<<7),1,0,true).applied,"jump/roll, not-ready and dead actors retain native orientation");
 check(!glideLaunchRotation(b,euler(0,pi/2,0),active,1,0,true).applied,"ambiguous vertical aim retains native heading");
 const auto replay=glideLaunchRotation(b,c,active,1,0,true);check(length(rotate(r.orientation,{0,0,1})-rotate(replay.orientation,{0,0,1}))<1e-7f,"prediction replay is deterministic without a consumed global latch");
 std::cout<<passed<<" passed, "<<failed<<" failed; "<<n<<" geometry cases\n";return failed?1:0;
}
