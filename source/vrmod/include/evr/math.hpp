#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <openxr/openxr.h>
#include <stdexcept>
namespace evr
{
constexpr float pi = 3.14159265358979323846f;
using Vec3 = XrVector3f;
using Quat = XrQuaternionf;
using Pose = XrPosef;
inline Vec3 operator+(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 a, float s)
{
    return {a.x * s, a.y * s, a.z * s};
}
inline float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a)
{
    return std::sqrt(dot(a, a));
}
inline bool finite(Vec3 a)
{
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
inline Quat normalized(Quat q)
{
    float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!std::isfinite(n) || n < 1e-8f)
        throw std::invalid_argument("invalid quaternion");
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}
inline Quat multiply(Quat a, Quat b)
{
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat inverse(Quat q)
{
    q = normalized(q);
    return {-q.x, -q.y, -q.z, q.w};
}
inline Vec3 rotate(Quat q, Vec3 v)
{
    q = normalized(q);
    Vec3 u{q.x, q.y, q.z};
    return v + cross(u, v) * (2 * q.w) + cross(u, cross(u, v)) * 2;
}
inline Quat euler(float yaw, float pitch, float roll)
{
    return multiply(
        multiply({0, std::sin(yaw / 2), 0, std::cos(yaw / 2)}, {std::sin(pitch / 2), 0, 0, std::cos(pitch / 2)}),
        {0, 0, std::sin(roll / 2), std::cos(roll / 2)});
}
inline Pose identityPose()
{
    return {{0, 0, 0, 1}, {0, 0, 0}};
}
inline Pose compose(Pose a, Pose b)
{
    return {normalized(multiply(a.orientation, b.orientation)), a.position + rotate(a.orientation, b.position)};
}
inline Pose inverse(Pose p)
{
    auto q = inverse(p.orientation);
    return {q, rotate(q, p.position * -1)};
}
struct Mat4
{
    std::array<float, 16> m{};
    float &at(int r, int c)
    {
        return m[c * 4 + r];
    }
    float at(int r, int c) const
    {
        return m[c * 4 + r];
    }
};
inline Mat4 identity()
{
    Mat4 a;
    for (int i = 0; i < 4; i++)
        a.at(i, i) = 1;
    return a;
}
inline Mat4 multiply(Mat4 a, Mat4 b)
{
    Mat4 c;
    for (int r = 0; r < 4; r++)
        for (int col = 0; col < 4; col++)
            for (int k = 0; k < 4; k++)
                c.at(r, col) += a.at(r, k) * b.at(k, col);
    return c;
}
inline Vec3 point(Mat4 m, Vec3 p)
{
    float w = m.at(3, 0) * p.x + m.at(3, 1) * p.y + m.at(3, 2) * p.z + m.at(3, 3);
    return {(m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3)) / w,
            (m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3)) / w,
            (m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3)) / w};
}
inline Mat4 transform(Pose p)
{
    auto a = identity();
    const Vec3 axes[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int c = 0; c < 3; c++)
    {
        auto v = rotate(p.orientation, axes[c]);
        a.at(0, c) = v.x;
        a.at(1, c) = v.y;
        a.at(2, c) = v.z;
    }
    a.at(0, 3) = p.position.x;
    a.at(1, 3) = p.position.y;
    a.at(2, 3) = p.position.z;
    return a;
}
inline Mat4 view(Pose p)
{
    return transform(inverse(p));
}
// OpenXR right handed, -Z forward. Vulkan NDC Y down, depth [0,1].
inline Mat4 projection(XrFovf f, float nearZ, float farZ)
{
    if (!std::isfinite(nearZ) || !std::isfinite(farZ) || !(nearZ > 0 && farZ > nearZ))
        throw std::invalid_argument("invalid clip planes");
    for (float a : {f.angleLeft, f.angleRight, f.angleUp, f.angleDown})
        if (!std::isfinite(a) || std::abs(a) >= pi / 2)
            throw std::invalid_argument("invalid FOV");
    float l = std::tan(f.angleLeft), r = std::tan(f.angleRight), u = std::tan(f.angleUp), d = std::tan(f.angleDown);
    if (!(r > l && u > d))
        throw std::invalid_argument("unordered FOV");
    Mat4 p;
    p.at(0, 0) = 2 / (r - l);
    p.at(0, 2) = (r + l) / (r - l);
    p.at(1, 1) = -2 / (u - d);
    p.at(1, 2) = -(u + d) / (u - d);
    p.at(2, 2) = -farZ / (farZ - nearZ);
    p.at(2, 3) = -farZ * nearZ / (farZ - nearZ);
    p.at(3, 2) = -1;
    return p;
}
// At vertical pitch yaw is unobservable from forward. Retain last valid heading.
inline float heading(Quat q, float previous)
{
    auto f = rotate(q, {0, 0, -1});
    return f.x * f.x + f.z * f.z > 1e-10f ? std::atan2(-f.x, -f.z) : previous;
}
inline Vec3 movement(float right, float forward, float yaw)
{
    return rotate(euler(yaw, 0, 0), {right, 0, -forward});
}
struct TrackingOrigin
{
    Pose trackingToAnchor = identityPose();
    float lastYaw = 0;
    void recenter(Pose head)
    {
        lastYaw = heading(head.orientation, lastYaw);
        auto q = euler(-lastYaw, 0, 0);
        trackingToAnchor = {q, rotate(q, head.position) * -1};
    }
    Pose camera(Vec3 stableRoot, float calibratedEyeHeight, float controllerYaw, Pose eye) const
    {
        if (!finite(stableRoot) || !finite(eye.position))
            throw std::invalid_argument("invalid position");
        Pose anchor{euler(controllerYaw, 0, 0), stableRoot + Vec3{0, calibratedEyeHeight, 0}};
        return compose(compose(anchor, trackingToAnchor), eye);
    }
};
enum class GameMode
{
    Combat,
    Gameplay,
    UI,
    Inventory,
    ItemInspection,
    Building,
    Special,
    Unknown
};
inline bool overrideAim(GameMode m, bool tracked)
{
    return tracked && (m == GameMode::Combat || m == GameMode::Gameplay);
}
inline bool overrideMovement(GameMode m, bool tracked)
{
    return tracked && (m == GameMode::Combat || m == GameMode::Gameplay);
}
enum class MeshPart
{
    Body,
    Arms,
    Legs,
    Head,
    Face,
    Eyes,
    Hair,
    Beard,
    HeadEquipment,
    Unknown
};
inline bool visibleInPass(bool localPlayer, bool vrEye, bool shadow, MeshPart p)
{
    return !(localPlayer && vrEye && !shadow &&
             (p == MeshPart::Head || p == MeshPart::Face || p == MeshPart::Eyes || p == MeshPart::Hair ||
              p == MeshPart::Beard || p == MeshPart::HeadEquipment));
}
inline std::array<uint32_t, 2> scaledExtent(uint32_t w, uint32_t h, float scale, uint32_t maxW, uint32_t maxH)
{
    if (!w || !h || !maxW || !maxH || !std::isfinite(scale) || scale <= 0)
        throw std::invalid_argument("invalid render scale/extent");
    double sw = std::round(double(w) * scale), sh = std::round(double(h) * scale);
    if (sw < 1 || sh < 1 || sw > maxW || sh > maxH)
        throw std::invalid_argument("scaled extent exceeds runtime limits");
    return {uint32_t(sw), uint32_t(sh)};
}
} // namespace evr
