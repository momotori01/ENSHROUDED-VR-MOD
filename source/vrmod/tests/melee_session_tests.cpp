#include <evr/melee_world_binding.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
unsigned checks=0;void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
struct Rig {
    MeleeSession driver{{2,.3f,.6f,2,100000000,12,30},{120,1000,200}};
    WorldLifetimes<4> worlds;WorldToken worldToken;bool worldBound=false;
    void bindWorld(){worldBound=true;worldToken=worlds.created({9,10,11,2});worlds.sealAdoption();}
    HandFrame f;Weapon w;MeleeNative n;MeleePermission permission{true,true,false};MeleeCommand out;uint64_t now=1000;float x=.2f;
    Rig(){f.focused=true;f.sessionGeneration=f.originGeneration=1;for(auto& h:f.hands){h.profile=1;h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;}
        w.binding=5;n.world=9;n.entity=1;n.item=7;n.binding=5;n.known=n.local=n.alive=n.gameplay=n.canPrimary=true;}
    MeleeCommand step(float delta=0){x+=delta;now+=20;++f.frame;f.predictedTime+=20000000;f.hands[1].grip.position={x,1,-.4f};n.milliseconds=now;
        return out=driver.update(now,f,now,w,worldBound?bindMeleeWorld(worlds,worldToken,n,2):n,permission);}
    void neutral(){for(int i=0;i<4;i++)step();}
    uint64_t swing(){neutral();step(.06f);check(out.issued&&out.level==255,"new physical swing requests native input");return out.issued;}
    void ack(){n.active=true;n.usedItem=n.item;n.trigger=1;n.sequence=8;++n.triggerTime;n.length=1000000000;n.currentTime=0;n.playedTime=0;n.canPrimary=false;step();}
    void finish(){n.active=false;n.canPrimary=true;step();}
};
int main(){try{
 {Rig r;uint64_t previous=0;
  for(unsigned attack=0;attack<300;attack++){
    const auto id=r.swing();check(id>previous,"monotonic IDs beyond finite test limit");previous=id;r.ack();check(r.out.accepted==id&&r.out.current==id,"native ack qualified");
    for(unsigned tick=1;tick<=50;tick++){
        r.n.currentTime=20000000ll*tick;r.step(tick%2?.06f:-.06f);
        check(!r.out.issued&&r.out.current==id&&!r.out.suspended,"cooldown swings never queued");
        check(r.out.remaining==double(r.n.length-r.n.currentTime)&&r.out.total==double(r.n.length),"native recovery units preserved");
        if(tick>=6)check(!r.out.level,"request pulse ends while action continues");
        const auto duplicate=r.driver.update(r.now+1,r.f,r.now,r.w,r.n,r.permission);check(!duplicate.issued&&!duplicate.accepted&&!duplicate.retired&&duplicate.current==id,"duplicate no repeated request/ack");
    }
    r.finish();check(r.out.retired==id&&!r.out.revoked&&!r.out.level,"native completion retires contact authority");
    r.step(.06f);check(!r.out.issued,"completion does not replay recovery swing");
  }
  check(r.driver.completed()==300&&r.now>300000,"over five minutes unbounded native-model actions");
 }
 for(unsigned fault=0;fault<15;fault++){
    Rig r;const auto id=r.swing();r.ack();const auto oldN=r.n;const auto oldF=r.f;const auto oldP=r.permission;const auto oldW=r.w;
    switch(fault){case 0:r.permission.enabled=false;break;case 1:r.permission.ownsInput=false;break;case 2:r.permission.reserved=true;break;
    case 3:r.n.gameplay=false;break;case 4:r.n.alive=false;break;case 5:r.f.focused=false;break;case 6:r.f.hands[1].gripFlags=0;break;
    case 7:r.f.hands[1].grab=false;break;case 8:r.n.world++;break;case 9:r.n.binding++;r.w.binding++;break;
    case 10:r.f.sessionGeneration++;break;case 11:r.f.originGeneration++;break;case 12:r.n.sequence++;break;
    case 13:r.n.triggerTime++;break;case 14:r.n.milliseconds=r.now-201;break;}
    const auto o=r.driver.update(r.now,r.f,r.now,r.w,r.n,r.permission);check(o.suspended&&!o.level&&o.revoked&&o.retired==id,"interrupt revokes and neutralizes every callback");
    r.n=oldN;r.f=oldF;r.permission=oldP;r.w=oldW;r.step(.06f);check(!r.out.issued&&!r.out.level,"recovery does not restart prior native action");
 }
 {Rig r;r.swing();for(int i=0;i<51;i++)r.step();check(r.out.suspended&&r.out.revoked&&!r.out.level,"unacknowledged input times out without retry");r.step(.06f);check(!r.out.issued,"timeout swing not queued");r.swing();}
 {Rig r;r.n.canPrimary=false;r.neutral();r.step(.06f);check(!r.out.issued,"native idle still requires CanPrimary");r.n.canPrimary=true;r.step(.06f);check(!r.out.issued,"permission gain cannot replay swing");r.swing();}
 {Rig r;r.swing();r.n.active=true;r.n.usedItem=99;r.n.trigger=1;r.n.sequence=8;r.n.triggerTime=1;r.n.length=100;r.step();check(r.out.revoked&&!r.out.level,"wrong item cannot acknowledge request");}
 {Rig r;r.swing();r.ack();r.now+=201;const auto x=r.driver.update(r.now,r.f,r.now-201,r.w,r.n,r.permission);check(x.revoked&&!x.level,"stale native and hand samples cancel");}
 {Rig r;r.w.twoHanded=true;r.w.offHandGripInModel={-.4f,0,0};r.w.offHandTolerance=.01f;
  for(int i=0;i<5;i++){r.f.hands[0].grip.position={r.x-.4f,1,-.4f};r.step();}
  r.f.hands[0].grip.position.x+=.06f;r.step(.06f);check(r.out.issued,"two hands qualify");r.ack();
  r.f.hands[0].grab=false;const auto x=r.driver.update(r.now,r.f,r.now,r.w,r.n,r.permission);check(x.revoked&&!x.level,"offhand release revokes accepted ticket immediately");}
 {Rig r;r.bindWorld();const auto id=r.swing();r.ack();auto cached=bindMeleeWorld(r.worlds,r.worldToken,r.n,2);const auto original=r.n;
  check(cached.known&&cached.worldSerial==r.worldToken.serial&&original.worldSerial==0,"binding adds epoch only to private value copy");
  check(r.worlds.pin(r.worldToken)&&r.worlds.beginClose(r.worldToken),"native root entry invalidates pinned World");
  const auto bound=bindMeleeWorld(r.worlds,r.worldToken,cached,2);check(!bound.known&&bound.worldSerial==0&&cached.known,"stale captured native value cannot revive after root begin");
  r.step();check(r.out.revoked&&r.out.retired==id&&!r.out.level&&r.out.reason==MeleeStop::NativeUnavailable,"closing World revokes accepted contact ticket and releases input");
  const auto old=r.worldToken;check(r.worlds.finishClose(old)&&!r.worlds.reclaim(old),"MOD callback retains ended value only");
  r.worldToken=r.worlds.created({9,10,11,2});check(bool(r.worldToken)&&!(r.worldToken==old),"same native addresses may return with a new epoch");
  r.step(.06f);check(!r.out.issued&&!r.out.level&&r.out.reason==MeleeStop::EpochChanged,"same Actor values cannot carry request into new World");
  check(!bindMeleeWorld(r.worlds,old,cached,2).known&&r.worlds.unpin(old)&&r.worlds.reclaim(old),"late old callback cannot publish into replacement World");
  r.n.active=false;r.n.canPrimary=true;r.neutral();check(r.swing()>id,"replacement requires fresh neutral then new physical swing");
 }
 {Rig r;r.bindWorld();const auto id=r.swing();const auto old=r.worldToken;check(r.worlds.beginClose(old)&&r.worlds.finishClose(old)&&r.worlds.reclaim(old),"world replaced between coordinator samples");
  r.worldToken=r.worlds.created({9,10,11,2});r.step();check(r.out.revoked&&r.out.retired==id&&r.out.reason==MeleeStop::EpochChanged&&!r.out.level,"epoch change cancels even if close happened between input samples");
 }
 {Rig r;r.bindWorld();check(!bindMeleeWorld(r.worlds,r.worldToken,r.n,0).known,"server and client roles cannot interchange");
  auto foreign=r.n;foreign.world++;check(!bindMeleeWorld(r.worlds,r.worldToken,foreign,2).known,"foreign World rejected despite matching Actor/item");
  foreign=r.n;foreign.known=false;check(!bindMeleeWorld(r.worlds,r.worldToken,foreign,2).known,"valid generation cannot grant native permission");
 }
 std::cout<<"PASS "<<checks<<" continuous melee lifecycle checks; native responses simulated, game integration pending\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
