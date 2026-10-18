#pragma once
#include <cstdint>
namespace evr::game {
// Finite test-only native hotbar input. It observes selection, never writes it.
// Only DPAD LEFT/RIGHT are emitted. Unknown UI, root changes or timeout abort.
struct EquipmentObservation {bool valid=false;uint32_t root=0,item=0,child=0,handle=0;};
class ControllerEquipmentFixture {
public:
 enum class Phase {WaitConnection,Settle,PressPrevious,WaitOther,ObserveOther,PressNext,WaitOriginal,ObserveOriginal,Complete,Aborted};
 struct Result {Phase phase;uint16_t buttons=0;};
 explicit ControllerEquipmentFixture(uint32_t expected=0):expected_(expected){}
 Phase phase()const{return phase_;}
 uint32_t otherItem()const{return other_;}
 Result update(uint64_t now,bool connected,bool hud,const EquipmentObservation& o){
  if(phase_==Phase::Complete||phase_==Phase::Aborted)return {phase_,0};
  if(!started_){started_=true;begin_=last_=at_=now;}
  if(now<last_||now-begin_>12000||!expected_||!hud||!o.valid||!o.root)return abort();
  last_=now;
  if(root_&&root_!=o.root)return abort();
  if(phase_!=Phase::WaitConnection&&!connected)return abort();
  const bool observed=o.item&&o.child&&o.handle;
  switch(phase_){
   case Phase::WaitConnection:
    if(connected){if(!observed||o.item!=expected_)return abort();root_=o.root;next(Phase::Settle,now);}break;
   case Phase::Settle:
    if(!observed||o.item!=expected_)return abort();if(now-at_>=750)next(Phase::PressPrevious,now);break;
   case Phase::PressPrevious:if(now-at_>=120)next(Phase::WaitOther,now);break;
   case Phase::WaitOther:
    if(observed&&o.item!=expected_){other_=o.item;next(Phase::ObserveOther,now);}else if(now-at_>2500)return abort();break;
   case Phase::ObserveOther:
    if(!observed||o.item!=other_)return abort();if(now-at_>=1500)next(Phase::PressNext,now);break;
   case Phase::PressNext:if(now-at_>=120)next(Phase::WaitOriginal,now);break;
   case Phase::WaitOriginal:
    if(observed&&o.item==expected_)next(Phase::ObserveOriginal,now);else if(now-at_>2500)return abort();break;
   case Phase::ObserveOriginal:
    if(!observed||o.item!=expected_)return abort();if(now-at_>=750)next(Phase::Complete,now);break;
   default:break;
  }
  return {phase_,uint16_t(phase_==Phase::PressPrevious?4:phase_==Phase::PressNext?8:0)};
 }
private:
 Phase phase_=Phase::WaitConnection;uint32_t expected_=0,root_=0,other_=0;bool started_=false;uint64_t begin_=0,last_=0,at_=0;
 void next(Phase p,uint64_t now){phase_=p;at_=now;}
 Result abort(){phase_=Phase::Aborted;return {phase_,0};}
};
}
