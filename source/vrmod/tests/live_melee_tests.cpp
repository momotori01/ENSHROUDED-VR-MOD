#include <evr/live_melee.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 constexpr float dt=1.f/90;const XrDuration tick=11111111;
 HandFrame f;LiveMelee t;LiveMeleeWeapon w;uint64_t now=1000;
 auto reset=[&](){t={};f={};w={};w.binding=77;w.forward={0,1,0};f.focused=true;f.sessionGeneration=f.originGeneration=1;f.predictedTime=1000000000;
  for(auto& h:f.hands){h.gripActive=true;h.gripFlags=15;h.profile=1;h.grip=identityPose();}now=1000;};
 // One hand frame: move the right hand by velocity*dt, turn it by angular*dt (tracking axes), the left by leftVelocity*dt.
 auto step=[&](Vec3 velocity,Vec3 angular={0,0,0},Vec3 leftVelocity={0,0,0},bool allowed=true){
  ++f.frame;f.predictedTime+=tick;now+=11;auto& g=f.hands[1].grip;g.position=g.position+velocity*dt;
  const float a=length(angular)*dt;if(a>0){const auto axis=angular*(1/length(angular));g.orientation=normalized(multiply(Quat{axis.x*std::sin(a/2),axis.y*std::sin(a/2),axis.z*std::sin(a/2),std::cos(a/2)},g.orientation));}
  f.hands[0].grip.position=f.hands[0].grip.position+leftVelocity*dt;return t.update(f,w,allowed,now);};
 auto calm=[&](unsigned frames=3){LiveMeleeResult r;for(unsigned i=0;i<frames;i++)r=step({0,0,0});return r;};

 // Value checks against VHVR's formulas.
 {const auto v=angularVelocity({0,0,0,1},{0,std::sin(.05f),0,std::cos(.05f)},.01f);ck(std::abs(v.y-10)<1e-3f&&std::abs(v.x)<1e-5f&&std::abs(v.z)<1e-5f);
  const LiveMeleeSettings s;LiveMeleeWeapon one;one.binding=1;one.forward={0,2,0};
  auto m=meleeMomentum(s,one,{},{2,0,0},{0,0,8});ck(std::abs(m.velocity.x-1)<1e-5f&&std::abs(m.speed-1)<1e-5f&&m.required==3&&!m.thrust); // (0,0,8) x (0,.125,0) = (-1,0,0)
  m=meleeMomentum(s,one,{},{0,0,0},{0,30,0});ck(m.speed<1e-5f); // a twist about the weapon axis adds nothing
  m=meleeMomentum(s,one,{},{0,4.2f,0},{});ck(m.thrust&&m.speed>4.19f);m=meleeMomentum(s,one,{},{0,3.9f,0},{});ck(!m.thrust);
  m=meleeMomentum(s,one,{},{5*std::sin(.6f),5*std::cos(.6f),0},{});ck(!m.thrust&&dot(m.velocity,{0,1,0})>4); // fast enough, but 34 degrees off the axis with one hand
  one.heavy=true;ck(meleeMomentum(s,one,{},{1,0,0},{}).required==4.5f);
  LiveMeleeWeapon two=one;two.heavy=false;two.twoHanded=true;
  m=meleeMomentum(s,two,{3.5f,0,0},{3.5f,0,0},{0,0,50});ck(std::abs(m.speed-3.5f)<1e-5f); // both hands together (the weapon hand's own stroke, turn included, is the slower one here: 2.75)
  m=meleeMomentum(s,two,{4,0,0},{-4,0,0},{});ck(std::abs(m.speed-4)<1e-5f&&m.velocity.x<0);  // opposite and equal: no common direction - the weapon hand's own stroke counts
  m=meleeMomentum(s,two,{0,0,0},{1,0,0},{0,0,-24});ck(std::abs(m.speed-4)<1e-4f);           // the weapon hand turns it: 1 + 24 x 0.125
  m=meleeMomentum(s,two,{.5f,0,0},{.5f,0,0},{});ck(std::abs(m.speed-.5f)<1e-5f);            // a slow hold stays slow
  {auto heavyTwo=two;heavyTwo.heavy=true;ck(meleeMomentum(s,heavyTwo,{},{1,0,0},{}).required==3.f);}   // held with both hands a heavy weapon asks for no more
  m=meleeMomentum(s,two,{4,0,0},{0,0,0},{});ck(std::abs(m.speed-4)<1e-5f);                // the larger share along the summed direction
  m=meleeMomentum(s,two,{3*std::sin(.6f),3*std::cos(.6f),0},{5*std::sin(.6f),5*std::cos(.6f),0},{});ck(m.thrust&&std::abs(m.speed-5)<1e-4f); // 34 degrees is a thrust with two hands
  for(unsigned bad=0;bad<3;bad++){bool threw=false;try{auto x=one;if(bad==0)x.forward={0,0,0};meleeMomentum(s,x,{},bad==1?Vec3{NAN,0,0}:Vec3{},bad==2?Vec3{0,NAN,0}:Vec3{});}catch(const std::invalid_argument&){threw=true;}ck(threw);}}

 // A 4.5 m/s stroke across the weapon: one press of 120 ms, then nothing until it slows and swings again.
 reset();ck(!calm().press&&t.armed());
 auto r=step({4.5f,0,0});ck(r.fired&&r.press&&r.sampled&&std::abs(r.speed-4.5f)<1e-3f&&t.presses()==1);
 unsigned held=1;for(unsigned i=0;i<20;i++){r=step({4.5f,0,0});ck(!r.fired);held+=r.press;}ck(held>=10&&held<=12&&!r.press);
 for(unsigned i=0;i<40;i++)ck(!step({4.5f,0,0}).fired);          // still moving fast: the same stroke
 ck(!step({2,0,0}).fired&&!t.armed());ck(!step({1.4f,0,0}).fired&&t.armed()); // slower than half the requirement re-arms
 ck(step({-4.5f,0,0}).fired&&t.presses()==2);
 // Slow movement and a wrist flick never press; a fast enough rotation does (lever 12.5 cm).
 reset();calm();for(unsigned i=0;i<30;i++)ck(!step({2.9f,0,0}).fired);
 reset();calm();for(unsigned i=0;i<10;i++)ck(!step({0,0,0},{0,0,9}).press);ck(t.presses()==0);
 reset();calm();ck(step({0,0,0},{0,0,30}).fired);
 reset();calm();for(unsigned i=0;i<10;i++)ck(!step({0,0,0},{0,40,0}).fired); // twist about the weapon axis
 // Two strokes inside 400 ms are one press; the next one after the interval counts.
 reset();calm();ck(step({4.5f,0,0}).fired);ck(!step({1,0,0}).fired&&t.armed());
 for(unsigned i=0;i<20;i++)ck(!step({-4.5f,0,0}).fired);ck(t.presses()==1); // 20 frames = 220 ms after the press
 for(unsigned i=0;i<17;i++)step({-4.5f,0,0});ck(t.presses()==2);            // still swinging when the 400 ms interval ends
 // Heavy weapon: 4.2 m/s across is not enough, a 4.2 m/s thrust along it is.
 reset();w.heavy=true;calm();for(unsigned i=0;i<5;i++)ck(!step({4.2f,0,0}).fired);
 reset();w.heavy=true;calm();r=step({0,4.2f,0});ck(r.fired&&r.thrust&&r.required==4.5f);
 // Two hands: the left hand alone can carry the stroke (VHVR), opposite hands cannot.
 reset();w.twoHanded=true;calm();ck(step({0,0,0},{0,0,0},{4,0,0}).fired);
 reset();w.twoHanded=true;calm();ck(step({4,0,0},{0,0,0},{-4,0,0}).fired);   // the weapon hand's stroke alone attacks (2026-10-13)
 reset();w.twoHanded=true;w.heavy=true;calm();ck(step({3.3f,0,0}).fired);     // a heavy weapon in both hands: 3 m/s
 reset();w.twoHanded=true;calm();for(unsigned i=0;i<5;i++)ck(!step({2.5f,0,0},{0,0,0},{-1,0,0}).fired);   // slower than that: nothing
 reset();w.twoHanded=true;calm();f.hands[0].gripFlags=0;ck(!step({4.5f,0,0}).fired&&!t.armed()); // support hand lost
 // Tracking jump, a refused frame and a new weapon all need a calm sample before the next press.
 reset();calm();ck(!step({20,0,0}).fired&&!t.armed());ck(!step({4.5f,0,0}).fired);ck(!step({0,0,0}).fired&&t.armed());ck(step({4.5f,0,0}).fired);
 reset();calm();ck(!step({4.5f,0,0},{0,0,0},{0,0,0},false).press);ck(!step({4.5f,0,0}).fired);calm(1);ck(step({4.5f,0,0}).fired);
 reset();calm();w.binding=88;ck(!step({4.5f,0,0}).fired);ck(!step({4.5f,0,0}).fired);calm(1);ck(step({4.5f,0,0}).fired);
 reset();calm();ck(step({4.5f,0,0}).fired);ck(!step({0,0,0},{0,0,0},{0,0,0},false).press); // losing permission lets the press go at once
 reset();w.binding=0;ck(!step({4.5f,0,0}).press);
 // The frontend may see one hand frame several times: the press continues, nothing new fires.
 reset();calm();ck(step({4.5f,0,0}).fired);for(unsigned i=0;i<5;i++){now+=11;const auto again=t.update(f,w,true,now);ck(again.press&&!again.fired&&!again.sampled);}
 now+=200;ck(!t.update(f,w,true,now).press);ck(t.presses()==1);
 // strokeTravel: the stroke attacks only after that distance above the required speed. 4.5 m/s is 5 cm a frame.
 {LiveMeleeSettings s;s.strokeTravel=.25f;const auto travel=[&](){reset();t=LiveMelee(s);calm();};
  travel();for(unsigned i=0;i<4;i++)ck(!step({4.5f,0,0}).fired);ck(step({4.5f,0,0}).fired);ck(t.presses()==1); // 5 frames = 25 cm
  travel();for(unsigned i=0;i<3;i++)ck(!step({4.5f,0,0}).fired);ck(!step({2,0,0}).fired);                       // slowing below the speed starts again
  for(unsigned i=0;i<4;i++)ck(!step({4.5f,0,0}).fired);ck(step({4.5f,0,0}).fired);
  travel();ck(!step({4.5f,0,0}).fired);ck(!step({9,0,0}).fired);{const bool third=step({9,0,0}).fired;ck(third||step({9,0,0}).fired);}   // a fast stroke gets there in 3 or 4 frames
  travel();ck(!step({20,0,0}).fired);for(unsigned i=0;i<6;i++)ck(!step({4.5f,0,0}).fired);}                    // a tracking jump is no travel and disarms
 // A jump of the tracking from a still hand and back (headset 2026-10-18: 0.2..0.35 m in one frame, then back): no press,
 // also for a tool (no target gate); a real stroke that speeds up over the frames still attacks.
 reset();calm();ck(!step({9,0,0}).fired);ck(!step({-9,0,0}).fired);ck(!step({0,0,0}).fired);ck(t.presses()==0);
 reset();calm();ck(!step({2.5f,0,0}).fired);ck(step({5,0,0}).fired);
 {LiveMeleeSettings s;ck(s.strokeTravel==0);} // default: at once, VHVR's rule
 // The cooldown (VHVR 3.6, with the user's 80 % rule): shared by the hands, started by the caller when a stroke fired.
 {LiveCooldown c;ck(!c.cooling(5000)&&c.readyAt()==0);
  c.start(5000,7);ck(c.begin(7));ck(c.lengthMs==630&&c.waitMs()==504&&c.cooling(5000)&&c.cooling(5503)&&!c.cooling(5504)&&c.readyAt()==5504);
  ck(!c.measure(8,550)&&c.lengthMs==630);                      // another attack's measurement
  ck(c.measure(7,550)&&c.waitMs()==440&&c.cooling(5439)&&!c.cooling(5440)); // the one-hand sword: 0.55 s, ready after 0.44 s
  ck(c.measure(7,50)&&c.lengthMs==200&&c.measure(7,60000)&&c.lengthMs==3000); // kept within bounds
  c.start(9000);ck(!c.measure(0,550)&&c.lengthMs==630);        // nobody measures an attack without a serial
  ck(c.begun&&c.cooling(9503)&&!c.lost(9503));                 // and such a cooldown counts as begun
  c.clear();ck(!c.cooling(9001));
  // A press that starts no attack: it cools for the grace time only, and is then known as lost.
  c.start(20000);c.expect(11);ck(!c.begun&&c.cooling(20349)&&!c.lost(20349)&&!c.cooling(20350)&&c.lost(20350));
  ck(!c.begin(12)&&!c.begun&&c.begin(11)&&c.begun&&c.cooling(20350)&&c.cooling(20503)&&!c.cooling(20504)&&!c.lost(20600)); // the attack began late: cooling again
  c.start(30000);c.expect(13,400);ck(c.cooling(30503)&&!c.lost(30749)&&c.lost(30750)); // a heavy attack's button is held 0.4 s first
  ck(c.measure(13,1500)&&c.begun&&c.cooling(31199)&&!c.cooling(31200));              // the measurement tells that it began
  c.clear();ck(!c.cooling(30001)&&!c.lost(40000));
  LiveCooldown shared;reset();calm();
  auto cooled=[&](Vec3 velocity){f.frame++;f.predictedTime+=11000000;now+=11;f.hands[1].grip.position=f.hands[1].grip.position+velocity*.011f;
   auto r=t.update(f,w,true,now,true,&shared);if(r.fired){shared.start(now,1);shared.begin(1);}return r;};
  ck(cooled({4.5f,0,0}).fired&&t.presses()==1);const auto struck=now;
  for(unsigned i=0;i<12;i++)cooled({0,0,0});                    // the stroke ends, the hand is calm: armed again
  unsigned refused=0;while(now+11-struck<504){const auto r=cooled({-4.5f,0,0});ck(!r.fired);refused+=r.cooling;}
  ck(refused>20&&t.presses()==1&&t.armed());                    // fast enough, but cooling: no attack, and the stroke stays armed
  ck(cooled({-4.5f,0,0}).fired&&t.presses()==2&&now-struck>=504&&now-struck<520); // still fast when 80 % of 0.63 s has passed
  // A measured, shorter cooldown lets the next stroke through earlier.
  shared.measure(1,550);for(unsigned i=0;i<12;i++)cooled({0,0,0});
  const auto second=shared.from;while(now+11-second<440)ck(!cooled({4.5f,0,0}).fired);
  ck(cooled({4.5f,0,0}).fired&&t.presses()==3);
  // Without a cooldown object the fixed interval stays (the older fixtures).
  reset();calm();ck(step({4.5f,0,0}).fired);for(unsigned i=0;i<20;i++)ck(!step({-4.5f,0,0}).fired);}
 // Contact gate: a fast stroke away from every target does not attack and stays armed; it attacks when the blade
 // comes near a target while still fast. At a target every new stroke attacks.
 {const auto gated=[&](Vec3 velocity,bool nearTarget){++f.frame;f.predictedTime+=tick;now+=11;f.hands[1].grip.position=f.hands[1].grip.position+velocity*dt;return t.update(f,w,true,now,nearTarget);};
  reset();calm();for(unsigned i=0;i<6;i++)ck(!gated({4.5f,0,0},false).fired);ck(t.presses()==0&&t.armed());ck(gated({4.5f,0,0},true).fired);ck(t.presses()==1);
  for(unsigned i=0;i<40;i++)ck(!gated({4.5f,0,0},true).fired);                 // the same stroke does not attack twice
  ck(!gated({0,0,0},true).fired);for(unsigned i=0;i<40;i++)gated({0,0,0},true);ck(gated({4.5f,0,0},true).fired);ck(t.presses()==2); // the next stroke at the target
  reset();calm();ck(!gated({2,0,0},true).fired);                                   // near a target, but too slow
  // Coming at a target (reached by the look-ahead only): a slower stroke attacks, 60 % of the speed (2026-10-15); not a crawl.
  {const auto coming=[&](Vec3 velocity,bool approaching){++f.frame;f.predictedTime+=tick;now+=11;f.hands[1].grip.position=f.hands[1].grip.position+velocity*dt;return t.update(f,w,true,now,true,nullptr,approaching);};
   reset();calm();for(unsigned i=0;i<3;i++)ck(!coming({2.5f,0,0},false).fired);reset();calm();ck(coming({2.5f,0,0},true).fired);ck(t.presses()==1);
   reset();calm();for(unsigned i=0;i<3;i++)ck(!coming({1.5f,0,0},true).fired);ck(t.presses()==0);}
  // In the air a stroke up needs a quarter more (2026-10-20): 3.4 m/s up attacks with the factor 1, not with 1.25, and stays
  // armed - a fast blow down then attacks; 4 m/s up attacks with 1.25.
  {const auto rising=[&](Vec3 velocity,float factor){++f.frame;f.predictedTime+=tick;now+=11;f.hands[1].grip.position=f.hands[1].grip.position+velocity*dt;return t.update(f,w,true,now,true,nullptr,false,factor);};
   reset();calm();ck(rising({0,3.4f,0},1.f).fired);
   reset();calm();for(unsigned i=0;i<4;i++)ck(!rising({0,3.4f,0},1.25f).fired);ck(t.presses()==0&&t.armed());rising({0,0,0},1.25f);ck(rising({0,-4.5f,0},1.25f).fired);
   reset();calm();ck(rising({0,4.f,0},1.25f).fired);
   reset();calm();ck(rising({3.4f,0,0},1.25f).fired);}                  // level: as before
  LiveMeleeWeapon d;ck(!d.blade&&d.needsTarget);}
 // The left-hand part of a weapon pair (hand 0): the left hand's own stroke attacks, the right hand's does not; and a
 // bare hand needs 0.45 of the requirement (VHVR FistCollision).
 {reset();w.hand=0;calm();
  auto leftStep=[&](Vec3 leftVelocity,Vec3 rightVelocity={0,0,0}){++f.frame;f.predictedTime+=tick;now+=11;
   f.hands[0].grip.position=f.hands[0].grip.position+leftVelocity*dt;f.hands[1].grip.position=f.hands[1].grip.position+rightVelocity*dt;return t.update(f,w,true,now);};
  for(unsigned i=0;i<3;i++)leftStep({0,0,0});
  for(unsigned i=0;i<6;i++)ck(!leftStep({0,0,0},{4.5f,0,0}).fired);ck(t.presses()==0);       // the other hand moves: nothing
  const auto r=leftStep({4.5f,0,0});ck(r.fired&&r.speed>4.4f&&r.required==3&&t.presses()==1);
  reset();w.hand=0;w.speedFactor=.45f;calm();for(unsigned i=0;i<3;i++)leftStep({0,0,0});
  ck(!leftStep({1.2f,0,0}).fired);for(unsigned i=0;i<60;i++)leftStep({0,0,0});
  const auto fist=leftStep({1.5f,0,0});ck(fist.fired&&std::abs(fist.required-1.35f)<1e-5f);
  reset();w.hand=2;bool rejected=false;try{step({0,0,0});}catch(const std::invalid_argument&){rejected=true;}ck(rejected);}
 // A fist (VHVR FistCollision): the hand's own speed, a turn of the wrist adds nothing; the jab runs along the knuckles
 // above 3 m/s; the game's unarmed item is the one of the data.
 {const LiveMeleeSettings s;LiveMeleeWeapon fist;fist.binding=(uint64_t(unarmedFistsItem)<<32)|2;fist.fist=true;fist.speedFactor=fistSpeedFactor;fist.forward={0,1,0};
  auto m=meleeMomentum(s,fist,{},{0,0,0},{0,0,40});ck(m.speed<1e-6f&&std::abs(m.required-1.35f)<1e-5f);       // a held weapon would have 5 m/s here
  LiveMeleeWeapon held=fist;held.fist=false;held.speedFactor=1;ck(meleeMomentum(s,held,{},{0,0,0},{0,0,40}).speed>4.9f);
  m=meleeMomentum(s,fist,{},{1.4f,0,0},{0,0,40});ck(std::abs(m.speed-1.4f)<1e-5f&&m.speed>m.required&&!m.thrust);
  m=meleeMomentum(s,fist,{},{0,3.2f,0},{});ck(m.thrust);m=meleeMomentum(s,fist,{},{0,2.9f,0},{});ck(!m.thrust&&m.speed>m.required);
  m=meleeMomentum(s,fist,{},{3.2f,0,0},{});ck(!m.thrust);                                                    // fast, but not along the knuckles
  ck(unarmedFistsItem==3667811355u&&uint32_t(fist.binding>>32)==unarmedFistsItem);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
