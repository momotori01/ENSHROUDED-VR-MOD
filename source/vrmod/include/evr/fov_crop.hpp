#pragma once
// Field-of-view crop from the runtime visibility mask (XR_KHR_visibility_mask), 2026-10-06.
// The runtime field of view is a rectangle in tangent space; the lens only shows the area inside the
// visibility mask. Rendering the mask's bounding box (plus an angular margin for reprojection) instead
// of the whole rectangle keeps the pixel density and drops pixels nobody can see.
// Mask vertices are tangents on the z = -1 plane (x right, y up), the same space as XrFovf tangents.
#include <openxr/openxr.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace evr::fovcrop
{
struct TanRect
{
    float left = 0, right = 0, up = 0, down = 0; // tangents: left/down negative, right/up positive
    float width() const { return right - left; }
    float height() const { return up - down; }
    bool valid() const { return std::isfinite(left) && std::isfinite(right) && std::isfinite(up) && std::isfinite(down) && right > left && up > down; }
};
inline TanRect fromFov(const XrFovf &f)
{
    return {std::tan(f.angleLeft), std::tan(f.angleRight), std::tan(f.angleUp), std::tan(f.angleDown)};
}
inline XrFovf toFov(const TanRect &r)
{
    return {std::atan(r.left), std::atan(r.right), std::atan(r.up), std::atan(r.down)};
}
inline bool insideTriangle(float x, float y, const XrVector2f &a, const XrVector2f &b, const XrVector2f &c)
{
    const auto side = [&](const XrVector2f &p, const XrVector2f &q) { return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x); };
    const float d1 = side(a, b), d2 = side(b, c), d3 = side(c, a);
    return !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));
}
// Bounding box of the visible area inside `fov`, or an invalid rect when the mask gives nothing usable.
// Visible meshes and line loops: the bounding box of their vertices. Hidden meshes: sampled on a grid
// (a point is visible unless a hidden triangle covers it).
inline TanRect visibleBounds(XrVisibilityMaskTypeKHR type, const std::vector<XrVector2f> &vertices,
                             const std::vector<uint32_t> &indices, const TanRect &fov, int grid = 256)
{
    TanRect box{INFINITY, -INFINITY, -INFINITY, INFINITY};
    const auto add = [&](float x, float y) {
        box.left = std::min(box.left, x); box.right = std::max(box.right, x);
        box.down = std::min(box.down, y); box.up = std::max(box.up, y);
    };
    if (type == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR)
    {
        if (indices.size() % 3) return {};
        for (uint32_t i : indices) if (i >= vertices.size()) return {};
        const float sx = fov.width() / grid, sy = fov.height() / grid;
        for (int j = 0; j < grid; ++j)
            for (int i = 0; i < grid; ++i)
            {
                const float x = fov.left + (i + .5f) * sx, y = fov.down + (j + .5f) * sy;
                bool hidden = false;
                for (size_t t = 0; t < indices.size() && !hidden; t += 3)
                    hidden = insideTriangle(x, y, vertices[indices[t]], vertices[indices[t + 1]], vertices[indices[t + 2]]);
                if (!hidden) { add(x - sx * .5f, y - sy * .5f); add(x + sx * .5f, y + sy * .5f); }
            }
    }
    else
    {
        if (type == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR && (indices.size() < 3 || indices.size() % 3)) return {};
        for (uint32_t i : indices) { if (i >= vertices.size()) return {}; add(vertices[i].x, vertices[i].y); }
    }
    if (!box.valid()) return {};
    return {std::max(box.left, fov.left), std::min(box.right, fov.right), std::min(box.up, fov.up), std::max(box.down, fov.down)};
}
// Visible bounds widened by `marginDegrees` on every side, never beyond the runtime field of view.
inline TanRect cropRect(const TanRect &fov, const TanRect &visible, float marginDegrees)
{
    const float m = marginDegrees * 3.14159265f / 180.f;
    const auto widen = [m](float t, float sign) { return std::tan(std::atan(t) + sign * m); };
    return {std::max(fov.left, widen(visible.left, -1)), std::min(fov.right, widen(visible.right, 1)),
            std::min(fov.up, widen(visible.up, 1)), std::max(fov.down, widen(visible.down, -1))};
}
// A mask is trusted only when its box is a plausible lens area: inside the field of view, covering at
// least `minCover` of each axis and containing the view centre. Anything else (empty masks, runtime bugs
// such as mis-scaled vertical coordinates) leaves the full field of view.
inline bool plausible(const TanRect &fov, const TanRect &visible, float minCover = .6f)
{
    return visible.valid() && visible.width() >= minCover * fov.width() && visible.height() >= minCover * fov.height() &&
           visible.left < 0 && visible.right > 0 && visible.up > 0 && visible.down < 0;
}
// Pixel extent for `crop` when `fullWidth`x`fullHeight` covers `fov` (same pixels per tangent unit).
inline void croppedExtent(const TanRect &fov, const TanRect &crop, uint32_t fullWidth, uint32_t fullHeight,
                          uint32_t &width, uint32_t &height)
{
    width = std::min(fullWidth, std::max(1u, uint32_t(std::ceil(fullWidth * crop.width() / fov.width()))));
    height = std::min(fullHeight, std::max(1u, uint32_t(std::ceil(fullHeight * crop.height() / fov.height()))));
}
// The crop of this frame's field of view: the fixed crop tangents, intersected with the current fov.
inline XrFovf apply(const XrFovf &current, const TanRect &crop)
{
    const auto f = fromFov(current);
    return toFov({std::max(f.left, crop.left), std::min(f.right, crop.right), std::min(f.up, crop.up), std::max(f.down, crop.down)});
}
} // namespace evr::fovcrop
