#pragma once
// Diagnostic (2026-10-19 night, EVR_DATA_WATCH=<view offset>[,<view offset>] [EVR_DATA_WATCH_AT=<XR frame>]): which
// instructions write a given place of the eye Views. The headset's View dumps (views-diff, game-20261019-010922) showed
// 138 places that hold values in the left eye's View and 0 in the right eye's on the frames where their LODs differed;
// reading the code did not tell which system fills them. From the given frame, the CPU's debug registers watch each
// offset in both eye Views (up to 2 offsets: DR0..DR3, 8 bytes, writes) on every thread of the game; a vectored
// handler records each hit (instruction after the write, thread, which View) and lets it go on. After 10 s the
// registers are cleared and the writers are logged once each as module+rva (data_watch_writer).
// Included inside the probe's anonymous namespace (view_resource_probe.cpp).
namespace dataWatch {
struct Hit {uintptr_t rip=0;uint32_t thread=0;uint8_t slot=0;};
inline std::array<uintptr_t,4> addresses{};inline std::array<int,4> eyes{},offsets{};inline unsigned count=0;
inline std::array<Hit,4096> hits{};inline std::atomic<uint32_t> hitCount{0};
inline std::atomic<bool> armed{false},done{false};inline uint64_t armedMs=0;inline PVOID handler=nullptr;
inline const std::vector<uint32_t> wanted=[]{std::vector<uint32_t> out;char v[64]{};const auto n=GetEnvironmentVariableA("EVR_DATA_WATCH",v,sizeof v);
    if(!n||n>=sizeof v)return out;char* p=v;while(*p&&out.size()<2){char* end=nullptr;const auto x=std::strtoul(p,&end,16);if(end==p)break;out.push_back(uint32_t(x&~7ul));p=*end==','?end+1:end;}return out;}();
inline const uint64_t atFrame=[]{char v[24]{};return GetEnvironmentVariableA("EVR_DATA_WATCH_AT",v,sizeof v)?std::strtoull(v,nullptr,10):600ull;}();
inline LONG CALLBACK onException(PEXCEPTION_POINTERS e){
    if(e->ExceptionRecord->ExceptionCode!=EXCEPTION_SINGLE_STEP)return EXCEPTION_CONTINUE_SEARCH;
    auto* c=e->ContextRecord;const auto status=c->Dr6&0xf;if(!status)return EXCEPTION_CONTINUE_SEARCH;
    for(uint8_t k=0;k<4;k++)if(status&(1u<<k)){const auto n=hitCount.fetch_add(1);if(n<hits.size())hits[n]={uintptr_t(c->Rip),GetCurrentThreadId(),k};}
    c->Dr6=0;return EXCEPTION_CONTINUE_EXECUTION;
}
// Sets (or clears) the registers of every thread of the process but the caller's own (a helper thread does the work,
// so the render thread is covered too).
inline void applyAll(bool set){
    std::thread([set]{
        const DWORD self=GetCurrentThreadId(),pid=GetCurrentProcessId();
        const HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return;
        THREADENTRY32 t{};t.dwSize=sizeof t;unsigned applied=0,failed=0;
        for(BOOL ok=Thread32First(snap,&t);ok;ok=Thread32Next(snap,&t)){if(t.th32OwnerProcessID!=pid||t.th32ThreadID==self)continue;
            const HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,t.th32ThreadID);if(!h){++failed;continue;}
            if(SuspendThread(h)!=DWORD(-1)){CONTEXT c{};c.ContextFlags=CONTEXT_DEBUG_REGISTERS;
                if(GetThreadContext(h,&c)){
                    DWORD64 dr7=c.Dr7&~DWORD64(0xffff00ff);DWORD64* dr[4]={&c.Dr0,&c.Dr1,&c.Dr2,&c.Dr3};
                    for(unsigned k=0;k<4;k++){*dr[k]=0;if(set&&k<count){*dr[k]=addresses[k];dr7|=DWORD64(1)<<(k*2);dr7|=DWORD64(0x1|(0x3<<2))<<(16+k*4);}}   // local enable, R/W=01 (write), LEN=11 (8 bytes)
                    c.Dr7=dr7;c.Dr6=0;if(SetThreadContext(h,&c))++applied;else ++failed;}
                ResumeThread(h);}
            CloseHandle(h);}
        CloseHandle(snap);
        try{logFile->event(set?"data_watch_armed":"data_watch_cleared","\"threads\":"+std::to_string(applied)+",\"failed\":"+std::to_string(failed));}catch(...){}
    }).join();
}
// Called at each eye's begin with the pair's Views; arms once at the frame asked for, ends 10 s later.
inline void tick(uintptr_t left,uintptr_t right,uint64_t frame){
    if(wanted.empty()||done.load())return;
    if(!armed.load()){
        if(frame<atFrame||!left||!right)return;
        count=0;for(const auto off:wanted)for(int eye=0;eye<2;eye++){if(count>=4)break;addresses[count]=(eye?right:left)+off;eyes[count]=eye;offsets[count]=int(off);++count;}
        handler=AddVectoredExceptionHandler(1,&onException);if(!handler)return;
        armedMs=GetTickCount64();armed=true;applyAll(true);
        try{std::string a;for(unsigned k=0;k<count;k++)a+=std::string(k?",":"")+"{\"eye\":"+std::to_string(eyes[k])+",\"offset\":"+hex(offsets[k])+",\"address\":"+hex(addresses[k])+"}";
            logFile->event("data_watch_targets","\"frame\":"+std::to_string(frame)+",\"watches\":["+a+"]");}catch(...){}
        return;}
    if(GetTickCount64()-armedMs<10000)return;
    done=true;applyAll(false);
    const auto n=std::min<uint32_t>(hitCount.load(),uint32_t(hits.size()));
    std::map<std::pair<uintptr_t,uint8_t>,std::pair<unsigned,uint32_t>> seen;
    for(uint32_t i=0;i<n;i++){auto& s=seen[{hits[i].rip,hits[i].slot}];++s.first;s.second=hits[i].thread;}
    for(const auto& [key,value]:seen){
        HMODULE m=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(key.first),&m);
        try{logFile->event("data_watch_writer","\"rip_rva\":"+hex(m==reinterpret_cast<HMODULE>(baseAddress)?key.first-baseAddress:key.first)+",\"in_game\":"+(m==reinterpret_cast<HMODULE>(baseAddress)?"true":"false")+
            ",\"eye\":"+std::to_string(eyes[key.second])+",\"offset\":"+hex(offsets[key.second])+",\"hits\":"+std::to_string(value.first)+",\"thread\":"+std::to_string(value.second));}catch(...){}}
    try{logFile->event("data_watch_done","\"hits\":"+std::to_string(hitCount.load())+",\"writers\":"+std::to_string(seen.size()));}catch(...){}
    if(handler){RemoveVectoredExceptionHandler(handler);handler=nullptr;}
}
}
