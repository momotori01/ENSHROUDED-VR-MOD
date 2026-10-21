// Bounded game-owned target allocation probe. Runs on the existing render thread.
// Creates two UNUSED offscreen targets, checks their identities, then releases
// imports non-owning aliases, then releases both kinds through the engine.
// Does not repeat drawing or modify camera/player data.
#include <MinHook.h>
#include <evr/log.hpp>
#include <evr/game_build.hpp>
#include <evr/observer_error.hpp>
#include <evr/signature.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <windows.h>
using namespace evr;
namespace
{
using Render = void *(__fastcall *)(void *, void *, void *, void *, void *);
using Create = void *(__fastcall *)(void *, void *, const void *);
using Release = void (__fastcall *)(void *, void *);
using Import = void *(__fastcall *)(void *, void *, void *, uint8_t, const uint64_t *);
Render original = nullptr;
uintptr_t gameBase = 0;
std::atomic<bool> started{false}, active{false}, claimed{false}, done{false};
std::atomic<unsigned> resultCode{4};
Log *retainedLog = nullptr;
constexpr uintptr_t renderRva = 0x129fc0;
constexpr const char *patternText = "4C 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 48 8D AC";
using Handle = std::array<uint8_t, 16>;
template<class T> T read(uintptr_t address)
{
    T value{}; SIZE_T count=0;
    if (!address || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(address), &value, sizeof(value), &count) || count!=sizeof(value))
        throw std::runtime_error("unreadable target metadata");
    return value;
}
template<class T, size_t N> T field(const std::array<uint8_t,N> &bytes, size_t offset)
{
    if (offset+sizeof(T)>N) throw std::runtime_error("field outside snapshot");
    T value; std::memcpy(&value, bytes.data()+offset, sizeof(value)); return value;
}
std::string hex(uintptr_t value) { std::ostringstream out;out<<"\"0x"<<std::hex<<value<<'"';return out.str(); }
void probe(void *target) noexcept
{
    const ObserverLastError preserve;
    const auto create = reinterpret_cast<Create>(gameBase+0x100f900);
    const auto release = reinterpret_cast<Release>(gameBase+0x10119e0);
    const auto import = reinterpret_cast<Import>(gameBase+0x100fbb0);
    std::array<Handle,2> handles{};
    std::array<Handle,2> aliases{};
    uintptr_t graph=0;
    try
    {
        const auto source=read<std::array<uint8_t,24>>(reinterpret_cast<uintptr_t>(target));
        graph=field<uintptr_t>(source,0);
        const auto index=field<uint16_t>(source,8);
        const auto table=read<uintptr_t>(graph+0x80);
        const auto capacity=read<uint64_t>(graph+0x88);
        if (!graph || !table || index>=capacity || capacity>65535) throw std::runtime_error("invalid graph texture index");
        const auto descriptor=read<std::array<uint8_t,48>>(table+size_t(index)*48);
        const auto width=field<uint32_t>(descriptor,4), height=field<uint32_t>(descriptor,8);
        if (!width || !height || width>8192 || height>8192 || descriptor[0x15]!=37)
            throw std::runtime_error("probe requires bounded RGBA8 menu target");
        const auto freeBefore=read<uint64_t>(graph+0xe0);
        if(freeBefore<6) throw std::runtime_error("insufficient free graph handles");
        std::array<uintptr_t,2> images{}, views{};
        std::array<uintptr_t,2> textures{};
        std::array<std::array<uint8_t,80>,2> snapshots{};
        std::array<uint16_t,2> indices{};
        for(size_t eye=0;eye<2;++eye)
        {
            create(handles[eye].data(), reinterpret_cast<void *>(graph), descriptor.data());
            if(field<uintptr_t>(handles[eye],0)!=graph) throw std::runtime_error("game texture creation failed");
            indices[eye]=field<uint16_t>(handles[eye],8);
            if(indices[eye]>=capacity || indices[eye]==index) throw std::runtime_error("unexpected allocated target index");
            const auto textureTable=read<uintptr_t>(graph+0x68);
            const auto texture=read<uintptr_t>(textureTable+size_t(indices[eye])*8);
            const auto snapshot=read<std::array<uint8_t,80>>(texture);
            textures[eye]=texture; snapshots[eye]=snapshot;
            images[eye]=field<uintptr_t>(snapshot,0x30); views[eye]=field<uintptr_t>(snapshot,0x38);
            if(!images[eye] || !views[eye] || field<uint32_t>(snapshot,0x10)!=width || field<uint32_t>(snapshot,0x14)!=height)
                throw std::runtime_error("created Vulkan target metadata mismatch");
            retainedLog->event("offscreen_target_created", "\"slot\":"+std::to_string(eye)+
                ",\"graph\":"+hex(graph)+",\"index\":"+std::to_string(indices[eye])+
                ",\"texture\":"+hex(texture)+",\"image\":"+hex(images[eye])+",\"view\":"+hex(views[eye])+
                ",\"width\":"+std::to_string(width)+",\"height\":"+std::to_string(height)+",\"rendered\":false");
        }
        if(indices[0]==indices[1] || images[0]==images[1] || views[0]==views[1]) throw std::runtime_error("offscreen targets alias");
        const uint64_t synchronization = 0;
        for(size_t eye=0;eye<2;++eye)
        {
            import(aliases[eye].data(), reinterpret_cast<void *>(graph), reinterpret_cast<void *>(textures[eye]), 0, &synchronization);
            const auto flags=field<uint32_t>(aliases[eye],8);
            const auto aliasIndex=field<uint16_t>(aliases[eye],8);
            if(field<uintptr_t>(aliases[eye],0)!=graph || !(flags&0x04000000) || (flags&0x02000000) ||
               aliasIndex>=capacity || aliasIndex==indices[0] || aliasIndex==indices[1])
                throw std::runtime_error("external import ownership/index mismatch");
            const auto textureTable=read<uintptr_t>(graph+0x68);
            if(read<uintptr_t>(textureTable+size_t(aliasIndex)*8)!=textures[eye])
                throw std::runtime_error("import does not refer to original texture");
            retainedLog->event("offscreen_external_import", "\"slot\":"+std::to_string(eye)+
                ",\"index\":"+std::to_string(aliasIndex)+",\"flags\":"+std::to_string(flags)+
                ",\"texture\":"+hex(textures[eye])+",\"rendered\":false");
        }
        if(field<uint16_t>(aliases[0],8)==field<uint16_t>(aliases[1],8))throw std::runtime_error("import handles alias");
        for(auto &alias:aliases)release(reinterpret_cast<void *>(graph),alias.data());
        for(size_t eye=0;eye<2;++eye)
        {
            const auto textureTable=read<uintptr_t>(graph+0x68);
            if(field<uintptr_t>(aliases[eye],0) || read<uintptr_t>(textureTable+size_t(indices[eye])*8)!=textures[eye] ||
               read<std::array<uint8_t,80>>(textures[eye])!=snapshots[eye])
                throw std::runtime_error("external alias release altered owned texture");
        }
        retainedLog->event("offscreen_external_release_complete", "\"owned_textures_unchanged\":true,\"aliases_cleared\":true");
        for(auto &handle:handles) release(reinterpret_cast<void *>(graph), handle.data());
        if(field<uintptr_t>(handles[0],0) || field<uintptr_t>(handles[1],0)) throw std::runtime_error("game handles not cleared on release");
        const auto current=read<std::array<uint8_t,48>>(table+size_t(index)*48);
        if(current!=descriptor) throw std::runtime_error("source target metadata changed");
        retainedLog->event("offscreen_target_probe_complete", "\"distinct_images\":true,\"handles_released\":true,\"source_unchanged\":true,\"free_before\":"+
            std::to_string(freeBefore)+",\"free_after\":"+std::to_string(read<uint64_t>(graph+0xe0)));
        resultCode=0;
    }
    catch(const std::exception &error)
    {
        if(graph) for(auto &alias:aliases) if(field<uintptr_t>(alias,0)==graph) release(reinterpret_cast<void *>(graph),alias.data());
        if(graph) for(auto &handle:handles) if(field<uintptr_t>(handle,0)==graph) release(reinterpret_cast<void *>(graph),handle.data());
        try { retainedLog->event("offscreen_target_probe_failure", "\"reason\":"+jsonString(error.what())); } catch(...) {}
        resultCode=1;
    }
    done.store(true,std::memory_order_release);
}
void * __fastcall render(void *output,void *context,void *view,void *parameters,void *target)
{
    if(active.load(std::memory_order_acquire) && !claimed.exchange(true)) probe(target);
    return original(output,context,view,parameters,target);
}
void require(MH_STATUS status,const char *operation)
{ if(status!=MH_OK)throw std::runtime_error(std::string(operation)+": "+MH_StatusToString(status)); }
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeRenderResources(void *directory)
{
    if(!directory || started.exchange(true)) return 2;
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&EVR_ProbeRenderResources),&pinned))return 3;
    void *target=nullptr;bool enabled=false;
    try
    {
        retainedLog=new Log((std::filesystem::path(static_cast<wchar_t *>(directory))/"render-resource-probe.jsonl").string(),true);
        auto *base=reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));gameBase=reinterpret_cast<uintptr_t>(base);
        const auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE)throw std::runtime_error("PE DOS signature");
        const auto *nt=reinterpret_cast<IMAGE_NT_HEADERS64 *>(base+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp || nt->OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)
            throw std::runtime_error("unsupported loaded executable build");
        const auto pattern=parsePattern(patternText);std::vector<uintptr_t> hits;
        const auto *section=IMAGE_FIRST_SECTION(nt);
        for(uint32_t i=0;i<nt->FileHeader.NumberOfSections;++i)
        {
            if(!(section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
            const size_t rva=section[i].VirtualAddress,size=section[i].Misc.VirtualSize;
            if(rva+size>nt->OptionalHeader.SizeOfImage)throw std::runtime_error("invalid section range");
            for(auto offset:scan(base+rva,size,pattern))hits.push_back(rva+offset);
        }
        if(hits!=std::vector<uintptr_t>{renderRva})throw std::runtime_error("render signature missing/ambiguous");
        const auto createPattern=parsePattern("40 53 55 56 41 56 48 83 EC 48 4D 8B F0 48");
        const auto releasePattern=parsePattern("48 8B C4 55 57 48 81 EC 88 00");
        const auto importPattern=parsePattern("40 55 56 57 41 56 41 57 48 8D 6C 24 D1 48 81 EC");
        if(scan(base+0x1012ce0,createPattern.size(),createPattern)!=std::vector<size_t>{0} ||
           scan(base+0x1012f90,importPattern.size(),importPattern)!=std::vector<size_t>{0} ||
           scan(base+0x1014dc0,releasePattern.size(),releasePattern)!=std::vector<size_t>{0})
            throw std::runtime_error("texture lifecycle signatures changed");
        target=base+renderRva;
        require(MH_Initialize(),"MH_Initialize");
        require(MH_CreateHook(target,reinterpret_cast<void *>(&render),reinterpret_cast<void **>(&original)),"MH_CreateHook");
        active=true;require(MH_EnableHook(target),"MH_EnableHook");enabled=true;
        retainedLog->event("render_resource_hook_enabled","\"mode\":\"allocate_unused_targets_only\",\"rva\":\"0x129fc0\"");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(2));
        active=false;require(MH_DisableHook(target),"MH_DisableHook");enabled=false;
        if(scan(static_cast<uint8_t *>(target),pattern.size(),pattern)!=std::vector<size_t>{0})throw std::runtime_error("original instruction restoration mismatch");
        retainedLog->event("render_resource_hook_complete","\"original_bytes_restored\":true,\"trampoline_retained\":true,\"result\":"+std::to_string(resultCode.load()));
        return resultCode.load();
    }
    catch(const std::exception &error)
    {
        active=false;const auto cleanup=enabled?MH_DisableHook(target):MH_OK;
        if(retainedLog)try{retainedLog->event("render_resource_hook_failure","\"reason\":"+jsonString(error.what())+",\"disable_status\":"+jsonString(MH_StatusToString(cleanup)));}catch(...){}
        return 1;
    }
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID)
{if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);return TRUE;}
