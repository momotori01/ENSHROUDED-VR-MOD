#pragma once
#include <evr/head_gesture.hpp>

namespace evr::motion {
// Only the VR-generated pad goes through this mapper. Never pass a physical
// Xbox packet here. Native UI/special modes keep their ordinary stick meaning.
enum class VerticalStickMode { Unavailable, Gameplay, NativeUi, NativeSpecial, HeadGesture, GestureCandidate, NativeOverlay };
struct VerticalStickConfig {
 float on=.65f,off=.45f,neutral=.20f;
 uint64_t maximumAgeMs=200;
};
struct VerticalStickResult {
 GesturePad pad{};
 VerticalStickMode mode=VerticalStickMode::Unavailable;
 uint16_t mappedButtons=0;
 bool valid=false,needsNeutral=true,contextChanged=false;
};
class VerticalStickMapping {
 VerticalStickConfig c;
 VerticalStickMode mode_=VerticalStickMode::Unavailable;
 uint64_t binding_=0,session_=0,origin_=0,profile_=0,frame_=0;
 XrTime time_=0;
 bool ready_=false,overlayExit_=false;
 int direction_=0;
 void reset(){mode_=VerticalStickMode::Unavailable;binding_=session_=origin_=profile_=frame_=0;time_=0;ready_=false;overlayExit_=false;direction_=0;}
 int axis(float v)const {
  if(v>=c.on)return 1;if(v<=-c.on)return -1;
  if(direction_==1&&v>c.off)return 1;if(direction_==-1&&v< -c.off)return -1;
  return 0;
 }
 static bool finitePad(const GesturePad& p){
  for(float x:{p.left.x,p.left.y,p.right.x,p.right.y})if(!std::isfinite(x)||std::abs(x)>1)return false;
  for(float x:{p.leftTrigger,p.rightTrigger})if(!std::isfinite(x)||x<0||x>1)return false;
  return true;
 }
public:
 explicit VerticalStickMapping(VerticalStickConfig config={}):c(config){
  if(!std::isfinite(c.on)||!std::isfinite(c.off)||!std::isfinite(c.neutral)||
     c.neutral<0||c.neutral>=c.off||c.off>=c.on||c.on>1||!c.maximumAgeMs)
   throw std::invalid_argument("invalid vertical stick mapping configuration");
 }
 // The frontend supplies a freshly observed native mode/binding each time,
 // even when the XR thread has not yet published another immutable hand frame.
 // binding identifies the input context owner, not its address alone.
 VerticalStickResult update(uint64_t now,uint64_t receipt,uint64_t binding,
     VerticalStickMode nativeMode,const HandFrame& hands,const HeadGestureResult& gesture){
  VerticalStickResult out;
  const auto& right=hands.hands[1];
  if(!binding||!receipt||now<receipt||now-receipt>c.maximumAgeMs||
     (nativeMode!=VerticalStickMode::Gameplay&&nativeMode!=VerticalStickMode::NativeUi&&nativeMode!=VerticalStickMode::NativeSpecial&&nativeMode!=VerticalStickMode::NativeOverlay)||
     !hands.focused||!hands.frame||!hands.sessionGeneration||!hands.originGeneration||hands.predictedTime<=0||
     !right.tracked()||!right.stickActive||!std::isfinite(right.stick.y)||std::abs(right.stick.y)>1||
     !gesture.valid||gesture.frame!=hands.frame||gesture.session!=hands.sessionGeneration||
     gesture.origin!=hands.originGeneration||gesture.time!=hands.predictedTime||!finitePad(gesture.pad)||!finitePad(gesture.sourcePad)||!std::isfinite(right.stick.x)||std::abs(right.stick.x)>1){
   reset();return out;
  }
  const bool epoch=binding_&&(binding_!=binding||session_!=hands.sessionGeneration||
      origin_!=hands.originGeneration||profile_!=right.profile);
  if(frame_&&!epoch&&(hands.frame<frame_||hands.predictedTime<time_||
      ((hands.frame==frame_)!=(hands.predictedTime==time_)))){reset();return out;}
  const auto mode=nativeMode==VerticalStickMode::NativeOverlay?nativeMode:gesture.active?VerticalStickMode::HeadGesture:
      gesture.resetSwing?VerticalStickMode::GestureCandidate:nativeMode;
  const bool changed=mode_!=mode||epoch||!binding_;
  const bool enteringOverlay=mode==VerticalStickMode::NativeOverlay&&mode_!=mode;
  if(mode_==VerticalStickMode::NativeOverlay&&mode_!=mode)overlayExit_=true;
  if(changed){ready_=false;direction_=0;}
  // The wheel owns both axes immediately on an ordinary UI transition.
  // Session/device/owner recovery still requires a physically neutral stick.
  if(enteringOverlay&&binding_&&!epoch)ready_=true;
  mode_=mode;binding_=binding;session_=hands.sessionGeneration;origin_=hands.originGeneration;
  profile_=right.profile;frame_=hands.frame;time_=hands.predictedTime;
  // Use the original physical axis to arm. The head mapper intentionally
  // outputs zero during a gesture, even when the physical stick is held down.
  const bool fullNeutral=std::abs(right.stick.x)<=c.neutral&&std::abs(right.stick.y)<=c.neutral;
  if(fullNeutral){ready_=true;overlayExit_=false;}
  else if(!overlayExit_&&mode!=VerticalStickMode::NativeOverlay&&std::abs(right.stick.y)<=c.neutral)ready_=true;
  out.mode=mode;out.valid=true;out.contextChanged=changed;out.pad=gesture.pad;
  out.needsNeutral=!ready_;
  if(mode==VerticalStickMode::NativeOverlay){
   // Native radial menus must not receive temple D-pad/Start/Select or L3/B.
   out.pad=gesture.sourcePad;direction_=0;
   if(!ready_)out.pad.right={};
   return out;
  }
  if(overlayExit_){out.pad.right={};out.pad.buttons&=~gesture.mappedButtons;direction_=0;return out;}
  if(mode==VerticalStickMode::HeadGesture){direction_=0;return out;}
  if(mode==VerticalStickMode::GestureCandidate){out.pad.right.y=0;ready_=false;direction_=0;out.needsNeutral=true;return out;}
  if(mode==VerticalStickMode::Gameplay){
   out.pad.right.y=0;
   // Respect the head mapper's neutral barrier as well as our own.
   direction_=ready_?axis(gesture.pad.right.y):0;
   out.mappedButtons=direction_>0?uint16_t(0x0040):direction_<0?uint16_t(0x2000):0;
   out.pad.buttons|=out.mappedButtons;
  }else{
   direction_=0;
   if(!ready_)out.pad.right.y=0;
  }
  return out;
 }
};
} // namespace evr::motion
