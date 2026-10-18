#include <evr/config.hpp>
#include <evr/math.hpp>
#include <evr/signature.hpp>
#include <evr/world_position.hpp>
#include <functional>
#include <iostream>
#include <limits>
using namespace evr;
int failures = 0, passed = 0;
void check(bool b, const char *name)
{
    std::cout << (b ? "PASS " : "FAIL ") << name << '\n';
    if (b)
        passed++;
    else
        failures++;
}
bool near(float a, float b, float eps = 1e-4f)
{
    return std::abs(a - b) < eps;
}
bool near(Vec3 a, Vec3 b)
{
    return length(a - b) < 1e-4f;
}
void rejects(const std::function<void()> &f, const char *name)
{
    try
    {
        f();
        check(false, name);
    }
    catch (const std::invalid_argument &)
    {
        check(true, name);
    }
}
int main()
{
    {
        WorldPositionQ32 root{int64_t(1000000000) * 4294967296ll + 37, -9223372036854770000ll, 0};
        auto camera = cameraPosition(root, {.003f, .01f, -.02f}, 1);
        check(std::abs(relativeQ32(camera.x, root.x) - .003) < 1e-8,
              "3 mm camera offset preserved at large world coordinate");
        check(root.x == int64_t(1000000000) * 4294967296ll + 37,
              "Q32 camera offset leaves player coordinate untouched");
        check(std::abs(relativeQ32(camera.z, root.z) + .02) < 1e-8, "negative camera offset preserved");
        check(relativeQ32(std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::min()) > 4e9,
              "Q32 subtraction avoids signed overflow");
        rejects([] { addQ32(std::numeric_limits<int64_t>::max(), .01); }, "Q32 positive overflow rejected");
        rejects([] { addQ32(std::numeric_limits<int64_t>::min(), -.01); }, "Q32 negative overflow rejected");
        rejects([] { addQ32(0, std::numeric_limits<double>::infinity()); }, "Q32 infinite offset rejected");
    }
    const uint8_t bytes[] = {0x48, 0x8d, 0x10, 0x48, 0x8d, 0x20};
    check(scan(bytes, sizeof(bytes), parsePattern("48 8D ??")) == std::vector<size_t>({0, 3}),
          "signature wildcards and final boundary");
    check(scan(bytes, 1, parsePattern("48 8D")).empty(), "signature bounds exclude short section");
    rejects([] { parsePattern("?? ??"); }, "all-wildcard signature rejected");
    rejects([] { parsePattern("48 ZZ"); }, "malformed signature rejected");
    bool ambiguous = false;
    try
    {
        uniqueMatch({1, 2});
    }
    catch (const std::runtime_error &)
    {
        ambiguous = true;
    }
    check(ambiguous, "ambiguous signature disables hook");
    {
        std::ofstream out("core-test.ini");
        out << "Enabled=false\nDebugMode=true\nRenderScale=1.25\n";
    }
    auto config = Config::read("core-test.ini");
    check(!config.enabled && config.debug && std::abs(config.renderScale - 1.25f) < 1e-6f,
          "configuration consumed with safe default");
    {
        std::ofstream out("core-test.ini");
        out << "RenderScale=nan\n";
    }
    rejects([] { Config::read("core-test.ini"); }, "NaN configuration rejected");
    {
        std::ofstream out("core-test.ini");
        out << "RenderScale=1\nRenderScale=2\n";
    }
    rejects([] { Config::read("core-test.ini"); }, "duplicate configuration rejected");
    const XrFovf f{-.8f, .9f, .75f, -.85f};
    auto p = projection(f, .05f, 1000);
    check(near(point(p, {0, 0, -.05f}).z, 0), "Vulkan near depth is zero");
    check(near(point(p, {0, 0, -1000}).z, 1), "Vulkan far depth is one");
    check(near(point(p, {std::tan(f.angleLeft), 0, -1}).x, -1) &&
              near(point(p, {std::tan(f.angleRight), 0, -1}).x, 1),
          "asymmetric horizontal FOV boundaries");
    check(near(point(p, {0, std::tan(f.angleUp), -1}).y, -1) &&
              near(point(p, {0, std::tan(f.angleDown), -1}).y, 1),
          "asymmetric vertical FOV boundaries");
    Pose head{euler(.7f, .3f, -.2f), {3, 4, 5}};
    check(near(point(view(head), head.position), {0, 0, 0}), "view matrix inverts eye pose");
    check(near(rotate(head.orientation, rotate(inverse(head.orientation), {1, 2, 3})), {1, 2, 3}),
          "quaternion inverse");
    check(near(movement(0, 1, 0), {0, 0, -1}), "forward movement");
    check(near(movement(0, 1, -pi / 2), {1, 0, 0}), "right 90 degree movement");
    check(near(movement(0, 1, pi / 2), {-1, 0, 0}), "left 90 degree movement");
    check(near(movement(0, 1, pi), {0, 0, 1}), "180 degree movement");
    for (float pitch : {-1.5f, -.7f, 0.f, .7f, 1.5f})
        for (float roll : {-1.f, 0.f, 1.f})
            check(near(heading(euler(.8f, pitch, roll), 0), .8f), "pitch/roll do not change heading");
    check(near(heading(euler(.8f, -pi / 2, 1), .8f), .8f), "straight down retains heading");
    check(near(heading(euler(.8f, pi / 2, -1), .8f), .8f), "straight up retains heading");
    TrackingOrigin origin;
    Vec3 root{100, 20, 30};
    auto before = root;
    origin.recenter(head);
    auto cam = origin.camera(root, 1.7f, 0, head);
    check(near(cam.position, root + Vec3{0, 1.7f, 0}), "recenter aligns physical head to stable eye anchor");
    head.position.x += .003f;
    auto cam2 = origin.camera(root, 1.7f, 0, head);
    check(near(length(cam2.position - cam.position), .003f), "3 mm HMD motion is preserved without deadzone");
    check(near(root, before), "physical HMD movement never mutates player root");
    auto cam3 = origin.camera(root + Vec3{2, 3, 4}, 1.7f, 0, head);
    check(near(cam3.position - cam2.position, {2, 3, 4}), "actual root displacement preserved exactly");
    Pose l{{0, 0, 0, 1}, {-.032f, 0, 0}}, r{{0, 0, 0, 1}, {.032f, 0, 0}};
    check(
        near(length(origin.camera(root, 1.7f, .3f, r).position - origin.camera(root, 1.7f, .3f, l).position),
             .064f),
        "eye separation preserved through origin transforms");
    check(overrideAim(GameMode::Combat, true) && !overrideAim(GameMode::Building, true) &&
              !overrideAim(GameMode::Unknown, true) && !overrideAim(GameMode::Combat, false),
          "Head Aim mode and tracking gate");
    for (auto m : {GameMode::UI, GameMode::Inventory, GameMode::ItemInspection, GameMode::Building,
                   GameMode::Special, GameMode::Unknown})
        check(!overrideMovement(m, true) && !overrideAim(m, true), "preserve game UI/special operation");
    check(near(rotate(euler(0, .5f, .3f), {0, 0, -1}).y, std::sin(.5f)), "Head Aim includes pitch");
    check(!visibleInPass(true, true, false, MeshPart::Head) &&
              visibleInPass(true, true, true, MeshPart::Head) &&
              visibleInPass(false, true, false, MeshPart::Head),
          "head hidden only in local VR eye passes");
    check(visibleInPass(true, true, false, MeshPart::Body) &&
              visibleInPass(true, true, false, MeshPart::Arms) &&
              visibleInPass(true, true, false, MeshPart::Legs),
          "body limbs retained");
    check(scaledExtent(1234, 1567, 1, 4096, 4096) == std::array<uint32_t, 2>{1234, 1567},
          "RenderScale 1 uses runtime recommendation");
    check(scaledExtent(1000, 1500, 1.2f, 4096, 4096) == std::array<uint32_t, 2>{1200, 1800},
          "RenderScale multiplies each runtime dimension");
    rejects([] { scaledExtent(1000, 1000, 5, 4096, 4096); }, "reject oversized extent");
    rejects([] { scaledExtent(1000, 1000, std::numeric_limits<float>::quiet_NaN(), 4096, 4096); },
            "reject NaN render scale");
    rejects([] { projection({-.8f, .8f, .8f, -.8f}, 0, 100); }, "reject invalid clip range");
    rejects([] { normalized({0, 0, 0, 0}); }, "reject invalid quaternion");
    std::cout << "RESULT " << passed << " PASS " << failures << " FAIL\n";
    return failures ? 1 : 0;
}
