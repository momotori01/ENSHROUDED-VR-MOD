#include <evr/weapon_mount.hpp>
#include <iostream>
using namespace evr;
int main(){try{
 auto require=[](bool ok){if(!ok)throw std::runtime_error("weapon mount invariant");};
 // Every admitted item/model has an ordered native rig; adjacent IDs and
 // altered resources must not borrow a profile. Offline metadata is not a
 // permission to mutate the source pose or enable a default live trial.
 unsigned counts[3]={};uint32_t previous=0;
 for(const auto& asset:motion::catalogRangedAssets){
  require(asset.item>previous);previous=asset.item;require(asset.rig<motion::catalogRangedRigs.size());
  require(motion::catalogRangedAsset(asset.item,asset.guid)==&asset);
  const auto& r=motion::catalogRangedRigs[asset.rig];require(r.type>=4&&r.type<=6);++counts[r.type-4];
  require(r.count==(r.type==4?6u:3u));require(r.skin>0&&r.skin<=5);
  for(unsigned i=0;i<r.count;++i)require(r.parents[i]==65535||r.parents[i]<i);
  for(unsigned i=0;i<r.skin;++i)require(r.skinMap[i]<r.count&&(i==0||r.skinMap[i]>r.skinMap[i-1]));
  auto wrong=asset.guid;wrong[0]^=1;require(!motion::catalogRangedAsset(asset.item,wrong));
  require(!motion::catalogRangedAsset(0,asset.guid));require(motion::catalogBowAsset(asset.item,asset.guid)==(r.type==4));
 }
 require(counts[0]==62&&counts[1]==49&&counts[2]==58);   // 1.0 (June 49, 40, 43)
 const Pose grip{euler(.6f,.2f,-.4f),{.3f,1.2f,-.4f}};
 const auto mounted=motion::mountLiveWeapon(grip);
 require(length(mounted.position-grip.position)<1e-7f);
 const auto relative=compose(inverse(grip),mounted);
 const auto tip=rotate(relative.orientation,{0,0,-1});
 require(std::abs(tip.x)<1e-6f&&std::abs(tip.y+std::sin(35*pi/180))<1e-6f&&std::abs(tip.z+std::cos(35*pi/180))<1e-6f);
 require(length(rotate(motion::mountLiveWeapon(grip,0).orientation,{0,0,-1})-rotate(grip.orientation,{0,0,-1}))<1e-6f);
 const auto reverse=compose(inverse(grip),motion::mountLiveWeapon(grip,35));require(rotate(reverse.orientation,{0,0,-1}).y>0);
 // A carried tool (the watering can) hangs under the fist: the spout (the model's +Y) where the hand points, the
 // body (the model's +Z, which the mount turns as -Z) straight down - whatever the hand's turn about that line.
 {auto body=[](const Pose& p){return rotate(p.orientation,{0,0,-1});};auto spout=[](const Pose& p){return rotate(p.orientation,{0,1,0});};
  auto near=[](Vec3 a,Vec3 b){return length(a-b)<1e-4f;};
  for(float roll:{0.f,.8f,1.5708f,-1.5708f,3.f}){
   const Pose level{euler(.7f,0,roll),{.3f,1.2f,-.4f}};const auto can=motion::mountCarriedTool(level);const auto ahead=rotate(level.orientation,{0,0,-1});
   require(near(body(can),{0,-1,0})&&near(spout(can),ahead)&&std::abs(ahead.y)<1e-5f&&length(can.position-level.position)<1e-7f);
   require(std::abs(length(Vec3{can.orientation.x,can.orientation.y,can.orientation.z})*length(Vec3{can.orientation.x,can.orientation.y,can.orientation.z})+can.orientation.w*can.orientation.w-1)<1e-5f);
   require(near(rotate(can.orientation,{1,0,0}),cross(rotate(can.orientation,{0,1,0}),rotate(can.orientation,{0,0,1}))));}   // a rotation, not a mirror
  // Tipped forward by 40 degrees: the spout goes down with the hand, the body swings back - it pours.
  {const Pose tipped{euler(0,-40*pi/180,.9f),{}};const auto can=motion::mountCarriedTool(tipped);
   require(near(spout(can),{0,-std::sin(40*pi/180),-std::cos(40*pi/180)})&&near(body(can),{0,-std::cos(40*pi/180),std::sin(40*pi/180)}));}
  // Pointing straight down (and up): no "straight down" square to that line, the fist's own down holds - no jump, no NaN.
  for(float pitch:{-90.f,-80.f,90.f}){const Pose steep{euler(.3f,pitch*pi/180,.5f),{}};const auto can=motion::mountCarriedTool(steep);
   require(near(spout(can),rotate(steep.orientation,{0,0,-1}))&&near(body(can),rotate(steep.orientation,{0,-1,0}))&&finite(body(can)));}
  // In between (70 degrees down: 20 degrees off the vertical) the body lies between the two, square to the spout.
  {const Pose between{euler(0,-70*pi/180,1.2f),{}};const auto can=motion::mountCarriedTool(between);const auto own=rotate(between.orientation,{0,-1,0});
   require(std::abs(dot(body(can),spout(can)))<1e-5f&&dot(body(can),own)>.3f&&dot(body(can),own)<.9999f&&std::abs(length(body(can))-1)<1e-5f);}
  bool rejected=false;try{motion::mountHangingTool({{0,0,0,1},{NAN,0,0}});}catch(const std::invalid_argument&){rejected=true;}require(rejected);
  // Only the watering can hangs so.
  require(motion::hangingTool(2395034423u,{0x0d6b97bau,0x48861a89u,0x29de899bu,0x51e2ae7bu})&&!motion::hangingTool(3833797336u,{0x0d6b97bau,0x48861a89u,0x29de899bu,0x51e2ae7bu})&&
          !motion::hangingTool(2395034423u,{0x0d6b97bbu,0x48861a89u,0x29de899bu,0x51e2ae7bu}));}
 // The earlier forms, kept for a comparison (turns of the weapon mount).
 {const auto weapon=motion::mountLiveWeapon(grip),carried=motion::mountCarriedTool(grip,motion::vhvrWeaponPitchDegrees,0);
  require(length(rotate(carried.orientation,{0,0,1})+rotate(weapon.orientation,{0,1,0}))<1e-5f&&length(rotate(carried.orientation,{0,1,0})+rotate(weapon.orientation,{0,0,1}))<1e-5f&&
          length(rotate(carried.orientation,{1,0,0})+rotate(weapon.orientation,{1,0,0}))<1e-5f&&length(carried.position-grip.position)<1e-6f);
  // The earlier forms, kept for a comparison: 1 the spout down the fist and the body ahead, 2 the spout up the blade's line.
  const auto first=motion::mountCarriedTool(grip,motion::vhvrWeaponPitchDegrees,1),second=motion::mountCarriedTool(grip,motion::vhvrWeaponPitchDegrees,2);
  require(length(rotate(first.orientation,{0,1,0})+rotate(weapon.orientation,{0,1,0}))<1e-5f&&length(rotate(first.orientation,{0,0,1})+rotate(weapon.orientation,{0,0,1}))<1e-5f&&
          length(rotate(first.orientation,{1,0,0})-rotate(weapon.orientation,{1,0,0}))<1e-5f);
  require(length(rotate(second.orientation,{0,1,0})-rotate(weapon.orientation,{0,1,0}))<1e-5f&&length(rotate(second.orientation,{0,0,1})+rotate(weapon.orientation,{0,0,1}))<1e-5f);}
 for(float bad:{91.f,-91.f,INFINITY,NAN}){bool rejected=false;try{motion::mountLiveWeapon(grip,bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected);}
 // The attachment between the hand slot and the model: identity for nearly everything, an offset for a few items.
 {std::array<uint8_t,48> bytes{};const float one=1;std::memcpy(bytes.data()+36,&one,4);bytes[41]=8;
  require(motion::nativeHandAttachment(bytes).identity);
  require(motion::nativeHandAttachment(std::array<uint8_t,48>{}).identity); // nothing observed
  // The offset is three floats in metres (the game data's SlotAttachment), not Q32 words.
  const float up=.07f,forward=.25f;std::memcpy(bytes.data()+4,&up,4);std::memcpy(bytes.data()+8,&forward,4);
  const auto a=motion::nativeHandAttachment(bytes);
  require(!a.identity&&length(a.local.position-Vec3{0,.07f,-.25f})<1e-6f&&a.local.orientation.w==1);
  const auto held=compose(motion::mountLiveWeapon(grip),a.local);
  require(length(held.position-(mounted.position+rotate(mounted.orientation,{0,.07f,-.25f})))<1e-6f);
  const Quat turn{0,0,.5f,.8660254f};std::memcpy(bytes.data()+24,&turn,16);
  const auto turned=motion::nativeHandAttachment(bytes);
  require(!turned.identity&&length(rotate(turned.local.orientation,{1,0,0})-Vec3{.5f,.8660254f,0})<1e-5f); // +60 degrees about Game Z is -60 about XR Z, seen mirrored
  const Quat broken{0,0,0,3};std::memcpy(bytes.data()+24,&broken,16);require(motion::nativeHandAttachment(bytes).identity);
  // The watering can's component as the running game holds it (run game-20261013-041940): 1 cm and 2.8 cm, not 0.77 m.
  const std::array<uint8_t,48> can{0x0a,0xd7,0x23,0xbc,0,0,0,0,0x42,0x60,0xe5,0x3c,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x80,0x3f,0,8,0,0,1,0,0,0};
  const auto offset=motion::nativeHandAttachment(can);
  require(!offset.identity&&length(offset.local.position-Vec3{-.01f,0,-.028f})<1e-6f&&offset.local.orientation.w==1);
  // A sword's 7 cm up the grip (13 swords of the game data).
  std::array<uint8_t,48> sword{};std::memcpy(sword.data()+4,&up,4);std::memcpy(sword.data()+36,&one,4);sword[41]=8;
  require(!motion::nativeHandAttachment(sword).identity&&length(motion::nativeHandAttachment(sword).local.position-Vec3{0,.07f,0})<1e-6f);
  // Nonsense stays the identity.
  const float far=3;std::memcpy(sword.data(),&far,4);require(motion::nativeHandAttachment(sword).identity);
 }
 // The bow remains on the left when the right hand moves. Reflect a nonzero
 // native Q32 offset and rotation, not a float reinterpretation of the bytes.
 require(motion::nativeLeftBow(4,5,9));
 require(!motion::nativeLeftBow(4,5,8)&&!motion::nativeLeftBow(7,1,9)&&!motion::nativeLeftBow(4,1,9));
 std::array<uint8_t,48> attachment{};attachment[41]=9;const uint32_t parent=123;
 std::memcpy(attachment.data()+44,&parent,4);Quat q=euler(.3f,-.4f,.2f);
 const std::array<int64_t,3> offset{{int64_t(1)<<30,-(int64_t(1)<<29),int64_t(1)<<28}};
 std::memcpy(attachment.data(),offset.data(),24);std::memcpy(attachment.data()+24,&q,16);
 const auto bow=motion::mountNativeBow(grip,attachment,parent);
 const auto local=compose(inverse(grip),bow);
 require(length(local.position-Vec3{.25f,-.125f,-.0625f})<1e-6f);
 const auto nativeDirection=rotate(q,{0,0,1}),xrDirection=rotate(local.orientation,{0,0,-1});
 require(length(xrDirection-Vec3{nativeDirection.x,nativeDirection.y,-nativeDirection.z})<1e-6f);
 const auto root=Pose{euler(.9f,.1f,-.2f),{3,4,5}};
 const auto moved=motion::mountNativeBow(compose(root,grip),attachment,parent),expected=compose(root,bow);
 require(length(moved.position-expected.position)<1e-5f&&length(rotate(moved.orientation,{1,0,0})-rotate(expected.orientation,{1,0,0}))<1e-5f);
 for(unsigned field:{40u,41u,44u,27u}){auto bad=attachment;bad[field]=255;bool rejected=false;
  try{motion::mountNativeBow(grip,bad,parent);}catch(const std::invalid_argument&){rejected=true;}require(rejected);}
 attachment.fill(0);attachment[41]=9;std::memcpy(attachment.data()+44,&parent,4);q={0,0,0,1};std::memcpy(attachment.data()+24,&q,16);
 const auto identity=motion::mountNativeBow(grip,attachment,parent);
 require(length(identity.position-grip.position)<1e-6f&&length(rotate(identity.orientation,{0,1,0})-rotate(grip.orientation,{0,1,0}))<1e-6f);
 const auto corrected=motion::mountNativeBow(grip,attachment,parent,true);
 const auto correctedLocal=compose(inverse(grip),corrected);
 require(length(corrected.position-grip.position)<1e-6f);
 require(length(rotate(correctedLocal.orientation,{0,1,0})-Vec3{0,1,0})<1e-6f);
 // Native string +Z is XR -Z before correction; afterwards it points back.
 require(length(rotate(correctedLocal.orientation,{0,0,-1})-Vec3{0,0,1})<1e-6f);
 const std::array<uint32_t,4> bowGuid{782483182u,1119002633u,871164063u,3497776913u};
 require(motion::reviewedBowFacing(327157552u,bowGuid));
 auto unknown=bowGuid;unknown[0]^=1;
 require(!motion::reviewedBowFacing(327157552u,unknown)&&!motion::reviewedBowFacing(123u,bowGuid));
 {const std::array<uint32_t,4> guid{494587688u,1236792456u,3139207297u,1775552796u};
 const auto profile=motion::nativeMeleeMount(1940764111u,7,guid);require(profile.known);
 for(Vec3 scale:{Vec3{1,1,1},Vec3{1.0002f,1.001f,1.0014f},Vec3{.9f,1.1f,1.f}}){
  const auto target=motion::mountLiveWeapon(grip),w=motion::mountNativeMelee(target,profile,scale);
  const auto v=profile.gripInModel;
  const auto measuredGrip=w.position+rotate(w.orientation,{v.x*scale.x,v.y*scale.y,v.z*scale.z});
  require(length(measuredGrip-grip.position)<1e-6f);
 }
 auto other=guid;other[0]^=1;require(!motion::nativeMeleeMount(1940764111u,7,other).known);
 bool rejected=false;try{motion::mountNativeMelee(grip,profile,{1,-1,1});}catch(const std::invalid_argument&){rejected=true;}require(rejected);
 }
 {const std::array<uint32_t,4> guid{1215536574u,1265059332u,3333187989u,3176988074u};
 const auto profile=motion::nativeMeleeMount(3936779840u,8,guid);require(profile.known);
 for(Vec3 scale:{Vec3{1,1,1},Vec3{1.0002f,1.001f,1.0014f},Vec3{.9f,1.1f,1.f}}){
  const auto target=motion::mountLiveWeapon(grip),w=motion::mountNativeMelee(target,profile,scale);
  const auto v=profile.gripInModel;
  const auto measuredGrip=w.position+rotate(w.orientation,{v.x*scale.x,v.y*scale.y,v.z*scale.z});
  require(length(measuredGrip-grip.position)<1e-6f);
 }
 auto other=guid;other[0]^=1;require(!motion::nativeMeleeMount(3936779840u,8,other).known);
 bool rejected=false;try{motion::mountNativeMelee(grip,profile,{1,-1,1});}catch(const std::invalid_argument&){rejected=true;}require(rejected);
 }
 {const std::array<uint32_t,4> guid{1602120618u,1327021138u,2095175581u,1329450576u};
 require(motion::reviewedShieldMount(691231505u,guid,12));auto bad=guid;bad[2]^=1;
 require(!motion::reviewedShieldMount(691231505u,bad,12)&&!motion::reviewedShieldMount(691231505u,guid,9)&&!motion::reviewedShieldMount(1u,guid,12));
 // Check handedness reflection and world equivariance; no weapon pitch added.
 const auto shield=motion::mountNativeShield(grip),local=compose(inverse(grip),shield);
 require(length(local.position-Vec3{-.004039420814f,.039536910005f,.027946638520f})<1e-6f);
 const Quat gameQ{.035990421819f,.092474961512f,.036084607052f,.994409861257f};
 const auto nativeNormal=rotate(gameQ,{1,0,0}),xrNormal=rotate(local.orientation,{1,0,0});
 require(length(xrNormal-Vec3{nativeNormal.x,nativeNormal.y,-nativeNormal.z})<1e-6f);
 const auto moved=motion::mountNativeShield(compose(root,grip)),want=compose(root,shield);
 require(length(moved.position-want.position)<1e-5f&&length(rotate(moved.orientation,{1,0,0})-rotate(want.orientation,{1,0,0}))<1e-5f);
 }
 std::cout<<"PASS weapon-only local pitch, pivot preservation, disable/reverse and invalid input\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
