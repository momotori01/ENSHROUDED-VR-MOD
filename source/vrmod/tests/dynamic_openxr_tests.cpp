// This target deliberately does NOT link openxr_loader.lib.
#include <evr/test_renderer.hpp>
#include <iostream>
using namespace evr;
int main(int argc, char **argv)
{
    if (argc != 3 || !std::filesystem::path(argv[1]).is_absolute()) return 2;
    HMODULE loader = LoadLibraryExW(std::filesystem::path(argv[1]).c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!loader) { std::cerr << "FAIL load explicit workspace loader\n"; return 1; }
    int result = 0;
    try
    {
        auto get = reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader, "xrGetInstanceProcAddr"));
        std::filesystem::create_directories(argv[2]);
        Log log(std::string(argv[2]) + "/openxr.jsonl", true);
        OpenXrClient xr(log, get);
        xr.init(1);
        {
            TestRenderer renderer(xr);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (!xr.active())
            {
                if (!xr.poll() || std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("dynamic loader session not ready");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            for (int frame = 0; frame < 3; ++frame)
                if (!xr.frame([&](uint32_t eye, uint32_t index, const XrView &pose, XrTime, uint64_t) {
                    renderer.draw(eye, index, pose, "");
                })) throw std::runtime_error("dynamic loader frame failed");
            if (xr.draws[0] != 3 || xr.draws[1] != 3)
                throw std::runtime_error("dynamic loader stereo frame count");
        }
        xr.shutdown();
        std::cout << "PASS dynamic-only OpenXR dispatch: instance, session, 3 paired GPU frames, cleanup\n";
    }
    catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; result = 1; }
    FreeLibrary(loader); // The client and its session are already destroyed.
    return result;
}
