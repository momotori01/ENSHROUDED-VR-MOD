#pragma once
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <evr/observer_error.hpp>
#include <evr/game_build.hpp>
#include <evr/signature.hpp>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

namespace evr::diagnostic::cpuArena {
// Opt-in, process-lifetime backing for the existing native Game_Render TLSF.
// The application's original region partitioning and every allocate/free ABI
// remain native. The borrowed backing must outlive ALL renderer allocations.
using Emit=void(*)(const char*,const std::string&) noexcept;
struct Span {void* data;uint64_t size;};
struct Name {const char* data;uint64_t size;};
// 1.0 (2026-10-16): the flags come by value (a byte in r8; Game_Render passes 0x80), before by a pointer to a byte.
using Initialize=void*(__fastcall*)(void*,const Span*,uint8_t,const Name*);
inline Initialize original=nullptr;
inline Emit emit=nullptr;
inline uintptr_t base=0;
inline uint64_t requestedBytes=0;
inline std::atomic<unsigned> callbacks{0};
inline std::atomic<bool> consumed{false},enabled{false};
inline void* backing=nullptr;
inline std::once_flag armOnce;
inline void record(const char* event,const std::string& fields) noexcept {if(emit)emit(event,fields);}
inline bool matches(uintptr_t caller,const Span* region,uint8_t,const Name* name) noexcept {
    // the call that makes Game_Render (0x838093 in 1.0: the one after Game_ClientPermanent and Game_ClientSwitch)
    return caller==base+0x839908&&region&&region->data&&region->size==450ull*1024*1024&&
        name&&name->data&&name->size==11&&!std::memcmp(name->data,"Game_Render",11);
}
inline void* __fastcall initialize(void* object,const Span* region,uint8_t flags,const Name* name){
    struct Count {Count(){++callbacks;}~Count(){--callbacks;}} count;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!matches(caller,region,flags,name)||consumed.exchange(true))return original(object,region,flags,name);
    Span replacement{};
    {const ObserverLastError preserve;
        MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);
        if(GlobalMemoryStatusEx(&memory)&&memory.ullAvailPageFile>=requestedBytes+512ull*1024*1024)
            replacement.data=VirtualAlloc(nullptr,SIZE_T(requestedBytes),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(replacement.data)replacement.size=requestedBytes;
    }
    if(!replacement.data){
        {const ObserverLastError preserve;try{record("render_cpu_arena_fallback","\"reason\":\"backing_allocation_unavailable\",\"native_bytes\":"+std::to_string(region->size));}catch(...){}}
        return original(object,region,flags,name);
    }
    // No release during Vulkan/Scene teardown: native objects can still exist.
    // Windows reclaims this one bounded mapping at process exit. Pin the layer.
    backing=replacement.data;
    void* result=original(object,&replacement,flags,name);
    {const ObserverLastError preserve;
        const auto* bytes=static_cast<const uint8_t*>(object);
        uintptr_t nativeBase=0;uint64_t nativeSize=0;
        std::memcpy(&nativeBase,bytes+0x78,8);std::memcpy(&nativeSize,bytes+0x80,8);
        try{record("render_cpu_arena_initialized","\"original_bytes\":"+std::to_string(region->size)+
            ",\"requested_bytes\":"+std::to_string(requestedBytes)+",\"native_bytes\":"+std::to_string(nativeSize)+
            ",\"native_backing_matches\":"+(nativeBase==reinterpret_cast<uintptr_t>(backing)&&nativeSize==requestedBytes?"true":"false")+
            ",\"original_partition_preserved\":true,\"lifetime\":\"game_process\",\"native_allocator_preserved\":true");}catch(...){}
    }
    return result;
}
inline void arm(Emit logger) noexcept {
    const ObserverLastError preserve;
    try{std::call_once(armOnce,[&]{
        const char* value=std::getenv("EVR_RENDER_CPU_ARENA_MIB");
        if(!value||!std::strcmp(value,"0"))return;
        emit=logger;char* end=nullptr;const auto mib=std::strtoul(value,&end,10);
        if(!*value||!end||*end||mib<512||mib>2048)throw std::runtime_error("invalid render CPU arena size (512..2048 MiB)");
        wchar_t path[32768]{};
        if(!GetModuleFileNameW(nullptr,path,DWORD(std::size(path)))||
            _wcsicmp(std::filesystem::path(path).filename().c_str(),L"enshrouded.exe"))
            throw std::runtime_error("CPU arena requires the supported game executable");
        base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if(nt->FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt->OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)
            throw std::runtime_error("CPU arena unsupported loaded game build");
        const auto pattern=parsePattern("48 83 EC 38 0F 10 02 49 8B C1 48 8D 54 24 20 45");
        auto* target=reinterpret_cast<void*>(base+0x8b3e90);
        if(scan(static_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0})
            throw std::runtime_error("CPU arena constructor signature mismatch");
        const auto* call=reinterpret_cast<const uint8_t*>(base+0x839903);int32_t delta=0;std::memcpy(&delta,call+1,4);
        if(*call!=0xe8||reinterpret_cast<uintptr_t>(call+5)+delta!=base+0x8b3e90)
            throw std::runtime_error("CPU arena exact constructor callsite mismatch");
        HMODULE pinned=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&initialize),&pinned))throw std::runtime_error("CPU arena layer pin failed");
        const auto status=MH_Initialize();
        if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error("CPU arena hook initialization failed");
        requestedBytes=uint64_t(mib)*1024*1024;
        if(MH_CreateHook(target,reinterpret_cast<void*>(&initialize),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(target)!=MH_OK)
            throw std::runtime_error("CPU arena constructor hook failed");
        enabled=true;
        record("render_cpu_arena_armed","\"mib\":"+std::to_string(mib)+",\"disk_bytes_changed\":false,\"scope\":\"game_process_only\"");
    });}catch(const std::exception& error){try{record("render_cpu_arena_disabled","\"reason\":"+jsonString(error.what()));}catch(...){}}
}
inline void retireHook() noexcept {
    if(!enabled||!consumed||callbacks.load()!=0)return;
    const ObserverLastError preserve;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(base+0x8b3e90));
    if(status==MH_OK)enabled=false;
    try{record("render_cpu_arena_hook_removed","\"status\":"+std::to_string(status)+",\"backing_retained_until_process_exit\":true");}catch(...){}
}
} // namespace evr::diagnostic::cpuArena
