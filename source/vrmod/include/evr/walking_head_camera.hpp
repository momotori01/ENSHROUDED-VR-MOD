#pragma once
#include <evr/idle_head_camera.hpp>

namespace evr::game {
// Value-copy stage for ordinary analog walking. Separate from the live idle
// diagnostic: no hook enables this function implicitly. Digital actions,
// airborne/special locomotion and unknown states retain all native bytes.
struct WalkingHeadGate {
    IdleHeadGate player;
    bool actorStateCurrent=false,gravityPresent=false,gravityActive=false;
    bool swimHead=true; // false: swimming keeps the native camera, as before 2026-10-13 (EVR_SWIM_HEAD_CAMERA=0)
    uint64_t actorState=0;
    Vec3 gravityDirection{};
};
inline constexpr uint64_t ordinaryWalkingStates =
    (1ull<<0)|(1ull<<6)|(1ull<<8)|(1ull<<13)|(1ull<<14)|(1ull<<16)|
    (1ull<<17)|(1ull<<18)|(1ull<<19)|(1ull<<20)|(1ull<<21)|(1ull<<22)|
    (1ull<<23)|(1ull<<24)|(1ull<<40)|(1ull<<41)|(1ull<<42)|(1ull<<46)|
    (1ull<<47)|(1ull<<49)|(1ull<<50)|(1ull<<52)|(1ull<<54)|(1ull<<55);
inline IdleHeadResult patchWalkingHeadCamera(const ClientInputBytes& source,ClientInputBytes& output,
        const WalkingHeadGate& gate,const HeadInputSnapshot& head,float previousYaw){
    output=source;IdleHeadResult result;
    const auto& player=gate.player;
    if(!player.normalUiBound){result.status=IdleHeadStatus::Ui;return result;}
    constexpr uint64_t requiredState=(1ull<<0)|(1ull<<50); // Grounded + IsReadyForGameplay
    if(!gate.actorStateCurrent||!gate.gravityPresent||!gate.gravityActive||
       (gate.actorState&requiredState)!=requiredState||(gate.actorState&~ordinaryWalkingStates)||
       !finite(gate.gravityDirection)||length(gate.gravityDirection-Vec3{0,-1,0})>1e-5f){
        result.status=IdleHeadStatus::SpecialMode;return result;
    }
    std::array<float,2> stick;std::memcpy(stick.data(),source.data()+clientStick,sizeof(stick));
    if(!std::isfinite(stick[0])||!std::isfinite(stick[1])||std::abs(stick[0])>1||std::abs(stick[1])>1)return result;
    // Retain the already-validated idle camera behavior when the last real
    // device was keyboard. Active analog movement still requires gamepad mode.
    if(source[clientInputMode]!=1&&(source[clientInputMode]!=0||stick[0]!=0||stick[1]!=0)){
        result.status=IdleHeadStatus::SpecialMode;return result;}
    // Reuse every existing camera/UI/tracking guard with only the analog test
    // relaxed. The real stick and all other fields are retained in output.
    auto idleSource=source;
    std::fill(idleSource.begin()+clientStick,idleSource.begin()+clientStick+8,0);
    ClientInputBytes scratch;
    result=patchIdleHeadCamera(idleSource,scratch,player,head,previousYaw);
    if(result.status!=IdleHeadStatus::Applied)return result;
    result.proposal.worldMovement=rotate(result.proposal.movementOrientation,{stick[0],0,stick[1]});
    writeHeadCamera(output,result.proposal);
    return result;
}

// Explicit, separately evidenced ordinary melee extension. Only the native
// main-hand press/tap/hold/release bits may accompany the private head camera.
// The output always begins from the ORIGINAL input: digital bytes are retained.
inline IdleHeadResult patchMeleeHeadCamera(const ClientInputBytes& source,ClientInputBytes& output,
        const WalkingHeadGate& gate,const HeadInputSnapshot& head,float previousYaw,bool meleeEvidence){
    output=source;
    if(!meleeEvidence)return patchWalkingHeadCamera(source,output,gate,head,previousYaw);
    uint64_t digital=0;std::memcpy(&digital,source.data()+clientDigital,8);
    if(digital&~uint64_t(15)){IdleHeadResult result;result.status=IdleHeadStatus::ActiveInput;return result;}
    auto validated=source;std::fill(validated.begin()+clientDigital,validated.begin()+clientDigital+8,0);
    auto reviewed=gate;reviewed.actorState&=~(1ull<<5); // native BlockOpportunity in melee playback
    ClientInputBytes temporary;auto result=patchWalkingHeadCamera(validated,temporary,reviewed,head,previousYaw);
    if(result.status==IdleHeadStatus::Applied)writeHeadCamera(output,result.proposal);
    return result;
}
// keen::actor::StateFlag FloatingOnWater, UnderWater. The game turns gravity off in the water (run
// game-20261013-185120: gravity present, not active, for all of the swim), so until 2026-10-13 swimming fell under
// "unusual gravity" and went where the native camera pointed, not where the player looks. VHVR steers swimming as
// it steers walking (locomotion_input.md 2.7, 3.1).
inline constexpr uint64_t swimmingStates=(1ull<<43)|(1ull<<44);
// Live VR play (2026-10-09 real-device feedback: jump/dodge launched "diagonally forward-left" while walking straight).
// Ordinary walking used the HMD heading, but any pressed button (ActiveInput) or airborne state (SpecialMode) fell back
// to the native camera heading. Glider physics separately reads body orientation;
// its launch direction is handled by glide_launch.hpp / glide_launch_bridge.hpp.
// Here every gameplay state of a gamepad player with normal gravity takes the head camera, buttons included; UI, player
// UI/target-lock flags, free/special cameras, keyboard play and unusual gravity keep every native byte.
inline IdleHeadResult patchVrLocomotionCamera(const ClientInputBytes& source,ClientInputBytes& output,
        const WalkingHeadGate& gate,const HeadInputSnapshot& head,float previousYaw){
    output=source;IdleHeadResult result;
    const auto& player=gate.player;
    if(!player.normalUiBound){result.status=IdleHeadStatus::Ui;return result;}
    if(!player.playerStateCurrent||!(player.playerFlags&1u)||(player.playerFlags&nativeCameraFlags())){
        result.status=IdleHeadStatus::PlayerState;return result;}
    const bool swimming=gate.swimHead&&gate.actorStateCurrent&&(gate.actorState&swimmingStates);
    if(player.freeCamera||source[clientMask]||source[clientBuildingCamera]||source[clientInputMode]!=1||!gate.actorStateCurrent||!gate.gravityPresent||(!gate.gravityActive&&!swimming)||
       !finite(gate.gravityDirection)||length(gate.gravityDirection-Vec3{0,-1,0})>1e-5f){result.status=IdleHeadStatus::SpecialMode;return result;}
    std::array<float,2> stick;std::memcpy(stick.data(),source.data()+clientStick,sizeof(stick));
    if(!std::isfinite(stick[0])||!std::isfinite(stick[1])||std::abs(stick[0])>1||std::abs(stick[1])>1)return result;
    NativeInputState geometry;geometry.enabled=geometry.known=geometry.local=geometry.ready=true;
    geometry.inputMode=1;geometry.playerFlags=player.playerFlags;
    result.proposal=proposeInput(geometry,head,previousYaw);
    if(!result.proposal.active()){
        result.status=result.proposal.fallback==InputFallback::TrackingUnavailable?IdleHeadStatus::Tracking:IdleHeadStatus::Invalid;return result;}
    result.proposal.worldMovement=rotate(result.proposal.movementOrientation,{stick[0],0,stick[1]});
    writeHeadCamera(output,result.proposal);
    result.status=IdleHeadStatus::Applied;return result;
}
} // namespace evr::game
