#pragma once
#include <array>
#include <cstdint>
namespace evr::motion {
// Generated from the read-only audit of every hand-held item of the game data; does not certify live handling.
// Generator: docs/analysis/held_1_0_20261017/generate_held_catalogs_1_0.py (1.0 game data ^/game38/branches/release_1.0)
// Weapons held as a pair, one in each hand (weapon type Dagger, 13). The item has no model of its own: its visual
// entity spawns two children with this model, one on the right hand slot (8) and one on the left (9), both with the
// identity transform. rearY/tipY: extent of the model along its long axis (+Y), metres.
struct CatalogDualWieldAsset {uint32_t item;std::array<uint32_t,4> guid;float rearY,tipY;};
inline constexpr std::array<CatalogDualWieldAsset,60> catalogDualWieldAssets{{
 {72164802u,{138786582u,1264774471u,2829777070u,2659024920u},-0.1196f,0.5476f}, // Weapon_T3_2H_DaggersEpic_Parry_Poison (wpn_dagger_nature_epic)
 {103223166u,{26079272u,1145626337u,686431666u,4207257268u},-0.1487f,0.5795f}, // Weapon_T5_2H_Daggers_Poison (wpn_dagger_mb_common_02)
 {191356948u,{3432609880u,1162019330u,1433568900u,1654783720u},-0.1720f,0.5453f}, // Weapon_T7_2H_DaggersEpic_Poison_Parry (wpn_dagger_t7_epic_01)
 {301510235u,{901954147u,1311887852u,3350506638u,41884412u},-0.1784f,0.5068f}, // Weapon_T1_2H_Daggers_02_Parry (wpn_dagger_03)
 {392894300u,{519904386u,1254028993u,3802615692u,2350937851u},-0.1096f,0.5720f}, // Weapon_T1_2H_DaggersEpic_DPS (wpn_dagger_basic_epic)
 {488946154u,{2794820206u,1135355220u,2975609472u,1464649977u},-0.1474f,0.5310f}, // Weapon_T4_2H_DaggersEpic_Poison (wpn_dagger_arabian_common)
 {552284259u,{1487595654u,1161469976u,1415458948u,578097959u},-0.1893f,0.4839f}, // Craft_Weapon_T5_2H_Lore_DaggersEpic_DPS_hunter_hint04 (wpn_dagger_t3_lore_epic_01)
 {764137977u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T7_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {803635338u,{1161066868u,1110331743u,745436571u,3648740269u},-0.1362f,0.5105f}, // Weapon_T1_2H_Daggers_01_DPS (wpn_dagger_02)
 {863684883u,{1487595654u,1161469976u,1415458948u,578097959u},-0.1893f,0.4839f}, // Craft_Weapon_T7_2H_Lore_DaggersEpic_DPS_hunter_hint04 (wpn_dagger_t3_lore_epic_01)
 {876348272u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T1_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {890590294u,{2306857243u,1136831423u,4269034680u,1367657742u},-0.1244f,0.5136f}, // Craft_Weapon_T5_2H_Lore_Daggers_DPS_ministory_02 (wpn_dagger_t5_lore_01)
 {913533703u,{2306857243u,1136831423u,4269034680u,1367657742u},-0.1244f,0.5136f}, // Weapon_T5_2H_Lore_Daggers_DPS_ministory_02 (wpn_dagger_t5_lore_01)
 {987312206u,{4147472284u,1151984704u,3477049254u,1171526006u},-0.1720f,0.5453f}, // Weapon_T7_2H_Daggers_Poison_Parry (wpn_dagger_t7_common_01)
 {1008092854u,{2845809627u,1298079287u,3805055635u,774915289u},-0.1173f,0.5676f}, // Weapon_T2_2H_DaggersEpic_Poison (wpn_dagger_wolf_epic)
 {1019461234u,{1487595654u,1161469976u,1415458948u,578097959u},-0.1893f,0.4839f}, // Craft_Weapon_T4_2H_Lore_DaggersEpic_DPS_hunter_hint04 (wpn_dagger_t3_lore_epic_01)
 {1072446832u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T3_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {1155686002u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T4_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {1156795516u,{832727430u,1318167789u,2148984988u,2229056400u},-0.1695f,0.7449f}, // Weapon_T4_2H_DaggersEpic_DPS (wpn_dagger_t4_epic_01)
 {1177521213u,{3324024508u,1201996589u,1370556077u,2464243835u},-0.1647f,0.5618f}, // Weapon_T5_2H_Daggers_Parry (wpn_dagger_mb_common_03)
 {1262344957u,{660133060u,1080992477u,3975534011u,1258654212u},-0.1517f,0.6173f}, // Weapon_T5_2H_DaggersEpic_Parry (wpn_dagger_mb_epic_03)
 {1551286900u,{2208177601u,1112775755u,4145697679u,2716722804u},-0.1366f,0.5397f}, // Weapon_T2_2H_Daggers_DPS (wpn_dagger_wolf_common)
 {1613250072u,{3033485193u,1197918534u,712782720u,3443497067u},-0.1618f,0.4783f}, // Weapon_T1-TX_2H_Daggers_Scavenger_Weakling (wpn_dagger_scavenger_weakling_02)
 {1710543728u,{998112958u,1309376408u,1509419702u,1538784671u},-0.1375f,0.5706f}, // Weapon_T2_2H_Daggers_Poison (wpn_dagger_09)
 {1729196030u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Craft_Weapon_T2_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {1790379797u,{1752298758u,1210892201u,76521103u,2326219399u},-0.1676f,0.5463f}, // Weapon_T5_2H_DaggersEpic_DPS (wpn_dagger_mb_epic_01)
 {1851176688u,{3604319436u,1107655876u,3331937706u,2013922796u},-0.1705f,0.6689f}, // Weapon_T3_2H_DaggersEpic_DPS (wpn_dagger_t3_01)
 {1873675161u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Craft_Weapon_T6_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {1916010187u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T6_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {1972844940u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Craft_Weapon_T5_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {2066454242u,{2645879839u,1133001498u,3146293133u,3367974108u},-0.1648f,0.7281f}, // Weapon_T4_2H_Daggers_DPS (wpn_dagger_arabian_epic)
 {2135782770u,{4278004393u,1199177336u,2688607414u,3914443203u},-0.1412f,0.6003f}, // Weapon_T6_2H_Daggers_DPS_Parry (wpn_dagger_t6_common_01)
 {2158737906u,{1055206331u,1121632793u,3749623220u,2017087415u},-0.1384f,0.6017f}, // Weapon_T6_2H_DaggersEpic_DPS_Parry (wpn_dagger_t6_epic_01)
 {2387592374u,{1487595654u,1161469976u,1415458948u,578097959u},-0.1893f,0.4839f}, // Craft_Weapon_T3_2H_Lore_DaggersEpic_DPS_hunter_hint04 (wpn_dagger_t3_lore_epic_01)
 {2432243778u,{1487595654u,1161469976u,1415458948u,578097959u},-0.1893f,0.4839f}, // Craft_Weapon_T6_2H_Lore_DaggersEpic_DPS_hunter_hint04 (wpn_dagger_t3_lore_epic_01)
 {2700871988u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Weapon_T2_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {2796408306u,{1898140759u,1238432950u,1245401782u,3153895190u},-0.1517f,0.5351f}, // Weapon_TX_2H_Daggers_DPS_FogNest (wpn_dagger_fog_epic_01)
 {2837156111u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Craft_Weapon_T3_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {2860747780u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Weapon_T1_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {2882588243u,{1090727824u,1278488722u,4151950524u,4093084453u},-0.1634f,0.5262f}, // Weapon_T6_2H_Daggers_Poison (wpn_dagger_t6_common_02)
 {2936153518u,{752577103u,1162537224u,3733549489u,327747874u},-0.1931f,0.5029f}, // Weapon_T4_2H_Daggers_Poison (wpn_dagger_04)
 {3025469016u,{1168018141u,1220077344u,634412689u,3118366739u},-0.1116f,0.6351f}, // Weapon_T4_2H_DaggersEpic_Parry (wpn_dagger_08_glow)
 {3054076942u,{500191089u,1196750286u,1121722001u,2588519599u},-0.1593f,0.5108f}, // Weapon_T3_2H_Daggers_Parry_Poison (wpn_dagger_nature_common)
 {3056420264u,{83124422u,1172941883u,467705486u,3684054887u},-0.2382f,0.6352f}, // Weapon_T1_2H_DaggersEpic_Parry (wpn_dagger_05)
 {3096148752u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Craft_Weapon_T7_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {3108868150u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T2_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {3231907188u,{1428149816u,1186016025u,3238736786u,3323370944u},-0.1819f,0.5037f}, // Craft_Weapon_T5_2H_Lore_Daggers_townsfolk_female_02_hint_01 (wpn_dagger_t1_01)
 {3397273990u,{1062365325u,1084064773u,212317852u,339768042u},-0.1677f,0.5502f}, // Weapon_T5_2H_Daggers_DPS (wpn_dagger_mb_common_01)
 {3412482507u,{3685598108u,1156545519u,2688752020u,146572967u},-0.1462f,0.5814f}, // Weapon_T5_2H_DaggersEpic_Poison (wpn_dagger_mb_epic_02)
 {3548335929u,{1487595654u,1161469976u,1415458948u,578097959u},-0.1893f,0.4839f}, // Weapon_T3_1H_Lore_DaggersEpic_DPS_hunter_hint04 (wpn_dagger_t3_lore_epic_01)
 {3567673697u,{2022088692u,1118189018u,4277964417u,2905207042u},-0.2197f,0.6610f}, // Weapon_T7_2H_Daggers_DPS (wpn_dagger_t7_common_02)
 {3583521141u,{2253273170u,1240899080u,2448967589u,3078469739u},-0.1116f,0.6351f}, // Weapon_T4_2H_Daggers_Parry (wpn_dagger_08)
 {3618171376u,{2306857243u,1136831423u,4269034680u,1367657742u},-0.1244f,0.5136f}, // Craft_Weapon_T7_2H_Lore_Daggers_DPS_ministory_02 (wpn_dagger_t5_lore_01)
 {3717136224u,{309747377u,1272671648u,3441296057u,4021367264u},-0.1613f,0.5168f}, // Craft_Weapon_T4_2H_Lore_DaggersEpic_Parry_bard_Hint02 (wpn_dagger_t2_epic_01)
 {3873714456u,{1086831894u,1244917272u,737085600u,3268752563u},-0.1340f,0.5923f}, // Weapon_T3_2H_Daggers_DPS (wpn_dagger_06)
 {3974938802u,{1645523785u,1190042778u,3956946308u,1521875859u},-0.1645f,0.4960f}, // Weapon_T0_2H_Daggers_DPS_Craft (wpn_dagger_basic_common)
 {4123213139u,{3572326038u,1162241878u,1137516973u,3562817838u},-0.1517f,0.5816f}, // Weapon_T2_2H_DaggersEpic_DPS (wpn_dagger_07)
 {4131716794u,{2306857243u,1136831423u,4269034680u,1367657742u},-0.1244f,0.5136f}, // Craft_Weapon_T6_2H_Lore_Daggers_DPS_ministory_02 (wpn_dagger_t5_lore_01)
 {4134622088u,{3309077779u,1122867049u,3639600535u,1101621189u},-0.4950f,0.6265f}, // Weapon_T7_2H_DaggersEpic_DPS (wpn_dagger_t7_epic_02_chain)
 {4248181976u,{3090639u,1109234788u,233052862u,3811519976u},-0.1442f,0.5262f}, // Weapon_T6_2H_DaggersEpic_Poison (wpn_dagger_t6_epic_02)
}};
inline const CatalogDualWieldAsset* catalogDualWieldItem(uint32_t item){
 for(const auto& a:catalogDualWieldAssets){if(a.item==item)return &a;if(a.item>item)break;}return nullptr;
}
inline const CatalogDualWieldAsset* catalogDualWieldAsset(uint32_t item,const std::array<uint32_t,4>& guid){
 const auto* a=catalogDualWieldItem(item);return a&&a->guid==guid?a:nullptr;
}
}
