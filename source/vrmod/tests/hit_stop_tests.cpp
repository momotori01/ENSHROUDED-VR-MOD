#include <evr/hit_stop.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
namespace {
// Both hands tracked: the right at `right` turned `yaw` about +Y, the left still at (-0.3, 1, 0).
HandFrame frame(Vec3 right,float yaw=0){
 HandFrame f;f.focused=true;
 for(unsigned i=0;i<2;++i){auto& h=f.hands[i];h.gripActive=true;h.gripFlags=15;h.aimActive=true;h.aimFlags=15;h.grip=identityPose();h.grip.position=i?right:Vec3{-.3f,1.f,0};
  h.aim=compose(h.grip,Pose{{0,0,0,1},{0,0,-.05f}});}
 f.hands[1].grip.orientation=euler(yaw,0,0);f.hands[1].aim=compose(f.hands[1].grip,Pose{{0,0,0,1},{0,0,-.05f}});
 return f;
}
float apart(Vec3 a,Vec3 b){return length(a-b);}
}
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 // The weapon classes' stops
 ck(hitStopSeconds(false,false)==.15&&hitStopSeconds(false,true)==.25&&hitStopSeconds(true,false)==.3&&hitStopSeconds(true,true)==.3);
 // No stop: the hands as they come
 {HitStop s;auto f=frame({.3f,1,0});auto o=s.apply(f,0);ck(apart(o.hands[1].grip.position,f.hands[1].grip.position)<1e-6f&&!s.active(1));}
 // A stop of the right hand for 0.1 s: it stays where it was shown, the left goes on; then it chases to the tracked hand
 {HitStop s;double t=0;s.apply(frame({.3f,1,0}),t);
  s.stop(2,.1);
  t+=.011;auto o=s.apply(frame({.3f,1,-.1f},.3f),t);
  ck(s.active(1)&&apart(o.hands[1].grip.position,{.3f,1,0})<1e-6f&&std::abs(o.hands[1].grip.orientation.w-1)<1e-6f);
  ck(apart(o.hands[1].aim.position,{.3f,1,-.05f})<1e-5f);                       // the aim stops with the grip
  ck(apart(o.hands[0].grip.position,{-.3f,1,0})<1e-6f&&!s.active(0));
  t+=.05;o=s.apply(frame({.3f,1,-.3f},.6f),t);ck(apart(o.hands[1].grip.position,{.3f,1,0})<1e-6f);
  // 0.1 s over: the chase begins at the stopped pose
  t=.112;o=s.apply(frame({.3f,1,-.4f},.6f),t);ck(s.active(1)&&apart(o.hands[1].grip.position,{.3f,1,0})<1e-4f);
  // after 0.04 s about 37 % of the difference is left; the hand moves toward the tracked one
  t+=.04;o=s.apply(frame({.3f,1,-.4f},.6f),t);{const float d=apart(o.hands[1].grip.position,{.3f,1,-.4f});ck(d>.1f&&d<.2f);}
  // its aim keeps the grip's frame while it chases
  {const Vec3 local=rotate(inverse(o.hands[1].grip.orientation),o.hands[1].aim.position-o.hands[1].grip.position);ck(apart(local,{0,0,-.05f})<1e-4f);}
  // in the end it is shown as tracked again
  for(int k=0;k<40;++k){t+=.011;o=s.apply(frame({.3f,1,-.4f},.6f),t);}
  ck(!s.active(1)&&apart(o.hands[1].grip.position,{.3f,1,-.4f})<1e-6f&&s.stops==1);}
 // Hitting again while stopped: it stops again where it is shown, for the new length
 {HitStop s;double t=0;s.apply(frame({.3f,1,0}),t);s.stop(2,.1);t=.01;s.apply(frame({.3f,1,-.1f}),t);
  t=.08;s.stop(2,.1);t=.09;auto o=s.apply(frame({.3f,1,-.2f}),t);ck(apart(o.hands[1].grip.position,{.3f,1,0})<1e-6f);
  t=.15;o=s.apply(frame({.3f,1,-.2f}),t);ck(apart(o.hands[1].grip.position,{.3f,1,0})<1e-6f&&s.stops==2);   // 0.09 + 0.1
  // while chasing, a hit stops it where it is shown then
  t=.2;o=s.apply(frame({.3f,1,-.2f}),t);t=.22;o=s.apply(frame({.3f,1,-.2f}),t);const Vec3 at=o.hands[1].grip.position;ck(apart(at,{.3f,1,0})>.01f);
  s.stop(2,.2);t=.23;o=s.apply(frame({.3f,1,-.5f}),t);ck(apart(o.hands[1].grip.position,at)<1e-6f);}
 // Both hands (a two-hand weapon); a hand that loses tracking is shown as tracked, the stop forgotten
 {HitStop s;double t=0;s.apply(frame({.3f,1,0}),t);s.stop(3,.2);t=.01;auto f=frame({.3f,1,-.1f});f.hands[0].grip.position={-.3f,1,-.1f};
  auto o=s.apply(f,t);ck(s.active(0)&&s.active(1)&&apart(o.hands[0].grip.position,{-.3f,1,0})<1e-6f);
  f.hands[1].gripFlags=0;t=.02;o=s.apply(f,t);ck(!s.active(1)&&s.active(0));}
 // A stop asked before anything was shown: nothing to stop at
 {HitStop s;s.stop(2,.1);auto o=s.apply(frame({.3f,1,-.1f}),0);ck(!s.active(1)&&apart(o.hands[1].grip.position,{.3f,1,-.1f})<1e-6f);}
 std::cout<<"PASS hit_stop "<<n<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
