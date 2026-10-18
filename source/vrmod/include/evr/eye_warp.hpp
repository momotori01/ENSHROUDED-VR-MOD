#pragma once
#include <evr/openxr_client.hpp>
#include <evr/stereo_transfer.hpp>
#include <eye_warp_vertex.h>
#include <eye_warp_fragment.h>
#include <ui_transfer_vertex.h>
#include <eye_depth_copy_fragment.h>
#include <unordered_map>
#include <fstream>
#include <functional>
#include <cstring>
namespace evr {
// Alternate-eye rendering, second stage (2026-10-15; user 2026-10-14: "try the PureDark way" = PureDark's Alternate
// Frame Warping). The first stage draws one eye a frame and shows the other eye's last picture at the pose it was
// drawn with (openxr_client.hpp showStaleEyeAt). Here that eye's picture is warped to this frame's pose instead:
// when an eye is drawn, its colour and the depth the game wrote for it (the depth it hands to DLSS, at the render
// size) are kept in images of this class, with the view they were drawn with; on a frame that leaves the eye out, a
// grid over the kept picture is moved by that depth into this frame's view (shaders/eye_warp.vert) and drawn into
// the eye's runtime image, which is then shown at this frame's pose like a drawn one. Where the new view sees past
// the old picture the image is black; where a near thing hid a far one the grid stretches (the known look of such
// warps). Runtime images and this class's own images only; no game image is written.
// Third stage (2026-10-15, the way PureDark's mod makes the other eye): the eye left out is made from the OTHER eye's
// picture of this same frame - the one just drawn - moved to this eye's view by its depth (draw with fromOther): no
// frame of lag for anything that moves (the hands, the weapons, the enemies). The cells that span a depth step are
// left out (eye_warp.frag: the revealed edges), and this eye's own last picture, warped as in the second stage and
// drawn first, shows through those holes. One instance keeps both eyes. With EVR_EYE_WARP_CHECK=1 a frame that draws
// both eyes now and then also makes one eye from the other and compares it with the drawn one (check: the mean
// difference and the holes' share).
class EyeWarp {
    Vulkan& v;VkFormat format;uint32_t width,height;
    VkRenderPass pass{};VkPipelineLayout layout{};VkPipeline pipeline{},fillPipeline{};
    // The depth attachment of the warp's render pass: the grid writes 0 where it draws, the hole fill (mode 2) draws at 0.5
    // only where that is still the cleared 1 (the holes), so the fill's fragment shader runs for the holes alone.
    VkImage depthBuffer{};VkDeviceMemory depthBufferMemory{};VkImageView depthBufferView{};
    // The uniform of the hole fill (binding 2 of both sets): the other eye's view -> this frame's view, its fov tangents.
    VkBuffer extras{};VkDeviceMemory extrasMemory{};void* extrasMapped=nullptr;
    VkDescriptorSetLayout setLayout{};VkDescriptorPool descriptors{};std::array<VkDescriptorSet,2> sets{};VkSampler colourSampler{},depthSampler{};
    VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};bool pending=false;
    VkBuffer readback{};VkDeviceMemory readbackMemory{};
    // The self-check's scratch picture and its readbacks (made on the first check).
    VkImage scratch{};VkDeviceMemory scratchMemory{};VkImageView scratchView{};VkFramebuffer scratchFramebuffer{};VkBuffer checkReadback[2]{};VkDeviceMemory checkReadbackMemory[2]{};
    struct Target {VkImageView view{};VkFramebuffer framebuffer{};};
    std::unordered_map<VkImage,Target> targets;
    struct Kept {
        VkImage colour{},depth{};VkDeviceMemory colourMemory{},depthMemory{};VkImageView colourView{},depthView{};VkFramebuffer depthFramebuffer{};
        uint32_t depthWidth=0,depthHeight=0;VkFormat depthFormat=VK_FORMAT_UNDEFINED;   // the game's depth image this one is made from, its size and format
        VkImage gameDepth{};VkImageView gameDepthView{};VkDescriptorSet gameDepthSet{};   // the game's depth image as the copy pass samples it
        bool colourWritten=false,depthWritten=false,bound=false,valid=false;XrView view{XR_TYPE_VIEW};uint64_t frame=0;
    };
    std::array<Kept,2> kept{};
    // The depth copy: the game's depth (no transfer-source usage; sampled) drawn into the kept R32 image by a
    // full-screen triangle (shaders/eye_depth_copy.frag), in the copy's command buffer.
    VkRenderPass depthPass{};VkPipelineLayout depthCopyLayout{};VkPipeline depthPipeline{};VkDescriptorSetLayout depthSetLayout{};VkDescriptorPool depthDescriptors{};unsigned depthSetsUsed=0;
    static constexpr unsigned depthSetCapacity=32;
#define EYE_WARP_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) \
 X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateSampler) X(vkDestroySampler) X(vkCmdBindDescriptorSets)
