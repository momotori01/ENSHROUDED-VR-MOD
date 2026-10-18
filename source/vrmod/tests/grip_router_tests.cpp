#include <evr/grip_router.hpp>
#include <tuple>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
// Synthetic test calibration only (not product body dimensions).
GripRouterCalibration calibration(){
 GripRouterCalibration c;c.rightShoulder={.2f,.45f,-.05f};c.leftShoulder={-.2f,.45f,-.05f};c.rightHip={.2f,-.05f,0};c.leftHip={-.2f,-.05f,0};
 c.enterRadius=.12f;c.exitRadius=.16f;c.calmSpeed=.3f;c.calmTime=200000000;c.longPress=400000000;c.maximumGap=100000000;return c;
}
struct Rig {
 GripRouter router{calibration()};HandFrame f;GuardBody body;GripAuthority n;
 Rig(){f.sessionGeneration=f.originGeneration=1;f.focused=true;
  for(auto& h:f.hands){h.gripActive=h.grabActive=true;h.gripFlags=15;h.profile=1;}
  body.navel={0,1,0};body.head={0,1.6f,0};body.forward={0,0,-1};body.known=true;body.calibration=1;
  n.enabled=n.known=n.local=n.alive=true;n.uiOrEvent=false;
  neutralPositions();}
 // In front of the body, outside every zone.
 void neutralPositions(){f.hands[0].grip.position={-.25f,1.1f,-.4f};f.hands[1].grip.position={.25f,1.1f,-.4f};}
 Vec3 zone(GripZoneOffset o){return {body.navel.x+o.right,body.navel.y+o.up,body.navel.z-o.forward};}
 GripRoute step(XrDuration dt=11111111){f.frame++;f.predictedTime+=dt;body.frame=f.frame;body.session=f.sessionGeneration;body.origin=f.originGeneration;body.time=f.predictedTime;return router.update(f,body,n);}
 GripRoute steps(unsigned count){GripRoute r;for(unsigned i=0;i<count;i++)r=step();return r;}
};
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("grip router invariant "+std::to_string(n));};
 const auto c=calibration();
 // A grip already held at start (or after any reset) must be released first.
 {Rig r;r.f.hands[1].grab=true;r.f.hands[1].grip.position=r.zone(c.rightShoulder);
  for(int i=0;i<60;i++){auto o=r.step();check(o.valid&&o.equip==EquipRequest::None&&!o.wheelHeld);}
  r.f.hands[1].grab=false;r.step();r.f.hands[1].grab=true;auto o=r.step();check(o.equip==EquipRequest::MainWeapon&&o.equipHand==1);}
 // Each same-side zone yields exactly one edge; holding never repeats or opens the wheel.
 for(auto [hand,zone,request]:{std::tuple{1u,GripZone::RightShoulder,EquipRequest::MainWeapon},std::tuple{1u,GripZone::RightHip,EquipRequest::Staff},
                                std::tuple{0u,GripZone::LeftShoulder,EquipRequest::Bow},std::tuple{0u,GripZone::LeftHip,EquipRequest::Shield}}){
  Rig r;r.step();r.f.hands[hand].grip.position=r.zone(gripZoneOffset(c,zone));r.f.hands[hand].grab=true;
  auto o=r.step();check(o.equip==request&&o.equipHand==hand&&o.hands[hand].consumed&&o.hands[hand].pressZone==zone&&o.attackRearmRequired[hand]);
  for(int i=0;i<120;i++){o=r.step();check(o.equip==EquipRequest::None&&!o.wheelHeld&&!o.supportHeld&&o.hands[hand].consumed);}
  r.f.hands[hand].grab=false;o=r.step();check(o.equip==EquipRequest::None&&!o.hands[hand].held);}
 // Crossing the body never equips (right hand on the left shoulder).
 {Rig r;r.step();r.f.hands[1].grip.position=r.zone(c.leftShoulder);r.f.hands[1].grab=true;auto o=r.step();check(o.equip==EquipRequest::None&&!o.hands[1].consumed);}
 // Right press outside the zones: wheel only after the long-press time, held until release.
 {Rig r;r.step();r.f.hands[1].grab=true;GripRoute o;unsigned frames=0;
  do{o=r.step();++frames;}while(!o.wheelHeld&&frames<100);
  check(o.wheelHeld&&frames>=36&&frames<=38);
  for(int i=0;i<50;i++){o=r.step();check(o.wheelHeld&&o.equip==EquipRequest::None);}
  // Drifting into a zone while the wheel press is held never equips.
  r.f.hands[1].grip.position=r.zone(c.rightShoulder);for(int i=0;i<20;i++){o=r.step();check(o.wheelHeld&&o.equip==EquipRequest::None&&o.hands[1].zone==GripZone::RightShoulder&&!o.hands[1].consumed);}
  r.f.hands[1].grab=false;o=r.step();check(!o.wheelHeld);}
 // Short outside press: no wheel, no equip.
 {Rig r;r.step();r.f.hands[1].grab=true;GripRoute o;for(int i=0;i<30;i++){o=r.step();check(!o.wheelHeld);}r.f.hands[1].grab=false;o=r.step();check(!o.wheelHeld&&o.equip==EquipRequest::None);}
 // A zone press that leaves the zone stays consumed: no wheel.
 {Rig r;r.step();r.f.hands[1].grip.position=r.zone(c.rightShoulder);r.f.hands[1].grab=true;r.step();r.neutralPositions();
  for(int i=0;i<80;i++){auto o=r.step();check(!o.wheelHeld&&o.hands[1].consumed);}}
 // Left grip outside zones is the support level; the open native wheel removes it.
 {Rig r;r.step();r.f.hands[0].grab=true;auto o=r.step();check(o.supportHeld&&o.equip==EquipRequest::None);
  r.n.wheelOverlay=true;o=r.step();check(!o.supportHeld);r.n.wheelOverlay=false;o=r.step();check(o.supportHeld);
  r.f.hands[0].grip.position=r.zone(c.leftShoulder);o=r.step();check(o.supportHeld&&o.equip==EquipRequest::None);}
 // With the wheel visible a new right press is never classified into a zone.
 {Rig r;r.step();r.n.wheelOverlay=true;r.f.hands[1].grip.position=r.zone(c.rightShoulder);r.f.hands[1].grab=true;auto o=r.step();check(o.equip==EquipRequest::None);}
 // UI/event cancels held presses; they must be released afterwards.
 {Rig r;r.step();r.f.hands[1].grab=true;r.steps(40);check(r.step().wheelHeld);
  r.n.uiOrEvent=true;auto o=r.step();check(!o.wheelHeld&&o.blocked==GripBlock::UiOrEvent);
  r.n.uiOrEvent=false;for(int i=0;i<60;i++){o=r.step();check(!o.wheelHeld);}
  r.f.hands[1].grab=false;r.step();r.f.hands[1].grip.position=r.zone(c.rightShoulder);r.f.hands[1].grab=true;o=r.step();check(o.equip==EquipRequest::MainWeapon);}
 // 2026-10-05 18:50: a temple gesture owns only the stick. Grips keep routing (equip, wheel, support).
 {Rig r;r.step();r.f.hands[1].grab=true;r.steps(40);check(r.step().wheelHeld);
  r.n.gestureActive=true;auto o=r.step();check(o.wheelHeld&&o.blocked==GripBlock::None&&o.gestureOverlap&&o.valid);
  r.f.hands[1].grab=false;r.step();r.f.hands[1].grip.position=r.zone(c.rightShoulder);r.f.hands[1].grab=true;o=r.step();
  check(o.equip==EquipRequest::MainWeapon&&o.gestureOverlap);
  r.f.hands[0].grab=true;o=r.step();check(o.supportHeld);
  r.f.hands[0].grab=false;r.step();r.f.hands[0].grip.position=r.zone(c.leftShoulder);r.f.hands[0].grab=true;o=r.step();check(o.equip==EquipRequest::Bow);
  r.n.gestureActive=false;o=r.step();check(!o.gestureOverlap&&o.equip==EquipRequest::None);}
 // Tracking loss, focus loss, gaps and session/origin/calibration changes reset and require release.
 for(int mode=0;mode<6;mode++){Rig r;r.step();r.f.hands[1].grip.position=r.zone(c.rightHip);r.f.hands[1].grab=true;check(r.step().equip==EquipRequest::Staff);
  GripRoute o;
  if(mode==0){r.f.hands[1].gripFlags=3;o=r.step();check(!o.valid&&o.blocked==GripBlock::Tracking);r.f.hands[1].gripFlags=15;}
  if(mode==1){r.f.focused=false;o=r.step();check(!o.valid);r.f.focused=true;}
  if(mode==2){o=r.step(200000000);check(!o.valid&&o.blocked==GripBlock::Discontinuity);}
  if(mode==3){r.f.sessionGeneration++;o=r.step();check(!o.valid);}
  if(mode==4){r.f.originGeneration++;o=r.step();check(!o.valid);}
  if(mode==5){r.body.calibration++;o=r.step();check(!o.valid&&o.blocked==GripBlock::Discontinuity);}
  for(int i=0;i<30;i++){o=r.step();check(o.equip==EquipRequest::None);}
  r.f.hands[1].grab=false;r.step();r.f.hands[1].grab=true;o=r.step();check(o.equip==EquipRequest::Staff);}
 // Equip re-arm: the drawing hand moves fast; attacks re-arm only after a calm period.
 {Rig r;r.step();r.f.hands[1].grip.position=r.zone(c.rightShoulder);r.f.hands[1].grab=true;auto o=r.step();check(o.attackRearmRequired[1]);
  for(int i=0;i<40;i++){r.f.hands[1].grip.position.z-=.02f;o=r.step();check(o.attackRearmRequired[1]);} // 1.8 m/s pull
  unsigned calm=0;do{o=r.step();++calm;}while(o.attackRearmRequired[1]&&calm<100);
  check(!o.attackRearmRequired[1]&&calm>=18&&calm<=20);check(!o.attackRearmRequired[0]);}
 // Hysteresis: a zone press keeps its zone up to the exit radius.
 {Rig r;r.step();auto p=r.zone(c.rightShoulder);r.f.hands[1].grip.position=p;r.f.hands[1].grab=true;r.step();
  r.f.hands[1].grip.position={p.x+.14f,p.y,p.z};auto o=r.step();check(o.hands[1].zone==GripZone::RightShoulder);
  r.f.hands[1].grip.position={p.x+.17f,p.y,p.z};o=r.step();check(o.hands[1].zone==GripZone::None);}
 // Body frame must match the hand frame and be horizontal.
 {Rig r;r.step();r.body.forward={0,.5f,-.866f};r.f.frame++;r.f.predictedTime+=11111111;r.body.frame=r.f.frame;r.body.time=r.f.predictedTime;auto o=r.router.update(r.f,r.body,r.n);check(!o.valid&&o.blocked==GripBlock::Invalid);}
 {Rig r;r.f.frame++;r.f.predictedTime+=11111111;r.body.frame=r.f.frame+1;r.body.session=r.body.origin=1;r.body.time=r.f.predictedTime;check(r.router.update(r.f,r.body,r.n).blocked==GripBlock::Invalid);}
 // Body yaw: zones rotate with the calibrated body direction.
 {Rig r;r.body.forward={1,0,0};r.step();const Vec3 right{0,0,1};
  r.f.hands[1].grip.position=r.body.navel+right*c.rightShoulder.right+Vec3{0,c.rightShoulder.up,0}+r.body.forward*c.rightShoulder.forward;r.f.hands[1].grab=true;
  check(r.step().equip==EquipRequest::MainWeapon);}
 // Calibration is mandatory and self-consistent.
 {auto bad=c;bad.exitRadius=bad.enterRadius;bool threw=false;try{GripRouter x(bad);}catch(const std::invalid_argument&){threw=true;}check(threw);
  bad=c;bad.rightShoulder.right=-.2f;threw=false;try{GripRouter x(bad);}catch(const std::invalid_argument&){threw=true;}check(threw);
  bad=c;bad.longPress=0;threw=false;try{GripRouter x(bad);}catch(const std::invalid_argument&){threw=true;}check(threw);}
 // ---- Head-relative shoulder reach (VHVR back-reach geometry as design reference) ----
 {auto rc=c;rc.rightShoulder={};rc.leftShoulder={};rc.shoulderReach={.25f,.125f,.5f,0.f,.04f};
  struct Reach : Rig {explicit Reach(GripRouterCalibration k){router=GripRouter(k);}
   Vec3 at(float right,float up,float forward)const{const Vec3 rr=cross(body.forward,Vec3{0,1,0});return body.head+rr*right+Vec3{0,up,0}+body.forward*forward;}};
  auto press=[&](Reach& r,unsigned hand,Vec3 p){r.f.hands[hand].grab=false;r.step();r.f.hands[hand].grip.position=p;r.f.hands[hand].grab=true;auto o=r.step();r.f.hands[hand].grab=false;r.step();r.neutralPositions();return o;};
  // Same-side points beside/behind the head between 25 cm below and 12.5 cm above it equip.
  for(auto [x,y,z]:{std::tuple{.2f,-.1f,-.1f},std::tuple{.18f,0.f,0.f},std::tuple{.05f,.12f,-.2f},std::tuple{.48f,-.24f,-.3f},std::tuple{.3f,-.2f,0.f},std::tuple{.01f,0.f,-.15f}}){
   Reach r(rc);r.step();check(press(r,1,r.at(x,y,z)).equip==EquipRequest::MainWeapon);
   Reach l(rc);l.step();check(press(l,0,l.at(-x,y,z)).equip==EquipRequest::Bow);}
  // Outside: in front of the face, too low/high, too far out, or across the body.
  for(auto [x,y,z]:{std::tuple{.2f,-.1f,.03f},std::tuple{.2f,-.27f,-.1f},std::tuple{.2f,.14f,-.1f},std::tuple{.52f,-.1f,-.1f},std::tuple{-.05f,-.1f,-.1f}}){
   Reach r(rc);r.step();auto o=press(r,1,r.at(x,y,z));check(o.equip==EquipRequest::None&&!o.hands[1].consumed);}
  // The temple gesture overlaps the shoulder region: the grip still equips.
  {Reach r(rc);r.step();r.n.gestureActive=true;check(press(r,1,r.at(.18f,0,0)).equip==EquipRequest::MainWeapon);}
  // Keep margin: a press inside stays reported up to 4 cm past every bound.
  {Reach r(rc);r.step();r.f.hands[1].grip.position=r.at(.2f,-.24f,-.1f);r.f.hands[1].grab=true;check(r.step().equip==EquipRequest::MainWeapon);
   r.f.hands[1].grip.position=r.at(.2f,-.28f,.03f);check(r.step().hands[1].zone==GripZone::RightShoulder);
   r.f.hands[1].grip.position=r.at(.2f,-.30f,-.1f);check(r.step().hands[1].zone==GripZone::None);}
  // Rotates with the calibrated body yaw.
  {Reach r(rc);r.body.forward={1,0,0};r.step();check(press(r,1,r.at(.2f,-.1f,-.1f)).equip==EquipRequest::MainWeapon);
   Reach q(rc);q.body.forward={1,0,0};q.step();check(press(q,1,q.body.head+Vec3{-.1f,-.1f,-.2f}).equip==EquipRequest::None);}
  // Hips keep their calibrated spheres; outside-zone presses still open the wheel.
  {Reach r(rc);r.step();check(press(r,1,r.zone(c.rightHip)).equip==EquipRequest::Staff);
   Reach w(rc);w.step();w.f.hands[1].grip.position=w.at(.2f,-.1f,.1f);w.f.hands[1].grab=true;GripRoute o;for(int i=0;i<40;i++)o=w.step();check(o.wheelHeld&&o.equip==EquipRequest::None);}
  // The head position is required.
  {Reach r(rc);r.body.head={std::numeric_limits<float>::quiet_NaN(),0,0};check(r.step().blocked==GripBlock::Invalid);}
  // Reach configuration bounds.
  for(int k=0;k<6;k++){auto bad=rc;switch(k){case 0:bad.shoulderReach.below=0;break;case 1:bad.shoulderReach.above=-.1f;break;case 2:bad.shoulderReach.lateral=1.5f;break;
    case 3:bad.shoulderReach.keepMargin=-.01f;break;case 4:bad.shoulderReach.forward=.4f;break;case 5:bad.shoulderReach.below=std::numeric_limits<float>::infinity();break;}
   bool threw=false;try{GripRouter x(bad);}catch(const std::invalid_argument&){threw=true;}check(threw);}
  {auto bad=c;bad.shoulderReach.lateral=-.1f;bool threw=false;try{GripRouter x(bad);}catch(const std::invalid_argument&){threw=true;}check(threw);}}
 // Native authority.
 {Rig r;r.n.enabled=false;check(r.step().blocked==GripBlock::Disabled);r.n.enabled=true;r.n.alive=false;check(r.step().blocked==GripBlock::NativeUnknown);}
 std::cout<<"PASS "<<n<<" grip router checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
