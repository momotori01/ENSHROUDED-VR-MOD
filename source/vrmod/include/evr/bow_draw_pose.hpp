#pragma once
#include <evr/bow_draw_profile.hpp>
namespace evr::motion {
// Private visual deformation only. The measured curve is indexed by model-space
// string displacement, not elapsed time or native projectile power.
inline std::array<hand::Joint,6> knownBowDrawPose(float modelPull){
 if(!std::isfinite(modelPull))throw std::invalid_argument("nonfinite physical bow pull");
 const auto& keys=knownBowDrawKeys;
 modelPull=std::clamp(modelPull,0.f,keys.back().pull);
 if(modelPull<=0)return keys.front().joints;
 if(modelPull>=keys.back().pull)return keys.back().joints;
 size_t hi=1;while(keys[hi].pull<modelPull)++hi;
 const auto& a=keys[hi-1];const auto& b=keys[hi];const float t=(modelPull-a.pull)/(b.pull-a.pull);
 auto out=a.joints;
 for(size_t j=2;j<out.size();++j){
  const auto& u=a.joints[j];const auto& v=b.joints[j];auto q=v.orientation;const auto r=u.orientation;
  if(r.x*q.x+r.y*q.y+r.z*q.z+r.w*q.w<0)q={-q.x,-q.y,-q.z,-q.w};
  out[j].orientation=normalized(Quat{r.x+(q.x-r.x)*t,r.y+(q.y-r.y)*t,r.z+(q.z-r.z)*t,r.w+(q.w-r.w)*t});
  out[j].position=u.position+(v.position-u.position)*t;
  out[j].scale=u.scale+(v.scale-u.scale)*t;
  out[j].positionPadding=u.positionPadding+(v.positionPadding-u.positionPadding)*t;
  out[j].scalePadding=u.scalePadding+(v.scalePadding-u.scalePadding)*t;
 }
 return out;
}
// The native charge curve also moves the actor's drawing hand sideways/up.
// Retain its limb deformation, but constrain the private string endpoint to the
// tracked drawing hand. Joint4 retains its sampled offset relative to joint5.
inline std::array<hand::Joint,6> knownBowNockPose(float modelPull,Vec3 targetTrackingModel){
 if(!finite(targetTrackingModel)||length(targetTrackingModel)>4.f)throw std::invalid_argument("physical bow nock bounds");
 auto out=knownBowDrawPose(modelPull);
 // Reviewed rig parents: [none,0,1,1,1,1], roots have unit scale.
 const auto root=compose(Pose{normalized(out[0].orientation),out[0].position},Pose{normalized(out[1].orientation),out[1].position});
 const Vec3 nativeTarget{targetTrackingModel.x,targetTrackingModel.y,-targetTrackingModel.z};
 const auto target=rotate(inverse(root.orientation),nativeTarget-root.position);
 const auto delta=target-out[5].position;
 out[5].position=target;out[4].position=out[4].position+delta;
 return out;
}
}
