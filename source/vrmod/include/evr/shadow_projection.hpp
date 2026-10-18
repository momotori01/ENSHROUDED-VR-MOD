#pragma once
#include <evr/game_stereo.hpp>

namespace evr::game {
// Native 13b630 reads perspective aspect/near/far/FOV directly, without checking
// the descriptor tag. Convert only its input; the eye projection stays untouched.
inline ProjectionDescriptor shadowPerspectiveDescriptor(const ProjectionDescriptor& eye){
    uint32_t tag=0,rotation=0;std::memcpy(&tag,eye.bytes.data(),4);
    std::memcpy(&rotation,eye.bytes.data()+0x44,4);
    std::array<float,6> b{};std::memcpy(b.data(),eye.bytes.data()+4,24);
    for(float x:b)if(!std::isfinite(x))throw std::invalid_argument("nonfinite shadow projection");
    if(tag!=3||rotation||eye.bytes[0x48]!=1||!(b[0]<0&&b[1]>0&&b[2]<0&&b[3]>0&&b[4]>0&&b[5]>b[4]))
        throw std::invalid_argument("unsupported shadow eye descriptor");
    const float x=std::max(-b[0],b[1]),y=std::max(-b[2],b[3]);
    const std::array<float,4> perspective{x/y,b[4],b[5],2*std::atan(y/b[4])};
    for(float v:perspective)if(!std::isfinite(v)||v<=0)throw std::invalid_argument("shadow perspective overflow");
    if(perspective[3]>=pi)throw std::invalid_argument("shadow perspective angular overflow");
    ProjectionDescriptor result; // tag 0: aspect, near, far, vertical FOV
    std::memcpy(result.bytes.data()+4,perspective.data(),sizeof(perspective));
    result.bytes[0x48]=1;
    return result;
}
}
