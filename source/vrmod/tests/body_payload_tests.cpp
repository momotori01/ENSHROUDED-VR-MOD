// Owned-buffer ABI/lifetime GPU fixture, independent of the game and OpenXR.
#include <evr/body_clip_payload_store.hpp>
#include <iostream>
#include <fstream>
#include <limits>
using namespace evr;
static std::atomic<unsigned> issues{0};
static VKAPI_ATTR VkBool32 VKAPI_CALL message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* d,void*){
    if(severity&(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)){++issues;std::cerr<<"VULKAN "<<d->pMessage<<'\n';}return VK_FALSE;
}
#define BODY_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCmdBindDescriptorSets) X(vkCreateComputePipelines) X(vkCmdDispatch)
struct Fixture {
    Vulkan v;std::unique_ptr<body::PayloadStore> payloads;VkDebugUtilsMessengerEXT messenger{};PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool descriptorPool{};VkDescriptorSet set{};VkPipelineLayout layout{};VkPipeline pipeline{};
    VkBuffer result{};VkDeviceMemory memory{};void* mapped=nullptr;VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};
#define DECL(n) PFN_##n n=nullptr;
    BODY_API(DECL)
#undef DECL
    ~Fixture(){if(v.device){v.vkDeviceWaitIdle(v.device);if(payloads)payloads->releaseAtDeviceDestroy();payloads.reset();
        if(mapped)v.vkUnmapMemory(v.device,memory);if(result)v.vkDestroyBuffer(v.device,result,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);
        if(fence)v.vkDestroyFence(v.device,fence,nullptr);if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);
        if(pipeline)v.vkDestroyPipeline(v.device,pipeline,nullptr);if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);
        if(descriptorPool)vkDestroyDescriptorPool(v.device,descriptorPool,nullptr);if(setLayout)vkDestroyDescriptorSetLayout(v.device,setLayout,nullptr);
        v.vkDestroyDevice(v.device,nullptr);v.device=VK_NULL_HANDLE;}
        if(messenger)destroyMessenger(v.instance,messenger,nullptr);
    }
    void init(const char* shaderPath){
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;app.pApplicationName="EVR owned body payload fixture";
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;debug.messageType=7;debug.pfnUserCallback=message;
        const VkValidationFeatureEnableEXT sync=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};validation.enabledValidationFeatureCount=1;validation.pEnabledValidationFeatures=&sync;validation.pNext=&debug;
        const char* layers[]{"VK_LAYER_KHRONOS_validation"};const char* extensions[]{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pNext=&validation;create.pApplicationInfo=&app;create.enabledLayerCount=1;create.ppEnabledLayerNames=layers;create.enabledExtensionCount=1;create.ppEnabledExtensionNames=extensions;
        VkInstance instance{};vkCheck(v.vkCreateInstance(&create,nullptr,&instance),"instance");v.owned=true;v.bindInstance(instance);
        auto makeMessenger=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(v.gipa(instance,"vkCreateDebugUtilsMessengerEXT"));destroyMessenger=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(v.gipa(instance,"vkDestroyDebugUtilsMessengerEXT"));
        vkCheck(makeMessenger(instance,&debug,nullptr,&messenger),"debug messenger");uint32_t count=0;vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,nullptr),"GPU count");std::vector<VkPhysicalDevice> physicals(count);vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,physicals.data()),"GPUs");if(physicals.empty())throw std::runtime_error("no GPU");auto physical=physicals.front();
        VkPhysicalDeviceVulkan12Features supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&supported;
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(v.gipa(instance,"vkGetPhysicalDeviceFeatures2"))(physical,&features);
        if(!supported.bufferDeviceAddress||!features.features.shaderClipDistance||!features.features.shaderInt64)throw std::runtime_error("required fixture features unavailable");
        VkPhysicalDeviceVulkan12Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};enabled.bufferDeviceAddress=VK_TRUE;
        VkPhysicalDeviceFeatures core{};core.shaderInt64=core.shaderClipDistance=VK_TRUE;
        uint32_t countQueues=0;v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&countQueues,nullptr);std::vector<VkQueueFamilyProperties> queues(countQueues);v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&countQueues,queues.data());uint32_t family=0;while(family<countQueues&&!(queues[family].queueFlags&VK_QUEUE_COMPUTE_BIT))++family;if(family==countQueues)throw std::runtime_error("no compute queue");
        float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueCount=1;qi.queueFamilyIndex=family;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pNext=&enabled;di.pEnabledFeatures=&core;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;VkDevice device{};
        if(!body::featuresEnabled(di))throw std::runtime_error("feature extraction disagrees");vkCheck(v.vkCreateDevice(physical,&di,nullptr,&device),"device");v.bindDevice(device,physical,family);
