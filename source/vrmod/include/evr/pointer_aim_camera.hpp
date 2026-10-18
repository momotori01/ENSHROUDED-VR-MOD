#pragma once
#include <evr/walking_head_camera.hpp>

namespace evr::game {
// Placing by hand, after VHVR (docs/analysis/vhvr_spec_20261010/equipment_tools.md 5.2, PlaceModeRayVectorProvider):
// the ray of the main hand's pointer takes the place of the camera's for the ray that finds where a piece goes.
//
// The game finds that place on the client, in the task client_cursor (249030; docs/analysis/building_cursor_20261012).
// The task builds a context on its own stack. The camera in it - a position, an orientation, and a distance by which
// the ray's origin is pushed forward (the boom of the game's camera) - is copied from the player's input component
// and from LocalPlayerCamera. 242300 and 2455e0 make the ray from it (origin = position + forward * distance, 31 m
// long) and 24d850 turns the piece by the same orientation. Here the pointer of the right hand becomes that camera,
// in the context only: the component, the camera and the input that is sent keep their values.
// (A first form wrote the pointer as the camera of the sent input. The preview did not follow it, Mock runs
// game101225 / game101645: the task reads the component, not what is sent.)
//
// The block of the context that holds the camera, +0x590 .. +0x5d0:
//   +0x00 position (3 x Q32)  +0x18 orientation (x y z w)  +0x28 scale (1 1 1)  +0x38 distance  +0x3c a byte of the input
// 1.0 (Hotfix 1.0.0.1, 0x287cd0 reads the orientation at +0x658, June 0x242300 at +0x5a8): the block is +0x640.
constexpr size_t cursorContextCamera=0x640;
using CursorCameraBytes=std::array<uint8_t,0x40>;
struct CursorRayResult {
    bool applied=false;const char* reason="";
    InputProposal pointer;            // what is written: position and orientation without roll, as with the head
    WorldPositionQ32 nativePosition{};Quat nativeOrientation{0,0,0,1};float nativeDistance=0;
};
// The pointer through the head's conversion: the same tracking space, anchor and scale.
inline InputProposal proposePointer(const HeadInputSnapshot& head,float previousYaw){
    NativeInputState geometry;geometry.enabled=geometry.known=geometry.local=geometry.ready=true;
    geometry.inputMode=1;geometry.playerFlags=1;
    auto hand=head;hand.head=head.pointer;hand.flags=head.pointerFlags;
    return proposeInput(geometry,hand,previousYaw);
}
// Whose ray: the right hand's pointer (what is placed: a building piece, the watering can, building mode), or the
// headset's (a tool that is swung - pickaxe, rake: it aims with the view, as in the game, and the view is the
// headset's; the pointer of a swinging hand would throw the aim about. Mock run game115643: with the pointer the aim
// circle of the pickaxe left the picture as soon as the hand pointed aside).
enum class CursorRaySource {Pointer,Head};
inline CursorRayResult pointCursorRay(const CursorCameraBytes& source,CursorCameraBytes& output,const HeadInputSnapshot& head,float previousYaw,
        CursorRaySource from=CursorRaySource::Pointer){
    output=source;CursorRayResult r;std::array<float,3> scale;
    std::memcpy(&r.nativePosition,source.data(),sizeof(WorldPositionQ32));std::memcpy(&r.nativeOrientation,source.data()+0x18,sizeof(Quat));
    std::memcpy(scale.data(),source.data()+0x28,sizeof(scale));std::memcpy(&r.nativeDistance,source.data()+0x38,sizeof(float));
    // The block is taken for the camera only while it looks like one: unit scale, a unit quaternion, a short distance.
    const auto& q=r.nativeOrientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(scale!=std::array<float,3>{1,1,1}||!std::isfinite(norm)||std::abs(norm-1)>1e-3f||
       !std::isfinite(r.nativeDistance)||r.nativeDistance<0||r.nativeDistance>64){r.reason="layout";return r;}
    if(from==CursorRaySource::Head){NativeInputState geometry;geometry.enabled=geometry.known=geometry.local=geometry.ready=true;
        geometry.inputMode=1;geometry.playerFlags=1;r.pointer=proposeInput(geometry,head,previousYaw);}
    else r.pointer=proposePointer(head,previousYaw);
    if(!r.pointer.active()){r.reason=r.pointer.fallback==InputFallback::TrackingUnavailable?"tracking":"invalid";return r;}
    std::memcpy(output.data(),&r.pointer.cameraPosition,sizeof(WorldPositionQ32));
    std::memcpy(output.data()+0x18,&r.pointer.aimWithoutRoll,sizeof(Quat));
    const float atTheHand=0;std::memcpy(output.data()+0x38,&atTheHand,sizeof(float));   // the ray starts at the pointer itself
    r.applied=true;r.reason="applied";return r;
}
// Only position, orientation and distance differ.
inline bool onlyCursorRayChanged(const CursorCameraBytes& source,const CursorCameraBytes& output){
    for(size_t i=0;i<source.size();++i)if(!(i<0x28||(i>=0x38&&i<0x3c))&&source[i]!=output[i])return false;
    return true;
}

// Building mode keeps the view and the walk of live play: the head camera goes into the sent input as on the HUD
// (patchVrLocomotionCamera). Its screen is not the HUD and sets the client mask to 3 (Mock runs game070726 /
// game100247), so the head camera was not applied there; the mask is accepted only together with that bound screen.
struct PointerAimGate {
    WalkingHeadGate walking;
    bool buildingScreenBound=false;   // building mode (BlueprintMultiTool: screen 28 until June, 29 in 1.0) bound to this player
};
constexpr uint8_t buildingClientMask=3;
inline IdleHeadResult patchBuildingHeadCamera(const ClientInputBytes& source,ClientInputBytes& output,const PointerAimGate& gate,
        const HeadInputSnapshot& head,float previousYaw){
    output=source;IdleHeadResult result;
    if(!gate.buildingScreenBound){result.status=IdleHeadStatus::Ui;return result;}
    if(source[clientMask]&&source[clientMask]!=buildingClientMask){result.status=IdleHeadStatus::SpecialMode;return result;}
    auto hud=gate.walking;hud.player.normalUiBound=true;
    auto plain=source;plain[clientMask]=0;ClientInputBytes scratch;
    result=patchVrLocomotionCamera(plain,scratch,hud,head,previousYaw);
    if(result.status==IdleHeadStatus::Applied)writeHeadCamera(output,result.proposal);
    return result;
}
} // namespace evr::game
