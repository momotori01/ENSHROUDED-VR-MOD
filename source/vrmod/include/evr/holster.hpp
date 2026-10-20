#pragma once
#include <evr/idle_head_camera.hpp>
#include <evr/motion_actions.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
namespace evr::motion {
// Holsters (user, 2026-10-13 after the headset run game-20261013-141010):
//   the left grip at the left shoulder: the ranged weapon of the ranged-combat key, on and off (a bow in the hand: bare hands);
//   the right grip at the right shoulder: the weapon in the hand off (bare hands), and on again;
//   the left grip at the left hip: the shield off, and on again.
//   "The areas as VHVR has them. A weak vibration when a grip has put something on or off; none for only reaching
//   into an area."
// The areas are VHVR's "the hand has reached the back / the hip" (Pose.cs:656-724, docs/analysis/vhvr_spec_20261010/
// equipment_tools.md 2.1), the hand's own side only: from the headset to the hand, v up, l to the right, s ahead
// (the headset's forward laid level).
//   shoulder: -0.25 <= v <= 0.125, the hand on its own side (l > 0 is the right), |l| <= 0.5, s <= 0 (not ahead of the head)
//   hip:      -0.75 <= v < -0.375, its own side (right hand: l > 0.5 s; left hand: l <= -0.5 s), |l| <= 0.5,
//             s <= -0.0625 and s + 0.75 |l| <= 0 (beside and behind the hip)
enum class HolsterPlace {None,LeftShoulder,RightShoulder,LeftHip,RightHip};
struct HolsterZones {
    float shoulderLow=-.25f,shoulderHigh=.125f,hipLow=-.75f,hipHigh=-.375f,side=.5f,shoulderAhead=0.f,hipAhead=-.0625f,hipSlope=.75f,hipSplit=.5f;
};
inline HolsterPlace holsterPlace(Vec3 head,Vec3 headForward,Vec3 hand,unsigned which,const HolsterZones& z={}){
    if(which>1||!finite(head)||!finite(headForward)||!finite(hand))return HolsterPlace::None;
    Vec3 forward{headForward.x,0,headForward.z};const float level=length(forward);
    if(level<.05f)return HolsterPlace::None;                           // looking straight up or down: no forward to go by
    forward=forward*(1/level);const Vec3 right=cross(forward,Vec3{0,1,0}),d=hand-head;
    const float v=d.y,l=dot(d,right),s=dot(d,forward);
    if(std::abs(l)>z.side)return HolsterPlace::None;
    if(v>=z.shoulderLow&&v<=z.shoulderHigh){
        if((l>0)!=(which==1)||s>z.shoulderAhead)return HolsterPlace::None;
        return which?HolsterPlace::RightShoulder:HolsterPlace::LeftShoulder;
    }
    if(v>=z.hipLow&&v<z.hipHigh){
        const bool rightSide=which?l>z.hipSplit*s:l>-z.hipSplit*s;
        if(rightSide!=(which==1)||s>z.hipAhead||s+z.hipSlope*std::abs(l)>0)return HolsterPlace::None;
        return which?HolsterPlace::RightHip:HolsterPlace::LeftHip;
    }
    return HolsterPlace::None;
}
enum class HolsterAct {None,Ranged,Weapon,Shield};
inline HolsterAct holsterAct(HolsterPlace p){
    return p==HolsterPlace::LeftShoulder?HolsterAct::Ranged:p==HolsterPlace::RightShoulder?HolsterAct::Weapon:p==HolsterPlace::LeftHip?HolsterAct::Shield:HolsterAct::None;
}
// The grips' presses. A press is looked at once, where it begins; one that began in a holster belongs to it until
// the grip is let go (it is no fist and no second hand on a weapon meanwhile). While not allowed (a menu, the quick
// wheel, no tracking) nothing begins, and a grip already down then has to be let go first.
struct HolsterPress {HolsterAct act=HolsterAct::None;HolsterPlace place=HolsterPlace::None;unsigned hand=2;};
class HolsterGrips {
    std::array<bool,2> down_{},owned_{};
public:
    HolsterZones zones{};
    bool owns(unsigned hand)const{return hand<2&&owned_[hand];}
    HolsterPress update(const HandFrame& f,const Pose& head,bool allowed){
        HolsterPress out;const Vec3 forward=rotate(head.orientation,Vec3{0,0,-1});
        for(unsigned h=0;h<2;++h){const auto& hand=f.hands[h];
            const bool pressed=f.focused&&hand.tracked()&&hand.grabActive&&hand.grab;
            if(!pressed){down_[h]=owned_[h]=false;continue;}
            if(down_[h])continue;
            down_[h]=true;if(!allowed)continue;
            const auto place=holsterPlace(head.position,forward,hand.grip.position,h,zones);const auto act=holsterAct(place);
            if(act==HolsterAct::None)continue;
            owned_[h]=true;if(out.act==HolsterAct::None)out={act,place,h};
        }
        return out;
    }
};
// What the game holds, as far as the holsters need it, and what they remember.
struct HolsterGame {
    bool known=false;
    uint8_t selected=0xff;        // the action bar's selected slot; 16 and above: none (bare hands)
    bool selectedIsRanged=false;  // the selected slot's item is a ranged weapon (a bow in the hand)
    bool rangedWorn=false;        // the equipment's ranged weapon slot holds one
    bool shieldWorn=false;        // the equipment's shield slot holds one
    bool shieldKept=false;        // a shield lies in the player's inventory, to be worn again
    uint8_t weaponSlot=0xff;      // the slot last selected with something other than a ranged weapon in it
    uint8_t rangedBarSlot=0xff;   // the action bar's slot that holds the equipment's ranged weapon (0xff: none)
};
struct HolsterMemory {bool armed=false;uint8_t slot=0xff;};
constexpr uint8_t holsterNoSlot=0xff;
struct HolsterOrder {
    bool change=false;bool arm=false;
    int select=-1;                // 0..15 a slot; 16: none
    int shield=-1;                // 1: off; 0: on
};
// One press. The memory is the caller's: `armed` is what the last order asked for.
inline HolsterOrder holsterOrder(HolsterAct act,const HolsterGame& g,HolsterMemory& m){
    HolsterOrder o;o.arm=m.armed;
    if(!g.known||act==HolsterAct::None)return o;
    const bool inHand=g.selected<16;
    const auto bare=[&]{if(inHand){m.slot=g.selected;o.select=16;}o.arm=m.armed=false;o.change=true;};
    // The bow put away at the left shoulder gives back what the hand held before (headset run 20261014-124515, user:
    // "putting the bow away at the shoulder does not equip again the one-hand weapon I had; the shield comes back").
    // The bow of the equipment is held by the ranged-combat bit over the slot that stays selected: letting the bit go
    // is enough. A bow of the action bar goes back to the slot that was selected before it.
    if(act==HolsterAct::Ranged){
        if(m.armed){o.arm=m.armed=false;o.change=true;}
        else if(inHand&&g.selectedIsRanged){
            if(g.weaponSlot<16&&g.weaponSlot!=g.selected){m.slot=g.weaponSlot;o.select=g.weaponSlot;o.change=true;}
            else bare();
        }
        // The equipment's bow on the action bar too: its slot is selected (2026-10-18 12:40, headset: "taking the bow
        // from the shoulder slows the walk" - the ranged-combat key is the game's aiming key, and held it halves the walk:
        // Mock game-20261018-124517, 3.6 m against 6.2 / 8.5 m in the same time). Held by the key only without one there.
        else if(g.rangedBarSlot<16){o.select=g.rangedBarSlot;o.change=true;}
        else if(g.rangedWorn){o.arm=m.armed=true;o.change=true;}
    }else if(act==HolsterAct::Weapon){
        if(m.armed||inHand)bare();
        else{o.select=m.slot<16?m.slot:0;o.change=true;}                // nothing remembered (the game began bare): the first slot
    }else if(act==HolsterAct::Shield){
        if(g.shieldWorn){o.shield=1;o.change=true;}
        else if(g.shieldKept){o.shield=0;o.change=true;}
    }
    return o;
}
} // namespace evr::motion
namespace evr::game {
// The player's input as it is sent (keen::ecs::ClientPlayerInputData at 0): +60 actionbarSelectionAction {version,
// selected index}, +17c changeEquipmentAction {inventory slot {entity, index}, version, equipment slot, 0 equip /
// 1 unequip}, +318 digitalInput, a bit for each keen::ecs::PlayerInputType (47 RangedCombatArm: the ranged weapon of
// the equipment is in the hand for as long as the bit is set). The game takes an action when its version differs from
// the last one it took (enshrouded.exe 283d70, 308152) - so an action of the mod stays in the input, tick after tick,
// until the game itself writes a new one there; putting the game's old one back would run that again.
// Selected index: 0..15 a slot; the game takes 16 off anything above 15 (283d7d), and a stored slot above 15 is "no
// slot" (cbde50): 32 is sent for bare hands, stored as 16.
constexpr size_t inputBarAction=0x60,inputEquipAction=clientEquipAction,inputDigital=clientDigital;   // 1.0: 0x1c0 / 0x3a8 (0x17c / 0x318)
// 1.0 (2026-10-16): keen::ecs::PlayerInputType gained AreaLoot 8, DropItem 12 and BuildingModeFly 39 - RangedCombatArm is 50
// (47; 47 is CombatTargetSelect2dRight now), Block 30 (28).
constexpr unsigned inputRangedCombatArmBit=50;constexpr uint64_t inputRangedCombatArm=1ull<<inputRangedCombatArmBit;
// AreaLoot (bit 8 in 1.0): what the game sends after the interaction is held a while (headset run 20261020-100726: one long
// hold of Y gave ContextualAction_Hold and AreaLoot) - it takes all that lies around, and a killed enemy's loot, which the
// interaction alone does not (user, 2026-10-20: "the left trigger picks up dropped items but not the items from an enemy's
// body - make the trigger the picking-up"). The mod sends it for a short while when the left trigger is pulled
// (controller_ui_fixture.hpp, include/evr/live_pad.hpp leftInteract): until areaLootUntilMs (GetTickCount64).
constexpr unsigned inputAreaLootBit=8;constexpr uint64_t inputAreaLoot=1ull<<inputAreaLootBit;
inline std::atomic<uint64_t> areaLootUntilMs{0};
inline bool applyAreaLoot(ClientInputBytes& output,uint64_t now){
    if(now>=areaLootUntilMs.load())return false;
    uint64_t digital=0;std::memcpy(&digital,output.data()+inputDigital,8);digital|=inputAreaLoot;std::memcpy(output.data()+inputDigital,&digital,8);return true;
}
constexpr uint8_t equipmentSlotShield=15,equipmentSlotInvalid=27;
struct HolsterRequest {
    uint64_t serial=0;bool arm=false;
    int select=-1;                                    // 0..15, 16: none
    int equip=-1;uint8_t equipmentSlot=equipmentSlotInvalid;uint32_t entity=0;uint8_t index=0xff;   // 0 equip from {entity, index}, 1 unequip
};
class HolsterSender {
    uint64_t taken_=0;bool arm_=false,barOn_=false,equipOn_=false;uint32_t version_=0x45560000;
    std::array<uint8_t,8> bar_{},barSource_{};std::array<uint8_t,16> equip_{},equipSource_{};
public:
    struct Applied {bool barBegan=false,barEnded=false,equipBegan=false,equipEnded=false,arm=false,barOn=false,equipOn=false;std::array<uint8_t,8> bar{};std::array<uint8_t,16> equip{};};
    void reset(){arm_=barOn_=equipOn_=false;}
    // Where the versions begin. The game's own are small counts; a later session must not begin where an earlier one
    // ended, in case the last version the game took outlives the session.
    void seed(uint32_t version){version_=version;}
    bool arm()const{return arm_;}
    Applied apply(const HolsterRequest& r,const ClientInputBytes& source,ClientInputBytes& output){
        Applied a;std::array<uint8_t,8> bar{};std::array<uint8_t,16> equip{};
        std::memcpy(bar.data(),source.data()+inputBarAction,8);std::memcpy(equip.data(),source.data()+inputEquipAction,16);
        if(barOn_&&bar!=barSource_){barOn_=false;a.barEnded=true;}          // the game's own action: it goes as it is
        if(equipOn_&&equip!=equipSource_){equipOn_=false;a.equipEnded=true;}
        if(r.serial!=taken_){taken_=r.serial;arm_=r.arm;
            if(r.select>=0&&r.select<=16){
                const uint32_t version=++version_;bar_={};std::memcpy(bar_.data(),&version,4);bar_[4]=uint8_t(r.select<16?r.select:32);
                barSource_=bar;barOn_=true;a.barBegan=true;}
            if(r.equip==0||r.equip==1){
                const uint32_t version=++version_;equip_={};std::memcpy(equip_.data(),&r.entity,4);equip_[4]=r.index;std::memcpy(equip_.data()+8,&version,4);
                equip_[12]=r.equipmentSlot;equip_[13]=uint8_t(r.equip);
                equipSource_=equip;equipOn_=true;a.equipBegan=true;}
        }
        if(barOn_)std::memcpy(output.data()+inputBarAction,bar_.data(),8);
        if(equipOn_)std::memcpy(output.data()+inputEquipAction,equip_.data(),16);
        if(arm_){uint64_t digital=0;std::memcpy(&digital,output.data()+inputDigital,8);digital|=inputRangedCombatArm;std::memcpy(output.data()+inputDigital,&digital,8);}
        a.arm=arm_;a.barOn=barOn_;a.equipOn=equipOn_;a.bar=bar_;a.equip=equip_;return a;
    }
};
// Diving by the arms' strokes (2026-10-20, user: "stroke up, stroke down to dive and come up"; include/evr/swim_stroke.hpp
// up): keen::ecs::PlayerInputType SwimStartDiving 34, DiveAscend 35, DiveDescend 36 in 1.0 (read from the type's own
// table in the executable at 0x16e9760, 40 bytes an entry, the known 7 / 8 / 50 / 55 in the same order; before 1.0 the
// table of docs/analysis/input_digital_enum.json had them at 32..34). The bits in swimInputBits go into the sent input
// until swimInputUntilMs (controller_ui_fixture.hpp sets them every frame while it wants them).
constexpr unsigned inputSwimStartDivingBit=34,inputDiveAscendBit=35,inputDiveDescendBit=36;
constexpr uint64_t inputSwimStartDiving=1ull<<inputSwimStartDivingBit,inputDiveAscend=1ull<<inputDiveAscendBit,inputDiveDescend=1ull<<inputDiveDescendBit;
constexpr uint64_t inputSwimBits=inputSwimStartDiving|inputDiveAscend|inputDiveDescend;
inline std::atomic<uint64_t> swimInputBits{0},swimInputUntilMs{0};
inline bool applySwimInput(ClientInputBytes& output,uint64_t now){
    const auto bits=swimInputBits.load()&inputSwimBits;
    if(!bits||now>=swimInputUntilMs.load())return false;
    uint64_t digital=0;std::memcpy(&digital,output.data()+inputDigital,8);digital|=bits;std::memcpy(output.data()+inputDigital,&digital,8);return true;
}
// Nothing but the head camera, the two actions, the ranged-combat bit, the AreaLoot bit and the diving bits differs.
inline bool onlyHolsterAndCameraChanged(const ClientInputBytes& a,const ClientInputBytes& b){
    for(size_t i=0;i<a.size();++i){
        if(a[i]==b[i]||(i>=0x20&&i<0x4c)||(i>=inputBarAction&&i<inputBarAction+8)||(i>=inputEquipAction&&i<inputEquipAction+16))continue;
        // the ranged-combat bit only (bit 50 in 1.0: byte 6, 0x04; June bit 47: byte 5, 0x80)
        if(i==inputDigital+inputRangedCombatArmBit/8&&((a[i]^b[i])&~uint8_t(1u<<(inputRangedCombatArmBit%8)))==0)continue;
        if(i==inputDigital+inputAreaLootBit/8&&((a[i]^b[i])&~uint8_t(1u<<(inputAreaLootBit%8)))==0)continue;
        if(i==inputDigital+4&&((a[i]^b[i])&~uint8_t(inputSwimBits>>32))==0)continue;   // bits 34..36: byte 4
        return false;}
    return true;
}
} // namespace evr::game
