#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <evr/observer_error.hpp>
#include <evr/observer_stack.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>
#include <xmmintrin.h>

extern "C"
{
    void *evr_camera_trampoline = nullptr;
    void evr_camera_stub();
    uint64_t evr_mid_fixture(const void *);
    uint64_t evr_entry_fixture(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    extern uint8_t evr_fixture_marker;
}
std::atomic<uint32_t> hits{0}, bad{0};
bool entryMode = false;
const std::array<uintptr_t, 10> expected{11, 22, 33, 44, 55, 66, 77, 88, 99, 110};
extern "C" void evr_fixture_observe(uintptr_t originalRsp) noexcept
{
    const evr::ObserverLastError lastError;
    SetLastError(ERROR_PARTIAL_COPY); // diagnostic read failure must not escape
    if (entryMode)
    {
        if (evr::observerEntryArguments(originalRsp) != std::array<uintptr_t, 5>{11, 22, 33, 44, 55})
            ++bad;
    }
    else
    {
        std::array<uintptr_t, 10> pointers{};
        std::memcpy(pointers.data(), reinterpret_cast<void *>(originalRsp + 0x50), sizeof(pointers));
        if (pointers != expected)
            ++bad;
        if (evr::observerSavedRdi(originalRsp) != 0x12345678abcdef01ull)
            ++bad;
    }
    ++hits;
    _mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) | _MM_ROUND_TOWARD_ZERO);
}
int main()
{
    const auto originalMxcsr = _mm_getcsr();
    const uint8_t pattern[]{0x48, 0x8b, 0x5c, 0x24, 0x68, 0x48, 0x8d, 0x95, 0x48, 0x01, 0x00, 0x00};
    auto require = [](bool ok, const char *text) {
        if (!ok)
        {
            std::cerr << "FAIL " << text << '\n';
            return false;
        }
        return true;
    };
    auto status = [&](MH_STATUS s, const char *text) { return require(s == MH_OK, text); };
    if (!require(!std::memcmp(&evr_fixture_marker, pattern, sizeof(pattern)),
                 "fixture machine-code signature"))
        return 1;
    if (!require(evr_mid_fixture(expected.data()) == 1036, "unhooked fixture"))
        return 1;
    if (!status(MH_Initialize(), "MH_Initialize"))
        return 1;
    if (!status(MH_CreateHook(&evr_fixture_marker, &evr_camera_stub, &evr_camera_trampoline),
                "MH_CreateHook"))
        return 1;
    if (!status(MH_EnableHook(&evr_fixture_marker), "MH_EnableHook"))
        return 1;
    for (int i = 0; i < 100000; ++i)
        if (evr_mid_fixture(expected.data()) != 1036 || _mm_getcsr() != originalMxcsr)
            ++bad;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]() {
            const auto mxcsr = _mm_getcsr();
            for (int i = 0; i < 25000; ++i)
                if (evr_mid_fixture(expected.data()) != 1036 || _mm_getcsr() != mxcsr)
                    ++bad;
        });
    for (auto &t : threads)
        t.join();
    if (!status(MH_DisableHook(&evr_fixture_marker), "MH_DisableHook"))
        return 1;
    if (!status(MH_RemoveHook(&evr_fixture_marker), "MH_RemoveHook"))
        return 1;
    if (!require(hits == 200000 && bad == 0, "register/flags/XMM/MXCSR/stack preservation"))
        return 1;
    if (!require(!std::memcmp(&evr_fixture_marker, pattern, sizeof(pattern)) &&
                     evr_mid_fixture(expected.data()) == 1036,
                 "original bytes and execution restored"))
        return 1;
    std::cout << "PASS 200000 in-process mid-hook calls, four worker threads, query data, "
                 "carry/GPR/XMM/MXCSR preservation, original bytes restored\n";
    entryMode = true;
    hits = 0;
    bad = 0;
    std::array<uint8_t, 15> entryBytes{};
    std::memcpy(entryBytes.data(), reinterpret_cast<void *>(&evr_entry_fixture), entryBytes.size());
    if (!status(MH_CreateHook(&evr_entry_fixture, &evr_camera_stub, &evr_camera_trampoline),
                "entry MH_CreateHook"))
        return 1;
    if (!status(MH_EnableHook(&evr_entry_fixture), "entry MH_EnableHook"))
        return 1;
    threads.clear();
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]() {
            const auto mxcsr = _mm_getcsr();
            SetLastError(1234);
            for (int i = 0; i < 25000; ++i)
                if (evr_entry_fixture(11, 22, 33, 44, 55) != 165 || _mm_getcsr() != mxcsr ||
                    GetLastError() != 1234)
                    ++bad;
        });
    for (auto &t : threads)
        t.join();
    if (!status(MH_DisableHook(&evr_entry_fixture), "entry MH_DisableHook"))
        return 1;
    if (!status(MH_RemoveHook(&evr_entry_fixture), "entry MH_RemoveHook"))
        return 1;
    if (!status(MH_Uninitialize(), "MH_Uninitialize"))
        return 1;
    if (!require(hits == 100000 && bad == 0, "entry register and fifth stack argument snapshot"))
        return 1;
    if (!require(!std::memcmp(reinterpret_cast<void *>(&evr_entry_fixture), entryBytes.data(),
                              entryBytes.size()) &&
                     evr_entry_fixture(11, 22, 33, 44, 55) == 165,
                 "entry original bytes and execution restored"))
        return 1;
    std::cout << "PASS 100000 entry-hook calls, four worker threads, five argument snapshots, execution and "
                 "code restoration\n";
    return 0;
}
