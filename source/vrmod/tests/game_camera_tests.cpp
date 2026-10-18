#include <evr/game_camera_math.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace evr;
static int failures = 0, passed = 0;
void check(bool ok, const char *name)
{
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    ok ? ++passed : ++failures;
}
Vec3 projected(game::MatrixRows matrix, Vec3 p)
{
    const auto &m = matrix.values;
    float w = p.x * m[3] + p.y * m[7] + p.z * m[11] + m[15];
    return {(p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12]) / w,
            (p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13]) / w,
            (p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14]) / w};
}
bool near(Vec3 a, Vec3 b, float tolerance = 1e-5f)
{
    return length(a - b) < tolerance;
}
template <typename T> void array(std::ostream &out, const T &values)
{
    out << '[';
    bool first = true;
    for (auto value : values)
    {
        if (!first)
            out << ',';
        out << value;
        first = false;
    }
    out << ']';
}
int main(int argc, char **argv)
{
    auto q = euler(.7f, .4f, -.3f);
    auto converted = game::trackingPose({q, {.03f, .11f, -.21f}});
    check(near(converted.position, {.03f, .11f, .21f}),
          "tracking preserves all positional motion and left/right");
    check(near(rotate(converted.orientation, {0, 0, 1}), game::trackingVector(rotate(q, {0, 0, -1}))),
          "head aim basis conversion");
    check(near(rotate(converted.orientation, {1, 0, 0}), game::trackingVector(rotate(q, {1, 0, 0}))),
          "eye baseline basis conversion");
    check(near(game::trackingPose(converted).position, {.03f, .11f, -.21f}), "tracking basis roundtrip");
    XrFovf f{-.7f, .9f, .8f, -.6f};
    auto p = game::projection(f, .05f, 500);
    check(near(projected(p, {0, 0, .05f}), {p.values[8], p.values[9], 1}),
          "game reverse depth near equals one");
    check(std::abs(projected(p, {0, 0, 500}).z) < 1e-6, "game reverse depth far equals zero");
    check(near(projected(p, {.05f * std::tan(f.angleLeft), .05f * std::tan(f.angleDown), .05f}), {-1, -1, 1}),
          "asymmetric bottom left");
    check(near(projected(p, {.05f * std::tan(f.angleRight), .05f * std::tan(f.angleUp), .05f}), {1, 1, 1}),
          "asymmetric top right");
    WorldPositionQ32 origin{int64_t(1e9 * 4294967296.), 0, 0};
    auto camera = cameraPosition(origin, {.003f, 1.6f, 0}, 1);
    auto v = game::cameraRelativeView(camera, {0, 0, 0, 1}, origin);
    check(near(projected(v, {.003f, 1.6f, 0}), {0, 0, 0}),
          "camera-relative Q32 keeps millimeter motion at large world coordinates");
    auto packed = game::shaderView(v);
    check(packed[3] == v.values[12] && packed[7] == v.values[13] && packed[11] == v.values[14],
          "shader affine packing retains translation");
    check(origin.x == int64_t(1e9 * 4294967296.) && origin.y == 0, "root remains unchanged");
    try
    {
        game::projection(f, 0, 10);
        check(false, "invalid clip rejected");
    }
    catch (const std::invalid_argument &)
    {
        check(true, "invalid clip rejected");
    }
    try
    {
        game::projection(f, .1f, std::numeric_limits<float>::infinity());
        check(false, "infinite clip rejected");
    }
    catch (const std::invalid_argument &)
    {
        check(true, "infinite clip rejected");
    }
    if (argc == 3 && std::string(argv[1]) == "--vectors")
    {
        std::ofstream out(argv[2]);
        if (!out)
            return 2;
        out << std::setprecision(10) << '[';
        // Deterministic varied 6DoF and asymmetric frusta for an independent
        // oracle: the original executable's math run inside a CPU emulator.
        for (int i = 0; i < 64; ++i)
        {
            if (i)
                out << ',';
            float factor = float(i) / 63;
            XrFovf fov{-.5f - factor * .3f, .9f - factor * .2f, .6f + factor * .25f, -.85f + factor * .25f};
            float n = .01f + factor * .3f, farZ = 10 + factor * 9990;
            auto orientation =
                euler(-pi + factor * 2 * pi, std::sin(float(i)) * .9f, std::cos(float(i)) * .6f);
            auto position = cameraPosition(
                origin, {std::sin(float(i)) * .2f, 1.6f + factor * .4f, std::cos(float(i)) * .3f}, 1);
            auto view = game::cameraRelativeView(position, orientation, origin);
            out << "{\"fov\":";
            array(out, std::array<float, 4>{fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown});
            out << ",\"near\":" << n << ",\"far\":" << farZ << ",\"projection\":";
            array(out, game::projection(fov, n, farZ).values);
            out << ",\"position_q32\":";
            array(out, std::array<int64_t, 3>{position.x, position.y, position.z});
            out << ",\"origin_q32\":";
            array(out, std::array<int64_t, 3>{origin.x, origin.y, origin.z});
            out << ",\"orientation\":";
            array(out, std::array<float, 4>{orientation.x, orientation.y, orientation.z, orientation.w});
            out << ",\"view\":";
            array(out, view.values);
            out << '}';
        }
        out << "]\n";
        if (!out)
            return 2;
    }
    std::cout << passed << " PASS / " << failures << " FAIL\n";
    return failures ? 1 : 0;
}
