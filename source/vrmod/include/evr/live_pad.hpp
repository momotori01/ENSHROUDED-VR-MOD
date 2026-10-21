#pragma once
#include <evr/vertical_stick_mapping.hpp>

namespace evr::motion {
// Continuous VR controller -> one synthetic gamepad (2026-10-09, first real-device input, phase 1).
// Built on the existing stick chain: HeadGesture (temple D-pad, stick-click Start/Select) and
// VerticalStickMapping (gameplay right stick up -> L3, down -> B; menus keep the raw axis; the native
// quick wheel takes the raw right stick). This class only adds the face and grip buttons:
//
//   source           gameplay / quick wheel     menus and other native screens
//   right primary    A  (jump)                  A  (confirm)
//   right secondary  Y  (interact)              Y  (take - the loot's and the catch's "Y")
//   left primary     -                          B  (back / cancel)
//   left secondary   -                          X
//   right grip       RB (quick wheel)           RB (next tab)
//   left grip        -                          LB (previous tab)
// Menus since 2026-10-20 (user: "the game asks for Y - make Y the right B; close and cancel on the left A, as most VR
// games confirm with the right A and cancel with the left A"; before: right B = B, left A = X, left B = Y). With the
// menu pointer on the screen the right A is the mouse's left button and the left A its right one (hooks/menu_pointer.hpp).
// The native screens that aim at the world - the building modes and the summoning staff's adjustment (WorldUi, set by the
// hook from the game's screen) - keep the table the building controls are made on (controller_ui_fixture.hpp: right B = B
// opens the piece menu, left A = X builds, left B is taken for the piece menu, LT leaves building): A, B, X, Y, RB, LB.
// The left trigger in menus is the game's Y too (2026-10-20, user: "in menus too the left trigger as Y"); held across a
// change of screen it waits for its release (a pull that opened a loot window does not take from it).
//
// Sticks and triggers pass through (left stick move, right stick X turn, LT/RT analog); in play LT stays released (below).
// Not in phase 1: motion melee attacks, posture guard, shoulder/hip equip zones (they need body
// calibration on the real device), left B -> X (the user's spec item is pending: native X is the
// melee MainhandAction).
// A button is classified once at its press edge and keeps that target until it is released, so a
// button held while a menu closes (B back -> Y interact) never becomes a new press of another
// button. After an unavailable/stale frame or a session/origin/profile change, held buttons must be
// released before they act again.
struct LivePadState {
 uint16_t buttons=0;int16_t leftX=0,leftY=0,rightX=0,rightY=0;uint8_t leftTrigger=0,rightTrigger=0;
 bool operator==(const LivePadState& o)const{return buttons==o.buttons&&leftX==o.leftX&&leftY==o.leftY&&rightX==o.rightX&&rightY==o.rightY&&
  leftTrigger==o.leftTrigger&&rightTrigger==o.rightTrigger;}
 bool operator!=(const LivePadState& o)const{return !(*this==o);}
};
enum class LiveScreen {Unavailable,Gameplay,Overlay,Ui,WorldUi};
// From the native frontend input predicate: valid World/UI/entity, the normal HUD, and the auxiliary
// state (1 = native quick wheel).
inline LiveScreen liveScreen(bool valid,bool normalHud,uint8_t auxiliary){
 if(!valid)return LiveScreen::Unavailable;
 if(auxiliary==1)return LiveScreen::Overlay;
 return normalHud?LiveScreen::Gameplay:LiveScreen::Ui;
}
inline VerticalStickMode liveStickMode(LiveScreen s){
 switch(s){case LiveScreen::Gameplay:return VerticalStickMode::Gameplay;case LiveScreen::Overlay:return VerticalStickMode::NativeOverlay;
  case LiveScreen::Ui:case LiveScreen::WorldUi:return VerticalStickMode::NativeUi;default:return VerticalStickMode::Unavailable;}
}
class LivePadMapper {
public:
 static constexpr uint16_t A=0x1000,B=0x2000,X=0x4000,Y=0x8000,LB=0x0100,RB=0x0200;
 enum Source:unsigned {RightPrimary,RightSecondary,LeftPrimary,LeftSecondary,RightGrip,LeftGrip,SourceCount};
 static uint16_t target(LiveScreen s,unsigned source){
  static constexpr uint16_t play[SourceCount]{A,Y,0,0,RB,0};
  static constexpr uint16_t ui[SourceCount]{A,Y,B,X,RB,LB};
  static constexpr uint16_t world[SourceCount]{A,B,X,Y,RB,LB};
  if(source>=SourceCount||s==LiveScreen::Unavailable)return 0;
  return (s==LiveScreen::Ui?ui:s==LiveScreen::WorldUi?world:play)[source];
 }
private:
 struct Latch {bool held=false,mustRelease=true;uint16_t target=0;};
 bool motionQuickSlot_=false,leftInteract_=false,menuLeftY_=true,leftWaits_=false,leftGame_=false;LiveScreen leftScreen_=LiveScreen::Unavailable;
 // The menu's left trigger (2026-10-21, the user: "the inventory's sort does not work" - "sort with LT + LB"): the game
 // sorts with LT + X, and the left B is the menu's X. A pull decides when it is clear: LB pressed while pulled - the game's
 // LT with it (the sort); let go within menuTapNs alone - one Y of yPulseNs (quick move); still pulled after menuTapNs - Y
 // held from then (take all, the game's long Y). The sort's X comes sortLeadNs after its LT: with both in one poll the game
 // took the X alone and picked the item up (Mock game-20261021-155753).
 enum class MenuPull:uint8_t {Idle,Pending,Sort,Hold};MenuPull menuPull_=MenuPull::Idle;XrTime menuPullAt_=0,yPulseUntil_=0,sortAt_=0;
 static constexpr XrTime menuTapNs=400000000,yPulseNs=100000000,sortLeadNs=80000000;
 std::array<Latch,SourceCount> latch_{};
 uint64_t session_=0,origin_=0;XrPath leftProfile_=0,rightProfile_=0;
 static bool pressed(const HandFrame& h,unsigned s){
  const auto& r=h.hands[1];const auto& l=h.hands[0];
  switch(s){
   case RightPrimary:return r.primaryActive&&r.primaryPressed;
   case RightSecondary:return r.secondaryActive&&r.secondaryPressed;
   case LeftPrimary:return l.primaryActive&&l.primaryPressed;
   case LeftSecondary:return l.secondaryActive&&l.secondaryPressed;
   case RightGrip:return r.grabActive&&r.grab;
   case LeftGrip:return l.grabActive&&l.grab;
  }
  return false;
 }
 void interrupt(){for(auto& l:latch_){l.held=false;l.target=0;l.mustRelease=true;}leftInteract_=false;leftWaits_=true;menuPull_=MenuPull::Idle;yPulseUntil_=0;}
 static int16_t axis(float v){return int16_t(std::lround(std::clamp(v,-1.f,1.f)*32767));}
 static uint8_t trigger(float v){return uint8_t(std::lround(std::clamp(v,0.f,1.f)*255));}
public:
 // The left trigger is interacting (pulled past 85 %, until under 50 %).
 bool leftInteract()const{return leftInteract_;}
 // false: in menus the left trigger stays the game's LT (EVR_MENU_LT_Y=0).
 void menuLeftTriggerY(bool on){menuLeftY_=on;}
 // true: in play the left trigger is the game's own LT this frame (2026-10-20, user: "I cannot enter the hammer's building
 // mode - with the hammer the trigger"): the hook sets it while the hand holds a building hammer or a blueprint, whose
 // LT enters building mode.
 void leftTriggerGame(bool on){leftGame_=on;}
 // Mapping and frame identity come from VerticalStickMapping; invalid means a neutral pad.
 LivePadState update(LiveScreen screen,const VerticalStickResult& mapped,const HandFrame& hands,bool wieldGrip=false){
  LivePadState out;
  if(screen==LiveScreen::Unavailable||!mapped.valid||!hands.focused){interrupt();session_=origin_=0;return out;}
  if(session_&&(session_!=hands.sessionGeneration||origin_!=hands.originGeneration||
     leftProfile_!=hands.hands[0].profile||rightProfile_!=hands.hands[1].profile))interrupt();
  session_=hands.sessionGeneration;origin_=hands.originGeneration;leftProfile_=hands.hands[0].profile;rightProfile_=hands.hands[1].profile;
  out.buttons=mapped.pad.buttons;
  out.leftX=axis(mapped.pad.left.x);out.leftY=axis(mapped.pad.left.y);out.rightX=axis(mapped.pad.right.x);out.rightY=axis(mapped.pad.right.y);
  out.leftTrigger=trigger(mapped.pad.leftTrigger);out.rightTrigger=trigger(mapped.pad.rightTrigger);
  // In play the left trigger is not the game's LT (user, 2026-10-19 01:30: "the left trigger keeps switching to the ranged
  // weapon of Q, over and over"): LT is the game's aim, which takes out the ranged weapon; a finger resting on the
  // trigger crossed its threshold again and again. The bow's aim is pressed by the mod itself (bow_draw.hpp); menus keep LT.
  if(screen==LiveScreen::Gameplay&&!leftGame_)out.leftTrigger=0;   // (a building hammer in the hand: below)
  // The left trigger interacts (2026-10-20, VHVR equipment_tools 6.2: "the left trigger - an interaction when pressed, the
  // long one while it is held"; a guest pressed every other button to take a caught fish off the ground, and a revive is
  // the interaction held): the game's Y while it is pulled past 85 % (a finger resting on it stays below - the 10-19 01:30
  // complaint) until it comes back under 50 %; not while the left grip holds a bow, a shield or a two-handed weapon.
  // In menus as well since 2026-10-20 (header).
  const bool leftY=(screen==LiveScreen::Gameplay&&!leftGame_)||(screen==LiveScreen::Ui&&menuLeftY_);
  if(leftY){const float t=mapped.pad.leftTrigger;const auto& lh=hands.hands[0];out.leftTrigger=0;
   if(screen!=leftScreen_){leftScreen_=screen;if(leftInteract_||t>=.5f)leftWaits_=true;leftInteract_=false;}
   if(leftWaits_){if(t<.5f)leftWaits_=false;}
   else{const bool was=leftInteract_;leftInteract_=!(lh.grabActive&&lh.grab)&&(leftInteract_?t>=.5f:t>=.85f);
    if(screen!=LiveScreen::Ui){if(leftInteract_)out.buttons|=Y;}
    else{const XrTime now=hands.predictedTime;
     if(leftInteract_&&!was){menuPull_=MenuPull::Pending;menuPullAt_=now;}
     if(leftInteract_&&menuPull_==MenuPull::Pending){
      if(pressed(hands,LeftSecondary)){menuPull_=MenuPull::Sort;sortAt_=now;}else if(now-menuPullAt_>=menuTapNs)menuPull_=MenuPull::Hold;}
     if(!leftInteract_&&was){if(menuPull_==MenuPull::Pending)yPulseUntil_=now+yPulseNs;menuPull_=MenuPull::Idle;}
     if(menuPull_==MenuPull::Hold||(yPulseUntil_&&now<yPulseUntil_))out.buttons|=Y;
     if(menuPull_==MenuPull::Sort)out.leftTrigger=255;}}}
  else{leftInteract_=false;leftScreen_=screen;menuPull_=MenuPull::Idle;}
  for(unsigned s=0;s<SourceCount;s++){
   auto& l=latch_[s];const bool down=pressed(hands,s);
   if(!down){l.held=false;l.target=0;l.mustRelease=false;continue;}
   if(l.mustRelease)continue;
   // Quick slot by hand motion (quick_slot.hpp): outside menus the right grip is free for other uses and
   // the right B button belongs to that gesture.
   const bool freed=motionQuickSlot_&&screen!=LiveScreen::Ui&&screen!=LiveScreen::WorldUi&&(s==RightGrip||s==RightSecondary);
   if(!l.held){l.held=true;l.target=freed||(wieldGrip&&screen==LiveScreen::Gameplay&&s==RightGrip)?0:target(screen,s);}
   out.buttons|=l.target;
  }
  if(menuPull_==MenuPull::Sort&&hands.predictedTime-sortAt_<sortLeadNs)out.buttons&=uint16_t(~X);   // LT first
  return out;
 }
 void motionQuickSlot(bool enabled){motionQuickSlot_=enabled;}
 uint16_t heldTargets()const{uint16_t b=0;for(const auto& l:latch_)if(l.held)b|=l.target;return b;}
};
} // namespace evr::motion
