#pragma once
#include <evr/body_clip_spirv_patch.hpp>
#include <evr/vulkan.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
namespace evr::body {
inline bool featuresEnabled(const VkDeviceCreateInfo& info){
    const VkPhysicalDeviceFeatures* core=info.pEnabledFeatures;bool address=false;std::array<const void*,64> seen{};size_t count=0;
    for(auto* node=static_cast<const VkBaseInStructure*>(info.pNext);node;node=node->pNext){
        if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,node)!=seen.begin()+count)return false;seen[count++]=node;
        if(node->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2){if(core)return false;core=&reinterpret_cast<const VkPhysicalDeviceFeatures2*>(node)->features;}
        if(node->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)address=reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(node)->bufferDeviceAddress!=0;
        if(node->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES)address=reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures*>(node)->bufferDeviceAddress!=0;
    }
    return core&&core->shaderClipDistance&&core->shaderInt64&&address;
}
// Immutable allocations: never rewrite a payload possibly in flight. Native vkDestroyDevice is the sole release boundary.
// At most 256 per device. When they are all taken, the one asked for least recently is rewritten - only when no frame has
// asked for it for 2 s, so that no GPU work can still read it (2026-10-25, a player's boss fight, log 20261025-135839: the
// held weapon's model came anew every 0.7 s, each a new payload, and after 256 "body immutable payload budget exhausted"
// stopped the eyes for good). None that old: the request is refused as before (the caller keeps the game's own constants).
class PayloadStore {
    struct Allocation {Payload payload;VkBuffer buffer{};VkDeviceMemory memory{};VkDeviceAddress address=0;uint64_t lastAskedMs=0;};
    static uint64_t nowMs(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    VkDevice device_;PFN_vkGetDeviceProcAddr get_;VkPhysicalDeviceMemoryProperties memory_{};
    std::mutex mutex_;std::vector<Allocation> values_;bool released_=false;
    template<class T>T fn(const char* name){auto p=reinterpret_cast<T>(get_(device_,name));if(!p)throw std::runtime_error(std::string("body payload entry absent: ")+name);return p;}
public:
    std::atomic<unsigned> shaderMask{0};std::atomic<bool> failed{false};std::atomic<unsigned> recycled{0};
    PayloadStore(VkDevice device,VkPhysicalDevice physical,VkInstance instance,PFN_vkGetDeviceProcAddr get,PFN_vkGetInstanceProcAddr gipa):device_(device),get_(get){
        auto query=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(gipa(instance,"vkGetPhysicalDeviceMemoryProperties"));if(!query)throw std::runtime_error("body memory properties absent");query(physical,&memory_);values_.reserve(256);
    }
    uint64_t get(const Payload& payload,bool& created){
        const auto ids=std::vector<uint32_t>(payload.models.begin(),payload.models.begin()+std::min(payload.count,maximumModels));
        const auto checked=appearanceTag(payload.tag)?packAppearance(ids,payload.reserved,appearanceExtra(payload),appearanceShield(payload)):pack(ids,payload.distance);
        if(std::memcmp(&checked,&payload,sizeof(payload)))throw std::runtime_error("body payload is not canonical");
        std::lock_guard<std::mutex> lock(mutex_);created=false;
        // The appearance payload (hands-only form) is read by the culling shader alone (appearance_spirv_patch.hpp); only the
        // clip payload needs both body vertex shaders. 1.0 (2026-10-16): those two are not ported yet (Mock run
        // game-20261016-064254: "body shader/device preparation incomplete", and the head stayed in the eyes).
        if(released_||failed||(!appearanceTag(payload.tag)&&shaderMask.load()!=3))throw std::runtime_error("body shader/device preparation incomplete");
        const auto now=nowMs();
        for(auto& value:values_)if(!std::memcmp(&value.payload,&payload,sizeof(payload))){value.lastAskedMs=now;return value.address;}
        if(values_.size()>=256){
            Allocation* oldest=nullptr;for(auto& value:values_)if(!oldest||value.lastAskedMs<oldest->lastAskedMs)oldest=&value;
            if(!oldest||now<oldest->lastAskedMs+2000)throw std::runtime_error("body immutable payload budget exhausted");
            void* mapped=nullptr;vkCheck(fn<PFN_vkMapMemory>("vkMapMemory")(device_,oldest->memory,0,sizeof(Payload),0,&mapped),"body map (reuse)");
            std::memcpy(mapped,&payload,sizeof(payload));fn<PFN_vkUnmapMemory>("vkUnmapMemory")(device_,oldest->memory);
            oldest->payload=payload;oldest->lastAskedMs=now;++recycled;created=true;return oldest->address;
        }
        Allocation a;a.payload=payload;a.lastAskedMs=now;void* mapped=nullptr;
        try{
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=sizeof(Payload);buffer.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vkCheck(fn<PFN_vkCreateBuffer>("vkCreateBuffer")(device_,&buffer,nullptr,&a.buffer),"body buffer");
            VkMemoryRequirements req{};fn<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(device_,a.buffer,&req);
            if(req.size>1024*1024)throw std::runtime_error("body GPU allocation exceeds per-payload budget");
            uint32_t type=0;for(;type<memory_.memoryTypeCount;type++)if((req.memoryTypeBits&(1u<<type))&&
                (memory_.memoryTypes[type].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))break;
            if(type==memory_.memoryTypeCount)throw std::runtime_error("body coherent memory unavailable");
            VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.pNext=&flags;memory.allocationSize=req.size;memory.memoryTypeIndex=type;
            vkCheck(fn<PFN_vkAllocateMemory>("vkAllocateMemory")(device_,&memory,nullptr,&a.memory),"body memory");
            vkCheck(fn<PFN_vkBindBufferMemory>("vkBindBufferMemory")(device_,a.buffer,a.memory,0),"body bind");
            vkCheck(fn<PFN_vkMapMemory>("vkMapMemory")(device_,a.memory,0,sizeof(Payload),0,&mapped),"body map");
            std::memcpy(mapped,&payload,sizeof(payload));fn<PFN_vkUnmapMemory>("vkUnmapMemory")(device_,a.memory);mapped=nullptr;
            VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};address.buffer=a.buffer;
            a.address=fn<PFN_vkGetBufferDeviceAddress>("vkGetBufferDeviceAddress")(device_,&address);
            if(!a.address||(a.address&7))throw std::runtime_error("body GPU address unavailable/alignment");
            values_.push_back(a);created=true;return a.address;
        }catch(...){if(mapped)fn<PFN_vkUnmapMemory>("vkUnmapMemory")(device_,a.memory);if(a.buffer)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device_,a.buffer,nullptr);if(a.memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device_,a.memory,nullptr);throw;}
    }
    size_t releaseAtDeviceDestroy(){
        std::lock_guard<std::mutex> lock(mutex_);const auto count=values_.size();
        for(auto& a:values_){fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device_,a.buffer,nullptr);fn<PFN_vkFreeMemory>("vkFreeMemory")(device_,a.memory,nullptr);}
        values_.clear();released_=true;return count;
    }
};
}
