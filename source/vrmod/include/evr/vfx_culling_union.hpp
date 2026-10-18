#pragma once
// VFX culling frustum for the eye that owns VFX on desktop-skip frames (2026-10-06).
// The owner eye's VFX stages cull lights and particles against its own frustum, so anything visible only
// at the outer edge of the other eye would be missing there. This computes, in the owner eye's view space,
// the tangent bounds that contain both eyes' frusta from `minDistance` to `farDistance` (parallax included),
// and widens the owner's game projection descriptor to them. Only the VFX culling planes use the result;
// the owner eye's own rendering keeps its projection.
#include <evr/game_stereo.hpp>
#include <algorithm>
#include <cmath>

namespace evr::game
{
struct TangentBounds
{
    double left, right, down, up; // tangents in the owner eye's view space (x right, y up, -z forward)
};
inline TangentBounds tangents(const XrFovf &f)
{
    return {std::tan(f.angleLeft), std::tan(f.angleRight), std::tan(f.angleDown), std::tan(f.angleUp)};
}
// Union of the owner's frustum and the other eye's frustum as seen from the owner eye. Corner rays of the other
// eye are sampled at minDistance (largest parallax) and farDistance. Throws if a sampled point is not in front of
// the owner eye or the result is not finite; the caller then keeps the native planes.
inline TangentBounds unionTangents(const XrView &owner, const XrView &other, float minDistance, float farDistance)
{
    if (!(minDistance > 0) || !(farDistance > minDistance))
        throw std::invalid_argument("invalid VFX culling union distances");
    auto bounds = tangents(owner.fov);
    const auto o = tangents(other.fov);
    const auto toOwner = inverse(owner.pose);
    for (double tx : {o.left, o.right})
        for (double ty : {o.down, o.up})
            for (float distance : {minDistance, farDistance})
            {
                const Vec3 local{float(tx * distance), float(ty * distance), -distance};
                const Vec3 world = compose(other.pose, Pose{{0, 0, 0, 1}, local}).position;
                const Vec3 p = compose(toOwner, Pose{{0, 0, 0, 1}, world}).position;
                if (!finite(p) || -p.z < 1e-4f)
                    throw std::invalid_argument("other eye corner behind the owner eye");
                const double x = p.x / -double(p.z), y = p.y / -double(p.z);
                bounds.left = std::min(bounds.left, x);
                bounds.right = std::max(bounds.right, x);
                bounds.down = std::min(bounds.down, y);
                bounds.up = std::max(bounds.up, y);
            }
    for (double v : {bounds.left, bounds.right, bounds.down, bounds.up})
        if (!std::isfinite(v))
            throw std::invalid_argument("nonfinite VFX culling union");
    return bounds;
}
// The owner's tag-3 descriptor with its near-plane bounds widened to `bounds` (never narrowed).
inline ProjectionDescriptor widenedDescriptor(const ProjectionDescriptor &owner, const TangentBounds &bounds)
{
    uint32_t tag = 0;
    std::memcpy(&tag, owner.bytes.data(), 4);
    std::array<float, 6> planes{};
    std::memcpy(planes.data(), owner.bytes.data() + 4, sizeof(planes));
    const float n = planes[4];
    if (tag != 3 || owner.bytes[0x48] != 1 || !(n > 0) || planes[0] >= planes[1] || planes[2] >= planes[3])
        throw std::invalid_argument("unsupported owner projection descriptor");
    planes[0] = std::min(planes[0], float(n * bounds.left));
    planes[1] = std::max(planes[1], float(n * bounds.right));
    planes[2] = std::min(planes[2], float(n * bounds.down));
    planes[3] = std::max(planes[3], float(n * bounds.up));
    for (float v : planes)
        if (!std::isfinite(v))
            throw std::invalid_argument("nonfinite widened descriptor");
    auto result = owner;
    std::memcpy(result.bytes.data() + 4, planes.data(), sizeof(planes));
    return result;
}
} // namespace evr::game
