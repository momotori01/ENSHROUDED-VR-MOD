#pragma once
#include <evr/image_submission.hpp>
#include <evr/native_image_evidence.hpp>
struct NativeImageState
{
    VkImage image{};
    uint64_t generation = 0;
    uint32_t width = 0, height = 0;
    VkFormat format{}; VkImageUsageFlags usage{};
    bool compatible = false;
    bool quiet = false; // diagnostic sweep images: tracked without per-barrier/submission notes
    diagnostic::ImageSubmission state;
};
struct CommandIdentity { VkCommandPool pool{}; VkCommandBufferLevel level{}; };
std::unordered_map<void *, std::unordered_map<VkImage, NativeImageState>> nativeImages;
std::unordered_map<VkCommandBuffer, CommandIdentity> commandIdentities;
std::unordered_map<void *, std::unordered_map<VkImage, VkImageCreateInfo>> imageCreation;
uint64_t nativeSubmissionSerial = 0;
// A metadata callback may stop part way through a list after an exception.
// In that case no previously recorded layout is safe evidence for XR copying.
// Fail closed until the diagnostic layer is recreated with the game process.
std::atomic<bool> nativeEvidenceFault{false};
void failNativeImageEvidence() noexcept {
    nativeEvidenceFault.store(true, std::memory_order_release);
    try { note("native_image_state_error", "\"fallback\":\"native_xr_copy_disabled_until_restart\""); } catch (...) {}
}
void resetNativeCommandLocked(VkCommandBuffer cmd)
{
    uiDraw::reset(cmd);
    auto device = nativeImages.find(key(cmd));
    if (device != nativeImages.end()) for (auto &entry : device->second)
        entry.second.state.reset(reinterpret_cast<uint64_t>(cmd));
}
VKAPI_ATTR VkResult VKAPI_CALL evrAllocateCommandBuffers(VkDevice device,const VkCommandBufferAllocateInfo* info,VkCommandBuffer* commands)
{
    auto result=deviceFn<PFN_vkAllocateCommandBuffers>(device,"vkAllocateCommandBuffers")(device,info,commands);
    if(result==VK_SUCCESS)try{std::lock_guard<std::mutex> lock(mutex);
        for(uint32_t n=0;n<info->commandBufferCount;++n)if(commandIdentities.size()<100000)
            commandIdentities[commands[n]]={info->commandPool,info->level};
    }catch(...){failNativeImageEvidence();}
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrBeginCommandBuffer(VkCommandBuffer cmd,const VkCommandBufferBeginInfo* info)
{
    auto result=deviceFn<PFN_vkBeginCommandBuffer>(cmd,"vkBeginCommandBuffer")(cmd,info);
    if(result==VK_SUCCESS){{std::lock_guard<std::mutex> lock(mutex);resetNativeCommandLocked(cmd);}gpuTimingBegin(cmd);giReprojectReset(cmd);}
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrEndCommandBuffer(VkCommandBuffer cmd)
{
    gpuTimingEnd(cmd);
    return deviceFn<PFN_vkEndCommandBuffer>(cmd,"vkEndCommandBuffer")(cmd);
}
VKAPI_ATTR VkResult VKAPI_CALL evrResetCommandBuffer(VkCommandBuffer cmd,VkCommandBufferResetFlags flags)
{
    auto result=deviceFn<PFN_vkResetCommandBuffer>(cmd,"vkResetCommandBuffer")(cmd,flags);
    if(result==VK_SUCCESS){{std::lock_guard<std::mutex> lock(mutex);resetNativeCommandLocked(cmd);}giReprojectReset(cmd);}
    return result;
}
void resetNativePoolLocked(VkDevice device,VkCommandPool pool,bool erase)
{
    for(auto it=commandIdentities.begin();it!=commandIdentities.end();){
        if(key(it->first)==key(device)&&it->second.pool==pool){resetNativeCommandLocked(it->first);
            if(erase){it=commandIdentities.erase(it);continue;}}
        ++it;
    }
}
VKAPI_ATTR VkResult VKAPI_CALL evrResetCommandPool(VkDevice device,VkCommandPool pool,VkCommandPoolResetFlags flags)
{
    auto result=deviceFn<PFN_vkResetCommandPool>(device,"vkResetCommandPool")(device,pool,flags);
    if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(mutex);resetNativePoolLocked(device,pool,false);}
    return result;
}
VKAPI_ATTR void VKAPI_CALL evrDestroyCommandPool(VkDevice device,VkCommandPool pool,const VkAllocationCallbacks* alloc)
{
    auto fn=deviceFn<PFN_vkDestroyCommandPool>(device,"vkDestroyCommandPool");
    {std::lock_guard<std::mutex> lock(mutex);resetNativePoolLocked(device,pool,true);}fn(device,pool,alloc);
}
VKAPI_ATTR void VKAPI_CALL evrFreeCommandBuffers(VkDevice device,VkCommandPool pool,uint32_t count,const VkCommandBuffer* commands)
{
    auto fn=deviceFn<PFN_vkFreeCommandBuffers>(device,"vkFreeCommandBuffers");
    {std::lock_guard<std::mutex> lock(mutex);for(uint32_t n=0;n<count;++n){resetNativeCommandLocked(commands[n]);commandIdentities.erase(commands[n]);}}
    fn(device,pool,count,commands);
}
template<class Barrier> void recordNativeBarrier(VkCommandBuffer cmd,const Barrier &b)
{
    std::string metadata;
    {std::lock_guard<std::mutex> lock(mutex);
        auto device=nativeImages.find(key(cmd));if(device==nativeImages.end())return;
        auto found=device->second.find(b.image);if(found==device->second.end())return;
        auto &image=found->second;auto identity=commandIdentities.find(cmd);
        const auto &r=b.subresourceRange;
        const bool supported=identity!=commandIdentities.end()&&identity->second.level==VK_COMMAND_BUFFER_LEVEL_PRIMARY&&
            b.srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED&&b.dstQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED&&
            (r.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT||r.aspectMask==VK_IMAGE_ASPECT_DEPTH_BIT||
             r.aspectMask==(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT))&&r.baseMipLevel==0&&r.baseArrayLayer==0&&
            (r.levelCount==1||r.levelCount==VK_REMAINING_MIP_LEVELS)&&(r.layerCount==1||r.layerCount==VK_REMAINING_ARRAY_LAYERS);
        image.state.record(reinterpret_cast<uint64_t>(cmd),b.newLayout,supported);
        if(image.state.revision<=64&&!image.quiet)metadata="\"image\":"+handleText(b.image)+",\"cmd\":"+handleText(cmd)+
            ",\"old_layout\":"+std::to_string(b.oldLayout)+",\"new_layout\":"+std::to_string(b.newLayout)+
            ",\"supported\":"+(supported?"true":"false")+",\"revision\":"+std::to_string(image.state.revision);
    }
    if(!metadata.empty())note("native_image_barrier",metadata);
}
VKAPI_ATTR void VKAPI_CALL evrCmdPipelineBarrier(VkCommandBuffer cmd,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags flags,
    uint32_t mc,const VkMemoryBarrier* m,uint32_t bc,const VkBufferMemoryBarrier* b,uint32_t ic,const VkImageMemoryBarrier* images)
{
    auto fn=deviceFn<PFN_vkCmdPipelineBarrier>(cmd,"vkCmdPipelineBarrier");
    try{for(uint32_t n=0;n<ic;++n){recordNativeBarrier(cmd,images[n]);uiGlyph::barrier(cmd,images[n].image,images[n].oldLayout,images[n].newLayout,images[n].srcQueueFamilyIndex,images[n].dstQueueFamilyIndex);}}catch(...){failNativeImageEvidence();}
    fn(cmd,src,dst,flags,mc,m,bc,b,ic,images);
    shadowShareBarrier(cmd,ic,images);atmosphereShareBarrier(cmd,ic,images);
}
void traceNativeBarrier2(VkCommandBuffer cmd,const VkDependencyInfo* info) noexcept
{try{for(uint32_t n=0;n<info->imageMemoryBarrierCount;++n){recordNativeBarrier(cmd,info->pImageMemoryBarriers[n]);const auto& b=info->pImageMemoryBarriers[n];uiGlyph::barrier(cmd,b.image,b.oldLayout,b.newLayout,b.srcQueueFamilyIndex,b.dstQueueFamilyIndex);}}catch(...){failNativeImageEvidence();}}
VKAPI_ATTR void VKAPI_CALL evrCmdPipelineBarrier2(VkCommandBuffer cmd,const VkDependencyInfo* info)
{auto fn=deviceFn<PFN_vkCmdPipelineBarrier2>(cmd,"vkCmdPipelineBarrier2");traceNativeBarrier2(cmd,info);fn(cmd,info);shadowShareBarrier2(cmd,info);atmosphereShareBarrier2(cmd,info);}
VKAPI_ATTR void VKAPI_CALL evrCmdPipelineBarrier2KHR(VkCommandBuffer cmd,const VkDependencyInfo* info)
{auto fn=deviceFn<PFN_vkCmdPipelineBarrier2KHR>(cmd,"vkCmdPipelineBarrier2KHR");traceNativeBarrier2(cmd,info);fn(cmd,info);shadowShareBarrier2(cmd,info);atmosphereShareBarrier2(cmd,info);}
void nativeCommandSubmitted(VkQueue queue,VkCommandBuffer cmd,VkResult result) noexcept
{
    if(result==VK_SUCCESS)uiGlyph::submitted(queue,cmd);
    uiDraw::submitted(queue,cmd,result);
    try {
        std::vector<std::string> notes;
        {std::lock_guard<std::mutex> lock(mutex);auto device=nativeImages.find(key(queue));if(device==nativeImages.end())return;
            auto family=queueFamilies.find(queue);const auto f=family==queueFamilies.end()?UINT32_MAX:family->second;
            const auto serial=++nativeSubmissionSerial;
            for(auto &entry:device->second){auto &im=entry.second;
                if(im.state.submit(reinterpret_cast<uint64_t>(cmd),f,serial,result==VK_SUCCESS)&&im.state.revision<=64&&!im.quiet)
                    notes.push_back("\"image\":"+handleText(im.image)+",\"cmd\":"+handleText(cmd)+",\"layout\":"+std::to_string(im.state.layout)+
                        ",\"family\":"+std::to_string(f)+",\"submission\":"+std::to_string(serial)+",\"ready\":"+(im.state.ready()?"true":"false"));
            }
        }for(auto &entry:notes)note("native_image_submitted",entry);
    }catch(...){failNativeImageEvidence();}
}
extern "C" __declspec(dllexport) int evrGetNativeImageEvidence(VkDevice device,VkImage image,diagnostic::NativeImageEvidence* result) noexcept
{
    if(!result||result->structSize!=sizeof(*result))return 2;
    result->ready=false;
    if(nativeEvidenceFault.load(std::memory_order_acquire))return 1;
    try{std::lock_guard<std::mutex> lock(mutex);auto d=nativeImages.find(key(device));if(d==nativeImages.end())return 1;
        auto found=d->second.find(image);if(found==d->second.end())return 1;const auto& r=found->second;
        result->width=r.width;result->height=r.height;result->format=r.format;result->usage=r.usage;
        result->layout=VkImageLayout(r.state.layout);result->family=r.state.family;
        result->generation=r.generation;result->revision=r.state.revision;result->submission=r.state.submission;
        result->ready=r.compatible&&r.state.ready()&&!nativeEvidenceFault.load(std::memory_order_acquire);return 0;
    }catch(...){return 2;}
}
void nativeSubmit2Images(VkQueue queue,uint32_t count,const VkSubmitInfo2* submits,VkResult result) noexcept
{for(uint32_t b=0;b<count;++b)for(uint32_t c=0;c<submits[b].commandBufferInfoCount;++c)
    nativeCommandSubmitted(queue,submits[b].pCommandBufferInfos[c].commandBuffer,result);}
VKAPI_ATTR VkResult VKAPI_CALL evrQueueSubmit2(VkQueue queue,uint32_t count,const VkSubmitInfo2* submits,VkFence fence)
{exitProgress::Scope exitTrace("QueueSubmit2",reinterpret_cast<uint64_t>(queue));uiGlyph::beforeSubmit(queue);auto result=deviceFn<PFN_vkQueueSubmit2>(queue,"vkQueueSubmit2")(queue,count,submits,fence);exitTrace.end(result);nativeSubmit2Images(queue,count,submits,result);return result;}
VKAPI_ATTR VkResult VKAPI_CALL evrQueueSubmit2KHR(VkQueue queue,uint32_t count,const VkSubmitInfo2* submits,VkFence fence)
{exitProgress::Scope exitTrace("QueueSubmit2KHR",reinterpret_cast<uint64_t>(queue));uiGlyph::beforeSubmit(queue);auto result=deviceFn<PFN_vkQueueSubmit2KHR>(queue,"vkQueueSubmit2KHR")(queue,count,submits,fence);exitTrace.end(result);nativeSubmit2Images(queue,count,submits,result);return result;}
