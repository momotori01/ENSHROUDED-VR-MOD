#pragma once
#include <evr/motion_actions.hpp>
#include <evr/motion_collider_pose.hpp>
#include <mutex>
namespace evr::motion {
struct ColliderFrameSnapshot {
 ColliderPose pose{};HandFrame hands{};uint64_t handMs=0;
 bool coherent()const {
  return pose.valid&&pose.frame&&pose.serial&&pose.session&&pose.origin&&pose.predictedTime>0&&pose.milliseconds&&handMs>=pose.milliseconds&&
   hands.frame==pose.frame&&hands.sessionGeneration==pose.session&&hands.originGeneration==pose.origin&&hands.predictedTime==pose.predictedTime;
 }
};
// Publish only a completed hand/weapon pair. Building its successor leaves the
// last complete immutable pair visible, with the original age bounds unchanged.
// Actual tracking/context failure explicitly invalidates; no stale revival.
class ColliderFrameChannel {
 mutable std::mutex mutex_;ColliderFrameSnapshot value_{};
public:
 bool publish(const ColliderPose& pose,const HandFrame& hands,uint64_t ms){
  ColliderFrameSnapshot next{pose,hands,ms};const bool valid=next.coherent();
  std::lock_guard<std::mutex> lock(mutex_);value_=valid?next:ColliderFrameSnapshot{};return valid;
 }
 void invalidate(){std::lock_guard<std::mutex> lock(mutex_);value_={};}
 ColliderFrameSnapshot snapshot()const {std::lock_guard<std::mutex> lock(mutex_);return value_;}
};
}
