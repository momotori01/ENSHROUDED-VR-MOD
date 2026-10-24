#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace evr::motion {
// The tools whose own mode the game's LT enters, as a building hammer's building mode (2026-10-24, user after the headset:
// "the summoning staff does not summon - LT does not enter its mode as with the building hammer"): the summoning staff - one
// item for each NPC (1.0 data, template Visuals_Tool_NPC_Summon, category 6 BuildTools) - and the screwdriver
// (Tool_Building_Screwdriver, its screen 33 next to the summoning staff's 37). From docs/analysis/held_1_0_20261017/held-models.json.
inline constexpr std::array<uint32_t,30> leftTriggerModeTools{{
    279729468u, // NPC_Assistant_Crafter_bard_01_ChesterLoft
    321292257u, // NPC_Townsfolk_Male_Villager_05_RobertGibson
    387784553u, // NPC_Assistant_Crafter_Blacksmith_01_RochelleAnders
    786532879u, // NPC_Crafter_Blacksmith01
    841170834u, // NPC_Townsfolk_Female_Villager_01_HannahReed
    1004393385u, // NPC_Crafter_Farmer01
    1055693550u, // NPC_Townsfolk_Female_Villager_03_GabrielleFairburn
    1100674176u, // NPC_Crafter_Bard01
    1149463877u, // NPC_Crafter_Carpenter01
    1182285312u, // NpcSummoner_Intro
    1209217157u, // NPC_Assistant_Crafter_Farmer_01_Fenrig
    1240116813u, // NPC_Townsfolk_Female_Villager_02_MarianneReed
    1340233313u, // NPC_Crafter_Hunter01
    1607688361u, // NPC_Townsfolk_Male_Villager_01_GuntherGaleforth
    1689944669u, // NPC_Townsfolk_Male_Villager_06_HugoVanderhall
    1710067644u, // NPC_Townsfolk_Female_Villager_06_LoraineCrim_AncientResearcher
    2159713419u, // NPC_Townsfolk_Male_Villager_02_RamoneJones
    2202966376u, // NPC_Assistant_Crafter_Alchemy_01_PauletteElovere
    2205178126u, // NPC_Assistant_Crafter_Carpenter_01_BraelynHawthorn
    2231489185u, // Tool_Building_Screwdriver
    2375735512u, // NPC_Townsfolk_Female_Villager_05_name_chineseNewYear_Trader
    2684532648u, // NPC_Townsfolk_Male_Villager_08_fisher_Trader
    2801667479u, // NPC_Townsfolk_Male_Villager_03_OscarGodwin
    3298879737u, // NPC_Townsfolk_Female_Villager_04_KaoriConway
    3721320872u, // NPC_Crafter_Barber01
    3764330463u, // NPC_Crafter_CryptKeeper01
    3944857827u, // NPC_Townsfolk_Male_Villager_04_AntonGodwin
    3954545655u, // NPC_Assistant_Crafter_cryptKeeper_01_namelessOne
    3991532476u, // NPC_Assistant_Crafter_Hunter_01_GalenYager
    4146744324u, // NPC_Crafter_Alchemist01
}};
inline bool leftTriggerModeTool(uint32_t item){return std::binary_search(leftTriggerModeTools.begin(),leftTriggerModeTools.end(),item);}
} // namespace evr::motion
