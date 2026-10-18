#pragma once
// Recorded host creation replay is a standalone test fixture, not a game hook.
#include "../layer/device_metadata.hpp"
#include <evr/vulkan.hpp>
#include <map>
#include <set>

namespace evr::fixture
{
struct HostDeviceConfiguration
{
    struct NamedFeature { std::string name; diagnostic::FeatureStorage storage; };
    struct Queue { uint32_t family, flags; std::vector<float> priorities; };
    std::vector<NamedFeature> features;
    std::set<std::string> assigned;
    std::vector<Queue> queues;
    uint32_t vendor = 0, device = 0;

    void feature(const std::string &assignment)
    {
        auto dot = assignment.find('.'), equal = assignment.find('=');
        if (dot == std::string::npos || equal == std::string::npos || dot >= equal ||
            (assignment.substr(equal + 1) != "0" && assignment.substr(equal + 1) != "1"))
            throw std::invalid_argument("expected feature Type.field=0 or 1");
        const auto key = assignment.substr(0, equal);
        if (!assigned.insert(key).second) throw std::invalid_argument("duplicate feature assignment");
        const auto type = assignment.substr(0, dot), field = assignment.substr(dot + 1, equal - dot - 1);
        auto it = std::find_if(features.begin(), features.end(), [&](const auto &f) { return f.name == type; });
        if (it == features.end())
        {
            auto storage = diagnostic::makeFeatureStorage(type.c_str());
            if (!storage.object) throw std::invalid_argument("unsupported recorded feature type: " + type);
            features.push_back({type, std::move(storage)});
            it = features.end() - 1;
        }
        auto &storage = it->storage;
        const auto *found = std::find_if(storage.fields.fields, storage.fields.fields + storage.fields.count,
                                        [&](const auto &f) { return field == f.name; });
        if (found == storage.fields.fields + storage.fields.count) throw std::invalid_argument("unknown recorded feature field");
        const VkBool32 value = assignment[equal + 1] == '1';
        std::memcpy(static_cast<char *>(storage.object.get()) + storage.valuesOffset + found->offset, &value, sizeof(value));
    }
    void queue(const std::string &spec)
    {
        std::istringstream in(spec);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(in, token, ':')) tokens.push_back(token);
        if (tokens.size() < 3 || tokens.size() > 66) throw std::invalid_argument("expected queue family:flags:priority[:priority]");
        auto integer = [](const std::string &s) {
            if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) throw std::invalid_argument("invalid queue integer");
            auto value = std::stoull(s);
            if (value > UINT32_MAX) throw std::invalid_argument("queue integer overflow");
            return uint32_t(value);
        };
        Queue q{integer(tokens[0]), integer(tokens[1]), {}};
        // Protected/global-priority queues need additional feature/pNext handling.
        if (q.flags) throw std::invalid_argument("fixture does not support nonzero queue create flags");
        for (const auto &existing : queues)
            if (existing.family == q.family) throw std::invalid_argument("duplicate queue family");
        for (size_t i = 2; i < tokens.size(); ++i)
        {
            size_t consumed = 0;
            float value = std::stof(tokens[i], &consumed);
            if (consumed != tokens[i].size() || !std::isfinite(value) || value < 0 || value > 1)
                throw std::invalid_argument("invalid queue priority");
            q.priorities.push_back(value);
        }
        queues.push_back(std::move(q));
    }
    void *chain()
    {
        for (size_t i = 0; i < features.size(); ++i)
        {
            auto *current = static_cast<VkBaseOutStructure *>(features[i].storage.object.get());
            current->pNext = i + 1 < features.size() ? static_cast<VkBaseOutStructure *>(features[i + 1].storage.object.get()) : nullptr;
        }
        return features.empty() ? nullptr : features[0].storage.object.get();
    }
    std::vector<VkDeviceQueueCreateInfo> queueInfos(const std::vector<VkQueueFamilyProperties> &supported, uint32_t graphicsFamily)
    {
        if (queues.empty()) queues.push_back({graphicsFamily, 0, {1.0f}});
        bool graphics = false;
        std::vector<VkDeviceQueueCreateInfo> result;
        for (const auto &q : queues)
        {
            if (q.family >= supported.size() || q.priorities.size() > supported[q.family].queueCount)
                throw std::invalid_argument("recorded queue family/count unsupported");
            VkDeviceQueueCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            info.queueFamilyIndex = q.family; info.flags = q.flags;
            info.queueCount = uint32_t(q.priorities.size()); info.pQueuePriorities = q.priorities.data();
            result.push_back(info);
            graphics |= q.family == graphicsFamily;
        }
        if (!graphics) throw std::invalid_argument("recorded queues omit fixture graphics family");
        return result;
    }
};
} // namespace evr::fixture
