#pragma once
#include <evr/openxr_client.hpp>
#include <evr/stereo_transfer.hpp>
#include <evr/centre_crop.hpp>
#include <ui_transfer_vertex.h>
#include <ui_transfer_fragment.h>
namespace evr {
// HUD auto-hide (hud_fade.hpp): the two `keep` rectangles (x0, y0, x1, y1 in pixels) stay fully shown, the rest is
// scaled by `outside`.
// cursor: the menu pointer's dot (hooks/menu_pointer.hpp) - x, y in pixels, radius in pixels (0 = none) and tone
// (1 light, 2 the green of a held button). It is drawn over the image after the fade.
// hole: x, y, radius in pixels (0 = none) of a disc that is left out of the image - the game's reticle while the aim
// mark replaces it (hooks/aim_mark.hpp).
// overlay: x0, y0, x1, y1 in pixels of a picture of the mod's own laid over the image after the fade (setOverlay; the VR
// bug report's button and its notes, hooks/bug_report.hpp); x1 <= x0 = none.
struct UiFade {std::array<float,4> keep{},keepWide{};float outside=1;std::array<float,4> cursor{},hole{},overlay{};};
// Single UI surface only. World images always use the independent native-eye path.
// Native queue gates + source identity/version/lifetime must be proved by caller.
class UiTransfer {
    Vulkan& v;
    EyeTarget target;
    VkFormat format;
    VkRenderPass pass{};VkPipelineLayout layout{};VkPipeline pipeline{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool descriptors{};VkDescriptorSet set{};VkSampler sampler{};
    // The overlay (binding 1): an RGBA8 picture in display values with straight alpha, sampled linearly over its rectangle.
    VkImage overlayImage{};VkDeviceMemory overlayMemory{};VkImageView overlayView{};VkSampler overlaySampler{};uint32_t overlayWidth=0,overlayHeight=0;
    VkBuffer overlayStaging{};VkDeviceMemory overlayStagingMemory{};VkDeviceSize overlayStagingBytes=0;bool overlayWritten=false,overlayUpload=false;std::vector<uint8_t> overlayBytes;
    VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};bool pending=false;
    std::vector<VkImageView> views;std::vector<VkFramebuffer> framebuffers;
    VkImageView sourceView{};VkBuffer readback{};VkDeviceMemory readbackMemory{};
    // HUD auto-hide: a small copy of the source (halved by linear blits down to cells of about 16 pixels), read back
    // one draw later, for the look at what changed in the UI.
    VkImage probeImage{};VkDeviceMemory probeMemory{};VkBuffer probeBuffer{};VkDeviceMemory probeBufferMemory{};
    uint32_t probeLevels=0,probeWidth=0,probeHeight=0;bool probeInFlight=false;std::vector<uint8_t> probeCells;uint64_t probeCount=0;
    // The desktop's picture (2026-10-13, user: "put the UI on the desktop side, for streaming"): the middle of the
    // right eye's copy (stereo_transfer.hpp's mirror image) with this UI laid over it the way the game lays it over
    // its own picture - display values, premultiplied, no conversion. The image is this transfer's own, of the UI's
    // size, written in the same submission as the headset's UI and left in TRANSFER_SRC_OPTIMAL for the desktop
    // mirror's blit (layer/desktop_mirror.hpp). Nothing of the headset's image changes with it.
    VkImage deskImage{};VkDeviceMemory deskMemory{};VkImageView deskView{};VkFramebuffer deskFramebuffer{};VkRenderPass deskPass{};VkPipeline deskPipeline{};
    VkBuffer deskReadback{};VkDeviceMemory deskReadbackMemory{};bool deskWritten=false,deskFresh=false,deskFailed=false;std::vector<uint8_t> deskBytes;
#define UI_TRANSFER_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) \
 X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateSampler) X(vkDestroySampler) X(vkCmdBindDescriptorSets) \
 X(vkCmdBlitImage) X(vkCmdCopyBufferToImage)
#define DECL(n) PFN_##n n=nullptr;
    UI_TRANSFER_API(DECL)
#undef DECL
    void barrier(VkImage image,VkImageLayout oldLayout,VkImageLayout next,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags from,VkPipelineStageFlags to){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=oldLayout;b.newLayout=next;b.srcAccessMask=src;b.dstAccessMask=dst;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        v.vkCmdPipelineBarrier(cmd,from,to,0,0,nullptr,0,nullptr,1,&b);
    }
    VkImageView view(VkImage image,VkFormat f){
        VkImageViewCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};i.image=image;i.viewType=VK_IMAGE_VIEW_TYPE_2D;i.format=f;
        i.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView result{};vkCheck(v.vkCreateImageView(v.device,&i,nullptr,&result),"UI transfer view");return result;
    }
    void levelBarrier(VkImage image,uint32_t level,VkImageLayout oldLayout,VkImageLayout next,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags from,VkPipelineStageFlags to){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=oldLayout;b.newLayout=next;b.srcAccessMask=src;b.dstAccessMask=dst;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,level,1,0,1};
        v.vkCmdPipelineBarrier(cmd,from,to,0,0,nullptr,0,nullptr,1,&b);
    }
    void ensureProbe(){
        if(probeImage)return;
        probeWidth=std::max(1u,target.width/2);probeHeight=std::max(1u,target.height/2);probeLevels=1;
        while((probeWidth>>(probeLevels-1))>192&&(probeHeight>>probeLevels)>0&&probeLevels<8)++probeLevels;
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=VK_FORMAT_R8G8B8A8_UNORM;i.extent={probeWidth,probeHeight,1};i.mipLevels=probeLevels;i.arrayLayers=1;
        i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        vkCheck(v.vkCreateImage(v.device,&i,nullptr,&probeImage),"UI probe image");VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,probeImage,&req);
        probeMemory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,probeImage,probeMemory,0),"UI probe image memory");
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=VkDeviceSize(probeCellsWidth())*probeCellsHeight()*4;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&probeBuffer),"UI probe buffer");VkMemoryRequirements breq{};v.vkGetBufferMemoryRequirements(v.device,probeBuffer,&breq);
        probeBufferMemory=v.allocate(breq,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkCheck(v.vkBindBufferMemory(v.device,probeBuffer,probeBufferMemory,0),"UI probe buffer memory");
    }
    // The copy recorded by the previous draw, once its commands have completed.
    void collectProbe(){
        if(!probeInFlight||pending)return;
        const size_t bytes=size_t(probeCellsWidth())*probeCellsHeight()*4;probeCells.resize(bytes);void* mapped{};
        vkCheck(v.vkMapMemory(v.device,probeBufferMemory,0,bytes,0,&mapped),"UI probe map");std::memcpy(probeCells.data(),mapped,bytes);v.vkUnmapMemory(v.device,probeBufferMemory);
        probeInFlight=false;++probeCount;
    }
    void destroy() noexcept {
        if(pending){auto r=v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000);if(r!=VK_SUCCESS && r!=VK_ERROR_DEVICE_LOST)return;}
        if(deskReadback)v.vkDestroyBuffer(v.device,deskReadback,nullptr);
        if(deskReadbackMemory)v.vkFreeMemory(v.device,deskReadbackMemory,nullptr);
        if(deskPipeline)v.vkDestroyPipeline(v.device,deskPipeline,nullptr);
        if(deskFramebuffer)v.vkDestroyFramebuffer(v.device,deskFramebuffer,nullptr);
        if(deskPass)v.vkDestroyRenderPass(v.device,deskPass,nullptr);
        if(deskView)v.vkDestroyImageView(v.device,deskView,nullptr);
        if(deskImage)v.vkDestroyImage(v.device,deskImage,nullptr);
        if(deskMemory)v.vkFreeMemory(v.device,deskMemory,nullptr);
        if(probeBuffer)v.vkDestroyBuffer(v.device,probeBuffer,nullptr);
        if(probeBufferMemory)v.vkFreeMemory(v.device,probeBufferMemory,nullptr);
        if(probeImage)v.vkDestroyImage(v.device,probeImage,nullptr);
        if(probeMemory)v.vkFreeMemory(v.device,probeMemory,nullptr);
        if(overlayView)v.vkDestroyImageView(v.device,overlayView,nullptr);if(overlayImage)v.vkDestroyImage(v.device,overlayImage,nullptr);if(overlayMemory)v.vkFreeMemory(v.device,overlayMemory,nullptr);
        if(overlayStaging)v.vkDestroyBuffer(v.device,overlayStaging,nullptr);if(overlayStagingMemory)v.vkFreeMemory(v.device,overlayStagingMemory,nullptr);if(overlaySampler)vkDestroySampler(v.device,overlaySampler,nullptr);
        if(sourceView)v.vkDestroyImageView(v.device,sourceView,nullptr);
        if(readback)v.vkDestroyBuffer(v.device,readback,nullptr);
        if(readbackMemory)v.vkFreeMemory(v.device,readbackMemory,nullptr);
        for(auto f:framebuffers)if(f)v.vkDestroyFramebuffer(v.device,f,nullptr);
        for(auto i:views)if(i)v.vkDestroyImageView(v.device,i,nullptr);
        if(pipeline)v.vkDestroyPipeline(v.device,pipeline,nullptr);
        if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);
        if(pass)v.vkDestroyRenderPass(v.device,pass,nullptr);
        if(sampler)vkDestroySampler(v.device,sampler,nullptr);
        if(descriptors)vkDestroyDescriptorPool(v.device,descriptors,nullptr);
        if(setLayout)vkDestroyDescriptorSetLayout(v.device,setLayout,nullptr);
        if(fence)v.vkDestroyFence(v.device,fence,nullptr);
        if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);
    }
    // over: the picture is laid over what the target holds (premultiplied), instead of replacing it.
    VkPipeline makePipeline(VkRenderPass renderPass,bool over){
        std::array<VkShaderModule,2> modules{};VkPipeline result{};
        try{
            for(unsigned i=0;i<2;++i){VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.pCode=i?uiTransferFragment:uiTransferVertex;sm.codeSize=i?sizeof(uiTransferFragment):sizeof(uiTransferVertex);vkCheck(v.vkCreateShaderModule(v.device,&sm,nullptr,&modules[i]),"UI transfer shader");}
            std::array<VkPipelineShaderStageCreateInfo,2> stages{};
            for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[i];stages[i].pName="main";}
            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=15;
            if(over){ba.blendEnable=VK_TRUE;ba.srcColorBlendFactor=ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.dstColorBlendFactor=ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=ba.alphaBlendOp=VK_BLEND_OP_ADD;}
            VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&ba;
            VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=dynamic;
            VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=stages.data();gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&bs;gp.pDynamicState=&ds;gp.layout=layout;gp.renderPass=renderPass;gp.basePipelineIndex=-1;
            vkCheck(v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&gp,nullptr,&result),"UI transfer pipeline");
        }catch(...){for(auto m:modules)if(m)v.vkDestroyShaderModule(v.device,m,nullptr);throw;}
        for(auto m:modules)v.vkDestroyShaderModule(v.device,m,nullptr);return result;
    }
    void destroyOverlayImage() noexcept {
        if(pending){v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000);pending=false;}
        if(overlayView)v.vkDestroyImageView(v.device,overlayView,nullptr);if(overlayImage)v.vkDestroyImage(v.device,overlayImage,nullptr);if(overlayMemory)v.vkFreeMemory(v.device,overlayMemory,nullptr);
        overlayView=VK_NULL_HANDLE;overlayImage=VK_NULL_HANDLE;overlayMemory=VK_NULL_HANDLE;overlayWritten=false;
    }
    // The overlay's image (of its picture's size) and the staging buffer that holds the bytes of a pending upload.
    void ensureOverlay(){
        if(!overlayImage){
            VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=VK_FORMAT_R8G8B8A8_UNORM;i.extent={overlayWidth,overlayHeight,1};i.mipLevels=i.arrayLayers=1;
            i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            vkCheck(v.vkCreateImage(v.device,&i,nullptr,&overlayImage),"UI overlay image");VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,overlayImage,&req);
            overlayMemory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,overlayImage,overlayMemory,0),"UI overlay image memory");
            overlayView=view(overlayImage,VK_FORMAT_R8G8B8A8_UNORM);overlayWritten=false;overlayUpload=true;
        }
        if(!overlayUpload)return;
        const VkDeviceSize bytes=VkDeviceSize(overlayBytes.size());
        if(bytes>overlayStagingBytes){
            if(overlayStaging)v.vkDestroyBuffer(v.device,overlayStaging,nullptr);if(overlayStagingMemory)v.vkFreeMemory(v.device,overlayStagingMemory,nullptr);
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&overlayStaging),"UI overlay staging");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,overlayStaging,&req);
            overlayStagingMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,overlayStaging,overlayStagingMemory,0),"UI overlay staging memory");overlayStagingBytes=bytes;
        }
        void* mapped{};vkCheck(v.vkMapMemory(v.device,overlayStagingMemory,0,bytes,0,&mapped),"UI overlay staging map");std::memcpy(mapped,overlayBytes.data(),size_t(bytes));v.vkUnmapMemory(v.device,overlayStagingMemory);
    }
    void ensureDesktop(){
        if(deskPipeline)return;
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=VK_FORMAT_R8G8B8A8_UNORM;i.extent={target.width,target.height,1};i.mipLevels=i.arrayLayers=1;
        i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        vkCheck(v.vkCreateImage(v.device,&i,nullptr,&deskImage),"desktop UI image");VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,deskImage,&req);
        deskMemory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,deskImage,deskMemory,0),"desktop UI image memory");
        deskView=view(deskImage,VK_FORMAT_R8G8B8A8_UNORM);
        VkAttachmentDescription a{};a.format=VK_FORMAT_R8G8B8A8_UNORM;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=a.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;
        VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&a;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;
        vkCheck(v.vkCreateRenderPass(v.device,&rp,nullptr,&deskPass),"desktop UI render pass");
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=deskPass;fi.attachmentCount=1;fi.pAttachments=&deskView;fi.width=target.width;fi.height=target.height;fi.layers=1;
        vkCheck(v.vkCreateFramebuffer(v.device,&fi,nullptr,&deskFramebuffer),"desktop UI framebuffer");
        deskPipeline=makePipeline(deskPass,true);
    }
    void init(){
#define LOAD(n) n=reinterpret_cast<PFN_##n>(v.vkGetDeviceProcAddr(v.device,#n));if(!n)throw std::runtime_error(#n);
        UI_TRANSFER_API(LOAD)
#undef LOAD
        if(!target.width || !target.height || target.images.empty() || !rgba8(format))throw std::invalid_argument("UI transfer target");
        std::array<VkDescriptorSetLayoutBinding,2> bindings{{{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=2;sl.pBindings=bindings.data();
        vkCheck(vkCreateDescriptorSetLayout(v.device,&sl,nullptr,&setLayout),"UI transfer descriptor layout");
        VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&ps;
        vkCheck(vkCreateDescriptorPool(v.device,&dp,nullptr,&descriptors),"UI transfer descriptor pool");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=descriptors;da.descriptorSetCount=1;da.pSetLayouts=&setLayout;
        vkCheck(vkAllocateDescriptorSets(v.device,&da,&set),"UI transfer descriptor");
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCheck(vkCreateSampler(v.device,&si,nullptr,&sampler),"UI transfer sampler");
        si.magFilter=si.minFilter=VK_FILTER_LINEAR;vkCheck(vkCreateSampler(v.device,&si,nullptr,&overlaySampler),"UI overlay sampler");
        overlayBytes.assign(4,0);overlayWidth=overlayHeight=1;overlayUpload=true;   // a transparent pixel until a picture is set
        VkAttachmentDescription a{};a.format=format;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;
        VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&a;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;
        vkCheck(v.vkCreateRenderPass(v.device,&rp,nullptr,&pass),"UI transfer render pass");
        for(const auto& image:target.images){
            views.push_back(view(image.image,format));framebuffers.push_back(VK_NULL_HANDLE);
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=1;fi.pAttachments=&views.back();fi.width=target.width;fi.height=target.height;fi.layers=1;
            vkCheck(v.vkCreateFramebuffer(v.device,&fi,nullptr,&framebuffers.back()),"UI transfer framebuffer");
        }
        VkPushConstantRange fadeRange{VK_SHADER_STAGE_FRAGMENT_BIT,0,96};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&fadeRange;
        vkCheck(v.vkCreatePipelineLayout(v.device,&pl,nullptr,&layout),"UI transfer pipeline layout");
        pipeline=makePipeline(pass,false);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=v.family;
        vkCheck(v.vkCreateCommandPool(v.device,&cp,nullptr,&pool),"UI transfer command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(v.device,&ca,&cmd),"UI transfer command buffer");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(v.device,&fi,nullptr,&fence),"UI transfer fence");
    }
  public:
    UiTransfer(Vulkan& device,const EyeTarget& images,VkFormat f):v(device),target(images),format(f){try{init();}catch(...){destroy();throw;}}
    UiTransfer(const UiTransfer&)=delete;UiTransfer& operator=(const UiTransfer&)=delete;
    ~UiTransfer(){destroy();}
    bool gpuPending()const{return pending;}
    // The overlay's picture (RGBA8, display values, straight alpha, rows from the top), uploaded with the next draw.
    void setOverlay(const std::vector<uint8_t>& rgba,uint32_t w,uint32_t h){
        if(!w||!h||rgba.size()!=size_t(w)*h*4)throw std::invalid_argument("UI overlay picture");
        overlayBytes=rgba;if(w!=overlayWidth||h!=overlayHeight){overlayWidth=w;overlayHeight=h;overlayWritten=false;destroyOverlayImage();}overlayUpload=true;
    }
    // HUD auto-hide. finishPrevious() waits for a draw still in flight and takes over its small copy of the source;
    // probe() is that copy (RGBA8, probeCellsWidth() x probeCellsHeight(), empty before the first), probes() counts them.
    void finishPrevious(){
        if(pending){vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"previous UI transfer completion");pending=false;}
        collectProbe();
    }
    const std::vector<uint8_t>& probe()const{return probeCells;}uint64_t probes()const{return probeCount;}
    uint32_t probeCellsWidth()const{return probeLevels?std::max(1u,probeWidth>>(probeLevels-1)):0;}
    uint32_t probeCellsHeight()const{return probeLevels?std::max(1u,probeHeight>>(probeLevels-1)):0;}
    bool previousPending=false; // last draw() found an in-flight draw and waited for it
    // The desktop's picture of the last draw(): empty unless that draw was given the picture behind the UI and made
    // it. The image stays this transfer's (TRANSFER_SRC_OPTIMAL between draws); desktopImage() is it whether or not
    // the last draw wrote it - for the caller that must stop reading it before this transfer goes.
    struct DesktopView {VkImage image{};uint32_t width=0,height=0;};
    DesktopView desktopView()const{return deskFresh?DesktopView{deskImage,target.width,target.height}:DesktopView{};}
    VkImage desktopImage()const{return deskImage;}
    // With capture: the desktop's picture of that draw as RGBA8 (empty without one).
    const std::vector<uint8_t>& desktopCapture()const{return deskBytes;}
    // wait=false leaves the draw in flight (queue-ordered on v.queue); the next draw() waits
    // for it before reusing the view/descriptor/command buffer. Captures always wait.
    // fade: see UiFade (the default leaves the image as the game drew it). probe: also record the small copy.
    // behind: the picture the desktop shows behind the UI (R8G8B8A8_UNORM display values, TRANSFER_SRC_OPTIMAL, this
    // device's family; its middle in the UI's shape is taken) - with it the desktop's picture is made too; a picture
    // of another kind is left alone and desktopView() stays empty. The game's reticle stays in the desktop's
    // picture: the aim mark that replaces it for the headset is drawn into the headset's images only.
    std::vector<uint8_t> draw(const TransferImage& source,uint32_t index,bool capture=false,bool wait=true,const UiFade& fade={},bool probe=false,const TransferImage* behind=nullptr){
        previousPending=pending;
        if(pending){vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"previous UI transfer completion");pending=false;}
        collectProbe();
        if(capture)wait=true;
        if(!std::isfinite(fade.outside)||fade.outside<0||fade.outside>1)throw std::invalid_argument("UI fade");
        for(const float c:fade.cursor)if(!std::isfinite(c)||c<0)throw std::invalid_argument("UI cursor");
        if(probe)ensureProbe();
        deskFresh=false;deskBytes.clear();
        bool desk=behind&&!deskFailed&&behind->image&&behind->image!=source.image&&behind->width&&behind->height&&behind->format==VK_FORMAT_R8G8B8A8_UNORM&&
            behind->layout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL&&behind->queueFamily==v.family&&(behind->usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        // The desktop's picture is an extra: without it the headset's UI is drawn as ever.
        if(desk)try{ensureDesktop();}catch(...){deskFailed=true;desk=false;}
        if(!source.image || source.width!=target.width || source.height!=target.height || index>=target.images.size() ||
           source.format!=VK_FORMAT_R8G8B8A8_UNORM || source.layout!=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
           source.queueFamily!=v.family || !(source.usage&VK_IMAGE_USAGE_SAMPLED_BIT))throw std::invalid_argument("UI source image unproven");
        for(const auto& i:target.images)if(i.image==source.image)throw std::invalid_argument("UI source aliases Runtime target");
        vkCheck(v.vkResetCommandBuffer(cmd,0),"UI transfer reset");
        if(sourceView){v.vkDestroyImageView(v.device,sourceView,nullptr);sourceView=VK_NULL_HANDLE;}
        sourceView=view(source.image,source.format);
        ensureOverlay();
        VkDescriptorImageInfo descriptor{sampler,sourceView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},overlayDescriptor{overlaySampler,overlayView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet,2> writes{};for(uint32_t i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;}
        writes[0].pImageInfo=&descriptor;writes[1].pImageInfo=&overlayDescriptor;
        vkUpdateDescriptorSets(v.device,2,writes.data(),0,nullptr);
        const auto bytes=VkDeviceSize(target.width)*target.height*4;
        if(capture&&desk&&!deskReadback){
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&deskReadback),"desktop UI readback");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,deskReadback,&req);
            deskReadbackMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,deskReadback,deskReadbackMemory,0),"desktop UI readback memory");
        }
        if(capture&&!readback){
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&readback),"UI transfer readback");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,readback,&req);
            readbackMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,readback,readbackMemory,0),"UI transfer readback memory");
        }
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(v.vkBeginCommandBuffer(cmd,&begin),"UI transfer begin");
        if(overlayUpload){
            barrier(overlayImage,overlayWritten?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,overlayWritten?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={overlayWidth,overlayHeight,1};
            vkCmdCopyBufferToImage(cmd,overlayStaging,overlayImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
            barrier(overlayImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            overlayUpload=false;overlayWritten=true;
        }
        barrier(source.image,source.layout,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        VkRenderPassBeginInfo r{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};r.renderPass=pass;r.framebuffer=framebuffers[index];r.renderArea={{0,0},{target.width,target.height}};
        v.vkCmdBeginRenderPass(cmd,&r,VK_SUBPASS_CONTENTS_INLINE);v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&set,0,nullptr);
        VkViewport viewport{0,0,float(target.width),float(target.height),0,1};VkRect2D scissor{{0,0},{target.width,target.height}};
        // As the shader's block: keep, keepWide, cursor, hole (four vec4), outside, display, (two pad), overlay (vec4 at 80).
        const std::array<float,24> push{fade.keep[0],fade.keep[1],fade.keep[2],fade.keep[3],fade.keepWide[0],fade.keepWide[1],fade.keepWide[2],fade.keepWide[3],
            fade.cursor[0],fade.cursor[1],fade.cursor[2],fade.cursor[3],fade.hole[0],fade.hole[1],fade.hole[2],fade.hole[3],fade.outside,0.f,0.f,0.f,
            fade.overlay[0],fade.overlay[1],fade.overlay[2],fade.overlay[3]};
        v.vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,96,push.data());
        v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);v.vkCmdDraw(cmd,3,1,0,0);v.vkCmdEndRenderPass(cmd);
        if(desk){
            // The picture behind was left in TRANSFER_SRC_OPTIMAL by its writer on this queue; it is only read.
            barrier(deskImage,deskWritten?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,deskWritten?VK_ACCESS_TRANSFER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            const auto crop=centreCrop(behind->width,behind->height,target.width,target.height);
            VkImageBlit blit{};blit.srcSubresource=blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            blit.srcOffsets[0]={crop.x0,crop.y0,0};blit.srcOffsets[1]={crop.x1,crop.y1,1};blit.dstOffsets[1]={int32_t(target.width),int32_t(target.height),1};
            vkCmdBlitImage(cmd,behind->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,deskImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
            barrier(deskImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            VkRenderPassBeginInfo over{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};over.renderPass=deskPass;over.framebuffer=deskFramebuffer;over.renderArea={{0,0},{target.width,target.height}};
            v.vkCmdBeginRenderPass(cmd,&over,VK_SUBPASS_CONTENTS_INLINE);v.vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,deskPipeline);
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&set,0,nullptr);
            auto flat=push;flat[12]=flat[13]=flat[14]=flat[15]=0;flat[17]=1;   // no hole; display values
            v.vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,96,flat.data());
            v.vkCmdSetViewport(cmd,0,1,&viewport);v.vkCmdSetScissor(cmd,0,1,&scissor);v.vkCmdDraw(cmd,3,1,0,0);v.vkCmdEndRenderPass(cmd);
            barrier(deskImage,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            if(capture){
                VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={target.width,target.height,1};
                v.vkCmdCopyImageToBuffer(cmd,deskImage,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,deskReadback,1,&region);
            }
        }
        if(capture){
            const auto image=target.images[index].image;
            barrier(image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={target.width,target.height,1};
            v.vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&region);
            barrier(image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        }
        barrier(source.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,source.layout,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        if(probe){
            // The source is back in TRANSFER_SRC_OPTIMAL (required above). Each level halves the one before it, so a
            // cell of the last level is the mean of its pixels; the game's image is only read.
            for(uint32_t level=0;level<probeLevels;++level){
                const int32_t fromW=level?int32_t(std::max(1u,probeWidth>>(level-1))):int32_t(source.width),fromH=level?int32_t(std::max(1u,probeHeight>>(level-1))):int32_t(source.height);
                levelBarrier(probeImage,level,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
                VkImageBlit blit{};blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level?level-1:0,0,1};blit.srcOffsets[1]={fromW,fromH,1};
                blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,1};blit.dstOffsets[1]={int32_t(std::max(1u,probeWidth>>level)),int32_t(std::max(1u,probeHeight>>level)),1};
                vkCmdBlitImage(cmd,level?probeImage:source.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,probeImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
                levelBarrier(probeImage,level,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            }
            VkBufferImageCopy cells{};cells.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,probeLevels-1,0,1};cells.imageExtent={probeCellsWidth(),probeCellsHeight(),1};
            v.vkCmdCopyImageToBuffer(cmd,probeImage,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,probeBuffer,1,&cells);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        }
        vkCheck(v.vkEndCommandBuffer(cmd),"UI transfer end");vkCheck(v.vkResetFences(v.device,1,&fence),"UI transfer fence reset");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
        vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"UI transfer submit");pending=true;if(probe)probeInFlight=true;
        if(desk)deskWritten=deskFresh=true;
        if(wait){vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"UI transfer GPU completion");pending=false;collectProbe();}
        std::vector<uint8_t> result;
        if(capture){result.resize(size_t(bytes));void* mapped{};vkCheck(v.vkMapMemory(v.device,readbackMemory,0,bytes,0,&mapped),"UI transfer readback map");std::memcpy(result.data(),mapped,result.size());v.vkUnmapMemory(v.device,readbackMemory);}
        if(capture&&desk){deskBytes.resize(size_t(bytes));void* mapped{};vkCheck(v.vkMapMemory(v.device,deskReadbackMemory,0,bytes,0,&mapped),"desktop UI readback map");std::memcpy(deskBytes.data(),mapped,deskBytes.size());v.vkUnmapMemory(v.device,deskReadbackMemory);}
        return result;
    }
#undef UI_TRANSFER_API
};
}
