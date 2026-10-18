#pragma once
#include <evr/stereo_transfer.hpp>
#include <ui_transfer_vertex.h>
#include <eye_mark_fragment.h>
#include <unordered_map>
namespace evr {
// The aim mark in an eye's image (aim_mark.hpp): a small dot drawn into the runtime's eye image right after the eye
// copy, like the status tint (edge_tint.hpp) - same command buffer, fence and queue ownership; runtime images only,
// no game image is touched. Only the few pixels around the dot are shaded (the scissor).
struct EyeMarkParams { float x=0,y=0,radius=0,strength=1; }; // pixels of the eye image
// A short beam (the throw's guide): from (x, y) to (toX, toY), radius in pixels.
struct EyeBeamParams { float x=0,y=0,toX=0,toY=0,radius=0,strength=1; };
class EyeMark {
    Vulkan& v;
    VkFormat format;
    VkRenderPass pass{};VkPipelineLayout layout{};VkPipeline pipeline{};
    struct Target{VkImageView view{};VkFramebuffer framebuffer{};uint32_t width=0,height=0;};
    std::unordered_map<VkImage,Target> targets;
    void destroy() noexcept {
        for(auto& [image,t]:targets){if(t.framebuffer)v.vkDestroyFramebuffer(v.device,t.framebuffer,nullptr);if(t.view)v.vkDestroyImageView(v.device,t.view,nullptr);}
        targets.clear();
        if(pipeline)v.vkDestroyPipeline(v.device,pipeline,nullptr);
        if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);
        if(pass)v.vkDestroyRenderPass(v.device,pass,nullptr);
    }
    void init(){
        if(!rgba8(format))throw std::invalid_argument("eye mark needs the RGBA8 eye format");
        VkAttachmentDescription a{};a.format=format;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;a.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;
        VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&a;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;
        vkCheck(v.vkCreateRenderPass(v.device,&rp,nullptr,&pass),"eye mark render pass");
        VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,32};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;
        vkCheck(v.vkCreatePipelineLayout(v.device,&pl,nullptr,&layout),"eye mark pipeline layout");
        std::array<VkShaderModule,2> modules{};
        try{
            for(unsigned i=0;i<2;++i){VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.pCode=i?eyeMarkFragment:uiTransferVertex;sm.codeSize=i?sizeof(eyeMarkFragment):sizeof(uiTransferVertex);vkCheck(v.vkCreateShaderModule(v.device,&sm,nullptr,&modules[i]),"eye mark shader");}
            std::array<VkPipelineShaderStageCreateInfo,2> stages{};
            for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[i];stages[i].pName="main";}
            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            // Straight alpha over the eye's colour; the eye image's alpha channel is left unchanged.
            VkPipelineColorBlendAttachmentState ba{};ba.blendEnable=VK_TRUE;ba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=VK_BLEND_OP_ADD;
            ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.alphaBlendOp=VK_BLEND_OP_ADD;
            ba.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT;
            VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
            VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=dynamic;
            VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;gp.pDynamicState=&ds;gp.layout=layout;gp.renderPass=pass;gp.basePipelineIndex=-1;
            vkCheck(v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline),"eye mark pipeline");
        }catch(...){for(auto m:modules)if(m)v.vkDestroyShaderModule(v.device,m,nullptr);throw;}
        for(auto m:modules)v.vkDestroyShaderModule(v.device,m,nullptr);
    }
    Target& target(const TransferImage& image){
        auto found=targets.find(image.image);
        if(found!=targets.end()){if(found->second.width!=image.width||found->second.height!=image.height)throw std::runtime_error("eye mark target extent changed");return found->second;}
        if(targets.size()>=16)throw std::runtime_error("eye mark target budget");
        if(image.format!=format||!(image.usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))throw std::invalid_argument("eye mark target needs the eye format and color attachment usage");
        Target t{};t.width=image.width;t.height=image.height;
        VkImageViewCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};i.image=image.image;i.viewType=VK_IMAGE_VIEW_TYPE_2D;i.format=format;i.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCheck(v.vkCreateImageView(v.device,&i,nullptr,&t.view),"eye mark view");
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=1;fi.pAttachments=&t.view;fi.width=t.width;fi.height=t.height;fi.layers=1;
        if(v.vkCreateFramebuffer(v.device,&fi,nullptr,&t.framebuffer)!=VK_SUCCESS){v.vkDestroyImageView(v.device,t.view,nullptr);throw std::runtime_error("eye mark framebuffer");}
        return targets.emplace(image.image,t).first->second;
    }
  public:
    EyeMark(Vulkan& device,VkFormat f):v(device),format(f){try{init();}catch(...){destroy();throw;}}
    EyeMark(const EyeMark&)=delete;EyeMark& operator=(const EyeMark&)=delete;
    // The owner destroys this only after the transfer fence retired (StereoTransfer is destroyed first).
    ~EyeMark(){destroy();}
    // The target's view and framebuffer, made before anything is recorded: a failure here leaves the command buffer
    // as it was.
    void prepare(const TransferImage& dst){target(dst);}
    // attachment false: dst is in TRANSFER_DST_OPTIMAL after the eye copy; true: an earlier draw (the status tint) left
    // it in COLOR_ATTACHMENT_OPTIMAL. Either way it is left in COLOR_ATTACHMENT_OPTIMAL.
    void record(VkCommandBuffer cmd,const TransferImage& dst,const EyeMarkParams& p,bool attachment){
        auto& t=target(dst);
        if(!attachment){
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=dst.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
        }
        // Only the dot's surroundings, kept inside the image.
        const float reach=p.radius+3;
        const int32_t x0=std::clamp(int32_t(std::floor(p.x-reach)),0,int32_t(t.width)),y0=std::clamp(int32_t(std::floor(p.y-reach)),0,int32_t(t.height));
        const int32_t x1=std::clamp(int32_t(std::ceil(p.x+reach)),0,int32_t(t.width)),y1=std::clamp(int32_t(std::ceil(p.y+reach)),0,int32_t(t.height));
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=pass;r.framebuffer=t.framebuffer;r.renderArea={{0,0},{t.width,t.height}};
        v.vkCmdBeginRenderPass(cmd,&r,VK_SUBPASS_CONTENTS_INLINE);
        if(x1>x0&&y1>y0){
            v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
            const float push[8]={p.x,p.y,p.radius,p.strength,0,0,0,0};
            v.vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push);
            VkViewport viewport{0,0,float(t.width),float(t.height),0,1};VkRect2D scissor{{x0,y0},{uint32_t(x1-x0),uint32_t(y1-y0)}};
            v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);v.vkCmdDraw(cmd,3,1,0,0);
        }
        v.vkCmdEndRenderPass(cmd);
    }
    // The beam: the same as record() for a segment - only the pixels round it are shaded.
    void recordBeam(VkCommandBuffer cmd,const TransferImage& dst,const EyeBeamParams& p,bool attachment){
        auto& t=target(dst);
        if(!attachment){
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=dst.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
        }
        const float reach=p.radius+3;
        const int32_t x0=std::clamp(int32_t(std::floor(std::min(p.x,p.toX)-reach)),0,int32_t(t.width)),y0=std::clamp(int32_t(std::floor(std::min(p.y,p.toY)-reach)),0,int32_t(t.height));
        const int32_t x1=std::clamp(int32_t(std::ceil(std::max(p.x,p.toX)+reach)),0,int32_t(t.width)),y1=std::clamp(int32_t(std::ceil(std::max(p.y,p.toY)+reach)),0,int32_t(t.height));
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=pass;r.framebuffer=t.framebuffer;r.renderArea={{0,0},{t.width,t.height}};
        v.vkCmdBeginRenderPass(cmd,&r,VK_SUBPASS_CONTENTS_INLINE);
        if(x1>x0&&y1>y0){
            v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
            const float push[8]={p.x,p.y,p.radius,p.strength,p.toX,p.toY,1,0};
            v.vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push);
            VkViewport viewport{0,0,float(t.width),float(t.height),0,1};VkRect2D scissor{{x0,y0},{uint32_t(x1-x0),uint32_t(y1-y0)}};
            v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);v.vkCmdDraw(cmd,3,1,0,0);
        }
        v.vkCmdEndRenderPass(cmd);
    }
};
}
