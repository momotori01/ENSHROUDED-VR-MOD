#pragma once
// Bounded CPU submission metadata. VK_SUCCESS means accepted, not GPU completion.
thread_local const char *queueCallContext = "outside_layer_present";
struct QueueCallScope
{
    const char *previous = queueCallContext;
    explicit QueueCallScope(const char *context) { queueCallContext = context; }
    ~QueueCallScope() { queueCallContext = previous; }
};
std::string moduleStack()
{
    void *addresses[32]{};
    const auto count = CaptureStackBackTrace(1, 32, addresses, nullptr);
    std::ostringstream out;
    out << '[';
    for (USHORT i = 0; i < count; ++i)
    {
        if (i) out << ',';
        HMODULE module{};
        wchar_t filename[32768]{};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(addresses[i]), &module) &&
            GetModuleFileNameW(module, filename, DWORD(std::size(filename))))
            out << "{\"module\":" << jsonString(std::filesystem::path(filename).filename().u8string())
                << ",\"rva\":" << uint64_t(reinterpret_cast<uintptr_t>(addresses[i]) - reinterpret_cast<uintptr_t>(module)) << '}';
        else out << "{\"module\":null,\"rva\":null}";
    }
    out << ']';
    return out.str();
}
std::unordered_map<void *, std::unordered_map<VkSemaphore, VkSemaphoreType>> semaphoreTypes;
std::string semaphoreType(VkQueue queue, VkSemaphore semaphore)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto device = semaphoreTypes.find(key(queue));
    if (device == semaphoreTypes.end()) return "unknown";
    auto found = device->second.find(semaphore);
    if (found == device->second.end()) return "unknown";
    return found->second == VK_SEMAPHORE_TYPE_TIMELINE ? "timeline" : "binary";
}
VKAPI_ATTR VkResult VKAPI_CALL evrCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *info,
    const VkAllocationCallbacks *allocator, VkSemaphore *semaphore)
{
    auto result = deviceFn<PFN_vkCreateSemaphore>(device, "vkCreateSemaphore")(device, info, allocator, semaphore);
    if (result == VK_SUCCESS) try
    {
        VkSemaphoreType type = VK_SEMAPHORE_TYPE_BINARY;
        auto node = static_cast<const VkBaseInStructure *>(info->pNext);
        for (int depth = 0; node && depth < 32; ++depth, node = node->pNext)
            if (node->sType == VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO)
                type = reinterpret_cast<const VkSemaphoreTypeCreateInfo *>(node)->semaphoreType;
        std::lock_guard<std::mutex> lock(mutex);
        semaphoreTypes[key(device)][*semaphore] = type;
    } catch (...) { note("semaphore_metadata_error"); }
    return result;
}
VKAPI_ATTR void VKAPI_CALL evrDestroySemaphore(VkDevice device, VkSemaphore semaphore,
    const VkAllocationCallbacks *allocator)
{
    auto fn = deviceFn<PFN_vkDestroySemaphore>(device, "vkDestroySemaphore");
    { std::lock_guard<std::mutex> lock(mutex);
      auto found = semaphoreTypes.find(key(device));
      if (found != semaphoreTypes.end()) found->second.erase(semaphore); }
    fn(device, semaphore, allocator);
}
bool traceQueueEnabled()
{
    static std::once_flag configured;
    std::call_once(configured, [] {
        if (auto value = std::getenv("EVR_QUEUE_TRACE_BUDGET")) {
            char *end = nullptr;
            auto count = std::strtoul(value, &end, 10);
            if (end != value && !*end && count <= 4096) queueTraceBudget = uint32_t(count);
        }
    });
    auto remaining = queueTraceBudget.load();
    while (remaining && !queueTraceBudget.compare_exchange_weak(remaining, remaining - 1)) {}
    return remaining != 0;
}
std::string queueTraceBase(VkQueue queue)
{
    uint32_t family = UINT32_MAX;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = queueFamilies.find(queue);
        if (it != queueFamilies.end()) family = it->second;
    }
    return "\"queue\":" + handleText(queue) + ",\"family\":" + std::to_string(family) +
           ",\"thread\":" + std::to_string(GetCurrentThreadId()) + ",\"last_present\":" + std::to_string(presents.load()) +
           ",\"call_context\":" + jsonString(queueCallContext);
}
VKAPI_ATTR VkResult VKAPI_CALL evrQueueSubmit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
    exitProgress::Scope exitTrace("submit",reinterpret_cast<uint64_t>(queue));
    uiGlyph::beforeSubmit(queue);
    auto fn = deviceFn<PFN_vkQueueSubmit>(queue, "vkQueueSubmit");
    const bool record = traceQueueEnabled();
    std::string metadata;
    if (record) try
    {
        std::ostringstream out;
        bool truncated = count > 64;
        out << queueTraceBase(queue) << ",\"fence\":" << handleText(fence) << ",\"batches\":[";
        for (uint32_t i = 0; i < std::min(count, 64u); ++i)
        {
            if (i) out << ',';
            const auto &submit = submits[i];
            const VkTimelineSemaphoreSubmitInfo *timeline = nullptr;
            auto node = static_cast<const VkBaseInStructure *>(submit.pNext);
            for (int depth = 0; node && depth < 32; ++depth, node = node->pNext)
                if (node->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
                    timeline = reinterpret_cast<const VkTimelineSemaphoreSubmitInfo *>(node);
            truncated |= node != nullptr;
            out << "{\"commands\":[";
            for (uint32_t j = 0; j < std::min(submit.commandBufferCount, 256u); ++j)
            { if (j) out << ','; out << handleText(submit.pCommandBuffers[j]); }
            out << "],\"waits\":[";
            for (uint32_t j = 0; j < std::min(submit.waitSemaphoreCount, 256u); ++j)
            {
                if (j) out << ',';
                out << "{\"semaphore\":" << handleText(submit.pWaitSemaphores[j]) << ",\"type\":" << jsonString(semaphoreType(queue, submit.pWaitSemaphores[j])) << ",\"stage\":" << submit.pWaitDstStageMask[j] << ",\"value\":";
                if (timeline && j < timeline->waitSemaphoreValueCount) out << timeline->pWaitSemaphoreValues[j]; else out << "null";
                out << '}';
            }
            out << "],\"signals\":[";
            for (uint32_t j = 0; j < std::min(submit.signalSemaphoreCount, 256u); ++j)
            {
                if (j) out << ',';
                out << "{\"semaphore\":" << handleText(submit.pSignalSemaphores[j]) << ",\"type\":" << jsonString(semaphoreType(queue, submit.pSignalSemaphores[j])) << ",\"value\":";
                if (timeline && j < timeline->signalSemaphoreValueCount) out << timeline->pSignalSemaphoreValues[j]; else out << "null";
                out << '}';
            }
            out << "]}";
            truncated |= submit.commandBufferCount > 256 || submit.waitSemaphoreCount > 256 || submit.signalSemaphoreCount > 256;
        }
        out << "],\"truncated\":" << (truncated ? "true" : "false") << ",\"game_rva_stack\":" << stack()
            << ",\"module_stack\":" << moduleStack();
        metadata = out.str();
    }
    catch (...) { note("queue_trace_metadata_error"); }
    const auto started = std::chrono::steady_clock::now();
    const auto result = fn(queue, count, submits, fence);
    exitTrace.end(result);
    for(uint32_t batch=0;batch<count;++batch)
        for(uint32_t command=0;command<submits[batch].commandBufferCount;++command)
            nativeCommandSubmitted(queue,submits[batch].pCommandBuffers[command],result);
    if (record && !metadata.empty())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
        note("queue_submit", metadata + ",\"cpu_call_us\":" + std::to_string(elapsed) + ",\"result\":" + std::to_string(result));
    }
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrQueueWaitIdle(VkQueue queue)
{
    exitProgress::Scope exitTrace("queue_wait_idle",reinterpret_cast<uint64_t>(queue));
    auto fn = deviceFn<PFN_vkQueueWaitIdle>(queue, "vkQueueWaitIdle");
    const bool record = traceQueueEnabled();
    const auto result = fn(queue);
    exitTrace.end(result);
    if (record) note("queue_wait_idle", queueTraceBase(queue) + ",\"result\":" + std::to_string(result) + ",\"game_rva_stack\":" + stack());
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL evrDeviceWaitIdle(VkDevice device)
{
    exitProgress::Scope exitTrace("device_wait_idle",reinterpret_cast<uint64_t>(device));
    auto fn = deviceFn<PFN_vkDeviceWaitIdle>(device, "vkDeviceWaitIdle");
    const bool record = traceQueueEnabled();
    const auto result = fn(device);
    exitTrace.end(result);
    if (record) note("device_wait_idle", "\"device\":" + handleText(device) + ",\"thread\":" + std::to_string(GetCurrentThreadId()) + ",\"result\":" + std::to_string(result));
    return result;
}
