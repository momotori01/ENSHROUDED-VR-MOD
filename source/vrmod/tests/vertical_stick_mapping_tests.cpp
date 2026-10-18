#include <evr/vertical_stick_mapping.hpp>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::motion;
struct Rig {
 HeadGesture head;VerticalStickMapping mapper;HandFrame hands;GestureHead h;
 GesturePad pad;VerticalStickMode mode=VerticalStickMode::Gameplay;uint64_t now=1000,receipt=1000,binding=1;
 HeadGestureResult gesture;
 Rig(){hands.sessionGeneration=hands.originGeneration=1;hands.focused=true;
  auto& r=hands.hands[1];r.gripActive=r.stickActive=true;r.gripFlags=15;r.profile=1;
  h.flags=15;h.pose.position={0,1.7f,0};outside();}
 void outside(){hands.hands[1].grip.position={.5f,1.7f,0};}
 void inside(){hands.hands[1].grip.position={.18f,1.7f,0};}
 VerticalStickResult consume(){return mapper.update(now,receipt,binding,mode,hands,gesture);}
 VerticalStickResult step(){now+=10;receipt=now;hands.frame++;hands.predictedTime+=10000000;
  h.frame=hands.frame;h.time=hands.predictedTime;h.session=hands.sessionGeneration;h.origin=hands.originGeneration;
  hands.hands[1].stick=pad.right;gesture=head.update(hands,h,pad);return consume();}
 void neutral(){pad.right={};step();step();}
};
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("vertical mapping invariant "+std::to_string(n));};
 {Rig r;r.neutral();r.pad.right={.7f,1};auto o=r.step();check(o.valid&&o.mappedButtons==0x40&&o.pad.right.x==.7f&&o.pad.right.y==0);
  for(unsigned i=0;i<1000;i++){o=r.step();check(o.mappedButtons==0x40&&o.pad.buttons==0x40&&o.pad.right.y==0);}
  r.pad.right.y=.5f;check(r.step().mappedButtons==0x40);r.pad.right.y=.4f;check(r.step().mappedButtons==0);
  r.pad.right.y=-1;check(r.step().mappedButtons==0x2000);r.pad.right.y=-.5f;check(r.step().mappedButtons==0x2000);
  r.pad.right.y=0;check(r.step().pad.buttons==0);
 }
 // Mode changes cannot turn a held stick into a new dodge/sprint/menu input.
 for(auto from:{VerticalStickMode::Gameplay,VerticalStickMode::NativeUi,VerticalStickMode::NativeSpecial})
  for(auto to:{VerticalStickMode::Gameplay,VerticalStickMode::NativeUi,VerticalStickMode::NativeSpecial})if(from!=to)
   for(float held:{-1.f,1.f}){Rig r;r.mode=from;r.neutral();r.pad.right.y=held;r.step();r.mode=to;
    for(int i=0;i<20;i++){auto o=r.step();check(o.mappedButtons==0&&o.pad.right.y==0&&o.needsNeutral);}
    r.neutral();r.pad.right.y=held;auto o=r.step();check(!o.needsNeutral);
    check(to==VerticalStickMode::Gameplay?o.mappedButtons==(held>0?0x40:0x2000):o.mappedButtons==0&&o.pad.right.y==held);
   }
 {Rig r;r.neutral();r.pad.right.y=-1;check(r.step().mappedButtons==0x2000);r.inside();
  for(int i=0;i<9;i++){auto o=r.step();check(!o.mappedButtons&&!(o.pad.buttons&0x2040));}
  check(r.gesture.active);r.pad.right={};r.step();r.pad.right={0,1};auto o=r.step();check(o.mode==VerticalStickMode::HeadGesture&&o.pad.buttons==1&&!o.mappedButtons);
  r.outside();for(int i=0;i<20;i++){o=r.step();check(!o.mappedButtons&&!o.pad.buttons&&o.pad.right.y==0);}
  r.neutral();r.pad.right.y=1;check(r.step().mappedButtons==0x40);
 }
 {Rig r;r.neutral();r.inside();for(int i=0;i<9;i++)r.step();r.pad.buttons=0xc0;
  auto o=r.step();check(o.pad.buttons==0x30&&o.mappedButtons==0);r.mode=VerticalStickMode::NativeUi;
  o=r.consume();check(o.pad.buttons==0x30&&o.mappedButtons==0);r.pad.buttons=0;check(r.step().pad.buttons==0);
 }
 // A horizontal stick still held after leaving the temple keeps the head
 // mapper disarmed. A vertical-only neutral must not bypass that barrier.
 {Rig r;r.neutral();r.inside();for(int i=0;i<9;i++)r.step();r.pad.right={1,0};r.step();r.outside();r.step();
  r.pad.right.y=1;for(int i=0;i<10;i++){auto o=r.step();check(!o.mappedButtons&&o.pad.right.x==0);}
  r.neutral();r.pad.right={0,1};check(r.step().mappedButtons==0x40);
 }
 // Native wheel consumes all stick directions, even when entered while held.
 for(float x:{-1.f,0.f,1.f})for(float y:{-1.f,0.f,1.f})if(x||y){
  Rig r;r.neutral();r.pad.right={x,y};r.step();r.mode=VerticalStickMode::NativeOverlay;
  auto o=r.step();check(o.valid&&o.pad.right.x==x&&o.pad.right.y==y&&!o.mappedButtons&&!(o.pad.buttons&0x2040));
  r.inside();for(int i=0;i<12;i++){o=r.step();check(o.mode==VerticalStickMode::NativeOverlay&&o.pad.right.x==x&&o.pad.right.y==y&&!o.mappedButtons&&!o.pad.buttons);}
  r.outside();r.mode=VerticalStickMode::Gameplay;
  for(int i=0;i<12;i++){o=r.step();check(o.pad.right.x==0&&o.pad.right.y==0&&!o.mappedButtons&&!o.pad.buttons&&o.needsNeutral);}
  r.pad.right={1,0};o=r.step();check(o.needsNeutral&&o.pad.right.x==0);
  r.pad.right.y=-1;check(!r.step().mappedButtons);
  r.neutral();r.pad.right.y=-1;check(r.step().mappedButtons==0x2000);
 }
 // A new input owner must not inherit the wheel's held axis.
 {Rig r;r.neutral();r.mode=VerticalStickMode::NativeOverlay;r.pad.right={1,-1};r.step();r.binding++;
  for(int i=0;i<10;i++){auto o=r.step();check(o.needsNeutral&&o.pad.right.x==0&&o.pad.right.y==0);}
  r.neutral();r.pad.right={1,-1};auto o=r.step();check(o.pad.right.x==1&&o.pad.right.y==-1&&!o.mappedButtons);
 }
 // Focus/session/origin/profile/owner changes, stale values and out-of-order frames.
 for(unsigned fault=0;fault<12;fault++){Rig r;r.neutral();r.pad.right.y=-1;check(r.step().mappedButtons==0x2000);
  switch(fault){case 0:r.now+=201;break;case 1:r.now=r.receipt-1;break;case 2:r.hands.focused=false;break;
   case 3:r.hands.hands[1].gripFlags=3;break;case 4:r.hands.hands[1].stickActive=false;break;case 5:r.binding=0;break;
   case 6:r.mode=VerticalStickMode::Unavailable;break;case 7:r.gesture.frame--;break;case 8:r.gesture.origin++;break;
   case 9:r.gesture.pad.right.y=std::numeric_limits<float>::quiet_NaN();break;case 10:r.hands.frame--;r.gesture.frame--;break;
   case 11:r.hands.predictedTime--;r.gesture.time--;break;}
  auto o=r.consume();check(!o.valid&&!o.mappedButtons&&!o.pad.buttons&&o.pad.right.y==0);
 }
 for(unsigned change=0;change<4;change++){Rig r;r.neutral();r.pad.right.y=1;r.step();
  switch(change){case 0:r.binding++;break;case 1:r.hands.sessionGeneration++;break;case 2:r.hands.originGeneration++;break;case 3:r.hands.hands[1].profile++;break;}
  for(int i=0;i<4;i++)check(!r.step().mappedButtons);
  r.neutral();r.pad.right.y=1;check(r.step().mappedButtons==0x40);
 }
 {Rig r;r.neutral();r.pad.left={.3f,-.4f};r.pad.right={.7f,-1};r.pad.buttons=0x1300;r.pad.leftTrigger=.2f;r.pad.rightTrigger=.8f;
  auto o=r.step();check(o.pad.left.x==.3f&&o.pad.left.y==-.4f&&o.pad.right.x==.7f&&o.pad.leftTrigger==.2f&&o.pad.rightTrigger==.8f&&o.pad.buttons==0x3300);
  for(int i=0;i<10;i++)check(r.consume().pad.buttons==0x3300);
  r.now+=201;check(!r.consume().valid);
 }
 for(unsigned bad=0;bad<6;bad++){VerticalStickConfig c;switch(bad){case 0:c.on=2;break;case 1:c.off=c.on;break;
  case 2:c.neutral=c.off;break;case 3:c.neutral=-.1f;break;case 4:c.on=std::numeric_limits<float>::infinity();break;case 5:c.maximumAgeMs=0;break;}
  bool rejected=false;try{VerticalStickMapping m(c);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 }
 std::cout<<"PASS "<<n<<" vertical stick and real head-gesture composition checks (native routing unverified)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
