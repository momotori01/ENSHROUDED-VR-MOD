#include <evr/world_epoch_broker.hpp>
#include <evr/game_build.hpp>
#include <evr/appearance_spirv_patch.hpp>
// Explicit Vulkan diagnostics; optional OpenXR graphics creation preparation.
// Default is observation. Opt-in UI capture preserves native output0 and adds a private MRT output.
// Native draw parameters/counts are unchanged; graphics preparation is disabled by default.
#include "capture.hpp"
#include "device_metadata.hpp"
#include "xr_preflight.hpp"
#include <evr/openxr_client.hpp>
#include <atomic>
#include <map>
#include <algorithm>
#include <evr/log.hpp>
#include <evr/head_spirv_patch.hpp>
#include <evr/body_clip_feature.hpp>
#include <evr/body_clip_payload_store.hpp>
#include <evr/vfx_writeback_patch.hpp>
#include <evr/vfx_decal_rebase.hpp>
#include <vfx_decal_writeback.h>
#include "native_cpu_arena.hpp"
#include <evr/vulkan.hpp>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <unordered_map>
#include <vulkan/vk_layer.h>
using namespace evr;
#include "shadow_receiver_shader.hpp"
#include "gi_probe_shader.hpp"
#include "ssr_edge_shader.hpp"
namespace
{
struct InstanceDispatch
{
    VkInstance instance{};
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    std::shared_ptr<diagnostic::XrPreflight> preflight;
    bool fixture = false;
    uint32_t apiVersion = 0;
    std::vector<std::string> enabledExtensions;
};
struct DeviceDispatch
{
    VkDevice device{};
    PFN_vkGetDeviceProcAddr gdpa = nullptr;
    VkInstance instance{};
    VkPhysicalDevice physical{};
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    std::shared_ptr<const BorrowedVulkan> fixtureBinding;
    std::shared_ptr<body::PayloadStore> bodyPayloads;
};
struct SwapData
{
    VkFormat format;
    uint32_t width, height;
    VkImageUsageFlags usage;
    std::vector<VkImage> images;
};
std::unordered_map<VkQueue, uint32_t> queueFamilies;
std::unordered_map<VkSwapchainKHR, SwapData> swapData;
std::mutex mutex;
struct BufferAudit {VkDeviceSize size=0;VkBufferUsageFlags usage=0;unsigned fills=0;VkDeviceMemory memory=VK_NULL_HANDLE;VkDeviceSize memoryOffset=0;};
std::unordered_map<void*,std::unordered_map<VkBuffer,BufferAudit>> bufferAudits;
std::unordered_map<void *, InstanceDispatch> instances;
std::unordered_map<void *, DeviceDispatch> devices;
std::unique_ptr<Log> logger;
std::atomic<uint64_t> presents{0}, draws{0}, indirects{0}, dispatches{0}, shaderCount{0};
std::string output;
uint64_t shaderBytes = 0;
bool saveShaders = false;
std::atomic<uint32_t> headShaderModules{0};
std::atomic<uint32_t> vfxShaderModules{0};
std::atomic<bool> vfxShaderFailed{false};
std::atomic<uint32_t> vfxDecalShaderModules{0};
std::atomic<bool> headShaderFailed{false};
template <class T> void *key(T h)
{
    return h ? *reinterpret_cast<void **>(h) : nullptr;
}
template <class T> std::string handleField(const char *name, T handle)
{
    std::ostringstream s;
    s << '"' << name << "\":\"0x" << std::hex << reinterpret_cast<uintptr_t>(handle) << '"';
    return s.str();
}
void initialize()
{
    if (logger)
        return;
    const char *dir = std::getenv("EVR_LAYER_OUTPUT");
    output = dir ? dir : "vrmod/output/game-layer";
    std::filesystem::create_directories(output);
    logger = std::make_unique<Log>(output + "/vulkan.jsonl", true);
    // Play sessions bound every event name (EVR_LOG_NAME_CAP, set by launch_diagnostics -Session).
    if (const auto cap = std::getenv("EVR_LOG_NAME_CAP")) { const auto n = std::strtoul(cap, nullptr, 10); if (n) logger->setNameCap(unsigned(n)); }
    saveShaders = std::getenv("EVR_CAPTURE_SHADERS") != nullptr;
    logger->event("hook_success", "\"kind\":\"explicit_vulkan_layer\",\"render_mode\":\"observation_only\"");
}
void note(const char *name, const std::string &f = "") noexcept
{
    try
    {
        std::lock_guard<std::mutex> l(mutex);
        initialize();
        logger->event(name, f);
    }
    catch (...)
    {
    }
}
bool preflightCandidate(const VkInstanceCreateInfo &info, bool &fixture)
{
    if (!diagnostic::prepareRequested()) return false;
    const char *fixtureOptIn = std::getenv("EVR_PREPARE_FIXTURE");
    fixture = fixtureOptIn && !std::strcmp(fixtureOptIn, "1") && info.pApplicationInfo &&
              info.pApplicationInfo->pApplicationName &&
              !std::strcmp(info.pApplicationInfo->pApplicationName, "EVR graphics preflight fixture");
    if (fixture) return true;
    wchar_t filename[32768]{};
    if (!GetModuleFileNameW(nullptr, filename, DWORD(std::size(filename))) ||
        _wcsicmp(std::filesystem::path(filename).filename().c_str(), L"enshrouded.exe")) return false;
    auto *base = reinterpret_cast<const uint8_t *>(GetModuleHandleW(nullptr));
    auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
    // the render function's first bytes (1.0: 0x129e20; tools/match_functions.py maps it at the next update)
    const uint8_t signature[]{0x4c,0x89,0x4c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08};
    if (nt->FileHeader.TimeDateStamp != evr::gameBuild::timeDateStamp || nt->OptionalHeader.SizeOfImage != evr::gameBuild::sizeOfImage ||
        std::memcmp(base + 0x129fc0, signature, sizeof(signature)))
    { note("preflight_disabled", "\"reason\":\"unsupported_loaded_game_build\""); return false; }
    const auto names = diagnostic::extensionNames(info.enabledExtensionCount, info.ppEnabledExtensionNames);
    return std::find(names.begin(), names.end(), "VK_KHR_win32_surface") != names.end() &&
           std::find(names.begin(), names.end(), "VK_KHR_surface") != names.end();
}
template <class F, class H> F deviceFn(H h, const char *name)
{
    std::lock_guard<std::mutex> l(mutex);
    auto it = devices.find(key(h));
    return it == devices.end() ? nullptr : reinterpret_cast<F>(it->second.gdpa(it->second.device, name));
}
template <class F> F instanceFn(VkInstance h, const char *name)
{
    std::lock_guard<std::mutex> l(mutex);
    auto it = instances.find(key(h));
    return it == instances.end() ? nullptr : reinterpret_cast<F>(it->second.gipa(h, name));
}
std::string stack()
{
    void *frames[24]{};
    USHORT n = CaptureStackBackTrace(1, 24, frames, nullptr);
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    std::ostringstream s;
    s << "[";
    bool first = true;
    for (USHORT i = 0; i < n; i++)
    {
        auto a = reinterpret_cast<uintptr_t>(frames[i]);
        if (a >= base && a < end)
        {
            if (!first)
                s << ',';
            first = false;
            s << '"' << "0x" << std::hex << (a - base) << '"';
        }
    }
    s << ']';
    return s.str();
}
// Diagnostic (2026-10-16, the VFX models that only the VFX owner's View draws on 1.0): EVR_CMD_STACKS_AT=<present>
// counts, for EVR_CMD_STACKS_FRAMES presents (30) from that present on, every dispatch and indirect draw by the game's
// call stack, and writes one cmd_stack_summary. A pass recorded for every View shows three times the count of a pass
// that only the VFX owner records.
uint64_t cmdStacksAt = [] { const char *v = std::getenv("EVR_CMD_STACKS_AT"); return v ? std::strtoull(v, nullptr, 10) : 0ull; }();
uint64_t cmdStacksFrames = [] { const char *v = std::getenv("EVR_CMD_STACKS_FRAMES"); const auto n = v ? std::strtoull(v, nullptr, 10) : 0ull; return n ? n : 30ull; }();
std::mutex cmdStacksMutex;
std::map<std::string, unsigned> cmdStacks;
bool cmdStacksWritten = false;
void cmdStack(const char *kind, const std::string &detail) noexcept
{
    if (!cmdStacksAt) return;
    const auto p = presents.load();
    if (p < cmdStacksAt) return;
    try
    {
        std::lock_guard<std::mutex> l(cmdStacksMutex);
        if (cmdStacksWritten) return;
        if (p >= cmdStacksAt + cmdStacksFrames)
        {
            cmdStacksWritten = true;
            std::vector<std::pair<unsigned, std::string>> rows;
            for (const auto &e : cmdStacks) rows.push_back({e.second, e.first});
            std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
            std::string out = "\"from_present\":" + std::to_string(cmdStacksAt) + ",\"presents\":" + std::to_string(cmdStacksFrames) + ",\"rows\":[";
            for (size_t i = 0; i < rows.size() && i < 600; ++i) out += (i ? "," : "") + std::string("{\"count\":") + std::to_string(rows[i].first) + "," + rows[i].second + "}";
            out += "]";
            note("cmd_stack_summary", out);
            return;
        }
        auto &n = cmdStacks["\"kind\":\"" + std::string(kind) + "\"," + detail + ",\"stack\":" + stack()];
        ++n;
    }
    catch (...)
    {
    }
}
} // namespace
#include "mesh_draw_guard.hpp"
#include "exit_progress.hpp"
#include "present_retirement.hpp"
void giReprojectModuleDestroyed(VkShaderModule module) noexcept;void giReprojectPipelineDestroyed(VkPipeline pipeline) noexcept;
#include "ui_pipeline_probe.hpp"
void gpuTimingBegin(VkCommandBuffer cmd) noexcept;void gpuTimingEnd(VkCommandBuffer cmd) noexcept;void gpuTimingNote(VkCommandBuffer cmd,uint32_t kind,uint32_t width=0,uint32_t height=0) noexcept;void gpuTimingRenderBegin(VkCommandBuffer cmd,const VkRenderingInfo* info) noexcept;void gpuTimingRenderEnd(VkCommandBuffer cmd) noexcept;void gpuTimingBind(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipeline pipeline) noexcept;void gpuTimingDispatch(VkCommandBuffer cmd,uint32_t x,uint32_t y,uint32_t z) noexcept;void gpuTimingSets(VkCommandBuffer cmd,VkPipelineBindPoint point,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets) noexcept;void gpuTimingDescriptorWrites(uint32_t count,const VkWriteDescriptorSet* writes) noexcept;
void giReprojectModule(VkShaderModule module,const VkShaderModuleCreateInfo* info) noexcept;void giReprojectPipeline(VkShaderModule module,VkPipeline pipeline) noexcept;void giReprojectWrites(uint32_t count,const VkWriteDescriptorSet* writes,uint32_t copies,const VkCopyDescriptorSet* copy) noexcept;void giReprojectReset(VkCommandBuffer c) noexcept;void giReprojectBind(VkCommandBuffer c,VkPipelineBindPoint point,VkPipeline pipeline) noexcept;void giReprojectSets(VkCommandBuffer c,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets) noexcept;void giReprojectForget(VkDevice device,VkImage image) noexcept;void giReprojectDispatch(VkCommandBuffer c,uint32_t gx,uint32_t gy,uint32_t gz) noexcept;void giReprojectDestroy(VkDevice device) noexcept;
bool atmosphereShareCreate(const VkImageCreateInfo& in,VkImageCreateInfo& out) noexcept;void atmosphereShareBarrier(VkCommandBuffer c,uint32_t count,const VkImageMemoryBarrier* images) noexcept;void atmosphereShareBarrier2(VkCommandBuffer c,const VkDependencyInfo* info) noexcept;void atmosphereShareForget(VkImage image) noexcept;bool shadowShareCreate(const VkImageCreateInfo& in,VkImageCreateInfo& out) noexcept;void shadowShareBarrier(VkCommandBuffer c,uint32_t count,const VkImageMemoryBarrier* images) noexcept;void shadowShareBarrier2(VkCommandBuffer c,const VkDependencyInfo* info) noexcept;void shadowShareForget(VkImage image) noexcept;
#include "render_trace.hpp"
#include "ui_glyph.hpp"
#include "image_state.hpp"
std::string imageAliasReportLocked(VkImage image);void forgetImageBindingLocked(VkImage image);VkDeviceMemory takeDealiasedMemoryLocked(VkImage image);void forgetSweepImageLocked(VkImage image);
void sweepDepthRendering(VkCommandBuffer cmd,const VkRenderingInfo* info) noexcept;void sweepDepthEnd(VkCommandBuffer cmd) noexcept;void sweepDepthDraw(VkCommandBuffer cmd,uint32_t kind=0) noexcept;
void sweepDepthClearAttachments(VkCommandBuffer cmd,uint32_t count,const VkClearAttachment* attachments,uint32_t rects,const VkClearRect* rect) noexcept;void sweepDepthTransfer(VkCommandBuffer cmd,VkImage image,const char* kind,const VkClearDepthStencilValue* value) noexcept;
#include "attachment_trace.hpp"
#include "queue_trace.hpp"
#include "session_fixture.hpp"
#include "diagnostic_readback.hpp"
#include "gpu_timing.hpp"
#include "gi_reproject.hpp"
#include "eye_motion_fix.hpp"
#include "shadow_share.hpp"
#include "atmosphere_share.hpp"
void desktopMirrorSource(VkDevice device,VkImage right) noexcept;
void desktopMirrorOwnedSource(VkDevice device,VkImage image,uint32_t width,uint32_t height) noexcept;
#include "native_xr.hpp"
#include "desktop_mirror.hpp"
#include "mod_update_check.hpp"
#include "screen_xr.hpp"
extern "C"
    __declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL evrGetInstanceProcAddr(VkInstance, const char *);
