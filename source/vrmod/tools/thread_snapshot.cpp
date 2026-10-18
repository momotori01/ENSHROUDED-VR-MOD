// Read-only thread/context snapshots. No debugger attach, breakpoints, or target writes.
// Stack words are candidates, not unwound call frames: live memory can change after capture.
#include <evr/log.hpp>
#include <windows.h>
#include <processsnapshot.h>
#include <tlhelp32.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace evr;
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Snapshot {
    HPSS value = nullptr;
    ~Snapshot() { if (value) PssFreeSnapshot(GetCurrentProcess(), value); }
    void close() {
        if (value) {
            const auto result = PssFreeSnapshot(GetCurrentProcess(), value);
            if (result) throw std::runtime_error("PssFreeSnapshot " + std::to_string(result));
            value = nullptr;
        }
    }
};
struct Marker {
    HPSSWALK value = nullptr;
    ~Marker() { if (value) PssWalkMarkerFree(value); }
};
static bool readMemory(HANDLE process, uintptr_t address, void* data, size_t size) {
    SIZE_T actual = 0;
    return ReadProcessMemory(process, reinterpret_cast<void*>(address), data, size, &actual) && actual == size;
}
int main(int argc, char** argv) {
    try {
        DWORD pid = 0;
        int count = 1, interval = 100;
        std::string output, expected;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--pid" && i + 1 < argc) pid = std::stoul(argv[++i]);
            else if (a == "--count" && i + 1 < argc) count = std::stoi(argv[++i]);
            else if (a == "--interval-ms" && i + 1 < argc) interval = std::stoi(argv[++i]);
            else if (a == "--output" && i + 1 < argc) output = argv[++i];
            else if (a == "--expected-path" && i + 1 < argc) expected = argv[++i];
            else throw std::runtime_error("invalid argument");
        }
        if (!pid || count < 1 || count > 100 || interval < 50 || interval > 1000 || output.empty() || expected.empty())
            throw std::runtime_error("invalid bounds or missing target identity");
        Handle process{OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid)};
        if (!process.value) throw std::runtime_error("OpenProcess " + std::to_string(GetLastError()));
        wchar_t actual[32768]{};
        DWORD length = 32768;
        if (!QueryFullProcessImageNameW(process.value, 0, actual, &length) ||
            !std::filesystem::equivalent(std::filesystem::path(actual), std::filesystem::path(expected)))
            throw std::runtime_error("target executable mismatch");
        Handle modules{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)};
        MODULEENTRY32W module{};
        module.dwSize = sizeof(module);
        if (modules.value == INVALID_HANDLE_VALUE || !Module32FirstW(modules.value, &module))
            throw std::runtime_error("main module unavailable");
        const uintptr_t base = reinterpret_cast<uintptr_t>(module.modBaseAddr);
        std::filesystem::create_directories(std::filesystem::path(output).parent_path());
        Log log(output, true);
        log.event("snapshot_identity", "\"pid\":" + std::to_string(pid) + ",\"module_base\":" + std::to_string(base));
        BOOL debugged = false;
        if (!CheckRemoteDebuggerPresent(process.value, &debugged) || debugged)
            throw std::runtime_error("target already debugged; snapshot refused");
        for (int sample = 0; sample < count; ++sample) {
            Snapshot snapshot;
            const auto start = std::chrono::steady_clock::now();
            DWORD error = PssCaptureSnapshot(process.value,
                PSS_CAPTURE_FLAGS(PSS_CAPTURE_THREADS | PSS_CAPTURE_THREAD_CONTEXT), CONTEXT_FULL, &snapshot.value);
            const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (error) throw std::runtime_error("PssCaptureSnapshot " + std::to_string(error));
            Marker marker;
            if (PssWalkMarkerCreate(nullptr, &marker.value)) throw std::runtime_error("PssWalkMarkerCreate");
            PSS_THREAD_ENTRY entry{};
            uint32_t threads = 0;
            uint32_t entries = 0;
            while ((error = PssWalkSnapshot(snapshot.value, PSS_WALK_THREADS, marker.value, &entry, sizeof(entry))) == ERROR_SUCCESS) {
                ++entries;
                if (!entry.ContextRecord || entry.SizeOfContextRecord < sizeof(CONTEXT)) {
                    log.event("thread_context_unavailable", "\"thread\":" + std::to_string(entry.ThreadId) +
                        ",\"context_bytes\":" + std::to_string(entry.SizeOfContextRecord) +
                        ",\"suspend_count\":" + std::to_string(entry.SuspendCount));
                    continue;
                }
                const auto& c = *entry.ContextRecord;
                std::ostringstream detail;
                detail << "\"sample\":" << sample << ",\"thread\":" << entry.ThreadId
                       << ",\"suspend_count\":" << entry.SuspendCount << ",\"rip\":\"0x" << std::hex << c.Rip
                       << "\",\"rsp\":\"0x" << c.Rsp << "\",\"start\":\"0x"
                       << reinterpret_cast<uintptr_t>(entry.Win32StartAddress) << "\",\"stack_candidates\":[";
                // Limit reads to the captured thread stack bounds (TEB NT_TIB prefix).
                std::array<uintptr_t, 3> tib{};
                std::array<uintptr_t, 1024> words{};
                size_t bytes = 0;
                if (readMemory(process.value, reinterpret_cast<uintptr_t>(entry.TebBaseAddress), tib.data(), sizeof(tib)) &&
                    c.Rsp >= tib[2] && c.Rsp < tib[1])
                    bytes = size_t(std::min<uint64_t>(sizeof(words), tib[1] - c.Rsp));
                bool readable = bytes && readMemory(process.value, c.Rsp, words.data(), bytes);
                bool first = true;
                if (readable) for (size_t w = 0; w < bytes / 8; ++w) {
                    if (words[w] < base || words[w] >= base + module.modBaseSize) continue;
                    if (!first) detail << ',';
                    first = false;
                    detail << "{\"stack_offset\":\"0x" << w * 8 << "\",\"rva\":\"0x" << words[w] - base << "\"}";
                }
                detail << "],\"stack_read\":" << (readable ? "true" : "false")
                       << ",\"stack_base\":\"0x" << tib[1] << "\",\"stack_limit\":\"0x" << tib[2] << "\"";
                log.event("thread_snapshot", detail.str());
                ++threads;
            }
            if (error != ERROR_NO_MORE_ITEMS) throw std::runtime_error("PssWalkSnapshot " + std::to_string(error));
            log.event("snapshot_complete", "\"sample\":" + std::to_string(sample) + ",\"threads\":" +
                std::to_string(threads) + ",\"entries\":" + std::to_string(entries) + ",\"capture_us\":" + std::to_string(duration));
            // The descriptor belongs to this caller, even when the captured process is remote.
            // Release the marker before its snapshot, and verify normal-path cleanup.
            PssWalkMarkerFree(marker.value);
            marker.value = nullptr;
            snapshot.close();
            if (sample + 1 < count) std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        if (!CheckRemoteDebuggerPresent(process.value, &debugged) || debugged)
            throw std::runtime_error("unexpected debugger state after capture");
        std::cout << "PASS " << count << " thread snapshots; no debugger attached\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
