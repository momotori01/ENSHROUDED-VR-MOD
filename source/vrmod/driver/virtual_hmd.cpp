// Test-only SteamVR driver, using Valve's documented OpenVR driver interfaces.
#include <atomic>
#include <chrono>
#include <cstring>
#include <evr/math.hpp>
#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <openvr_driver.h>
#include <sstream>
#include <thread>
namespace
{
class Hmd final : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent
{
    std::atomic<bool> active{false};
    std::thread thread;
    std::mutex mutex;
    vr::DriverPose_t pose{};
    uint32_t index = vr::k_unTrackedDeviceIndexInvalid;
    vr::PropertyContainerHandle_t container{};
    vr::VRInputComponentHandle_t proximity{};
    float ipd = .064f;
    uint32_t width = 640, height = 720;
    std::string path;
    bool automatic = false;
    void updatePose(const std::string &text)
    {
        std::istringstream in(text);
        std::string command;
        float x, y, z, yaw, pitch, roll, newIpd;
        int tracked, connected;
        in >> command;
        if (command == "auto")
        {
            automatic = true;
            return;
        }
        if (command != "pose" || !(in >> x >> y >> z >> yaw >> pitch >> roll >> newIpd >> tracked >> connected))
            return;
        if (!evr::finite({x, y, z}) || !std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(roll) ||
            !std::isfinite(newIpd) || newIpd < .02f || newIpd > .12f)
            return;
        auto q = evr::euler(yaw * evr::pi / 180, pitch * evr::pi / 180, roll * evr::pi / 180);
        pose.vecPosition[0] = x;
        pose.vecPosition[1] = y;
        pose.vecPosition[2] = z;
        pose.qRotation = {q.w, q.x, q.y, q.z};
        pose.poseIsValid = tracked != 0;
        pose.deviceIsConnected = connected != 0;
        pose.result = tracked ? vr::TrackingResult_Running_OK : vr::TrackingResult_Running_OutOfRange;
        ipd = newIpd;
        automatic = false;
    }
    void worker()
    {
        auto start = std::chrono::steady_clock::now();
        float appliedIpd = -1;
        while (active)
        {
            if (!path.empty())
            {
                auto request = std::filesystem::path(path).parent_path() / "shutdown.request";
                std::error_code ec;
                if (std::filesystem::remove(request, ec))
                {
                    vr::VREvent_Data_t data{};
                    vr::VRServerDriverHost()->VendorSpecificEvent(index, vr::VREvent_DriverRequestedQuit, data, 0);
                }
            }
            vr::DriverPose_t snapshot;
            float currentIpd;
            {
                std::lock_guard<std::mutex> l(mutex);
                if (!path.empty())
                {
                    std::ifstream input(path);
                    std::string line;
                    if (std::getline(input, line))
                        updatePose(line);
                }
                if (automatic)
                {
                    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
                    auto q = evr::euler(.8f * std::sin(t * .4f), .3f * std::sin(t * .7f), .1f * std::sin(t * .9f));
                    pose.vecPosition[0] = .1 * std::sin(t);
                    pose.vecPosition[1] = 1.65 + .08 * std::sin(t * .7);
                    pose.vecPosition[2] = .1 * std::cos(t);
                    pose.qRotation = {q.w, q.x, q.y, q.z};
                }
                snapshot = pose;
                currentIpd = ipd;
            }
            if (currentIpd != appliedIpd)
            {
                vr::HmdMatrix34_t left{{{1, 0, 0, -currentIpd / 2}, {0, 1, 0, 0}, {0, 0, 1, 0}}},
                    right{{{1, 0, 0, currentIpd / 2}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
                vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserIpdMeters_Float, currentIpd);
                vr::VRServerDriverHost()->SetDisplayEyeToHead(index, left, right);
                appliedIpd = currentIpd;
            }
            vr::VRDriverInput()->UpdateBooleanComponent(proximity, snapshot.deviceIsConnected, 0);
            vr::VRServerDriverHost()->TrackedDevicePoseUpdated(index, snapshot, sizeof(snapshot));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

  public:
    ~Hmd()
    {
        Deactivate();
    }
    vr::EVRInitError Activate(uint32_t id) override
    {
        index = id;
        container = vr::VRProperties()->TrackedDeviceToPropertyContainer(id);
        auto p = vr::VRProperties();
        p->SetStringProperty(container, vr::Prop_ModelNumber_String, "EVR Virtual HMD");
        p->SetStringProperty(container, vr::Prop_ManufacturerName_String, "EVR development");
        p->SetStringProperty(container, vr::Prop_TrackingSystemName_String, "evr_virtual");
        p->SetStringProperty(container, vr::Prop_SerialNumber_String, "EVR-VIRTUAL-001");
        p->SetFloatProperty(container, vr::Prop_DisplayFrequency_Float, 90);
        p->SetFloatProperty(container, vr::Prop_SecondsFromVsyncToPhotons_Float, 1.f / 90);
        p->SetFloatProperty(container, vr::Prop_UserHeadToEyeDepthMeters_Float, 0);
        p->SetBoolProperty(container, vr::Prop_IsOnDesktop_Bool, false);
        p->SetBoolProperty(container, vr::Prop_DisplayDebugMode_Bool, true);
        p->SetBoolProperty(container, vr::Prop_ContainsProximitySensor_Bool, true);
        p->SetStringProperty(container, vr::Prop_InputProfilePath_String, "{evr_virtual}/input/hmd_profile.json");
        vr::VRDriverInput()->CreateBooleanComponent(container, "/proximity", &proximity);
        p->SetUint64Property(container, vr::Prop_CurrentUniverseId_Uint64, 170000001);
        char buffer[2048]{};
        vr::VRSettings()->GetString("driver_evr_virtual", "poseFile", buffer, sizeof(buffer));
        path = buffer;
        auto w = vr::VRSettings()->GetInt32("driver_evr_virtual", "renderWidth"),
             h = vr::VRSettings()->GetInt32("driver_evr_virtual", "renderHeight");
        if (w > 0 && w <= 4096)
            width = uint32_t(w);
        if (h > 0 && h <= 4096)
            height = uint32_t(h);
        pose = {};
        pose.qWorldFromDriverRotation.w = pose.qDriverFromHeadRotation.w = pose.qRotation.w = 1;
        pose.vecPosition[1] = 1.65;
        pose.poseIsValid = pose.deviceIsConnected = true;
        pose.result = vr::TrackingResult_Running_OK;
        pose.shouldApplyHeadModel = false;
        active = true;
        thread = std::thread(&Hmd::worker, this);
        vr::VRDriverLog()->Log("EVR Virtual HMD activated; local test pose only");
        return vr::VRInitError_None;
    }
    void Deactivate() override
    {
        active = false;
        if (thread.joinable())
            thread.join();
        index = vr::k_unTrackedDeviceIndexInvalid;
    }
    void EnterStandby() override
    {
    }
    void *GetComponent(const char *name) override
    {
        return std::strcmp(name, vr::IVRDisplayComponent_Version) ? nullptr
                                                                  : static_cast<vr::IVRDisplayComponent *>(this);
    }
    void DebugRequest(const char *request, char *response, uint32_t capacity) override
    {
        std::lock_guard<std::mutex> l(mutex);
        updatePose(request ? request : "");
        if (capacity)
            std::snprintf(response, capacity, "EVR pose accepted (validate input via telemetry)");
    }
    vr::DriverPose_t GetPose() override
    {
        std::lock_guard<std::mutex> l(mutex);
        return pose;
    }
    void GetWindowBounds(int32_t *x, int32_t *y, uint32_t *w, uint32_t *h) override
    {
        *x = *y = 0;
        *w = 1280;
        *h = 720;
    }
    bool IsDisplayOnDesktop() override
    {
        return true;
    }
    bool IsDisplayRealDisplay() override
    {
        return false;
    }
    void GetRecommendedRenderTargetSize(uint32_t *w, uint32_t *h) override
    {
        *w = width;
        *h = height;
    }
    void GetEyeOutputViewport(vr::EVREye eye, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h) override
    {
        *x = eye == vr::Eye_Left ? 0 : 640;
        *y = 0;
        *w = 640;
        *h = 720;
    }
    void GetProjectionRaw(vr::EVREye eye, float *l, float *r, float *t, float *b) override
    {
        *l = eye == vr::Eye_Left ? -1.1f : -1.f;
        *r = eye == vr::Eye_Left ? 1.f : 1.1f;
        *t = -1.f;
        *b = 1.05f;
    }
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye, float u, float v) override
    {
        vr::DistortionCoordinates_t c{};
        c.rfRed[0] = c.rfGreen[0] = c.rfBlue[0] = u;
        c.rfRed[1] = c.rfGreen[1] = c.rfBlue[1] = v;
        return c;
    }
    bool ComputeInverseDistortion(vr::HmdVector2_t *result, vr::EVREye, uint32_t, float u, float v) override
    {
        result->v[0] = u;
        result->v[1] = v;
        return true;
    }
};
class Provider : public vr::IServerTrackedDeviceProvider
{
    std::unique_ptr<Hmd> hmd;

  public:
    vr::EVRInitError Init(vr::IVRDriverContext *context) override
    {
        VR_INIT_SERVER_DRIVER_CONTEXT(context);
        hmd = std::make_unique<Hmd>();
        return vr::VRServerDriverHost()->TrackedDeviceAdded("EVR-VIRTUAL-001", vr::TrackedDeviceClass_HMD, hmd.get())
                   ? vr::VRInitError_None
                   : vr::VRInitError_Driver_Unknown;
    }
    void Cleanup() override
    {
        hmd.reset();
        VR_CLEANUP_SERVER_DRIVER_CONTEXT();
    }
    const char *const *GetInterfaceVersions() override
    {
        return vr::k_InterfaceVersions;
    }
    void RunFrame() override
    {
        vr::VREvent_t e{};
        while (vr::VRServerDriverHost()->PollNextEvent(&e, sizeof(e)))
        {
        }
    }
    bool ShouldBlockStandbyMode() override
    {
        return true;
    }
    void EnterStandby() override
    {
    }
    void LeaveStandby() override
    {
    }
};
Provider provider;
} // namespace
extern "C" __declspec(dllexport) void *HmdDriverFactory(const char *name, int *error)
{
    if (!std::strcmp(name, vr::IServerTrackedDeviceProvider_Version))
        return &provider;
    if (error)
        *error = vr::VRInitError_Init_InterfaceNotFound;
    return nullptr;
}
