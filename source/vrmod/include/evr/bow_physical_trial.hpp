#pragma once
#include <evr/bow_input_trial.hpp>
#include <evr/world_position.hpp>
namespace evr::motion {
// Explicit Mock diagnostic: VHVR-style near-string acquisition and physical
// pull measurement. Native LT is charged only after acquisition. Every release
// cancels; no firing, native draw-power write or projectile aim is implemented.
struct BowPhysicalGeometry {
 uint64_t binding=0,frame=0;Pose model=identityPose();Vec3 scale{1,1,1};
 WorldPositionQ32 worldPosition{};Quat worldOrientation{0,0,0,1};uint64_t sourceWorld=0;uint32_t entity=0;
};
struct BowPhysicalMetrics {bool valid=false,withinRange=false;float distance=0,pull=0,lateral=0;Vec3 nock{};};
class BowPhysicalTrial {
public: enum Phase {Waiting,Armed,Held,Cancelled,Aborted};
private:
 Phase phase_=Waiting;BowPhysicalMetrics metrics_{};
 uint64_t binding_=0,session_=0,origin_=0;std::array<XrPath,2> profiles_{};XrTime began_=0,lastTime_=0;
public:
 // Known model eebea32e: idle joint5 through native roots0/1, reflected to XR.
 static constexpr Vec3 restString{0,.00155006f,.24005109f};
 static constexpr float attachRange=.20f; // VHVR BowLocalManager attachRange
 Phase phase()const{return phase_;}const BowPhysicalMetrics& metrics()const{return metrics_;}
 LivePadState update(LivePadState pad,const HandFrame& f,const BowPhysicalGeometry& g,bool allowed){
  pad.leftTrigger=pad.rightTrigger=0;pad.buttons&=~LivePadMapper::X;metrics_={};
  if(phase_==Cancelled||phase_==Aborted)return pad; // One acquisition per diagnostic.
  const auto& right=f.hands[1];const auto q=g.model.orientation;
  const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  const bool valid=allowed&&g.binding&&g.frame==f.frame&&f.frame&&f.sessionGeneration&&f.originGeneration&&
   f.predictedTime>0&&f.focused&&f.hands[0].tracked()&&right.tracked()&&right.triggerActive&&
   std::isfinite(right.trigger)&&right.trigger>=0&&right.trigger<=1&&finite(right.grip.position)&&finite(g.model.position)&&
   std::isfinite(norm)&&std::abs(norm-1)<.02f&&finite(g.scale)&&g.scale.x>.5f&&g.scale.y>.5f&&g.scale.z>.5f&&
   g.scale.x<1.5f&&g.scale.y<1.5f&&g.scale.z<1.5f;
  const bool changed=binding_!=g.binding||session_!=f.sessionGeneration||origin_!=f.originGeneration||
   profiles_[0]!=f.hands[0].profile||profiles_[1]!=right.profile;
  if(phase_==Held&&(!valid||changed||f.predictedTime<lastTime_||f.predictedTime-began_>10000000000ll)){
   phase_=Aborted;return pad;
  }
  if(!valid){phase_=Waiting;return pad;}
  if(changed)phase_=Waiting;
  binding_=g.binding;session_=f.sessionGeneration;origin_=f.originGeneration;
  profiles_={f.hands[0].profile,right.profile};lastTime_=f.predictedTime;
  const Vec3 nock{restString.x*g.scale.x,restString.y*g.scale.y,restString.z*g.scale.z};
  const auto local=rotate(inverse(g.model.orientation),right.grip.position-g.model.position);
  const auto delta=local-nock;
  metrics_={true,length(delta)<=attachRange,length(delta),std::max(0.f,delta.z),std::sqrt(delta.x*delta.x+delta.y*delta.y),g.model.position+rotate(g.model.orientation,nock)};
  if(phase_==Waiting&&right.trigger<=.2f)phase_=Armed;
  else if(phase_==Armed&&right.trigger>=.8f){
   // A press made away from the string cannot acquire later while held.
   if(metrics_.withinRange){phase_=Held;began_=f.predictedTime;}else phase_=Waiting;
  }else if(phase_==Held){
   if(metrics_.distance>1.2f||metrics_.lateral>.35f||delta.z<-.25f){phase_=Aborted;return pad;}
   if(right.trigger<=.2f)phase_=Cancelled;
  }
  if(phase_==Held)pad.leftTrigger=255;
  return pad;
 }
};
}
