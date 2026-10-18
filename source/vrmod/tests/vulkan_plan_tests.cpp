#include <evr/vulkan_extensions.hpp>
#include "../layer/device_metadata.hpp"
#include <cstring>
#include <iostream>
using namespace evr;
int main()
{
    const std::vector<std::string> game{"VK_Game", "VK_Shared"}, runtime{"VK_Shared", "VK_XR", "VK_XR"};
    const auto plan = planVulkanExtensions(game, runtime, {"VK_Game", "VK_Shared", "VK_XR"});
    if (!plan.compatible() || plan.enabled != std::vector<std::string>{"VK_Game", "VK_Shared", "VK_XR"} ||
        plan.added != std::vector<std::string>{"VK_XR"}) return 1;
    std::cout << "PASS game names/order preserved, runtime addition deduplicated\n";
    const auto unsupported = planVulkanExtensions(game, runtime, {"VK_Game", "VK_Shared"});
    if (unsupported.compatible() || unsupported.unavailable != std::vector<std::string>{"VK_XR"} || game.size() != 2) return 1;
    std::cout << "PASS unsupported requirement blocks candidate; original game list unchanged\n";
    int fills = 0;
    auto names = supportedVulkanExtensions([&](uint32_t *count, VkExtensionProperties *data) {
        if (!data) { *count = 1; return VK_SUCCESS; }
        if (++fills == 1) return VK_INCOMPLETE;
        std::strcpy(data[0].extensionName, "VK_XR"); *count = 1; return VK_SUCCESS;
    });
    if (names != std::vector<std::string>{"VK_XR"} || fills != 2) return 1;
    std::cout << "PASS incomplete extension enumeration retried\n";
    try {
        supportedVulkanExtensions([](uint32_t *count, VkExtensionProperties *data) {
            *count = 1; return data ? VK_INCOMPLETE : VK_SUCCESS;
        });
        return 1;
    } catch (const std::runtime_error &) { std::cout << "PASS repeated incomplete enumeration bounded\n"; }
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.timelineSemaphore = VK_TRUE;
    VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.pNext = &features12;
    VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create.pNext = &features2;
    const auto description = diagnostic::deviceFeatures(create);
    if (description.find("\"timelineSemaphore\":1") == std::string::npos ||
        description.find("\"samplerAnisotropy\":1") == std::string::npos ||
        description.find("\"bufferDeviceAddress\":0") == std::string::npos ||
        description.find("\"feature_metadata_complete\":true") == std::string::npos) return 1;
    std::cout << "PASS nested core and Vulkan12 enabled/disabled feature values recorded\n";
    VkBaseInStructure unknown{VkStructureType(0x7ffffff0), nullptr};
    features12.pNext = &unknown;
    if (diagnostic::deviceFeatures(create).find("\"feature_metadata_complete\":false") == std::string::npos) return 1;
    unknown.pNext = reinterpret_cast<const VkBaseInStructure *>(&features2);
    if (diagnostic::deviceFeatures(create).find("\"feature_chain_cycle\":true") == std::string::npos) return 1;
    std::cout << "PASS unknown feature structure remains unknown; cyclic chain bounded\n";
    if (features12.timelineSemaphore != VK_TRUE || features2.features.samplerAnisotropy != VK_TRUE ||
        features2.pNext != &features12 || features12.pNext != &unknown) return 1;
    std::cout << "PASS feature observation preserves host inputs\n";
    return 0;
}
