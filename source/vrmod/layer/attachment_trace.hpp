#pragma once
// Dynamic-rendering attachment metadata; no target or command modifications.
struct ImageViewRecord
{
    VkImage image{};
    VkFormat format{};
    VkImageSubresourceRange range{};
    uint64_t generation = 0;
};
std::unordered_map<void *, std::unordered_map<VkImageView, ImageViewRecord>> imageViews;
std::atomic<uint64_t> imageViewGeneration{0};
std::unordered_map<void *, std::unordered_map<VkImage, uint64_t>> trackedImages;
std::atomic<uint64_t> imageGeneration{0};
#include "ui_draw_capture.hpp"
bool registerNativeImageLocked(VkDevice device,VkImage image,uint64_t generation,size_t cap=8,bool quiet=false) {
            const auto creationDevice=imageCreation.find(key(device));
            // The cap counts ordinary (eye/calibration/slot) images only; quiet sweep images have their own.
            size_t count=0;for(const auto& entry:nativeImages[key(device)])count+=entry.second.quiet==quiet;
            if(generation&&creationDevice!=imageCreation.end()&&count<cap&&!nativeImages[key(device)].count(image)){
                const auto creation=creationDevice->second.find(image);
                if(creation!=creationDevice->second.end()){
                    const auto& c=creation->second;NativeImageState state;state.image=image;state.generation=generation;
                    state.width=c.extent.width;state.height=c.extent.height;state.format=c.format;state.usage=c.usage;
                    state.quiet=quiet;
                    state.compatible=c.imageType==VK_IMAGE_TYPE_2D&&c.extent.depth==1&&c.mipLevels==1&&c.arrayLayers==1&&
                        c.samples==VK_SAMPLE_COUNT_1_BIT&&c.sharingMode==VK_SHARING_MODE_EXCLUSIVE&&
                        !(c.flags&(VK_IMAGE_CREATE_SPARSE_BINDING_BIT|VK_IMAGE_CREATE_ALIAS_BIT|VK_IMAGE_CREATE_DISJOINT_BIT));
                    nativeImages[key(device)].emplace(image,std::move(state));
                }
            }
    return nativeImages[key(device)].count(image)!=0;
}
extern "C" __declspec(dllexport) int evrTrackNativeEyeImage(VkDevice device,VkImage image,uint32_t eye) noexcept {
    try {
        const auto enabled=std::getenv("EVR_LIVE_NATIVE_XR");
        if(!enabled||std::strcmp(enabled,"1")||eye>1||!image)return 2;
        uint64_t generation=0;bool registered=false;std::string aliases;
        {std::lock_guard<std::mutex> lock(mutex);auto d=trackedImages.find(key(device));
            if(d!=trackedImages.end()){auto found=d->second.find(image);if(found!=d->second.end())generation=found->second;}
            registered=registerNativeImageLocked(device,image,generation);aliases=imageAliasReportLocked(image);
        }
        if(registered)note("native_eye_image_registered","\"eye\":"+std::to_string(eye)+",\"image\":"+handleText(image)+",\"generation\":"+std::to_string(generation)+","+aliases);
        return registered?0:1;
    }catch(...){return 1;}
}
VKAPI_ATTR VkResult VKAPI_CALL evrSetDebugUtilsObjectNameEXT(VkDevice device,const VkDebugUtilsObjectNameInfoEXT* info)
{
    const auto result=deviceFn<PFN_vkSetDebugUtilsObjectNameEXT>(device,"vkSetDebugUtilsObjectNameEXT")(device,info);
    if(result==VK_SUCCESS&&info&&info->objectType==VK_OBJECT_TYPE_IMAGE&&info->pObjectName&&
       std::strncmp(info->pObjectName,"EVR calibration eye ",20)==0)try{
        uint64_t generation=0;const VkImage image=(VkImage)info->objectHandle;
        {std::lock_guard<std::mutex> lock(mutex);auto d=trackedImages.find(key(device));if(d!=trackedImages.end()){auto i=d->second.find(image);if(i!=d->second.end())generation=i->second;}
            registerNativeImageLocked(device,image,generation);
        }
        note("calibration_image_named","\"name\":"+jsonString(info->pObjectName)+",\"image\":"+handleText(image)+",\"generation\":"+std::to_string(generation)+",\"device\":"+handleText(device));
    }catch(...){}
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateImageView(VkDevice device, const VkImageViewCreateInfo *info,
    const VkAllocationCallbacks *allocator, VkImageView *view)
{
    const auto result = deviceFn<PFN_vkCreateImageView>(device, "vkCreateImageView")(device, info, allocator, view);
    if (result == VK_SUCCESS) try
    {
        const auto generation = ++imageViewGeneration;
        { std::lock_guard<std::mutex> lock(mutex);
          auto &entries = imageViews[key(device)];
          if (entries.size() < 100000) entries[*view] = {info->image, info->format, info->subresourceRange, generation}; }
        if (generation <= 1024)
            note("image_view_created", "\"view\":" + handleText(*view) + ",\"image\":" + handleText(info->image) +
                 ",\"output_address\":" + handleText(view) + ",\"generation\":" + std::to_string(generation) +
                 ",\"game_rva_stack\":" + stack());
    } catch (...) { note("image_view_metadata_error"); }
    return result;
}
VKAPI_ATTR void VKAPI_CALL evrDestroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks *allocator)
{
    auto fn = deviceFn<PFN_vkDestroyImageView>(device, "vkDestroyImageView");
    uint64_t generation = 0;
    { std::lock_guard<std::mutex> lock(mutex);
      auto deviceEntry = imageViews.find(key(device));
      if (deviceEntry != imageViews.end()) {
          auto entry = deviceEntry->second.find(view);
          if (entry != deviceEntry->second.end()) { generation = entry->second.generation; deviceEntry->second.erase(entry); }
      } }
    fn(device, view, allocator);
    if (generation) try {
        note("image_view_destroyed", "\"view\":" + handleText(view) + ",\"device\":" + handleText(device) +
            ",\"generation\":" + std::to_string(generation) + ",\"game_rva_stack\":" + stack());
    } catch (...) {}
}
VKAPI_ATTR void VKAPI_CALL evrDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *allocator)
{
    auto fn = deviceFn<PFN_vkDestroyImage>(device, "vkDestroyImage");
    giReprojectForget(device, image); shadowShareForget(image); atmosphereShareForget(image);
    uint64_t generation = 0; VkDeviceMemory dealiased = VK_NULL_HANDLE;
    { std::lock_guard<std::mutex> lock(mutex);
      auto deviceEntry = trackedImages.find(key(device));
      nativeImages[key(device)].erase(image); imageCreation[key(device)].erase(image); forgetImageBindingLocked(image); forgetSweepImageLocked(image); dealiased = takeDealiasedMemoryLocked(image);
      if (deviceEntry != trackedImages.end()) {
          auto entry = deviceEntry->second.find(image);
          if (entry != deviceEntry->second.end()) { generation = entry->second; deviceEntry->second.erase(entry); }
      } }
    fn(device, image, allocator);
    if (dealiased) deviceFn<PFN_vkFreeMemory>(device, "vkFreeMemory")(device, dealiased, nullptr);
    if (generation) try {
        note("render_image_destroyed", "\"image\":" + handleText(image) + ",\"device\":" + handleText(device) +
            ",\"generation\":" + std::to_string(generation) + ",\"game_rva_stack\":" + stack());
    } catch (...) {}
}
std::string attachmentImage(VkCommandBuffer command, VkImageView view)
{
    std::ostringstream out;
    out << "{\"view\":" << handleText(view);
    std::lock_guard<std::mutex> lock(mutex);
    const auto deviceEntry = imageViews.find(key(command));
    if (deviceEntry != imageViews.end())
    {
        auto found = deviceEntry->second.find(view);
        if (found != deviceEntry->second.end())
        {
            const auto &record = found->second;
            out << ",\"image\":" << handleText(record.image) << ",\"format\":" << record.format
                << ",\"generation\":" << record.generation << ",\"aspect\":" << record.range.aspectMask
                << ",\"base_mip\":" << record.range.baseMipLevel << ",\"mip_count\":" << record.range.levelCount
                << ",\"base_layer\":" << record.range.baseArrayLayer << ",\"layer_count\":" << record.range.layerCount;
        }
    }
    out << '}';
    return out.str();
}
void traceRendering(VkCommandBuffer command, const VkRenderingInfo *info, const char *api) noexcept
{
    if (!traceEnabled()) return;
    try
    {
        std::ostringstream out;
        out << "\"cmd\":" << handleText(command) << ",\"api\":" << jsonString(api)
            << ",\"flags\":" << info->flags << ",\"width\":" << info->renderArea.extent.width
            << ",\"height\":" << info->renderArea.extent.height << ",\"offset\":[" << info->renderArea.offset.x
            << ',' << info->renderArea.offset.y << "],\"layers\":" << info->layerCount << ",\"view_mask\":" << info->viewMask;
        auto attachment = [&](const VkRenderingAttachmentInfo *a) {
            if (!a) { out << "null"; return; }
            out << "{\"target\":" << attachmentImage(command, a->imageView) << ",\"layout\":" << a->imageLayout
                << ",\"load_op\":" << a->loadOp << ",\"store_op\":" << a->storeOp
                << ",\"resolve_mode\":" << a->resolveMode << ",\"resolve\":" << attachmentImage(command, a->resolveImageView) << '}';
        };
        out << ",\"colors\":[";
        for (uint32_t i = 0; i < std::min(info->colorAttachmentCount, 32u); ++i)
        { if (i) out << ','; attachment(&info->pColorAttachments[i]); }
        out << "],\"depth\":"; attachment(info->pDepthAttachment);
        out << ",\"stencil\":"; attachment(info->pStencilAttachment);
        out << ",\"truncated\":" << (info->colorAttachmentCount > 32 ? "true" : "false")
            << ",\"game_rva_stack\":" << stack();
        note("trace_rendering_attachments", out.str());
    }
    catch (...) { note("attachment_trace_error"); }
}
VKAPI_ATTR void VKAPI_CALL evrCmdBeginRendering(VkCommandBuffer command, const VkRenderingInfo *info)
{
    auto fn = deviceFn<PFN_vkCmdBeginRendering>(command, "vkCmdBeginRendering");
    traceRendering(command, info, "vkCmdBeginRendering");sweepDepthRendering(command, info);gpuTimingRenderBegin(command, info);
    fn(command, info);
    auto end=deviceFn<PFN_vkCmdEndRendering>(command, "vkCmdEndRendering");
    uiDraw::began(command,info,fn,end);
}
VKAPI_ATTR void VKAPI_CALL evrCmdBeginRenderingKHR(VkCommandBuffer command, const VkRenderingInfo *info)
{
    auto fn = deviceFn<PFN_vkCmdBeginRenderingKHR>(command, "vkCmdBeginRenderingKHR");
    traceRendering(command, info, "vkCmdBeginRenderingKHR");sweepDepthRendering(command, info);gpuTimingRenderBegin(command, info);
    fn(command, info);
    auto end=deviceFn<PFN_vkCmdEndRenderingKHR>(command, "vkCmdEndRenderingKHR");
    uiDraw::began(command,info,fn,end);
}
VKAPI_ATTR void VKAPI_CALL evrCmdEndRendering(VkCommandBuffer command)
{
    auto fn = deviceFn<PFN_vkCmdEndRendering>(command, "vkCmdEndRendering");sweepDepthEnd(command);
    if (traceEnabled()) try { note("trace_rendering_end", "\"cmd\":" + handleText(command)); } catch (...) {}
    fn(command);
    gpuTimingRenderEnd(command);
    uiDraw::ended(command);
}
VKAPI_ATTR void VKAPI_CALL evrCmdEndRenderingKHR(VkCommandBuffer command)
{
    auto fn = deviceFn<PFN_vkCmdEndRenderingKHR>(command, "vkCmdEndRenderingKHR");sweepDepthEnd(command);
    if (traceEnabled()) try { note("trace_rendering_end", "\"cmd\":" + handleText(command)); } catch (...) {}
    fn(command);
    gpuTimingRenderEnd(command);
    uiDraw::ended(command);
}
