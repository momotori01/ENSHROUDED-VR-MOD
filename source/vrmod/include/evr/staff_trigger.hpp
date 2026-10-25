#pragma once
#include <cmath>
#include <cstdint>
namespace evr::motion {
// The staff's right trigger in motion VR (2026-10-25, user: "now RT charges and, once charged, fires by itself - make it fire
// when the trigger is let go after the charge, as LT's charge and then RT does; with the staff LT charges only, LT's charge is
// fired by RT too; then nothing goes off by accident and Y or the D-pad can switch the spells; take care the bow's quick
// equip does not fire by RT"). The game's own staff: RT held charges and casts when full; LT held aims and charges
// (AimAndCharge) and RT then casts. Here the trigger held is the game's LT, and its release the game's RT for tapMs with LT
// still held, and LT on for aimAfterMs after the tap - the cast. The game's RT never goes alone, so the staff does not cast
// while the trigger is held. The game takes RT's press and release as its "shoot tap" when RT goes up (Mock 20261025-140606:
// PlayerInput bit 52 RangedCombatShoot_Tap the frame after; with LT let go at the same time the weapon was holstered already,
// state 0, and nothing was cast): LT stays a while after the tap, as for the bow (bow_draw.hpp: tapMs 80, aimMs 500).
// ready: the staff is in the hand on the play screen and nothing else owns the trigger (the bow taken from the shoulder, a
// head gesture, a menu). A pull that began while not ready is not the staff's until it is let go; a release while not ready,
// or a staff no longer ready during the cast, casts nothing (the bow's quick equip: the game still holds the staff for a
// moment while the trigger draws the shoulder's bow).
struct StaffTrigger {
 struct Out {bool leftTrigger=false,rightTrigger=false,cast=false,cancelled=false;};
 float on=.8f,off=.2f;uint64_t tapMs=100,aimAfterMs=400;
 Out update(float trigger,bool triggerActive,bool ready,uint64_t now){
  Out o;const bool was=pressed;
  // the trigger not read (the controller lost): as not ready - nothing is cast by it
  if(!triggerActive||!std::isfinite(trigger)){pressed=false;ready=false;}else if(trigger>=on)pressed=true;else if(trigger<=off)pressed=false;
  if(!ready){if(owned)o.cancelled=true;owned=false;castUntil=0;return o;}
  if(pressed&&!was)owned=true;
  if(owned&&pressed)o.leftTrigger=true;
  if(owned&&!pressed&&was){owned=false;castAt=now;castUntil=now+tapMs+aimAfterMs;o.cast=true;}
  if(castUntil&&now<castUntil){o.leftTrigger=true;o.rightTrigger=now<castAt+tapMs;}else castUntil=0;
  return o;
 }
 bool held()const{return pressed;}bool charging()const{return owned&&pressed;}bool casting()const{return castUntil!=0;}
private:
 bool pressed=false,owned=false;uint64_t castAt=0,castUntil=0;
};
} // namespace evr::motion
