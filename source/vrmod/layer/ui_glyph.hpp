// The game's button pictures ("[Y] Pick up") drawn over with the VR input's (2026-10-21, the user: "replace every button
// label with the VR one"; docs/analysis/vr_button_glyphs_20261021). Mock game-20261021-142914 (EVR_GLYPH_OBSERVE=1): every
// picture is a VkImage of its own (64x64 R8G8B8A8, 80x64 / 96x64 for a stick with its direction), uploaded once at the start
// with vkCmdCopyBufferToImage in TRANSFER_DST_OPTIMAL from mapped memory and then turned to SHADER_READ_ONLY_OPTIMAL; the
// copied bytes are the game data's (56 pictures known by their FNV-1a 64).
// Here: an upload whose bytes are one of the pictures the mod replaces (include/evr/button_glyph_catalog.hpp) keeps its image
// and a copy of the game's pixels. The hooks publish the scene, what the hands hold and the controllers (evrSetButtonGlyphs,
// controller_ui_fixture.hpp); before a submission on the queue the pictures went up on, the images whose label changed get
// the VR picture (include/evr/button_glyph_labels.hpp, drawn ahead on a thread) or the game's own back, in a command buffer
// of the layer's (shader read -> transfer -> copy -> shader read). EVR_BUTTON_GLYPHS=0: the game's pictures stay.
// EVR_GLYPH_OBSERVE=1 also logs every small RGBA upload, name and barrier (ui_glyph_upload ...).
#include <evr/button_glyph_labels.hpp>
#include <map>
#include <thread>
namespace uiGlyph {
inline const bool observe=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GLYPH_OBSERVE",v,sizeof v);return n==1&&v[0]=='1';}();
inline const bool replace=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_BUTTON_GLYPHS",v,sizeof v);return !(n==1&&v[0]=='0');}();
// concurrent: VK_SHARING_MODE_CONCURRENT over `families`; owner: the family an ownership transfer (a barrier with two
// families) gave the image to, UINT32_MAX while none was seen.
struct Candidate{uint32_t width=0,height=0,format=0;unsigned uploads=0,barriers=0;bool concurrent=false;std::array<uint32_t,4> families{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};uint32_t owner=UINT32_MAX;};
struct Glyph{VkImage image{};evr::glyph::Pic pic=evr::glyph::Pic::None;uint32_t width=0,height=0;std::vector<uint8_t> original;
    VkCommandBuffer uploadCmd{};VkQueue queue{};uint32_t uploadFamily=UINT32_MAX;uint64_t submittedAt=0;std::wstring applied;bool hasApplied=false;};
inline std::mutex guard;
inline std::unordered_map<VkImage,Candidate> candidates;
inline std::unordered_map<VkDeviceMemory,const uint8_t*> mapped;   // the memory's offset 0 on the host
inline std::vector<Glyph> glyphs;
inline std::atomic<unsigned> candidateLogs{0},uploadLogs{0},nameLogs{0},barrierLogs{0},knownLogs{0},updates{0};
inline std::atomic<uint32_t> wanted{0xffffffffu};                  // family | scene << 4 | held << 8, from the hooks
inline thread_local bool own=false;                                // the layer's own recording and submission
inline bool small(const VkImageCreateInfo& i){
    return i.imageType==VK_IMAGE_TYPE_2D&&(i.format==VK_FORMAT_R8G8B8A8_UNORM||i.format==VK_FORMAT_R8G8B8A8_SRGB||i.format==VK_FORMAT_B8G8R8A8_UNORM)&&
        i.mipLevels==1&&i.arrayLayers==1&&i.extent.width>=16&&i.extent.width<=128&&i.extent.height>=16&&i.extent.height<=128&&(i.usage&VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}
inline bool active(){return observe||replace;}
inline void created(const VkImageCreateInfo* info,VkImage image) noexcept {
    if(!active()||own||!info||!small(*info))return;
    try{{std::lock_guard<std::mutex> lock(guard);if(candidates.size()<20000){Candidate c{info->extent.width,info->extent.height,uint32_t(info->format)};
            c.concurrent=info->sharingMode==VK_SHARING_MODE_CONCURRENT;for(uint32_t i=0;i<info->queueFamilyIndexCount&&i<4&&info->pQueueFamilyIndices;++i)c.families[i]=info->pQueueFamilyIndices[i];
            candidates[image]=c;}}
        if(observe&&candidateLogs.fetch_add(1)<300)note("ui_glyph_candidate","\"image\":"+handleText(image)+",\"width\":"+std::to_string(info->extent.width)+",\"height\":"+std::to_string(info->extent.height)+
            ",\"format\":"+std::to_string(info->format)+",\"usage\":"+std::to_string(info->usage)+",\"tiling\":"+std::to_string(info->tiling));
    }catch(...){}
}
inline void destroyed(VkImage image) noexcept {
    if(!active())return;
    try{std::lock_guard<std::mutex> lock(guard);candidates.erase(image);
        glyphs.erase(std::remove_if(glyphs.begin(),glyphs.end(),[&](const Glyph& g){return g.image==image;}),glyphs.end());}catch(...){}
}
inline void memoryMapped(VkDeviceMemory memory,VkDeviceSize offset,void* data) noexcept {
    if(!active()||!data)return;try{std::lock_guard<std::mutex> lock(guard);mapped[memory]=static_cast<const uint8_t*>(data)-offset;}catch(...){}
}
inline void memoryUnmapped(VkDeviceMemory memory) noexcept {if(!active())return;try{std::lock_guard<std::mutex> lock(guard);mapped.erase(memory);}catch(...){}}
inline void copied(VkCommandBuffer cmd,VkBuffer src,VkImage dst,VkImageLayout layout,uint32_t count,const VkBufferImageCopy* regions) noexcept {
    if(!active()||own||!regions)return;
    try{
        Candidate c;const uint8_t* base=nullptr;VkDeviceSize memoryOffset=0;
        {std::lock_guard<std::mutex> lock(guard);auto f=candidates.find(dst);if(f==candidates.end())return;c=f->second;++f->second.uploads;}
        {std::lock_guard<std::mutex> lock(mutex);for(auto& d:bufferAudits){auto b=d.second.find(src);if(b!=d.second.end()){
            std::lock_guard<std::mutex> g(guard);auto m=mapped.find(b->second.memory);if(m!=mapped.end()){base=m->second;memoryOffset=b->second.memoryOffset;}break;}}}
        for(uint32_t r=0;r<count;++r){const auto& g=regions[r];
            const uint32_t w=g.imageExtent.width,h=g.imageExtent.height,row=g.bufferRowLength?g.bufferRowLength:w;
            uint64_t hash=0;std::string head;const uint8_t* p=nullptr;
            if(base&&row==w&&w&&h&&w<=256&&h<=256){p=base+memoryOffset+g.bufferOffset;hash=evr::glyph::fnv64(p,size_t(w)*h*4);
                static const char* d="0123456789abcdef";for(size_t i=0;i<16;++i){head+=d[p[i]>>4];head+=d[p[i]&15];}}
            const auto pic=p&&g.imageOffset.x==0&&g.imageOffset.y==0&&w==c.width&&h==c.height?evr::glyph::findPicture(hash,w,h):evr::glyph::Pic::None;
            if(pic!=evr::glyph::Pic::None&&replace){
                std::lock_guard<std::mutex> lock(guard);
                Glyph* found=nullptr;for(auto& e:glyphs)if(e.image==dst)found=&e;
                if(!found&&glyphs.size()<256){glyphs.emplace_back();found=&glyphs.back();}
                if(found){*found={};found->image=dst;found->pic=pic;found->width=w;found->height=h;found->original.assign(p,p+size_t(w)*h*4);found->uploadCmd=cmd;}
                if(knownLogs.fetch_add(1)<64)note("ui_glyph_known","\"image\":"+handleText(dst)+",\"picture\":"+std::to_string(unsigned(pic))+",\"width\":"+std::to_string(w)+",\"height\":"+std::to_string(h)+
                    ",\"concurrent\":"+(c.concurrent?"true":"false")+",\"families\":["+std::to_string(int(c.families[0]))+","+std::to_string(int(c.families[1]))+","+std::to_string(int(c.families[2]))+"]");
            }
            if(observe&&uploadLogs.fetch_add(1)<800)note("ui_glyph_upload","\"image\":"+handleText(dst)+",\"cmd\":"+handleText(cmd)+",\"width\":"+std::to_string(c.width)+",\"height\":"+std::to_string(c.height)+
                ",\"region\":["+std::to_string(g.imageOffset.x)+","+std::to_string(g.imageOffset.y)+","+std::to_string(w)+","+std::to_string(h)+"],\"row\":"+std::to_string(g.bufferRowLength)+
                ",\"buffer_offset\":"+std::to_string(g.bufferOffset)+",\"layout\":"+std::to_string(layout)+",\"mapped\":"+(base?"true":"false")+",\"fnv\":\""+[&]{char s[24];snprintf(s,sizeof s,"%016llx",(unsigned long long)hash);return std::string(s);}()+
                "\",\"head\":\""+head+"\",\"upload\":"+std::to_string(c.uploads+1));
        }
    }catch(...){}
}
inline void named(const VkDebugUtilsObjectNameInfoEXT* info) noexcept {
    if(!observe||!info||info->objectType!=VK_OBJECT_TYPE_IMAGE||!info->pObjectName)return;
    try{const VkImage image=(VkImage)info->objectHandle;bool candidate=false;{std::lock_guard<std::mutex> lock(guard);candidate=candidates.count(image)!=0;}
        if(candidate&&nameLogs.fetch_add(1)<600)note("ui_glyph_named","\"image\":"+handleText(image)+",\"name\":"+jsonString(info->pObjectName));}catch(...){}
}
inline void barrier(VkCommandBuffer cmd,VkImage image,VkImageLayout from,VkImageLayout to,uint32_t srcFamily=VK_QUEUE_FAMILY_IGNORED,uint32_t dstFamily=VK_QUEUE_FAMILY_IGNORED) noexcept {
    if(!active()||own)return;
    try{bool log=false,transfer=false;
        {std::lock_guard<std::mutex> lock(guard);auto f=candidates.find(image);
            if(f!=candidates.end()&&f->second.uploads){
                if(srcFamily!=dstFamily&&srcFamily!=VK_QUEUE_FAMILY_IGNORED&&dstFamily!=VK_QUEUE_FAMILY_IGNORED&&f->second.owner!=dstFamily){f->second.owner=dstFamily;transfer=true;}
                if(f->second.barriers<3){++f->second.barriers;log=true;}}}
        if((observe&&log&&barrierLogs.fetch_add(1)<600)||(transfer&&barrierLogs.fetch_add(1)<600))note("ui_glyph_barrier","\"image\":"+handleText(image)+",\"cmd\":"+handleText(cmd)+",\"old_layout\":"+std::to_string(from)+",\"new_layout\":"+std::to_string(to)+
            ",\"src_family\":"+std::to_string(int(srcFamily))+",\"dst_family\":"+std::to_string(int(dstFamily)));}catch(...){}
}
// The queue a picture's upload went to the GPU on (nativeCommandSubmitted, per command buffer).
inline void submitted(VkQueue queue,VkCommandBuffer cmd) noexcept {
    if(!replace||own)return;
    try{uint32_t family=UINT32_MAX;
        {std::lock_guard<std::mutex> lock(guard);bool any=false;for(auto& g:glyphs)any=any||(g.uploadCmd==cmd&&!g.submittedAt);if(!any)return;}
        {std::lock_guard<std::mutex> lock(mutex);auto f=queueFamilies.find(queue);if(f!=queueFamilies.end())family=f->second;}
        std::lock_guard<std::mutex> lock(guard);for(auto& g:glyphs)if(g.uploadCmd==cmd&&!g.submittedAt){g.queue=queue;g.uploadFamily=family;g.submittedAt=GetTickCount64();}}catch(...){}
}
// The VR pictures, drawn ahead (GDI, a few ms each) on a thread of their own: label and size -> premultiplied RGBA.
struct Painted{std::mutex m;std::map<std::pair<std::wstring,uint32_t>,std::vector<uint8_t>> pixels;std::atomic<bool> busy{false};};
inline Painted painted;
inline const std::vector<uint8_t>* paintedFor(const std::wstring& label,uint32_t w,uint32_t h){
    std::lock_guard<std::mutex> lock(painted.m);auto f=painted.pixels.find({label,w<<16|h});return f==painted.pixels.end()?nullptr:&f->second;}
inline void paintAhead(std::vector<std::tuple<std::wstring,uint32_t,uint32_t>> todo){
    if(painted.busy.exchange(true))return;
    std::thread([w=std::move(todo)]{
        try{for(const auto& [label,width,height]:w){if(paintedFor(label,width,height))continue;
            evr::glyph::Style style;style.round=evr::glyph::faceLabel(label);auto px=evr::glyph::paint(width,height,label,style);std::lock_guard<std::mutex> lock(painted.m);painted.pixels[{label,width<<16|height}]=std::move(px);}}
        catch(const std::exception& e){note("ui_glyph_paint_failed","\"reason\":"+jsonString(e.what()));}
        painted.busy=false;}).detach();
}
// The layer's own command buffer, staging buffer and fence on the device of the pictures' queue.
struct Device{std::unique_ptr<evr::Vulkan> v;PFN_vkCmdCopyBufferToImage copy=nullptr;VkDevice device{};VkQueue queue{};uint32_t family=0;
    VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};VkBuffer buffer{};VkDeviceMemory memory{};uint8_t* data=nullptr;bool failed=false;};
inline std::mutex deviceMutex;inline Device dev;
constexpr VkDeviceSize slotBytes=128*128*4,stagingSlots=64;
inline bool ensure(VkQueue queue){
    if(dev.failed)return false;
    if(dev.v){std::lock_guard<std::mutex> lock(mutex);auto f=queueFamilies.find(queue);return f!=queueFamilies.end()&&f->second==dev.family;}
    try{
        DeviceDispatch dd{};uint32_t family=UINT32_MAX;
        {std::lock_guard<std::mutex> lock(mutex);auto found=devices.find(key(queue));if(found==devices.end())return false;dd=found->second;
            auto f=queueFamilies.find(queue);if(f!=queueFamilies.end())family=f->second;}
        if(family==UINT32_MAX)return false;
        auto v=std::make_unique<evr::Vulkan>();v->gipa=dd.gipa;v->bindInstance(dd.instance);
        v->vkGetDeviceProcAddr=reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(v->module,"vkGetDeviceProcAddr"));
        if(!v->vkGetDeviceProcAddr)throw std::runtime_error("public device dispatcher unavailable");
        v->bindDevice(dd.device,dd.physical,family);
        dev.copy=reinterpret_cast<PFN_vkCmdCopyBufferToImage>(v->vkGetDeviceProcAddr(dd.device,"vkCmdCopyBufferToImage"));
        if(!dev.copy)throw std::runtime_error("vkCmdCopyBufferToImage missing");
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pc.queueFamilyIndex=family;
        evr::vkCheck(v->vkCreateCommandPool(dd.device,&pc,nullptr,&dev.pool),"glyph pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=dev.pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        evr::vkCheck(v->vkAllocateCommandBuffers(dd.device,&ca,&dev.cmd),"glyph command");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fc.flags=VK_FENCE_CREATE_SIGNALED_BIT;
        evr::vkCheck(v->vkCreateFence(dd.device,&fc,nullptr,&dev.fence),"glyph fence");
        VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bc.size=slotBytes*stagingSlots;bc.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;bc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        evr::vkCheck(v->vkCreateBuffer(dd.device,&bc,nullptr,&dev.buffer),"glyph staging");
        VkMemoryRequirements req{};v->vkGetBufferMemoryRequirements(dd.device,dev.buffer,&req);
        dev.memory=v->allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        evr::vkCheck(v->vkBindBufferMemory(dd.device,dev.buffer,dev.memory,0),"glyph staging bind");
        void* data=nullptr;evr::vkCheck(v->vkMapMemory(dd.device,dev.memory,0,VK_WHOLE_SIZE,0,&data),"glyph staging map");dev.data=static_cast<uint8_t*>(data);
        dev.device=dd.device;dev.queue=queue;dev.family=family;dev.v=std::move(v);
        note("ui_glyph_ready","\"family\":"+std::to_string(family)+",\"queue\":"+handleText(queue));
        return true;
    }catch(const std::exception& e){dev.failed=true;note("ui_glyph_failed","\"reason\":"+jsonString(e.what()));return false;}
}
inline void destroy(VkDevice d) noexcept {
    try{std::lock_guard<std::mutex> lock(deviceMutex);if(!dev.v||dev.device!=d)return;auto& v=*dev.v;own=true;
        if(dev.fence){v.vkWaitForFences(v.device,1,&dev.fence,VK_TRUE,2000000000ull);v.vkDestroyFence(v.device,dev.fence,nullptr);}
        if(dev.pool)v.vkDestroyCommandPool(v.device,dev.pool,nullptr);
        if(dev.buffer)v.vkDestroyBuffer(v.device,dev.buffer,nullptr);if(dev.memory){v.vkUnmapMemory(v.device,dev.memory);v.vkFreeMemory(v.device,dev.memory,nullptr);}
        own=false;note("ui_glyph_destroyed","\"updates\":"+std::to_string(updates.load()));dev=Device{};
    }catch(...){own=false;}
}
// Before a submission of the game's on `queue`: bring the pictures whose label changed up to date.
// The mod's own queue (native_xr.hpp, afw_pacing.hpp pacing 2): the game's pictures are not written from it.
inline std::atomic<VkQueue> modOwnQueue{VK_NULL_HANDLE};
inline void beforeSubmit(VkQueue queue) noexcept {
    if(!replace||own||queue==modOwnQueue.load())return;
    const auto want=wanted.load();if(want==0xffffffffu)return;
    try{
        using namespace evr::glyph;
        const auto family=Family(want&15);const auto scene=Scene((want>>4)&15);const auto held=Held((want>>8)&15);
        const auto now=GetTickCount64();
        uint32_t queueFamily=UINT32_MAX;{std::lock_guard<std::mutex> lock(mutex);auto f=queueFamilies.find(queue);if(f!=queueFamilies.end())queueFamily=f->second;}
        if(queueFamily==UINT32_MAX)return;
        struct Job{VkImage image;uint32_t width,height;std::wstring label;std::vector<uint8_t> pixels;};   // a copy: the glyphs may grow meanwhile
        std::vector<Job> jobs;std::vector<std::tuple<std::wstring,uint32_t,uint32_t>> missing;
        {std::lock_guard<std::mutex> lock(guard);
            // the game's own left stick press picture, for the spare one (button_glyph_labels.hpp originalLeftPress)
            const std::vector<uint8_t>* leftPress=nullptr;uint32_t leftPressW=0,leftPressH=0;
            for(const auto& g:glyphs)if(g.pic==Pic::LeftStickPress){leftPress=&g.original;leftPressW=g.width;leftPressH=g.height;}
            for(auto& g:glyphs){
                if(!g.submittedAt||now<g.submittedAt+1500)continue;                   // its upload went to the GPU a while ago
                // The family that may write it: any of a concurrent image's, else the one it was handed to, else the upload's.
                {const auto c=candidates.find(g.image);bool may=false;
                 if(c!=candidates.end()&&c->second.concurrent){for(auto fam:c->second.families)may=may||fam==queueFamily;}
                 else if(c!=candidates.end()&&c->second.owner!=UINT32_MAX)may=c->second.owner==queueFamily;
                 else may=g.uploadFamily==queueFamily;
                 if(!may)continue;}
                const auto l=label(g.pic,family,scene,held);
                if(g.hasApplied&&l==g.applied)continue;
                if(l.empty()){if(!g.hasApplied)continue;jobs.push_back({g.image,g.width,g.height,l,g.original});continue;}
                if(l==originalLeftPress){if(!leftPress||leftPressW!=g.width||leftPressH!=g.height)continue;jobs.push_back({g.image,g.width,g.height,l,*leftPress});continue;}
                const auto px=paintedFor(l,g.width,g.height);
                if(!px){missing.emplace_back(l,g.width,g.height);continue;}
                jobs.push_back({g.image,g.width,g.height,l,*px});
            }}
        if(!missing.empty())paintAhead(std::move(missing));
        if(jobs.empty())return;
        std::lock_guard<std::mutex> lock(deviceMutex);
        if(!ensure(queue))return;
        auto& v=*dev.v;own=true;
        struct Own{~Own(){own=false;}} ownScope;
        if(v.vkWaitForFences(v.device,1,&dev.fence,VK_TRUE,0)!=VK_SUCCESS)return;      // the last update is still on the GPU: next time
        evr::vkCheck(v.vkResetFences(v.device,1,&dev.fence),"glyph fence reset");
        evr::vkCheck(v.vkResetCommandBuffer(dev.cmd,0),"glyph command reset");
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        evr::vkCheck(v.vkBeginCommandBuffer(dev.cmd,&bi),"glyph command begin");
        size_t slot=0;std::vector<std::pair<VkImage,std::wstring>> done;
        for(const auto& j:jobs){
            if(slot==stagingSlots)break;const size_t bytes=size_t(j.width)*j.height*4;if(bytes>slotBytes||j.pixels.size()<bytes)continue;
            std::memcpy(dev.data+slot*slotBytes,j.pixels.data(),bytes);
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=j.image;
            b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            v.vkCmdPipelineBarrier(dev.cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferImageCopy r{};r.bufferOffset=slot*slotBytes;r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};r.imageExtent={j.width,j.height,1};
            dev.copy(dev.cmd,dev.buffer,j.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&r);
            b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            v.vkCmdPipelineBarrier(dev.cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
            done.emplace_back(j.image,j.label);++slot;
        }
        evr::vkCheck(v.vkEndCommandBuffer(dev.cmd),"glyph command end");
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&dev.cmd;
        evr::vkCheck(v.vkQueueSubmit(queue,1,&si,dev.fence),"glyph submit");
        {std::lock_guard<std::mutex> lock(guard);for(const auto& [image,l]:done)for(auto& g:glyphs)if(g.image==image){g.applied=l;g.hasApplied=true;}}
        const auto n=++updates;
        if(n<=40||n%100==0)note("ui_glyph_update","\"pictures\":"+std::to_string(done.size())+",\"family\":"+std::to_string(want&15)+",\"scene\":"+std::to_string((want>>4)&15)+
            ",\"held\":"+std::to_string((want>>8)&15)+",\"count\":"+std::to_string(n));
    }catch(const std::exception& e){own=false;dev.failed=true;note("ui_glyph_failed","\"reason\":"+jsonString(e.what()));}
}
}
extern "C" __declspec(dllexport) int evrSetButtonGlyphs(uint32_t packed) noexcept {uiGlyph::wanted=packed;return uiGlyph::replace?0:1;}
