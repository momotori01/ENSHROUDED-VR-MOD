#pragma once
#include <cstdint>
namespace evr::game {
// While the game is paused (the pause menu and the settings, UI screen 2, alone in the world) its simulation stands
// still: the observers that sample the game per tick bring nothing new, and what they saw last stays true until the
// pause ends. Counted on the wall clock those samples became "too old" after half a second, and what was shown from
// them went away: the hands and the held weapon vanished behind the pause menu (Mock run game-20261013-041321,
// frames 529..1002: motion_visual_dormant "tracking_or_no_calibrated_appearance").
// So a sample's age is counted on a clock that stands still from the moment the pause began - and for a short while
// after it ended, until the observers have run again. A sample that was already too old when the pause began stays
// too old; with other players in the world the game does not stand still, the observers go on and the first rule
// (the plain age) holds as before.
struct PauseClock {
 uint64_t pausedAt=0,resumedAt=0;bool paused=false;
 void update(bool nowPaused,uint64_t now){
  if(nowPaused&&!paused)pausedAt=now;else if(!nowPaused&&paused)resumedAt=now;
  paused=nowPaused;
 }
 // The clock stands still: during the pause and for `grace` ms after it.
 bool still(uint64_t now,uint64_t grace)const{return pausedAt&&(paused||(resumedAt&&now>=resumedAt&&now-resumedAt<=grace));}
 // A sample taken at `taken` (ms, 0: never) is at most `limit` ms old.
 bool fresh(uint64_t taken,uint64_t now,uint64_t limit,uint64_t grace=500)const{
  if(!taken||now<taken)return false;
  if(now-taken<=limit)return true;
  return still(now,grace)&&(taken>=pausedAt||pausedAt-taken<=limit);
 }
};
}
