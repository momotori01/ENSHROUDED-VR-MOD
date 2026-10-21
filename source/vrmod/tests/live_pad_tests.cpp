#include <evr/live_pad.hpp>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::motion;
// Same chain as the hook: HeadGesture -> VerticalStickMapping -> LivePadMapper.
struct Rig {
 HeadGesture head;VerticalStickMapping vertical;LivePadMapper live;HandFrame hands;GestureHead h;
 bool wield=false;LiveScreen screen=LiveScreen::Gameplay;uint64_t now=1000,receipt=1000,binding=1;HeadGestureResult gesture;
 Rig(){hands.sessionGeneration=hands.originGeneration=1;hands.focused=true;
  for(unsigned i=0;i<2;i++){auto& s=hands.hands[i];s.gripActive=s.stickActive=s.stickClickActive=s.triggerActive=s.grabActive=s.primaryActive=s.secondaryActive=true;
   s.gripFlags=15;s.profile=7;s.grip.position={i?.5f:-.5f,1.2f,0};}
  h.flags=15;h.pose.position={0,1.7f,0};}
 GesturePad pad()const{GesturePad p;p.left=hands.hands[0].stick;p.right=hands.hands[1].stick;
  for(unsigned i=0;i<2;i++){const auto& s=hands.hands[i];if(s.stickClick)p.buttons|=i?0x80:0x40;(i?p.rightTrigger:p.leftTrigger)=s.trigger;}return p;}
 LivePadState consume(){auto m=vertical.update(now,receipt,binding,liveStickMode(screen),hands,gesture);return live.update(screen,m,hands,wield);}
 LivePadState step(){now+=10;receipt=now;hands.frame++;hands.predictedTime+=10000000;
  h.frame=hands.frame;h.time=hands.predictedTime;h.session=hands.sessionGeneration;h.origin=hands.originGeneration;
  gesture=head.update(hands,h,pad());return consume();}
 LivePadState settle(){LivePadState o;for(int i=0;i<3;i++)o=step();return o;}
 auto& r(){return hands.hands[1];}auto& l(){return hands.hands[0];}
 void release(){for(auto& s:hands.hands){s.primaryPressed=s.secondaryPressed=s.grab=s.stickClick=false;s.stick={};s.trigger=0;}}
};
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("live pad invariant "+std::to_string(n));};
 using M=LivePadMapper;
 // Gameplay table.
 {Rig x;check(x.settle()==LivePadState{});
  struct Case{bool LivePadState::*unused;};
  x.r().primaryPressed=true;check(x.step().buttons==M::A);x.release();check(x.step().buttons==0);
  x.r().secondaryPressed=true;check(x.step().buttons==M::Y);x.release();x.step();
  x.l().primaryPressed=true;check(x.step().buttons==0);x.release();x.step();
  x.l().secondaryPressed=true;check(x.step().buttons==0);x.release();x.step();
  x.r().grab=true;check(x.step().buttons==M::RB);x.release();x.step();
  x.l().grab=true;check(x.step().buttons==0);x.release();x.step();
  x.l().stick={1,-1};x.r().stick={-1,0};x.l().trigger=1;x.r().trigger=.5f;auto o=x.step();
  check(o.leftX==32767&&o.leftY==-32767&&o.rightX==-32767&&o.rightY==0&&o.leftTrigger==0&&o.rightTrigger==128&&o.buttons==M::Y);   // LT released in play; pulled deep it interacts (2026-10-20)
  // Into a menu with the trigger still pulled: no Y there until it is let go (2026-10-20); then it is Y in the menu too.
  x.screen=LiveScreen::Ui;{auto u=x.step();check(u.leftTrigger==0&&!(u.buttons&M::Y));}x.l().trigger=0;check(!(x.step().buttons&M::Y));
  // The menu's left trigger (2026-10-21): a short pull is one Y as it is let go (quick move), a pull held 0.4 s Y from then
  // (take all), and the left B while pulled the sort - the game's LT with X (the menu's left B), not Y.
  x.l().trigger=1;check(!(x.step().buttons&M::Y));x.l().trigger=0;check(x.step().buttons&M::Y);
  for(int i=0;i<10;i++)x.step();check(!(x.step().buttons&M::Y));
  x.l().trigger=1;for(int i=0;i<38;i++)check(!(x.step().buttons&M::Y));for(int i=0;i<4;i++)x.step();check(x.step().buttons&M::Y);
  x.l().trigger=0;x.step();for(int i=0;i<3;i++)check(!(x.step().buttons&M::Y));
  x.l().trigger=1;x.step();x.l().secondaryPressed=true;{auto u=x.step();check(u.leftTrigger==255&&!(u.buttons&(M::X|M::B|M::Y)));}   // LT first
  for(int i=0;i<7;i++)x.step();{auto u=x.step();check(u.leftTrigger==255&&(u.buttons&M::X)&&!(u.buttons&M::B)&&!(u.buttons&M::Y));}   // X 80 ms after
  x.release();x.step();{auto u=x.step();check(!(u.buttons&(M::Y|M::X|M::B))&&u.leftTrigger==0);}
  // A screen that aims at the world (building): the game's LT, the old table (right B = B, left A = X, left B = Y)
  x.screen=LiveScreen::WorldUi;x.l().trigger=1;{auto w=x.step();check(w.leftTrigger==255&&!(w.buttons&M::Y));}x.l().trigger=0;x.step();
  x.r().secondaryPressed=true;check(x.step().buttons==M::B);x.release();x.step();x.l().primaryPressed=true;check(x.step().buttons==M::X);x.release();x.step();
  x.l().secondaryPressed=true;check(x.step().buttons==M::Y);x.release();x.step();
  {Rig z;z.screen=LiveScreen::Ui;z.live.menuLeftTriggerY(false);z.settle();z.l().trigger=1;auto u=z.step();check(u.leftTrigger==255&&!(u.buttons&M::Y));}   // EVR_MENU_LT_Y=0
  // A building hammer in the hand: the game's LT in play (it enters building mode), no Y
  {Rig z;z.settle();z.live.leftTriggerGame(true);z.l().trigger=1;auto u=z.step();check(u.leftTrigger==255&&!(u.buttons&M::Y));
   z.live.leftTriggerGame(false);z.l().trigger=0;z.step();z.l().trigger=1;check(z.step().buttons&M::Y);}
  x.screen=LiveScreen::Gameplay;x.step();
  // The left trigger's interaction: from 85 % down to 50 %; a resting finger (30 %) does nothing; not with the left grip held.
  {Rig y;y.settle();y.l().trigger=.3f;check(!(y.step().buttons&M::Y));y.l().trigger=.84f;check(!(y.step().buttons&M::Y));y.l().trigger=.86f;check(y.step().buttons&M::Y);
   y.l().trigger=.55f;check(y.step().buttons&M::Y);y.l().trigger=.45f;check(!(y.step().buttons&M::Y));y.l().trigger=.9f;y.l().grab=true;check(!(y.step().buttons&M::Y));}
  x.release();x.settle();x.r().stick={0,1};check(x.step().buttons==0x40);x.r().stick={0,-1};check(x.step().buttons==0x2000);
  x.release();check(x.settle().buttons==0);
  x.l().stickClick=true;check(x.step().buttons==0x40);x.release();x.step();x.r().stickClick=true;check(x.step().buttons==0x80);
 }
 // A grip captured for wield stays consumed until release, including after
 // support is lost; an already-open wheel cannot be stolen mid-press.
 {Rig x;x.settle();x.wield=true;x.r().grab=true;check(x.step().buttons==0);
  x.wield=false;check(x.step().buttons==0);x.r().grab=false;x.step();x.r().grab=true;check(x.step().buttons==M::RB);
  x.wield=true;check(x.step().buttons==M::RB);x.r().grab=false;x.step();x.r().grab=true;check(x.step().buttons==0);
  x.r().grab=false;x.step();x.screen=LiveScreen::Ui;x.r().grab=true;check(x.step().buttons==M::RB);
 }
 // Menu table (2026-10-20: right B = Y, left A = B, left B = X); the right stick keeps its vertical axis.
 {Rig x;x.screen=LiveScreen::Ui;x.settle();
  const std::pair<bool HandSample::*,unsigned> rows[]{{&HandSample::primaryPressed,1},{&HandSample::secondaryPressed,1},{&HandSample::primaryPressed,0},{&HandSample::secondaryPressed,0},{&HandSample::grab,1},{&HandSample::grab,0}};
  const uint16_t want[]{M::A,M::Y,M::B,M::X,M::RB,M::LB};
  for(unsigned i=0;i<6;i++){x.hands.hands[rows[i].second].*rows[i].first=true;check(x.step().buttons==want[i]);x.release();check(x.step().buttons==0);}
  x.r().stick={0,-.8f};auto o=x.step();check(o.rightY==int16_t(std::lround(-.8f*32767))&&o.buttons==0);
  x.release();x.step();x.r().primaryPressed=x.r().secondaryPressed=true;x.l().grab=x.r().grab=true;check(x.step().buttons==(M::A|M::Y|M::LB|M::RB));
 }
 // A press keeps the target of its press edge across screen changes.
 {Rig x;x.screen=LiveScreen::Ui;x.settle();x.l().primaryPressed=true;check(x.step().buttons==M::B);
  x.screen=LiveScreen::Gameplay;for(int i=0;i<10;i++){auto o=x.step();check(o.buttons==M::B);}
  x.release();check(x.step().buttons==0);x.l().primaryPressed=true;check(x.step().buttons==0);
  x.screen=LiveScreen::Ui;check(x.step().buttons==0);x.release();x.step();x.l().primaryPressed=true;check(x.step().buttons==M::B);
 }
 {Rig x;x.settle();x.l().grab=true;check(x.step().buttons==0);x.screen=LiveScreen::Ui;
  for(int i=0;i<5;i++)check(x.step().buttons==0); // pressed in gameplay (no target): no LB until pressed again
  x.release();x.step();x.l().grab=true;check(x.step().buttons==M::LB);
 }
 // The native wheel keeps the grip that opened it and receives the raw right stick.
 {Rig x;x.settle();x.r().grab=true;check(x.step().buttons==M::RB);x.screen=LiveScreen::Overlay;
  x.r().stick={0,1};auto o=x.step();check(o.buttons==M::RB&&o.rightY==32767);x.r().stick={1,0};o=x.step();check(o.rightX==32767&&!(o.buttons&0x40));
 }
 // Unavailable/stale/unfocused frames are neutral; buttons held through them need a release.
 for(unsigned fault=0;fault<4;fault++){Rig x;x.settle();x.r().primaryPressed=true;x.l().stick={1,0};check(x.step().buttons==M::A);
  LivePadState o;
  switch(fault){case 0:x.screen=LiveScreen::Unavailable;o=x.step();x.screen=LiveScreen::Gameplay;break;
   case 1:x.now+=201;o=x.consume();x.receipt=x.now;break;
   case 2:x.hands.focused=false;o=x.step();x.hands.focused=true;break;
   case 3:x.binding=0;o=x.step();x.binding=1;break;}
  check(o==LivePadState{});
  for(int i=0;i<5;i++){o=x.step();check(!(o.buttons&M::A));}
  x.r().primaryPressed=false;x.step();x.r().primaryPressed=true;check(x.step().buttons&M::A);
 }
 // Session, origin and controller profile changes.
 for(unsigned change=0;change<4;change++){Rig x;x.settle();x.r().secondaryPressed=true;check(x.step().buttons==M::Y);
  switch(change){case 0:x.hands.sessionGeneration++;break;case 1:x.hands.originGeneration++;break;case 2:x.r().profile++;break;case 3:x.l().profile++;break;}
  for(int i=0;i<5;i++)check(!(x.step().buttons&M::Y));
  x.release();x.step();x.r().secondaryPressed=true;check(x.step().buttons==M::Y);
 }
 // Inactive (unbound) actions never press; a controller without face buttons stays silent.
 {Rig x;x.settle();x.r().primaryActive=false;x.r().primaryPressed=true;x.l().grabActive=false;x.l().grab=true;x.screen=LiveScreen::Ui;check(x.step().buttons==0);}
 // Temple gesture: the right stick becomes the D-pad, stick clicks Start/Select; faces still act.
 {Rig x;x.settle();x.r().grip.position={.18f,1.7f,0};for(int i=0;i<12;i++)x.step();check(x.gesture.active);
  x.r().stick={0,1};auto o=x.step();check(o.buttons==1&&o.rightY==0&&o.rightX==0);
  x.release();x.step();x.r().stickClick=true;o=x.step();check(o.buttons==0x10);
  x.release();x.step();x.r().primaryPressed=true;o=x.step();check(o.buttons==M::A);
  x.release();x.l().stick={0,1};o=x.step();check(o.leftY==32767);
 }
 check(liveScreen(false,true,0)==LiveScreen::Unavailable&&liveScreen(true,true,0)==LiveScreen::Gameplay&&liveScreen(true,false,1)==LiveScreen::Overlay&&
       liveScreen(true,false,0)==LiveScreen::Ui&&liveScreen(true,false,2)==LiveScreen::Ui);
 check(M::target(LiveScreen::Unavailable,0)==0&&M::target(LiveScreen::Gameplay,99)==0);
 // Grip hysteresis (2026-10-09, EDF6VR values): Index force press .30/release .10, value press .75/release .45.
 {using Act=Actions;
  check(!Act::gripStep(false,.29f,true)&&Act::gripStep(false,.31f,true)&&Act::gripStep(true,.11f,true)&&!Act::gripStep(true,.09f,true));
  check(!Act::gripStep(false,.74f,false)&&Act::gripStep(false,.76f,false)&&Act::gripStep(true,.46f,false)&&!Act::gripStep(true,.44f,false));
  check(!Act::gripStep(false,.6f,false)&&Act::gripStep(false,.6f,true)); // a resting hand (value .6) is not a press
 }
 // Grip trim (EDF6VR angles): a natural real hold points the grip -Z tube up-forward (60 degrees up here);
 // after the trim the tube points forward, for both hands, and the default is identity.
 {Actions act;check(act.gripTrimFor(0).w==1&&act.gripTrimFor(1).w==1);
  act.setGripTrim(4.47f,-59.27f,3.61f);const float d=3.14159265f/180;
  for(unsigned hand=0;hand<2;hand++){
   const auto natural=euler(0,60*d,0);const auto trimmed=normalized(multiply(natural,act.gripTrimFor(hand)));
   const auto tube=rotate(trimmed,{0,0,-1});check(tube.z< -.99f&&std::abs(tube.y)<.1f);
   const auto raw=rotate(natural,{0,0,-1});check(raw.y>.8f);
  }
  const auto l=act.gripTrimFor(0),r=act.gripTrimFor(1);check(std::abs(l.x-r.x)<1e-6f&&std::abs(l.y+r.y)<1e-6f&&std::abs(l.z+r.z)<1e-6f); // mirrored yaw/roll
  bool rejected=false;try{act.setGripTrim(0,std::numeric_limits<float>::quiet_NaN(),0);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 }
 std::cout<<"PASS "<<n<<" live pad mapping checks (CPU only; native routing and the real controller unverified)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
