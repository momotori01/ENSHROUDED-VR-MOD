// Bounded observation. Original code restored after <=3 seconds of capture.
// DLL/trampoline stay resident until exit, protecting in-flight stub return addresses.
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <evr/log.hpp>
#include <evr/game_build.hpp>
#include <evr/observer_error.hpp>
#include <evr/observer_stack.hpp>
#include <evr/signature.hpp>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <thread>
#include <windows.h>
using namespace evr;
extern "C"
{
    void *evr_camera_trampoline = nullptr;
    void evr_camera_stub();
}
namespace
{
constexpr uint32_t limit = 20;
#ifdef EVR_RENDER_ENTRY
constexpr uintptr_t targetRva = 0x129ec0;
constexpr size_t componentBytes = 0xb98;
constexpr const char *patternText =
    "4C 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 48 8D AC";
constexpr const char *eventPrefix = "render";
constexpr const wchar_t *logFilename = L"render-observer.jsonl";
constexpr std::array<size_t, 10> readLengths{0, 64, 0xb98, 0x400, 24, 0x9a8, 8, 48, 8, 0xd0};
#define EVR_OBSERVE_EXPORT EVR_ObserveRender
#else
constexpr uintptr_t targetRva = 0x2b2aa0;
constexpr size_t componentBytes = 192;
constexpr const char *patternText = "48 8B 5C 24 68 48 8D 95 48 01 00 00";
constexpr const char *eventPrefix = "camera";
constexpr const wchar_t *logFilename = L"camera-observer.jsonl";
constexpr std::array<size_t, 10> readLengths{192, 192, 192, 192, 192, 192, 192, 192, 192, 192};
#define EVR_OBSERVE_EXPORT EVR_ObserveCamera
#endif
struct Sample
{
    DWORD thread{};
    uint64_t tick{};
    bool queryValid{};
    uintptr_t returnAddress{};
    std::array<uintptr_t, 10> pointers{};
    std::array<bool, 10> valid{};
    std::array<std::array<uint8_t, componentBytes>, 10> bytes{};
    uintptr_t taskContext{}, taskState{};
    uint32_t queryIndex{}, entity{}, localOwner{}, interpolationTick{}, updateSource{}, updateTarget{};
    float interpolationFraction{};
    bool contextValid{}, localEntity{};
};
std::array<Sample, limit> samples;
std::array<std::atomic<bool>, limit> ready{};
std::atomic<uint32_t> reserved{0};
std::atomic<bool> capturing{false}, started{false};
bool read(uintptr_t address, void *buffer, size_t size) noexcept
{
    SIZE_T bytes = 0;
    return address &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(address), buffer, size, &bytes) &&
           bytes == size;
}
void require(MH_STATUS result, const char *operation)
{
    if (result != MH_OK)
        throw std::runtime_error(std::string(operation) + ": " + MH_StatusToString(result));
}
} // namespace
extern "C" void evr_observe_camera(uintptr_t rsp) noexcept
{
    const ObserverLastError lastError;
    if (!capturing.load(std::memory_order_relaxed))
        return;
    const auto index = reserved.fetch_add(1, std::memory_order_relaxed);
    if (index >= limit)
        return;
    auto &sample = samples[index];
    sample.thread = GetCurrentThreadId();
    sample.tick = GetTickCount64();
#ifdef EVR_RENDER_ENTRY
    const auto args = observerEntryArguments(rsp);
    std::copy(args.begin(), args.end(), sample.pointers.begin());
    sample.queryValid = read(rsp, &sample.returnAddress, sizeof(sample.returnAddress));
    // Follow one statically observed scene pointer only; no arbitrary recursive reads.
    read(sample.pointers[2] + 8, &sample.pointers[5], sizeof(uintptr_t));
    uintptr_t renderer = 0;
    if (read(sample.pointers[5] + 8, &renderer, sizeof(renderer)) && renderer)
        sample.pointers[6] = renderer + 0x14960; // admission limit/count, not a simulation frame ID
    uintptr_t textureProvider = 0, textureTable = 0, frameContext = 0;
    uint16_t textureIndex = 0;
    if (read(args[4], &textureProvider, sizeof(textureProvider)) && textureProvider &&
        read(args[4] + 8, &textureIndex, sizeof(textureIndex)) &&
        read(textureProvider + 0x80, &textureTable, sizeof(textureTable)) && textureTable)
        sample.pointers[7] = textureTable + size_t(textureIndex) * 48; // f923d0 record layout
    if (read(args[1] + 8, &frameContext, sizeof(frameContext)) && frameContext)
        sample.pointers[8] = frameContext + 0x18;          // deb820 render context serial
    read(args[2], &sample.pointers[9], sizeof(uintptr_t)); // owning 0xd0-byte wrapper
#else
    sample.queryValid = read(rsp + 0x50, sample.pointers.data(), sizeof(sample.pointers));
    // Exact read-only equivalents of validated task getters 8d3490/8d32c0/
    // 8d7a90/8e2390. Do not invoke engine tasks or retain live component pointers.
    sample.taskContext = observerSavedRdi(rsp);
    uintptr_t tickTable = 0, entityTable = 0;
    if (sample.queryValid && sample.taskContext &&
        read(sample.taskContext, &sample.taskState, 8) && sample.taskState &&
        read(sample.taskContext + 8, &sample.queryIndex, 4) && sample.queryIndex &&
        read(sample.taskState + 0x7d0, &tickTable, 8) && tickTable &&
        read(sample.taskState + 0x7e0, &entityTable, 8) && entityTable)
    {
        const uintptr_t index = sample.queryIndex - 1;
        // UInt32 index arithmetic promoted before multiplication; no wrapping address.
        if (tickTable <= UINTPTR_MAX - index * 4 && entityTable <= UINTPTR_MAX - index * 16)
            sample.contextValid =
                read(tickTable + index * 4, &sample.interpolationTick, 4) &&
                read(entityTable + index * 16, &sample.entity, 4) &&
                read(sample.taskState + 0x88, &sample.interpolationFraction, 4) &&
                read(sample.taskState + 0x6c, &sample.updateSource, 4) &&
                read(sample.taskState + 0x70, &sample.updateTarget, 4) &&
                sample.pointers[3] && read(sample.pointers[3] + 4, &sample.localOwner, 4);
        sample.localEntity = sample.contextValid && sample.entity == sample.localOwner;
    }
#endif
    if (sample.queryValid)
        for (size_t i = 0; i < 10; ++i)
            sample.valid[i] =
                readLengths[i] && read(sample.pointers[i], sample.bytes[i].data(), readLengths[i]);
    ready[index].store(true, std::memory_order_release);
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_OBSERVE_EXPORT(void *outputDirectory)
{
    if (!outputDirectory || started.exchange(true))
        return 2;
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<LPCWSTR>(&EVR_OBSERVE_EXPORT), &pinned))
        return 3;
    std::unique_ptr<Log> log;
    void *target = nullptr;
    bool enabled = false;
    try
    {
        std::filesystem::path path(static_cast<const wchar_t *>(outputDirectory));
        std::filesystem::create_directories(path);
        log = std::make_unique<Log>((path / logFilename).string(), true);
        auto *base = reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));
        const auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            throw std::runtime_error("PE DOS signature");
        const auto *nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != evr::gameBuild::timeDateStamp || nt->OptionalHeader.SizeOfImage != evr::gameBuild::sizeOfImage)
            throw std::runtime_error("unsupported loaded executable build");
        const auto pattern = parsePattern(patternText);
        std::vector<uintptr_t> hits;
        const auto *section = IMAGE_FIRST_SECTION(nt);
        for (uint32_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            if (!(section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;
            const size_t rva = section[i].VirtualAddress, size = section[i].Misc.VirtualSize;
            if (rva + size > nt->OptionalHeader.SizeOfImage)
                throw std::runtime_error("invalid section range");
            for (auto offset : scan(base + rva, size, pattern))
                hits.push_back(rva + offset);
        }
        if (hits.size() != 1 || hits[0] != targetRva)
            throw std::runtime_error("camera code signature missing/ambiguous");
        target = base + hits[0];
        require(MH_Initialize(), "MH_Initialize");
        require(MH_CreateHook(target, reinterpret_cast<void *>(&evr_camera_stub), &evr_camera_trampoline),
                "MH_CreateHook");
        capturing = true;
        require(MH_EnableHook(target), "MH_EnableHook");
        enabled = true;
        std::ostringstream enabledFields;
        enabledFields << "\"rva\":\"0x" << std::hex << targetRva
                      << "\",\"mode\":\"observe_only\",\"max_samples\":20,\"pattern\":"
                      << jsonString(patternText);
        log->event(std::string(eventPrefix) + "_hook_enabled", enabledFields.str());
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!ready[limit - 1].load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        capturing = false;
        require(MH_DisableHook(target), "MH_DisableHook");
        enabled = false;
        // Trampoline intentionally retained even after the target's original bytes are restored.
        uint32_t captured = 0;
        for (uint32_t index = 0; index < limit; ++index)
        {
            if (!ready[index].load(std::memory_order_acquire))
                continue;
            ++captured;
            const auto &sample = samples[index];
            std::ostringstream fields;
            fields << "\"sample\":" << index << ",\"thread\":" << sample.thread
                   << ",\"tick_ms\":" << sample.tick << ",\"return_rva\":"
                   << jsonString(
                          sample.returnAddress >= reinterpret_cast<uintptr_t>(base) &&
                                  sample.returnAddress <
                                      reinterpret_cast<uintptr_t>(base) + nt->OptionalHeader.SizeOfImage
                              ? std::to_string(sample.returnAddress - reinterpret_cast<uintptr_t>(base))
                              : "outside_game_or_not_entry")
                   << ",\"query_read\":" << (sample.queryValid ? "true" : "false") << ",\"components\":[";
            for (size_t i = 0; i < 10; ++i)
            {
                if (i)
                    fields << ',';
                fields << "{\"index\":" << std::dec << i << ",\"address\":\"0x" << std::hex
                       << sample.pointers[i] << "\",\"readable\":" << (sample.valid[i] ? "true" : "false")
                       << ",\"bytes\":\"";
                if (sample.valid[i])
                    for (size_t j = 0; j < readLengths[i]; ++j)
                    {
                        auto b = sample.bytes[i][j];
                        const char *h = "0123456789abcdef";
                        fields << h[b >> 4] << h[b & 15];
                    }
                fields << "\"}";
            }
            fields << ']';
#ifndef EVR_RENDER_ENTRY
            fields << ",\"task_context\":{\"valid\":" << (sample.contextValid ? "true" : "false")
                   << ",\"address\":\"0x" << std::hex << sample.taskContext << "\",\"state\":\"0x"
                   << sample.taskState << "\",\"query_index\":" << std::dec << sample.queryIndex
                   << ",\"entity\":" << sample.entity << ",\"local_owner\":" << sample.localOwner
                   << ",\"local_entity\":" << (sample.localEntity ? "true" : "false")
                   << ",\"tick\":" << sample.interpolationTick << ",\"fraction\":";
            if (std::isfinite(sample.interpolationFraction))
                fields << std::setprecision(9) << sample.interpolationFraction;
            else
                fields << "null";
            fields << ",\"update_source\":" << sample.updateSource << ",\"update_target\":"
                   << sample.updateTarget << '}';
#endif
            log->event(std::string(eventPrefix) + "_query_sample", fields.str());
        }
        if (scan(static_cast<uint8_t *>(target), pattern.size(), pattern) != std::vector<size_t>{0})
            throw std::runtime_error("original instruction restoration mismatch");
        log->event(std::string(eventPrefix) + "_hook_complete",
                   "\"samples\":" + std::to_string(captured) +
                       ",\"original_bytes_restored\":true,\"trampoline_retained\":true");
        return captured ? 0 : 4;
    }
    catch (const std::exception &e)
    {
        capturing = false;
        const auto cleanup = enabled ? MH_DisableHook(target) : MH_OK;
        if (log)
            try
            {
                log->event(std::string(eventPrefix) + "_hook_failure",
                           "\"reason\":" + jsonString(e.what()) +
                               ",\"disable_status\":" + jsonString(MH_StatusToString(cleanup)));
            }
            catch (...)
            {
            }
        return 1;
    }
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
