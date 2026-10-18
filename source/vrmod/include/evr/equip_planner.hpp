#pragma once
#include <evr/grip_router.hpp>
#include <array>
#include <cmath>
#include <cstdint>
namespace evr::motion {
// Value-only equip planning from a native actionbar snapshot (game175428: Inventory of the
// first SlotSelection actionbar entity = 8 x {itemId, count, itemEntity}). Selecting a wheel
// slot USES a consumable (game170323), so only unique weapon instances are ever targeted:
// itemEntity != 0, count == 1, ItemInfo category 2 and the role's equipment slot.
struct ActionbarSlot {uint32_t item=0,count=0,entity=0;uint8_t category=0,equipmentSlot=0;bool metadata=false;};
struct ActionbarSnapshot {
    uint64_t milliseconds=0,sequence=0;uint32_t barEntity=0;uint8_t selected=255;bool valid=false;
    std::array<ActionbarSlot,8> slots{};
};
enum class EquipPlanResult {Unknown,Target,AlreadySelected,NoMatchingSlot};
struct EquipPlan {EquipPlanResult result=EquipPlanResult::Unknown;uint8_t slot=255;};
inline bool equipSlotMatches(const ActionbarSlot& s,EquipRequest r){
    if(!s.metadata||!s.item||!s.entity||s.count!=1||s.category!=2)return false;
    switch(r){
        case EquipRequest::MainWeapon:return s.equipmentSlot==1;               // MeleeWeapon
        case EquipRequest::Bow:return s.equipmentSlot==5;                      // Range
        case EquipRequest::Staff:return s.equipmentSlot==4||s.equipmentSlot==7; // MidRange / Spell
        case EquipRequest::Shield:return s.equipmentSlot==18;                  // usually not on the bar
        default:return false;
    }
}
// preferredMelee: the last melee slot the player actually had selected (keeps "main weapon"
// meaning the user's weapon, not simply the first melee item on the bar).
inline EquipPlan planEquip(const ActionbarSnapshot& a,EquipRequest r,uint8_t preferredMelee=255){
    if(!a.valid||a.selected>7||r==EquipRequest::None)return {};
    if(equipSlotMatches(a.slots[a.selected],r))return {EquipPlanResult::AlreadySelected,a.selected};
    if(r==EquipRequest::MainWeapon&&preferredMelee<8&&equipSlotMatches(a.slots[preferredMelee],r))return {EquipPlanResult::Target,preferredMelee};
    for(uint8_t k=0;k<8;k++)if(equipSlotMatches(a.slots[k],r))return {EquipPlanResult::Target,k};
    return {EquipPlanResult::NoMatchingSlot,255};
}
// Gamepad quick wheel slot centres measured in game171831: theta = -91 + 26 * slot degrees,
// clockwise from up. Returns a unit right-stick vector.
inline XrVector2f wheelSlotDirection(uint8_t slot){
    if(slot>7)throw std::invalid_argument("wheel slot outside actionbar");
    const float t=(-91.f+26.f*slot)*3.14159265358979f/180.f;return {std::sin(t),std::cos(t)};
}
struct WheelSelectConfig {XrDuration openTimeout=0,aimHold=0,closeTimeout=0,settleTime=0,settleTimeout=0;
    bool valid()const{return openTimeout>0&&aimHold>0&&closeTimeout>0&&settleTime>0&&settleTimeout>settleTime;}};
// Drives RB + right stick through one native wheel selection and confirms it from the
// debounced native actionbar selection. Never retries: a failure ends with neutral output.
class WheelSelect {
public:
    enum class Phase {Idle,Opening,Aiming,Releasing,Settling,Done,Failed};
    struct Step {uint16_t buttons=0;XrVector2f stick{};Phase phase=Phase::Idle;const char* error=nullptr;};
    explicit WheelSelect(WheelSelectConfig c):c_(c){if(!c_.valid())throw std::invalid_argument("wheel select timing required");}
    bool active()const{return phase_==Phase::Opening||phase_==Phase::Aiming||phase_==Phase::Releasing||phase_==Phase::Settling;}
    Phase phase()const{return phase_;}uint8_t target()const{return target_;}
    void start(uint8_t slot,XrTime now){if(active())throw std::logic_error("wheel selection already active");dir_=wheelSlotDirection(slot);target_=slot;phase_=Phase::Opening;since_=now;stable_=0;}
    // selected: native actionbar index from a fresh SlotSelection read (255 when stale/unknown).
    Step update(XrTime now,bool overlay,uint8_t selected){
        Step o;o.phase=phase_;
        auto fail=[&](const char* e){phase_=Phase::Failed;o={};o.phase=phase_;o.error=e;return o;};
        switch(phase_){
        case Phase::Opening:
            o.buttons=0x0200;
            if(overlay){phase_=Phase::Aiming;since_=now;o.stick=dir_;}
            else if(now-since_>c_.openTimeout)return fail("wheel did not open");
            break;
        case Phase::Aiming:
            if(!overlay)return fail("wheel closed before selection");
            o.buttons=0x0200;o.stick=dir_;
            if(now-since_>=c_.aimHold){phase_=Phase::Releasing;since_=now;o.buttons=0;}
            break;
        case Phase::Releasing:
            o.stick=dir_; // keep the aim while RB is released, as the gamepad wheel requires
            if(!overlay){phase_=Phase::Settling;since_=now;o.stick={};}
            else if(now-since_>c_.closeTimeout)return fail("wheel did not close");
            break;
        case Phase::Settling:
            if(overlay)return fail("wheel reopened while settling");
            if(selected==target_){if(!stable_)stable_=now;if(now-stable_>=c_.settleTime)phase_=Phase::Done;}
            else stable_=0;
            if(phase_!=Phase::Done&&now-since_>c_.settleTimeout)return fail("selection not confirmed");
            break;
        default:break;
        }
        o.phase=phase_;return o;
    }
    void cancel(){if(active())phase_=Phase::Failed;}
private:
    WheelSelectConfig c_;Phase phase_=Phase::Idle;XrVector2f dir_{};uint8_t target_=255;XrTime since_=0,stable_=0;
};
} // namespace evr::motion
