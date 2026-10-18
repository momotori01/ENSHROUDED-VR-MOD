// Read-only diagnostic (world-object-watch.txt, 2026-10-06): which bytes of a per-View object each engine
// function writes. Entry: snapshot the object passed in a given argument register; return: diff and log the
// changed 8-byte offsets as ranges. Sampled on pairs 300..302 for both eyes (and the original View).
// Purpose: classify per-eye (view) state versus world state before sharing an object between the eyes.
extern "C" { void* evr_watch_trampolines[10]{}; }
extern "C" void evr_watch_stub0();extern "C" void evr_watch_stub1();extern "C" void evr_watch_stub2();extern "C" void evr_watch_stub3();
extern "C" void evr_watch_stub4();extern "C" void evr_watch_stub5();extern "C" void evr_watch_stub6();extern "C" void evr_watch_stub7();
extern "C" void evr_watch_stub8();extern "C" void evr_watch_stub9();extern "C" void evr_watch_return_stub();
namespace objectWatch {
// arg: 0=rcx 1=rdx 2=r8 3=r9 (the saved array in the stub is r9,r8,rdx,rcx).
struct Target{uintptr_t rva;const char* pattern;unsigned arg;uint32_t bytes;const char* label;bool enabled=false;};
// 18ae90 (fog readiness predicate) is owned by world_ready_probe.hpp and is not watched.
inline std::array<Target,7> targets{{
    {0x19da50,"48 8B C4 4C 89 40 18 55 53 56 57 41",1,0x83e0,"fog_world_19d9b0"},
    {0x1bb160,"4C 89 44 24 18 48 89 54 24 10 48 89",1,0x83e0,"fog_view_192e70"},
    {0x190ac0,"48 89 5C 24 08 57 48 83 EC 40 0F 10",1,0x83e0,"fog_190a20"},
    {0x1b96a0,"48 85 C9 ? ? 48 8B 41 08 48 83 B8",0,0x83e0,"fog_1917d0"},
    {0x1b8a00,"48 83 EC 58 0F 10 82 70 01 00 00 4C",1,0x83e0,"fog_190b50"},
    {0x1904e0,"48 83 EC 28 0F 10 82 90 09 00 00 4C",1,0x83e0,"fog_190440"},
    {0x1a3c30,"48 8B C4 48 89 58 08 48 89 70 10 57",0,0x83e0,"fog_1a3b90"},
}};
inline bool requested=false,enabled=false;
inline std::atomic<bool> sampling{false};
inline thread_local int currentEye=-1; // set by recordPair around each eye (-1 = original View)
inline std::atomic<uint32_t> renderThread{0};
struct Record{uint32_t index;uintptr_t ret,object;int eye;uint32_t thread;std::vector<uint8_t> before;};
inline thread_local std::vector<Record> stack;
inline std::atomic<uint64_t> logged{0};
inline bool readable(uintptr_t address,size_t bytes,std::vector<uint8_t>& out){
    out.resize(bytes);SIZE_T copied=0;
    return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out.data(),bytes,&copied)&&copied==bytes;
}
inline void install(){
    void(*stubs[10])()={evr_watch_stub0,evr_watch_stub1,evr_watch_stub2,evr_watch_stub3,evr_watch_stub4,evr_watch_stub5,evr_watch_stub6,evr_watch_stub7,evr_watch_stub8,evr_watch_stub9};
    for(size_t i=0;i<targets.size();++i){
        auto& t=targets[i];const auto bytes=parsePattern(t.pattern);const auto site=baseAddress+t.rva;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("watch signature changed at "+hex(t.rva));
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(stubs[i]),&evr_watch_trampolines[i])!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("watch hook failed at "+hex(t.rva));
        t.enabled=true;
    }
    enabled=true;logFile->event("object_watch_enabled","\"targets\":"+std::to_string(targets.size()));
}
inline void uninstall() noexcept {
    sampling=false;
    for(auto it=targets.rbegin();it!=targets.rend();++it)if(it->enabled){
        const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+it->rva));if(status==MH_OK)it->enabled=false;}
    if(enabled)try{logFile->event("object_watch_removed","\"records\":"+std::to_string(logged.load()));}catch(...){}
    enabled=false;
}
}
extern "C" void evr_watch_enter(uint32_t index,uintptr_t* returnSlot,const uintptr_t* savedRegs) noexcept {
    using namespace objectWatch;
    Record r{index,*returnSlot,0,currentEye,GetCurrentThreadId(),{}};
    if(index<targets.size()&&sampling.load(std::memory_order_relaxed)){
        r.object=savedRegs[3-targets[index].arg];
        try{if(!readable(r.object,targets[index].bytes,r.before))r.before.clear();}catch(...){r.before.clear();}
    }
    try{stack.push_back(std::move(r));}catch(...){return;} // on failure the return address stays untouched
    *returnSlot=reinterpret_cast<uintptr_t>(&evr_watch_return_stub);
}
extern "C" uintptr_t evr_watch_leave() noexcept {
    using namespace objectWatch;
    auto r=std::move(stack.back());stack.pop_back();
    if(!r.before.empty())try{
        std::vector<uint8_t> after;
        if(readable(r.object,r.before.size(),after)){
            std::ostringstream f;f<<"\"function\":"<<jsonString(targets[r.index].label)<<",\"eye\":"<<r.eye<<",\"thread\":"<<r.thread<<",\"render_thread\":"<<renderThread.load()<<",\"return\":"<<hex(r.ret-baseAddress)<<",\"object\":"<<hex(r.object)<<",\"changed\":[";
            bool first=true;size_t runStart=SIZE_MAX;
            const size_t n=r.before.size()/8;
            for(size_t q=0;q<=n;++q){
                const bool diff=q<n&&std::memcmp(r.before.data()+q*8,after.data()+q*8,8)!=0;
                if(diff&&runStart==SIZE_MAX)runStart=q;
                if(!diff&&runStart!=SIZE_MAX){f<<(first?"":",")<<"[\""<<std::hex<<runStart*8<<"\",\""<<q*8<<"\"]"<<std::dec;first=false;runStart=SIZE_MAX;}
            }
            f<<']';logFile->event("object_watch",f.str());++logged;
        }
    }catch(...){}
    return r.ret;
}
