// Test-only OpenXR runtime subset. Uses actual Vulkan swapchain images.
// Not a production compositor or a Khronos conformance implementation.
#include <atomic>
#include <mutex>
#include <cstring>
#include <deque>
#include <evr/log.hpp>
#include <evr/math.hpp>
#include <evr/mock_navigation.hpp>
#include <evr/vulkan.hpp>
#include <fstream>
#include <memory>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>
#include <set>
#include <sstream>
#include <thread>
using namespace evr;
namespace
{
struct Sample
{
    Pose head = identityPose();
    float ipd = .064f;
    std::array<XrFovf,2> fov{{{-.88f,.82f,.8f,-.85f},{-.82f,.88f,.8f,-.85f}}};
    bool valid = true, connected = true;
    uint32_t width = 640, height = 720;
    float yaw = 0, pitch = 0, roll = 0; // degrees, as in the pose CSV (EVR_MockHeadTurn keeps the roll, adds to the yaw)
};
// A turn of the head asked by the game's diagnostic input script (hooks/key_script.hpp, "TURN <yaw> <pitch>",
// 2026-10-14: the self-test looks round at the places of a recorded walk). Mock only; off = the pose CSV's head.
std::atomic<bool> headTurnOn{false};
std::atomic<float> headTurnYaw{0}, headTurnPitch{0};
bool headTurnLogged = false;
float headTurnLoggedYaw = 0, headTurnLoggedPitch = 0;
std::vector<Sample> samples;
Sample sample;
uint64_t frame = 0;
std::string navigationPath;
bool navigationHandInput=false; // EVR_MOCK_NAV_HAND_INPUT=1: the hand script keeps its buttons and sticks, a command only adds its own
mock::NavigationState navigation;
bool navigationMoving=false;
int64_t navigationClock(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool instanceLive = false, sessionLive = false, begun = false, waited = false, frameBegun = false, requirements = false;
// The display time the last xrWaitFrame gave (xrEndFrame takes only it). EVR_MOCK_CLOCK_TIME=1 (2026-10-23, the mod's own frames
// for every refresh): the steady clock's time since the start plus two frames, instead of the frame's number times 11.1 ms,
// so that a frame held up (a capture) does not shift the times against the game's clock.
XrTime lastPredicted = 0;
XrInstance inst = reinterpret_cast<XrInstance>(uintptr_t(0x10001));
XrSession sess = reinterpret_cast<XrSession>(uintptr_t(0x20001));
std::unique_ptr<Vulkan> vk;
std::unique_ptr<Log> logFile;
XrSessionState state = XR_SESSION_STATE_UNKNOWN;
std::deque<XrEventDataSessionStateChanged> events;
// The headset's own view reset, for a test (2026-10-18): EVR_MOCK_RECENTER_FRAME=<n> announces a new LOCAL space once,
// at frame n, taking effect from frame n+2 (the poses themselves stay as the pose script has them).
std::deque<XrEventDataReferenceSpaceChangePending> spaceEvents;
struct Space
{
    XrReferenceSpaceType type;
    Pose pose;
    XrAction action=XR_NULL_HANDLE;
    XrPath subaction=XR_NULL_PATH;
};
std::set<Space *> spaces;
struct Swap
{
    VkImage image[3]{};
    VkDeviceMemory memory[3]{};
    uint32_t width = 0, height = 0, next = 0, index = 0;
    int phase = 0;
    uint64_t releasedFrame = 0;
};
std::set<Swap *> swaps;
void event(XrSessionState next)
{
    state = next;
    XrEventDataSessionStateChanged e{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
    e.session = sess;
    e.state = next;
    e.time = XrTime(frame + 1) * 11111111;
    events.push_back(e);
}
void note(const char *name, const std::string &f = "")
{
    if (logFile)
        logFile->event(name, f);
}
void updateNavigation(){
    if(navigationPath.empty())return;
    char bytes[513]{};std::ifstream in(navigationPath,std::ios::binary);in.read(bytes,sizeof(bytes));
    std::string text(bytes,size_t(in.gcount()));
    const auto now=navigationClock();
    const auto wall=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(navigation.update(text,wall,now)){
        note("mock_navigation_command","\"frame\":"+std::to_string(frame)+",\"command\":\""+std::to_string(navigation.command.sequence)+"\",\"hold_ms\":"+std::to_string(navigation.command.holdMs));
    }
    if(navigation.command.sequence&&!navigationHandInput)sample.head.orientation=euler(navigation.command.yaw*pi/180,navigation.command.pitch*pi/180,0);
    const bool moving=navigation.active(now);
    if(moving!=navigationMoving)note("mock_navigation_input","\"frame\":"+std::to_string(frame)+",\"active\":"+(moving?"true":"false"));
    navigationMoving=moving;
}
void loadSamples()
{
    samples.clear();
    const char *path = std::getenv("EVR_POSE_SEQUENCE");
    if (!path || !*path)
        return;
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("pose CSV not found");
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        double time;
        Sample s;
        float yaw, pitch, roll;
        int tracked, connected;
        if (!(row >> time >> s.head.position.x >> s.head.position.y >> s.head.position.z >> yaw >> pitch >> roll >>
              s.ipd >> tracked >> connected >> s.width >> s.height))
            throw std::runtime_error("invalid pose CSV row");
        // Optional final 8 columns: recorded left/right eye FOV, radians, L/R/U/D.
        // Legacy 12-column fixtures retain the original mock optics.
        row >> std::ws;
        if (!row.eof()) {
            for (auto& f : s.fov) {
                if (!(row >> f.angleLeft >> f.angleRight >> f.angleUp >> f.angleDown) ||
                    !std::isfinite(f.angleLeft) || !std::isfinite(f.angleRight) ||
                    !std::isfinite(f.angleUp) || !std::isfinite(f.angleDown) ||
                    f.angleLeft >= 0 || f.angleRight <= 0 || f.angleUp <= 0 || f.angleDown >= 0 ||
                    f.angleLeft <= -pi/2 || f.angleRight >= pi/2 || f.angleUp >= pi/2 || f.angleDown <= -pi/2)
                    throw std::runtime_error("invalid recorded mock FOV");
            }
            row >> std::ws;
            if (!row.eof()) throw std::runtime_error("unexpected pose CSV columns");
        }
        s.head.orientation = euler(yaw * pi / 180, pitch * pi / 180, roll * pi / 180);
        s.yaw = yaw;
        s.pitch = pitch;
        s.roll = roll;
        s.valid = tracked != 0;
        s.connected = connected != 0;
        if (!finite(s.head.position) || !std::isfinite(s.ipd) || s.ipd <= 0 || !s.width || !s.height ||
            s.width > 4096 || s.height > 4096)
            throw std::runtime_error("invalid pose CSV value");
        samples.push_back(s);
    }
    if (!samples.empty())
        sample = samples.front();
}
template <class T> XrResult enumerate(uint32_t capacity, uint32_t *count, T *dest, const std::vector<T> &src)
{
    if (!count)
        return XR_ERROR_VALIDATION_FAILURE;
    *count = uint32_t(src.size());
    if (!capacity)
        return XR_SUCCESS;
    if (capacity < src.size())
        return XR_ERROR_SIZE_INSUFFICIENT;
    if (!dest)
        return XR_ERROR_VALIDATION_FAILURE;
    std::copy(src.begin(), src.end(), dest);
    return XR_SUCCESS;
}
bool good(XrInstance i)
{
    return instanceLive && i == inst;
}
bool good(XrSession s)
{
    return sessionLive && s == sess;
}
// Test visibility mask (EVR_MOCK_VISIBILITY_MASK=<scale 0.3..1>): XR_KHR_visibility_mask is offered and the
// visible area of each eye is an ellipse at `scale` of each tangent half-extent of that eye's fov.
float maskScale()
{
    const char *v = std::getenv("EVR_MOCK_VISIBILITY_MASK");
    const float s = v ? float(std::atof(v)) : 0.f;
    return s >= .3f && s <= 1.f ? s : 0.f;
}
XrFovf mockFov(int eye)
{
    return sample.fov.at(size_t(eye));
}
} // namespace
#define XR_FN(name) static XrResult XRAPI_CALL name
// Calls from more than one thread (2026-10-23: the mod's own frame for every refresh, include/evr/afw_pacing.hpp, waits
// for and ends frames on a thread of its own while the game's thread locates views and syncs actions): every function
// runs under this lock (xrGetInstanceProcAddr hands out Locked<...>::call); xrWaitFrame takes it after its pacing sleep.
static std::recursive_mutex mockLock;
template <auto F> struct Locked;
template <class... A, XrResult(XRAPI_CALL *F)(A...)> struct Locked<F>
{
    static XrResult XRAPI_CALL call(A... a)
    {
        std::lock_guard<std::recursive_mutex> guard(mockLock);
        return F(a...);
    }
};
#include "mock_actions.hpp"
XR_FN(m_xrEnumerateInstanceExtensionProperties)(const char *layer, uint32_t cap, uint32_t *count,
                                                XrExtensionProperties *props)
{
    if (layer)
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    XrExtensionProperties x{XR_TYPE_EXTENSION_PROPERTIES};
    std::strcpy(x.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
    x.extensionVersion = 2;
    XrExtensionProperties legacy{XR_TYPE_EXTENSION_PROPERTIES};
    std::strcpy(legacy.extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
    legacy.extensionVersion = XR_KHR_vulkan_enable_SPEC_VERSION;
    if (maskScale() > 0)
    {
        XrExtensionProperties mask{XR_TYPE_EXTENSION_PROPERTIES};
        std::strcpy(mask.extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
        mask.extensionVersion = XR_KHR_visibility_mask_SPEC_VERSION;
        return enumerate(cap, count, props, {x, legacy, mask});
    }
    return enumerate(cap, count, props, {x, legacy});
}
XR_FN(m_xrCreateInstance)(const XrInstanceCreateInfo *info, XrInstance *result)
{
    if (instanceLive)
        return XR_ERROR_LIMIT_REACHED;
    if (!info || !result)
        return XR_ERROR_VALIDATION_FAILURE;
    if (XR_VERSION_MAJOR(info->applicationInfo.apiVersion) != 1)
        return XR_ERROR_API_VERSION_UNSUPPORTED;
    for (uint32_t i = 0; i < info->enabledExtensionCount; i++)
        if (std::strcmp(info->enabledExtensionNames[i], XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) &&
            std::strcmp(info->enabledExtensionNames[i], XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) &&
            (maskScale() <= 0 || std::strcmp(info->enabledExtensionNames[i], XR_KHR_VISIBILITY_MASK_EXTENSION_NAME)))
            return XR_ERROR_EXTENSION_NOT_PRESENT;
    try
    {
        loadSamples();
        loadMotionSamples();
        navigation={};navigationMoving=false;
        const char* navigationEnv=std::getenv("EVR_MOCK_NAV_FILE");
        navigationPath=navigationEnv?navigationEnv:"";
        const char* navigationHands=std::getenv("EVR_MOCK_NAV_HAND_INPUT");
        navigationHandInput=navigationHands&&std::string(navigationHands)=="1";
        const char *p = std::getenv("EVR_MOCK_LOG");
        logFile = std::make_unique<Log>(p ? p : "mock_runtime.jsonl", true);
    }
    catch (...)
    {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    instanceLive = true;
    requirements = false;
    frame = 0;
    *result = inst;
    note("instance_created");
    return XR_SUCCESS;
}
XR_FN(m_xrDestroyInstance)(XrInstance i)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (sessionLive || !motionSets.empty())
        return XR_ERROR_CALL_ORDER_INVALID;
    instanceLive = false;
    note("instance_destroyed");
    logFile.reset();
    return XR_SUCCESS;
}
XR_FN(m_xrGetInstanceProperties)(XrInstance i, XrInstanceProperties *p)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    p->runtimeVersion = XR_MAKE_VERSION(0, 1, 0);
    std::strcpy(p->runtimeName, "EVR deterministic Vulkan test runtime");
    return XR_SUCCESS;
}
XR_FN(m_xrGetSystem)(XrInstance i, const XrSystemGetInfo *info, XrSystemId *system)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (info->formFactor != XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)
        return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
    if (!sample.connected)
        return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
    *system = 1;
    return XR_SUCCESS;
}
XR_FN(m_xrGetSystemProperties)(XrInstance i, XrSystemId system, XrSystemProperties *p)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (system != 1)
        return XR_ERROR_SYSTEM_INVALID;
    p->systemId = 1;
    p->vendorId = 0;
    std::strcpy(p->systemName, "EVR Virtual HMD");
    p->graphicsProperties = {4096, 4096, 16};   // the layer count m_xrEndFrame takes (XR_MIN_COMPOSITION_LAYERS_SUPPORTED)
    p->trackingProperties = {XR_TRUE, XR_TRUE};
    return XR_SUCCESS;
}
XR_FN(m_xrEnumerateViewConfigurations)(XrInstance i, XrSystemId, uint32_t c, uint32_t *n, XrViewConfigurationType *p)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    return enumerate(c, n, p, {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO});
}
XR_FN(m_xrGetViewConfigurationProperties)(XrInstance i, XrSystemId, XrViewConfigurationType type,
                                          XrViewConfigurationProperties *p)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (type != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    p->viewConfigurationType = type;
    p->fovMutable = XR_FALSE;
    return XR_SUCCESS;
}
XR_FN(m_xrEnumerateViewConfigurationViews)(XrInstance i, XrSystemId, XrViewConfigurationType type, uint32_t c,
                                           uint32_t *n, XrViewConfigurationView *p)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (type != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    XrViewConfigurationView v{XR_TYPE_VIEW_CONFIGURATION_VIEW};
    v.recommendedImageRectWidth = sample.width;
    v.recommendedImageRectHeight = sample.height;
    v.maxImageRectWidth = v.maxImageRectHeight = 4096;
    v.recommendedSwapchainSampleCount = v.maxSwapchainSampleCount = 1;
    return enumerate(c, n, p, {v, v});
}
XR_FN(m_xrEnumerateEnvironmentBlendModes)(XrInstance i, XrSystemId, XrViewConfigurationType, uint32_t c, uint32_t *n,
                                          XrEnvironmentBlendMode *p)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    return enumerate(c, n, p, {XR_ENVIRONMENT_BLEND_MODE_OPAQUE});
}
XR_FN(m_xrGetVulkanGraphicsRequirements2KHR)(XrInstance i, XrSystemId, XrGraphicsRequirementsVulkan2KHR *r)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    r->minApiVersionSupported = XR_MAKE_VERSION(1, 1, 0);
    r->maxApiVersionSupported = XR_MAKE_VERSION(1, 3, 0);
    requirements = true;
    return XR_SUCCESS;
}
XR_FN(m_xrCreateVulkanInstanceKHR)(XrInstance i, const XrVulkanInstanceCreateInfoKHR *info, VkInstance *result,
                                   VkResult *error)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    auto fn = reinterpret_cast<PFN_vkCreateInstance>(info->pfnGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    *error = fn(info->vulkanCreateInfo, info->vulkanAllocator, result);
    return XR_SUCCESS;
}
XR_FN(m_xrGetVulkanGraphicsDevice2KHR)(XrInstance i, const XrVulkanGraphicsDeviceGetInfoKHR *info,
                                       VkPhysicalDevice *result)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    try
    {
        Vulkan v;
        v.bindInstance(info->vulkanInstance);
        uint32_t n = 0;
        vkCheck(v.vkEnumeratePhysicalDevices(v.instance, &n, nullptr), "enumerate physical");
        if (!n)
            return XR_ERROR_RUNTIME_FAILURE;
        std::vector<VkPhysicalDevice> p(n);
        vkCheck(v.vkEnumeratePhysicalDevices(v.instance, &n, p.data()), "enumerate physical");
        *result = p[0];
        for (auto d : p)
        {
            VkPhysicalDeviceProperties props{};
            v.vkGetPhysicalDeviceProperties(d, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                *result = d;
                break;
            }
        }
        return XR_SUCCESS;
    }
    catch (...)
    {
        return XR_ERROR_RUNTIME_FAILURE;
    }
}
XR_FN(m_xrCreateVulkanDeviceKHR)(XrInstance i, const XrVulkanDeviceCreateInfoKHR *info, VkDevice *result,
                                 VkResult *error)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    auto fn = reinterpret_cast<PFN_vkCreateDevice>(info->pfnGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice"));
    if (!fn)
    {
        Vulkan v;
        fn = reinterpret_cast<PFN_vkCreateDevice>(GetProcAddress(v.module, "vkCreateDevice"));
    }
    if (!fn)
        return XR_ERROR_RUNTIME_FAILURE;
    *error = fn(info->vulkanPhysicalDevice, info->vulkanCreateInfo, info->vulkanAllocator, result);
    return XR_SUCCESS;
}
XR_FN(m_xrGetVulkanGraphicsRequirementsKHR)(XrInstance i, XrSystemId s, XrGraphicsRequirementsVulkanKHR *r)
{
    return m_xrGetVulkanGraphicsRequirements2KHR(i, s, r);
}
XR_FN(m_xrGetVulkanGraphicsDeviceKHR)(XrInstance i, XrSystemId s, VkInstance v, VkPhysicalDevice *p)
{
    XrVulkanGraphicsDeviceGetInfoKHR info{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    info.systemId = s;
    info.vulkanInstance = v;
    return m_xrGetVulkanGraphicsDevice2KHR(i, &info, p);
}
XR_FN(m_xrGetVulkanInstanceExtensionsKHR)(XrInstance i, XrSystemId, uint32_t cap, uint32_t *count, char *names)
{
    if (!good(i)) return XR_ERROR_HANDLE_INVALID;
    const std::string required = "VK_KHR_get_physical_device_properties2";
    std::vector<char> list(required.begin(), required.end());
    list.push_back('\0');
    return enumerate(cap, count, names, list);
}
XR_FN(m_xrGetVulkanDeviceExtensionsKHR)(XrInstance i, XrSystemId, uint32_t cap, uint32_t *count, char *names)
{
    if (!good(i)) return XR_ERROR_HANDLE_INVALID;
    const std::string required = "VK_KHR_maintenance1";
    std::vector<char> list(required.begin(), required.end());
    list.push_back('\0');
    return enumerate(cap, count, names, list);
}
XR_FN(m_xrCreateSession)(XrInstance i, const XrSessionCreateInfo *info, XrSession *result)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (sessionLive)
        return XR_ERROR_LIMIT_REACHED;
    if (!requirements)
        return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
    if (info->systemId != 1)
        return XR_ERROR_SYSTEM_INVALID;
    auto *b = static_cast<const XrGraphicsBindingVulkan2KHR *>(info->next);
    if (!b || b->type != XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR)
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    try
    {
        vk = std::make_unique<Vulkan>();
        vk->bindInstance(b->instance);
        vk->bindDevice(b->device, b->physicalDevice, b->queueFamilyIndex);
    }
    catch (...)
    {
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }
    sessionLive = true;
    begun = waited = frameBegun = false;
    events.clear();
    *result = sess;
    event(XR_SESSION_STATE_IDLE);
    event(XR_SESSION_STATE_READY);
    resetMotionSession();
    note("session_created");
    return XR_SUCCESS;
}
XR_FN(m_xrDestroySession)(XrSession s)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (!swaps.empty() || !spaces.empty())
        return XR_ERROR_CALL_ORDER_INVALID;
    resetMotionSession();
    sessionLive = false;
    begun = false;
    events.clear();
    spaceEvents.clear();
    vk.reset();
    note("session_destroyed");
    return XR_SUCCESS;
}
XR_FN(m_xrPollEvent)(XrInstance i, XrEventDataBuffer *out)
{
    if (!good(i))
        return XR_ERROR_HANDLE_INVALID;
    if (!spaceEvents.empty())
    {
        auto c = spaceEvents.front();
        spaceEvents.pop_front();
        std::memcpy(out, &c, sizeof(c));
        note("reference_space_change", "\"space\":" + std::to_string(int(c.referenceSpaceType)) + ",\"change_time\":" + std::to_string(c.changeTime));
        return XR_SUCCESS;
    }
    if (events.empty())
        return XR_EVENT_UNAVAILABLE;
    auto e = events.front();
    events.pop_front();
    std::memcpy(out, &e, sizeof(e));
    note("session_state", "\"state\":" + std::to_string(e.state));
    return XR_SUCCESS;
}
XR_FN(m_xrBeginSession)(XrSession s, const XrSessionBeginInfo *info)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (begun)
        return XR_ERROR_SESSION_RUNNING;
    if (state != XR_SESSION_STATE_READY)
        return XR_ERROR_SESSION_NOT_READY;
    if (info->primaryViewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    begun = true;
    event(XR_SESSION_STATE_SYNCHRONIZED);
    event(XR_SESSION_STATE_VISIBLE);
    event(XR_SESSION_STATE_FOCUSED);
    return XR_SUCCESS;
}
XR_FN(m_xrRequestExitSession)(XrSession s)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (!begun)
        return XR_ERROR_SESSION_NOT_RUNNING;
    event(XR_SESSION_STATE_STOPPING);
    return XR_SUCCESS;
}
XR_FN(m_xrEndSession)(XrSession s)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (state != XR_SESSION_STATE_STOPPING)
        return XR_ERROR_SESSION_NOT_STOPPING;
    begun = false;
    frameBegun = waited = false;
    event(XR_SESSION_STATE_IDLE);
    event(XR_SESSION_STATE_EXITING);
    return XR_SUCCESS;
}
XR_FN(m_xrEnumerateReferenceSpaces)(XrSession s, uint32_t c, uint32_t *n, XrReferenceSpaceType *p)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    return enumerate(c, n, p,
                     {XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_STAGE});
}
XR_FN(m_xrCreateReferenceSpace)(XrSession s, const XrReferenceSpaceCreateInfo *i, XrSpace *result)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (i->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_VIEW &&
        i->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_LOCAL &&
        i->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE)
        return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
    auto *p = new Space{i->referenceSpaceType, i->poseInReferenceSpace};
    spaces.insert(p);
    *result = reinterpret_cast<XrSpace>(p);
    return XR_SUCCESS;
}
XR_FN(m_xrDestroySpace)(XrSpace h)
{
    auto *s = reinterpret_cast<Space *>(h);
    if (!spaces.erase(s))
        return XR_ERROR_HANDLE_INVALID;
    delete s;
    return XR_SUCCESS;
}
static Pose spacePose(Space *s)
{
    if(s->action)return compose(motionSpacePose(s),s->pose);
    return s->type == XR_REFERENCE_SPACE_TYPE_VIEW ? compose(sample.head, s->pose) : s->pose;
}
XR_FN(m_xrLocateSpace)(XrSpace h, XrSpace base, XrTime, XrSpaceLocation *loc)
{
    auto *s = reinterpret_cast<Space *>(h);
    auto *b = reinterpret_cast<Space *>(base);
    if (!spaces.count(s) || !spaces.count(b))
        return XR_ERROR_HANDLE_INVALID;
    loc->locationFlags = sample.valid
                             ? (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT)
                             : 0;
    if((s->action && !motionSpaceTracked(s)) || (b->action && !motionSpaceTracked(b)))loc->locationFlags=0;
    loc->pose = compose(inverse(spacePose(b)), spacePose(s));
    return XR_SUCCESS;
}
XR_FN(m_xrGetReferenceSpaceBoundsRect)(XrSession s, XrReferenceSpaceType type, XrExtent2Df *p)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (type != XR_REFERENCE_SPACE_TYPE_STAGE)
        return XR_SPACE_BOUNDS_UNAVAILABLE;
    *p = {3, 3};
    return XR_SUCCESS;
}
XR_FN(m_xrLocateViews)(XrSession s, const XrViewLocateInfo *i, XrViewState *vs, uint32_t c, uint32_t *n, XrView *out)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (!begun)
        return XR_ERROR_SESSION_NOT_RUNNING;
    auto *b = reinterpret_cast<Space *>(i->space);
    if (!spaces.count(b))
        return XR_ERROR_HANDLE_INVALID;
    vs->viewStateFlags = sample.valid ? 15 : 0;
    std::vector<XrView> v(2, {XR_TYPE_VIEW});
    for (int eye = 0; eye < 2; eye++)
    {
        Pose ep{{0, 0, 0, 1}, {(eye ? 1 : -1) * sample.ipd / 2, 0, 0}};
        v[eye].pose = compose(inverse(spacePose(b)), compose(sample.head, ep));
        v[eye].fov = mockFov(eye);
    }
    return enumerate(c, n, out, v);
}
XR_FN(m_xrGetVisibilityMaskKHR)(XrSession s, XrViewConfigurationType config, uint32_t eye, XrVisibilityMaskTypeKHR type,
                               XrVisibilityMaskKHR *mask)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (maskScale() <= 0)
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    if (config != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO || eye > 1 || !mask || mask->type != XR_TYPE_VISIBILITY_MASK_KHR)
        return XR_ERROR_VALIDATION_FAILURE;
    const auto f = mockFov(int(eye));
    const float scale = maskScale(), l = std::tan(f.angleLeft), r = std::tan(f.angleRight), u = std::tan(f.angleUp),
                d = std::tan(f.angleDown);
    constexpr uint32_t segments = 48;
    std::vector<XrVector2f> vertices;
    std::vector<uint32_t> indices;
    if (type == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR || type == XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR)
    {
        const bool fan = type == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR;
        if (fan) vertices.push_back({0, 0});
        for (uint32_t i = 0; i < segments; ++i)
        {
            const float a = 6.2831853f * float(i) / segments, c = std::cos(a), sn = std::sin(a);
            vertices.push_back({(c > 0 ? r : -l) * scale * c, (sn > 0 ? u : -d) * scale * sn});
        }
        for (uint32_t i = 0; i < segments; ++i)
            if (fan) indices.insert(indices.end(), {0u, i + 1, (i + 1) % segments + 1});
            else indices.push_back(i);
    }
    // Hidden mesh: not offered by this test runtime (zero counts are a valid answer).
    if (!mask->vertexCapacityInput && !mask->indexCapacityInput)
    {
        mask->vertexCountOutput = uint32_t(vertices.size());
        mask->indexCountOutput = uint32_t(indices.size());
        return XR_SUCCESS;
    }
    if (mask->vertexCapacityInput < vertices.size() || mask->indexCapacityInput < indices.size())
        return XR_ERROR_SIZE_INSUFFICIENT;
    if ((!vertices.empty() && !mask->vertices) || (!indices.empty() && !mask->indices))
        return XR_ERROR_VALIDATION_FAILURE;
    std::copy(vertices.begin(), vertices.end(), mask->vertices);
    std::copy(indices.begin(), indices.end(), mask->indices);
    mask->vertexCountOutput = uint32_t(vertices.size());
    mask->indexCountOutput = uint32_t(indices.size());
    note("visibility_mask_query");
    return XR_SUCCESS;
}
XR_FN(m_xrEnumerateSwapchainFormats)(XrSession s, uint32_t c, uint32_t *n, int64_t *p)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    return enumerate<int64_t>(c, n, p, {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM});
}
XR_FN(m_xrCreateSwapchain)(XrSession s, const XrSwapchainCreateInfo *info, XrSwapchain *result)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (info->format != VK_FORMAT_R8G8B8A8_SRGB && info->format != VK_FORMAT_R8G8B8A8_UNORM)
        return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
    if (!info->width || !info->height || info->width > 4096 || info->height > 4096 || info->arraySize != 1 ||
        info->sampleCount != 1 || info->mipCount != 1 || info->faceCount != 1)
        return XR_ERROR_VALIDATION_FAILURE;
    auto *swap = new Swap;
    swap->width = info->width;
    swap->height = info->height;
    try
    {
        for (int j = 0; j < 3; j++)
        {
            VkImageCreateInfo im{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            im.imageType = VK_IMAGE_TYPE_2D;
            im.format = VkFormat(info->format);
            im.extent = {info->width, info->height, 1};
            im.mipLevels = 1;
            im.arrayLayers = 1;
            im.samples = VK_SAMPLE_COUNT_1_BIT;
            im.tiling = VK_IMAGE_TILING_OPTIMAL;
            im.usage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            if (info->usageFlags & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT)
                im.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vkCheck(vk->vkCreateImage(vk->device, &im, nullptr, &swap->image[j]), "mock image");
            VkMemoryRequirements req{};
            vk->vkGetImageMemoryRequirements(vk->device, swap->image[j], &req);
            swap->memory[j] = vk->allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkCheck(vk->vkBindImageMemory(vk->device, swap->image[j], swap->memory[j], 0), "mock bind image");
        }
    }
    catch (...)
    {
        for (int j = 0; j < 3; j++)
        {
            if (swap->image[j])
                vk->vkDestroyImage(vk->device, swap->image[j], nullptr);
            if (swap->memory[j])
                vk->vkFreeMemory(vk->device, swap->memory[j], nullptr);
        }
        delete swap;
        return XR_ERROR_RUNTIME_FAILURE;
    }
    swaps.insert(swap);
    *result = reinterpret_cast<XrSwapchain>(swap);
    note("swapchain_created",
         "\"width\":" + std::to_string(info->width) + ",\"height\":" + std::to_string(info->height));
    return XR_SUCCESS;
}
XR_FN(m_xrDestroySwapchain)(XrSwapchain h)
{
    auto *s = reinterpret_cast<Swap *>(h);
    if (!swaps.count(s))
        return XR_ERROR_HANDLE_INVALID;
    // OpenXR permits destroying an acquired swapchain after graphics completion.
    note("swapchain_destroyed", "\"pending_phase\":"+std::to_string(s->phase));
    vk->vkDeviceWaitIdle(vk->device);
    for (int j = 0; j < 3; j++)
    {
        vk->vkDestroyImage(vk->device, s->image[j], nullptr);
        vk->vkFreeMemory(vk->device, s->memory[j], nullptr);
    }
    swaps.erase(s);
    delete s;
    return XR_SUCCESS;
}
XR_FN(m_xrEnumerateSwapchainImages)(XrSwapchain h, uint32_t c, uint32_t *n, XrSwapchainImageBaseHeader *images)
{
    auto *s = reinterpret_cast<Swap *>(h);
    if (!swaps.count(s))
        return XR_ERROR_HANDLE_INVALID;
    std::vector<XrSwapchainImageVulkan2KHR> v(3, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    for (int i = 0; i < 3; i++)
        v[i].image = s->image[i];
    return enumerate(c, n, reinterpret_cast<XrSwapchainImageVulkan2KHR *>(images), v);
}
XR_FN(m_xrAcquireSwapchainImage)(XrSwapchain h, const XrSwapchainImageAcquireInfo *, uint32_t *index)
{
    auto *s = reinterpret_cast<Swap *>(h);
    if (!swaps.count(s))
        return XR_ERROR_HANDLE_INVALID;
    if (s->phase)
        return XR_ERROR_CALL_ORDER_INVALID;
    s->index = s->next++ % 3;
    s->phase = 1;
    *index = s->index;
    note("acquire", "\"frame\":" + std::to_string(frame));
    return XR_SUCCESS;
}
XR_FN(m_xrWaitSwapchainImage)(XrSwapchain h, const XrSwapchainImageWaitInfo *)
{
    auto *s = reinterpret_cast<Swap *>(h);
    if (!swaps.count(s))
        return XR_ERROR_HANDLE_INVALID;
    if (s->phase != 1)
        return XR_ERROR_CALL_ORDER_INVALID;
    s->phase = 2;
    note("wait_image", "\"frame\":" + std::to_string(frame));
    return XR_SUCCESS;
}
XR_FN(m_xrReleaseSwapchainImage)(XrSwapchain h, const XrSwapchainImageReleaseInfo *)
{
    auto *s = reinterpret_cast<Swap *>(h);
    if (!swaps.count(s))
        return XR_ERROR_HANDLE_INVALID;
    if (s->phase != 2)
        return XR_ERROR_CALL_ORDER_INVALID;
    s->phase = 0;
    s->releasedFrame = frame;
    note("release", "\"frame\":" + std::to_string(frame));
    return XR_SUCCESS;
}
XR_FN(m_xrWaitFrame)(XrSession s, const XrFrameWaitInfo *, XrFrameState *p)
{
    // EVR_MOCK_FRAME_MS=<n> (2026-10-14): one frame every n milliseconds at most, so that runs of the same input script
    // go the same way - the game moves by time, a script steps by frames (frames taking longer still take longer).
    // Slept before the lock (2026-10-23): another thread's calls go on meanwhile.
    {
        static const int frameMs = [] { const char *v = std::getenv("EVR_MOCK_FRAME_MS"); const int n = v ? std::atoi(v) : 0; return n > 0 && n <= 200 ? n : 0; }();
        static std::chrono::steady_clock::time_point due{};
        if (frameMs)
        {
            const auto now = std::chrono::steady_clock::now();
            if (due.time_since_epoch().count() == 0 || now > due + std::chrono::milliseconds(frameMs))
                due = now;
            else
                std::this_thread::sleep_until(due);
            due += std::chrono::milliseconds(frameMs);
        }
    }
    if (const char *realtime = std::getenv("EVR_MOCK_REALTIME"))
    {
        if (std::string(realtime) == "1")
            std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }
    std::lock_guard<std::recursive_mutex> guard(mockLock);
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (!begun)
        return XR_ERROR_SESSION_NOT_RUNNING;
    // OpenXR allows waiting for the next frame while one is begun; its xrBeginFrame then discards the frame begun first
    // (the steady pacing of the alternate eyes, include/evr/afw_pacing.hpp, 2026-10-23).
    if (waited)
        return XR_ERROR_CALL_ORDER_INVALID;
    waited = true;
    frame++;
    {
        static const uint64_t recenterFrame = [] { const char *v = std::getenv("EVR_MOCK_RECENTER_FRAME"); return v ? std::strtoull(v, nullptr, 10) : 0ull; }();
        if (recenterFrame && frame == recenterFrame)
        {
            XrEventDataReferenceSpaceChangePending c{XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
            c.session = sess;
            c.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            c.changeTime = XrTime(frame + 2) * 11111111;
            c.poseValid = XR_FALSE;
            spaceEvents.push_back(c);
        }
    }
    if (!samples.empty())
        sample = samples[(frame - 1) % samples.size()];
    {
        const bool on = headTurnOn.load();
        const float yaw = headTurnYaw.load(), pitch = headTurnPitch.load();
        if (on)
            sample.head.orientation = euler((sample.yaw + yaw) * pi / 180, pitch * pi / 180, sample.roll * pi / 180);
        if (on != headTurnLogged || (on && (yaw != headTurnLoggedYaw || pitch != headTurnLoggedPitch)))
        {
            note("mock_head_turn", "\"frame\":" + std::to_string(frame) + ",\"on\":" + (on ? "true" : "false") +
                                       ",\"yaw_added\":" + std::to_string(yaw) + ",\"pitch\":" + std::to_string(pitch));
            headTurnLogged = on;
            headTurnLoggedYaw = yaw;
            headTurnLoggedPitch = pitch;
        }
    }
    updateNavigation();
    {
        static const bool clockTime = [] { const char *v = std::getenv("EVR_MOCK_CLOCK_TIME"); return v && !std::strcmp(v, "1"); }();
        static const auto start = std::chrono::steady_clock::now();
        const XrTime clock = XrTime(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) + 2 * 11111111;
        lastPredicted = clockTime ? std::max(clock, lastPredicted + 1) : XrTime(frame) * 11111111;
    }
    p->predictedDisplayTime = lastPredicted;
    p->predictedDisplayPeriod = 11111111;
    p->shouldRender = sample.connected ? XR_TRUE : XR_FALSE;
    if (!sample.connected)
        event(XR_SESSION_STATE_LOSS_PENDING);
    note("wait_frame", "\"frame\":" + std::to_string(frame));
    return XR_SUCCESS;
}
XR_FN(m_xrBeginFrame)(XrSession s, const XrFrameBeginInfo *)
{
    if (!good(s))
        return XR_ERROR_HANDLE_INVALID;
    if (!begun)
        return XR_ERROR_SESSION_NOT_RUNNING;
    if (!waited)
        return XR_ERROR_CALL_ORDER_INVALID;
    const bool discarded = frameBegun;
    waited = false;
    frameBegun = true;
    if (discarded)
    {
        note("frame_discarded", "\"frame\":" + std::to_string(frame));
        return XR_FRAME_DISCARDED;
    }
    return XR_SUCCESS;
}
// This validates only the projection + UI quads subset, not arbitrary composition. Since 2026-10-13 the UI image may
// come as several quads of the same swapchain (the plate around the aim cluster and the cluster itself,
// include/evr/aim_mark.hpp): up to XR_MIN_COMPOSITION_LAYERS_SUPPORTED (16) layers, the projection first.
static XrResult validateSubImage(const XrSwapchainSubImage& image) {
    auto* sc=reinterpret_cast<Swap*>(image.swapchain);
    if(!swaps.count(sc))return XR_ERROR_HANDLE_INVALID;
    // OpenXR: "xrEndFrame will use the most recently released swapchain image" - also one released in an earlier
    // frame (alternate-eye rendering leaves one eye's swapchain alone for a frame). Never released, or held: invalid.
    if(sc->phase || !sc->releasedFrame)return XR_ERROR_CALL_ORDER_INVALID;
    if(sc->releasedFrame!=frame){static unsigned told=0;if(told++<3||!(told%1000))note("earlier_released_image","\"frame\":"+std::to_string(frame)+",\"released_frame\":"+std::to_string(sc->releasedFrame)+",\"count\":"+std::to_string(told));}
    const auto& r=image.imageRect;
    if(image.imageArrayIndex || r.offset.x<0 || r.offset.y<0 || r.extent.width<=0 || r.extent.height<=0 ||
       uint64_t(r.offset.x)+uint64_t(r.extent.width)>sc->width || uint64_t(r.offset.y)+uint64_t(r.extent.height)>sc->height)
        return XR_ERROR_SWAPCHAIN_RECT_INVALID;
    return XR_SUCCESS;
}
static bool validLayerPose(const Pose& p) {
    const auto& q=p.orientation;
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return finite(p.position) && std::isfinite(norm) && std::abs(norm-1.f)<.01f;
}
XR_FN(m_xrEndFrame)(XrSession s, const XrFrameEndInfo *info)
{
    if (!good(s))return XR_ERROR_HANDLE_INVALID;
    if (!frameBegun)return XR_ERROR_CALL_ORDER_INVALID;
    frameBegun = false;
    if(!info || info->type!=XR_TYPE_FRAME_END_INFO || info->next)return XR_ERROR_VALIDATION_FAILURE;
    if (info->displayTime != lastPredicted)return XR_ERROR_TIME_INVALID;
    if (info->environmentBlendMode != XR_ENVIRONMENT_BLEND_MODE_OPAQUE)return XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED;
    if (info->layerCount) {
        if (info->layerCount>16 || !info->layers || !info->layers[0])return XR_ERROR_LAYER_INVALID;
        const bool screen=info->layerCount==1 && info->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_QUAD;
        const auto* p=screen?nullptr:reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);
        if(!screen && info->layers[0]->type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION)return XR_ERROR_LAYER_INVALID;
        if(p){
        if(p->next || p->layerFlags || p->viewCount!=2 || !p->views)return XR_ERROR_VALIDATION_FAILURE;
        if(!spaces.count(reinterpret_cast<Space*>(p->space)))return XR_ERROR_HANDLE_INVALID;
        for(uint32_t i=0;i<2;i++) {
            const auto& v=p->views[i];
            if(v.type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW || v.next || !validLayerPose(v.pose))return XR_ERROR_VALIDATION_FAILURE;
            auto result=validateSubImage(v.subImage);if(XR_FAILED(result))return result;
        }
        if(p->views[0].subImage.swapchain==p->views[1].subImage.swapchain)return XR_ERROR_VALIDATION_FAILURE;
        }
        for(uint32_t index=screen?0:1;index<info->layerCount;++index) {
            if(!info->layers[index] || info->layers[index]->type!=XR_TYPE_COMPOSITION_LAYER_QUAD)return XR_ERROR_LAYER_INVALID;
            const auto* q=reinterpret_cast<const XrCompositionLayerQuad*>(info->layers[index]);
            if(q->next || (q->layerFlags & ~(XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT|
                XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT)) ||
               (q->eyeVisibility!=XR_EYE_VISIBILITY_BOTH && q->eyeVisibility!=XR_EYE_VISIBILITY_LEFT && q->eyeVisibility!=XR_EYE_VISIBILITY_RIGHT) ||
               !validLayerPose(q->pose) || !std::isfinite(q->size.width) || !std::isfinite(q->size.height) || q->size.width<=0 || q->size.height<=0)
                return XR_ERROR_VALIDATION_FAILURE;
            if(!spaces.count(reinterpret_cast<Space*>(q->space)))return XR_ERROR_HANDLE_INVALID;
            auto result=validateSubImage(q->subImage);if(XR_FAILED(result))return result;
            if(p && (q->subImage.swapchain==p->views[0].subImage.swapchain || q->subImage.swapchain==p->views[1].subImage.swapchain))
                return XR_ERROR_VALIDATION_FAILURE;
            note("ui_quad","\"frame\":"+std::to_string(frame)+",\"layer\":"+std::to_string(index)+",\"space_type\":"+std::to_string(reinterpret_cast<Space*>(q->space)->type)+
                ",\"flags\":"+std::to_string(q->layerFlags)+",\"eye_visibility\":"+std::to_string(q->eyeVisibility)+
                ",\"x\":"+std::to_string(q->subImage.imageRect.offset.x)+",\"y\":"+std::to_string(q->subImage.imageRect.offset.y)+
                ",\"width\":"+std::to_string(q->subImage.imageRect.extent.width)+",\"height\":"+std::to_string(q->subImage.imageRect.extent.height)+
                ",\"position\":["+std::to_string(q->pose.position.x)+","+std::to_string(q->pose.position.y)+","+std::to_string(q->pose.position.z)+
                "],\"metres\":["+std::to_string(q->size.width)+","+std::to_string(q->size.height)+"]");
        }
    }
    note("end_frame", "\"frame\":" + std::to_string(frame) + ",\"layers\":" + std::to_string(info->layerCount));
    return XR_SUCCESS;
}
XR_FN(m_xrResultToString)(XrInstance, XrResult r, char buffer[XR_MAX_RESULT_STRING_SIZE])
{
    std::snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XrResult(%d)", r);
    return XR_SUCCESS;
}
XR_FN(m_xrStructureTypeToString)(XrInstance, XrStructureType t, char buffer[XR_MAX_STRUCTURE_NAME_SIZE])
{
    std::snprintf(buffer, XR_MAX_STRUCTURE_NAME_SIZE, "XrStructureType(%d)", t);
    return XR_SUCCESS;
}
XR_FN(m_xrGetInstanceProcAddr)(XrInstance, const char *name, PFN_xrVoidFunction *fn)
{
    if (!name || !fn)
        return XR_ERROR_VALIDATION_FAILURE;
    *fn = nullptr;
    if (!std::strcmp(name, "xrWaitFrame"))   // it locks itself after its sleep
    {
        *fn = reinterpret_cast<PFN_xrVoidFunction>(m_xrWaitFrame);
        return XR_SUCCESS;
    }
#define MAP(n)                                                                                                         \
    if (!std::strcmp(name, #n))                                                                                        \
    {                                                                                                                  \
        *fn = reinterpret_cast<PFN_xrVoidFunction>(&Locked<m_##n>::call);                                                           \
        return XR_SUCCESS;                                                                                             \
    }
    EVR_MOCK_ACTION_MAP(MAP)
    MAP(xrGetInstanceProcAddr)
    MAP(xrGetVulkanGraphicsRequirementsKHR)
    MAP(xrGetVulkanGraphicsDeviceKHR)
    MAP(xrGetVulkanInstanceExtensionsKHR)
    MAP(xrGetVulkanDeviceExtensionsKHR)
    MAP(xrEnumerateInstanceExtensionProperties) MAP(xrCreateInstance) MAP(xrDestroyInstance)
        MAP(xrGetInstanceProperties) MAP(xrGetSystem) MAP(xrGetSystemProperties) MAP(xrEnumerateViewConfigurations)
            MAP(xrGetViewConfigurationProperties) MAP(xrEnumerateViewConfigurationViews)
                MAP(xrEnumerateEnvironmentBlendModes) MAP(xrGetVulkanGraphicsRequirements2KHR)
                    MAP(xrCreateVulkanInstanceKHR) MAP(xrGetVulkanGraphicsDevice2KHR) MAP(xrCreateVulkanDeviceKHR)
                        MAP(xrCreateSession) MAP(xrDestroySession) MAP(xrPollEvent) MAP(xrBeginSession)
                            MAP(xrRequestExitSession) MAP(xrEndSession) MAP(xrEnumerateReferenceSpaces)
                                MAP(xrCreateReferenceSpace) MAP(xrDestroySpace) MAP(xrLocateSpace)
                                    MAP(xrGetReferenceSpaceBoundsRect) MAP(xrLocateViews)
                                        MAP(xrEnumerateSwapchainFormats) MAP(xrCreateSwapchain) MAP(xrDestroySwapchain)
                                            MAP(xrEnumerateSwapchainImages) MAP(xrAcquireSwapchainImage)
                                                MAP(xrWaitSwapchainImage) MAP(xrReleaseSwapchainImage)
                                                    MAP(xrBeginFrame) MAP(xrEndFrame) MAP(xrResultToString)
                                                        MAP(xrStructureTypeToString) MAP(xrGetVisibilityMaskKHR)
#undef MAP
                                                            return XR_ERROR_FUNCTION_UNSUPPORTED;
}
// The head turn above, for hooks/key_script.hpp (the Mock runtime is loaded into the game's process): yaw in degrees
// added to the pose CSV's, pitch in degrees instead of the CSV's; on = 0 gives the CSV's head back.
extern "C" __declspec(dllexport) void EVR_MockHeadTurn(int on, float yawDegrees, float pitchDegrees)
{
    headTurnYaw = yawDegrees;
    headTurnPitch = pitchDegrees;
    headTurnOn = on != 0;
}
extern "C" __declspec(dllexport) XrResult XRAPI_CALL
xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo *loader, XrNegotiateRuntimeRequest *request)
{
    if (!loader || !request || loader->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loader->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loader->structSize != sizeof(XrNegotiateLoaderInfo) ||
        request->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST ||
        request->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION ||
        request->structSize != sizeof(XrNegotiateRuntimeRequest) || loader->minInterfaceVersion > 1 ||
        loader->maxInterfaceVersion < 1)
        return XR_ERROR_INITIALIZATION_FAILED;
    request->runtimeInterfaceVersion = 1;
    request->runtimeApiVersion = XR_MAKE_VERSION(1, 0, 34);
    request->getInstanceProcAddr = m_xrGetInstanceProcAddr;
    return XR_SUCCESS;
}
