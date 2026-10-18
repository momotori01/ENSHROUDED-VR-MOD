#pragma once
// Desktop right-eye mirror (EVR_DESKTOP_MIRROR=1, 2026-10-06; user requirement: the desktop shows the
// right eye for streaming). At vkQueuePresentKHR the latest right-eye image handed to the XR copy is
// blitted (centre crop to the window aspect, linear filter) over the game's swapchain image on the
// present queue. The blit waits on the game's present semaphores and the present waits on the mirror's
// own semaphore, so the game's rendering into the swapchain image completes first. Both images return
// to their original layouts. If no proven right-eye image exists, the game's own image is presented.
namespace desktopMirror {
inline bool requested(){static const bool on=[]{const auto f=std::getenv("EVR_DESKTOP_MIRROR");return f&&!std::strcmp(f,"1");}();return on;}
// owned (2026-10-09): a transfer-owned copy of the right eye, already in TRANSFER_SRC_OPTIMAL (stereo_transfer.hpp).
struct Source {VkDevice device{};VkImage image{};bool owned=false;uint32_t width=0,height=0;};
inline std::mutex sourceMutex;inline Source source;
struct Slot {VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};VkSemaphore done{};};
// Device calls go through the PUBLIC loader (like the capture path) so the mirror's command buffers
// get loader dispatch data; their recording passes through this layer's own hooks.
struct State {std::unique_ptr<Vulkan> v;PFN_vkCmdBlitImage blit=nullptr;PFN_vkCreateSemaphore createSemaphore=nullptr;PFN_vkDestroySemaphore destroySemaphore=nullptr;
    VkDevice device{};uint32_t family=UINT32_MAX;std::array<Slot,4> slots{};uint32_t next=0;uint64_t mirrored=0,skipped=0;bool failed=false;};
inline std::mutex stateMutex;inline State state;
// Holding stateMutex as well means no blit is being recorded with the old image once this returns.
inline void setSource(VkDevice device,VkImage right,bool owned=false,uint32_t width=0,uint32_t height=0){
    std::lock_guard<std::mutex> busy(stateMutex);std::lock_guard<std::mutex> lock(sourceMutex);source={device,right,owned,width,height};}
