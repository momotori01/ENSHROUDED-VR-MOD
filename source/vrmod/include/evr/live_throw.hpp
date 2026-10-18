#pragma once
#include <evr/motion_intent.hpp>
#include <algorithm>
#include <array>
namespace evr::motion {
// Throwing for live play, after VHVR ThrowableManager (docs/analysis/vhvr_spec_20261010/staff_throw.md, section 5):
// hold the trigger to wind up, let it go while the hand moves, and the item leaves the hand along the hand's
// movement ("Classic" direction: the hand's average velocity). VHVR also wants the grip held; here the trigger alone
// winds up, as with the bow (the grip belongs to the wheel). There is no speed threshold: the throw needs the hand to
// have travelled more than 6.25 cm within the last 0.4 s of the wind-up. The game throws at its own speed; origin and
// direction are handed to the projectile's spawn (hooks/bow_spawn_observer.hpp).
// Which movement: VHVR takes the hand's velocity at the release, sampled 90 times a second. In the headset run
// game-20261013-005905 the hands arrived 22 times a second and the trigger counted as let go a frame or two after
// the fingers opened: all three throws were measured in the follow-through - the hand already low and going down
// (directions (0, -0.95, -0.30), (-0.32, -0.86, -0.39), 0.5..2.7 m/s) - and the bombs fell at the player's feet
// (the user: "they often drop at my feet"). So the release looks back for the throw itself: from the newest
// movement backwards, taking the fastest, until the hand was all but still (under 0.3 of the fastest so far: the
// pause or the turn before the throw - a wind-up beyond it does not count), and throws along the movement there.
// Where the throw turns into the follow-through the averaged movement is slow for a moment (Mock run game023439:
// 1.0 m/s between 1.35 and 3.0), which is why the search does not stop at the first slower sample. A release in the
// middle of the throw finds its fastest moment at once and is what it was. The trigger also counts as let go sooner
// (half way instead of nearly open).
struct LiveThrowSettings {
 float minimumTravel=.0625f;                 // VHVR: the longest displacement of the hand has to exceed this
 XrDuration history=400000000;               // VHVR: at most 0.4 s back
 XrDuration average=66666667;                // the direction: the hand's movement over about this long
 float releaseAt=.5f;                        // the trigger counts as let go below this (it counts as pulled from 0.8)
 float peakSlack=.3f;                        // looking back for the fastest moment ends where the hand had this share of it or less
 uint64_t pressMs=120;                       // native attack tap
 XrDuration maximumGap=100000000;float maximumSpeed=15.f;
};
struct LiveThrowResult {bool aiming=false,thrown=false,press=false;Vec3 origin{},direction{};float speed=0,travel=0;};
class LiveThrow {
 struct Sample {XrTime time=0;Vec3 position{};};
 LiveThrowSettings settings_{};std::array<Sample,64> ring_{};unsigned count_=0,next_=0;
 bool pressed_=false,aiming_=false;XrTime aimStart_=0;uint64_t frame_=0,session_=0,origin_=0,pressUntil_=0;unsigned throws_=0;
 void lose(){count_=next_=0;pressed_=aiming_=false;frame_=0;}
 const Sample& back(unsigned n)const{return ring_[(next_+ring_.size()-1-n)%ring_.size()];}   // n=0: the newest
public:
 LiveThrow()=default;explicit LiveThrow(LiveThrowSettings s):settings_(s){}
 unsigned throws()const{return throws_;}bool aiming()const{return aiming_;}
 // allowed: gameplay screen, a throwable selected, the right hand free of other input.
 LiveThrowResult update(const HandFrame& f,bool allowed,uint64_t now){
  LiveThrowResult out;const auto& h=f.hands[1];
  const bool valid=allowed&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&h.tracked()&&finite(h.grip.position)&&
   h.triggerActive&&std::isfinite(h.trigger);
  if(!valid){lose();out.press=now<pressUntil_;return out;}
  if(frame_&&(f.sessionGeneration!=session_||f.originGeneration!=origin_))lose();
  if(frame_==f.frame){out.aiming=aiming_;out.press=now<pressUntil_;return out;}   // the frontend runs faster than the hands arrive
  if(count_&&(f.predictedTime<=back(0).time||f.predictedTime-back(0).time>settings_.maximumGap)){count_=next_=0;aiming_=false;}
  frame_=f.frame;session_=f.sessionGeneration;origin_=f.originGeneration;
  ring_[next_]={f.predictedTime,h.grip.position};next_=(next_+1)%unsigned(ring_.size());count_=std::min<unsigned>(count_+1,unsigned(ring_.size()));
  const float t=std::clamp(h.trigger,0.f,1.f);
  if(t>=.8f)pressed_=true;else if(t<=settings_.releaseAt)pressed_=false;
  if(pressed_&&!aiming_){aiming_=true;aimStart_=f.predictedTime;}
  else if(!pressed_&&aiming_){
   aiming_=false;
   const auto window=std::min<XrDuration>(settings_.history,f.predictedTime-aimStart_);
   const auto& last=back(0);float travel=0;
   for(unsigned n=1;n<count_;++n){const auto& s=back(n);if(last.time-s.time>window)break;
    travel=std::max(travel,length(last.position-s.position));}
   out.travel=travel;
   // The hand's movement up to sample n: from the oldest sample within `average` before it (or the one before it).
   const auto movement=[&](unsigned n,Vec3& velocity){
    const auto& to=back(n);const Sample* from=nullptr;
    for(unsigned m=n+1;m<count_;++m){const auto& s=back(m);if(last.time-s.time>window)break;
     if(to.time-s.time<=settings_.average||!from)from=&s;else break;}
    if(!from||to.time<=from->time)return false;
    velocity=(to.position-from->position)*(1e9f/float(to.time-from->time));return finite(velocity);};
   Vec3 best{};float bestSpeed=0;bool any=false;
   for(unsigned n=0;n+1<count_;++n){
    Vec3 v;if(last.time-back(n).time>window||!movement(n,v))break;
    const float s=length(v);
    if(!any||s>bestSpeed){best=v;bestSpeed=s;any=true;}       // the hand was faster before: the release came in the follow-through
    else if(s<=bestSpeed*settings_.peakSlack)break;           // all but still: the throw began after this
   }
   if(travel>settings_.minimumTravel&&any){
    const float speed=bestSpeed;
    if(std::isfinite(speed)&&speed>1e-3f&&speed<=settings_.maximumSpeed){
     out.thrown=true;out.origin=last.position;out.direction=best*(1/speed);out.speed=speed;pressUntil_=now+settings_.pressMs;++throws_;}
   }
  }
  out.aiming=aiming_;out.press=now<pressUntil_;return out;
 }
};
// The guide of a throw (user, 2026-10-18, after the headset run: "throwing as in Contractors and other games: with a
// throwable, the right trigger gets the throw ready; the left hand held forward shows a guide line of where it goes,
// decided by the left hand's direction; letting the trigger go while swinging the right hand throws it along the guide.
// The guide can be short - a curve if that is possible, otherwise a straight line of about 30 cm that shows 'it goes
// this way'"). VHVR's nearest way is SecondHandAiming (staff_throw.md 5.3: the other hand's position minus the head's);
// here the direction is the left hand's own pointing (its aim pose's -Z), as the user said.
// While the throw is being got ready (the right trigger held): the left hand counts as held forward when it is at
// least `ahead` in front of the headset, along where the head looks (level), and no more than `below` under it (a hand
// held at ease in front of the waist is 20-30 cm ahead of the eyes too; an arm held out is at the chest or higher, about
// 50-65 cm ahead). Then the guide shows from the hand along
// its pointing, and that direction is kept: a throw pulls the left arm back as the right one swings (as with a real
// throw), so the release takes the last direction the forward hand gave in this wind-up, if it is no older than
// `keep`. Without one the swing decides, as before (LiveThrow).
// The line is straight: the game launches the item at its own speed, not known here, and over 30 cm a thrown item's
// path is straight to the eye anyway. Drawn as a short red beam between its two ends (layer/native_xr.hpp; user 12:40:
// "a short laser rather than a dotted line" - it was 9 dots).
struct ThrowGuideSettings {float ahead=.3f,below=.45f;float start=.04f,length=.32f;unsigned dots=2;XrDuration keep=1500000000;};
struct ThrowGuideResult {bool shown=false,kept=false;Vec3 from{},direction{};};
class ThrowGuide {
 Vec3 direction_{};XrTime at_=0;bool kept_=false;
public:
 ThrowGuideSettings settings{};
 void reset(){kept_=false;at_=0;}
 // aiming: the right trigger holds a wind-up (LiveThrow::aiming before this frame's update); head: the headset in the
 // same space as the hands.
 ThrowGuideResult update(const HandFrame& f,const Pose& head,bool headTracked,bool aiming){
  ThrowGuideResult out;
  if(!aiming||!f.focused){reset();return out;}
  const auto& l=f.hands[0];
  if(headTracked&&l.aimActive&&(l.aimFlags&15)==15&&finite(l.aim.position)&&finite(head.position)){
   Vec3 look=rotate(head.orientation,{0,0,-1});look.y=0;const float n=length(look);
   if(n>.2f){
    look=look*(1/n);const Vec3 rel=l.aim.position-head.position;
    if(rel.x*look.x+rel.z*look.z>=settings.ahead&&rel.y>=-settings.below){
     Vec3 d=rotate(l.aim.orientation,{0,0,-1});const float m=length(d);
     if(std::isfinite(m)&&m>.5f){direction_=d*(1/m);at_=f.predictedTime;kept_=true;out.shown=true;out.from=l.aim.position;}
    }
   }
  }
  if(kept_&&(f.predictedTime<at_||f.predictedTime-at_>settings.keep))kept_=false;
  out.kept=kept_;out.direction=direction_;return out;
 }
 // The dots of the line in the tracking space: from `start` to `start + length` along it.
 unsigned dots(const ThrowGuideResult& g,Vec3* out,unsigned capacity)const{
  if(!g.shown||!capacity||settings.dots<2)return 0;const unsigned count=std::min(capacity,settings.dots);
  for(unsigned i=0;i<count;++i)out[i]=g.from+g.direction*(settings.start+settings.length*float(i)/float(count-1));
  return count;
 }
};
}
