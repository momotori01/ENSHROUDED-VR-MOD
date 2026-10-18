#pragma once
#include <evr/live_pad.hpp>
#include <utility>
namespace evr::motion {
// Live guard: VHVR ShieldBlock "Gesture" and WeaponBlock
// (docs/analysis/vhvr_spec_20261010/block.md 2.2, 3.1, 4.1) turned into the game's
// own block button. Block cost, damage reduction, parry effect and timing window
// stay native.
//
// VHVR decides at the moment of a hit, from the direction of that hit. This mod
// cannot read incoming hits yet, so the attack side is taken to be in front of the
// head (or above it), and the user's own rule is added so that a hanging arm never
// holds the block: the guarding hand is raised to navel height or higher and held
// ahead of the body. A parry is a new block press: raising the guard is one, and
// with the guard already up VHVR's parry motion (the guarding hand passing
// 1.5 m/s) releases and presses again.
// Wider since the headset run 20261014-124515 (user: "the guard succeeded very rarely - take a much wider range, so
// that when I think I guarded, it is a guard"). That run: 105 guards in 30 minutes, half of them gone within 0.74 s
// and a quarter within 0.3 s - 66 times the face turned past the exit, 39 times the hand height or reach. The guard
// came and went under the hit. Now the face may turn to 75 degrees to come in and to 90 to stay, the hand may be
// lower and nearer, and a guard that the pose leaves is still held for releaseMs.
struct LiveGuardSettings {
 float facing=.25f,facingExit=0.f;                 // shield face within 75 degrees of the front to come in, 90 to stay (VHVR Gesture 60, sweeping 120)
 float raised=-.65f,raisedExit=-.75f;              // guarding hand height relative to the head, metres
 float front=.10f,frontExit=0.f;                   // shield hand ahead of the head along the body's front
 uint64_t releaseMs=300;                           // the press stays this long after the pose has left the guard
 // After a melee press the block stays released this long. The game reads the attack press on its next frame and
 // starts the action 80..140 ms later (to 340 ms behind a block); a block press in between starts a block instead,
 // or ends the attack it had just begun. Headset run 20261014-124515: of 39 strokes that attacked, 8 started
 // nothing, and 6 of those had the shield's block (sequence 1650552817) start or run right after the 16 ms press.
 uint64_t attackYieldMs=450;
 // The body's front follows the head's heading: at once when the head is turned further than this from it, and
 // otherwise slowly (it has come to the head after about this many seconds).
 float bodyDeadZone=.785f;float bodySettleSeconds=2.f;
 float weaponFront=.25f,weaponFrontExit=.18f;      // both hands of a two-hand weapon guard, pushed out further than a carry
 float overhead=-.05f;                             // hand this close to eye level or above: a shield face turned up also guards
 float parryEntrySpeed=1.5f,parryExitSpeed=.2f;    // VHVR MIN_PARRY_ENTRY_SPEED / PARRY_EXIT_SPEED
 unsigned parryExitSamples=3;
 uint64_t parryGapMs=70;                           // released this long so the game sees a new press
 float acrossLow=60,acrossHigh=120;                // VHVR WeaponBlock: weapon across the attack line, degrees
 float calmSpeed=1.f;uint64_t calmMs=150;          // a two-hand guard is a held stance, not a point passed in a swing
 float maximumSpeed=15.f;XrDuration maximumGap=100000000;
 // The grip rule (user 2026-10-14: "with a weapon that can guard, the guard is up while the left grip is held; the
 // block faces where the shield faces; the moment the grip is pressed, or the left controller swung fast, is a parry -
 // the same for a two-hand weapon"): the left grip alone takes the guard up and down, at once; the pose only gives the
 // direction. Off, the pose rule above decides (the tests of the pose).
 // With the grip rule a swing does not release the guard: the game parries only with a block pressed while no block is
 // running (277390 leaves when the actor's sequence is the block's, trigger 8, at 277455), and a block let go is still
 // running for about 0.35 s - headset run 9 (20261015-002837): pressed again 75 ms after a swing let it go, the block
 // came back 340 ms later (a first press: 50 ms), and no parry. The swing's release only left the shield down under
 // the hit. The grip's press is the game's own press (a parry press); a fast swing of the left hand while the guard
 // is held (VHVR's parry motion; with a two-hand weapon the left hand alone - both together are a stroke) is reported
 // as swingParry, and the game is made to take a parry at that moment (hooks/parry_observer.hpp; user 2026-10-15:
 // "while guarding, a quick swing of the left hand parries at that moment - a parry you make intuitively by swinging
 // the shield during the block").
 bool gripGuard=false;
 // The swing parry's risk (user 2026-10-15: "after a swing parry the block let go runs on for about 0.35 s - a time in
 // which an attack is not guarded, the risk of a missed parry"; "both": the release and a pause between swing parries):
 // a swing parry lets the block go for swingReleaseMs, parried or not, and the next swing parry waits swingCooldownMs.
 uint64_t swingReleaseMs=350,swingCooldownMs=1000;
 // The swing parry starts at this speed of the left hand (user 2026-10-19 01:30: "the swing parry succeeds only when I swing
 // as the enemy winds up - I must swing much too early; make it take earlier"): 1.5 m/s (VHVR's shield entry) is reached
 // well into a flick of the wrist, 1.0 (VHVR's fist parry) nearer its start. Only with the grip rule and the guard held.
 float swingParrySpeed=1.f;
};
struct LiveGuardEquipment {
 uint64_t binding=0;bool shield=false,twoHandWeapon=false; // shield on the left hand, or a weapon held with both hands
 Vec3 weaponForward{0,1,0};                                 // tracking space, for the two-hand weapon
};
// direction: the way the guard faces, level, in tracking space (unit; zero when unknown) - the back of the shield hand,
// or from the head to the middle of the hands holding a weapon across. The game blocks toward where the character
// looks; while the guard holds, the character looks this way (user 2026-10-14: "decide the guard by the hand's
// direction, not the camera's").
// repress: the block pressed again by itself while the guard was held (after a swing parry's release or an attack) -
// not the user's press, so no parry press (parry_observer.hpp takes the game's parry input of it away).
struct LiveGuardResult {bool pressed=false,posture=false,parry=false,sampled=false,swingParry=false,repress=false;float facing=0,speed=0;Vec3 direction{};};
class LiveGuard {
 LiveGuardSettings settings_{};HandFrame previous_{};bool havePrevious_=false,posture_=false,attempting_=false,pressedBefore_=false;
 uint64_t binding_=0,gapUntil_=0,calmSince_=0,pressedSince_=0,graceUntil_=0,attackUntil_=0,swingCooldownUntil_=0;unsigned slow_=0,presses_=0;Vec3 front_{0,0,-1};LiveGuardResult last_{};
 float bodyYaw_=0;bool bodyKnown_=false;
 void lose(){havePrevious_=posture_=attempting_=pressedBefore_=false;gapUntil_=calmSince_=pressedSince_=graceUntil_=0;slow_=0;}
 static bool ahead(float height,float distance,float needed){return height>=0||distance>=needed;} // overhead needs no reach
public:
 LiveGuard()=default;explicit LiveGuard(LiveGuardSettings s):settings_(s){}
 unsigned presses()const{return presses_;}
 // attackPress: the live melee press is down on this tick; the attack has priority (user rule), and keeps it for
 // attackYieldMs after the press is let go.
 // The level part of v as a unit vector, or zero when it is (almost) vertical.
 static Vec3 level(Vec3 v){const float n=std::sqrt(v.x*v.x+v.z*v.z);return n>.2f*std::max(1e-6f,length(v))?Vec3{v.x/n,0,v.z/n}:Vec3{};}
 LiveGuardResult update(const HandFrame& f,Pose head,bool headTracked,const LiveGuardEquipment& e,bool allowed,bool attackPress,uint64_t now){
  LiveGuardResult out;
  if(attackPress)attackUntil_=now+settings_.attackYieldMs;
  const auto& left=f.hands[0];const auto& right=f.hands[1];
  const bool valid=allowed&&headTracked&&e.binding&&(e.shield||e.twoHandWeapon)&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&
   finite(head.position)&&left.tracked()&&finite(left.grip.position)&&(e.shield||(right.tracked()&&finite(right.grip.position)&&finite(e.weaponForward)&&length(e.weaponForward)>1e-4f));
  if(!valid){lose();return out;}
  const bool repeated=havePrevious_&&f.frame==previous_.frame&&f.sessionGeneration==previous_.sessionGeneration;
  if(!repeated){
   try{
    const bool continuous=havePrevious_&&binding_==e.binding&&previous_.sessionGeneration==f.sessionGeneration&&previous_.originGeneration==f.originGeneration&&
     previous_.hands[0].profile==left.profile&&previous_.hands[1].profile==right.profile&&f.frame>previous_.frame&&
     f.predictedTime>previous_.predictedTime&&f.predictedTime-previous_.predictedTime<=settings_.maximumGap;
    if(!continuous){posture_=attempting_=false;calmSince_=0;slow_=0;}
    float speed=0,dt=0,rightSpeed=0;
    if(continuous){
     dt=float(f.predictedTime-previous_.predictedTime)*1e-9f;
     speed=length(left.grip.position-previous_.hands[0].grip.position)/dt;
     if(!e.shield)rightSpeed=length(right.grip.position-previous_.hands[1].grip.position)/dt;
     if(!e.shield&&!settings_.gripGuard)speed=std::max(speed,rightSpeed);
     if(!std::isfinite(speed)||speed>settings_.maximumSpeed)throw std::invalid_argument("tracking jump");
    }
    // The body's front: the head's heading, which it follows at once only when the head is turned further than
    // bodyDeadZone (45 degrees) from it, and otherwise slowly. Headset run 20261014-162900 (user: "the guard hardly
    // ever holds, even with the shield held before the body"): the front was the direction from the head to the
    // point between the hands, and it swung with every stroke of the sword hand and every reach to a holster - 49
    // guards, half of them gone within 1.0 s, 32 of those at a face exactly 90 degrees from that front. Before that
    // (headset run game100334) the head's own heading made a resting hand a guard as soon as the head turned; the
    // dead zone keeps a glance from moving the front. Looking straight up or down keeps the last front.
    {const auto forward=rotate(head.orientation,{0,0,-1});
     if(forward.x*forward.x+forward.z*forward.z>.04f){
      const float headYaw=std::atan2(-forward.x,-forward.z);
      if(!bodyKnown_){bodyYaw_=headYaw;bodyKnown_=true;}
      else{const float d=std::remainder(headYaw-bodyYaw_,2*pi);
       if(std::abs(d)>settings_.bodyDeadZone)bodyYaw_=headYaw-std::copysign(settings_.bodyDeadZone,d);
       else if(dt>0&&settings_.bodySettleSeconds>0)bodyYaw_+=d*std::min(1.f,dt/settings_.bodySettleSeconds);}
      front_={-std::sin(bodyYaw_),0,-std::cos(bodyYaw_)};}}
    const auto fromHead=[&](Vec3 p){const auto d=p-head.position;return std::pair<float,float>{d.y,dot(d,front_)};};
    const auto [leftHeight,leftAhead]=fromHead(left.grip.position);
    bool enter=false,stay=false;
    if(e.shield){
     // VHVR: the shield face is the back of the shield hand (left controller -X), not the mesh.
     const auto face=rotate(left.grip.orientation,{-1,0,0});
     // A face turned up guards only overhead: at chest height that is simply a resting hand (same run).
     out.facing=leftHeight>=settings_.overhead?std::max(dot(face,front_),face.y):dot(face,front_);
     out.direction=level(face);
     enter=out.facing>settings_.facing&&leftHeight>=settings_.raised&&ahead(leftHeight,leftAhead,settings_.front);
     stay=out.facing>=settings_.facingExit&&leftHeight>=settings_.raisedExit&&ahead(leftHeight,leftAhead,settings_.frontExit);
    }else{
     const auto [rightHeight,rightAhead]=fromHead(right.grip.position);
     out.direction=level((left.grip.position+right.grip.position)*.5f-head.position);
     const auto axis=e.weaponForward*(1/length(e.weaponForward));
     const float angle=std::acos(std::clamp(dot(axis,front_),-1.f,1.f))*180.f/pi;out.facing=angle;
     const bool across=angle>settings_.acrossLow&&angle<settings_.acrossHigh;
     const bool raised=leftHeight>=settings_.raised&&rightHeight>=settings_.raised&&ahead(leftHeight,leftAhead,settings_.weaponFront)&&ahead(rightHeight,rightAhead,settings_.weaponFront);
     if(continuous&&speed<settings_.calmSpeed){if(!calmSince_)calmSince_=now;}else calmSince_=0;
     enter=across&&raised&&calmSince_&&now-calmSince_>=settings_.calmMs;
     stay=angle>settings_.acrossLow-10&&angle<settings_.acrossHigh+10&&leftHeight>=settings_.raisedExit&&rightHeight>=settings_.raisedExit&&
      ahead(leftHeight,leftAhead,settings_.weaponFrontExit)&&ahead(rightHeight,rightAhead,settings_.weaponFrontExit);
    }
    if(settings_.gripGuard)enter=stay=left.grabActive&&left.grab;
    const bool was=posture_;
    posture_=continuous&&(posture_?stay:enter);
    // A guard the pose has just left is held a moment longer; a lost hand, another item or a jump lets go at once.
    if(!continuous)graceUntil_=0;else if(was&&!posture_&&pressedBefore_&&!settings_.gripGuard)graceUntil_=now+settings_.releaseMs;else if(posture_)graceUntil_=0;
    // VHVR parry motion: one attempt per fast move; it re-arms once the hand has stopped.
    if(continuous){
     const float entry=settings_.gripGuard&&posture_&&was?std::min(settings_.parryEntrySpeed,settings_.swingParrySpeed):settings_.parryEntrySpeed;
     if(!attempting_&&speed>entry){
      attempting_=true;slow_=0;
      // Two-hand weapon, grip rule: both hands swung together is a stroke - the attack comes first (user 2026-10-14:
      // "held with both hands and swung with both at once, as some do, the attack has priority"); the left alone parries.
      const bool alone=e.shield||!settings_.gripGuard||rightSpeed<settings_.parryEntrySpeed;
      if(alone&&!settings_.gripGuard&&was&&posture_&&pressedBefore_&&now>=pressedSince_&&now-pressedSince_>settings_.parryGapMs&&now>=gapUntil_){gapUntil_=now+settings_.parryGapMs;out.parry=true;}
      if(alone&&settings_.gripGuard&&was&&posture_&&pressedBefore_&&!attackPress&&now>=attackUntil_&&now>=swingCooldownUntil_){
       out.swingParry=true;gapUntil_=now+settings_.swingReleaseMs;swingCooldownUntil_=now+settings_.swingCooldownMs;}
     }else if(attempting_){if(speed<=settings_.parryExitSpeed){if(++slow_>=settings_.parryExitSamples)attempting_=false;}else slow_=0;}
    }
    if(posture_&&!was&&(attempting_||settings_.gripGuard))out.parry=true; // raised quickly into place (or the grip pressed): the press itself is the parry press
    out.speed=speed;out.sampled=true;
   }catch(const std::exception&){lose();out={};}
   previous_=f;binding_=e.binding;havePrevious_=true;
  }else{out=last_;out.parry=out.sampled=false;}
  out.posture=posture_;
  out.pressed=(posture_||now<graceUntil_)&&!attackPress&&now>=attackUntil_&&now>=gapUntil_;
  out.repress=settings_.gripGuard&&out.pressed&&!pressedBefore_&&!out.parry&&posture_;
  if(out.pressed&&!pressedBefore_){pressedSince_=now;++presses_;}
  pressedBefore_=out.pressed;last_=out;return out;
 }
};
}
