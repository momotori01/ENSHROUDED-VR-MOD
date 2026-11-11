#pragma once
// Diagnostic (2026-11-11, the stick-turn motion vector bursts - docs/analysis/motion_blur_20261111.md). The eyes' motion of
// still things is written by one compute pass of the game (shader 2256 bytes; set 0: binding 0 the constants {uint2 size,
// float2 1/size, float4x4 this frame's (uv, depth, 1) -> last frame's uv}, 1 the own-motion flags, 2 the depth, 3 the
// motion image). The CPU builds those constants right (hooks motion_matrices) and a RenderDoc capture with FSR showed them
// right on the GPU, yet with DLSS the motion is off in bursts. EVR_MOTION_REPROJ_PROBE=1 reads the 80 bytes as the GPU will
// read them - from the mapped ring buffer when the command buffer is submitted - and logs them (motion_reproj_constant).
#include <unordered_set>
namespace motionReprojProbe {
inline const bool on=[]{const char* v=std::getenv("EVR_MOTION_REPROJ_PROBE");return v&&v[0]=='1';}();
constexpr size_t shaderBytes=2256;
struct SetRec {VkBuffer ubo{};VkDeviceSize offset=0,range=0;bool dynamic=false;VkImageView motion{},flags{};VkImageLayout flagsLayout{};};
struct Bind {uint32_t first=0;std::vector<VkDescriptorSet> sets;std::vector<uint32_t> offsets;};
struct Pending {VkBuffer ubo{};VkDeviceSize offset=0;VkImageView motion{};uint32_t gx=0,gy=0;};
struct Cmd {VkPipeline pipeline{};std::vector<Bind> binds;std::vector<Pending> pending;};
inline std::mutex lock;inline std::unordered_set<VkShaderModule> modules;inline std::unordered_set<VkPipeline> pipelines;
inline std::unordered_map<VkDescriptorSet,SetRec> records;inline std::unordered_map<VkCommandBuffer,Cmd> commands;
inline std::atomic<unsigned> logged{0},misses{0};
struct FlagsOf {VkImageView view{};VkImageLayout layout{};};
inline std::unordered_map<VkImageView,FlagsOf> flagsOfMotion;   // the reprojection's motion (storage) view -> its flags
inline void shader(VkShaderModule module,size_t bytes){if(on&&module&&bytes==shaderBytes){std::lock_guard<std::mutex> l(lock);modules.insert(module);}}
inline void pipeline(VkShaderModule module,VkPipeline p){if(!on||!p)return;std::lock_guard<std::mutex> l(lock);if(modules.count(module)){pipelines.insert(p);}}
inline void writes(uint32_t count,const VkWriteDescriptorSet* w,uint32_t copies,const VkCopyDescriptorSet* c){
    if(!on)return;
    try{std::lock_guard<std::mutex> l(lock);if(pipelines.empty())return;if(records.size()>300000)records.clear();
        for(uint32_t i=0;i<count&&w;i++){const auto& x=w[i];if(x.dstArrayElement||x.descriptorCount!=1)continue;
            if(x.dstBinding==0&&x.pBufferInfo&&(x.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER||x.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)){
                auto& r=records[x.dstSet];r.ubo=x.pBufferInfo[0].buffer;r.offset=x.pBufferInfo[0].offset;r.range=x.pBufferInfo[0].range;r.dynamic=x.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;}
            else if(x.dstBinding==3&&x.descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_IMAGE&&x.pImageInfo)records[x.dstSet].motion=x.pImageInfo[0].imageView;
            else if(x.dstBinding==1&&x.descriptorType==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE&&x.pImageInfo){auto& r=records[x.dstSet];r.flags=x.pImageInfo[0].imageView;r.flagsLayout=x.pImageInfo[0].imageLayout;}}
        for(uint32_t i=0;i<copies&&c;i++){const auto& x=c[i];const auto from=records.find(x.srcSet);if(from==records.end())continue;const auto rec=from->second;auto& to=records[x.dstSet];
            for(uint32_t k=0;k<x.descriptorCount;k++){if(x.srcBinding+k!=x.dstBinding+k)continue;const auto b=x.srcBinding+k;
                if(b==0){to.ubo=rec.ubo;to.offset=rec.offset;to.range=rec.range;to.dynamic=rec.dynamic;}if(b==3)to.motion=rec.motion;if(b==1){to.flags=rec.flags;to.flagsLayout=rec.flagsLayout;}}}
    }catch(...){}
}
inline void reset(VkCommandBuffer c){if(!on)return;try{std::lock_guard<std::mutex> l(lock);commands.erase(c);}catch(...){}}
inline void bind(VkCommandBuffer c,VkPipelineBindPoint point,VkPipeline p){
    if(!on||point!=VK_PIPELINE_BIND_POINT_COMPUTE)return;try{std::lock_guard<std::mutex> l(lock);if(pipelines.empty())return;commands[c].pipeline=p;}catch(...){}
}
inline void bindSets(VkCommandBuffer c,VkPipelineBindPoint point,uint32_t first,uint32_t count,const VkDescriptorSet* s,uint32_t dynamicCount,const uint32_t* offsets){
    if(!on||point!=VK_PIPELINE_BIND_POINT_COMPUTE||!s||!count)return;
    try{std::lock_guard<std::mutex> l(lock);if(pipelines.empty())return;auto& cmd=commands[c];
        Bind b;b.first=first;b.sets.assign(s,s+count);if(dynamicCount&&offsets)b.offsets.assign(offsets,offsets+dynamicCount);
        cmd.binds.erase(std::remove_if(cmd.binds.begin(),cmd.binds.end(),[&](const Bind& o){return o.first>=first&&o.first+o.sets.size()<=first+count;}),cmd.binds.end());
        if(cmd.binds.size()>=16)cmd.binds.erase(cmd.binds.begin());cmd.binds.push_back(std::move(b));}catch(...){}
}
inline void dispatch(VkCommandBuffer c,uint32_t gx,uint32_t gy){
    if(!on)return;
    try{std::lock_guard<std::mutex> l(lock);const auto f=commands.find(c);if(f==commands.end()||!pipelines.count(f->second.pipeline))return;
        VkDescriptorSet set{};uint32_t dyn=0;
        for(auto it=f->second.binds.rbegin();it!=f->second.binds.rend();++it)if(it->first==0&&!it->sets.empty()){set=it->sets[0];dyn=it->offsets.empty()?0:it->offsets[0];break;}
        const auto r=records.find(set);if(!set||r==records.end()||!r->second.ubo){++misses;return;}
        f->second.pending.push_back({r->second.ubo,r->second.offset+(r->second.dynamic?dyn:0),r->second.motion,gx,gy});
        if(r->second.motion&&r->second.flags)flagsOfMotion[r->second.motion]={r->second.flags,r->second.flagsLayout};}catch(...){}
}
// Called with each command buffer just before it is submitted: the constants are in place by then.
inline void submitted(VkCommandBuffer c){
    if(!on)return;
    std::vector<Pending> p;
    try{std::lock_guard<std::mutex> l(lock);const auto f=commands.find(c);if(f==commands.end())return;p.swap(f->second.pending);}catch(...){return;}
    for(const auto& e:p){
        VkDeviceMemory memory{};VkDeviceSize memoryOffset=0;bool found=false;
        VkImage image{};
        {std::lock_guard<std::mutex> l(mutex);for(const auto& d:bufferAudits){const auto b=d.second.find(e.ubo);if(b!=d.second.end()){memory=b->second.memory;memoryOffset=b->second.memoryOffset;found=true;break;}}
         for(const auto& d:imageViews){const auto v=d.second.find(e.motion);if(v!=d.second.end()){image=v->second.image;break;}}}
        const uint8_t* base=nullptr;
        if(found){std::lock_guard<std::mutex> g(uiGlyph::guard);const auto m=uiGlyph::mapped.find(memory);if(m!=uiGlyph::mapped.end())base=m->second;}
        if(!base){if(misses.fetch_add(1)<5)note("motion_reproj_unmapped","\"buffer\":"+handleText(e.ubo));continue;}
        float v[20];std::memcpy(v,base+memoryOffset+e.offset,sizeof v);
        if(logged.fetch_add(1)>=20000)continue;
        std::ostringstream o;o<<std::setprecision(9)<<"\"motion\":"<<handleText(e.motion)<<",\"image\":"<<handleText(image)<<",\"groups\":["<<e.gx<<','<<e.gy<<"],\"c\":[";
        for(int i=0;i<20;i++)o<<(i?",":"")<<(i<2?double(reinterpret_cast<const uint32_t*>(v)[i]):double(v[i]));o<<']';
        note("motion_reproj_constant",o.str());
    }
}
} // namespace motionReprojProbe
// The layer's call points (declared before render_trace.hpp, which comes before ui_glyph.hpp).
void mrpShader(VkShaderModule m,size_t bytes) noexcept {try{motionReprojProbe::shader(m,bytes);}catch(...){}}
void mrpPipeline(VkShaderModule m,VkPipeline p) noexcept {try{motionReprojProbe::pipeline(m,p);}catch(...){}}
void mrpWrites(uint32_t count,const VkWriteDescriptorSet* w,uint32_t copies,const VkCopyDescriptorSet* c) noexcept {motionReprojProbe::writes(count,w,copies,c);}
void mrpReset(VkCommandBuffer c) noexcept {motionReprojProbe::reset(c);}
void mrpBind(VkCommandBuffer c,VkPipelineBindPoint point,VkPipeline p) noexcept {motionReprojProbe::bind(c,point,p);}
void mrpSets(VkCommandBuffer c,VkPipelineBindPoint point,uint32_t first,uint32_t count,const VkDescriptorSet* s,uint32_t dynamicCount,const uint32_t* offsets) noexcept {motionReprojProbe::bindSets(c,point,first,count,s,dynamicCount,offsets);}
void mrpDispatch(VkCommandBuffer c,uint32_t gx,uint32_t gy) noexcept {motionReprojProbe::dispatch(c,gx,gy);}
void mrpSubmitted(VkCommandBuffer c) noexcept {try{motionReprojProbe::submitted(c);}catch(...){}}
// The flags image of the motion image behind a DLSS evaluation's motion view (both views lead to the same image).
bool mrpFlagsFor(VkImageView motionView,VkImage* image,VkImageLayout* layout) noexcept {
    try{VkImage motionImage{};
        {std::lock_guard<std::mutex> l(mutex);for(const auto& d:imageViews){const auto v=d.second.find(motionView);if(v!=d.second.end()){motionImage=v->second.image;break;}}}
        if(!motionImage)return false;
        std::vector<std::pair<VkImageView,motionReprojProbe::FlagsOf>> all;
        {std::lock_guard<std::mutex> l(motionReprojProbe::lock);all.assign(motionReprojProbe::flagsOfMotion.begin(),motionReprojProbe::flagsOfMotion.end());}
        std::lock_guard<std::mutex> l(mutex);
        for(const auto& entry:all){VkImage si{},fi{};
            for(const auto& d:imageViews){const auto v=d.second.find(entry.first);if(v!=d.second.end())si=v->second.image;const auto w=d.second.find(entry.second.view);if(w!=d.second.end())fi=w->second.image;}
            if(si==motionImage&&fi){*image=fi;*layout=entry.second.layout;return true;}}
    }catch(...){}
    return false;
}
