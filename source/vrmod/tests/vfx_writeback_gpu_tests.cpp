// Standalone GPU fixture; no game process or OpenXR/SteamVR setting is touched.
#include <evr/vulkan.hpp>
#include <evr/vfx_writeback_patch.hpp>
#include <fstream>
#include <iostream>
#include <set>
using namespace evr;
#define COMPUTE_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) \
 X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateComputePipelines) \
 X(vkCmdBindDescriptorSets) X(vkCmdDispatch)
struct Fixture {
    Vulkan v;
    VkDescriptorSetLayout setLayout{};VkDescriptorPool descriptors{};VkDescriptorSet set{};
    VkPipelineLayout layout{};std::array<VkPipeline,2> pipelines{};
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};
    struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};void* mapped=nullptr;VkDeviceSize size=0;};
    std::array<Buffer,5> buffers{};
#define DECL(name) PFN_##name name=nullptr;
    COMPUTE_API(DECL)
#undef DECL
    ~Fixture(){
        if(!v.device)return;
        v.vkDeviceWaitIdle(v.device);
        for(auto& b:buffers){if(b.mapped)v.vkUnmapMemory(v.device,b.memory);if(b.handle)v.vkDestroyBuffer(v.device,b.handle,nullptr);if(b.memory)v.vkFreeMemory(v.device,b.memory,nullptr);}
        if(fence)v.vkDestroyFence(v.device,fence,nullptr);
        if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);
        for(auto p:pipelines)if(p)v.vkDestroyPipeline(v.device,p,nullptr);
        if(layout)v.vkDestroyPipelineLayout(v.device,layout,nullptr);
        if(descriptors)vkDestroyDescriptorPool(v.device,descriptors,nullptr);
        if(setLayout)vkDestroyDescriptorSetLayout(v.device,setLayout,nullptr);
    }
    void init(const std::vector<uint32_t>& source){
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="EVR VFX isolated fixture";app.apiVersion=VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
        VkInstance instance{};vkCheck(v.vkCreateInstance(&ici,nullptr,&instance),"instance");v.owned=true;v.bindInstance(instance);
        uint32_t count=0;vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,nullptr),"physical count");
        std::vector<VkPhysicalDevice> devices(count);vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,devices.data()),"physical devices");
        if(devices.empty())throw std::runtime_error("no Vulkan GPU");auto physical=devices.front();
        uint32_t families=0;v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,nullptr);
        std::vector<VkQueueFamilyProperties> props(families);v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,props.data());
        uint32_t family=0;while(family<families&&!(props[family].queueFlags&VK_QUEUE_COMPUTE_BIT))++family;
        if(family==families)throw std::runtime_error("no compute queue");
        VkPhysicalDeviceVulkan12Features available{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&available;
        auto getFeatures=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(v.gipa(instance,"vkGetPhysicalDeviceFeatures2"));
        if(!getFeatures)throw std::runtime_error("features2 unavailable");getFeatures(physical,&features);
        if(!available.scalarBlockLayout)throw std::runtime_error("native shader requires scalarBlockLayout");
        VkPhysicalDeviceVulkan12Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};enabled.scalarBlockLayout=VK_TRUE;
        const float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&enabled;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qi;
        VkDevice device{};vkCheck(v.vkCreateDevice(physical,&dci,nullptr,&device),"device");v.bindDevice(device,physical,family);
#define LOAD(name) name=reinterpret_cast<PFN_##name>(v.vkGetDeviceProcAddr(device,#name));if(!name)throw std::runtime_error(#name);
        COMPUTE_API(LOAD)
