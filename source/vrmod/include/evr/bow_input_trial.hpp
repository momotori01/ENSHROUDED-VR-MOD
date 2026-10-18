#pragma once
#include <evr/live_pad.hpp>
namespace evr::motion {
// Mock-only one-shot diagnostic. Native PresetA X is MainhandAction; RT is
// SpecialAbility. This does not implement physical draw/aim or safe shot cancel.
// Survey game081611: native bow charge keeps screen 1/nativeResult=false,
// but changes the auxiliary HUD from 0 to 3. Only this explicit known-bow
// diagnostic accepts 3; full-screen UI and native blocking still reject.
inline bool bowTrialHud(bool valid,bool normal,uint8_t screen,uint8_t auxiliary,bool blocked){
 return valid&&!blocked&&screen==1&&(normal||auxiliary==3);
}
class BowInputTrial {
public: enum Phase {Waiting,Armed,Held,Released,Aborted};
private:
 bool aimed_=false,releaseFire_=false,cancelOnly_=false;XrTime released_=0;uint64_t releaseMs_=0;
 Phase phase_=Waiting;uint64_t binding_=0,session_=0,origin_=0;std::array<XrPath,2> profiles_{};XrTime began_=0;
public:
 explicit BowInputTrial(bool aimed=false,bool releaseFire=false,bool cancelOnly=false):aimed_(aimed||releaseFire||cancelOnly),releaseFire_(releaseFire&&!cancelOnly),cancelOnly_(cancelOnly){}
 bool cancellationTrial()const{return cancelOnly_;}
 Phase phase()const{return phase_;}
 LivePadState update(LivePadState p,const HandFrame& f,uint64_t binding,bool allowed,uint64_t nowMs=0){
  // Explicit trial consumes RT even after its single completed press. It can
  // never turn that same hold into SpecialAbility or a second shot.
  p.rightTrigger=0;
  const bool valid=allowed&&binding&&f.focused&&f.hands[0].tracked()&&f.hands[1].tracked()&&f.hands[1].triggerActive&&
   std::isfinite(f.hands[1].trigger)&&f.hands[1].trigger>=0&&f.hands[1].trigger<=1;
  const bool changed=binding_!=binding||session_!=f.sessionGeneration||origin_!=f.originGeneration||
   profiles_[0]!=f.hands[0].profile||profiles_[1]!=f.hands[1].profile;
  if(aimed_)p.leftTrigger=0;
  if(phase_==Released||phase_==Aborted){
   // Keep native aiming briefly after intentional shot release, without a
   // second shoot press. Never retain it on identity/scope loss.
   if(aimed_&&!cancelOnly_&&phase_==Released&&valid&&!changed){
    if(releaseFire_){
     if(nowMs>=releaseMs_&&nowMs-releaseMs_<500){p.leftTrigger=255;if(nowMs-releaseMs_<60)p.rightTrigger=255;}
    }else if(f.predictedTime>=released_&&f.predictedTime-released_<500000000ll)p.leftTrigger=255;
   }
   return p;
  }
  if(phase_==Held&&(!valid||changed||f.predictedTime<began_||f.predictedTime-began_>10000000000ll)){
   phase_=Aborted;return p; // Report as failed, never as intentional shot release.
  }
  if(!valid){phase_=Waiting;return p;}
  if(changed&&phase_!=Held)phase_=Waiting;
  binding_=binding;session_=f.sessionGeneration;origin_=f.originGeneration;profiles_={f.hands[0].profile,f.hands[1].profile};
  const float t=f.hands[1].trigger;
  if(phase_==Waiting&&t<=.2f)phase_=Armed;
  else if(phase_==Armed&&t>=.8f){phase_=Held;began_=f.predictedTime;}
  else if(phase_==Held&&t<=.2f){phase_=Released;released_=f.predictedTime;releaseMs_=nowMs;}
  if(cancelOnly_){if(phase_==Held)p.leftTrigger=255;return p;}
  if(aimed_){
   if((phase_==Armed&&!releaseFire_)||phase_==Held||phase_==Released)p.leftTrigger=255;
   if((phase_==Held&&!releaseFire_)||(phase_==Released&&releaseFire_))p.rightTrigger=255;
  }else if(phase_==Held)p.buttons|=LivePadMapper::X;
  return p;
 }
};
}
