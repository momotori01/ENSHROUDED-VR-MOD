#pragma once
#include <cstdint>
#include <cmath>
namespace evr::game {
// Explicit in-place diagnostic. Never writes Player Transform or HMD Pose.
// A second A edge is permitted only after a real native airborne Actor state and
// positive Root displacement. No attack, stick, trigger or inventory input.
struct MotionObservation {uint32_t entity=0;uint8_t camera=255;double x=0,y=0,z=0;bool valid=false;uint64_t actorState=1,rootSample=0,rootAgeMs=0,actorAgeMs=0;};
class ControllerMotionFixture {
public:
 enum class Phase {WaitConnection,Settle,PressFirst,ReleaseFirst,Airborne,PressSecond,ReleaseSecond,WaitLand,ObserveLand,Complete,Aborted};
 struct Result {Phase phase;uint16_t buttons=0;uint32_t captureTag=0;};
 explicit ControllerMotionFixture(bool twice=false,bool glide=false):twice_(twice||glide),glide_(glide){}
 Result update(uint64_t now,bool connected,uint8_t screen,uint8_t auxiliary,MotionObservation s){
  if(phase_==Phase::Complete||phase_==Phase::Aborted)return {phase_};
  if(!started_){started_=true;begin_=last_=at_=now;}
  if(now<last_||now-begin_>12000||screen!=1||auxiliary||!s.valid||!s.entity||!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z))return abort();
  last_=now;
  if(phase_!=Phase::WaitConnection&&(!connected||s.entity!=origin_.entity))return abort();
  if(phase_!=Phase::WaitConnection&&(std::hypot(s.x-origin_.x,s.z-origin_.z)>(glide_?20.:.25)||std::abs(s.y-origin_.y)>4))return abort();
  if(s.camera!=4&&s.camera!=19&&s.camera!=43&&s.camera!=44&&!(glide_&&s.camera==25))return abort();
  const bool gliding=(s.actorState&(1ull<<37))!=0;
  const bool ground=(s.actorState&1)!=0,air=!ground&&((s.actorState&((1ull<<1)|(1ull<<32)))||(glide_&&gliding));
  const bool settled=ground&&!gliding&&!(s.actorState&(1ull<<3))&&std::abs(s.y-origin_.y)<0.03;
  const bool landingRoll=glide_&&ground&&(phase_==Phase::WaitLand||phase_==Phase::ObserveLand);
  if((!ground&&!air)||(s.actorState&(1ull<<7))||((s.actorState&(1ull<<3))&&!landingRoll))return abort();
  switch(phase_){
   case Phase::WaitConnection:if(connected){if(!ground)return abort();origin_=s;next(Phase::Settle,now);}break;
   case Phase::Settle:if(!ground)return abort();if(now-at_>=500)next(Phase::PressFirst,now);break;
   case Phase::PressFirst:if(now-at_>=120)next(Phase::ReleaseFirst,now);break;
   // A zero packet was emitted on entry to ReleaseFirst. Only the explicit
   // glide fixture deploys promptly at verified native air on slow GPU frames.
   case Phase::ReleaseFirst:if(air&&s.y>origin_.y+0.03)next(glide_?Phase::PressSecond:Phase::Airborne,now);else if(now-at_>1800)return abort();break;
   case Phase::Airborne:if(!air)return abort();if(now-at_>=200)next(twice_?Phase::PressSecond:Phase::WaitLand,now);break;
   case Phase::PressSecond:if(!air)return abort();if(glide_&&gliding){next(Phase::WaitLand,now);break;}if(now-at_>=120)next(Phase::ReleaseSecond,now);break;
   case Phase::ReleaseSecond:next(Phase::WaitLand,now);break;
   // Contact may precede a native landing roll/root dip. Wait with zero input
   // for stable ground; all global owner/time/motion bounds remain active.
   case Phase::WaitLand:if(settled)next(Phase::ObserveLand,now);break;
   case Phase::ObserveLand:if(!settled){landCaptured_=false;next(Phase::WaitLand,now);break;}if(now-at_>=700)next(Phase::Complete,now);break;
   default:break;
  }
  uint32_t capture=0;
  if(!airCaptured_&&phase_==Phase::WaitLand&&air&&(glide_?(gliding&&s.camera==25):now-at_>=100)){capture=1;airCaptured_=true;}
  if(!landCaptured_&&phase_==Phase::ObserveLand&&now-at_>=300){capture=2;landCaptured_=true;}
  return {phase_,uint16_t(phase_==Phase::PressFirst||phase_==Phase::PressSecond?0x1000:0),capture};
 }
 Phase phase()const{return phase_;}
private:
 Phase phase_=Phase::WaitConnection;bool started_=false,twice_=false,glide_=false,airCaptured_=false,landCaptured_=false;
 uint64_t begin_=0,last_=0,at_=0;MotionObservation origin_{};
 void next(Phase p,uint64_t now){phase_=p;at_=now;}
 Result abort(){phase_=Phase::Aborted;return {phase_};}
};
}
