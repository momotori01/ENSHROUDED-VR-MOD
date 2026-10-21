// Read-only native GPU feedback pool observation. No allocator/serial override.
namespace downloadFeedbackDiagnostic {
using Copy=void*(__fastcall*)(void*,void*,void*,void*,const void*);
inline Copy original=nullptr;
inline bool enabled=false;
constexpr uintptr_t rva=0x10153c0;
constexpr std::array<uintptr_t,5> callers{0x2a49e,0x1fada2,0x7af30,0x7cad2,0x17eed1};
struct Slot {uint64_t inFlight=0,completed=0;};
struct Snapshot {
    bool valid=false;
    uint64_t count=0,current=0,submitted=0,completed=0,pending=0;
    std::array<Slot,64> slots{};
};
struct Observation {uintptr_t caller=0,pool=0,requestedPool=0;int status=-1;Snapshot before,after;};
struct Context {size_t count=0;bool overflow=false;std::array<Observation,8> sample{};};
inline thread_local Context* current=nullptr;
struct Scope {Context* previous=current;explicit Scope(Context* value){current=value;}~Scope(){current=previous;}};
inline Snapshot snapshot(uintptr_t pool) noexcept {
    Snapshot s;
    try {
        s.count=read<uint64_t>(pool+0x28);
        if(!s.count||s.count>s.slots.size())return s;
        const auto entries=read<uintptr_t>(pool+0x20),graphics=read<uintptr_t>(pool+8);
        if(!entries||!graphics)return s;
        s.current=read<uint64_t>(graphics+0x540);
        s.submitted=read<uint64_t>(graphics+0x548);
        s.completed=read<uint64_t>(graphics+0x550);
        s.pending=read<uint64_t>(pool+0x278);
        for(size_t i=0;i<s.count;++i){
            s.slots[i].inFlight=read<uint64_t>(entries+i*0x40+0x10);
            s.slots[i].completed=read<uint64_t>(entries+i*0x40+0x28);
        }
        s.valid=true;
    }catch(...){}
    return s;
}
void* __fastcall copy(void* graph,void* out,void* pool,void* regions,const void* context){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    const auto requestedPool=reinterpret_cast<uintptr_t>(pool);
    pool=reinterpret_cast<void*>(waterFeedback::route(requestedPool,caller));
    Observation* observation=nullptr;
    if(current&&std::find(callers.begin(),callers.end(),caller)!=callers.end()){
        if(current->count<current->sample.size()){
            observation=&current->sample[current->count++];
            observation->caller=caller;observation->pool=reinterpret_cast<uintptr_t>(pool);
            observation->requestedPool=requestedPool;
            observation->before=snapshot(observation->pool);
        }else current->overflow=true;
    }
    auto* result=original(graph,out,pool,regions,context);
    if(observation){
        try{observation->status=read<uint8_t>(reinterpret_cast<uintptr_t>(out));}catch(...){}
        observation->after=snapshot(observation->pool);
    }
    return result;
}
inline void install(){
    const auto pattern=parsePattern("48 89 54 24 10 48 89 4C 24 08 53 57 41 55 41 57 48 81 EC E8 00 00 00");
    const auto target=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0})
        throw std::runtime_error("download feedback signature changed");
    for(auto caller:callers){const auto call=baseAddress+caller-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
            throw std::runtime_error("download feedback caller changed");
    }
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&copy),reinterpret_cast<void**>(&original))!=MH_OK||
        MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("download feedback observer hook failed");
    enabled=true;
    logFile->event("download_feedback_hook_enabled","\"rva\":\"0x10153c0\",\"changes_native_results\":false,\"private_water_routing\":"+std::string(privateWaterFeedback?"true":"false"));
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("download_feedback_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
}
inline void format(std::ostringstream& f,const Snapshot& s){
    f<<"{\"valid\":"<<(s.valid?"true":"false")<<",\"capacity\":"<<s.count<<",\"current\":"<<s.current
     <<",\"submitted\":"<<s.submitted<<",\"completed\":"<<s.completed<<",\"pending\":"<<s.pending<<",\"slots\":[";
    if(s.valid)for(size_t i=0;i<s.count;++i){if(i)f<<',';f<<'['<<s.slots[i].inFlight<<','<<s.slots[i].completed<<']';}
    f<<"]}";
}
inline void write(const Context& c,int eye,uint64_t serial){
    std::ostringstream f;f<<"\"eye\":"<<eye<<",\"serial\":"<<serial<<",\"overflow\":"<<(c.overflow?"true":"false")<<",\"calls\":[";
    for(size_t i=0;i<c.count;++i){if(i)f<<',';const auto& o=c.sample[i];
        f<<"{\"caller\":"<<hex(o.caller)<<",\"pool\":"<<hex(o.pool)<<",\"requested_pool\":"<<hex(o.requestedPool)<<",\"status\":"<<o.status<<",\"before\":";
        format(f,o.before);f<<",\"after\":";format(f,o.after);f<<'}';
    }
    f<<']';logFile->event("download_feedback_pool",f.str());
}
}
