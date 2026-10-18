#pragma once
#include <evr/live_melee.hpp>
#include <array>
namespace evr::motion {
// Casting the fishing rod by a swing, after VHVR SwingLaunchManager / FishingManager
// (docs/analysis/vhvr_spec_20261010/staff_throw.md 4.2, equipment_tools.md 7.2): pressing the trigger starts the
// preparation and clears the highest speed; while the trigger is held the speed of the rod's tip is measured (one
// hand: the hand's velocity plus its rotation at the tip; the direction is the hand's average movement); as soon as
// the tip has reached 3.5 m/s the rod casts, without waiting for the trigger to be let go (the staff waits, the rod
// does not).
// In the game the trigger is the rod's use button (the HUD: "charge and cast"): holding it charges, letting it go
// casts.
// User decision 2026-10-12 (after trying the description above): the rod casts "at the moment the trigger is let go
// while swinging" - VHVR's rule for the staff (launch on the release, when the tip reached 3.5 m/s at some time of
// the preparation), not its rule for the rod. That is `onRelease` (the default): nothing happens at the swing itself,
// `cast` is reported on the release, and the button simply follows the trigger. With onRelease off the first form
// remains: the swing lets the button go early and `letGo` stays set until the trigger itself is let go.
// A trigger let go without a swing still casts in the game, because letting its button go is the game's own cast.
// Where the cast goes (onRelease). VHVR takes, for a rod in one hand, the hand's average movement of the moment (90
// samples a second). In the headset run game-20261013-005905 (22 samples a second, the release a frame or two late)
// four of six casts went 42..99 degrees to the side of where the player looked (the user: "it flies to the right"):
// the hand's own movement of the last instant is small and belongs to the follow-through, while the rod's tip, 1.9 m
// from the grip, moved at 20..38 m/s. So the direction is the tip's movement (VHVR's rule for a rod in both hands)
// over the latest stroke - the way the tip went from the stroke's start to its end, not its velocity at one moment:
// the tip of a rod that is flicked down from 51 degrees to level moves forwards and down at first and straight down
// at the end (Mock run game024520: the last moment alone had no level part). A stroke is over when the tip has
// slowed to under 0.3 of its peak; what comes after replaces it only if it gets as fast as 0.6 of that peak (the
// forward stroke after the wind-up does, a twitch after the cast does not), or when the old stroke is more than half
// a second past. The cast's strength is that stroke's peak speed.
struct LiveSwingCastSettings {
 float minimumSpeed=3.5f;                    // VHVR MIN_THROW_SPEED, m/s at the tip
 float fullSpeed=5.f;                        // VHVR FULL_THROW_SPEED: tip speed / this is the strength of the cast
 // The strength as this mod takes it: 0 at strengthFrom, 1 at strengthFull, m/s at the tip. With VHVR's "speed / 5"
 // every cast of the headset run game-20261013-105615 was a full one - its eight strokes peaked at 20 .. 45 m/s at
 // the tip, 1.9 m from the grip (10 .. 24 rad/s of the wrist) - and the user: "a slight stroke already flies the
 // full distance". So the scale is the one that was measured: nothing more than the game's own charge up to VHVR's
 // full speed, the full distance at 45 m/s; a slight stroke (20 m/s) is 0.38, which the game lands 8.6 m out.
 float strengthFrom=5.f,strengthFull=45.f;
 // Between the two the share is squared. With the straight line the 19 casts of the headset run game-20261013-141010
 // (tip 6 .. 45 m/s, most of them 22 .. 40) landed 9 .. 16 m out and the user: "there is a difference by the force
 // now, but make the difference larger". Squared, 22 m/s is 0.18 (5.7 m), 30 is 0.39 (8.9 m), 37 is 0.64 (12.6 m),
 // 42 is 0.86 (15.8 m), 45 the full 18 m - a light flick stays near, the far end needs a real stroke.
 float strengthCurve=2.f;
 float strength(float peak)const{
  if(!std::isfinite(peak)||!(strengthFull>strengthFrom)||!(strengthCurve>0))return 0.f;
  return std::pow(std::clamp((peak-strengthFrom)/(strengthFull-strengthFrom),0.f,1.f),strengthCurve);
 }
 XrDuration average=66666667;                // the direction: the hand's movement over about this long
 bool onRelease=true;                        // cast when the trigger is let go after a swing, not at the swing
 float strokeEnd=.3f,strokeNext=.6f;         // shares of a stroke's peak: where it is over, what replaces it
 XrDuration strokeMemory=500000000;          // after this long any new stroke replaces the old one
 XrDuration maximumGap=100000000;float maximumSpeed=15.f,maximumAngularSpeed=60.f;   // tracking jump limits
};
// The charge the game casts with (its fishing state machine's chargeProgression, 0..1). The game fills it over the
// two seconds the button is held and lands the float 3 m + 15 m x charge ahead (hooks/fishing_charge.hpp). VHVR puts
// "the tip's speed / 5 m/s" in the place of the game's draw (BowAndFishingPatches.cs:29-32) - the strength of
// LiveSwingCastResult. The game's own charge stays when it is the higher one: the HUD shows that gauge while the
// trigger is held, and a cast must not fall shorter than the gauge said.
inline float swingCharge(float game,float strength){
 const float own=std::isfinite(game)?std::clamp(game,0.f,1.f):0.f;
 return std::isfinite(strength)?std::max(own,std::clamp(strength,0.f,1.f)):own;
}
inline float castDistance(float charge){return 3.f+15.f*charge;}   // metres, the game's 30fff0
// The rod as shown in the hand: its axis in tracking space (handle to tip) and the metres from the grip to the tip.
struct LiveSwingRod {uint64_t binding=0;Vec3 forward{0,1,0};float tip=0;};
struct LiveSwingCastResult {bool preparing=false,cast=false,letGo=false,sampled=false,released=false;float speed=0,peak=0,strength=0;Vec3 velocity{},direction{};};
class LiveSwingCast {
 struct Sample {XrTime time=0;Vec3 position{};Quat orientation{0,0,0,1};};
 LiveSwingCastSettings settings_{};std::array<Sample,16> ring_{};unsigned count_=0,next_=0;
 bool pressed_=false,preparing_=false,launched_=false;float peak_=0,speed_=0;Vec3 direction_{};
 // The latest stroke of the tip while the trigger is held, and what may replace it.
 struct Stroke {float peak=0;Vec3 direction{},path{};XrTime at=0;
  Vec3 way()const{const float size=length(path);return size>.02f?path*(1/size):direction;}};   // the tip's way; a tiny one: its fastest moment
 Stroke stroke_{},next_stroke_{};bool strokeOver_=false;
 uint64_t frame_=0,session_=0,origin_=0,binding_=0;unsigned casts_=0;
 void lose(){count_=next_=0;pressed_=preparing_=launched_=false;peak_=0;speed_=0;direction_={};stroke_=next_stroke_={};strokeOver_=false;frame_=0;}
 void strokeSample(float speed,Vec3 velocity,XrTime time,float dt){
  if(!std::isfinite(speed))return;
  const bool moving=speed>1e-3f;const Vec3 direction=moving?velocity*(1/speed):Vec3{};
  if(!strokeOver_){
   if(stroke_.peak>0&&speed<stroke_.peak*settings_.strokeEnd){strokeOver_=true;next_stroke_={};return;}
   stroke_.path=stroke_.path+velocity*dt;
   if(moving&&speed>=stroke_.peak){stroke_.peak=speed;stroke_.direction=direction;stroke_.at=time;}
   return;}
  next_stroke_.path=next_stroke_.path+velocity*dt;
  if(moving&&speed>next_stroke_.peak){next_stroke_.peak=speed;next_stroke_.direction=direction;next_stroke_.at=time;}
  if(next_stroke_.peak>0&&(next_stroke_.peak>=stroke_.peak*settings_.strokeNext||(next_stroke_.peak>=settings_.minimumSpeed&&next_stroke_.at-stroke_.at>settings_.strokeMemory))){
   stroke_=next_stroke_;next_stroke_={};strokeOver_=false;}
 }
 const Sample& back(unsigned n)const{return ring_[(next_+ring_.size()-1-n)%ring_.size()];}   // n=0: the newest
public:
 LiveSwingCast()=default;explicit LiveSwingCast(LiveSwingCastSettings s):settings_(s){}
 unsigned casts()const{return casts_;}bool preparing()const{return preparing_;}
 // allowed: gameplay screen, the rod selected and shown with these hands, no pointer of a menu.
 LiveSwingCastResult update(const HandFrame& f,const LiveSwingRod& rod,bool allowed){
  LiveSwingCastResult out;const auto& h=f.hands[1];const float axis=length(rod.forward);
  const bool valid=allowed&&rod.binding&&finite(rod.forward)&&axis>1e-4f&&std::isfinite(rod.tip)&&rod.tip>.1f&&rod.tip<10&&
   f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&h.tracked()&&finite(h.grip.position)&&
   h.triggerActive&&std::isfinite(h.trigger);
  if(!valid){lose();return out;}
  if(frame_&&(f.sessionGeneration!=session_||f.originGeneration!=origin_||rod.binding!=binding_))lose();
  if(frame_==f.frame){out.preparing=preparing_;out.letGo=launched_;out.peak=peak_;out.direction=direction_;return out;}   // the frontend runs faster than the hands arrive
  if(count_&&(f.predictedTime<=back(0).time||f.predictedTime-back(0).time>settings_.maximumGap))count_=next_=0;   // a gap: no speed across it
  frame_=f.frame;session_=f.sessionGeneration;origin_=f.originGeneration;binding_=rod.binding;
  const float t=std::clamp(h.trigger,0.f,1.f);const bool before=pressed_;
  if(t>=.8f)pressed_=true;else if(t<=.2f)pressed_=false;
  if(pressed_&&!before){preparing_=true;launched_=false;peak_=0;direction_={};speed_=0;stroke_=next_stroke_={};strokeOver_=false;}
  const bool released=!pressed_&&before&&preparing_;
  if(!pressed_){preparing_=launched_=false;}
  if(count_){
   const auto& last=back(0);const float dt=float(f.predictedTime-last.time)*1e-9f;
   const auto hand=(h.grip.position-last.position)*(1/dt);const auto angular=angularVelocity(last.orientation,h.grip.orientation,dt);
   if(finite(hand)&&finite(angular)&&length(hand)<=settings_.maximumSpeed&&length(angular)<=settings_.maximumAngularSpeed){
    out.velocity=hand+cross(angular,rod.forward*(rod.tip/axis));out.speed=length(out.velocity);out.sampled=true;
    // The frame of the release belongs to the swing as well. The trigger comes up during the forward flick, and at
    // 22..30 hand frames a second that flick may be this one frame: headset run 20261014-124515, the first cast
    // went at 40.9 m/s on the release frame while the wind-up before it (32.7 m/s, backwards) was taken for the
    // stroke, so the float went where the head looked (user: "the rod does not cast where I swing it").
    if(preparing_||released){speed_=out.speed;peak_=std::max(peak_,out.speed);strokeSample(out.speed,out.velocity,f.predictedTime,dt);}
    if(preparing_){
     // VHVR: the direction is the hand's average velocity (felt more natural than the tip's own movement). A flick of
     // the wrist alone hardly moves the hand; then the tip's movement stands in.
     const Sample* from=&last;for(unsigned n=1;n<count_;++n){const auto& s=back(n);if(f.predictedTime-s.time>settings_.average)break;from=&s;}
     const auto mean=(h.grip.position-from->position)*(1e9f/float(f.predictedTime-from->time));
     const auto along=length(mean)>.25f?mean:out.velocity;const float size=length(along);
     if(size>1e-4f)direction_=along*(1/size);
    }
   }else{count_=next_=0;}   // a tracking jump is not a swing
  }
  ring_[next_]={f.predictedTime,h.grip.position,h.grip.orientation};next_=(next_+1)%unsigned(ring_.size());count_=std::min<unsigned>(count_+1,unsigned(ring_.size()));
  if(settings_.onRelease){
   // VHVR's staff: the release launches when the tip was fast enough at some time while the trigger was held.
   // The latest stroke decides: fast enough, it is the cast - along the tip's movement at its fastest moment.
   if(released&&stroke_.peak>=settings_.minimumSpeed){out.cast=true;out.strength=settings_.strength(stroke_.peak);direction_=stroke_.way();++casts_;}
  }else if(preparing_&&!launched_&&peak_>=settings_.minimumSpeed){
   launched_=true;out.cast=true;out.strength=settings_.strength(out.speed);++casts_;}
  out.preparing=preparing_;out.letGo=launched_;out.peak=peak_;out.direction=direction_;out.released=released;return out;
 }
};
}
