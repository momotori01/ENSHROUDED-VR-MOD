#include <evr/live_wield.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned count=0;auto require=[&](bool b){++count;if(!b)throw std::runtime_error("live wield invariant "+std::to_string(count));};
 const NativeMeleeMount p{true,{0,0,0,1},{.01f,.63f,-.03f}};const Vec3 scale{1.0002f,1.001f,1.0014f};
 HandFrame f;f.focused=true;f.sessionGeneration=f.originGeneration=1;
 for(auto& h:f.hands){h.gripActive=h.grabActive=true;h.gripFlags=15;h.profile=1;}
 f.hands[1].grip.position={.2f,1.3f,-.7f};
 LiveWield state;auto step=[&](bool allowed=true){++f.frame;f.predictedTime+=11111111;return state.update(f,123,p,scale,mountNativeMelee(f.hands[1].grip,p,scale),allowed);};
 auto single=mountNativeMelee(f.hands[1].grip,p,scale);f.hands[0].grip.position=single.position;
 // Held at startup must release before acquisition, as with the input mapper.
 f.hands[0].grab=f.hands[1].grab=true;require(!step().active);
 f.hands[1].grab=false;require(!step().active);f.hands[1].grab=true;auto out=step();require(out.active);
 const Vec3 fixedLeft=f.hands[0].grip.position;const auto initialRight=f.hands[1].grip.position;
 float lo=10,hi=-10;
 for(unsigned i=0;i<40;i++){
  f.hands[1].grip.position=initialRight;f.hands[1].grip.position.y-=.18f*float(i)/39;
  out=step();require(out.active);lo=std::min(lo,out.mainY);hi=std::max(hi,out.mainY);
  const Vec3 a{p.gripInModel.x*scale.x,out.mainY*scale.y,p.gripInModel.z*scale.z},b{0,out.offY*scale.y,0};
  require(length(out.pose.position+rotate(out.pose.orientation,a)-f.hands[1].grip.position)<1e-5f);
  require(length(out.pose.position+rotate(out.pose.orientation,b)-fixedLeft)<1e-5f);
 }
 require(hi-lo>.17f);
 f.hands[0].grab=false;require(!step().active);f.hands[0].grab=true;require(!step().active); // release right before reacquiring
 f.hands[1].grab=false;step();f.hands[0].grip.position={5,5,5};f.hands[1].grab=true;require(!step().active);
 for(unsigned fault=0;fault<5;fault++){
  state.reset();f.hands[1].grip.position=initialRight;single=mountNativeMelee(f.hands[1].grip,p,scale);f.hands[0].grip.position=single.position;
  f.hands[1].grab=false;step();f.hands[1].grab=true;require(step().active);
  if(fault==0)f.focused=false;if(fault==1)f.hands[0].gripFlags=0;if(fault==2)f.originGeneration++;
  if(fault==3)f.predictedTime+=300000000;
  require(!step(fault!=4).active);
  f.focused=true;f.hands[0].gripFlags=15;require(!step().active);
 }
 // VHVR entry: the left grip, hands along the weapon; no reach to the handle, no re-press, no right grip.
 {const NativeMeleeMount v{true,{0,0,0,1},{0,.63f,0},-.33f,1.43f};const Vec3 one{1,1,1};LiveWield wield(true);HandFrame h;h.focused=true;h.sessionGeneration=h.originGeneration=1;
  for(auto& x:h.hands){x.gripActive=x.grabActive=true;x.gripFlags=15;x.profile=1;x.grab=true;}
  const Vec3 rightAt{.2f,1.3f,-.5f};h.hands[1].grip.position=rightAt;
  auto run=[&](bool allowed=true){++h.frame;h.predictedTime+=11111111;return wield.update(h,9,v,one,mountNativeMelee(h.hands[1].grip,v,one),allowed);};
  const auto along=[&](float y,float side=0){return rightAt+Vec3{side,y,0};}; // the weapon points along +Y from the right hand
  // Left hand 80 cm behind on the axis: two-handed at once, left on the rear grip, right 80 cm up the handle.
  h.hands[0].grip.position=along(-.8f);auto w=run();require(w.active&&std::abs(w.offY)<1e-5f&&std::abs(w.mainY-.8f)<1e-5f&&std::abs(w.distance-.8f)<1e-5f);
  require(length(w.pose.position-h.hands[0].grip.position)<1e-5f&&length(rotate(w.pose.orientation,{0,1,0})-Vec3{0,1,0})<1e-5f);
  // Closer than the grip spacing: the right hand keeps the main grip, the left slides up.
  h.hands[0].grip.position=along(-.4f);w=run();require(w.active&&std::abs(w.mainY-.63f)<1e-5f&&std::abs(w.offY-.23f)<1e-5f);
  require(length(w.pose.position+rotate(w.pose.orientation,{0,.63f,0})-rightAt)<1e-5f);
  // Once two-handed the weapon follows the line between the hands, whatever its angle.
  h.hands[0].grip.position=rightAt+Vec3{-.4f,0,0};w=run();require(w.active&&length(rotate(w.pose.orientation,{0,1,0})-Vec3{1,0,0})<1e-5f);
  require(length(w.pose.position+rotate(w.pose.orientation,{0,w.mainY,0})-rightAt)<1e-5f&&length(w.pose.position+rotate(w.pose.orientation,{0,w.offY,0})-h.hands[0].grip.position)<1e-5f);
  // The left grip released: one hand again; pressing again re-enters only when the hands line up.
  h.hands[0].grab=false;require(!run().active);h.hands[0].grab=true;require(!run().active); // hands are now across the single-hand axis
  // The right grip plays no part: the weapon is in that hand anyway (it stays the quick wheel).
  h.hands[0].grip.position=along(-.5f);require(run().active);h.hands[1].grab=false;require(run().active);h.hands[1].grabActive=false;require(run().active);
  h.hands[0].grab=false;require(!run().active);h.hands[0].grab=true;require(run().active);h.hands[1].grab=h.hands[1].grabActive=true;require(run().active);
  // Entry limits: 60 degrees off the axis, and half the weapon length along it (1.76 m weapon -> 0.88 m).
  wield.reset();h.hands[0].grip.position=along(-.3f,.6f);require(!run().active);   // 63 degrees
  h.hands[0].grip.position=along(-.3f,.45f);require(run().active);                 // 56 degrees
  wield.reset();h.hands[0].grip.position=along(-.9f);require(!run().active);h.hands[0].grip.position=along(-.85f);require(run().active);
  // The other way round: the left hand ahead of the right makes the right hand the rear one.
  wield.reset();h.hands[0].grip.position=along(.5f);w=run();require(w.active&&std::abs(w.offY-.63f)<1e-5f&&std::abs(w.mainY-.13f)<1e-5f);
  require(length(rotate(w.pose.orientation,{0,1,0})-Vec3{0,1,0})<1e-5f&&length(w.pose.position+rotate(w.pose.orientation,{0,.63f,0})-h.hands[0].grip.position)<1e-5f);
  // Scope loss ends it. A tracking break (origin change, time gap) asks the entry rule again,
  // so hands that have since turned across the single-hand axis fall back to one hand.
  for(unsigned fault=0;fault<5;fault++){wield.reset();h.hands[0].grip.position=along(-.5f);require(run().active);
   h.hands[0].grip.position=rightAt+Vec3{-.4f,0,0};require(run().active);
   if(fault==0)h.focused=false;if(fault==1)h.hands[0].gripFlags=0;if(fault==2)h.originGeneration++;if(fault==3)h.predictedTime+=300000000;
   require(!run(fault!=4).active);h.focused=true;h.hands[0].gripFlags=15;}
  LiveWield unknown(true);NativeMeleeMount none{};require(!unknown.update(h,9,none,one,identityPose(),true).active);
  // Staff on the plain right-hand mount (origin in the right hand), VHVR Magic geometry.
  {const std::array<uint32_t,4> staffGuid{2530149898u,1281451878u,1386785673u,341680974u};
   const auto staff=twoHandHoldProfile(1255239784u,5,staffGuid,8);
   require(staff.known&&staff.rearY==-.6988f&&staff.tipY==.8356f&&staff.rearReach==.6988f&&std::abs(staff.mainBehind-.1747f)<1e-6f&&staff.gripInModel.y==0);
   require(!twoHandHoldProfile(1255239784u,6,staffGuid,8).known&&!twoHandHoldProfile(1255239784u,5,staffGuid,9).known&&!twoHandHoldProfile(1255239784u,5,{1,2,3,4},8).known);
   LiveWield hold(true);require(hold.vhvrEntry()&&!LiveWield().vhvrEntry());
   auto cast=[&]{++h.frame;h.predictedTime+=11111111;return hold.update(h,11,staff,one,h.hands[1].grip,true);}; // single pose: the right grip itself
   // Weapon hand in front, hands 40 cm apart: the origin stays in the right hand, the left hand is 40 cm down the staff.
   h.hands[0].grip.position=along(-.4f);auto s=cast();require(s.active&&std::abs(s.mainY)<1e-5f&&std::abs(s.offY+.4f)<1e-5f&&length(s.pose.position-rightAt)<1e-5f);
   // Farther apart than the rear end allows: the left hand holds the rear end and the right hand slides up.
   h.hands[0].grip.position=along(-.8f);s=cast();require(s.active&&std::abs(s.offY+.6988f)<1e-5f&&std::abs(s.mainY-.1012f)<1e-5f);
   require(length(s.pose.position+rotate(s.pose.orientation,{0,s.offY,0})-h.hands[0].grip.position)<1e-5f);
   // Weapon hand behind: it sits a quarter of the rear length behind the origin, whatever the spacing.
   hold.reset();h.hands[0].grip.position=along(.5f);s=cast();require(s.active&&std::abs(s.mainY+.1747f)<1e-5f&&std::abs(s.offY-.3253f)<1e-5f);
   h.hands[0].grip.position=along(.3f);s=cast();require(s.active&&std::abs(s.mainY+.1747f)<1e-5f&&std::abs(s.offY-.1253f)<1e-5f);
   require(length(s.pose.position+rotate(s.pose.orientation,{0,s.mainY,0})-rightAt)<1e-5f&&length(rotate(s.pose.orientation,{0,1,0})-Vec3{0,1,0})<1e-5f);
   // The weapon axis is the line between the hands: turning the front hand aside turns the aim.
   h.hands[0].grip.position=rightAt+Vec3{.3f,.3f,0};s=cast();require(s.active&&length(rotate(s.pose.orientation,{0,1,0})-Vec3{.70710678f,.70710678f,0})<1e-5f);}
  // The catalog: every two-hand melee weapon and staff of the game data has its hold.
  {const std::array<uint32_t,4> axeGuid{494587688u,1236792456u,3139207297u,1775552796u},hammerGuid{963158432u,1159201196u,1874105256u,641260995u},swordGuid{988234869u,1209454596u,4028247965u,2243415596u};
   unsigned left=0,right=0,staffs=0;
   for(const auto& a:catalogTwoHandAssets){
    require(catalogTwoHandAsset(a.item,a.guid)==&a&&a.rearY<0&&a.tipY>a.rearY+.9f&&(a.slot==8||a.slot==9));
    const auto mount=catalogMeleeMount(a.item,a.type,a.guid,a.slot),hold=twoHandHoldProfile(a.item,a.type,a.guid,a.slot);
    require(mount.known!=hold.known);                                   // one of the two rules, never both
    require(mount.known==(a.slot==9)&&(a.slot==9)==(a.type>=7&&a.type<=9));
    const auto& p=mount.known?mount:hold;require(p.rearY==a.rearY&&p.tipY==a.tipY);
    if(mount.known){require(mount.gripInModel.y==twoHandMeleeVrGripY&&mount.rearReach==0&&mount.mainBehind==0);left++;}
    else if(a.type==5){require(hold.rearReach==-a.rearY&&std::abs(hold.mainBehind+.25f*a.rearY)<1e-6f);staffs++;}
    else{require(a.type==10&&std::abs(hold.rearReach-std::max(-.75f*a.rearY,.125f))<1e-6f&&hold.mainBehind==0);right++;}
   }
   require(left==68&&right==39&&staffs==49);   // 1.0 (June 54, 30, 40)
   // Measured assets keep their own palm offset and orientation; the group mount differs from them by about a
   // millimetre and a degree. Both hold at the VR grip height, not at the game's own 0.63 m (kept on record).
   const auto measured=catalogMeleeMount(1940764111u,7,axeGuid,9),family=catalogFamilyMount(1940764111u,7,axeGuid,9),native=nativeMeleeMount(1940764111u,7,axeGuid);
   require(native.known&&native.gripInModel.y==.633441743f&&measured.known&&measured.gripInModel.y==twoHandMeleeVrGripY&&measured.gripInModel.x==native.gripInModel.x);
   require(family.known&&family.gripInModel.y==twoHandMeleeVrGripY&&family.gripInModel.x!=measured.gripInModel.x);
   require(length(family.gripInModel-measured.gripInModel)<.002f&&std::abs(dot(rotate(family.orientation,{0,1,0}),rotate(measured.orientation,{0,1,0}))-1)<.001f);
   // An unmeasured hammer of the left-hand group, wrong slot, wrong type, wrong model.
   const auto hammer=catalogMeleeMount(7015425u,9,hammerGuid,9);require(hammer.known&&hammer.rearY==-.3687f&&hammer.tipY==1.3033f);
   require(!catalogMeleeMount(7015425u,9,hammerGuid,8).known&&!catalogMeleeMount(7015425u,8,hammerGuid,9).known&&!catalogMeleeMount(7015425u,9,axeGuid,9).known&&!catalogMeleeMount(1,9,hammerGuid,9).known);
   // A great sword stays in the right hand: the left hand joins below it, at most three quarters of the hilt back.
   const auto sword=twoHandHoldProfile(19063957u,10,swordGuid,8);require(sword.known&&std::abs(sword.rearReach-.312225f)<1e-6f&&!catalogMeleeMount(19063957u,10,swordGuid,8).known);
   LiveWield hold(true);auto swing=[&]{++h.frame;h.predictedTime+=11111111;return hold.update(h,12,sword,one,h.hands[1].grip,true);};
   h.hands[0].grip.position=along(-.2f);auto g=swing();require(g.active&&std::abs(g.mainY)<1e-5f&&std::abs(g.offY+.2f)<1e-5f&&length(g.pose.position-rightAt)<1e-5f);
   h.hands[0].grip.position=along(-.5f);g=swing();require(g.active&&std::abs(g.offY+.312225f)<1e-5f&&std::abs(g.mainY-.187775f)<1e-5f);}
  // Bows: the reviewed one always, every other catalog bow with the catalog display route.
  {const std::array<uint32_t,4> reviewed{782483182u,1119002633u,871164063u,3497776913u},other{2167670689u,1113590387u,520167602u,4039637253u};
   unsigned bows=0;for(const auto& a:catalogRangedAssets){const bool bow=catalogRangedRigs[a.rig].type==4;bows+=bow;require(catalogBowItem(a.item)==bow&&bowDrawAsset(a.item,a.guid,true)==bow);}
   require(bows==62&&!catalogBowItem(0)&&!catalogBowItem(1940764111u));
   require(bowDrawAsset(327157552u,reviewed,false)&&!bowDrawAsset(375831467u,other,false)&&bowDrawAsset(375831467u,other,true)&&!bowDrawAsset(375831467u,reviewed,true));}
  // The measured-contact trial keeps its own rules.
  LiveWield legacy;h.hands[0].grip.position=along(-.8f);++h.frame;h.predictedTime+=11111111;require(!legacy.update(h,9,v,one,mountNativeMelee(h.hands[1].grip,v,one),true).active);}
 std::cout<<"PASS "<<count<<" live two-hand display, sliding contact, release and interruption checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
