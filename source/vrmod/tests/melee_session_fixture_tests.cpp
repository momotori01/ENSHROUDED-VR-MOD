#include <evr/melee_session_fixture.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned checks=0;try{
 auto check=[&](bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);};
 Weapon w;w.twoHanded=true;w.binding=1;w.offHandGripInModel={-.4f,0,0};w.offHandTolerance=.01f;
 game::MeleeSessionFixture fixture(w,1,false);game::MotionCombatObservation n;n.valid=n.alive=n.ready=n.canPrimary=true;n.entity=1;n.item=7;n.binding=1;n.world=9;
 HandFrame f;f.focused=true;f.sessionGeneration=f.originGeneration=1;for(auto& h:f.hands){h.profile=1;h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;}
 unsigned requests=0;game::MotionAttackFixture::Result result;float x=.2f;
 for(uint64_t frame=1;frame<=100;frame++){
  const auto now=1000+frame*20;n.nativeMs=now;f.frame=frame;f.predictedTime=frame*20000000;
  if(frame==12)x+=.06f;f.hands[1].grip.position={x,1,-.4f};f.hands[0].grip.position={x-.4f,1,-.4f};
  if(frame>=13&&frame<65){n.active=true;n.usedItem=7;n.trigger=1;n.sequence=8;n.triggerTime=1;n.length=2000000000;n.currentTime=(frame-13)*20000000;n.canPrimary=false;}
  else {n.active=false;n.canPrimary=true;}
  result=fixture.update(now,frame>3,true,f,now,n);
  check(result.phase!=game::MotionAttackFixture::Phase::Aborted,"finite native harness stays valid");
  if(result.gesture.request){requests++;check(frame==12,"single intended wrist motion");}
  if(frame>=65)check(result.phase==game::MotionAttackFixture::Phase::Complete&&!result.pulseLevel&&result.completed==1,"completion remains neutral");
 }
 check(requests==1,"one request before finite stop");
 for(unsigned exitFrame:{4u,12u,30u}){
  game::MeleeSessionFixture stopFixture(w,2,false);auto stopNative=n;stopNative.active=false;stopNative.canPrimary=true;stopNative.triggerTime=0;float sx=.2f;
  for(uint64_t frame=1;frame<=exitFrame;++frame){
   const auto now=1000+frame*20;stopNative.nativeMs=now;f.frame=frame;f.predictedTime=frame*20000000;
   if(frame==12)sx+=.06f;f.hands[1].grip.position={sx,1,-.4f};f.hands[0].grip.position={sx-.4f,1,-.4f};
   if(frame>=13){stopNative.active=true;stopNative.usedItem=7;stopNative.trigger=1;stopNative.sequence=8;stopNative.triggerTime=1;stopNative.length=2000000000;stopNative.currentTime=(frame-13)*20000000;stopNative.canPrimary=false;}
   stopFixture.update(now,true,true,f,now,stopNative);
  }
  stopFixture.closeEpoch();const auto c=stopFixture.lastCommand;
  check(stopFixture.phase()==game::MotionAttackFixture::Phase::Aborted&&!c.level&&c.reason==MeleeStop::EpochChanged,"explicit epoch stop needs no fresh native or hand input");
  check((c.retired!=0)==(exitFrame>=12)&&c.revoked==(exitFrame>=12),"pending and accepted tickets retire once; idle stop invents no ticket");
  const auto late=stopFixture.update(9999,true,true,f,9999,stopNative);
  check(!late.pulseLevel&&!stopFixture.lastCommand.retired&&!stopFixture.lastCommand.issued,"late samples cannot replay cancellation or revive finite owner");
 }

 std::cout<<"PASS "<<checks<<" unlimited coordinator / bounded integration harness checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
