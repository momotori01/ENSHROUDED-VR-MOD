#pragma once
#include <unordered_set>
#include <smmintrin.h>
// Native UI extraction; optional XR consumer must prove same-frame ownership below.
// All native draw calls execute once. Only reviewed UI draws add an attachment.
namespace uiDraw {
#define UI_DRAW_API(X) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory) \
 X(vkFreeMemory) X(vkBindImageMemory) X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateBuffer) \
 X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory) \
 X(vkCmdPipelineBarrier) X(vkCmdBindPipeline) X(vkCmdCopyImageToBuffer) X(vkQueueWaitIdle) X(vkDeviceWaitIdle)
struct Target {
    VkImage wsi{},image{};VkImageView view{};VkDeviceMemory imageMemory{},bufferMemory{};VkBuffer buffer{};
    void* mapped=nullptr;uint32_t width=0,height=0;
    uint64_t version=0,submitted=0,present=UINT64_MAX,presentedVersion=0,presentedFrame=UINT64_MAX;VkQueue queue{};uint32_t passes=0,draws=0;bool valid=false;
};
struct Device {
    VkDevice handle{};VkPhysicalDeviceMemoryProperties memory{};DWORD owner=0;bool failed=false;
    std::unordered_map<VkImage,std::unique_ptr<Target>> targets;
#define DECL(n) PFN_##n n=nullptr;
    UI_DRAW_API(DECL)
#undef DECL
    uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags){
        for(uint32_t i=0;i<memory.memoryTypeCount;i++)if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("UI memory type");
    }
    VkDeviceMemory allocate(VkMemoryRequirements req,VkMemoryPropertyFlags flags){
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=memoryType(req.memoryTypeBits,flags);
        VkDeviceMemory m{};evr::vkCheck(vkAllocateMemory(handle,&a,nullptr,&m),"UI memory");return m;
    }
    void dispose(Target& t){
        if(t.mapped)vkUnmapMemory(handle,t.bufferMemory);
        if(t.buffer)vkDestroyBuffer(handle,t.buffer,nullptr);
        if(t.bufferMemory)vkFreeMemory(handle,t.bufferMemory,nullptr);
        if(t.view)vkDestroyImageView(handle,t.view,nullptr);
        if(t.image)vkDestroyImage(handle,t.image,nullptr);
        if(t.imageMemory)vkFreeMemory(handle,t.imageMemory,nullptr);
    }
    Target* target(VkImage wsi,uint32_t width,uint32_t height){
        auto found=targets.find(wsi);if(found!=targets.end()){if(found->second->width!=width||found->second->height!=height)throw std::runtime_error("UI WSI identity/extent changed");return found->second.get();}
        if(targets.size()>=8||!width||!height||width>8192||height>8192)throw std::runtime_error("UI target budget");
        auto t=std::make_unique<Target>();t->wsi=wsi;t->width=width;t->height=height;
        try{
            VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_R8G8B8A8_UNORM;
            ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;
            ci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
            evr::vkCheck(vkCreateImage(handle,&ci,nullptr,&t->image),"UI image");VkMemoryRequirements req{};vkGetImageMemoryRequirements(handle,t->image,&req);
            t->imageMemory=allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);evr::vkCheck(vkBindImageMemory(handle,t->image,t->imageMemory,0),"UI image bind");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=t->image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;
            vi.format=ci.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            evr::vkCheck(vkCreateImageView(handle,&vi,nullptr,&t->view),"UI image view");
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=uint64_t(width)*height*4;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            evr::vkCheck(vkCreateBuffer(handle,&bi,nullptr,&t->buffer),"UI readback");vkGetBufferMemoryRequirements(handle,t->buffer,&req);
            t->bufferMemory=allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            evr::vkCheck(vkBindBufferMemory(handle,t->buffer,t->bufferMemory,0),"UI buffer bind");
            evr::vkCheck(vkMapMemory(handle,t->bufferMemory,0,bi.size,0,&t->mapped),"UI map");
        }catch(...){dispose(*t);throw;}
        auto* pointer=t.get();targets.emplace(wsi,std::move(t));return pointer;
    }
};
struct Pass {
    std::shared_ptr<Device> device;Target* target=nullptr;
    VkRenderingInfo info{};std::array<VkRenderingAttachmentInfo,2> colors{};
    PFN_vkCmdBeginRendering begin=nullptr;PFN_vkCmdEndRendering end=nullptr;
    uint64_t version=0;unsigned known=0,unknown=0;bool mrt=false,used=false,valid=true;
};
struct Recorded {Target* target;uint64_t version;bool valid;unsigned draws;};
struct Command {VkPipeline native{};bool blocked=false;std::unique_ptr<Pass> pass;std::vector<Recorded> recorded;};
std::mutex drawMutex;
std::unordered_map<VkDevice,std::shared_ptr<Device>> drawDevices;
std::unordered_map<VkCommandBuffer,Command> commands;
bool enabled(){static const bool yes=[](){const char* p=std::getenv("EVR_UI_CAPTURE");return p&&std::strcmp(p,"1")==0;}();return yes;}
// A UI frame with unreviewed draws is kept (2026-10-06 a diagnostic, EVR_UI_PARTIAL=1; on by default since 2026-10-17):
// those draws reach only the native target (missing from the XR layer) and are still counted as unknown_draws.
// Headset run 2026-10-17 02:22 (report 21, "the menu cannot be seen nor used"): menus of 1.0 draw four pipelines outside
// the reviewed ones - two multiply (DST_COLOR) blends of the reviewed UI shaders, which add nothing over the layer's
// transparent black anyway, and two new shaders - and every such frame left the whole UI out of the headset
// (native_ui_omitted "unproven_frame_or_disabled" from frame 17389 on). EVR_UI_PARTIAL=0: such frames are left out as before.
bool partial(){static const bool yes=[](){const char* p=std::getenv("EVR_UI_PARTIAL");return !(p&&std::strcmp(p,"0")==0);}();return yes;}
std::shared_ptr<Device> deviceFor(VkCommandBuffer command){
    DeviceDispatch dispatch;
    {std::lock_guard<std::mutex> lock(mutex);auto d=devices.find(key(command));if(d==devices.end())throw std::runtime_error("UI command device");dispatch=d->second;}
    {std::lock_guard<std::mutex> lock(drawMutex);auto d=drawDevices.find(dispatch.device);if(d!=drawDevices.end())return d->second;}
    auto result=std::make_shared<Device>();result->handle=dispatch.device;
    auto getMemory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(dispatch.gipa(dispatch.instance,"vkGetPhysicalDeviceMemoryProperties"));
    if(!getMemory)throw std::runtime_error("UI memory properties");getMemory(dispatch.physical,&result->memory);
#define LOAD(n) result->n=reinterpret_cast<PFN_##n>(dispatch.gdpa(dispatch.device,#n));if(!result->n)throw std::runtime_error(#n);
    UI_DRAW_API(LOAD)
#undef LOAD
    std::lock_guard<std::mutex> lock(drawMutex);auto inserted=drawDevices.emplace(dispatch.device,result);return inserted.first->second;
}
void reset(VkCommandBuffer c) noexcept {if(enabled()){std::lock_guard<std::mutex> lock(drawMutex);commands.erase(c);}}
void bind(VkCommandBuffer c,VkPipelineBindPoint point,VkPipeline pipe) noexcept {
    if(!enabled()||point!=VK_PIPELINE_BIND_POINT_GRAPHICS)return;
    try{std::lock_guard<std::mutex> lock(drawMutex);if(commands.size()<8192||commands.count(c))commands[c].native=pipe;}catch(...){}
}
void barrier(Pass& p,VkCommandBuffer c){
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    p.device->vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&b,0,nullptr,0,nullptr);
}
void switchMode(Pass& p,VkCommandBuffer c,bool mrt){
    if(p.mrt==mrt)return;
    p.end(c);barrier(p,c);
    if(mrt&&!p.used){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=p.target->image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        p.device->vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
    }
    p.colors[0].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;p.colors[1].loadOp=p.used?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR;
    auto ri=p.info;ri.pColorAttachments=p.colors.data();ri.colorAttachmentCount=mrt?2:1;p.begin(c,&ri);
    p.mrt=mrt;p.used|=mrt;
}
void began(VkCommandBuffer c,const VkRenderingInfo* info,PFN_vkCmdBeginRendering begin,PFN_vkCmdEndRendering end) noexcept {
    if(!enabled())return;
    try{
        if(!info||info->pNext||info->flags||info->viewMask||info->layerCount!=1||info->colorAttachmentCount!=1||!info->pColorAttachments||
           info->pDepthAttachment||info->pStencilAttachment||info->renderArea.offset.x||info->renderArea.offset.y||!begin||!end)return;
        const auto& a=info->pColorAttachments[0];
        if(a.pNext||a.loadOp!=VK_ATTACHMENT_LOAD_OP_LOAD||a.storeOp!=VK_ATTACHMENT_STORE_OP_STORE||a.resolveMode!=VK_RESOLVE_MODE_NONE||
           a.imageLayout!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)return;
        VkImage wsi=VK_NULL_HANDLE;
        {std::lock_guard<std::mutex> lock(mutex);
            auto identity=commandIdentities.find(c);if(identity==commandIdentities.end()||identity->second.level!=VK_COMMAND_BUFFER_LEVEL_PRIMARY)return;
            auto views=imageViews.find(key(c));if(views==imageViews.end())return;auto v=views->second.find(a.imageView);if(v==views->second.end())return;
            const auto& r=v->second;
            if(r.format!=VK_FORMAT_R8G8B8A8_UNORM||r.range.aspectMask!=VK_IMAGE_ASPECT_COLOR_BIT||r.range.baseMipLevel||
               r.range.baseArrayLayer||r.range.levelCount!=1||r.range.layerCount!=1)return;
            for(const auto& s:swapData)if(s.second.width==info->renderArea.extent.width&&s.second.height==info->renderArea.extent.height&&
                std::find(s.second.images.begin(),s.second.images.end(),r.image)!=s.second.images.end()){wsi=r.image;break;}
        }
        if(!wsi)return;auto d=deviceFor(c);
        std::lock_guard<std::mutex> lock(drawMutex);auto& command=commands[c];
        if(d->failed||command.blocked||command.pass)return;
        if(d->owner&&d->owner!=GetCurrentThreadId()){d->failed=true;throw std::runtime_error("UI command owner thread changed; capture disabled");}d->owner=GetCurrentThreadId();
        Target* t=nullptr;
        try{t=d->target(wsi,info->renderArea.extent.width,info->renderArea.extent.height);}
        catch(...){d->failed=true;throw;}
        const auto frame=presents.load();if(t->present!=frame){t->present=frame;t->passes=0;}++t->passes;t->valid=false;
        auto p=std::make_unique<Pass>();p->device=d;p->target=t;p->version=++t->version;
        p->info=*info;p->info.pColorAttachments=nullptr;p->colors[0]=a;
        auto& ui=p->colors[1];ui.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;ui.imageView=t->view;ui.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ui.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ui.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        p->begin=begin;p->end=end;p->valid=t->passes==1;command.pass=std::move(p);
    }catch(const std::exception& e){note("ui_capture_begin_failed","\"reason\":"+jsonString(e.what()));}
}
void draw(VkCommandBuffer c,bool direct) noexcept {
    if(!enabled())return;
    std::lock_guard<std::mutex> lock(drawMutex);auto found=commands.find(c);if(found==commands.end()||!found->second.pass)return;
    auto& command=found->second;auto& p=*command.pass;VkPipeline clone=VK_NULL_HANDLE;
    if(direct&&!command.blocked&&!p.device->failed){
        std::lock_guard<std::mutex> pipelineLock(uiCapture::stateMutex);auto d=uiCapture::state.find(p.device->handle);
        if(d!=uiCapture::state.end()){auto pipeline=d->second.pipelines.find(command.native);if(pipeline!=d->second.pipelines.end())clone=pipeline->second;}
    }
    if(clone){switchMode(p,c,true);p.device->vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,clone);++p.known;}
    else {switchMode(p,c,false);if(command.native)p.device->vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,command.native);++p.unknown;if(!partial())p.valid=false;
        // 2026-10-06: name each unreviewed pipeline once (the map tab drops the whole UI frame); drawMutex is held.
        static std::unordered_set<uint64_t> reported;
        if(reported.size()<64&&reported.insert(uint64_t(command.native)).second)
            note("ui_unknown_pipeline_drawn","\"pipeline\":"+uiCapture::h(uint64_t(command.native))+",\"direct\":"+std::string(direct?"true":"false")+",\"blocked\":"+std::string(command.blocked?"true":"false")+
                ",\"pass_colors\":"+std::to_string(p.info.colorAttachmentCount)+",\"area\":["+std::to_string(p.info.renderArea.extent.width)+","+std::to_string(p.info.renderArea.extent.height)+"],\"mrt\":"+std::string(p.mrt?"true":"false"));}
}
void block(VkCommandBuffer c) noexcept {
    if(!enabled())return;
    std::lock_guard<std::mutex> lock(drawMutex);auto& command=commands[c];command.blocked=true;
    if(command.pass){auto& p=*command.pass;switchMode(p,c,false);p.valid=false;if(command.native)p.device->vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,command.native);}
}
void ended(VkCommandBuffer c) noexcept {
    if(!enabled())return;std::string metadata;
    try{
        {std::lock_guard<std::mutex> lock(drawMutex);auto found=commands.find(c);if(found==commands.end()||!found->second.pass)return;
            auto& command=found->second;auto p=std::move(command.pass);
            if(p->used){
                if(command.native)p->device->vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,command.native);
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.image=p->target->image;
                b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                p->device->vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={p->target->width,p->target->height,1};
                p->device->vkCmdCopyImageToBuffer(c,p->target->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,p->target->buffer,1,&copy);
                VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                p->device->vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
            }
            if(command.recorded.size()<8)command.recorded.push_back({p->target,p->version,p->used&&p->valid&&(partial()||!p->unknown),p->known});else p->target->valid=false;
            if(p->target->version<=3||presents.load()%120==0)
                metadata="\"cmd\":"+handleText(c)+",\"wsi\":"+handleText(p->target->wsi)+",\"image\":"+handleText(p->target->image)+
                    ",\"version\":"+std::to_string(p->version)+",\"present\":"+std::to_string(p->target->present)+",\"known_draws\":"+std::to_string(p->known)+
                    ",\"unknown_draws\":"+std::to_string(p->unknown)+",\"valid\":"+std::string(p->used&&p->valid&&(partial()||!p->unknown)?"true":"false");
        }
        if(!metadata.empty())note("ui_capture_pass",metadata);
    }catch(const std::exception& e){note("ui_capture_end_failed","\"reason\":"+jsonString(e.what()));}
}
void submitted(VkQueue q,VkCommandBuffer c,VkResult result) noexcept {
    if(!enabled())return;std::lock_guard<std::mutex> lock(drawMutex);auto found=commands.find(c);if(found==commands.end())return;
    for(const auto& r:found->second.recorded)if(r.target->version==r.version){
        r.target->submitted=result==VK_SUCCESS?r.version:0;r.target->valid=result==VK_SUCCESS&&r.valid;
        r.target->queue=q;r.target->draws=r.draws;
    }
}
void presented(VkQueue q,const VkPresentInfoKHR* info,VkResult result) noexcept {
    if(!enabled()||result!=VK_SUCCESS||!info||info->swapchainCount!=1)return;
    try{
        VkImage wsi=VK_NULL_HANDLE;
        {std::lock_guard<std::mutex> lock(mutex);auto s=swapData.find(info->pSwapchains[0]);
            if(s!=swapData.end()&&info->pImageIndices[0]<s->second.images.size())wsi=s->second.images[info->pImageIndices[0]];}
        if(!wsi)return;
        const auto request=std::filesystem::path(output)/"ui-capture.request";
        const bool captureRequested=std::filesystem::exists(request);
        std::string metadata;
        {std::lock_guard<std::mutex> lock(drawMutex);
            for(auto& entry:drawDevices){
                auto& d=*entry.second;auto found=d.targets.find(wsi);if(found==d.targets.end())continue;auto& t=*found->second;
                if(d.failed||d.owner!=GetCurrentThreadId()||!t.valid||t.submitted!=t.version||t.queue!=q||t.present!=presents.load())return;
                t.presentedVersion=t.version;t.presentedFrame=t.present;
                if(!captureRequested)break;
                evr::vkCheck(d.vkQueueWaitIdle(q),"UI capture GPU completion");
                const auto prefix="ui-frame-"+std::to_string(t.present);const auto path=std::filesystem::path(output)/prefix;
                std::ofstream file(path.string()+".rgba8",std::ios::binary);file.write(static_cast<const char*>(t.mapped),std::streamsize(uint64_t(t.width)*t.height*4));
                if(!file)throw std::runtime_error("UI raw image write");
                metadata="\"frame\":"+std::to_string(t.present)+",\"version\":"+std::to_string(t.version)+",\"draws\":"+std::to_string(t.draws)+
                    ",\"width\":"+std::to_string(t.width)+",\"height\":"+std::to_string(t.height)+",\"wsi\":"+handleText(wsi)+
                    ",\"image\":"+handleText(t.image)+",\"queue\":"+handleText(q)+",\"queue_idle\":true,\"format\":\"RGBA8_UNORM\",\"premultiplied\":true";
                std::ofstream meta(path.string()+".json");meta<<'{'<<metadata<<"}\n";if(!meta)throw std::runtime_error("UI metadata write");
                std::filesystem::remove(request);break;
            }
        }
        if(!metadata.empty())note("ui_capture_saved",metadata);
    }catch(const std::exception& e){
        std::error_code ignored;std::filesystem::remove(std::filesystem::path(output)/"ui-capture.request",ignored);
        note("ui_capture_readback_failed","\"reason\":"+jsonString(e.what()));
    }
}
struct FrameSource {VkImage image{};uint32_t width=0,height=0;uint64_t version=0,present=0;};
bool sourceForFrame(VkDevice device,uint64_t beginPresent,VkQueue queue,FrameSource& result){
    if(!enabled() || beginPresent==UINT64_MAX || presents.load()!=beginPresent+1)return false;
    std::lock_guard<std::mutex> lock(drawMutex);auto d=drawDevices.find(device);
    if(d==drawDevices.end() || d->second->failed || d->second->owner!=GetCurrentThreadId())return false;
    FrameSource found{};
    for(const auto& entry:d->second->targets){const auto& t=*entry.second;
        if(t.present!=beginPresent || t.presentedFrame!=beginPresent || !t.version || t.submitted!=t.version ||
           t.presentedVersion!=t.version || !t.valid || t.passes!=1 || !t.draws || t.queue!=queue)continue;
        if(found.image)return false; // Ambiguous ownership must never select an arbitrary UI.
        found={t.image,t.width,t.height,t.version,t.present};
    }
    if(!found.image)return false;result=found;return true;
}
// Only after DeviceWaitIdle under every native queue gate. Used for diagnostic comparison.
std::vector<uint8_t> captureSource(VkDevice device,const FrameSource& source){
    std::lock_guard<std::mutex> lock(drawMutex);auto d=drawDevices.find(device);
    if(d!=drawDevices.end())for(const auto& entry:d->second->targets){const auto& t=*entry.second;
        if(t.image==source.image && t.version==source.version && t.present==source.present && t.valid){
            // Out of the mapped memory with streaming loads, 64 bytes a step (2026-10-22: a plain copy of the 8 MB took tens of
            // milliseconds on the frame - mapped memory the CPU does not cache reads slowly line by line).
            const auto* start=static_cast<const uint8_t*>(t.mapped);const size_t size=size_t(t.width)*t.height*4;std::vector<uint8_t> out(size);
            size_t done=0;
            if(!(reinterpret_cast<uintptr_t>(start)&15)&&!(reinterpret_cast<uintptr_t>(out.data())&15)){
                auto* s=reinterpret_cast<__m128i*>(const_cast<uint8_t*>(start));auto* d=reinterpret_cast<__m128i*>(out.data());const size_t blocks=size/64;
                for(size_t i=0;i<blocks;++i){const __m128i a=_mm_stream_load_si128(s+4*i),b=_mm_stream_load_si128(s+4*i+1),c=_mm_stream_load_si128(s+4*i+2),e=_mm_stream_load_si128(s+4*i+3);
                    _mm_store_si128(d+4*i,a);_mm_store_si128(d+4*i+1,b);_mm_store_si128(d+4*i+2,c);_mm_store_si128(d+4*i+3,e);}
                done=blocks*64;}
            if(done<size)std::memcpy(out.data()+done,start+done,size-done);
            return out;
        }
    }
    throw std::runtime_error("UI capture source identity changed");
}
// Caller holds the native queue gates. A snapshot is evidence, never a resource lease.
// Queue submissions must remain excluded until GPU reads finish.
std::string frameEvidence(VkDevice device,uint64_t beginPresent,VkQueue queue){
    if(!enabled())return "\"enabled\":false";
    std::lock_guard<std::mutex> lock(drawMutex);std::ostringstream out;
    out<<"\"enabled\":true,\"begin_present\":"<<beginPresent<<",\"current_present\":"<<presents.load()
       <<",\"xr_thread\":"<<GetCurrentThreadId()<<",\"candidates\":[";
    auto found=drawDevices.find(device);bool first=true;
    if(found!=drawDevices.end())for(const auto& entry:found->second->targets){
        const auto& t=*entry.second;if(!first)out<<',';first=false;
        out<<"{\"image\":"<<handleText(t.image)<<",\"wsi\":"<<handleText(t.wsi)<<",\"version\":"<<t.version
           <<",\"submitted\":"<<t.submitted<<",\"present\":"<<t.present<<",\"presented_version\":"<<t.presentedVersion
           <<",\"presented_frame\":"<<t.presentedFrame<<",\"ui_thread\":"<<found->second->owner
           <<",\"valid\":"<<(t.valid?"true":"false")<<",\"queue_match\":"<<(t.queue==queue?"true":"false")
           <<",\"failed\":"<<(found->second->failed?"true":"false")<<'}';
    }
    return out.str()+']';
}
void destroy(VkDevice device) noexcept {
    if(!enabled())return;std::shared_ptr<Device> d;
    {std::lock_guard<std::mutex> lock(drawMutex);auto found=drawDevices.find(device);if(found==drawDevices.end())return;d=found->second;}
    const auto result=d->vkDeviceWaitIdle(device);
    size_t count=0;
    {std::lock_guard<std::mutex> lock(drawMutex);count=d->targets.size();for(auto& t:d->targets)d->dispose(*t.second);
        for(auto i=commands.begin();i!=commands.end();)if(key(i->first)==key(device))i=commands.erase(i);else ++i;drawDevices.erase(device);}
    note("ui_capture_cleanup","\"targets\":"+std::to_string(count)+",\"idle_result\":"+std::to_string(result));
}
#undef UI_DRAW_API
}
VKAPI_ATTR void VKAPI_CALL evrCmdBeginQuery(VkCommandBuffer c,VkQueryPool p,uint32_t query,VkQueryControlFlags flags){
    uiDraw::block(c);deviceFn<PFN_vkCmdBeginQuery>(c,"vkCmdBeginQuery")(c,p,query,flags);
}

