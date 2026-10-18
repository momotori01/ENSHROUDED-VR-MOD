#pragma once
#include <evr/world_position.hpp>
#include <cstring>
namespace evr::motion {
struct EffectTransform {WorldPositionQ32 position;Quat orientation;Vec3 scale;uint32_t padding=0;};
struct EffectOffset {Vec3 local,world;Quat orientation;};
static_assert(sizeof(EffectTransform)==56&&offsetof(EffectTransform,scale)==40,"native transform value layout");
static_assert(sizeof(EffectOffset)==40,"native attachment offset value layout");
inline void requireEffectQuaternion(Quat q){
 const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
 if(!std::isfinite(n)||std::abs(n-1)>0.002f)throw std::invalid_argument("nonunit effect mount quaternion");
}
// The observed self-attached static torch uses a metre offset, NOT model scale.
// Keep the raw affine quaternion rule used by the game's offset transformation.
inline EffectTransform mountHeldEffect(EffectTransform model,EffectOffset offset){
 requireEffectQuaternion(model.orientation);requireEffectQuaternion(offset.orientation);
 if(!finite(offset.local)||!finite(offset.world)||length(offset.local)>10||length(offset.world)>10||!finite(model.scale)||
    model.scale.x<=0||model.scale.y<=0||model.scale.z<=0)throw std::invalid_argument("invalid held effect offset/scale");
 const auto q=model.orientation;const Vec3 u{q.x,q.y,q.z};
 const Vec3 delta=offset.local+cross(u,offset.local)*(2*q.w)+cross(u,cross(u,offset.local))*2+offset.world;
 model.position=cameraPosition(model.position,delta,1);model.orientation=multiply(q,offset.orientation);return model;
}
inline double effectPositionError(const EffectTransform& a,const EffectTransform& b){
 const double x=relativeQ32(a.position.x,b.position.x),y=relativeQ32(a.position.y,b.position.y),z=relativeQ32(a.position.z,b.position.z);
 return std::sqrt(x*x+y*y+z*z);
}
inline float effectRotationError(Quat a,Quat b){
 return std::min(std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z),std::abs(a.w-b.w)}),
                 std::max({std::abs(a.x+b.x),std::abs(a.y+b.y),std::abs(a.z+b.z),std::abs(a.w+b.w)}));
}
}
