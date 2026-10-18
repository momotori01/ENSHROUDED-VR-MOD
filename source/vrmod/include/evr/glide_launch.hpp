#pragma once
#include <evr/math.hpp>
#include <cstdint>
namespace evr::game {
struct GlideLaunchRotation {bool applied=false;Quat orientation{};float yawDelta=0;};
// Only a native glider sequence's first simulation step. Preserve native pitch
// and bank, then let the original physics update orientation/speed normally.
inline GlideLaunchRotation glideLaunchRotation(Quat body,Quat camera,uint64_t actorState,
        uint8_t sequenceFlags,int64_t playedTime,bool inputCurrent){
    GlideLaunchRotation r;r.orientation=body;
    constexpr uint64_t gliding=1ull<<37,ready=1ull<<50,dead=1ull<<7;
    if(!inputCurrent||!(sequenceFlags&1)||playedTime!=0||(actorState&(gliding|ready))!=(gliding|ready)||(actorState&dead))return r;
    const auto forward=rotate(body,{0,0,1}),aim=rotate(camera,{0,0,1});
    if(std::hypot(forward.x,forward.z)<1e-4f||std::hypot(aim.x,aim.z)<1e-4f)return r;
    r.yawDelta=std::remainder(std::atan2(aim.x,aim.z)-std::atan2(forward.x,forward.z),2*pi);
    r.orientation=normalized(multiply(euler(r.yawDelta,0,0),body));r.applied=true;return r;
}
}