#define LOAD(n) n=reinterpret_cast<PFN_##n>(v.vkGetDeviceProcAddr(device,#n));if(!n)throw std::runtime_error(#n);
        BODY_API(LOAD)
#undef LOAD
        payloads=std::make_unique<body::PayloadStore>(device,physical,instance,v.vkGetDeviceProcAddr,v.gipa);payloads->shaderMask=3; // The fixture tests the payload ABI; native patch activation is a separate test.
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=128;bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;vkCheck(v.vkCreateBuffer(device,&bi,nullptr,&result),"result buffer");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(device,result,&req);memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(device,result,memory,0),"result bind");vkCheck(v.vkMapMemory(device,memory,0,128,0,&mapped),"result map");
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=1;sl.pBindings=&binding;vkCheck(vkCreateDescriptorSetLayout(device,&sl,nullptr,&setLayout),"set layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&size;vkCheck(vkCreateDescriptorPool(device,&dp,nullptr,&descriptorPool),"descriptor pool");
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocate.descriptorPool=descriptorPool;allocate.descriptorSetCount=1;allocate.pSetLayouts=&setLayout;vkCheck(vkAllocateDescriptorSets(device,&allocate,&set),"set");VkDescriptorBufferInfo db{result,0,128};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&db;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,16};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;vkCheck(v.vkCreatePipelineLayout(device,&pl,nullptr,&layout),"pipeline layout");
        std::ifstream input(shaderPath,std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("fixture shader absent");auto length=input.tellg();if(length<=0||length>65536||length%4)throw std::runtime_error("fixture shader extent");std::vector<uint32_t> code(size_t(length)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),length);if(!input)throw std::runtime_error("fixture shader read");
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=code.size()*4;sm.pCode=code.data();VkShaderModule shader{};vkCheck(v.vkCreateShaderModule(device,&sm,nullptr,&shader),"shader");VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipelineInfo.layout=layout;pipelineInfo.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"main",nullptr};auto status=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&pipeline);v.vkDestroyShaderModule(device,shader,nullptr);vkCheck(status,"pipeline");
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=family;pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;vkCheck(v.vkCreateCommandPool(device,&pc,nullptr,&pool),"pool");VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vkCheck(v.vkAllocateCommandBuffers(device,&ca,&command),"command");VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(device,&fi,nullptr,&fence),"fence");
    }
    std::array<uint32_t,32> run(uint64_t address,uint32_t instance,float w){
        struct Args{uint64_t address;uint32_t instance;float w;};static_assert(sizeof(Args)==16);Args args{address,instance,w};
        vkCheck(v.vkResetCommandBuffer(command,0),"reset");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};vkCheck(v.vkBeginCommandBuffer(command,&begin),"begin");
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        v.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);v.vkCmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(args),&args);vkCmdDispatch(command,1,1,1);
        barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);vkCheck(v.vkEndCommandBuffer(command),"end");vkCheck(v.vkResetFences(v.device,1,&fence),"reset fence");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"submit");vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,3000000000),"wait");std::array<uint32_t,32> resultWords{};std::memcpy(resultWords.data(),mapped,128);return resultWords;
    }
};
int main(int argc,char** argv){unsigned checks=0;try{
    if(argc!=2)throw std::runtime_error("fixture shader argument required");auto check=[&](bool ok){if(!ok)throw std::runtime_error("body payload invariant case "+std::to_string(checks+1));++checks;};
    {Fixture fixture;fixture.init(argv[1]);auto payload=body::pack({42,3,0xffffff,15,3},.15f);bool created=false;auto address=fixture.payloads->get(payload,created);check(address&&created);check(fixture.payloads->get(payload,created)==address&&!created);
        for(auto id:{0u,3u,15u,42u,144u,0xffffffu})for(float w:{-1.f,.01f,.15f,.2f,1.f}){
            auto actual=fixture.run(address,id,w);check(!std::memcmp(actual.data(),&payload,sizeof(payload)));const bool selected=std::find(payload.models.begin(),payload.models.begin()+payload.count,id)!=payload.models.begin()+payload.count;float clip;std::memcpy(&clip,&actual[28],4);check(clip==(selected?w-payload.distance:1.f)&&actual[29]==uint32_t(selected));
        }
        auto inactive=fixture.run(0,3,-1);float clip;std::memcpy(&clip,&inactive[28],4);check(clip==1.f&&inactive[29]==0&&std::all_of(inactive.begin(),inactive.begin()+28,[](auto n){return n==0;}));
        for(float distance:{0.f,-1.f,1.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){bool rejected=false;try{body::pack({3},distance);}catch(...){rejected=true;}check(rejected);}
        for(auto ids:std::vector<std::vector<uint32_t>>{{},{0x1000000},std::vector<uint32_t>(25)}){if(ids.size()==25)for(uint32_t n=0;n<25;n++)ids[n]=n;bool rejected=false;try{body::pack(ids,.15f);}catch(...){rejected=true;}check(rejected);}
        auto malformed=payload;malformed.count=25;bool rejected=false;try{fixture.payloads->get(malformed,created);}catch(...){rejected=true;}check(rejected);
        auto appearance=body::packAppearance({0,5,1000,0xffffff},14);auto appearanceAddress=fixture.payloads->get(appearance,created);check(appearanceAddress&&created&&appearanceAddress!=address);
        check(fixture.payloads->get(appearance,created)==appearanceAddress&&!created);
        for(auto id:{0u,5u,13u,42u,1000u,0xffffffu}){
            auto actual=fixture.run(appearanceAddress,id,.2f);check(!std::memcmp(actual.data(),&appearance,sizeof(appearance)));
            float clip;std::memcpy(&clip,&actual[28],4);check(clip==1.f&&actual[29]==0); // appearance can never activate vertex distance clipping
        }
        for(auto ids:std::vector<std::vector<uint32_t>>{{},{0x1000000},{13}}){bool bad=false;try{body::packAppearance(ids,14);}catch(...){bad=true;}check(bad);}
        {auto bad=appearance;bad.distance=.1f;bool rejected=false;try{fixture.payloads->get(bad,created);}catch(...){rejected=true;}check(rejected);}
        {bool rejected=false;try{body::packAppearance({1},0x1000001);}catch(...){rejected=true;}check(rejected);}
        auto pair=body::packAppearance({0,5,1000,0xffffff},14,24);auto pairAddress=fixture.payloads->get(pair,created);check(pairAddress&&created&&pairAddress!=appearanceAddress);
        check(body::appearanceExtra(pair)==24&&pair.reserved==14);check(fixture.payloads->get(pair,created)==pairAddress&&!created);
        for(auto id:{0u,13u,23u,1000u,0xffffffu}){auto actual=fixture.run(pairAddress,id,.2f);check(!std::memcmp(actual.data(),&pair,sizeof(pair)));float clip;std::memcpy(&clip,&actual[28],4);check(clip==1.f&&actual[29]==0);}
        for(auto extra:{14u,6u,0x1000001u}){bool rejected=false;try{body::packAppearance({0,5},14,extra);}catch(...){rejected=true;}check(rejected);}
        auto triple=body::packAppearance({0,5,1000,0xffffff},14,24,34);auto tripleAddress=fixture.payloads->get(triple,created);check(tripleAddress&&created);
        check(triple.tag==body::appearanceShieldMarker&&body::appearanceShield(triple)==34&&triple.count==4);
        auto tripleGpu=fixture.run(tripleAddress,33,.2f);check(!std::memcmp(tripleGpu.data(),&triple,sizeof(triple))&&tripleGpu[29]==0);
        for(auto third:{14u,24u,6u,0x1000001u}){bool rejected=false;try{body::packAppearance({0,5},14,24,third);}catch(...){rejected=true;}check(rejected);}
        {std::vector<uint32_t> full(24);for(uint32_t i=0;i<24;i++)full[i]=i;bool rejected=false;try{body::packAppearance(full,30,31,32);}catch(...){rejected=true;}check(rejected);}
        {auto malformed=triple;malformed.models.back()=0;bool rejected=false;try{fixture.payloads->get(malformed,created);}catch(...){rejected=true;}check(rejected);}
        check(fixture.payloads->get(triple,created)==tripleAddress&&!created);
        for(uint32_t n=4;n<256;n++){auto next=body::pack({1000+n},.25f);check(fixture.payloads->get(next,created)&&created);}
        rejected=false;try{fixture.payloads->get(body::pack({2000},.25f),created);}catch(...){rejected=true;}check(rejected);check(fixture.payloads->get(payload,created)==address&&!created);auto stable=fixture.run(address,42,.2f);check(!std::memcmp(stable.data(),&payload,sizeof(payload)));
    }
    check(issues.load()==0);std::cout<<checks<<" PASS / 0 FAIL; Core+Sync issues="<<issues<<"; isolated payload ABI only\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"; validation="<<issues<<'\n';return 1;}}
