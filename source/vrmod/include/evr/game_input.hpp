#pragma once
#include <evr/game_stereo.hpp>
#include <atomic>

namespace evr::game {
// Value-only input proposal. This does not mutate a game component or implement
// a network format. Native movement/aim consumers still need separate routing.
enum class InputFallback {
    None, Disabled, UnknownState, NotLocal, NotReady, NotGamepad,
    Ui, BuildingOrCursor, TargetLock, TrackingUnavailable, InvalidInput
};
inline const char* fallbackName(InputFallback reason) {
    switch(reason) {
    case InputFallback::None:return "none";
    case InputFallback::Disabled:return "disabled";
    case InputFallback::UnknownState:return "unknown_state";
    case InputFallback::NotLocal:return "not_local";
    case InputFallback::NotReady:return "not_ready";
    case InputFallback::NotGamepad:return "not_gamepad";
    case InputFallback::Ui:return "ui";
    case InputFallback::BuildingOrCursor:return "building_or_cursor";
    case InputFallback::TargetLock:return "target_lock";
    case InputFallback::TrackingUnavailable:return "tracking_unavailable";
    case InputFallback::InvalidInput:return "invalid_input";
    }
    return "unknown_state";
}
struct NativeInputState {
    bool enabled=false,known=false,local=false,ready=false;
    uint8_t inputMode=0,clientMask=0;
    uint32_t playerFlags=0;
    bool buildingCamera=false,specialCursor=false;
    std::array<float,2> stick{}; // game coordinates: right, forward (+Z)
};
struct HeadInputSnapshot {
    StableAnchor anchor;
    Pose trackingToAnchor=identityPose(),head=identityPose();
    XrSpaceLocationFlags flags=0;
    // The pointer (aim pose) of the right hand in the same tracking space, for placing by hand (pointer_aim_camera.hpp).
    Pose pointer=identityPose();XrSpaceLocationFlags pointerFlags=0;
    uint64_t xrFrame=0,originEpoch=0;
    XrTime predictedDisplayTime=0;
    bool current=false; // caller must compare session/origin generation and age
};
struct InputProposal {
    InputFallback fallback=InputFallback::Disabled;
    uint64_t xrFrame=0,originEpoch=0;
    XrTime predictedDisplayTime=0;
    WorldPositionQ32 cameraPosition{};
    Quat cameraOrientation{0,0,0,1},aimWithoutRoll{0,0,0,1},movementOrientation{0,0,0,1};
    Vec3 aimForward{},worldMovement{};
    float yaw=0;
    bool active() const {return fallback==InputFallback::None;}
};
// keen::ecs::PlayerFlags InTargetLockCombatMode: bit 14 in 1.0 (the executable's reflection, 2026-10-19). The June build had
// the lock at 12, which 1.0 gives to WantsToWalk - so from the 1.0 update on, the lock no longer handed the walk to the game's
// lock camera, and the walk toggle did instead (headset run 20261019-152240: player flags 16897 = bits 0, 9, 14 while locked,
// the head camera applied throughout).
inline constexpr uint32_t targetLockFlag=1u<<14;
// A lock on a target does not hand the walk to the game's lock camera: the headset steers as without one. (Lock-on
// movement - the lock camera, the target ahead - was an option from 15:30 to 18:10 on 2026-10-19; the user after trying
// it: "the lock-on based camera is awkward to use, remove it".) The UI's full-screen states still keep the game's camera.
inline constexpr uint32_t nativeCameraFlags(){return (1u<<1)|(1u<<2);}
inline InputFallback inputGate(const NativeInputState& s,const HeadInputSnapshot& h) {
    if(!s.enabled)return InputFallback::Disabled;
    if(!s.known)return InputFallback::UnknownState;
    if(!s.local)return InputFallback::NotLocal;
    if(!s.ready||!(s.playerFlags&1u))return InputFallback::NotReady;
    if(s.inputMode!=1)return InputFallback::NotGamepad;
    if(s.playerFlags&((1u<<1)|(1u<<2)))return InputFallback::Ui;
    // All currently known ClientPlayerState bits represent special modes.
    // Unknown future bits also fall back; zero isn't proof of absence of UI.
    if(s.clientMask||s.buildingCamera||s.specialCursor)return InputFallback::BuildingOrCursor;
    constexpr auto tracking=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|
                            XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    if(!h.current||!h.xrFrame||!h.originEpoch||h.predictedDisplayTime<=0||(h.flags&tracking)!=tracking)
        return InputFallback::TrackingUnavailable;
    return InputFallback::None;
}
inline InputProposal proposeInput(const NativeInputState& state,const HeadInputSnapshot& head,float previousYaw) {
    InputProposal result;result.fallback=inputGate(state,head);
    if(!result.active())return result;
    try {
        if(!std::isfinite(previousYaw)||!std::isfinite(state.stick[0])||!std::isfinite(state.stick[1])||
           std::abs(state.stick[0])>1||std::abs(state.stick[1])>1)
            throw std::invalid_argument("invalid input axes/heading");
        const auto orientation=normalized(head.anchor.orientation);
        const auto local=trackingPose(compose(head.trackingToAnchor,head.head));
        const auto rootCamera=evr::cameraPosition(head.anchor.root,rotate(orientation,head.anchor.cameraOffsetGameUnits),1);
        result.cameraPosition=evr::cameraPosition(rootCamera,rotate(orientation,local.position),head.anchor.worldUnitsPerMeter);
        result.cameraOrientation=normalized(multiply(orientation,local.orientation));
        result.aimForward=rotate(result.cameraOrientation,{0,0,1});
        result.yaw=heading(result.cameraOrientation,previousYaw);
        result.movementOrientation=euler(result.yaw,0,0);
        result.worldMovement=rotate(result.movementOrientation,{state.stick[0],0,state.stick[1]});
        // Separate from the rendered camera: same forward, no physical roll.
        // This alone does NOT fix native full-rotation locomotion paths.
        // asin(y) amplifies float rounding at vertical pitch and can produce
        // a false horizontal component. atan2 retains the measured tiny X/Z.
        const float pitch=std::atan2(-result.aimForward.y,
            std::hypot(result.aimForward.x,result.aimForward.z));
        result.aimWithoutRoll=euler(result.yaw,pitch,0);
        result.xrFrame=head.xrFrame;result.originEpoch=head.originEpoch;
        result.predictedDisplayTime=head.predictedDisplayTime;
    }catch(const std::invalid_argument&){result=InputProposal{};result.fallback=InputFallback::InvalidInput;}
    return result;
}
} // namespace evr::game
