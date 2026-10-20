#pragma once
#include <evr/live_pad.hpp>
#include <evr/motion_intent.hpp>
#include <array>
namespace evr::motion {
// Continuous melee for live play: VHVR WeaponCollision::hasMomentum decides that a
// swing happened (docs/analysis/vhvr_spec_20261010/melee.md, 3.2 and 3.4) and the
// swing becomes one native attack press. Stamina, combo, damage and the hit test
// stay native.
//
// VHVR tests momentum when the weapon enters a target, so a stroke through the air
// does nothing there. update() takes that as nearTarget (live_targets.hpp: the blade
// is near something that has Health). Without the gate (nearTarget always true) a
// stroke through the air also attacks. One press per stroke; between attacks stands the cooldown
// (LiveCooldown below), VHVR's per-target cooldown taken as one for the player's strokes.
struct LiveMeleeSettings {
 float swingSpeed=3.f;                         // VHVR SwingSpeedRequirement default, m/s
 float heavyFactor=1.5f;                       // VHVR battle axe / sledge / polearm
 float lever=.125f;                            // VHVR: rotation counts 12.5 cm ahead of the hand, not at the tip
 float thrustSpeed=4.f;                        // VHVR MIN_STAB_SPEED along the weapon
 float thrustDegrees=30.f,thrustDegreesTwoHanded=40.f;
 float jabSpeed=3.f;                           // VHVR FistCollision: a bare hand's jab, within thrustDegrees of the fist's direction
 float rearmFactor=.5f;                        // the stroke must slow below this share before the next press
 // The share of the required speed that attacks while the blade is still coming at a target (update's approaching:
 // the target is reached by the look-ahead of live_melee_targets.hpp only, not by the blade itself). The game needs
 // about 0.1 s from the press to the hit volume; with the full speed asked only at contact, the press came when the
 // blade was already in the body (headset run 10: 0.4..0.8 m from the axis at 3.9 m/s), the hit a tenth of a second
 // later (user 2026-10-15: "the hit and the enemy's reaction come well after I felt the sword land"). 1 = no change.
 float approachFactor=.8f;   // .6 until headset run 11 (2026-10-15): half of the ground presses came at 1.8..2.9 m/s - "attacks go off by themselves"
 // Metres the hand has to cover above the required speed before the stroke attacks. VHVR needs none: it looks at the
 // speed only when the weapon enters a target, so a quick hand movement in the air does nothing there. Here, without
 // that gate, 0 attacks on the first fast sample, which felt too sensitive on the headset (game180455).
 float strokeTravel=0.f;
 uint64_t pressMs=120,intervalMs=400;          // native tap length; shortest time between presses without a LiveCooldown
 uint64_t releaseMs=50;                        // with one: the button is up at least this long before the next press
 float maximumSpeed=15.f,maximumAngularSpeed=60.f; // tracking jump limits, m/s and rad/s
 // A tracking jump also when the hand's velocity changes by more than this from one sample to the next (2026-10-18;
 // headset, user: "walking, my feet stop and my hands shake - the felling axe reacts to the tracking's jitter"): the
 // right controller jumped 0.17..0.34 m in one frame (about 30 ms) from a still hand and back, five times in 30 s, and
 // each time a tool, which has no target gate, chopped (bug report 20261018-092250). Real strokes change by 2.4 m/s
 // a sample at the most at that frame rate (68 strokes of 2026-10-17); the jumps by 9.7 or more.
 float maximumSpeedChange=6.f;
 XrDuration maximumGap=100000000;
};
struct LiveMeleeWeapon {
 uint64_t binding=0;bool heavy=false,twoHanded=false;
 // Contact gate: the blade from the grip to the tip as two world points (Q32 fixed point), and whether this item
 // needs a target at all (a tool does not: mining and felling aim with the view, as in the game).
 bool blade=false,needsTarget=true;std::array<int64_t,3> bladeFrom{},bladeTo{};
 Vec3 forward{0,1,0}; // weapon axis in tracking space on this frame, handle to tip
 // The hand that carries it (1 right, 0 left: the left-hand part of a weapon pair, or the left fist), and the share
 // of the swing requirement it needs (VHVR FistCollision: a bare hand needs 0.45 of it; a held weapon all of it).
 unsigned hand=1;float speedFactor=1.f;
 // A bare hand (VHVR FistCollision): the hand's own speed counts, without the lever point of a held weapon; `forward`
 // is the direction of the knuckles.
 bool fist=false;
 // For VHVR's secondary attack by motion (live_secondary.hpp): the native weapon type (255: none of them, a tool)
 // and the metres from the grip to the tip.
 uint8_t weaponType=255;float reach=0;
};
// The game's weapon of the bare hands: the only item of weapon type UnarmedFists in the game data
// (docs/analysis/weapon_mount_20261010/held-models/held-models.json: _Weapon_Unarmed_Fists_Hand_lvl00, no model).
constexpr uint32_t unarmedFistsItem=3667811355u;constexpr float fistSpeedFactor=.45f;
// VHVR gives every target a cooldown as long as the weapon's attack and refuses an attack on it while that runs
// (docs/analysis/vhvr_spec_20261010/melee.md 3.6; user, 2026-10-12: "as VHVR's default, no hit during the cooldown").
// Here a stroke is one native attack and the game's own hit volume finds whom it hits, so there is one cooldown for
// the player's strokes, either hand's. It starts with the attack a stroke started. Its length is the game's own: the
// time from that attack's start until its action lets the next attack begin (the first EnableAbilityEvent with the
// attack ability; measured from the action's data when it starts, hooks/motion_event_selection_observer.hpp) - one-hand
// sword 0.55 s, great axe 0.70 s, daggers 0.60 s, fists 0.70 s, felling axe 1.2 s. Until that is known: VHVR's default
// attack duration, 0.63 s.
// The user's rule on top (2026-10-12): the next stroke attacks once `share` (80 %) of the cooldown has passed, and
// the hand vibrates until then - so that a stroke made "about when it should be over" comes out. A little more
// damage per second than the game's own pace is accepted.
struct LiveCooldown {
 float share=.8f;
 uint64_t fallbackMs=630,minimumMs=200,maximumMs=3000;
 // A press that the game did not take (the character was hurt, dodging, in the closed part of an action) starts no
 // attack: there is nothing to cool down from, and the next stroke must not wait for it (headset run
 // game-20261013-005905, the user: "the vibration had ended and the second strike did not come", "a cooldown even
 // after a stroke that did nothing"). The caller tells when the attack of the press has begun (begin(), or measure());
 // until then the press cools for graceMs only - the press took 157 ms (median) and at most 378 ms to start its
 // attack in that run - plus the time a heavy attack's button is held.
 uint64_t graceMs=350;
 uint64_t from=0,lengthMs=0,serial=0,grace=0;bool begun=false;
 // A stroke attacked. `attack` identifies it for begin() and measure() (LiveAttackPress::serial; 0 when nobody will
 // tell: such a cooldown counts as begun).
 void start(uint64_t now,uint64_t attack=0){from=now;lengthMs=fallbackMs;serial=attack;grace=graceMs;begun=!attack;}
 // The press of the stroke that started this cooldown, and how long its button is held before the game acts on it.
 void expect(uint64_t attack,uint64_t heldMs=0){serial=attack;grace=graceMs+heldMs;begun=!attack;}
 // The game began the attack of that press.
 bool begin(uint64_t attack){if(!from||!attack||attack!=serial)return false;begun=true;return true;}
 // The game's own time for that attack.
 bool measure(uint64_t attack,uint64_t ms){if(!from||!attack||attack!=serial)return false;lengthMs=std::clamp(ms,minimumMs,maximumMs);begun=true;return true;}
 uint64_t waitMs()const{return uint64_t(double(lengthMs)*double(share)+.5);}
 uint64_t readyAt()const{return from?from+waitMs():0;}
 bool cooling(uint64_t now)const{return from&&now>=from&&now-from<waitMs()&&(begun||now-from<grace);}
 // The press started nothing within its grace time.
 bool lost(uint64_t now)const{return from&&!begun&&now>=from&&now-from>=grace;}
 void clear(){from=0;lengthMs=0;serial=0;grace=0;begun=false;}
};
struct LiveMeleeResult {bool press=false,fired=false,thrust=false,sampled=false;float speed=0,required=0;Vec3 velocity{};
 bool cooling=false;                      // a stroke fast enough to attack met the cooldown on this sample
 Vec3 handVelocity{},otherVelocity{};};   // the hands themselves this frame (the other one only for a two-hand hold)
// Angular velocity vector (rad/s, tracking space) that turns a into b in dt seconds.
inline Vec3 angularVelocity(Quat a,Quat b,float dt){
 auto d=multiply(normalized(b),inverse(a));if(d.w<0)d={-d.x,-d.y,-d.z,-d.w};
 const float s=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
 if(s<1e-7f)return {0,0,0};
 const float angle=2*std::atan2(s,d.w);return Vec3{d.x,d.y,d.z}*(angle/(s*dt));
}
// VHVR hasMomentum. One hand: hand velocity plus rotation at the lever point.
// Two hands: along the direction of both hand velocities summed, the larger share.
// With both hands on the weapon the weapon hand's own stroke counts as well, by the rule for one hand, and the
// heavier weapons ask for no more speed than the others (user after the headset run game-20261013-105615: "two-hand
// weapons probably do not attack while held with both hands - let the right hand's stroke alone attack also when
// both hands hold it". In that run not one of the 73 attacks came from a two-hand hold: 52 with one hand at 3 m/s,
// 21 with one hand on a heavy weapon at 4.5 m/s. Held with two hands, both hands move slowly - the weapon turns
// between them - and VHVR's rule takes the hands' speed, with no share for the turn.) That the heavy weapons' factor
// is left out for the two-hand hold is this mod's choice, not the user's word.
inline LiveMeleeResult meleeMomentum(const LiveMeleeSettings& s,const LiveMeleeWeapon& w,Vec3 leftVelocity,Vec3 rightVelocity,Vec3 rightAngular){
 LiveMeleeResult out;const float axis=length(w.forward);
 if(!finite(leftVelocity)||!finite(rightVelocity)||!finite(rightAngular)||!finite(w.forward)||axis<1e-4f)throw std::invalid_argument("melee momentum input");
 const auto forward=w.forward*(1/axis);
 if(w.twoHanded){
  const auto sum=leftVelocity+rightVelocity;const float size=length(sum);
  if(size>1e-5f){const auto direction=sum*(1/size);out.velocity=direction*std::max(dot(leftVelocity,direction),dot(rightVelocity,direction));}
  const auto own=rightVelocity+cross(rightAngular,forward*s.lever);
  if(length(own)>length(out.velocity))out.velocity=own;
 }else out.velocity=w.fist?rightVelocity:rightVelocity+cross(rightAngular,forward*s.lever);
 out.speed=length(out.velocity);out.required=s.swingSpeed*(w.heavy&&!w.twoHanded?s.heavyFactor:1.f)*w.speedFactor;
 if(out.speed>1e-5f){
  const float along=dot(out.velocity,forward);
  const float angle=std::acos(std::clamp(along/out.speed,-1.f,1.f))*180.f/pi;
  out.thrust=angle<(w.twoHanded?s.thrustDegreesTwoHanded:s.thrustDegrees)&&(w.fist?along>s.jabSpeed:along>=s.thrustSpeed);
 }
 return out;
}
class LiveMelee {
 LiveMeleeSettings settings_{};HandFrame previous_{};bool havePrevious_=false,armed_=false;
 uint64_t binding_=0,pressUntil_=0,lastPress_=0;unsigned presses_=0;LiveMeleeResult last_{};float travel_=0;
 Vec3 lastVelocity_[2]{};bool haveVelocity_=false;
 void lose(){havePrevious_=armed_=haveVelocity_=false;travel_=0;}
public:
 LiveMelee()=default;explicit LiveMelee(LiveMeleeSettings s):settings_(s){}
 unsigned presses()const{return presses_;}bool armed()const{return armed_;}
 // allowed: gameplay screen, no gesture, a reviewed melee weapon shown with this frame's hands.
 // nearTarget: VHVR's contact. A fast stroke away from every target keeps the stroke armed, so that it attacks when the
 // blade reaches a target while still fast; staying at a target, each new stroke (or thrust) attacks again.
 // cooldown: the player's one (shared by both hands' LiveMelee); the caller starts it when a stroke fired. A stroke
 // that meets it stays armed: still fast when the wait ends, it attacks then.
 // upwardFactor: the requirement times this while the weapon hand goes up faster than 1 m/s (2026-10-20, in the air: user,
 // "the rising slash of a jump comes too easily - lifting the sword for a downward blow sets it off"). Such a lift that
 // stays below it keeps the stroke armed: a fast blow down right after it attacks.
 LiveMeleeResult update(const HandFrame& f,const LiveMeleeWeapon& w,bool allowed,uint64_t now,bool nearTarget=true,const LiveCooldown* cooldown=nullptr,bool approaching=false,float upwardFactor=1.f){
  LiveMeleeResult out;
  // "right" is the hand that carries the weapon, "left" the other one (the names of the usual right-hand case).
  if(w.hand>1||!std::isfinite(w.speedFactor)||w.speedFactor<=0||w.speedFactor>2)throw std::invalid_argument("melee weapon hand");
  const unsigned main=w.hand,other=1-w.hand;
  const auto& right=f.hands[main];const auto& left=f.hands[other];
  const bool valid=allowed&&w.binding&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&right.tracked()&&
   finite(right.grip.position)&&(!w.twoHanded||(left.tracked()&&finite(left.grip.position)));
  if(!valid){lose();pressUntil_=0;return out;}
  if(havePrevious_&&f.frame==previous_.frame&&f.sessionGeneration==previous_.sessionGeneration){
   // The frontend runs faster than the hands arrive: keep the press, add no sample.
   out=last_;out.fired=out.sampled=false;out.press=now<pressUntil_;return out;
  }
  const bool continuous=havePrevious_&&binding_==w.binding&&previous_.sessionGeneration==f.sessionGeneration&&previous_.originGeneration==f.originGeneration&&
   previous_.hands[main].profile==right.profile&&previous_.hands[other].profile==left.profile&&f.frame>previous_.frame&&
   f.predictedTime>previous_.predictedTime&&f.predictedTime-previous_.predictedTime<=settings_.maximumGap;
  if(continuous){
   try{
    const float dt=float(f.predictedTime-previous_.predictedTime)*1e-9f;
    const auto rightVelocity=(right.grip.position-previous_.hands[main].grip.position)*(1/dt);
    const auto leftVelocity=w.twoHanded?(left.grip.position-previous_.hands[other].grip.position)*(1/dt):Vec3{};
    const auto angular=angularVelocity(previous_.hands[main].grip.orientation,right.grip.orientation,dt);
    if(length(rightVelocity)>settings_.maximumSpeed||length(leftVelocity)>settings_.maximumSpeed||length(angular)>settings_.maximumAngularSpeed)throw std::invalid_argument("tracking jump");
    if(haveVelocity_&&(length(rightVelocity-lastVelocity_[0])>settings_.maximumSpeedChange||(w.twoHanded&&length(leftVelocity-lastVelocity_[1])>settings_.maximumSpeedChange)))
     throw std::invalid_argument("tracking jump");
    lastVelocity_[0]=rightVelocity;lastVelocity_[1]=leftVelocity;haveVelocity_=true;
    out=meleeMomentum(settings_,w,leftVelocity,rightVelocity,angular);out.sampled=true;out.handVelocity=rightVelocity;out.otherVelocity=leftVelocity;
    if(approaching&&nearTarget&&std::isfinite(settings_.approachFactor)&&settings_.approachFactor>0&&settings_.approachFactor<1)out.required*=settings_.approachFactor;
    if(std::isfinite(upwardFactor)&&upwardFactor>1.f&&upwardFactor<=3.f&&rightVelocity.y>1.f)out.required*=upwardFactor;
    const bool momentum=out.thrust||out.speed>out.required;
    if(!momentum&&out.speed<out.required*settings_.rearmFactor)armed_=true;
    travel_=momentum?travel_+out.speed*dt:0;
    const bool ready=momentum&&armed_&&nearTarget&&travel_>=settings_.strokeTravel&&now>=pressUntil_;
    const bool apart=cooldown?!lastPress_||now-lastPress_>=settings_.pressMs+settings_.releaseMs:!lastPress_||now-lastPress_>=settings_.intervalMs;
    out.cooling=ready&&cooldown&&cooldown->cooling(now);
    if(ready&&apart&&!out.cooling){
     armed_=false;travel_=0;out.fired=true;pressUntil_=now+settings_.pressMs;lastPress_=now;++presses_;
    }
   }catch(const std::exception&){lose();out={};}
  }else{armed_=haveVelocity_=false;travel_=0;} // a new weapon or a tracking gap needs a calm sample first
  previous_=f;binding_=w.binding;havePrevious_=true;
  out.press=now<pressUntil_;last_=out;return out;
 }
};
}
