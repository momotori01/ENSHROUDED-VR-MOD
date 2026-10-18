#include <evr/live_guard.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 constexpr float dt=1.f/90;const XrDuration tick=11111111;
 const Pose head{{0,0,0,1},{0,1.6f,0}};                 // looking along -Z
 const Quat faceFront=euler(-pi/2,0,0),faceSide{0,0,0,1},faceUp=euler(0,0,-pi/2); // where the back of the left hand (-X) points
 const Vec3 up{-.15f,1.3f,-.35f},hanging{-.25f,.8f,0};  // 30 cm below the head and 35 cm ahead; 80 cm below at the side
 HandFrame f;LiveGuard t;LiveGuardEquipment e;uint64_t now=1000;Pose h=head;bool attack=false,allowed=true,tracked=true;
 auto reset=[&](){t={};f={};e={};e.binding=5;e.shield=true;h=head;attack=false;allowed=tracked=true;f.focused=true;f.sessionGeneration=f.originGeneration=1;f.predictedTime=1000000000;
  for(auto& x:f.hands){x.gripActive=true;x.gripFlags=15;x.profile=1;x.grip=identityPose();}f.hands[0].grip={faceFront,hanging};f.hands[1].grip.position={.15f,.8f,-.35f};now=1000;};
 auto step=[&](Vec3 leftVelocity={0,0,0},Vec3 rightVelocity={0,0,0}){++f.frame;f.predictedTime+=tick;now+=11;
  f.hands[0].grip.position=f.hands[0].grip.position+leftVelocity*dt;f.hands[1].grip.position=f.hands[1].grip.position+rightVelocity*dt;return t.update(f,h,tracked,e,allowed,attack,now);};
 auto place=[&](Vec3 p,Quat q){f.hands[0].grip={q,p};step();return step();}; // a jump is not continuous motion: two frames settle it
 auto calm=[&](unsigned frames){LiveGuardResult r;for(unsigned i=0;i<frames;i++)r=step();return r;};
 // Move the left hand to a point at 1 m/s (below the parry speed), then rest two frames.
 auto glide=[&](Vec3 target){for(;;){const auto d=target-f.hands[0].grip.position;const float distance=length(d);if(distance<1e-5f)break;
   step(d*(std::min(1.f,distance/dt)/distance));}return calm(2);};

 // Shield: hanging arm, or raised but facing sideways, never blocks; raised, ahead and facing does.
 reset();ck(!calm(5).pressed);                                              // back of the hand forward, but the arm hangs
 ck(!place(up,faceSide).pressed);
 auto r=place(up,faceFront);ck(r.pressed&&r.posture&&r.facing>.99f&&t.presses()==1);
 for(unsigned i=0;i<30;i++)ck(step().pressed);ck(t.presses()==1);
 // Hysteresis: 70 cm below the head still holds, 80 cm lets go - after the 300 ms the press is held on.
 ck(glide({up.x,1.6f-.70f,up.z}).pressed);{const auto low=glide({up.x,1.6f-.80f,up.z});ck(!low.posture&&low.pressed);ck(!calm(30).pressed);}
 ck(!glide({up.x,1.6f-.70f,up.z}).pressed);                                  // and does not re-enter until properly raised
 ck(glide(up).pressed&&t.presses()==2);
 ck(glide({up.x,up.y,-.02f}).pressed);{const auto back=glide({up.x,up.y,.05f});ck(!back.posture);ck(!calm(30).pressed);} // pulled back behind the head
 // Turned 80 degrees away from the front is outside, 70 inside (75 to come in; VHVR's Gesture is 60).
 reset();ck(!place(up,euler(-pi/2+80*pi/180,0,0)).pressed);ck(place(up,euler(-pi/2+70*pi/180,0,0)).pressed);
 // A guard the pose leaves for a moment is not lost: turned to 95 degrees and back within 300 ms keeps the one press.
 reset();ck(place(up,faceFront).pressed);f.hands[0].grip.orientation=euler(-pi/2+95*pi/180,0,0);
 for(unsigned i=0;i<20;i++)ck(step().pressed);f.hands[0].grip.orientation=faceFront;ck(calm(3).pressed&&t.presses()==1);
 // Overhead: the shield face up above the head needs no reach forward.
 reset();ck(place({0,1.75f,0},faceUp).pressed);
 // The body's front follows the head, 45 degrees behind at most: turning the head 90 degrees either way does not drop
 // a guard held before the body, and a glance (30 degrees) moves the front only slowly.
 reset();ck(place(up,faceFront).pressed);h.orientation=euler(pi/2,0,0);ck(calm(2).pressed);h.orientation=euler(-pi/2,0,0);ck(calm(2).pressed);
 reset();ck(!place(up,faceSide).pressed);h.orientation=euler(pi/6,0,0);ck(!calm(3).pressed);   // a raised hand facing the side: no guard while the head glances
 h.orientation=euler(pi/2,0,0);ck(calm(3).pressed);                                         // looking that way, it is one (towards where the player looks)
 // The other hand does not move the front: a stroke of the sword hand across and back, and a reach to the right
 // shoulder, keep the guard (headset run 20261014-162900).
 reset();ck(place(up,faceFront).pressed);
 for(unsigned i=0;i<30;i++)ck(step({0,0,0},{-3,0,2}).pressed);for(unsigned i=0;i<30;i++)ck(step({0,0,0},{3,.5f,-1}).pressed);
 f.hands[1].grip.position={.25f,1.45f,.15f};ck(calm(2).pressed&&t.presses()==1);
 // A hand resting at chest height with its back turned up is no guard either; overhead it is (above).
 reset();ck(!place(up,faceUp).pressed);ck(!calm(5).pressed);
 // Hands under the head give no direction: the head's heading stands in, and looking straight down keeps the last one.
 reset();f.hands[1].grip.position={.15f,.8f,.35f};ck(place(up,faceFront).pressed);h.orientation=euler(pi/2,0,0);ck(calm(2).posture&&calm(30).pressed);
 h=head;ck(calm(2).pressed);h.orientation=euler(0,-1.5f,0);ck(calm(2).pressed);

 // Parry 1: raising the shield quickly into place is itself the new press.
 reset();calm(3);f.hands[0].grip.orientation=faceFront;bool parry=false,pressed=false;
 for(unsigned i=0;i<40&&!pressed;i++){r=step({(up.x-hanging.x)*3,(up.y-hanging.y)*3,(up.z-hanging.z)*3});parry|=r.parry;pressed=r.pressed;ck(r.speed>1.5f);}
 ck(pressed&&parry&&t.presses()==1);
 // Parry 2: with the guard up, a push past 1.5 m/s releases for 70 ms and presses again, once per push.
 reset();place(up,faceFront);calm(12);ck(t.presses()==1);
 r=step({0,0,-2});ck(r.parry&&!r.pressed);unsigned gap=1;
 for(unsigned i=0;i<12;i++){r=step({0,0,-2});ck(!r.parry);if(!r.pressed)++gap;}ck(r.pressed&&gap>=6&&gap<=8&&t.presses()==2); // 70 ms at 11 ms per tick
 for(unsigned i=0;i<10;i++)ck(!step({0,0,-.5f}).parry);                      // slower, but not stopped: still the same attempt
 calm(4);r=step({0,0,2});ck(r.parry);calm(12);ck(t.presses()==3);            // stopped, so the next push counts
 // A slow adjustment is no parry.
 reset();place(up,faceFront);calm(12);for(unsigned i=0;i<20;i++)ck(!step({0,0,-1.2f}).parry);ck(t.presses()==1);
 // The attack has priority: the block is released while the melee press is down and 450 ms after it.
 reset();place(up,faceFront);calm(3);attack=true;ck(!step().pressed&&step().posture);attack=false;
 for(unsigned i=0;i<38;i++)ck(!step().pressed);ck(calm(4).pressed&&t.presses()==2);   // 418 ms after the last press: still released; 451 ms: back
 // Scope loss lets go at once and needs a fresh entry.
 for(unsigned bad=0;bad<6;bad++){reset();place(up,faceFront);ck(calm(2).pressed);
  if(bad==0)allowed=false;if(bad==1)tracked=false;if(bad==2)f.hands[0].gripFlags=0;if(bad==3)f.focused=false;if(bad==4)e.binding=0;if(bad==5)e.shield=false;
  ck(!step().pressed);allowed=tracked=true;f.hands[0].gripFlags=15;f.focused=true;e.binding=5;e.shield=true;ck(!step().pressed);ck(step().pressed);}
 reset();place(up,faceFront);ck(calm(2).pressed);e.binding=6;ck(!step().pressed);ck(step().pressed); // another shield: one calm frame
 reset();place(up,faceFront);ck(calm(2).pressed);ck(!step({0,20,0}).pressed);                      // tracking jump
 // The frontend may repeat a hand frame: the press and the parry gap follow the clock only.
 reset();place(up,faceFront);calm(12);r=step({0,0,-2});ck(r.parry&&!r.pressed);
 for(unsigned i=0;i<4;i++){now+=11;const auto again=t.update(f,h,true,e,true,false,now);ck(!again.pressed&&!again.parry&&!again.sampled);}
 now+=40;ck(t.update(f,h,true,e,true,false,now).pressed);

 // Two-hand weapon: held across the front, both hands raised and ahead, and held still for 150 ms.
 auto weapon=[&](Vec3 axis){reset();e.shield=false;e.twoHandWeapon=true;e.weaponForward=axis;f.hands[0].grip.position={-.25f,1.3f,-.35f};f.hands[1].grip.position={.25f,1.3f,-.35f};};
 weapon({1,0,0});ck(!calm(10).pressed);ck(calm(8).pressed);                  // 18 frames = 198 ms, calm from the second frame
 weapon({0,0,-1});ck(!calm(30).pressed);                                     // pointing at the enemy is not a guard
 weapon({1,.3f,0});ck(calm(20).pressed);weapon({.4f,0,-1});ck(!calm(30).pressed); // 22 degrees off the front is not across
 weapon({1,0,0});f.hands[1].grip.position.y=.8f;ck(!calm(30).pressed);       // one hand down
 weapon({1,0,0});for(unsigned i=0;i<12;i++)ck(!step({0,0,-3},{0,0,-3}).pressed); // passing through the pose in a swing
 weapon({1,0,0});calm(30);ck(calm(1).pressed);r=step({0,0,-2},{0,0,-2});ck(r.parry&&!r.pressed); // pushing the held guard out is a parry
 weapon({1,0,0});calm(17);r=step({0,0,-2},{0,0,-2});ck(!r.parry&&r.pressed);  // pressed only a moment ago: that press is still the new one
 weapon({1,0,0});calm(20);ck(calm(1).pressed);f.hands[1].gripFlags=0;ck(!step().pressed); // weapon hand lost
 // The grip rule (gripGuard, 2026-10-14): the left grip held is the guard wherever the hand is, letting go ends it at
 // once, the press is a parry press; a fast swing while held is a swing parry, which lets the block go for 350 ms (the
 // risk of a missed parry) and waits 1 s for the next; the direction is the back of the shield hand. A two-hand weapon:
 // the same, the direction towards the hands.
 {LiveGuardSettings s;s.gripGuard=true;
  reset();t=LiveGuard(s);ck(!calm(3).pressed);                              // hanging arm, no grip: nothing
  f.hands[0].grabActive=true;f.hands[0].grab=true;{const auto p=step();ck(p.pressed&&p.parry&&t.presses()==1);}
  {const auto p=calm(20);ck(p.pressed&&!p.parry&&t.presses()==1&&std::abs(p.direction.z+1)<1e-3f&&std::abs(p.direction.x)<1e-3f);} // faces -Z
  f.hands[0].grab=false;ck(!step().pressed);                                // let go: at once, no grace
  f.hands[0].grab=true;ck(step().pressed&&t.presses()==2);calm(20);
  bool gap=false,swing=false;for(unsigned i=0;i<4;i++){const auto p=step({2.f,0,0});gap|=!p.pressed;swing|=p.swingParry;}  // swung at 2 m/s: a swing parry,
  ck(gap&&swing);                                                                                                            // the block let go
  {bool repress=false,early=false;for(unsigned i=0;i<40;i++){const auto p=step();repress|=p.repress;early|=p.pressed&&i<25;}   // 350 ms, then pressed again by itself
   ck(repress&&!early&&calm(1).pressed&&t.presses()==3);}
  gap=swing=false;for(unsigned i=0;i<4;i++){const auto p=step({2.f,0,0});gap|=!p.pressed;swing|=p.swingParry;}ck(!gap&&!swing);  // within 1 s: nothing
  calm(60);swing=false;for(unsigned i=0;i<4;i++)swing|=step({2.f,0,0}).swingParry;ck(swing);calm(60);                          // after it: again
  // 2026-10-19 (user: "the swing parry takes only when I swing as the enemy winds up - too early"): from 1.0 m/s, not 1.5.
  calm(60);swing=false;for(unsigned i=0;i<4;i++)swing|=step({1.2f,0,0}).swingParry;ck(swing);calm(60);                        // 1.2 m/s: a swing parry
  swing=false;for(unsigned i=0;i<4;i++)swing|=step({.8f,0,0}).swingParry;ck(!swing);calm(60);                                  // 0.8 m/s: none
  attack=true;ck(!step().pressed);attack=false;ck(!calm(10).pressed);ck(calm(40).pressed);   // an attack still comes first
  reset();t=LiveGuard(s);e.shield=false;e.twoHandWeapon=true;e.weaponForward={1,0,0};f.hands[0].grip.position={-.2f,1.3f,-.4f};f.hands[1].grip.position={.2f,1.3f,-.4f};
  calm(3);f.hands[0].grabActive=true;f.hands[0].grab=true;{const auto p=step();ck(p.pressed&&p.parry&&std::abs(p.direction.z+1)<1e-3f);}
  const auto swung=[&](Vec3 l,Vec3 r){bool any=false,released=false;for(unsigned i=0;i<4;i++){const auto p=step(l,r);any|=p.swingParry;released|=!p.pressed;}ck(any==released);return any;};
  calm(20);ck(!swung({0,0,0},{3.f,0,0}));   // the right hand's swing is not a parry
  calm(20);ck(!swung({2.f,0,0},{2.f,0,0})); // both together: a stroke, no parry
  calm(20);ck(swung({2.f,0,0},{0,0,0}));calm(40);   // the left hand alone is
  f.hands[0].grab=false;calm(5);f.hands[0].grab=true;{const auto p=step();ck(p.pressed&&p.parry);}  // pressed again: a parry press
  // Not guarding: a swing presses nothing and is no swing parry.
  f.hands[0].grab=false;ck(!calm(10).pressed);
  {bool any=false;for(unsigned i=0;i<4;i++){const auto p=step({2.f,0,0});any|=p.pressed||p.swingParry;}ck(!any);}}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
