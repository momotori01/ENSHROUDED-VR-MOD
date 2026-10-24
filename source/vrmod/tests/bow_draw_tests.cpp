#include <evr/bow_draw.hpp>
#include <evr/bow_tracked_shot.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 HandFrame f;BowPhysicalGeometry g;BowDraw t;uint64_t now=1000;LivePadState raw;
 auto reset=[&](){t={};f={};g={};g.binding=77;f.focused=true;f.sessionGeneration=f.originGeneration=1;
  for(auto& h:f.hands){h.gripActive=h.triggerActive=true;h.gripFlags=15;h.profile=1;}now=1000;raw={};raw.buttons=LivePadMapper::X|LivePadMapper::A;raw.leftTrigger=40;raw.rightTrigger=50;};
 auto at=[&](float trigger,Vec3 hand,uint64_t delta=20,bool allowed=true){++f.frame;g.frame=f.frame;f.predictedTime+=11111111;now+=delta;f.hands[1].trigger=trigger;f.hands[1].grip.position=hand;return t.update(raw,f,g,allowed,now);};
 auto step=[&](float trigger,float pull,uint64_t delta=20,bool allowed=true){return at(trigger,BowPhysicalTrial::restString+Vec3{0,0,pull},delta,allowed);};
 auto shoot=[&](){step(1,0);step(1,.35f,500);return step(0,.35f);};

 // Three shots in one session: LT while held, one 80 ms RT tap, 500 ms native aim, then re-armed.
 reset();auto p=step(0,0);ck(t.phase()==BowDraw::Idle&&!p.leftTrigger&&!p.rightTrigger&&!(p.buttons&LivePadMapper::X)&&(p.buttons&LivePadMapper::A));
 for(unsigned shot=1;shot<=3;shot++){
  p=step(1,0);ck(t.phase()==BowDraw::Held&&p.leftTrigger==255&&!p.rightTrigger);
  p=step(1,.35f,500);ck(t.phase()==BowDraw::Held&&p.leftTrigger==255&&!p.rightTrigger&&std::abs(t.metrics().pull-.35f)<1e-5f);
  p=step(0,.35f);ck(t.phase()==BowDraw::Released&&p.leftTrigger==255&&p.rightTrigger==255&&t.shots()==shot&&std::abs(t.releasePull()-.35f)<1e-5f);
  ck(step(0,.35f,79).rightTrigger==255);p=step(0,.2f,1);ck(!p.rightTrigger&&p.leftTrigger==255);
  ck(step(1,0,100).leftTrigger==255&&t.phase()==BowDraw::Released); // a press inside the aim window is not a second shot
  p=step(0,0,320);ck(t.phase()==BowDraw::Idle&&!p.leftTrigger&&!p.rightTrigger);
 }
 // VHVR: a trigger pressed away from the string takes it once the hand arrives.
 reset();step(0,0);p=at(1,BowPhysicalTrial::restString+Vec3{.3f,0,0});ck(t.phase()==BowDraw::Idle&&p.leftTrigger==255&&!t.metrics().withinRange);   // LT early (preAimRange)
 p=at(1,BowPhysicalTrial::restString+Vec3{.21f,0,0});ck(t.phase()==BowDraw::Idle);
 p=at(1,BowPhysicalTrial::restString+Vec3{.19f,0,0});ck(t.phase()==BowDraw::Held&&p.leftTrigger==255);
 // Never near: nothing is taken and nothing is sent.
 reset();step(0,0);for(unsigned i=0;i<20;i++){p=at(1,BowPhysicalTrial::restString+Vec3{.5f,0,0});ck(t.phase()==BowDraw::Idle&&!p.leftTrigger&&!p.rightTrigger);}
 // On the way to the string (within 0.45 m, the trigger held) LT already goes to the game - its nocking starts - but
 // nothing is taken; let go there, LT goes and nothing is shot.
 reset();step(0,0);p=at(1,BowPhysicalTrial::restString+Vec3{.4f,0,0});ck(t.phase()==BowDraw::Idle&&p.leftTrigger==255&&!p.rightTrigger);
 p=at(1,BowPhysicalTrial::restString+Vec3{.1f,0,0});ck(t.phase()==BowDraw::Held&&p.leftTrigger==255);
 reset();step(0,0);at(1,BowPhysicalTrial::restString+Vec3{.4f,0,0});p=at(0,BowPhysicalTrial::restString+Vec3{.4f,0,0});ck(t.phase()==BowDraw::Idle&&!p.leftTrigger&&!p.rightTrigger&&!t.shots());
 reset();t.preAim(0);step(0,0);p=at(1,BowPhysicalTrial::restString+Vec3{.4f,0,0});ck(!p.leftTrigger);
 // Short pull or too quick a release cancels without a tap, then can be taken again.
 for(float pull:{0.f,.05f,.099f}){reset();step(0,0);step(1,0);step(1,pull,500);p=step(0,pull);ck(t.phase()==BowDraw::Cancelled&&!p.rightTrigger&&!p.leftTrigger&&!t.shots());
  step(0,0);ck(t.phase()==BowDraw::Idle);ck(shoot().rightTrigger==255);}
 reset();step(0,0);step(1,0);p=step(0,.35f,20);ck(t.phase()==BowDraw::Cancelled&&!p.rightTrigger);
 // Scope loss while held or during the tap aborts; the held trigger cannot take the string again.
 for(unsigned phase=0;phase<2;phase++)for(unsigned bad=0;bad<8;bad++){
  reset();step(0,0);step(1,0);step(1,.35f,500);if(phase)step(0,.35f);
  if(bad==0)f.focused=false;if(bad==1)f.hands[1].gripFlags=0;if(bad==2)++f.sessionGeneration;if(bad==3)++f.originGeneration;if(bad==4)g.binding=88;if(bad==5)++f.hands[0].profile;if(bad==6)f.predictedTime=-100000000;
  p=step(phase?0.f:1.f,.35f,20,bad!=7);ck(t.phase()==BowDraw::Aborted&&!p.rightTrigger&&!p.leftTrigger);
  if(bad==0)f.focused=true;if(bad==1)f.hands[1].gripFlags=15;if(bad==6)f.predictedTime=2000000000;
  p=step(1,0);ck(t.phase()==BowDraw::Blocked&&!p.leftTrigger&&!p.rightTrigger);
  p=step(1,0);ck(t.phase()==BowDraw::Blocked);step(0,0);ck(t.phase()==BowDraw::Idle);ck(shoot().rightTrigger==255);
 }
 // The strong shot: the string taken with the right grip holds RT (no LT) while drawn and lets it go with the string; a
 // grip closed during a trigger's draw turns it to the strong shot's; with strongByGrip off the grip takes nothing.
 reset();step(0,0);f.hands[1].grabActive=f.hands[1].grab=true;p=step(0,0);
 ck(t.phase()==BowDraw::Held&&t.strongDraw()&&p.rightTrigger==255&&!p.leftTrigger);
 p=step(0,.35f,500);ck(t.phase()==BowDraw::Held&&p.rightTrigger==255&&!p.leftTrigger);
 f.hands[1].grab=false;p=step(0,.35f);ck(t.phase()==BowDraw::Released&&t.strongRelease()&&!p.rightTrigger&&!p.leftTrigger&&t.shots()==1);
 ck(!step(0,.35f,100).rightTrigger);p=step(0,.35f,420);ck(t.phase()==BowDraw::Idle&&!p.leftTrigger&&!p.rightTrigger);
 reset();step(0,0);step(1,0);p=step(1,.2f,300);ck(p.leftTrigger==255&&!p.rightTrigger&&!t.strongDraw());
 f.hands[1].grabActive=f.hands[1].grab=true;p=step(1,.35f,300);ck(t.strongDraw()&&p.rightTrigger==255&&!p.leftTrigger);
 f.hands[1].grab=false;p=step(0,.35f);ck(t.phase()==BowDraw::Released&&t.strongRelease()&&!p.rightTrigger);
 reset();step(0,0);step(1,0);step(1,.35f,500);p=step(0,.35f);ck(t.phase()==BowDraw::Released&&!t.strongRelease()&&p.rightTrigger==255&&p.leftTrigger==255);
 {reset();BowDrawSettings off;off.strongByGrip=false;t=BowDraw{off};f.hands[1].grabActive=f.hands[1].grab=true;step(0,0);p=step(0,0);ck(t.phase()==BowDraw::Idle&&!p.rightTrigger&&!p.leftTrigger);}
 // The mod's bow: the grip draws as the trigger does (LT, a normal shot), and it brings the game's bow on the way too.
 reset();t.gripAsTrigger(true);step(0,0);f.hands[1].grabActive=f.hands[1].grab=true;
 p=at(0,BowPhysicalTrial::restString+Vec3{.4f,0,0});ck(t.phase()==BowDraw::Idle&&p.leftTrigger==255&&!p.rightTrigger);
 p=step(0,0);ck(t.phase()==BowDraw::Held&&!t.strongDraw()&&p.leftTrigger==255&&!p.rightTrigger);
 p=step(0,.35f,500);ck(p.leftTrigger==255&&!p.rightTrigger);
 f.hands[1].grab=false;p=step(0,.35f);ck(t.phase()==BowDraw::Released&&!t.strongRelease()&&p.rightTrigger==255&&p.leftTrigger==255);
 // Off the model axis the pull is still the handle-to-hand distance behind the string,
 // so a bow hand held at an angle neither shortens the draw nor cancels the release.
 {const Vec3 off{.3f,BowPhysicalTrial::restString.y,.5f};const float expected=std::sqrt(.34f)-BowPhysicalTrial::restString.z;
  reset();step(0,0);step(1,0);p=at(1,off,500);ck(t.phase()==BowDraw::Held&&std::abs(t.metrics().pull-expected)<1e-5f);
  p=at(0,off);ck(t.phase()==BowDraw::Released&&p.rightTrigger==255&&std::abs(t.releasePull()-expected)<1e-5f);}
 // A tracking glitch far from the bow aborts instead of stretching the string.
 reset();step(0,0);step(1,0);p=step(1,1.6f);ck(t.phase()==BowDraw::Aborted&&!p.leftTrigger);
 // Without a reviewed bow the pad is untouched; after a bow was held the triggers stay consumed until released.
 reset();g.binding=0;p=step(1,0);ck(p==raw&&!t.bowMode());
 reset();step(0,0);step(1,0);ck(t.phase()==BowDraw::Held);g.binding=0;p=step(1,.2f);ck(t.phase()==BowDraw::Aborted&&!p.leftTrigger&&!p.rightTrigger&&!(p.buttons&LivePadMapper::X));
 p=step(1,.2f,400);ck(!p.rightTrigger&&t.bowMode());p=step(0,.2f,10);ck(p==raw&&!t.bowMode());
 reset();step(0,0);g.binding=0;p=step(0,0,100);ck(!p.rightTrigger&&t.bowMode());p=step(0,0,250);ck(p==raw);
 // abort() from the caller (refused tracked shot) ends the tap immediately.
 reset();step(0,0);shoot();t.abort();ck(t.phase()==BowDraw::Aborted);p=step(0,.35f);ck(t.phase()==BowDraw::Idle&&!p.leftTrigger&&!p.rightTrigger&&t.shots()==1);
 reset();step(0,0);shoot();t.abort();p=step(1,0);ck(t.phase()==BowDraw::Blocked&&!p.leftTrigger&&!p.rightTrigger);

 // VHVR pull percentage: squared, full at fullDrawLength from the handle.
 ck(bowPullPercentage(0,.24f,.6f)==0);ck(std::abs(bowPullPercentage(.18f,.24f,.6f)-.25f)<1e-5f);ck(bowPullPercentage(.36f,.24f,.6f)==1);ck(bowPullPercentage(.9f,.24f,.6f)==1);
 ck(std::abs(bowPullPercentage(.05f,.24f,.2f)-.25f)<1e-5f); // full draw is never shorter than brace + 10 cm
 reset();step(0,0);step(1,0);step(1,.18f,500);ck(std::abs(t.pullPercentage(g)-std::pow(.18f/(.6f-BowPhysicalTrial::restString.z),2.f))<1e-5f);

 // VHVR "Full" restriction: travel allowed by the game's own draw progress, Lerp(brace, full, sqrt(p)).
 ck(std::abs(bowPullLimit(0,.24f,.6f)-.0036f)<1e-6f);ck(std::abs(bowPullLimit(.25f,.24f,.6f)-.18f)<1e-6f);ck(std::abs(bowPullLimit(1,.24f,.6f)-.36f)<1e-6f);
 ck(bowPullLimit(2,.24f,.6f)==bowPullLimit(1,.24f,.6f));ck(bowPullLimit(-1,.24f,.6f)==bowPullLimit(0,.24f,.6f));
 {bool threw=false;try{bowPullLimit(NAN,.24f,.6f);}catch(const std::invalid_argument&){threw=true;}ck(threw);}
 // The full-draw pulse is offered once per hold, only at full draw.
 reset();step(0,0);ck(!t.takeFullDraw(g));step(1,0);ck(!t.takeFullDraw(g));step(1,.35f,500);ck(!t.takeFullDraw(g));step(1,.37f);ck(t.takeFullDraw(g));ck(!t.takeFullDraw(g));
 step(1,.2f);step(1,.4f);ck(!t.takeFullDraw(g));step(0,.4f);ck(t.phase()==BowDraw::Released&&!t.takeFullDraw(g));step(0,0,500);step(1,0);step(1,.4f,500);ck(t.takeFullDraw(g));

 // Two-hand bow orientation: the string axis passes through the drawing hand, the handle stays put.
 unsigned poses=0;
 for(float yaw:{-1.2f,-.3f,0.f,.7f,2.4f})for(float pitch:{-.6f,0.f,.5f})for(float roll:{-.8f,0.f,.4f})
  for(Vec3 hand:{Vec3{0,0,.5f},Vec3{.2f,-.1f,.4f},Vec3{-.3f,.25f,.3f},Vec3{.05f,.4f,.2f}})for(float elevation:{0.f,.00155006f,.15f}){
   const Pose rest{euler(yaw,pitch,roll),{.4f,1.2f,-.3f}};const auto world=rest.position+rotate(rest.orientation,hand);
   const auto drawn=bowDrawnPose(rest,world,elevation);const auto local=rotate(inverse(drawn.orientation),world-drawn.position);
   ck(length(drawn.position-rest.position)==0);ck(std::abs(local.x)<5e-5f&&std::abs(local.y-elevation)<5e-5f&&local.z>0);
   ck(std::abs(length(local)-length(hand))<5e-5f);
   // Roll comes from the bow hand: the limb axis stays in the plane of the old limb axis and the draw line.
   const auto up=rotate(drawn.orientation,{0,1,0}),hint=rotate(rest.orientation,{0,1,0}),back=(world-rest.position)*(1/length(hand));
   if(elevation==0){ck(std::abs(dot(up,cross(hint,back)))<5e-5f);ck(dot(up,hint)>0);}
   ++poses;
  }
 ck(poses==5*3*3*4*3);
 // Already aligned stays the same; a degenerate hand keeps the rest pose.
 {const Pose rest{euler(.3f,-.2f,.1f),{0,1,0}};const auto same=bowDrawnPose(rest,rest.position+rotate(rest.orientation,{0,0,.4f}),0);
  ck(length(rotate(same.orientation,{1,0,0})-rotate(rest.orientation,{1,0,0}))<1e-5f&&length(rotate(same.orientation,{0,1,0})-rotate(rest.orientation,{0,1,0}))<1e-5f);
  const auto tiny=bowDrawnPose(rest,rest.position+Vec3{0,0,.01f},0);ck(tiny.orientation.w==rest.orientation.w&&tiny.orientation.x==rest.orientation.x);
  bool threw=false;try{bowDrawnPose(rest,{NAN,0,0},0);}catch(const std::invalid_argument&){threw=true;}ck(threw);}

 // One token per shot: a fresh BowTrackedShot per release, with the longer play window.
 {BowTrackedShot shot;BowTrackedShotValue out;HandFrame h;BowPhysicalGeometry b;h.frame=b.frame=720;h.sessionGeneration=h.originGeneration=1;h.predictedTime=100000000;h.focused=true;
  for(auto& x:h.hands){x.gripActive=true;x.gripFlags=15;x.profile=1;}h.hands[1].grip.position={.03f,.06f,.59f};b.binding=77;b.sourceWorld=123;b.entity=1;b.worldPosition={150000000000ll,160000000000ll,170000000000ll};
  ck(shot.arm(h,b,1000));h.frame=b.frame=735;ck(!BowTrackedShot(shot).consume(h,b,1400,1,out));ck(shot.consume(h,b,1400,1,out,500,30));
  shot={};h.frame=b.frame=900;ck(shot.arm(h,b,3000));h.frame=b.frame=904;ck(shot.consume(h,b,3100,1,out,500,30)&&out.frame==900);
  shot={};ck(shot.arm(h,b,4000));h.frame=b.frame=940;ck(!shot.consume(h,b,4100,1,out,500,30));
  shot={};h.frame=b.frame=950;ck(shot.arm(h,b,5000));ck(!shot.consume(h,b,5501,1,out,500,30));
  // 79 degrees off the model axis: refused by the one-shot diagnostic, aimed along the hands by the draw route.
  shot={};h.hands[1].grip.position={.5f,0,.1f};ck(!shot.arm(h,b,6000));ck(shot.arm(h,b,6000,-1.f));
  const auto d=shot.value().aim.direction;ck(std::abs(length(d)-1)<1e-5f&&d.x<-.9f);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
