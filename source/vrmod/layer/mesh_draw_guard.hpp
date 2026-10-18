#pragma once
#include <unordered_set>
#include <unordered_map>
// A mesh-task draw while a pipeline without mesh stages is bound is not valid Vulkan, and the GPU then reads the
// indirect arguments as task group counts. On 1.0 the left eye records one such draw (RenderDoc of Mock
// game-20261016-184244: event 7335, vkCmdDrawMeshTasksIndirectCountEXT with a vertex/fragment pipeline, arguments
// written as indexed draws of 20 bytes by the compute shader 3C48ED152C; the right eye records the same draw as
// vkCmdDrawIndexedIndirectCount). While its count stays 0 nothing happens; when it is 1 the GPU runs
// 294 x 1 x 125954872 groups: 170+ ms a frame, then "Vulkan device lost" (Mock game-20261016-182449; headset run
// 2026-10-16 21:48, logs 20261016-214837: frames of 300..660 ms, then the crash). Why the game chooses the mesh
// command there for the left eye only is not known yet.
// This guard keeps, per command buffer, the graphics pipeline bound last and whether that pipeline has a task or mesh
// stage, and does not pass a mesh-task draw on to the driver when it has not (the draw draws nothing then - what it
// would draw in that eye is left out for that frame). The first ones go on record with the game's call stack
// (mesh_draw_guard_skipped); mesh_draw_guard_summary when the device goes. EVR_MESH_DRAW_GUARD=0: off.
namespace meshDrawGuard {
inline const bool enabled = [] { const char *v = std::getenv("EVR_MESH_DRAW_GUARD"); return !(v && v[0] == '0' && !v[1]); }();
inline std::mutex guard;
inline std::unordered_set<uint64_t> meshPipelines;
inline std::unordered_map<uint64_t, uint64_t> bound;   // command buffer -> graphics pipeline bound last
inline std::atomic<uint64_t> skipped{0}, passed{0};
inline void created(const VkGraphicsPipelineCreateInfo &info, VkPipeline pipeline) noexcept
{
    if (!enabled || !pipeline) return;
    bool mesh = false;
    for (uint32_t j = 0; j < info.stageCount; j++)
        if (info.pStages[j].stage & (VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)) mesh = true;
    if (!mesh) return;
    try { std::lock_guard<std::mutex> l(guard); meshPipelines.insert(uint64_t(pipeline)); } catch (...) {}
}
inline void destroyed(VkPipeline pipeline) noexcept
{
    if (!enabled || !pipeline) return;
    try { std::lock_guard<std::mutex> l(guard); meshPipelines.erase(uint64_t(pipeline)); } catch (...) {}
}
inline void bind(VkCommandBuffer c, VkPipelineBindPoint point, VkPipeline pipeline) noexcept
{
    if (!enabled || point != VK_PIPELINE_BIND_POINT_GRAPHICS) return;
    try { std::lock_guard<std::mutex> l(guard); bound[uint64_t(c)] = uint64_t(pipeline); } catch (...) {}
}
inline void forget(VkCommandBuffer c) noexcept
{
    if (!enabled) return;
    try { std::lock_guard<std::mutex> l(guard); bound.erase(uint64_t(c)); } catch (...) {}
}
// True: pass the draw on. A command buffer whose pipeline is not known (none bound through this layer) passes.
inline bool allow(VkCommandBuffer c, const char *kind, const std::string &detail) noexcept
{
    if (!enabled) return true;
    uint64_t pipeline = 0; bool known = false, mesh = false;
    try
    {
        std::lock_guard<std::mutex> l(guard);
        const auto b = bound.find(uint64_t(c));
        if (b != bound.end()) { known = true; pipeline = b->second; mesh = meshPipelines.count(pipeline) != 0; }
    }
    catch (...) { return true; }
    if (!known || mesh) { passed.fetch_add(1); return true; }
    const auto n = skipped.fetch_add(1);
    if (n < 8 || (n < 4096 && (n & (n - 1)) == 0))
        note("mesh_draw_guard_skipped", "\"kind\":\"" + std::string(kind) + "\",\"pipeline\":\"0x" + [&] { std::ostringstream s; s << std::hex << pipeline; return s.str(); }() +
                                            "\"," + detail + ",\"skipped\":" + std::to_string(n + 1) + ",\"game_rva_stack\":" + stack());
    return false;
}
inline void summary() noexcept
{
    if (enabled && (skipped || passed))
        note("mesh_draw_guard_summary", "\"skipped\":" + std::to_string(skipped.load()) + ",\"passed\":" + std::to_string(passed.load()));
}
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer c, VkBuffer b, VkDeviceSize offset, VkBuffer count,
                                                               VkDeviceSize countOffset, uint32_t maximum, uint32_t stride)
{
    if (!meshDrawGuard::allow(c, "indirect_count", "\"offset\":" + std::to_string(offset) + ",\"count_offset\":" + std::to_string(countOffset) +
                                                       ",\"max_draws\":" + std::to_string(maximum) + ",\"stride\":" + std::to_string(stride)))
        return;
    deviceFn<PFN_vkCmdDrawMeshTasksIndirectCountEXT>(c, "vkCmdDrawMeshTasksIndirectCountEXT")(c, b, offset, count, countOffset, maximum, stride);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawMeshTasksIndirectEXT(VkCommandBuffer c, VkBuffer b, VkDeviceSize offset, uint32_t draws, uint32_t stride)
{
    if (!meshDrawGuard::allow(c, "indirect", "\"offset\":" + std::to_string(offset) + ",\"draws\":" + std::to_string(draws) + ",\"stride\":" + std::to_string(stride)))
        return;
    deviceFn<PFN_vkCmdDrawMeshTasksIndirectEXT>(c, "vkCmdDrawMeshTasksIndirectEXT")(c, b, offset, draws, stride);
}
VKAPI_ATTR void VKAPI_CALL evrCmdDrawMeshTasksEXT(VkCommandBuffer c, uint32_t x, uint32_t y, uint32_t z)
{
    if (!meshDrawGuard::allow(c, "direct", "\"groups\":[" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + "]"))
        return;
    deviceFn<PFN_vkCmdDrawMeshTasksEXT>(c, "vkCmdDrawMeshTasksEXT")(c, x, y, z);
}
