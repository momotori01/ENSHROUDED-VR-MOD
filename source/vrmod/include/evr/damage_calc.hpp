#pragma once
// A player's melee hit on an enemy: the health it takes, as the game's applyDamage computes it (exe build 26082781,
// 0x3d50d0; docs/analysis/damage_formula_20261022/README.md, reference implementation damage_model.py). For a guest's own
// stroke on its own PC (2026-10-22, user: "compute it from the weapon and my own state from the start").
//   B[t] = itemDamage * distribution[t] * (1 + 0.05 * strength) * weaponCategoryMod * attackMod          (multiplied)
//   D[t] = B[t] * (1 + (damageMod-1) + (meleeDamageMod-1) + (meleeTypeMod[t]-1) + (against-1) + crit)  (added to one base)
//   E[t] = D[t] * susceptibility[t];  loss = round(sum E[t]), half away from zero
// against: 1 + (class mod - 1) + (flying mod - 1) + (faction mod - 1). crit: a server-side roll - here its expected value
// p * (critDamageMod - 1), or 0 / (critDamageMod - 1) when told. Back stab, sneak and merciless are not taken (unknown here).
#include <array>
#include <cmath>
#include <cstdint>

namespace evr::damage {
// distribution index: 0 cut, 1 pierce, 2 blunt, 3 fire, 4 ice, 5 fog, 6 lightning, 7 healing, 8 poison (the item's
// DamageDistribution); susceptibility: cut, pierce, blunt, fire, ice, fog, lightning, poison.
struct HitInputs {
    float itemDamage=0;                  // ItemState.baseDamageUi of the weapon's instance (= trunc(trunc(10+1.5L)*dmgMod*speedMod))
    std::array<float,9> distribution{};  // the item's, with the active damage perks added
    float strength=0,categoryMod=1,attackMod=1;
    float damageMod=1,meleeDamageMod=1;std::array<float,3> meleeTypeMod{1,1,1};   // cut, pierce, blunt
    float against=1,critChance=0,critDamageMod=1.5f;bool melee=true;
    int crit=-1;                         // -1 expected value, 0 none, 1 a crit
    std::array<float,8> susceptibility{1,1,1,1,1,1,1,1};
};
struct HitResult {int32_t loss=0;float beforeSusceptibility=0,total=0,additive=0;};
inline float f32(double v){return float(v);}
inline HitResult hitLoss(const HitInputs& in){
    HitResult r;
    const float scale=f32(1.0+0.05*double(in.strength));
    float add=(in.damageMod-1.f)+(in.melee?in.meleeDamageMod-1.f:0.f)+(in.against-1.f);
    const float critAdd=in.critDamageMod-1.f;
    if(in.crit<0)add+=std::min(1.f,std::max(0.f,in.critChance))*critAdd;else if(in.crit>0)add+=critAdd;
    r.additive=add;
    static constexpr int toSusceptibility[9]={0,1,2,3,4,5,6,-1,7};
    double total=0,before=0;
    for(int t=0;t<9;++t){
        if(t==7||in.distribution[size_t(t)]==0)continue;   // healing is not damage
        const float d0=f32(double(in.itemDamage)*in.distribution[size_t(t)]);
        const float b=f32(double(d0)*scale*in.categoryMod*in.attackMod);
        const float typeAdd=(in.melee&&t<3)?in.meleeTypeMod[size_t(t)]-1.f:0.f;
        const float d=f32(double(b)*(1.0+double(add)+typeAdd));
        before+=d;total+=double(d)*in.susceptibility[size_t(toSusceptibility[t])];
    }
    r.beforeSusceptibility=float(before);r.total=float(total);
    r.loss=int32_t(total>=0?std::floor(total+0.5):-std::floor(-total+0.5));
    return r;
}
}
