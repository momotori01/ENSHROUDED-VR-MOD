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
// A d-pad picture as the right stick with the same direction (two letters and a badge, button_glyph.hpp stickLabel).
inline std::wstring rightStickFor(Pic pic){
    switch(pic){
    case Pic::DpadUp:return stickLabel(L"RS",StickMark::Up);
    case Pic::DpadDown:return stickLabel(L"RS",StickMark::Down);
    case Pic::DpadLeft:return stickLabel(L"RS",StickMark::Left);
    case Pic::DpadRight:return stickLabel(L"RS",StickMark::Right);
    case Pic::DpadVertical:return stickLabel(L"RS",StickMark::Vertical);
    case Pic::DpadHorizontal:return stickLabel(L"RS",StickMark::Horizontal);
    default:return stickLabel(L"RS",StickMark::None);
    }
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
        case Pic::FaceRight:return stickLabel(L"RS",StickMark::Down);          // dodge: the right stick down
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
        case Pic::ShoulderRightLower:return held==Held::Weapon?gripSwingLabel:held==Held::Wand?L"RG+RT":(held==Held::Bow||held==Held::Staff||held==Held::Nothing)?L"RT":
            held==Held::BuildTool?L"RG":L"";   // a piece in the hand: the right grip is the game's RT (controller_ui_fixture.hpp, category 6)
        // sprint / sneak: the right stick up - but a piece in the hand takes the right stick as the d-pad while the right grip is held,
        // so its "RT + L3" (the fine move) is the left stick's press (2026-10-26, user: "the keys shown when placing do not match")
        case Pic::LeftStickPress:return held==Held::BuildTool?stickLabel(L"LS",StickMark::Press):stickLabel(L"RS",StickMark::Up);
        case Pic::RightStickPress:return stickLabel(L"RS",StickMark::Press);   // the right stick's press (first person, "RT + R3" fine turn)
        // a piece in the hand: RT + d-pad is the right stick with the grip
        case Pic::Dpad:case Pic::DpadUp:case Pic::DpadDown:case Pic::DpadLeft:case Pic::DpadRight:case Pic::DpadVertical:case Pic::DpadHorizontal:
            return held==Held::BuildTool?rightStickFor(pic):L"";
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
    // The building screen (2026-10-26, user: "the keys shown when placing a piece do not match the VR buttons"): the HUD shows B and Y
    // after RT ("RT + B undo", "RT + Y sort") - RT is already shown as RG, so B and Y are the buttons alone (before: "RG+RB" after
    // "RG"); the d-pad (RT + d-pad nearer / farther / turn, RB + d-pad pages) is the right stick, with the right grip or the piece
    // menu held; LT (leave) is the VR LT; L3 (RT + L3, the fine move) the left stick's press.
    case Scene::Building:
        switch(pic){
        case Pic::FaceBottom:return RA;
        case Pic::FaceRight:return RB;                                          // RT + B: with the right grip
        case Pic::FaceLeft:return L"RT";
        case Pic::FaceTop:return LB;                                            // RT + Y: with the right grip
        case Pic::ShoulderLeftUpper:return L"LG";
        case Pic::ShoulderLeftLower:return L"LT";
        case Pic::ShoulderRightUpper:return LB;                                 // the piece menu (held)
        case Pic::ShoulderRightLower:return L"RG";                              // the game's modifier
        case Pic::LeftStickPress:return stickLabel(L"LS",StickMark::Press);
        case Pic::RightStickPress:return stickLabel(L"RS",StickMark::Press);
        case Pic::Dpad:case Pic::DpadUp:case Pic::DpadDown:case Pic::DpadLeft:case Pic::DpadRight:case Pic::DpadVertical:case Pic::DpadHorizontal:
            return rightStickFor(pic);
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
