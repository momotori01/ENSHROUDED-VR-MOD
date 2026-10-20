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
// The shown weapon's poses of the last second (2026-10-20, for the sweep from the press - motion_collider_apply.hpp
// sweepFromPress).
inline std::array<motion::ColliderPose,96> past{};inline size_t pastNext=0;   // guarded by mutex
inline void publish(const motion::ColliderPose& p,const motion::HandFrame& h,uint64_t ms){
    if(!requested)return;
    frames.publish(p,h,ms);
    if(p.valid&&p.item){std::lock_guard<std::mutex> lock(mutex);past[pastNext++%past.size()]=p;}
}
// The first pose of `item` shown at or after `ms` (within 60 ms of it).
inline bool pastAt(uint64_t ms,uint32_t item,motion::ColliderPose& out){
    std::lock_guard<std::mutex> lock(mutex);bool found=false;
    for(const auto& p:past)if(p.valid&&p.item==item&&p.milliseconds>=ms&&p.milliseconds<=ms+60&&(!found||p.milliseconds<out.milliseconds)){out=p;found=true;}
    return found;
}
inline motion::ColliderFrameSnapshot snapshotFrame(){return frames.snapshot();}
}
