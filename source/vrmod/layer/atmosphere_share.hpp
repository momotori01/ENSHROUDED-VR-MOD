#pragma once
// One sky for both eyes (2026-10-17 evening; headset report 21 and the night Mock hq, game-20261017-131623: the left eye's
// sky about 10 % redder, green and blue alike; the user chose to reproject rather than to give up the stereo of the fog).
// Each eye's View computes the game's atmospheric scattering tables on its own, every frame, from its own previous frame
// (RenderDoc hq: 6554 / 10342 [22,22,16] make 14401..14407 from 14402..14408, then 6558+6562 / 10346+10350 integrate them
// into two 3D R32_UINT tables of 88x88x64 - left 14473/14474, right 14683/14684 - which the sky and fog composite
// (6596 / 10384, DeleteMe/20261017/rd-hq-fog/fog6596.hlsl) reads as R9G9B9E5 at (altitude, view zenith, sun zenith)).
// No screen position and no eye position are in them (only the camera's height, the same to 6 cm), so the first eye's
// tables are right for the second eye as they are: no reprojection is needed. In hq they differed (by depth slice 0.3 to
// 1.5 times) and the sky band after the composite was 7 % redder in the left eye.
// Here the first eye's two tables are copied over the second eye's when the second eye's are put into a read layout.
// The tables are told from the history volumes of the same shape by being written in every frame (a history pair is
// written every other frame). In a frame without the desktop View four tables are written, the first eye's two first;
// 30 such frames alike lock the pairs (each eye's two in creation order). The images get TRANSFER_SRC/DST at creation.
// EVR_SHARE_ATMOSPHERE=0: off.
namespace atmosphereShare {
// 2026-10-17 20:20: OFF - wrong premise. The first Mock (game-20261017-201417) locked on other 3D images (768x32x32), and the
// 88x88x64 volumes are the eye's screen / 12 (the desktop View's are 93x53x64): screen-aligned fog volumes, not the
// atmosphere tables (those are the shared 64x128 3D images 26..28). Kept for reference only; EVR_SHARE_ATMOSPHERE=1 would
// turn it on - do not.
inline bool enabled(){static const bool yes=[]{const char* v=std::getenv("EVR_SHARE_ATMOSPHERE");return v&&!std::strcmp(v,"1");}();return yes;}
inline bool candidate(const VkImageCreateInfo& c){
    return c.imageType==VK_IMAGE_TYPE_3D&&c.format==VK_FORMAT_R32_UINT&&c.extent.depth>=16&&c.mipLevels==1&&c.arrayLayers==1&&
           c.samples==VK_SAMPLE_COUNT_1_BIT&&(c.usage&VK_IMAGE_USAGE_STORAGE_BIT)&&(c.usage&VK_IMAGE_USAGE_SAMPLED_BIT);
}
struct Image {bool candidate=false;uint64_t generation=0;VkExtent3D extent{};VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED;uint64_t lastWrite=0,previousWrite=0;};
struct Pairs {std::array<VkImage,2> first{},second{};uint32_t streak=0;bool locked=false;uint64_t copiedPresent[2]{};};
inline std::mutex atmosphereMutex;
inline std::unordered_map<VkImage,Image> images;
inline std::vector<VkImage> frameTables;inline uint64_t framePresent=UINT64_MAX;
inline Pairs pairs;inline uint64_t copies=0,failures=0;
inline std::map<std::string,uint64_t> said;
inline void say(const char* event,const std::string& fields){const auto n=++said[event];if(n<=8||n%1800==0||!std::strcmp(event,"atmosphere_share_failed"))note(event,fields);}
inline bool readLayout(VkImageLayout l){return l==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL||l==VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;}
inline bool writeLayout(VkImageLayout l){return l==VK_IMAGE_LAYOUT_GENERAL||l==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;}
// A frame's tables are complete when the next present comes: four, the first eye's two first.
inline void closeFrame(uint64_t present){
    if(framePresent!=UINT64_MAX&&framePresent!=present&&frameTables.size()==4){
        auto byCreation=[&](VkImage a,VkImage b){return images[a].generation<images[b].generation;};
        std::array<VkImage,2> first{frameTables[0],frameTables[1]},second{frameTables[2],frameTables[3]};
        std::sort(first.begin(),first.end(),byCreation);std::sort(second.begin(),second.end(),byCreation);
        if(pairs.locked&&(first!=pairs.first||second!=pairs.second)){
            say("atmosphere_share_unlocked","\"present\":"+std::to_string(present));pairs=Pairs{};}
        if(!pairs.locked){
            if(first==pairs.first&&second==pairs.second)++pairs.streak;else{pairs.first=first;pairs.second=second;pairs.streak=1;}
            if(pairs.streak>=30){pairs.locked=true;
                say("atmosphere_share_pair","\"first\":["+handleText(first[0])+","+handleText(first[1])+"],\"second\":["+handleText(second[0])+","+handleText(second[1])+
                    "],\"present\":"+std::to_string(present)+",\"extent\":["+std::to_string(images[first[0]].extent.width)+","+std::to_string(images[first[0]].extent.height)+","+
                    std::to_string(images[first[0]].extent.depth)+"]");}
        }
    }
    if(framePresent!=present){framePresent=present;frameTables.clear();}
}
inline void copy(VkCommandBuffer c,VkImage a,VkImageLayout la,VkImage b,VkImageLayout lb,VkExtent3D e){
    auto barrier=deviceFn<PFN_vkCmdPipelineBarrier>(c,"vkCmdPipelineBarrier");auto copyImage=deviceFn<PFN_vkCmdCopyImage>(c,"vkCmdCopyImage");
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    VkImageMemoryBarrier to[2]{};
    to[0]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,la,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
           VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,a,range};
    to[1]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,lb,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
           VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,b,range};
    barrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,to);
    VkImageCopy region{};region.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.dstSubresource=region.srcSubresource;region.extent=e;
    copyImage(c,a,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,b,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
    VkImageMemoryBarrier back[2]{};
    back[0]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,0,VK_ACCESS_MEMORY_READ_BIT,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,la,
             VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,a,range};
    back[1]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,lb,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,b,range};
    barrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,2,back);
    ++copies;
    say("atmosphere_share_copy","\"cmd\":"+handleText(c)+",\"from\":"+handleText(a)+",\"to\":"+handleText(b)+",\"from_layout\":"+std::to_string(la)+
        ",\"to_layout\":"+std::to_string(lb)+",\"present\":"+std::to_string(presents.load())+",\"copies\":"+std::to_string(copies));
}
struct Entry {VkImage image;VkImageLayout to;};
inline void barriers(VkCommandBuffer c,const Entry* entries,uint32_t count){
    if(!enabled()||!count)return;
    try{
        std::lock_guard<std::mutex> lock(atmosphereMutex);
        const auto present=presents.load();
        struct Due {VkImage a,b;VkImageLayout la,lb;VkExtent3D e;};std::vector<Due> due;
        for(uint32_t n=0;n<count;++n){
            const auto& e=entries[n];
            auto found=images.find(e.image);
            if(found==images.end()){
                VkImageCreateInfo info{};bool known=false;uint64_t generation=0;
                {std::lock_guard<std::mutex> imageLock(mutex);auto d=imageCreation.find(key(c));
                 if(d!=imageCreation.end()){auto i=d->second.find(e.image);if(i!=d->second.end()){info=i->second;known=true;}}
                 auto t=trackedImages.find(key(c));if(t!=trackedImages.end()){auto i=t->second.find(e.image);if(i!=t->second.end())generation=i->second;}}
                Image record;record.candidate=known&&candidate(info);record.generation=generation;record.extent=info.extent;
                found=images.emplace(e.image,record).first;
                if(record.candidate)say("atmosphere_share_image","\"image\":"+handleText(e.image)+",\"extent\":["+std::to_string(info.extent.width)+","+
                    std::to_string(info.extent.height)+","+std::to_string(info.extent.depth)+"],\"usage\":"+std::to_string(info.usage)+",\"generation\":"+std::to_string(generation));
            }
            auto& im=found->second;if(!im.candidate)continue;
            im.layout=e.to;
            closeFrame(present);
            if(writeLayout(e.to)&&im.lastWrite!=present){im.previousWrite=im.lastWrite;im.lastWrite=present;}
            // a table: written in this frame and in the one before
            if(im.lastWrite==present&&im.previousWrite+1==present&&std::find(frameTables.begin(),frameTables.end(),e.image)==frameTables.end())
                frameTables.push_back(e.image);
            if(!pairs.locked||!readLayout(e.to))continue;
            for(int k=0;k<2;++k){
                if(e.image!=pairs.second[k]||pairs.copiedPresent[k]==present)continue;
                const auto& a=images[pairs.first[k]];
                if(!readLayout(a.layout)||a.lastWrite+1<present||a.extent.width!=im.extent.width||a.extent.height!=im.extent.height||a.extent.depth!=im.extent.depth)continue;
                due.push_back({pairs.first[k],e.image,a.layout,e.to,im.extent});pairs.copiedPresent[k]=present;
            }
        }
        for(const auto& d:due)copy(c,d.a,d.la,d.b,d.lb,d.e);
    }catch(const std::exception& e){++failures;say("atmosphere_share_failed","\"reason\":"+jsonString(e.what()));}
    catch(...){++failures;say("atmosphere_share_failed","\"reason\":\"unknown\"");}
}
}
bool atmosphereShareCreate(const VkImageCreateInfo& in,VkImageCreateInfo& out) noexcept {
    if(!atmosphereShare::enabled()||!atmosphereShare::candidate(in))return false;
    out=in;out.usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;return true;
}
void atmosphereShareBarrier(VkCommandBuffer c,uint32_t count,const VkImageMemoryBarrier* images) noexcept {
    if(!atmosphereShare::enabled()||!count||!images)return;
    try{std::vector<atmosphereShare::Entry> e;e.reserve(count);for(uint32_t n=0;n<count;++n)e.push_back({images[n].image,images[n].newLayout});
        atmosphereShare::barriers(c,e.data(),count);}catch(...){}
}
void atmosphereShareBarrier2(VkCommandBuffer c,const VkDependencyInfo* info) noexcept {
    if(!atmosphereShare::enabled()||!info||!info->imageMemoryBarrierCount)return;
    try{std::vector<atmosphereShare::Entry> e;e.reserve(info->imageMemoryBarrierCount);
        for(uint32_t n=0;n<info->imageMemoryBarrierCount;++n)e.push_back({info->pImageMemoryBarriers[n].image,info->pImageMemoryBarriers[n].newLayout});
        atmosphereShare::barriers(c,e.data(),uint32_t(e.size()));}catch(...){}
}
void atmosphereShareForget(VkImage image) noexcept {
    try{std::lock_guard<std::mutex> lock(atmosphereShare::atmosphereMutex);atmosphereShare::images.erase(image);
        for(int k=0;k<2;++k)if(atmosphereShare::pairs.first[k]==image||atmosphereShare::pairs.second[k]==image)atmosphereShare::pairs=atmosphereShare::Pairs{};}catch(...){}
}
