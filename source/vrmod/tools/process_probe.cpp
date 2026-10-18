// Bounded development debugger. Hardware execution breakpoint; no game code/data writes.
// Test the debugger itself against --fixture before attaching to a validated game build.
#include <array>
#include <chrono>
#include <evr/log.hpp>
#include <evr/signature.hpp>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>
#include <windows.h>
#include <tlhelp32.h>

using namespace evr;
volatile uint64_t fixtureSink = 0;
__declspec(noinline) void fixtureMarker(void *p)
{
    if (p)
        fixtureSink = *static_cast<volatile uint64_t *>(p);
}
struct SavedThread
{
    HANDLE handle{};
    CONTEXT original{};
};
static bool read(HANDLE p, uintptr_t address, void *data, size_t size)
{
    SIZE_T actual = 0;
    return address && ReadProcessMemory(p, reinterpret_cast<void *>(address), data, size, &actual) &&
           actual == size;
}
static uintptr_t moduleBase(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        throw std::runtime_error("module snapshot failed");
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool ok = Module32FirstW(snapshot, &entry);
    CloseHandle(snapshot);
    if (!ok)
        throw std::runtime_error("main module unavailable");
    return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
}
int main(int argc, char **argv)
{
    if (argc == 2 && std::string(argv[1]) == "--fixture")
    {
        std::array<uint64_t, 8> data{0x12345678, 0x9abcdef0};
        std::array<void *, 10> pointers{};
        for (auto &p : pointers)
            p = data.data();
        std::cout << GetCurrentProcessId() << ' ' << std::hex
                  << (reinterpret_cast<uintptr_t>(&fixtureMarker) -
                      reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)))
                  << std::endl;
        for (int i = 0; i < 15000; i++)
        {
            fixtureMarker(pointers.data());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return 0;
    }
    DWORD pid = 0;
    uintptr_t rva = 0;
    uint32_t seconds = 15, limit = 20;
    std::string out = "vrmod/output/process-probe.jsonl";
    std::string expectedPath, expectedBytes;
    bool rcxPointers = false;
    try
    {
        for (int i = 1; i < argc; i++)
        {
            std::string a = argv[i];
            if (a == "--pid" && i + 1 < argc)
                pid = std::stoul(argv[++i]);
            else if (a == "--rva" && i + 1 < argc)
                rva = std::stoull(argv[++i], nullptr, 16);
            else if (a == "--seconds" && i + 1 < argc)
                seconds = std::stoul(argv[++i]);
            else if (a == "--samples" && i + 1 < argc)
                limit = std::stoul(argv[++i]);
            else if (a == "--output" && i + 1 < argc)
                out = argv[++i];
            else if (a == "--expected-path" && i + 1 < argc)
                expectedPath = argv[++i];
            else if (a == "--expected-bytes" && i + 1 < argc)
                expectedBytes = argv[++i];
            else if (a == "--fixture-pointers")
                rcxPointers = true;
            else
                throw std::invalid_argument("invalid arguments");
        }
        if (!pid || !rva || !seconds || seconds > 120 || !limit || limit > 500)
            throw std::invalid_argument("invalid probe bounds");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 2;
    }
    std::filesystem::create_directories(std::filesystem::path(out).parent_path());
    Log log(out, true);
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process)
    {
        std::cerr << "OpenProcess failed " << GetLastError() << '\n';
        return 1;
    }
    BOOL already = false;
    {
        wchar_t actual[32768]{}; DWORD length = 32768;
        if (!QueryFullProcessImageNameW(process, 0, actual, &length)) {
            CloseHandle(process); std::cerr << "Target identity unavailable\n"; return 1;
        }
        if (_wcsicmp(std::filesystem::path(actual).filename().c_str(), L"enshrouded.exe") == 0) {
            CloseHandle(process);
            std::cerr << "Enshrouded hardware breakpoints disabled: exception reached game crash handler. Use offline dump/read-only memory tools.\n";
            return 1;
        }
    }
    if(!expectedPath.empty()) {
        wchar_t actual[32768]{};DWORD length=32768;
        std::error_code ec;
        if(!QueryFullProcessImageNameW(process,0,actual,&length)||
           !std::filesystem::equivalent(std::filesystem::path(actual),std::filesystem::path(expectedPath),ec)||ec) {
            CloseHandle(process);std::cerr<<"Target executable differs from validated path\n";return 1;
        }
    }
    if (!CheckRemoteDebuggerPresent(process, &already) || already)
    {
        CloseHandle(process);
        std::cerr << "Refusing existing debugger\n";
        return 1;
    }
    uintptr_t address = 0;
    try
    {
        address = moduleBase(pid) + rva;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what();
        CloseHandle(process);
        return 1;
    }
    uint8_t instruction[5]{};
    if (!read(process, address, instruction, sizeof(instruction)))
    {
        CloseHandle(process);
        std::cerr << "unreadable breakpoint address\n";
        return 1;
    }
    if(!expectedBytes.empty()) {
        try {
            auto pattern=parsePattern(expectedBytes);std::vector<uint8_t> code(pattern.size());
            if(!read(process,address,code.data(),code.size())||scan(code.data(),code.size(),pattern)!=std::vector<size_t>{0})
                throw std::runtime_error("Loaded code signature differs; breakpoint disabled");
        }catch(const std::exception& e){CloseHandle(process);std::cerr<<e.what()<<'\n';return 1;}
    }
    const auto attachStarted = std::chrono::steady_clock::now();
    log.event("probe_attach", "\"pid\":" + std::to_string(pid) + ",\"rva\":" + std::to_string(rva));
    if (!DebugActiveProcess(pid))
    {
        std::cerr << "DebugActiveProcess failed " << GetLastError() << '\n';
        CloseHandle(process);
        return 1;
    }
    if(!DebugSetProcessKillOnExit(FALSE)){DebugActiveProcessStop(pid);CloseHandle(process);std::cerr<<"Unable to disable kill-on-exit\n";return 1;}
    std::map<DWORD, SavedThread> threads;
    uint32_t samples = 0;
    bool initialBreak = true, stopRequested = false, stopBreak = false, done = false, failed = false, exited = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    auto install = [&](DWORD id, HANDLE handle) {
        SavedThread saved{handle, {}};
        saved.original.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(handle, &saved.original))
            throw std::runtime_error("GetThreadContext failed");
        if (saved.original.Dr7 & 3)
            throw std::runtime_error("DR0 already occupied");
        CONTEXT context = saved.original;
        context.Dr0 = address;
        context.Dr7 = (context.Dr7 & ~(0xfull << 16)) | 1;
        threads[id] = saved;
        if (!SetThreadContext(handle, &context))
            throw std::runtime_error("SetThreadContext failed");
    };
    auto restore = [&]() {
        bool ok = true;
        for (auto &item : threads)
        {
            auto &saved = item.second;
            if (WaitForSingleObject(saved.handle, 0) == WAIT_OBJECT_0)
                continue;
            CONTEXT c = saved.original;
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (!SetThreadContext(saved.handle, &c))
                ok = false;
        }
        return ok;
    };
    while (!done)
    {
        if (!stopRequested && (samples >= limit || std::chrono::steady_clock::now() >= deadline || failed))
        {
            // Generate an event to stop all threads before restoring their debug registers.
            stopRequested = true;
            if (!DebugBreakProcess(process))
            {
                failed = true;
                log.event("probe_error", "\"operation\":\"DebugBreakProcess\"");
            }
        }
        DEBUG_EVENT event{};
        if (!WaitForDebugEvent(&event, 100))
        {
            if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
            {
                exited = done = true;
                break;
            }
            continue;
        }
        DWORD status = DBG_CONTINUE;
        try
        {
            if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
            {
                if (event.u.CreateProcessInfo.hFile)
                    CloseHandle(event.u.CreateProcessInfo.hFile);
                CloseHandle(event.u.CreateProcessInfo.hProcess);
                install(event.dwThreadId, event.u.CreateProcessInfo.hThread);
            }
            else if (event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
                install(event.dwThreadId, event.u.CreateThread.hThread);
            else if (event.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT)
            {
                auto it = threads.find(event.dwThreadId);
                if (it != threads.end())
                {
                    CloseHandle(it->second.handle);
                    threads.erase(it);
                }
            }
            else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
            {
                if (event.u.LoadDll.hFile)
                    CloseHandle(event.u.LoadDll.hFile);
            }
            else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            {
                exited = done = true;
            }
            else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
            {
                auto code = event.u.Exception.ExceptionRecord.ExceptionCode;
                if (code == EXCEPTION_SINGLE_STEP)
                {
                    auto it = threads.find(event.dwThreadId);
                    CONTEXT c{};
                    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
                    if (it != threads.end() && GetThreadContext(it->second.handle, &c) && (c.Dr6 & 1) &&
                        c.Rip == address)
                    {
                        if (samples < limit)
                        {
                            std::array<uintptr_t, 10> pointers{};
                            bool valid = read(process, rcxPointers ? c.Rcx : c.Rsp + 0x50, pointers.data(),
                                              sizeof(pointers));
                            std::ostringstream s;
                            s << "\"sample\":" << ++samples << ",\"thread\":" << event.dwThreadId
                              << ",\"query_read\":" << (valid ? "true" : "false") << ",\"rcx\":\"0x"
                              << std::hex << c.Rcx << "\",\"components\":[";
                            for (size_t i = 0; i < pointers.size(); i++)
                            {
                                if (i)
                                    s << ',';
                                std::array<uint8_t, 192> bytes{};
                                bool readable =
                                    valid && read(process, pointers[i], bytes.data(), bytes.size());
                                s << "{\"index\":" << std::dec << i << ",\"address\":\"0x" << std::hex
                                  << pointers[i] << "\",\"readable\":" << (readable ? "true" : "false")
                                  << ",\"bytes\":\"";
                                if (readable)
                                    for (auto b : bytes)
                                    {
                                        const char *h = "0123456789abcdef";
                                        s << h[b >> 4] << h[b & 15];
                                    }
                                s << "\"}";
                            }
                            s << ']';
                            log.event("camera_query_sample", s.str());
                        }
                        c.Dr6 &= ~1ull;
                        c.EFlags |= 0x10000; // RF resumes the trapped instruction exactly once.
                        if (!SetThreadContext(it->second.handle, &c))
                            throw std::runtime_error("resume context failed");
                    }
                    else
                        status = DBG_EXCEPTION_NOT_HANDLED;
                }
                else if (code == EXCEPTION_BREAKPOINT && initialBreak)
                {
                    initialBreak = false;
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - attachStarted).count();
                    log.event("probe_ready", "\"threads\":" + std::to_string(threads.size()) +
                              ",\"attach_ms\":" + std::to_string(elapsed));
                }
                else if (code == EXCEPTION_BREAKPOINT && stopRequested)
                    stopBreak = true;
                else
                    status = DBG_EXCEPTION_NOT_HANDLED;
            }
            // Wait for the requested breakpoint itself. Detaching at its preceding
            // CREATE_THREAD event would leave an unhandled breakpoint in the target.
            if (stopRequested && stopBreak && !exited)
            {
                if (restore())
                    done = true;
                else
                {
                    failed = true;
                    log.event("restore_retry");
                }
            }
        }
        catch (const std::exception &e)
        {
            failed = true;
            log.event("probe_error", "\"reason\":" + jsonString(e.what()));
        }
        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, status);
    }
    bool detached = exited || DebugActiveProcessStop(pid);
    for (auto &t : threads)
        CloseHandle(t.second.handle);
    CloseHandle(process);
    log.event("probe_complete", "\"samples\":" + std::to_string(samples) +
                                    ",\"observation\":" + jsonString(samples ? "captured" : "no_hit") +
                                    ",\"detached\":" + (detached ? "true" : "false") +
                                    ",\"target_exited\":" + (exited ? "true" : "false"));
    std::cout << (failed || !detached ? "FAIL" : "PASS") << " probe cleanup; " << samples << " samples\n";
    return failed || !detached ? 1 : 0;
}
