// Bounded private-copy routing trial. The component is never modified.
// A private copy lives until the native builder has consumed it into prediction,
// history, and serialization. Identity-only unless an explicit head camera
// policy is requested. The separate TEST fixture also changes analog/mode bytes.
namespace inputShadow {
// 1.0 (2026-10-16): the builder takes six arguments - June's limit (the sixth, a qword) is gone and the freecam byte is the
// sixth (both callers, 0x8584a9 / 0x86e75e: a byte at [rsp+0x28]; the builder compares [rbp+0xe98], its sixth). Read as the
// seventh, the freecam was whatever the caller's stack held there, nearly always set, and the head never steered the walk
// (headset run 20261016-134128, every Mock since 1.0: free_camera true in every input_head_walking_sample).
using Build=uintptr_t(__fastcall*)(void*,void*,uint32_t,void*,uint32_t,uint8_t);
using Get=void*(__fastcall*)(void*,void*,uint32_t,uint32_t);
inline Build originalBuild=nullptr;inline Get originalGet=nullptr;
inline bool requested=false,buildEnabled=false,getEnabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},scopes{0},copies{0},failures{0};
constexpr uintptr_t buildRva=0x3a2470,getRva=0x9a6190;
constexpr const char* buildPattern="4C 89 4C 24 20 44 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D8 F1 FF FF 48 81 EC 28 0F 00 00 8B 9D 90 0E 00";
constexpr const char* getPattern="48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8D BA C0";
struct Scope {
    Scope* previous=nullptr;uintptr_t context=0,source=0;uint32_t tick=0,entity=0;
    bool supplied=false,freecam=false;
    alignas(16) game::ClientInputBytes initial{},shadow{},expected{};
};
inline thread_local Scope* current=nullptr;
struct CallbackScope{CallbackScope(){callbacks.fetch_add(1);}~CallbackScope(){callbacks.fetch_sub(1);}};
struct CurrentScope {Scope* previous=current;explicit CurrentScope(Scope& s){s.previous=previous;current=&s;}~CurrentScope(){current=previous;}};
inline void failure(const char* reason){if(failures.fetch_add(1)==0)logFile->event("input_shadow_failure","\"reason\":"+jsonString(reason));}
__declspec(noinline) uintptr_t __fastcall build(void* output,void* buffer,uint32_t tick,void* context,uint32_t connection,uint8_t freecam){
    const CallbackScope callback;Scope scope;bool use=false;
    {const ObserverLastError preserve;
     if(accepting.load()&&!current){
        try{
            const auto ctx=reinterpret_cast<uintptr_t>(context);
            rootCamera::Sample local;
            {std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
            // 1.0: the connection and the entity of the sender context moved from +0x5c0/+0x5c4 to +0x648/+0x64c
            if(ctx&&local.number&&GetTickCount64()-local.milliseconds<1000&&read<uint32_t>(ctx+0x648)==connection&&
               read<uint32_t>(ctx+0x64c)==local.entity){scope.context=ctx;scope.tick=tick;scope.entity=local.entity;scope.freecam=freecam!=0;use=true;}
        }catch(const std::exception& e){failure(e.what());}
     }}
    if(!use)return originalBuild(output,buffer,tick,context,connection,freecam);
    const CurrentScope active(scope);
    const auto result=originalBuild(output,buffer,tick,context,connection,freecam);
    {const ObserverLastError preserve;
     if(scope.supplied){
        try{
            const bool sourceUnchanged=read<game::ClientInputBytes>(scope.source)==scope.initial;
            const bool shadowUnchanged=scope.shadow==scope.expected;
            const bool cameraOnly=game::onlyHeadCameraChanged(scope.initial,scope.shadow);
            const bool allowedChange=cameraOnly||(controllerUiFixture::liveGuardRequested&&game::onlyHeadCameraAndStickChanged(scope.initial,scope.shadow))||
                (inputHeadIdle::fixtureRequested&&game::onlyFixtureAndCameraChanged(scope.initial,scope.shadow))||
                (holsterInput::requested&&game::onlyHolsterAndCameraChanged(scope.initial,scope.shadow));
            const unsigned sample=scopes.fetch_add(1)+1;
            if(sample<=8||sample%128==0||!sourceUnchanged||!shadowUnchanged){
                std::ostringstream f;f<<"\"sample\":"<<sample<<",\"tick\":"<<scope.tick<<",\"entity\":"<<scope.entity
                    <<",\"source_unchanged\":"<<(sourceUnchanged?"true":"false")<<",\"shadow_unchanged\":"<<(shadowUnchanged?"true":"false")
                    <<",\"bytes\":"<<game::clientInputBytes<<",\"identity_only\":"<<(inputHeadIdle::requested?"false":"true")
                    <<",\"camera_only\":"<<(cameraOnly?"true":"false")
                    <<",\"controller_fixture\":"<<(inputHeadIdle::fixtureRequested?"true":"false")
                    <<",\"permitted_fields_only\":"<<(allowedChange?"true":"false");logFile->event("input_shadow_scope",f.str());
            }
            if(!sourceUnchanged||!shadowUnchanged||!allowedChange)failure("native input source/private copy changed outside permitted patch");
        }catch(const std::exception& e){failure(e.what());}
     }}
    return result;
}
__declspec(noinline) void* __fastcall get(void* output,void* ecs,uint32_t component,uint32_t entity){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const CallbackScope callback;
    auto result=originalGet(output,ecs,component,entity);
    const ObserverLastError preserve;
    if(!accepting.load()||!current||caller!=baseAddress+0x3a24e8)return result;
    try{
        auto& s=*current;const auto out=reinterpret_cast<uintptr_t>(output);
        const auto span=read<std::array<uintptr_t,2>>(out);
        // 1.0 layout search (EVR_LAYOUT_DUMP=1): the ClientPlayerInput the builder reads, a sample every 32 calls.
        {static const bool dump=[]{char v[8]{};const auto k=GetEnvironmentVariableA("EVR_LAYOUT_DUMP",v,sizeof v);return k==1&&v[0]=='1';}();
         static std::atomic<unsigned> seen{0},written{0};
         if(dump&&span[0]&&span[1]&&span[1]<=8192&&seen.fetch_add(1)%48==0&&written.load()<48){const auto n=written.fetch_add(1);
            std::vector<uint8_t> bytes(static_cast<size_t>(span[1]));SIZE_T got=0;ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(span[0]),bytes.data(),bytes.size(),&got);
            std::ofstream(calibrationDirectory/("layout-client-input-"+std::to_string(n)+".bin"),std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
            logFile->event("input_shadow_layout_dump","\"sample\":"+std::to_string(n)+",\"bytes\":"+std::to_string(span[1])+",\"tick\":"+std::to_string(s.tick)+",\"frame\":"+std::to_string(liveFrame.frame));}}
        if(result!=output||s.supplied||entity!=s.entity||read<uintptr_t>(s.context+8)!=reinterpret_cast<uintptr_t>(ecs)||
           !span[0]||span[1]!=s.shadow.size())throw std::runtime_error("input source span identity/stride mismatch");
        s.source=span[0];s.initial=read<game::ClientInputBytes>(s.source);s.shadow=s.initial;
        inputHeadIdle::patch(s.initial,s.shadow,s.context,s.entity,s.tick,s.freecam);s.expected=s.shadow;
        // Only a local return wrapper is changed. Its second word (stride), ECS
        // component storage and camera owner remain intact.
        const auto privateData=reinterpret_cast<uintptr_t>(s.shadow.data());
        std::memcpy(output,&privateData,sizeof(privateData));s.supplied=true;copies.fetch_add(1);
    }catch(const std::exception& e){failure(e.what());}
    return result;
}
inline void verify(){
    inputPipeline::verify(getRva,getPattern,0x3a24e8);
    const auto bytes=parsePattern(buildPattern);
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+buildRva),bytes.size(),bytes)!=std::vector<size_t>{0})
        throw std::runtime_error("input builder signature changed");
}
inline void install(){
    if(!requested)return;verify();
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+buildRva),reinterpret_cast<void*>(&build),reinterpret_cast<void**>(&originalBuild))!=MH_OK||
       MH_CreateHook(reinterpret_cast<void*>(baseAddress+getRva),reinterpret_cast<void*>(&get),reinterpret_cast<void**>(&originalGet))!=MH_OK)
        throw std::runtime_error("input private copy hooks creation failed");
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+getRva))!=MH_OK)throw std::runtime_error("input private getter enable failed");getEnabled=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+buildRva))!=MH_OK)throw std::runtime_error("input private builder enable failed");buildEnabled=true;
    accepting=true;logFile->event("input_shadow_enabled","\"identity_only\":"+std::string(inputHeadIdle::requested?"false":"true")+
        ",\"controller_fixture\":"+(inputHeadIdle::fixtureRequested?"true":"false")+",\"build_rva\":"+hex(buildRva)+",\"get_rva\":"+hex(getRva)+
        ",\"build_pattern\":"+jsonString(buildPattern)+",\"get_pattern\":"+jsonString(getPattern));
}
inline void disable(){
    accepting=false;const bool had=buildEnabled||getEnabled;
    if(buildEnabled){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+buildRva))!=MH_OK)throw std::runtime_error("input private builder disable failed");buildEnabled=false;}
    if(getEnabled){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+getRva))!=MH_OK)throw std::runtime_error("input private getter disable failed");getEnabled=false;}
    if(!had)return;
    const auto deadline=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    verify();logFile->event("input_shadow_removed","\"restored\":true,\"scopes\":"+std::to_string(scopes.load())+
        ",\"copies\":"+std::to_string(copies.load())+",\"failures\":"+std::to_string(failures.load())+
        ",\"remaining_callbacks\":"+std::to_string(callbacks.load()));
    if(inputHeadIdle::requested)logFile->event(inputHeadIdle::walkingRequested?"input_head_walking_removed":"input_head_idle_removed",
        "\"attempted\":"+std::to_string(inputHeadIdle::attempted.load())+",\"applied\":"+std::to_string(inputHeadIdle::applied.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    if(callbacks.load())throw std::runtime_error("private input callbacks did not quiesce after hook removal");
}
}
