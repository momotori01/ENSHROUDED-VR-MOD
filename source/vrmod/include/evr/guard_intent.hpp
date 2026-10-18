#pragma once
#include <evr/motion_intent.hpp>
namespace evr::motion {
// Posture intent only. No native input writes, parry timers, hit results or
// player transforms. All distances below require explicit user calibration.
enum class GuardKind { None, Shield, TwoHanded };
enum class GuardBlock { None, Disabled, NativeUnknown, UiOrEvent, Tracking, Invalid, Discontinuity, Grip, Outside, NativeUnavailable, AttackPriority };
struct GuardBody {
    Vec3 navel{},forward{}; // Tracking-space, stable horizontal body direction.
    Vec3 head{};            // Same-sample HMD position (head-relative shoulder reach).
    uint64_t frame=0,session=0,origin=0,calibration=0;
    XrTime time=0;bool known=false;
};
struct GuardEquipment {
    GuardKind kind=GuardKind::None;uint64_t binding=0;unsigned shieldHand=0;
    Weapon support{}; // Same measured two-hand support constraint as attack.
};
struct GuardCalibration {
    float shieldEnterFront=0,shieldExitFront=0,twoHandEnterFront=0,twoHandExitFront=0;
    float releaseHeightMargin=0,overheadHeight=0,overheadBackAllowance=0;
    float maximumHandSpeed=0,maximumHandAngularSpeed=0;
    XrDuration maximumGap=0,rearmOutsideTime=0;
    bool valid()const {
        const float a[]{shieldEnterFront,shieldExitFront,twoHandEnterFront,twoHandExitFront,releaseHeightMargin,
            overheadHeight,overheadBackAllowance,maximumHandSpeed,maximumHandAngularSpeed};
        for(float v:a)if(!std::isfinite(v)||v<0)return false;
        return shieldEnterFront>shieldExitFront&&twoHandEnterFront>twoHandExitFront&&releaseHeightMargin>0&&
            overheadHeight>releaseHeightMargin&&maximumHandSpeed>0&&maximumHandAngularSpeed>0&&maximumGap>0&&rearmOutsideTime>0;
    }
};
struct GuardAuthority {
    bool enabled=false,known=false,local=false,alive=false,uiOrEvent=true,reservedGesture=false;
    bool canBlock=false,canRequestParry=false;
    // A verified native adapter must explicitly permit overlap. Ability masks
    // alone cannot establish it. A denied entry is discarded, never queued.
    bool attackRequested=false,attackInProgress=false,attackAndGuardVerified=false;
};
struct GuardIntent {
    uint64_t frame=0,binding=0;GuardBlock blocked=GuardBlock::Disabled;
    bool valid=false,posture=false,entered=false,released=false,blockHeld=false,parryCandidate=false;
    float forward=0,aboveNavel=0;
};
class GuardGate {
    GuardCalibration c;HandFrame previous{};GuardBody previousBody{};GuardEquipment previousEquipment{};
    bool havePrevious=false,stance=false,armed=false;XrTime outsideSince=0;
    void reset(){havePrevious=stance=armed=false;outsideSince=0;}
public:
    explicit GuardGate(GuardCalibration calibration):c(calibration){if(!c.valid())throw std::invalid_argument("guard body calibration required");}
    GuardIntent update(const HandFrame& f,const GuardBody& body,const GuardEquipment& equipment,const GuardAuthority& n){
        GuardIntent o;o.frame=f.frame;o.binding=equipment.binding;
        auto stop=[&](GuardBlock b){o.released=stance;o.blocked=b;reset();return o;};
        if(!n.enabled)return stop(GuardBlock::Disabled);
        if(!n.known||!n.local||!n.alive)return stop(GuardBlock::NativeUnknown);
        if(n.uiOrEvent||n.reservedGesture)return stop(GuardBlock::UiOrEvent);
        if(!body.known||!body.calibration||body.frame!=f.frame||body.session!=f.sessionGeneration||body.origin!=f.originGeneration||body.time!=f.predictedTime||
            !f.frame||!f.sessionGeneration||!f.originGeneration||f.predictedTime<=0||!equipment.binding||(equipment.kind!=GuardKind::Shield&&equipment.kind!=GuardKind::TwoHanded)||equipment.shieldHand>1||
            !finite(body.navel)||!finite(body.forward)||std::abs(body.forward.y)>1e-5f||std::abs(length(body.forward)-1.f)>1e-4f)
            return stop(GuardBlock::Invalid);
        if(!f.focused)return stop(GuardBlock::Tracking);
        const bool two=equipment.kind==GuardKind::TwoHanded;
        const unsigned hand=two?0:equipment.shieldHand;
        try {
            for(unsigned h=0;h<2;h++)if(two||h==hand){
                if(!f.hands[h].tracked()||!f.hands[h].profile)return stop(GuardBlock::Tracking);
                if(!finite(f.hands[h].grip.position))return stop(GuardBlock::Invalid);
                (void)normalized(f.hands[h].grip.orientation);
            }
            if(two&&(!equipment.support.twoHanded||equipment.support.binding!=equipment.binding||!twoHandSupportValid(f,equipment.support)))
                return stop(GuardBlock::Grip);
            if(havePrevious){
                bool continuous=f.sessionGeneration==previous.sessionGeneration&&f.originGeneration==previous.originGeneration&&
                    f.frame>previous.frame&&f.predictedTime>previous.predictedTime&&f.predictedTime-previous.predictedTime<=c.maximumGap&&
                    body.calibration==previousBody.calibration&&equipment.binding==previousEquipment.binding&&
                    equipment.kind==previousEquipment.kind&&equipment.shieldHand==previousEquipment.shieldHand;
                for(unsigned h=0;h<2;h++)if(two||h==hand)continuous=continuous&&f.hands[h].profile==previous.hands[h].profile;
                if(!continuous)return stop(GuardBlock::Discontinuity);
                const float dt=float(f.predictedTime-previous.predictedTime)*1e-9f;
                for(unsigned h=0;h<2;h++)if(two||h==hand){
                    const auto a=normalized(previous.hands[h].grip.orientation),b=normalized(f.hands[h].grip.orientation);
                    const float speed=length(f.hands[h].grip.position-previous.hands[h].grip.position)/dt;
                    const float angular=2*std::acos(std::clamp(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w),0.f,1.f))/dt;
                    if(!std::isfinite(speed)||!std::isfinite(angular)||speed>c.maximumHandSpeed||angular>c.maximumHandAngularSpeed)return stop(GuardBlock::Discontinuity);
                }
            }
            const auto p=f.hands[hand].grip.position-body.navel;
            if(!finite(p))return stop(GuardBlock::Invalid);
            o.forward=dot(p,body.forward);o.aboveNavel=p.y;
            const float front=two?(stance?c.twoHandExitFront:c.twoHandEnterFront):(stance?c.shieldExitFront:c.shieldEnterFront);
            const float margin=stance?c.releaseHeightMargin:0;
            // Front half-space above the navel OR the calibrated overhead area.
            // There is deliberately no maximum Y / height cap.
            const bool inside=(o.aboveNavel>=-margin&&o.forward>=front)||
                (o.aboveNavel>=c.overheadHeight-margin&&o.forward>=-c.overheadBackAllowance);
            const bool entry=havePrevious&&!stance&&inside;
            o.entered=entry;o.released=stance&&!inside;
            const bool entryArmed=entry&&armed;
            if(inside){outsideSince=0;armed=false;}
            else {
                if(!outsideSince)outsideSince=f.predictedTime;
                if(f.predictedTime-outsideSince>=c.rearmOutsideTime)armed=true;
            }
            stance=inside;havePrevious=true;previous=f;previousBody=body;previousEquipment=equipment;
            o.valid=true;o.posture=inside;
            if(!inside){o.blocked=GuardBlock::Outside;return o;}
            if((n.attackRequested||n.attackInProgress)&&!n.attackAndGuardVerified){o.blocked=GuardBlock::AttackPriority;return o;}
            if(!n.canBlock){o.blocked=GuardBlock::NativeUnavailable;return o;}
            o.blocked=GuardBlock::None;o.blockHeld=true;
            // First sample or recovered tracking may sustain a block, but must
            // not fabricate a new parry edge. Native permission loss is not a
            // posture release and never rearms this candidate.
            o.parryCandidate=entryArmed&&n.canRequestParry;return o;
        }catch(const std::exception&){return stop(GuardBlock::Invalid);}
    }
};
} // namespace evr::motion
