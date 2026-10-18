#pragma once
#include <evr/motion_intent.hpp>
#include <evr/world_position.hpp>
#include <cstring>
namespace evr::motion {
// Value-only prototype, not a firing authority. The caller must establish current
// local actor/item/world/tracking ownership before using this in a live route.
struct BowWorldAim {WorldPositionQ32 origin{};Quat orientation{0,0,0,1};Vec3 direction{};};
// minimumForward: the one-shot diagnostic refuses a line more than 60 degrees off the
// model. The VHVR-style draw turns the bow onto the line itself and passes -1.
inline BowWorldAim knownBowWorldAim(WorldPositionQ32 modelPosition,Quat modelOrientation,Vec3 scale,Vec3 nockTrackingModel,float minimumForward=.5f){
 if(!finite(scale)||scale.x<.5f||scale.y<.5f||scale.z<.5f||scale.x>1.5f||scale.y>1.5f||scale.z>1.5f||!finite(nockTrackingModel))throw std::invalid_argument("bow aim geometry");
 modelOrientation=normalized(modelOrientation);
 // Provisional arrow rest at the measured string's rest elevation, on the grip
 // plane. Actual arrow model/fingertip calibration is a separate hardware task.
 const Vec3 rest{0,.00155006f*scale.y,0};
 const Vec3 nock{nockTrackingModel.x*scale.x,nockTrackingModel.y*scale.y,-nockTrackingModel.z*scale.z};
 const auto delta=rest-nock;const float span=length(delta);
 if(!std::isfinite(span)||span<.10f||span>1.5f||delta.z/span<minimumForward)throw std::invalid_argument("bow aim draw direction");
 const auto direction=rotate(modelOrientation,delta*(1/span));
 const auto q=normalized(multiply(alignDirections(rotate(modelOrientation,{0,0,1}),direction),modelOrientation));
 return {cameraPosition(modelPosition,rotate(modelOrientation,rest),1),q,direction};
}
using BowSpawnEvent=std::array<uint8_t,336>;
using BowSpawnCamera=std::array<uint8_t,44>;
// projectileFlag: the bow and wand spawn event carries isProjectile at +0x148. The
// staff cast reaches the same placement with a shorter event; it passes false.
inline BowSpawnCamera bowAimCamera(const BowWorldAim& aim,const BowSpawnEvent& event,bool projectileFlag=true){
 if((projectileFlag&&event[0x148]!=1)||(event[0x90]!=2&&event[0x90]!=3))throw std::invalid_argument("not a Crosshair projectile event");
 Vec3 local{},world{};Quat offset{};
 std::memcpy(&local,event.data()+0xc0,12);std::memcpy(&world,event.data()+0xcc,12);std::memcpy(&offset,event.data()+0xd8,16);
 const float norm=offset.x*offset.x+offset.y*offset.y+offset.z*offset.z+offset.w*offset.w;
 if(!finite(local)||!finite(world)||length(local)>5||length(world)>5||!std::isfinite(norm)||std::abs(norm-1)>.002f)throw std::invalid_argument("bow event offset bounds");
 // Compensate the fixed native muzzle transform in a private camera value.
 // Native event offsets and random spread are retained, never zeroed or edited.
 const auto q=normalized(multiply(aim.orientation,inverse(offset)));
 const auto p=cameraPosition(aim.origin,(rotate(q,local)+world)*-1.f,1);
 BowSpawnCamera out{};std::memcpy(out.data(),&p,24);std::memcpy(out.data()+24,&q,16);return out; // distance=0
}
// The same camera without the spawn event, for a player whose arrows the server of another PC makes (2026-10-19,
// online: the friend's arrows "went up, not where aimed" - that player's mod never sees the spawn, the host's server
// used the camera that player's game sent). The muzzle of the Crosshair projectile event is fixed: from 39 shots of
// three headset sessions (bow_tracked_aim_call against bow_tracked_shot_armed) local (0, -0.1999, 0.007), world 0, and
// the offset a turn of 2 degrees about x (-0.017452, 0, 0, 0.999848), the same for every shot to the float's precision.
inline BowSpawnCamera bowAimCameraKnownMuzzle(const BowWorldAim& aim){
 const Quat offset{-.0174524f,0,0,.9998477f};const Vec3 local{0,-.1999f,.007f};
 const auto q=normalized(multiply(aim.orientation,inverse(offset)));
 const auto p=cameraPosition(aim.origin,rotate(q,local)*-1.f,1);
 BowSpawnCamera out{};std::memcpy(out.data(),&p,24);std::memcpy(out.data()+24,&q,16);return out; // distance=0
}
}
