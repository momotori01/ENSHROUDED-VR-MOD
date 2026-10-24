#pragma once
#include <evr/bow_physical_trial.hpp>
#include <evr/motion_intent.hpp>
namespace evr::motion {
// VHVR BowLocalManager/BowManager behaviour mapped onto Enshrouded's native bow
// input; see docs/analysis/vhvr_spec_20261010/bow.md. Holding the string is native
// LT (aim/charge), a valid release is one RT tap, anything else lets LT go.
// Repeatable. Native charge time, ammunition and projectile rules stay native.
constexpr float bowFullDrawLength=.60f;
struct BowDrawSettings {
 float attachRange=BowPhysicalTrial::attachRange; // VHVR attachRange
 float fullDrawLength=bowFullDrawLength;          // VHVR BowFullDrawLength, from the handle
 // Enshrouded guards, not VHVR: native needs its aim state before the tap, and
 // power is still native time based, so a token pull must not shoot.
 float minimumPull=.10f;uint64_t minimumHoldMs=100;
 // The frontend clock steps by about 16 ms, so a tap lasts up to one step less than asked:
 // 60 ms came out as 49 ms once (combined run game034329). 80 ms keeps the tap above 60 ms,
 // two input polls of the game even at the headset's 35 frames per second.
 uint64_t tapMs=80,aimMs=500,unboundMs=300;
 // Not VHVR (Valheim has no such wait): the game's bow starts drawing about 0.35 s after its LT (its nocking; logs of
 // 10-17..10-21: 0.31 .. 0.48 s alone, 0.35 .. 0.45 s as a guest - user 2026-10-21: "the start of the draw was slow").
 // With the trigger held and the hand within preAimRange of the string, LT goes to the game already, so that its
 // nocking runs while the hand reaches the string. Let go without taking the string: LT goes, nothing is shot. 0: off.
 float preAimRange=.45f;
 // The strong shot (2026-10-21, the user: "draw with the grip and the trigger held and let go: the strong shot"; 15:20
 // after the headset: "it does not come out - the grip alone is enough"). The game takes RT held past its threshold and
 // let go as the bow's special ability (SpecialAbility_Hold on the release: Mock game-20261021-134251, held 600 ms; 300
 // ms was a tap), but not while the bow is aimed (LT): with LT held through it none of 17 strong releases of headset run
 // 20261021-150238 began the strong shot (its action 1963645007 draws by itself and shoots at 400 ms; the gamepad's way
 // is RT held without aiming - guess). So a string taken or held with the right grip is the strong shot's: RT is held
 // (no LT) for as long as the string is, and let go with it. false: the grip takes no string.
 bool strongByGrip=true;
};
// VHVR realLifePullPercentage: squared share of the travel behind the rest string.
inline float bowPullPercentage(float pull,float brace,float fullDrawLength){
 if(!std::isfinite(pull)||!std::isfinite(brace)||!std::isfinite(fullDrawLength)||brace<0)throw std::invalid_argument("bow pull percentage");
 const float t=std::clamp(pull/(std::max(fullDrawLength,brace+.1f)-brace),0.f,1.f);return t*t;
}
// VHVR BowDrawRestrictType "Full" (its default): the string travels only as far
// as the game's own time-based draw has progressed, Lerp(brace, full, sqrt(progress)).
// nativeProgress is the native bow's own string displacement, 0 at rest and 1 at full draw.
inline float bowPullLimit(float nativeProgress,float brace,float fullDrawLength){
 if(!std::isfinite(nativeProgress)||!std::isfinite(brace)||!std::isfinite(fullDrawLength)||brace<0)throw std::invalid_argument("bow pull limit");
 return (std::max(fullDrawLength,brace+.1f)-brace)*std::max(std::sqrt(std::clamp(nativeProgress,0.f,1.f)),.01f);
}
// VHVR rotateBowOnPulling: while the string is held the bow looks along the line
// between both hands. The bow hand keeps the handle position and only its roll
// about that line. Model axes in tracking space: +Y limb, +Z towards the string.
inline Pose bowDrawnPose(Pose rest,Vec3 drawingHand,float restElevation){
 if(!finite(rest.position)||!finite(drawingHand)||!std::isfinite(restElevation))throw std::invalid_argument("bow drawn pose");
 const auto toHand=drawingHand-rest.position;const float distance=length(toHand);
 if(!std::isfinite(distance)||distance<.05f||std::abs(restElevation)>=distance)return rest;
 const auto back=toHand*(1/distance);
 auto q=normalized(multiply(alignDirections(rotate(rest.orientation,{0,0,1}),back),rest.orientation));
 const auto hint=rotate(rest.orientation,{0,1,0});auto up=hint-back*dot(hint,back);
 if(length(up)>.05f){
  const auto current=rotate(q,{0,1,0});
  // Both are perpendicular to the draw line; skip the ambiguous half turn.
  if(dot(current,up)>-.99f*length(up))q=normalized(multiply(alignDirections(current,up),q));
 }
 // The nock sits restElevation above the handle, so tilt until the hand is on it.
 const float tilt=std::asin(restElevation/distance);
 return {normalized(multiply(q,Quat{std::sin(tilt/2),0,0,std::cos(tilt/2)})),rest.position};
}
class BowDraw {
public: enum Phase:unsigned {Idle=0,Blocked=1,Held=2,Cancelled=3,Aborted=4,Released=5};
private:
 BowDrawSettings settings_{};Phase phase_=Blocked;BowPhysicalMetrics metrics_{};
 bool pressed_=false,bowMode_=false,fullDrawn_=false,strong_=false,byGrip_=false,gripAsTrigger_=false;float releasePull_=0;unsigned shots_=0;
 uint64_t binding_=0,session_=0,origin_=0,heldMs_=0,releaseMs_=0,lastMs_=0,boundMs_=0;
 XrTime lastTime_=0;std::array<XrPath,2> profiles_{};
 static bool scope(const HandFrame& f,const BowPhysicalGeometry& g,bool allowed){
  const auto& h=f.hands[1];const auto q=g.model.orientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  return allowed&&g.binding&&g.frame==f.frame&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&
   f.hands[0].tracked()&&h.tracked()&&h.triggerActive&&std::isfinite(h.trigger)&&h.trigger>=0&&h.trigger<=1&&
   finite(h.grip.position)&&finite(g.model.position)&&std::isfinite(norm)&&std::abs(norm-1)<.02f&&finite(g.scale)&&
   g.scale.x>.5f&&g.scale.y>.5f&&g.scale.z>.5f&&g.scale.x<1.5f&&g.scale.y<1.5f&&g.scale.z<1.5f;
 }
public:
 BowDraw()=default;explicit BowDraw(BowDrawSettings s):settings_(s){}
 void preAim(float range){settings_.preAimRange=range;}
 // The string taken with the grip draws as with the trigger (LT, a normal shot), not the strong shot's RT: for the mod's
 // bow (holster.hpp modBow) the game's hand is bare until the LT brings its bow - RT alone would be the fists' attack.
 void gripAsTrigger(bool on){gripAsTrigger_=on;}
 Phase phase()const{return phase_;}const BowPhysicalMetrics& metrics()const{return metrics_;}
 float releasePull()const{return releasePull_;}unsigned shots()const{return shots_;}
 // The last release was the strong shot's (the string held with the right grip).
 bool strongRelease()const{return strong_;}
 // The string is held with the right grip now (the strong shot's draw: RT, not LT).
 bool strongDraw()const{return phase_==Held&&byGrip_&&!gripAsTrigger_;}
 bool bowMode()const{return bowMode_;}
 // A refused shot or a screen that owns the triggers: let go, then require a new press.
 void abort(){if(phase_==Held||phase_==Released)phase_=Aborted;else if(phase_!=Aborted)phase_=Blocked;}
 LivePadState update(LivePadState pad,const HandFrame& f,const BowPhysicalGeometry& g,bool allowed,uint64_t now){
  metrics_={};
  const auto& right=f.hands[1];
  const float t=right.triggerActive&&std::isfinite(right.trigger)?std::clamp(right.trigger,0.f,1.f):0;
  if(t>=.8f)pressed_=true;else if(t<=.2f)pressed_=false;
  const bool grip=settings_.strongByGrip&&right.grabActive&&right.grab;
  if(phase_==Cancelled)phase_=Idle;else if(phase_==Aborted)phase_=Blocked; // reported for one update
  if(!g.binding){
   // No reviewed bow this frame. Keep the triggers until the draw input is
   // released, so a held string never becomes another native trigger action.
   if(phase_==Held||phase_==Released)phase_=Aborted;else if(phase_!=Aborted)phase_=Blocked;
   if(bowMode_&&!pressed_&&now>=boundMs_&&now-boundMs_>=settings_.unboundMs)bowMode_=false;
   if(!bowMode_)return pad;
   pad.leftTrigger=pad.rightTrigger=0;pad.buttons&=~LivePadMapper::X;return pad;
  }
  bowMode_=true;boundMs_=now;
  pad.leftTrigger=pad.rightTrigger=0;pad.buttons&=~LivePadMapper::X;
  const bool valid=scope(f,g,allowed);
  const bool changed=binding_!=g.binding||session_!=f.sessionGeneration||origin_!=f.originGeneration||
   profiles_[0]!=f.hands[0].profile||profiles_[1]!=right.profile;
  if(phase_==Released){
   if(!valid||changed||now<lastMs_||f.predictedTime<lastTime_){phase_=Aborted;return pad;}
   lastMs_=now;lastTime_=f.predictedTime;
   const auto age=now-releaseMs_;
   if(age>=settings_.aimMs){phase_=Idle;return pad;}
   if(strong_)return pad;                                   // RT was let go with the string: the game takes its hold
   pad.leftTrigger=255;if(age<settings_.tapMs)pad.rightTrigger=255;return pad;
  }
  if(phase_==Held&&(!valid||changed||f.predictedTime<lastTime_)){phase_=Aborted;return pad;}
  if(!valid){if(phase_!=Aborted)phase_=Blocked;return pad;}
  if(changed)phase_=Blocked;
  binding_=g.binding;session_=f.sessionGeneration;origin_=f.originGeneration;
  profiles_={f.hands[0].profile,right.profile};lastTime_=f.predictedTime;
  const Vec3 nock{BowPhysicalTrial::restString.x*g.scale.x,BowPhysicalTrial::restString.y*g.scale.y,BowPhysicalTrial::restString.z*g.scale.z};
  const auto delta=rotate(inverse(g.model.orientation),right.grip.position-g.model.position)-nock;
  // The drawn bow turns onto the line between both hands, so the pull is the
  // handle-to-hand distance behind the rest string, whichever way the model faces
  // this frame. The string itself is only located for taking it.
  const float reach=length(right.grip.position-g.model.position);
  const float pull=std::sqrt(std::max(0.f,reach*reach-nock.y*nock.y))-nock.z;
  metrics_={true,length(delta)<=settings_.attachRange,length(delta),std::max(0.f,pull),0,g.model.position+rotate(g.model.orientation,nock)};
  if(phase_==Blocked){if(!pressed_&&!grip)phase_=Idle;}
  // VHVR checks the string every frame while the input is held, so a trigger
  // pressed on the way to the string still takes it.
  else if(phase_==Idle){if((pressed_||grip)&&metrics_.withinRange){phase_=Held;heldMs_=now;fullDrawn_=false;byGrip_=grip;}}
  else if(phase_==Held){
   if(grip)byGrip_=true;                                    // the grip closed during a trigger's draw: the strong shot's from here
   if(metrics_.distance>1.5f){phase_=Aborted;return pad;}
   if(!pressed_&&!grip){
    if(now>=heldMs_&&now-heldMs_>=settings_.minimumHoldMs&&metrics_.pull>=settings_.minimumPull){
     phase_=Released;releaseMs_=lastMs_=now;releasePull_=metrics_.pull;++shots_;
     strong_=byGrip_&&!gripAsTrigger_;
     if(!strong_)pad.leftTrigger=pad.rightTrigger=255;
    }else phase_=Cancelled;
   }
  }
  if(phase_==Held){if(byGrip_&&!gripAsTrigger_)pad.rightTrigger=255;else pad.leftTrigger=255;}
  else if(phase_==Idle&&(pressed_||(gripAsTrigger_&&grip))&&settings_.preAimRange>0&&metrics_.distance<=settings_.preAimRange)pad.leftTrigger=255;
  return pad;
 }
 float pullPercentage(const BowPhysicalGeometry& g)const{
  return metrics_.valid?bowPullPercentage(metrics_.pull,BowPhysicalTrial::restString.z*g.scale.z,settings_.fullDrawLength):0;
 }
 // VHVR pulses the bow hand once when the draw first becomes full. True once per hold.
 bool takeFullDraw(const BowPhysicalGeometry& g){
  if(phase_!=Held||fullDrawn_||pullPercentage(g)<1)return false;
  fullDrawn_=true;return true;
 }
};
}
