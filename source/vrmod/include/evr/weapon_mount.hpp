#pragma once
#include <evr/math.hpp>
#include <evr/aim_mark.hpp>
#include <evr/ranged_asset_catalog.hpp>
#include <evr/two_hand_catalog.hpp>
#include <evr/shield_catalog.hpp>
#include <evr/dual_wield_catalog.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace evr::motion {
// VHVR VrikCreator equipped/unequipped hand targets differ by 35 degrees
// about controller X (Unity); reflecting into OpenXR gives -35 degrees.
// Weapon-only comfort trim for the uncalibrated live mount. Per-asset pivot
// calibration is separate; measured combat profiles remain unchanged.
constexpr float vhvrWeaponPitchDegrees=-35.f;
// A tool that is carried by a handle instead of being held like a rod: the watering can, the only hand item of the
// game data whose attachment to the right hand is not the identity (held-models: Tool_T1_WateringCan; its body lies
// 0.5 m along the model's +Y and the spout 0.5 m along +Z). On the weapon mount the can stood where a blade would,
// 0.3 m off the fist (headset run game145015: "the watering can is not in the hand"; Mock run game154945).
// On the weapon mount the model's +Y runs along the fist from the little finger to the thumb (where a blade goes)
// and the model's +Z, mirrored into tracking space, points back from the knuckles.
// The model (held-models: x -0.15..0.15, y -0.12..0.52, z -0.02..0.50) is the can as the game's character carries it,
// by the handle on top with the arm down: the origin is at that handle, the can hangs from it along +Z, the spout
// reaches forward along +Y (0.52) and the rear handle back (-0.12).
// Forms tried: 1 (2026-10-12) half a turn about the mount's X axis - the body ahead of the knuckles and the spout
// down out of the bottom of the fist (headset run game-20261013-005905, the user: "the model shows off to the right,
// facing down"); 2 (2026-10-13, Mock only) half a turn about the mount's Y axis - the spout along the blade's line
// and the body out towards the palm (Mock run game-20261013-041940: the can up and to the left of the hand).
// Form 0 (2026-10-13, Mock only) was meant to hang the can from the fist, a half turn about the line between the
// mount's Y and -Z - but it was worked out without the mirror: a model's vector (x, y, z) stands in tracking space
// where the mount turns (x, y, -z) (game_camera_math.hpp trackingPose), so that turn sent the can's body UP (Mock run
// game-20261013-042455, frame 1263: the shown orientation takes the model's +Z to (0, 0.97, -0.26) of the world).
// The shape, read from the game data on 2026-10-13 (863 vertices; scratch can_geometry.py): the body is a drum of
// 0.15 m radius about the model's Z, its top rim at z 0.12 and its bottom at z 0.50; the handle is a hoop over the
// top, from the rim on the spout's side over the origin (its highest point, z -0.02) and down the back of the drum
// (y -0.12); the spout leaves the drum on the +Y side and rises to the rose at y 0.5, z 0.05. That is how the
// vertices are stored, and how the copy in the hand is drawn: neither the game's own instance nor the copy has a
// pose (Mock runs game-20261013-091609 / 091948). (A pose at the hierarchy's default joints draws it half a turn
// about X instead - default joint 1 is Rx(180) where the inverse bind is the identity; tried in game-20261013-093301.)
// What kept the can off the hand in every form was not its turn but the offset of its attachment, misread
// (nativeHandAttachment below).
// Form 3, used: the can hangs under the fist whatever way the hand is turned about the line it points along. The
// spout (+Y) goes where the hand points (the grip's -Z), the body (+Z) goes straight down, made square to that line:
// the hoop turns in the loosely closed hand as a real one would, and tipping the hand forward pours. Headset runs
// of 2026-10-12/13: the right hand is held with its back up as often as with the thumb up, so a can fixed to the
// fist would stick out to the side half of the time. Where the hand points nearly straight up or down, "straight
// down" tells nothing about the turn; from 17 degrees off the vertical and nearer the fist's own down (the grip's
// -Y) takes over, blended in between 30 and 17 degrees.
inline Pose mountHangingTool(Pose grip);
// The tool that hangs so: the watering can (Tool_T1_WateringCan, model tool_watering_can_01 - the one item with
// this model). The other tool with an offset on the right hand, the water pouch, stays on the plain mount with its
// offset, as the game holds it.
constexpr uint32_t wateringCanItem=2395034423u;
constexpr std::array<uint32_t,4> wateringCanModel{0x0d6b97bau,0x48861a89u,0x29de899bu,0x51e2ae7bu};
inline bool hangingTool(uint32_t item,const std::array<uint32_t,4>& model){return item==wateringCanItem&&model==wateringCanModel;}
inline Pose mountCarriedTool(Pose grip,float pitchDegrees=vhvrWeaponPitchDegrees,unsigned form=3);
// The private hand rig locates socket 64/94 at the tracked left/right grip.
// Preserve the native bow-to-left-socket transform, rather than applying the
// uncalibrated melee pitch. VHVR likewise keeps bow hand and arrow hand separate.
inline bool nativeLeftBow(unsigned weaponType,unsigned itemSlot,unsigned attachmentSlot){
    return weaponType==4&&itemSlot==5&&attachmentSlot==9;
}
inline Pose nativeBowAttachment(const std::array<uint8_t,48>& bytes,uint32_t parent){
    uint32_t owner=0;std::memcpy(&owner,bytes.data()+44,4);
    if(!parent||owner!=parent||bytes[40]!=0||bytes[41]!=9)
        throw std::invalid_argument("native bow attachment owner/slot mismatch");
    std::array<int64_t,3> q32{};Quat q{};
    std::memcpy(q32.data(),bytes.data(),24);std::memcpy(&q,bytes.data()+24,16);
    Vec3 p{float(double(q32[0])/4294967296.),float(double(q32[1])/4294967296.),float(double(q32[2])/4294967296.)};
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(length(p)>2.f||!std::isfinite(norm)||std::abs(norm-1.f)>.02f)
        throw std::invalid_argument("native bow attachment transform invalid");
    // Native left-handed Game coordinates -> right-handed OpenXR coordinates.
    return {normalized(Quat{-q.x,-q.y,q.z,q.w}),{p.x,p.y,-p.z}};
}
// Local model evidence: game072034 frames936/2436 and native AABB.
// This bow's shaft is +Y and its string is on +Z (Game). OpenXR forward
// maps to +Z (Game), so rotate about the shaft to place the string behind.
// Only this observed asset receives that geometry correction.
inline bool reviewedBowFacing(uint32_t item,const std::array<uint32_t,4>& guid){
    return item==327157552u&&guid==std::array<uint32_t,4>{782483182u,1119002633u,871164063u,3497776913u};
}
// The draw route (private string pose, bow turned to the line between the hands, tracked shot).
// All 49 bows of the read-only catalog share one skeleton (model catalog hierarchy 5cec9261:
// handle, string joint 5, the same rest and draw travel) and the same identity attachment to
// the left hand, so the geometry taken from the reviewed bow holds for them. In play it is
// proven on the reviewed bow only; the others enter with the catalog display route.
inline bool catalogBowItem(uint32_t item){
    for(const auto& a:catalogRangedAssets){if(a.item==item)return catalogRangedRigs[a.rig].type==4;if(a.item>item)break;}
    return false;
}
inline bool bowDrawAsset(uint32_t item,const std::array<uint32_t,4>& guid,bool catalogDisplay){
    return reviewedBowFacing(item,guid)||(catalogDisplay&&catalogBowAsset(item,guid));
}
inline Pose mountNativeBow(Pose leftGrip,const std::array<uint8_t,48>& attachment,uint32_t parent,bool stringToPlayer=false){
    if(!finite(leftGrip.position))throw std::invalid_argument("native bow grip invalid");
    auto local=nativeBowAttachment(attachment,parent);
    if(stringToPlayer)local.orientation=normalized(multiply(local.orientation,Quat{0,1,0,0}));
    return compose(leftGrip,local);
}