#define DECL(n) PFN_##n n=nullptr;
    EYE_WARP_API(DECL)
#undef DECL
    static bool depthFormatOf(VkFormat f){return f==VK_FORMAT_D16_UNORM||f==VK_FORMAT_D32_SFLOAT||f==VK_FORMAT_X8_D24_UNORM_PACK32||f==VK_FORMAT_D16_UNORM_S8_UINT||f==VK_FORMAT_D24_UNORM_S8_UINT||f==VK_FORMAT_D32_SFLOAT_S8_UINT;}
    void barrier(VkCommandBuffer c,VkImage image,VkImageAspectFlags aspect,VkImageLayout oldLayout,VkImageLayout next,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags from,VkPipelineStageFlags to){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=oldLayout;b.newLayout=next;b.srcAccessMask=src;b.dstAccessMask=dst;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={aspect,0,1,0,1};
        v.vkCmdPipelineBarrier(c,from,to,0,0,nullptr,0,nullptr,1,&b);
    }
    VkImageView view(VkImage image,VkFormat f,VkImageAspectFlags aspect){
        VkImageViewCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};i.image=image;i.viewType=VK_IMAGE_VIEW_TYPE_2D;i.format=f;
        i.subresourceRange={aspect,0,1,0,1};VkImageView result{};vkCheck(v.vkCreateImageView(v.device,&i,nullptr,&result),"eye warp view");return result;
    }
    void makeImage(VkImage& image,VkDeviceMemory& memory,uint32_t w,uint32_t h,VkFormat f,VkImageUsageFlags usage,const char* what){
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=f;i.extent={w,h,1};i.mipLevels=i.arrayLayers=1;
        i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=usage;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        vkCheck(v.vkCreateImage(v.device,&i,nullptr,&image),what);VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,image,&req);
        memory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,image,memory,0),what);
    }
    void waitDraw(){if(pending){vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"previous eye warp completion");pending=false;}}
    void destroyKept(Kept& k) noexcept {
        if(k.depthFramebuffer)v.vkDestroyFramebuffer(v.device,k.depthFramebuffer,nullptr);if(k.gameDepthView)v.vkDestroyImageView(v.device,k.gameDepthView,nullptr);
        if(k.colourView)v.vkDestroyImageView(v.device,k.colourView,nullptr);if(k.depthView)v.vkDestroyImageView(v.device,k.depthView,nullptr);
        if(k.colour)v.vkDestroyImage(v.device,k.colour,nullptr);if(k.depth)v.vkDestroyImage(v.device,k.depth,nullptr);
        if(k.colourMemory)v.vkFreeMemory(v.device,k.colourMemory,nullptr);if(k.depthMemory)v.vkFreeMemory(v.device,k.depthMemory,nullptr);
        k=Kept{};
    }
    void destroy() noexcept {
        if(pending){auto r=v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000);if(r!=VK_SUCCESS&&r!=VK_ERROR_DEVICE_LOST)return;}
        for(auto& k:kept)destroyKept(k);
        for(auto& [image,t]:targets){if(t.framebuffer)v.vkDestroyFramebuffer(v.device,t.framebuffer,nullptr);if(t.view)v.vkDestroyImageView(v.device,t.view,nullptr);}
        targets.clear();
        if(readback)v.vkDestroyBuffer(v.device,readback,nullptr);if(readbackMemory)v.vkFreeMemory(v.device,readbackMemory,nullptr);
        if(scratchFramebuffer)v.vkDestroyFramebuffer(v.device,scratchFramebuffer,nullptr);if(scratchView)v.vkDestroyImageView(v.device,scratchView,nullptr);
        if(scratch)v.vkDestroyImage(v.device,scratch,nullptr);if(scratchMemory)v.vkFreeMemory(v.device,scratchMemory,nullptr);
        for(unsigned i=0;i<2;++i){if(checkReadback[i])v.vkDestroyBuffer(v.device,checkReadback[i],nullptr);if(checkReadbackMemory[i])v.vkFreeMemory(v.device,checkReadbackMemory[i],nullptr);}
        if(fillPipeline)v.vkDestroyPipeline(v.device,fillPipeline,nullptr);if(depthBufferView)v.vkDestroyImageView(v.device,depthBufferView,nullptr);if(depthBuffer)v.vkDestroyImage(v.device,depthBuffer,nullptr);if(depthBufferMemory)v.vkFreeMemory(v.device,depthBufferMemory,nullptr);
        if(extrasMapped)v.vkUnmapMemory(v.device,extrasMemory);if(extras)v.vkDestroyBuffer(v.device,extras,nullptr);if(extrasMemory)v.vkFreeMemory(v.device,extrasMemory,nullptr);
        if(pipeline)v.vkDestroyPipeline(v.device,pipeline,nullptr);if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);if(pass)v.vkDestroyRenderPass(v.device,pass,nullptr);
        if(depthPipeline)v.vkDestroyPipeline(v.device,depthPipeline,nullptr);if(depthCopyLayout)v.vkDestroyPipelineLayout(v.device,depthCopyLayout,nullptr);if(depthPass)v.vkDestroyRenderPass(v.device,depthPass,nullptr);
        if(depthDescriptors)vkDestroyDescriptorPool(v.device,depthDescriptors,nullptr);if(depthSetLayout)vkDestroyDescriptorSetLayout(v.device,depthSetLayout,nullptr);
        if(colourSampler)vkDestroySampler(v.device,colourSampler,nullptr);if(depthSampler)vkDestroySampler(v.device,depthSampler,nullptr);
        if(descriptors)vkDestroyDescriptorPool(v.device,descriptors,nullptr);if(setLayout)vkDestroyDescriptorSetLayout(v.device,setLayout,nullptr);
        if(fence)v.vkDestroyFence(v.device,fence,nullptr);if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);
    }
    void init(){
#define LOAD(n) n=reinterpret_cast<PFN_##n>(v.vkGetDeviceProcAddr(v.device,#n));if(!n)throw std::runtime_error(#n);
        EYE_WARP_API(LOAD)
#undef LOAD
        if(!width||!height||!rgba8(format))throw std::invalid_argument("eye warp target");
        std::array<VkDescriptorSetLayoutBinding,3> bindings{{{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=3;sl.pBindings=bindings.data();
        vkCheck(vkCreateDescriptorSetLayout(v.device,&sl,nullptr,&setLayout),"eye warp descriptor layout");
        std::array<VkDescriptorPoolSize,2> ps{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2}}};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=2;dp.poolSizeCount=2;dp.pPoolSizes=ps.data();
        vkCheck(vkCreateDescriptorPool(v.device,&dp,nullptr,&descriptors),"eye warp descriptor pool");
        std::array<VkDescriptorSetLayout,2> layouts{setLayout,setLayout};
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=descriptors;da.descriptorSetCount=2;da.pSetLayouts=layouts.data();
        vkCheck(vkAllocateDescriptorSets(v.device,&da,sets.data()),"eye warp descriptors");
        {   // the extras of the hole fill, written by the host before each draw (the earlier draw is waited for)
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=256;b.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&extras),"eye warp extras");
            VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,extras,&req);extrasMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,extras,extrasMemory,0),"eye warp extras memory");vkCheck(v.vkMapMemory(v.device,extrasMemory,0,256,0,&extrasMapped),"eye warp extras map");std::memset(extrasMapped,0,256);
            VkDescriptorBufferInfo info{extras,0,256};std::array<VkWriteDescriptorSet,2> writes{};
            for(uint32_t i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=sets[i];writes[i].dstBinding=2;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[i].pBufferInfo=&info;}
            vkUpdateDescriptorSets(v.device,2,writes.data(),0,nullptr);
        }
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCheck(vkCreateSampler(v.device,&si,nullptr,&colourSampler),"eye warp colour sampler");
        si.magFilter=si.minFilter=VK_FILTER_NEAREST;
        vkCheck(vkCreateSampler(v.device,&si,nullptr,&depthSampler),"eye warp depth sampler");
        VkAttachmentDescription a{};a.format=format;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentDescription d{};d.format=VK_FORMAT_D32_SFLOAT;d.samples=VK_SAMPLE_COUNT_1_BIT;d.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;d.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        d.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;d.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;d.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;d.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},dr{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;sub.pDepthStencilAttachment=&dr;
        VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        std::array<VkAttachmentDescription,2> attachments{a,d};
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=2;rp.pAttachments=attachments.data();rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;
        vkCheck(v.vkCreateRenderPass(v.device,&rp,nullptr,&pass),"eye warp render pass");
        makeImage(depthBuffer,depthBufferMemory,width,height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,"eye warp depth buffer");depthBufferView=view(depthBuffer,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT);
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,112};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};std::array<VkDescriptorSetLayout,2> setLayouts{setLayout,setLayout};pl.setLayoutCount=2;pl.pSetLayouts=setLayouts.data();pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;
        vkCheck(v.vkCreatePipelineLayout(v.device,&pl,nullptr,&layout),"eye warp pipeline layout");
        std::array<VkShaderModule,2> modules{};
        try{
            for(unsigned i=0;i<2;++i){VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.pCode=i?eyeWarpFragment:eyeWarpVertex;sm.codeSize=i?sizeof(eyeWarpFragment):sizeof(eyeWarpVertex);vkCheck(v.vkCreateShaderModule(v.device,&sm,nullptr,&modules[i]),"eye warp shader");}
            std::array<VkPipelineShaderStageCreateInfo,2> stages{};
            for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[i];stages[i].pName="main";}
            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=15;
            VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
            VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=dynamic;
            VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;
            VkPipelineDepthStencilStateCreateInfo dss{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};dss.depthTestEnable=VK_TRUE;dss.depthWriteEnable=VK_TRUE;dss.depthCompareOp=VK_COMPARE_OP_ALWAYS;   // the grid: depth 0 where it draws
            gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;gp.pDynamicState=&ds;gp.pDepthStencilState=&dss;gp.layout=layout;gp.renderPass=pass;
            vkCheck(v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline),"eye warp pipeline");
            dss.depthWriteEnable=VK_FALSE;dss.depthCompareOp=VK_COMPARE_OP_LESS;   // the hole fill at depth 0.5: only where the grid drew nothing
            vkCheck(v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&gp,nullptr,&fillPipeline),"eye warp fill pipeline");
        }catch(...){for(auto m:modules)if(m)v.vkDestroyShaderModule(v.device,m,nullptr);throw;}
        for(auto m:modules)v.vkDestroyShaderModule(v.device,m,nullptr);
        // The depth copy pass: R32 colour attachment, left in SHADER_READ_ONLY for the warp.
        {
            VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
            VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};dsl.bindingCount=1;dsl.pBindings=&binding;
            vkCheck(vkCreateDescriptorSetLayout(v.device,&dsl,nullptr,&depthSetLayout),"eye depth copy descriptor layout");
            VkDescriptorPoolSize dps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,depthSetCapacity};VkDescriptorPoolCreateInfo ddp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};ddp.maxSets=depthSetCapacity;ddp.poolSizeCount=1;ddp.pPoolSizes=&dps;
            vkCheck(vkCreateDescriptorPool(v.device,&ddp,nullptr,&depthDescriptors),"eye depth copy descriptor pool");
            VkAttachmentDescription da{};da.format=VK_FORMAT_R32_SFLOAT;da.samples=VK_SAMPLE_COUNT_1_BIT;da.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;da.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
            da.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;da.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;da.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;da.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAttachmentReference dar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription dsub{};dsub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;dsub.colorAttachmentCount=1;dsub.pColorAttachments=&dar;
            std::array<VkSubpassDependency,2> deps{};
            deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;deps[0].srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;deps[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT;deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;deps[1].dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;deps[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            VkRenderPassCreateInfo drp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};drp.attachmentCount=1;drp.pAttachments=&da;drp.subpassCount=1;drp.pSubpasses=&dsub;drp.dependencyCount=2;drp.pDependencies=deps.data();
            vkCheck(v.vkCreateRenderPass(v.device,&drp,nullptr,&depthPass),"eye depth copy render pass");
            VkPipelineLayoutCreateInfo dpl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};dpl.setLayoutCount=1;dpl.pSetLayouts=&depthSetLayout;
            vkCheck(v.vkCreatePipelineLayout(v.device,&dpl,nullptr,&depthCopyLayout),"eye depth copy pipeline layout");
            std::array<VkShaderModule,2> modules{};
            try{
                for(unsigned i=0;i<2;++i){VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.pCode=i?eyeDepthCopyFragment:uiTransferVertex;sm.codeSize=i?sizeof(eyeDepthCopyFragment):sizeof(uiTransferVertex);vkCheck(v.vkCreateShaderModule(v.device,&sm,nullptr,&modules[i]),"eye depth copy shader");}
                std::array<VkPipelineShaderStageCreateInfo,2> stages{};
                for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[i];stages[i].pName="main";}
                VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
                VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
                VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
                VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
                VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=1;
                VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
                VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=dynamic;
                VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;
                gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;gp.pDynamicState=&ds;gp.layout=depthCopyLayout;gp.renderPass=depthPass;
                vkCheck(v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&gp,nullptr,&depthPipeline),"eye depth copy pipeline");
            }catch(...){for(auto m:modules)if(m)v.vkDestroyShaderModule(v.device,m,nullptr);throw;}
            for(auto m:modules)v.vkDestroyShaderModule(v.device,m,nullptr);
        }
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=v.family;
        vkCheck(v.vkCreateCommandPool(v.device,&cp,nullptr,&pool),"eye warp command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(v.device,&ca,&cmd),"eye warp command buffer");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(v.device,&fi,nullptr,&fence),"eye warp fence");
    }
    const Target& target(VkImage image){
        auto found=targets.find(image);if(found!=targets.end())return found->second;
        Target t;t.view=view(image,format,VK_IMAGE_ASPECT_COLOR_BIT);
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;std::array<VkImageView,2> attachments{t.view,depthBufferView};fi.attachmentCount=2;fi.pAttachments=attachments.data();fi.width=width;fi.height=height;fi.layers=1;
        try{vkCheck(v.vkCreateFramebuffer(v.device,&fi,nullptr,&t.framebuffer),"eye warp framebuffer");}catch(...){v.vkDestroyImageView(v.device,t.view,nullptr);throw;}
        return targets.emplace(image,t).first->second;
    }
  public:
    // cells: the grid's cells along each side; near: the game's near plane (its depth = near / distance); far: the
    // distance shown where the depth is 0 (the sky).
    unsigned cells=96;float nearPlane=.01f,farDistance=1000.f;   // (near / far are macros of windows.h)
    uint64_t keeps=0,draws=0,drawsFromOther=0,fills=0;bool lastFromOther=false,lastFromSelf=false;uint64_t lastOtherFrame=0;
    // Drawn into the warp's command buffer after its picture, with the runtime image in TRANSFER_DST_OPTIMAL (as the
    // copy's afterCopy): the edge tint of the eye. True: it left the image in COLOR_ATTACHMENT_OPTIMAL.
    std::function<bool(VkCommandBuffer,uint32_t,const TransferImage&)> afterWarp;
    EyeWarp(Vulkan& device,VkFormat runtimeFormat,uint32_t w,uint32_t h):v(device),format(runtimeFormat),width(w),height(h){try{init();}catch(...){destroy();throw;}}
    EyeWarp(const EyeWarp&)=delete;EyeWarp& operator=(const EyeWarp&)=delete;
    ~EyeWarp(){destroy();}
    bool gpuPending()const{return pending;}
    // The eye's last drawn picture is kept, with its depth, or (fromOther) the other eye's: it can be warped.
    bool canWarp(uint32_t eye,bool fromOther=false)const{return eye<2&&(kept[eye].valid||(fromOther&&kept[1-eye].valid));}
    bool keptThisFrame(uint32_t eye,uint64_t frame)const{return eye<2&&kept[eye].valid&&kept[eye].frame==frame;}
    uint64_t keptFrame(uint32_t eye)const{return eye<2?kept[eye].frame:0;}
    const XrView& keptView(uint32_t eye)const{return kept[eye].view;}
    void forget(){for(auto& k:kept){k.valid=false;}}
    // Records, into the command buffer of the eye's copy (stereo_transfer.hpp, its source in TRANSFER_SRC_OPTIMAL,
    // the copy before it waited for its earlier commands), the copy of this eye's colour and a pass that samples the
    // game's depth into the kept R32 image (depthLayout: the layout the game left the depth in; it is put back; the
    // depth needs SAMPLED usage). The view is the one the eye was drawn with in this frame.
    void keep(VkCommandBuffer c,uint32_t eye,const TransferImage& colour,VkImage depth,VkImageLayout depthLayout,uint32_t depthWidth,uint32_t depthHeight,VkFormat depthFormat,const XrView& drawnView,uint64_t frame){
        if(eye>=2||!colour.image||!depth||!depthWidth||!depthHeight)throw std::invalid_argument("eye warp keep");
        if(colour.width!=width||colour.height!=height||!rgba8(colour.format))throw std::invalid_argument("eye warp colour source");   // the caller has it in TRANSFER_SRC_OPTIMAL
        auto& k=kept[eye];const VkImageAspectFlags aspect=depthFormatOf(depthFormat)?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
        if(k.depth&&(k.depthWidth!=depthWidth||k.depthHeight!=depthHeight||k.depthFormat!=depthFormat)){waitDraw();destroyKept(k);}
        if(!k.colour){
            makeImage(k.colour,k.colourMemory,width,height,format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,"eye warp colour image");
            k.colourView=view(k.colour,format,VK_IMAGE_ASPECT_COLOR_BIT);k.colourWritten=false;k.bound=false;
        }
        if(!k.depth){
            makeImage(k.depth,k.depthMemory,depthWidth,depthHeight,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,"eye warp depth image");
            k.depthView=view(k.depth,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT);k.depthWidth=depthWidth;k.depthHeight=depthHeight;k.depthFormat=depthFormat;k.depthWritten=false;k.bound=false;
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=depthPass;fi.attachmentCount=1;fi.pAttachments=&k.depthView;fi.width=depthWidth;fi.height=depthHeight;fi.layers=1;
            vkCheck(v.vkCreateFramebuffer(v.device,&fi,nullptr,&k.depthFramebuffer),"eye depth copy framebuffer");
        }
        if(k.gameDepth!=depth){
            // The game's depth image changed: its view and descriptor (the earlier copy's commands are complete).
            waitDraw();
            if(k.gameDepthView){v.vkDestroyImageView(v.device,k.gameDepthView,nullptr);k.gameDepthView=VK_NULL_HANDLE;}
            if(depthSetsUsed>=depthSetCapacity)throw std::runtime_error("eye warp: the game's depth image changed too often");
            k.gameDepthView=view(depth,depthFormat,aspect);
            VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=depthDescriptors;da.descriptorSetCount=1;da.pSetLayouts=&depthSetLayout;
            vkCheck(vkAllocateDescriptorSets(v.device,&da,&k.gameDepthSet),"eye depth copy descriptor");++depthSetsUsed;
            VkDescriptorImageInfo info{depthSampler,k.gameDepthView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet=k.gameDepthSet;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&info;
            vkUpdateDescriptorSets(v.device,1,&write,0,nullptr);k.gameDepth=depth;
        }
        if(!k.bound){
            waitDraw();   // the descriptor set of a draw still in flight is not written
            std::array<VkDescriptorImageInfo,2> infos{{{colourSampler,k.colourView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{depthSampler,k.depthView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}};
            std::array<VkWriteDescriptorSet,2> writes{};
            for(uint32_t i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=sets[eye];writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=&infos[i];}
            vkUpdateDescriptorSets(v.device,2,writes.data(),0,nullptr);k.bound=true;
        }
        // The colour: the eye's picture as the headset gets it.
        barrier(c,k.colour,VK_IMAGE_ASPECT_COLOR_BIT,k.colourWritten?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            k.colourWritten?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageCopy region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.extent={width,height,1};
        v.vkCmdCopyImage(c,colour.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,k.colour,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
        barrier(c,k.colour,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        k.colourWritten=true;
        // The depth: the game's, sampled by the copy pass into the kept R32 image, and left in its layout.
        if(depthLayout!=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            barrier(c,depth,aspect,depthLayout,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=depthPass;r.framebuffer=k.depthFramebuffer;r.renderArea={{0,0},{depthWidth,depthHeight}};
        v.vkCmdBeginRenderPass(c,&r,VK_SUBPASS_CONTENTS_INLINE);v.vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,depthPipeline);
        vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,depthCopyLayout,0,1,&k.gameDepthSet,0,nullptr);
        VkViewport viewport{0,0,float(depthWidth),float(depthHeight),0,1};VkRect2D scissor{{0,0},{depthWidth,depthHeight}};
        v.vkCmdSetViewport(c,0,1,&viewport);v.vkCmdSetScissor(c,0,1,&scissor);v.vkCmdDraw(c,3,1,0,0);v.vkCmdEndRenderPass(c);
        if(depthLayout!=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            barrier(c,depth,aspect,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,depthLayout,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        k.depthWritten=true;k.view=drawnView;k.frame=frame;k.valid=true;++keeps;
    }
    // old eye's view space -> new eye's view space, as the shader's column-major mat4
    static std::array<float,16> oldToNew(const XrPosef& oldPose,const XrPosef& newPose){
        const Quat qo{oldPose.orientation.x,oldPose.orientation.y,oldPose.orientation.z,oldPose.orientation.w},qn{newPose.orientation.x,newPose.orientation.y,newPose.orientation.z,newPose.orientation.w};
        const Quat r=normalized(multiply(inverse(qn),qo));
        const Vec3 t=rotate(inverse(qn),Vec3{oldPose.position.x-newPose.position.x,oldPose.position.y-newPose.position.y,oldPose.position.z-newPose.position.z});
        const Vec3 c0=rotate(r,{1,0,0}),c1=rotate(r,{0,1,0}),c2=rotate(r,{0,0,1});
        return {c0.x,c0.y,c0.z,0,c1.x,c1.y,c1.z,0,c2.x,c2.y,c2.z,0,t.x,t.y,t.z,1};
    }
    // The push constants of one pass: the kept picture k, warped from its view to `now`; mode 0 stretches over depth
    // steps, mode 1 leaves holes there (eye_warp.vert).
    std::array<float,28> constants(const Kept& k,const XrView& now,float mode)const{
        const auto m=oldToNew(k.view.pose,now.pose);
        std::array<float,28> push{};for(size_t i=0;i<16;++i)push[i]=m[i];
        push[16]=std::tan(k.view.fov.angleLeft);push[17]=std::tan(k.view.fov.angleRight);push[18]=std::tan(k.view.fov.angleDown);push[19]=std::tan(k.view.fov.angleUp);
        push[20]=std::tan(now.fov.angleLeft);push[21]=std::tan(now.fov.angleRight);push[22]=std::tan(now.fov.angleDown);push[23]=std::tan(now.fov.angleUp);
        push[24]=float(cells);push[25]=nearPlane;push[26]=farDistance;push[27]=mode;
        for(const float f:push)if(!std::isfinite(f))throw std::invalid_argument("eye warp constants");
        return push;
    }
    // set 0: the kept picture drawn from (source), set 1: the other kept picture (the hole fill reads its depth). vertices: 0 = the grid.
    void drawPass(VkCommandBuffer c,uint32_t source,const std::array<float,28>& push,uint32_t vertices=0){
        std::array<VkDescriptorSet,2> bound{sets[source],sets[1-source]};
        vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,2,bound.data(),0,nullptr);
        v.vkCmdPushConstants(c,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,112,push.data());
        v.vkCmdDraw(c,vertices?vertices:cells*cells*6,1,0,0);
    }
    // Draws the picture of the eye left out, warped to `now`, into the runtime image (COLOR_ATTACHMENT_OPTIMAL
    // afterwards): this eye's kept picture (second stage) and, with fromOther, the other eye's kept picture over it
    // with holes at the depth steps (third stage; the other eye's picture of this frame when it was drawn in it,
    // else its last). Its own submission on v.queue, after the copies of this frame. capture: the result as RGBA8 bytes.
    std::vector<uint8_t> draw(uint32_t eye,const TransferImage& runtime,const XrView& now,uint64_t frame,bool fromOther,bool capture=false,bool wait=false){
        if(!canWarp(eye,fromOther))throw std::logic_error("eye warp without a kept picture");
        waitDraw();
        const uint32_t other=1-eye;const bool self=kept[eye].valid,fromOtherNow=fromOther&&kept[other].valid;
        const auto& t=target(runtime.image);
        const auto selfPush=self?constants(kept[eye],now,0):std::array<float,28>{};
        const auto otherPush=fromOtherNow?constants(kept[other],now,1):std::array<float,28>{};
        lastFromSelf=self;lastFromOther=fromOtherNow;lastOtherFrame=fromOtherNow?kept[other].frame:0;(void)frame;
        const auto bytes=VkDeviceSize(width)*height*4;
        if(capture&&!readback){
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&readback),"eye warp readback");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,readback,&req);
            readbackMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(v.device,readback,readbackMemory,0),"eye warp readback memory");
        }
        vkCheck(v.vkResetCommandBuffer(cmd,0),"eye warp reset");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(v.vkBeginCommandBuffer(cmd,&begin),"eye warp begin");
        std::array<VkClearValue,2> clear{};clear[0].color={{0,0,0,1}};clear[1].depthStencil={1.f,0};
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=pass;r.framebuffer=t.framebuffer;r.renderArea={{0,0},{width,height}};r.clearValueCount=2;r.pClearValues=clear.data();
        v.vkCmdBeginRenderPass(cmd,&r,VK_SUBPASS_CONTENTS_INLINE);v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};
        v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);
        if(fromOtherNow){
            drawPass(cmd,other,otherPush);++drawsFromOther;
            if(self){   // the holes: this eye's last picture, each hole's ray checked against the other eye (mode 2, shaders/eye_warp.frag)
                auto* e=static_cast<float*>(extrasMapped);for(size_t i=0;i<20;++i)e[i]=otherPush[i];
                v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,fillPipeline);drawPass(cmd,eye,constants(kept[eye],now,2),3);++fills;
            }
        }else if(self)drawPass(cmd,eye,selfPush);
        v.vkCmdEndRenderPass(cmd);
        if(afterWarp){
            barrier(cmd,runtime.image,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            TransferImage dst=runtime;dst.layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            if(!afterWarp(cmd,eye,dst))
                barrier(cmd,runtime.image,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }
        if(capture){
            barrier(cmd,runtime.image,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};
            v.vkCmdCopyImageToBuffer(cmd,runtime.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&region);
            barrier(cmd,runtime.image,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
            wait=true;
        }
        vkCheck(v.vkEndCommandBuffer(cmd),"eye warp end");vkCheck(v.vkResetFences(v.device,1,&fence),"eye warp fence reset");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
        vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"eye warp submit");pending=true;++draws;
        if(wait){vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"eye warp GPU completion");pending=false;}
        std::vector<uint8_t> result;
        if(capture){result.resize(size_t(bytes));void* mapped{};vkCheck(v.vkMapMemory(v.device,readbackMemory,0,bytes,0,&mapped),"eye warp readback map");std::memcpy(result.data(),mapped,result.size());v.vkUnmapMemory(v.device,readbackMemory);}
        return result;
    }
    // The self-check (EVR_EYE_WARP_CHECK): on a frame that drew both eyes, the eye is made from the other eye's picture
    // of this frame (third stage, holes left black) into a scratch picture and compared with the eye's own drawn
    // picture, every 4th pixel each way: the mean absolute difference of the covered pixels (0..255, the three channels)
    // and the share of holes. Waits for the GPU.
    struct Check {double meanDifference=0,holes=0;uint64_t compared=0;};
    // madeOut / drawnOut: the two pictures compared (RGBA8), for a look at where they differ (EVR_EYE_WARP_CHECK_CAPTURE).
    Check check(uint32_t eye,uint64_t frame,std::vector<uint8_t>* madeOut=nullptr,std::vector<uint8_t>* drawnOut=nullptr){
        if(eye>=2||!keptThisFrame(eye,frame)||!keptThisFrame(1-eye,frame))throw std::logic_error("eye warp check needs both eyes of this frame");
        waitDraw();
        const auto bytes=VkDeviceSize(width)*height*4;
        if(!scratch){
            makeImage(scratch,scratchMemory,width,height,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,"eye warp check image");
            scratchView=view(scratch,format,VK_IMAGE_ASPECT_COLOR_BIT);
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;std::array<VkImageView,2> attachments{scratchView,depthBufferView};fi.attachmentCount=2;fi.pAttachments=attachments.data();fi.width=width;fi.height=height;fi.layers=1;
            vkCheck(v.vkCreateFramebuffer(v.device,&fi,nullptr,&scratchFramebuffer),"eye warp check framebuffer");
            for(unsigned i=0;i<2;++i){VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&checkReadback[i]),"eye warp check readback");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,checkReadback[i],&req);
                checkReadbackMemory[i]=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(v.device,checkReadback[i],checkReadbackMemory[i],0),"eye warp check readback memory");}
        }
        const uint32_t other=1-eye;const auto push=constants(kept[other],kept[eye].view,1);
        vkCheck(v.vkResetCommandBuffer(cmd,0),"eye warp check reset");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(v.vkBeginCommandBuffer(cmd,&begin),"eye warp check begin");
        std::array<VkClearValue,2> clear{};clear[0].color={{0,0,0,0}};clear[1].depthStencil={1.f,0};
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=pass;r.framebuffer=scratchFramebuffer;r.renderArea={{0,0},{width,height}};r.clearValueCount=2;r.pClearValues=clear.data();
        v.vkCmdBeginRenderPass(cmd,&r,VK_SUBPASS_CONTENTS_INLINE);v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};
        v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);drawPass(cmd,other,push);v.vkCmdEndRenderPass(cmd);
        barrier(cmd,scratch,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        barrier(cmd,kept[eye].colour,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};
        v.vkCmdCopyImageToBuffer(cmd,scratch,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,checkReadback[0],1,&region);
        v.vkCmdCopyImageToBuffer(cmd,kept[eye].colour,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,checkReadback[1],1,&region);
        barrier(cmd,kept[eye].colour,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        vkCheck(v.vkEndCommandBuffer(cmd),"eye warp check end");vkCheck(v.vkResetFences(v.device,1,&fence),"eye warp check fence reset");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
        vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"eye warp check submit");
        vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"eye warp check completion");pending=false;
        Check result;void* madeRaw{};void* drawnRaw{};
        vkCheck(v.vkMapMemory(v.device,checkReadbackMemory[0],0,bytes,0,&madeRaw),"eye warp check map");
        vkCheck(v.vkMapMemory(v.device,checkReadbackMemory[1],0,bytes,0,&drawnRaw),"eye warp check map");
        const auto* made=static_cast<const uint8_t*>(madeRaw);const auto* drawn=static_cast<const uint8_t*>(drawnRaw);
        if(madeOut)madeOut->assign(made,made+size_t(bytes));if(drawnOut)drawnOut->assign(drawn,drawn+size_t(bytes));
        double sum=0;uint64_t covered=0,holes=0;
        for(uint32_t y=0;y<height;y+=4)for(uint32_t x=0;x<width;x+=4){const size_t i=(size_t(y)*width+x)*4;
            if(made[i+3]==0){++holes;continue;}
            sum+=std::abs(int(made[i])-int(drawn[i]))+std::abs(int(made[i+1])-int(drawn[i+1]))+std::abs(int(made[i+2])-int(drawn[i+2]));++covered;}
        v.vkUnmapMemory(v.device,checkReadbackMemory[0]);v.vkUnmapMemory(v.device,checkReadbackMemory[1]);
        result.compared=covered;result.meanDifference=covered?sum/(3.*double(covered)):0;result.holes=(covered+holes)?double(holes)/double(covered+holes):0;
        return result;
    }
    // RGBA8 bytes of the eye's size as a binary PPM.
    void writePpm(const std::string& path,const std::vector<uint8_t>& rgba)const{
        if(rgba.size()!=size_t(width)*height*4)throw std::invalid_argument("eye warp picture size");
        std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("eye warp capture path unavailable");
        out<<"P6\n"<<width<<' '<<height<<"\n255\n";std::vector<char> row(size_t(width)*3);
        for(uint32_t y=0;y<height;++y){const auto* p=rgba.data()+size_t(y)*width*4;for(uint32_t x=0;x<width;++x){row[x*3]=char(p[x*4]);row[x*3+1]=char(p[x*4+1]);row[x*3+2]=char(p[x*4+2]);}
            out.write(row.data(),std::streamsize(row.size()));}
    }
#undef EYE_WARP_API
};
}
