#pragma once
// Opt-in graphics creation preparation only. No XR session, swapchain, or frame.
#include <evr/vulkan.hpp>
#include <evr/vulkan_extensions.hpp>
#include <evr/openxr_dispatch.hpp>
#include <evr/process_exit.hpp>
#include <openxr/openxr_platform.h>
#include <filesystem>

namespace evr::diagnostic
{
inline thread_local bool insideXrPreflight = false;
inline thread_local VkPhysicalDevice preflightPhysicalAtLayer = VK_NULL_HANDLE;
struct XrPreflightGuard
{
    bool previous = insideXrPreflight;
    XrPreflightGuard() { insideXrPreflight = true; }
    ~XrPreflightGuard() { insideXrPreflight = previous; }
};
inline bool prepareRequested()
{
    const char *enabled = std::getenv("EVR_PREPARE_OPENXR");
    return enabled && !std::strcmp(enabled, "1") && !insideXrPreflight;
}
inline std::vector<std::string> extensionNames(uint32_t count, const char *const *names)
{
    std::vector<std::string> result;
    for (uint32_t i = 0; i < count; ++i) result.emplace_back(names[i]);
    return result;
}
inline std::vector<const char *> extensionPointers(const std::vector<std::string> &names)
{
    std::vector<const char *> result;
    for (const auto &name : names) result.push_back(name.c_str());
    return result;
}
class XrPreflight
{
    HMODULE loader = nullptr;
    PFN_xrGetInstanceProcAddr gipa = nullptr;
    PFN_xrDestroyInstance destroy = nullptr;
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    void cleanup() noexcept
    {
        if (processExiting()) return; // static destructor at process exit: leave the runtime alone (process_exit.hpp)
        XrPreflightGuard guard;
        if (instance && destroy) destroy(instance);
        instance = XR_NULL_HANDLE;
        if (loader) FreeLibrary(loader);
        loader = nullptr;
    }
    static void check(XrResult result, const char *operation)
    {
        if (XR_FAILED(result)) throw std::runtime_error(std::string(operation) + " XrResult=" + std::to_string(result));
    }
    template <class F> F function(const char *name)
    {
        PFN_xrVoidFunction address{};
        check(gipa(instance, name, &address), name);
        if (!address) throw std::runtime_error(std::string("missing XR function: ") + name);
        return reinterpret_cast<F>(address);
    }
    template <class F> std::vector<std::string> queryNames(F function)
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            uint32_t size = 0;
            check(function(instance, system, 0, &size, nullptr), "XR extension size");
            if (!size || size > 65536) throw std::runtime_error("invalid XR extension list size");
            std::vector<char> bytes(size, 0);
            auto result = function(instance, system, uint32_t(bytes.size()), &size, bytes.data());
            if (result == XR_ERROR_SIZE_INSUFFICIENT) continue;
            check(result, "XR extension names");
            if (size > bytes.size() || !std::memchr(bytes.data(), 0, bytes.size())) throw std::runtime_error("unterminated XR extension list");
            std::istringstream in(bytes.data());
            std::vector<std::string> names;
            for (std::string name; in >> name;) names.push_back(name);
            return names;
        }
        throw std::runtime_error("XR extension list remained incomplete");
    }
