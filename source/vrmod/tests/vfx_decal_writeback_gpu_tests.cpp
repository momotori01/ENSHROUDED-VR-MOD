// The stand-in for the game's decal writeback shader on the actual GPU, against a CPU copy of the same sums.
// Standalone: no game process, no OpenXR. With one argument - a local copy of the game's own shader (1.0: 2016 bytes) -
// the game's shader and the stand-in are also run on the same input and must leave the same target.
#include <evr/vulkan.hpp>
#include <evr/vfx_decal_rebase.hpp>
#include <evr/head_spirv_patch.hpp>
#include <vfx_decal_writeback.h>
#include <fstream>
#include <iostream>
using namespace evr;
#define COMPUTE_API(X) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) \
 X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateComputePipelines) \
 X(vkCmdBindDescriptorSets) X(vkCmdDispatch)
constexpr uint32_t records=300,groups=5,groupSize=64;   // 1.0: groups of 64
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
    void pipeline(unsigned index,const uint32_t* code,size_t bytes){
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=bytes;sm.pCode=code;
        VkShaderModule shader{};vkCheck(v.vkCreateShaderModule(v.device,&sm,nullptr,&shader),"shader module");
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.layout=layout;
        ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,shader,"cs_main",nullptr};
        const auto result=vkCreateComputePipelines(v.device,VK_NULL_HANDLE,1,&ci,nullptr,&pipelines[index]);
        v.vkDestroyShaderModule(v.device,shader,nullptr);vkCheck(result,"compute pipeline");
    }
    void init(){
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="EVR decal writeback fixture";app.apiVersion=VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
        VkInstance instance{};vkCheck(v.vkCreateInstance(&ici,nullptr,&instance),"instance");v.owned=true;v.bindInstance(instance);
        uint32_t count=0;vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,nullptr),"physical count");
        std::vector<VkPhysicalDevice> devices(count);vkCheck(v.vkEnumeratePhysicalDevices(instance,&count,devices.data()),"physical devices");
        if(devices.empty())throw std::runtime_error("no Vulkan GPU");auto physical=devices.front();
        uint32_t families=0;v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,nullptr);
        std::vector<VkQueueFamilyProperties> props(families);v.vkGetPhysicalDeviceQueueFamilyProperties(physical,&families,props.data());
        uint32_t family=0;while(family<families&&!(props[family].queueFlags&VK_QUEUE_COMPUTE_BIT))++family;
        if(family==families)throw std::runtime_error("no compute queue");
        const float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qi;
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
        // payload, sort data, source, target, counters
        const VkDeviceSize sizes[]{20,8*uint64_t(groups)*groupSize,uint64_t(vfx::decalWords)*4*records,uint64_t(vfx::decalWords)*4*groups*groupSize,64};
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
    void run(unsigned which,uint32_t payloadBytes){
        std::array<VkDescriptorBufferInfo,5> infos{};std::array<VkWriteDescriptorSet,5> writes{};
        for(uint32_t i=0;i<5;i++){
            infos[i]={buffers[i].handle,0,i==0?payloadBytes:buffers[i].size};
            writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,set,i,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&infos[i],nullptr};
        }
        vkUpdateDescriptorSets(v.device,5,writes.data(),0,nullptr);
        vkCheck(v.vkResetCommandBuffer(command,0),"reset command");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkCheck(v.vkBeginCommandBuffer(command,&begin),"begin");
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        v.vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[which]);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);vkCmdDispatch(command,groups,1,1);
        barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        v.vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        vkCheck(v.vkEndCommandBuffer(command),"end");vkCheck(v.vkResetFences(v.device,1,&fence),"reset fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
        vkCheck(v.vkQueueSubmit(v.queue,1,&submit,fence),"submit");vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,10'000'000'000),"completion");
    }
};
int main(int argc,char** argv){unsigned checks=0;auto require=[&](bool ok,const char* name){if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';};
    try{
        // The sums themselves.
        {vfx::DecalRecord d{};const float values[]{1,0,0,5, 0,2,0,6, .5f,.5f,3,7, 10,20,30,4};for(unsigned i=0;i<16;++i)d[i]=vfx::decalBits(values[i]);for(unsigned i=16;i<28;++i)d[i]=0xa5000000u+i;
         const std::array<float,3> delta{.064f,-.25f,1.5f};
         const auto into=vfx::rebaseDecal(d,delta,vfx::decalMarkerInto),place=vfx::rebaseDecal(d,delta,vfx::decalMarkerPlace);
         auto about=[](uint32_t bits,float want){return std::abs(vfx::decalFloat(bits)-want)<1e-5f;};
         require(about(into[3],5-.064f)&&about(into[7],6+.5f)&&about(into[11],7-(.032f-.125f+4.5f)),"into the decal's space: the row's offset loses row.delta");
         require(about(place[3],5+.064f)&&about(place[7],6-.25f)&&about(place[11],7+1.5f),"the decal's place: the fourth column gains delta");
         bool rest=true;for(unsigned i=0;i<28;++i)if(i!=3&&i!=7&&i!=11)rest&=into[i]==d[i]&&place[i]==d[i];
         require(rest&&into[12]==d[12]&&into[13]==d[13]&&into[14]==d[14]&&into[15]==d[15]&&place[12]==d[12],"1.0: the sphere (a world place), the radius and everything after stay");
         require(vfx::rebaseDecal(d,delta,0x12345678u)==d&&vfx::decalPayload(delta,vfx::decalMarkerInto)==std::array<uint32_t,5>{0u,0x45565244u,vfx::decalBits(.064f),vfx::decalBits(-.25f),vfx::decalBits(1.5f)},"another marker changes nothing; the payload's words");}
        Fixture f;f.init();f.pipeline(0,vfxDecalWriteback,sizeof(vfxDecalWriteback));
        bool native=false;
        if(argc==2){
            std::ifstream input(argv[1],std::ios::binary|std::ios::ate);if(!input||input.tellg()!=std::streamoff(vfx::decalWritebackBytes))throw std::runtime_error("native shader extent");
            std::vector<uint32_t> source(vfx::decalWritebackBytes/4);input.seekg(0);input.read(reinterpret_cast<char*>(source.data()),vfx::decalWritebackBytes);
            if(head::sha256(source.data(),vfx::decalWritebackBytes)!=vfx::decalWritebackSha)throw std::runtime_error("native shader is not the reviewed one");
            f.pipeline(1,source.data(),vfx::decalWritebackBytes);native=true;
        }
        auto* payload=static_cast<uint32_t*>(f.buffers[0].mapped);auto* sort=static_cast<uint32_t*>(f.buffers[1].mapped);
        auto* source=static_cast<uint32_t*>(f.buffers[2].mapped);auto* target=static_cast<uint32_t*>(f.buffers[3].mapped);auto* counters=static_cast<uint32_t*>(f.buffers[4].mapped);
        const uint32_t slots=groups*groupSize,counterOffset=28;   // the counter's byte offset, as the game hands it
        uint32_t seed=12345;auto next=[&]{seed=seed*1664525u+1013904223u;return seed;};
        std::vector<vfx::DecalRecord> input(records);
        for(uint32_t i=0;i<records;++i){for(unsigned w=0;w<vfx::decalWords;++w)input[i][w]=w<16?vfx::decalBits(float(int(next()>>8)%20001-10000)/100.f):next();std::memcpy(source+i*vfx::decalWords,input[i].data(),112);}
        std::vector<uint32_t> order(slots);
        for(uint32_t i=0;i<slots;++i){order[i]=next()%records;sort[i*2]=next();sort[i*2+1]=order[i];}   // 1.0: {key, index}
        const std::array<float,3> delta{.0641f,-.0123f,.0077f};
        struct Case {const char* name;unsigned pipeline;uint32_t bytes,marker,count;};
        std::vector<Case> cases{{"the game's own call: a plain copy",0,4,0,217},{"into the decal's space",0,20,vfx::decalMarkerInto,217},{"the decal's place",0,20,vfx::decalMarkerPlace,300},
            {"five words with another marker: a plain copy",0,20,0x11111111u,64},{"nothing to write",0,20,vfx::decalMarkerInto,0},{"every slot",0,20,vfx::decalMarkerInto,slots}};
        if(native){cases.push_back({"the game's shader, one word",1,4,0,217});cases.push_back({"the game's shader ignores further words",1,20,vfx::decalMarkerInto,217});}
        std::vector<uint32_t> nativeResult,ownResult;
        for(const auto& c:cases){
            std::memset(counters,0,64);counters[counterOffset/4]=c.count;counters[0]=slots;   // a wrong counter in front of the right one
            const auto words=vfx::decalPayload(delta,c.marker);std::memcpy(payload,words.data(),20);payload[0]=counterOffset;
            for(uint32_t i=0;i<slots*vfx::decalWords;++i)target[i]=0xdeadbeefu;
            f.run(c.pipeline,c.bytes);
            bool same=true;
            for(uint32_t i=0;i<slots&&same;++i){
                vfx::DecalRecord want;want.fill(0xdeadbeefu);
                if(i<c.count)want=c.pipeline==0&&c.bytes>=20?vfx::rebaseDecal(input[order[i]],delta,c.marker):input[order[i]];
                for(unsigned w=0;w<vfx::decalWords&&same;++w){
                    const uint32_t got=target[i*vfx::decalWords+w];
                    // moved floats may differ in the last bit between the GPU's sum and the CPU's
                    const bool moved=w==3||w==7||w==11;
                    same=got==want[w]||(i<c.count&&moved&&std::abs(vfx::decalFloat(got)-vfx::decalFloat(want[w]))<=2e-5f*std::max(1.f,std::abs(vfx::decalFloat(want[w]))));
                }
            }
            require(same,c.name);
            if(c.pipeline==1&&c.bytes==4)nativeResult.assign(target,target+slots*vfx::decalWords);
            if(c.pipeline==0&&c.bytes==4)ownResult.assign(target,target+slots*vfx::decalWords);
        }
        if(native)require(nativeResult==ownResult,"the game's shader and the stand-in leave the same target bit for bit");
        std::cout<<"RESULT "<<checks<<" PASS 0 FAIL (actual GPU"<<(native?", with the game's shader":"")<<")\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
