// Explicit, time-bounded native projection calibration. This is NOT a VR mode.
// Changes only the output of the render camera's descriptor conversion, after
// its original function returns. No player, input, game camera or save writes.
#include <MinHook.h>
#include <evr/game_stereo.hpp>
#include <evr/game_build.hpp>
#include <evr/recorded_xr.hpp>
#include <fstream>
#include <evr/log.hpp>
#include <evr/observer_error.hpp>
#include <evr/signature.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <intrin.h>
#include <thread>
#include <windows.h>
using namespace evr;
namespace
{
using Convert = void *(__fastcall *)(void *, const void *, float);
Convert original = nullptr;
uintptr_t gameBase = 0;
constexpr uintptr_t targetRva = 0x118190, callerRva = 0x12ae4d;
constexpr const char *patternText = "48 89 5C 24 10 57 48 83 EC 30 0F 29 74 24 20 48 8B FA 0F 28 F2 48 8B D9";
std::atomic<bool> started{false};
std::atomic<unsigned> phase{5};
std::array<std::atomic<uint64_t>, 5> counts{};
std::atomic<uint64_t> rejected{0};
bool recordedFov=false;
std::array<XrFovf,2> recordedFovs{};
Log *retainedLog = nullptr; // pinned with trampoline: callbacks may still be returning after disable

__declspec(noinline) void * __fastcall convert(void *output, const void *camera, float aspect)
{
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    void *result = original(output, camera, aspect);
    const ObserverLastError preserve;
    const unsigned step = phase.load(std::memory_order_acquire);
    if (caller != gameBase + callerRva || step >= counts.size()) return result;
    try
    {
        std::array<uint8_t, 80> prior{};
        std::memcpy(prior.data(), output, prior.size());
        uint32_t kind = 0;
        std::memcpy(&kind, prior.data(), sizeof(kind));
        if (kind != 0) { ++rejected; return result; } // orthographic/special camera kept native
        std::array<float, 4> values{};
        std::memcpy(values.data(), prior.data()+4, sizeof(values)); // aspect, near, far, fov
        const float halfY = std::tan(values[3]*.5f), halfX = halfY*values[0];
        if (!std::isfinite(halfY) || !std::isfinite(halfX) || halfY <= 0 || halfX <= 0)
        { ++rejected; return result; }
        const float shift = step == 2 ? .25f : step == 3 ? -.25f : 0;
        XrFovf fov{std::atan(halfX*(-1+shift)), std::atan(halfX*(1+shift)),
                         std::atan(halfY), std::atan(-halfY)};
        if(recordedFov&&step>=1&&step<=3)fov=recordedFovs[step==2?1:0];
        const auto descriptor = game::projectionDescriptor(fov, values[1], values[2]);
        const bool applied = step >= 1 && step <= 3;
        if (applied) std::memcpy(output, descriptor.bytes.data(), descriptor.bytes.size());
        if (counts[step].fetch_add(1, std::memory_order_relaxed) == 0)
        {
            std::ostringstream fields;
            fields << "\"phase\":" << step << ",\"descriptor_overridden\":" << (applied ? "true" : "false")
                   << ",\"caller_rva\":\"0x12ae4d\",\"original_type\":" << kind
                   << ",\"output_type\":" << (applied ? 3 : kind)
                   << ",\"aspect\":" << values[0] << ",\"near\":" << values[1] << ",\"far\":" << values[2]
                   << ",\"fov\":[" << fov.angleLeft << ',' << fov.angleRight << ',' << fov.angleUp << ',' << fov.angleDown
                   << "],\"horizontal_shift_fraction\":" << shift << ",\"game_camera_written\":false";
            retainedLog->event("projection_phase_applied", fields.str());
        }
    }
    catch (...) { ++rejected; } // descriptor is built completely before publishing
    return result;
}
void require(MH_STATUS status, const char *operation)
{
    if (status != MH_OK) throw std::runtime_error(std::string(operation)+": "+MH_StatusToString(status));
}
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeProjection(void *outputDirectory)
{
    if (!outputDirectory || started.exchange(true)) return 2;
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&EVR_ProbeProjection), &pinned)) return 3;
    void *target = nullptr;
    bool enabled = false;
    try
    {
        const std::filesystem::path out(static_cast<const wchar_t *>(outputDirectory));
        retainedLog = new Log((out / "projection-probe.jsonl").string(), true);
        auto *base = reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));
        gameBase = reinterpret_cast<uintptr_t>(base);
        const auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) throw std::runtime_error("PE DOS signature");
        const auto *nt = reinterpret_cast<IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != evr::gameBuild::timeDateStamp || nt->OptionalHeader.SizeOfImage != evr::gameBuild::sizeOfImage)
            throw std::runtime_error("unsupported loaded executable build");
        if(recordedFov){std::ifstream in(out/"recorded-xr.txt");const auto record=diagnostic::RecordedXr::read(in);
            for(size_t eye=0;eye<2;eye++)recordedFovs[eye]=record.frames[0].views[eye].fov;
            retainedLog->event("recorded_fov_comparison","\"full_original_view_features\":true,\"private_eye_views\":false,\"camera_pose_changed\":false");}
        const auto pattern = parsePattern(patternText);
        std::vector<uintptr_t> hits;
        const auto *section = IMAGE_FIRST_SECTION(nt);
        for (uint32_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            if (!(section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            const size_t rva = section[i].VirtualAddress, size = section[i].Misc.VirtualSize;
            if (rva+size > nt->OptionalHeader.SizeOfImage) throw std::runtime_error("invalid section range");
            for (auto offset : scan(base+rva, size, pattern)) hits.push_back(rva+offset);
        }
        if (hits.size()!=1 || hits[0]!=targetRva) throw std::runtime_error("projection signature missing/ambiguous");
        // Independently verify the exact caller instruction leading to f8acf.
        const uint8_t call[]{0xe8,0x43,0xd3,0xfe,0xff};   // 1.0: call 0x1180f0 at 0x12ada8
        if (std::memcmp(base+callerRva-5, call, sizeof(call))) throw std::runtime_error("projection caller changed");
        target=base+targetRva;
        require(MH_Initialize(), "MH_Initialize");
        require(MH_CreateHook(target, reinterpret_cast<void *>(&convert), reinterpret_cast<void **>(&original)), "MH_CreateHook");
        require(MH_EnableHook(target), "MH_EnableHook");
        enabled=true;
        retainedLog->event("projection_hook_enabled", "\"rva\":\"0x118190\",\"pattern\":"+jsonString(patternText)+
            ",\"mode\":\"bounded_projection_calibration\",\"stereo\":false,\"duration_seconds\":15");
        for (unsigned step=0; step<counts.size(); ++step)
        {
            phase.store(step, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        phase.store(5, std::memory_order_release);
        require(MH_DisableHook(target), "MH_DisableHook"); enabled=false;
        if (scan(static_cast<uint8_t *>(target), pattern.size(), pattern)!=std::vector<size_t>{0})
            throw std::runtime_error("original instruction restoration mismatch");
        bool complete=true;
        std::ostringstream fields;
        fields << "\"phase_counts\":[";
        for (size_t i=0;i<counts.size();++i) { if(i)fields<<','; const auto n=counts[i].load(); fields<<n; complete &= n>0; }
        fields << "],\"rejected\":" << rejected.load()
               << ",\"original_bytes_restored\":true,\"trampoline_retained\":true";
        retainedLog->event("projection_hook_complete", fields.str());
        return complete && !rejected.load() ? 0 : 4;
    }
    catch (const std::exception &error)
    {
        phase.store(5, std::memory_order_release);
        const auto cleanup=enabled?MH_DisableHook(target):MH_OK;
        if (retainedLog) try { retainedLog->event("projection_hook_failure", "\"reason\":"+jsonString(error.what())+
            ",\"disable_status\":"+jsonString(MH_StatusToString(cleanup))); } catch (...) {}
        return 1;
    }
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason==DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeRecordedFov(void* directory){recordedFov=true;return EVR_ProbeProjection(directory);}
