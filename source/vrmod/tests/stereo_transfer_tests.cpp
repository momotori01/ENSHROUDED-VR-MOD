// Real GPU geometry -> separate RGBA8 images -> acquired OpenXR images.
// These fixture images are not Enshrouded renders.
#include <evr/test_renderer.hpp>
#include <evr/stereo_transfer.hpp>
#include <iostream>
using namespace evr;
namespace {
struct Sources
{
    Vulkan &v;
    std::array<EyeTarget, 2> targets;
    std::array<VkDeviceMemory, 2> memory{};
    explicit Sources(OpenXrClient &xr) : v(xr.vk)
    {
        try { for (size_t eye = 0; eye < 2; ++eye)
        {
            auto &t = targets[eye]; t.width = xr.targets[eye].width; t.height = xr.targets[eye].height;
            t.images.resize(1, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
            VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            i.imageType = VK_IMAGE_TYPE_2D; i.format = VK_FORMAT_R8G8B8A8_UNORM;
            i.extent = {t.width, t.height, 1}; i.mipLevels = i.arrayLayers = 1;
            i.samples = VK_SAMPLE_COUNT_1_BIT; i.tiling = VK_IMAGE_TILING_OPTIMAL;
            i.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            vkCheck(v.vkCreateImage(v.device, &i, nullptr, &t.images[0].image), "source image");
            VkMemoryRequirements req{}; v.vkGetImageMemoryRequirements(v.device, t.images[0].image, &req);
            memory[eye] = v.allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkCheck(v.vkBindImageMemory(v.device, t.images[0].image, memory[eye], 0), "source memory");
        } } catch (...) { destroy(); throw; }
    }
    void destroy() { for (size_t eye = 0; eye < 2; ++eye) {
        if (!targets[eye].images.empty() && targets[eye].images[0].image)
            v.vkDestroyImage(v.device, targets[eye].images[0].image, nullptr);
        if (memory[eye]) v.vkFreeMemory(v.device, memory[eye], nullptr);
    } }
    ~Sources() { destroy(); }
};
std::string bytes(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("fixture capture missing");
    return std::string(std::istreambuf_iterator<char>(in), {});
}
}
int main(int argc, char **argv)
{
    const std::string output = argc > 1 ? argv[1] : "stereo-transfer-test";
    unsigned checks = 0;
    auto require = [&](bool value, const std::string &name) {
        if (!value) throw std::runtime_error(name);
        ++checks; std::cout << "PASS " << name << '\n';
    };
    try
    {
        std::filesystem::create_directories(output);
        Log log(output + "/openxr.jsonl", true);
        for (float scale : {1.0f, 0.75f, 0.5f})
        {
            OpenXrClient xr(log); xr.init(scale, nullptr, nullptr, XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT);
            if (scale == 1.f)
            {
                VkPhysicalDeviceMemoryProperties properties{};
                xr.vk.vkGetPhysicalDeviceMemoryProperties(xr.vk.physical, &properties);
                uint32_t cached=0,uncached=0;
                constexpr auto required=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                for (uint32_t i=0;i<properties.memoryTypeCount;++i)
                    if ((properties.memoryTypes[i].propertyFlags & required)==required)
                        ((properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)?cached:uncached) |= 1u<<i;
                const auto selected=xr.vk.memoryType(cached|uncached,required,VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
                require(((cached?cached:uncached)&(1u<<selected))!=0,"prefer cached readback when supported");
                if (uncached)
                    require((uncached&(1u<<xr.vk.memoryType(uncached,required,VK_MEMORY_PROPERTY_HOST_CACHED_BIT)))!=0,
                            "uncached-only compatible types remain usable");
                bool refused=false;
                try {xr.vk.memoryType(0,required,VK_MEMORY_PROPERTY_HOST_CACHED_BIT);}
                catch (const std::runtime_error&) {refused=true;}
                require(refused,"cached preference cannot bypass compatibility bits");
                std::cout<<"Readback memory type "<<selected<<" flags "<<properties.memoryTypes[selected].propertyFlags<<'\n';
            }
            {
                const auto sourceLayout = scale == 1 ? VK_IMAGE_LAYOUT_GENERAL : scale == .75f
                    ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                Sources sources(xr); TestRenderer renderer(xr.vk, VK_FORMAT_R8G8B8A8_UNORM, sources.targets, sourceLayout);
                StereoTransfer transfer(xr.vk);
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                while (!xr.active()) {
                    if (!xr.poll() || std::chrono::steady_clock::now() > deadline) throw std::runtime_error("XR session not ready");
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                for (unsigned frame = 0; frame < 12; ++frame)
                {
                    if (!xr.beginFrame()) throw std::runtime_error("XR frame stopped");
                    const auto snapshot = xr.pendingFrame();
                    if (!snapshot.shouldRender) throw std::runtime_error("fixture tracking unexpectedly invalid");
                    std::array<TransferImage, 2> src{}, dst{};
                    std::array<std::string, 2> captured{};
                    for (uint32_t eye = 0; eye < 2; ++eye)
                    {
                        const auto &s = sources.targets[eye]; const auto &d = xr.targets[eye];
                        const std::string suffix = std::to_string(int(scale * 100)) + "-" + std::to_string(frame) + "-" + std::to_string(eye) + ".ppm";
                        renderer.draw(eye, 0, snapshot.views[eye], output + "/source-" + suffix);
                        src[eye] = {s.images[0].image, s.width, s.height, VK_FORMAT_R8G8B8A8_UNORM,
                            sourceLayout, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, xr.vk.family};
                        dst[eye] = {d.images[snapshot.imageIndices[eye]].image, d.width, d.height, xr.format,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, xr.vk.family};
                        captured[eye] = output + "/xr-" + suffix;
                    }
                    if (!frame)
                    {
                        for (int test = 0; test < 8; ++test)
                        {
                            auto invalid = src;
                            switch (test) {
                            case 0: invalid[1].image = invalid[0].image; break;
                            case 1: invalid[0].image = dst[1].image; break;
                            case 2: invalid[0].width++; break;
                            case 3: invalid[0].format = VK_FORMAT_B8G8R8A8_UNORM; break;
                            case 4: invalid[0].usage = 0; break;
                            case 5: invalid[0].queueFamily = VK_QUEUE_FAMILY_IGNORED; break;
                            case 6: invalid[0].layout = VK_IMAGE_LAYOUT_UNDEFINED; break;
                            case 7: invalid[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                invalid[0].usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT; break;
                            }
                            bool refused = false;
                            try { transfer.copy(invalid, dst); } catch (const std::invalid_argument &) { refused = true; }
                            require(refused && !transfer.gpuPending(), "unsafe transfer refused before GPU submit " + std::to_string(test));
                        }
                    }
                    transfer.copy(src, dst, captured);
                    require(!transfer.gpuPending(), "both-eye GPU completion " + std::to_string(frame));
                    for (uint32_t eye = 0; eye < 2; ++eye)
                    {
                        const auto suffix = std::to_string(int(scale * 100)) + "-" + std::to_string(frame) + "-" + std::to_string(eye) + ".ppm";
                        require(bytes(output + "/source-" + suffix) == bytes(captured[eye]), "exact RGB copy eye " + std::to_string(eye));
                        xr.markEyeRendered(eye, snapshot.frame);
                    }
                    require(bytes(captured[0]) != bytes(captured[1]), "distinct stereo geometry");
                    xr.finishFrame();
                }
                require(xr.draws[0] == 12 && xr.draws[1] == 12, "same-frame pair count and swapchain cycling");
            }
            xr.shutdown();
            require(true, "session teardown and RenderScale " + std::to_string(scale));
        }
        std::cout << "PASS total " << checks << "\n"; return 0;
    }
    catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
