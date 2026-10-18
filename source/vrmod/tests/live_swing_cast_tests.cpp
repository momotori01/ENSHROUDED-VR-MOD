#include <evr/live_swing_cast.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 constexpr float dt=1.f/90;const XrDuration tick=11111111;
 HandFrame f;LiveSwingCast c;LiveSwingRod rod;float yaw=0;
 // The cast's strength: 0 up to 5 m/s at the tip, 1 from 45 m/s (the scale measured on the headset, 2026-10-13).
 auto strength=[](float peak){const float share=std::clamp((peak-5)/40,0.f,1.f);return share*share;};
 {const LiveSwingCastSettings s;ck(s.strength(5)==0&&s.strength(3)==0&&s.strength(45)==1&&s.strength(60)==1&&std::abs(s.strength(20)-.140625f)<1e-5f&&std::abs(s.strength(35)-.5625f)<1e-5f&&std::abs(s.strength(22)-.180625f)<1e-5f&&s.strength(std::nanf(""))==0);}
 LiveSwingCastSettings atSwing;atSwing.onRelease=false;   // the first form: the swing itself casts
 auto reset=[&](){c=LiveSwingCast(atSwing);f={};f.focused=true;f.sessionGeneration=f.originGeneration=1;f.predictedTime=1000000000;yaw=0;
  for(auto& h:f.hands){h.gripActive=true;h.gripFlags=15;h.profile=1;h.grip=identityPose();h.triggerActive=true;}
  rod={(uint64_t(425570534u)<<32)|7,{0,1,0},1.888f};};
 // One frame: the hand moves with `velocity` and turns about the vertical axis with `spin` rad/s (the rod lies level, along +Z).
 auto step=[&](Vec3 velocity,float spin,float trigger,bool allowed=true){++f.frame;f.predictedTime+=tick;f.hands[1].trigger=trigger;
  f.hands[1].grip.position=f.hands[1].grip.position+velocity*dt;yaw+=spin*dt;f.hands[1].grip.orientation=euler(yaw,0,0);
  rod.forward=rotate(f.hands[1].grip.orientation,{0,0,1});return c.update(f,rod,allowed);};

 // The trigger alone prepares; a still hand casts nothing and keeps the button.
 reset();for(unsigned i=0;i<5;i++){const auto r=step({0,0,0},0,0);ck(!r.preparing&&!r.letGo&&!r.cast);}
 for(unsigned i=0;i<30;i++){const auto r=step({0,0,0},0,1);ck(r.preparing&&!r.letGo&&!r.cast&&r.peak<1e-3f);}
 // A flick of the wrist: 2 rad/s at a tip 1.888 m away is 3.78 m/s - above 3.5, the rod casts at once, once.
 {const auto r=step({0,0,0},2,1);ck(r.cast&&r.letGo&&r.preparing&&c.casts()==1&&std::abs(r.speed-3.776f)<.05f&&r.strength==0);
  ck(std::abs(length(r.direction)-1)<1e-4f&&std::abs(r.direction.y)<1e-3f);}                  // the tip moves level, across the rod
 for(unsigned i=0;i<20;i++){const auto r=step({0,0,0},2,1);ck(!r.cast&&r.letGo);}             // the button stays let go while the trigger is held
 {const auto r=step({0,0,0},0,0);ck(!r.cast&&!r.letGo&&!r.preparing);}                        // the trigger let go: ready for the next cast
 ck(c.casts()==1);
 // The next press starts over: the highest speed is cleared, so a still hand does not cast.
 for(unsigned i=0;i<10;i++){const auto r=step({0,0,0},0,1);ck(r.preparing&&!r.cast&&!r.letGo&&r.peak<1e-3f);}

 // Moving the whole arm: 4 m/s of the hand without any turn casts along the hand's movement.
 reset();step({0,0,0},0,0);step({0,0,0},0,1);for(unsigned i=0;i<3;i++)ck(!step({0,1,-1},0,1).cast);   // 1.4 m/s: too slow
 {LiveSwingCastResult r;unsigned at=0;for(unsigned i=0;i<4&&!r.cast;i++){r=step({0,2.4f,-3.2f},0,1);at=i;}
  ck(r.cast&&at==0&&std::abs(r.speed-4)<.05f&&r.strength==0);
  ck(r.direction.z<-.6f&&r.direction.y>.4f&&std::abs(r.direction.x)<1e-3f);}                  // the hand's average movement (the slow start still counts)
 // Faster: the strength grows with the tip's speed.
 reset();step({0,0,0},0,0);step({0,0,0},0,1);{const auto r=step({0,0,-7},0,1);ck(r.cast&&std::abs(r.strength-strength(7))<.01f);}

 // Too slow a swing never casts by itself; letting the trigger go is then the game's own cast (not ours).
 reset();step({0,0,0},0,0);step({0,0,0},0,1);for(unsigned i=0;i<30;i++){const auto r=step({0,0,-1},1,1);ck(!r.cast&&!r.letGo&&r.peak<3.5f&&r.peak>1);}
 {const auto r=step({0,0,0},0,0);ck(!r.cast&&!r.letGo&&c.casts()==0);}
 // A swing before the trigger does not count: the preparation starts at the press.
 reset();for(unsigned i=0;i<10;i++)ck(!step({0,0,-6},0,0).cast);
 for(unsigned i=0;i<6;i++)step({0,0,0},0,0);
 for(unsigned i=0;i<10;i++){const auto r=step({0,0,0},0,1);ck(!r.cast&&r.peak<1e-3f);}
 // The speed of the frame that presses the trigger does count (VHVR measures while the trigger is held).
 reset();for(unsigned i=0;i<4;i++)step({0,0,-6},0,0);ck(step({0,0,-6},0,1).cast);

 // Not allowed (a menu, another item) or no rod shown: nothing, and the preparation does not survive.
 reset();step({0,0,0},0,0);step({0,0,0},0,1);ck(c.preparing());ck(!step({0,0,-6},0,1,false).cast);ck(!c.preparing());
 {const auto r=step({0,0,-6},0,1);ck(!r.cast&&r.preparing);ck(step({0,0,-6},0,1).cast);}     // pressed again counts as a new press, the first sample has no speed
 reset();step({0,0,0},0,0);step({0,0,0},0,1);rod.tip=0;++f.frame;f.predictedTime+=tick;ck(!c.update(f,rod,true).preparing);
 reset();step({0,0,0},0,0);step({0,0,0},0,1);rod.binding=0;++f.frame;f.predictedTime+=tick;ck(!c.update(f,rod,true).preparing);
 // A tracking jump is not a swing; a gap in time gives no speed.
 reset();step({0,0,0},0,0);step({0,0,0},0,1);{const auto r=step({0,0,-2000},0,1);ck(!r.cast&&!r.sampled);}
 {const auto r=step({0,0,0},0,1);ck(!r.cast&&r.sampled&&r.speed<1e-3f&&r.preparing);}   // measured again from where the hand now is
 reset();step({0,0,0},0,0);step({0,0,0},0,1);f.predictedTime+=200000000;f.hands[1].grip.position.z-=1;{const auto r=step({0,0,0},0,1);ck(!r.cast&&!r.sampled&&r.preparing);}
 // The same frame twice (the frontend runs faster than the hands arrive): no second sample, no second cast.
 reset();step({0,0,0},0,0);step({0,0,0},0,1);ck(step({0,0,-6},0,1).cast);{const auto r=c.update(f,rod,true);ck(!r.cast&&r.letGo&&c.casts()==1);}
 // Another rod (a new binding) starts over.
 reset();step({0,0,0},0,0);step({0,0,0},0,1);ck(step({0,0,-6},0,1).cast);rod.binding+=1;{const auto r=step({0,0,-6},0,1);ck(!r.cast&&!r.letGo);}
 // --- The default: the cast comes when the trigger is let go after a swing (user decision 2026-10-12) ---
 auto resetRelease=[&](){reset();c=LiveSwingCast();};
 // Swing with the trigger held: nothing yet, and the button is never let go early. Letting go while still moving casts.
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);
 for(unsigned i=0;i<12;i++){const auto r=step({0,0,-5},0,1);ck(!r.cast&&!r.letGo&&r.preparing);}
 {const auto r=step({0,0,-5},0,0);ck(r.cast&&r.released&&!r.letGo&&!r.preparing&&c.casts()==1&&r.strength<1e-3f&&r.direction.z<-.9f);}   // 5 m/s: the scale begins there
 ck(!step({0,0,0},0,0).cast);
 // The swing may be over when the trigger is let go: what counts is the highest speed while it was held (VHVR).
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);for(unsigned i=0;i<6;i++)step({0,0,0},2,1);for(unsigned i=0;i<20;i++)step({0,0,0},0,1);
 {const auto r=step({0,0,0},0,0);ck(r.cast&&r.released&&r.peak>3.5f&&r.strength==0&&std::abs(r.direction.y)<1e-3f&&std::abs(length(r.direction)-1)<1e-4f);} // the stroke's own speed and way
 // Let go without a swing, or after one that was too slow: no cast by the swing (the game still casts on its own).
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);for(unsigned i=0;i<20;i++)step({0,0,0},0,1);{const auto r=step({0,0,0},0,0);ck(!r.cast&&r.released&&c.casts()==0);}
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);for(unsigned i=0;i<20;i++)step({0,0,-1},1,1);{const auto r=step({0,0,-1},1,0);ck(!r.cast&&r.released&&r.peak<3.5f);}
 // A swing before the trigger does not count; the next hold starts over.
 resetRelease();for(unsigned i=0;i<10;i++)step({0,0,-6},0,0);for(unsigned i=0;i<6;i++)step({0,0,0},0,0);step({0,0,0},0,1);ck(!step({0,0,0},0,0).cast);
 step({0,0,0},0,1);for(unsigned i=0;i<4;i++)step({0,0,-5},0,1);ck(step({0,0,-5},0,0).cast&&c.casts()==1);
 // The cast goes where the rod's tip went at the stroke's fastest moment - not where the hand drifts when the trigger
 // comes up (headset run game-20261013-005905: casts 42..99 degrees to the side).
 // The rod lies level along +Z; a turn about the vertical moves its tip along +X (to the left of -Z... the sign is the test's own).
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);
 {const auto swing=step({0,0,0},6,1);const Vec3 way=swing.velocity*(1/length(swing.velocity));     // the tip's way at 6 rad/s: 11 m/s
  for(unsigned i=0;i<10;i++)step({0,0,0},6,1);
  for(unsigned i=0;i<8;i++)step({0,-.6f,0},0,1);                                                    // the follow-through: the hand sinks, slowly
  const auto r=step({0,-.6f,0},0,0);
  ck(r.cast&&std::abs(r.strength-strength(6*1.888f))<.02f&&std::abs(r.direction.y)<.05f&&dot(r.direction,way)>.7f);}   // along the stroke, not downwards; 11.3 m/s at the tip
 // A wind-up the other way before the stroke: the stroke replaces it.
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);
 {for(unsigned i=0;i<8;i++)step({0,0,0},-5,1);for(unsigned i=0;i<3;i++)step({0,0,0},0,1);
  const auto first=step({0,0,0},6,1);const Vec3 way=first.velocity*(1/length(first.velocity));
  for(unsigned i=0;i<8;i++)step({0,0,0},6,1);
  const auto r=step({0,0,0},0,0);ck(r.cast&&dot(r.direction,way)>.5f);}
 // The forward flick may be the release frame itself: it replaces the wind-up before it.
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);
 {for(unsigned i=0;i<8;i++)step({0,0,6},0,1);for(unsigned i=0;i<3;i++)step({0,0,0},0,1);                // the wind-up backwards at 6 m/s, then still
  const auto r=step({0,0,-12},0,0);ck(r.cast&&r.released&&r.direction.z<-.9f&&r.peak>11.f);}            // let go in the flick forwards at 12 m/s
 // A twitch after the cast (under 0.6 of the stroke's peak) does not replace the stroke.
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);
 {const auto first=step({0,0,0},8,1);const Vec3 way=first.velocity*(1/length(first.velocity));
  for(unsigned i=0;i<8;i++)step({0,0,0},8,1);for(unsigned i=0;i<3;i++)step({0,0,0},0,1);
  for(unsigned i=0;i<4;i++)step({0,0,0},-3,1);                                                      // back at 5.7 m/s: a twitch against 15 m/s
  const auto r=step({0,0,0},0,0);ck(r.cast&&dot(r.direction,way)>.5f);}
 // A rod held 51 degrees up and flicked down to level (the Mock script's cast, run game024520): its tip moves forwards
 // and down at first and straight down at the end. The cast goes the way the tip went as a whole - forwards.
 {resetRelease();float pitch=.89f;                                                                   // the rod's angle above the level, pointing along -Z
  auto tilt=[&](float rate,float trigger){++f.frame;f.predictedTime+=tick;f.hands[1].trigger=trigger;pitch+=rate*dt;
   f.hands[1].grip.orientation=euler(0,pitch,0);rod.forward=rotate(f.hands[1].grip.orientation,{0,0,-1});return c.update(f,rod,true);};
  tilt(0,0);tilt(0,1);for(unsigned i=0;i<12;i++)tilt(-6.28f,1);                                      // 360 degrees a second for 48 degrees
  for(unsigned i=0;i<30;i++)tilt(0,1);
  const auto r=tilt(0,0);ck(r.cast&&r.direction.z<-.35f&&r.direction.y<-.5f&&std::abs(r.direction.x)<.02f);}
 // Not allowed while held (a menu): the hold does not survive, nothing on the release.
 resetRelease();step({0,0,0},0,0);step({0,0,0},0,1);for(unsigned i=0;i<4;i++)step({0,0,-5},0,1);step({0,0,-5},0,1,false);ck(!step({0,0,-5},0,0).cast);
 // The game's charge at the cast: the swing's strength, unless the game's own (the time held) is higher.
 {const float nan=std::nanf("");
  ck(swingCharge(.15f,1)==1&&swingCharge(.15f,.7f)==.7f&&swingCharge(.9f,.7f)==.9f&&swingCharge(1,1)==1);
  ck(swingCharge(.15f,0)==.15f&&swingCharge(.15f,nan)==.15f&&swingCharge(nan,.7f)==.7f&&swingCharge(nan,nan)==0);
  ck(swingCharge(-1,-1)==0&&swingCharge(2,.5f)==1&&swingCharge(.2f,7)==1);
  ck(castDistance(0)==3&&castDistance(1)==18&&std::abs(castDistance(.15f)-5.25f)<1e-4f);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
