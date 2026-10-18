#pragma once
#include <evr/bow_physical_trial.hpp>
namespace evr::motion {
// Mock-only input experiment. Retains native time-based power and native aim.
// The default physical trial remains no-shot. A short/invalid pull still cancels.
class BowPhysicalRelease {
 BowPhysicalTrial input_;BowPhysicalMetrics metrics_{};unsigned phase_=0;
 bool released_=false,ended_=false;uint64_t heldMs_=0,releaseMs_=0,lastMs_=0,binding_=0,session_=0,origin_=0;
 XrTime predicted_=0;std::array<XrPath,2> profiles_{};float releasePull_=0;
 static bool scope(const HandFrame& f,const BowPhysicalGeometry& g,bool allowed){
  const auto& h=f.hands[1];const auto q=g.model.orientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  return allowed&&g.binding&&g.frame==f.frame&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&
   f.hands[0].tracked()&&h.tracked()&&h.triggerActive&&std::isfinite(h.trigger)&&h.trigger>=0&&h.trigger<=1&&
   finite(h.grip.position)&&finite(g.model.position)&&std::isfinite(norm)&&std::abs(norm-1)<.02f&&finite(g.scale)&&
   g.scale.x>.5f&&g.scale.y>.5f&&g.scale.z>.5f&&g.scale.x<1.5f&&g.scale.y<1.5f&&g.scale.z<1.5f;
 }
public:
 static constexpr float minimumPull=.10f; // diagnostic threshold, not native power
 void abort(){phase_=4;ended_=true;}
 unsigned phase()const{return phase_;}float releasePull()const{return releasePull_;}
 const BowPhysicalMetrics& metrics()const{return metrics_;}
 LivePadState update(LivePadState pad,const HandFrame& f,const BowPhysicalGeometry& g,bool allowed,uint64_t now){
  pad.leftTrigger=pad.rightTrigger=0;pad.buttons&=~LivePadMapper::X;metrics_={};
  if(ended_)return pad;
  if(released_){
   const bool same=binding_==g.binding&&session_==f.sessionGeneration&&origin_==f.originGeneration&&profiles_[0]==f.hands[0].profile&&profiles_[1]==f.hands[1].profile;
   if(!scope(f,g,allowed)||!same||now<lastMs_||f.predictedTime<predicted_){phase_=4;ended_=true;return pad;}
   lastMs_=now;predicted_=f.predictedTime;
   const auto age=now-releaseMs_;
   if(age>=500){phase_=6;ended_=true;return pad;}
   pad.leftTrigger=255;if(age<60)pad.rightTrigger=255;return pad;
  }
  const auto before=input_.phase();pad=input_.update(pad,f,g,allowed);metrics_=input_.metrics();phase_=unsigned(input_.phase());
  if(before!=BowPhysicalTrial::Held&&input_.phase()==BowPhysicalTrial::Held)heldMs_=now;
  if(before==BowPhysicalTrial::Held&&input_.phase()==BowPhysicalTrial::Cancelled){
   if(!scope(f,g,allowed)||!now||now<heldMs_||now-heldMs_<100||metrics_.pull<minimumPull){ended_=true;return pad;}
   released_=true;phase_=5;releaseMs_=lastMs_=now;predicted_=f.predictedTime;releasePull_=metrics_.pull;
   binding_=g.binding;session_=f.sessionGeneration;origin_=f.originGeneration;profiles_={f.hands[0].profile,f.hands[1].profile};
   pad.leftTrigger=pad.rightTrigger=255;
  }
  if(input_.phase()==BowPhysicalTrial::Aborted)ended_=true;
  return pad;
 }
};
}
