#pragma once
// Included after layer dispatch helpers. Metadata only: no buffer or shader changes.
std::atomic<uint32_t> traceBudget{0};
std::atomic<uint32_t> queueTraceBudget{0};
template <class H> std::string handleText(H handle)
{
    std::ostringstream s;
    s << '"' << "0x" << std::hex << (uint64_t)handle << '"';
    return s.str();
}
bool traceEnabled()
{
    uint32_t n = traceBudget.load();
    while (n && !traceBudget.compare_exchange_weak(n, n - 1))
    {
    }
    return n != 0;
}
void pollTraceRequest(uint64_t frame) noexcept
{
    if (frame % 30)
        return;
    try
    {
        std::error_code ec;
        if (std::filesystem::remove(std::filesystem::path(output) / "trace.request", ec))
        {
            traceBudget = 6000;
            queueTraceBudget = 256;
            note("trace_begin", "\"present\":" + std::to_string(frame) + ",\"max_records\":6000");
        }
    }
    catch (...)
    {
    }
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateGraphicsPipelines(VkDevice d, VkPipelineCache cache, uint32_t count,
                                                          const VkGraphicsPipelineCreateInfo *info,
                                                          const VkAllocationCallbacks *allocator,
                                                          VkPipeline *pipelines)
{
    auto r = deviceFn<PFN_vkCreateGraphicsPipelines>(d, "vkCreateGraphicsPipelines")(d, cache, count, info,
                                                                                     allocator, pipelines);
    for (uint32_t i = 0; i < count; i++)
        if (pipelines[i])
        {
            std::ostringstream s;
            s << "\"pipeline\":" << handleText(pipelines[i]) << ",\"layout\":" << handleText(info[i].layout)
              << ",\"render_pass\":" << handleText(info[i].renderPass) << ",\"stages\":[";
            for (uint32_t j = 0; j < info[i].stageCount; j++)
            {
                if (j)
                    s << ',';
                s << "{\"stage\":" << info[i].pStages[j].stage
                  << ",\"module\":" << handleText(info[i].pStages[j].module) << '}';
            }
            s << ']';
            note("graphics_pipeline", s.str());
            uiCapture::pipelineCreated(d,info[i],pipelines[i]);
            meshDrawGuard::created(info[i],pipelines[i]);
        }
    return r;
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateComputePipelines(VkDevice d, VkPipelineCache cache, uint32_t count,
                                                         const VkComputePipelineCreateInfo *info,
                                                         const VkAllocationCallbacks *allocator,
                                                         VkPipeline *pipelines)
{
    auto r = deviceFn<PFN_vkCreateComputePipelines>(d, "vkCreateComputePipelines")(d, cache, count, info,
                                                                                   allocator, pipelines);
    for (uint32_t i = 0; i < count; i++)
        if (pipelines[i])
        {
            giReprojectPipeline(info[i].stage.module, pipelines[i]);
            note("compute_pipeline", "\"pipeline\":" + handleText(pipelines[i]) +
                                         ",\"layout\":" + handleText(info[i].layout) +
                                         ",\"module\":" + handleText(info[i].stage.module));
        }
    return r;
}
VKAPI_ATTR void VKAPI_CALL evrCmdBindPipeline(VkCommandBuffer c, VkPipelineBindPoint point,
                                              VkPipeline pipeline)
{
    if (traceEnabled())
        note("trace_pipeline", "\"cmd\":" + handleText(c) + ",\"bind_point\":" + std::to_string(point) +
                                   ",\"pipeline\":" + handleText(pipeline) +
                                   ",\"game_rva_stack\":" + stack());
    uiDraw::bind(c,point,pipeline);gpuTimingBind(c,point,pipeline);meshDrawGuard::bind(c,point,pipeline);giReprojectBind(c,point,pipeline);
    deviceFn<PFN_vkCmdBindPipeline>(c, "vkCmdBindPipeline")(c, point, pipeline);
}
VKAPI_ATTR void VKAPI_CALL evrCmdBindDescriptorSets(VkCommandBuffer c, VkPipelineBindPoint point,
                                                    VkPipelineLayout layout, uint32_t first, uint32_t count,
                                                    const VkDescriptorSet *sets, uint32_t dynamicCount,
                                                    const uint32_t *offsets)
{
    if (traceEnabled())
    {
        std::ostringstream s;
        s << "\"cmd\":" << handleText(c) << ",\"layout\":" << handleText(layout)
          << ",\"bind_point\":" << point << ",\"first_set\":" << first << ",\"sets\":[";
        for (uint32_t i = 0; i < count; i++)
        {
            if (i)
                s << ',';
            s << handleText(sets[i]);
        }
        s << "],\"dynamic_offsets\":[";
        for (uint32_t i = 0; i < dynamicCount; i++)
        {
            if (i)
                s << ',';
            s << offsets[i];
        }
        s << ']';
        note("trace_bind_sets", s.str());
    }
    gpuTimingSets(c, point, first, count, sets, dynamicCount, offsets);
    giReprojectSets(c, point, layout, first, count, sets, dynamicCount, offsets);
    deviceFn<PFN_vkCmdBindDescriptorSets>(c, "vkCmdBindDescriptorSets")(c, point, layout, first, count, sets,
                                                                        dynamicCount, offsets);
}
VKAPI_ATTR void VKAPI_CALL evrUpdateDescriptorSets(VkDevice d, uint32_t count,
                                                   const VkWriteDescriptorSet *writes, uint32_t copies,
                                                   const VkCopyDescriptorSet *copy)
{
    if (traceEnabled())
    {
        for (uint32_t i = 0; i < count; i++)
        {
            const auto &w = writes[i];
            if (w.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
                w.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
                w.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
                w.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                continue;
            for (uint32_t j = 0; j < w.descriptorCount; j++)
            {
                const auto &b = w.pBufferInfo[j];
                note("trace_buffer_binding",
                     "\"set\":" + handleText(w.dstSet) + ",\"binding\":" + std::to_string(w.dstBinding) +
                         ",\"element\":" + std::to_string(w.dstArrayElement + j) + ",\"type\":" +
                         std::to_string(w.descriptorType) + ",\"buffer\":" + handleText(b.buffer) +
                         ",\"offset\":" + std::to_string(b.offset) + ",\"range\":" + std::to_string(b.range));
            }
        }
    }
    gpuTimingDescriptorWrites(count, writes);
    giReprojectWrites(count, writes, copies, copy);
    deviceFn<PFN_vkUpdateDescriptorSets>(d, "vkUpdateDescriptorSets")(d, count, writes, copies, copy);
}
