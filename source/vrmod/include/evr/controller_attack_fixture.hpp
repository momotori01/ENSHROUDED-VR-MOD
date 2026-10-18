#pragma once
#include <cstdint>
namespace evr::game {
struct AttackObservation {uint32_t entity=0,item=0;bool valid=false,active=false;uint64_t activeSamples=0;int64_t triggerTime=0;};
// One native button/trigger pulse, never a combat implementation or an inferred cooldown.
class ControllerAttackFixture {
public:
 enum class Phase {WaitConnection,Settle,Press,Observe,Complete,Aborted};
 struct Result {Phase phase;uint8_t pulseLevel=0;};
 Result update(uint64_t now,bool connected,uint8_t screen,uint8_t auxiliary,AttackObservation s){
  if(phase_==Phase::Complete||phase_==Phase::Aborted)return {phase_};
  if(!started_){started_=true;begin_=last_=at_=now;}
  if(now<last_||now-begin_>8000||screen!=1||auxiliary||!s.valid||!s.entity)return abort();
  last_=now;
  if(phase_!=Phase::WaitConnection&&(!connected||s.entity!=origin_.entity||s.item!=origin_.item))return abort();
  switch(phase_){
   case Phase::WaitConnection:if(connected){if(s.active)return abort();origin_=s;next(Phase::Settle,now);}break;
   case Phase::Settle:if(s.active)return abort();if(now-at_>=500){origin_.activeSamples=s.activeSamples;next(Phase::Press,now);}break;
   case Phase::Press:if(now-at_>=120)next(Phase::Observe,now);break;
   case Phase::Observe:if(s.activeSamples>origin_.activeSamples&&!s.active&&now-at_>=1500)next(Phase::Complete,now);break;
   default:break;
  }
  return {phase_,uint8_t(phase_==Phase::Press?255:0)};
 }
 Phase phase()const{return phase_;}
private:
 Phase phase_=Phase::WaitConnection;bool started_=false;uint64_t begin_=0,last_=0,at_=0;AttackObservation origin_{};
 void next(Phase p,uint64_t now){phase_=p;at_=now;}
 Result abort(){phase_=Phase::Aborted;return {phase_};}
};
}
