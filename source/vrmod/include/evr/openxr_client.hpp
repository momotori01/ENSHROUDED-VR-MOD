#pragma once
#include <iomanip>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <evr/log.hpp>
#include <evr/math.hpp>
#include <evr/fov_crop.hpp>
#include <evr/symmetric_fov.hpp>
#include <evr/openxr_dispatch.hpp>
#include <evr/vulkan.hpp>
#include <functional>
#include <openxr/openxr_platform.h>
#include <thread>
namespace evr
{
inline void xrCheck(XrResult r, const char *operation)
{
    if (XR_FAILED(r))
        throw std::runtime_error(std::string(operation) + " XrResult=" + std::to_string(r));
}
template <class T> T xrFunction(XrInstance instance, const char *name)
{
    PFN_xrVoidFunction fn = nullptr;
    xrCheck(xrGetInstanceProcAddr(instance, name, &fn), name);
    if (!fn)
        throw std::runtime_error(std::string("null OpenXR function ") + name);
    return reinterpret_cast<T>(fn);
}
struct EyeTarget
{
    XrSwapchain handle = XR_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    std::vector<XrSwapchainImageVulkanKHR> images;
};
struct OpenXrFrameSnapshot
{
    std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::array<uint32_t, 2> imageIndices{};
    XrTime predictedDisplayTime = 0;
    uint64_t frame = 0;
    bool shouldRender = false;
    bool screenOnly = false; // Explicit flat media/menu frame; never a world stereo substitute.
    // The eyes drawn in this frame (1 left, 2 right, 3 both). An eye left out shows the picture it was last drawn
    // with, at the pose it was drawn at: its swapchain is not acquired, and xrEndFrame takes "the most recently
    // released swapchain image" (OpenXR, rendering chapter). Alternate-eye rendering (2026-10-14, user: "try the
    // PureDark way" - one eye a frame, the other from before).
    uint32_t eyeMask = 3;
};
// A part of the UI image as a quad of its own (the aim cluster, include/evr/aim_mark.hpp): its pixels in the image,
// the pose of its centre in the VIEW space or (localSpace) the LOCAL space, its size in metres.
struct UiQuadPart {
    int32_t x = 0, y = 0, pixelsX = 0, pixelsY = 0;
    bool localSpace = false;
    Pose pose = identityPose();
    float width = 0, height = 0;
};
struct UiQuadPlacement {
    float distance = 1.5f, width = 2.f;
    bool localSpace = false;
    // Frozen viewing origin for a screen in LOCAL space. Head motion after this
    // value is chosen changes the view of the screen, not the screen's pose.
    Pose origin = identityPose();
    // The image is the game's whole picture (layer/screen_xr.hpp): shown as it is, whatever its alpha holds.
    bool opaque = false;
    // When partCount is not 0 the UI image is submitted as these quads instead of the one plate (twelve: the plate around
    // two parts taken out - the aim cluster and the locked enemy's health bar - and those two, aim_mark.hpp plateWithout).
    std::array<UiQuadPart, 12> parts{};
    uint32_t partCount = 0;
};
class OpenXrClient : private OpenXrDispatch
{
    Log &log;
    bool ownsXrInstance = true;
    bool symmetricFov = false; // explicit per-client compatibility mode; default unchanged
    // With symmetricFov: the symmetric frustum turned toward the middle of the runtime's (symmetric_fov.hpp
    // cantedEnvelope, 2026-10-19), the eye extent shrunk to keep the pixels per degree. EVR_CANTED_FOV=0: the plain
    // envelope at the full extent, as before.
    bool cantedFov = true;
    // masked: the turned frustums of the first frame hold the lens' visible area (cantedEnvelope with the mask) and are
    // used while the runtime fov stays that of the first frame; a changed mask or fov goes back to the corners' frustum.
    // cacheWritten: the lens' visible area came later than the first frame and was kept for the next start (fovCache)
    struct Cant {bool decided = false, masked = false, cacheWritten = false, lateGaveUp = false; std::string lateWhy;
        std::array<uint32_t, 2> fullWidth{}, fullHeight{};
        std::array<XrFovf, 2> runtime{}; std::array<CantedFov, 2> turned{};} cant;
    bool running = false, exit = false, loss = false;
    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    struct PendingFrame
    {
        OpenXrFrameSnapshot snapshot;
        std::array<bool, 2> acquired{}, ready{}, rendered{};
        // warpable: an eye left out whose image was acquired for a warped picture (eye_warp.hpp); warped: that
        // picture was drawn, so the eye is shown at this frame's pose like a drawn one.
        std::array<bool, 2> warpable{}, warped{};
        bool active = false, endAttempted = false, uiAttempted = false;
    } pending;
    uint32_t warpMask = 0;   // the eyes to acquire for a warped picture in the next frame, when left out
    // Per eye: the view (pose and fov) its swapchain's latest released image was drawn with. Valid only while that
    // image is one that was drawn; an image released unused (a skipped or screen frame, a failure) ends it.
    std::array<XrView, 2> lastDrawn{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::array<bool, 2> lastDrawnValid{};
    uint32_t nextEyeMask = 3;
    // The pose an eye left out is shown at in this frame, when the application knows a better one than the pose it was
    // drawn at (the world moved under the tracking space: a turn by the stick, a step).
    std::array<bool, 2> staleOverride{};
    std::array<XrPosef, 2> stalePose{};
    EyeTarget uiTarget;
    VkFormat uiFormat = VK_FORMAT_UNDEFINED;
    // Visibility-mask field of view crop (fov_crop.hpp). Decided once on the first rendered frame; the eye
    // extent (targets[].width/height) shrinks inside the full-size swapchain image and every frame's fov is
    // intersected with the crop tangents. EVR_FOV_CROP=0 disables it; EVR_FOV_CROP_MARGIN_DEG sets the margin.
    PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR = nullptr;
    struct FovCrop {
        bool decided = false, active = false;
        std::array<fovcrop::TanRect, 2> rect{}, full{};
        std::array<uint32_t, 2> fullWidth{}, fullHeight{};
    } crop;
    static bool fovCropRequested()
    {
        const char *v = std::getenv("EVR_FOV_CROP");
        return !v || std::strcmp(v, "0");
    }
    static float fovCropMarginDegrees()
    {
        const char *v = std::getenv("EVR_FOV_CROP_MARGIN_DEG");
        const float m = v ? float(std::atof(v)) : 1.f;
        return std::isfinite(m) ? std::clamp(m, 0.f, 10.f) : 1.f;
    }
    bool visibleBounds(uint32_t eye, const fovcrop::TanRect &fov, fovcrop::TanRect &out, const char *&kind)
    {
        for (auto type : {XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR,
                          XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR})
        {
            XrVisibilityMaskKHR mask{XR_TYPE_VISIBILITY_MASK_KHR};
            if (XR_FAILED(xrGetVisibilityMaskKHR(session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, eye, type, &mask)) ||
                !mask.vertexCountOutput || !mask.indexCountOutput || mask.vertexCountOutput > 65536 || mask.indexCountOutput > 196608)
                continue;
            std::vector<XrVector2f> vertices(mask.vertexCountOutput);
            std::vector<uint32_t> indices(mask.indexCountOutput);
            mask.vertexCapacityInput = uint32_t(vertices.size()); mask.vertices = vertices.data();
            mask.indexCapacityInput = uint32_t(indices.size()); mask.indices = indices.data();
            if (XR_FAILED(xrGetVisibilityMaskKHR(session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, eye, type, &mask)))
                continue;
            vertices.resize(std::min<size_t>(vertices.size(), mask.vertexCountOutput));
            indices.resize(std::min<size_t>(indices.size(), mask.indexCountOutput));
            out = fovcrop::visibleBounds(type, vertices, indices, fov);
            kind = type == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR ? "visible_mesh"
                 : type == XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR ? "line_loop" : "hidden_mesh";
            return true;
        }
        return false;
    }
    // The eye extent for the turned symmetric frustum (cantedEnvelope), decided once on the first rendered frame as the
    // crop below: the pixels per tangent unit of the plain envelope at the full extent, for the smaller turned frustum.
    // One extent for both eyes (the larger of the two), as the layer's eye passes expect (the GI and fog copies find
    // the other eye by its extent, the eye warp needs both alike); the swapchain images keep their full size.
    // The turned frustums held to the lens of this headset, kept for the next start (2026-10-19; the user: "as EDF6 does,
    // measured for each headset at the first start and trimmed from the second - from the first, if possible"). When the
    // runtime gives no visible area at the first frame (the headset of 10-19 04:26: "mask none"), it is asked again for a
    // while (lateMask); what it then gives is written to EVR_FOV_CACHE_FILE (the package: EnshroudedVR\run\fov-cache.txt)
    // with the runtime fovs it belongs to, and the next start whose runtime fovs are the same uses it from its first frame.
    // A line: EVR_FOV_CACHE_1 <margin degrees> <8 runtime fov angles> <4 tangents: eye 0 x y, eye 1 x y> <mask kind>.
    static std::filesystem::path fovCacheFile()
    {
        const wchar_t *v = _wgetenv(L"EVR_FOV_CACHE_FILE");   // wide: the game folder may have any name
        return v && *v ? std::filesystem::path(v) : std::filesystem::path();
    }
    bool readFovCache(const std::array<XrView, 2> &views, std::array<double, 4> &tangents, std::string &kind)
    {
        const auto path = fovCacheFile();
        if (path.empty()) return false;
        std::ifstream in(path);
        std::string magic;
        float margin = 0;
        std::array<float, 8> fov{};
        if (!(in >> magic >> margin) || magic != "EVR_FOV_CACHE_1" || std::abs(margin - fovCropMarginDegrees()) > 1e-3f) return false;
        for (auto &a : fov) if (!(in >> a)) return false;
        for (auto &t : tangents) if (!(in >> t) || !std::isfinite(t) || t < .2 || t > 6) return false;
        if (!(in >> kind)) kind = "?";
        for (uint32_t eye = 0; eye < 2; ++eye)
        {
            const auto &f = views[eye].fov;
            const std::array<float, 4> now{f.angleLeft, f.angleRight, f.angleUp, f.angleDown};
            for (size_t i = 0; i < 4; ++i) if (std::abs(now[i] - fov[eye * 4 + i]) > 1e-4f) return false;
        }
        return true;
    }
    void writeFovCache(const std::array<XrView, 2> &views, const std::array<CantedFov, 2> &turned, const char *kind)
    {
        const auto path = fovCacheFile();
        if (path.empty()) return;
        try
        {
            const auto& p = path;
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
            std::ofstream out(p, std::ios::trunc);
            out << std::setprecision(9) << "EVR_FOV_CACHE_1 " << fovCropMarginDegrees();
            for (uint32_t eye = 0; eye < 2; ++eye)
            {
                const auto &f = views[eye].fov;
                out << ' ' << f.angleLeft << ' ' << f.angleRight << ' ' << f.angleUp << ' ' << f.angleDown;
            }
            for (uint32_t eye = 0; eye < 2; ++eye) out << ' ' << std::tan(turned[eye].fov.angleRight) << ' ' << std::tan(turned[eye].fov.angleUp);
            out << ' ' << kind << '\n';
            log.event("canted_fov_cache_written", "\"file\":" + jsonString(path.filename().string()) + ",\"ok\":" + (out ? "true" : "false"));
        }
        catch (const std::exception &e)
        {
            log.event("canted_fov_cache_written", "\"ok\":false,\"reason\":" + jsonString(e.what()));
        }
    }
    // Asked again every 30 frames for the first 3000 when the first frame had no visible area (lateMask): kept for the next
    // start only - this session's eyes keep the extent decided at their first frame.
    void lateMask(const std::array<XrView, 2> &views)
    {
        std::array<std::vector<XrVector2f>, 2> visible;
        const char *kind = "none";
        std::string why;
        for (uint32_t eye = 0; eye < 2; ++eye)
            if (!visiblePoints(eye, fovcrop::fromFov(views[eye].fov), visible[eye], kind, &why)) { cant.lateWhy = "eye " + std::to_string(eye) + ": " + why; return; }
        std::array<CantedFov, 2> turned;
        try
        {
            for (uint32_t eye = 0; eye < 2; ++eye) turned[eye] = cantedEnvelope(views[eye].fov, &visible[eye], fovCropMarginDegrees() * pi / 180.f);
        }
        catch (const std::exception &e) { cant.lateWhy = e.what(); return; }
        cant.cacheWritten = true;
        std::ostringstream d;
        d << std::setprecision(6) << "\"frame\":" << frames << ",\"mask\":\"" << kind << "\",\"turned_fov\":[" << turned[0].fov.angleRight << ','
          << turned[0].fov.angleUp << ',' << turned[1].fov.angleRight << ',' << turned[1].fov.angleUp << "],\"from_the_next_start\":true";
        log.event("canted_fov_mask_late", d.str());
        writeFovCache(views, turned, kind);
    }
    void decideCant(const std::array<XrView, 2> &views)
    {
        cant.decided = true;
        std::ostringstream detail;
        detail << std::setprecision(6);
        uint32_t width = 0, height = 0;
        try
        {
            // the lens' visible area of both eyes, when the runtime gives a plausible one
            std::array<std::vector<XrVector2f>, 2> visible;
            bool masked = xrGetVisibilityMaskKHR && fovCropRequested();
            const char *kind = "none";
            // why there is no mask (the headset of 10-19 04:26 gave "none" and nothing more): recorded for the next run
            std::string maskWhy = !xrGetVisibilityMaskKHR ? "runtime_without_visibility_mask" : !fovCropRequested() ? "disabled_by_EVR_FOV_CROP" : "";
            for (uint32_t eye = 0; eye < 2 && masked; ++eye)
            {
                std::string why;
                masked = visiblePoints(eye, fovcrop::fromFov(views[eye].fov), visible[eye], kind, &why);
                if (!masked) maskWhy = "eye " + std::to_string(eye) + ": " + why;
            }
            const float margin = fovCropMarginDegrees() * pi / 180.f;
            // no visible area now: the one this headset gave at an earlier start (fovCache), when its fovs are the same
            std::array<double, 4> cached{};
            std::string cachedKind;
            const bool fromCache = !masked && readFovCache(views, cached, cachedKind);
            detail << "\"mask\":\"" << (masked ? kind : fromCache ? "cache" : "none") << "\",\"mask_why\":" << jsonString(maskWhy)
                   << ",\"cache\":" << (fromCache ? jsonString(cachedKind) : std::string("null"))
                   << ",\"margin_deg\":" << fovCropMarginDegrees() << ",\"eyes\":[";
            for (uint32_t eye = 0; eye < 2; ++eye)
            {
                const auto plain = symmetricEnvelope(views[eye].fov);
                auto turned = cantedEnvelope(views[eye].fov, masked ? &visible[eye] : nullptr, margin);
                if (fromCache)   // the same turn (it follows the runtime fov); never wider than the corners' frustum
                {
                    const float x = float(std::atan(std::min(cached[eye * 2], double(std::tan(turned.fov.angleRight)))));
                    const float y = float(std::atan(std::min(cached[eye * 2 + 1], double(std::tan(turned.fov.angleUp)))));
                    turned.fov = {-x, x, y, -y};
                }
                cant.runtime[eye] = views[eye].fov; cant.turned[eye] = turned;
                const double sx = std::tan(plain.angleRight), sy = std::tan(plain.angleUp);
                const double cx = std::tan(turned.fov.angleRight), cy = std::tan(turned.fov.angleUp);
                width = std::max(width, uint32_t(std::ceil(targets[eye].width * cx / sx)));
                height = std::max(height, uint32_t(std::ceil(targets[eye].height * cy / sy)));
                detail << (eye ? "," : "") << "{\"eye\":" << eye << ",\"runtime_fov\":[" << views[eye].fov.angleLeft << ','
                       << views[eye].fov.angleRight << ',' << views[eye].fov.angleUp << ',' << views[eye].fov.angleDown
                       << "],\"turned_fov\":[" << turned.fov.angleRight << ',' << turned.fov.angleUp << "],\"plain_fov\":["
                       << plain.angleRight << ',' << plain.angleUp << "],\"turn\":[" << turned.turn.x << ',' << turned.turn.y << ','
                       << turned.turn.z << ',' << turned.turn.w << "]}";
            }
            cant.masked = masked || fromCache;   // the frames keep these frustums while the runtime fov stays the same
            if (masked) { writeFovCache(views, cant.turned, kind); cant.cacheWritten = true; }
            else if (!fromCache) cant.lateWhy = maskWhy;
        }
        catch (const std::exception &e)
        {
            cantedFov = false;
            log.event("canted_fov", "\"applied\":false,\"reason\":" + jsonString(e.what()));
            return;
        }
        width += width & 1; height += height & 1;
        detail << "],\"full\":[" << targets[0].width << ',' << targets[0].height << "]";
        if (targets[0].width != targets[1].width || targets[0].height != targets[1].height ||
            width > targets[0].width || height > targets[0].height || width < 64 || height < 64)
        {
            detail << ",\"extent_kept\":true";   // the turn still applies: the pixels per degree only rise
            log.event("canted_fov", detail.str() + ",\"applied\":true");
            return;
        }
        for (uint32_t eye = 0; eye < 2; ++eye)
        {
            cant.fullWidth[eye] = targets[eye].width; cant.fullHeight[eye] = targets[eye].height;
            targets[eye].width = width; targets[eye].height = height;
        }
        detail << ",\"extent\":[" << width << ',' << height << "],\"pixel_saving\":"
               << 1.0 - double(width) * height / (double(cant.fullWidth[0]) * cant.fullHeight[0]);
        log.event("canted_fov", detail.str() + ",\"applied\":true");
    }
    // The lens' visible area as points (tangents on z = -1 of the runtime eye) for cantedEnvelope: the vertices of a
    // visible mesh or line loop, or for a hidden mesh the corners of the visible cells of a grid. Only a plausible area
    // (fovcrop::plausible: inside the fov, most of each axis, around the centre); false leaves the corners' frustum.
    bool visiblePoints(uint32_t eye, const fovcrop::TanRect &fov, std::vector<XrVector2f> &out, const char *&kind, std::string *why = nullptr)
    {
        out.clear();
        fovcrop::TanRect bounds{};
        const auto no = [&](const std::string &reason) { if (why) *why = reason; return false; };
        if (!fov.valid()) return no("fov invalid");
        if (!visibleBounds(eye, fov, bounds, kind)) return no("no mask from the runtime (all three types empty or failed)");
        if (!fovcrop::plausible(fov, bounds))
        {
            std::ostringstream b;
            b << std::setprecision(5) << "mask " << kind << " not plausible: bounds [" << bounds.left << ',' << bounds.right << ',' << bounds.up << ','
              << bounds.down << "] in fov [" << fov.left << ',' << fov.right << ',' << fov.up << ',' << fov.down << ']';
            return no(b.str());
        }
        for (auto type : {XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR,
                          XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR})
        {
            XrVisibilityMaskKHR mask{XR_TYPE_VISIBILITY_MASK_KHR};
            if (XR_FAILED(xrGetVisibilityMaskKHR(session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, eye, type, &mask)) ||
                !mask.vertexCountOutput || !mask.indexCountOutput || mask.vertexCountOutput > 65536 || mask.indexCountOutput > 196608)
                continue;
            std::vector<XrVector2f> vertices(mask.vertexCountOutput);
            std::vector<uint32_t> indices(mask.indexCountOutput);
            mask.vertexCapacityInput = uint32_t(vertices.size()); mask.vertices = vertices.data();
            mask.indexCapacityInput = uint32_t(indices.size()); mask.indices = indices.data();
            if (XR_FAILED(xrGetVisibilityMaskKHR(session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, eye, type, &mask)))
                continue;
            vertices.resize(std::min<size_t>(vertices.size(), mask.vertexCountOutput));
            indices.resize(std::min<size_t>(indices.size(), mask.indexCountOutput));
            const auto clampIn = [&](XrVector2f p) { return XrVector2f{std::clamp(p.x, fov.left, fov.right), std::clamp(p.y, fov.down, fov.up)}; };
            if (type == XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR)
            {
                if (indices.size() % 3) continue;
                bool bad = false; for (uint32_t i : indices) bad |= i >= vertices.size(); if (bad) continue;
                constexpr int grid = 96;
                const float sx = fov.width() / grid, sy = fov.height() / grid;
                for (int j = 0; j < grid; ++j)
                    for (int i = 0; i < grid; ++i)
                    {
                        const float x = fov.left + (i + .5f) * sx, y = fov.down + (j + .5f) * sy;
                        bool hidden = false;
                        for (size_t t = 0; t < indices.size() && !hidden; t += 3)
                            hidden = fovcrop::insideTriangle(x, y, vertices[indices[t]], vertices[indices[t + 1]], vertices[indices[t + 2]]);
                        if (!hidden)
                            for (float dx : {-.5f, .5f}) for (float dy : {-.5f, .5f}) out.push_back(clampIn({x + dx * sx, y + dy * sy}));
                    }
            }
            else
            {
                bool bad = false; for (uint32_t i : indices) bad |= i >= vertices.size(); if (bad) continue;
                for (uint32_t i : indices) out.push_back(clampIn(vertices[i]));
            }
            if (out.size() >= 3) return true;
            out.clear();
        }
        return no(std::string("mask ") + kind + " plausible but gave no points");
    }
    void decideFovCrop(const std::array<XrView, 2> &views)
    {
        crop.decided = true;
        const auto skip = [&](const char *reason) { log.event("fov_crop", std::string("\"applied\":false,\"reason\":") + jsonString(reason)); };
        if (symmetricFov) { if (cantedFov) decideCant(views); return skip("symmetric_projection_keeps_full_runtime_fov"); }
        if (!fovCropRequested()) return skip("disabled_by_EVR_FOV_CROP");
        if (!xrGetVisibilityMaskKHR) return skip("runtime_without_visibility_mask");
        const float margin = fovCropMarginDegrees();
        std::array<fovcrop::TanRect, 2> rects{};
        std::array<uint32_t, 2> w{}, h{};
        uint64_t full = 0, cropped = 0;
        std::ostringstream detail;
        detail << std::setprecision(6) << "\"margin_deg\":" << margin << ",\"eyes\":[";
        for (uint32_t eye = 0; eye < 2; ++eye)
        {
            const auto fov = fovcrop::fromFov(views[eye].fov);
            fovcrop::TanRect visible{};
            const char *kind = "none";
            if (!fov.valid() || !visibleBounds(eye, fov, visible, kind)) return skip("mask_unavailable");
            const bool ok = fovcrop::plausible(fov, visible);
            rects[eye] = ok ? fovcrop::cropRect(fov, visible, margin) : fov;
            fovcrop::croppedExtent(fov, rects[eye], targets[eye].width, targets[eye].height, w[eye], h[eye]);
            full += uint64_t(targets[eye].width) * targets[eye].height;
            cropped += uint64_t(w[eye]) * h[eye];
            detail << (eye ? "," : "") << "{\"eye\":" << eye << ",\"mask\":\"" << kind << "\",\"plausible\":" << (ok ? "true" : "false")
                   << ",\"fov_tan\":[" << fov.left << ',' << fov.right << ',' << fov.up << ',' << fov.down << "],\"visible_tan\":["
                   << visible.left << ',' << visible.right << ',' << visible.up << ',' << visible.down << "],\"crop_tan\":["
                   << rects[eye].left << ',' << rects[eye].right << ',' << rects[eye].up << ',' << rects[eye].down << "],\"full\":["
                   << targets[eye].width << ',' << targets[eye].height << "],\"cropped\":[" << w[eye] << ',' << h[eye] << "]}";
        }
        const double saving = full ? 1.0 - double(cropped) / double(full) : 0.0;
        // Below 2 % the change is not worth a different eye extent.
        const bool apply = saving >= 0.02;
        detail << "],\"pixel_saving\":" << saving << ",\"applied\":" << (apply ? "true" : "false");
        if (apply)
            for (uint32_t eye = 0; eye < 2; ++eye)
            {
                crop.fullWidth[eye] = targets[eye].width; crop.fullHeight[eye] = targets[eye].height;
                crop.rect[eye] = rects[eye]; crop.full[eye] = fovcrop::fromFov(views[eye].fov);
                targets[eye].width = w[eye]; targets[eye].height = h[eye];
            }
        crop.active = apply;
        log.event("fov_crop", detail.str());
    }
    // A changed mask that reaches outside the crop ends the crop (full fov at the decided extent, so
    // pixel density drops but nothing visible is cut). The eye extent cannot grow during a session.
    void visibilityMaskChanged(uint32_t eye)
    {
        // the turned frustums held the first mask: from now on they hold the runtime rectangle (safe, a little wider)
        if (cant.masked) { cant.masked = false; log.event("canted_fov_mask_changed", "\"eye\":" + std::to_string(eye) + ",\"corners_from_now\":true"); }
        if (!crop.active || eye >= 2) return;
        fovcrop::TanRect visible{};
        const char *kind = "none";
        bool kept = visibleBounds(eye, crop.full[eye], visible, kind) && fovcrop::plausible(crop.full[eye], visible);
        if (kept)
        {
            const auto wanted = fovcrop::cropRect(crop.full[eye], visible, fovCropMarginDegrees());
            kept = wanted.left >= crop.rect[eye].left && wanted.right <= crop.rect[eye].right &&
                   wanted.up <= crop.rect[eye].up && wanted.down >= crop.rect[eye].down;
        }
        if (!kept) crop.active = false;
        log.event("fov_crop_mask_changed", "\"eye\":" + std::to_string(eye) + ",\"crop_kept\":" + (kept ? "true" : "false"));
    }
    std::vector<int64_t> availableFormats;
    XrSystemGraphicsProperties graphics{};
    struct UiPending {
        uint64_t frame = 0;
        uint32_t index = 0;
        bool acquired = false, ready = false, rendered = false;
        UiQuadPlacement placement{};
    } uiPending;
    void releaseUi() {
        if (!uiPending.acquired) return;
        if (!uiPending.ready) {
            XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=100000000;
            const auto result=xrWaitSwapchainImage(uiTarget.handle,&w);
            if (result!=XR_SUCCESS) throw std::runtime_error("UI cleanup wait timeout/error "+std::to_string(result));
            uiPending.ready=true;
        }
        XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrCheck(xrReleaseSwapchainImage(uiTarget.handle,&r),"UI release image");
        uiPending.acquired=uiPending.ready=false;
    }

  public:
    Vulkan vk;
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace local = XR_NULL_HANDLE, headSpace = XR_NULL_HANDLE;
    std::array<EyeTarget, 2> targets;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint64_t frames = 0, draws[2]{0, 0}, emptyFrames = 0;
    XrSpaceLocation headLocation{XR_TYPE_SPACE_LOCATION};
    // The runtime's view reset (2026-10-18; user: "the reset by SteamVR's or Meta's system button held"): each
    // XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING for this session's LOCAL space counts here with the time
    // from which poses are in the new space; the hooks read them (evrNativeLocalSpaceChange) and take the view's
    // origin again from then on.
    uint64_t localSpaceChanges = 0;
    XrTime localSpaceChangeTime = 0;
    float movementYaw = 0;
    std::string diagnosticCameraMode = "test_scene";
    explicit OpenXrClient(Log &l, PFN_xrGetInstanceProcAddr get = &::xrGetInstanceProcAddr)
        : OpenXrDispatch(get), log(l)
    {
    }
    OpenXrClient(const OpenXrClient &) = delete;
    OpenXrClient &operator=(const OpenXrClient &) = delete;
    ~OpenXrClient()
    {
        shutdown();
    }
    void init(float renderScale, const BorrowedVulkan *borrowed = nullptr, const BorrowedOpenXr *borrowedXr = nullptr,
              XrSwapchainUsageFlags extraUsage = 0)
    {
        const auto symmetricFlag=std::getenv("EVR_SYMMETRIC_FOV");
        symmetricFov=symmetricFlag&&!std::strcmp(symmetricFlag,"1");
        {const auto cantFlag=std::getenv("EVR_CANTED_FOV");cantedFov=!(cantFlag&&!std::strcmp(cantFlag,"0"));}
        if (instance || vk.instance)
            throw std::runtime_error("OpenXrClient init is single-use; create a new client for recreation");
        if (borrowedXr && (!borrowed || !borrowedXr->instance || !borrowedXr->vulkanEnable1Enabled))
            throw std::invalid_argument("borrowed OpenXR requires an enabled legacy Vulkan binding and host device");
        uint32_t n = 0;
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr),
                "xrEnumerateInstanceExtensionProperties");
        std::vector<XrExtensionProperties> extensions(n, {XR_TYPE_EXTENSION_PROPERTIES});
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr, n, &n, extensions.data()),
                "xrEnumerateInstanceExtensionProperties");
        bool enable2 = false, enable1 = false, visibilityMask = false;
        for (const auto &e : extensions)
        {
            if (!std::strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
                enable2 = true;
            if (!std::strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME))
                enable1 = true;
            if (!std::strcmp(e.extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME))
                visibilityMask = true;
        }
        if (!enable2 && !enable1)
            throw std::runtime_error("runtime has no OpenXR Vulkan graphics binding");
        // Legacy binding specifies the required extension lists explicitly, which lets us
        // validate an existing game device without silently creating a second device.
        if (borrowed)
        {
            if (!enable1)
                throw std::runtime_error(
                    "runtime cannot validate an existing Vulkan device; enable1 unavailable");
            enable2 = false;
        }
        const char *extension =
            enable2 ? XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME : XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(ci.applicationInfo.applicationName, "Enshrouded VR development");
        ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
        const std::array<const char *, 2> enabled{extension, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME};
        ci.enabledExtensionCount = visibilityMask && fovCropRequested() ? 2 : 1;
        ci.enabledExtensionNames = enabled.data();
        if (borrowedXr)
        {
            ownsXrInstance = false;
            instance = borrowedXr->instance;
        }
        else
            xrCheck(xrCreateInstance(&ci, &instance), "xrCreateInstance");
        bindOpenXrInstance(instance);
        if (borrowedXr ? borrowedXr->visibilityMaskEnabled : ci.enabledExtensionCount == 2)
            xrGetVisibilityMaskKHR = xrFunction<PFN_xrGetVisibilityMaskKHR>(instance, "xrGetVisibilityMaskKHR");
        log.event("openxr_instance_ownership", std::string("\"owned\":") + (ownsXrInstance ? "true" : "false"));
        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
        xrCheck(xrGetInstanceProperties(instance, &props), "xrGetInstanceProperties");
        log.event("runtime", "\"name\":" + jsonString(props.runtimeName) +
                                 ",\"version\":" + std::to_string(props.runtimeVersion) +
                                 ",\"binding\":" + jsonString(extension));
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
        si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        xrCheck(xrGetSystem(instance, &si, &system), "xrGetSystem");
        XrGraphicsRequirementsVulkanKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        if (enable2)
            xrCheck(xrFunction<PFN_xrGetVulkanGraphicsRequirements2KHR>(
                        instance, "xrGetVulkanGraphicsRequirements2KHR")(instance, system, &req),
                    "xrGetVulkanGraphicsRequirements2KHR");
        else
            xrCheck(xrFunction<PFN_xrGetVulkanGraphicsRequirementsKHR>(
                        instance, "xrGetVulkanGraphicsRequirementsKHR")(instance, system, &req),
                    "xrGetVulkanGraphicsRequirementsKHR");
        VkInstance native{};
        VkPhysicalDevice physical{};
        VkDevice device{};
        uint32_t family = 0, queueIndex = 0;
        if (borrowed)
        {
            const auto &b = *borrowed;
            if (!b.getInstanceProcAddr || !b.instance || !b.physical || !b.device || !b.createdQueueCount ||
                b.queueIndex >= b.createdQueueCount)
                throw std::runtime_error("incomplete borrowed Vulkan creation records");
            const XrVersion version =
                XR_MAKE_VERSION(VK_VERSION_MAJOR(b.apiVersion), VK_VERSION_MINOR(b.apiVersion),
                                VK_VERSION_PATCH(b.apiVersion));
            if (version < req.minApiVersionSupported || version > req.maxApiVersionSupported)
                throw std::runtime_error("borrowed Vulkan API version outside runtime range");
            auto requireExtensions = [&](auto fn, const std::vector<std::string> &enabled, const char *kind) {
                uint32_t size = 0;
                xrCheck(fn(instance, system, 0, &size, nullptr), "borrowed required extension size");
                std::vector<char> names(std::max(1u, size), '\0');
                xrCheck(fn(instance, system, size, &size, names.data()), "borrowed required extensions");
                std::istringstream list(names.data());
                std::string name;
                while (list >> name)
                    if (std::find(enabled.begin(), enabled.end(), name) == enabled.end())
                        throw std::runtime_error(std::string("borrowed Vulkan missing ") + kind +
                                                 " extension " + name);
            };
            requireExtensions(xrFunction<PFN_xrGetVulkanInstanceExtensionsKHR>(
                                  instance, "xrGetVulkanInstanceExtensionsKHR"),
                              b.enabledInstanceExtensions, "instance");
            requireExtensions(
                xrFunction<PFN_xrGetVulkanDeviceExtensionsKHR>(instance, "xrGetVulkanDeviceExtensionsKHR"),
                b.enabledDeviceExtensions, "device");
            xrCheck(xrFunction<PFN_xrGetVulkanGraphicsDeviceKHR>(instance, "xrGetVulkanGraphicsDeviceKHR")(
                        instance, system, b.instance, &physical),
                    "borrowed runtime GPU");
            if (physical != b.physical)
                throw std::runtime_error("borrowed Vulkan GPU differs from runtime GPU");
            vk.gipa = b.getInstanceProcAddr;
            vk.bindInstance(b.instance);
            uint32_t count = 0;
            vk.vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vk.vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
            if (b.queueFamily >= count || b.createdQueueCount > families[b.queueFamily].queueCount ||
                !(families[b.queueFamily].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                throw std::runtime_error("borrowed Vulkan graphics queue invalid");
            native = b.instance;
            device = b.device;
            family = b.queueFamily;
            queueIndex = b.queueIndex;
            vk.bindDevice(device, physical, family, queueIndex);
            log.event("borrowed_vulkan", "\"owned\":false,\"queue_family\":" + std::to_string(family) +
                                             ",\"queue_index\":" + std::to_string(queueIndex));
        }
        else
        {
            XrVersion api = std::max(req.minApiVersionSupported, XR_MAKE_VERSION(1, 1, 0));
            if (api > req.maxApiVersionSupported)
                throw std::runtime_error("runtime Vulkan version range unsupported");
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = "EVR stereo test";
            app.apiVersion = VK_MAKE_VERSION(uint32_t(XR_VERSION_MAJOR(api)), uint32_t(XR_VERSION_MINOR(api)),
                                             uint32_t(XR_VERSION_PATCH(api)));
            VkInstanceCreateInfo vi{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            vi.pApplicationInfo = &app;
            std::vector<std::string> inames, dnames;
            std::vector<const char *> iptrs, dptrs;
            auto split = [](const std::vector<char> &buffer) {
                std::istringstream input(buffer.data());
                std::vector<std::string> names;
                std::string name;
                while (input >> name)
                    names.push_back(name);
                return names;
            };
            if (!enable2)
            {
                auto fn = xrFunction<PFN_xrGetVulkanInstanceExtensionsKHR>(
                    instance, "xrGetVulkanInstanceExtensionsKHR");
                uint32_t size = 0;
                xrCheck(fn(instance, system, 0, &size, nullptr), "instance extensions size");
                std::vector<char> b(size);
                xrCheck(fn(instance, system, size, &size, b.data()), "instance extensions");
                inames = split(b);
                for (auto &name : inames)
                    iptrs.push_back(name.c_str());
                vi.enabledExtensionCount = uint32_t(iptrs.size());
                vi.ppEnabledExtensionNames = iptrs.data();
            }
            VkResult vr = VK_SUCCESS;
            if (enable2)
            {
                XrVulkanInstanceCreateInfoKHR xvi{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
                xvi.systemId = system;
                xvi.pfnGetInstanceProcAddr = vk.gipa;
                xvi.vulkanCreateInfo = &vi;
                xrCheck(xrFunction<PFN_xrCreateVulkanInstanceKHR>(instance, "xrCreateVulkanInstanceKHR")(
                            instance, &xvi, &native, &vr),
                        "xrCreateVulkanInstanceKHR");
                vkCheck(vr, "runtime vkCreateInstance");
            }
            else
                vkCheck(vk.vkCreateInstance(&vi, nullptr, &native), "vkCreateInstance");
            vk.owned = true;
            vk.bindInstance(native);
            if (enable2)
            {
                XrVulkanGraphicsDeviceGetInfoKHR gi{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
                gi.systemId = system;
                gi.vulkanInstance = native;
                xrCheck(xrFunction<PFN_xrGetVulkanGraphicsDevice2KHR>(
                            instance, "xrGetVulkanGraphicsDevice2KHR")(instance, &gi, &physical),
                        "xrGetVulkanGraphicsDevice2KHR");
            }
            else
                xrCheck(xrFunction<PFN_xrGetVulkanGraphicsDeviceKHR>(
                            instance, "xrGetVulkanGraphicsDeviceKHR")(instance, system, native, &physical),
                        "xrGetVulkanGraphicsDeviceKHR");
            VkPhysicalDeviceProperties gpu{};
            vk.vkGetPhysicalDeviceProperties(physical, &gpu);
            log.event("gpu", "\"name\":" + jsonString(gpu.deviceName));
            vk.vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, nullptr);
            std::vector<VkQueueFamilyProperties> families(n);
            vk.vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, families.data());
            for (; family < n; family++)
                if (families[family].queueCount && (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    break;
            if (family == n)
                throw std::runtime_error("runtime GPU has no graphics queue");
            float priority = 1;
            VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            qi.queueFamilyIndex = family;
            qi.queueCount = 1;
            qi.pQueuePriorities = &priority;
            VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            di.queueCreateInfoCount = 1;
            di.pQueueCreateInfos = &qi;
            if (!enable2)
            {
                auto fn = xrFunction<PFN_xrGetVulkanDeviceExtensionsKHR>(instance,
                                                                         "xrGetVulkanDeviceExtensionsKHR");
                uint32_t size = 0;
                xrCheck(fn(instance, system, 0, &size, nullptr), "device extensions size");
                std::vector<char> b(size);
                xrCheck(fn(instance, system, size, &size, b.data()), "device extensions");
                dnames = split(b);
                for (auto &name : dnames)
                    dptrs.push_back(name.c_str());
                di.enabledExtensionCount = uint32_t(dptrs.size());
                di.ppEnabledExtensionNames = dptrs.data();
            }
            if (enable2)
            {
                XrVulkanDeviceCreateInfoKHR xdi{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
                xdi.systemId = system;
                xdi.pfnGetInstanceProcAddr = vk.gipa;
                xdi.vulkanPhysicalDevice = physical;
                xdi.vulkanCreateInfo = &di;
                xrCheck(xrFunction<PFN_xrCreateVulkanDeviceKHR>(instance, "xrCreateVulkanDeviceKHR")(
                            instance, &xdi, &device, &vr),
                        "xrCreateVulkanDeviceKHR");
                vkCheck(vr, "runtime vkCreateDevice");
            }
            else
                vkCheck(vk.vkCreateDevice(physical, &di, nullptr, &device), "vkCreateDevice");
            vk.bindDevice(device, physical, family);
        }
        XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
        binding.instance = native;
        binding.physicalDevice = physical;
        binding.device = device;
        binding.queueFamilyIndex = family;
        binding.queueIndex = queueIndex;
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
        sci.next = &binding;
        sci.systemId = system;
        xrCheck(xrCreateSession(instance, &sci, &session), "xrCreateSession");
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        space.poseInReferenceSpace = identityPose();
        xrCheck(xrCreateReferenceSpace(session, &space, &local), "create LOCAL");
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        xrCheck(xrCreateReferenceSpace(session, &space, &headSpace), "create VIEW");
        xrCheck(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                 0, &n, nullptr),
                "blend modes");
        std::vector<XrEnvironmentBlendMode> blends(n);
        xrCheck(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                 n, &n, blends.data()),
                "blend modes");
        if (std::find(blends.begin(), blends.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE) == blends.end())
            throw std::runtime_error("opaque HMD blend mode unavailable");
        xrCheck(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                  0, &n, nullptr),
                "view count");
        if (n != 2)
            throw std::runtime_error("PRIMARY_STEREO must expose two views");
        std::vector<XrViewConfigurationView> configs(n, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        xrCheck(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                  n, &n, configs.data()),
                "view config");
        xrCheck(xrEnumerateSwapchainFormats(session, 0, &n, nullptr), "formats");
        std::vector<int64_t> formats(n);
        xrCheck(xrEnumerateSwapchainFormats(session, n, &n, formats.data()), "formats");
        for (auto preferred : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_FORMAT_B8G8R8A8_UNORM})
            if (std::find(formats.begin(), formats.end(), int64_t(preferred)) != formats.end())
            {
                format = preferred;
                break;
            }
        if (format == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("no supported color format");
        availableFormats = formats;
        XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES};
        xrCheck(xrGetSystemProperties(instance, system, &sp), "system properties");
        graphics=sp.graphicsProperties;
        for (int eye = 0; eye < 2; eye++)
        {
            auto &cfg = configs[eye];
            auto size =
                scaledExtent(cfg.recommendedImageRectWidth, cfg.recommendedImageRectHeight, renderScale,
                             std::min(cfg.maxImageRectWidth, sp.graphicsProperties.maxSwapchainImageWidth),
                             std::min(cfg.maxImageRectHeight, sp.graphicsProperties.maxSwapchainImageHeight));
            auto &t = targets[eye];
            t.width = size[0];
            t.height = size[1];
            XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | extraUsage;
            sc.format = format;
            sc.sampleCount = 1;
            sc.width = t.width;
            sc.height = t.height;
            sc.faceCount = sc.arraySize = sc.mipCount = 1;
            xrCheck(xrCreateSwapchain(session, &sc, &t.handle), "xrCreateSwapchain");
            xrCheck(xrEnumerateSwapchainImages(t.handle, 0, &n, nullptr), "swap images");
            t.images.resize(n, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
            xrCheck(xrEnumerateSwapchainImages(
                        t.handle, n, &n, reinterpret_cast<XrSwapchainImageBaseHeader *>(t.images.data())),
                    "swap images");
            log.event("swapchain",
                      "\"eye\":" + std::to_string(eye) +
                          ",\"recommended_width\":" + std::to_string(cfg.recommendedImageRectWidth) +
                          ",\"recommended_height\":" + std::to_string(cfg.recommendedImageRectHeight) +
                          ",\"width\":" + std::to_string(t.width) + ",\"height\":" +
                          std::to_string(t.height) + ",\"render_scale\":" + std::to_string(renderScale));
        }
    }
    // Optional native UI. Its pixel dimensions come from the native UI surface, not an HMD model.
    // Caller serializes these methods with frame/queue operations and completes all GPU work
    // before markUiRendered, resize, or shutdown. No stale UI is reused across XR frames.
    const EyeTarget& uiSwapchain() const { return uiTarget; }
    VkFormat uiSwapchainFormat() const { return uiFormat; }
    void configureUi(uint32_t width,uint32_t height,VkFormat requested) {
        if (!session || uiPending.acquired || (pending.active && pending.uiAttempted))
            throw std::logic_error("UI configure while unavailable or acquired");
        if (graphics.maxLayerCount<2 || !width || !height || width>graphics.maxSwapchainImageWidth ||
            height>graphics.maxSwapchainImageHeight || width>INT32_MAX || height>INT32_MAX)
            throw std::invalid_argument("UI exceeds Runtime layer/extent limits");
        if ((requested!=VK_FORMAT_R8G8B8A8_SRGB && requested!=VK_FORMAT_R8G8B8A8_UNORM) ||
            std::find(availableFormats.begin(),availableFormats.end(),int64_t(requested))==availableFormats.end())
            throw std::invalid_argument("UI format unsupported by Runtime");
        if (uiTarget.handle && width==uiTarget.width && height==uiTarget.height && requested==uiFormat) return;
        EyeTarget next;next.width=width;next.height=height;
        try {
            XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            sc.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
            sc.format=requested;sc.width=width;sc.height=height;sc.sampleCount=sc.faceCount=sc.arraySize=sc.mipCount=1;
            xrCheck(xrCreateSwapchain(session,&sc,&next.handle),"UI create swapchain");
            uint32_t n=0;xrCheck(xrEnumerateSwapchainImages(next.handle,0,&n,nullptr),"UI image count");
            if (!n) throw std::runtime_error("UI swapchain has no images");
            next.images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
            xrCheck(xrEnumerateSwapchainImages(next.handle,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(next.images.data())),"UI enumerate images");
            if (uiTarget.handle) {
                vkCheck(vk.vkDeviceWaitIdle(vk.device),"UI resize GPU completion");
                xrCheck(xrDestroySwapchain(uiTarget.handle),"UI destroy previous swapchain");
            }
        } catch (...) { if(next.handle)xrDestroySwapchain(next.handle);throw; }
        uiTarget=std::move(next);uiFormat=requested;
        log.event("ui_swapchain","\"width\":"+std::to_string(width)+",\"height\":"+std::to_string(height)+",\"format\":"+std::to_string(requested));
    }
    uint32_t acquireUi(uint64_t expectedFrame) {
        if (!pending.active || !pending.snapshot.shouldRender || expectedFrame!=pending.snapshot.frame ||
            !uiTarget.handle || pending.uiAttempted || uiPending.acquired)
            throw std::logic_error("UI acquire invalid frame/order");
        pending.uiAttempted=true;uiPending={};uiPending.frame=expectedFrame;
        try {
            XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrCheck(xrAcquireSwapchainImage(uiTarget.handle,&a,&uiPending.index),"UI acquire image");uiPending.acquired=true;
            XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=1000000000;
            const auto result=xrWaitSwapchainImage(uiTarget.handle,&w);
            if(result!=XR_SUCCESS)throw std::runtime_error("UI wait timeout/error "+std::to_string(result));
            uiPending.ready=true;
            if(uiPending.index>=uiTarget.images.size())throw std::runtime_error("UI Runtime returned invalid image index");
            return uiPending.index;
        } catch (...) { abortFrame("ui_acquire_failure");throw; }
    }
    // How many composition layers the runtime takes in one frame (the projection is one of them).
    uint32_t maxLayers() const { return graphics.maxLayerCount; }
    void markUiRendered(uint64_t expectedFrame,UiQuadPlacement placement={}) {
        if (!pending.active || expectedFrame!=pending.snapshot.frame || expectedFrame!=uiPending.frame ||
            !pending.uiAttempted || !uiPending.ready || uiPending.rendered)
            throw std::logic_error("UI completion invalid frame/order");
        if (!std::isfinite(placement.distance) || placement.distance<=0 || !std::isfinite(placement.width) || placement.width<=0 ||
            !std::isfinite(placement.width*float(uiTarget.height)/float(uiTarget.width)))
            throw std::invalid_argument("UI quad placement invalid");
        if (!finite(placement.origin.position))throw std::invalid_argument("UI origin position invalid");
        placement.origin.orientation=normalized(placement.origin.orientation);
        if (placement.localSpace && !finite(compose(placement.origin,Pose{{0,0,0,1},{0,0,-placement.distance}}).position))
            throw std::invalid_argument("UI local placement overflow");
        if (placement.partCount > placement.parts.size())throw std::invalid_argument("UI quad part count");
        if (placement.partCount + 1 > graphics.maxLayerCount)placement.partCount = 0;   // the runtime takes too few layers: the one plate
        for (uint32_t i = 0; i < placement.partCount; ++i) {
            auto &part = placement.parts[i];
            if (part.x < 0 || part.y < 0 || part.pixelsX <= 0 || part.pixelsY <= 0 || uint32_t(part.x + part.pixelsX) > uiTarget.width ||
                uint32_t(part.y + part.pixelsY) > uiTarget.height || !std::isfinite(part.width) || part.width <= 0 || !std::isfinite(part.height) ||
                part.height <= 0 || !finite(part.pose.position))
                throw std::invalid_argument("UI quad part invalid");
            part.pose.orientation = normalized(part.pose.orientation);
        }
        uiPending.placement=placement;uiPending.rendered=true;
    }
    bool poll()
    {
        XrEventDataBuffer e{XR_TYPE_EVENT_DATA_BUFFER};
        for (;;)
        {
            XrResult result = xrPollEvent(instance, &e);
            if (result == XR_EVENT_UNAVAILABLE)
                break;
            xrCheck(result, "xrPollEvent");
            if (e.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
            {
                auto &s = *reinterpret_cast<XrEventDataSessionStateChanged *>(&e);
                if (s.session == session)
                {
                    state = s.state;
                    log.event("session_state", "\"state\":" + std::to_string(state));
                    if (state == XR_SESSION_STATE_READY && !running)
                    {
                        XrSessionBeginInfo b{XR_TYPE_SESSION_BEGIN_INFO};
                        b.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        xrCheck(xrBeginSession(session, &b), "xrBeginSession");
                        running = true;
                    }
                    if (state == XR_SESSION_STATE_STOPPING && running)
                    {
                        xrCheck(xrEndSession(session), "xrEndSession");
                        running = false;
                    }
                    if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
                    {
                        exit = true;
                        loss = state == XR_SESSION_STATE_LOSS_PENDING;
                    }
                }
            }
            else if (e.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
            {
                exit = loss = true;
            }
            else if (e.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
            {
                const auto &c = *reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(&e);
                const bool ours = c.session == session && c.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL;
                if (ours)
                {
                    ++localSpaceChanges;
                    localSpaceChangeTime = c.changeTime;
                }
                log.event("reference_space_change_pending",
                          "\"space\":" + std::to_string(int(c.referenceSpaceType)) + ",\"this_session\":" +
                              (c.session == session ? "true" : "false") + ",\"change_time\":" + std::to_string(c.changeTime) +
                              ",\"pose_valid\":" + (c.poseValid ? "true" : "false") + ",\"local_changes\":" +
                              std::to_string(localSpaceChanges));
            }
            else if (e.type == XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR)
            {
                const auto &m = *reinterpret_cast<XrEventDataVisibilityMaskChangedKHR *>(&e);
                if (m.session == session) visibilityMaskChanged(m.viewIndex);
            }
            e = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        return !exit;
    }
    bool active() const
    {
        return running && !exit;
    }
    bool lost() const
    {
        return loss;
    }
    using Render = std::function<void(uint32_t, uint32_t, const XrView &, XrTime, uint64_t)>;
    // Freeze application camera inputs for both eyes once, after pose location
    // and before any swapchain image is acquired or either eye is rendered.
    using Prepare = std::function<void(const std::array<XrView, 2> &, const XrViewState &,
                                       const XrSpaceLocation &, XrTime, uint64_t)>;
    // All staged operations must be serialized by the caller. Queue access must
    // also be externally synchronized with the game and runtime. No new pose
    // query occurs between beginFrame and finishFrame.
    const OpenXrFrameSnapshot &pendingFrame() const
    {
        if (!pending.active) throw std::logic_error("no pending OpenXR frame");
        return pending.snapshot;
    }
    bool hasPendingFrame() const { return pending.active; }
    // The eyes to draw in the next world frame (1, 2 or 3). An eye whose last picture is not a drawn one is drawn
    // anyway; pendingFrame().eyeMask says what was decided.
    void requestEyes(uint32_t mask) { nextEyeMask = (mask & 3) ? (mask & 3) : 3; }
    // The eyes that, when left out of the next frame, get an image acquired for a warped picture (markEyeWarped).
    void requestWarp(uint32_t mask) { warpMask = mask & 3; }
    bool eyeWarpable(uint32_t eye) const { return pending.active && eye < 2 && pending.warpable[eye]; }
    // The warped picture of an eye left out was drawn into its acquired image (same rules as markEyeRendered).
    void markEyeWarped(uint32_t eye, uint64_t expectedFrame)
    {
        if (!pending.active || pending.snapshot.frame != expectedFrame || !pending.snapshot.shouldRender || eye >= 2 || !pending.warpable[eye] || !pending.ready[eye] || pending.rendered[eye])
            throw std::logic_error("invalid/duplicate OpenXR eye warp completion");
        pending.rendered[eye] = true; pending.warped[eye] = true;
    }
    // For the pending frame only, and only for an eye it leaves out.
    bool showStaleEyeAt(uint32_t eye, const XrPosef &pose)
    {
        if (!pending.active || eye >= 2 || (pending.snapshot.eyeMask >> eye & 1) || !lastDrawnValid[eye]) return false;
        const auto &q = pose.orientation;const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (!std::isfinite(n) || std::abs(n - 1.f) > 1e-3f || !std::isfinite(pose.position.x) || !std::isfinite(pose.position.y) || !std::isfinite(pose.position.z)) return false;
        staleOverride[eye] = true;stalePose[eye] = pose;return true;
    }
    bool beginFrame(const Prepare &prepare = {}) { return beginFrameContent(prepare,false); }
    // Upstream must explicitly identify a flat presentation. No missing eye or
    // render failure automatically changes a world frame into this mode.
    bool beginScreenFrame() { return beginFrameContent({},true); }
  private:
    bool beginFrameContent(const Prepare &prepare,bool screenOnly)
    {
        if (pending.active) throw std::logic_error("previous OpenXR frame is still pending");
        if (uiPending.acquired) throw std::logic_error("unresolved UI acquisition requires session teardown");
        if (!poll() || !running)
            return false;
        XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState fs{XR_TYPE_FRAME_STATE};
        xrCheck(xrWaitFrame(session, &wi, &fs), "xrWaitFrame");
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
        xrCheck(xrBeginFrame(session, &bi), "xrBeginFrame");
        frames++;
        pending = {};staleOverride = {};
        pending.active = true;
        pending.snapshot.frame = frames;
        pending.snapshot.screenOnly = screenOnly;
        pending.snapshot.predictedDisplayTime = fs.predictedDisplayTime;
        auto &views = pending.snapshot.views;
        XrViewState vs{XR_TYPE_VIEW_STATE};
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
        li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime = fs.predictedDisplayTime;
        li.space = local;
        uint32_t n = 0;
        auto &acquired = pending.acquired;
        auto &ready = pending.ready;
        auto &indices = pending.snapshot.imageIndices;
        try
        {
            xrCheck(xrLocateViews(session, &li, &vs, 2, &n, views.data()), "xrLocateViews");
            const XrViewStateFlags valid =
                XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
            headLocation = {XR_TYPE_SPACE_LOCATION};
            xrCheck(xrLocateSpace(headSpace, local, fs.predictedDisplayTime, &headLocation),
                    "xrLocateSpace HMD");
            if ((headLocation.locationFlags &
                 (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT)) == 3)
            {
                auto h = headLocation.pose;
                movementYaw = heading(h.orientation, movementYaw);
                auto aim = rotate(h.orientation, {0, 0, -1});
                std::ostringstream hd;
                hd << std::setprecision(10) << "\"frame\":" << frames << ",\"pose_float_digits\":10,\"position\":[" << h.position.x << ',' << h.position.y
                   << ',' << h.position.z << "],\"orientation\":[" << h.orientation.x << ','
                   << h.orientation.y << ',' << h.orientation.z << ',' << h.orientation.w
                   << "],\"head_forward\":[" << aim.x << ',' << aim.y << ',' << aim.z
                   << "],\"movement_yaw\":" << movementYaw << ",\"mode\":" << jsonString(diagnosticCameraMode);
                log.event("hmd", hd.str(), true);
            }
            if (!fs.shouldRender || n != 2 || (vs.viewStateFlags & valid) != valid)
            {
                emptyFrames++;
                log.event("empty_frame", "\"frame\":" + std::to_string(frames) +
                                             ",\"reason\":\"not_visible_or_tracking_invalid\"");
                return true;
            }
            pending.snapshot.shouldRender = true;
            if (!crop.decided) decideFovCrop(views);
            if (symmetricFov && cantedFov && cant.decided && !cant.masked && !cant.cacheWritten && !cant.lateGaveUp && xrGetVisibilityMaskKHR &&
                fovCropRequested() && frames % 30 == 0)
            {
                if (frames <= 3000) lateMask(views);
                else { cant.lateGaveUp = true; log.event("canted_fov_mask_never", "\"frames\":" + std::to_string(frames) + ",\"why\":" + jsonString(cant.lateWhy)); }
            }
            if (crop.active)
                for (uint32_t eye = 0; eye < 2; ++eye) views[eye].fov = fovcrop::apply(views[eye].fov, crop.rect[eye]);
            if (symmetricFov) {
             std::array<XrFovf,2> runtimes{views[0].fov,views[1].fov},renders{};
             for(uint32_t eye=0;eye<2;++eye) {
                const auto runtime=views[eye].fov;
                XrFovf render{};
                if(cantedFov){   // turned toward the middle of the runtime's field (cantedEnvelope); pose and fov together
                    const auto& first=cant.runtime[eye];
                    const bool same=cant.masked&&std::abs(runtime.angleLeft-first.angleLeft)<1e-5f&&std::abs(runtime.angleRight-first.angleRight)<1e-5f&&
                        std::abs(runtime.angleUp-first.angleUp)<1e-5f&&std::abs(runtime.angleDown-first.angleDown)<1e-5f;
                    const auto turned=same?cant.turned[eye]:cantedEnvelope(runtime);
                    views[eye].pose.orientation=normalized(multiply(views[eye].pose.orientation,turned.turn));
                    render=turned.fov;
                }else render=symmetricEnvelope(runtime);
                renders[eye]=render;
             }
             // One field for both eyes (2026-10-20, the user's 10-19 report "the trees change their detail a step apart in the
             // two eyes"): each eye's symmetric field differs a little (the user's headset: 0.8920 / 0.9037 and 0.8955 / 0.8992 rad
             // across / up, the Mock 0.9585 / 0.9251 and 0.9503 / 0.9332), and what the game sizes by the picture - the detail of
             // a model by its size on the screen (guess) - then changes at another distance in each eye. Both eyes take the
             // larger angle of each direction: the lenses' areas stay inside, a little under 1 % more field in one direction.
             // EVR_EQUAL_EYE_FOV=0: each eye its own as before.
             static const bool equalFov=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EQUAL_EYE_FOV",v,sizeof v);return !(n==1&&v[0]=='0');}();
             if(equalFov){
                const float side=std::max({std::abs(renders[0].angleLeft),renders[0].angleRight,std::abs(renders[1].angleLeft),renders[1].angleRight});
                const float up=std::max({renders[0].angleUp,std::abs(renders[0].angleDown),renders[1].angleUp,std::abs(renders[1].angleDown)});
                for(auto& r:renders){r.angleLeft=-side;r.angleRight=side;r.angleUp=up;r.angleDown=-up;}
             }
             for(uint32_t eye=0;eye<2;++eye) {
                const auto runtime=runtimes[eye];const auto render=renders[eye];
                views[eye].fov=render; // same snapshot reaches game cameras and xrEndFrame
                if(frames<=3||frames%300==0){
                    const auto a=fovcrop::fromFov(runtime),b=fovcrop::fromFov(render);
                    std::ostringstream f;f<<std::setprecision(10)<<"\"frame\":"<<frames<<",\"eye\":"<<eye
                     <<",\"runtime_fov\":["<<runtime.angleLeft<<','<<runtime.angleRight<<','<<runtime.angleUp<<','<<runtime.angleDown<<']'
                     <<",\"render_fov\":["<<render.angleLeft<<','<<render.angleRight<<','<<render.angleUp<<','<<render.angleDown<<']'
                     <<",\"pixel_density_ratio\":["<<a.width()/b.width()<<','<<a.height()/b.height()
                     <<"],\"pose_unchanged\":"<<(cantedFov?"false":"true")<<",\"extent\":["<<targets[eye].width<<','<<targets[eye].height<<']';
                    log.event("symmetric_render_fov",f.str()+",\"equal_fov\":"+(equalFov?"true":"false"));
                }
             }
            }
            if (prepare)
                prepare(views, vs, headLocation, fs.predictedDisplayTime, frames);
            if (screenOnly)return true;
            {
                uint32_t mask = nextEyeMask;
                for (uint32_t eye = 0; eye < 2; eye++)
                    if (!(mask >> eye & 1) && !lastDrawnValid[eye]) mask = 3;
                pending.snapshot.eyeMask = mask;
            }
            nextEyeMask = 3;
            const uint32_t warp = warpMask; warpMask = 0;
            for (uint32_t eye = 0; eye < 2; eye++)
            {
                const bool drawn = pending.snapshot.eyeMask >> eye & 1;
                pending.warpable[eye] = !drawn && (warp >> eye & 1);
                if (!drawn && !pending.warpable[eye]) continue;
                XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                xrCheck(xrAcquireSwapchainImage(targets[eye].handle, &a, &indices[eye]),
                        "xrAcquireSwapchainImage");
                acquired[eye] = true;
                XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                w.timeout = 1000000000;
                auto result = xrWaitSwapchainImage(targets[eye].handle, &w);
                if (result != XR_SUCCESS)
                    throw std::runtime_error("xrWaitSwapchainImage timeout/error " + std::to_string(result));
                ready[eye] = true;
            }
            return true;
        }
        catch (...)
        {
            abortFrame();
            throw;
        }
    }
  public:
    // Call only after that eye's graphics work has been submitted with the
    // required ordering/completion for XR image release. CPU command recording
    // alone is not completion. This API does not infer game GPU fence state.
    void markEyeRendered(uint32_t eye, uint64_t expectedFrame)
    {
        if (!pending.active || pending.snapshot.frame != expectedFrame || !pending.snapshot.shouldRender || eye >= 2 || !pending.ready[eye] || pending.rendered[eye])
            throw std::logic_error("invalid/duplicate OpenXR eye completion");
        pending.rendered[eye] = true;
        ++draws[eye];
    }
    void finishFrame()
    {
        if (!pending.active) throw std::logic_error("no pending OpenXR frame to finish");
        const auto &snapshot = pending.snapshot;
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = snapshot.predictedDisplayTime;
        end.environmentBlendMode = blend;
        try
        {
            if (!snapshot.shouldRender)
            {
                pending.endAttempted = true;
                xrCheck(xrEndFrame(session, &end), "xrEndFrame empty");
                log.event("empty_frame_completed", "\"frame\":"+std::to_string(snapshot.frame)+
                    ",\"predicted_time\":"+std::to_string(snapshot.predictedDisplayTime)+",\"layers\":0");
                pending = {};
                return;
            }
            if (pending.uiAttempted && (!uiPending.ready || !uiPending.rendered || uiPending.frame!=snapshot.frame))
                throw std::logic_error("cannot submit unfinished UI frame");
            if (snapshot.screenOnly) {
                if (!pending.uiAttempted)throw std::logic_error("cannot submit screen without current content");
            } else for (uint32_t eye = 0; eye < 2; eye++)
                if ((snapshot.eyeMask >> eye & 1) || pending.warped[eye] ? !pending.rendered[eye] : !lastDrawnValid[eye])
                    throw std::logic_error("cannot submit incomplete stereo frame");
            std::array<XrCompositionLayerProjectionView, 2> pv{
                {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
            if (!snapshot.screenOnly)for (uint32_t eye = 0; eye < 2; eye++)
            {
                const bool drawn = snapshot.eyeMask >> eye & 1;
                // A warped picture (markEyeWarped) is shown at this frame's pose like a drawn one; an eye left out
                // without one: its last drawn picture, at the pose and fov it was drawn with.
                const bool shown = drawn || pending.warped[eye];
                pv[eye].pose = shown ? snapshot.views[eye].pose : staleOverride[eye] ? stalePose[eye] : lastDrawn[eye].pose;
                pv[eye].fov = shown ? snapshot.views[eye].fov : lastDrawn[eye].fov;
                pv[eye].subImage.swapchain = targets[eye].handle;
                pv[eye].subImage.imageRect.extent = {int32_t(targets[eye].width),
                                                     int32_t(targets[eye].height)};
                if (!shown)
                {
                    // Acquired for a warp that did not come: released unused (its contents are shown once).
                    if (pending.acquired[eye])
                    {
                        XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                        xrCheck(xrReleaseSwapchainImage(targets[eye].handle, &r), "xrReleaseSwapchainImage unused warp image");
                        pending.acquired[eye] = pending.ready[eye] = false;
                        log.event("eye_warp_image_unused", "\"frame\":" + std::to_string(snapshot.frame) + ",\"eye\":" + std::to_string(eye));
                    }
                    continue;
                }
                XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                xrCheck(xrReleaseSwapchainImage(targets[eye].handle, &r), "xrReleaseSwapchainImage");
                pending.acquired[eye] = pending.ready[eye] = false;
                if (!drawn) continue;   // the warped picture is not a drawn one: the kept picture stays the last drawn
                lastDrawn[eye] = snapshot.views[eye];lastDrawnValid[eye] = true;
            }
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            layer.space = local;
            layer.viewCount = 2;
            layer.views = pv.data();
            XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
            std::array<XrCompositionLayerQuad,12> partQuads{};
            std::array<const XrCompositionLayerBaseHeader*,14> layers{reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
            end.layerCount=snapshot.screenOnly?0:1;end.layers=layers.data();
            if (pending.uiAttempted && uiPending.placement.partCount) {
                // The UI image as several quads (the plate around the aim cluster, and the cluster at the aimed point).
                for (uint32_t i = 0; i < uiPending.placement.partCount; ++i) {
                    const auto &part = uiPending.placement.parts[i];auto &q = partQuads[i];
                    q = XrCompositionLayerQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
                    q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    q.space=part.localSpace?local:headSpace;q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
                    q.subImage.swapchain=uiTarget.handle;
                    q.subImage.imageRect.offset={part.x,part.y};q.subImage.imageRect.extent={part.pixelsX,part.pixelsY};
                    q.pose=part.pose;q.size={part.width,part.height};
                    layers[end.layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
                }
                quad.size={uiPending.placement.width,uiPending.placement.width*float(uiTarget.height)/float(uiTarget.width)};
                quad.pose=identityPose();quad.pose.position.z=-uiPending.placement.distance;
                releaseUi();
            } else if (pending.uiAttempted) {
                quad.layerFlags=uiPending.placement.opaque?0:XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                quad.space=uiPending.placement.localSpace?local:headSpace;quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
                quad.subImage.swapchain=uiTarget.handle;
                quad.subImage.imageRect.extent={int32_t(uiTarget.width),int32_t(uiTarget.height)};
                quad.pose=identityPose();quad.pose.position.z=-uiPending.placement.distance;
                if(uiPending.placement.localSpace)quad.pose=compose(uiPending.placement.origin,quad.pose);
                quad.size={uiPending.placement.width,uiPending.placement.width*float(uiTarget.height)/float(uiTarget.width)};
                releaseUi();
                layers[end.layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
            }
            pending.endAttempted = true;staleOverride = {};
            xrCheck(xrEndFrame(session, &end), "xrEndFrame");
            if(pending.uiAttempted)log.event("ui_quad_submitted","\"frame\":"+std::to_string(snapshot.frame)+",\"layers\":"+std::to_string(end.layerCount)+
                ",\"premultiplied\":true,\"parts\":"+std::to_string(uiPending.placement.partCount)+",\"space\":"+jsonString(uiPending.placement.localSpace?"LOCAL":"VIEW")+
                ",\"distance_m\":"+std::to_string(uiPending.placement.distance)+",\"width_m\":"+std::to_string(quad.size.width)+
                ",\"position\":["+std::to_string(quad.pose.position.x)+","+std::to_string(quad.pose.position.y)+","+std::to_string(quad.pose.position.z)+"]");
            if(snapshot.screenOnly)log.event("screen_frame","\"frame\":"+std::to_string(snapshot.frame)+
                ",\"predicted_time\":"+std::to_string(snapshot.predictedDisplayTime)+",\"layers\":1,\"world_eyes_rendered\":0");
            else
            log.event("stereo_frame",
                      "\"frame\":" + std::to_string(frames) +
                          ",\"predicted_time\":" + std::to_string(snapshot.predictedDisplayTime) +
                          ",\"left_draws\":" + std::to_string(draws[0]) +
                          ",\"right_draws\":" + std::to_string(draws[1]) +
                          ",\"eyes\":" + std::to_string(snapshot.eyeMask),
                      true);
            pending = {};
        }
        catch (...)
        {
            abortFrame();
            throw;
        }
    }
    // A world frame whose eyes are not drawn (the application deferred it) shows the flat picture instead of
    // nothing: the eye images that were acquired go back unused, and the frame is completed as a screen frame
    // (configureUi, acquireUi, markUiRendered, finishFrame).
    void screenInsteadOfEyes()
    {
        if (!pending.active || !pending.snapshot.shouldRender || pending.snapshot.screenOnly || pending.rendered[0] ||
            pending.rendered[1] || pending.uiAttempted)
            throw std::logic_error("this frame cannot become a screen frame");
        try
        {
            for (uint32_t eye = 0; eye < 2; ++eye)
            {
                if (pending.acquired[eye] && !pending.ready[eye])
                {
                    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout = 100000000;
                    xrCheck(xrWaitSwapchainImage(targets[eye].handle, &wait), "screen xrWaitSwapchainImage");
                    pending.ready[eye] = true;
                }
                if (pending.ready[eye])
                {
                    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    lastDrawnValid[eye] = false;
                    xrCheck(xrReleaseSwapchainImage(targets[eye].handle, &release), "screen xrReleaseSwapchainImage");
                    pending.ready[eye] = pending.acquired[eye] = false;
                }
            }
            pending.snapshot.screenOnly = true;
        }
        catch (...) { abortFrame("screen_instead_of_eyes_failure");throw; }
    }
    void skipFrame(const char *reason)
    {
        const auto snapshot = pendingFrame();
        if (pending.rendered[0] || pending.rendered[1] || (pending.uiAttempted && uiPending.rendered))
            throw std::logic_error("cannot skip a frame after marking an XR eye rendered");
        try
        {
            releaseUi();
            for (uint32_t eye = 0; eye < 2; ++eye)
            {
                if (pending.acquired[eye] && !pending.ready[eye])
                {
                    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout = 100000000;
                    xrCheck(xrWaitSwapchainImage(targets[eye].handle, &wait), "skip xrWaitSwapchainImage");
                    pending.ready[eye] = true;
                }
                if (pending.ready[eye])
                {
                    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    lastDrawnValid[eye] = false;
                    xrCheck(xrReleaseSwapchainImage(targets[eye].handle, &release), "skip xrReleaseSwapchainImage");
                    pending.ready[eye] = pending.acquired[eye] = false;
                }
            }
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
            end.displayTime = snapshot.predictedDisplayTime;end.environmentBlendMode = blend;
            pending.endAttempted = true;
            xrCheck(xrEndFrame(session, &end), "skip xrEndFrame");
            pending = {};++emptyFrames;
            log.event("empty_frame", "\"frame\":" + std::to_string(snapshot.frame) +
                ",\"predicted_time\":" + std::to_string(snapshot.predictedDisplayTime) +
                ",\"reason\":" + jsonString(reason) + ",\"application_deferred\":true");
        }
        catch (...) { abortFrame("skip_frame_failure");throw; }
    }
    void abortFrame(const char *reason = "frame_failure") noexcept
    {
        if (!pending.active) return;
        try { releaseUi(); } catch (const std::exception& e) {
            try { log.event("ui_cleanup_deferred","\"reason\":"+jsonString(e.what())); } catch (...) {}
        }
        for (int eye = 0; eye < 2; eye++)
        {
            if (pending.acquired[eye] && !pending.ready[eye])
            {
                XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                w.timeout = 100000000;
                pending.ready[eye] = xrWaitSwapchainImage(targets[eye].handle, &w) == XR_SUCCESS;
            }
            if (pending.ready[eye])
            {
                XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                lastDrawnValid[eye] = false;
                xrReleaseSwapchainImage(targets[eye].handle, &r);
            }
        }
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = pending.snapshot.predictedDisplayTime;
        end.environmentBlendMode = blend;
        if (!pending.endAttempted) xrEndFrame(session, &end);
        pending = {};
        try { log.event("fallback", "\"reason\":" + jsonString(reason)); } catch (...) {}
    }
    bool frame(const Render &render, const Prepare &prepare = {})
    {
        if (!beginFrame(prepare)) return false;
        try
        {
            const auto &snapshot = pendingFrame();
            if (snapshot.shouldRender)
                for (uint32_t eye = 0; eye < 2; ++eye)
                {
                    render(eye, snapshot.imageIndices[eye], snapshot.views[eye], snapshot.predictedDisplayTime, snapshot.frame);
                    markEyeRendered(eye, snapshot.frame);
                }
            finishFrame();
        }
        catch (...)
        {
            abortFrame();
            throw;
        }
        return true;
    }
    void shutdown() noexcept
    {
        if (!instance)
            return;
        if (vk.device && vk.vkDeviceWaitIdle)
            vk.vkDeviceWaitIdle(vk.device);
        abortFrame("shutdown_pending_frame");
        if (session && running && !exit)
        {
            if (XR_SUCCEEDED(xrRequestExitSession(session)))
            {
                for (int i = 0; i < 100 && running; i++)
                {
                    try
                    {
                        poll();
                    }
                    catch (...)
                    {
                        break;
                    }
                    if (running)
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
        }
        if(uiTarget.handle) {
            try { releaseUi(); } catch (...) {}
            // xrDestroySwapchain permits outstanding acquisitions after submitted GPU work completes.
            const auto r=xrDestroySwapchain(uiTarget.handle);
            try { log.event(XR_FAILED(r)?"cleanup_error":"ui_swapchain_destroyed","\"operation\":\"xrDestroySwapchain UI\",\"code\":"+std::to_string(r)); } catch (...) {}
            uiTarget={};uiPending={};uiFormat=VK_FORMAT_UNDEFINED;
        }
        for (auto &t : targets)
        {
            if (t.handle)
            {
                auto r = xrDestroySwapchain(t.handle);
                if (XR_FAILED(r))
                    try
                    {
                        log.event("cleanup_error",
                                  "\"operation\":\"xrDestroySwapchain\",\"code\":" + std::to_string(r));
                    }
                    catch (...)
                    {
                    }
                t.handle = XR_NULL_HANDLE;
            }
            t.images.clear();
        }
        if (headSpace)
        {
            xrDestroySpace(headSpace);
            headSpace = XR_NULL_HANDLE;
        }
        if (local)
        {
            xrDestroySpace(local);
            local = XR_NULL_HANDLE;
        }
        if (session)
        {
            xrDestroySession(session);
            session = XR_NULL_HANDLE;
        }
        if (ownsXrInstance && xrDestroyInstance)
            xrDestroyInstance(instance);
        instance = XR_NULL_HANDLE;
    }
};
} // namespace evr
