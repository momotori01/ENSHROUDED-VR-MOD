#pragma once
#include <evr/game_input.hpp>
#include <cstring>

namespace evr::game {
// ClientPlayerInput value copy. Bounded idle-only integration
// diagnostic: does not synthesize buttons, movement, UI or a network format.
// 1.0 (build 25936191, 2026-10-16) from the game's reflection (ClientPlayerInputData / ClientPlayerInput): 0x600 bytes
// (0x570); the camera +0x20 and the action bar +0x60 as before; the equipment change +0x1c0 (0x17c), the movement stick
// +0x2b8 (0x254), the mask +0x3a0 (0x310), the digital input +0x3a8 (0x318), the input mode +0x5a1 (0x511), the
// client-only input +0x5a4 (0x514) with its building-camera byte at +6 (+0x5aa, 0x51a).
constexpr size_t clientInputBytes=0x600,clientEquipAction=0x1c0,clientStick=0x2b8,clientMask=0x3a0,clientDigital=0x3a8,
    clientInputMode=0x5a1,clientBuildingCamera=0x5aa;
// The ECS PlayerInput keeps the client's data from +8 (fromClient: the camera +0x28 / +0x40); 1.0: the stick +0x2c0
// (0x25c), the mask +0x3a8 (0x318), the digital input +0x3b0 (0x320), fromClientInputMode +0x5a9 (0x519),
// fromClientSettings +0x5ac (0x51a), fromClientIsReadyForGameplay +0x5b8 (0x520).
constexpr size_t playerInputStick=0x2c0,playerInputMask=0x3a8,playerInputDigital=0x3b0,playerInputMode=0x5a9,playerInputSettings=0x5ac,playerInputReady=0x5b8;
// ActorInput: desiredWorldLookDirection +0x1b8 (0x1a4); the local and world move inputs +0x18c / +0x198 as before.
constexpr size_t actorInputLook=0x1b8;
using ClientInputBytes=std::array<uint8_t,clientInputBytes>;
struct IdleHeadGate {
    bool normalUiBound=false,playerStateCurrent=false,freeCamera=false;
    uint32_t playerFlags=0;
};
enum class IdleHeadStatus {Applied,Ui,PlayerState,SpecialMode,ActiveInput,Tracking,Invalid};
inline const char* idleHeadStatusName(IdleHeadStatus s){
    switch(s){case IdleHeadStatus::Applied:return "applied";case IdleHeadStatus::Ui:return "ui";
    case IdleHeadStatus::PlayerState:return "player_state";case IdleHeadStatus::SpecialMode:return "special_mode";
    case IdleHeadStatus::ActiveInput:return "active_input";case IdleHeadStatus::Tracking:return "tracking";
    case IdleHeadStatus::Invalid:return "invalid";}return "invalid";
}
struct IdleHeadResult {IdleHeadStatus status=IdleHeadStatus::Invalid;InputProposal proposal;};
inline void writeHeadCamera(ClientInputBytes& output,const InputProposal& proposal){
    std::memcpy(output.data()+0x20,&proposal.cameraPosition,sizeof(WorldPositionQ32));
    std::memcpy(output.data()+0x38,&proposal.aimWithoutRoll,sizeof(Quat));
    const float firstPersonDistance=0;
    std::memcpy(output.data()+0x48,&firstPersonDistance,sizeof(firstPersonDistance));
}
inline IdleHeadResult patchIdleHeadCamera(const ClientInputBytes& source,ClientInputBytes& output,
        const IdleHeadGate& gate,const HeadInputSnapshot& head,float previousYaw){
    output=source;IdleHeadResult r;
    if(!gate.normalUiBound){r.status=IdleHeadStatus::Ui;return r;}
    if(!gate.playerStateCurrent||!(gate.playerFlags&1u)||(gate.playerFlags&nativeCameraFlags())){
        r.status=IdleHeadStatus::PlayerState;return r;}
    if(gate.freeCamera||source[clientMask]||source[clientBuildingCamera]||source[clientInputMode]>1){r.status=IdleHeadStatus::SpecialMode;return r;}
    std::array<float,2> stick;uint64_t digital;
    std::memcpy(stick.data(),source.data()+clientStick,sizeof(stick));std::memcpy(&digital,source.data()+clientDigital,sizeof(digital));
    if(!std::isfinite(stick[0])||!std::isfinite(stick[1]))return r;
    if(stick[0]!=0||stick[1]!=0||digital){r.status=IdleHeadStatus::ActiveInput;return r;}
    // This diagnostic intentionally accepts either native input mode while
    // idle. It never changes the native mode byte or claims gamepad testing.
    NativeInputState geometry;geometry.enabled=geometry.known=geometry.local=geometry.ready=true;
    geometry.inputMode=1;geometry.playerFlags=gate.playerFlags;
    r.proposal=proposeInput(geometry,head,previousYaw);
    if(!r.proposal.active()){
        r.status=r.proposal.fallback==InputFallback::TrackingUnavailable?IdleHeadStatus::Tracking:IdleHeadStatus::Invalid;return r;}
    // Keep original FOV/aspect/clips/padding and every action. The shared ECS
    // component is never passed as output. Roll remains in rendering only.
    writeHeadCamera(output,r.proposal);
    r.status=IdleHeadStatus::Applied;return r;
}
inline bool onlyHeadCameraChanged(const ClientInputBytes& a,const ClientInputBytes& b){
    for(size_t i=0;i<a.size();++i)if((i<0x20||i>=0x4c)&&a[i]!=b[i])return false;
    return true;
}
// The camera and the movement stick (+254, two floats): the live guard turns the camera to the guard's direction and
// the stick back by the same angle, so that walking keeps the head's direction (input_head_idle.hpp, 2026-10-14).
inline bool onlyHeadCameraAndStickChanged(const ClientInputBytes& a,const ClientInputBytes& b){
    for(size_t i=0;i<a.size();++i)if((i<0x20||i>=0x4c)&&(i<clientStick||i>=clientStick+8)&&a[i]!=b[i])return false;
    return true;
}
} // namespace evr::game
