#pragma once
#include <gi_reproject_capture.h>
#include <gi_reproject_apply.h>
#include <fog_reproject.h>
// One indirect light for both eyes (2026-10-17; headset reports 3/5/16/20 and the user: "in many places the darkness of
// the shadows differs between the eyes, or a shadow is missing in one").
// Each eye's View runs the game's dynamic GI on its own; the two converge on their own, so a rock face under an
// overhang was dark in one eye's indirect light and bright in the other's (RenderDoc gq, game-20261017-111411: of the
// deferred lighting's inputs the sun's shadow mask and the shadow volume are alike in both eyes, the irradiance of
// binding 32 is not - 27 % of 8x8 blocks more than 20 % apart). The indirect light is diffuse, so one eye's value at a
// point of the world is right for the other eye as well.
// The deferred lighting of 1.0 is one compute dispatch a View (shader 50177f15, 74292 bytes; set 2: binding 24 its
// constants - c2.zw the pixel scale, c7..c10 world-to-clip, c11..c14 clip-to-camera-relative, c15 the camera -,
// binding 32 the irradiance (an R32_UINT image read through an R9G9B9E5 view), binding 33 the depth). Here, just
// before the first such dispatch of a pair, that eye's depth, irradiance words and world-to-clip matrix are copied
// into the layer's own images and buffer (shaders/gi_reproject_capture.comp); just before the next dispatch of the
// same extent in the same frame (the other eye), every pixel of that eye the first one also saw takes the first eye's
// irradiance word (shaders/gi_reproject_apply.comp, the depth must agree within 3 %; a hidden point takes the same
// surface just past the nearer thing's edge); the edges outside the first eye's picture and the sky keep the eye's own. The game's compute pipeline and descriptor
// sets are bound again afterwards. The second eye's image then also carries the shared light into its own history.
// EVR_GI_REPROJECT=0: off. EVR_GI_REPROJECT_TOLERANCE=<fraction>: the depth agreement (0.03). EVR_GI_REPROJECT_MARK=1:
// a test - the taken pixels are painted red in the second eye's indirect light. EVR_GI_REPROJECT_HIDDEN=stretch: a point
// hidden from the first eye takes the surface past the near thing's edge (before 2026-10-18; the default keeps its own).
namespace giReproject {
constexpr uint32_t lightingBytes=74292;
constexpr const char* lightingSha="50177f15315cb95c82320a0c8fce5a66daec9652786f12601cac48e1355ff537";
constexpr uint32_t gameSet=2,uboBinding=24,irradianceBinding=32,depthBinding=33,ring=16;
inline bool enabled(){static const bool yes=[]{const char* v=std::getenv("EVR_GI_REPROJECT");return !(v&&!std::strcmp(v,"0"));}();return yes;}
inline float tolerance(){static const float t=[]{const char* v=std::getenv("EVR_GI_REPROJECT_TOLERANCE");const float f=v?float(std::atof(v)):0.f;return f>0.f&&f<1.f?f:0.03f;}();return t;}
inline uint32_t markMode(){static const uint32_t m=[]{const char* v=std::getenv("EVR_GI_REPROJECT_MARK");return v&&!std::strcmp(v,"1")?1u:0u;}();return m;}
// What the second eye shows where the first eye looked at something nearer (shaders/gi_reproject_apply.comp hidden):
// 0 its own light (the default since 2026-10-18), 1 "stretch": the surface just past the near thing's edge (before).
inline uint32_t hiddenMode(){static const uint32_t m=[]{const char* v=std::getenv("EVR_GI_REPROJECT_HIDDEN");return v&&!std::strcmp(v,"stretch")?1u:0u;}();return m;}
struct SetRecord {VkImageView irradiance{},depth{};VkImageLayout irradianceLayout{},depthLayout{};VkBuffer ubo{};VkDeviceSize uboOffset=0,uboRange=0;VkDescriptorType uboType=VK_DESCRIPTOR_TYPE_MAX_ENUM;};
struct BindCall {VkPipelineLayout layout{};uint32_t first=0;std::vector<VkDescriptorSet> sets;std::vector<uint32_t> offsets;};
struct Cmd {VkPipeline compute{};std::vector<BindCall> binds;};
#define GI_REPROJECT_API(X) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory) X(vkBindImageMemory) \
 X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkCreateShaderModule) \
 X(vkDestroyShaderModule) X(vkCreateDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkCreateComputePipelines) X(vkCreateDescriptorPool) \
 X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCmdPipelineBarrier) X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) X(vkCmdDispatch) \
 X(vkCmdFillBuffer) X(vkCreateSampler) X(vkDestroySampler) \
 X(vkDestroyPipeline) X(vkDestroyPipelineLayout) X(vkDestroyDescriptorSetLayout) X(vkDestroyDescriptorPool) X(vkDestroyBuffer) X(vkFreeMemory)
