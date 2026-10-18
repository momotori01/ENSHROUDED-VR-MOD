#pragma once
#include <array>
#include <cstdint>
namespace evr::motion {
// Generated from the read-only audit of every hand-held item of the game data; does not certify live handling.
// Generator: docs/analysis/held_1_0_20261017/generate_held_catalogs_1_0.py (1.0 game data ^/game38/branches/release_1.0)
// Items thrown with the attack button (weapon type Throwable, 12; item category Weapons, equipment slot 4).
// None has a model for the hand; the game spawns the projectile on the throw.
inline constexpr std::array<uint32_t,13> catalogThrowables{{
 305542871u, // Consumable_Necromancer_Grenade_T4
 1105258830u, // Consumable_Necromancer_Grenade_T3
 1462961786u, // Consumable_Grenade_Fire_T5
 1529443326u, // Consumable_Water_Bomb
 1567174048u, // Consumable_Necromancer_Grenade_T5
 1596935538u, // Consumable_Necromancer_Grenade_T6
 1614998716u, // Consumable_Grenade_Explosive_PowderBall_T4
 2283229289u, // Consumable_Grenade_Explosive_PowderBall_T1
 3124613974u, // Consumable_Necromancer_Grenade_T2
 3145568607u, // _Consumable_ShroudPheromone_Grenade_T3
 3870359442u, // Consumable_CritterCrawler_Bomb_Grenade_T5
 4084762910u, // Consumable_Toy_Grenade_Snowball_T5
 4233298973u, // Consumable_Grenade_Explosive_PowderBall_T3
}};
inline bool catalogThrowable(uint32_t item){for(const auto a:catalogThrowables){if(a==item)return true;if(a>item)break;}return false;}
}
