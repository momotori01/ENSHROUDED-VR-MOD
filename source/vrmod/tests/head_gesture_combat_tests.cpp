#include <evr/head_gesture_combat.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("head/combat invariant "+std::to_string(n));};
 for(unsigned variant=0;variant<4;variant++){
  HeadGesture gesture;IntentGate gate{{2.f,.3f,.6f,2,100000000,12.f,30.f}};HandFrame f;
  f.focused=true;f.sessionGeneration=f.originGeneration=1;auto& h=f.hands[1];
  h.gripActive=h.grabActive=h.grab=h.triggerActive=true;h.gripFlags=15;h.profile=1;h.grip.position={.5f,1.7f,0};
  GestureHead head;head.flags=15;head.pose.position={0,1.7f,0};GesturePad pad;Weapon w;w.binding=7;
  if(variant==1)w.kind=WeaponKind::Staff;
  NativeCombat native;native.enabled=native.known=native.local=native.alive=native.canStart=true;native.uiOrEvent=false;
  HeadGestureResult g;Intent result;
  auto step=[&](float dx=0,bool stale=false){f.frame++;f.predictedTime+=10000000;h.grip.position.x+=dx;head.frame=f.frame;head.session=f.sessionGeneration;head.origin=f.originGeneration;head.time=f.predictedTime;
   g=gesture.update(f,head,pad);if(stale)g.frame--;result=gestureCombat(gate,f,w,native,g);};
  for(unsigned i=0;i<5;i++)step();
  // Start near the boundary at rest. Crossing it at combat speed is the first
  // reserved-region sample, and must be blocked before 80ms UI activation.
  h.grip.position.x=.285f;step();for(unsigned i=0;i<3;i++)step();h.trigger=0;
  step(-.03f);check(g.resetSwing&&!g.active&&!result.request&&result.blocked==Block::UiOrEvent);
  h.trigger=1;for(unsigned i=0;i<9;i++){step(-.004f);check(!result.request);}
  check(g.active&&gate.pendingRequest()==0);
  if(variant==2){f.originGeneration++;step();check(!g.active&&!result.request);}
  if(variant==3){step(0,true);check(!result.request&&result.blocked==Block::UiOrEvent);}
  for(unsigned i=0;i<30;i++){step(i%2?.02f:-.02f);check(!result.request);}
  h.grip.position.x=.5f;step();check(!result.request);
  for(unsigned i=0;i<8;i++){step(.03f);check(!result.request);}
  h.trigger=0;for(unsigned i=0;i<4;i++)step();
  h.trigger=1;step(.04f);check(result.request==1);check(gate.acknowledge(1));
  // Acknowledged native action keeps its externally supplied recovery. UI
  // inhibition is not an acknowledgement/cooldown bypass or a second request.
  native.canStart=false;native.recoveryTotal=100;native.recoveryRemaining=80;
  h.grip.position={.18f,1.7f,0};for(unsigned i=0;i<10;i++){step();check(!result.request&&native.recoveryRemaining==80);}
  h.grip.position.x=.5f;for(unsigned i=0;i<10;i++){step(.03f);check(!result.request);}
  native.canStart=true;native.recoveryRemaining=0;h.trigger=0;for(unsigned i=0;i<4;i++)step();h.trigger=1;step(.04f);check(result.request==2);
  h.grip.position.x=.25f;step();check(result.cancelledRequest==2&&!gate.pendingRequest());
  for(unsigned i=0;i<10;i++){step();check(!result.request&&!result.castHeld);}
 }
 std::cout<<"PASS "<<n<<" head gesture/combat interlock checks (value-only, native action untouched)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
