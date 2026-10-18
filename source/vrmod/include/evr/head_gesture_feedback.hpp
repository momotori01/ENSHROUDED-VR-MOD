#pragma once
#include <evr/head_gesture.hpp>
#include <evr/motion_feedback.hpp>
#include <limits>
namespace evr::motion {
struct GestureFeedbackResult {bool fresh=false;std::array<float,2> amplitude{};std::array<XrDuration,2> duration{100000000,100000000};};
class GestureFeedback {
 uint64_t lastFrame_=0,session_=0,origin_=0;XrTime time_=0,expires_=0;float amplitude_=0;XrPath profile_=0;unsigned hand_=1;
public:
 GestureFeedbackResult update(const HandFrame& f,const HeadGestureResult& gesture,const Feedback& attack,uint64_t now){
  GestureFeedbackResult out;
  if(f.sessionGeneration==session_&&f.originGeneration==origin_&&f.frame==lastFrame_)return out;
  out.fresh=true;
  const bool gestureCurrent=gesture.frame==f.frame&&gesture.session==f.sessionGeneration&&gesture.origin==f.originGeneration&&gesture.time==f.predictedTime;
  const bool epoch=f.sessionGeneration!=session_||f.originGeneration!=origin_||f.hands[1].profile!=profile_;
  const bool gap=lastFrame_&&(f.predictedTime<=time_||f.predictedTime-time_>100000000||(!epoch&&f.frame<lastFrame_));
  if(epoch||gap||!f.focused||!f.hands[1].tracked()||!gesture.valid||!gestureCurrent||gesture.left){expires_=0;amplitude_=0;}
  lastFrame_=f.frame;session_=f.sessionGeneration;origin_=f.originGeneration;time_=f.predictedTime;profile_=f.hands[1].profile;
  if(!f.frame||!session_||!origin_||f.predictedTime<=0||!f.focused||gap)return out;
  out.amplitude=feedbackForFrame(attack,f,now);
  // The entry pulse goes to the hand whose temple region started the gesture.
  if(gestureCurrent&&gesture.entered&&gesture.active&&gesture.valid&&gesture.enteredHand<2&&f.hands[gesture.enteredHand].tracked()&&
     std::isfinite(gesture.pulseAmplitude)&&gesture.pulseAmplitude>0&&gesture.pulseAmplitude<=1&&
     gesture.pulseDuration>0&&gesture.pulseDuration<=100000000&&f.predictedTime<=std::numeric_limits<XrTime>::max()-gesture.pulseDuration){
   expires_=f.predictedTime+gesture.pulseDuration;amplitude_=gesture.pulseAmplitude;hand_=gesture.enteredHand;
  }
  if(expires_>f.predictedTime&&f.hands[hand_].tracked()){
   if(!out.amplitude[hand_])out.duration[hand_]=expires_-f.predictedTime;
   out.amplitude[hand_]=std::max(out.amplitude[hand_],amplitude_);
  }else {expires_=0;amplitude_=0;}
  return out;
 }
};
}
