#pragma once
#include <array>
#include <cstdint>
namespace evr::motion {
// Generated from the 1.0 game data (bows_arrows.json); does not certify live handling.
// Generator: docs/analysis/arrow_flight_skills_20261021/generate_bow_speed_catalog.py
// Source SHA256: c3c2e0d0b0d369802007e3e6eda4efc6351d5d03657472547ce2a1cb77bfcdbb
// Game data: 1269579|^/game38/branches/release_1.0|2026-10-16T12:58:37.402390Z
// The speed an arrow leaves each bow at (m/s, along the shot): the bow's placedEntityLocalSpawnVelocity z.
struct BowSpeed {uint32_t item;float speed;};
inline constexpr std::array<BowSpeed,62> catalogBowSpeeds{{
 {101106603u,90.0f}, // Weapon_T7_2H_Lore_Bow_drakHeavy
 {327157552u,90.0f}, // Weapon_T1_2H_Bow_02
 {375831467u,100.0f}, // Weapon_T3_2H_Bow_02
 {457119037u,90.0f}, // Weapon_T4_2H_BowEpic_01
 {463074629u,90.0f}, // Craft_Weapon_T4_2H_Lore_BowEpic_ministory_19
 {476811263u,80.0f}, // Weapon_T1_2H_BowEpic_01
 {486021928u,90.0f}, // Craft_Weapon_T4_2H_BowEpic_chineseNewYear_01
 {526254228u,90.0f}, // Craft_Weapon_T6_2H_Lore_BowEpic_ministory_39
 {633590787u,100.0f}, // Weapon_T5_2H_Bow_01
 {689240895u,90.0f}, // Craft_Weapon_T5_2H_Lore_BowEpic_ministory_39
 {692270451u,90.0f}, // Weapon_T1_2H_BowEpic_02
 {735111003u,90.0f}, // Weapon_T2-TX_2H_Bow_Enemy_Fogger_Ranger_Elite
 {767990996u,90.0f}, // Weapon_T7_2H_BowEpic_02
 {773245753u,100.0f}, // Weapon_T7_2H_Bow_02
 {852156015u,90.0f}, // Craft_Weapon_T6_2H_Lore_BowEpic_Ministory_15
 {886466633u,90.0f}, // Weapon_TX_2H_Bow_FogNest
 {902475103u,90.0f}, // Craft_Weapon_T5_2H_Lore_BowEpic_ministory_19
 {944519165u,90.0f}, // Weapon_T2_2H_BowEpic_02
 {957618427u,100.0f}, // Weapon_T6_2H_Bow_02
 {1054611658u,90.0f}, // Weapon_T4_2H_Lore_BowEpic_ministory_19
 {1065913662u,100.0f}, // Weapon_T2_2H_Bow_02
 {1113531582u,90.0f}, // Weapon_T3_2H_BowEpic_chineseNewYear_01
 {1136608237u,90.0f}, // Weapon_T7_2H_BowEpic_01
 {1164879675u,90.0f}, // Craft_Weapon_T3_2H_BowEpic_chineseNewYear_01
 {1207792305u,90.0f}, // Weapon_T6_2H_BowEpic_02
 {1278504043u,90.0f}, // Weapon_T7_2H_Lore_Bow_ministory_03
 {1286173021u,90.0f}, // Weapon_T5_2H_Lore_BowEpic_ministory_39
 {1454083076u,90.0f}, // Craft_Weapon_T5_2H_Lore_BowEpic_Ministory_15
 {1457307757u,90.0f}, // Weapon_T4_2H_BowEpic_02
 {1482081791u,100.0f}, // Weapon_T1-TX_2H_Bow_Enemy_Fogger_Ranger
 {1640849157u,100.0f}, // Weapon_T4_2H_Bow_01
 {2178370335u,90.0f}, // Weapon_T5_2H_BowEpic_02
 {2187973877u,90.0f}, // Craft_Weapon_T7_2H_Lore_Bow_ministory_03
 {2237665335u,90.0f}, // Weapon_T3_2H_BowEpic_01
 {2242220014u,80.0f}, // Weapon_T1_2H_BowEpic_03
 {2296172399u,90.0f}, // Weapon_T2_2H_BowEpic_01
 {2490862466u,100.0f}, // Weapon_T4_2H_Bow_02
 {2517864696u,90.0f}, // Craft_Weapon_T3_2H_Lore_BowEpic_Ministory_15
 {2664238131u,90.0f}, // Weapon_T2_2H_Lore_BowEpic_Ministory_15
 {2676513257u,90.0f}, // Craft_Weapon_T7_2H_Lore_BowEpic_Ministory_15
 {2813408350u,90.0f}, // Craft_Weapon_T5_2H_BowEpic_chineseNewYear_01
 {2878180661u,100.0f}, // Weapon_T5_2H_Bow_02
 {2901533456u,100.0f}, // Weapon_T3_2H_Bow_01
 {3188079984u,100.0f}, // Weapon_T7_2H_Bow_01
 {3226031787u,90.0f}, // Craft_Weapon_T7_2H_BowEpic_chineseNewYear_01
 {3308817829u,90.0f}, // Weapon_T2-TX_2H_Bow_Enemy_Fogger_Ranger_Armored
 {3467068456u,100.0f}, // Weapon_T2_2H_Bow_01
 {3484636804u,100.0f}, // Weapon_T6_2H_Bow_01
 {3488008219u,90.0f}, // Craft_Weapon_T6_2H_BowEpic_chineseNewYear_01
 {3527909057u,75.0f}, // Weapon_T0_2H_Bow_Improvised_Craft
 {3540633303u,80.0f}, // Weapon_T1_2H_Bow_03
 {3578254606u,90.0f}, // Craft_Weapon_T6_2H_Lore_BowEpic_ministory_19
 {3597494234u,90.0f}, // Weapon_T6_2H_BowEpic_01
 {3626685627u,90.0f}, // Craft_Weapon_T7_2H_Lore_BowEpic_ministory_19
 {3854011426u,90.0f}, // Craft_Weapon_T7_2H_Lore_Bow_drakHeavy
 {3865285429u,90.0f}, // Craft_Weapon_T7_2H_Lore_BowEpic_ministory_39
 {3881967409u,90.0f}, // Weapon_T1_2H_Bow_01
 {3886257993u,90.0f}, // Craft_Weapon_T2_2H_Lore_BowEpic_Ministory_15
 {3971823801u,90.0f}, // Weapon_T5_2H_BowEpic_01
 {4016669731u,90.0f}, // Craft_Weapon_T4_2H_Lore_BowEpic_Ministory_15
 {4095266738u,90.0f}, // Weapon_TX_2H_Bow_HollowHall
 {4203062243u,90.0f}, // Weapon_T3_2H_BowEpic_02
}};
// 0 for an item not in the table.
inline float catalogBowSpeed(uint32_t item){for(const auto& b:catalogBowSpeeds){if(b.item==item)return b.speed;if(b.item>item)break;}return 0;}
}
