#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <array>
#include <memory>
#include <vector>
#include <stdexcept>
namespace evr {
// Owned, call-scoped vkCreateDevice candidate. Never edits game/loader nodes.
// Accept the observed loader-only prefix before Features2; unfamiliar prefixes
// keep the original device request and disable this optional experiment.
class BodyClipFeaturePlan {
    VkPhysicalDeviceFeatures legacy_{};
    VkPhysicalDeviceFeatures2 features_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    std::vector<std::unique_ptr<VkLayerDeviceCreateInfo>> prefix_;
    bool changed_=false;
public:
    VkDeviceCreateInfo info{};
    const char* status="disabled";
    BodyClipFeaturePlan(const BodyClipFeaturePlan&)=delete;
    BodyClipFeaturePlan& operator=(const BodyClipFeaturePlan&)=delete;
    BodyClipFeaturePlan(const VkDeviceCreateInfo& original,bool requested,bool supported):info(original){
        if(!requested)return;
        const auto* node=reinterpret_cast<const VkBaseInStructure*>(original.pNext);
        std::array<const void*,64> seen{};size_t n=0;const VkPhysicalDeviceFeatures2* found=nullptr;
        std::vector<const VkLayerDeviceCreateInfo*> prefix;bool unknownPrefix=false;
        while(node){
            if(n==seen.size()){status="chain_limit";return;}
            for(size_t i=0;i<n;i++)if(seen[i]==node){status="chain_cycle";return;}
            seen[n++]=node;
            if(node->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2){
                if(found){status="duplicate_features2";return;}
                found=reinterpret_cast<const VkPhysicalDeviceFeatures2*>(node);
            }else if(!found){
                if(node->sType!=VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)unknownPrefix=true;
                else prefix.push_back(reinterpret_cast<const VkLayerDeviceCreateInfo*>(node));
            }
            node=node->pNext;
        }
        if(found&&original.pEnabledFeatures){status="ambiguous_core_features";return;}
        const auto* current=found?&found->features:original.pEnabledFeatures;
        if(current&&current->shaderClipDistance){status="already_enabled";return;}
        if(!supported){status="unsupported";return;}
        if(found){
            if(unknownPrefix){status="unreviewed_prefix";return;}
            features_=*found;features_.features.shaderClipDistance=VK_TRUE;
            const void* next=&features_;
            for(auto it=prefix.rbegin();it!=prefix.rend();++it){
                auto value=std::make_unique<VkLayerDeviceCreateInfo>(**it);
                value->pNext=next;next=value.get();prefix_.push_back(std::move(value));
            }
            info.pNext=next;
        }else{
            if(current)legacy_=*current;
            legacy_.shaderClipDistance=VK_TRUE;info.pEnabledFeatures=&legacy_;
        }
        changed_=true;status="enabled_in_owned_candidate";
    }
    bool changed()const{return changed_;}
    bool enabled()const{
        if(info.pEnabledFeatures)return info.pEnabledFeatures->shaderClipDistance!=0;
        const auto* p=reinterpret_cast<const VkBaseInStructure*>(info.pNext);
        for(size_t i=0;p&&i<64;i++,p=p->pNext)if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            return reinterpret_cast<const VkPhysicalDeviceFeatures2*>(p)->features.shaderClipDistance!=0;
        return false;
    }
};
}
