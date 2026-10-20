#include <evr/swim_stroke.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
namespace {
// Both hands tracked at the given places (the hands' space: +Y up; the head looks toward -Z), palms facing each other's
// side as a grip held upright: the left palm's normal is the grip's +X, the right's -X.
HandFrame frame(XrTime t,Vec3 left,Vec3 right){
 HandFrame f;f.focused=true;f.predictedTime=t;f.frame=uint64_t(t/11000000);
 for(unsigned i=0;i<2;++i){auto& h=f.hands[i];h.gripActive=true;h.gripFlags=15;h.grip=identityPose();h.grip.position=i?right:left;}
 return f;
}
Pose headAt(float y,float yaw=0){Pose p=identityPose();p.position={0,y,0};p.orientation=euler(yaw,0,0);return p;}
constexpr XrTime step=11111111;   // 90 a second
}
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 const auto head=headAt(1.6f);
 // A breaststroke's push: both palms turned backward (the left grip's +X and the right grip's -X toward +Z), both hands
 // pushed back (+Z) at 2 m/s under the water for 0.3 s: the body goes forward (-Z), the stick forward.
 auto backward=[](HandFrame f){
  for(unsigned i=0;i<2;++i)f.hands[i].grip.orientation=euler(i?(pi/2):-(pi/2),0,0);   // the palms' normals to +Z
  return f;};
 {SwimStroke s;SwimStrokeResult r;XrTime t=step;
  for(int k=0;k<28;++k,t+=step){const float z=.0222f*k;r=s.update(backward(frame(t,{-.3f,1.f,z},{.3f,1.f,z})),head,true);}
  ck(r.stroking&&r.moving&&r.stickY>.5f&&std::abs(r.stickX)<.05f&&r.counted[0]&&r.counted[1]);
  ck(r.wanted.z<-4.f);                                                  // (2 - 0.5) x 2 hands x 2 = 6 m/s, toward -Z
  // the hands still: the stroke carries on and fades over a few seconds (VHVR: 1 s decay in the water)
  const float earlier=length(r.velocity);for(int k=0;k<90;++k,t+=step)r=s.update(backward(frame(t,{-.3f,1.f,.6f},{.3f,1.f,.6f})),head,true);
  ck(!r.stroking&&r.moving&&length(r.velocity)<earlier*.5f&&r.stickY>0);
  for(int k=0;k<500;++k,t+=step)r=s.update(backward(frame(t,{-.3f,1.f,.6f},{.3f,1.f,.6f})),head,true);
  ck(!r.moving&&r.stickY==0);}
 // Above the water's surface (0.05 m under the head, 2026-10-20 11:40) a hand does nothing; nor a hand held back (the
 // trigger, the grip closed - the thumb on the stick does not hold it back since 11:40); nor slow strokes (0.5 m/s is
 // taken off each hand), nor out of the water.
 auto stroke=[&](float y,auto&& hold,bool swimming,float speed=2.f){SwimStroke s;SwimStrokeResult r;XrTime t=step;
  for(int k=0;k<28;++k,t+=step){auto f=backward(frame(t,{-.3f,y,speed*k*step*1e-9f},{.3f,y,speed*k*step*1e-9f}));hold(f);r=s.update(f,head,swimming);}
  return r;};
 auto none=[](HandFrame&){};
 ck(stroke(1.f,none,true).moving&&stroke(1.5f,none,true).moving&&!stroke(1.58f,none,true).moving&&!stroke(1.f,none,false).moving);
 ck(stroke(1.f,[](HandFrame& f){for(auto& h:f.hands){h.thumbTouchActive=true;h.thumbTouch=true;h.stickActive=true;h.stick={0,.8f};}},true).moving);
 ck(!stroke(1.f,[](HandFrame& f){for(auto& h:f.hands){h.triggerActive=true;h.trigger=.9f;}},true).moving);
 ck(!stroke(1.f,[](HandFrame& f){for(auto& h:f.hands){h.grabActive=true;h.grab=true;}},true).moving);
 ck(!stroke(1.f,none,true,.6f).moving);                                // (0.6 - 0.5) x 2 = 0.2 < 0.375
 ck(stroke(1.f,[](HandFrame& f){f.hands[0].triggerActive=true;f.hands[0].trigger=1;},true).moving);   // one hand alone: 1.5 m/s
 // 0.25 m/s is the stick's full throw (fitted 2026-10-20 11:40): a short stroke of 80 ms at 2.5 m/s moves the stick far
 {SwimStroke s;SwimStrokeResult r;XrTime t=step;float most=0;
  for(int k=0;k<40;++k,t+=step){const float z=k<8?.0278f*k:.0278f*7;r=s.update(backward(frame(t,{-.3f,1.f,z},{.3f,1.f,z})),head,true);most=std::max(most,r.stickY);}
  ck(most>.6f);}
 // The palms turned forward and the hands moved back: no push (only the palm's side pushes water).
 {SwimStroke s;SwimStrokeResult r;XrTime t=step;
  for(int k=0;k<28;++k,t+=step){auto f=frame(t,{-.3f,1.f,.0222f*k},{.3f,1.f,.0222f*k});
   for(unsigned i=0;i<2;++i)f.hands[i].grip.orientation=euler(i?-(pi/2):(pi/2),0,0);r=s.update(f,head,true);}
  ck(!r.moving);}
 // Sideways: the head turned 90 degrees to the left (yaw +90: it looks toward -X); a push toward -X is forward for it.
 {SwimStroke s;SwimStrokeResult r;XrTime t=step;const auto turned=headAt(1.6f,(pi/2));
  for(int k=0;k<28;++k,t+=step){auto f=frame(t,{.0222f*k,1.f,-.3f},{.0222f*k,1.f,.3f});
   for(unsigned i=0;i<2;++i)f.hands[i].grip.orientation=euler(0,0,0);   // left palm +X, right palm -X
   f.hands[1].grip.orientation=euler(pi,0,0);                    // the right grip turned round: its -X is +X too
   r=s.update(f,turned,true);}
  ck(r.moving&&r.stickY>.5f&&std::abs(r.stickX)<.05f);}
 // Up and down (2026-10-20 12:20): both hands pushed down at 2 m/s, the palms facing down (the left grip's +X and the
 // right grip's -X turned to -Y): the push lifts - up and upStick positive; pulled up with the palms up: negative.
 {auto vertical=[&](float dir){SwimStroke s;SwimStrokeResult r;XrTime t=step;
   for(int k=0;k<20;++k,t+=step){const float y=1.f+dir*.0222f*k;auto f=frame(t,{-.3f,y,0},{.3f,y,0});
    f.hands[0].grip.orientation=euler(0,0,dir<0?-(pi/2):(pi/2));f.hands[1].grip.orientation=euler(0,0,dir<0?(pi/2):-(pi/2));r=s.update(f,headAt(1.8f),true);}
   return r;};
  const auto down=vertical(-1),up=vertical(1);
  ck(down.strokingUp&&down.up>.1f&&down.upStick>.3f&&!down.moving);   // lifts, no forward
  ck(up.strokingUp&&up.up<-.1f&&up.upStick<-.3f);}
 // A tracking jump (a hand 1 m in one frame) is not a stroke; a gap over 0.1 s starts over.
 {SwimStroke s;XrTime t=step;s.update(backward(frame(t,{-.3f,1.f,0},{.3f,1.f,0})),head,true);t+=step;
  auto r=s.update(backward(frame(t,{-.3f,1.f,1.f},{.3f,1.f,1.f})),head,true);ck(!r.stroking&&!r.moving);
  t+=step*20;r=s.update(backward(frame(t,{-.3f,1.f,1.5f},{.3f,1.f,1.5f})),head,true);ck(!r.stroking);}
 std::cout<<"swim_stroke "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"swim_stroke FAIL "<<e.what()<<"\n";return 1;}}
