#pragma once
#include <evr/motion_actions.hpp>
namespace evr::motion {
// Value-only stage. Native integration must supply verified action permission,
// recovery timing and acknowledgements. No guessed game cooldown lives here.
enum class WeaponKind {Melee,Staff};
enum class IntentKind {None,Melee,AirborneSpecial,Cast};
enum class Block {None,Disabled,NativeUnknown,UiOrEvent,Tracking,Discontinuity,Grip,Cooldown,NativeBusy,AwaitAck,AwaitNeutral,Invalid};
struct Weapon {
    uint64_t binding=0; // Lifetime-qualified local equipment identity, not a model index.
    WeaponKind kind=WeaponKind::Melee;unsigned mainHand=1;bool twoHanded=false;
    Pose gripToModel=identityPose(),modelToMuzzle=identityPose();
    Vec3 offHandGripInModel{}; // Contact/model geometry never supplies attack speed.
    float offHandTolerance=0; // Required per-item calibration if two handed.
};
struct NativeCombat {
    bool enabled=false,known=false,local=false,alive=false,uiOrEvent=true;
    bool canStart=false,airborne=false,airborneSpecialAllowed=false;
    // Same native time unit for both values. Adapter verifies current action,
    // combo branch and speed modifiers before publishing these values.
    double recoveryRemaining=0,recoveryTotal=0;
};
struct GestureConfig {
    float beginSpeed=0,resetSpeed=0,maxHaptic=0;
    uint32_t neutralSamples=0;
    XrDuration maximumGap=0;
    // Explicit tracking-integrity limits, measured on raw wrist poses rather
    // than the weapon tip. They must be calibrated before combat is enabled.
    float maximumWristSpeed=0,maximumWristAngularSpeed=0;
    bool valid()const{return std::isfinite(beginSpeed)&&std::isfinite(resetSpeed)&&std::isfinite(maxHaptic)&&
        beginSpeed>resetSpeed&&resetSpeed>=0&&maxHaptic>=0&&maxHaptic<=1&&neutralSamples>0&&maximumGap>0&&
        std::isfinite(maximumWristSpeed)&&maximumWristSpeed>0&&
        std::isfinite(maximumWristAngularSpeed)&&maximumWristAngularSpeed>0;}
};
struct Intent {
    IntentKind kind=IntentKind::None;Block blocked=Block::Disabled;uint64_t request=0,cancelledRequest=0,frame=0,binding=0;
    float speed=0;std::array<Vec3,2> controllerVelocity{};uint64_t velocityPreviousFrame=0;
    std::array<float,2> haptic{};bool castHeld=false;
    Pose weaponInTracking=identityPose();Vec3 aimOrigin{},aimDirection{};
};
inline Quat alignDirections(Vec3 from,Vec3 to){
    auto a=length(from),b=length(to);if(a<1e-6f||b<1e-6f)throw std::invalid_argument("degenerate two hand grip axis");
    from=from*(1/a);to=to*(1/b);const float d=std::clamp(dot(from,to),-1.f,1.f);
    if(d<-.99999f){auto axis=cross(from,std::abs(from.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0});axis=axis*(1/length(axis));return {axis.x,axis.y,axis.z,0};}
    const auto c=cross(from,to);return normalized({c.x,c.y,c.z,1+d});
}
inline Pose weaponPose(const HandFrame& f,const Weapon& w){
    if(w.mainHand>1||!w.binding)throw std::invalid_argument("invalid bound weapon");
    auto pose=compose(f.hands[w.mainHand].grip,w.gripToModel);
    if(!w.twoHanded)return pose;
    const auto main=f.hands[w.mainHand].grip.position;
    const auto expected=pose.position+rotate(pose.orientation,w.offHandGripInModel)-main;
    const auto actual=f.hands[1-w.mainHand].grip.position-main;
    const auto correction=alignDirections(expected,actual);
    return {normalized(multiply(correction,pose.orientation)),main+rotate(correction,pose.position-main)};
}
// Shared value-only support check for frontend, event selection and physics.
// It does not infer attack authority from a pose, and never changes native state.
inline bool twoHandSupportValid(const HandFrame& f,const Weapon& w){
    if(!w.twoHanded||w.mainHand>1||!w.binding||!f.focused||!std::isfinite(w.offHandTolerance)||w.offHandTolerance<=0)return false;
    for(const auto& h:f.hands)if(!h.tracked()||!h.grabActive||!h.grab||!finite(h.grip.position))return false;
    try{
        for(const auto& h:f.hands)(void)normalized(h.grip.orientation);
        const auto pose=weaponPose(f,w);const auto expected=pose.position+rotate(pose.orientation,w.offHandGripInModel);
        return finite(expected)&&length(expected-f.hands[1-w.mainHand].grip.position)<=w.offHandTolerance;
    }catch(...){return false;}
}
class IntentGate {
    GestureConfig config;
    HandFrame previous{};uint64_t binding=0,nextRequest=0,pending=0;uint32_t neutral=0;
    bool havePrevious=false,armed=false,triggerWasDown=true;
    void clearGesture(){havePrevious=armed=false;neutral=0;triggerWasDown=true;}
public:
    explicit IntentGate(GestureConfig c):config(c){if(!config.valid())throw std::invalid_argument("motion gesture calibration required");}
    uint64_t pendingRequest()const{return pending;}
    // Must be called only for the matching verified native acceptance/rejection.
    // Rejection is not queued for retry. A cooldown is still read from native.
    bool acknowledge(uint64_t id){if(!id||id!=pending)return false;pending=0;armed=false;neutral=0;return true;}
    Intent update(const HandFrame& f,const Weapon& w,const NativeCombat& n){
        Intent out;out.frame=f.frame;out.binding=w.binding;
        auto cancel=[&](){out.cancelledRequest=pending;pending=0;out.castHeld=false;out.haptic={};clearGesture();};
        auto block=[&](Block b){out.blocked=b;out.castHeld=false;out.haptic={};cancel();return out;};
        if(!n.enabled)return block(Block::Disabled);
        if(!n.known||!n.local||!n.alive)return block(Block::NativeUnknown);
        if(n.uiOrEvent)return block(Block::UiOrEvent);
        if(!std::isfinite(n.recoveryRemaining)||!std::isfinite(n.recoveryTotal)||n.recoveryRemaining<0||n.recoveryTotal<0||n.recoveryRemaining>n.recoveryTotal)
            return block(Block::Invalid);
        if(w.mainHand>1||!w.binding||!f.frame||!f.sessionGeneration||!f.originGeneration||f.predictedTime<=0)return block(Block::Invalid);
        const auto& main=f.hands[w.mainHand];const auto& off=f.hands[1-w.mainHand];
        if(!f.focused||!main.tracked()||!main.grabActive||(w.kind==WeaponKind::Staff&&!main.triggerActive)||
            (w.twoHanded&&(!off.tracked()||!off.grabActive)))return block(Block::Tracking);
        try{
            if(!finite(main.grip.position)||!std::isfinite(main.trigger)||main.trigger<0||main.trigger>1)return block(Block::Invalid);
            out.weaponInTracking=weaponPose(f,w);const auto muzzle=compose(out.weaponInTracking,w.modelToMuzzle);
            out.aimOrigin=muzzle.position;out.aimDirection=rotate(muzzle.orientation,{0,0,-1});
            if(!finite(out.weaponInTracking.position)||!finite(out.aimOrigin)||!finite(out.aimDirection))return block(Block::Invalid);
            if(w.twoHanded){
                const auto grip=out.weaponInTracking.position+rotate(out.weaponInTracking.orientation,w.offHandGripInModel);
                if(!std::isfinite(w.offHandTolerance)||w.offHandTolerance<=0||!finite(grip)||!finite(off.grip.position))return block(Block::Invalid);
                if(length(grip-off.grip.position)>w.offHandTolerance)return block(Block::Grip);
            }
            if(!main.grab||(w.twoHanded&&!off.grab))return block(Block::Grip);
            if(n.recoveryRemaining>0){const float a=config.maxHaptic*float(n.recoveryRemaining/n.recoveryTotal);out.haptic[w.mainHand]=a;if(w.twoHanded)out.haptic[1-w.mainHand]=a;}
            const bool continuous=havePrevious&&binding==w.binding&&previous.sessionGeneration==f.sessionGeneration&&previous.originGeneration==f.originGeneration&&
                previous.hands[w.mainHand].profile==main.profile&&(!w.twoHanded||previous.hands[1-w.mainHand].profile==off.profile)&&
                f.frame>previous.frame&&f.predictedTime>previous.predictedTime&&f.predictedTime-previous.predictedTime<=config.maximumGap;
            if(continuous){
                const float dt=float(f.predictedTime-previous.predictedTime)*1e-9f;
                for(unsigned h=0;h<2;h++)if(h==w.mainHand||w.twoHanded){
                    const auto& a=previous.hands[h].grip;const auto& b=f.hands[h].grip;
                    const auto qa=normalized(a.orientation),qb=normalized(b.orientation);
                    const float qdot=std::abs(qa.x*qb.x+qa.y*qb.y+qa.z*qb.z+qa.w*qb.w);
                    const float angular=2.f*std::acos(std::clamp(qdot,0.f,1.f))/dt;
                    out.controllerVelocity[h]=(b.position-a.position)*(1.f/dt);
                    const float linear=length(out.controllerVelocity[h]);
                    if(!std::isfinite(linear)||!std::isfinite(angular)||linear>config.maximumWristSpeed||angular>config.maximumWristAngularSpeed)
                        return block(Block::Discontinuity);
                }
                // User policy 2026-10-04: two-hand attacks use RIGHT controller
                // translation only; the left hand is a support/pose constraint.
                // Both hands still need valid tracking, grip and continuity.
                // Wrist rotation, model length and support-only motion add no speed.
                out.velocityPreviousFrame=previous.frame;
                out.speed=length(out.controllerVelocity[w.twoHanded?1:w.mainHand]);
            }else cancel();
            previous=f;binding=w.binding;havePrevious=true;
            if(!std::isfinite(out.speed))return block(Block::Invalid);
            const bool triggerDown=main.trigger>=.75f||(triggerWasDown&&main.trigger>.25f);
            const bool edge=triggerDown&&!triggerWasDown;triggerWasDown=triggerDown;
            if(!continuous){out.blocked=Block::Discontinuity;return out;}
            // Native hold input is a level for sustained casting; start intent
            // remains edge-triggered and never bypasses native recovery.
            out.castHeld=w.kind==WeaponKind::Staff&&triggerDown;
            if(pending){out.blocked=Block::AwaitAck;return out;}
            if(n.recoveryRemaining>0){armed=false;neutral=0;out.blocked=Block::Cooldown;return out;}
            if(!n.canStart){armed=false;neutral=0;out.blocked=Block::NativeBusy;return out;}
            const bool resting=w.kind==WeaponKind::Staff?!triggerDown:out.speed<=config.resetSpeed;
            if(resting){if(neutral<config.neutralSamples)++neutral;if(neutral>=config.neutralSamples)armed=true;}
            else neutral=0;
            const bool fire=armed&&(w.kind==WeaponKind::Staff?edge:out.speed>=config.beginSpeed);
            if(!fire){out.blocked=armed?Block::None:Block::AwaitNeutral;return out;}
            out.kind=w.kind==WeaponKind::Staff?IntentKind::Cast:(n.airborne&&n.airborneSpecialAllowed?IntentKind::AirborneSpecial:IntentKind::Melee);
            out.request=pending=++nextRequest;out.blocked=Block::None;armed=false;neutral=0;return out;
        }catch(const std::exception&){return block(Block::Invalid);}
    }
};
} // namespace evr::motion
