#pragma once
#include <evr/head_gesture_combat.hpp>
#include <evr/combat_worlds.hpp>
namespace evr::game {
// Bounded integration diagnostic: one or two physical gestures -> separate native XInput pulses.
// This is not a hit/contact replacement or a production attack/cooldown adapter.
struct MotionCombatObservation {
 uint32_t entity=0,item=0,usedItem=0,sequence=0;uint64_t binding=0;
 int64_t triggerTime=0,playedTime=0,currentTime=0,length=0;
 bool valid=false,active=false,alive=false,ready=false;
 uint64_t world=0,nativeMs=0;uint16_t trigger=0;bool canPrimary=false;
 motion::CombatWorlds worlds{};
};
class MotionAttackFixture {
public:
 enum class Phase {WaitConnection,Settle,WaitSwing,Press,Observe,Complete,Aborted};
 struct Result {Phase phase=Phase::WaitConnection;uint8_t pulseLevel=0;uint64_t request=0;
   motion::Intent gesture{};bool accepted=false;unsigned completed=0;double remaining=0,total=0;};
 explicit MotionAttackFixture(motion::GestureConfig c):gate_(c){}
 void setAttackLimit(unsigned count){if(started_||count<1||count>2)throw std::invalid_argument("bounded attack limit");limit_=count;}
 unsigned attackLimit()const{return limit_;}
 void requireHeadGesture(){if(started_)throw std::invalid_argument("gesture policy must precede input");gestureRequired_=true;}
 // Explicit measured two-hand diagnostic. Default historical fixtures remain
 // one-hand backend probes; this path binds geometry to the expected item.
 void setTwoHandWeapon(motion::Weapon w,uint32_t item){
  if(started_||!item||w.mainHand!=1||!w.twoHanded||w.kind!=motion::WeaponKind::Melee||
     !finite(w.gripToModel.position)||!finite(w.offHandGripInModel)||
     !std::isfinite(w.offHandTolerance)||w.offHandTolerance<=0)
   throw std::invalid_argument("measured two-hand melee calibration required");
  w.gripToModel.orientation=normalized(w.gripToModel.orientation);
  const auto separation=length(w.gripToModel.position+rotate(w.gripToModel.orientation,w.offHandGripInModel));
  if(!std::isfinite(separation)||separation<1e-4f||w.offHandTolerance>=separation*.5f)
   throw std::invalid_argument("two-hand support radius must be smaller than the measured grip spacing");
  w.binding=0;
  weapon_=w;weaponItem_=item;
 }

