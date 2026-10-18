#pragma once
#include <array>
#include <cstdint>
namespace evr::motion {
// Generated from the read-only local game data; does not certify live handling.
// Generator: docs/analysis/held_1_0_20261017/generate_held_catalogs_1_0.py (1.0 game data ^/game38/branches/release_1.0)
// Shields with a visual model (item category 1, item slot 18). Every row attaches to native slot 12 with
// the identity transform, as the shield measured in game (691231505).
struct CatalogShieldAsset {uint32_t item;std::array<uint32_t,4> guid;};
inline constexpr std::array<CatalogShieldAsset,58> catalogShieldAssets{{
 {40007242u,{1171625637u,1174786956u,2187717801u,2660578971u}}, // Shield_T2_DPS (wpn_shield_01)
 {53830276u,{3108713566u,1193110865u,3280210322u,2997815635u}}, // Shield_T7_DPS_Loot (wpn_shield_t7_02)
 {255353296u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T7_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {273247586u,{4037393232u,1274534986u,578422930u,897210844u}}, // Shield_T7_Rogue_Loot (wpn_shield_t7_03)
 {300032798u,{1106893973u,1236106350u,2097819313u,3739907311u}}, // Shield_T1_DPS_Loot (wpn_shield_09)
 {349263893u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Craft_Shield_T7_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {408308370u,{1648560777u,1277943317u,268830863u,3734813673u}}, // Shield_T5_Rogue_Loot (wpn_shield_mb_03)
 {419035885u,{4151751848u,1195619463u,1196579477u,2582074309u}}, // Shield_T2_Tank_Loot (wpn_shield_04)
 {454512717u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T1_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {514520429u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T5_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {634089750u,{2437095308u,1288433682u,659774647u,1101342440u}}, // Shield_T4_DPS (wpn_shield_12)
 {691231505u,{1602120618u,1327021138u,2095175581u,1329450576u}}, // Shield_T0_Wooden (wpn_shield_00)
 {704475806u,{1060362669u,1223873041u,1950604437u,3488221580u}}, // Shield_T6_DPS_Loot (wpn_shield_t6_02)
 {870935206u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Craft_Shield_T3_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {913087809u,{3205974189u,1323349152u,2220702653u,1957785851u}}, // Shield_T3_Tank_Loot (wpn_shield_11)
 {1237193461u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T3_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {1294112531u,{3531383816u,1240451682u,1115522997u,2650154617u}}, // Shield_T5_DPS (wpn_shield_mb_02)
 {1334950192u,{3205974189u,1323349152u,2220702653u,1957785851u}}, // Shield_T3_Tank (wpn_shield_11)
 {1349555194u,{2081389258u,1086606663u,1033026952u,683948148u}}, // Shield_T3_DPS_Loot (wpn_shield_10)
 {1474807415u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Craft_Shield_T6_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {1630326106u,{1315225754u,1094749559u,2372217749u,1163784534u}}, // Shield_T4_Rogue_Loot (wpn_shield_06)
 {1777113935u,{943971158u,1127753713u,1161435021u,3284305293u}}, // Shield_T6_Tank (wpn_shield_t6_01)
 {1828068654u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Shield_T1_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {1900300234u,{943971158u,1127753713u,1161435021u,3284305293u}}, // Shield_T6_Tank_Loot (wpn_shield_t6_01)
 {1907066224u,{4037393232u,1274534986u,578422930u,897210844u}}, // Shield_T7_Rogue (wpn_shield_t7_03)
 {1942545282u,{2317143075u,1220695502u,462010528u,321960333u}}, // Shield_T5_Tank_Loot (wpn_shield_mb_01)
 {1974697806u,{3225118843u,1196887955u,2286439840u,3669992657u}}, // Shield_T3_Rogue_Loot (wpn_shield_13)
 {2053916217u,{1688795220u,1333691730u,2644129960u,2113271460u}}, // Shield_T1-TX_Enemy_Scavenger_Weakling (wpn_shield_scavenger_weakling_01)
 {2108531401u,{3225118843u,1196887955u,2286439840u,3669992657u}}, // Shield_T3_Rogue (wpn_shield_13)
 {2159741777u,{4151751848u,1195619463u,1196579477u,2582074309u}}, // Shield_T2_Tank (wpn_shield_04)
 {2164917554u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T4_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {2165424296u,{591576286u,1190171457u,3862850456u,2787654448u}}, // Shield_TX_Skull_HollowHall (wpn_shield_skull_01)
 {2363054959u,{2081389258u,1086606663u,1033026952u,683948148u}}, // Shield_T3_DPS (wpn_shield_10)
 {2455654433u,{3108713566u,1193110865u,3280210322u,2997815635u}}, // Shield_T7_DPS (wpn_shield_t7_02)
 {2566805157u,{1106893973u,1236106350u,2097819313u,3739907311u}}, // Shield_T1_DPS (wpn_shield_09)
 {2584194594u,{2583686609u,1173748180u,4019458488u,255379809u}}, // Shield_T4_Tank (wpn_shield_02)
 {2644725310u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Craft_Shield_T4_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {2659868410u,{1247193057u,1214663098u,4023462828u,291234210u}}, // Shield_T6_Rogue (wpn_shield_t6_04)
 {2828262065u,{1315225754u,1094749559u,2372217749u,1163784534u}}, // Shield_T4_Rogue (wpn_shield_06)
 {2863750697u,{1060362669u,1223873041u,1950604437u,3488221580u}}, // Shield_T6_DPS (wpn_shield_t6_02)
 {2903659724u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Craft_Shield_T5_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {2917228480u,{1648560777u,1277943317u,268830863u,3734813673u}}, // Shield_T5_Rogue (wpn_shield_mb_03)
 {2966845440u,{45671594u,1235510396u,1355538583u,3320388992u}}, // Shield_T7_Tank_Loot (wpn_shield_t7_01)
 {2972800174u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Craft_Shield_T2_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {3056575447u,{2583686609u,1173748180u,4019458488u,255379809u}}, // Shield_T4_Tank_Loot (wpn_shield_02)
 {3062146687u,{2317143075u,1220695502u,462010528u,321960333u}}, // Shield_T5_Tank (wpn_shield_mb_01)
 {3190651700u,{114173210u,1073948255u,3006705810u,1607721179u}}, // Shield_T6-TX_Enemy_Lizard_Weakling (wpn_shield_lizard_weakling_01)
 {3395268278u,{3531383816u,1240451682u,1115522997u,2650154617u}}, // Shield_T5_DPS_Loot (wpn_shield_mb_02)
 {3506644433u,{1247193057u,1214663098u,4023462828u,291234210u}}, // Shield_T6_Rogue_Loot (wpn_shield_t6_04)
 {3632234844u,{2437095308u,1288433682u,659774647u,1101342440u}}, // Shield_T4_DPS_Loot (wpn_shield_12)
 {3738285842u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T2_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {3744513726u,{1171625637u,1174786956u,2187717801u,2660578971u}}, // Shield_T2_DPS_Loot (wpn_shield_01)
 {3805706024u,{2638408417u,1312724507u,3090282125u,2001410960u}}, // Shield_TX_Enemy_Fogger_Knight (wpn_shield_fogger_knight_01)
 {3928873510u,{45671594u,1235510396u,1355538583u,3320388992u}}, // Shield_T7_Tank (wpn_shield_t7_01)
 {3957855784u,{632449306u,1337089112u,2744206980u,4292806698u}}, // Craft_Shield_T6_Lore_ShieldEpic_carpenter_hint06 (wpn_shield_08)
 {3981836644u,{4274070832u,1301155669u,3147724457u,736703355u}}, // Shield_TX_Enemy_Fogger (wpn_shield_05)
 {4004201468u,{1527045799u,1155771192u,2446994067u,693331776u}}, // Shield_T2_Lore_ShieldEpic_Ministory_17 (wpn_shield_03)
 {4213879490u,{3829411593u,1229378058u,2196329902u,2608498158u}}, // Shield_T0-TX_Enemy_Fogger_Depleted (wpn_shield_07)
}};
inline const CatalogShieldAsset* catalogShieldAsset(uint32_t item,const std::array<uint32_t,4>& guid){
 for(const auto& a:catalogShieldAssets){if(a.item==item)return a.guid==guid?&a:nullptr;if(a.item>item)break;}return nullptr;
}
}
