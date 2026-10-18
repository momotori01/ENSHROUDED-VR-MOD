#pragma once
// Explicit bounded entry only. Never invoked automatically for a game.
// Caller supplies PUBLIC Vulkan handles after device creation has fully returned,
// with all its queue use stopped until this synchronous function returns.
int testPreparedSession(VkInstance instance, VkPhysicalDevice physical,
    VkDevice device, uint32_t family, bool gameProbe) noexcept
{
    try
    {
        const auto requested = std::getenv(gameProbe?"EVR_PREPARE_GAME_SESSION":"EVR_PREPARE_SESSION_FIXTURE");
        if (!requested || std::strcmp(requested, "1")) return 2;
        std::shared_ptr<diagnostic::XrPreflight> owner;
        BorrowedVulkan binding;
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto child = devices.find(key(device));
            auto parent = child==devices.end()?instances.end():instances.find(key(child->second.instance));
            if (parent == instances.end() || child == devices.end() || parent->second.fixture == gameProbe ||
                !parent->second.preflight || !child->second.fixtureBinding || child->second.instance != parent->second.instance)
                return 2;
            owner = parent->second.preflight;
            binding = *child->second.fixtureBinding;
        }
        if (family != binding.queueFamily) return 2;
        owner->validateGpu(instance,binding.physical); // Public Runtime selection must normalize to THIS device's layer GPU.
        auto module = GetModuleHandleW(L"vulkan-1.dll");
        binding.getInstanceProcAddr = module ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vkGetInstanceProcAddr")) : nullptr;
        binding.instance = instance; binding.physical = physical; binding.device = device;
        QueueCallScope context(gameProbe?"openxr_game_session_probe":"openxr_fixture");
        Log log(output + (gameProbe?"/game-session-openxr.jsonl":"/fixture-openxr.jsonl"), true);
        auto shared = owner->borrowInstance();
        OpenXrClient xr(log, owner->loaderEntryPoint());
        xr.init(1, &binding, &shared);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (!xr.active())
        {
            if (!xr.poll() || std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("prepared fixture session not ready");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        for (int frame = 0; frame < 3; ++frame)
        {
            if (!xr.beginFrame()) throw std::runtime_error("prepared fixture frame stopped");
            xr.abortFrame("fixture_no_game_renderer"); // No pixels rendered; submit zero composition layers.
        }
        if (xr.frames != 3 || xr.draws[0] || xr.draws[1]) throw std::runtime_error("prepared fixture frame accounting");
        xr.shutdown();
        PFN_xrVoidFunction raw{};
        xrCheck(owner->loaderEntryPoint()(shared.instance, "xrGetInstanceProperties", &raw), "fixture owner function");
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        xrCheck(reinterpret_cast<PFN_xrGetInstanceProperties>(raw)(shared.instance, &properties), "fixture owner survived");
        note(gameProbe?"prepared_game_session_probe":"prepared_session_fixture", "\"status\":\"PASS\",\"empty_frames\":3,\"game_stereo\":false");
        return 0;
    }
    catch (const std::exception &e) { note(gameProbe?"prepared_game_session_probe_failed":"prepared_session_fixture_failed", "\"reason\":" + jsonString(e.what())); }
    catch (...) { note(gameProbe?"prepared_game_session_probe_failed":"prepared_session_fixture_failed"); }
    return 1;
}
extern "C" __declspec(dllexport) int evrTestPreparedSession(VkInstance instance,VkPhysicalDevice physical,VkDevice device,uint32_t family) noexcept {
    return testPreparedSession(instance,physical,device,family,false);
}
extern "C" __declspec(dllexport) int evrTestPreparedGameSession(VkInstance instance,VkPhysicalDevice physical,VkDevice device,uint32_t family) noexcept {
    return testPreparedSession(instance,physical,device,family,true);
}
