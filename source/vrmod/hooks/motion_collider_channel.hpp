#pragma once
// Exact displayed weapon transform, copied across threads without ECS pointers.
namespace motionColliderChannel {
inline bool requested=false,requireAuthority=false;
// Unlimited play (world-live-melee-contact.txt): every attack of the local player's shown weapon, no grip button,
// either hand slot, no event authority; an update without a fresh tracked pose keeps the game's transform.
inline bool live=false;
inline std::mutex mutex;
inline motion::ColliderFrameChannel frames;
inline motion::EventAuthority authority;
inline std::atomic<bool> eventBlocked{false};
inline void authorize(const motion::EventAuthority& a){std::lock_guard<std::mutex> lock(mutex);authority=a;}
inline motion::EventAuthority authorized(){std::lock_guard<std::mutex> lock(mutex);return authority;}
inline void invalidate(){if(requested)frames.invalidate();}
inline void publish(const motion::ColliderPose& p,const motion::HandFrame& h,uint64_t ms){if(requested)frames.publish(p,h,ms);}
inline motion::ColliderFrameSnapshot snapshotFrame(){return frames.snapshot();}
}
