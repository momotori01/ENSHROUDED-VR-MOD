// The game stopped: where every thread is (2026-11-11). Players' starts stopped right after the first presents (Kip, Toasted,
// a Steam Frame start: the last events present 24..30, ui_glyph_known; docs/analysis/kip_preprocessing_20261108.md) and
// nothing in the logs says which call never returned. This watches the presents: when none has come for 20 s (and again
// after 120 s), it writes <log folder>/hang-stacks.txt - for every thread of the game its name and its calls, module +
// offset, the way a debugger would show them.
// Each thread is suspended only for GetThreadContext and resumed at once - no allocation and no lock while one is
// suspended (it could hold the heap's or a loader's lock). The calls are unwound afterwards from that context
// (RtlLookupFunctionEntry / RtlVirtualUnwind) on the thread's stack, which does not change while the thread waits.
// The file is written with plain WinAPI, not through note() (a stop on the layer's `mutex` would stop the writer too).
// EVR_HANG_WATCH=0 leaves it out. EVR_TEST_HANG_AT_PRESENT=<n> (tests): the present n sleeps 30 s, to see the file.
#include <tlhelp32.h>
namespace hangWatch {
constexpr unsigned frameLimit=40;
struct Stack{DWORD id=0;CONTEXT context{};bool read=false;};
inline std::atomic<bool> started{false};
inline bool readable(uint64_t address){
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof m)||m.State!=MEM_COMMIT)return false;
    return (m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY))&&!(m.Protect&PAGE_GUARD);
}
// The calls from a thread's context; plain C (structured exception handling), no objects.
inline unsigned unwind(CONTEXT context,uint64_t* out,unsigned limit){
    unsigned n=0;
    __try{
        while(n<limit&&context.Rip){
            out[n++]=context.Rip;
            DWORD64 base=0;const auto function=RtlLookupFunctionEntry(context.Rip,&base,nullptr);
            if(!function){   // a leaf: the return address is at the stack's top
                if(!readable(context.Rsp))break;
                context.Rip=*reinterpret_cast<const DWORD64*>(context.Rsp);context.Rsp+=8;
            }else{
                void* data=nullptr;DWORD64 frame=0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,context.Rip,function,&context,&data,&frame,nullptr);
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
    return n;
}
inline std::string where(uint64_t address){
    HMODULE module{};char path[MAX_PATH]{};
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(address),&module)&&
       GetModuleFileNameA(module,path,MAX_PATH)){
        const char* name=std::strrchr(path,'\\');name=name?name+1:path;char text[MAX_PATH+40];
        std::snprintf(text,sizeof text,"%s+0x%llx",name,static_cast<unsigned long long>(address-reinterpret_cast<uint64_t>(module)));return text;}
    char text[32];std::snprintf(text,sizeof text,"0x%llx",static_cast<unsigned long long>(address));return text;
}
inline std::string threadName(DWORD id){
    std::string out;
    if(const HANDLE h=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,id)){
        using Describe=HRESULT(WINAPI*)(HANDLE,PWSTR*);
        static const auto describe=reinterpret_cast<Describe>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetThreadDescription"));
        PWSTR text=nullptr;
        if(describe&&SUCCEEDED(describe(h,&text))&&text){
            const int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
            if(n>1){out.resize(size_t(n-1));WideCharToMultiByte(CP_UTF8,0,text,-1,out.data(),n,nullptr,nullptr);}
            LocalFree(text);}
        CloseHandle(h);}
    return out;
}
inline void dump(uint64_t stalledMs,uint64_t present,unsigned round){
    // The thread list first, while nothing is suspended.
    std::vector<Stack> stacks;
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot==INVALID_HANDLE_VALUE)return;
    THREADENTRY32 entry{sizeof entry};
    for(BOOL more=Thread32First(snapshot,&entry);more;more=Thread32Next(snapshot,&entry))
        if(entry.th32OwnerProcessID==GetCurrentProcessId()&&entry.th32ThreadID!=GetCurrentThreadId()){Stack s;s.id=entry.th32ThreadID;stacks.push_back(s);}
    CloseHandle(snapshot);
    for(auto& s:stacks){
        const HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,s.id);if(!h)continue;
        if(SuspendThread(h)!=DWORD(-1)){s.context.ContextFlags=CONTEXT_FULL;s.read=GetThreadContext(h,&s.context)!=0;ResumeThread(h);}
        CloseHandle(h);
    }
    std::string text="hang watch: no present for "+std::to_string(stalledMs/1000)+" s after present "+std::to_string(present)+" (round "+std::to_string(round)+
        "), "+std::to_string(stacks.size())+" other threads; the watching thread is "+std::to_string(GetCurrentThreadId())+"\r\n";
    for(const auto& s:stacks){
        text+="\r\nthread "+std::to_string(s.id);const auto name=threadName(s.id);if(!name.empty())text+=" \""+name+"\"";text+="\r\n";
        if(!s.read){text+="  (context not read)\r\n";continue;}
        uint64_t frames[frameLimit]{};const unsigned n=unwind(s.context,frames,frameLimit);
        for(unsigned i=0;i<n;++i)text+="  "+where(frames[i])+"\r\n";
    }
    const auto path=output+"/hang-stacks"+(round>1?"-"+std::to_string(round):std::string())+".txt";
    const HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr);CloseHandle(file);}
}
inline void start() noexcept {
    if(started.exchange(true))return;
    {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_HANG_WATCH",v,sizeof v);if(n==1&&v[0]=='0')return;}
    try{
        std::thread([]{
            uint64_t last=presents.load(),since=GetTickCount64();unsigned round=0;
            for(;;){
                Sleep(1000);
                const auto now=presents.load();const auto t=GetTickCount64();
                if(now!=last){last=now;since=t;round=0;continue;}
                const auto stalled=t-since;
                if((round==0&&stalled>=20000)||(round==1&&stalled>=120000)){++round;try{dump(stalled,now,round);}catch(...){}}
            }
        }).detach();
    }catch(...){}
}
// Tests: the present n sleeps 30 s on the presenting thread.
inline void testStop(uint64_t present) noexcept {
    static const uint64_t at=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_TEST_HANG_AT_PRESENT",v,sizeof v);return n&&n<sizeof v?std::strtoull(v,nullptr,10):0ull;}();
    if(at&&present==at)Sleep(30000);
}
}
