#include <evr/motion_intent.hpp>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::motion;
unsigned checks=0;void require(bool b,const char* what){if(!b)throw std::runtime_error(what);++checks;std::cout<<"PASS "<<what<<'\n';}
struct Fixture {
    IntentGate gate{{2.f,.3f,.6f,2,100000000,12.f,30.f}};HandFrame f{};Weapon w{};NativeCombat n{};
    Fixture(){f.focused=true;f.sessionGeneration=f.originGeneration=1;w.binding=100;
        for(auto& h:f.hands){h.grip.position={0,1.5f,-.5f};h.gripActive=h.aimActive=h.triggerActive=h.grabActive=h.grab=true;h.gripFlags=h.aimFlags=15;h.profile=1;}
        n.enabled=n.known=n.local=n.alive=n.canStart=true;n.uiOrEvent=false;}
    Intent step(float dx=0){++f.frame;f.predictedTime+=10000000;f.hands[1].grip.position.x+=dx;f.hands[0].grip.position.x+=dx;return gate.update(f,w,n);}
    void rest(){for(int i=0;i<3;i++)require(step().kind==IntentKind::None,"neutral motion cannot start attack");}
};
int main(){try{
 {Fixture x;x.rest();auto hit=x.step(.04f);require(hit.kind==IntentKind::Melee&&hit.request==1,"one physical swing creates one melee request");
  for(int i=0;i<12;i++)require(x.step(i%2?-.04f:.04f).kind==IntentKind::None,"no repeated swing while native acknowledgement pending");
  require(!x.gate.acknowledge(2)&&x.gate.pendingRequest()==1,"wrong native acknowledgement cannot consume request");
  require(x.gate.acknowledge(1),"matching native acknowledgement consumed");x.n.recoveryTotal=100;
  float previous=1;for(double remaining:{100.,75.,50.,25.,1.}){x.n.recoveryRemaining=remaining;auto b=x.step(.04f);require(b.blocked==Block::Cooldown&&b.kind==IntentKind::None,"strict native cooldown rejects attacks");require(b.haptic[1]<previous&&b.haptic[0]==0,"haptic decreases with native recovery without touching other hand");previous=b.haptic[1];}
  x.n.recoveryRemaining=0;require(x.step(.04f).kind==IntentKind::None,"cooldown expiry does not replay a queued swing");x.rest();require(x.step(-.04f).request==2,"new neutral and swing allow the next attack");}
 {Fixture x;x.n.airborne=x.n.airborneSpecialAllowed=true;x.rest();require(x.step(.04f).kind==IntentKind::AirborneSpecial,"airborne swing selects permitted native special action");}
 {Fixture x;x.n.airborne=true;x.rest();require(x.step(.04f).kind==IntentKind::Melee,"unavailable special is not invented");}
 for(unsigned failure=0;failure<11;failure++){
  Fixture x;x.rest();switch(failure){case 0:x.n.uiOrEvent=true;break;case 1:x.n.known=false;break;case 2:x.n.local=false;break;
  case 3:x.n.alive=false;break;case 4:x.f.focused=false;break;case 5:x.f.hands[1].gripFlags=3;break;case 6:x.f.hands[1].gripActive=false;break;
  case 7:x.f.hands[1].grab=false;break;case 8:x.n.canStart=false;break;case 9:x.n.recoveryRemaining=std::numeric_limits<double>::quiet_NaN();break;case 10:x.w.binding=0;break;}
  auto r=x.step(.04f);require(r.kind==IntentKind::None&&!r.castHeld&&r.haptic[0]==0&&r.haptic[1]==0,"UI/native/tracking/grip/invalid gate preserves original gameplay");
 }
 for(unsigned discontinuity=0;discontinuity<6;discontinuity++){
  Fixture x;x.rest();switch(discontinuity){case 0:x.f.originGeneration++;break;case 1:x.f.sessionGeneration++;break;case 2:x.w.binding++;break;
   case 3:x.f.predictedTime+=1000000000;break;case 4:x.f.hands[1].profile++;break;case 5:x.f.predictedTime-=20000000;break;}
  auto r=x.step(.4f);require(r.kind==IntentKind::None&&r.blocked==Block::Discontinuity,"recenter/session/item/profile/gap/time reversal suppress spurious swings");
  x.rest();require(x.step(.04f).kind==IntentKind::Melee,"new neutral state restores gesture after discontinuity");
 }
 {Fixture x;x.rest();auto r=x.gate.update(x.f,x.w,x.n);require(r.blocked==Block::Discontinuity&&!r.request,"duplicate eye sampling cannot create an action");}
 {Fixture x;x.w.twoHanded=true;x.w.offHandGripInModel={0,0,-.4f};x.w.offHandTolerance=.05f;x.f.hands[0].grip.position={.4f,1.5f,-.5f};
  auto p=weaponPose(x.f,x.w);require(length(p.position-x.f.hands[1].grip.position)<1e-6f,"two-hand rotation preserves dominant grip position");
  require(length(rotate(p.orientation,{0,0,-1})-Vec3{1,0,0})<1e-5f,"two-handed weapon points through the actual support hand");
  x.rest();require(x.step(.04f).kind==IntentKind::Melee,"both calibrated grips allow attack");}
 for(unsigned failure=0;failure<4;failure++){
  Fixture x;x.w.twoHanded=true;x.w.offHandGripInModel={0,0,-.4f};x.w.offHandTolerance=.05f;x.f.hands[0].grip.position.z=-.9f;
  x.rest();switch(failure){case 0:x.f.hands[0].grab=false;break;case 1:x.f.hands[0].gripFlags=0;break;case 2:x.f.hands[0].grip.position.x+=2;break;case 3:x.f.hands[0].grip.position=x.f.hands[1].grip.position;break;}
  require(x.step(.04f).kind==IntentKind::None,"one hand/missing tracking/far support/degenerate grip cannot attack with two-hand weapon");
 }
 {Fixture x;x.w.kind=WeaponKind::Staff;x.w.modelToMuzzle.position={0,0,-.3f};x.f.hands[1].grip.orientation=euler(pi/2,0,0);x.rest();
  x.f.hands[1].trigger=1;auto cast=x.step();require(cast.kind==IntentKind::Cast&&cast.castHeld,"staff trigger creates cast and persistent hold");
  require(length(cast.aimDirection-Vec3{-1,0,0})<1e-5f&&length(cast.aimOrigin-Vec3{-.3f,1.5f,-.5f})<1e-5f,"staff ray comes from calibrated weapon muzzle, independent of head/pointer aim");
  require(x.gate.acknowledge(cast.request),"staff native acknowledgement");x.n.canStart=false;
  require(x.step().castHeld&&x.step().kind==IntentKind::None,"native casting retains trigger level without duplicate starts");
  x.n.canStart=true;require(x.step().kind==IntentKind::None,"held trigger does not restart automatically when ready");x.f.hands[1].trigger=0;x.rest();x.f.hands[1].trigger=1;require(x.step().kind==IntentKind::Cast,"release and repress creates next staff cast");
  x.n.uiOrEvent=true;require(!x.step().castHeld,"opening UI releases magic hold");}
 {Fixture x;x.rest();x.n.recoveryRemaining=2;x.n.recoveryTotal=4;auto a=x.step();require(std::abs(a.haptic[1]-.3f)<1e-6f,"haptic amplitude has independent ratio reference");x.f.focused=false;auto b=x.step();require(b.haptic[1]==0,"focus loss stops cooldown haptic output");}
 {Fixture x;x.rest();x.f.hands[1].grip.position.x=std::numeric_limits<float>::quiet_NaN();require(x.step().blocked==Block::Invalid,"nonfinite tracked point rejected");}
 // Outstanding requests are cancelled by any invalidating boundary. A late
 // acknowledgement must never consume the next equipment/session request.
 for(unsigned failure=0;failure<12;failure++){
  Fixture x;x.rest();const auto first=x.step(.04f);require(first.request==1,"cancellation fixture pending request");
  switch(failure){case 0:x.n.uiOrEvent=true;break;case 1:x.f.focused=false;break;case 2:x.f.hands[1].grab=false;break;
   case 3:x.f.originGeneration++;break;case 4:x.f.sessionGeneration++;break;case 5:x.w.binding++;break;
   case 6:x.f.hands[1].profile++;break;case 7:x.n.enabled=false;break;case 8:x.n.known=false;break;
   case 9:x.f.hands[1].grip.position.x+=1;break;case 10:x.f.hands[1].grip.orientation=euler(pi,0,0);break;case 11:x.f.predictedTime+=1000000000;break;}
  const auto cancelled=x.step();require(cancelled.kind==IntentKind::None&&cancelled.cancelledRequest==1&&x.gate.pendingRequest()==0,"invalidating boundary cancels outstanding intent exactly once");
  require(!cancelled.castHeld&&cancelled.haptic[0]==0&&cancelled.haptic[1]==0,"cancelled intent releases cast and haptics");
  require(!x.gate.acknowledge(first.request),"late native acknowledgement rejected after cancellation");
  x.n.uiOrEvent=false;x.n.enabled=x.n.known=true;x.f.focused=x.f.hands[1].grab=true;x.rest();
  const auto next=x.step(.04f);require(next.request==2,"fresh neutral and swing restore next monotonically identified request");
  require(!x.gate.acknowledge(first.request)&&x.gate.pendingRequest()==next.request,"stale acknowledgement cannot consume new request");
 }
 {Fixture x;x.rest();x.f.hands[1].grip.orientation={0,0,0,-1};auto a=x.step(.04f);require(a.kind==IntentKind::Melee,"quaternion sign flip is not a physical discontinuity");}
 // A long visual offset moves metres per second during a small wrist turn,
 // but attack authority comes only from the tracked grip translation.
 for(unsigned hand:{0u,1u})for(float offset:{0.f,.63f,2.f}){
  Fixture x;x.w.mainHand=hand;x.w.gripToModel.position={0,0,-offset};x.rest();
  x.f.hands[hand].grip.orientation=euler(.08f,0,0);auto a=x.step();
  require(a.kind==IntentKind::None&&a.speed==0,"pure wrist twist cannot attack for either hand or any model offset");
  a=x.step(.04f);require(a.kind==IntentKind::Melee&&std::abs(a.speed-4)<1e-5f,"controller translation qualifies independently of model offset and rotation");
 }
 for(unsigned pivot:{0u,1u}){
  Fixture x;x.w.twoHanded=true;x.w.gripToModel.position={0,0,.63f};x.w.offHandTolerance=.035f;
  x.f.hands[0].grip.position=x.f.hands[1].grip.position+Vec3{0,0,.63f};x.rest();
  // Pivot either hand: moving hand describes an arc with unchanged grip span.
  const auto fixed=x.f.hands[pivot].grip.position;const auto axis=x.f.hands[1-pivot].grip.position-fixed;
  x.f.hands[1-pivot].grip.position=fixed+rotate(euler(.064f,0,0),axis);
  auto a=x.step();
  if(pivot==0)require(a.kind==IntentKind::Melee&&a.speed>4&&a.speed<4.04f,"left pivot and moving right controller qualify two-hand attack");
  else require(a.kind==IntentKind::None&&a.speed==0,"moving left support cannot qualify attack with a stationary right controller");
  require(length(a.controllerVelocity[pivot])==0&&a.velocityPreviousFrame==3,"speed provenance identifies stationary pivot and previous tracked sample");
 }
 for(float angle:{.01f,.04f,.08f}){
  Fixture x;x.w.twoHanded=true;x.w.gripToModel.position={0,0,.63f};x.w.offHandTolerance=.035f;
  x.f.hands[0].grip.position=x.f.hands[1].grip.position+Vec3{0,0,.63f};x.rest();
  const auto center=(x.f.hands[0].grip.position+x.f.hands[1].grip.position)*.5f;
  for(auto& h:x.f.hands)h.grip.position=center+rotate(euler(angle,0,0),h.grip.position-center);
  auto a=x.step();const float rightSpeed=length(a.controllerVelocity[1]);
  require(std::abs(a.speed-rightSpeed)<1e-5f,"opposite left movement cannot cancel or amplify right-hand speed");
  require((a.kind==IntentKind::Melee)==(rightSpeed>=2),"two-hand qualification follows right threshold with both hands gripping");
 }
 {Fixture x;x.w.twoHanded=true;x.w.offHandGripInModel={0,0,-.4f};x.w.offHandTolerance=.035f;
  x.f.hands[0].grip.position.z=-.9f;x.rest();
  for(auto& h:x.f.hands)h.grip.orientation=euler(.08f,0,0);
  auto a=x.step();require(a.kind==IntentKind::None&&a.speed==0,"two fixed controller positions do not attack when both wrists rotate");
 }
 {Fixture x;x.rest();for(unsigned i=0;i<90;i++){x.f.hands[1].grip.orientation=euler(.05f*std::sin(float(i)),0,0);
  const auto a=x.step(i%2?.0005f:-.0005f);require(a.kind==IntentKind::None&&a.speed<.1f,"small tracking jitter and wrist twists stay below swing speed");}}

 {Fixture x;x.w.twoHanded=true;x.w.offHandGripInModel={0,0,-.4f};x.w.offHandTolerance=.05f;x.f.hands[0].grip.position.z=-.9f;x.rest();
  x.f.hands[0].grip.position={.4f,1.5f,-.5f};require(x.step().blocked==Block::Discontinuity,"support wrist discontinuity is rejected even if final grip distance is valid");}
 {bool rejected=false;try{IntentGate bad({2.f,.3f,.6f,2,100000000});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"unconfigured tracking-integrity limits cannot enable combat");}
 {Fixture x;x.rest();for(unsigned i=0;i<180*90;i++){ // Simulated three minutes, not hardware pacing.
    ++x.f.frame;x.f.predictedTime+=11111111;const auto world=compose(Pose{euler(float(i)*.01f,0,0),{float(i),100,50}},x.f.hands[1].grip);
    (void)world;auto r=x.gate.update(x.f,x.w,x.n);if(r.kind!=IntentKind::None)throw std::runtime_error("root motion became swing");}
  require(true,"three-minute locomotion/turning with stationary tracking hands creates zero attacks");}
 std::cout<<"RESULT "<<checks<<" PASS 0 FAIL (value-only intents; native combat not connected)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
