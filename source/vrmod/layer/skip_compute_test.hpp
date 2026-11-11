#pragma once
// Test only (2026-11-11, the stick-turn motion vector bursts that come with DLSS and not in a RenderDoc capture with FSR -
// docs/analysis/motion_blur_20261111.md): EVR_TEST_SKIP_COMPUTE_BYTES=<SPIR-V bytes>[/<bytes>...] leaves out every dispatch
// of a compute pipeline whose shader module has that size, to see which of the game's passes that write the eyes' motion
// image after its depth reprojection (a temporal pass, 38624 bytes; the lighting/water pass, 27004) makes the bursts. The
// picture is wrong while a pass is left out; never in play.
#include <unordered_set>
namespace skipCompute {
inline const std::vector<size_t> sizes=[]{std::vector<size_t> out;const char* v=std::getenv("EVR_TEST_SKIP_COMPUTE_BYTES");if(!v)return out;
    std::string t(v);size_t at=0;while(at<t.size()){auto e=t.find_first_of("/,",at);if(e==std::string::npos)e=t.size();
        const auto n=std::strtoull(t.substr(at,e-at).c_str(),nullptr,10);if(n)out.push_back(size_t(n));at=e+1;}return out;}();
inline std::mutex lock;inline std::unordered_set<VkShaderModule> modules;inline std::unordered_set<VkPipeline> pipelines;
inline std::unordered_map<VkCommandBuffer,VkPipeline> bound;inline std::atomic<uint64_t> skipped{0};
inline void shaderCreated(VkShaderModule module,size_t bytes){
    if(sizes.empty()||!module||std::find(sizes.begin(),sizes.end(),bytes)==sizes.end())return;
    std::lock_guard<std::mutex> l(lock);modules.insert(module);
}
inline void pipelineCreated(VkShaderModule module,VkPipeline pipeline){
    if(sizes.empty())return;std::lock_guard<std::mutex> l(lock);if(modules.count(module))pipelines.insert(pipeline);
}
inline void bind(VkCommandBuffer c,VkPipelineBindPoint point,VkPipeline pipeline){
    if(sizes.empty()||point!=VK_PIPELINE_BIND_POINT_COMPUTE)return;std::lock_guard<std::mutex> l(lock);bound[c]=pipeline;
}
inline bool skip(VkCommandBuffer c){
    if(sizes.empty())return false;
    {std::lock_guard<std::mutex> l(lock);const auto it=bound.find(c);if(it==bound.end()||!pipelines.count(it->second))return false;}
    const auto n=++skipped;if(n<=3||n%2000==0)note("test_compute_skipped","\"count\":"+std::to_string(n));
    return true;
}
} // namespace skipCompute
