// Independent GPU experiment; never loads the game or changes runtime settings.
#include <evr/vulkan.hpp>
#include <evr/ui_spirv_patch.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <atomic>
#include <cmath>
using namespace evr;
namespace fs=std::filesystem;
static std::atomic<unsigned> validationIssues{0};
static VKAPI_ATTR VkBool32 VKAPI_CALL message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void*){
    if(severity&(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)){
        ++validationIssues;std::cerr<<"VULKAN "<<data->pMessage<<'\n';
    }return VK_FALSE;
}
static std::vector<uint32_t> words(const fs::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("missing input "+path.string());
    const auto n=f.tellg();if(n<=0||n>1024*1024||n%4)throw std::runtime_error("invalid input size");
    std::vector<uint32_t> w(size_t(n)/4);f.seekg(0);f.read(reinterpret_cast<char*>(w.data()),n);
    if(!f)throw std::runtime_error("input read");return w;
}
#define UI_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) \
 X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateSampler) \
 X(vkDestroySampler) X(vkCmdBindDescriptorSets) X(vkCmdCopyBufferToImage) X(vkCmdBeginRendering) X(vkCmdEndRendering) X(vkCmdSetStencilReference)
struct Fixture {
    static constexpr uint32_t width=96,height=64;
    static constexpr size_t imageBytes=width*height*4;
    Vulkan v;
    VkDebugUtilsMessengerEXT messenger{};
    PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool descriptors{};VkDescriptorSet set{};
    VkPipelineLayout layout{};VkSampler sampler{};std::array<std::array<VkPipeline,4>,2> pipelines{};
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};
    struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};void* mapped=nullptr;VkDeviceSize size=0;};
    struct Image {VkImage handle{};VkDeviceMemory memory{};VkImageView view{};};
    Buffer uniform,upload,readback;Image texture;std::array<Image,2> color{};
#define DECL(name) PFN_##name name=nullptr;
    UI_API(DECL)
