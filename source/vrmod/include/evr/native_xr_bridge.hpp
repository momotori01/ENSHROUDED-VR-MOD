#pragma once
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <array>
#include <evr/motion_actions.hpp>
#include <cstdint>
namespace evr::diagnostic {
inline constexpr uint32_t maximumNativeDiagnosticFrames=3000;
inline constexpr uint64_t nativeXrFrameAbiVersion=5; // v2 face action states; v3 grip value and thumb touch (finger curl); v4 eyeMask; v5 Index trackpad force.
// A valid session can temporarily have no renderable pose. This result means
// its empty XR frame has already ended; continue the ordinary game frame.
inline constexpr int nativeXrFrameDeferred=3;
struct NativeXrFrame {
    uint32_t structSize = sizeof(NativeXrFrame);
    uint32_t diagnosticFrames = 1;
    std::array<uint32_t,2> width{},height{};
    uint64_t frame = 0;
    XrTime time = 0;
    XrPosef head{{0,0,0,1},{0,0,0}};
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
    XrSpaceLocationFlags headFlags = 0;
    uint64_t sessionGeneration = 0;
    motion::HandFrame hands;
    // In: the eyes the application wants to draw in this frame (1 left, 2 right, 3 both). Out: the eyes it must draw;
    // an eye left out keeps its last drawn picture in the headset (alternate-eye rendering, openxr_client.hpp eyeMask).
    uint32_t eyeMask = 3;
    uint64_t abiVersion = nativeXrFrameAbiVersion; // Explicit tail changes size even when hand padding does not.
};
using BeginNativeXr = int (*)(VkInstance,VkPhysicalDevice,VkDevice,uint32_t,float,NativeXrFrame*);
using FinishNativeXr = int (*)(uint64_t,const VkImage*);
using CloseNativeXr = int (*)();
using NativeGestureHaptic = int (*)(uint64_t,uint64_t,const float*,const XrDuration*);
using NativeMotionHaptic = int (*)(uint64_t,uint64_t,const float*);
using SkipNativeXr = int (*)(uint64_t);
}
