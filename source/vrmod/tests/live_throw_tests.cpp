#include <evr/live_throw.hpp>
#include <evr/throwable_catalog.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 constexpr float dt=1.f/90;const XrDuration tick=11111111;
 HandFrame f;LiveThrow t;uint64_t now=1000;
 auto reset=[&](){t={};f={};f.focused=true;f.sessionGeneration=f.originGeneration=1;f.predictedTime=1000000000;
  for(auto& h:f.hands){h.gripActive=true;h.gripFlags=15;h.profile=1;h.grip=identityPose();h.triggerActive=true;}now=1000;};
 auto step=[&](Vec3 velocity,float trigger,bool allowed=true){++f.frame;f.predictedTime+=tick;now+=11;f.hands[1].trigger=trigger;
  f.hands[1].grip.position=f.hands[1].grip.position+velocity*dt;return t.update(f,allowed,now);};
 // Wind up with the trigger, move the hand, let go: one throw along the movement, from where the hand is.
 reset();for(unsigned i=0;i<5;i++)ck(!step({0,0,0},0).aiming);
 ck(step({0,0,0},1).aiming);for(unsigned i=0;i<12;i++){const auto r=step({0,3,-4},1);ck(r.aiming&&!r.thrown&&!r.press);}
 {const auto r=step({0,3,-4},0);ck(r.thrown&&r.press&&!r.aiming&&t.throws()==1);
  ck(std::abs(r.speed-5)<.05f&&std::abs(r.direction.y-.6f)<1e-3f&&std::abs(r.direction.z+.8f)<1e-3f&&std::abs(r.direction.x)<1e-4f);
  ck(length(r.origin-f.hands[1].grip.position)<1e-6f&&r.travel>.0625f);}
 ck(step({0,0,0},0).press);for(unsigned i=0;i<12;i++)step({0,0,0},0);ck(!step({0,0,0},0).press);   // a tap, not a hold
 // Letting go with a still hand throws nothing (VHVR: 6.25 cm of travel within the last 0.4 s).
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<20;i++)step({0,0,0},1);
 {const auto r=step({0,0,0},0);ck(!r.thrown&&!r.press&&r.travel<1e-6f&&t.throws()==0);}
 // Movement that ended long before the release does not count: the hand rested for more than 0.4 s.
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<10;i++)step({3,0,0},1);for(unsigned i=0;i<45;i++)step({0,0,0},1);
 ck(!step({0,0,0},0).thrown);
 // A short wind-up only looks back as far as it lasted.
 reset();for(unsigned i=0;i<10;i++)step({3,0,0},0);step({0,0,0},1);ck(!step({0,0,0},0).thrown);
 // Not allowed (another item, a menu): nothing, and the wind-up does not survive.
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<6;i++)step({0,0,-4},1);step({0,0,-4},1,false);ck(!t.aiming());ck(!step({0,0,-4},0).thrown);
 // A tracking jump is not a throw.
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<3;i++)step({0,0,0},1);ck(!step({0,0,-2000},0).thrown);
 // The release comes late, in the follow-through (headset run game-20261013-005905: the hand already low and going
 // down): the throw goes along its fastest moment - forwards and up -, not along the hand's last movement.
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<6;i++)step({0,0,0},1);
 for(unsigned i=0;i<14;i++)step({0,2,-5},1);                                            // the throw: 0.16 s forwards and up
 for(unsigned i=0;i<9;i++)step({0,-1.5f,-.5f},1);                                        // the follow-through: 0.1 s downwards, slower
 {const auto r=step({0,-1.5f,-.5f},0);ck(r.thrown&&r.direction.z<-.85f&&r.direction.y>.3f&&std::abs(r.speed-std::sqrt(29.f))<.3f);
  ck(length(r.origin-f.hands[1].grip.position)<1e-6f);}                                  // it leaves the hand where the hand now is
 // The turn from the throw into the follow-through is slow for a moment when averaged (Mock run game023439, the hands
 // arriving on every second frame): the search goes on through it.
 {reset();const XrDuration two=22222222;auto twoStep=[&](Vec3 velocity,float trigger){++f.frame;f.predictedTime+=two;now+=22;f.hands[1].trigger=trigger;
   f.hands[1].grip.position=f.hands[1].grip.position+velocity*(2*dt);return t.update(f,true,now);};
  twoStep({0,0,0},0);twoStep({0,0,0},1);for(unsigned i=0;i<8;i++)twoStep({0,0,0},1);
  for(unsigned i=0;i<10;i++)twoStep({2.7f,1.35f,0},1);for(unsigned i=0;i<3;i++)twoStep({0,-1.35f,0},1);
  const auto r=twoStep({0,-1.35f,0},0);ck(r.thrown&&r.direction.x>.85f&&r.direction.y>.4f&&std::abs(r.speed-3.02f)<.1f);}
 // The same with the hands arriving 22 times a second.
 {reset();const XrDuration slow=45000000;auto slowStep=[&](Vec3 velocity,float trigger){++f.frame;f.predictedTime+=slow;now+=45;f.hands[1].trigger=trigger;
   f.hands[1].grip.position=f.hands[1].grip.position+velocity*.045f;return t.update(f,true,now);};
  slowStep({0,0,0},0);slowStep({0,0,0},1);slowStep({0,0,0},1);
  for(unsigned i=0;i<4;i++)slowStep({0,2,-5},1);for(unsigned i=0;i<2;i++)slowStep({0,-1.5f,-.5f},1);
  const auto r=slowStep({0,-1.5f,-.5f},.4f);                                             // half open counts as let go
  ck(r.thrown&&r.direction.z<-.85f&&r.direction.y>.3f&&r.speed>5);}
 // A wind-up backwards before the throw is not the throw: the nearest peak is the forward one.
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<10;i++)step({0,0,6},1);for(unsigned i=0;i<4;i++)step({0,0,0},1);
 for(unsigned i=0;i<12;i++)step({0,1,-4},1);
 {const auto r=step({0,1,-4},0);ck(r.thrown&&r.direction.z<-.9f&&r.speed<4.5f);}
 // The trigger half open is a release, a little open is not.
 reset();step({0,0,0},0);step({0,0,0},1);for(unsigned i=0;i<10;i++)step({0,0,-4},1);ck(step({0,0,-4},.7f).aiming);ck(step({0,0,-4},.45f).thrown);
 // The guide (2026-10-18): only while a throw is got ready, only with the left hand held forward (30 cm ahead of the
 // head, level, and no lower than 45 cm under it); its direction is the left hand's pointing, kept for 1.5 s after the hand goes back.
 {ThrowGuide g;HandFrame h;h.focused=true;h.frame=1;h.predictedTime=1000000000;const Pose head{{0,0,0,1},{0,1.6f,0}};
  auto& l=h.hands[0];l.aimActive=true;l.aimFlags=15;l.aim={{0,0,0,1},{-.1f,1.4f,-.45f}};   // ahead, pointing along -Z
  ck(!g.update(h,head,true,false).shown);                                                  // no wind-up: no guide
  auto r=g.update(h,head,true,true);ck(r.shown&&r.kept&&std::abs(r.direction.z+1)<1e-5f&&length(r.from-l.aim.position)<1e-6f);
  Vec3 d[16];const unsigned k=g.dots(r,d,16);ck(k==2&&std::abs(d[0].z-(-.45f-.04f))<1e-5f&&std::abs(d[1].z-(-.45f-.36f))<1e-5f&&std::abs(d[1].x+.1f)<1e-6f);   // the beam's two ends
  // pointing up and to the right: that direction
  l.aim.orientation=euler(-.6f,.4f,0);h.predictedTime+=11111111;r=g.update(h,head,true,true);
  const Vec3 up=rotate(l.aim.orientation,{0,0,-1});ck(r.shown&&length(r.direction-up)<1e-5f&&r.direction.y>.3f);
  // the arm pulled back (a throw): no guide, the direction kept
  l.aim.position={-.2f,1.3f,-.05f};h.predictedTime+=11111111;r=g.update(h,head,true,true);ck(!r.shown&&r.kept&&length(r.direction-up)<1e-5f&&g.dots(r,d,16)==0);
  h.predictedTime+=1400000000;ck(g.update(h,head,true,true).kept);h.predictedTime+=200000000;ck(!g.update(h,head,true,true).kept);   // 1.5 s
  // the head turned to the right (looking along +X): the same hand is now beside it, not ahead
  l.aim.position={-.1f,1.4f,-.45f};const Pose right{euler(-1.5707963f,0,0),{0,1.6f,0}};ck(!g.update(h,right,true,true).shown);
  ck(!g.update(h,head,false,true).shown);                                                  // the head not tracked
  l.aim.position={-.1f,1.1f,-.45f};ck(!g.update(h,head,true,true).shown);                  // in front of the waist (50 cm under the eyes)
  l.aim.position={-.1f,1.4f,-.25f};ck(!g.update(h,head,true,true).shown);l.aim.position={-.1f,1.4f,-.45f};   // only 25 cm ahead
  l.aimFlags=0;ck(!g.update(h,head,true,true).shown);l.aimFlags=15;
  ck(g.update(h,head,true,true).kept&&!g.update(h,head,true,false).kept);}                 // the wind-up ends: forgotten
 // The catalog of the game data: sorted, the powder ball of the home save is in it, a sword is not.
 for(size_t i=1;i<catalogThrowables.size();i++)ck(catalogThrowables[i-1]<catalogThrowables[i]);
 ck(catalogThrowables.size()==14&&catalogThrowable(2283229289u)&&!catalogThrowable(1125155603u)&&!catalogThrowable(0));
 ck(catalogThrowable(32289505u));   // the glowing root thrown under water: a Tool, the first of the list
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
