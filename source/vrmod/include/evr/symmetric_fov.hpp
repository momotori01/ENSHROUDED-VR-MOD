#pragma once
#include <evr/math.hpp>
namespace evr {
// The native engine has perspective sphere/HZB paths that assume a centered projection.
// Keep every runtime ray while supplying the engine with a symmetric frustum. The exact
// render FOV must also be submitted to OpenXR; never present these pixels as the old FOV.
// OpenXR XrCompositionLayerProjectionView permits a different FOV and maps it to the display.
inline XrFovf symmetricEnvelope(XrFovf runtime) {
    projection(runtime,.01f,10000.f); // validate finite ordered angles and clip/ray contract
    const float x=std::max(std::abs(runtime.angleLeft),std::abs(runtime.angleRight));
    const float y=std::max(std::abs(runtime.angleUp),std::abs(runtime.angleDown));
    return {-x,x,y,-y};
}
}