#undef DECL
    ~Fixture(){
        if(v.device){
            v.vkDeviceWaitIdle(v.device);
            for(auto* b:{&uniform,&upload,&readback}){
                if(b->mapped)v.vkUnmapMemory(v.device,b->memory);
                if(b->handle)v.vkDestroyBuffer(v.device,b->handle,nullptr);
                if(b->memory)v.vkFreeMemory(v.device,b->memory,nullptr);
            }
            for(auto* i:{&texture,&color[0],&color[1]}){
                if(i->view)v.vkDestroyImageView(v.device,i->view,nullptr);
                if(i->handle)v.vkDestroyImage(v.device,i->handle,nullptr);
                if(i->memory)v.vkFreeMemory(v.device,i->memory,nullptr);
            }
            if(fence)v.vkDestroyFence(v.device,fence,nullptr);
            if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);
            for(auto& pair:pipelines)for(auto p:pair)if(p)v.vkDestroyPipeline(v.device,p,nullptr);
            if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);
            if(descriptors)vkDestroyDescriptorPool(v.device,descriptors,nullptr);
            if(setLayout)vkDestroyDescriptorSetLayout(v.device,setLayout,nullptr);
            if(sampler)vkDestroySampler(v.device,sampler,nullptr);
            // Keep validation active through device destruction.
            v.vkDestroyDevice(v.device,nullptr);v.device=VK_NULL_HANDLE;
        }
        if(messenger)destroyMessenger(v.instance,messenger,nullptr);
    }
    void makeBuffer(Buffer& b,VkDeviceSize size,VkBufferUsageFlags usage){
        b.size=size;VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=size;ci.usage=usage;
        vkCheck(v.vkCreateBuffer(v.device,&ci,nullptr,&b.handle),"buffer");
        VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,b.handle,&req);
        b.memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkCheck(v.vkBindBufferMemory(v.device,b.handle,b.memory,0),"bind buffer");
        vkCheck(v.vkMapMemory(v.device,b.memory,0,size,0,&b.mapped),"map buffer");
    }
    void makeImage(Image& i,uint32_t w,uint32_t h,VkImageUsageFlags usage){
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;
        ci.format=VK_FORMAT_R8G8B8A8_UNORM;ci.extent={w,h,1};ci.mipLevels=1;ci.arrayLayers=1;
        ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
        vkCheck(v.vkCreateImage(v.device,&ci,nullptr,&i.handle),"image");
        VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,i.handle,&req);
        i.memory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkCheck(v.vkBindImageMemory(v.device,i.handle,i.memory,0),"bind image");
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=i.handle;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;
        vi.format=ci.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCheck(v.vkCreateImageView(v.device,&vi,nullptr,&i.view),"image view");
    }
    VkShaderModule shader(const std::vector<uint32_t>& code){
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=code.size()*4;ci.pCode=code.data();
        VkShaderModule m{};vkCheck(v.vkCreateShaderModule(v.device,&ci,nullptr,&m),"shader");return m;
    }
    void pipeline(unsigned kind,unsigned mrt,const std::vector<uint32_t>& vert,const std::vector<uint32_t>& frag,bool additive=false){
        const auto vs=shader(vert),ps=shader(frag);
        std::array<VkPipelineShaderStageCreateInfo,2> stages{{
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vs,"main",nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,ps,"fs_main",nullptr}}};
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;
        rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        std::array<VkPipelineColorBlendAttachmentState,2> blend{};
        for(auto& b:blend){b.blendEnable=VK_TRUE;b.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;b.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            b.colorBlendOp=VK_BLEND_OP_ADD;b.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;b.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            b.alphaBlendOp=VK_BLEND_OP_ADD;b.colorWriteMask=15;
            if(additive)b.dstColorBlendFactor=b.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;}
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1+mrt;cb.pAttachments=blend.data();
        const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dy.dynamicStateCount=3;dy.pDynamicStates=states;
        const VkFormat formats[]{VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM};
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1+mrt;rendering.pColorAttachmentFormats=formats;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.pNext=&rendering;ci.stageCount=2;ci.pStages=stages.data();
        ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;
        ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&cb;ci.pDynamicState=&dy;ci.layout=layout;
        auto result=v.vkCreateGraphicsPipelines(v.device,VK_NULL_HANDLE,1,&ci,nullptr,&pipelines[kind][mrt+(additive?2:0)]);
        v.vkDestroyShaderModule(v.device,ps,nullptr);v.vkDestroyShaderModule(v.device,vs,nullptr);vkCheck(result,"graphics pipeline");
    }
    void init(const std::array<std::vector<uint32_t>,2>& native,const std::vector<uint32_t>& vert){
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="EVR isolated native UI fixture";app.apiVersion=VK_API_VERSION_1_3;
        const char* layers[]{"VK_LAYER_KHRONOS_validation"};const char* extensions[]{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        debug.pfnUserCallback=message;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;ci.pNext=&debug;
        ci.enabledLayerCount=1;ci.ppEnabledLayerNames=layers;ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=extensions;
        VkInstance instance{};vkCheck(v.vkCreateInstance(&ci,nullptr,&instance),"instance");v.owned=true;v.bindInstance(instance);
        auto createMessenger=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(v.gipa(instance,"vkCreateDebugUtilsMessengerEXT"));
        destroyMessenger=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(v.gipa(instance,"vkDestroyDebugUtilsMessengerEXT"));
        if(!createMessenger||!destroyMessenger)throw std::runtime_error("debug utils");
        vkCheck(createMessenger(instance,&debug,nullptr,&messenger),"validation messenger");
        uint32_t count=0;vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,nullptr),"physical count");
        std::vector<VkPhysicalDevice> physicals(count);vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,physicals.data()),"physical devices");
        if(physicals.empty())throw std::runtime_error("no GPU");const auto physical=physicals.front();
        VkPhysicalDeviceProperties props{};v.vkGetPhysicalDeviceProperties(physical,&props);
        if(props.apiVersion<VK_API_VERSION_1_3||props.limits.maxColorAttachments<2)throw std::runtime_error("GPU lacks MRT/dynamic rendering");
        std::cout<<"GPU "<<props.deviceName<<" driver="<<props.driverVersion<<'\n';
        uint32_t families=0;v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,queues.data());
        uint32_t family=0;while(family<families&&!(queues[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))++family;
        if(family==families)throw std::runtime_error("no graphics queue");
        auto getFeatures=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(v.gipa(instance,"vkGetPhysicalDeviceFeatures2"));
        VkPhysicalDeviceVulkan13Features supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&supported;getFeatures(physical,&features);
        if(!supported.dynamicRendering)throw std::runtime_error("dynamic rendering unsupported");
        VkPhysicalDeviceVulkan13Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};enabled.dynamicRendering=VK_TRUE;
        const float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pNext=&enabled;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        VkDevice device{};vkCheck(v.vkCreateDevice(physical,&di,nullptr,&device),"device");v.bindDevice(device,physical,family);