 Result update(uint64_t now,bool connected,bool normalUi,const motion::HandFrame& hands,uint64_t handMs,const MotionCombatObservation& s,const motion::HeadGestureResult* gesture=nullptr){
  Result out;out.phase=phase_;out.request=request_;out.accepted=accepted_;out.completed=completed_;
  if(phase_==Phase::Complete||phase_==Phase::Aborted)return out;
  auto abort=[&](){phase_=Phase::Aborted;out.phase=phase_;out.pulseLevel=0;out.gesture={};if(gate_.pendingRequest())gate_.acknowledge(gate_.pendingRequest());return out;};
  if(!started_){started_=true;begin_=last_=at_=now;}
  if(now<last_||now-begin_>30000||!normalUi||!s.valid||!s.alive||!s.ready||!s.entity||!s.item||!s.binding||
     !handMs||now<handMs||now-handMs>200||!hands.focused||!hands.hands[1].tracked()||!hands.hands[1].grabActive||!hands.hands[1].grab)return abort();
  if(weaponItem_&&s.item!=weaponItem_)return abort();
  // Before a request, missing support merely blocks a swing. After a request,
  // revoke the diagnostic contact ticket and release input; native action/cost
  // and root movement remain owned by the game. Check even duplicate frames.
  if(weaponItem_&&request_&&!supported(hands,s.binding))return abort();
  last_=now;
  if(phase_!=Phase::WaitConnection&&(!connected||s.entity!=origin_.entity||s.item!=origin_.item||s.binding!=origin_.binding))return abort();
  if(phase_==Phase::WaitConnection){if(!connected)return out;if(s.active)return abort();origin_=s;next(Phase::Settle,now);}
  if(phase_==Phase::Settle){if(s.active)return abort();if(now-at_>=500)next(Phase::WaitSwing,now);}
  if(s.active){
   if(!request_||s.usedItem!=origin_.item||!s.sequence||s.triggerTime==origin_.triggerTime||s.length<=0||s.playedTime<0||s.currentTime<0||s.playedTime>s.currentTime||s.currentTime>s.length)return abort();
   if(!accepted_){if(!gate_.acknowledge(request_))return abort();accepted_=true;acceptedSequence_=s.sequence;acceptedTime_=s.triggerTime;}
   if(s.sequence!=acceptedSequence_||s.triggerTime!=acceptedTime_)return abort();
   lastActive_=now;out.remaining=double(s.length-s.currentTime);out.total=double(s.length);
  }
  // Frontend callbacks may reuse one renderer sample. It is never differentiated
  // twice, while UI/tracking/identity/freshness guards above run on every call.
  if(hands.frame!=lastFrame_||hands.sessionGeneration!=lastSession_||
     (gestureRequired_&&(!gesture||motion::gestureInhibitsCombat(hands,*gesture)))){
   lastFrame_=hands.frame;lastSession_=hands.sessionGeneration;
   motion::NativeCombat n;n.enabled=n.known=n.local=n.alive=true;n.uiOrEvent=false;
   n.canStart=phase_==Phase::WaitSwing&&!s.active;n.recoveryRemaining=out.remaining;n.recoveryTotal=out.total;
   motion::Weapon w=weapon_;w.binding=s.binding;
   const motion::HeadGestureResult missing{};
   out.gesture=gestureRequired_?motion::gestureCombat(gate_,hands,w,n,gesture?*gesture:missing):gate_.update(hands,w,n);
   if(request_&&(out.gesture.cancelledRequest||out.gesture.blocked==motion::Block::Discontinuity||out.gesture.blocked==motion::Block::Invalid||(gestureRequired_&&out.gesture.blocked==motion::Block::UiOrEvent)))return abort();
   if(out.gesture.request){if(phase_!=Phase::WaitSwing||request_)return abort();request_=out.gesture.request;next(Phase::Press,now);}
  }
  if(phase_==Phase::Press&&now-at_>=120)next(Phase::Observe,now);
  if(phase_==Phase::Observe&&accepted_&&!s.active&&now>=lastActive_&&now-lastActive_>=250){
   ++completed_;
   if(completed_==limit_)next(Phase::Complete,now);
   else{origin_=s;request_=0;accepted_=false;acceptedSequence_=0;acceptedTime_=0;next(Phase::WaitSwing,now);}
  }
  out.phase=phase_;out.request=request_;out.accepted=accepted_;out.completed=completed_;out.pulseLevel=phase_==Phase::Press?255:0;return out;
 }
 Phase phase()const{return phase_;}
private:
 motion::IntentGate gate_;Phase phase_=Phase::WaitConnection;bool started_=false,accepted_=false,gestureRequired_=false;
 uint64_t begin_=0,last_=0,at_=0,lastActive_=0,lastFrame_=0,lastSession_=0,request_=0;
 unsigned limit_=1,completed_=0;uint32_t acceptedSequence_=0;int64_t acceptedTime_=0;MotionCombatObservation origin_{};
 motion::Weapon weapon_{};uint32_t weaponItem_=0;
 bool supported(const motion::HandFrame& f,uint64_t binding)const{
  auto w=weapon_;w.binding=binding;return motion::twoHandSupportValid(f,w);
 }
 void next(Phase p,uint64_t now){phase_=p;at_=now;}
};
}
