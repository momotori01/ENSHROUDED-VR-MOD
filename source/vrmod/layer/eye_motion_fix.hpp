#pragma once
#include <eye_motion_fix.h>
// Motion vectors for alternate-eye rendering (2026-10-23; the user: "PureDark is done when the scenery does not shake while
// walking"). With one eye drawn a frame, DLSS's history of an eye is its picture of two frames back. The game's motion
// vectors carry the camera's motion from the View's own previous camera - right, two frames - but a moving thing's motion
// from where it was one frame back (Mock 2026-10-23: the right hand moved 5 mm a frame, the held bow's vectors were 2.23
// pixels with both eyes drawn and 2.23 with one eye a frame; RenderDoc of a head turn: the camera's part 6.26 pixels where
// two frames make 6.39). DLSS took the hands, the weapons and the grass from half way and smeared them (Mock pictures: the
// gauntlet's edges doubled with one eye a frame, sharp with both).
// The hooks (hooks/dlss_eval_observer.hpp) call evrFixEyeMotion just before the game's DLSS evaluation of such an eye, with
// the matrix from this frame's clip space to the eye's last drawing's (the two game cameras the mod gave the game): here a
// compute pass in the game's command buffer works out each pixel's camera motion from its depth, stretches the rest by the
// frames between (shaders/eye_motion_fix.comp) into the layer's own image, and the evaluation reads that image instead.
// EVR_AFW_MOTION_CHECK=N: every Nth pass also writes the game's motion minus the camera's (pixels) and logs how much of the
// picture moves by itself (afw_motion_check) - near 0 where nothing moves, when the matrix is right.
namespace eyeMotion {
#define EYE_MOTION_API(X) X(vkCreateImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory) X(vkBindImageMemory) X(vkCreateImageView) \
 X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreateDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkCreateComputePipelines) \
 X(vkCreateDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCmdPipelineBarrier) X(vkCmdBindPipeline) \
 X(vkCmdBindDescriptorSets) X(vkCmdDispatch) X(vkCmdPushConstants) X(vkCreateSampler) X(vkCreateBuffer) X(vkGetBufferMemoryRequirements) \
 X(vkBindBufferMemory) X(vkMapMemory) X(vkCmdCopyImageToBuffer)