#define LOAD(name) name=reinterpret_cast<PFN_##name>(v.vkGetDeviceProcAddr(device,#name));if(!name)throw std::runtime_error(#name);
        UI_API(LOAD)
#undef LOAD
        const VkDescriptorSetLayoutBinding bindings[]{
            {1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=3;sl.pBindings=bindings;
        vkCheck(vkCreateDescriptorSetLayout(device,&sl,nullptr,&setLayout),"descriptor layout");
        const VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1},{VK_DESCRIPTOR_TYPE_SAMPLER,1}};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=3;dp.pPoolSizes=sizes;
        vkCheck(vkCreateDescriptorPool(device,&dp,nullptr,&descriptors),"descriptor pool");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=descriptors;da.descriptorSetCount=1;da.pSetLayouts=&setLayout;
        vkCheck(vkAllocateDescriptorSets(device,&da,&set),"descriptor set");
        const VkPushConstantRange pc{VK_SHADER_STAGE_VERTEX_BIT,0,16};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&pc;
        vkCheck(v.vkCreatePipelineLayout(device,&pl,nullptr,&layout),"pipeline layout");
        for(unsigned kind=0;kind<2;kind++){
            pipeline(kind,0,vert,native[kind]);
            pipeline(kind,1,vert,ui::duplicateFragmentOutput(native[kind].data(),native[kind].size()*4));
            pipeline(kind,0,vert,native[kind],true);
            pipeline(kind,1,vert,ui::duplicateFragmentOutput(native[kind].data(),native[kind].size()*4,true),true);
        }
        makeBuffer(uniform,48,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        makeBuffer(upload,16,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        makeBuffer(readback,imageBytes*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        makeImage(texture,2,2,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        for(auto& c:color)makeImage(c,width,height,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=VK_FILTER_NEAREST;si.minFilter=VK_FILTER_NEAREST;
        si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.maxLod=0;
        vkCheck(vkCreateSampler(device,&si,nullptr,&sampler),"sampler");
        VkDescriptorBufferInfo bi{uniform.handle,0,48};
        VkDescriptorImageInfo ii{VK_NULL_HANDLE,texture.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},sam{sampler,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED};
        const VkWriteDescriptorSet writes[]{
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,1,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,nullptr,&bi,nullptr},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,2,0,1,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,&ii,nullptr,nullptr},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,3,0,1,VK_DESCRIPTOR_TYPE_SAMPLER,&sam,nullptr,nullptr}};
        vkUpdateDescriptorSets(device,3,writes,0,nullptr);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=family;
        vkCheck(v.vkCreateCommandPool(device,&cp,nullptr,&pool),"command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(device,&ca,&command),"command");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(device,&fi,nullptr,&fence),"fence");
    }
    void transition(Image& i,VkImageLayout oldLayout,VkImageLayout newLayout,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkAccessFlags read,VkAccessFlags write){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=read;b.dstAccessMask=write;b.oldLayout=oldLayout;b.newLayout=newLayout;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i.handle;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        v.vkCmdPipelineBarrier(command,src,dst,0,0,nullptr,0,nullptr,1,&b);
    }
    std::array<std::vector<uint8_t>,2> run(unsigned kind,bool mrt,std::array<float,4> background,unsigned draws,std::array<float,4> tint,bool fallbackMiddle=false,bool skipMiddle=false,unsigned blendMode=0){
        vkCheck(v.vkResetCommandBuffer(command,0),"reset");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkCheck(v.vkBeginCommandBuffer(command,&begin),"begin");
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_UNIFORM_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT;
        v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&host,0,nullptr,0,nullptr);
        transition(texture,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={2,2,1};
        vkCmdCopyBufferToImage(command,upload.handle,texture.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        transition(texture,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
        std::array<VkRenderingAttachmentInfo,2> attachments{};
        for(unsigned i=0;i<(mrt?2u:1u);i++){
            transition(color[i],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            auto& a=attachments[i];a.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;a.imageView=color[i].view;a.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
            if(!i)std::copy(background.begin(),background.end(),a.clearValue.color.float32);
        }
        VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};ri.renderArea={{0,0},{width,height}};ri.layerCount=1;ri.colorAttachmentCount=mrt?2:1;ri.pColorAttachments=attachments.data();
        vkCmdBeginRendering(command,&ri);
        vkCmdSetStencilReference(command,VK_STENCIL_FACE_FRONT_AND_BACK,17);
        v.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[kind][mrt?1:0]);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&set,0,nullptr);
        const VkViewport vp{0,0,float(width),float(height),0,1};v.vkCmdSetViewport(command,0,1,&vp);
        bool activeMrt=mrt;
        for(unsigned i=0;i<draws;i++){
            if(skipMiddle&&i==1)continue;
            const bool wanted=mrt&&!(fallbackMiddle&&i==1);
            if(activeMrt!=wanted){
                vkCmdEndRendering(command);
                for(auto& target:color)
                    transition(target,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                for(auto& a:attachments)a.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
                ri.colorAttachmentCount=wanted?2:1;vkCmdBeginRendering(command,&ri);
                v.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[kind][wanted?1:0]);
                activeMrt=wanted;
            }
            const bool additive=blendMode==1||(blendMode==2&&i==1);
            v.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[kind][(activeMrt?1:0)+(additive?2:0)]);
            const VkRect2D scissor{{int32_t(4+i*9),int32_t(3+i*7)},{width-12-i*12,height-9-i*8}};
            v.vkCmdSetScissor(command,0,1,&scissor);
            auto c=tint;if(i){c[0]*=.4f;c[1]*=.7f;c[3]*=.65f;}
            v.vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,16,c.data());v.vkCmdDraw(command,3,1,0,0);
        }
        vkCmdEndRendering(command);
        for(unsigned i=0;i<(mrt?2u:1u);i++){
            transition(color[i],VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy out{};out.bufferOffset=imageBytes*i;out.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};out.imageExtent={width,height,1};
            v.vkCmdCopyImageToBuffer(command,color[i].handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&out);
        }
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        vkCheck(v.vkEndCommandBuffer(command),"end");vkCheck(v.vkResetFences(v.device,1,&fence),"reset fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
        vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"submit");vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,10'000'000'000),"complete");
        std::array<std::vector<uint8_t>,2> result;
        for(unsigned i=0;i<(mrt?2u:1u);i++){auto* p=static_cast<uint8_t*>(readback.mapped)+imageBytes*i;result[i].assign(p,p+imageBytes);}
        return result;
    }
};
int main(int argc,char** argv){try{
    if(argc!=4)throw std::runtime_error("usage: evr_ui_gpu_tests local-ui-raster-dir fixture.vert.spv new-output-dir");
    const fs::path source=argv[1],out=argv[3];
    if(fs::exists(out))throw std::runtime_error("output already exists; preserve earlier evidence");
    fs::create_directories(out);
    unsigned passed=0,failed=0;
    std::ofstream report(out/"checks.jsonl");
    auto check=[&](bool okay,const std::string& name){
        (okay?passed:failed)++;std::cout<<(okay?"PASS ":"FAIL ")<<name<<'\n';
        report<<"{\"pass\":"<<(okay?"true":"false")<<",\"name\":\""<<name<<"\"}\n";
    };
    std::array<std::vector<uint32_t>,2> native{words(source/"event_13934_stage_4.spv"),words(source/"event_13941_stage_4.spv")};
    const auto original=native;
    for(unsigned k=0;k<2;k++){
        auto p=ui::duplicateFragmentOutput(native[k].data(),native[k].size()*4);
        std::ofstream f(out/(std::to_string(k)+"-mrt.spv"),std::ios::binary);f.write(reinterpret_cast<const char*>(p.data()),std::streamsize(p.size()*4));
        check(native[k]==original[k],"shader "+std::to_string(k)+" source unchanged");
        auto bad=native[k];bad.back()^=1;bool rejected=false;
        try{ui::duplicateFragmentOutput(bad.data(),bad.size()*4);}catch(const std::exception&){rejected=true;}
        check(rejected,"shader "+std::to_string(k)+" unknown hash rejected");
        rejected=false;try{ui::duplicateFragmentOutput(native[k].data(),native[k].size()*4-4);}catch(const std::exception&){rejected=true;}
        check(rejected,"shader "+std::to_string(k)+" truncated source rejected");
    }
    for(unsigned k=0;k<2;k++){
        const auto code=ui::duplicateFragmentOutput(native[k].data(),native[k].size()*4,true);
        std::ofstream f(out/(std::to_string(k)+"-additive-mrt.spv"),std::ios::binary);
        f.write(reinterpret_cast<const char*>(code.data()),std::streamsize(code.size()*4));
    }
    bool nullRejected=false;try{ui::duplicateFragmentOutput(nullptr,2640);}catch(const std::exception&){nullRejected=true;}
    check(nullRejected,"null source rejected");
    {
        Fixture fixture;fixture.init(native,words(argv[2]));
        for(unsigned k=0;k<2;k++){
            const auto cb=words(source/(k?"event_13941_stage_4_constant_0.bin":"event_13934_stage_4_constant_0.bin"));
            std::memset(fixture.uniform.mapped,0,48);std::memcpy(fixture.uniform.mapped,cb.data(),cb.size()*4);
            for(unsigned pattern=0;pattern<5;pattern++)for(unsigned draws:{0u,1u,3u}){
                auto* tex=static_cast<uint8_t*>(fixture.upload.mapped);
                for(unsigned i=0;i<4;i++){
                    const unsigned a=pattern==0?0:pattern==1?127:pattern==2?255:((i&1)?255:64);
                    // Normal image uses premultiplied texture RGB; font samples the red distance field.
                    tex[i*4]=uint8_t(k?(i*63+32):a*.6);tex[i*4+1]=uint8_t(a*.35);tex[i*4+2]=uint8_t(a*.8);tex[i*4+3]=uint8_t(a);
                }
                const std::array<float,4> tint{.9f,.7f,.6f,pattern==4?.4f:1.0f},bg{.2f,.4f,.6f,1};
                const auto normal=fixture.run(k,false,bg,draws,tint)[0];
                const auto isolated=fixture.run(k,false,{},draws,tint)[0];
                const auto mrt=fixture.run(k,true,bg,draws,tint);
                const auto alternate=fixture.run(k,true,{.8f,.1f,.3f,1},draws,tint);
                const auto tag=std::to_string(k)+"/"+std::to_string(pattern)+"/"+std::to_string(draws);
                check(normal==mrt[0],tag+" original color byte-exact");
                check(isolated==mrt[1],tag+" isolated UI byte-exact");
                check(mrt[1]==alternate[1],tag+" UI independent of world color");
                size_t transparent=0,covered=0;int maxError=0;
                for(size_t p=0;p<normal.size();p+=4){
                    const int a=mrt[1][p+3];transparent+=a==0;covered+=a>0;
                    for(unsigned c=0;c<4;c++){
                        const int value=std::min(255,int(std::lround(mrt[1][p+c]+bg[c]*(255-a))));
                        maxError=std::max(maxError,std::abs(value-int(normal[p+c])));
                    }
                }
                check(transparent>0,tag+" untouched UI area remains transparent");
                check(draws==0?covered==0:(k||pattern!=0?covered>0:covered==0),tag+" expected coverage");
                check(maxError<=3,tag+" premultiplied recomposition max error <=3");
                if(draws==3){
                    const auto fallback=fixture.run(k,true,bg,draws,tint,true);
                    const auto knownOnly=fixture.run(k,false,{},draws,tint,false,true)[0];
                    check(fallback[0]==normal,tag+" split pass fallback preserves original byte-exact");
                    check(fallback[1]==knownOnly,tag+" split pass resumes only known UI draws");
                }
                if(k==0&&pattern==3&&draws==3){
                    for(unsigned i=0;i<2;i++){std::ofstream f(out/(i?"ui.rgba8":"desktop.rgba8"),std::ios::binary);f.write(reinterpret_cast<const char*>(mrt[i].data()),mrt[i].size());}
                }
            }
        }
        for(unsigned k=0;k<2;k++)for(unsigned mode:{1u,2u}){
            const auto cb=words(source/(k?"event_13941_stage_4_constant_0.bin":"event_13934_stage_4_constant_0.bin"));
            std::memset(fixture.uniform.mapped,0,48);std::memcpy(fixture.uniform.mapped,cb.data(),cb.size()*4);
            const std::array<uint8_t,16> texture{90,30,60,127,150,70,100,200,45,65,30,127,220,180,140,255};
            std::memcpy(fixture.upload.mapped,texture.data(),texture.size());
            const std::array<float,4> bg{.05f,.05f,.05f,1},tint{.18f,.14f,.12f,.65f};
            const auto normal=fixture.run(k,false,bg,3,tint,false,false,mode)[0];
            const auto mrt=fixture.run(k,true,bg,3,tint,false,false,mode);
            const auto alternative=fixture.run(k,true,{.1f,.07f,.1f,1},3,tint,false,false,mode);
            const auto tag="additive "+std::to_string(k)+"/"+std::to_string(mode);
            check(normal==mrt[0],tag+" original byte-exact");
            check(mrt[1]==alternative[1],tag+" UI independent of world");
            int maxError=0;size_t zeroAlpha=0,visibleRgb=0;
            for(size_t p=0;p<normal.size();p+=4){
                const int a=mrt[1][p+3];zeroAlpha+=a==0;visibleRgb+=(mrt[1][p]+mrt[1][p+1]+mrt[1][p+2])>0;
                for(unsigned c=0;c<4;c++){
                    const int value=std::min(255,int(std::lround(mrt[1][p+c]+bg[c]*(255-a))));
                    maxError=std::max(maxError,std::abs(value-int(normal[p+c])));
                }
            }
            check(visibleRgb>0,tag+" visible additive RGB");
            check(mode!=1||zeroAlpha==Fixture::width*Fixture::height,tag+" additive does not occlude world");
            check(maxError<=3,tag+" low-range recomposition <=3");
            if(mode==1&&k==0){
                // A native transparent clear retains additive alpha and is NOT a
                // valid source-over layer. This negative control must detect it.
                const auto incorrect=fixture.run(k,false,{},3,tint,false,false,mode)[0];
                size_t bad=0;
                for(size_t p=0;p<normal.size();p+=4)for(unsigned c=0;c<3;c++){
                    const int value=std::min(255,int(std::lround(incorrect[p+c]+bg[c]*(255-incorrect[p+3]))));
                    bad+=std::abs(value-int(normal[p+c]))>3;
                }
                check(bad>0,"negative control rejects native additive alpha as UI coverage");
            }
        }
    }
    check(validationIssues==0,"Vulkan validation including teardown");
    std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";
    std::ofstream(out/"summary.json")<<"{\"passed\":"<<passed<<",\"failed\":"<<failed<<",\"validationIssues\":"<<validationIssues.load()
        <<",\"width\":"<<Fixture::width<<",\"height\":"<<Fixture::height<<"}\n";
    return failed?1:0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
