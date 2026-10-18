#include <evr/live_secondary.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const XrDuration tick=11111111;   // hands at 90 Hz
 // --- MotionHistory (VHVR PhysicsEstimator) ---
 {MotionHistory h;XrTime t=1000000000;Vec3 p{};
  ck(length(h.velocity())==0&&length(h.averageVelocity())==0&&length(h.longestLocomotion(1))==0);
  for(unsigned i=0;i<180;++i){t+=tick;p.x+=2.f/90;h.add(t,p);}                       // two seconds at 2 m/s
  ck(std::abs(h.velocity().x-2)<.02f&&std::abs(h.averageVelocity().x-2)<.02f&&std::abs(h.velocity().y)<1e-4f);
  {const auto far1=h.longestLocomotion(1),half=h.longestLocomotion(.5f),tiny=h.longestLocomotion(.05f);
   ck(far1.x>1.7f&&far1.x<2.5f);ck(half.x>.7f&&half.x<1.4f&&half.x<far1.x);ck(length(tiny)==0);}   // the last second / half second / nothing that recent
  for(unsigned i=0;i<180;++i){t+=tick;h.add(t,p);}                                    // then two seconds at rest
  ck(length(h.velocity())<1e-4f&&length(h.averageVelocity())<1e-4f&&length(h.longestLocomotion(1))<1e-4f);
  // The mean lags behind the newest velocity: right after a start it is smaller.
  for(unsigned i=0;i<6;++i){t+=tick;p.z-=4.f/90;h.add(t,p);}
  ck(h.velocity().z<-.5f&&std::abs(h.averageVelocity().z)<std::abs(h.velocity().z));
  // A gap in time or a bad position starts over.
  h.add(t+500000000,p);ck(length(h.velocity())==0&&length(h.longestLocomotion(1))==0);
  for(unsigned i=0;i<20;++i){t+=tick;h.add(t+500000000,p);}h.add(t+500000000+tick,{std::nanf(""),0,0});ck(length(h.averageVelocity())==0);}
 // A slow frame rate (25 a second, as on the headset) still measures the same speed.
 {MotionHistory h;XrTime t=1000000000;Vec3 p{};for(unsigned i=0;i<50;++i){t+=40000000;p.y-=3.f/25;h.add(t,p);}
  ck(std::abs(h.velocity().y+3)<.03f&&std::abs(h.averageVelocity().y+3)<.03f&&h.longestLocomotion(1).y<-2.5f);}

 // --- which rule a weapon takes ---
 ck(secondaryStyle(2,false)==SecondaryStyle::Sword&&secondaryStyle(10,false)==SecondaryStyle::Sword);
 ck(secondaryStyle(0,false)==SecondaryStyle::AxeClub&&secondaryStyle(1,false)==SecondaryStyle::AxeClub);
 ck(secondaryStyle(13,true)==SecondaryStyle::DualKnives&&secondaryStyle(13,false)==SecondaryStyle::None);
 for(uint8_t t:{uint8_t(3),uint8_t(4),uint8_t(5),uint8_t(6),uint8_t(7),uint8_t(8),uint8_t(9),uint8_t(11),uint8_t(12),uint8_t(255)})ck(secondaryStyle(t,true)==SecondaryStyle::None);

 // --- the rules (swing requirement 3 m/s) ---
 SecondaryMotion base;base.forward={0,0,-1};base.otherForward={0,0,-1};base.player={0,0,0};base.mainPosition={.3f,1.2f,-.4f};base.otherPosition={-.3f,1.2f,-.4f};
 // Sword: a strong thrust - along the blade, faster than 3 m/s, a metre forward within the last second.
 {auto m=base;m.mainVelocity={0,0,-4};m.mainAverage={0,0,-3.5f};m.mainLongest={0,0,-1.2f};
  auto r=secondaryAttack(SecondaryStyle::Sword,m);ck(r.secondary&&r.thrust&&r.strongThrust);
  auto s=m;s.mainLongest={0,0,-.6f};ck(!secondaryAttack(SecondaryStyle::Sword,s).secondary);            // a short jab
  s=m;s.mainVelocity={0,0,-2.5f};ck(!secondaryAttack(SecondaryStyle::Sword,s).secondary);              // too slow
  s=m;s.mainAverage={3,0,-3};ck(!secondaryAttack(SecondaryStyle::Sword,s).thrust);                     // 45 degrees off the blade: a swing
  s=m;s.mainAverage={0,-1.5f,-3};ck(secondaryAttack(SecondaryStyle::Sword,s).secondary);               // 27 degrees: still a thrust
  // Two hands: 40 degrees, and both hands together along the blade; no travel needed.
  s=m;s.twoHanded=true;s.mainVelocity={0,0,-2};s.otherVelocity={0,0,-1.5f};s.mainLongest={};s.mainAverage={0,-2,-3};ck(secondaryAttack(SecondaryStyle::Sword,s).secondary);
  s.otherVelocity={0,0,-.5f};ck(!secondaryAttack(SecondaryStyle::Sword,s).secondary);
  // A wide swing of the sword is the first attack.
  s=m;s.mainVelocity={0,-6,0};s.mainAverage={0,-6,0};s.hitLongest={0,-2,0};ck(!secondaryAttack(SecondaryStyle::Sword,s).secondary);}
 // Axe and club: a strong swing that is not a thrust - 4.5 m/s without the sideways part, the hit point 1.5 m within half a second.
 {auto m=base;m.mainVelocity={0,-5,0};m.mainAverage={0,-5,0};m.hitLongest={0,-1.6f,0};
  auto r=secondaryAttack(SecondaryStyle::AxeClub,m);ck(r.secondary&&r.strongSwing&&!r.thrust);
  auto s=m;s.hitLongest={0,-1.2f,0};ck(!secondaryAttack(SecondaryStyle::AxeClub,s).secondary);         // a short chop
  s=m;s.mainVelocity={0,-4,0};ck(!secondaryAttack(SecondaryStyle::AxeClub,s).secondary);               // below 4.5 m/s
  // Sideways around the body does not count: the hand is in front right, so "sideways" is (-0.8, 0, -0.6).
  s=m;s.mainVelocity={-4.8f,0,-3.6f};s.mainAverage=s.mainVelocity;s.hitLongest={-1.6f,0,-1.2f};
  {const auto q=secondaryAttack(SecondaryStyle::AxeClub,s);ck(!q.strongSwing&&!q.secondary);}
  // A thrust with an axe is not its second attack, however fast.
  s=m;s.mainVelocity={0,0,-6};s.mainAverage={0,0,-6};s.hitLongest={0,0,-2};s.mainLongest={0,0,-2};
  {const auto q=secondaryAttack(SecondaryStyle::AxeClub,s);ck(q.thrust&&!q.secondary);}
  // Two hands: the two hands' speeds together.
  s=m;s.twoHanded=true;s.mainVelocity={0,-2.5f,0};s.otherVelocity={0,-2.5f,0};s.hitLongest={};ck(secondaryAttack(SecondaryStyle::AxeClub,s).secondary);
  s.otherVelocity={0,-1.5f,0};ck(!secondaryAttack(SecondaryStyle::AxeClub,s).secondary);}
 // The pair of daggers: a hook, or both hands striking together.
 {auto m=base;m.forward={0,1,0};m.otherForward={0,1,0};m.mainAverage={4,0,0};m.mainVelocity={4,0,0};m.mainLongest={1.1f,0,.1f};
  auto r=secondaryAttack(SecondaryStyle::DualKnives,m);ck(r.secondary&&r.hook&&!r.bothHands);
  auto s=m;s.mainAverage={3.5f,0,0};ck(!secondaryAttack(SecondaryStyle::DualKnives,s).secondary);       // below 3.75 m/s
  s=m;s.mainLongest={.6f,0,0};ck(!secondaryAttack(SecondaryStyle::DualKnives,s).secondary);            // less than a metre
  s=m;s.mainLongest={1.1f,0,1.1f};ck(!secondaryAttack(SecondaryStyle::DualKnives,s).secondary);        // the way travelled bends by 45 degrees
  // Both hands along their own blades (pointing forward here), close together.
  s=base;s.mainAverage={0,0,-2.5f};s.otherAverage={0,0,-2};s.mainPosition={.2f,1.2f,-.4f};s.otherPosition={-.2f,1.2f,-.4f};
  r=secondaryAttack(SecondaryStyle::DualKnives,s);ck(r.secondary&&r.bothHands&&!r.hook);
  auto t=s;t.otherAverage={0,0,-.5f};ck(!secondaryAttack(SecondaryStyle::DualKnives,t).secondary);      // one hand hardly moves
  t=s;t.otherAverage={0,0,-1.2f};ck(!secondaryAttack(SecondaryStyle::DualKnives,t).secondary);          // 3.7 m/s together
  t=s;t.otherPosition={-.6f,1.2f,-.4f};ck(!secondaryAttack(SecondaryStyle::DualKnives,t).secondary);    // 0.8 m apart
  // The same movements with one sword are no second attack, and a weapon without a rule never has one.
  ck(!secondaryAttack(SecondaryStyle::Sword,m).secondary&&!secondaryAttack(SecondaryStyle::None,m).secondary&&!secondaryAttack(SecondaryStyle::None,s).secondary);}
 // Bad input never attacks.
 {auto m=base;m.mainVelocity={0,0,-4};m.mainAverage={0,0,-3.5f};m.mainLongest={0,0,-1.2f};m.forward={0,0,0};ck(!secondaryAttack(SecondaryStyle::Sword,m).secondary);
  m.forward={0,0,-1};m.mainVelocity.x=std::nanf("");ck(!secondaryAttack(SecondaryStyle::Sword,m).secondary);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
