#pragma once
// Included inside the probe's anonymous namespace (view_resource_probe.cpp): the standard headers come from there.
// Diagnostic (2026-10-19 night, EVR_THREAD_STACKS=1): where the game's server thread and the render thread spend their
// time. A thread of the mod's own suspends one of them about every millisecond, copies its registers and the live part
// of its stack, lets it run again and only then walks the copy (RtlVirtualUnwind on relocated registers), so nothing is
// looked up or allocated while the game's thread is held. Every 5 s per thread: the share of samples with any frame in a
// mod module ("mod_any_pct"), the functions the time is spent in ("self") and below ("incl"), as module+rva - the game's
// RVAs are matched with the hooked functions, the mod's with its PDB offline.
// Why: the server thread uses 76-92 % of a core while the headset plays online (thread_cpu, 2026-10-19 01:04), and the
// game logs "[server] Bad Performance" every 5 s; with a joined friend the hits and guards came late. Is the mod on that
// thread (its hooks on the simulation), or is the game's own work this heavy?
namespace threadStacks {
inline const bool wanted=[]{char v[8]{};return GetEnvironmentVariableA("EVR_THREAD_STACKS",v,sizeof v)==1&&v[0]=='1';}();
inline std::atomic<DWORD> renderThread{0};
constexpr int maxFrames=64;constexpr size_t maxStack=192*1024;
struct Walk{DWORD64 rip[maxFrames];int n=0;};
// Unwinds the copied stack. POD only: the walk runs under SEH, a bad saved value ends the walk instead of the game.
inline int unwindCopy(CONTEXT* c,uintptr_t realLo,uintptr_t realHi,uintptr_t copy,DWORD64* out){
    int n=0;const DWORD64 delta=DWORD64(copy)-DWORD64(realLo);const DWORD64 copyLo=copy,copyHi=copy+(realHi-realLo);
    __try{
        DWORD64* regs[]={&c->Rbp,&c->Rbx,&c->Rsi,&c->Rdi,&c->R12,&c->R13,&c->R14,&c->R15};
        c->Rsp+=delta;
        for(int k=0;k<8;k++)if(*regs[k]>=realLo&&*regs[k]<realHi)*regs[k]+=delta;
        while(n<maxFrames){
            const DWORD64 rip=c->Rip;if(!rip)break;out[n++]=rip;
            DWORD64 imageBase=0;const auto* fe=RtlLookupFunctionEntry(rip,&imageBase,nullptr);
            if(!fe){if(c->Rsp<copyLo||c->Rsp+8>copyHi)break;c->Rip=*reinterpret_cast<DWORD64*>(c->Rsp);c->Rsp+=8;}
            else{PVOID handler=nullptr;DWORD64 frame=0;RtlVirtualUnwind(UNW_FLAG_NHANDLER,imageBase,rip,const_cast<PRUNTIME_FUNCTION>(fe),c,&handler,&frame,nullptr);}
            for(int k=0;k<8;k++)if(*regs[k]>=realLo&&*regs[k]<realHi)*regs[k]+=delta;
            if(c->Rsp<copyLo||c->Rsp>copyHi)break;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
    return n;
}
struct Module{uintptr_t base=0,end=0;std::string name;bool mod=false;};
inline std::vector<Module> modules;
inline const Module* moduleOf(uintptr_t a){
    for(const auto& m:modules)if(a>=m.base&&a<m.end)return &m;
    HMODULE h=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(a),&h)||!h)return nullptr;
    Module m;m.base=reinterpret_cast<uintptr_t>(h);
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(h);const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(m.base+dos->e_lfanew);m.end=m.base+nt->OptionalHeader.SizeOfImage;
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(h,path,MAX_PATH);std::wstring w(path);const auto slash=w.find_last_of(L"\\/");if(slash!=std::wstring::npos)w=w.substr(slash+1);
    for(auto& ch:w)if(ch>=L'A'&&ch<=L'Z')ch=wchar_t(ch+32);
    m.name.assign(w.begin(),w.end());m.mod=m.name.rfind("evr",0)==0||m.name=="dinput8.dll";
    modules.push_back(m);return &modules.back();
}
inline uintptr_t functionKey(uintptr_t rip){DWORD64 base=0;const auto* fe=RtlLookupFunctionEntry(rip,&base,nullptr);return fe?uintptr_t(base+fe->BeginAddress):rip;}
inline std::string label(uintptr_t a){const auto* m=moduleOf(a);if(!m)return hex(a);char b[160];std::snprintf(b,sizeof b,"%s+0x%llx",m->name.c_str(),static_cast<unsigned long long>(a-m->base));return jsonString(b);}
struct Stats{uint64_t samples=0,failed=0,modAny=0,modLeaf=0,depth=0;std::unordered_map<uintptr_t,uint32_t> self,incl,modIncl;};
inline std::string top(const std::unordered_map<uintptr_t,uint32_t>& m,size_t count,uint64_t samples){
    std::vector<std::pair<uintptr_t,uint32_t>> v(m.begin(),m.end());std::sort(v.begin(),v.end(),[](auto& a,auto& b){return a.second>b.second;});
    std::string s;char b[32];
    for(size_t i=0;i<v.size()&&i<count;i++){std::snprintf(b,sizeof b,"%.1f",samples?100.0*v[i].second/double(samples):0.0);s+=std::string(i?",":"")+"["+label(v[i].first)+","+b+"]";}
    return s;
}
inline DWORD findServer(){
    static const auto getDescription=reinterpret_cast<HRESULT(WINAPI*)(HANDLE,PWSTR*)>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetThreadDescription"));
    if(!getDescription)return 0;const HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return 0;
    THREADENTRY32 e{};e.dwSize=sizeof e;DWORD found=0;const DWORD pid=GetCurrentProcessId();
    for(BOOL ok=Thread32First(snap,&e);ok&&!found;ok=Thread32Next(snap,&e)){if(e.th32OwnerProcessID!=pid)continue;
        const HANDLE h=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,e.th32ThreadID);if(!h)continue;
        PWSTR d=nullptr;if(SUCCEEDED(getDescription(h,&d))&&d){if(wcsstr(d,L"GameServer"))found=e.th32ThreadID;LocalFree(d);}CloseHandle(h);}
    CloseHandle(snap);return found;
}
inline void run(){
    std::vector<uint8_t> copy(maxStack);Walk walk;
    struct Target{const char* name;DWORD id=0;HANDLE h=nullptr;uintptr_t stackTop=0;Stats s;};
    Target targets[2]{{"GameServer"},{"render"}};
    uint64_t lastLog=GetTickCount64(),lastFind=0;unsigned turn=0;
    for(;;){
        Sleep(1);const auto now=GetTickCount64();
        if(now-lastFind>=5000){lastFind=now;
            const DWORD ids[2]={findServer(),renderThread.load()};
            for(int k=0;k<2;k++)if(ids[k]!=targets[k].id){if(targets[k].h)CloseHandle(targets[k].h);targets[k].h=nullptr;targets[k].id=ids[k];targets[k].stackTop=0;
                if(ids[k]&&ids[k]!=GetCurrentThreadId())targets[k].h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,ids[k]);}
        }
        auto& t=targets[turn++&1];
        if(t.h){
            CONTEXT c{};c.ContextFlags=CONTEXT_FULL;size_t len=0;bool ok=false;
            if(SuspendThread(t.h)!=DWORD(-1)){
                if(GetThreadContext(t.h,&c)){
                    if(!t.stackTop){MEMORY_BASIC_INFORMATION mbi{};if(VirtualQuery(reinterpret_cast<void*>(c.Rsp),&mbi,sizeof mbi))t.stackTop=reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize;}
                    if(t.stackTop>c.Rsp){len=std::min<size_t>(t.stackTop-c.Rsp,maxStack);std::memcpy(copy.data(),reinterpret_cast<const void*>(c.Rsp),len);ok=true;}
                }
                ResumeThread(t.h);
            }
            if(!ok)++t.s.failed;
            else{
                const uintptr_t lo=c.Rsp;walk.n=unwindCopy(&c,lo,lo+len,reinterpret_cast<uintptr_t>(copy.data()),walk.rip);
                if(walk.n){
                    ++t.s.samples;t.s.depth+=walk.n;
                    uintptr_t keys[maxFrames];bool anyMod=false;
                    for(int k=0;k<walk.n;k++){keys[k]=functionKey(uintptr_t(walk.rip[k]));const auto* m=moduleOf(keys[k]);if(m&&m->mod){anyMod=true;if(k==0)++t.s.modLeaf;}}
                    ++t.s.self[keys[0]];if(anyMod)++t.s.modAny;
                    std::sort(keys,keys+walk.n);const auto end=std::unique(keys,keys+walk.n);
                    for(auto* p=keys;p!=end;++p){++t.s.incl[*p];const auto* m=moduleOf(*p);if(m&&m->mod)++t.s.modIncl[*p];}
                }else ++t.s.failed;
            }
        }
        if(now-lastLog>=5000){
            for(auto& x:targets){if(!x.s.samples&&!x.s.failed)continue;char b[96];
                std::snprintf(b,sizeof b,"\"mod_any_pct\":%.1f,\"mod_leaf_pct\":%.1f,\"mean_depth\":%.1f",x.s.samples?100.0*x.s.modAny/double(x.s.samples):0.0,
                    x.s.samples?100.0*x.s.modLeaf/double(x.s.samples):0.0,x.s.samples?double(x.s.depth)/double(x.s.samples):0.0);
                try{logFile->event("thread_stacks","\"thread\":"+jsonString(x.name)+",\"tid\":"+std::to_string(x.id)+",\"span_ms\":"+std::to_string(now-lastLog)+",\"samples\":"+std::to_string(x.s.samples)+
                    ",\"failed\":"+std::to_string(x.s.failed)+","+b+",\"self\":["+top(x.s.self,30,x.s.samples)+"],\"incl\":["+top(x.s.incl,40,x.s.samples)+"],\"mod_incl\":["+top(x.s.modIncl,30,x.s.samples)+"]");}catch(...){}
                x.s=Stats{};}
            lastLog=now;
        }
    }
}
inline void start(){
    static std::atomic<bool> started{false};
    if(!wanted||started.exchange(true))return;
    renderThread=GetCurrentThreadId();
    std::thread(run).detach();
    try{logFile->event("thread_stacks_enabled","\"interval_ms\":1,\"threads\":[\"GameServer\",\"render\"],\"max_frames\":64,\"stack_copy_kb\":192,\"walk_after_resume\":true");}catch(...){}
}
}
