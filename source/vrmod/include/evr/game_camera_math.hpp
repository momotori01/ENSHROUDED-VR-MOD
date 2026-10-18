#pragma once
#include <evr/world_position.hpp>

namespace evr::game
{
// Verified against build 23966345's isolated CPU math. No game pointers or writes.
// Game camera space has +Z forward. OpenXR has -Z forward. This is a basis
// reflection, not a 180-degree yaw (which would also reverse left/right).
inline Vec3 trackingVector(Vec3 xr)
{
    if (!finite(xr))
        throw std::invalid_argument("invalid tracking vector");
    return {xr.x, xr.y, -xr.z};
}
inline Pose trackingPose(Pose xr)
{
    auto q = normalized(xr.orientation);
    return {{-q.x, -q.y, q.z, q.w}, trackingVector(xr.position)};
}

// Four rows of four floats, used with row vectors. CPU affine Matrix43 has
// this 64-byte stride too; its fourth column is padding and must not be read.
struct MatrixRows
{
    std::array<float, 16> values{};
};

// CPU game projection has Y up and reversed [0,1] depth. Any Vulkan viewport
// inversion/jitter is a separate renderer decision; do not apply it here.
inline MatrixRows projection(XrFovf fov, float nearZ, float farZ)
{
    // Reuse the validated FOV contract; produce the game's convention below.
    evr::projection(fov, nearZ, farZ);
    const float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
    const float b = std::tan(fov.angleDown), t = std::tan(fov.angleUp);
    MatrixRows result;
    auto &m = result.values;
    m[0] = 2 / (r - l);
    m[5] = 2 / (t - b);
    m[8] = -(r + l) / (r - l);
    m[9] = -(t + b) / (t - b);
    m[10] = -nearZ / (farZ - nearZ);
    m[11] = 1;
    m[14] = nearZ * farZ / (farZ - nearZ);
    return result;
}

inline MatrixRows cameraRelativeView(WorldPositionQ32 camera, Quat gameOrientation,
                                     WorldPositionQ32 renderOrigin)
{
    const Vec3 local{static_cast<float>(relativeQ32(camera.x, renderOrigin.x)),
                     static_cast<float>(relativeQ32(camera.y, renderOrigin.y)),
                     static_cast<float>(relativeQ32(camera.z, renderOrigin.z))};
    const auto columnView = evr::view(Pose{gameOrientation, local});
    MatrixRows result;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            result.values[row * 4 + col] = columnView.at(col, row);
    return result;
}

// Reflected shader GraphicsMatrix43 packs three float4 rows (48 bytes).
// Transpose the CPU row-vector representation, dropping the affine last row.
inline std::array<float, 12> shaderView(MatrixRows cpuView)
{
    std::array<float, 12> result{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
            result[row * 4 + col] = cpuView.values[col * 4 + row];
    return result;
}
} // namespace evr::game