constexpr uint32_t ring=16,checkRing=4;
struct Output {VkImage image{},residual{};VkImageView view{},residualView{};uint32_t width=0,height=0;};
struct Check {VkBuffer buffer{};void* mapped=nullptr;uint32_t width=0,height=0;uint64_t call=0;bool pending=false;};
struct Device {
    VkDevice handle{};VkPhysicalDeviceMemoryProperties memory{};bool ready=false;
#define EYE_MOTION_DECL(n) PFN_##n n=nullptr;
    EYE_MOTION_API(EYE_MOTION_DECL)
#undef EYE_MOTION_DECL
    VkDescriptorSetLayout setLayout{};VkPipelineLayout pipelineLayout{};VkPipeline pipeline{};VkDescriptorPool pool{};VkSampler sampler{};
    std::array<VkDescriptorSet,ring> sets{};uint32_t next=0;
    std::array<Output,2> outputs{};std::array<Check,checkRing> checks{};uint32_t checkNext=0;
    uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags){
        for(uint32_t i=0;i<memory.memoryTypeCount;i++)if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("eye motion memory type");
    }
};
inline std::mutex fixMutex;inline std::unordered_map<VkDevice,std::shared_ptr<Device>> fixDevices;
inline uint64_t calls=0,failures=0;
// The mod's own frames for every refresh (include/evr/afw_pacing.hpp, pacing 2; 2026-10-23): each pass also writes the things'
// own motion (the game's less the camera's, UV) into the eye's residual image, and the eye's copy keeps it with the picture
// (native_xr.hpp takeObjectMotion) - the refreshes carry a moving thing on by it. Set when the mod's own queue is made.
inline std::atomic<bool> keepObjectMotion{false};
struct ObjectMotion {VkImage image{};uint32_t width=0,height=0;bool fresh=false;};
inline std::array<ObjectMotion,2> objectMotion{};   // under fixMutex
// The eye's things' own motion written since the last take (by this frame's pass), else false.
inline bool takeObjectMotion(uint32_t eye,VkImage& image,uint32_t& width,uint32_t& height){
    std::lock_guard<std::mutex> l(fixMutex);auto& o=objectMotion[eye&1];
    if(!o.fresh||!o.image)return false;
    o.fresh=false;image=o.image;width=o.width;height=o.height;return true;
}
inline void check(VkResult r,const char* what){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(what)+" "+std::to_string(r));}
inline std::shared_ptr<Device> deviceFor(VkCommandBuffer command){
    DeviceDispatch dispatch;
    {std::lock_guard<std::mutex> l(mutex);auto d=devices.find(key(command));if(d==devices.end())throw std::runtime_error("eye motion device");dispatch=d->second;}
    auto found=fixDevices.find(dispatch.device);if(found!=fixDevices.end())return found->second;
    auto r=std::make_shared<Device>();r->handle=dispatch.device;
    auto getMemory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(dispatch.gipa(dispatch.instance,"vkGetPhysicalDeviceMemoryProperties"));
    if(!getMemory)throw std::runtime_error("eye motion memory properties");getMemory(dispatch.physical,&r->memory);
#define EYE_MOTION_LOAD(n) r->n=reinterpret_cast<PFN_##n>(dispatch.gdpa(dispatch.device,#n));if(!r->n)throw std::runtime_error(#n);
    EYE_MOTION_API(EYE_MOTION_LOAD)
#undef EYE_MOTION_LOAD
    fixDevices.emplace(dispatch.device,r);return r;
}
// The push constants of shaders/eye_motion_fix.comp.
struct Push {float reproject[16];float size[2];float objectScale,ySign;uint32_t check,pad[3];};
static_assert(sizeof(Push)==96,"eye motion push constants");
inline void prepare(Device& d){
    if(d.ready)return;
    std::array<VkDescriptorSetLayoutBinding,4> b{};
    const VkDescriptorType types[4]{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
    for(uint32_t i=0;i<4;i++){b[i].binding=i;b[i].descriptorType=types[i];b[i].descriptorCount=1;b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=4;li.pBindings=b.data();
    check(d.vkCreateDescriptorSetLayout(d.handle,&li,nullptr,&d.setLayout),"eye motion set layout");
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Push)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&d.setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;
    check(d.vkCreatePipelineLayout(d.handle,&pl,nullptr,&d.pipelineLayout),"eye motion pipeline layout");
    VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.pCode=eyeMotionFix;si.codeSize=sizeof(eyeMotionFix);
    VkShaderModule module{};check(d.vkCreateShaderModule(d.handle,&si,nullptr,&module),"eye motion shader");
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";ci.layout=d.pipelineLayout;
    const auto r=d.vkCreateComputePipelines(d.handle,VK_NULL_HANDLE,1,&ci,nullptr,&d.pipeline);d.vkDestroyShaderModule(d.handle,module,nullptr);check(r,"eye motion pipeline");
    const VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,ring*2},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,ring*2}};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=ring;pi.poolSizeCount=2;pi.pPoolSizes=sizes;
    check(d.vkCreateDescriptorPool(d.handle,&pi,nullptr,&d.pool),"eye motion descriptor pool");
    std::array<VkDescriptorSetLayout,ring> layouts;layouts.fill(d.setLayout);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=d.pool;ai.descriptorSetCount=ring;ai.pSetLayouts=layouts.data();
    check(d.vkAllocateDescriptorSets(d.handle,&ai,d.sets.data()),"eye motion descriptor sets");
    VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sc.magFilter=sc.minFilter=VK_FILTER_NEAREST;sc.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sc.addressModeU=sc.addressModeV=sc.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sc.maxLod=0.f;
    check(d.vkCreateSampler(d.handle,&sc,nullptr,&d.sampler),"eye motion sampler");
    d.ready=true;
}
inline VkImage image2d(Device& d,uint32_t w,uint32_t h,VkImageUsageFlags usage,VkImageView& view){
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_R16G16_SFLOAT;ci.extent={w,h,1};ci.mipLevels=ci.arrayLayers=1;
    ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
    VkImage image{};check(d.vkCreateImage(d.handle,&ci,nullptr,&image),"eye motion image");VkMemoryRequirements req{};d.vkGetImageMemoryRequirements(d.handle,image,&req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=d.memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory m{};check(d.vkAllocateMemory(d.handle,&a,nullptr,&m),"eye motion memory");check(d.vkBindImageMemory(d.handle,image,m,0),"eye motion bind");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=VK_FORMAT_R16G16_SFLOAT;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    check(d.vkCreateImageView(d.handle,&vi,nullptr,&view),"eye motion view");
    return image;   // kept for the device's life (two eyes, made once; a new size makes new ones)
}
inline Output& outputFor(Device& d,uint32_t eye,uint32_t w,uint32_t h){
    auto& o=d.outputs[eye&1];
    if(o.width!=w||o.height!=h){
        o.image=image2d(d,w,h,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,o.view);
        o.residual=image2d(d,w,h,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,o.residualView);
        o.width=w;o.height=h;
        note("afw_motion_image","\"eye\":"+std::to_string(eye)+",\"size\":["+std::to_string(w)+","+std::to_string(h)+"]");
    }
    return o;
}
inline float half(uint16_t h){
    const uint32_t s=(h>>15)&1,e=(h>>10)&31,m=h&1023;float v;
    if(e==0)v=std::ldexp(float(m),-24);else if(e==31)v=m?NAN:INFINITY;else v=std::ldexp(float(m|1024),int(e)-25);
    return s?-v:v;
}
// A check whose copy is done (at least checkRing passes later): how far the game's motion is from the camera's.
inline void readChecks(Device& d){
    for(auto& c:d.checks){
        if(!c.pending||calls<c.call+checkRing)continue;
        c.pending=false;
        const auto* p=static_cast<const uint16_t*>(c.mapped);std::vector<float> xs,ys,mags;xs.reserve(70000);ys.reserve(70000);mags.reserve(70000);
        for(uint32_t y=0;y<c.height;y+=4)for(uint32_t x=0;x<c.width;x+=4){
            const float rx=half(p[(size_t(y)*c.width+x)*2]),ry=half(p[(size_t(y)*c.width+x)*2+1]);
            if(!std::isfinite(rx)||!std::isfinite(ry))continue;xs.push_back(rx);ys.push_back(ry);mags.push_back(std::hypot(rx,ry));}
        if(mags.empty())continue;
        const auto at=[](std::vector<float> v,double q){std::nth_element(v.begin(),v.begin()+size_t(q*(v.size()-1)),v.end());return v[size_t(q*(v.size()-1))];};
        size_t still=0,small=0;for(const float m:mags){if(m<.25f)++still;if(m<1.f)++small;}
        std::ostringstream o;o<<std::setprecision(4)<<"\"pass\":"<<c.call<<",\"median_px\":["<<at(xs,.5)<<','<<at(ys,.5)<<"],\"p10_px\":["<<at(xs,.1)<<','<<at(ys,.1)
            <<"],\"p90_px\":["<<at(xs,.9)<<','<<at(ys,.9)<<"],\"under_quarter_px\":"<<double(still)/mags.size()<<",\"under_1px\":"<<double(small)/mags.size()<<",\"magnitude_p90\":"<<at(mags,.9);
        note("afw_motion_check",o.str());
    }
}
} // namespace eyeMotion
// hooks/dlss_eval_observer.hpp, just before the game's DLSS evaluation of an eye drawn one frame in `frames`: records the
// pass into `command` and returns the layer's image (R16G16_SFLOAT, SHADER_READ_ONLY_OPTIMAL afterwards) and its view for
// the evaluation's motion vectors. reproject: 16 floats, row-major (this frame's clip -> the last drawing's clip, the
// mod's row-vector convention). inputLayout: the layout of the game's motion and depth images there. 0 done, else not.
extern "C" __declspec(dllexport) int evrFixEyeMotion(VkCommandBuffer command,uint32_t eye,VkImageView motionView,VkImageView depthView,uint32_t width,uint32_t height,
                                                     const float* reproject,float frames,float ySign,uint32_t inputLayout,VkImage* outImage,VkImageView* outView) noexcept {
    using namespace eyeMotion;
    try{
        if(!command||!motionView||!depthView||!width||!height||width>8192||height>8192||!reproject||!outImage||!outView||!(frames>=1&&frames<=8))return 2;
        for(int i=0;i<16;i++)if(!std::isfinite(reproject[i]))return 2;
        std::lock_guard<std::mutex> l(fixMutex);
        auto dp=deviceFor(command);auto& d=*dp;prepare(d);++calls;readChecks(d);
        auto& o=outputFor(d,eye,width,height);
        static const uint32_t checkEvery=[]{const char* v=std::getenv("EVR_AFW_MOTION_CHECK");const auto n=v?std::strtoul(v,nullptr,10):0ul;return uint32_t(n>1000?1000:n);}();
        const bool keepObject=keepObjectMotion.load();
        const bool checking=!keepObject&&checkEvery&&calls%checkEvery==0;
        const VkImageLayout in=VkImageLayout(inputLayout);
        const auto set=d.sets[d.next++%ring];
        VkDescriptorImageInfo images[4]{{d.sampler,motionView,in},{d.sampler,depthView,in},{VK_NULL_HANDLE,o.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,o.residualView,VK_IMAGE_LAYOUT_GENERAL}};
        std::array<VkWriteDescriptorSet,4> w{};
        for(uint32_t i=0;i<4;i++){w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w[i].dstSet=set;w[i].dstBinding=i;w[i].descriptorCount=1;
            w[i].descriptorType=i<2?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;w[i].pImageInfo=&images[i];}
        d.vkUpdateDescriptorSets(d.handle,4,w.data(),0,nullptr);
        // the game's writes of its motion and depth before the pass; the layer's images start anew (the last evaluation read them)
        VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};memory.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;memory.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        std::array<VkImageMemoryBarrier,2> start{};
        for(int i=0;i<2;i++){auto& b=start[i];b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.srcAccessMask=0;b.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
            b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=i?o.residual:o.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};}
        d.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&memory,0,nullptr,2,start.data());
        d.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,d.pipeline);
        d.vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,d.pipelineLayout,0,1,&set,0,nullptr);
        Push push{};std::memcpy(push.reproject,reproject,sizeof push.reproject);push.size[0]=float(width);push.size[1]=float(height);
        push.objectScale=frames;push.ySign=ySign;push.check=checking?1u:keepObject?2u:0u;
        d.vkCmdPushConstants(command,d.pipelineLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof push,&push);
        d.vkCmdDispatch(command,(width+7)/8,(height+7)/8,1);
        VkImageMemoryBarrier done{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};done.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;done.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        done.oldLayout=VK_IMAGE_LAYOUT_GENERAL;done.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;done.srcQueueFamilyIndex=done.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        done.image=o.image;done.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        d.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&done);
        if(checking){
            auto& c=d.checks[d.checkNext++%checkRing];
            if(!c.buffer||c.width!=width||c.height!=height){
                VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=VkDeviceSize(width)*height*4;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                check(d.vkCreateBuffer(d.handle,&bi,nullptr,&c.buffer),"eye motion check buffer");VkMemoryRequirements req{};d.vkGetBufferMemoryRequirements(d.handle,c.buffer,&req);
                VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=d.memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                VkDeviceMemory m{};check(d.vkAllocateMemory(d.handle,&a,nullptr,&m),"eye motion check memory");check(d.vkBindBufferMemory(d.handle,c.buffer,m,0),"eye motion check bind");
                check(d.vkMapMemory(d.handle,m,0,VK_WHOLE_SIZE,0,&c.mapped),"eye motion check map");c.width=width;c.height=height;
            }
            VkImageMemoryBarrier copy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};copy.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;copy.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            copy.oldLayout=VK_IMAGE_LAYOUT_GENERAL;copy.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;copy.srcQueueFamilyIndex=copy.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            copy.image=o.residual;copy.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            d.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&copy);
            VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};
            d.vkCmdCopyImageToBuffer(command,o.residual,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.buffer,1,&region);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            d.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
            c.call=calls;c.pending=true;
        }
        if(keepObject)objectMotion[eye&1]={o.residual,width,height,true};   // left in GENERAL; the copy waits for this queue
        *outImage=o.image;*outView=o.view;
        if(calls<=3||calls%3600==0)note("afw_motion_fix","\"passes\":"+std::to_string(calls)+",\"eye\":"+std::to_string(eye)+",\"frames\":"+std::to_string(frames)+",\"size\":["+std::to_string(width)+","+std::to_string(height)+"],\"input_layout\":"+std::to_string(inputLayout));
        return 0;
    }catch(const std::exception& e){if(eyeMotion::failures++<5)note("afw_motion_fix_failed",std::string("\"reason\":\"")+e.what()+"\"");return 1;}
}
