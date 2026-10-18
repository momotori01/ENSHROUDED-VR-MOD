#pragma once
#include <cstdint>
namespace evr::game {
// Finite test-only native LB hold/release. Uses the game's normal input path;
// never writes block state, stamina, parry targets or attack sequences.
struct GuardObservation {bool valid=false;uint32_t root=0,item=0;bool canBlock=false,blocking=false;uint64_t digital=0,binding=0;bool attacking=false;};
class ControllerGuardFixture {
public:
 enum class Phase {WaitConnection,Settle,Hold,Release,Complete,Aborted};
 struct Result {Phase phase;uint16_t buttons=0;};
 Phase phase()const{return phase_;}unsigned holds()const{return holds_;}
 Result update(uint64_t now,bool connected,bool hud,const GuardObservation& o){
  if(phase_==Phase::Complete||phase_==Phase::Aborted)return {phase_,0};
  if(!started_){started_=true;begin_=last_=at_=now;}
  if(now<last_||now-begin_>12000||!hud||!o.valid||!o.root||!o.item)return abort();
  last_=now;
  if(root_&&(root_!=o.root||item_!=o.item))return abort();
  if(phase_!=Phase::WaitConnection&&!connected)return abort();
  const bool blockInput=(o.digital&(1ull<<30))!=0;
  // PresetA LB also sets SecondaryBuildingAction4 in the native packet.
  // Permit only that observed paired alias under the caller's fresh melee/HUD
  // guard. A standalone building bit, attack or movement still aborts.
  if(o.digital!=0&&o.digital!=(1ull<<30)&&o.digital!=((1ull<<30)|(1ull<<4)))return abort();
  switch(phase_){
   case Phase::WaitConnection:
    if(connected){if(!o.canBlock||o.blocking||blockInput)return abort();root_=o.root;item_=o.item;next(Phase::Settle,now);}break;
   case Phase::Settle:
    if(!o.canBlock||o.blocking||blockInput)return abort();if(now-at_>=750){seen_=false;next(Phase::Hold,now);}break;
   case Phase::Hold:
    seen_|=o.blocking&&blockInput;
    if(now-at_>=1200){if(!seen_)return abort();++holds_;seen_=false;next(Phase::Release,now);}break;
   case Phase::Release:
    seen_|=!o.blocking&&!blockInput;
    if(now-at_>=1000){if(!seen_||o.blocking||blockInput)return abort();if(holds_==2)next(Phase::Complete,now);else {seen_=false;next(Phase::Hold,now);}}break;
   default:break;
  }
  return {phase_,uint16_t(phase_==Phase::Hold?0x100:0)};
 }
private:
 Phase phase_=Phase::WaitConnection;uint32_t root_=0,item_=0;unsigned holds_=0;bool started_=false,seen_=false;uint64_t begin_=0,last_=0,at_=0;
 void next(Phase p,uint64_t now){phase_=p;at_=now;}
 Result abort(){phase_=Phase::Aborted;return {phase_,0};}
};
}
