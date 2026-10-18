#pragma once
#include <evr/bow_aim.hpp>
#include <evr/ranged_asset_catalog.hpp>
namespace evr::motion {
// VHVR ShootingStaffManager for a staff or wand held in one hand
// (docs/analysis/vhvr_spec_20261010/staff_throw.md, section 3): the shot leaves a
// point on the weapon's own axis and flies along the hand's pointer ray, not along
// the weapon. The pointer ray is the OpenXR aim pose. Value-only; the caller owns
// the native identity and freshness checks.
struct RangedAimModel {unsigned type=0;float emitterY=0;};
// Every staff (native type 5) and wand (type 6) of the read-only catalog. The long axis
// is model +Y and the native emitter joint 2 sits on it; each group shares one skeleton
// (model catalog hierarchy 0870033d: 0.63 m, beb0ccbe: 0.5228 m). Seen in play on one of
// each (survey game233325: Weapon_T2_2H_StaffEpic_01, Weapon_T1_1H_WandEpic_01_Fire).
inline const RangedAimModel* rangedAimModel(uint32_t item){
 static constexpr RangedAimModel staff{5,.63f},wand{6,.5228f};
 for(const auto& a:catalogRangedAssets){
  if(a.item==item){const auto type=catalogRangedRigs[a.rig].type;return type==5?&staff:type==6?&wand:nullptr;}
  if(a.item>item)break;
 }
 return nullptr;
}
// The wand's native attack is the attack button, not the right trigger.
inline bool rangedAimWand(uint32_t item){const auto m=rangedAimModel(item);return m&&m->type==6;}
// +Z along direction, +Y as close to upHint as the direction allows.
inline Quat lookAlong(Vec3 direction,Vec3 upHint){
 const float size=length(direction);if(!finite(direction)||!finite(upHint)||size<1e-6f)throw std::invalid_argument("look direction");
 const auto d=direction*(1/size);auto q=alignDirections({0,0,1},d);
 const auto up=upHint-d*dot(upHint,d);
 if(length(up)>.05f){const auto current=rotate(q,{0,1,0});
  if(dot(current,up)>-.99f*length(up))q=normalized(multiply(alignDirections(current,up),q));}
 return normalized(q);
}
// model: the weapon model in tracking space. worldPosition/worldOrientation: the same
// model in the native world (native model frame = tracking model frame with Z reflected).
// originTracking/directionTracking: the wanted shot line in tracking space.
inline BowWorldAim rangedWorldAim(Pose model,WorldPositionQ32 worldPosition,Quat worldOrientation,Vec3 originTracking,Vec3 directionTracking){
 if(!finite(model.position)||!finite(originTracking)||!finite(directionTracking))throw std::invalid_argument("ranged aim input");
 const auto inverseModel=inverse(model.orientation);
 const auto o=rotate(inverseModel,originTracking-model.position),d=rotate(inverseModel,directionTracking);
 const float size=length(d);
 if(!std::isfinite(size)||size<1e-4f||length(o)>3.f)throw std::invalid_argument("ranged aim line");
 worldOrientation=normalized(worldOrientation);
 const auto direction=rotate(worldOrientation,Vec3{d.x,d.y,-d.z}*(1/size));
 return {cameraPosition(worldPosition,rotate(worldOrientation,{o.x,o.y,-o.z}),1),lookAlong(direction,{0,1,0}),direction};
}
// The emitter on the weapon axis and the hand's pointer ray.
inline BowWorldAim rangedPointerAim(const RangedAimModel& weapon,Pose model,Vec3 scale,WorldPositionQ32 worldPosition,Quat worldOrientation,Pose aimPose){
 if(!finite(scale)||scale.y<.5f||scale.y>1.5f)throw std::invalid_argument("ranged aim scale");
 const auto origin=model.position+rotate(model.orientation,{0,weapon.emitterY*scale.y,0});
 return rangedWorldAim(model,worldPosition,worldOrientation,origin,rotate(aimPose.orientation,{0,0,-1}));
}
// VHVR staff held with both hands (staff_throw.md section 3, "two-handed"): the shot flies
// along the weapon, i.e. along the line from the rear hand to the front hand, from the same
// emitter. model is the two-hand pose of this frame.
inline BowWorldAim rangedAxisAim(const RangedAimModel& weapon,Pose model,Vec3 scale,WorldPositionQ32 worldPosition,Quat worldOrientation){
 if(!finite(scale)||scale.y<.5f||scale.y>1.5f)throw std::invalid_argument("ranged aim scale");
 const auto axis=rotate(model.orientation,{0,1,0});
 return rangedWorldAim(model,worldPosition,worldOrientation,model.position+axis*(weapon.emitterY*scale.y),axis);
}
}
