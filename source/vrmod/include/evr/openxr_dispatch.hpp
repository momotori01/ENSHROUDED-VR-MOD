#pragma once
#include <openxr/openxr.h>
#include <stdexcept>
#include <string>

namespace evr
{
// Per-client dispatch supports a dynamically loaded, caller-owned OpenXR loader.
// There is no global replacement of function pointers or Runtime selection.
#define EVR_XR_INSTANCE_FUNCTIONS(X) \
    X(xrDestroyInstance) X(xrGetInstanceProperties) X(xrGetSystem) X(xrGetSystemProperties) \
    X(xrCreateSession) X(xrDestroySession) X(xrEnumerateEnvironmentBlendModes) \
    X(xrEnumerateViewConfigurationViews) X(xrEnumerateSwapchainFormats) \
    X(xrCreateSwapchain) X(xrDestroySwapchain) X(xrEnumerateSwapchainImages) \
    X(xrCreateReferenceSpace) X(xrDestroySpace) X(xrLocateSpace) X(xrLocateViews) \
    X(xrPollEvent) X(xrBeginSession) X(xrEndSession) X(xrRequestExitSession) \
    X(xrWaitFrame) X(xrBeginFrame) X(xrEndFrame) \
    X(xrAcquireSwapchainImage) X(xrWaitSwapchainImage) X(xrReleaseSwapchainImage)
class OpenXrDispatch
{
protected:
    PFN_xrGetInstanceProcAddr getProcAddress;
    PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties = nullptr;
    PFN_xrCreateInstance xrCreateInstance = nullptr;
#define DECLARE(name) PFN_##name name = nullptr;
    EVR_XR_INSTANCE_FUNCTIONS(DECLARE)
#undef DECLARE
    explicit OpenXrDispatch(PFN_xrGetInstanceProcAddr get) : getProcAddress(get)
    {
        if (!get) throw std::invalid_argument("OpenXR dispatch requires a loader entry point");
        xrEnumerateInstanceExtensionProperties = xrFunction<PFN_xrEnumerateInstanceExtensionProperties>(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties");
        xrCreateInstance = xrFunction<PFN_xrCreateInstance>(XR_NULL_HANDLE, "xrCreateInstance");
    }
    template <class T> T xrFunction(XrInstance instance, const char *name) const
    {
        PFN_xrVoidFunction address{};
        const auto result = getProcAddress(instance, name, &address);
        if (XR_FAILED(result) || !address)
            throw std::runtime_error(std::string("OpenXR dispatch ") + name + " XrResult=" + std::to_string(result));
        return reinterpret_cast<T>(address);
    }
    void bindOpenXrInstance(XrInstance instance)
    {
#define LOAD(name) name = xrFunction<PFN_##name>(instance, #name);
        EVR_XR_INSTANCE_FUNCTIONS(LOAD)
#undef LOAD
    }
};
#undef EVR_XR_INSTANCE_FUNCTIONS

struct BorrowedOpenXr
{
    XrInstance instance = XR_NULL_HANDLE;
    // Must describe what was enabled at creation, not advertised capabilities.
    bool vulkanEnable1Enabled = false;
    bool visibilityMaskEnabled = false; // XR_KHR_visibility_mask enabled at creation
};
} // namespace evr
