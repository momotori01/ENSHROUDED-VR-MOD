#include <evr/damage_calc.hpp>
#include <evr/damage_tables.hpp>
#include <evr/breakable_models.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::damage;
// The worked examples of docs/analysis/damage_formula_20261022/damage_model.py (one-hand sword L1, strength 0, no crit):
// a fog soldier (all 1.0) 11 / third stroke 17 / heavy 33, a skeleton (cut 0.8) 9 / 13 / 26.
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 HitInputs in;in.itemDamage=11;in.distribution[0]=1;in.crit=0;
 for(const auto& [mod,fog,bone]:{std::array<float,3>{1.f,11.f,9.f},std::array<float,3>{1.5f,17.f,13.f},std::array<float,3>{3.f,33.f,26.f}}){
  in.attackMod=mod;in.susceptibility={1,1,1,1,1,1,1,1};ck(hitLoss(in).loss==int(fog));
  in.susceptibility[0]=.8f;ck(hitLoss(in).loss==int(bone));}
 // strength 10: x1.5; a two-type weapon (half cut, half fire) on fire 1.2
 in.attackMod=1;in.susceptibility={1,1,1,1,1,1,1,1};in.strength=10;ck(hitLoss(in).loss==17);   // 11*1.5=16.5 -> 17
 in.strength=0;in.distribution={.5f,0,0,.5f,0,0,0,0,0};in.susceptibility[3]=1.2f;ck(hitLoss(in).loss==12);   // 5.5+6.6=12.1
 // the additive group adds to one base: damageMod 1.2 and meleeDamageMod 1.1 -> x1.3 (not 1.32)
 in.distribution={1,0,0,0,0,0,0,0,0};in.susceptibility={1,1,1,1,1,1,1,1};in.itemDamage=100;in.damageMod=1.2f;in.meleeDamageMod=1.1f;ck(hitLoss(in).loss==130);
 in.melee=false;ck(hitLoss(in).loss==120);in.melee=true;in.damageMod=1;in.meleeDamageMod=1;
 // the expected crit: 10 % of +0.5 -> +5 %
 in.crit=-1;in.critChance=.1f;in.critDamageMod=1.5f;ck(hitLoss(in).loss==105);in.crit=1;ck(hitLoss(in).loss==150);
 // healing is not damage
 in.crit=0;in.distribution={0,0,0,0,0,0,0,1,0};ck(hitLoss(in).loss==0);
 // the tables: sorted, found
 ck(std::is_sorted(attacks.begin(),attacks.end(),[](const Attack& a,const Attack& b){return a.sequence<b.sequence;}));
 ck(std::is_sorted(enemies.begin(),enemies.end(),[](const Enemy& a,const Enemy& b){return a.guid<b.guid;}));
 ck(attackMod(71878861u)==1.5f&&attackKnown(71878861u)&&attackMod(1u)==1.f&&!attackKnown(1u));   // Attack_Player_1H_Club_LightChain03
 ck(enemy(enemies[3].guid)==&enemies[3]&&!enemy({1,2,3,4}));
 ck(perk(1934371769u)&&perk(1934371769u)->type==0&&!perk(1u));
 ck(std::is_sorted(evr::breakable::models.begin(),evr::breakable::models.end(),[](const evr::breakable::Model& a,const evr::breakable::Model& b){return a.guid<b.guid;}));
 // the palisade of the solo test (game-20261022-023136): wood, 300
 {const auto* p=evr::breakable::find({3768114363u,1226531168u,3327279514u,724879804u});ck(p&&p->material==1&&p->healthMax==300);}
 std::cout<<"PASS damage_calc "<<n<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
