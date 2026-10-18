#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace evr
{
struct VulkanExtensionPlan
{
    std::vector<std::string> enabled, added, unavailable;
    bool compatible() const { return unavailable.empty(); }
};
inline VulkanExtensionPlan planVulkanExtensions(const std::vector<std::string> &game,
                                                const std::vector<std::string> &runtime,
                                                const std::vector<std::string> &supported)
{
    VulkanExtensionPlan result;
    for (const auto *group : {&game, &runtime})
        for (const auto &name : *group)
        {
            if (std::find(result.enabled.begin(), result.enabled.end(), name) != result.enabled.end())
                continue;
            result.enabled.push_back(name);
            if (std::find(supported.begin(), supported.end(), name) == supported.end())
                result.unavailable.push_back(name);
            if (group == &runtime && std::find(game.begin(), game.end(), name) == game.end())
                result.added.push_back(name);
        }
    return result; // only a candidate; caller must preserve original create info on failure
}
template <class Enumerate> std::vector<std::string> supportedVulkanExtensions(Enumerate enumerate)
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        uint32_t count = 0;
        auto result = enumerate(&count, nullptr);
        if (result != VK_SUCCESS || count > 16384)
            throw std::runtime_error("Vulkan extension enumeration count failed");
        std::vector<VkExtensionProperties> properties(count);
        result = enumerate(&count, properties.data());
        if (result == VK_INCOMPLETE) continue;
        if (result != VK_SUCCESS || count > properties.size())
            throw std::runtime_error("Vulkan extension enumeration failed");
        std::vector<std::string> names;
        for (uint32_t i = 0; i < count; ++i)
            names.emplace_back(properties[i].extensionName);
        return names;
    }
    throw std::runtime_error("Vulkan extension enumeration remained incomplete");
}
} // namespace evr