// Frozen local measurements, game071014/probe071145; see
// docs/analysis/weapon_mount_20261010/notes.md. Never follow idle animation.
// rearY/tipY: the model's extent along its long axis (+Y), from the read-only model
// catalog (wpn_greataxe_02, wpn_hammer_06). The origin is the rear, native left-hand grip.
// Two-hand hold (live_wield.hpp, VHVR entry): rearReach is how far behind the main grip the
// rear hand may sit (0: as far as the model origin, i.e. gripInModel.y); mainBehind is where
// the weapon hand sits behind the main grip when it is the rear hand (0: the same rule).
struct NativeMeleeMount {bool known=false;Quat orientation{0,0,0,1};Vec3 gripInModel{};float rearY=0,tipY=0,rearReach=0,mainBehind=0;};
inline NativeMeleeMount nativeMeleeMount(uint32_t item,unsigned type,const std::array<uint32_t,4>& guid){
    if(item==1940764111u&&type==7&&guid==std::array<uint32_t,4>{494587688u,1236792456u,3139207297u,1775552796u})return {true,{-0.0144574582f,-0.101498001f,-0.0155454277f,0.994609208f},{-0.0104326454f,0.633441743f,-0.0302771029f},-0.3344f,1.4326f};
    if(item==3936779840u&&type==8&&guid==std::array<uint32_t,4>{1215536574u,1265059332u,3333187989u,3176988074u})return {true,{-0.0105201576f,-0.123461902f,-0.0171802206f,0.992144811f},{-0.0137864026f,0.634056141f,-0.0292811297f},-0.2956f,1.2477f};
    return {};
}
// Every two-hand melee weapon the game attaches to the left hand (great axes and hammers:
// native types 7..9, slot 9, identity attachment; two_hand_catalog.hpp). The model origin is
// the left-hand grip, and the game's own stance puts the right hand 0.63 m up the handle:
// measured on the great axe and the hammer above (0.6334 and 0.6341 m, orientations 2.6
// degrees apart). The mean of the two stands for the rest of the group; that the whole group
// shares the stance is an assumption, proven in play only for those two.
// Where the right hand holds these weapons in VR, along the model axis from the origin. In the game's own
// stance it is 0.63 m up; held there the axe looked far shorter than its attack on the headset, the hand close
// under the blade (user, 2026-10-11, run game100334). The hand goes near the left-hand grip instead.
constexpr float twoHandMeleeVrGripY=.15f;
inline NativeMeleeMount catalogFamilyMount(uint32_t item,unsigned type,const std::array<uint32_t,4>& guid,unsigned slot){
    const auto* a=catalogTwoHandAsset(item,guid);
    if(!a||a->type!=type||a->slot!=slot||slot!=9||type<7||type>9)return {};
    return {true,normalized(Quat{-0.0124888079f,-0.112479952f,-0.0163628242f,0.99337701f}),{-0.012109524f,twoHandMeleeVrGripY,-0.0297791163f},a->rearY,a->tipY};
}
inline NativeMeleeMount catalogMeleeMount(uint32_t item,unsigned type,const std::array<uint32_t,4>& guid,unsigned slot){
    auto measured=nativeMeleeMount(item,type,guid);
    if(!measured.known)return catalogFamilyMount(item,type,guid,slot);
    measured.gripInModel.y=twoHandMeleeVrGripY;return measured; // the measured palm offset and orientation, the VR grip height
}
// Tools (equipment slots 2 and 3) are not weapons and not in the model catalog. The long ones hang on the left hand
// as the great axes do (slot 9, identity attachment): the 7 felling axes, 7 pickaxes and 2 rakes of the game data
// (docs/analysis/weapon_mount_20261010/held-models; home save 2026-10-11: pickaxe 2671192505), so they take that
// group's mount. The blade length comes from the model at run time; the two-hand hold stays off (rear = tip).
inline NativeMeleeMount toolMount(unsigned itemSlot,unsigned slot){
    if((itemSlot!=2&&itemSlot!=3)||slot!=9)return {};
    return {true,normalized(Quat{-0.0124888079f,-0.112479952f,-0.0163628242f,0.99337701f}),{-0.012109524f,twoHandMeleeVrGripY,-0.0297791163f},0,0};
}
// Two-hand hold of the weapons the game attaches to the right hand (slot 9 is handled above):
// staffs (type 5) and great swords (type 10) stay on the plain right-hand mount, the model
// origin in the right hand. VHVR geometry, D being the length from the origin to the rear end:
//   staff, Magic class (docs/analysis/vhvr_spec_20261010/staff_throw.md 2.3): the other hand
//     reaches back to the rear end; a weapon hand held behind sits D/4 behind the origin.
//   great sword, Sword class (melee.md 2.4): the other hand reaches max(0.75 D, 0.125) back.
inline NativeMeleeMount twoHandHoldProfile(uint32_t item,unsigned type,const std::array<uint32_t,4>& guid,unsigned slot){
    const auto* a=catalogTwoHandAsset(item,guid);
    if(!a||a->type!=type||a->slot!=slot||slot!=8)return {};
    const float d=-a->rearY;
    if(type==5)return {true,{0,0,0,1},{},a->rearY,a->tipY,d,.25f*d};
    if(type==10)return {true,{0,0,0,1},{},a->rearY,a->tipY,std::max(.75f*d,.125f),0};
    return {};
}
inline Pose mountNativeMelee(Pose grip,const NativeMeleeMount& profile,Vec3 scale){
    if(!profile.known||!finite(grip.position)||!finite(scale)||scale.x<=0||scale.y<=0||scale.z<=0)
        throw std::invalid_argument("native melee mount invalid");
    const auto v=profile.gripInModel;
    const auto q=normalized(profile.orientation);
    // Scale the measured model-space grip before solving its rigid placement.
    const Pose offset{q,rotate(q,{v.x*scale.x,v.y*scale.y,v.z*scale.z})*-1.f};
    return compose(grip,offset);
}
// One rigid shield measured in home game113501/probe113632, six samples.
// Native slot12 -> bone67; frozen left socket64 relation (0.118mm/0.007deg
// variation). This avoids following the original character's idle animation.
inline bool reviewedShieldMount(uint32_t item,const std::array<uint32_t,4>& guid,unsigned slot){
    return item==691231505u&&slot==12&&guid==std::array<uint32_t,4>{1602120618u,1327021138u,2095175581u,1329450576u};
}
// Every shield of the model catalog attaches as the measured one does (slot 12, identity): the same mount places it.
inline bool catalogShieldMount(uint32_t item,const std::array<uint32_t,4>& guid,unsigned slot){return slot==12&&catalogShieldAsset(item,guid);}
inline Pose mountNativeShield(Pose leftGrip){
    if(!finite(leftGrip.position))throw std::invalid_argument("shield grip invalid");
    return compose(leftGrip,Pose{{-.035990421819f,-.092474961512f,.036084607052f,.994409861257f},{-.004039420814f,.039536910005f,.027946638520f}});
}
// What the game's attachment puts between the hand slot and the model. Nearly every hand-held item attaches with
// the identity; 15 items of the game data carry an offset (13 swords 7 cm up the grip, the watering can, the water
// pouch; docs/analysis/weapon_mount_20261010/audit_held_models.py). The live component is the game data's
// SlotAttachment: the offset as three floats in metres (+0), the orientation (+24), in Game coordinates, reflected to
// OpenXR. Anything unreadable counts as the identity.
// Until 2026-10-13 the offset was read as three Q32 words. The watering can's bytes (0ad723bc 00000000 4260e53c ..:
// the floats -0.01, 0, 0.028 of the game data) so became 0.735 m and 0.238 m - the can stood 0.77 m off the hand in
// every run, whatever its turn (what the headset runs of 2026-10-12/13 showed as "not in the hand" and "off to the
// right") - and the swords' (0, 0.07, 0) became 1e9 m, which the bound below turned into the identity: their 7 cm
// were never applied.
struct NativeAttachment {bool identity=true;Pose local{{0,0,0,1},{0,0,0}};};
inline NativeAttachment nativeHandAttachment(const std::array<uint8_t,48>& bytes){
    std::array<float,3> offset{};Quat q{};std::memcpy(offset.data(),bytes.data(),12);std::memcpy(&q,bytes.data()+24,16);
    const Vec3 p{offset[0],offset[1],offset[2]};
    if(p.x==0&&p.y==0&&p.z==0&&q.x==0&&q.y==0&&q.z==0&&q.w==1)return {};
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!finite(p)||length(p)>2.f||!std::isfinite(norm)||std::abs(norm-1.f)>.02f)return {};
    return {false,{normalized(Quat{-q.x,-q.y,q.z,q.w}),{p.x,p.y,-p.z}}};
}
// A weapon held as a pair (VHVR DualKnives / DualAxes: one in each hand, either hand strikes). Each part sits in its
// own hand as a one-hand weapon does: the hand slot at the grip with the weapon pitch, then the game's attachment.
inline Pose mountPairPart(Pose grip,const std::array<uint8_t,48>& attachment,float pitchDegrees=vhvrWeaponPitchDegrees);
inline Pose mountLiveWeapon(Pose grip,float pitchDegrees=vhvrWeaponPitchDegrees){
    if(!finite(grip.position)||!std::isfinite(pitchDegrees)||std::abs(pitchDegrees)>90.f)
        throw std::invalid_argument("invalid live weapon mount");
    grip.orientation=normalized(multiply(grip.orientation,euler(0,pitchDegrees*pi/180.f,0)));
    return grip;
}
inline Pose mountHangingTool(Pose grip){
    if(!finite(grip.position))throw std::invalid_argument("invalid hanging tool mount");
    const auto hand=normalized(grip.orientation);
    const Vec3 ahead=rotate(hand,{0,0,-1}),own=rotate(hand,{0,-1,0}),straight{0,-1,0};
    Vec3 hung=straight-ahead*dot(straight,ahead);const float level=length(hung);   // 1: the hand points level, 0: straight up or down
    const float weight=std::clamp((level-.3f)/.2f,0.f,1.f);
    if(level>1e-4f)hung=hung*(1/level);
    Vec3 down=own*(1-weight)+hung*weight;down=down-ahead*dot(down,ahead);
    const float size=length(down);down=size>1e-3f?down*(1/size):own;                  // the two cancel: the fist's own
    // The mount turns the model's (x, y, z) as (x, y, -z): its Y is the spout's line, its Z the line up from the body.
    grip.orientation=aim::fromAxes(cross(ahead,down*-1.f),ahead,down*-1.f);return grip;
}
inline Pose mountCarriedTool(Pose grip,float pitchDegrees,unsigned form){
    if(form==3)return mountHangingTool(grip);
    auto mounted=mountLiveWeapon(grip,pitchDegrees);
    const Quat turn=form==1?Quat{1,0,0,0}:form==2?Quat{0,1,0,0}:Quat{0,.70710678f,-.70710678f,0};
    mounted.orientation=normalized(multiply(mounted.orientation,turn));return mounted;
}
inline Pose mountPairPart(Pose grip,const std::array<uint8_t,48>& attachment,float pitchDegrees){
    const auto slot=mountLiveWeapon(grip,pitchDegrees);const auto local=nativeHandAttachment(attachment);
    return local.identity?slot:compose(slot,local.local);
}
}
