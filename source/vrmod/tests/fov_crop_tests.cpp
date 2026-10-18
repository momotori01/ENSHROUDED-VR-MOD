#include <evr/fov_crop.hpp>
#include <evr/symmetric_fov.hpp>
#include <iostream>
using namespace evr::fovcrop;
static std::vector<XrVector2f> ellipse(const TanRect &fov, float scale, int segments, bool centre)
{
    std::vector<XrVector2f> v;
    if (centre) v.push_back({0, 0});
    for (int i = 0; i < segments; ++i)
    {
        const float a = 6.2831853f * i / segments, c = std::cos(a), s = std::sin(a);
        v.push_back({(c > 0 ? fov.right : -fov.left) * scale * c, (s > 0 ? fov.up : -fov.down) * scale * s});
    }
    return v;
}
int main()
{
    unsigned passed = 0, failed = 0;
    auto check = [&](bool yes, const char *name) { (yes ? passed : failed)++; std::cout << (yes ? "PASS " : "FAIL ") << name << '\n'; };
    const XrFovf runtimeFov{-.88f, .82f, .8f, -.85f};
    const auto fov = fromFov(runtimeFov);
    // Visible triangle fan of an ellipse at 90 % of each tangent half-extent.
    const auto fan = ellipse(fov, .9f, 64, true);
    std::vector<uint32_t> fanIndices;
    for (uint32_t i = 1; i <= 64; ++i) { fanIndices.push_back(0); fanIndices.push_back(i); fanIndices.push_back(i % 64 + 1); }
    const auto visible = visibleBounds(XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, fan, fanIndices, fov);
    check(visible.valid() && std::abs(visible.right - fov.right * .9f) < 1e-4f && std::abs(visible.down - fov.down * .9f) < 1e-4f,
          "visible mesh bounds are the vertex bounding box");
    // The same lens as a line loop.
    const auto loop = ellipse(fov, .9f, 64, false);
    std::vector<uint32_t> loopIndices;
    for (uint32_t i = 0; i < 64; ++i) loopIndices.push_back(i);
    const auto loopBox = visibleBounds(XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR, loop, loopIndices, fov);
    check(std::abs(loopBox.left - visible.left) < 1e-5f && std::abs(loopBox.up - visible.up) < 1e-5f, "line loop gives the same bounds");
    // Hidden mesh: four corner triangles and a 10 % band on the right edge.
    const float bandX = fov.right - .1f * fov.width();
    std::vector<XrVector2f> hidden{{fov.left, fov.up}, {fov.left + .3f, fov.up}, {fov.left, fov.up - .3f},
                                   {bandX, fov.up}, {fov.right, fov.up}, {fov.right, fov.down}, {bandX, fov.down}};
    std::vector<uint32_t> hiddenIndices{0, 1, 2, 3, 4, 5, 3, 5, 6};
    const auto sampled = visibleBounds(XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, hidden, hiddenIndices, fov);
    check(sampled.valid() && std::abs(sampled.right - bandX) < 2 * fov.width() / 256 && std::abs(sampled.left - fov.left) < 1e-5f &&
              std::abs(sampled.up - fov.up) < 1e-5f,
          "hidden mesh bounds come from grid sampling (corner triangle does not shrink the box)");
    // Margin widens by angle and never beyond the runtime field of view.
    const auto crop = cropRect(fov, visible, 1.f);
    check(std::abs(std::atan(crop.right) - std::atan(visible.right) - 3.14159265f / 180) < 1e-4f, "one degree margin is angular");
    check(cropRect(fov, fov, 5.f).right == fov.right && cropRect(fov, fov, 5.f).down == fov.down, "margin never exceeds the runtime fov");
    uint32_t w = 0, h = 0;
    croppedExtent(fov, crop, 2064, 2208, w, h);
    check(w < 2064 && h < 2208 && std::abs(double(w) / 2064 - crop.width() / fov.width()) < 1e-3, "extent keeps pixels per tangent");
    croppedExtent(fov, fov, 2064, 2208, w, h);
    check(w == 2064 && h == 2208, "full fov keeps the runtime extent");
    // Plausibility: empty, off-centre or tiny masks are rejected.
    check(plausible(fov, visible), "lens-shaped mask accepted");
    check(!plausible(fov, TanRect{}), "empty mask rejected");
    check(!plausible(fov, TanRect{.1f, .7f, .7f, -.7f}), "mask not containing the centre rejected");
    check(!plausible(fov, TanRect{-.3f, .3f, .3f, -.3f}), "mask covering under 60 % rejected");
    check(!visibleBounds(XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, fan, {0, 1, 99}, fov).valid(), "out-of-range index rejected");
    // Per-frame fov: the fixed crop intersected with the current fov, in angles.
    const auto frameFov = apply(runtimeFov, crop);
    check(frameFov.angleRight < runtimeFov.angleRight && frameFov.angleLeft > runtimeFov.angleLeft &&
              std::abs(frameFov.angleRight - std::atan(crop.right)) < 1e-6f,
          "frame fov uses the crop tangents");
    const XrFovf narrower{-.5f, .5f, .5f, -.5f};
    const auto kept = apply(narrower, crop);
    check(kept.angleLeft == narrower.angleLeft && kept.angleUp == narrower.angleUp, "a narrower frame fov is never widened");
    for(const auto source:{XrFovf{-.958523333f,.735325396f,.769307733f,-.925051451f},
                           XrFovf{-.741855681f,.950275421f,.759112358f,-.933196187f},XrFovf{-.8f,.8f,.8f,-.8f}}){
        const auto symmetric=evr::symmetricEnvelope(source);const auto a=fromFov(source),b=fromFov(symmetric);
        check(symmetric.angleLeft==-symmetric.angleRight&&symmetric.angleDown==-symmetric.angleUp,"render frustum is centered on both axes");
        check(b.left<=a.left&&b.right>=a.right&&b.up>=a.up&&b.down<=a.down,"symmetric frustum contains every runtime ray");
        bool sameRays=true;
        for(unsigned i=0;i<=10;++i)for(unsigned j=0;j<=10;++j){
            const float x=a.left+a.width()*i/10,y=a.down+a.height()*j/10;
            const float u=(x-b.left)/b.width(),v=(y-b.down)/b.height();
            sameRays&=u>=-1e-6f&&u<=1.000001f&&v>=-1e-6f&&v<=1.000001f&&std::abs(b.left+b.width()*u-x)<1e-6f&&std::abs(b.down+b.height()*v-y)<1e-6f;
        }
        check(sameRays,"display remapping preserves all sampled physical rays");
    }
    try{evr::symmetricEnvelope({.9f,.8f,.8f,-.8f});check(false,"invalid runtime FOV rejected");}
    catch(const std::invalid_argument&){check(true,"invalid runtime FOV rejected");}
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
