// Observe native early-return predicates only during an explicit world eye call.
// Results/arguments are forwarded unchanged; formatting happens after rendering.
namespace worldReadyDiagnostic {
using One=bool(__fastcall*)(void*);
using Two=bool(__fastcall*)(void*,void*);
struct Entry {uintptr_t function,caller;const char* pattern;void* original=nullptr;bool enabled=false;};
// 1.0 (2026-10-16): the render function's five early returns, in order (0x12a383..0x12a3e8): the renderer (0x11dcd0, one
// argument now; June's first predicate e5f60 took two), the pipeline (0x47cd0, renderer+0x16e10), the fog (0x1b1ae0,
// View+0xb40), the water (0x1ec5b0, View+0xb48) and the post object's asynchronous preparation (0xa4c220, the same
// one-instruction flag read as June's 9625b0). The matcher had paired them with other calls.
std::array<Entry,5> entries{{
    {0x11de60,0x12a52b,"48 8D 81 48 6D 01 00 48 8D 88 B0 00 00 00 48 3B"},
    {0x47d60,0x12a53f,"48 89 5C 24 08 48 89 74 24 10 57 48 81 EC A0 00 00 00 48 8B F9 48 85 C9"},
    {0x1b21d0,0x12a558,"40 53 48 83 EC 40 48 8B 19 48 8D 4C 24 20 44 8B"},
    {0x1eca60,0x12a571,"40 53 48 83 EC 40 48 85 C9 0F 84 ? ? ? ? 48"},
    {0xa4e3a0,0x12a586,"0F B6 81 FC 00 00 00 C3"}}};
struct Observation {size_t entry=0;uintptr_t object=0;bool result=false;};
struct Scope {std::array<Observation,8> observations{};size_t count=0;bool overflow=false;};
thread_local Scope* current=nullptr;
void observed(size_t entry,void* object,bool result,uintptr_t caller) noexcept {
    auto* scope=current;
    if(!scope||caller!=baseAddress+entries[entry].caller)return;
    if(scope->count>=scope->observations.size()){scope->overflow=true;return;}
    scope->observations[scope->count++]={entry,reinterpret_cast<uintptr_t>(object),result};
}
bool __fastcall scene(void* object,void* value){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool result=reinterpret_cast<Two>(entries[0].original)(object,value);
    observed(0,object,result,caller);return result;
}
template<size_t Index> bool __fastcall one(void* object){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool result=reinterpret_cast<One>(entries[Index].original)(object);
    observed(Index,object,result,caller);return result;
}
void install(){
    const std::array<void*,5> replacements{reinterpret_cast<void*>(&one<0>),reinterpret_cast<void*>(&one<1>),
        reinterpret_cast<void*>(&one<2>),reinterpret_cast<void*>(&one<3>),reinterpret_cast<void*>(&one<4>)};
    for(size_t i=0;i<entries.size();++i){auto& e=entries[i];const auto pattern=parsePattern(e.pattern);
        const auto target=baseAddress+e.function,call=baseAddress+e.caller-5;
        if(scan(reinterpret_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0}||
            read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target)
            throw std::runtime_error("world readiness predicate signature/caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(target),replacements[i],&e.original)!=MH_OK||
            MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("world readiness hook failed");
        e.enabled=true;
    }
}
void disable() noexcept {
    for(auto& e:entries)if(e.enabled){const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+e.function));
        if(status==MH_OK)e.enabled=false;
        try{logFile->event("world_readiness_hook_removed","\"function\":"+hex(e.function)+",\"status\":"+std::to_string(status));}catch(...){}
    }
}
void write(const Scope& scope,size_t eye){
    for(size_t i=0;i<scope.count;++i){const auto& o=scope.observations[i];const auto& e=entries[o.entry];
        logFile->event("world_readiness_predicate","\"eye\":"+std::to_string(eye)+",\"function\":"+hex(e.function)+
            ",\"caller\":"+hex(e.caller)+",\"object\":"+hex(o.object)+",\"result\":"+(o.result?"true":"false"));
    }
    if(scope.overflow)throw std::runtime_error("world readiness observation capacity exceeded");
}
} // namespace worldReadyDiagnostic
