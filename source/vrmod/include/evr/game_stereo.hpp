#pragma once
#include <cstddef>
#include <cstring>
#include <evr/game_camera_math.hpp>

namespace evr::game
{
// Owned value types only. No game allocation, ABI calls, or writable player pointers.
// The descriptor layout is specific to the documented build and must be gated at
// the eventual integration boundary. Near-plane bounds feed engine frustum math.
struct ProjectionDescriptor
{
    std::array<uint8_t, 80> bytes{};
};
inline ProjectionDescriptor projectionDescriptor(XrFovf fov, float nearZ, float farZ)
{
    projection(fov, nearZ, farZ); // finite, ordering, clip-plane validation
    ProjectionDescriptor result;
    const uint32_t tag = 3;
    const std::array<float, 6> planes{nearZ * std::tan(fov.angleLeft), nearZ * std::tan(fov.angleRight),
                                      nearZ * std::tan(fov.angleDown), nearZ * std::tan(fov.angleUp), nearZ, farZ};
    for (float value : planes)
        if (!std::isfinite(value))
            throw std::invalid_argument("game frustum plane overflow");
    std::memcpy(result.bytes.data(), &tag, sizeof(tag));
    std::memcpy(result.bytes.data() + 4, planes.data(), sizeof(planes));
    result.bytes[0x48] = 1; // reverse depth; screen rotation +0x44 remains zero
    return result;
}

struct StableAnchor
{
    WorldPositionQ32 root;
    Quat orientation = {0, 0, 0, 1}; // stable game-space facing, not animated head rotation
    Vec3 cameraOffsetGameUnits = {0, 0, 0}; // calibrated offset from root, no animation motion
    double worldUnitsPerMeter = 1;
};
// Draw-only replay reference, expressed directly in the original XR tracking space.
// Cancels a session's recenter without changing any tracking/input or game entity state.
inline StableAnchor replayAnchor(const StableAnchor& reference,Pose trackingToAnchor)
{
    if(reference.worldUnitsPerMeter!=1||length(reference.cameraOffsetGameUnits)!=0)
        throw std::invalid_argument("replay reference must have unit scale and no extra camera offset");
    const auto local=trackingPose(trackingToAnchor);
    StableAnchor result;
    result.orientation=normalized(multiply(normalized(reference.orientation),inverse(local.orientation)));
    result.root=cameraPosition(reference.root,rotate(result.orientation,local.position*-1.f),1);
    return result;
}
struct EyeCamera
{
    WorldPositionQ32 position;
    Quat orientation;
    XrFovf fov;
    MatrixRows view, projection;
    ProjectionDescriptor descriptor;
};
struct StereoCameras
{
    uint64_t xrFrame = 0;
    XrTime predictedDisplayTime = 0;
    std::array<EyeCamera, 2> eyes;
};

inline StereoCameras stereoCameras(const StableAnchor &anchor, Pose trackingToAnchor,
                                   const std::array<XrView, 2> &views, XrViewStateFlags flags,
                                   XrTime predictedDisplayTime, uint64_t xrFrame,
                                   WorldPositionQ32 renderOrigin, float nearZ, float farZ)
{
    constexpr XrViewStateFlags valid = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    if ((flags & valid) != valid || predictedDisplayTime <= 0 || !xrFrame)
        throw std::invalid_argument("invalid stereo tracking/frame snapshot");
    if (!finite(trackingToAnchor.position))
        throw std::invalid_argument("invalid tracking origin");
    const auto orientation = normalized(anchor.orientation);
    const auto rootCamera = cameraPosition(anchor.root, rotate(orientation, anchor.cameraOffsetGameUnits), 1);
    StereoCameras result;
    result.xrFrame = xrFrame;
    result.predictedDisplayTime = predictedDisplayTime;
    for (size_t i = 0; i < result.eyes.size(); ++i)
    {
        // xrLocateViews already returns each complete eye pose, including HMD
        // translation/rotation and IPD. Do not multiply by the HMD pose again.
        const auto local = trackingPose(compose(trackingToAnchor, views[i].pose));
        auto &eye = result.eyes[i];
        eye.position = cameraPosition(rootCamera, rotate(orientation, local.position), anchor.worldUnitsPerMeter);
        eye.orientation = normalized(multiply(orientation, local.orientation));
        eye.fov = views[i].fov;
        eye.view = cameraRelativeView(eye.position, eye.orientation, renderOrigin);
        eye.projection = projection(eye.fov, nearZ, farZ);
        eye.descriptor = projectionDescriptor(eye.fov, nearZ, farZ);
    }
    return result; // constructed atomically; no partially published left eye on failure
}
} // namespace evr::game