#undef LOAD
        std::array<VkDescriptorSetLayoutBinding,5> bindings{};
        for(uint32_t i=0;i<5;i++)bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=5;sl.pBindings=bindings.data();
        vkCheck(vkCreateDescriptorSetLayout(device,&sl,nullptr,&setLayout),"set layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,5};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&size;vkCheck(vkCreateDescriptorPool(device,&dp,nullptr,&descriptors),"descriptor pool");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=descriptors;da.descriptorSetCount=1;da.pSetLayouts=&setLayout;
        vkCheck(vkAllocateDescriptorSets(device,&da,&set),"descriptor set");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;
        vkCheck(v.vkCreatePipelineLayout(device,&pl,nullptr,&layout),"pipeline layout");
        const auto patched=vfx::patchWriteback(source.data(),source.size()*4);
        for(unsigned i=0;i<2;i++){
            const auto& code=i?patched:source;VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=code.size()*4;sm.pCode=code.data();
            VkShaderModule shader{};vkCheck(v.vkCreateShaderModule(device,&sm,nullptr,&shader),"shader module");
            VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.layout=layout;
            ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"cs_main",nullptr};
            const auto result=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipelines[i]);
            v.vkDestroyShaderModule(device,shader,nullptr);vkCheck(result,"compute pipeline");
        }
        const VkDeviceSize sizes[]{28,16*513,180*513,180*515,64};
        for(size_t i=0;i<5;i++){
            auto& b=buffers[i];b.size=sizes[i];VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=b.size;bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            vkCheck(v.vkCreateBuffer(device,&bi,nullptr,&b.handle),"buffer");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(device,b.handle,&req);
            b.memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(device,b.handle,b.memory,0),"buffer memory");vkCheck(v.vkMapMemory(device,b.memory,0,b.size,0,&b.mapped),"map");
        }
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=family;pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCheck(v.vkCreateCommandPool(device,&pc,nullptr,&pool),"command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(device,&ca,&command),"command buffer");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(device,&fi,nullptr,&fence),"fence");
        VkPhysicalDeviceProperties properties{};v.vkGetPhysicalDeviceProperties(physical,&properties);std::cout<<"GPU "<<properties.deviceName<<'\n';
    }
    void run(bool patched,uint32_t payloadBytes,uint32_t capacity){
        std::array<VkDescriptorBufferInfo,5> infos{};std::array<VkWriteDescriptorSet,5> writes{};
        for(uint32_t i=0;i<5;i++){
            infos[i]={buffers[i].handle,0,i==0?payloadBytes:i==3?uint64_t(capacity)*180:buffers[i].size};
            writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,i,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&infos[i],nullptr};
        }
        vkUpdateDescriptorSets(v.device,5,writes.data(),0,nullptr);
        vkCheck(v.vkResetCommandBuffer(command,0),"reset command");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkCheck(v.vkBeginCommandBuffer(command,&begin),"begin");
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        v.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[patched?1:0]);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);vkCmdDispatch(command,5,1,1);
        barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        vkCheck(v.vkEndCommandBuffer(command),"end");vkCheck(v.vkResetFences(v.device,1,&fence),"reset fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
        vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"submit");vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,10'000'000'000),"completion");
    }
};