public:
    std::vector<std::string> instanceExtensions, deviceExtensions;
    bool visibilityMask = false;
    XrGraphicsRequirementsVulkanKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    XrPreflight()
    {
        XrPreflightGuard guard;
        try
        {
            const char *runtime = std::getenv("XR_RUNTIME_JSON"), *path = std::getenv("EVR_PREPARE_LOADER");
            if (!runtime || !path || !std::filesystem::path(runtime).is_absolute() || !std::filesystem::path(path).is_absolute())
                throw std::runtime_error("preflight requires explicit absolute runtime/loader paths");
            // Load only the requested workspace loader, without changing DLL search state.
            loader = LoadLibraryExW(std::filesystem::path(path).c_str(), nullptr,
                                   LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!loader) throw std::runtime_error("workspace OpenXR loader load failed");
            gipa = reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader, "xrGetInstanceProcAddr"));
            auto create = reinterpret_cast<PFN_xrCreateInstance>(GetProcAddress(loader, "xrCreateInstance"));
            destroy = reinterpret_cast<PFN_xrDestroyInstance>(GetProcAddress(loader, "xrDestroyInstance"));
            if (!gipa || !create || !destroy) throw std::runtime_error("OpenXR loader exports missing");
            // XR_KHR_visibility_mask is enabled when the runtime offers it (field of view crop, fov_crop.hpp);
            // EVR_FOV_CROP=0 leaves the instance with the graphics binding only.
            auto enumerateExtensions = reinterpret_cast<PFN_xrEnumerateInstanceExtensionProperties>(GetProcAddress(loader, "xrEnumerateInstanceExtensionProperties"));
            const char *crop = std::getenv("EVR_FOV_CROP");
            if (enumerateExtensions && (!crop || std::strcmp(crop, "0")))
            {
                uint32_t count = 0;
                if (XR_SUCCEEDED(enumerateExtensions(nullptr, 0, &count, nullptr)) && count && count < 4096)
                {
                    std::vector<XrExtensionProperties> available(count, {XR_TYPE_EXTENSION_PROPERTIES});
                    if (XR_SUCCEEDED(enumerateExtensions(nullptr, count, &count, available.data())))
                        for (uint32_t i = 0; i < count && i < available.size(); ++i)
                            visibilityMask |= !std::strcmp(available[i].extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
                }
            }
            const char *extensions[] = {XR_KHR_VULKAN_ENABLE_EXTENSION_NAME, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME};
            XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
            std::strcpy(info.applicationInfo.applicationName, "Enshrouded VR graphics preflight");
            info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
            info.enabledExtensionCount = visibilityMask ? 2 : 1; info.enabledExtensionNames = extensions;
            check(create(&info, &instance), "preflight xrCreateInstance");
            XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
            systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            check(function<PFN_xrGetSystem>("xrGetSystem")(instance, &systemInfo, &system), "preflight xrGetSystem");
            check(function<PFN_xrGetInstanceProperties>("xrGetInstanceProperties")(instance, &properties), "preflight properties");
            check(function<PFN_xrGetVulkanGraphicsRequirementsKHR>("xrGetVulkanGraphicsRequirementsKHR")(instance, system, &requirements), "preflight Vulkan requirements");
            instanceExtensions = queryNames(function<PFN_xrGetVulkanInstanceExtensionsKHR>("xrGetVulkanInstanceExtensionsKHR"));
            deviceExtensions = queryNames(function<PFN_xrGetVulkanDeviceExtensionsKHR>("xrGetVulkanDeviceExtensionsKHR"));
        }
        catch (...) { cleanup(); throw; }
    }
    ~XrPreflight() { cleanup(); }
    XrPreflight(const XrPreflight &) = delete;
    XrPreflight &operator=(const XrPreflight &) = delete;
    PFN_xrGetInstanceProcAddr loaderEntryPoint() const { return gipa; }
    BorrowedOpenXr borrowInstance() const { return {instance, true, visibilityMask}; }
    void validateApi(uint32_t version) const
    {
        if (!version) version = VK_API_VERSION_1_0;
        const auto xrVersion = XR_MAKE_VERSION(VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));
        if (xrVersion < requirements.minApiVersionSupported || xrVersion > requirements.maxApiVersionSupported)
            throw std::runtime_error("game Vulkan API outside Runtime range");
    }
    // The GPU as the runtime names it for this Vulkan instance: an application-level handle, the one the game itself
    // holds and the session's binding is checked against (openxr_client.hpp). The plate's session (screen_xr.hpp) is
    // made from inside the layer, where the same GPU has the loader's handle.
    VkPhysicalDevice runtimeGpu(VkInstance vkInstance)
    {
        XrPreflightGuard guard;
        VkPhysicalDevice required{};
        check(function<PFN_xrGetVulkanGraphicsDeviceKHR>("xrGetVulkanGraphicsDeviceKHR")(instance, system, vkInstance, &required), "runtime GPU for the plate's session");
        return required;
    }
    void validateGpu(VkInstance vkInstance, VkPhysicalDevice gameGpu)
    {
        XrPreflightGuard guard;
        VkPhysicalDevice required{};
        check(function<PFN_xrGetVulkanGraphicsDeviceKHR>("xrGetVulkanGraphicsDeviceKHR")(instance, system, vkInstance, &required), "preflight GPU selection");
        // Runtime returns an application-level handle. The Vulkan loader can wrap
        // it before it reaches this layer. Route a standard read-only query from
        // the public entry point back through this layer to compare at ONE level.
        auto module = GetModuleHandleW(L"vulkan-1.dll");
        auto publicGipa = module ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vkGetInstanceProcAddr")) : nullptr;
        auto properties = publicGipa ? reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(publicGipa(vkInstance, "vkGetPhysicalDeviceProperties")) : nullptr;
        if (!properties) throw std::runtime_error("public Vulkan GPU identity query unavailable");
        preflightPhysicalAtLayer = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties ignored{};
        properties(required, &ignored);
        const auto atLayer = preflightPhysicalAtLayer;
        preflightPhysicalAtLayer = VK_NULL_HANDLE;
        if (!atLayer || atLayer != gameGpu) throw std::runtime_error("game GPU differs from Runtime at layer boundary");
    }
};
} // namespace evr::diagnostic
