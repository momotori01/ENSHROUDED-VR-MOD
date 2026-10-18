#pragma once
#include <evr/stereo_transfer.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
namespace evr {
// The last seconds of both eyes, kept on the GPU for the VR bug report (2026-10-18; the user: "the left/right difference
// of the detailed models I see mostly while moving - when I see it I open the menu and press the bug report, so keep the
// 10 seconds before"; "won't keeping them cost a lot? already the 30 seconds after the press stutter").
// Every `everyMs` (on a frame of both eyes) each eye's picture, right after its copy into the headset's image (stereo
// transfer, the source still in TRANSFER_SRC_OPTIMAL), is shrunk to half its width and height into the next slot of a
// ring on the GPU (one blit an eye, no wait: nothing reaches the CPU). Only save() - asked when the button is pressed, in
// the menu, and at the end of the report's window - reads the slots back (waiting for them) and writes them, oldest
// first, as native-xr/frame-<n>-eye-<e>.ppm, the names the pictures of the report have.
class EyeHistory {
    Vulkan& v;
    PFN_vkCmdBlitImage blit=nullptr;
    struct Slot {std::array<VkImage,2> image{};std::array<VkDeviceMemory,2> memory{};uint64_t frame=0;uint64_t ms=0;std::array<bool,2> written{};};
    std::vector<Slot> ring;uint32_t width=0,height=0;VkFormat format=VK_FORMAT_UNDEFINED;
    uint32_t next=0;uint64_t lastMs=0;bool due=false;uint64_t dueFrame=0,dueMs=0;
    VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};
    void destroySlots() noexcept {
        for(auto& s:ring)for(unsigned e=0;e<2;++e){if(s.image[e])v.vkDestroyImage(v.device,s.image[e],nullptr);if(s.memory[e])v.vkFreeMemory(v.device,s.memory[e],nullptr);}
        ring.clear();width=height=0;format=VK_FORMAT_UNDEFINED;next=0;
    }
    void ensure(uint32_t w,uint32_t h,VkFormat f){
        if(!ring.empty()&&w==width&&h==height&&f==format)return;
        destroySlots();
        ring.resize(slots);
        for(auto& s:ring)for(unsigned e=0;e<2;++e){
            VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=f;i.extent={w,h,1};i.mipLevels=1;i.arrayLayers=1;
            i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            vkCheck(v.vkCreateImage(v.device,&i,nullptr,&s.image[e]),"eye history image");
            VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,s.image[e],&req);
            s.memory[e]=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,s.image[e],s.memory[e],0),"eye history bind");
        }
        width=w;height=h;format=f;
    }
    void barrier(VkCommandBuffer c,VkImage image,VkImageLayout from,VkImageLayout to,VkAccessFlags srcAccess,VkAccessFlags dstAccess,VkPipelineStageFlags srcStage,VkPipelineStageFlags dstStage){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=srcAccess;b.dstAccessMask=dstAccess;b.oldLayout=from;b.newLayout=to;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        v.vkCmdPipelineBarrier(c,srcStage,dstStage,0,0,nullptr,0,nullptr,1,&b);
    }
