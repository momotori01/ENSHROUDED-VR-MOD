#include <evr/pose_smoothing.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const XrTime step=33'333'333;const PoseSmoothingSettings s;
 auto turned=[](float radians){return Quat{0,std::sin(radians/2),0,std::cos(radians/2)};};
 // The first pose is taken as it is; a hand that does not move stays where it is.
 {PoseSmoother f;XrTime t=1'000'000'000;Pose p{{0,0,0,1},{.2f,1.1f,-.3f}};
  ck(length(f.update(p,t,s).position-p.position)==0);
  for(int i=0;i<30;++i){const auto o=f.update(p,t+=step,s);ck(length(o.position-p.position)<1e-6f&&std::abs(o.orientation.w-1)<1e-6f);}}
 // A tremble of 1 mm to either side, sample by sample, comes out at less than 0.6 mm.
 {PoseSmoother f;XrTime t=1'000'000'000;float worst=0;
  for(int i=0;i<90;++i){const float x=(i&1?.001f:-.001f);const auto o=f.update({{0,0,0,1},{x,1,0}},t+=step,s);if(i>30)worst=std::max(worst,std::abs(o.position.x));}
  ck(worst<.0006f&&worst>.0001f);}
 // ... and one of half a degree at less than 0.3 degrees.
 {PoseSmoother f;XrTime t=1'000'000'000;float worst=0;
  for(int i=0;i<90;++i){const auto o=f.update({turned(i&1?.0087f:-.0087f),{0,1,0}},t+=step,s);if(i>30)worst=std::max(worst,2*std::asin(std::min(1.f,std::abs(o.orientation.y))));}
  ck(worst<.0052f&&worst>.001f);}
 // A stroke at 3 m/s is followed 1.5 cm behind at the most; a slow reach at 0.2 m/s within 6 mm.
 for(const auto& [speed,limit]:{std::pair{3.f,.015f},std::pair{.2f,.006f}}){PoseSmoother f;XrTime t=1'000'000'000;float x=0,behind=0;
  f.update({{0,0,0,1},{0,1,0}},t,s);
  for(int i=0;i<45;++i){x+=speed*.0333333f;const auto o=f.update({{0,0,0,1},{x,1,0}},t+=step,s);if(i>20)behind=std::max(behind,x-o.position.x);ck(o.position.x<=x+1e-6f);}
  ck(behind<limit&&behind>0);}
 // A turn of 6 rad/s is followed within 3 degrees; the result stays a unit quaternion, also across the sign change.
 {PoseSmoother f;XrTime t=1'000'000'000;float angle=0,behind=0;f.update({turned(0),{0,1,0}},t,s);
  for(int i=0;i<60;++i){angle+=6*.0333333f;auto q=turned(angle);if(i%7==3){q.x=-q.x;q.y=-q.y;q.z=-q.z;q.w=-q.w;}
   const auto o=f.update({q,{0,1,0}},t+=step,s).orientation;const float norm=o.x*o.x+o.y*o.y+o.z*o.z+o.w*o.w;ck(std::abs(norm-1)<1e-4f);
   const auto want=turned(angle);const float d=std::abs(o.x*want.x+o.y*want.y+o.z*want.z+o.w*want.w);if(i>20)behind=std::max(behind,2*std::acos(std::min(1.f,d)));}
  ck(behind<.052f);}
 // A jump (the origin moved, tracking came back elsewhere) and a gap in time start again from the tracked pose.
 {PoseSmoother f;XrTime t=1'000'000'000;f.update({{0,0,0,1},{0,1,0}},t,s);f.update({{0,0,0,1},{.001f,1,0}},t+=step,s);
  ck(f.update({{0,0,0,1},{.8f,1,0}},t+=step,s).position.x==.8f);
  f.update({{0,0,0,1},{.801f,1,0}},t+=step,s);ck(f.update({{0,0,0,1},{.9f,1,0}},t+=400'000'000,s).position.x==.9f);
  ck(f.update({{0,0,0,1},{.95f,1,0}},t,s).position.x==.95f);}                                   // the same time again: no step to divide by
 // Switched off: the poses as they are.
 {PoseSmoother f;PoseSmoothingSettings off;off.cutoff=0;XrTime t=1'000'000'000;f.update({{0,0,0,1},{0,1,0}},t,off);ck(f.update({{0,0,0,1},{.01f,1,0}},t+=step,off).position.x==.01f);}
 // The hands of a frame: tracked hands are smoothed, an untracked one passes and starts again; buttons stay.
 {HandSmoothing h;HandFrame f;f.focused=true;f.predictedTime=1'000'000'000;f.frame=7;
  for(auto& hand:f.hands){hand.gripActive=hand.aimActive=true;hand.gripFlags=hand.aimFlags=15;hand.grip=hand.aim={{0,0,0,1},{0,1,0}};hand.trigger=.7f;}
  h.update(f);f.predictedTime+=step;f.hands[0].grip.position.x=.002f;f.hands[1].grip.position.x=.002f;f.hands[1].aim.position.y=1.002f;f.hands[1].gripFlags=3;
  const auto o=h.update(f);
  ck(o.frame==7&&o.hands[0].trigger==.7f&&o.hands[0].grip.position.x>0&&o.hands[0].grip.position.x<.0015f);
  ck(o.hands[1].grip.position.x==.002f&&o.hands[1].aim.position.y>1&&o.hands[1].aim.position.y<1.0015f);
  f.predictedTime+=step;f.hands[1].gripFlags=15;f.hands[1].grip.position.x=.004f;ck(h.update(f).hands[1].grip.position.x==.004f);   // tracked again: from there
  f.predictedTime+=step;f.focused=false;f.hands[0].grip.position.x=.3f;ck(h.update(f).hands[0].grip.position.x==.3f);}
 // Tracking that drops: the hand stays where it was shown, an estimated pose is shown as it comes; buttons stay.
 {HandHold hold;HandFrame f;f.focused=true;f.predictedTime=1'000'000'000;f.sessionGeneration=1;f.originGeneration=1;using S=HandHold::State;
  for(auto& hand:f.hands){hand.gripActive=hand.aimActive=true;hand.gripFlags=hand.aimFlags=15;hand.grip={{0,0,0,1},{.2f,1,0}};hand.aim={{0,0,0,1},{.2f,1.05f,0}};}
  // Never tracked: nothing to hold.
  {HandHold fresh;auto g=f;g.hands[1].gripFlags=0;g.hands[1].gripActive=false;const auto o=fresh.update(g);ck(o.hands[0].tracked()&&!o.hands[1].tracked()&&fresh.state[0]==S::Tracked&&fresh.state[1]==S::None);}
  auto o=hold.update(f);ck(o.hands[0].tracked()&&o.hands[1].tracked()&&hold.state[1]==S::Tracked);
  // One frame without tracking and without a pose (as in the headset run): the last pose, shown as tracked.
  auto lost=f;lost.hands[1].gripFlags=lost.hands[1].aimFlags=0;lost.hands[1].grip.position={9,9,9};lost.hands[1].aim.position={9,9,9};lost.hands[1].trigger=.4f;
  o=hold.update(lost);ck(o.hands[1].tracked()&&(o.hands[1].aimFlags&15)==15&&o.hands[1].grip.position.x==.2f&&o.hands[1].aim.position.y==1.05f&&o.hands[1].trigger==.4f&&hold.state[1]==S::Held&&hold.state[0]==S::Tracked);
  ck(!lost.hands[1].tracked());                                                                    // the frame as tracked is not touched
  // The action itself goes away (controller off): still held.
  auto off=lost;off.hands[1].gripActive=off.hands[1].aimActive=false;o=hold.update(off);ck(o.hands[1].tracked()&&o.hands[1].grip.position.x==.2f&&hold.state[1]==S::Held);
  // A pose that is valid but not tracked (estimated): shown as it comes, and kept from there on.
  auto guess=f;guess.hands[1].gripFlags=guess.hands[1].aimFlags=3;guess.hands[1].grip.position.x=.25f;guess.hands[1].aim.position.x=.25f;
  o=hold.update(guess);ck(o.hands[1].tracked()&&o.hands[1].grip.position.x==.25f&&o.hands[1].aim.position.x==.25f&&hold.state[1]==S::Estimated);
  o=hold.update(lost);ck(o.hands[1].grip.position.x==.25f&&o.hands[1].aim.position.x==.25f&&hold.state[1]==S::Held);
  // Orientation only (no valid position): not a pose to show - the last one stays.
  auto turning=f;turning.hands[1].gripFlags=5;turning.hands[1].grip.position={7,7,7};o=hold.update(turning);ck(o.hands[1].grip.position.x==.25f&&hold.state[1]==S::Held);
  // Tracked again: as tracked.
  auto back=f;back.hands[1].grip.position.x=.3f;o=hold.update(back);ck(o.hands[1].grip.position.x==.3f&&hold.state[1]==S::Tracked);
  // Without the focus nothing is held; with it back the hand is held again.
  auto away=lost;away.focused=false;o=hold.update(away);ck(!o.hands[1].tracked()&&hold.state[1]==S::None);
  o=hold.update(lost);ck(o.hands[1].tracked()&&o.hands[1].grip.position.x==.3f);
  // A new origin (recentre) or a new session: the kept pose is of another space - gone until tracked again.
  auto moved=lost;moved.originGeneration=2;o=hold.update(moved);ck(!o.hands[1].tracked()&&hold.state[1]==S::None&&o.hands[0].tracked());
  auto again=f;again.originGeneration=2;hold.update(again);auto session=lost;session.originGeneration=2;session.sessionGeneration=2;o=hold.update(session);ck(!o.hands[1].tracked());}
 std::cout<<"pose_smoothing "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"pose_smoothing FAIL "<<e.what()<<"\n";return 1;}}
