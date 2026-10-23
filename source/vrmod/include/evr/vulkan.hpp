#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <windows.h>
namespace evr
{
// Populate from the application's actual Vulkan creation records, never from supported-extension lists.
struct BorrowedVulkan
{
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t apiVersion = 0, queueFamily = 0, queueIndex = 0, createdQueueCount = 0;
    // The queue (queueIndex) is one the layer added beside the game's (index 0) for the mod's own frames for every refresh
    // (include/evr/afw_pacing.hpp, pacing 2): what the game draws is then read on another queue than it was drawn on.
    bool ownQueue = false;
    std::vector<std::string> enabledInstanceExtensions, enabledDeviceExtensions;
};
inline void vkCheck(VkResult r, const char *operation)
{
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " VkResult=" + std::to_string(r));
}
#define EVR_VK_GLOBAL(X) X(vkCreateInstance) X(vkEnumerateInstanceExtensionProperties)
#define EVR_VK_INSTANCE(X)                                                                                             \
    X(vkDestroyInstance)                                                                                               \
    X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceMemoryProperties)              \
        X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkCreateDevice) X(vkGetDeviceProcAddr)
#define EVR_VK_DEVICE(X)                                                                                               \
    X(vkDestroyDevice)                                                                                                 \
    X(vkGetDeviceQueue) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory)         \
        X(vkFreeMemory) X(vkBindImageMemory) X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateBuffer)              \
            X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory)  \
                X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandBuffer)     \
                    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCmdPipelineBarrier) X(vkCmdCopyImageToBuffer)    \
                        X(vkCmdCopyImage) X(vkQueueSubmit) X(vkQueueWaitIdle) X(vkDeviceWaitIdle) X(vkCreateRenderPass) \
                            X(vkDestroyRenderPass) X(vkCreateFramebuffer) X(vkDestroyFramebuffer)                      \
                                X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreatePipelineLayout)             \
                                    X(vkDestroyPipelineLayout) X(vkCreateGraphicsPipelines) X(vkDestroyPipeline)       \
                                        X(vkCmdBeginRenderPass) X(vkCmdEndRenderPass) X(vkCmdBindPipeline)             \
                                            X(vkCmdBindVertexBuffers) X(vkCmdPushConstants) X(vkCmdSetViewport)        \
                                                X(vkCmdSetScissor) X(vkCmdDraw) X(vkCreateFence) X(vkDestroyFence)     \
                                                    X(vkWaitForFences) X(vkResetFences)
struct Vulkan
{
    HMODULE module = nullptr;
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    bool owned = false;
#define DECL(name) PFN_##name name = nullptr;
    EVR_VK_GLOBAL(DECL)
    EVR_VK_INSTANCE(DECL) EVR_VK_DEVICE(DECL)
#undef DECL
        Vulkan()
    {
        module = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module)
            throw std::runtime_error("Vulkan loader missing");
        gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vkGetInstanceProcAddr"));
        if (!gipa)
            throw std::runtime_error("Vulkan entry point missing");
#define LOAD(name) name = reinterpret_cast<PFN_##name>(gipa(VK_NULL_HANDLE, #name));
        EVR_VK_GLOBAL(LOAD)
#undef LOAD
    }
    Vulkan(const Vulkan &) = delete;
    Vulkan &operator=(const Vulkan &) = delete;
    ~Vulkan()
    {
        if (owned)
        {
            if (device)
            {
                vkDeviceWaitIdle(device);
                vkDestroyDevice(device, nullptr);
            }
            if (instance)
                vkDestroyInstance(instance, nullptr);
        }
        if (module)
            FreeLibrary(module);
    }
    void bindInstance(VkInstance i)
    {
        instance = i;
#define LOAD(name)                                                                                                     \
    name = reinterpret_cast<PFN_##name>(gipa(instance, #name));                                                        \
    if (!name)                                                                                                         \
        throw std::runtime_error("missing " #name);
        EVR_VK_INSTANCE(LOAD)
#undef LOAD
    }
    void bindDevice(VkDevice d, VkPhysicalDevice p, uint32_t f, uint32_t queueIndex = 0)
    {
        device = d;
        physical = p;
        family = f;
#define LOAD(name)                                                                                                     \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));                                           \
    if (!name)                                                                                                         \
        throw std::runtime_error("missing " #name);
        EVR_VK_DEVICE(LOAD)
#undef LOAD
        vkGetDeviceQueue(device, family, queueIndex, &queue);
    }
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags, VkMemoryPropertyFlags preferred = 0)
    {
        VkPhysicalDeviceMemoryProperties m{};
        vkGetPhysicalDeviceMemoryProperties(physical, &m);
        if (preferred)
            for (uint32_t i = 0; i < m.memoryTypeCount; i++)
                if ((bits & (1u << i)) && (m.memoryTypes[i].propertyFlags & (flags | preferred)) == (flags | preferred))
                    return i;
        for (uint32_t i = 0; i < m.memoryTypeCount; i++)
            if ((bits & (1u << i)) && (m.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        throw std::runtime_error("no suitable Vulkan memory type");
    }
    VkDeviceMemory allocate(VkMemoryRequirements req, VkMemoryPropertyFlags flags, VkMemoryPropertyFlags preferred = 0)
    {
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        a.allocationSize = req.size;
        a.memoryTypeIndex = memoryType(req.memoryTypeBits, flags, preferred);
        VkDeviceMemory mem{};
        vkCheck(vkAllocateMemory(device, &a, nullptr, &mem), "vkAllocateMemory");
        return mem;
    }
};
} // namespace evr
