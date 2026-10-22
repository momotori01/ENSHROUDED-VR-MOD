#pragma once
// The game's own rule of a block (2026-10-22, user: "is the guard invincible then? that is not allowed"; build 26082781):
// applyDamage (0x3d50d0) blocks a blow when the target's Actor is in its guard state (Blocking, 2; 0x21aaa0) and the hit
// angle passes 0x21a4f0: (angle + pi) wrapped into [0, 2 pi) lies in [0, 80] or [280, 360] degrees (0x15038c0 1.3962634,
// 0x15038c4 4.8869219). The hit angle (0x2190e0) is the signed angle between the target's front (its orientation turning
// +Z, level) and the way from the attacker to the target (level): a blow from in front has 180 degrees, so the block holds
// for an attacker within 80 degrees of the target's front. A blocked blow takes no health and costs stamina instead
// (0x3d6e42..0x3d702a, 0xe618d0): round(10/50 x damage x StaminaDamageMod - Block), 0 at the least.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace evr::guard {
inline constexpr double halfAngle=1.3962633609771729;   // 80 degrees
// The angle (radians, 0..pi) between the front (x, z) and the way from the target to the attacker (x, z); 0 when the
// attacker stands on the target (closer than 5 cm) or the front is not known.
inline double angleFromFront(double frontX,double frontZ,double toAttackerX,double toAttackerZ){
    const double f=std::sqrt(frontX*frontX+frontZ*frontZ),a=std::sqrt(toAttackerX*toAttackerX+toAttackerZ*toAttackerZ);
    if(!(f>1e-6)||!(a>=.05)||!std::isfinite(f)||!std::isfinite(a))return 0;
    return std::acos(std::clamp((frontX*toAttackerX+frontZ*toAttackerZ)/(f*a),-1.,1.));
}
inline bool blocks(double frontX,double frontZ,double toAttackerX,double toAttackerZ){return angleFromFront(frontX,frontZ,toAttackerX,toAttackerZ)<=halfAngle;}
// The stamina a blocked blow costs: the health it would have taken (damage > 0), the target's StaminaDamageMod and Block.
inline int32_t staminaCost(int32_t damage,double staminaDamageMod,int32_t block){
    const double x=10.0/50.0*double(damage<0?-damage:damage)*staminaDamageMod-double(block);
    if(!(x>0)||!std::isfinite(x))return 0;
    return int32_t(x+.5);   // the game: + 0.5, truncated (x > 0 here)
}
}
