#pragma once
// One sun shadow map for both eyes (2026-10-17 afternoon; the user: "the darkness of the shadows differs, or a shadow is
// missing in one eye, here and there", headset report 20 "one eye's shadow reacts late").
// Each eye's View keeps its own cascaded sun shadow map (D32 1024x1024x4 at shadows "Low", 1536x1536x5 at "High") and its
// depth summary (64x64x4 / 96x96x5), and
// redraws them only now and then - both eyes in the same frames, every 100-180 frames in a Mock look-around
// (game-20261017-141222, -GpuTimingEvery 7); the frames between only read them. In a night capture (hq,
// game-20261017-131623) the two maps share the texel grid and the depth mapping (where both have a caster the depth
// differs by 1e-7..3e-5 at the median), but the second eye's map lacks the terrain inside a round area of cascades 2
// and 3 and a block of cascade 1 that the first eye's map has (guess: the casters the second eye draws come from the
// terrain chunks it reuses from the first eye without their own update).
// Here the first eye's map is copied into the second eye's (layer by layer, depth: since 2026-10-18 the eyes redraw some
// cascades every frame) when a layer of the second eye's map is put into a read layout after both eyes drew it in this
// frame - the game's later reads of the second eye's map then see the first eye's casters. The same for the 64x64
// summaries. The pair is learned from the order of use: in a frame (between
// presents) without the desktop View there are two maps of a kind, the first eye's first; 30 such frames alike lock
// the pair. The images get TRANSFER_SRC/DST when they are created.
// EVR_SHARE_SHADOW_MAP=0: off.
namespace shadowShare {
inline bool enabled(){static const bool yes=[]{const char* v=std::getenv("EVR_SHARE_SHADOW_MAP");return !(v&&!std::strcmp(v,"0"));}();return yes;}
// kind 0: the cascades, kind 1: their summaries. Shadows "Low" (the user's setting and the Mock's): 1024x1024x4 and
// 64x64x4; "High" (test_env/settings-high-variants-20261017.json): 1536x1536x5 and 96x96x5. Square D32 arrays of 3..8
// layers; the eyes' other depth arrays are 512x512x2 and 256x256x9 (Mock game-20261017-144807).
inline int kindOf(const VkImageCreateInfo& c){
    if(c.imageType!=VK_IMAGE_TYPE_2D||c.format!=VK_FORMAT_D32_SFLOAT||c.arrayLayers<3||c.arrayLayers>8||c.mipLevels!=1||c.samples!=VK_SAMPLE_COUNT_1_BIT||
       c.extent.depth!=1||c.extent.width!=c.extent.height||!(c.usage&VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)||!(c.usage&VK_IMAGE_USAGE_SAMPLED_BIT))return -1;
    if(c.extent.width>=512)return 0;
    if(c.extent.width<=128)return 1;
    return -1;
}
inline bool readLayout(VkImageLayout l){
    return l==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL||l==VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL||
           l==VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL||l==VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL||l==VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
}
inline bool writeLayout(VkImageLayout l){
    return l==VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL||l==VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL||l==VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL||
           l==VK_IMAGE_LAYOUT_GENERAL||l==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL||l==VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
}
// Per layer (2026-10-18): the eyes redraw their cascades one by one now - cascade 0 every frame, 1 every 2nd, 2 and 3 every
// 4th (hooks/shadow_projection_bridge.hpp, the camera block's frame number) - and each layer goes to the attachment layout
// and back on its own, so the layouts and the presents of the last draw are kept for each layer.
struct Image {int kind=-1;uint32_t width=0,height=0;VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED;uint64_t present=0,generation=0;uint32_t layers=0;
    std::array<VkImageLayout,8> layerLayout{};std::array<uint64_t,8> layerDrawn{};std::array<bool,8> layerDirty{};};
struct Pair {VkImage first{},second{};uint32_t streak=0;bool locked=false,dirty=true;uint64_t lastCopyPresent=0;};
struct Order {uint64_t present=UINT64_MAX;std::vector<VkImage> images;};
inline std::mutex shareMutex;
inline std::unordered_map<VkImage,Image> images;
inline Pair pairs[2];inline Order orders[2];
inline uint64_t copies[2]{},transitions=0,failures=0;
// The present in which each eye's summary was last drawn (= its map redrawn, with its cascade fit of that moment).
inline uint64_t redrawn[2]{};
inline std::map<std::string,uint64_t> said;
inline void say(const char* event,const std::string& fields){const auto n=++said[event];if(n<=8||n%600==0||!std::strcmp(event,"shadow_share_failed"))note(event,fields);}
// Each map's summary is created right after it (handles and creation numbers next to each other in every Mock).
inline void summariesOf(uint64_t present){
    const auto a=images.find(pairs[0].first),b=images.find(pairs[0].second);
    if(a==images.end()||b==images.end()||!a->second.generation||!b->second.generation)return;
    VkImage sa{},sb{};
    for(const auto& [image,im]:images)if(im.kind==1){if(im.generation==a->second.generation+1)sa=image;if(im.generation==b->second.generation+1)sb=image;}
    if(!sa||!sb)return;
    pairs[1]=Pair{sa,sb,0,true,true,0};
    say("shadow_share_pair","\"kind\":1,\"first\":"+handleText(sa)+",\"second\":"+handleText(sb)+",\"present\":"+std::to_string(present)+",\"by\":\"creation_after_the_maps\"");
}
// A frame's order of use is complete when the next present comes: learn the pair from it.
inline void closeOrder(int kind,uint64_t present){
    auto& o=orders[kind];auto& p=pairs[kind];
    if(o.present!=UINT64_MAX&&o.present!=present&&o.images.size()==2){
        if(p.locked&&(o.images[0]!=p.first||o.images[1]!=p.second)){
            say("shadow_share_unlocked","\"kind\":"+std::to_string(kind)+",\"first\":"+handleText(o.images[0])+",\"second\":"+handleText(o.images[1])+
                ",\"was_first\":"+handleText(p.first)+",\"was_second\":"+handleText(p.second));
            p=Pair{};
        }
        if(!p.locked){
            if(o.images[0]==p.first&&o.images[1]==p.second)++p.streak;else{p.first=o.images[0];p.second=o.images[1];p.streak=1;}
            // The summaries change only when the maps are drawn again, so they are seen in few frames: 3 alike lock them,
            // and they are taken at once with the maps' pair when each was created right after its map (below).
            if(p.streak>=(kind?3u:30u)){p.locked=true;p.dirty=true;
                say("shadow_share_pair","\"kind\":"+std::to_string(kind)+",\"first\":"+handleText(p.first)+",\"second\":"+handleText(p.second)+",\"present\":"+std::to_string(present)+",\"by\":\"order\"");
                if(kind==0&&!pairs[1].locked)summariesOf(present);}
        }
    }
    if(o.present!=present){o.present=present;o.images.clear();}
}
inline void copy(VkCommandBuffer c,int kind,VkImage a,VkImageLayout la,VkImage b,VkImageLayout lb,uint32_t w,uint32_t h,uint32_t layer){
    auto barrier=deviceFn<PFN_vkCmdPipelineBarrier>(c,"vkCmdPipelineBarrier");auto copyImage=deviceFn<PFN_vkCmdCopyImage>(c,"vkCmdCopyImage");
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,layer,1};
    VkImageMemoryBarrier to[2]{};
    to[0]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,la,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
           VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,a,range};
    to[1]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,lb,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
           VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,b,range};
    barrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,to);
    VkImageCopy region{};region.srcSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,layer,1};region.dstSubresource=region.srcSubresource;region.extent={w,h,1};
    copyImage(c,a,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,b,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
    VkImageMemoryBarrier back[2]{};
    back[0]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,0,VK_ACCESS_MEMORY_READ_BIT,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,la,
             VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,a,range};
    back[1]={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,nullptr,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,lb,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,b,range};
    barrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,2,back);
    const auto n=++copies[kind];
    say("shadow_share_copy","\"kind\":"+std::to_string(kind)+",\"layer\":"+std::to_string(layer)+",\"cmd\":"+handleText(c)+",\"from\":"+handleText(a)+",\"to\":"+handleText(b)+
        ",\"from_layout\":"+std::to_string(la)+",\"to_layout\":"+std::to_string(lb)+",\"present\":"+std::to_string(presents.load())+",\"copies\":"+std::to_string(n));
}
struct Entry {VkImage image;VkImageLayout from,to;uint32_t baseLayer,layers;};
inline void barriers(VkCommandBuffer c,const Entry* entries,uint32_t count){
    if(!enabled()||!count)return;
    try{
        std::lock_guard<std::mutex> lock(shareMutex);
        const auto present=presents.load();
        struct Due {int kind;VkImage a,b;VkImageLayout la,lb;uint32_t w,h,layer;};std::vector<Due> due;
        for(uint32_t n=0;n<count;++n){
            const auto& e=entries[n];
            auto found=images.find(e.image);
            if(found==images.end()){
                // The creation info is kept for attachment images (diagnostic_layer.cpp evrCreateImage).
                VkImageCreateInfo info{};bool known=false;
                uint64_t generation=0;
                {std::lock_guard<std::mutex> imageLock(mutex);auto d=imageCreation.find(key(c));
                 if(d!=imageCreation.end()){auto i=d->second.find(e.image);if(i!=d->second.end()){info=i->second;known=true;}}
                 auto t=trackedImages.find(key(c));if(t!=trackedImages.end()){auto i=t->second.find(e.image);if(i!=t->second.end())generation=i->second;}}
                const int kind=known?kindOf(info):-1;
                Image record{kind,info.extent.width,info.extent.height};record.generation=generation;record.layers=info.arrayLayers;
                found=images.emplace(e.image,record).first;
                if(kind>=0)say("shadow_share_image","\"kind\":"+std::to_string(kind)+",\"image\":"+handleText(e.image)+",\"size\":"+std::to_string(info.extent.width)+",\"layers\":"+std::to_string(info.arrayLayers)+",\"usage\":"+std::to_string(info.usage)+",\"generation\":"+std::to_string(generation));
            }
            auto& im=found->second;if(im.kind<0)continue;
            if(++transitions<=48)note("shadow_share_layout","\"kind\":"+std::to_string(im.kind)+",\"image\":"+handleText(e.image)+",\"from\":"+std::to_string(e.from)+
                ",\"to\":"+std::to_string(e.to)+",\"layers\":["+std::to_string(e.baseLayer)+","+std::to_string(e.layers)+"],\"present\":"+std::to_string(present));
            im.layout=e.to;im.present=present;
            const uint32_t lo=std::min(e.baseLayer,im.layers),hi=e.layers==VK_REMAINING_ARRAY_LAYERS?im.layers:std::min(im.layers,e.baseLayer+e.layers);
            for(uint32_t l=lo;l<hi&&l<8;++l){im.layerLayout[l]=e.to;if(writeLayout(e.to)){im.layerDrawn[l]=present;im.layerDirty[l]=true;}}
            closeOrder(im.kind,present);
            auto& o=orders[im.kind];if(std::find(o.images.begin(),o.images.end(),e.image)==o.images.end())o.images.push_back(e.image);
            auto& p=pairs[im.kind];
            if(!p.locked)continue;
            if(writeLayout(e.to)&&(e.image==p.first||e.image==p.second))p.dirty=true;
            // Diagnostic (2026-10-18): the eyes' layouts layer by layer over 3 presents, 300 presents after both pairs locked
            if((e.image==p.first||e.image==p.second)&&pairs[0].locked&&pairs[1].locked){
                static uint64_t from=0;static unsigned traced=0;if(!from)from=present+300;
                if(present>=from&&present<from+3&&traced<300){++traced;
                    note("shadow_share_trace","\"kind\":"+std::to_string(im.kind)+",\"eye\":"+(e.image==p.first?"0":"1")+",\"from\":"+std::to_string(e.from)+
                        ",\"to\":"+std::to_string(e.to)+",\"layers\":["+std::to_string(e.baseLayer)+","+std::to_string(e.layers)+"],\"present\":"+std::to_string(present)+",\"cmd\":"+handleText(c));}}
            // 2026-10-17 evening (headset: "every shadow switches about twice a second"): when each eye's summary is drawn
            // again (it changes only then), with the present, to see how often the eyes' maps are redrawn
            if(im.kind==1&&writeLayout(e.to)&&(e.image==p.first||e.image==p.second)){
                static unsigned redraws=0;const int side=e.image==p.first?0:1;
                if(redrawn[side]!=present&&redraws<400){++redraws;
                    note("shadow_share_redraw","\"eye\":"+std::to_string(side)+",\"present\":"+std::to_string(present)+",\"since\":"+std::to_string(present-redrawn[side])+
                        ",\"other_eye_this_frame\":"+(redrawn[1-side]==present?"true":"false"));}
                redrawn[side]=present;}
            if(e.image!=p.second||!readLayout(e.to))continue;
            // A layer of the second eye's map that was drawn in this frame and is put into a read layout takes the first
            // eye's same layer when that was drawn in this frame too (or one present before: the desktop's present between
            // the eyes). Both eyes' cascades come from the same camera block and frame number (the bridge), so the same
            // layers are drawn in the same frames under the same fit; a layer drawn by one eye alone is not copied. A layer
            // drawn again after its copy (the desktop View draws each layer twice a frame) is copied again.
            auto first=images.find(p.first);
            if(first==images.end()||first->second.width!=im.width||first->second.height!=im.height||first->second.layers!=im.layers)continue;
            auto& a=first->second;
            for(uint32_t l=lo;l<hi&&l<8;++l){
                if(im.layerDrawn[l]!=present||!im.layerDirty[l])continue;
                if(a.layerDrawn[l]+1<present||!readLayout(a.layerLayout[l])){
                    static unsigned alone=0;if(alone++<40)
                        note("shadow_share_not_copied","\"kind\":"+std::to_string(im.kind)+",\"layer\":"+std::to_string(l)+",\"present\":"+std::to_string(present)+
                            ",\"first_drawn\":"+std::to_string(a.layerDrawn[l])+",\"first_layout\":"+std::to_string(a.layerLayout[l]));
                    continue;}
                due.push_back({im.kind,p.first,p.second,a.layerLayout[l],e.to,im.width,im.height,l});
                im.layerDirty[l]=false;p.lastCopyPresent=present;
            }
        }
        for(const auto& d:due)copy(c,d.kind,d.a,d.la,d.b,d.lb,d.w,d.h,d.layer);
    }catch(const std::exception& e){++failures;say("shadow_share_failed","\"reason\":"+jsonString(e.what()));}
    catch(...){++failures;say("shadow_share_failed","\"reason\":\"unknown\"");}
}
}
bool shadowShareCreate(const VkImageCreateInfo& in,VkImageCreateInfo& out) noexcept {
    if(!shadowShare::enabled()||shadowShare::kindOf(in)<0)return false;
    out=in;out.usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;return true;
}
void shadowShareBarrier(VkCommandBuffer c,uint32_t count,const VkImageMemoryBarrier* images) noexcept {
    if(!shadowShare::enabled()||!count||!images)return;
    try{std::vector<shadowShare::Entry> e;e.reserve(count);
        for(uint32_t n=0;n<count;++n)e.push_back({images[n].image,images[n].oldLayout,images[n].newLayout,images[n].subresourceRange.baseArrayLayer,images[n].subresourceRange.layerCount});
        shadowShare::barriers(c,e.data(),count);}catch(...){}
}
void shadowShareBarrier2(VkCommandBuffer c,const VkDependencyInfo* info) noexcept {
    if(!shadowShare::enabled()||!info||!info->imageMemoryBarrierCount)return;
    try{std::vector<shadowShare::Entry> e;e.reserve(info->imageMemoryBarrierCount);
        for(uint32_t n=0;n<info->imageMemoryBarrierCount;++n){const auto& b=info->pImageMemoryBarriers[n];
            e.push_back({b.image,b.oldLayout,b.newLayout,b.subresourceRange.baseArrayLayer,b.subresourceRange.layerCount});}
        shadowShare::barriers(c,e.data(),uint32_t(e.size()));}catch(...){}
}
void shadowShareForget(VkImage image) noexcept {
    try{std::lock_guard<std::mutex> lock(shadowShare::shareMutex);shadowShare::images.erase(image);
        for(auto& p:shadowShare::pairs)if(p.first==image||p.second==image)p=shadowShare::Pair{};}catch(...){}
}
