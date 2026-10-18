#pragma once
#include <evr/vulkan.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <vector>

namespace evr
{
// Only one color mip/layer, single-sample, non-aliasing images are supported.
// Caller proves source work was submitted, queue-family ownership and image lifetime,
// and serializes every host/runtime use of v.queue until copy() returns.
// copy(..., wait=false) leaves the submission in flight: the barriers order it after all
// earlier work on v.queue and before all later work, and the next copy() (or destruction)
// waits for its fence before any resource is reused. Captures always wait.
struct TransferImage
{
    VkImage image{};
    uint32_t width = 0, height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    uint32_t queueFamily = VK_QUEUE_FAMILY_IGNORED;
};
// EVR_CAPTURE_HALF=1 (self-tests along a kept route, 2026-10-14): pictures of the eyes at half the width and height,
// each pixel the mean of four, and no UI pictures - 48 pictures of a self-test would be 2.3 GB at full size.
// captureHalfOverride (the VR bug report's 30 seconds, layer evrSetCaptureHalf): 1 half, 0 full, -1 the environment's.
inline std::atomic<int> captureHalfOverride{-1};
inline bool captureHalfSize()
{
    static const bool half = [] { char f[4]{}; size_t n = 0; return !getenv_s(&n, f, sizeof f, "EVR_CAPTURE_HALF") && !std::strcmp(f, "1"); }();
    const int o = captureHalfOverride.load();
    return o >= 0 ? o != 0 : half;
}

inline bool rgba8(VkFormat format)
{
    return format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
}
// eyes: the eyes that are copied (1 left, 2 right, 3 both; alternate-eye rendering copies the drawn one only).
inline void validateStereoTransfer(const std::array<TransferImage, 2> &src,
                                   const std::array<TransferImage, 2> &dst, uint32_t family, uint32_t eyes = 3)
{
    if (!(eyes & 3) || (eyes & ~3u)) throw std::invalid_argument("stereo transfer eye mask");
    std::array<VkImage, 4> images{};size_t count = 0;
    for (size_t eye = 0; eye < 2; ++eye) if (eyes >> eye & 1) { images[count++] = src[eye].image; images[count++] = dst[eye].image; }
    for (size_t i = 0; i < count; ++i)
    {
        if (!images[i]) throw std::invalid_argument("stereo transfer null image");
        for (size_t j = 0; j < i; ++j)
            if (images[i] == images[j]) throw std::invalid_argument("stereo transfer duplicate image");
    }
    for (size_t eye = 0; eye < 2; ++eye)
    {
        if (!(eyes >> eye & 1)) continue;
        const auto &s = src[eye]; const auto &d = dst[eye];
        if (!s.width || !s.height || s.width != d.width || s.height != d.height)
            throw std::invalid_argument("stereo transfer extent mismatch");
        // Copy preserves encoded bytes. BGRA requires an explicit conversion path.
        if (!rgba8(s.format) || !rgba8(d.format))
            throw std::invalid_argument("stereo transfer needs RGBA8 byte-compatible formats");
        if (!(s.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || !(d.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
            throw std::invalid_argument("stereo transfer missing image usage");
        if (s.queueFamily != family || d.queueFamily != family)
            throw std::invalid_argument("stereo transfer queue ownership unproven");
        if (s.layout != VK_IMAGE_LAYOUT_GENERAL && s.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
            s.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            throw std::invalid_argument("stereo transfer source layout unsupported");
        if (s.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
            !(s.usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)))
            throw std::invalid_argument("stereo transfer shader-read layout missing sampled/input usage");
        if (d.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            throw std::invalid_argument("stereo transfer destination not XR color layout");
    }
}

class StereoTransfer
{
    Vulkan &v;
    VkCommandPool pool{};
    VkCommandBuffer cmd{};
    VkFence fence{};
    bool pending = false;
    struct Readback { VkBuffer buffer{}; VkDeviceMemory memory{}; VkDeviceSize bytes = 0; };
    std::array<Readback, 2> readbacks{};
    // Desktop mirror copy (2026-10-09): the mirror used to blit the right eye's render target at the game's present,
    // when the next frame's passes may already be writing it (eye-private transient memory), so the desktop showed
    // half-drawn frames: blinking grass and far trees, shadows popping by blocks. The right eye is now copied here,
    // in the same submission as its XR copy, into an image owned by this transfer; the desktop blits that image.
    struct Mirror { VkImage image{}; VkDeviceMemory memory{}; uint32_t width = 0, height = 0; VkFormat format = VK_FORMAT_UNDEFINED; bool written = false; };
    Mirror mirror{};
    void destroyMirror() noexcept
    {
        if (mirror.image) v.vkDestroyImage(v.device, mirror.image, nullptr);
        if (mirror.memory) v.vkFreeMemory(v.device, mirror.memory, nullptr);
        mirror = {};
    }
    void ensureMirror(uint32_t width, uint32_t height, VkFormat format)
    {
        if (mirror.image && mirror.width == width && mirror.height == height && mirror.format == format) return;
        destroyMirror();
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        i.imageType = VK_IMAGE_TYPE_2D; i.format = format; i.extent = {width, height, 1}; i.mipLevels = 1; i.arrayLayers = 1;
        i.samples = VK_SAMPLE_COUNT_1_BIT; i.tiling = VK_IMAGE_TILING_OPTIMAL;
        i.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; i.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkCheck(v.vkCreateImage(v.device, &i, nullptr, &mirror.image), "mirror image");
        VkMemoryRequirements req{}; v.vkGetImageMemoryRequirements(v.device, mirror.image, &req);
        try { mirror.memory = v.allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); vkCheck(v.vkBindImageMemory(v.device, mirror.image, mirror.memory, 0), "mirror bind"); }
        catch (...) { destroyMirror(); throw; }
        mirror.width = width; mirror.height = height; mirror.format = format;
    }

    void barrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                 VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.srcAccessMask = srcAccess; b.dstAccessMask = dstAccess;
        b.oldLayout = oldLayout; b.newLayout = newLayout;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        v.vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }
    void freeReadbacks() noexcept
    {
        for (auto &r : readbacks)
        {
            if (r.buffer) v.vkDestroyBuffer(v.device, r.buffer, nullptr);
            if (r.memory) v.vkFreeMemory(v.device, r.memory, nullptr);
            r = {};
        }
    }
    void destroy() noexcept
    {
        // On an actual GPU timeout keep in-flight resources alive until device teardown.
        // The caller must not release XR images while gpuPending() remains true.
        if (pending)
        {
            auto result = v.vkWaitForFences(v.device, 1, &fence, VK_TRUE, 5000000000);
            if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) return;
        }
        freeReadbacks();
        destroyMirror();
        if (fence) v.vkDestroyFence(v.device, fence, nullptr);
        if (pool) v.vkDestroyCommandPool(v.device, pool, nullptr);
    }
  public:
    struct CpuTiming {
        int64_t freeUs=0,allocateUs=0,recordUs=0,submitWaitUs=0,mapUs=0,writeUs=0,totalUs=0,previousWaitUs=0;
        bool previousPending=false,waited=false;
        std::array<uint32_t,2> memoryTypes{},memoryFlags{};
        // Opt-in luma readback (diagnostic): mean of R,G,B over every 4th pixel per eye, -1 if not read.
        std::array<double,2> luma{-1,-1};
    };
  private:
    CpuTiming timing{};
  public:
    const CpuTiming& cpuTiming() const {return timing;}
    // Optional extra draw into the XR image right after its copy (VR status tint, 2026-10-06). Called with the
    // destination in TRANSFER_DST_OPTIMAL; returns true when it left the image in COLOR_ATTACHMENT_OPTIMAL.
    std::function<bool(VkCommandBuffer, size_t, const TransferImage &)> afterCopy;
    // Alternate-eye warping (eye_warp.hpp, 2026-10-15): called for every eye copied, right after its copy, with the
    // source still in TRANSFER_SRC_OPTIMAL, to keep the eye's colour and depth for the frames that leave it out.
    std::function<void(VkCommandBuffer, size_t, const TransferImage &)> keepEye;
    // The bug report's last seconds (eye_history.hpp, 2026-10-18): the same moment as keepEye.
    std::function<void(VkCommandBuffer, size_t, const TransferImage &)> history;
    // The last copy's GPU work done (before something else reads what it wrote).
    void finish()
    {
        if (!pending) return;
        vkCheck(v.vkWaitForFences(v.device, 1, &fence, VK_TRUE, 5000000000), "transfer finish fence");
        pending = false;
    }
    bool mirrorRight = false; // keep a copy of the right eye for the desktop mirror
    struct MirrorView { VkImage image{}; uint32_t width = 0, height = 0; VkFormat format = VK_FORMAT_UNDEFINED; };
    MirrorView mirrorView() const { return mirror.written ? MirrorView{mirror.image, mirror.width, mirror.height, mirror.format} : MirrorView{}; }
    explicit StereoTransfer(Vulkan &vk) : v(vk)
    {
        try
        {
            VkCommandPoolCreateInfo p{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            p.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; p.queueFamilyIndex = v.family;
            vkCheck(v.vkCreateCommandPool(v.device, &p, nullptr, &pool), "transfer command pool");
            VkCommandBufferAllocateInfo c{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            c.commandPool = pool; c.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; c.commandBufferCount = 1;
            vkCheck(v.vkAllocateCommandBuffers(v.device, &c, &cmd), "transfer command buffer");
            VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            vkCheck(v.vkCreateFence(v.device, &f, nullptr, &fence), "transfer fence");
        }
        catch (...) { destroy(); throw; }
    }
    StereoTransfer(const StereoTransfer &) = delete;
    StereoTransfer &operator=(const StereoTransfer &) = delete;
    ~StereoTransfer() { destroy(); }
    bool gpuPending() const { return pending; }
    void copy(const std::array<TransferImage, 2> &src, const std::array<TransferImage, 2> &dst,
              const std::array<std::string, 2> &captures = {}, bool wait = true, bool luma = false, uint32_t eyes = 3)
    {
        validateStereoTransfer(src, dst, v.family, eyes);
        for (size_t eye = 0; eye < 2; ++eye)
            if (!(eyes >> eye & 1) && !captures[eye].empty()) throw std::invalid_argument("stereo transfer capture of an eye not copied");
        if (luma && eyes != 3) throw std::invalid_argument("stereo transfer luma needs both eyes");
        timing={};
        using Clock=std::chrono::steady_clock;
        const auto micros=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::microseconds>(b-a).count();};
        const auto start=Clock::now();
        if (pending)
        {
            // Previous in-flight copy: its command buffer and readbacks are reused below.
            timing.previousPending = true;
            vkCheck(v.vkWaitForFences(v.device, 1, &fence, VK_TRUE, 5000000000), "previous transfer completion fence");
            pending = false;
        }
        timing.previousWaitUs=micros(start,Clock::now());
        if (!captures[0].empty() || !captures[1].empty() || luma) wait = true;
        freeReadbacks();
        const auto freed=Clock::now();timing.freeUs=micros(start,freed);
        for (size_t eye = 0; eye < 2; ++eye)
            if (!captures[eye].empty() || luma)
            {
                if (!(dst[eye].usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
                    throw std::invalid_argument("capture needs destination transfer source usage");
                auto &r = readbacks[eye]; r.bytes = VkDeviceSize(dst[eye].width) * dst[eye].height * 4;
                VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                b.size = r.bytes; b.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                vkCheck(v.vkCreateBuffer(v.device, &b, nullptr, &r.buffer), "transfer readback buffer");
                VkMemoryRequirements req{}; v.vkGetBufferMemoryRequirements(v.device, r.buffer, &req);
                // Readback favors CPU-cached memory. Keep coherent as a hard
                // requirement so the existing fence/host barrier remains valid
                // without a noncoherent invalidate path. Uncached is a fallback.
                constexpr auto required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                allocation.allocationSize = req.size;
                allocation.memoryTypeIndex = v.memoryType(req.memoryTypeBits, required, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
                VkPhysicalDeviceMemoryProperties properties{};
                v.vkGetPhysicalDeviceMemoryProperties(v.physical, &properties);
                timing.memoryTypes[eye] = allocation.memoryTypeIndex;
                timing.memoryFlags[eye] = properties.memoryTypes[allocation.memoryTypeIndex].propertyFlags;
                vkCheck(v.vkAllocateMemory(v.device, &allocation, nullptr, &r.memory), "transfer cached readback memory");
                vkCheck(v.vkBindBufferMemory(v.device, r.buffer, r.memory, 0), "transfer readback bind");
            }
        const auto allocated=Clock::now();timing.allocateUs=micros(freed,allocated);
        vkCheck(v.vkResetCommandBuffer(cmd, 0), "transfer reset command");
        VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(v.vkBeginCommandBuffer(cmd, &b), "transfer begin command");
        for (size_t eye = 0; eye < 2; ++eye)
        {
            if (!(eyes >> eye & 1)) continue;   // that eye keeps the picture it was last drawn with
            const auto &s = src[eye]; const auto &d = dst[eye];
            barrier(s.image, s.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            // XR wait succeeded; discard prior contents but retain binding-queue ownership.
            barrier(d.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkImageCopy region{};
            region.srcSubresource = region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.extent = {s.width, s.height, 1};
            v.vkCmdCopyImage(cmd, s.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            d.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            if (eye == 1 && mirrorRight)
            {
                ensureMirror(s.width, s.height, s.format);
                // The previous desktop blit read this image (TRANSFER_SRC) on the same queue.
                barrier(mirror.image, mirror.written ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                v.vkCmdCopyImage(cmd, s.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                mirror.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                barrier(mirror.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                mirror.written = true;
            }
            if (keepEye) keepEye(cmd, eye, s);
            if (history) history(cmd, eye, s);
            const bool drawn = afterCopy && afterCopy(cmd, eye, d);
            const VkImageLayout current = drawn ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            const VkAccessFlags currentAccess = drawn ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
            const VkPipelineStageFlags currentStage = drawn ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
            if (readbacks[eye].buffer)
            {
                barrier(d.image, current, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        currentAccess, VK_ACCESS_TRANSFER_READ_BIT,
                        currentStage, VK_PIPELINE_STAGE_TRANSFER_BIT);
                VkBufferImageCopy capture{};
                capture.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; capture.imageExtent = region.extent;
                v.vkCmdCopyImageToBuffer(cmd, d.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        readbacks[eye].buffer, 1, &capture);
            }
            barrier(d.image, readbacks[eye].buffer ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : current,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | currentAccess,
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT | currentStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            // Preserve the native graph's layout, including the world's postprocess sampled image.
            // ALL_COMMANDS covers graphics and compute consumers without guessing the next shader stage.
            const VkAccessFlags restoredAccess = s.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
                : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            barrier(s.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.layout,
                    VK_ACCESS_TRANSFER_READ_BIT, restoredAccess,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        if (readbacks[0].buffer || readbacks[1].buffer)
        {
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            v.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  0, 1, &host, 0, nullptr, 0, nullptr);
        }
        vkCheck(v.vkEndCommandBuffer(cmd), "transfer end command");
        vkCheck(v.vkResetFences(v.device, 1, &fence), "transfer reset fence");
        const auto recorded=Clock::now();timing.recordUs=micros(allocated,recorded);
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
        vkCheck(v.vkQueueSubmit(v.queue, 1, &submit, fence), "transfer submit both eyes");
        pending = true;timing.waited = wait;
        if (wait)
        {
            vkCheck(v.vkWaitForFences(v.device, 1, &fence, VK_TRUE, 5000000000), "transfer completion fence");
            pending = false;
        }
        const auto completed=Clock::now();timing.submitWaitUs=micros(recorded,completed);
        if (luma) for (size_t eye = 0; eye < 2; ++eye) if (readbacks[eye].buffer)
        {
            void *mapped{}; auto &r = readbacks[eye];
            vkCheck(v.vkMapMemory(v.device, r.memory, 0, r.bytes, 0, &mapped), "transfer luma map");
            const auto *p = static_cast<const unsigned char *>(mapped);
            const uint64_t pixels = uint64_t(dst[eye].width) * dst[eye].height;
            uint64_t sum = 0, count = 0;
            for (uint64_t i = 0; i < pixels; i += 4) { sum += uint64_t(p[i * 4]) + p[i * 4 + 1] + p[i * 4 + 2]; ++count; }
            v.vkUnmapMemory(v.device, r.memory);
            timing.luma[eye] = count ? double(sum) / (3.0 * double(count)) : -1;
        }
        for (size_t eye = 0; eye < 2; ++eye) if (readbacks[eye].buffer && !captures[eye].empty())
        {
            // Pack one RGB row before writing to avoid per-pixel stream calls.
            const bool half = captureHalfSize();
            const uint32_t width = half ? dst[eye].width / 2 : dst[eye].width, height = half ? dst[eye].height / 2 : dst[eye].height;
            std::vector<char> rgbRow(size_t(width) * 3);
            std::ofstream out(captures[eye], std::ios::binary);
            if (!out) throw std::runtime_error("transfer capture path unavailable");
            out << "P6\n" << width << ' ' << height << "\n255\n";
            void *mapped{}; auto &r = readbacks[eye];
            const auto mapStart=Clock::now();
            vkCheck(v.vkMapMemory(v.device, r.memory, 0, r.bytes, 0, &mapped), "transfer capture map");
            const auto mappedAt=Clock::now();timing.mapUs+=micros(mapStart,mappedAt);
            const auto *p = static_cast<const char *>(mapped);
            for (uint32_t y = 0; y < height; ++y)
            {
                if (half)
                {
                    const auto *upper = reinterpret_cast<const unsigned char *>(p + VkDeviceSize(2 * y) * dst[eye].width * 4), *lower = upper + size_t(dst[eye].width) * 4;
                    for (uint32_t x = 0; x < width; ++x)
                        for (int c = 0; c < 3; ++c)
                            rgbRow[size_t(x) * 3 + c] = char((unsigned(upper[8 * x + c]) + upper[8 * x + 4 + c] + lower[8 * x + c] + lower[8 * x + 4 + c] + 2) / 4);
                }
                else
                {
                    const auto *row = p + VkDeviceSize(y) * dst[eye].width * 4;
                    for (uint32_t x = 0; x < width; ++x)
                        std::memcpy(rgbRow.data() + size_t(x) * 3, row + size_t(x) * 4, 3);
                }
                out.write(rgbRow.data(), std::streamsize(rgbRow.size()));
            }
            v.vkUnmapMemory(v.device, r.memory);
            timing.writeUs+=micros(mappedAt,Clock::now());
            if (!out) throw std::runtime_error("transfer capture write failed");
        }
        timing.totalUs=micros(start,Clock::now());
    }
};
} // namespace evr