extern "C" __declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL evrGetDeviceProcAddr(VkDevice, const char *);
VKAPI_ATTR VkResult VKAPI_CALL evrEnumerateInstanceExtensionProperties(const char *layer, uint32_t *count,
                                                                       VkExtensionProperties *)
{
    if (layer && std::strcmp(layer, "VK_LAYER_EVR_diagnostic"))
        return VK_ERROR_LAYER_NOT_PRESENT;
    if (!count)
        return VK_ERROR_INITIALIZATION_FAILED;
    *count = 0;
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL evrEnumerateDeviceExtensionProperties(VkPhysicalDevice physical,
                                                                     const char *layer, uint32_t *count,
                                                                     VkExtensionProperties *props)
{
    if (layer)
    {
        if (std::strcmp(layer, "VK_LAYER_EVR_diagnostic"))
            return VK_ERROR_LAYER_NOT_PRESENT;
        *count = 0;
        return VK_SUCCESS;
    }
    InstanceDispatch dispatch;
    {
        std::lock_guard<std::mutex> l(mutex);
        auto it = instances.find(key(physical));
        if (it == instances.end())
            return VK_ERROR_INITIALIZATION_FAILED;
        dispatch = it->second;
    }
    return reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(dispatch.gipa(
        dispatch.instance, "vkEnumerateDeviceExtensionProperties"))(physical, nullptr, count, props);
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateInstance(const VkInstanceCreateInfo *info,
                                                 const VkAllocationCallbacks *allocator, VkInstance *result)
{
    note("create_instance_enter");
    diagnostic::cpuArena::arm(&note);
    auto *link = reinterpret_cast<VkLayerInstanceCreateInfo *>(const_cast<void *>(info->pNext));
    while (link && (link->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                    link->function != VK_LAYER_LINK_INFO))
        link = reinterpret_cast<VkLayerInstanceCreateInfo *>(const_cast<void *>(link->pNext));
    if (!link)
        return VK_ERROR_INITIALIZATION_FAILED;
    auto next = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    auto create = reinterpret_cast<PFN_vkCreateInstance>(next(VK_NULL_HANDLE, "vkCreateInstance"));
    auto *nextLayer = link->u.pLayerInfo->pNext;
    link->u.pLayerInfo = nextLayer;
    const auto *original = info;
    VkInstanceCreateInfo modified = *info;
    VulkanExtensionPlan plan;
    std::vector<const char *> pointers;
    std::shared_ptr<diagnostic::XrPreflight> preflight;
    bool fixture = false;
    try
    {
        if (preflightCandidate(*info, fixture))
        {
            preflight = std::make_shared<diagnostic::XrPreflight>();
            preflight->validateApi(info->pApplicationInfo ? info->pApplicationInfo->apiVersion : 0);
            // Pre-instance layer entry points may use the loader's four-argument
            // chain ABI, despite being returned by a downstream GIPA (RenderDoc
            // 1.46 does this). Only the public loader export has the standard
            // three-argument Vulkan ABI. Never call the downstream pointer here.
            const auto loader=GetModuleHandleW(L"vulkan-1.dll");
            auto enumerate = loader?reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(GetProcAddress(loader,"vkEnumerateInstanceExtensionProperties")):nullptr;
            if (!enumerate) throw std::runtime_error("instance extension enumerator unavailable");
            const auto supported = supportedVulkanExtensions([&](uint32_t *count, VkExtensionProperties *p) { return enumerate(nullptr, count, p); });
            plan = planVulkanExtensions(diagnostic::extensionNames(info->enabledExtensionCount, info->ppEnabledExtensionNames), preflight->instanceExtensions, supported);
            if (!plan.compatible()) throw std::runtime_error("Runtime instance extension unsupported");
            pointers = diagnostic::extensionPointers(plan.enabled);
            modified.enabledExtensionCount = uint32_t(pointers.size()); modified.ppEnabledExtensionNames = pointers.data();
            info = &modified;
            note("preflight_runtime", "\"name\":" + jsonString(preflight->properties.runtimeName) +
                 ",\"version\":" + std::to_string(preflight->properties.runtimeVersion) + ",\"session_created\":false");
        }
    }
    catch (const std::exception &e)
    { preflight.reset(); info = original; note("preflight_fallback", "\"phase\":\"instance_plan\",\"reason\":" + jsonString(e.what())); }
    auto r = create(info, allocator, result);
    if (r != VK_SUCCESS && info != original)
    {
        note("preflight_fallback", "\"phase\":\"instance_create\",\"result\":" + std::to_string(r));
        preflight.reset(); info = original; link->u.pLayerInfo = nextLayer;
        r = create(info, allocator, result);
    }
    if (r == VK_SUCCESS)
    {
        {
            std::lock_guard<std::mutex> l(mutex);
            instances[key(*result)] = {*result, next, preflight, fixture};
            if (preflight)
            {
                auto &record = instances.at(key(*result));
                record.apiVersion = info->pApplicationInfo ? info->pApplicationInfo->apiVersion : VK_API_VERSION_1_0;
                record.enabledExtensions = diagnostic::extensionNames(info->enabledExtensionCount, info->ppEnabledExtensionNames);
            }
        }
        std::ostringstream fields;
        fields << handleField("instance", *result) << ",\"api\":" << (info->pApplicationInfo ? info->pApplicationInfo->apiVersion : 0)
               << ",\"extensions\":[";
        for (uint32_t i = 0; i < info->enabledExtensionCount; i++)
        {
            if (i)
                fields << ',';
            fields << jsonString(info->ppEnabledExtensionNames[i]);
        }
        fields << ']';
        fields << ",\"openxr_extensions_prepared\":" << (preflight ? "true" : "false");
        note("create_instance", fields.str());
    }
    return r;
}
VKAPI_ATTR void VKAPI_CALL evrDestroyInstance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    std::shared_ptr<diagnostic::XrPreflight> preflight;
    auto fn = instanceFn<PFN_vkDestroyInstance>(instance, "vkDestroyInstance");
    {
        std::lock_guard<std::mutex> l(mutex);
        auto it = instances.find(key(instance));
        if (it != instances.end()) { preflight = std::move(it->second.preflight); instances.erase(it); }
    }
    preflight.reset(); // outside mutex: runtime teardown may call Vulkan
    if (fn)
        fn(instance, allocator);
    note("destroy_instance", handleField("instance", instance));
}
// The terrain bricks' frame arena of enshrouded.exe, three times as large (2026-10-18; hooks/world_shared_stages.hpp
// terrain::arena): the other eye runs its own terrain update, and two Views' requests a frame overran the native 8 MB
// (0x7d0000, set at 0x4f845 when the brick system is made, after the device). The hook library comes too late for that (it
// is loaded after the start; Mock game-20261018-162238: patched there, the arena was still 8 MB), so the layer does it here,
// before the game's device exists. Only where the eight bytes are the expected instruction; EVR_TERRAIN_ARENA=0 or
// EVR_EYE_TERRAIN=0: left as it is.
static void enlargeTerrainArena()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    auto is = [](const char *name, char c) { char v[8]{}; const auto n = GetEnvironmentVariableA(name, v, sizeof v); return n == 1 && v[0] == c; };
    if (is("EVR_TERRAIN_ARENA", '0') || !is("EVR_EYE_TERRAIN", '1'))   // only with the other eye's own terrain update (off by default)
        return;
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
        return;
    const std::wstring exe(path);
    const std::wstring name = exe.substr(exe.find_last_of(L"\\/") + 1);
    if (_wcsicmp(name.c_str(), L"enshrouded.exe"))
        return;
    auto *at = reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr)) + 0x4f845;
    const uint8_t expected[8]{0x48, 0xc7, 0x45, 0xe7, 0x00, 0x00, 0x7d, 0x00};
    const uint32_t bytes = 0x1770000;
    if (std::memcmp(at, expected, 8))
    {
        note("terrain_arena_left", "\"reason\":\"the creation's bytes differ\"");
        return;
    }
    DWORD old = 0;
    if (!VirtualProtect(at + 4, 4, PAGE_EXECUTE_READWRITE, &old))
        return;
    std::memcpy(at + 4, &bytes, 4);
    DWORD ignored = 0;
    VirtualProtect(at + 4, 4, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, 8);
    note("terrain_arena_enlarged", "\"site\":\"0x4f845\",\"native_bytes\":8192000,\"bytes\":24576000,\"by\":\"layer\"");
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo *info,
                                               const VkAllocationCallbacks *allocator, VkDevice *result)
{
    enlargeTerrainArena();
    auto *link = reinterpret_cast<VkLayerDeviceCreateInfo *>(const_cast<void *>(info->pNext));
    while (link && (link->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                    link->function != VK_LAYER_LINK_INFO))
        link = reinterpret_cast<VkLayerDeviceCreateInfo *>(const_cast<void *>(link->pNext));
    if (!link)
        return VK_ERROR_INITIALIZATION_FAILED;
    auto nextI = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    auto nextD = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    VkInstance instance{};
    std::shared_ptr<diagnostic::XrPreflight> preflight;
    bool fixture = false;
    {
        std::lock_guard<std::mutex> l(mutex);
        auto it = instances.find(key(physical));
        if (it == instances.end())
            return VK_ERROR_INITIALIZATION_FAILED;
        instance = it->second.instance;
        preflight = it->second.preflight;
        fixture = it->second.fixture;
    }
    auto create = reinterpret_cast<PFN_vkCreateDevice>(nextI(instance, "vkCreateDevice"));
    auto *nextLayer = link->u.pLayerInfo->pNext;
    link->u.pLayerInfo = nextLayer;
    const auto *original = info;
    VkDeviceCreateInfo modified = *info;
    VkPhysicalDeviceFeatures faultFeatures{};
    VulkanExtensionPlan plan;
    std::vector<const char *> pointers;
    bool prepared = false;
    // The mod's own frames for every refresh (include/evr/afw_pacing.hpp, pacing 2 at the start; 2026-10-23): one more queue
    // in the game's first family, the XR binding's - the refreshes' warps run beside the game's frames instead of after them.
    std::vector<VkDeviceQueueCreateInfo> ownQueueInfos;std::vector<float> ownQueuePriorities;bool ownQueue = false;
    try
    {
        if (preflight)
        {
            // The layer's VkInstance can be wrapped and is not necessarily a
            // public loader handle. Required GPU matching is performed by the
            // explicit XR entry after vkCreateDevice returns to the application.
            note("preflight_gpu_validation_deferred",handleField("instance",instance)+","+handleField("physical",physical));
            auto enumerate = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(nextI(instance, "vkEnumerateDeviceExtensionProperties"));
            if (!enumerate) throw std::runtime_error("device extension enumerator unavailable");
            const auto supported = supportedVulkanExtensions([&](uint32_t *count, VkExtensionProperties *p) { return enumerate(physical, nullptr, count, p); });
            plan = planVulkanExtensions(diagnostic::extensionNames(info->enabledExtensionCount, info->ppEnabledExtensionNames), preflight->deviceExtensions, supported);
            if (!plan.compatible()) throw std::runtime_error("Runtime device extension unsupported");
            const char *fail = std::getenv("EVR_PREPARE_FAIL_DEVICE");
            if (fixture && fail && !std::strcmp(fail, "1"))
            {
                auto getFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(nextI(instance, "vkGetPhysicalDeviceFeatures"));
                if (!getFeatures) throw std::runtime_error("fixture feature enumerator absent");
                VkPhysicalDeviceFeatures supportedFeatures{};
                getFeatures(physical, &supportedFeatures);
                bool found = false;
                const auto metadata = diagnostic::coreFeatureType();
                for (size_t i = 0; i < metadata.count; ++i)
                {
                    VkBool32 supportedValue;
                    std::memcpy(&supportedValue, reinterpret_cast<const char *>(&supportedFeatures) + metadata.fields[i].offset, sizeof(supportedValue));
                    if (!supportedValue)
                    {
                        const VkBool32 enabled = VK_TRUE;
                        std::memcpy(reinterpret_cast<char *>(&faultFeatures) + metadata.fields[i].offset, &enabled, sizeof(enabled));
                        found = true;
                        note("preflight_fixture_failure_injection", "\"unsupported_feature\":" + jsonString(metadata.fields[i].name));
                        break;
                    }
                }
                if (!found) throw std::runtime_error("fixture could not find unsupported core feature");
                modified.pEnabledFeatures = &faultFeatures;
            }
            pointers = diagnostic::extensionPointers(plan.enabled);
            modified.enabledExtensionCount = uint32_t(pointers.size()); modified.ppEnabledExtensionNames = pointers.data();
            info = &modified; prepared = true;
            // With the alternate eyes at all (2026-10-23 11:30; the user chose "fill" in the game's VR settings in two starts that had
            // begun with another pacing - it waited for the next start, and the pacing chosen after it was the one kept): the queue
            // is made for every start with them, and "fill" can be chosen in the game at any time.
            const char *pacing = std::getenv("EVR_AFW_PACING");const char *alternate = std::getenv("EVR_ALTERNATE_EYES");
            if (((pacing && !std::strcmp(pacing, "2")) || (alternate && !std::strcmp(alternate, "1"))) && modified.queueCreateInfoCount && !modified.pQueueCreateInfos[0].flags && modified.pQueueCreateInfos[0].pQueuePriorities)
            {
                auto getQueues = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(nextI(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
                uint32_t count = 0;if (getQueues) getQueues(physical, &count, nullptr);
                std::vector<VkQueueFamilyProperties> families(count);if (count) getQueues(physical, &count, families.data());
                const auto &first = modified.pQueueCreateInfos[0];
                if (first.queueFamilyIndex < count && first.queueCount < families[first.queueFamilyIndex].queueCount)
                {
                    ownQueueInfos.assign(modified.pQueueCreateInfos, modified.pQueueCreateInfos + modified.queueCreateInfoCount);
                    ownQueuePriorities.assign(first.pQueuePriorities, first.pQueuePriorities + first.queueCount);ownQueuePriorities.push_back(1.f);
                    ownQueueInfos[0].queueCount = first.queueCount + 1;ownQueueInfos[0].pQueuePriorities = ownQueuePriorities.data();
                    modified.pQueueCreateInfos = ownQueueInfos.data();ownQueue = true;
                }
                note("afw_own_queue", "\"added\":" + std::string(ownQueue ? "true" : "false") + ",\"family\":" + std::to_string(first.queueFamilyIndex) +
                    ",\"game_queues\":" + std::to_string(ownQueue ? first.queueCount : modified.pQueueCreateInfos[0].queueCount) + ",\"family_queues\":" + std::to_string(first.queueFamilyIndex < count ? families[first.queueFamilyIndex].queueCount : 0));
            }
        }
    }
    catch (const std::exception &e)
    { info = original; note("preflight_fallback", "\"phase\":\"device_plan\",\"reason\":" + jsonString(e.what())); }
    const char* bodyOption=std::getenv("EVR_BODY_CLIP_PREPARE");
    const bool bodyRequested=bodyOption&&!std::strcmp(bodyOption,"1");
    VkPhysicalDeviceFeatures bodySupported{};
    if(bodyRequested){
        auto query=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(nextI(instance,"vkGetPhysicalDeviceFeatures"));
        if(query)query(physical,&bodySupported);
    }
    BodyClipFeaturePlan bodyPlan(*info,bodyRequested,bodySupported.shaderClipDistance!=0);
    if(bodyRequested)note("body_clip_feature_plan","\"status\":"+jsonString(bodyPlan.status)+
        ",\"supported\":"+(bodySupported.shaderClipDistance?std::string("true"):std::string("false"))+
        ",\"caller_nodes_unchanged\":true");
    if(bodyPlan.changed())info=&bodyPlan.info;
    auto r = create(physical, info, allocator, result);
    if (r != VK_SUCCESS && info != original)
    {
        note("preflight_fallback", "\"phase\":\"device_create\",\"result\":" + std::to_string(r));
        info = original; prepared = false; link->u.pLayerInfo = nextLayer;
        r = create(physical, info, allocator, result);
    }
    if(bodyRequested)note("body_clip_feature_result","\"result\":"+std::to_string(r)+
        ",\"enabled\":"+std::string(r==VK_SUCCESS&&BodyClipFeaturePlan(*info,false,false).enabled()?"true":"false")+
        ",\"preparation_only_no_shader_change\":"+std::string(std::getenv("EVR_BODY_CLIP_SHADER")&&!std::strcmp(std::getenv("EVR_BODY_CLIP_SHADER"),"1")?"false":"true")+"");
    if (r == VK_SUCCESS)
    {
        {
            std::lock_guard<std::mutex> l(mutex);
            devices[key(*result)] = {*result, nextD, instance, physical, nextI};
            if (prepared && info->queueCreateInfoCount && !info->pQueueCreateInfos[0].flags)
            {
                const auto &parent = instances.at(key(physical));
                auto record = std::make_shared<BorrowedVulkan>();
                record->instance = instance; record->device = *result; record->physical = physical;
                record->apiVersion = parent.apiVersion;
                record->enabledInstanceExtensions = parent.enabledExtensions;
                record->enabledDeviceExtensions = diagnostic::extensionNames(info->enabledExtensionCount, info->ppEnabledExtensionNames);
                record->queueFamily = info->pQueueCreateInfos[0].queueFamilyIndex;
                record->createdQueueCount = info->pQueueCreateInfos[0].queueCount;
                if (ownQueue && record->createdQueueCount >= 2) { record->queueIndex = record->createdQueueCount - 1;record->ownQueue = true; }
                devices.at(key(*result)).fixtureBinding = record;
            }
        }
        const char* bodyShaders=std::getenv("EVR_BODY_CLIP_SHADER");
        if(bodyShaders&&!std::strcmp(bodyShaders,"1")){
            try{
                if(!body::featuresEnabled(*info))throw std::runtime_error("required body clip features not enabled");
                auto store=std::make_shared<body::PayloadStore>(*result,physical,instance,nextD,nextI);
                {std::lock_guard<std::mutex> lock(mutex);devices.at(key(*result)).bodyPayloads=std::move(store);}
                note("body_clip_device_ready","\"clip_distance\":true,\"shader_int64\":true,\"buffer_device_address\":true");
            }catch(const std::exception& e){note("body_clip_device_fallback","\"reason\":"+jsonString(e.what()));}
        }
        std::ostringstream fields;
        fields << handleField("instance", instance) << ',' << handleField("physical_device", physical)
               << ',' << handleField("device", *result) << ",\"game_rva_stack\":" << stack() << ",\"extensions\":[";
        for (uint32_t i = 0; i < info->enabledExtensionCount; ++i)
        {
            if (i) fields << ',';
            fields << jsonString(info->ppEnabledExtensionNames[i]);
        }
        fields << "],\"queues\":[";
        for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i)
        {
            if (i) fields << ',';
            const auto &queue = info->pQueueCreateInfos[i];
            fields << "{\"family\":" << queue.queueFamilyIndex << ",\"count\":" << queue.queueCount
                   << ",\"flags\":" << queue.flags << ",\"priorities\":[";
            for (uint32_t j = 0; j < queue.queueCount; ++j)
            {
                if (j) fields << ',';
                fields << queue.pQueuePriorities[j];
            }
            fields << "]}";
        }
        fields << ']';
        try { const auto metadata = diagnostic::deviceFeatures(*info); fields << ',' << metadata; }
        catch (...) { fields << ",\"feature_metadata_error\":true"; }
        fields << ",\"openxr_extensions_prepared\":" << (prepared ? "true" : "false");
        note("create_device", fields.str());
        try
        {
            auto getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(nextI(instance, "vkGetPhysicalDeviceProperties"));
            auto getQueues = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(nextI(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
            if (getProperties && getQueues)
            {
                VkPhysicalDeviceProperties properties{};
                getProperties(physical, &properties);
                uint32_t count = 0;
                getQueues(physical, &count, nullptr);
                if (count > 16384) throw std::runtime_error("unexpected queue family count");
                std::vector<VkQueueFamilyProperties> queues(count);
                getQueues(physical, &count, queues.data());
                if (count > queues.size()) throw std::runtime_error("queue family count grew");
                std::ostringstream details;
                details << handleField("device", *result) << ',' << handleField("physical_device", physical)
                        << ",\"name\":" << jsonString(properties.deviceName) << ",\"vendor_id\":" << properties.vendorID
                        << ",\"device_id\":" << properties.deviceID << ",\"api\":" << properties.apiVersion
                        << ",\"driver_version\":" << properties.driverVersion << ",\"queue_families\":[";
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (i) details << ',';
                    details << "{\"family\":" << i << ",\"flags\":" << queues[i].queueFlags
                            << ",\"count\":" << queues[i].queueCount << '}';
                }
                details << ']';
                note("physical_device_properties", details.str());
            }
        }
        catch (...) { note("device_metadata_error"); }
    }
    return r;
}
VKAPI_ATTR void VKAPI_CALL evrGetPhysicalDeviceProperties(VkPhysicalDevice physical, VkPhysicalDeviceProperties *properties)
{
    InstanceDispatch dispatch;
    {
        std::lock_guard<std::mutex> l(mutex);
        auto it = instances.find(key(physical));
        if (it == instances.end()) return;
        dispatch = it->second;
    }
    if (diagnostic::insideXrPreflight) diagnostic::preflightPhysicalAtLayer = physical;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(dispatch.gipa(dispatch.instance, "vkGetPhysicalDeviceProperties"));
    fn(physical, properties);
}
VKAPI_ATTR void VKAPI_CALL evrDestroyDevice(VkDevice device, const VkAllocationCallbacks *allocator)
{
    meshDrawGuard::summary();
    presentRetirement::pump(1);
    exitProgress::Scope exitTrace("destroy_device",reinterpret_cast<uint64_t>(device));
    auto fn = deviceFn<PFN_vkDestroyDevice>(device, "vkDestroyDevice");
    std::shared_ptr<body::PayloadStore> bodyStore;
    {std::lock_guard<std::mutex> lock(mutex);auto it=devices.find(key(device));if(it!=devices.end())bodyStore=it->second.bodyPayloads;}
    if(bodyStore){try{const auto n=bodyStore->releaseAtDeviceDestroy();note("body_payloads_released","\"allocations\":"+std::to_string(n)+",\"boundary\":\"native_device_destroy\"");}catch(const std::exception& e){note("body_payload_release_failure","\"reason\":"+jsonString(e.what()));}}
    if(liveNativeXr&&liveNativeXr->device==device){note("native_xr_closed_with_the_device","\"submitted_pairs\":"+std::to_string(liveNativeXr->submittedPairs));closeLiveNativeXr(false,true);}
    uiDraw::destroy(device);
    giReprojectDestroy(device);
    uiCapture::destroyDevice(device);
    desktopMirror::destroy(device);
    uiGlyph::destroy(device);
    {
        std::lock_guard<std::mutex> l(mutex);
        devices.erase(key(device));
        bufferAudits.erase(key(device));
        semaphoreTypes.erase(key(device));
        imageViews.erase(key(device));
        trackedImages.erase(key(device));
        nativeImages.erase(key(device)); imageCreation.erase(key(device));
        for(auto it=commandIdentities.begin();it!=commandIdentities.end();)
            if(key(it->first)==key(device))it=commandIdentities.erase(it);else ++it;
        for (auto it = queueFamilies.begin(); it != queueFamilies.end(); )
            if (key(it->first) == key(device)) it = queueFamilies.erase(it); else ++it;
    }
    if (fn)
        fn(device, allocator);
    exitTrace.end(0);
    note("destroy_device", handleField("device", device));
}
VKAPI_ATTR void VKAPI_CALL evrGetDeviceQueue(VkDevice d, uint32_t family, uint32_t index, VkQueue *q)
{
    deviceFn<PFN_vkGetDeviceQueue>(d, "vkGetDeviceQueue")(d, family, index, q);
    { std::lock_guard<std::mutex> l(mutex); queueFamilies[*q] = family; }
    note("get_device_queue", handleField("device", d) + ',' + handleField("queue", *q) +
         ",\"family\":" + std::to_string(family) + ",\"index\":" + std::to_string(index) +
         ",\"thread\":" + std::to_string(GetCurrentThreadId()));
}
VKAPI_ATTR void VKAPI_CALL evrGetDeviceQueue2(VkDevice d, const VkDeviceQueueInfo2 *info, VkQueue *q)
{
    deviceFn<PFN_vkGetDeviceQueue2>(d, "vkGetDeviceQueue2")(d, info, q);
    { std::lock_guard<std::mutex> l(mutex); if (*q) queueFamilies[*q] = info->queueFamilyIndex; }
    note("get_device_queue", handleField("device", d) + ',' + handleField("queue", *q) +
         ",\"family\":" + std::to_string(info->queueFamilyIndex) + ",\"index\":" + std::to_string(info->queueIndex) +
         ",\"flags\":" + std::to_string(info->flags) + ",\"thread\":" + std::to_string(GetCurrentThreadId()));
}
void rememberSwap(VkDevice d, VkSwapchainKHR swap, const VkSwapchainCreateInfoKHR *info)
{
    uint32_t n = 0;
    auto fn = deviceFn<PFN_vkGetSwapchainImagesKHR>(d, "vkGetSwapchainImagesKHR");
    if (fn(d, swap, &n, nullptr) != VK_SUCCESS)
        return;
    SwapData data{info->imageFormat, info->imageExtent.width, info->imageExtent.height, info->imageUsage, {}};
    data.images.resize(n);
    if (fn(d, swap, &n, data.images.data()) != VK_SUCCESS)
        return;
    std::lock_guard<std::mutex> l(mutex);
    swapData[swap] = std::move(data);
}
void maybeCapture(VkQueue q, const VkPresentInfoKHR *p, VkPresentInfoKHR &replacement)
{
    if (p->swapchainCount != 1 || presents.load() % 30)
        return;
    try
    {
        std::string destination;
        SwapData sc;
        DeviceDispatch dd;
        uint32_t family;
        {
            std::lock_guard<std::mutex> l(mutex);
            if (output.empty() || !std::filesystem::exists(output + "/capture.request"))
                return;
            auto si = swapData.find(p->pSwapchains[0]);
            auto qi = queueFamilies.find(q);
            auto di = devices.find(key(q));
            if (si == swapData.end() || qi == queueFamilies.end() || di == devices.end())
                return;
            sc = si->second;
            family = qi->second;
            dd = di->second;
            destination = output + "/game_" + std::to_string(presents.load()) + ".ppm";
        }
        if (!(sc.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || p->pImageIndices[0] >= sc.images.size() ||
            (sc.format != VK_FORMAT_R8G8B8A8_UNORM && sc.format != VK_FORMAT_R8G8B8A8_SRGB &&
             sc.format != VK_FORMAT_B8G8R8A8_UNORM && sc.format != VK_FORMAT_B8G8R8A8_SRGB))
        {
            note("capture_unsupported");
            return;
        }
        Vulkan v;
        // Instance/physical handles stored during layer creation belong to this
        // chain level. A wrapping layer (RenderDoc) can make the stored instance
        // invalid at the public loader; keep physical queries in the same chain.
        // Device dispatch must still use the PUBLIC loader so newly allocated
        // command buffers receive their loader dispatch data.
        v.gipa=dd.gipa;
        v.bindInstance(dd.instance);
        v.vkGetDeviceProcAddr=reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(v.module,"vkGetDeviceProcAddr"));
        if(!v.vkGetDeviceProcAddr)throw std::runtime_error("capture public device dispatcher unavailable");
        v.bindDevice(dd.device,dd.physical,family);
        note("capture_dispatch_binding",handleField("layer_instance",dd.instance)+','+handleField("layer_physical",dd.physical)+
            ','+handleField("device",dd.device)+",\"public_device_dispatch\":true,\"instance_queries_below_layer\":true");
        bool consumed = false;
        bool ok = capturePresent(v, q, sc.images[p->pImageIndices[0]], sc.format, sc.width, sc.height, p,
                                 destination, consumed);
        if (consumed)
        {
            replacement.waitSemaphoreCount = 0;
            replacement.pWaitSemaphores = nullptr;
        }
        note(ok ? "game_capture" : "capture_failed", "\"path\":" + jsonString(destination));
        std::filesystem::remove(output + "/capture.request");
    }
    catch (const std::exception &e)
    {
        note("capture_exception", "\"reason\":" + jsonString(e.what()));
        std::error_code ignored;
        std::filesystem::remove(output + "/capture.request", ignored);
    }
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateSwapchainKHR(VkDevice d, const VkSwapchainCreateInfoKHR *info,
                                                     const VkAllocationCallbacks *a, VkSwapchainKHR *s)
{
    auto fn = deviceFn<PFN_vkCreateSwapchainKHR>(d, "vkCreateSwapchainKHR");
    // Desktop mirror: the right-eye blit needs TRANSFER_DST (and the diagnostic capture TRANSFER_SRC) on the swapchain images.
    VkSwapchainCreateInfoKHR mirrorInfo{};
    if (info && desktopMirror::requested() && (info->imageUsage & (VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) != (VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) { mirrorInfo = *info; mirrorInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT; }
    auto r = mirrorInfo.sType ? fn(d, &mirrorInfo, a, s) : fn(d, info, a, s);
    if (mirrorInfo.sType) { note("desktop_mirror_swapchain_usage", "\"usage\":" + std::to_string(mirrorInfo.imageUsage) + ",\"result\":" + std::to_string(r));
        if (r != VK_SUCCESS) { r = fn(d, info, a, s); mirrorInfo = {}; } else info = &mirrorInfo; }
    if (r == VK_SUCCESS)
        rememberSwap(d, *s, info);
    note("game_swapchain", "\"width\":" + std::to_string(info->imageExtent.width) +
                               ',' + handleField("swapchain", r == VK_SUCCESS ? *s : VK_NULL_HANDLE) +
                               ",\"height\":" + std::to_string(info->imageExtent.height) +
                               ",\"format\":" + std::to_string(info->imageFormat) +
                               ",\"usage\":" + std::to_string(info->imageUsage) +
                               ",\"result\":" + std::to_string(r) + ",\"game_rva_stack\":" + stack());
    return r;
}
VKAPI_ATTR VkResult VKAPI_CALL evrGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images)
{
    const auto capacity = images && count ? *count : 0;
    const auto result = deviceFn<PFN_vkGetSwapchainImagesKHR>(device, "vkGetSwapchainImagesKHR")(device, swapchain, count, images);
    if (images && (result == VK_SUCCESS || result == VK_INCOMPLETE)) try
    {
        std::ostringstream out;
        out << handleField("swapchain", swapchain) << ",\"result\":" << result << ",\"images\":[";
        const auto returned = std::min({capacity, *count, 32u});
        for (uint32_t i = 0; i < returned; ++i) { if (i) out << ','; out << handleText(images[i]); }
        out << "],\"truncated\":" << (*count > returned ? "true" : "false") << ",\"game_rva_stack\":" << stack();
        note("swapchain_images", out.str());
    } catch (...) { note("swapchain_image_metadata_error"); }
    return result;
}
VKAPI_ATTR void VKAPI_CALL evrDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocator)
{
    auto fn = deviceFn<PFN_vkDestroySwapchainKHR>(device, "vkDestroySwapchainKHR");
    fn(device, swapchain, allocator);
    { std::lock_guard<std::mutex> lock(mutex); swapData.erase(swapchain); }
    note("destroy_swapchain", handleField("swapchain", swapchain));
}
VKAPI_ATTR VkResult VKAPI_CALL evrQueuePresentKHR(VkQueue q, const VkPresentInfoKHR *p)
{
    exitProgress::Scope exitTrace("present",reinterpret_cast<uint64_t>(q));
    diagnostic::cpuArena::retireHook();
    auto fn = deviceFn<PFN_vkQueuePresentKHR>(q, "vkQueuePresentKHR");
    VkPresentInfoKHR replacement = *p;
    // The plate in the headset (screen_xr.hpp) takes the game's own picture, before the mirror writes over it.
    VkSemaphore screenWait{};
    { QueueCallScope screenContext("screen_xr"); screenXr::present(q, p, replacement, screenWait); }
    // Mirror first so a requested diagnostic capture records what the desktop actually shows.
    VkSemaphore mirrorWait{};
    { QueueCallScope mirrorContext("desktop_mirror"); desktopMirror::apply(q, p, replacement, mirrorWait); }
    { QueueCallScope captureContext("diagnostic_capture"); maybeCapture(q, &replacement, replacement); }
    VkResult r;
    { QueueCallScope presentContext("downstream_present"); r = fn(q, &replacement); }
    exitTrace.end(r);
    presentRetirement::pump(0);
    uiDraw::presented(q,p,r);
    auto f = ++presents;
    pollTraceRequest(f);
    auto d = draws.exchange(0), i = indirects.exchange(0), c = dispatches.exchange(0);
    if (f <= 10 || f % 120 == 0)
        note("game_frame", "\"frame\":" + std::to_string(f) + ",\"direct_draws\":" + std::to_string(d) +
                               ",\"indirect_draws\":" + std::to_string(i) +
                               ",\"compute_dispatches\":" + std::to_string(c) +
                               ",\"result\":" + std::to_string(r) + ",\"game_rva_stack\":" + stack());
    return r;
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateShaderModule(VkDevice d, const VkShaderModuleCreateInfo *info,
                                                     const VkAllocationCallbacks *a, VkShaderModule *module)
{
    auto fn = deviceFn<PFN_vkCreateShaderModule>(d, "vkCreateShaderModule");
    VkResult r=VK_ERROR_UNKNOWN;bool replaced=false;
    try{
        const auto code=shadowReceiver::load(*info);
        if(!code.empty()){
            auto changed=*info;changed.pCode=code.data();changed.codeSize=code.size()*4;
            r=fn(d,&changed,a,module);
            if(r!=VK_SUCCESS)throw std::runtime_error("shadow receiver shader module creation failed");
            replaced=true;
            note("shadow_receiver_shader_patched","\"source_sha256\":"+jsonString(shadowReceiver::sourceSha)+
                ",\"shader_sha256\":"+jsonString(head::sha256(code.data(),code.size()*4))+
                ",\"bytes\":"+std::to_string(changed.codeSize)+",\"scope\":\"reviewed sun visibility, all views\"");
        }
    }catch(const std::exception& e){note("shadow_receiver_shader_failed","\"reason\":"+jsonString(e.what()));}
    if(!replaced)try{
        const auto code=giProbe::load(*info);
        if(!code.empty()){
            auto changed=*info;changed.pCode=code.data();changed.codeSize=code.size()*4;
            r=fn(d,&changed,a,module);
            if(r!=VK_SUCCESS)throw std::runtime_error("GI probe shader module creation failed");
            replaced=true;
            note("gi_probe_shader_patched","\"source_sha256\":"+jsonString(giProbe::sourceSha)+
                ",\"shader_sha256\":"+jsonString(head::sha256(code.data(),code.size()*4))+
                ",\"bytes\":"+std::to_string(changed.codeSize)+",\"scope\":\"reviewed GI empty-probe fallback, all views\"");
        }
    }catch(const std::exception& e){note("gi_probe_shader_failed","\"reason\":"+jsonString(e.what()));}
    // The water reflection's screen-edge band, narrowed (ssr_edge_shader.hpp; EVR_SSR_EDGE_PERCENT=100 keeps the game's).
    if(!replaced)try{
        const auto code=ssrEdge::patch(*info);
        if(!code.empty()){
            auto changed=*info;changed.pCode=code.data();changed.codeSize=code.size()*4;
            r=fn(d,&changed,a,module);
            if(r!=VK_SUCCESS)throw std::runtime_error("SSR edge shader module creation failed");
            replaced=true;
            note("ssr_edge_shader_patched","\"source_sha256\":"+jsonString(ssrEdge::sourceSha)+",\"percent\":"+std::to_string(ssrEdge::percent())+
                ",\"bytes\":"+std::to_string(changed.codeSize));
        }
    }catch(const std::exception& e){note("ssr_edge_shader_failed","\"reason\":"+jsonString(e.what()));}
    const char* headOptIn=std::getenv("EVR_HEAD_VISIBILITY_SHADER");
    if(headOptIn&&std::strcmp(headOptIn,"1")==0&&info->codeSize==head::sourceBytes){
        try{
            if(head::sha256(info->pCode,info->codeSize)==head::sourceSha){
                const auto flag=std::getenv("EVR_HANDS_ONLY");const bool handsOnly=flag&&!std::strcmp(flag,"1");
                const auto code=handsOnly?appearance::patch(info->pCode,info->codeSize):head::patch(info->pCode,info->codeSize);auto changed=*info;
                changed.pCode=code.data();changed.codeSize=code.size()*4;r=fn(d,&changed,a,module);
                if(r!=VK_SUCCESS)throw std::runtime_error("dynamic culling shader module creation failed");
                replaced=true;headShaderModules.fetch_add(1);
                note("head_shader_patched","\"hands_only\":"+std::string(handsOnly?"true":"false")+",\"source_sha256\":"+jsonString(head::sourceSha)+",\"patched_sha256\":"+jsonString(head::sha256(code.data(),code.size()*4))+
                    ",\"bytes\":"+std::to_string(changed.codeSize)+",\"module\":"+handleText(*module)+",\"native_zero_payload_is_inactive\":true");
            }
        }catch(const std::exception& e){headShaderFailed=true;note("head_shader_patch_failure","\"reason\":"+jsonString(e.what()));}
    }
    const char* vfxOptIn=std::getenv("EVR_VFX_TRANSPARENT_SHADER");
    if(!replaced&&vfxOptIn&&std::strcmp(vfxOptIn,"1")==0&&info->codeSize==2720){
        try{
            if(head::sha256(info->pCode,info->codeSize)==vfx::writebackSha){
                const auto code=vfx::patchWriteback(info->pCode,info->codeSize);auto changed=*info;
                changed.pCode=code.data();changed.codeSize=code.size()*4;r=fn(d,&changed,a,module);
                if(r!=VK_SUCCESS)throw std::runtime_error("VFX writeback shader module creation failed");
                replaced=true;vfxShaderModules.fetch_add(1);
                note("vfx_transparent_shader_patched","\"source_sha256\":"+jsonString(vfx::writebackSha)+
                    ",\"patched_sha256\":"+jsonString(head::sha256(code.data(),code.size()*4))+
                    ",\"bytes\":"+std::to_string(changed.codeSize)+",\"native_four_byte_payload_preserved\":true");
            }
        }catch(const std::exception& e){vfxShaderFailed=true;note("vfx_transparent_shader_failure","\"reason\":"+jsonString(e.what()));}
    }
    // The decal writeback (hooks/vfx_decal_rebase.hpp): our own shader with the same bindings stands in for the
    // game's. With the game's payload of one word it does what the game's does; EVR_VFX_DECAL_SHADER=0 keeps the game's.
    if(!replaced&&vfxOptIn&&std::strcmp(vfxOptIn,"1")==0&&info->codeSize==vfx::decalWritebackBytes){
        const char* decalOptOut=std::getenv("EVR_VFX_DECAL_SHADER");
        if(!(decalOptOut&&!std::strcmp(decalOptOut,"0")))try{
            if(head::sha256(info->pCode,info->codeSize)==vfx::decalWritebackSha){
                auto changed=*info;changed.pCode=vfxDecalWriteback;changed.codeSize=sizeof(vfxDecalWriteback);r=fn(d,&changed,a,module);
                if(r!=VK_SUCCESS)throw std::runtime_error("VFX decal writeback shader module creation failed");
                replaced=true;vfxDecalShaderModules.fetch_add(1);
                note("vfx_decal_shader_replaced","\"source_sha256\":"+jsonString(vfx::decalWritebackSha)+
                    ",\"shader_sha256\":"+jsonString(head::sha256(vfxDecalWriteback,sizeof(vfxDecalWriteback)))+
                    ",\"bytes\":"+std::to_string(changed.codeSize)+",\"one_word_payload_is_the_native_copy\":true");
            }
        }catch(const std::exception& e){note("vfx_decal_shader_failure","\"reason\":"+jsonString(e.what()));}
    }
    if(!replaced&&(info->codeSize==31688||info->codeSize==55756)){
        std::shared_ptr<body::PayloadStore> store;
        {std::lock_guard<std::mutex> lock(mutex);auto it=devices.find(key(d));if(it!=devices.end())store=it->second.bodyPayloads;}
        if(store)try{
            const bool color=info->codeSize==55756;const auto expected=color?body::colorSha:body::depthSha;
            if(head::sha256(info->pCode,info->codeSize)==expected){
                const auto code=body::patch(info->pCode,info->codeSize,color);auto changed=*info;changed.pCode=code.data();changed.codeSize=code.size()*4;
                r=fn(d,&changed,a,module);if(r!=VK_SUCCESS)throw std::runtime_error("body shader creation failed");replaced=true;store->shaderMask.fetch_or(color?2:1);
                note("body_shader_patched","\"kind\":"+jsonString(color?"color":"depth")+",\"source_sha256\":"+jsonString(expected)+",\"patched_sha256\":"+jsonString(head::sha256(code.data(),code.size()*4))+",\"zero_payload_inactive\":true");
            }
        }catch(const std::exception& e){store->failed=true;note("body_shader_patch_failure","\"reason\":"+jsonString(e.what()));}
    }
    if(!replaced)r=fn(d,info,a,module);
    if(r==VK_SUCCESS)uiCapture::shaderCreated(d,info,*module);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < info->codeSize / 4; i++)
    {
        h ^= info->pCode[i];
        h *= 1099511628211ull;
    }
    auto id = ++shaderCount;
    std::ostringstream hs;
    hs << std::hex << h;
    note("shader", "\"module\":" + handleText(r == VK_SUCCESS ? *module : VK_NULL_HANDLE) +
                       ",\"id\":" + std::to_string(id) + ",\"hash\":" + jsonString(hs.str()) +
                       ",\"bytes\":" + std::to_string(info->codeSize));
    try
    {
        std::lock_guard<std::mutex> l(mutex);
        if (saveShaders && shaderBytes + info->codeSize < 64 * 1024 * 1024)
        {
            std::ofstream file(output + "/shader_" + hs.str() + ".spv", std::ios::binary);
            file.write(reinterpret_cast<const char *>(info->pCode), info->codeSize);
            shaderBytes += info->codeSize;
        }
    }
    catch (...)
    {
    }
    if(r==VK_SUCCESS&&module)giReprojectModule(*module,info);
    return r;
}
extern "C" __declspec(dllexport) int evrBodyClipPayload(VkDevice device,const body::Payload* payload,uint64_t* address){
    if(address)*address=0;if(!payload||!address)return -1;
    try{
        std::shared_ptr<body::PayloadStore> store;{std::lock_guard<std::mutex> lock(mutex);auto it=devices.find(key(device));if(it!=devices.end())store=it->second.bodyPayloads;}
        if(!store)throw std::runtime_error("body payload device not prepared");bool created=false;*address=store->get(*payload,created);
        if(created)note("body_payload_created","\"address\":"+handleText(*address)+",\"models\":"+std::to_string(payload->count)+",\"distance\":"+std::to_string(payload->distance)+",\"tag\":"+std::to_string(payload->tag)+",\"hand_plus_one\":"+std::to_string(payload->reserved)+",\"extra_plus_one\":"+std::to_string(body::appearanceExtra(*payload))+",\"shield_plus_one\":"+std::to_string(body::appearanceShield(*payload))+",\"bytes\":112,\"immutable\":true");return 0;
    }catch(const std::exception& e){note("body_payload_fallback","\"reason\":"+jsonString(e.what()));return -1;}
}
extern "C" __declspec(dllexport) uint32_t evrHandsOnlyShaderStatus(){const auto flag=std::getenv("EVR_HANDS_ONLY");return flag&&!std::strcmp(flag,"1")&&!headShaderFailed.load()?headShaderModules.load():0;}
extern "C" __declspec(dllexport) uint32_t evrHeadVisibilityShaderStatus(){return headShaderFailed.load()?0:headShaderModules.load();}
extern "C" __declspec(dllexport) uint32_t evrVfxTransparentShaderStatus(){return vfxShaderFailed.load()?0:vfxShaderModules.load();}
extern "C" __declspec(dllexport) uint32_t evrVfxDecalShaderStatus(){return vfxDecalShaderModules.load();}
VKAPI_ATTR void VKAPI_CALL evrCmdDraw(VkCommandBuffer c, uint32_t a, uint32_t b, uint32_t d, uint32_t e)
{
    draws++;
    sweepDepthDraw(c,a<=6?2:1);gpuTimingNote(c,0);uiDraw::draw(c,true);
    deviceFn<PFN_vkCmdDraw>(c, "vkCmdDraw")(c, a, b, d, e);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndexed(VkCommandBuffer c, uint32_t a, uint32_t b, uint32_t d, int32_t e,
                                             uint32_t f)
{
    draws++;
    sweepDepthDraw(c,a<=6?2:1);gpuTimingNote(c,0);uiDraw::draw(c,true);
    deviceFn<PFN_vkCmdDrawIndexed>(c, "vkCmdDrawIndexed")(c, a, b, d, e, f);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndirect(VkCommandBuffer c, VkBuffer b, VkDeviceSize o, uint32_t n,
                                              uint32_t s)
{
    indirects++;cmdStack("draw_indirect","\"draws\":"+std::to_string(n));
    sweepDepthDraw(c);gpuTimingNote(c,0);uiDraw::draw(c,false);
    deviceFn<PFN_vkCmdDrawIndirect>(c, "vkCmdDrawIndirect")(c, b, o, n, s);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndexedIndirect(VkCommandBuffer c, VkBuffer b, VkDeviceSize o,
                                                     uint32_t n, uint32_t s)
{
    indirects++;cmdStack("draw_indexed_indirect","\"draws\":"+std::to_string(n));
    sweepDepthDraw(c);gpuTimingNote(c,0);uiDraw::draw(c,false);
    deviceFn<PFN_vkCmdDrawIndexedIndirect>(c, "vkCmdDrawIndexedIndirect")(c, b, o, n, s);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDispatch(VkCommandBuffer c, uint32_t x, uint32_t y, uint32_t z)
{
    dispatches++;gpuTimingDispatch(c,x,y,z);cmdStack("dispatch","\"d\":0");giReprojectDispatch(c,x,y,z);
    deviceFn<PFN_vkCmdDispatch>(c, "vkCmdDispatch")(c, x, y, z);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDispatchIndirect(VkCommandBuffer c, VkBuffer b, VkDeviceSize o)
{
    dispatches++;gpuTimingNote(c,1);cmdStack("dispatch_indirect","\"d\":0");
    deviceFn<PFN_vkCmdDispatchIndirect>(c, "vkCmdDispatchIndirect")(c, b, o);
}
// Eye depth write sources (diagnostic, EVR_EYE_IMAGE_SWEEP=1): transfer writes into sweep depth images.
VKAPI_ATTR void VKAPI_CALL evrCmdClearDepthStencilImage(VkCommandBuffer c,VkImage image,VkImageLayout layout,const VkClearDepthStencilValue* value,uint32_t count,const VkImageSubresourceRange* ranges){
    sweepDepthTransfer(c,image,"clear_depth_stencil_image",value);deviceFn<PFN_vkCmdClearDepthStencilImage>(c,"vkCmdClearDepthStencilImage")(c,image,layout,value,count,ranges);
}
VKAPI_ATTR void VKAPI_CALL evrCmdCopyImage(VkCommandBuffer c,VkImage src,VkImageLayout srcLayout,VkImage dst,VkImageLayout dstLayout,uint32_t count,const VkImageCopy* regions){
    sweepDepthTransfer(c,dst,"copy_image",nullptr);deviceFn<PFN_vkCmdCopyImage>(c,"vkCmdCopyImage")(c,src,srcLayout,dst,dstLayout,count,regions);
}
VKAPI_ATTR void VKAPI_CALL evrCmdCopyImage2(VkCommandBuffer c,const VkCopyImageInfo2* info){
    if(info)sweepDepthTransfer(c,info->dstImage,"copy_image2",nullptr);deviceFn<PFN_vkCmdCopyImage2>(c,"vkCmdCopyImage2")(c,info);
}
VKAPI_ATTR void VKAPI_CALL evrCmdCopyBufferToImage(VkCommandBuffer c,VkBuffer src,VkImage dst,VkImageLayout layout,uint32_t count,const VkBufferImageCopy* regions){
    uiGlyph::copied(c,src,dst,layout,count,regions);
    sweepDepthTransfer(c,dst,"copy_buffer_to_image",nullptr);deviceFn<PFN_vkCmdCopyBufferToImage>(c,"vkCmdCopyBufferToImage")(c,src,dst,layout,count,regions);
}
VKAPI_ATTR void VKAPI_CALL evrCmdBlitImage(VkCommandBuffer c,VkImage src,VkImageLayout srcLayout,VkImage dst,VkImageLayout dstLayout,uint32_t count,const VkImageBlit* regions,VkFilter filter){
    sweepDepthTransfer(c,dst,"blit_image",nullptr);deviceFn<PFN_vkCmdBlitImage>(c,"vkCmdBlitImage")(c,src,srcLayout,dst,dstLayout,count,regions,filter);
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateRenderPass(VkDevice d, const VkRenderPassCreateInfo *info,
                                                   const VkAllocationCallbacks *a, VkRenderPass *p)
{
    auto r = deviceFn<PFN_vkCreateRenderPass>(d, "vkCreateRenderPass")(d, info, a, p);
    note("render_pass", "\"attachments\":" + std::to_string(info->attachmentCount) +
                            ",\"subpasses\":" + std::to_string(info->subpassCount));
    return r;
}
extern std::atomic<DWORD> dealiasThread;
std::atomic<uint32_t> sweepUsageAdded{0};
VKAPI_ATTR VkResult VKAPI_CALL evrCreateImage(VkDevice d, const VkImageCreateInfo *info,
                                              const VkAllocationCallbacks *a, VkImage *p)
{
    // Eye image sweep: images created inside the eye de-alias window also get TRANSFER_SRC so the
    // diagnostic readback can read visibility IDs and the primary depth (diagnostic launches only).
    VkImageCreateInfo readable{};
    if (info && diagnosticReadback::sweepRequested() && dealiasThread.load() && dealiasThread.load() == GetCurrentThreadId() &&
        !(info->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) && info->tiling == VK_IMAGE_TILING_OPTIMAL &&
        (info->usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT))) {
        readable = *info; readable.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; info = &readable;
        const auto n = ++sweepUsageAdded;
        if (n <= 64) try { note("sweep_usage_added", "\"format\":" + std::to_string(info->format) + ",\"width\":" + std::to_string(info->extent.width) +
            ",\"height\":" + std::to_string(info->extent.height) + ",\"usage\":" + std::to_string(info->usage)); } catch (...) {}
    }
    // The eye targets (2026-10-23, a player's log: Apple Vision Pro through SteamVR, eyes of 4294 x 4138, full screen
    // forced to 3840 x 2160, vsync on): the game made them without the transfer usages there (usage 28; 31 on this PC,
    // also with that player's resolution scale 0.67 - Mock game-20261023-004801 - so not that; which setting is not
    // known), so the copy into the headset's swapchain was refused ("stereo transfer missing image
    // usage") and the VR session stopped at its first pair. An RGBA8 colour target of an eye swapchain's size gets both
    // transfer usages - what it has here anyway. Not the runtime's own images (made inside xrCreateSwapchain, or with
    // external memory: shared with the compositor, whose import must match).
    VkImageCreateInfo eyeTarget{};
    if (info && !::evr::creatingXrSwapchain && info->imageType == VK_IMAGE_TYPE_2D && info->tiling == VK_IMAGE_TILING_OPTIMAL &&
        info->mipLevels == 1 && info->arrayLayers == 1 && info->samples == VK_SAMPLE_COUNT_1_BIT &&
        (info->format == VK_FORMAT_R8G8B8A8_UNORM || info->format == VK_FORMAT_R8G8B8A8_SRGB) && (info->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
        bool eyeSized = false, external = false;
        for (int e = 0; e < 2; ++e) { const auto w = ::evr::eyeExtents.width[e].load(), h = ::evr::eyeExtents.height[e].load();
            if (w && h && info->extent.width == w && info->extent.height == h) eyeSized = true; }
        for (auto next = reinterpret_cast<const VkBaseInStructure *>(info->pNext); next; next = next->pNext)
            if (next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO) external = true;
        if (eyeSized && !external) {
            constexpr VkImageUsageFlags both = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            const auto asked = info->usage;
            if ((asked & both) != both) { eyeTarget = *info; eyeTarget.usage |= both; info = &eyeTarget; }
            // on record: what the game asked for and what it got (here 31 both; the player's log: 28 asked)
            static std::atomic<unsigned> logged{0};
            if (logged.fetch_add(1) < 8) try { note("eye_target_image", "\"format\":" + std::to_string(info->format) + ",\"width\":" + std::to_string(info->extent.width) +
                ",\"height\":" + std::to_string(info->extent.height) + ",\"asked_usage\":" + std::to_string(asked) + ",\"usage\":" + std::to_string(info->usage)); } catch (...) {}
        }
    }
    // The eyes' sun shadow maps can be copied from one to the other (shadow_share.hpp).
    VkImageCreateInfo shareable{};
    if (info && shadowShareCreate(*info, shareable)) info = &shareable;
    VkImageCreateInfo atmosphere{};
    if (info && atmosphereShareCreate(*info, atmosphere)) info = &atmosphere;
    auto r = deviceFn<PFN_vkCreateImage>(d, "vkCreateImage")(d, info, a, p);
    if (r == VK_SUCCESS && p) uiGlyph::created(info, *p);
    if (info->usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_STORAGE_BIT))
    try {
        uint64_t generation = 0;
        if (r == VK_SUCCESS) {
            generation = ++imageGeneration;
            std::lock_guard<std::mutex> lock(mutex);
            auto &entries = trackedImages[key(d)];
            if (entries.size() < 100000) {
                entries[*p] = generation;
                auto copy=*info;copy.pNext=nullptr;copy.pQueueFamilyIndices=nullptr;
                imageCreation[key(d)][*p]=copy;
            }
        }
        note("render_image", "\"width\":" + std::to_string(info->extent.width) +
                                 ',' + handleField("image", r == VK_SUCCESS ? *p : VK_NULL_HANDLE) +
                                 ',' + handleField("device", d) + ",\"result\":" + std::to_string(r) +
                                 ",\"height\":" + std::to_string(info->extent.height) +
                                 ",\"format\":" + std::to_string(info->format) +
                                 ",\"generation\":" + std::to_string(generation) +
                                 ",\"layers\":" + std::to_string(info->arrayLayers) +
                                 ",\"usage\":" + std::to_string(info->usage));
    } catch (...) { note("image_metadata_error"); }
    return r;
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateBuffer(VkDevice d,const VkBufferCreateInfo* info,const VkAllocationCallbacks* a,VkBuffer* p){
    const auto r=deviceFn<PFN_vkCreateBuffer>(d,"vkCreateBuffer")(d,info,a,p);
    if(r==VK_SUCCESS)try{
        {std::lock_guard<std::mutex> lock(mutex);auto& entries=bufferAudits[key(d)];if(entries.size()<100000)entries[*p]={info->size,info->usage,0};}
        note("buffer_created",handleField("device",d)+','+handleField("buffer",*p)+",\"size\":"+std::to_string(info->size)+",\"usage\":"+std::to_string(info->usage));
    }catch(...){}
    return r;
}
VKAPI_ATTR void VKAPI_CALL evrDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks* a){
    deviceFn<PFN_vkDestroyBuffer>(d,"vkDestroyBuffer")(d,b,a);
    try{std::lock_guard<std::mutex> lock(mutex);const auto found=bufferAudits.find(key(d));if(found!=bufferAudits.end())found->second.erase(b);}catch(...){}
}
void recordBufferBinding(VkDevice d,VkBuffer b,VkDeviceMemory memory,VkDeviceSize offset) noexcept {
    try{
        {std::lock_guard<std::mutex> lock(mutex);auto owner=bufferAudits.find(key(d));if(owner!=bufferAudits.end()){
            auto found=owner->second.find(b);if(found!=owner->second.end()){found->second.memory=memory;found->second.memoryOffset=offset;}}}
        note("buffer_memory_bound",handleField("device",d)+','+handleField("buffer",b)+','+handleField("memory",memory)+",\"offset\":"+std::to_string(offset));
    }catch(...){}
}
// Image memory placement (2026-10-05 aliasing investigation): every bound image keeps its
// memory/offset/size so tracked eye and diagnostic images can report overlapping images.
struct ImageBinding{VkDeviceMemory memory{};VkDeviceSize offset=0,size=0;};
std::unordered_map<VkImage,ImageBinding> imageBindings;
void recordImageBinding(VkDevice d,VkImage image,VkDeviceMemory memory,VkDeviceSize offset){
    try{VkMemoryRequirements req{};deviceFn<PFN_vkGetImageMemoryRequirements>(d,"vkGetImageMemoryRequirements")(d,image,&req);
        std::lock_guard<std::mutex> lock(mutex);imageBindings[image]={memory,offset,req.size};}catch(...){}
}
void forgetImageBindingLocked(VkImage image){imageBindings.erase(image);}
std::string imageAliasReportLocked(VkImage image){
    auto found=imageBindings.find(image);if(found==imageBindings.end())return "\"binding\":null";
    const auto b=found->second;std::ostringstream o;unsigned overlaps=0;
    o<<"\"binding\":{\"memory\":"<<handleText(b.memory)<<",\"offset\":"<<b.offset<<",\"size\":"<<b.size<<"},\"overlaps\":[";
    for(const auto& [other,ob]:imageBindings){
        if(other==image||ob.memory!=b.memory||ob.offset>=b.offset+b.size||b.offset>=ob.offset+ob.size)continue;
        if(overlaps++<32)o<<(overlaps>1?",":"")<<"{\"image\":"<<handleText(other)<<",\"offset\":"<<ob.offset<<",\"size\":"<<ob.size<<'}';
    }
    o<<"],\"overlap_count\":"<<overlaps;return o.str();
}
// Eye View de-aliasing (2026-10-05): while the probe builds an eye View's feature/resolution
// resources on its render thread, images the game binds into shared (aliasable) pools are bound
// to their own dedicated device memory instead. Both eyes and the original View then never share
// transient resolution memory (DLSS inputs/outputs were found at identical pool offsets).
std::atomic<DWORD> dealiasThread{0};
struct DealiasedImage{VkDevice device{};VkDeviceMemory memory{};VkDeviceSize size=0;};
std::unordered_map<VkImage,DealiasedImage> dealiasedImages;
std::atomic<uint64_t> dealiasCount{0},dealiasBytes{0};
std::atomic<uint32_t> dealiasWindows{0};
bool dealiasImage(VkDevice d,VkImage image,VkDeviceMemory originalMemory,VkDeviceSize originalOffset,VkResult& result){
    if(!dealiasThread.load()||dealiasThread.load()!=GetCurrentThreadId())return false;
    DeviceDispatch dd{};
    {std::lock_guard<std::mutex> lock(mutex);auto found=devices.find(key(d));if(found==devices.end())return false;dd=found->second;}
    VkMemoryRequirements req{};deviceFn<PFN_vkGetImageMemoryRequirements>(d,"vkGetImageMemoryRequirements")(d,image,&req);
    VkPhysicalDeviceMemoryProperties props{};
    auto getProps=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(dd.gipa(dd.instance,"vkGetPhysicalDeviceMemoryProperties"));
    if(!getProps)return false;getProps(dd.physical,&props);
    uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<props.memoryTypeCount;i++)if((req.memoryTypeBits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}
    if(type==UINT32_MAX)return false;
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.image=image;
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.pNext=&dedicated;a.allocationSize=req.size;a.memoryTypeIndex=type;
    VkDeviceMemory memory{};
    if(deviceFn<PFN_vkAllocateMemory>(d,"vkAllocateMemory")(d,&a,nullptr,&memory)!=VK_SUCCESS)return false; // fall back to the game's binding
    result=deviceFn<PFN_vkBindImageMemory>(d,"vkBindImageMemory")(d,image,memory,0);
    if(result!=VK_SUCCESS){deviceFn<PFN_vkFreeMemory>(d,"vkFreeMemory")(d,memory,nullptr);return false;}
    {std::lock_guard<std::mutex> lock(mutex);dealiasedImages[image]={d,memory,req.size};imageBindings[image]={memory,0,req.size};
        if(diagnosticReadback::sweepRequested()){
            uint64_t generation=0;auto td=trackedImages.find(key(d));
            if(td!=trackedImages.end()){auto g=td->second.find(image);if(g!=td->second.end())generation=g->second;}
            const auto windows=dealiasWindows.load();
            if(registerNativeImageLocked(d,image,generation,4096,true))diagnosticReadback::sweep.push_back({image,d,windows?windows-1:0});
            const auto ci=imageCreation[key(d)].find(image);
            if(ci!=imageCreation[key(d)].end()&&(ci->second.usage&VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)){
                std::lock_guard<std::mutex> depthLock(diagnosticReadback::depthMutex);diagnosticReadback::sweepDepth[image]=windows?windows-1:0;}
        }}
    const auto n=++dealiasCount;dealiasBytes+=req.size;
    if(n<=256)note("image_dealiased",handleField("image",image)+",\"size\":"+std::to_string(req.size)+",\"type\":"+std::to_string(type)+
        ","+handleField("original_memory",originalMemory)+",\"original_offset\":"+std::to_string(originalOffset)+","+handleField("memory",memory));
    return true;
}
VkDeviceMemory takeDealiasedMemoryLocked(VkImage image){
    auto found=dealiasedImages.find(image);if(found==dealiasedImages.end())return VK_NULL_HANDLE;
    const auto memory=found->second.memory;dealiasedImages.erase(found);return memory;
}
extern "C" __declspec(dllexport) int evrSetImageDealiasWindow(uint32_t open) noexcept {
    try{if(open)++dealiasWindows;dealiasThread=open?GetCurrentThreadId():0;
        note("image_dealias_window","\"open\":"+std::string(open?"true":"false")+",\"thread\":"+std::to_string(GetCurrentThreadId())+
            ",\"dealiased\":"+std::to_string(dealiasCount.load())+",\"bytes\":"+std::to_string(dealiasBytes.load())+
            ",\"window\":"+std::to_string(dealiasWindows.load())+",\"sweep\":"+std::to_string(diagnosticReadback::sweep.size()));return 0;}catch(...){return 1;}
}
VKAPI_ATTR VkResult VKAPI_CALL evrBindImageMemory(VkDevice d,VkImage image,VkDeviceMemory memory,VkDeviceSize offset){
    VkResult dealiased=VK_SUCCESS;
    if(dealiasImage(d,image,memory,offset,dealiased))return dealiased;
    const auto result=deviceFn<PFN_vkBindImageMemory>(d,"vkBindImageMemory")(d,image,memory,offset);
    if(result==VK_SUCCESS)recordImageBinding(d,image,memory,offset);return result;
}
template<class Fn> VkResult bindImages2(VkDevice d,uint32_t count,const VkBindImageMemoryInfo* infos,Fn original){
    if(dealiasThread.load()==GetCurrentThreadId()&&dealiasThread.load()){
        for(uint32_t i=0;i<count;i++){
            VkResult r=VK_SUCCESS;
            if(!infos[i].pNext&&dealiasImage(d,infos[i].image,infos[i].memory,infos[i].memoryOffset,r)){if(r!=VK_SUCCESS)return r;continue;}
            r=original(d,1,&infos[i]);if(r!=VK_SUCCESS)return r;recordImageBinding(d,infos[i].image,infos[i].memory,infos[i].memoryOffset);
        }
        return VK_SUCCESS;
    }
    const auto result=original(d,count,infos);
    if(result==VK_SUCCESS)for(uint32_t i=0;i<count;i++)recordImageBinding(d,infos[i].image,infos[i].memory,infos[i].memoryOffset);return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrBindImageMemory2(VkDevice d,uint32_t count,const VkBindImageMemoryInfo* infos){
    return bindImages2(d,count,infos,deviceFn<PFN_vkBindImageMemory2>(d,"vkBindImageMemory2"));
}
VKAPI_ATTR VkResult VKAPI_CALL evrBindImageMemory2KHR(VkDevice d,uint32_t count,const VkBindImageMemoryInfo* infos){
    return bindImages2(d,count,infos,deviceFn<PFN_vkBindImageMemory2KHR>(d,"vkBindImageMemory2KHR"));
}
VKAPI_ATTR VkResult VKAPI_CALL evrMapMemory(VkDevice d,VkDeviceMemory memory,VkDeviceSize offset,VkDeviceSize size,VkMemoryMapFlags flags,void** data){
    const auto result=deviceFn<PFN_vkMapMemory>(d,"vkMapMemory")(d,memory,offset,size,flags,data);
    if(result==VK_SUCCESS&&data)uiGlyph::memoryMapped(memory,offset,*data);return result;
}
VKAPI_ATTR void VKAPI_CALL evrUnmapMemory(VkDevice d,VkDeviceMemory memory){
    uiGlyph::memoryUnmapped(memory);deviceFn<PFN_vkUnmapMemory>(d,"vkUnmapMemory")(d,memory);
}
VKAPI_ATTR VkResult VKAPI_CALL evrBindBufferMemory(VkDevice d,VkBuffer b,VkDeviceMemory memory,VkDeviceSize offset){
    const auto result=deviceFn<PFN_vkBindBufferMemory>(d,"vkBindBufferMemory")(d,b,memory,offset);
    if(result==VK_SUCCESS)recordBufferBinding(d,b,memory,offset);return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrBindBufferMemory2(VkDevice d,uint32_t count,const VkBindBufferMemoryInfo* infos){
    const auto result=deviceFn<PFN_vkBindBufferMemory2>(d,"vkBindBufferMemory2")(d,count,infos);
    if(result==VK_SUCCESS)for(uint32_t i=0;i<count;i++)recordBufferBinding(d,infos[i].buffer,infos[i].memory,infos[i].memoryOffset);return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrBindBufferMemory2KHR(VkDevice d,uint32_t count,const VkBindBufferMemoryInfo* infos){
    const auto result=deviceFn<PFN_vkBindBufferMemory2KHR>(d,"vkBindBufferMemory2KHR")(d,count,infos);
    if(result==VK_SUCCESS)for(uint32_t i=0;i<count;i++)recordBufferBinding(d,infos[i].buffer,infos[i].memory,infos[i].memoryOffset);return result;
}
VKAPI_ATTR void VKAPI_CALL evrCmdFillBuffer(VkCommandBuffer c,VkBuffer b,VkDeviceSize offset,VkDeviceSize size,uint32_t value){
    try{
        BufferAudit copy{};bool observed=false;
        {std::lock_guard<std::mutex> lock(mutex);auto owner=bufferAudits.find(key(c));if(owner!=bufferAudits.end()){
            auto entry=owner->second.find(b);if(entry!=owner->second.end()){copy=entry->second;++entry->second.fills;observed=true;}}}
        if(observed){const bool valid=(copy.usage&VK_BUFFER_USAGE_TRANSFER_DST_BIT)&&offset<copy.size&&offset%4==0&&
            (size==VK_WHOLE_SIZE||(size&&size%4==0&&size<=copy.size-offset));
            if(copy.fills<2||!valid||!copy.memory)note("buffer_fill_audit",handleField("buffer",b)+",\"offset\":"+std::to_string(offset)+",\"size\":"+std::to_string(size)+
                ",\"allocation_size\":"+std::to_string(copy.size)+",\"usage\":"+std::to_string(copy.usage)+",\"value\":"+std::to_string(value)+",\"valid_usage_and_range\":"+(valid?"true":"false")+
                ",\"memory_bound\":"+(copy.memory?"true":"false")+','+handleField("memory",copy.memory)+",\"memory_offset\":"+std::to_string(copy.memoryOffset));}
    }catch(...){}
    deviceFn<PFN_vkCmdFillBuffer>(c,"vkCmdFillBuffer")(c,b,offset,size,value);
}
static PFN_vkVoidFunction intercept(const char *name)
{
    // clang-format off
#define MAP(vkname, evrname) if (!std::strcmp(name, #vkname)) return reinterpret_cast<PFN_vkVoidFunction>(evrname);
    MAP(vkGetDeviceQueue, evrGetDeviceQueue)
    MAP(vkGetDeviceQueue2, evrGetDeviceQueue2)
    MAP(vkQueueSubmit, evrQueueSubmit)
    MAP(vkQueueSubmit2, evrQueueSubmit2)
    MAP(vkQueueSubmit2KHR, evrQueueSubmit2KHR)
    MAP(vkAllocateCommandBuffers, evrAllocateCommandBuffers)
    MAP(vkBeginCommandBuffer, evrBeginCommandBuffer)
    MAP(vkEndCommandBuffer, evrEndCommandBuffer)
    MAP(vkResetCommandBuffer, evrResetCommandBuffer)
    MAP(vkResetCommandPool, evrResetCommandPool)
    MAP(vkDestroyCommandPool, evrDestroyCommandPool)
    MAP(vkFreeCommandBuffers, evrFreeCommandBuffers)
    MAP(vkCmdPipelineBarrier, evrCmdPipelineBarrier)
    MAP(vkCmdPipelineBarrier2, evrCmdPipelineBarrier2)
    MAP(vkCmdPipelineBarrier2KHR, evrCmdPipelineBarrier2KHR)
    MAP(vkCreateSemaphore, evrCreateSemaphore)
    MAP(vkDestroySemaphore, evrDestroySemaphore)
    MAP(vkCreateImageView, evrCreateImageView)
    MAP(vkDestroyImageView, evrDestroyImageView)
    MAP(vkSetDebugUtilsObjectNameEXT, evrSetDebugUtilsObjectNameEXT)
    MAP(vkCmdBeginRendering, evrCmdBeginRendering)
    MAP(vkCmdBeginRenderingKHR, evrCmdBeginRenderingKHR)
    MAP(vkCmdEndRendering, evrCmdEndRendering)
    MAP(vkCmdEndRenderingKHR, evrCmdEndRenderingKHR)
    MAP(vkQueueWaitIdle, evrQueueWaitIdle)
    MAP(vkDeviceWaitIdle, evrDeviceWaitIdle)
    MAP(vkEnumerateInstanceExtensionProperties, evrEnumerateInstanceExtensionProperties)
    MAP(vkEnumerateDeviceExtensionProperties, evrEnumerateDeviceExtensionProperties)
    MAP(vkGetInstanceProcAddr, evrGetInstanceProcAddr)
    MAP(vkGetDeviceProcAddr, evrGetDeviceProcAddr)
    MAP(vkCreateInstance, evrCreateInstance)
    MAP(vkDestroyInstance, evrDestroyInstance)
    MAP(vkGetPhysicalDeviceProperties, evrGetPhysicalDeviceProperties)
    MAP(vkCreateDevice, evrCreateDevice)
    MAP(vkDestroyDevice, evrDestroyDevice)
    MAP(vkCreateSwapchainKHR, evrCreateSwapchainKHR)
    MAP(vkGetSwapchainImagesKHR, evrGetSwapchainImagesKHR)
    MAP(vkDestroySwapchainKHR, evrDestroySwapchainKHR)
    MAP(vkQueuePresentKHR, evrQueuePresentKHR)
    MAP(vkCreateShaderModule, evrCreateShaderModule)
    MAP(vkDestroyShaderModule, evrDestroyShaderModule)
    MAP(vkDestroyPipeline, evrDestroyPipeline)
    MAP(vkCmdBeginQuery, evrCmdBeginQuery)
    MAP(vkCmdClearAttachments, evrCmdClearAttachments)
    MAP(vkCmdClearDepthStencilImage, evrCmdClearDepthStencilImage)
    MAP(vkCmdCopyImage, evrCmdCopyImage)
    MAP(vkCmdCopyImage2, evrCmdCopyImage2)
    MAP(vkCmdCopyBufferToImage, evrCmdCopyBufferToImage)
    MAP(vkCmdBlitImage, evrCmdBlitImage)
    MAP(vkCmdDrawIndirectCount, evrCmdDrawIndirectCount)
    MAP(vkCmdDrawIndexedIndirectCount, evrCmdDrawIndexedIndirectCount)
    MAP(vkCmdDrawIndirectCountKHR, evrCmdDrawIndirectCountKHR)
    MAP(vkCmdDrawIndexedIndirectCountKHR, evrCmdDrawIndexedIndirectCountKHR)
    MAP(vkCmdDraw, evrCmdDraw)
    MAP(vkCmdDrawIndexed, evrCmdDrawIndexed)
    MAP(vkCmdDrawIndirect, evrCmdDrawIndirect)
    MAP(vkCmdDrawIndexedIndirect, evrCmdDrawIndexedIndirect)
    MAP(vkCmdDrawMeshTasksIndirectCountEXT, evrCmdDrawMeshTasksIndirectCountEXT)
    MAP(vkCmdDrawMeshTasksIndirectEXT, evrCmdDrawMeshTasksIndirectEXT)
    MAP(vkCmdDrawMeshTasksEXT, evrCmdDrawMeshTasksEXT)
    MAP(vkCmdDispatch, evrCmdDispatch)
    MAP(vkCmdDispatchIndirect, evrCmdDispatchIndirect)
    MAP(vkCreateRenderPass, evrCreateRenderPass)
    MAP(vkCreateImage, evrCreateImage)
    MAP(vkCreateBuffer, evrCreateBuffer)
    MAP(vkBindBufferMemory, evrBindBufferMemory)
    MAP(vkMapMemory, evrMapMemory)
    MAP(vkUnmapMemory, evrUnmapMemory)
    MAP(vkBindBufferMemory2, evrBindBufferMemory2)
    MAP(vkBindBufferMemory2KHR, evrBindBufferMemory2KHR)
    MAP(vkBindImageMemory, evrBindImageMemory)
    MAP(vkBindImageMemory2, evrBindImageMemory2)
    MAP(vkBindImageMemory2KHR, evrBindImageMemory2KHR)
    MAP(vkDestroyBuffer, evrDestroyBuffer)
    MAP(vkCmdFillBuffer, evrCmdFillBuffer)
    MAP(vkDestroyImage, evrDestroyImage)
    MAP(vkCreateGraphicsPipelines, evrCreateGraphicsPipelines)
    MAP(vkCreateComputePipelines, evrCreateComputePipelines)
    MAP(vkCmdBindPipeline, evrCmdBindPipeline)
    MAP(vkCmdBindDescriptorSets, evrCmdBindDescriptorSets)
    MAP(vkUpdateDescriptorSets, evrUpdateDescriptorSets)
#undef MAP
    // clang-format on
    return nullptr;
}
extern "C" __declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL evrGetInstanceProcAddr(VkInstance i,
                                                                                      const char *n)
{
    if ((!std::strcmp(n,"vkCmdDrawIndirectCount") || !std::strcmp(n,"vkCmdDrawIndexedIndirectCount") ||
         !std::strcmp(n,"vkCmdDrawIndirectCountKHR") || !std::strcmp(n,"vkCmdDrawIndexedIndirectCountKHR") || !std::strcmp(n,"vkBindBufferMemory2") || !std::strcmp(n,"vkBindBufferMemory2KHR") || !std::strcmp(n,"vkBindImageMemory2") || !std::strcmp(n,"vkBindImageMemory2KHR") || !std::strcmp(n, "vkCmdBeginRendering") || !std::strcmp(n, "vkCmdBeginRenderingKHR") ||
         !std::strcmp(n, "vkCmdEndRendering") || !std::strcmp(n, "vkCmdEndRenderingKHR") || !std::strcmp(n,"vkSetDebugUtilsObjectNameEXT") || !std::strcmp(n,"vkCmdPipelineBarrier2") || !std::strcmp(n,"vkCmdPipelineBarrier2KHR") || !std::strcmp(n,"vkQueueSubmit2") || !std::strcmp(n,"vkQueueSubmit2KHR") || !std::strcmp(n,"vkCmdDrawMeshTasksIndirectCountEXT") || !std::strcmp(n,"vkCmdDrawMeshTasksIndirectEXT") || !std::strcmp(n,"vkCmdDrawMeshTasksEXT")) &&
        (!i || !instanceFn<PFN_vkVoidFunction>(i, n))) return nullptr;
    if (auto p = intercept(n))
        return p;
    return instanceFn<PFN_vkVoidFunction>(i, n);
}
extern "C" __declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL evrGetDeviceProcAddr(VkDevice d, const char *n)
{
    if ((!std::strcmp(n,"vkCmdDrawIndirectCount") || !std::strcmp(n,"vkCmdDrawIndexedIndirectCount") ||
         !std::strcmp(n,"vkCmdDrawIndirectCountKHR") || !std::strcmp(n,"vkCmdDrawIndexedIndirectCountKHR") || !std::strcmp(n,"vkBindBufferMemory2") || !std::strcmp(n,"vkBindBufferMemory2KHR") || !std::strcmp(n,"vkBindImageMemory2") || !std::strcmp(n,"vkBindImageMemory2KHR") || !std::strcmp(n, "vkCmdBeginRendering") || !std::strcmp(n, "vkCmdBeginRenderingKHR") ||
         !std::strcmp(n, "vkCmdEndRendering") || !std::strcmp(n, "vkCmdEndRenderingKHR") || !std::strcmp(n,"vkSetDebugUtilsObjectNameEXT") || !std::strcmp(n,"vkCmdPipelineBarrier2") || !std::strcmp(n,"vkCmdPipelineBarrier2KHR") || !std::strcmp(n,"vkQueueSubmit2") || !std::strcmp(n,"vkQueueSubmit2KHR") || !std::strcmp(n,"vkCmdDrawMeshTasksIndirectCountEXT") || !std::strcmp(n,"vkCmdDrawMeshTasksIndirectEXT") || !std::strcmp(n,"vkCmdDrawMeshTasksEXT")) &&
        !deviceFn<PFN_vkVoidFunction>(d, n)) return nullptr;
    if (auto p = intercept(n))
        return p;
    return deviceFn<PFN_vkVoidFunction>(d, n);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *info)
{
    if (!info || info->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (info->loaderLayerInterfaceVersion > 2)
        info->loaderLayerInterfaceVersion = 2;
    info->pfnGetInstanceProcAddr = evrGetInstanceProcAddr;
    info->pfnGetDeviceProcAddr = evrGetDeviceProcAddr;
    info->pfnGetPhysicalDeviceProcAddr = nullptr;
    return VK_SUCCESS;
}

// Process-lifetime value authority shared by independently loaded diagnostics.
// No game hooks are installed by this ABI; the verified native publisher feeds it.
namespace { epoch::Broker<> worldEpochBroker; }
extern "C" __declspec(dllexport) uint32_t evrWorldEpochCall(const epoch::Request* request,epoch::Response* response) noexcept {
 const ObserverLastError preserve;
 if(!request||!response)return uint32_t(epoch::Status::Invalid);
 if(request->size!=sizeof(epoch::Request)||request->version!=epoch::abiVersion||response->size!=sizeof(epoch::Response)||response->version!=epoch::abiVersion)return uint32_t(epoch::Status::AbiMismatch);
 if(request->operation==uint32_t(epoch::Operation::Register)){
  HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&evrWorldEpochCall),&pinned))return uint32_t(epoch::Status::Unavailable);
 }
 const auto value=worldEpochBroker.call(*request);*response=value;return value.status;
}

extern "C" __declspec(dllexport) uint32_t evrArmExitProgress(uint32_t milliseconds) noexcept {return exitProgress::arm(milliseconds);}

extern "C" __declspec(dllexport) uint32_t evrRegisterPresentRetirement(const diagnostic::PresentRetirementRequest* request) noexcept {return presentRetirement::install(request);}