inline bool ensure(VkDevice d,uint32_t family){
    if(state.failed)return false;
    if(state.device){return state.device==d&&state.family==family;}
    try{
        DeviceDispatch dd{};{std::lock_guard<std::mutex> lock(mutex);auto found=devices.find(key(d));if(found==devices.end())return false;dd=found->second;}
        auto v=std::make_unique<Vulkan>();v->gipa=dd.gipa;v->bindInstance(dd.instance);
        v->vkGetDeviceProcAddr=reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(v->module,"vkGetDeviceProcAddr"));
        if(!v->vkGetDeviceProcAddr)throw std::runtime_error("public device dispatcher unavailable");
        v->bindDevice(dd.device,dd.physical,family);
        state.blit=reinterpret_cast<PFN_vkCmdBlitImage>(v->vkGetDeviceProcAddr(dd.device,"vkCmdBlitImage"));
        state.createSemaphore=reinterpret_cast<PFN_vkCreateSemaphore>(v->vkGetDeviceProcAddr(dd.device,"vkCreateSemaphore"));
        state.destroySemaphore=reinterpret_cast<PFN_vkDestroySemaphore>(v->vkGetDeviceProcAddr(dd.device,"vkDestroySemaphore"));
        if(!state.blit||!state.createSemaphore||!state.destroySemaphore)throw std::runtime_error("mirror device functions missing");
        for(auto& s:state.slots){
            VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pc.queueFamilyIndex=family;
            vkCheck(v->vkCreateCommandPool(dd.device,&pc,nullptr,&s.pool),"mirror pool");
            VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=s.pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
            vkCheck(v->vkAllocateCommandBuffers(dd.device,&ca,&s.cmd),"mirror command");
            VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fc.flags=VK_FENCE_CREATE_SIGNALED_BIT;
            vkCheck(v->vkCreateFence(dd.device,&fc,nullptr,&s.fence),"mirror fence");
            VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCheck(state.createSemaphore(dd.device,&sc,nullptr,&s.done),"mirror semaphore");
        }
        state.v=std::move(v);state.device=d;state.family=family;
        note("desktop_mirror_ready","\"family\":"+std::to_string(family)+",\"slots\":"+std::to_string(state.slots.size()));
        return true;
    }catch(const std::exception& e){state.failed=true;note("desktop_mirror_failed","\"reason\":"+jsonString(e.what()));return false;}
}
// Called from vkDestroyDevice before the device is destroyed.
inline void destroy(VkDevice d) noexcept {
    try{std::lock_guard<std::mutex> lock(stateMutex);if(!state.v||state.device!=d)return;auto& v=*state.v;
        for(auto& s:state.slots){if(s.fence){v.vkWaitForFences(v.device,1,&s.fence,VK_TRUE,2000000000ull);v.vkDestroyFence(v.device,s.fence,nullptr);}
            if(s.done)state.destroySemaphore(v.device,s.done,nullptr);if(s.pool)v.vkDestroyCommandPool(v.device,s.pool,nullptr);s={};}
        note("desktop_mirror_destroyed","\"mirrored\":"+std::to_string(state.mirrored)+",\"skipped\":"+std::to_string(state.skipped));
        state.v.reset();state.device={};
    }catch(...){}
}
// Returns true when `replacement` now waits on the mirror semaphore instead of the game's semaphores.
inline bool apply(VkQueue queue,const VkPresentInfoKHR* present,VkPresentInfoKHR& replacement,VkSemaphore& waitOut) noexcept {
    if(!requested()||!present||present->swapchainCount!=1)return false;
    try{
        VkDevice device{};uint32_t family=UINT32_MAX;SwapData swap;
        {std::lock_guard<std::mutex> lock(mutex);
            auto dd=devices.find(key(queue));if(dd==devices.end())return false;device=dd->second.device;
            auto f=queueFamilies.find(queue);if(f!=queueFamilies.end())family=f->second;
            auto s=swapData.find(present->pSwapchains[0]);if(s==swapData.end())return false;swap=s->second;}
        if(family==UINT32_MAX||!(swap.usage&VK_IMAGE_USAGE_TRANSFER_DST_BIT)||present->pImageIndices[0]>=swap.images.size())return false;
        Source src;{std::lock_guard<std::mutex> lock(sourceMutex);src=source;}
        std::lock_guard<std::mutex> lock(stateMutex);
        {std::lock_guard<std::mutex> again(sourceMutex);src=source;} // re-read under stateMutex (teardown clears it)
        diagnostic::NativeImageEvidence e{};
        if(src.owned&&src.image&&src.device==device){e.ready=true;e.width=src.width;e.height=src.height;e.layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;e.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;}
        else if(!src.image||src.device!=device||src.owned||evrGetNativeImageEvidence(device,src.image,&e)||!e.ready||!(e.usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)){
            ++state.skipped;return false;}
        if(!ensure(device,family))return false;
        auto& slot=state.slots[state.next];state.next=(state.next+1)%state.slots.size();
        auto& v=*state.v;
        if(v.vkWaitForFences(v.device,1,&slot.fence,VK_TRUE,1000000000ull)!=VK_SUCCESS){++state.skipped;return false;}
        v.vkResetFences(v.device,1,&slot.fence);
        const auto cmd=slot.cmd;const auto target=swap.images[present->pImageIndices[0]];
        v.vkResetCommandBuffer(cmd,0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(v.vkBeginCommandBuffer(cmd,&bi)!=VK_SUCCESS){state.failed=true;return false;}
        const auto barrier=v.vkCmdPipelineBarrier;
        std::array<VkImageMemoryBarrier,2> to{};
        for(auto& b:to){b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};}
        to[0].image=target;to[0].oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;to[0].newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to[0].srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT;to[0].dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        to[1].image=src.image;to[1].oldLayout=VkImageLayout(e.layout);to[1].newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to[1].srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;to[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        barrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,to.data());
        // Centre crop of the eye image to the window aspect, scaled to the whole swapchain image.
        const double W=swap.width,H=swap.height,w=e.width,h=e.height;
        int32_t x0=0,y0=0,x1=int32_t(e.width),y1=int32_t(e.height);
        if(w/h<W/H){const auto ch=int32_t(std::lround(w*H/W));y0=(int32_t(e.height)-ch)/2;y1=y0+ch;}
        else{const auto cw=int32_t(std::lround(h*W/H));x0=(int32_t(e.width)-cw)/2;x1=x0+cw;}
        VkImageBlit blit{};blit.srcSubresource=blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        blit.srcOffsets[0]={x0,y0,0};blit.srcOffsets[1]={x1,y1,1};blit.dstOffsets[1]={int32_t(swap.width),int32_t(swap.height),1};
        state.blit(cmd,src.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
        std::array<VkImageMemoryBarrier,2> back=to;
        back[0].oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;back[0].newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;back[0].srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;back[0].dstAccessMask=0;
        back[1].oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;back[1].newLayout=VkImageLayout(e.layout);back[1].srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;back[1].dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        barrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,2,back.data());
        if(v.vkEndCommandBuffer(cmd)!=VK_SUCCESS){state.failed=true;return false;}
        std::vector<VkPipelineStageFlags> stages(replacement.waitSemaphoreCount,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.waitSemaphoreCount=replacement.waitSemaphoreCount;s.pWaitSemaphores=replacement.pWaitSemaphores;s.pWaitDstStageMask=stages.data();
        s.commandBufferCount=1;s.pCommandBuffers=&cmd;s.signalSemaphoreCount=1;s.pSignalSemaphores=&slot.done;
        if(v.vkQueueSubmit(queue,1,&s,slot.fence)!=VK_SUCCESS){state.failed=true;note("desktop_mirror_failed","\"reason\":\"submit\"");return false;}
        waitOut=slot.done;replacement.waitSemaphoreCount=1;replacement.pWaitSemaphores=&waitOut;
        const auto n=++state.mirrored;
        if(n<=3||n%600==0)note("desktop_mirror","\"mirrored\":"+std::to_string(n)+",\"skipped\":"+std::to_string(state.skipped)+",\"source\":"+handleText(src.image)+
            ",\"source_size\":["+std::to_string(e.width)+','+std::to_string(e.height)+"],\"crop\":["+std::to_string(x0)+','+std::to_string(y0)+','+std::to_string(x1)+','+std::to_string(y1)+
            "],\"swapchain\":["+std::to_string(swap.width)+','+std::to_string(swap.height)+"],\"format\":"+std::to_string(swap.format));
        return true;
    }catch(...){return false;}
}
}
void desktopMirrorSource(VkDevice device,VkImage right) noexcept {try{desktopMirror::setSource(device,right);}catch(...){}}
void desktopMirrorOwnedSource(VkDevice device,VkImage image,uint32_t width,uint32_t height) noexcept {try{desktopMirror::setSource(device,image,true,width,height);}catch(...){}}
