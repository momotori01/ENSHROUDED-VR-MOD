#pragma once
// Opt-in diagnostic image readback (2026-10-05 DLSS black-frame investigation).
// The probe registers up to four game images (eye DLSS inputs/outputs) by slot; on luma frames
// the XR completion path reads each registered image back with its tracked layout and logs the
// mean RGB. RGBA16F and RGBA8 only. Read-only with respect to game state: the image is returned
// to its tracked layout and the queue submission waits on its own fence.
namespace diagnosticReadback {
struct Slot {VkImage image{};VkDevice device{};};
inline std::array<Slot,4> slots{};
inline float halfToFloat(uint16_t h){
    const uint32_t s=(h>>15)&1,e=(h>>10)&31,m=h&1023;
    if(!e)return (s?-1.f:1.f)*std::ldexp(float(m),-24);
    if(e==31)return m?std::numeric_limits<float>::quiet_NaN():(s?-1.f:1.f)*std::numeric_limits<float>::infinity();
    return (s?-1.f:1.f)*std::ldexp(float(m|1024),int(e)-25);
}
// Texel decoding for the formats the game uses for eye render targets (color, motion, depth).
enum class Kind{None,Half,Float,Unorm8,Unorm16,Snorm16,Uint8,Uint16,Uint32,B10G11R11,A2B10G10R10,D24};
struct Format{uint32_t bytes=0,channels=0;Kind kind=Kind::None;VkImageAspectFlags copyAspect=VK_IMAGE_ASPECT_COLOR_BIT,barrierAspect=VK_IMAGE_ASPECT_COLOR_BIT;};
inline Format formatOf(uint32_t f){
    constexpr VkImageAspectFlags D=VK_IMAGE_ASPECT_DEPTH_BIT,DS=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    switch(VkFormat(f)){
    case VK_FORMAT_R16G16B16A16_SFLOAT:return {8,3,Kind::Half};
    case VK_FORMAT_R16G16_SFLOAT:return {4,2,Kind::Half};
    case VK_FORMAT_R16_SFLOAT:return {2,1,Kind::Half};
    case VK_FORMAT_R32_SFLOAT:return {4,1,Kind::Float};
    case VK_FORMAT_R32G32_SFLOAT:return {8,2,Kind::Float};
    case VK_FORMAT_R32G32B32A32_SFLOAT:return {16,3,Kind::Float};
    case VK_FORMAT_R8G8B8A8_UNORM:case VK_FORMAT_R8G8B8A8_SRGB:case VK_FORMAT_B8G8R8A8_UNORM:case VK_FORMAT_B8G8R8A8_SRGB:return {4,3,Kind::Unorm8};
    case VK_FORMAT_R8G8_UNORM:return {2,2,Kind::Unorm8};
    case VK_FORMAT_R8_UNORM:return {1,1,Kind::Unorm8};
    case VK_FORMAT_R16G16B16A16_UNORM:return {8,3,Kind::Unorm16};
    case VK_FORMAT_R16G16_UNORM:return {4,2,Kind::Unorm16};
    case VK_FORMAT_R16G16_SNORM:return {4,2,Kind::Snorm16};
    case VK_FORMAT_R16_UNORM:return {2,1,Kind::Unorm16};
    case VK_FORMAT_R8_UINT:return {1,1,Kind::Uint8};
    case VK_FORMAT_R8G8_UINT:return {2,2,Kind::Uint8};
    case VK_FORMAT_R8G8B8A8_UINT:return {4,4,Kind::Uint8};
    case VK_FORMAT_R16_UINT:return {2,1,Kind::Uint16};
    case VK_FORMAT_R16G16_UINT:return {4,2,Kind::Uint16};
    case VK_FORMAT_R32_UINT:return {4,1,Kind::Uint32};
    case VK_FORMAT_R32G32_UINT:return {8,2,Kind::Uint32};
    case VK_FORMAT_R32G32B32A32_UINT:return {16,4,Kind::Uint32};
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:return {4,3,Kind::B10G11R11};
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:return {4,3,Kind::A2B10G10R10};
    case VK_FORMAT_D32_SFLOAT:return {4,1,Kind::Float,D,D};
    case VK_FORMAT_D32_SFLOAT_S8_UINT:return {4,1,Kind::Float,D,DS};
    case VK_FORMAT_D24_UNORM_S8_UINT:return {4,1,Kind::D24,D,DS};
    case VK_FORMAT_X8_D24_UNORM_PACK32:return {4,1,Kind::D24,D,D};
    case VK_FORMAT_D16_UNORM:return {2,1,Kind::Unorm16,D,D};
    default:return {};
    }
}
inline float smallFloat(uint32_t x,uint32_t mantissaBits){
    const uint32_t m=x&((1u<<mantissaBits)-1),e=(x>>mantissaBits)&31;
    if(!e)return std::ldexp(float(m),-14-int(mantissaBits));
    if(e==31)return m?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity();
    return std::ldexp(float(m|(1u<<mantissaBits)),int(e)-15-int(mantissaBits));
}
inline float channel(const uint8_t* t,const Format& f,uint32_t c){
    uint32_t w=0;
    switch(f.kind){
    case Kind::Half:{uint16_t h;std::memcpy(&h,t+2*c,2);return halfToFloat(h);}
    case Kind::Float:{float v;std::memcpy(&v,t+4*c,4);return v;}
    case Kind::Unorm8:return t[c]/255.f;
    case Kind::Unorm16:{uint16_t h;std::memcpy(&h,t+2*c,2);return h/65535.f;}
    case Kind::Snorm16:{int16_t h;std::memcpy(&h,t+2*c,2);return std::max(-1.f,h/32767.f);}
    case Kind::Uint8:return float(t[c]);
    case Kind::Uint16:{uint16_t h;std::memcpy(&h,t+2*c,2);return float(h);}
    case Kind::Uint32:{std::memcpy(&w,t+4*c,4);return float(w);}
    case Kind::B10G11R11:std::memcpy(&w,t,4);return c==0?smallFloat(w&2047,6):c==1?smallFloat((w>>11)&2047,6):smallFloat((w>>22)&1023,5);
    case Kind::A2B10G10R10:std::memcpy(&w,t,4);return float((w>>(10*c))&1023)/1023.f;
    case Kind::D24:std::memcpy(&w,t,4);return float(w&0xffffff)/16777215.f;
    default:return 0;
    }
}
struct Stats {double mean=-1;double zero=-1;uint64_t nonFinite=0;};
inline Stats measure(const uint8_t* data,uint64_t texels,const Format& f,uint64_t stride=1){
    Stats r;double sum=0;uint64_t count=0,zeros=0;
    for(uint64_t i=0;i<texels;i+=stride){
        const auto* t=data+i*f.bytes;double texel=0;bool finite=true,zero=true;
        for(uint32_t c=0;c<f.channels;c++){const float v=channel(t,f,c);if(!std::isfinite(v)){finite=false;break;}zero&=v==0.f;texel+=std::fabs(v);}
        if(!finite){++r.nonFinite;continue;}
        sum+=texel/f.channels;zeros+=zero;++count;
    }
    if(count){r.mean=sum/double(count);r.zero=double(zeros)/double(count);}
    return r;
}
// Most frequent raw texel value (single-channel formats only, e.g. visibility IDs or depth) and its share.
inline std::pair<uint32_t,double> dominant(const uint8_t* data,uint64_t texels,const Format& f){
    if(f.channels!=1||f.bytes>4||!texels)return {0,-1};
    std::unordered_map<uint32_t,uint32_t> counts;counts.reserve(4096);
    for(uint64_t i=0;i<texels;i++){uint32_t v=0;std::memcpy(&v,data+i*f.bytes,f.bytes);++counts[v];}
    uint32_t best=0,bestCount=0;for(const auto& [v,c]:counts)if(c>bestCount){best=v;bestCount=c;}
    return {best,double(bestCount)/double(texels)};
}
struct Result {bool ok=false;double mean=-1;uint64_t nonFinite=0;uint32_t layout=0,width=0,height=0,format=0;std::string error;};
inline Result read(Vulkan& v,VkImage image,VkDevice device){
    Result r;
    diagnostic::NativeImageEvidence e;
    if(evrGetNativeImageEvidence(device,image,&e)){r.error="no evidence";return r;}
    r.layout=e.layout;r.width=e.width;r.height=e.height;r.format=e.format;
    if(!e.ready){r.error="not ready";return r;}
    const auto f=formatOf(e.format);const uint32_t bpp=f.bytes;
    if(!bpp){r.error="unsupported format";return r;}
    if(!(e.usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)){r.error="no transfer source usage";return r;}
    VkBuffer buffer{};VkDeviceMemory memory{};VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};
    auto cleanup=[&]{if(fence)v.vkDestroyFence(v.device,fence,nullptr);if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);if(buffer)v.vkDestroyBuffer(v.device,buffer,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);};
    try{
        const VkDeviceSize bytes=VkDeviceSize(e.width)*e.height*bpp;
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&buffer),"diag readback buffer");
        VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,buffer,&req);
        memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkCheck(v.vkBindBufferMemory(v.device,buffer,memory,0),"diag readback bind");
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=v.family;
        vkCheck(v.vkCreateCommandPool(v.device,&pc,nullptr,&pool),"diag readback pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(v.device,&ca,&cmd),"diag readback command");
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(v.vkBeginCommandBuffer(cmd,&bi),"diag readback begin");
        VkImageMemoryBarrier im{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        im.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;im.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        im.oldLayout=VkImageLayout(e.layout);im.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        im.srcQueueFamilyIndex=im.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;im.image=image;im.subresourceRange={f.barrierAspect,0,1,0,1};
        v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&im);
        VkBufferImageCopy copy{};copy.imageSubresource={f.copyAspect,0,0,1};copy.imageExtent={e.width,e.height,1};
        v.vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
        im.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;im.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        im.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;im.newLayout=VkImageLayout(e.layout);
        v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&im);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        vkCheck(v.vkEndCommandBuffer(cmd),"diag readback end");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(v.device,&fc,nullptr,&fence),"diag readback fence");
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&cmd;
        vkCheck(v.vkQueueSubmit(v.queue,1,&s,fence),"diag readback submit");
        vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"diag readback wait");
        void* mapped{};vkCheck(v.vkMapMemory(v.device,memory,0,bytes,0,&mapped),"diag readback map");
        const auto stats=measure(static_cast<const uint8_t*>(mapped),uint64_t(e.width)*e.height,f,4);
        v.vkUnmapMemory(v.device,memory);
        r.mean=stats.mean;r.nonFinite=stats.nonFinite;r.ok=true;
    }catch(const std::exception& x){r.error=x.what();}
    cleanup();return r;
}
// Eye image sweep (EVR_EYE_IMAGE_SWEEP=1 with the eye de-alias window): every image the game binds
// while building an eye View's resources is tracked (quietly) and, on luma frames, the central
// region (at most 256x256) of each readable one is copied back in one submission. Mean |value| and
// the fraction of all-zero texels per image locate the first black stage. Diagnostic only.
struct SweepImage {VkImage image{};VkDevice device{};uint32_t eye=0;};
inline std::vector<SweepImage> sweep; // guarded by the layer mutex
// Depth write sources for sweep depth images: begin-rendering load/clear, draw counts, clears and
// transfer writes, with the game call stack. Separate mutex; draw hooks only touch it when enabled.
inline std::mutex depthMutex;
inline std::unordered_map<VkImage,uint32_t> sweepDepth;
struct ActiveDepth {VkImage image{};uint32_t eye=0,draws=0,direct=0,small=0;uint64_t sequence=0;};
inline std::unordered_map<VkCommandBuffer,ActiveDepth> activeDepth;
inline std::atomic<uint64_t> depthOps{0};
constexpr uint64_t depthOpLimit=60000;
// Ops are written only while recording a luma pair (set by evrFinishNativeXr for the next pair), so the
// budget covers the measured frames instead of the warm-up frames before live pairs.
inline std::atomic<bool> depthLogWindow{false};
inline std::atomic<uint64_t> depthLogPair{0};
inline bool sweepRequested(){static const bool on=[]{const auto f=std::getenv("EVR_EYE_IMAGE_SWEEP");return f&&!std::strcmp(f,"1");}();return on;}
struct SweepRead {std::string inventory,frame;unsigned read=0,skipped=0;};
// Optional full dumps use the existing dedicated eye images, one request and <=256 MiB.
// Post-pair contents only: the manifest does not claim an intermediate shader-stage snapshot.
inline SweepRead readSweep(Vulkan& v,bool withInventory,const std::filesystem::path& dumpDir={},const std::vector<size_t>& selected={}){
    SweepRead out;std::vector<SweepImage> list;{std::lock_guard<std::mutex> lock(mutex);list=sweep;}
    struct Item{size_t index=0;VkImage image{};diagnostic::NativeImageEvidence e;Format f;uint32_t x=0,y=0,w=0,h=0;VkDeviceSize offset=0;std::string reason;};
    std::vector<Item> items;VkDeviceSize total=0;std::ostringstream inv;
    const bool dump=!dumpDir.empty();
    if(dump&&(selected.empty()||selected.size()>16)){out.frame="\"error\":\"invalid dump selection\"";return out;}
    if(dump)for(size_t n=0;n<selected.size();++n){
        if(selected[n]>=list.size()||std::find(selected.begin(),selected.begin()+n,selected[n])!=selected.begin()+n){
            out.frame="\"error\":\"missing or duplicate dump index\"";return out;}
    }
    for(size_t i=0;i<list.size();++i){
        if(dump&&std::find(selected.begin(),selected.end(),i)==selected.end())continue;
        Item it;it.index=i;it.image=list[i].image;
        if(dump){std::lock_guard<std::mutex> lock(mutex);
            const auto owner=imageCreation.find(key(list[i].device));
            if(owner==imageCreation.end()){out.frame="\"error\":\"missing dump image metadata\"";return out;}
            const auto ci=owner->second.find(it.image);
            if(ci==owner->second.end()||ci->second.imageType!=VK_IMAGE_TYPE_2D||ci->second.arrayLayers!=1||ci->second.samples!=VK_SAMPLE_COUNT_1_BIT){
                out.frame="\"error\":\"dump requires single layer 2D image\"";return out;}
        }
        if(evrGetNativeImageEvidence(list[i].device,list[i].image,&it.e))it.reason="no evidence";
        else{it.f=formatOf(it.e.format);
            if(!it.e.ready)it.reason="not ready";else if(!it.f.bytes)it.reason="unsupported format";
            else if(!(it.e.usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT))it.reason="no transfer source usage";}
        if(withInventory)inv<<(inv.tellp()>0?",":"")<<"{\"i\":"<<i<<",\"image\":"<<handleText(list[i].image)<<",\"eye\":"<<list[i].eye<<",\"w\":"<<it.e.width<<",\"h\":"<<it.e.height
            <<",\"format\":"<<it.e.format<<",\"usage\":"<<it.e.usage<<",\"layout\":"<<it.e.layout<<",\"readable\":"<<(it.reason.empty()?"true":"false")<<",\"reason\":"<<jsonString(it.reason)<<'}';
        if(!it.reason.empty()){++out.skipped;continue;}
        it.w=dump?it.e.width:std::min(it.e.width,256u);it.h=dump?it.e.height:std::min(it.e.height,256u);it.x=(it.e.width-it.w)/2;it.y=(it.e.height-it.h)/2;
        it.offset=total;total+=(VkDeviceSize(it.w)*it.h*it.f.bytes+15)&~VkDeviceSize(15);items.push_back(it);
        if(dump&&total>256ull*1024*1024){out.frame="\"error\":\"dump exceeds 256 MiB\"";return out;}
    }
    out.inventory=inv.str();
    if(dump&&items.size()!=selected.size()){out.frame="\"error\":\"dump contains unreadable image\"";return out;}
    std::vector<std::pair<double,double>> stats(list.size(),{-1,-1});
    std::vector<std::pair<uint32_t,double>> modes(list.size(),{0,-1});
    if(!items.empty()){
        VkBuffer buffer{};VkDeviceMemory memory{};VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};
        auto cleanup=[&]{if(fence)v.vkDestroyFence(v.device,fence,nullptr);if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);if(buffer)v.vkDestroyBuffer(v.device,buffer,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);};
        try{
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=total;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&buffer),"sweep buffer");
            VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,buffer,&req);
            memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkCheck(v.vkBindBufferMemory(v.device,buffer,memory,0),"sweep bind");
            VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=v.family;
            vkCheck(v.vkCreateCommandPool(v.device,&pc,nullptr,&pool),"sweep pool");
            VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
            vkCheck(v.vkAllocateCommandBuffers(v.device,&ca,&cmd),"sweep command");
            VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkCheck(v.vkBeginCommandBuffer(cmd,&bi),"sweep begin");
            std::vector<VkImageMemoryBarrier> to,back;
            for(const auto& it:items){
                VkImageMemoryBarrier im{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                im.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;im.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                im.oldLayout=VkImageLayout(it.e.layout);im.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                im.srcQueueFamilyIndex=im.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;im.image=it.image;im.subresourceRange={it.f.barrierAspect,0,1,0,1};
                to.push_back(im);
                im.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;im.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
                im.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;im.newLayout=VkImageLayout(it.e.layout);back.push_back(im);
            }
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,uint32_t(to.size()),to.data());
            for(const auto& it:items){
                VkBufferImageCopy copy{};copy.bufferOffset=it.offset;copy.imageSubresource={it.f.copyAspect,0,0,1};
                copy.imageOffset={int32_t(it.x),int32_t(it.y),0};copy.imageExtent={it.w,it.h,1};
                v.vkCmdCopyImageToBuffer(cmd,it.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
            }
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,uint32_t(back.size()),back.data());
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
            vkCheck(v.vkEndCommandBuffer(cmd),"sweep end");
            VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(v.device,&fc,nullptr,&fence),"sweep fence");
            VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&cmd;
            vkCheck(v.vkQueueSubmit(v.queue,1,&s,fence),"sweep submit");
            vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"sweep wait");
            void* mapped{};vkCheck(v.vkMapMemory(v.device,memory,0,total,0,&mapped),"sweep map");
            try{
                if(dump)std::filesystem::create_directories(dumpDir);
                for(const auto& it:items){
                    const auto* data=static_cast<const uint8_t*>(mapped)+it.offset;
                    const auto st=measure(data,uint64_t(it.w)*it.h,it.f,dump?16:1);stats[it.index]={st.mean,st.zero};
                    if(!dump)modes[it.index]=dominant(data,uint64_t(it.w)*it.h,it.f);
                    if(dump){std::ofstream file(dumpDir/("image-"+std::to_string(it.index)+".bin"),std::ios::binary);
                        file.write(reinterpret_cast<const char*>(data),std::streamsize(uint64_t(it.w)*it.h*it.f.bytes));file.close();
                        if(!file)throw std::runtime_error("dump file write failed");}
                    ++out.read;
                }
            }catch(...){v.vkUnmapMemory(v.device,memory);throw;}
            v.vkUnmapMemory(v.device,memory);
        }catch(const std::exception& x){out.frame="\"error\":"+jsonString(x.what())+',';}
        cleanup();
    }
    std::ostringstream o;o<<std::setprecision(5)<<"\"m\":[";
    for(size_t i=0;i<stats.size();++i)o<<(i?",":"")<<stats[i].first;
    o<<"],\"z\":[";for(size_t i=0;i<stats.size();++i)o<<(i?",":"")<<stats[i].second;
    o<<"],\"d\":[";for(size_t i=0;i<modes.size();++i)o<<(i?",":"")<<modes[i].second;
    o<<"],\"dv\":[";for(size_t i=0;i<modes.size();++i)o<<(i?",":"")<<modes[i].first;
    o<<"],\"read\":"<<out.read<<",\"skipped\":"<<out.skipped;
    out.frame+=o.str();return out;
}
}
extern "C" __declspec(dllexport) int evrTrackDiagnosticImage(VkDevice device,VkImage image,uint32_t slot) noexcept {
    try{
        if(slot>=diagnosticReadback::slots.size()||!image)return 2;
        uint64_t generation=0;bool registered=false;std::string aliases;
        {std::lock_guard<std::mutex> lock(mutex);auto d=trackedImages.find(key(device));
            if(d!=trackedImages.end()){auto found=d->second.find(image);if(found!=d->second.end())generation=found->second;}
            registered=registerNativeImageLocked(device,image,generation);aliases=imageAliasReportLocked(image);
            if(registered)diagnosticReadback::slots[slot]={image,device};
        }
        note("diagnostic_image_registered","\"slot\":"+std::to_string(slot)+",\"image\":"+handleText(image)+",\"generation\":"+std::to_string(generation)+",\"registered\":"+(registered?"true":"false")+","+aliases);
        return registered?0:1;
    }catch(...){return 1;}
}
void forgetSweepImageLocked(VkImage image){
    {std::lock_guard<std::mutex> depthLock(diagnosticReadback::depthMutex);diagnosticReadback::sweepDepth.erase(image);}
    auto& list=diagnosticReadback::sweep;
    list.erase(std::remove_if(list.begin(),list.end(),[&](const diagnosticReadback::SweepImage& s){return s.image==image;}),list.end());
}
void sweepDepthRendering(VkCommandBuffer cmd,const VkRenderingInfo* info) noexcept {
    if(!diagnosticReadback::sweepRequested()||!info)return;
    try{
        using namespace diagnosticReadback;
        VkImage image{};
        if(info->pDepthAttachment&&info->pDepthAttachment->imageView){
            std::lock_guard<std::mutex> lock(mutex);auto dev=imageViews.find(key(cmd));
            if(dev!=imageViews.end()){auto v=dev->second.find(info->pDepthAttachment->imageView);if(v!=dev->second.end())image=v->second.image;}
        }
        uint32_t eye=0;uint64_t sequence=0;
        {std::lock_guard<std::mutex> lock(depthMutex);activeDepth.erase(cmd);
            if(!image)return;auto found=sweepDepth.find(image);if(found==sweepDepth.end())return;
            eye=found->second;sequence=++depthOps;activeDepth[cmd]=ActiveDepth{image,eye,0,0,0,sequence};}
        if(sequence>depthOpLimit||!depthLogWindow.load())return;
        const auto& a=*info->pDepthAttachment;std::ostringstream o;o<<std::setprecision(9);
        o<<"\"kind\":\"rendering\",\"seq\":"<<sequence<<",\"image\":"<<handleText(image)<<",\"eye\":"<<eye<<",\"cmd\":"<<handleText(cmd)
         <<",\"load_op\":"<<a.loadOp<<",\"store_op\":"<<a.storeOp<<",\"layout\":"<<a.imageLayout<<",\"clear\":"<<a.clearValue.depthStencil.depth
         <<",\"area\":["<<info->renderArea.offset.x<<','<<info->renderArea.offset.y<<','<<info->renderArea.extent.width<<','<<info->renderArea.extent.height
         <<"],\"colors\":"<<info->colorAttachmentCount<<",\"pair\":"<<depthLogPair.load()<<",\"thread\":"<<GetCurrentThreadId();
        if(info->colorAttachmentCount&&info->pColorAttachments[0].imageView){
            std::lock_guard<std::mutex> lock(mutex);auto dev=imageViews.find(key(cmd));
            if(dev!=imageViews.end()){auto v=dev->second.find(info->pColorAttachments[0].imageView);
                if(v!=dev->second.end())o<<",\"color0\":"<<handleText(v->second.image)<<",\"color0_load\":"<<info->pColorAttachments[0].loadOp;}
        }
        o<<",\"game_rva_stack\":"<<stack();
        note("sweep_depth_op",o.str());
    }catch(...){}
}
void sweepDepthDraw(VkCommandBuffer cmd,uint32_t kind) noexcept {
    if(!diagnosticReadback::sweepRequested())return;
    try{std::lock_guard<std::mutex> lock(diagnosticReadback::depthMutex);auto found=diagnosticReadback::activeDepth.find(cmd);
        if(found!=diagnosticReadback::activeDepth.end()){++found->second.draws;found->second.direct+=kind>=1;found->second.small+=kind==2;}}catch(...){}
}
void sweepDepthEnd(VkCommandBuffer cmd) noexcept {
    if(!diagnosticReadback::sweepRequested())return;
    try{
        diagnosticReadback::ActiveDepth active;
        {std::lock_guard<std::mutex> lock(diagnosticReadback::depthMutex);auto found=diagnosticReadback::activeDepth.find(cmd);
            if(found==diagnosticReadback::activeDepth.end())return;active=found->second;diagnosticReadback::activeDepth.erase(found);}
        if(active.sequence>diagnosticReadback::depthOpLimit||!diagnosticReadback::depthLogWindow.load())return;
        note("sweep_depth_op","\"kind\":\"rendering_end\",\"seq\":"+std::to_string(active.sequence)+",\"image\":"+handleText(active.image)+",\"eye\":"+std::to_string(active.eye)+
            ",\"cmd\":"+handleText(cmd)+",\"draws\":"+std::to_string(active.draws)+",\"direct\":"+std::to_string(active.direct)+",\"small_direct\":"+std::to_string(active.small));
    }catch(...){}
}
void sweepDepthClearAttachments(VkCommandBuffer cmd,uint32_t count,const VkClearAttachment* attachments,uint32_t rects,const VkClearRect* rect) noexcept {
    if(!diagnosticReadback::sweepRequested()||!attachments)return;
    try{
        diagnosticReadback::ActiveDepth active;
        {std::lock_guard<std::mutex> lock(diagnosticReadback::depthMutex);auto found=diagnosticReadback::activeDepth.find(cmd);
            if(found==diagnosticReadback::activeDepth.end())return;active=found->second;}
        for(uint32_t i=0;i<count;i++){
            if(!(attachments[i].aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT))continue;
            if(!diagnosticReadback::depthLogWindow.load())return;
            const auto sequence=++diagnosticReadback::depthOps;if(sequence>diagnosticReadback::depthOpLimit)return;
            std::ostringstream o;o<<std::setprecision(9)<<"\"kind\":\"clear_attachments\",\"seq\":"<<sequence<<",\"image\":"<<handleText(active.image)<<",\"eye\":"<<active.eye
             <<",\"cmd\":"<<handleText(cmd)<<",\"clear\":"<<attachments[i].clearValue.depthStencil.depth<<",\"rects\":"<<rects;
            if(rects&&rect)o<<",\"rect\":["<<rect[0].rect.offset.x<<','<<rect[0].rect.offset.y<<','<<rect[0].rect.extent.width<<','<<rect[0].rect.extent.height<<']';
            o<<",\"draws_before\":"<<active.draws<<",\"game_rva_stack\":"<<stack();
            note("sweep_depth_op",o.str());
        }
    }catch(...){}
}
void sweepDepthTransfer(VkCommandBuffer cmd,VkImage image,const char* kind,const VkClearDepthStencilValue* value) noexcept {
    if(!diagnosticReadback::sweepRequested())return;
    try{
        uint32_t eye=0;
        {std::lock_guard<std::mutex> lock(diagnosticReadback::depthMutex);auto found=diagnosticReadback::sweepDepth.find(image);if(found==diagnosticReadback::sweepDepth.end())return;eye=found->second;}
        if(!diagnosticReadback::depthLogWindow.load())return;
        const auto sequence=++diagnosticReadback::depthOps;if(sequence>diagnosticReadback::depthOpLimit)return;
        std::ostringstream o;o<<std::setprecision(9)<<"\"kind\":"<<jsonString(kind)<<",\"seq\":"<<sequence<<",\"image\":"<<handleText(image)<<",\"eye\":"<<eye<<",\"cmd\":"<<handleText(cmd);
        if(value)o<<",\"clear\":"<<value->depth;
        o<<",\"game_rva_stack\":"<<stack();
        note("sweep_depth_op",o.str());
    }catch(...){}
}