struct Device {
    VkDevice handle{};VkPhysicalDeviceMemoryProperties memory{};bool failed=false,ready=false;
#define GI_DECL(n) PFN_##n n=nullptr;
    GI_REPROJECT_API(GI_DECL)
#undef GI_DECL
    VkDescriptorSetLayout setLayouts[2]{};VkPipelineLayout pipelineLayouts[2]{};VkPipeline pipelines[2]{};VkDescriptorPool pool{};
    std::array<VkDescriptorSet,ring> sets[2]{};uint32_t next[2]{};
    VkImage keptDepth{},keptIrradiance{};VkImageView keptDepthView{},keptIrradianceView{};uint32_t keptWidth=0,keptHeight=0;bool keptFresh=false;
    std::vector<std::pair<VkImage,VkImageView>> retired;   // kept images outgrown; never destroyed while a frame may use them
    VkBuffer keptBuffer{};
    std::unordered_map<VkImage,VkImageView> uintViews;
    std::vector<VkDeviceMemory> memories;   // every allocation of the layer's own, freed with the device
    // the fog pass (fogPrepare): its pipeline, a ring of sets, a linear sampler, and the layer's views of the game's volumes
    bool fogReady=false;VkDescriptorSetLayout fogSetLayout{};VkPipelineLayout fogPipelineLayout{};VkPipeline fogPipeline{};VkDescriptorPool fogPool{};
    std::array<VkDescriptorSet,ring> fogSets{};uint32_t fogNext=0;VkSampler fogSampler{};
    std::unordered_map<VkImage,VkImageView> fogSampledViews,fogStorageViews;
    uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags){
        for(uint32_t i=0;i<memory.memoryTypeCount;i++)if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("GI reprojection memory type");
    }
};
inline std::mutex giMutex;
inline std::unordered_set<VkShaderModule> lightingModules;inline std::unordered_set<VkPipeline> lightingPipelines;
inline std::unordered_map<VkDescriptorSet,SetRecord> setRecords;inline std::unordered_map<VkCommandBuffer,Cmd> commands;
inline std::unordered_map<VkDevice,std::shared_ptr<Device>> giDevices;
// The fog (2026-10-17 evening; user: "(b) reproject with the depth, as the GI"; shaders/fog_reproject.comp): the game's fog
// composite (shader af60aa16, 26088 bytes; DeleteMe/20261017/rd-hq-fog/fog6596.hlsl) reads set 0: binding 0 its constants
// (1392 bytes), 10 and 11 the eye's two fog volumes (3D, the picture / 12 across, 64 deep, R9G9B9E5 through views of
// R32_UINT images). Each eye builds them from its own previous frame (RenderDoc hq: 6554 -> 6558/6562, right 10342 ->
// 10346/10350), so they drift apart; the night sky came out 7 % redder in the left eye. Just before the second eye's
// composite every cell of its volumes takes the first eye's volumes at the same point of the world (the GI pass's kept
// first-eye matrix and this eye's lighting constants, recorded when the GI was taken). EVR_FOG_REPROJECT=0: off.
constexpr uint32_t fogBytes=26088,fogSet=0,fogUboBinding=0,fogScatterBinding=10,fogThroughBinding=11;
constexpr const char* fogSha="af60aa16a14645ef23a714a6589fd2c759b0408971d49193c0fbfb759924ffcb";
inline bool fogEnabled(){static const bool yes=[]{const char* v=std::getenv("EVR_FOG_REPROJECT");return enabled()&&!(v&&!std::strcmp(v,"0"));}();return yes;}
struct FogRecord {VkImageView scatter{},through{};VkImageLayout scatterLayout{},throughLayout{};VkBuffer ubo{};VkDeviceSize uboOffset=0,uboRange=0;VkDescriptorType uboType=VK_DESCRIPTOR_TYPE_MAX_ENUM;};
inline std::unordered_set<VkShaderModule> fogModules;inline std::unordered_set<VkPipeline> fogPipelines;
inline std::unordered_map<VkDescriptorSet,FogRecord> fogRecords;
struct FogPending {bool valid=false;VkImage scatter{},through{};VkImageLayout scatterLayout{},throughLayout{};VkExtent3D extent{};uint64_t present=0,tracked=0;};
inline FogPending fogPending;inline uint64_t fogDispatches=0,fogFirsts=0,fogApplies=0,fogSkips=0,fogTracked=0;
// the second eye's lighting constants, recorded when its GI was taken (the fog pass of the same frame uses them)
struct Applied {bool valid=false;uint64_t present=0;VkBuffer ubo{};VkDeviceSize offset=0,range=0;};
inline Applied lastApply;
struct Pending {bool valid=false;VkImage irradiance{};uint32_t width=0,height=0;uint64_t dispatch=0,present=0,tracked=0;};
inline Pending pending;inline uint64_t lightingDispatches=0,captures=0,applies=0,skips=0,tracked=0;
// The first 6 of each kind, then one in 1800 of the captures and applies (the counts are in every line).
inline std::map<std::string,uint64_t> said;
inline void say(const std::string& event,const std::string& fields){const auto n=++said[event];if(n<=6||n%1800==0||event=="gi_reproject_failed")note(event.c_str(),fields);}
inline std::shared_ptr<Device> deviceFor(VkCommandBuffer command){
    DeviceDispatch dispatch;
    {std::lock_guard<std::mutex> lock(mutex);auto d=devices.find(key(command));if(d==devices.end())throw std::runtime_error("GI reprojection device");dispatch=d->second;}
    auto found=giDevices.find(dispatch.device);if(found!=giDevices.end())return found->second;
    auto r=std::make_shared<Device>();r->handle=dispatch.device;
    auto getMemory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(dispatch.gipa(dispatch.instance,"vkGetPhysicalDeviceMemoryProperties"));
    if(!getMemory)throw std::runtime_error("GI reprojection memory properties");getMemory(dispatch.physical,&r->memory);
#define GI_LOAD(n) r->n=reinterpret_cast<PFN_##n>(dispatch.gdpa(dispatch.device,#n));if(!r->n)throw std::runtime_error(#n);
    GI_REPROJECT_API(GI_LOAD)
#undef GI_LOAD
    giDevices.emplace(dispatch.device,r);return r;
}
inline void check(VkResult r,const char* what){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(what)+" "+std::to_string(r));}
// The two pipelines: set 0 = the bindings of shaders/gi_reproject_*.comp.
inline void prepare(Device& d){
    if(d.ready)return;
    const VkDescriptorType capture[6]{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
    const uint32_t applyBindings[6]{0,2,3,4,5,6};
    const VkDescriptorType apply[6]{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
    for(int pass=0;pass<2;++pass){
        std::array<VkDescriptorSetLayoutBinding,6> b{};
        for(uint32_t i=0;i<6;i++){b[i].binding=pass?applyBindings[i]:i;b[i].descriptorType=pass?apply[i]:capture[i];b[i].descriptorCount=1;b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=6;li.pBindings=b.data();
        check(d.vkCreateDescriptorSetLayout(d.handle,&li,nullptr,&d.setLayouts[pass]),"GI set layout");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&d.setLayouts[pass];
        check(d.vkCreatePipelineLayout(d.handle,&pl,nullptr,&d.pipelineLayouts[pass]),"GI pipeline layout");
        VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        si.pCode=pass?giReprojectApply:giReprojectCapture;si.codeSize=pass?sizeof(giReprojectApply):sizeof(giReprojectCapture);
        VkShaderModule module{};check(d.vkCreateShaderModule(d.handle,&si,nullptr,&module),"GI shader");
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";ci.layout=d.pipelineLayouts[pass];
        // the apply pass's settings as specialization constants (no push constants: they would disturb the game's)
        const struct {float tolerance;uint32_t mode;uint32_t hidden;} settings{tolerance(),markMode(),hiddenMode()};
        const VkSpecializationMapEntry entries[3]{{0,0,4},{1,4,4},{2,8,4}};VkSpecializationInfo special{3,entries,sizeof settings,&settings};
        if(pass)ci.stage.pSpecializationInfo=&special;
        const auto r=d.vkCreateComputePipelines(d.handle,VK_NULL_HANDLE,1,&ci,nullptr,&d.pipelines[pass]);d.vkDestroyShaderModule(d.handle,module,nullptr);check(r,"GI pipeline");
    }
    const VkDescriptorPoolSize sizes[4]{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,ring*3},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,ring*2},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,ring*5},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,ring*2}};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=ring*2;pi.poolSizeCount=4;pi.pPoolSizes=sizes;
    check(d.vkCreateDescriptorPool(d.handle,&pi,nullptr,&d.pool),"GI descriptor pool");
    for(int pass=0;pass<2;++pass){std::array<VkDescriptorSetLayout,ring> layouts;layouts.fill(d.setLayouts[pass]);
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=d.pool;ai.descriptorSetCount=ring;ai.pSetLayouts=layouts.data();
        check(d.vkAllocateDescriptorSets(d.handle,&ai,d.sets[pass].data()),"GI descriptor sets");}
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=256;bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    check(d.vkCreateBuffer(d.handle,&bi,nullptr,&d.keptBuffer),"GI buffer");VkMemoryRequirements req{};d.vkGetBufferMemoryRequirements(d.handle,d.keptBuffer,&req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=d.memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory m{};check(d.vkAllocateMemory(d.handle,&a,nullptr,&m),"GI buffer memory");d.memories.push_back(m);check(d.vkBindBufferMemory(d.handle,d.keptBuffer,m,0),"GI buffer bind");
    d.ready=true;
}
// The fog pass: set 0 = the bindings of shaders/fog_reproject.comp.
inline void fogPrepare(Device& d){
    if(d.fogReady)return;
    const VkDescriptorType types[9]{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
    std::array<VkDescriptorSetLayoutBinding,9> b{};
    for(uint32_t i=0;i<9;i++){b[i].binding=i;b[i].descriptorType=types[i];b[i].descriptorCount=1;b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=9;li.pBindings=b.data();
    check(d.vkCreateDescriptorSetLayout(d.handle,&li,nullptr,&d.fogSetLayout),"fog set layout");
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&d.fogSetLayout;
    check(d.vkCreatePipelineLayout(d.handle,&pl,nullptr,&d.fogPipelineLayout),"fog pipeline layout");
    VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.pCode=fogReproject;si.codeSize=sizeof(fogReproject);
    VkShaderModule module{};check(d.vkCreateShaderModule(d.handle,&si,nullptr,&module),"fog shader");
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";ci.layout=d.fogPipelineLayout;
    // shaders/fog_reproject.comp keepOwn (constant 0): EVR_FOG_KEEP_OWN=1 keeps this eye's own fog behind the first eye's
    // surfaces and outside its view (until 2026-10-18 15:30); by default the first eye's fog is taken everywhere.
    // behindSearch (constant 1, 2026-10-18 18:40): how many cells beside a cell the first eye cannot see are searched for
    // its fog at the same depth (the clear sword-shaped patch in the right eye's sky); EVR_FOG_BEHIND_SEARCH=<0..64>, 0 = 15:40.
    static const std::array<uint32_t,2> fogSpec=[]{const char* v=std::getenv("EVR_FOG_KEEP_OWN");const char* b=std::getenv("EVR_FOG_BEHIND_SEARCH");
        const int n=b?std::atoi(b):16;return std::array<uint32_t,2>{uint32_t(v&&!std::strcmp(v,"1")?1:0),uint32_t(n<0?0:n>64?64:n)};}();
    const uint32_t keepOwn=fogSpec[0];
    const VkSpecializationMapEntry fogEntries[2]{{0,0,sizeof(uint32_t)},{1,sizeof(uint32_t),sizeof(uint32_t)}};
    VkSpecializationInfo spec{2,fogEntries,sizeof fogSpec,fogSpec.data()};
    ci.stage.pSpecializationInfo=&spec;
    const auto r=d.vkCreateComputePipelines(d.handle,VK_NULL_HANDLE,1,&ci,nullptr,&d.fogPipeline);d.vkDestroyShaderModule(d.handle,module,nullptr);check(r,"fog pipeline");
    const VkDescriptorPoolSize sizes[5]{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,ring*2},{VK_DESCRIPTOR_TYPE_SAMPLER,ring},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,ring*3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,ring*2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,ring}};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=ring;pi.poolSizeCount=5;pi.pPoolSizes=sizes;
    check(d.vkCreateDescriptorPool(d.handle,&pi,nullptr,&d.fogPool),"fog descriptor pool");
    std::array<VkDescriptorSetLayout,ring> layouts;layouts.fill(d.fogSetLayout);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=d.fogPool;ai.descriptorSetCount=ring;ai.pSetLayouts=layouts.data();
    check(d.vkAllocateDescriptorSets(d.handle,&ai,d.fogSets.data()),"fog descriptor sets");
    VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sc.magFilter=sc.minFilter=VK_FILTER_LINEAR;sc.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sc.addressModeU=sc.addressModeV=sc.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sc.maxLod=0.f;
    check(d.vkCreateSampler(d.handle,&sc,nullptr,&d.fogSampler),"fog sampler");
    d.fogReady=true;
    say("fog_reproject_mode",std::string("\"keep_own\":")+(keepOwn?"true":"false")+",\"behind_search\":"+std::to_string(fogSpec[1]));
}
// The layer's views of the game's fog volumes: R9G9B9E5 to sample the first eye's, R32_UINT to write the second eye's.
inline VkImageView fogView(Device& d,std::unordered_map<VkImage,VkImageView>& cache,VkImage image,VkFormat format){
    auto found=cache.find(image);if(found!=cache.end())return found->second;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_3D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    // the images are storage images; an R9G9B9E5 view can only be sampled (validation VUID-VkImageViewCreateInfo-usage-02275)
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage=format==VK_FORMAT_R32_UINT?VK_IMAGE_USAGE_STORAGE_BIT:VK_IMAGE_USAGE_SAMPLED_BIT;vi.pNext=&usage;
    VkImageView view{};check(d.vkCreateImageView(d.handle,&vi,nullptr,&view),"fog view");
    if(cache.size()>32)throw std::runtime_error("fog views beyond the budget");
    cache.emplace(image,view);return view;
}
inline VkImage image2d(Device& d,VkFormat format,uint32_t w,uint32_t h,VkImageView& view){
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={w,h,1};ci.mipLevels=ci.arrayLayers=1;
    ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_STORAGE_BIT;
    VkImage image{};check(d.vkCreateImage(d.handle,&ci,nullptr,&image),"GI kept image");VkMemoryRequirements req{};d.vkGetImageMemoryRequirements(d.handle,image,&req);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=req.size;a.memoryTypeIndex=d.memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory m{};check(d.vkAllocateMemory(d.handle,&a,nullptr,&m),"GI kept memory");d.memories.push_back(m);check(d.vkBindImageMemory(d.handle,image,m,0),"GI kept bind");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    check(d.vkCreateImageView(d.handle,&vi,nullptr,&view),"GI kept view");return image;
}
inline void keep(Device& d,uint32_t w,uint32_t h){
    if(w<=d.keptWidth&&h<=d.keptHeight)return;
    if(d.keptDepth){d.retired.push_back({d.keptDepth,d.keptDepthView});d.retired.push_back({d.keptIrradiance,d.keptIrradianceView});}
    w=std::max(w,d.keptWidth);h=std::max(h,d.keptHeight);
    d.keptDepth=image2d(d,VK_FORMAT_R32_SFLOAT,w,h,d.keptDepthView);d.keptIrradiance=image2d(d,VK_FORMAT_R32_UINT,w,h,d.keptIrradianceView);
    d.keptWidth=w;d.keptHeight=h;d.keptFresh=true;
}
// The layer's R32_UINT view of the game's irradiance image (made R32_UINT, read by the game through another view).
inline VkImageView uintView(Device& d,VkImage image,const VkImageSubresourceRange& range){
    auto found=d.uintViews.find(image);if(found!=d.uintViews.end())return found->second;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=VK_FORMAT_R32_UINT;
    vi.subresourceRange=range;vi.subresourceRange.levelCount=1;vi.subresourceRange.layerCount=1;
    VkImageView view{};check(d.vkCreateImageView(d.handle,&vi,nullptr,&view),"GI irradiance view");
    if(d.uintViews.size()>64)throw std::runtime_error("GI irradiance views beyond the budget");
    d.uintViews.emplace(image,view);return view;
}
inline void barrier(Device& d,VkCommandBuffer c){
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    d.vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&b,0,nullptr,0,nullptr);
}
inline void layout(Device& d,VkCommandBuffer c,VkImage image,const VkImageSubresourceRange& range,VkImageLayout from,VkImageLayout to){
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=from;b.newLayout=to;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image;b.subresourceRange=range;b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    d.vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);
}
// The game's compute state as it was: its pipeline and every descriptor set binding since, in their order.
inline void restore(Device& d,VkCommandBuffer c,const Cmd& cmd){
    d.vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_COMPUTE,cmd.compute);
    for(const auto& b:cmd.binds)d.vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_COMPUTE,b.layout,b.first,uint32_t(b.sets.size()),b.sets.data(),uint32_t(b.offsets.size()),b.offsets.data());
}
// The fog composite's dispatch (giMutex held): the first of an extent in a frame is the first eye's (remembered), the next
// one the second eye's (its volumes take the first eye's, then the game's state is put back).
inline void fogDispatch(VkCommandBuffer c,const Cmd& cmd){
    const auto n=++fogDispatches;
    VkDescriptorSet set{};for(const auto& b:cmd.binds)if(fogSet>=b.first&&fogSet<b.first+b.sets.size())set=b.sets[fogSet-b.first];
    auto rec=fogRecords.find(set);
    if(!set||rec==fogRecords.end()||!rec->second.scatter||!rec->second.through||!rec->second.ubo){
        const auto k=++fogSkips;if(k<=6||k%1800==0)note("fog_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"skips\":"+std::to_string(k)+",\"reason\":\"composite set 0 not tracked\",\"found\":"+(rec==fogRecords.end()?"false":"true"));
        return;}
    const auto r=rec->second;
    if(r.uboType!=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER||r.uboRange<1392){++fogSkips;say("fog_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"reason\":\"constants not a plain uniform buffer\",\"range\":"+std::to_string(r.uboRange));return;}
    ImageViewRecord a{},b{};VkImageCreateInfo ai{},bi{};
    {std::lock_guard<std::mutex> l(mutex);auto views=imageViews.find(key(c));if(views==imageViews.end())return;
        auto x=views->second.find(r.scatter),y=views->second.find(r.through);if(x==views->second.end()||y==views->second.end())return;a=x->second;b=y->second;
        auto created=imageCreation.find(key(c));if(created==imageCreation.end())return;
        auto ia=created->second.find(a.image),ib=created->second.find(b.image);if(ia==created->second.end()||ib==created->second.end())return;ai=ia->second;bi=ib->second;}
    auto same=[](const VkExtent3D& x,const VkExtent3D& y){return x.width==y.width&&x.height==y.height&&x.depth==y.depth;};
    if(ai.imageType!=VK_IMAGE_TYPE_3D||bi.imageType!=VK_IMAGE_TYPE_3D||!same(ai.extent,bi.extent)||!(ai.usage&VK_IMAGE_USAGE_STORAGE_BIT)||!(bi.usage&VK_IMAGE_USAGE_STORAGE_BIT)||
       ai.format!=VK_FORMAT_R32_UINT||bi.format!=VK_FORMAT_R32_UINT||!(ai.flags&VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)||!(bi.flags&VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)){++fogSkips;
        say("fog_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"reason\":\"volumes differ\",\"type\":"+std::to_string(ai.imageType)+",\"format\":"+std::to_string(ai.format)+",\"usage\":"+std::to_string(ai.usage)+
            ",\"flags\":"+std::to_string(ai.flags)+",\"extent\":["+std::to_string(ai.extent.width)+","+std::to_string(ai.extent.height)+","+std::to_string(ai.extent.depth)+"]");return;}
    const uint64_t present=presents.load(),index=++fogTracked;
    const bool second=fogPending.valid&&same(fogPending.extent,ai.extent)&&
        (fogPending.present==present||(fogPending.present+1==present&&fogPending.tracked+1==index&&fogPending.scatter!=a.image));
    if(!second){fogPending={true,a.image,b.image,r.scatterLayout,r.throughLayout,ai.extent,present,index};++fogFirsts;
        say("fog_reproject_first","\"dispatch\":"+std::to_string(n)+",\"scatter\":"+handleText(a.image)+",\"through\":"+handleText(b.image)+",\"extent\":["+std::to_string(ai.extent.width)+","+
            std::to_string(ai.extent.height)+","+std::to_string(ai.extent.depth)+"],\"layouts\":["+std::to_string(r.scatterLayout)+","+std::to_string(r.throughLayout)+"],\"firsts\":"+std::to_string(fogFirsts));
        return;}
    const auto first=fogPending;fogPending={};
    if(!lastApply.valid||(lastApply.present!=present&&lastApply.present+1!=present)){++fogSkips;
        say("fog_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"reason\":\"no GI pass in this frame\",\"present\":"+std::to_string(present)+",\"gi_present\":"+std::to_string(lastApply.present));return;}
    std::shared_ptr<Device> dev;
    try{
        dev=deviceFor(c);auto& d=*dev;if(d.failed||!d.ready||!d.keptBuffer)return;
        fogPrepare(d);
        const VkImageView firstScatter=fogView(d,d.fogSampledViews,first.scatter,VK_FORMAT_E5B9G9R9_UFLOAT_PACK32),firstThrough=fogView(d,d.fogSampledViews,first.through,VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
        const VkImageView scatter=fogView(d,d.fogStorageViews,a.image,VK_FORMAT_R32_UINT),through=fogView(d,d.fogStorageViews,b.image,VK_FORMAT_R32_UINT);
        const auto s=d.fogSets[d.fogNext++%ring];
        if(!d.keptDepthView)return;
        VkDescriptorImageInfo images[6]{{VK_NULL_HANDLE,firstScatter,first.scatterLayout},{VK_NULL_HANDLE,firstThrough,first.throughLayout},{d.fogSampler,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED},
            {VK_NULL_HANDLE,scatter,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,through,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,d.keptDepthView,VK_IMAGE_LAYOUT_GENERAL}};
        VkDescriptorBufferInfo lighting{lastApply.ubo,lastApply.offset,std::min<VkDeviceSize>(lastApply.range,2048)},kept{d.keptBuffer,0,256},fogConstants{r.ubo,r.uboOffset,std::min<VkDeviceSize>(r.uboRange,1392)};
        std::array<VkWriteDescriptorSet,9> wr{};
        const VkDescriptorType types[9]{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
        for(uint32_t i=0;i<9;i++){auto& x=wr[i];x.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;x.dstSet=s;x.dstBinding=i;x.descriptorCount=1;x.descriptorType=types[i];
            if(i<5)x.pImageInfo=&images[i];else if(i==8)x.pImageInfo=&images[5];else x.pBufferInfo=i==5?&lighting:i==6?&kept:&fogConstants;}
        d.vkUpdateDescriptorSets(d.handle,9,wr.data(),0,nullptr);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        // the second eye's volumes: written by its own pass just before, read from here on by compute and fragment shaders
        VkImageMemoryBarrier to[2]{};
        to[0]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,r.scatterLayout,VK_IMAGE_LAYOUT_GENERAL,
               VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,a.image,range};
        to[1]=to[0];to[1].oldLayout=r.throughLayout;to[1].image=b.image;
        d.vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,2,to);
        d.vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_COMPUTE,d.fogPipeline);
        d.vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_COMPUTE,d.fogPipelineLayout,0,1,&s,0,nullptr);
        d.vkCmdDispatch(c,(ai.extent.width+3)/4,(ai.extent.height+3)/4,(ai.extent.depth+3)/4);
        VkImageMemoryBarrier back[2]{};
        back[0]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_IMAGE_LAYOUT_GENERAL,r.scatterLayout,
                 VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,a.image,range};
        back[1]=back[0];back[1].newLayout=r.throughLayout;back[1].image=b.image;
        d.vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,2,back);
        restore(d,c,cmd);
        ++fogApplies;
        say("fog_reproject_apply","\"dispatch\":"+std::to_string(n)+",\"cmd\":"+handleText(c)+",\"first\":["+handleText(first.scatter)+","+handleText(first.through)+"],\"second\":["+handleText(a.image)+","+
            handleText(b.image)+"],\"extent\":["+std::to_string(ai.extent.width)+","+std::to_string(ai.extent.height)+","+std::to_string(ai.extent.depth)+"],\"layouts\":["+std::to_string(r.scatterLayout)+","+
            std::to_string(r.throughLayout)+"],\"applies\":"+std::to_string(fogApplies)+",\"firsts\":"+std::to_string(fogFirsts)+",\"skips\":"+std::to_string(fogSkips));
    }catch(const std::exception& e){if(dev)dev->failed=true;say("gi_reproject_failed","\"reason\":"+jsonString(std::string("fog: ")+e.what()));}
}
inline void failNow(const std::shared_ptr<Device>& d,const std::string& reason){if(d)d->failed=true;say("gi_reproject_failed","\"reason\":"+jsonString(reason)+",\"captures\":"+std::to_string(captures)+",\"applies\":"+std::to_string(applies));}
}
void giReprojectModule(VkShaderModule module,const VkShaderModuleCreateInfo* info) noexcept {
    if(!giReproject::enabled()||!info)return;
    const bool lighting=info->codeSize==giReproject::lightingBytes,fog=giReproject::fogEnabled()&&info->codeSize==giReproject::fogBytes;
    if(!lighting&&!fog)return;
    try{const auto sha=head::sha256(info->pCode,info->codeSize);
        std::lock_guard<std::mutex> lock(giReproject::giMutex);
        if(lighting&&sha==giReproject::lightingSha){giReproject::lightingModules.insert(module);giReproject::say("gi_reproject_lighting_shader","\"module\":"+handleText(module));}
        else if(fog&&sha==giReproject::fogSha){giReproject::fogModules.insert(module);giReproject::say("fog_reproject_shader","\"module\":"+handleText(module));}}catch(...){}
}
void giReprojectPipeline(VkShaderModule module,VkPipeline pipeline) noexcept {
    if(!giReproject::enabled()||!pipeline)return;
    try{std::lock_guard<std::mutex> lock(giReproject::giMutex);
        if(giReproject::fogModules.count(module)){giReproject::fogPipelines.insert(pipeline);giReproject::say("fog_reproject_pipeline","\"pipeline\":"+handleText(pipeline));return;}
        if(!giReproject::lightingModules.count(module))return;giReproject::lightingPipelines.insert(pipeline);
        giReproject::say("gi_reproject_lighting_pipeline","\"pipeline\":"+handleText(pipeline));}catch(...){}
}
// A destroyed module's or pipeline's handle comes back for another one (Mock hn, game-20261017-125809: dispatches of 9x10
// and 256x1 groups on "lighting" pipelines made from a reused module handle), so they leave the sets with it.
void giReprojectModuleDestroyed(VkShaderModule module) noexcept {
    if(!giReproject::enabled())return;
    try{std::lock_guard<std::mutex> lock(giReproject::giMutex);giReproject::lightingModules.erase(module);giReproject::fogModules.erase(module);}catch(...){}
}
void giReprojectPipelineDestroyed(VkPipeline pipeline) noexcept {
    if(!giReproject::enabled())return;
    try{std::lock_guard<std::mutex> lock(giReproject::giMutex);giReproject::lightingPipelines.erase(pipeline);giReproject::fogPipelines.erase(pipeline);}catch(...){}
}
void giReprojectWrites(uint32_t count,const VkWriteDescriptorSet* writes,uint32_t copies,const VkCopyDescriptorSet* copy) noexcept {
    if(!giReproject::enabled())return;
    try{
        using namespace giReproject;std::lock_guard<std::mutex> lock(giMutex);
        if(fogEnabled()&&!fogPipelines.empty()){
            if(fogRecords.size()>300000)fogRecords.clear();
            for(uint32_t i=0;i<count&&writes;i++){const auto& w=writes[i];
                if(w.dstArrayElement||w.descriptorCount!=1||w.descriptorType!=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE||!w.pImageInfo)continue;
                if(w.dstBinding==fogScatterBinding){auto& f=fogRecords[w.dstSet];f.scatter=w.pImageInfo[0].imageView;f.scatterLayout=w.pImageInfo[0].imageLayout;}
                else if(w.dstBinding==fogThroughBinding){auto& f=fogRecords[w.dstSet];f.through=w.pImageInfo[0].imageView;f.throughLayout=w.pImageInfo[0].imageLayout;}}
            for(uint32_t i=0;i<count&&writes;i++){const auto& w=writes[i];
                if(w.dstBinding!=fogUboBinding||w.dstArrayElement||w.descriptorCount!=1||!w.pBufferInfo||
                   (w.descriptorType!=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER&&w.descriptorType!=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC))continue;
                auto f=fogRecords.find(w.dstSet);if(f==fogRecords.end())continue;
                f->second.ubo=w.pBufferInfo[0].buffer;f->second.uboOffset=w.pBufferInfo[0].offset;f->second.uboRange=w.pBufferInfo[0].range;f->second.uboType=w.descriptorType;}
        }
        if(lightingPipelines.empty())return;
        for(uint32_t i=0;i<count&&writes;i++){
            const auto& w=writes[i];
            if(w.dstArrayElement||w.descriptorCount!=1)continue;
            if((w.dstBinding==irradianceBinding||w.dstBinding==depthBinding)&&w.descriptorType==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE&&w.pImageInfo){
                if(setRecords.size()>300000)setRecords.clear();
                auto& r=setRecords[w.dstSet];
                if(w.dstBinding==irradianceBinding){r.irradiance=w.pImageInfo[0].imageView;r.irradianceLayout=w.pImageInfo[0].imageLayout;}
                else{r.depth=w.pImageInfo[0].imageView;r.depthLayout=w.pImageInfo[0].imageLayout;}
            }else if(w.dstBinding==uboBinding&&(w.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER||w.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)&&w.pBufferInfo){
                if(setRecords.size()>300000)setRecords.clear();
                auto& r=setRecords[w.dstSet];r.ubo=w.pBufferInfo[0].buffer;r.uboOffset=w.pBufferInfo[0].offset;r.uboRange=w.pBufferInfo[0].range;r.uboType=w.descriptorType;
            }
        }
        for(uint32_t i=0;i<copies&&copy;i++){
            const auto& c=copy[i];auto from=setRecords.find(c.srcSet);if(from==setRecords.end())continue;
            auto rec=from->second;auto& to=setRecords[c.dstSet];
            for(uint32_t k=0;k<c.descriptorCount;k++){const auto sb=c.srcBinding,db=c.dstBinding;
                if(sb==irradianceBinding&&db==irradianceBinding){to.irradiance=rec.irradiance;to.irradianceLayout=rec.irradianceLayout;}
                if(sb==depthBinding&&db==depthBinding){to.depth=rec.depth;to.depthLayout=rec.depthLayout;}
                if(sb==uboBinding&&db==uboBinding){to.ubo=rec.ubo;to.uboOffset=rec.uboOffset;to.uboRange=rec.uboRange;to.uboType=rec.uboType;}}
        }
    }catch(...){}
}
void giReprojectReset(VkCommandBuffer c) noexcept {
    if(!giReproject::enabled())return;
    try{std::lock_guard<std::mutex> lock(giReproject::giMutex);giReproject::commands.erase(c);}catch(...){}
}
void giReprojectBind(VkCommandBuffer c,VkPipelineBindPoint point,VkPipeline pipeline) noexcept {
    if(!giReproject::enabled()||point!=VK_PIPELINE_BIND_POINT_COMPUTE)return;
    try{std::lock_guard<std::mutex> lock(giReproject::giMutex);if(giReproject::lightingPipelines.empty())return;
        if(giReproject::commands.size()>16384&&!giReproject::commands.count(c))giReproject::commands.clear();
        giReproject::commands[c].compute=pipeline;}catch(...){}
}
void giReprojectSets(VkCommandBuffer c,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets) noexcept {
    if(!giReproject::enabled()||point!=VK_PIPELINE_BIND_POINT_COMPUTE||!sets||!count)return;
    try{
        std::lock_guard<std::mutex> lock(giReproject::giMutex);if(giReproject::lightingPipelines.empty())return;
        auto& cmd=giReproject::commands[c];
        giReproject::BindCall b;b.layout=layout;b.first=first;b.sets.assign(sets,sets+count);if(dynamicCount&&offsets)b.offsets.assign(offsets,offsets+dynamicCount);
        // a call whose sets are all bound again here no longer matters
        cmd.binds.erase(std::remove_if(cmd.binds.begin(),cmd.binds.end(),[&](const giReproject::BindCall& o){return o.first>=first&&o.first+o.sets.size()<=first+count;}),cmd.binds.end());
        if(cmd.binds.size()>=16)cmd.binds.erase(cmd.binds.begin());
        cmd.binds.push_back(std::move(b));
    }catch(...){}
}
void giReprojectForget(VkDevice device,VkImage image) noexcept {
    if(!giReproject::enabled())return;
    try{std::lock_guard<std::mutex> lock(giReproject::giMutex);auto d=giReproject::giDevices.find(device);if(d==giReproject::giDevices.end())return;
        for(auto* cache:{&d->second->uintViews,&d->second->fogSampledViews,&d->second->fogStorageViews}){auto v=cache->find(image);if(v==cache->end())continue;
            d->second->vkDestroyImageView(device,v->second,nullptr);cache->erase(v);}}catch(...){}
}
// Before every compute dispatch: the game's deferred lighting gets the passes above.
void giReprojectDispatch(VkCommandBuffer c,uint32_t gx,uint32_t gy,uint32_t gz) noexcept {
    if(!giReproject::enabled())return;
    using namespace giReproject;
    std::shared_ptr<Device> dev;
    try{
        std::lock_guard<std::mutex> lock(giMutex);
        auto found=commands.find(c);if(found==commands.end())return;
        if(fogEnabled()&&fogPipelines.count(found->second.compute)){fogDispatch(c,found->second);return;}
        if(!lightingPipelines.count(found->second.compute))return;
        const auto& cmd=found->second;const auto n=++lightingDispatches;
        VkDescriptorSet set{};for(const auto& b:cmd.binds)if(gameSet>=b.first&&gameSet<b.first+b.sets.size())set=b.sets[gameSet-b.first];
        auto rec=setRecords.find(set);
        if(!set||rec==setRecords.end()||!rec->second.irradiance||!rec->second.depth||!rec->second.ubo){
            const auto k=++skips;
            if(k<=6||k%1800==0){   // the record is put together only when it is written (this runs for every such dispatch)
                std::string calls;for(const auto& b:cmd.binds)calls+=(calls.empty()?"":",")+std::string("[")+std::to_string(b.first)+","+std::to_string(b.sets.size())+"]";
                note("gi_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"skips\":"+std::to_string(k)+",\"reason\":\"lighting set 2 not tracked\",\"set\":"+handleText(set)+",\"found\":"+(rec==setRecords.end()?"false":"true")+
                    ",\"groups\":["+std::to_string(gx)+","+std::to_string(gy)+","+std::to_string(gz)+"],\"cmd\":"+handleText(c)+",\"pipeline\":"+handleText(cmd.compute)+",\"bind_calls\":["+calls+"]");}
            return;}
        const auto r=rec->second;
        if(r.uboType!=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER||r.uboRange<256){++skips;say("gi_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"reason\":\"constants not a plain uniform buffer\",\"type\":"+std::to_string(r.uboType)+",\"range\":"+std::to_string(r.uboRange));return;}
        ImageViewRecord irr{},depth{};VkImageCreateInfo irrInfo{},depthInfo{};
        {std::lock_guard<std::mutex> l(mutex);auto views=imageViews.find(key(c));if(views==imageViews.end())return;
            auto a=views->second.find(r.irradiance),b=views->second.find(r.depth);if(a==views->second.end()||b==views->second.end())return;irr=a->second;depth=b->second;
            auto created=imageCreation.find(key(c));if(created==imageCreation.end())return;
            auto ia=created->second.find(irr.image),ib=created->second.find(depth.image);if(ia==created->second.end()||ib==created->second.end())return;irrInfo=ia->second;depthInfo=ib->second;}
        const uint32_t w=depthInfo.extent.width,h=depthInfo.extent.height;
        if(irrInfo.extent.width!=w||irrInfo.extent.height!=h||!(irrInfo.usage&VK_IMAGE_USAGE_STORAGE_BIT)||!(irrInfo.usage&VK_IMAGE_USAGE_SAMPLED_BIT)||
           (irrInfo.format!=VK_FORMAT_R32_UINT&&!(irrInfo.flags&VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT))){++skips;
            say("gi_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"reason\":\"irradiance image differs\",\"format\":"+std::to_string(irrInfo.format)+",\"usage\":"+std::to_string(irrInfo.usage)+
                ",\"extent\":["+std::to_string(irrInfo.extent.width)+","+std::to_string(irrInfo.extent.height)+"],\"depth\":["+std::to_string(w)+","+std::to_string(h)+"]");return;}
        // the lighting's own dispatch covers its picture in groups of 8x8; anything else is not this pass
        if(gx!=(w+7)/8||gy!=(h+7)/8||gz!=1){++skips;say("gi_reproject_skipped","\"dispatch\":"+std::to_string(n)+",\"reason\":\"groups do not cover the picture\",\"groups\":["+std::to_string(gx)+","+std::to_string(gy)+","+std::to_string(gz)+"],\"extent\":["+std::to_string(w)+","+std::to_string(h)+"]");return;}
        dev=deviceFor(c);auto& d=*dev;if(d.failed)return;
        prepare(d);
        // The images are not the eye's own: the frame graph hands the same VkImage to one View after another (Mock gw,
        // game-20261017-114623: one irradiance image and two depth images for the desktop View and both eyes). So the
        // pairs are taken by order: of the dispatches of one extent between two presents, the first takes the copy and
        // the next one uses it (a single View a frame - the desktop, a loading screen - never pairs with its next frame).
        // A present between the two eyes (not seen in the Mock; a guard for the headset): the very previous lighting
        // dispatch with another irradiance image still pairs. One View a frame (the desktop, loading) keeps one image.
        const uint64_t present=presents.load(),index=++tracked;
        const bool apply=pending.valid&&pending.width==w&&pending.height==h&&
            (pending.present==present||(pending.present+1==present&&pending.tracked+1==index&&pending.irradiance!=irr.image));
        // the decisions of 24 dispatches in a row every 2000 (which frames take the first eye's light)
        if(n%3000<12)note("gi_reproject_trace","\"dispatch\":"+std::to_string(n)+",\"apply\":"+(apply?"true":"false")+",\"cmd\":"+handleText(c)+",\"thread\":"+std::to_string(GetCurrentThreadId())+
            ",\"irradiance\":"+handleText(irr.image)+",\"depth\":"+handleText(depth.image)+",\"extent\":["+std::to_string(w)+","+std::to_string(h)+"],\"pending\":"+(pending.valid?"true":"false")+
            ",\"pending_irradiance\":"+handleText(pending.irradiance)+",\"pending_extent\":["+std::to_string(pending.width)+","+std::to_string(pending.height)+"],\"pending_dispatch\":"+std::to_string(pending.dispatch)+",\"presents\":"+std::to_string(presents.load()));
        VkDescriptorImageInfo images[5]{};VkDescriptorBufferInfo ubo{r.ubo,r.uboOffset,std::min<VkDeviceSize>(r.uboRange,2048)},kept{d.keptBuffer,0,256};
        std::array<VkWriteDescriptorSet,7> wr{};uint32_t k=0;
        auto put=[&](uint32_t binding,VkDescriptorType type,const VkDescriptorImageInfo* image,const VkDescriptorBufferInfo* buffer,VkDescriptorSet s){
            auto& x=wr[k++];x.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;x.dstSet=s;x.dstBinding=binding;x.descriptorCount=1;x.descriptorType=type;x.pImageInfo=image;x.pBufferInfo=buffer;};
        const int pass=apply?1:0;const auto s=d.sets[pass][d.next[pass]++%ring];
        if(!apply){
            keep(d,w,h);
            images[0]={VK_NULL_HANDLE,r.depth,r.depthLayout};images[1]={VK_NULL_HANDLE,uintView(d,irr.image,irr.range),r.irradianceLayout};
            images[2]={VK_NULL_HANDLE,d.keptDepthView,VK_IMAGE_LAYOUT_GENERAL};images[3]={VK_NULL_HANDLE,d.keptIrradianceView,VK_IMAGE_LAYOUT_GENERAL};
            put(0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,&images[0],nullptr,s);put(1,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,&images[1],nullptr,s);put(2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,nullptr,&ubo,s);
            put(3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&images[2],nullptr,s);put(4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&images[3],nullptr,s);put(5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&kept,s);
        }else{
            images[0]={VK_NULL_HANDLE,r.depth,r.depthLayout};
            images[2]={VK_NULL_HANDLE,d.keptDepthView,VK_IMAGE_LAYOUT_GENERAL};images[3]={VK_NULL_HANDLE,d.keptIrradianceView,VK_IMAGE_LAYOUT_GENERAL};
            images[4]={VK_NULL_HANDLE,uintView(d,irr.image,irr.range),VK_IMAGE_LAYOUT_GENERAL};
            put(0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,&images[0],nullptr,s);put(2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,nullptr,&ubo,s);
            put(3,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&images[2],nullptr,s);put(4,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&images[3],nullptr,s);
            put(5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&kept,s);put(6,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&images[4],nullptr,s);
        }
        d.vkUpdateDescriptorSets(d.handle,k,wr.data(),0,nullptr);
        barrier(d,c);
        if(d.keptFresh){const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            layout(d,c,d.keptDepth,all,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);layout(d,c,d.keptIrradiance,all,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);d.keptFresh=false;}
        VkImageSubresourceRange range=irr.range;range.levelCount=1;range.layerCount=1;
        if(apply&&r.irradianceLayout!=VK_IMAGE_LAYOUT_GENERAL)layout(d,c,irr.image,range,r.irradianceLayout,VK_IMAGE_LAYOUT_GENERAL);
        d.vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_COMPUTE,d.pipelines[pass]);
        d.vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_COMPUTE,d.pipelineLayouts[pass],0,1,&s,0,nullptr);
        d.vkCmdDispatch(c,(w+7)/8,(h+7)/8,1);
        if(apply&&r.irradianceLayout!=VK_IMAGE_LAYOUT_GENERAL)layout(d,c,irr.image,range,VK_IMAGE_LAYOUT_GENERAL,r.irradianceLayout);
        barrier(d,c);
        restore(d,c,cmd);
        if(apply){pending={};++applies;lastApply={true,present,r.ubo,r.uboOffset,r.uboRange};}else{pending={true,irr.image,w,h,n,present,index};++captures;}
        say(apply?"gi_reproject_apply":"gi_reproject_capture","\"dispatch\":"+std::to_string(n)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"cmd\":"+handleText(c)+
            ",\"irradiance\":"+handleText(irr.image)+",\"depth\":"+handleText(depth.image)+",\"extent\":["+std::to_string(w)+","+std::to_string(h)+"],\"irradiance_layout\":"+std::to_string(r.irradianceLayout)+
            ",\"depth_layout\":"+std::to_string(r.depthLayout)+",\"ubo_offset\":"+std::to_string(r.uboOffset)+",\"captures\":"+std::to_string(captures)+",\"applies\":"+std::to_string(applies)+",\"tolerance\":"+std::to_string(tolerance())+",\"hidden\":"+std::to_string(hiddenMode())+",\"mark\":"+std::to_string(markMode()));
    }catch(const std::exception& e){giReproject::failNow(dev,e.what());}
}
// With the device (vkDestroyDevice; the game has waited for it to be idle): everything the layer made for this.
void giReprojectDestroy(VkDevice device) noexcept {
    try{
        std::shared_ptr<giReproject::Device> d;
        {std::lock_guard<std::mutex> lock(giReproject::giMutex);auto found=giReproject::giDevices.find(device);if(found==giReproject::giDevices.end())return;d=found->second;giReproject::giDevices.erase(found);
            giReproject::pending={};giReproject::commands.clear();giReproject::setRecords.clear();giReproject::fogPending={};giReproject::fogRecords.clear();giReproject::lastApply={};}
        auto& x=*d;const auto h=x.handle;
        for(auto& v:x.uintViews)x.vkDestroyImageView(h,v.second,nullptr);
        for(auto& v:x.fogSampledViews)x.vkDestroyImageView(h,v.second,nullptr);for(auto& v:x.fogStorageViews)x.vkDestroyImageView(h,v.second,nullptr);
        if(x.fogPipeline)x.vkDestroyPipeline(h,x.fogPipeline,nullptr);if(x.fogPipelineLayout)x.vkDestroyPipelineLayout(h,x.fogPipelineLayout,nullptr);
        if(x.fogSetLayout)x.vkDestroyDescriptorSetLayout(h,x.fogSetLayout,nullptr);if(x.fogPool)x.vkDestroyDescriptorPool(h,x.fogPool,nullptr);if(x.fogSampler)x.vkDestroySampler(h,x.fogSampler,nullptr);
        for(auto& r:x.retired){x.vkDestroyImageView(h,r.second,nullptr);x.vkDestroyImage(h,r.first,nullptr);}
        if(x.keptDepthView)x.vkDestroyImageView(h,x.keptDepthView,nullptr);if(x.keptIrradianceView)x.vkDestroyImageView(h,x.keptIrradianceView,nullptr);
        if(x.keptDepth)x.vkDestroyImage(h,x.keptDepth,nullptr);if(x.keptIrradiance)x.vkDestroyImage(h,x.keptIrradiance,nullptr);
        if(x.keptBuffer)x.vkDestroyBuffer(h,x.keptBuffer,nullptr);
        for(int i=0;i<2;i++){if(x.pipelines[i])x.vkDestroyPipeline(h,x.pipelines[i],nullptr);if(x.pipelineLayouts[i])x.vkDestroyPipelineLayout(h,x.pipelineLayouts[i],nullptr);if(x.setLayouts[i])x.vkDestroyDescriptorSetLayout(h,x.setLayouts[i],nullptr);}
        if(x.pool)x.vkDestroyDescriptorPool(h,x.pool,nullptr);
        for(auto m:x.memories)x.vkFreeMemory(h,m,nullptr);
        note("gi_reproject_destroyed","\"captures\":"+std::to_string(giReproject::captures)+",\"applies\":"+std::to_string(giReproject::applies)+",\"skips\":"+std::to_string(giReproject::skips)+
            ",\"fog_firsts\":"+std::to_string(giReproject::fogFirsts)+",\"fog_applies\":"+std::to_string(giReproject::fogApplies)+",\"fog_skips\":"+std::to_string(giReproject::fogSkips));
    }catch(...){}
}
