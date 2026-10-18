#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace evr::diagnostic {
// POD ABI shared by the injected native-camera probe and Vulkan layer.
struct NativeImageEvidence {
    uint32_t structSize = sizeof(NativeImageEvidence);
    uint32_t width = 0, height = 0, family = UINT32_MAX;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    uint32_t ready = 0;
    uint64_t generation = 0, revision = 0, submission = 0;
};
using GetNativeImageEvidence = int (*)(VkDevice,VkImage,NativeImageEvidence*);
}
