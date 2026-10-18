#pragma once
#include <map>
// Opt-in GPU timing (EVR_GPU_TIMING_EVERY=N, 2026-10-06 performance work).
// For one pair in every N, each primary command buffer begun while the window is open gets a
// timestamp after begin, before every vkCmdBeginRendering, after every vkCmdEndRendering and before
// end, and before every dispatch (with the bound compute pipeline). Segments between marks are either a render pass (area, color count, draws) or the
// compute/transfer block between passes (dispatch count). Results are read a few pairs later with
// availability bits (never waits) and logged as one `gpu_timing` event. Apart from the timestamps
// and one query reset at the start of each sampled buffer, the game's commands are unchanged.
namespace gpuTiming {
inline unsigned every(){static const unsigned n=[]{const auto f=std::getenv("EVR_GPU_TIMING_EVERY");return f?unsigned(std::strtoul(f,nullptr,10)):0u;}();return n;}
constexpr uint32_t perBuffer=4096;
struct Mark {char kind=0;uint32_t width=0,height=0,colors=0,draws=0,dispatches=0;uint64_t pipeline=0;uint32_t gx=0,gy=0,gz=0;uint64_t sets=0;
    uint32_t site=0,site2=0;}; // dispatch marks: RVAs of the first two enshrouded.exe frames on the recording stack
// The first two return addresses inside the game executable (which game function recorded this command).
inline std::pair<uint32_t,uint32_t> gameSites() noexcept {
    static const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    static const auto size=[]{const auto b=reinterpret_cast<const uint8_t*>(base);const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(b);
        return uintptr_t(reinterpret_cast<const IMAGE_NT_HEADERS64*>(b+dos->e_lfanew)->OptionalHeader.SizeOfImage);}();
    void* frames[32]{};const auto n=RtlCaptureStackBackTrace(1,32,frames,nullptr);
    std::pair<uint32_t,uint32_t> r{};
    for(USHORT i=0;i<n;i++){const auto a=reinterpret_cast<uintptr_t>(frames[i]);
        if(a>=base&&a<base+size){if(!r.first)r.first=uint32_t(a-base);else{r.second=uint32_t(a-base);break;}}}
    return r;
}
struct Cmd {uint32_t first=0,used=0;uint64_t order=0;uint32_t draws=0,dispatches=0;uint64_t compute=0;std::array<uint64_t,4> computeSets{};std::vector<Mark> marks;};
// FNV-1a over the bound compute descriptor sets 0..3 and their dynamic offsets: equal hashes in two Views
// mean the dispatch read/wrote through the very same descriptor sets (same resources and offsets).
inline uint64_t setsHash(const std::array<uint64_t,4>& sets){uint64_t h=1469598103934665603ull;for(auto v:sets){for(int i=0;i<8;i++){h^=(v>>(8*i))&0xff;h*=1099511628211ull;}}return h;}
inline std::mutex timingMutex;
inline std::atomic<bool> window{false};
inline VkDevice device{};inline VkQueryPool pool{};inline uint32_t capacity=0,next=0;inline double period=1.0;inline uint64_t order=0;
inline std::unordered_map<VkCommandBuffer,Cmd> active;inline std::vector<Cmd> finished;
inline uint64_t windowPair=0,harvestPair=0;
inline bool ensurePool(VkDevice d){
    if(pool)return d==device;
    DeviceDispatch dd{};{std::lock_guard<std::mutex> lock(mutex);auto found=devices.find(key(d));if(found==devices.end())return false;dd=found->second;}
    auto props=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(dd.gipa(dd.instance,"vkGetPhysicalDeviceProperties"));
    if(!props)return false;VkPhysicalDeviceProperties p{};props(dd.physical,&p);
    if(!p.limits.timestampComputeAndGraphics||p.limits.timestampPeriod<=0)return false;
    VkQueryPoolCreateInfo q{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};q.queryType=VK_QUERY_TYPE_TIMESTAMP;q.queryCount=perBuffer*4;
    if(deviceFn<PFN_vkCreateQueryPool>(d,"vkCreateQueryPool")(d,&q,nullptr,&pool)!=VK_SUCCESS){pool={};return false;}
    device=d;capacity=q.queryCount;period=p.limits.timestampPeriod;
    note("gpu_timing_pool","\"queries\":"+std::to_string(capacity)+",\"period_ns\":"+std::to_string(period)+",\"every\":"+std::to_string(every()));
    return true;
}
// Appends a mark and writes its timestamp; callers only mark outside render passes.
inline void mark(VkCommandBuffer cmd,Mark m,VkPipelineStageFlagBits stage){
    uint32_t query=0;VkQueryPool p{};
    {std::lock_guard<std::mutex> lock(timingMutex);auto found=active.find(cmd);if(found==active.end())return;auto& c=found->second;
        if(c.used>=perBuffer)return;
        m.draws=c.draws;m.dispatches=c.dispatches;c.draws=c.dispatches=0;
        query=c.first+c.used++;c.marks.push_back(m);p=pool;}
    deviceFn<PFN_vkCmdWriteTimestamp>(cmd,"vkCmdWriteTimestamp")(cmd,stage,p,query);
}
}
void gpuTimingBegin(VkCommandBuffer cmd) noexcept {
    if(!gpuTiming::every()||!gpuTiming::window.load())return;
    try{
        {std::lock_guard<std::mutex> lock(mutex);auto id=commandIdentities.find(cmd);if(id==commandIdentities.end()||id->second.level!=VK_COMMAND_BUFFER_LEVEL_PRIMARY)return;}
        uint32_t first=0;VkQueryPool pool{};
        {std::lock_guard<std::mutex> lock(gpuTiming::timingMutex);
            gpuTiming::active.erase(cmd);
            if(!gpuTiming::pool||gpuTiming::next+gpuTiming::perBuffer>gpuTiming::capacity)return;
            first=gpuTiming::next;gpuTiming::next+=gpuTiming::perBuffer;pool=gpuTiming::pool;
            gpuTiming::Cmd c;c.first=first;c.order=++gpuTiming::order;gpuTiming::active[cmd]=std::move(c);}
        deviceFn<PFN_vkCmdResetQueryPool>(cmd,"vkCmdResetQueryPool")(cmd,pool,first,gpuTiming::perBuffer);
        gpuTiming::mark(cmd,{'S'},VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }catch(...){}
}
void gpuTimingRenderBegin(VkCommandBuffer cmd,const VkRenderingInfo* info) noexcept {
    if(!gpuTiming::every()||!info)return;
    try{gpuTiming::Mark m{'B',info->renderArea.extent.width,info->renderArea.extent.height,info->colorAttachmentCount};
        gpuTiming::mark(cmd,m,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);}catch(...){}
}
void gpuTimingRenderEnd(VkCommandBuffer cmd) noexcept {
    if(!gpuTiming::every())return;
    try{gpuTiming::mark(cmd,{'E'},VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);}catch(...){}
}
void gpuTimingEnd(VkCommandBuffer cmd) noexcept {
    if(!gpuTiming::every())return;
    try{
        gpuTiming::mark(cmd,{'X'},VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        std::lock_guard<std::mutex> lock(gpuTiming::timingMutex);auto found=gpuTiming::active.find(cmd);if(found==gpuTiming::active.end())return;
        gpuTiming::finished.push_back(std::move(found->second));gpuTiming::active.erase(found);
    }catch(...){}
}
void gpuTimingNote(VkCommandBuffer cmd,uint32_t kind,uint32_t,uint32_t) noexcept {
    if(kind==1){gpuTimingDispatch(cmd,0,0,0);return;}
    if(!gpuTiming::every())return;
    try{std::lock_guard<std::mutex> lock(gpuTiming::timingMutex);auto found=gpuTiming::active.find(cmd);if(found!=gpuTiming::active.end())++found->second.draws;}catch(...){}
}
// Before every dispatch (direct: group counts; indirect: 0,0,0) with the bound compute pipeline.
void gpuTimingDispatch(VkCommandBuffer cmd,uint32_t x,uint32_t y,uint32_t z) noexcept {
    if(!gpuTiming::every())return;
    try{
        uint64_t pipeline=0;
        uint64_t sets=0;
        {std::lock_guard<std::mutex> lock(gpuTiming::timingMutex);auto found=gpuTiming::active.find(cmd);if(found==gpuTiming::active.end())return;
            ++found->second.dispatches;pipeline=found->second.compute;sets=gpuTiming::setsHash(found->second.computeSets);}
        gpuTiming::Mark m{'D'};m.pipeline=pipeline;m.gx=x;m.gy=y;m.gz=z;m.sets=sets;
        const auto sites=gpuTiming::gameSites();m.site=sites.first;m.site2=sites.second;
        gpuTiming::mark(cmd,m,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }catch(...){}
}
// Resource identity of a descriptor set: storage images (by VkImage) and storage buffers (buffer, offset,
// range), per binding. Uniform buffers (per-View constants) and sampled inputs are excluded, so equal
// values in two Views mean the dispatch wrote/read-modified the very same storage resources.
namespace gpuTiming { inline std::mutex setMutex; inline std::unordered_map<VkDescriptorSet,uint64_t> setResources; }
void gpuTimingDescriptorWrites(uint32_t count,const VkWriteDescriptorSet* writes) noexcept {
    if(!gpuTiming::every()||!writes)return;
    try{
        std::unordered_map<VkDescriptorSet,uint64_t> local;
        for(uint32_t i=0;i<count;i++){
            const auto& w=writes[i];uint64_t h=0;
            auto mix=[&](uint64_t v){h^=v+0x9e3779b97f4a7c15ull+(h<<6)+(h>>2);};
            if(w.descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_IMAGE&&w.pImageInfo){
                std::lock_guard<std::mutex> lock(mutex);
                for(uint32_t j=0;j<w.descriptorCount;j++){uint64_t image=reinterpret_cast<uint64_t>(w.pImageInfo[j].imageView);
                    for(auto& [k,views]:imageViews){auto v=views.find(w.pImageInfo[j].imageView);if(v!=views.end()){image=reinterpret_cast<uint64_t>(v->second.image)^(uint64_t(v->second.range.baseMipLevel)<<56);break;}}
                    mix(uint64_t(w.dstBinding)<<40^j);mix(image);}
            }else if((w.descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER||w.descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)&&w.pBufferInfo){
                for(uint32_t j=0;j<w.descriptorCount;j++){mix(uint64_t(w.dstBinding)<<40^j);mix(reinterpret_cast<uint64_t>(w.pBufferInfo[j].buffer));mix(w.pBufferInfo[j].offset);mix(w.pBufferInfo[j].range);}
            }else continue;
            local[w.dstSet]^=h;
        }
        if(local.empty())return;
        std::lock_guard<std::mutex> lock(gpuTiming::setMutex);
        if(gpuTiming::setResources.size()>200000)gpuTiming::setResources.clear();
        for(auto& [s,h]:local)gpuTiming::setResources[s]=h;
    }catch(...){}
}
void gpuTimingSets(VkCommandBuffer cmd,VkPipelineBindPoint point,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t,const uint32_t*) noexcept {
    if(!gpuTiming::every()||point!=VK_PIPELINE_BIND_POINT_COMPUTE||!sets)return;
    try{
        std::array<uint64_t,4> hashes{};
        {std::lock_guard<std::mutex> lock(gpuTiming::setMutex);
            for(uint32_t i=0;i<count&&first+i<4;i++){auto f=gpuTiming::setResources.find(sets[i]);hashes[first+i]=f==gpuTiming::setResources.end()?0:f->second;}}
        std::lock_guard<std::mutex> lock(gpuTiming::timingMutex);auto found=gpuTiming::active.find(cmd);if(found==gpuTiming::active.end())return;
        for(uint32_t i=0;i<count&&first+i<4;i++)found->second.computeSets[first+i]=hashes[first+i];
    }catch(...){}
}
void gpuTimingBind(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipeline pipeline) noexcept {
    if(!gpuTiming::every()||point!=VK_PIPELINE_BIND_POINT_COMPUTE)return;
    try{std::lock_guard<std::mutex> lock(gpuTiming::timingMutex);auto found=gpuTiming::active.find(cmd);if(found!=gpuTiming::active.end())found->second.compute=reinterpret_cast<uint64_t>(pipeline);}catch(...){}
}
// Called once per completed pair on the XR completion thread.
inline void gpuTimingPair(VkDevice device,uint64_t pair,Log& log) noexcept {
    const auto n=gpuTiming::every();if(!n)return;
    try{
        using namespace gpuTiming;
        if(window.load()&&pair>=windowPair){window=false;harvestPair=pair+4;}
        if(harvestPair&&pair>=harvestPair){
            std::vector<Cmd> cmds;{std::lock_guard<std::mutex> lock(timingMutex);cmds.swap(finished);active.clear();}
            std::sort(cmds.begin(),cmds.end(),[](const Cmd& a,const Cmd& b){return a.order<b.order;});
            uint64_t base=UINT64_MAX;std::vector<std::vector<uint64_t>> all;
            for(const auto& c:cmds){
                std::vector<uint64_t> data(size_t(c.used)*2);
                if(c.used)deviceFn<PFN_vkGetQueryPoolResults>(gpuTiming::device,"vkGetQueryPoolResults")(gpuTiming::device,pool,c.first,c.used,data.size()*8,data.data(),16,VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
                if(c.used&&data[1])base=std::min(base,data[0]);
                all.push_back(std::move(data));
            }
            std::ostringstream o;o<<std::setprecision(4)<<"\"pair\":"<<windowPair<<",\"buffers\":"<<cmds.size()<<",\"period_ns\":"<<period<<",\"cmds\":[";
            for(size_t i=0;i<cmds.size();++i){
                const auto& c=cmds[i];const auto& d=all[i];
                o<<(i?",":"")<<"{\"o\":"<<c.order<<",\"segs\":[";
                bool first=true;
                for(uint32_t k=0;k+1<c.used;++k){
                    if(!d[k*2+1]||!d[(k+1)*2+1])continue;
                    const auto& m=c.marks[k];const auto& nm=c.marks[k+1];
                    const double start=double(d[k*2]-base)*period/1e6,ms=double(d[(k+1)*2]-d[k*2])*period/1e6;
                    // Segment k..k+1 is a render pass when mark k is 'B' (its draws are counted at mark k+1),
                    // otherwise the block of dispatches/transfers between passes.
                    o<<(first?"":",")<<"[\""<<m.kind<<"\","<<start<<','<<ms<<','<<m.width<<','<<m.height<<','<<m.colors<<','<<nm.draws<<','<<nm.dispatches<<','<<m.pipeline<<','<<m.gx<<','<<m.gy<<','<<m.gz<<','<<m.sets<<','<<m.site<<','<<m.site2<<']';first=false;
                }
                o<<"]}";
            }
            o<<']';log.event("gpu_timing",o.str());
            harvestPair=0;next=0;
        }
        if(!window.load()&&!harvestPair&&(pair+1)%n==0){
            if(ensurePool(device)){std::lock_guard<std::mutex> lock(timingMutex);finished.clear();active.clear();next=0;windowPair=pair+1;window=true;}
        }
    }catch(...){}
}
