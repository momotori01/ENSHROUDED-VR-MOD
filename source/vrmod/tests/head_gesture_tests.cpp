#include <evr/head_gesture.hpp>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::motion;
struct Rig {
 HeadGesture gesture;HandFrame hands;GestureHead head;GesturePad input;
 Rig(){hands.sessionGeneration=1;hands.originGeneration=1;hands.focused=true;
  hands.hands[1].gripActive=true;hands.hands[1].gripFlags=15;hands.hands[1].profile=1;
  head.flags=15;head.pose.position={0,1.7f,0};outside();}
 void point(Vec3 relative){hands.hands[1].grip.position=head.pose.position+rotate(head.pose.orientation,relative);}
 void leftPoint(Vec3 relative){hands.hands[0].grip.position=head.pose.position+rotate(head.pose.orientation,relative);}
 void trackLeft(){hands.hands[0].gripActive=true;hands.hands[0].gripFlags=15;hands.hands[0].profile=1;leftPoint({-.5f,0,0});}
 HeadGestureResult enterLeft(){leftPoint({-.5f,0,0});step();leftPoint({-.18f,0,0});HeadGestureResult r;for(unsigned i=0;i<9;i++)r=step();return r;}
 void outside(){point({.5f,0,0});}void inside(){point({.18f,0,0});}
 HeadGestureResult step(){++hands.frame;hands.predictedTime+=10000000;head.frame=hands.frame;head.session=hands.sessionGeneration;head.origin=hands.originGeneration;head.time=hands.predictedTime;return gesture.update(hands,head,input);}
 HeadGestureResult enter(){outside();step();inside();HeadGestureResult r;for(unsigned i=0;i<9;i++)r=step();return r;}
};
int main(){unsigned n=0;try{
 auto check=[&](bool value){++n;if(!value)throw std::runtime_error("head gesture invariant "+std::to_string(n));};
 {Rig r;auto out=r.enter();check(out.active&&out.entered&&out.pulseAmplitude==.15f&&out.pulseDuration==30000000&&out.resetSwing);
  for(int i=0;i<100;i++){out=r.step();check(out.active&&!out.entered&&!out.pulseAmplitude);}
  r.input.right={1,1};r.input.buttons=0xc0;out=r.step();check(out.pad.buttons==0x19+0x20&&out.mappedButtons==out.pad.buttons&&out.pad.right.x==0&&out.pad.right.y==0);
  r.input.right={.5f,.5f};out=r.step();check((out.pad.buttons&15)==9);
  r.input.right={.4f,.4f};out=r.step();check((out.pad.buttons&15)==0);
  r.input.right={-1,-1};out=r.step();check((out.pad.buttons&15)==6);
  r.outside();out=r.step();check(out.left&&!out.active&&out.pad.buttons==0&&out.pad.right.x==0);
  out=r.step();check(out.pad.buttons==0&&out.pad.right.x==0);
  r.input={};r.step();r.input.right={.7f,-.8f};r.input.buttons=0xc0;out=r.step();check(!out.active&&out.pad.buttons==0xc0&&out.pad.right.x==.7f);
 }
 {Rig r;r.step();r.input.right={1,0};r.input.buttons=0xc0;r.inside();HeadGestureResult out;for(int i=0;i<9;i++)out=r.step();
  check(out.active&&out.pad.buttons==0&&out.pad.right.x==0);r.input.right={};out=r.step();check(out.pad.buttons==0);
  r.input.buttons=0x80;out=r.step();check(out.pad.buttons==0);r.input.buttons=0;out=r.step();check(out.pad.buttons==0);
  r.input.buttons=0xc0;out=r.step();check(out.pad.buttons==0x30);
 }
 {Rig r;r.inside();for(int i=0;i<25;i++)check(!r.step().active);check(r.enter().entered);
  for(int i=0;i<100;i++){r.point({.18f+(i%2?.11f:.13f),0,0});auto out=r.step();check(out.active&&!out.pulseAmplitude);}
  r.point({.33f,0,0});check(r.step().left);
 }
 {Rig r;r.step();for(int i=0;i<40;i++){r.point({.18f+(i%2?.11f:.09f),0,0});auto out=r.step();check(!out.active&&!out.pulseAmplitude);}}
 for(int yaw=-180;yaw<=180;yaw+=30)for(int pitch=-90;pitch<=90;pitch+=45)for(int roll=-90;roll<=90;roll+=90){
  Rig r;r.head.pose={euler(yaw*pi/180,pitch*pi/180,roll*pi/180),{12.2f,8.3f,-19.6f}};auto out=r.enter();
  check(out.active&&out.entered&&length(out.handInHead-Vec3{.18f,0,0})<1e-4f);
 }
 for(int kind=0;kind<8;kind++){
  Rig r;r.enter();r.input.right={0,1};r.input.buttons=0xc0;check(r.step().mappedButtons==0x31);
  switch(kind){case 0:r.hands.focused=false;break;case 1:r.hands.hands[1].gripFlags=3;break;case 2:r.head.flags=3;break;
   case 3:r.hands.originGeneration++;break;case 4:r.hands.sessionGeneration++;break;case 5:r.hands.hands[1].profile++;break;
   case 6:r.hands.predictedTime+=200000000;break;case 7:r.head.pose.orientation={0,0,0,0};break;}
  auto out=r.step();check(!out.active&&out.left&&out.pad.buttons==0&&out.pad.right.y==0&&!out.pulseAmplitude&&out.resetSwing);
  r.hands.focused=true;r.hands.hands[1].gripFlags=15;r.head.flags=15;r.head.pose.orientation={0,0,0,1};r.inside();
  for(int i=0;i<15;i++)check(!r.step().active);
  r.input={};check(r.enter().active);
 }
 {Rig r;r.enter();r.input.left={.8f,-.9f};r.input.leftTrigger=.4f;r.input.rightTrigger=.7f;r.input.buttons=0x1300;
  auto out=r.step();check(out.pad.left.x==.8f&&out.pad.left.y==-.9f&&out.pad.leftTrigger==.4f&&out.pad.rightTrigger==.7f&&out.pad.buttons==0x1300);
  r.input.right={1,0};r.input.buttons=4;out=r.step();check(!(out.pad.buttons&12));
  r.input.right.x=std::numeric_limits<float>::quiet_NaN();out=r.step();check(!out.valid&&out.pad.buttons==0&&out.pad.left.x==0);
 }
 {Rig r;r.enter();r.hands.hands[0].grip.position=r.head.pose.position;r.outside();check(r.step().left);}
 {Rig r;r.enter();auto out=r.gesture.update(r.hands,r.head,r.input);check(!out.active&&!out.valid&&out.left);}
 for(unsigned invalid=0;invalid<9;invalid++){
  HeadGestureConfig c;switch(invalid){case 0:c.enterRadius=0;break;case 1:c.exitRadius=c.enterRadius;break;case 2:c.pulseDuration=100000001;break;
   case 3:c.pulseAmplitude=std::numeric_limits<float>::quiet_NaN();break;case 4:c.entryDwell=0;break;case 5:c.stickOff=c.stickOn;break;case 6:c.rightTemple.x=-.2f;break;
   case 7:c.leftTemple.x=.2f;break;case 8:c.leftTemple.y=std::numeric_limits<float>::infinity();break;}
  bool rejected=false;try{HeadGesture bad(c);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 }
 {HeadGestureConfig c;c.leftHand=false;c.leftTemple.x=.2f;bool ok=true;try{HeadGesture g(c);}catch(...){ok=false;}check(ok);}
 // ---- 2026-10-05 18:50: left hand at the left temple also maps the right stick to the D-pad ----
 {Rig r;r.trackLeft();auto out=r.enterLeft();
  check(out.active&&out.entered&&out.enteredHand==0&&out.activeHands==1&&out.leftTracked&&out.pulseAmplitude==.15f&&out.resetSwing);
  check(length(out.leftHandInHead-Vec3{-.18f,0,0})<1e-4f);
  // Right stick becomes the D-pad; the left stick and triggers keep their values (movement continues).
  r.input.right={1,1};r.input.left={.3f,-.9f};r.input.leftTrigger=.5f;out=r.step();
  check(out.active&&(out.pad.buttons&15)==9&&out.pad.right.x==0&&out.pad.right.y==0&&out.pad.left.x==.3f&&out.pad.left.y==-.9f&&out.pad.leftTrigger==.5f);
  r.input.right={-1,-1};out=r.step();check((out.pad.buttons&15)==6&&out.pad.left.y==-.9f);
  // Leaving the left region ends the gesture and re-arms the right-stick neutral barrier.
  r.leftPoint({-.5f,0,0});out=r.step();check(out.left&&!out.active&&out.activeHands==0&&out.pad.right.x==0&&out.pad.buttons==0);
  r.input.right={.7f,.7f};out=r.step();check(!out.active&&out.pad.right.x==0);
  r.input.right={};r.step();r.input.right={.7f,.7f};out=r.step();check(out.pad.right.x==.7f&&out.pad.left.x==.3f);}
 // Left region before the dwell already inhibits swings; brief touches never activate.
 {Rig r;r.trackLeft();r.step();r.leftPoint({-.18f,0,0});auto out=r.step();check(!out.active&&out.resetSwing);
  r.leftPoint({-.5f,0,0});out=r.step();check(!out.active&&!out.entered);
  out=r.step();check(!out.resetSwing);}
 // A left hand that starts inside its region must leave it first.
 {Rig r;r.trackLeft();r.leftPoint({-.18f,0,0});for(int i=0;i<25;i++){auto out=r.step();check(!out.active&&out.resetSwing);}
  check(r.enterLeft().entered);}
 // Untracked or disabled left hand: no region, no permanent swing inhibition.
 {Rig r;r.step();r.hands.hands[0].grip.position=r.head.pose.position+Vec3{-.18f,0,0};for(int i=0;i<30;i++){auto out=r.step();check(!out.active&&!out.resetSwing&&!out.leftTracked);}}
 {HeadGestureConfig c;c.leftHand=false;Rig r;r.gesture=HeadGesture(c);r.trackLeft();r.step();r.leftPoint({-.18f,0,0});
  for(int i=0;i<30;i++){auto out=r.step();check(!out.active&&!out.resetSwing);}}
 // Both regions: the gesture stays active while either hand remains; one entry, one exit.
 {Rig r;r.trackLeft();auto out=r.enter();check(out.entered&&out.enteredHand==1&&out.activeHands==2);
  r.leftPoint({-.18f,0,0});unsigned entries=0;for(int i=0;i<12;i++){out=r.step();entries+=out.entered;check(out.active&&!out.left);}
  check(!entries&&out.activeHands==3);
  r.outside();out=r.step();check(out.active&&!out.left&&out.activeHands==1);
  r.leftPoint({-.5f,0,0});out=r.step();check(out.left&&!out.active);}
 // Losing left tracking (or a left profile change) ends a left-only gesture; it needs a fresh exit.
 for(int cause=0;cause<2;cause++){Rig r;r.trackLeft();check(r.enterLeft().active);
  if(cause==0)r.hands.hands[0].gripFlags=3;else r.hands.hands[0].profile=2;
  auto out=r.step();check(out.left&&!out.active&&out.valid&&out.resetSwing);
  r.hands.hands[0].gripFlags=15;for(int i=0;i<20;i++)check(!r.step().active);
  check(r.enterLeft().entered);}
 // The left region follows head rotation like the right one.
 for(int yaw=-180;yaw<=180;yaw+=45){Rig r;r.head.pose={euler(yaw*pi/180,.3f,-.2f),{3.f,1.6f,-2.f}};r.trackLeft();auto out=r.enterLeft();
  check(out.active&&out.enteredHand==0&&length(out.leftHandInHead-Vec3{-.18f,0,0})<1e-4f);}
 // The right hand at the left temple (or the left hand at the right temple) never activates.
 {Rig r;r.trackLeft();r.step();r.point({-.18f,0,0});r.leftPoint({.18f,0,0});for(int i=0;i<20;i++)check(!r.step().active);}
 std::cout<<"PASS "<<n<<" head gesture checks (value-only; no game input/XR haptic integration)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
