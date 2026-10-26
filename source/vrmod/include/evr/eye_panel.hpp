#pragma once
#include <evr/stereo_transfer.hpp>
#include <ui_transfer_vertex.h>
#include <eye_panel_fragment.h>
#include <unordered_map>
namespace evr {
// The mod's own picture in an eye's image (2026-10-26, the staff's spells beside the aim mark - user: "cutting the UI always goes
// wrong somewhere; take the spell switching out of the UI and draw what is needed at the reticle"): an RGBA8 picture (display
// values, straight alpha, rows from the top) drawn over a rectangle of the runtime's eye image right after the eye copy, like the
// aim mark (eye_mark.hpp) - same command buffer, fence and queue ownership; runtime images only, no game image is touched. The
// picture is uploaded once per change, in the first command buffer that draws it after setPicture.
// x0..y1: the pixels to shade (the picture's corners as this eye sees them, kept inside the image); h: this eye's pixel (x, y, 1)
// to the picture's place (u, v, w) - u / w, v / w from 0 to 1 over the picture (eye_panel.frag).
struct EyePanelParams { float x0=0,y0=0,x1=0,y1=0;std::array<float,9> h{};float strength=1; };
class EyePanel {
    Vulkan& v;
    VkFormat format,pictureFormat;
    VkRenderPass pass{};VkPipelineLayout layout{};VkPipeline pipeline{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkDescriptorSet set{};VkSampler sampler{};
    VkImage image{};VkDeviceMemory memory{};VkImageView view{};uint32_t width=0,height=0;bool written=false,upload=false;
    VkBuffer staging{};VkDeviceMemory stagingMemory{};VkDeviceSize stagingBytes=0;std::vector<uint8_t> bytes;
    struct Target{VkImageView view{};VkFramebuffer framebuffer{};uint32_t width=0,height=0;};
    std::unordered_map<VkImage,Target> targets;
#define EYE_PANEL_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) \
 X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateSampler) X(vkDestroySampler) X(vkCmdBindDescriptorSets) X(vkCmdCopyBufferToImage)
#define DECL(n) PFN_##n n=nullptr;
    EYE_PANEL_API(DECL)
#undef DECL
    static bool srgb(VkFormat f){return f==VK_FORMAT_R8G8B8A8_SRGB||f==VK_FORMAT_B8G8R8A8_SRGB;}
    void destroyPicture() noexcept {
        if(view)v.vkDestroyImageView(v.device,view,nullptr);if(image)v.vkDestroyImage(v.device,image,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);
        view=VK_NULL_HANDLE;image=VK_NULL_HANDLE;memory=VK_NULL_HANDLE;written=false;
    }
    void destroy() noexcept {
        for(auto& [img,t]:targets){if(t.framebuffer)v.vkDestroyFramebuffer(v.device,t.framebuffer,nullptr);if(t.view)v.vkDestroyImageView(v.device,t.view,nullptr);}
        targets.clear();destroyPicture();
        if(staging)v.vkDestroyBuffer(v.device,staging,nullptr);if(stagingMemory)v.vkFreeMemory(v.device,stagingMemory,nullptr);
        if(sampler&&vkDestroySampler)vkDestroySampler(v.device,sampler,nullptr);
        if(pool&&vkDestroyDescriptorPool)vkDestroyDescriptorPool(v.device,pool,nullptr);
        if(setLayout&&vkDestroyDescriptorSetLayout)vkDestroyDescriptorSetLayout(v.device,setLayout,nullptr);
        if(pipeline)v.vkDestroyPipeline(v.device,pipeline,nullptr);
        if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);
        if(pass)v.vkDestroyRenderPass(v.device,pass,nullptr);
    }
    void init(){
        if(!rgba8(format))throw std::invalid_argument("eye panel needs the RGBA8 eye format");
#define LOAD(n) n=reinterpret_cast<PFN_##n>(v.vkGetDeviceProcAddr(v.device,#n));if(!n)throw std::runtime_error(#n);
        EYE_PANEL_API(LOAD)
#undef LOAD
        VkAttachmentDescription a{};a.format=format;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;a.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;
        VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&a;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;
        vkCheck(v.vkCreateRenderPass(v.device,&rp,nullptr,&pass),"eye panel render pass");
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};dl.bindingCount=1;dl.pBindings=&binding;
        vkCheck(vkCreateDescriptorSetLayout(v.device,&dl,nullptr,&setLayout),"eye panel set layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&size;
        vkCheck(vkCreateDescriptorPool(v.device,&dp,nullptr,&pool),"eye panel pool");
        VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ds.descriptorPool=pool;ds.descriptorSetCount=1;ds.pSetLayouts=&setLayout;
        vkCheck(vkAllocateDescriptorSets(v.device,&ds,&set),"eye panel set");
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.maxLod=0;
        vkCheck(vkCreateSampler(v.device,&si,nullptr,&sampler),"eye panel sampler");
        VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,64};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;
        vkCheck(v.vkCreatePipelineLayout(v.device,&pl,nullptr,&layout),"eye panel pipeline layout");
        std::array<VkShaderModule,2> modules{};
        try{
            for(unsigned i=0;i<2;++i){VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.pCode=i?eyePanelFragment:uiTransferVertex;sm.codeSize=i?sizeof(eyePanelFragment):sizeof(uiTransferVertex);vkCheck(v.vkCreateShaderModule(v.device,&sm,nullptr,&modules[i]),"eye panel shader");}
            std::array<VkPipelineShaderStageCreateInfo,2> stages{};
            for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[i];stages[i].pName="main";}
            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState ba{};ba.blendEnable=VK_TRUE;ba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=VK_BLEND_OP_ADD;
            ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.alphaBlendOp=VK_BLEND_OP_ADD;
            ba.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT;
            VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
            VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dy.dynamicStateCount=2;dy.pDynamicStates=dynamic;
            VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;gp.pDynamicState=&dy;gp.layout=layout;gp.renderPass=pass;
            vkCheck(v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline),"eye panel pipeline");
        }catch(...){for(auto m:modules)if(m)v.vkDestroyShaderModule(v.device,m,nullptr);throw;}
        for(auto m:modules)v.vkDestroyShaderModule(v.device,m,nullptr);
    }
    Target& target(const TransferImage& dst){
        auto found=targets.find(dst.image);
        if(found!=targets.end()){if(found->second.width!=dst.width||found->second.height!=dst.height)throw std::runtime_error("eye panel target extent changed");return found->second;}
        if(targets.size()>=16)throw std::runtime_error("eye panel target budget");
        if(dst.format!=format||!(dst.usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))throw std::invalid_argument("eye panel target needs the eye format and color attachment usage");
        Target t{};t.width=dst.width;t.height=dst.height;
        VkImageViewCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};i.image=dst.image;i.viewType=VK_IMAGE_VIEW_TYPE_2D;i.format=format;i.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCheck(v.vkCreateImageView(v.device,&i,nullptr,&t.view),"eye panel view");
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=1;fi.pAttachments=&t.view;fi.width=t.width;fi.height=t.height;fi.layers=1;
        if(v.vkCreateFramebuffer(v.device,&fi,nullptr,&t.framebuffer)!=VK_SUCCESS){v.vkDestroyImageView(v.device,t.view,nullptr);throw std::runtime_error("eye panel framebuffer");}
        return targets.emplace(dst.image,t).first->second;
    }
    // The picture's image and the staging buffer's bytes, made before anything is recorded.
    void preparePicture(){
        if(!image){
            VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=pictureFormat;i.extent={width,height,1};i.mipLevels=i.arrayLayers=1;
            i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            vkCheck(v.vkCreateImage(v.device,&i,nullptr,&image),"eye panel image");VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,image,&req);
            memory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,image,memory,0),"eye panel image memory");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=pictureFormat;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            vkCheck(v.vkCreateImageView(v.device,&vi,nullptr,&view),"eye panel picture view");
            VkDescriptorImageInfo d{sampler,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};w.dstSet=set;w.dstBinding=0;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w.pImageInfo=&d;
            vkUpdateDescriptorSets(v.device,1,&w,0,nullptr);written=false;upload=true;
        }
        if(!upload)return;
        const VkDeviceSize n=VkDeviceSize(bytes.size());
        if(n>stagingBytes){
            if(staging)v.vkDestroyBuffer(v.device,staging,nullptr);if(stagingMemory)v.vkFreeMemory(v.device,stagingMemory,nullptr);staging=VK_NULL_HANDLE;stagingMemory=VK_NULL_HANDLE;stagingBytes=0;
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=n;b.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;b.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&staging),"eye panel staging");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,staging,&req);
            stagingMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,staging,stagingMemory,0),"eye panel staging memory");stagingBytes=n;
        }
        void* mapped{};vkCheck(v.vkMapMemory(v.device,stagingMemory,0,n,0,&mapped),"eye panel staging map");std::memcpy(mapped,bytes.data(),size_t(n));v.vkUnmapMemory(v.device,stagingMemory);
    }
    void imageBarrier(VkCommandBuffer cmd,VkImage img,VkImageLayout from,VkImageLayout to,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags a,VkPipelineStageFlags b){
        VkImageMemoryBarrier m{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};m.oldLayout=from;m.newLayout=to;m.srcAccessMask=src;m.dstAccessMask=dst;
        m.srcQueueFamilyIndex=m.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;m.image=img;m.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        v.vkCmdPipelineBarrier(cmd,a,b,0,0,nullptr,0,nullptr,1,&m);
    }
  public:
    EyePanel(Vulkan& device,VkFormat f):v(device),format(f),pictureFormat(srgb(f)?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM){try{init();}catch(...){destroy();throw;}}
    EyePanel(const EyePanel&)=delete;EyePanel& operator=(const EyePanel&)=delete;
    // The owner destroys this only after the transfer fence retired (StereoTransfer is destroyed first).
    ~EyePanel(){destroy();}
    bool hasPicture() const {return width&&height&&!bytes.empty();}
    uint32_t pictureWidth() const {return width;}
    uint32_t pictureHeight() const {return height;}
    // A new picture: RGBA8, display values, straight alpha, rows from the top. Uploaded with the next record(); the image is made
    // again when the size changes - only between frames (the caller's fence retired the last draw, as for the aim mark).
    void setPicture(const std::vector<uint8_t>& rgba,uint32_t w,uint32_t h){
        if(!w||!h||w>4096||h>4096||rgba.size()!=size_t(w)*h*4)throw std::invalid_argument("eye panel picture");
        bytes=rgba;if(w!=width||h!=height){destroyPicture();width=w;height=h;}upload=true;
    }
    // attachment false: dst is in TRANSFER_DST_OPTIMAL after the eye copy; true: an earlier draw left it in
    // COLOR_ATTACHMENT_OPTIMAL. Either way it is left in COLOR_ATTACHMENT_OPTIMAL.
    void record(VkCommandBuffer cmd,const TransferImage& dst,const EyePanelParams& p,bool attachment){
        if(!hasPicture())throw std::logic_error("eye panel without a picture");
        auto& t=target(dst);preparePicture();
        if(upload){
            imageBarrier(cmd,image,written?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,written?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};
            vkCmdCopyBufferToImage(cmd,staging,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
            imageBarrier(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            upload=false;written=true;
        }
        if(!attachment)imageBarrier(cmd,dst.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        const int32_t x0=std::clamp(int32_t(std::floor(p.x0)),0,int32_t(t.width)),y0=std::clamp(int32_t(std::floor(p.y0)),0,int32_t(t.height));
        const int32_t x1=std::clamp(int32_t(std::ceil(p.x1)),0,int32_t(t.width)),y1=std::clamp(int32_t(std::ceil(p.y1)),0,int32_t(t.height));
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=pass;r.framebuffer=t.framebuffer;r.renderArea={{0,0},{t.width,t.height}};
        v.vkCmdBeginRenderPass(cmd,&r,VK_SUBPASS_CONTENTS_INLINE);
        if(x1>x0&&y1>y0){
            v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&set,0,nullptr);
            const float push[16]={p.h[0],p.h[1],p.h[2],0,p.h[3],p.h[4],p.h[5],0,p.h[6],p.h[7],p.h[8],0,p.strength,0,0,0};
            v.vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push);
            VkViewport viewport{0,0,float(t.width),float(t.height),0,1};VkRect2D scissor{{x0,y0},{uint32_t(x1-x0),uint32_t(y1-y0)}};
            v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);v.vkCmdDraw(cmd,3,1,0,0);
        }
        v.vkCmdEndRenderPass(cmd);
    }
};
#undef EYE_PANEL_API
}
