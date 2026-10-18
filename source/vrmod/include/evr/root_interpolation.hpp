#pragma once
#include <evr/world_position.hpp>
#include <algorithm>
#include <cstddef>

namespace evr::game {
// Observers may wait for a fresh camera; rendering still requires one.
// A UI hold never makes a missing sample or a backwards clock usable.
inline bool rootSampleUsable(uint64_t number,uint64_t sampledMs,uint64_t now,bool uiHeld){
    return number&&now>=sampledMs&&(now-sampledMs<=1000||uiHeld);
}
struct RootTransform {
    WorldPositionQ32 position;
    Quat orientation;
    Vec3 scale;
    uint32_t padding=0;
};
struct RootHistory {
    RootTransform rendered,source,target;
    uint32_t sourceTick=0,newestTick=0;
};
static_assert(sizeof(RootTransform)==56&&sizeof(RootHistory)==176);
static_assert(offsetof(RootTransform,orientation)==24&&offsetof(RootHistory,source)==56&&offsetof(RootHistory,target)==112);
inline Quat rootSlerp(Quat a,Quat b,float t){
    auto valid=[](Quat q){const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(n)&&std::abs(n-1)<.002f;};
    if(!valid(a)||!valid(b))throw std::invalid_argument("non-unit native root quaternion");
    float dot=((a.x*b.x+a.y*b.y)+a.z*b.z)+a.w*b.w;
    if(dot<0){dot=-dot;b={-b.x,-b.y,-b.z,-b.w};}
    float wa=1-t,wb=t;
    // Threshold and non-normalized near-linear branch of native 29b0b0.
    if(dot<.999f){const float angle=std::acos(dot), reciprocal=1/std::sin(angle);wa=std::sin(angle*wa)*reciprocal;wb=std::sin(angle*t)*reciprocal;}
    return {a.x*wa+b.x*wb,a.y*wa+b.y*wb,a.z*wa+b.z*wb,a.w*wa+b.w*wb};
}
inline int64_t interpolateRootAxis(int64_t source,int64_t target,float fraction){
    const uint64_t magnitude=target>=source?uint64_t(target)-uint64_t(source):uint64_t(source)-uint64_t(target);
    if(magnitude>uint64_t(INT64_MAX))throw std::invalid_argument("native root delta overflow");
    const int64_t delta=target>=source?int64_t(magnitude):-int64_t(magnitude);
    // Preserve the native single-precision local difference and truncation.
    // Absolute Q32 positions are never converted to float.
    const float meters=float(delta)*2.3283064365386963e-10f;
    const float raw=(meters*fraction)*4294967296.f;
    if(!std::isfinite(raw)||double(raw)>=9223372036854775808.||double(raw)<-9223372036854775808.)
        throw std::invalid_argument("interpolated native root delta overflow");
    const auto offset=int64_t(raw);
    if((offset>0&&source>INT64_MAX-offset)||(offset<0&&source<INT64_MIN-offset))throw std::invalid_argument("interpolated native root coordinate overflow");
    return source+offset;
}
inline RootTransform stableRoot(const RootHistory& history,uint32_t tick,float fraction){
    if(!std::isfinite(fraction)||fraction<0||fraction>1)throw std::invalid_argument("invalid game interpolation fraction");
    const uint32_t gap=tick>history.sourceTick?tick-history.sourceTick:1;
    const float alpha=(float(gap-1)+fraction)/float(gap);
    RootTransform out;
    out.position={interpolateRootAxis(history.source.position.x,history.target.position.x,alpha),
                  interpolateRootAxis(history.source.position.y,history.target.position.y,alpha),
                  interpolateRootAxis(history.source.position.z,history.target.position.z,alpha)};
    out.orientation=rootSlerp(history.source.orientation,history.target.orientation,alpha);
    out.scale=history.source.scale+(history.target.scale-history.source.scale)*alpha;
    if(!finite(out.scale))throw std::invalid_argument("invalid native root scale");
    return out; // Ignore prior rendered/animation position and never advance history.
}
}
