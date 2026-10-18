#pragma once
#include <cstdint>
namespace evr::game {
// Diagnostic only. START pulses cannot move, attack, consume items or confirm
// destructive menu choices. Opt-in tab tests add only D-pad RIGHT and RB/LB
// inside the observed native InGameMenu. Altar diagnostic opens Y and cancels B
// only from FlameAltar12. Release immediately on state change; never confirm. Unknown UI or a changed player scope aborts.
// Tour (2026-10-06, VR menu check): from the opened tab LB x3 (to the first tab) then RB x5 (to the last), capturing
// every tab; tabs may report another non-gameplay screen than InGameMenu4, so only gameplay(1)/unknown(0) abort there.
class ControllerMenuFixture {
public:
 enum class Phase {WaitConnection,Settle,PressOpen,WaitOpen,ObserveOpen,PressClose,WaitClose,ObserveClosed,Complete,Aborted,PressSelectNext,ObserveSelection,PressNextTab,ObserveNextTab,PressPreviousTab,ObservePreviousTab,TourPress,TourObserve};
 explicit ControllerMenuFixture(bool navigate=false,bool altar=false,bool tour=false):navigate_(navigate),altar_(altar),tour_(tour){}
 static constexpr unsigned tourSteps=8; // LB,LB,LB,RB,RB,RB,RB,RB
 unsigned tourStep()const{return tourStep_;}
 struct Result {Phase phase;uint16_t buttons;uint32_t captureTag=0;};
 Result update(uint64_t now,bool connected,uint8_t screen,uint8_t auxiliary,bool valid){
  if(phase_==Phase::Complete||phase_==Phase::Aborted)return {phase_,0};
  if(!started_){started_=true;begin_=last_=phaseAt_=now;}
  const bool touring=phase_==Phase::TourPress||phase_==Phase::TourObserve;
  if(now<last_||now-begin_>(tour_?32000u:navigate_?20000u:9000u)||!valid||auxiliary||!(screen==1||screen==menuScreen()||(touring&&screen!=0)))return abort();
  last_=now;
  if(phase_!=Phase::WaitConnection&&!connected)return abort();
  switch(phase_){
   case Phase::WaitConnection:if(connected){if(screen!=1)return abort();next(Phase::Settle,now);}break;
   case Phase::Settle:if(screen!=1)return abort();if(now-phaseAt_>=500)next(Phase::PressOpen,now);break;
   case Phase::PressOpen:if((altar_&&screen==menuScreen())||now-phaseAt_>=120)next(Phase::WaitOpen,now);break;
   case Phase::WaitOpen:if(screen==menuScreen())next(Phase::ObserveOpen,now);break;
   case Phase::ObserveOpen:if(screen!=menuScreen())return abort();if(now-phaseAt_>=1500)next(tour_?Phase::TourPress:navigate_?Phase::PressSelectNext:Phase::PressClose,now);break;
   case Phase::TourPress:if(screen==1)return abort();if(now-phaseAt_>=120){capturedTour_=false;next(Phase::TourObserve,now);}break;
   case Phase::TourObserve:if(screen==1)return abort();if(now-phaseAt_>=1600){++tourStep_;next(tourStep_<tourSteps?Phase::TourPress:Phase::PressClose,now);}break;
   case Phase::PressSelectNext:if(screen!=menuScreen())return abort();if(now-phaseAt_>=120)next(Phase::ObserveSelection,now);break;
   case Phase::ObserveSelection:if(screen!=menuScreen())return abort();if(now-phaseAt_>=1500)next(Phase::PressNextTab,now);break;
   case Phase::PressNextTab:if(screen!=menuScreen())return abort();if(now-phaseAt_>=120)next(Phase::ObserveNextTab,now);break;
   case Phase::ObserveNextTab:if(screen!=menuScreen())return abort();if(now-phaseAt_>=1500)next(Phase::PressPreviousTab,now);break;
   case Phase::PressPreviousTab:if(screen!=menuScreen())return abort();if(now-phaseAt_>=120)next(Phase::ObservePreviousTab,now);break;
   case Phase::ObservePreviousTab:if(screen!=menuScreen())return abort();if(now-phaseAt_>=1500)next(Phase::PressClose,now);break;
   case Phase::PressClose:if((altar_&&screen==1)||now-phaseAt_>=120)next(Phase::WaitClose,now);break;
   case Phase::WaitClose:if(screen==1)next(Phase::ObserveClosed,now);break;
   case Phase::ObserveClosed:if(screen!=1)return abort();if(now-phaseAt_>=800)next(Phase::Complete,now);break;
   default:break;
  }
  uint32_t capture=0;
  if(phase_==Phase::ObserveOpen&&now-phaseAt_>=700&&!capturedOpen_){capture=1;capturedOpen_=true;}
  if(phase_==Phase::ObserveClosed&&now-phaseAt_>=300&&!capturedClosed_){capture=2;capturedClosed_=true;}
  if(phase_==Phase::ObserveSelection&&now-phaseAt_>=700&&!capturedSelection_){capture=3;capturedSelection_=true;}
  if(phase_==Phase::ObserveNextTab&&now-phaseAt_>=700&&!capturedNext_){capture=4;capturedNext_=true;}
  if(phase_==Phase::ObservePreviousTab&&now-phaseAt_>=700&&!capturedPrevious_){capture=5;capturedPrevious_=true;}
  if(phase_==Phase::TourObserve&&now-phaseAt_>=1000&&!capturedTour_){capture=3;capturedTour_=true;}
  uint16_t navigation=phase_==Phase::PressSelectNext?0x0008:phase_==Phase::PressNextTab?0x0200:phase_==Phase::PressPreviousTab?0x0100:
      phase_==Phase::TourPress?uint16_t(tourStep_<3?0x0100:0x0200):0;
  return {phase_,uint16_t(phase_==Phase::PressOpen?(altar_?0x8000:0x0010):phase_==Phase::PressClose?(altar_?0x2000:0x0010):navigation),capture};
 }
 Phase phase()const{return phase_;}
private:
 Phase phase_=Phase::WaitConnection;bool started_=false,capturedOpen_=false,capturedClosed_=false,navigate_=false,capturedSelection_=false,capturedNext_=false,capturedPrevious_=false,altar_=false,tour_=false,capturedTour_=false;uint64_t begin_=0,last_=0,phaseAt_=0;unsigned tourStep_=0;
 uint8_t menuScreen()const{return altar_?12:4;}
 void next(Phase p,uint64_t now){phase_=p;phaseAt_=now;}
 Result abort(){phase_=Phase::Aborted;return {phase_,0};}
};
}
