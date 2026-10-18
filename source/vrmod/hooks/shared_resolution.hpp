// Included inside the probe namespace, after checked metadata reads.
// Scope is thread-local: the game's own Scene resize/destruction still executes.
struct SharedResolutionScope;
thread_local SharedResolutionScope* privateResolution = nullptr;
struct SharedResolutionScope {
    SharedResolutionScope* previous = privateResolution;
    std::array<uintptr_t,3> objects{};
    std::array<uintptr_t,2> rendererSsr{};
    std::array<PrivateSsr,2>* ownedSsr=nullptr;
    explicit SharedResolutionScope(uintptr_t scene,std::array<PrivateSsr,2>* ssr=nullptr):ownedSsr(ssr) {
        // 1.0: the scene's three shared resolution objects (0x930, 0x9c8, 0x9d8 before), the renderer's two SSR objects.
        objects={read<uintptr_t>(scene+0x9d0),read<uintptr_t>(scene+0xa80),read<uintptr_t>(scene+0xa88)};
        const auto renderer=read<uintptr_t>(scene+layout::sceneRenderer);
        rendererSsr={read<uintptr_t>(renderer+layout::rendererSsr),read<uintptr_t>(renderer+layout::rendererSsr+8)};
        privateResolution=this;
    }
    SharedResolutionScope(const SharedResolutionScope&)=delete;
    SharedResolutionScope& operator=(const SharedResolutionScope&)=delete;
    ~SharedResolutionScope(){privateResolution=previous;}
};
using SharedInit=void (__fastcall*)(void*,const void*);
using SharedFree=void (__fastcall*)(void*);
struct SharedHook {
    uintptr_t target,caller;
    const char* pattern;
    void* original=nullptr;
    bool enabled=false;
    uintptr_t secondCaller=0;
};
std::array<SharedHook,8> sharedHooks{{
    {0x14aa40,0x10446c,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 48 8B FA 66"},
    {0x4fe60,0x1040c1,"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 48 8B EC 48 83 EC 40 48 8B F2 48 8B F9 48"},
    {0x1f9f30,0x1040df,"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 6C 24 20 55 41 56 41 57 48 8B EC 48 83 EC 50 33 C0 48 C7 45 E8 20 00 00 00 88 45 F4 4C 8D"},
    {0x14b0c0,0x107c26,"40 53 48 83 EC 20 48 8B D9 48 8D 91 E0 00 00 00 48 8B 49 10 E8 ? ? ? ?"},
    {0x52550,0x107d4e,"40 53 48 83 EC 20 48 8B D9 48 8D 51 48 48 8B 49"},
    {0x1faf60,0x107d5f,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 BE 02 00"},
    {0x17a490,0x104011,"40 55 56 57 41 54 41 56 41 57 48 8D AC 24 78",nullptr,false,0x104147},
    // 1.0: the SSR free (0x153fe0 in June) is 0x17b210, called for both slots (View+0xf18 at 0x107c87, +0xf20 at
    // 0x107ccb), as before.
    {0x17b2b0,0x107d2c,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 79 10 48",nullptr,false,0x107d70}
}};
bool skipShared(size_t index,void* object,uintptr_t caller) {
    if(!privateResolution)return false;
    const auto& hook=sharedHooks[index];
    if(caller!=baseAddress+hook.caller&&(!hook.secondCaller||caller!=baseAddress+hook.secondCaller))return false;
    const auto address=reinterpret_cast<uintptr_t>(object);
    if(index<6){if(address!=privateResolution->objects[index%3])return false;}
    else if(address!=privateResolution->rendererSsr[0]&&address!=privateResolution->rendererSsr[1])return false;
    const ObserverLastError preserve;
    try{logFile->event("shared_resolution_preserved","\"operation\":"+jsonString(index<3||index==6?"init":"free")+
        ",\"function\":"+hex(sharedHooks[index].target)+",\"object\":"+hex(reinterpret_cast<uintptr_t>(object)));}catch(...){}
    return true;
}
void completePrivateSsrResolution(void* object,uintptr_t caller,bool initialized) noexcept {
    if(!privateResolution||!privateResolution->ownedSsr)return;
    const auto& hook=sharedHooks[initialized?6:7];
    if(caller!=baseAddress+hook.caller&&caller!=baseAddress+hook.secondCaller)return;
    for(auto& owned:*privateResolution->ownedSsr)
        if(owned.object&&owned.object==reinterpret_cast<uintptr_t>(object))owned.resolutionInitialized=initialized;
}
template<size_t Index> void __fastcall sharedInit(void* object,const void* parameters) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!skipShared(Index,object,caller)){
        reinterpret_cast<SharedInit>(sharedHooks[Index].original)(object,parameters);
        if constexpr(Index==6)completePrivateSsrResolution(object,caller,true);
    }
}
template<size_t Index> void __fastcall sharedFree(void* object) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!skipShared(Index,object,caller)){
        reinterpret_cast<SharedFree>(sharedHooks[Index].original)(object);
        if constexpr(Index==7)completePrivateSsrResolution(object,caller,false);
    }
}
void installSharedResolutionHooks(){
    const std::array<void*,8> detours{reinterpret_cast<void*>(&sharedInit<0>),reinterpret_cast<void*>(&sharedInit<1>),
        reinterpret_cast<void*>(&sharedInit<2>),reinterpret_cast<void*>(&sharedFree<3>),
        reinterpret_cast<void*>(&sharedFree<4>),reinterpret_cast<void*>(&sharedFree<5>),
        reinterpret_cast<void*>(&sharedInit<6>),reinterpret_cast<void*>(&sharedFree<7>)};
    for(size_t i=0;i<sharedHooks.size();++i){auto& h=sharedHooks[i];
        const auto bytes=parsePattern(h.pattern);
        if(scan(reinterpret_cast<uint8_t*>(baseAddress+h.target),bytes.size(),bytes)!=std::vector<size_t>{0})
            throw std::runtime_error("shared resolution signature changed at "+hex(h.target));
        for(const auto caller:{h.caller,h.secondCaller})if(caller){
            const auto call=baseAddress+caller-5;
            if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+h.target)
                throw std::runtime_error("shared resolution caller changed");
        }
        if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+h.target),detours[i],&h.original)!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(baseAddress+h.target))!=MH_OK)
            throw std::runtime_error("shared resolution hook failed");
        h.enabled=true;
    }
}
void disableSharedResolutionHooks() noexcept {
    for(auto& h:sharedHooks)if(h.enabled){const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+h.target));
        h.enabled=status!=MH_OK;
        try{logFile->event("shared_resolution_hook_removed","\"function\":"+hex(h.target)+",\"status\":"+std::to_string(status));}catch(...){}
    }
}
