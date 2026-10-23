#pragma once
// Which VR input a game button picture stands for, by the mod's own state (2026-10-21, the user: "every button label for
// VR: [Y] Pick up becomes [LT] Pick up"; "LT RT LA LB LG RG, Index and Quest told apart" - Quest's face buttons are A B
// (right) and X Y (left); START, BACK and what only the hand-at-the-temple gesture gives keep the game's picture; what is
// done by swinging alone (a weapon, the wand) shows a swing). The reverse map of the mod's input:
// docs/analysis/vr_button_glyphs_20261021/mapping.md (the star of each row is the input shown). An empty label keeps the
// game's picture.
#include <evr/button_glyph_catalog.hpp>
#include <evr/button_glyph.hpp>
#include <string>
namespace evr::glyph {
enum class Family : uint8_t {Index,Quest};
// Play: the HUD (screen 1, auxiliary 0 or 3); Wheel: the game's quick wheel (auxiliary 1); Menu: other screens and the
// death screen; Building: the building screen (29); WorldMenu: the screwdriver's and the summoning staff's (33, 37).
enum class Scene : uint8_t {Play,Wheel,Menu,Building,WorldMenu};
// What the hands hold, as far as the play's mapping differs (controller_ui_fixture.hpp, mapping.md 2.2).
enum class Held : uint8_t {Nothing,Weapon,Wand,Staff,Bow,Throwable,Tool,Fishing,BuildTool,Hammer};
inline std::wstring face(Family f,bool right,bool primary){
    if(f==Family::Quest)return right?(primary?L"A":L"B"):(primary?L"X":L"Y");
    return std::wstring(right?L"R":L"L")+(primary?L"A":L"B");
}
// The label that is the game's own left stick press picture (the spare picture's, for the quick slot change).
inline const std::wstring originalLeftPress=L"<original:LeftStickPress>";
inline std::wstring label(Pic pic,Family f,Scene scene,Held held){
    if(pic==Pic::SpareLeftPress)return originalLeftPress;   // in every scene (hooks/glyph_choice_observer.hpp hands it out)
    const auto RA=face(f,true,true),RB=face(f,true,false),LA=face(f,false,true),LB=face(f,false,false);
    const bool tool=held==Held::Tool||held==Held::Fishing||held==Held::BuildTool||held==Held::Hammer;
    switch(scene){
    case Scene::Play:
        switch(pic){
        case Pic::FaceBottom:return RA;                                         // jump
        case Pic::FaceRight:return L"RS▼";                                 // dodge: the right stick down
        case Pic::FaceTop:return held==Held::Hammer?RB:L"LT";                   // interact (the hammer keeps LT: its short RB)
        case Pic::FaceLeft:                                                     // attack / use
            if(held==Held::Weapon||held==Held::Nothing||held==Held::Wand||held==Held::Throwable)return swingLabel;
            if(tool)return L"RT";
            return L"";
        case Pic::ShoulderLeftUpper:return tool?LB:L"LG";                       // guard; a tool's second action
        case Pic::ShoulderRightUpper:return RB;                                 // the action bar wheel (held)
        case Pic::ShoulderLeftLower:return held==Held::Bow?L"RT":held==Held::Hammer?L"LT":L"";   // the bow's draw: the trigger at the string
        // the game's RT: a weapon's and the wand's special ability (2026-10-23: a melee weapon's by a swing with the right grip
        // held since 10-21, the wand's by the right grip and the trigger), the bow's and the staff's shot
        case Pic::ShoulderRightLower:return held==Held::Weapon?gripSwingLabel:held==Held::Wand?L"RG+RT":(held==Held::Bow||held==Held::Staff||held==Held::Nothing)?L"RT":L"";
        case Pic::LeftStickPress:return L"RS▲";                            // sprint / sneak: the right stick up
        default:return L"";
        }
    case Scene::Wheel:
        switch(pic){
        case Pic::FaceBottom:return RA;
        case Pic::ShoulderRightUpper:return RB;
        default:return L"";
        }
    case Scene::Menu:
        switch(pic){
        case Pic::FaceBottom:return RA;
        case Pic::FaceRight:return LA;
        case Pic::FaceLeft:return LB;
        case Pic::FaceTop:return RB;
        case Pic::ShoulderLeftUpper:return L"LG";
        case Pic::ShoulderRightUpper:return L"RG";
        default:return L"";
        }
    case Scene::Building:
        switch(pic){
        case Pic::FaceBottom:return RA;
        case Pic::FaceRight:return L"RG+"+RB;
        case Pic::FaceLeft:return L"RT";
        case Pic::FaceTop:return L"RG+"+LB;
        case Pic::ShoulderLeftUpper:return L"LG";
        case Pic::ShoulderRightUpper:return LB;                                 // the piece menu (held)
        case Pic::ShoulderRightLower:return L"RG";                              // the game's modifier
        default:return L"";
        }
    case Scene::WorldMenu:
        switch(pic){
        case Pic::FaceBottom:return RA;
        case Pic::FaceRight:return RB;
        case Pic::FaceLeft:return LA;
        case Pic::FaceTop:return LB;
        case Pic::ShoulderLeftUpper:return L"LG";
        case Pic::ShoulderRightUpper:return L"RG";
        default:return L"";
        }
    }
    return L"";
}
}
