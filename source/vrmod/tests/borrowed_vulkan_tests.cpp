// Standalone host-device fixture: tests borrowing, never attaches to Enshrouded.
#include <evr/test_renderer.hpp>
#include <evr/game_stereo.hpp>
#include <evr/vulkan_extensions.hpp>
#include "host_device_configuration.hpp"
#include <iostream>
using namespace evr;

int main(int argc, char **argv)
{
    std::string output = argc > 1 ? argv[1] : "borrowed-vulkan-test";
    bool requirementsOnly = false;
    bool retainXrInstance = false;
    uint32_t hostApi = 0;
    fixture::HostDeviceConfiguration recorded;
    std::vector<std::string> gameInstanceExtensions, gameDeviceExtensions;
    try { for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--requirements-only") requirementsOnly = true;
        else if (arg == "--retain-xr-instance") retainXrInstance = true;
        else if (i + 1 < argc && arg == "--host-api") hostApi = uint32_t(std::stoul(argv[++i]));
        else if (i + 1 < argc && arg == "--host-instance-extension") gameInstanceExtensions.emplace_back(argv[++i]);
        else if (i + 1 < argc && arg == "--host-device-extension") gameDeviceExtensions.emplace_back(argv[++i]);
        else if (i + 1 < argc && arg == "--host-feature") recorded.feature(argv[++i]);
        else if (i + 1 < argc && arg == "--host-queue") recorded.queue(argv[++i]);
        else if (i + 1 < argc && arg == "--host-vendor-id") recorded.vendor = uint32_t(std::stoul(argv[++i]));
        else if (i + 1 < argc && arg == "--host-device-id") recorded.device = uint32_t(std::stoul(argv[++i]));
        else { std::cerr << "invalid fixture argument " << arg << '\n'; return 2; }
    } } catch (const std::exception &e) { std::cerr << "invalid fixture argument: " << e.what() << '\n'; return 2; }
    std::filesystem::create_directories(output);
    Log log(output + "/openxr.jsonl", true);
    Vulkan host;
    BorrowedVulkan binding;
    XrInstance bootstrap = XR_NULL_HANDLE;
    try
    {
        // Ask the runtime before creating the fixture's host device. A real game integration
        // must do this before game vkCreateInstance/vkCreateDevice, or refuse missing requirements.
        const char *extension = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(ci.applicationInfo.applicationName, "EVR host-device fixture");
        ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
        ci.enabledExtensionCount = 1;
        ci.enabledExtensionNames = &extension;
        xrCheck(xrCreateInstance(&ci, &bootstrap), "fixture XR bootstrap");
        XrSystemId system{};
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
        si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        xrCheck(xrGetSystem(bootstrap, &si, &system), "fixture XR system");
        XrGraphicsRequirementsVulkanKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        xrCheck(xrFunction<PFN_xrGetVulkanGraphicsRequirementsKHR>(
                    bootstrap, "xrGetVulkanGraphicsRequirementsKHR")(bootstrap, system, &req),
                "fixture requirements");
        auto names = [&](auto fn) {
            uint32_t size = 0;
            xrCheck(fn(bootstrap, system, 0, &size, nullptr), "fixture extensions size");
            std::vector<char> bytes(std::max(1u, size), '\0');
            xrCheck(fn(bootstrap, system, size, &size, bytes.data()), "fixture extensions");
            std::istringstream in(bytes.data());
            std::vector<std::string> list;
            for (std::string name; in >> name;)
                list.push_back(name);
            return list;
        };
        binding.enabledInstanceExtensions = names(
            xrFunction<PFN_xrGetVulkanInstanceExtensionsKHR>(bootstrap, "xrGetVulkanInstanceExtensionsKHR"));
        binding.enabledDeviceExtensions = names(
            xrFunction<PFN_xrGetVulkanDeviceExtensionsKHR>(bootstrap, "xrGetVulkanDeviceExtensionsKHR"));
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        xrCheck(xrGetInstanceProperties(bootstrap, &properties), "fixture runtime properties");
        std::ofstream requirements(output + "/vulkan-requirements.json");
        requirements << "{\"runtime_name\":" << jsonString(properties.runtimeName)
                     << ",\"runtime_version\":" << properties.runtimeVersion
                     << ",\"min_vulkan_version\":" << req.minApiVersionSupported
                     << ",\"max_vulkan_version\":" << req.maxApiVersionSupported;
        for (const auto &group : {std::make_pair("instance_extensions", &binding.enabledInstanceExtensions),
                                  std::make_pair("device_extensions", &binding.enabledDeviceExtensions)})
        {
            requirements << "," << jsonString(group.first) << ":[";
            for (size_t i = 0; i < group.second->size(); ++i)
            {
                if (i)
                    requirements << ',';
                requirements << jsonString((*group.second)[i]);
            }
            requirements << ']';
        }
        requirements << "}\n";
        requirements.close();
        if (!requirements)
            throw std::runtime_error("requirements report write failed");
        if (requirementsOnly)
        {
            xrCheck(xrDestroyInstance(bootstrap), "requirements bootstrap teardown");
            bootstrap = XR_NULL_HANDLE;
            std::cout << "PASS runtime requirements captured; no Vulkan device or XR session created\n";
            return 0;
        }
        std::vector<const char *> inames, dnames;
        auto instanceSupported = supportedVulkanExtensions([&](uint32_t *count, VkExtensionProperties *p) {
            return host.vkEnumerateInstanceExtensionProperties(nullptr, count, p);
        });
        auto instancePlan = planVulkanExtensions(gameInstanceExtensions, binding.enabledInstanceExtensions, instanceSupported);
        if (!instancePlan.compatible())
            throw std::runtime_error("host/runtime instance extension unsupported: " + instancePlan.unavailable[0]);
        binding.enabledInstanceExtensions = instancePlan.enabled;
        for (auto &n : binding.enabledInstanceExtensions) inames.push_back(n.c_str());
        const XrVersion version = std::max(req.minApiVersionSupported, XR_MAKE_VERSION(1, 1, 0));
        if (version > req.maxApiVersionSupported)
            throw std::runtime_error("fixture Vulkan version range");
        binding.apiVersion =
            VK_MAKE_VERSION(uint32_t(XR_VERSION_MAJOR(version)), uint32_t(XR_VERSION_MINOR(version)),
                            uint32_t(XR_VERSION_PATCH(version)));
        if (hostApi)
        {
            const XrVersion requested = XR_MAKE_VERSION(VK_VERSION_MAJOR(hostApi), VK_VERSION_MINOR(hostApi), VK_VERSION_PATCH(hostApi));
            if (requested < req.minApiVersionSupported || requested > req.maxApiVersionSupported)
                throw std::runtime_error("recorded host Vulkan API outside runtime range");
            binding.apiVersion = hostApi;
        }
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion = binding.apiVersion;
        VkInstanceCreateInfo vi{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        vi.pApplicationInfo = &app;
        vi.enabledExtensionCount = uint32_t(inames.size());
        vi.ppEnabledExtensionNames = inames.data();
        vkCheck(host.vkCreateInstance(&vi, nullptr, &binding.instance), "host vkCreateInstance");
        host.owned = true;
        host.bindInstance(binding.instance);
        xrCheck(xrFunction<PFN_xrGetVulkanGraphicsDeviceKHR>(bootstrap, "xrGetVulkanGraphicsDeviceKHR")(
                    bootstrap, system, binding.instance, &binding.physical),
                "host physical device");
        VkPhysicalDeviceProperties hostProperties{};
        host.vkGetPhysicalDeviceProperties(binding.physical, &hostProperties);
        if ((recorded.vendor && recorded.vendor != hostProperties.vendorID) ||
            (recorded.device && recorded.device != hostProperties.deviceID))
            throw std::runtime_error("runtime GPU model differs from recorded game device");
        auto enumerateDeviceExtensions = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            host.gipa(binding.instance, "vkEnumerateDeviceExtensionProperties"));
        if (!enumerateDeviceExtensions) throw std::runtime_error("device extension enumerator unavailable");
        auto deviceSupported = supportedVulkanExtensions([&](uint32_t *n, VkExtensionProperties *p) {
            return enumerateDeviceExtensions(binding.physical, nullptr, n, p);
        });
        auto devicePlan = planVulkanExtensions(gameDeviceExtensions, binding.enabledDeviceExtensions, deviceSupported);
        if (!devicePlan.compatible())
            throw std::runtime_error("host/runtime device extension unsupported: " + devicePlan.unavailable[0]);
        binding.enabledDeviceExtensions = devicePlan.enabled;
        for (auto &n : binding.enabledDeviceExtensions) dnames.push_back(n.c_str());
        uint32_t count = 0;
        host.vkGetPhysicalDeviceQueueFamilyProperties(binding.physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        host.vkGetPhysicalDeviceQueueFamilyProperties(binding.physical, &count, families.data());
        while (binding.queueFamily < count &&
               !(families[binding.queueFamily].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            ++binding.queueFamily;
        if (binding.queueFamily == count)
            throw std::runtime_error("host graphics queue unavailable");
        const auto queueInfos = recorded.queueInfos(families, binding.queueFamily);
        for (const auto &queue : queueInfos)
            if (queue.queueFamilyIndex == binding.queueFamily) binding.createdQueueCount = queue.queueCount;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        di.pNext = recorded.chain();
        di.queueCreateInfoCount = uint32_t(queueInfos.size());
        di.pQueueCreateInfos = queueInfos.data();
        di.enabledExtensionCount = uint32_t(dnames.size());
        di.ppEnabledExtensionNames = dnames.data();
        vkCheck(host.vkCreateDevice(binding.physical, &di, nullptr, &binding.device), "host vkCreateDevice");
        host.bindDevice(binding.device, binding.physical, binding.queueFamily);
        {
            std::ofstream creation(output + "/host-vulkan.jsonl");
            auto emit = [&](const char *event, const std::vector<std::string> &extensions) {
                creation << "{\"event\":" << jsonString(event) << ",\"instance\":"
                         << jsonString(std::to_string(reinterpret_cast<uintptr_t>(binding.instance)))
                         << ",\"api\":" << binding.apiVersion << ",\"extensions\":[";
                for (size_t i = 0; i < extensions.size(); ++i) {
                    if (i) creation << ',';
                    creation << jsonString(extensions[i]);
                }
                creation << ']';
                if (!std::strcmp(event, "create_device"))
                {
                    creation << ',' << diagnostic::deviceFeatures(di) << ",\"queues\":[";
                    for (size_t i = 0; i < queueInfos.size(); ++i)
                    {
                        if (i) creation << ',';
                        const auto &queue = queueInfos[i];
                        creation << "{\"family\":" << queue.queueFamilyIndex << ",\"count\":" << queue.queueCount
                                 << ",\"flags\":" << queue.flags << ",\"priorities\":[";
                        for (uint32_t j = 0; j < queue.queueCount; ++j)
                        { if (j) creation << ','; creation << queue.pQueuePriorities[j]; }
                        creation << "]}";
                    }
                    creation << "],\"vendor_id\":" << hostProperties.vendorID << ",\"device_id\":" << hostProperties.deviceID;
                }
                creation << "}\n";
            };
            emit("create_instance", binding.enabledInstanceExtensions);
            emit("create_device", binding.enabledDeviceExtensions);
            if (!creation) throw std::runtime_error("host creation report failed");
        }
        std::cout << "PASS host extension union created: " << instancePlan.added.size() << " instance, "
                  << devicePlan.added.size() << " device additions; original names preserved\n";
        binding.getInstanceProcAddr = host.gipa;
        BorrowedOpenXr xrBorrow{bootstrap, true};
        if (!retainXrInstance)
        {
            xrCheck(xrDestroyInstance(bootstrap), "fixture bootstrap teardown");
            bootstrap = XR_NULL_HANDLE;
        }
        const auto *sharedXr = retainXrInstance ? &xrBorrow : nullptr;
        auto hostAlive = [&]() {
            VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            info.size = 64;
            info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            VkBuffer buffer{};
            vkCheck(host.vkCreateBuffer(host.device, &info, nullptr, &buffer),
                    "host device survives VR teardown");
            host.vkDestroyBuffer(host.device, buffer, nullptr);
            if (retainXrInstance)
            {
                XrInstanceProperties ownerProperties{XR_TYPE_INSTANCE_PROPERTIES};
                xrCheck(xrGetInstanceProperties(bootstrap, &ownerProperties), "owner XR instance survives client teardown");
            }
        };
        auto rejected = [&](BorrowedVulkan bad, const char *expected) {
            bool caught = false;
            {
                OpenXrClient xr(log);
                try
                {
                    xr.init(1, &bad, sharedXr);
                }
                catch (const std::exception &e)
                {
                    caught = std::string(e.what()).find(expected) != std::string::npos;
                    if (!caught)
                        throw;
                }
            }
            if (!caught)
                throw std::runtime_error(std::string("missing rejection: ") + expected);
            hostAlive();
            std::cout << "PASS rejected " << expected << "; host device remains alive\n";
        };
        auto bad = binding;
        bad.apiVersion = VK_MAKE_VERSION(9, 0, 0);
        rejected(bad, "API version");
        bad = binding;
        bad.queueIndex = bad.createdQueueCount;
        rejected(bad, "creation records");
        bad = binding;
        bad.queueFamily = UINT32_MAX;
        rejected(bad, "graphics queue invalid");
        bad = binding;
        bad.physical = reinterpret_cast<VkPhysicalDevice>(uintptr_t(1));
        rejected(bad, "GPU differs");
        if (!binding.enabledInstanceExtensions.empty())
        {
            bad = binding;
            bad.enabledInstanceExtensions.clear();
            rejected(bad, "missing instance extension");
        }
        if (!binding.enabledDeviceExtensions.empty())
        {
            bad = binding;
            bad.enabledDeviceExtensions.clear();
            rejected(bad, "missing device extension");
        }
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            {
                OpenXrClient xr(log);
                xr.init(1, &binding, sharedXr);
                if (xr.vk.owned)
                    throw std::runtime_error("borrowed device ownership transferred");
                TestRenderer renderer(xr);
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                while (!xr.active())
                {
                    if (!xr.poll() || std::chrono::steady_clock::now() > deadline)
                        throw std::runtime_error("borrowed session READY timeout");
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                game::StereoCameras cameras;
                uint32_t preparedCount = 0, eyeOrder = 2;
                auto runFrame = [&](const OpenXrClient::Render &draw, const OpenXrClient::Prepare &prepare) {
                    if (cycle == 0) return xr.frame(draw, prepare);
                    if (!xr.beginFrame(prepare)) return false;
                    try
                    {
                        // The engine integration will record game commands after
                        // beginFrame, then submit/copy in a later render phase.
                        const auto snapshot = xr.pendingFrame();
                        if (snapshot.shouldRender)
                            for (uint32_t eye = 0; eye < 2; ++eye)
                            {
                                draw(eye, snapshot.imageIndices[eye], snapshot.views[eye], snapshot.predictedDisplayTime, snapshot.frame);
                                xr.markEyeRendered(eye, snapshot.frame); // fixture draw waits its GPU fence
                            }
                        xr.finishFrame();
                        return true;
                    }
                    catch (...) { xr.abortFrame(); throw; }
                };
                for (int frame = 0; frame < 30; ++frame)
                    if (!runFrame([&](uint32_t eye, uint32_t index, const XrView &pose, XrTime time,
                                      uint64_t number) {
                            if (cameras.xrFrame != number || cameras.predictedDisplayTime != time || eyeOrder != eye)
                                throw std::runtime_error("per-frame camera preparation/order mismatch");
                            ++eyeOrder;
                            std::string path;
                            if (number == 1 || number == 30)
                                path = output + "/c" + std::to_string(cycle) + "_f" + std::to_string(number) +
                                       (eye ? "_right.ppm" : "_left.ppm");
                            renderer.draw(eye, index, pose, path);
                            log.event("eye_render", "\"frame\":" + std::to_string(number) +
                                                        ",\"eye\":" + std::to_string(eye) +
                                                        ",\"time\":" + std::to_string(time));
                        }, [&](const std::array<XrView, 2> &views, const XrViewState &state,
                               const XrSpaceLocation &, XrTime time, uint64_t number) {
                            if (eyeOrder != 2)
                                throw std::runtime_error("new camera snapshot before previous pair completed");
                            game::StableAnchor anchor{}; // fixture root; no game process access
                            cameras = game::stereoCameras(anchor, identityPose(), views, state.viewStateFlags,
                                                         time, number, anchor.root, .05f, 100);
                            ++preparedCount;
                            eyeOrder = 0;
                            log.event("game_camera_prepared", "\"frame\":" + std::to_string(number) +
                                      ",\"time\":" + std::to_string(time) + ",\"game_rendered\":false");
                        }))
                        throw std::runtime_error("borrowed frame stopped");
                if (xr.draws[0] != 30 || xr.draws[1] != 30)
                    throw std::runtime_error("borrowed stereo counts");
                if (preparedCount != 30 || eyeOrder != 2)
                    throw std::runtime_error("paired camera snapshot count mismatch");
                std::cout << "PASS prepared both game camera values once before each of 30 eye pairs\n";
                log.event("cycle_complete",
                          "\"cycle\":" + std::to_string(cycle) + ",\"frames\":30,\"left\":30,\"right\":30");
            }
            hostAlive();
            std::cout << "PASS borrowed cycle " << cycle << ": 30 paired frames; host device survives\n";
        }
        {
            std::filesystem::create_directories(output + "/preparation-failure");
            Log failureLog(output + "/preparation-failure/openxr.jsonl", true);
            OpenXrClient xr(failureLog);
            xr.init(1, &binding, sharedXr);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (!xr.active())
            {
                if (!xr.poll() || std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("preparation failure session READY timeout");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            bool rejectedPreparation = false;
            int drawsAttempted = 0;
            try
            {
                xr.frame([&](uint32_t, uint32_t, const XrView &, XrTime, uint64_t) { ++drawsAttempted; },
                         [](const std::array<XrView, 2> &, const XrViewState &,
                            const XrSpaceLocation &, XrTime, uint64_t) {
                             throw std::runtime_error("synthetic camera preparation failure");
                         });
            }
            catch (const std::runtime_error &e)
            {
                rejectedPreparation = std::string(e.what()) == "synthetic camera preparation failure";
            }
            if (!rejectedPreparation || drawsAttempted || xr.draws[0] || xr.draws[1] || xr.frames != 1)
                throw std::runtime_error("camera preparation failure did not prevent both eye renders");
            for(int pending=0;pending<3;++pending){
                if(!xr.beginFrame())throw std::runtime_error("deferred frame begin failed");
                xr.skipFrame("fixture_view_resources_pending");
            }
            if(xr.frames!=4||xr.emptyFrames!=3||xr.draws[0]||xr.draws[1])
                throw std::runtime_error("deferred frames changed stereo counters");
            TestRenderer resumed(xr);
            if(!xr.frame([&](uint32_t eye,uint32_t index,const XrView& pose,XrTime,uint64_t){resumed.draw(eye,index,pose,"");})||xr.draws[0]!=1||xr.draws[1]!=1||xr.frames!=5)
                throw std::runtime_error("stereo did not resume after three deferred frames");
            std::cout<<"PASS three checked empty frames release both images; same session resumes a GPU-rendered stereo pair\n";
        }
        hostAlive();
        std::cout << "PASS camera preparation failure submits neither eye; host device survives teardown\n";
        if (retainXrInstance)
        {
            BorrowedOpenXr disabled{bootstrap, false};
            bool rejected = false;
            try { OpenXrClient client(log); client.init(1, &binding, &disabled); }
            catch (const std::invalid_argument &) { rejected = true; }
            if (!rejected) throw std::runtime_error("borrowed XR enabled-extension record not checked");
            hostAlive();
            xrCheck(xrDestroyInstance(bootstrap), "owner explicitly destroys shared XR instance after all clients");
            bootstrap = XR_NULL_HANDLE;
            std::cout << "PASS shared XR instance survived failed init and all three sessions; owner teardown succeeded\n";
        }
        std::cout << "RESULT PASS (external-device test scene only, no game integration)\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        if (bootstrap)
            xrDestroyInstance(bootstrap);
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