public:
    uint32_t slots=20;uint64_t everyMs=500;   // 10 seconds at two pictures a second
    bool frozen=false;                         // while a save is pending: nothing new goes in
    uint64_t shots=0,saves=0;
    explicit EyeHistory(Vulkan& vk):v(vk){
        blit=reinterpret_cast<PFN_vkCmdBlitImage>(v.vkGetDeviceProcAddr(v.device,"vkCmdBlitImage"));
        if(!blit)throw std::runtime_error("eye history: no vkCmdBlitImage");
        VkCommandPoolCreateInfo p{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};p.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;p.queueFamilyIndex=v.family;
        vkCheck(v.vkCreateCommandPool(v.device,&p,nullptr,&pool),"eye history pool");
        VkCommandBufferAllocateInfo c{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};c.commandPool=pool;c.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;c.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(v.device,&c,&cmd),"eye history command buffer");
        VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(v.device,&f,nullptr,&fence),"eye history fence");
    }
    EyeHistory(const EyeHistory&)=delete;EyeHistory& operator=(const EyeHistory&)=delete;
    // The owner destroys this after the GPU is done with the transfers (as the stereo transfer).
    ~EyeHistory(){destroySlots();if(fence)v.vkDestroyFence(v.device,fence,nullptr);if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);}
    // Before a pair's copy: is a picture due in it (both eyes drawn, everyMs since the last, not frozen)?
    void begin(uint64_t frame,uint64_t ms,bool bothEyes){due=!frozen&&bothEyes&&(!lastMs||ms<lastMs||ms-lastMs>=everyMs);dueFrame=frame;dueMs=ms;}
    // stereo transfer's history hook: the eye's source, in TRANSFER_SRC_OPTIMAL, right after its copy.
    void record(VkCommandBuffer c,size_t eye,const TransferImage& s){
        if(!due||eye>1||!s.width||!s.height)return;
        ensure(std::max(1u,s.width/2),std::max(1u,s.height/2),s.format);
        auto& slot=ring[next];
        // the slot's old picture is dropped (its layout too); the last read of it was a save's, which waited
        barrier(c,slot.image[eye],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        region.srcOffsets[1]={int32_t(s.width),int32_t(s.height),1};region.dstOffsets[1]={int32_t(width),int32_t(height),1};
        blit(c,s.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,slot.image[eye],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_LINEAR);
        barrier(c,slot.image[eye],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        slot.written[eye]=true;slot.frame=dueFrame;slot.ms=dueMs;
    }
    // After the pair's copy was recorded.
    void end(){if(!due||ring.empty())return;if(ring[next].written[0]||ring[next].written[1]){next=(next+1)%uint32_t(ring.size());lastMs=dueMs;++shots;}due=false;}
    // The slots, oldest first, read back and written (`folder`/frame-<n>-eye-<e>.ppm, RGB). Waits for the GPU: call it
    // between pairs on the thread that records the transfers, after the transfer's own work is done (finish()).
    // Returns the frames written; the slots are emptied.
    std::vector<uint64_t> save(VkQueue queue,const std::string& folder){
        std::vector<uint64_t> frames;if(ring.empty())return frames;
        const VkDeviceSize bytes=VkDeviceSize(width)*height*4;
        VkBuffer buffer{};VkDeviceMemory memory{};
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes*2;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&buffer),"eye history readback buffer");
        try{
            VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,buffer,&req);
            memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,buffer,memory,0),"eye history readback bind");
            std::vector<uint32_t> order;for(uint32_t k=0;k<ring.size();++k){const uint32_t at=(next+k)%uint32_t(ring.size());if(ring[at].written[0]&&ring[at].written[1])order.push_back(at);}
            std::vector<char> row(size_t(width)*3);
            for(const auto at:order){
                auto& slot=ring[at];
                vkCheck(v.vkResetCommandBuffer(cmd,0),"eye history reset");
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkCheck(v.vkBeginCommandBuffer(cmd,&begin),"eye history begin");
                for(unsigned e=0;e<2;++e){VkBufferImageCopy copy{};copy.bufferOffset=bytes*e;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};
                    v.vkCmdCopyImageToBuffer(cmd,slot.image[e],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);}
                VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
                vkCheck(v.vkEndCommandBuffer(cmd),"eye history end");vkCheck(v.vkResetFences(v.device,1,&fence),"eye history reset fence");
                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
                vkCheck(v.vkQueueSubmit(queue,1,&submit,fence),"eye history submit");
                vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"eye history fence");
                void* mapped=nullptr;vkCheck(v.vkMapMemory(v.device,memory,0,bytes*2,0,&mapped),"eye history map");
                const bool bgr=format==VK_FORMAT_B8G8R8A8_UNORM||format==VK_FORMAT_B8G8R8A8_SRGB;
                for(unsigned e=0;e<2;++e){
                    std::ofstream out(folder+"/frame-"+std::to_string(slot.frame)+"-eye-"+std::to_string(e)+".ppm",std::ios::binary);
                    if(!out)continue;
                    out<<"P6\n"<<width<<' '<<height<<"\n255\n";
                    const auto* p=static_cast<const unsigned char*>(mapped)+bytes*e;
                    for(uint32_t y=0;y<height;++y){const auto* r=p+size_t(y)*width*4;
                        for(uint32_t x=0;x<width;++x){row[x*3]=char(r[x*4+(bgr?2:0)]);row[x*3+1]=char(r[x*4+1]);row[x*3+2]=char(r[x*4+(bgr?0:2)]);}
                        out.write(row.data(),std::streamsize(row.size()));}
                }
                v.vkUnmapMemory(v.device,memory);
                frames.push_back(slot.frame);slot.written={false,false};
            }
        }catch(...){v.vkDestroyBuffer(v.device,buffer,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);throw;}
        v.vkDestroyBuffer(v.device,buffer,nullptr);v.vkFreeMemory(v.device,memory,nullptr);
        ++saves;return frames;
    }
};
}