int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: evr_vfx_writeback_gpu_tests local-native-writeback.spv");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);if(!input||input.tellg()!=2720)throw std::runtime_error("native shader extent");
    std::vector<uint32_t> source(680);input.seekg(0);input.read(reinterpret_cast<char*>(source.data()),2720);
    if(!input||head::sha256(source.data(),2720)!=vfx::writebackSha)throw std::runtime_error("native shader hash");
    Fixture fixture;fixture.init(source);unsigned passed=0,failed=0;
    auto check=[&](bool okay,const char* name){(okay?passed:failed)++;std::cout<<(okay?"PASS ":"FAIL ")<<name<<'\n';};
    using Record=std::array<uint8_t,180>;
    std::array<Record,513> records{};
    for(unsigned i=0;i<records.size();i++){
        // Each non-position word stays byte-identical, including material fields.
        for(unsigned word=0;word<45;word++){uint32_t value=0x3f000000u+i*64+word;std::memcpy(records[i].data()+word*4,&value,4);}
        std::memcpy(records[i].data()+20,&i,4);
        const float position[]{float(i)/16,float(i)/8,-float(i)/32};std::memcpy(records[i].data()+40,position,12);
    }
    auto prepare=[&](uint32_t count,uint32_t existing,uint32_t cap,uint32_t marker,uint32_t sourceOffset,std::array<float,3> delta){
        for(auto& b:fixture.buffers)std::memset(b.mapped,0,b.size);
        uint32_t payload[]{8,marker,sourceOffset,cap,0,0,0};std::memcpy(payload+4,delta.data(),12);
        std::memcpy(fixture.buffers[0].mapped,payload,28);
        auto* sort=static_cast<uint32_t*>(fixture.buffers[1].mapped);for(uint32_t i=0;i<513;i++)sort[i*4+2]=512-i;
        std::memcpy(fixture.buffers[2].mapped,records.data(),sizeof(records));std::memset(fixture.buffers[3].mapped,0xa5,fixture.buffers[3].size);
        auto* counter=static_cast<uint32_t*>(fixture.buffers[4].mapped);counter[0]=count;counter[1]=0x11223344;counter[2]=existing;counter[3]=0x66778899;
    };
    auto snapshot=[&](){std::vector<uint8_t> result(size_t(fixture.buffers[3].size));std::memcpy(result.data(),fixture.buffers[3].mapped,result.size());return result;};
    for(uint32_t count:{0u,1u,127u,128u,257u,513u}){
        prepare(count,count,513,0,0,{});fixture.run(false,4,513);const auto baseline=snapshot();
        prepare(count,count,513,0,0,{});fixture.run(true,4,513);
        check(snapshot()==baseline,"four-byte native payload remains byte-exact");
        check(static_cast<uint32_t*>(fixture.buffers[4].mapped)[2]==count,"native path does not update counter");
    }
    for(auto delta:std::array<std::array<float,3>,3>{{{.032f,-.064f,.15f},{-.032f,.064f,-.15f},{0,0,0}}})
        for(uint32_t count:{0u,1u,127u,257u,513u})for(uint32_t cap:{11u,513u}){
        constexpr uint32_t existing=3;prepare(count,existing,cap,vfx::payloadMarker,0,delta);
        const auto before=snapshot();fixture.run(true,28,cap);const auto after=snapshot();
        const auto expectedCount=std::min(existing+count,cap);auto* counters=static_cast<uint32_t*>(fixture.buffers[4].mapped);
        check(counters[0]==count&&counters[1]==0x11223344&&counters[2]==expectedCount&&counters[3]==0x66778899,"source counter preserved; destination count bounded");
        bool preserved=std::equal(after.begin(),after.begin()+existing*180,before.begin())&&std::equal(after.begin()+expectedCount*180,after.end(),before.begin()+expectedCount*180);
        check(preserved,"existing records and out-of-range canary preserved");
        std::set<uint32_t> selected;bool contents=true;
        for(uint32_t slot=existing;slot<expectedCount;slot++){
            Record actual{};std::memcpy(actual.data(),after.data()+slot*180,180);uint32_t identity=0;std::memcpy(&identity,actual.data()+20,4);
            if(identity>=513||identity<513-count||!selected.insert(identity).second){contents=false;continue;}
            auto expected=records[identity];std::memcpy(expected.data()+16,&slot,4);float pos[3]{};std::memcpy(pos,expected.data()+40,12);
            for(unsigned k=0;k<3;k++)pos[k]+=delta[k];std::memcpy(expected.data()+40,pos,12);contents&=actual==expected;
        }
        check(contents,"unique atomic append rebases only position and native instanceOffset");
        check(std::memcmp(fixture.buffers[2].mapped,records.data(),sizeof(records))==0,"simulation particle records untouched");
    }
    for(auto invalid:std::array<std::array<uint32_t,2>,2>{{{0,0},{vfx::payloadMarker,8}}}){
        prepare(257,5,513,invalid[0],invalid[1],{2,3,4});fixture.run(false,28,513);const auto baseline=snapshot();
        prepare(257,5,513,invalid[0],invalid[1],{2,3,4});fixture.run(true,28,513);
        check(snapshot()==baseline&&static_cast<uint32_t*>(fixture.buffers[4].mapped)[2]==5,"invalid marker or self-source falls back to native copy");
    }
    std::cout<<"PASS "<<passed<<" FAIL "<<failed<<'\n';return failed?1:0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
