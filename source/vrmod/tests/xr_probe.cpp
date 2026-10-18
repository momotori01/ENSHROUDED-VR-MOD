#include <evr/test_renderer.hpp>
#include <iostream>
#include "renderdoc_capture.hpp"
using namespace evr;
int main(int argc, char **argv)
{
    std::string output = "vrmod/output/probe";
    int frames = 180, cycles = 1, captureEvery = 30;
    float scale = 1;
    bool debug = true;
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--output" && i + 1 < argc)
            output = argv[++i];
        else if (a == "--frames" && i + 1 < argc)
            frames = std::stoi(argv[++i]);
        else if (a == "--cycles" && i + 1 < argc)
            cycles = std::stoi(argv[++i]);
        else if (a == "--scale" && i + 1 < argc)
            scale = std::stof(argv[++i]);
        else if (a == "--capture-every" && i + 1 < argc)
            captureEvery = std::stoi(argv[++i]);
        else if (a == "--quiet")
            debug = false;
        else
        {
            std::cerr << "invalid argument " << a << '\n';
            return 2;
        }
    }
    if (frames <= 0 || cycles <= 0 || captureEvery < 0)
    {
        std::cerr << "invalid test bounds\n";
        return 2;
    }
    std::filesystem::create_directories(output);
    Log log(output + "/openxr.jsonl", debug);
    try
    {
        for (int cycle = 0; cycle < cycles; cycle++)
        {
            OpenXrClient xr(log);
            xr.init(scale);
            std::cout << "PASS OpenXR instance/system/Vulkan/session/reference spaces/runtime extents/swapchains\n";
            {
                TestRenderer renderer(xr);
                FixtureRenderDoc renderdoc(xr.vk.instance);
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                while (!xr.active())
                {
                    if (!xr.poll())
                        throw std::runtime_error("session exited before READY");
                    if (std::chrono::steady_clock::now() > deadline)
                        throw std::runtime_error("session READY timeout");
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                for (int frame = 0; frame < frames; frame++)
                {
                    renderdoc.begin(frame);
                    if (!xr.frame([&](uint32_t eye, uint32_t image, const XrView &viewPose, XrTime time,
                                      uint64_t number) {
                            std::string path;
                            if (captureEvery && (number == 1 || number % captureEvery == 0))
                                path = output + "/c" + std::to_string(cycle) + "_f" + std::to_string(number) +
                                       (eye ? "_right.ppm" : "_left.ppm");
                            renderer.draw(eye, image, viewPose, path);
                            std::ostringstream detail;
                            auto p = viewPose.pose;
                            detail << "\"frame\":" << number << ",\"eye\":" << eye << ",\"time\":" << time
                                   << ",\"position\":[" << p.position.x << ',' << p.position.y << ',' << p.position.z
                                   << "],\"orientation\":[" << p.orientation.x << ',' << p.orientation.y << ','
                                   << p.orientation.z << ',' << p.orientation.w << "],\"fov\":["
                                   << viewPose.fov.angleLeft << ',' << viewPose.fov.angleRight << ','
                                   << viewPose.fov.angleUp << ',' << viewPose.fov.angleDown << "],\"view\":[";
                            auto vm = view(p), pm = projection(viewPose.fov, .05f, 100);
                            for (int j = 0; j < 16; j++)
                            {
                                if (j)
                                    detail << ',';
                                detail << vm.m[j];
                            }
                            detail << "],\"projection\":[";
                            for (int j = 0; j < 16; j++)
                            {
                                if (j)
                                    detail << ',';
                                detail << pm.m[j];
                            }
                            detail << ']';
                            log.event("eye_render", detail.str(), true);
                        }))
                    {
                        if (xr.lost())
                        {
                            log.event("runtime_loss_handled");
                            break;
                        }
                        throw std::runtime_error("session stopped during probe");
                    }
                    renderdoc.end();
                }
                if (xr.draws[0] != xr.draws[1] || xr.draws[0] == 0)
                    throw std::runtime_error("stereo draw count mismatch/empty");
                std::cout << "PASS same-frame stereo: " << xr.draws[0] << " left, " << xr.draws[1] << " right, "
                          << xr.emptyFrames << " empty frames\n";
                log.event("cycle_complete",
                          "\"cycle\":" + std::to_string(cycle) + ",\"frames\":" + std::to_string(xr.frames) +
                              ",\"left\":" + std::to_string(xr.draws[0]) + ",\"right\":" + std::to_string(xr.draws[1]));
            }
            xr.shutdown();
            std::cout << "PASS session/instance teardown cycle " << cycle << '\n';
        }
        std::cout << "RESULT PASS (test scene only; Enshrouded renderer is not connected)\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        log.event("fallback", "\"reason\":" + jsonString(e.what()));
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
