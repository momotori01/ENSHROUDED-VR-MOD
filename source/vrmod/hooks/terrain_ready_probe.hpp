// Observe native terrain upload readiness without changing arguments or results.
// All statistics are local to the f7c50 thread/scope; formatting is deferred.
namespace terrainReadyDiagnostic {
using Poll=bool(__fastcall*)(void*,uint32_t);
inline Poll original=nullptr;
inline bool enabled=false;
constexpr uintptr_t rva=0xd7270;
constexpr std::array<uintptr_t,3> callers{0x1b823b,0x1b8264,0x1b828d};
struct Observation {uint32_t handle=0;bool ready=false;int flags=-1;bool generationMatches=false;};
struct Context {uint64_t calls=0,ready=0;uintptr_t manager=0;bool multipleManagers=false;std::array<Observation,12> sample{};};
inline thread_local Context* current=nullptr;
struct Scope {Context* previous=current;explicit Scope(Context* value){current=value;}~Scope(){current=previous;}};
bool __fastcall poll(void* manager,uint32_t handle){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    const bool result=original(manager,handle);
    if(current&&std::find(callers.begin(),callers.end(),caller)!=callers.end()){
        auto& c=*current;
        const auto address=reinterpret_cast<uintptr_t>(manager);
        if(!c.manager)c.manager=address;else if(c.manager!=address)c.multipleManagers=true;
        if(c.calls<c.sample.size()){
            auto& s=c.sample[size_t(c.calls)];s.handle=handle;s.ready=result;
            // Read the same slot immediately after the native predicate, without
            // changing it. This is not an atomic snapshot across worker threads.
            try{
                const auto shift=read<uint64_t>(address+0x270),mask=read<uint64_t>(address+0x280);
                if(shift<=16&&mask<=65535){
                    const auto page=uint64_t(handle&0xffff)>>shift,index=uint64_t(handle&0xffff)&mask;
                    if(page<read<uint64_t>(address+0x250)){
                        const auto entry=read<uintptr_t>(address+0x248)+page*64;
                        if(index<read<uint64_t>(entry+0x10)){
                            const auto slot=read<uintptr_t>(entry+8)+index*0x140;
                            s.generationMatches=(read<uint32_t>(slot)&0x7fffffffu)==(handle>>16);
                            if(s.generationMatches)s.flags=read<uint8_t>(slot+0x110);
                        }
                    }
                }
            }catch(...){}
        }
        ++c.calls;if(result)++c.ready;
    }
    return result;
}
inline void install(){
    const auto pattern=parsePattern("4C 8B D1 85 D2 74 71 48 8B 89 70 02 00 00 44 0F B7 C2");
    const auto target=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0})
        throw std::runtime_error("terrain readiness signature changed");
    for(auto caller:callers){const auto call=baseAddress+caller-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
            throw std::runtime_error("terrain readiness caller changed");
    }
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&poll),reinterpret_cast<void**>(&original))!=MH_OK||
        MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("terrain readiness observer hook failed");
    enabled=true;
    logFile->event("terrain_readiness_hook_enabled","\"rva\":\"0xd7270\",\"changes_native_results\":false");
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("terrain_readiness_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
}
inline void write(const Context& c,int eye,uint64_t serial){
    std::ostringstream f;f<<"\"eye\":"<<eye<<",\"serial\":"<<serial<<",\"calls\":"<<c.calls<<",\"ready\":"<<c.ready
        <<",\"manager\":"<<hex(c.manager)<<",\"multiple_managers\":"<<(c.multipleManagers?"true":"false")<<",\"sample\":[";
    for(size_t i=0;i<std::min<uint64_t>(c.calls,c.sample.size());++i){if(i)f<<',';const auto& s=c.sample[i];f<<"{\"handle\":"<<s.handle<<",\"ready\":"<<(s.ready?"true":"false")<<",\"flags_after\":"<<s.flags<<",\"generation_matches\":"<<(s.generationMatches?"true":"false")<<'}';}
    f<<']';logFile->event("terrain_native_ready_poll",f.str());
}
}