VKAPI_ATTR void VKAPI_CALL evrCmdClearAttachments(VkCommandBuffer c,uint32_t count,const VkClearAttachment* attachments,uint32_t rectangles,const VkClearRect* rects){
    uiDraw::block(c);sweepDepthClearAttachments(c,count,attachments,rectangles,rects);deviceFn<PFN_vkCmdClearAttachments>(c,"vkCmdClearAttachments")(c,count,attachments,rectangles,rects);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndirectCount(VkCommandBuffer c,VkBuffer b,VkDeviceSize offset,VkBuffer count,VkDeviceSize countOffset,uint32_t maximum,uint32_t stride){
    cmdStack("draw_indirect_count","\"maximum\":"+std::to_string(maximum));sweepDepthDraw(c);gpuTimingNote(c,0);uiDraw::draw(c,false);deviceFn<PFN_vkCmdDrawIndirectCount>(c,"vkCmdDrawIndirectCount")(c,b,offset,count,countOffset,maximum,stride);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndexedIndirectCount(VkCommandBuffer c,VkBuffer b,VkDeviceSize offset,VkBuffer count,VkDeviceSize countOffset,uint32_t maximum,uint32_t stride){
    cmdStack("draw_indirect_count","\"maximum\":"+std::to_string(maximum));sweepDepthDraw(c);gpuTimingNote(c,0);uiDraw::draw(c,false);deviceFn<PFN_vkCmdDrawIndexedIndirectCount>(c,"vkCmdDrawIndexedIndirectCount")(c,b,offset,count,countOffset,maximum,stride);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndirectCountKHR(VkCommandBuffer c,VkBuffer b,VkDeviceSize offset,VkBuffer count,VkDeviceSize countOffset,uint32_t maximum,uint32_t stride){
    cmdStack("draw_indirect_count","\"maximum\":"+std::to_string(maximum));sweepDepthDraw(c);gpuTimingNote(c,0);uiDraw::draw(c,false);deviceFn<PFN_vkCmdDrawIndirectCountKHR>(c,"vkCmdDrawIndirectCountKHR")(c,b,offset,count,countOffset,maximum,stride);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawIndexedIndirectCountKHR(VkCommandBuffer c,VkBuffer b,VkDeviceSize offset,VkBuffer count,VkDeviceSize countOffset,uint32_t maximum,uint32_t stride){
    cmdStack("draw_indirect_count","\"maximum\":"+std::to_string(maximum));sweepDepthDraw(c);gpuTimingNote(c,0);uiDraw::draw(c,false);deviceFn<PFN_vkCmdDrawIndexedIndirectCountKHR>(c,"vkCmdDrawIndexedIndirectCountKHR")(c,b,offset,count,countOffset,maximum,stride);
}
