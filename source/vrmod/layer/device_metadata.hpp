#pragma once
#include "feature_metadata.hpp"
#include <evr/log.hpp>
#include <array>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <vulkan/vk_layer.h>

namespace evr::diagnostic
{
inline std::string featureValues(const void *object, FeatureType type)
{
    std::ostringstream out;
    out << '{';
    for (size_t i = 0; i < type.count; ++i)
    {
        VkBool32 value;
        std::memcpy(&value, static_cast<const char *>(object) + type.fields[i].offset, sizeof(value));
        if (i) out << ',';
        out << jsonString(type.fields[i].name) << ':' << value;
    }
    out << '}';
    return out.str();
}
inline std::string deviceFeatures(const VkDeviceCreateInfo &info)
{
    std::ostringstream out;
    out << "\"core_features\":";
    if (info.pEnabledFeatures) out << featureValues(info.pEnabledFeatures, coreFeatureType());
    else out << "null";
    out << ",\"feature_chain\":[";
    std::array<const VkBaseInStructure *, 64> seen{};
    size_t count = 0;
    bool complete = true, cycle = false;
    auto node = static_cast<const VkBaseInStructure *>(info.pNext);
    while (node && count < seen.size())
    {
        if (std::find(seen.begin(), seen.begin() + count, node) != seen.begin() + count)
        { cycle = true; break; }
        seen[count++] = node;
        if (count > 1) out << ',';
        out << "{\"sType\":" << node->sType;
        auto type = featureType(node->sType);
        if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            out << ",\"name\":\"VkPhysicalDeviceFeatures2\",\"values\":"
                << featureValues(&reinterpret_cast<const VkPhysicalDeviceFeatures2 *>(node)->features, coreFeatureType());
        else if (type.name)
            out << ",\"name\":" << jsonString(type.name) << ",\"values\":" << featureValues(node, type);
        else if (node->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
            out << ",\"name\":\"loader_device_create_info\",\"scope\":\"loader_private_not_features\"";
        else
        { out << ",\"name\":null,\"values\":null"; complete = false; }
        out << '}';
        node = node->pNext;
    }
    out << "],\"feature_metadata_complete\":" << ((!node && complete) ? "true" : "false")
        << ",\"feature_chain_truncated\":" << ((node && !cycle) ? "true" : "false")
        << ",\"feature_chain_cycle\":" << (cycle ? "true" : "false");
    return out.str();
}
} // namespace evr::diagnostic
