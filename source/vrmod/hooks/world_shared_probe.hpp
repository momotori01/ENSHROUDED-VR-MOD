// Read-only observation of the three Scene+9d8 CPU arrays implicated by the
// 20260913-033503 shutdown dump. No allocation/free arguments or data are changed.
namespace worldSharedDiagnostic {
using Free = void* (__fastcall*)(void*,void*,void*,uint8_t*);
inline Free originalFree=nullptr;
inline bool enabled=false;
inline HANDLE file=INVALID_HANDLE_VALUE;
inline std::array<std::atomic<uintptr_t>,3> watched{};
inline std::atomic<unsigned> calls{0},callbacks{0};
inline uintptr_t object=0;
inline void* __fastcall freeMemory(void* allocator,void* output,void* pointer,uint8_t* tag){
    callbacks.fetch_add(1);
    const auto incoming=GetLastError();
    bool matched=false;
    for(const auto& value:watched)if(value.load()==reinterpret_cast<uintptr_t>(pointer)&&pointer)matched=true;
    if(matched&&calls.fetch_add(1)<64&&file!=INVALID_HANDLE_VALUE){
        // Write a bounded record before native free can assert. No C++ allocations,
        // mutexes or buffered logger calls in the allocator observer.
        void* stack[16]{};const auto count=RtlCaptureStackBackTrace(1,16,stack,nullptr);
        uint64_t header=0;SIZE_T bytes=0;
        ReadProcessMemory(GetCurrentProcess(),static_cast<uint8_t*>(pointer)-8,&header,8,&bytes);
        char line[1536]{};
        int at=snprintf(line,sizeof(line),"{\"event\":\"watched_native_free\",\"thread\":%lu,\"allocator\":\"0x%llx\",\"pointer\":\"0x%llx\",\"header\":%llu,\"header_read\":%s,\"stack\":[",
            GetCurrentThreadId(),static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(allocator)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pointer)),static_cast<unsigned long long>(header),bytes==8?"true":"false");
        for(unsigned i=0;i<count&&at>0&&at<int(sizeof(line))-40;++i){
            const auto address=reinterpret_cast<uintptr_t>(stack[i]);
            at+=snprintf(line+at,sizeof(line)-at,"%s\"0x%llx\"",i?",":"",static_cast<unsigned long long>(address));
        }
        if(at>0&&at<int(sizeof(line))-4){line[at++]=']';line[at++]='}';line[at++]='\n';DWORD written=0;WriteFile(file,line,at,&written,nullptr);}
    }
    SetLastError(incoming);
    void* result=originalFree(allocator,output,pointer,tag);
    const auto outgoing=GetLastError();callbacks.fetch_sub(1);SetLastError(outgoing);return result;
}
inline void snapshot(const char* phase,uintptr_t scene,bool initialize=false){
    if(!enabled)return;
    const auto current=read<uintptr_t>(scene+0xa88);
    if(initialize)object=current;
    if(!current||current!=object)throw std::runtime_error("shared Scene object changed during world diagnostic");
    const std::array<size_t,3> offsets{0xb0,0x1c8,0x238};
    for(size_t i=0;i<offsets.size();++i){
        const auto pointer=read<uintptr_t>(object+offsets[i]);
        if(initialize)watched[i]=pointer;
        uint64_t header=0;SIZE_T bytes=0;
        if(pointer)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pointer-8),&header,8,&bytes);
        logFile->event("world_shared_array_snapshot","\"phase\":"+jsonString(phase)+",\"object\":"+hex(object)+
            ",\"array\":"+std::to_string(i)+",\"pointer\":"+hex(pointer)+",\"watched\":"+hex(watched[i].load())+
            ",\"capacity\":"+std::to_string(read<uint64_t>(object+offsets[i]+8))+
            ",\"header\":"+std::to_string(header)+",\"header_read\":"+(bytes==8?"true":"false"));
    }
}
inline void install(const std::filesystem::path& directory){
    const auto pattern=parsePattern("48 89 5C 24 10 4C 89 44 24 18 56 57 41 56 48 83 EC 40");
    auto* target=reinterpret_cast<void*>(baseAddress+0x8b3db0);
    if(scan(static_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0})
        throw std::runtime_error("shared array free observer signature mismatch");
    file=CreateFileW((directory/"world-shared-free.jsonl").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("shared array free trace unavailable");
    if(MH_CreateHook(target,reinterpret_cast<void*>(&freeMemory),reinterpret_cast<void**>(&originalFree))!=MH_OK||MH_EnableHook(target)!=MH_OK)
        throw std::runtime_error("shared array free observer hook failed");
    enabled=true;logFile->event("world_shared_free_hook_enabled","\"rva\":\"0x8b3db0\",\"read_only\":true");
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0x8b3db0));
    if(status==MH_OK)enabled=false;
    // Keep the file/trampoline valid until process exit for any pre-disable caller.
    try{logFile->event("world_shared_free_hook_removed","\"status\":"+std::to_string(status)+",\"callbacks\":"+std::to_string(callbacks.load()));}catch(...){}
}
}
