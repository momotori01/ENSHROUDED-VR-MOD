// Isolated, unused detail-view allocation diagnostic. Include in the probe
// namespace after read()/hex(); install before the render hook is enabled.
// The caller must quiesce render callbacks before disabling the reserve hook.
// No Scene/View pointer, shared manager field or game-owned object is changed.
namespace detailFactoryDiagnostic {
using Reserve = bool (__fastcall*)(void*,uint64_t,bool);
using Allocate = void* (__fastcall*)(void*,uint64_t,uint64_t,uint8_t*,uint8_t);
using Free = void (__fastcall*)(void*,void*,uint8_t*);
using Factory = void* (__fastcall*)(void*,const void*);
using Dispose = void (__fastcall*)(void*,void*);
using AllocateResult = void* (__fastcall*)(void*,void*,uint64_t,uint64_t,uint8_t*,uint8_t);
Reserve originalReserve=nullptr;
Factory originalTracedFactory=nullptr;
AllocateResult originalAllocateResult=nullptr;
bool reserveEnabled=false;
bool factoryTraceEnabled=false;
bool allocatorTraceEnabled=false;
unsigned nativeFactoryOrdinal=0;
struct AllocatorSnapshot {
    uintptr_t tlsf=0,growthProvider=0,growthBytes=0,growthHead=0,liveBytes=0,peakBytes=0,fallbackBytes=0;
    uint32_t allocations=0,fallbackAllocations=0;
    uint8_t policy=0,frozen=0;
    bool readable=false;
};
struct AllocationObservation {
    uintptr_t allocator=0,caller=0,result=0;
    uint64_t size=0,alignment=0,actualSize=0;
    uint8_t tagBefore=0,tagAfter=0,flags=0;
    AllocatorSnapshot before{},after{};
};
struct Body {
    uintptr_t address=0;
    uint8_t returnedTag=0;
    DWORD returnedError=0;
    bool consumed=false,freed=false,nativeZero=false;
};
struct AllocatorFacade {
    struct Forwarded {uintptr_t address=0;bool active=false;};
    void** vtable=nullptr;
    uintptr_t original=0;
    Allocate allocate=nullptr;
    Free free=nullptr;
    std::array<Body,2>* bodies=nullptr;
    size_t eye=0;
    bool contractViolation=false;
    std::array<Forwarded,8> forwarded{};
};
struct Scope {
    Scope* previous=nullptr;
    uintptr_t body=0;
    size_t eye=0;
    unsigned observations=0,failedReserves=0;
    bool discoverBody=false;
    std::array<AllocationObservation,16> allocations{};
    unsigned allocationCount=0;
    bool allocationOverflow=false;
};
thread_local Scope* current=nullptr;
struct ScopeLease {
    Scope& scope;
    explicit ScopeLease(Scope& value):scope(value){scope.previous=current;current=&scope;}
    ~ScopeLease(){current=scope.previous;}
};
AllocatorSnapshot snapshotAllocator(uintptr_t address) noexcept {
    const ObserverLastError preserve;
    AllocatorSnapshot out;
    try{
        out.tlsf=read<uintptr_t>(address+8);out.policy=read<uint8_t>(address+0x70);
        out.growthProvider=read<uintptr_t>(address+0x88);out.growthBytes=read<uintptr_t>(address+0x90);
        out.growthHead=read<uintptr_t>(address+0x98);out.liveBytes=read<uintptr_t>(address+0xa0);
        out.peakBytes=read<uintptr_t>(address+0xa8);out.allocations=read<uint32_t>(address+0xb0);
        out.fallbackBytes=read<uintptr_t>(address+0xb8);out.fallbackAllocations=read<uint32_t>(address+0xc8);
        out.frozen=read<uint8_t>(address+0x108);out.readable=true;
    }catch(...){}
    return out;
}
void* __fastcall traceAllocateResult(void* allocator,void* output,uint64_t size,uint64_t alignment,uint8_t* tag,uint8_t flags){
    // Fixed storage only: this function may execute while a native allocator
    // lock is held. All formatting/file IO is deferred until factory return.
    auto* scope=current;
    if(!scope||!scope->discoverBody)return originalAllocateResult(allocator,output,size,alignment,tag,flags);
    AllocationObservation* observation=nullptr;
    {const ObserverLastError preserve;
        if(scope->allocationCount<scope->allocations.size()){
            observation=&scope->allocations[scope->allocationCount++];
            observation->allocator=reinterpret_cast<uintptr_t>(allocator);
            observation->caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
            observation->size=size;observation->alignment=alignment;observation->flags=flags;
            observation->tagBefore=tag?*tag:0;observation->before=snapshotAllocator(observation->allocator);
        }else scope->allocationOverflow=true;
    }
    void* result=originalAllocateResult(allocator,output,size,alignment,tag,flags);
    {const ObserverLastError preserve;
        if(observation){
            observation->tagAfter=tag?*tag:0;observation->after=snapshotAllocator(observation->allocator);
            try{observation->result=read<uintptr_t>(reinterpret_cast<uintptr_t>(output));
                observation->actualSize=read<uint64_t>(reinterpret_cast<uintptr_t>(output)+8);}catch(...){}
        }
    }
    return result;
}
std::string allocatorJson(const AllocatorSnapshot& value){
    return "{\"readable\":"+std::string(value.readable?"true":"false")+",\"tlsf\":"+hex(value.tlsf)+
        ",\"policy\":"+std::to_string(value.policy)+",\"frozen\":"+std::to_string(value.frozen)+
        ",\"growth_provider\":"+hex(value.growthProvider)+",\"growth_bytes\":"+std::to_string(value.growthBytes)+
        ",\"growth_head\":"+hex(value.growthHead)+",\"live_bytes\":"+std::to_string(value.liveBytes)+
        ",\"peak_bytes\":"+std::to_string(value.peakBytes)+",\"allocations\":"+std::to_string(value.allocations)+
        ",\"fallback_bytes\":"+std::to_string(value.fallbackBytes)+",\"fallback_allocations\":"+std::to_string(value.fallbackAllocations)+"}";
}
bool snapshotArray(void* array,std::array<uintptr_t,5>& out) noexcept {
    try{out=read<std::array<uintptr_t,5>>(reinterpret_cast<uintptr_t>(array));return true;}
    catch(...){return false;}
}
bool __fastcall reserve(void* array,uint64_t count,bool mayFail) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto* scope=current;
    const auto address=reinterpret_cast<uintptr_t>(array);
    const bool knownCaller=caller==baseAddress+0x4790e||caller==baseAddress+0x47933||
        caller==baseAddress+0x47962||caller==baseAddress+0x47987;
    if(scope&&knownCaller&&scope->discoverBody&&!scope->body){
        const uintptr_t offset=(caller==baseAddress+0x4790e||caller==baseAddress+0x47933)?0xc8:0xf8;
        if(address>offset)scope->body=address-offset;
    }
    if(!scope||!knownCaller||(address!=scope->body+0xc8&&address!=scope->body+0xf8))
        return originalReserve(array,count,mayFail);
    std::array<uintptr_t,5> before{},after{};bool beforeValid=false,afterValid=false;
    {const ObserverLastError preserve;beforeValid=snapshotArray(array,before);}
    const bool result=originalReserve(array,count,mayFail);
    {const ObserverLastError preserve;
        afterValid=snapshotArray(array,after);++scope->observations;if(!result)++scope->failedReserves;
        try{logFile->event("detail_array_reserve","\"eye\":"+std::to_string(scope->eye)+
            ",\"callsite\":"+hex(caller-baseAddress)+",\"array\":"+hex(address)+
            ",\"requested_capacity\":"+std::to_string(count)+",\"may_fail\":"+(mayFail?"true":"false")+
            ",\"before_readable\":"+(beforeValid?"true":"false")+",\"after_readable\":"+(afterValid?"true":"false")+
            ",\"capacity_before\":"+std::to_string(before[2])+",\"capacity_after\":"+std::to_string(after[2])+
            ",\"allocator_with_flag_before\":"+hex(before[4])+",\"allocator_with_flag_after\":"+hex(after[4])+
            ",\"data_before\":"+hex(before[0])+",\"data_after\":"+hex(after[0])+",\"result\":"+(result?"true":"false"));}catch(...){}
    }
    return result;
}
void* __fastcall traceFactory(void* manager,const void* context){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    // Original Scene initialization is forwarded without observation. Only
    // c02a0 inside our private resolution scope can activate this trace.
    if(!privateResolution||caller!=baseAddress+0xf2612)return originalTracedFactory(manager,context);
    Scope scope;scope.eye=nativeFactoryOrdinal++;scope.discoverBody=true;
    void* result=nullptr;
    {ScopeLease lease(scope);result=originalTracedFactory(manager,context);}
    const ObserverLastError preserve;
    for(unsigned i=0;i<scope.allocationCount;++i){const auto& a=scope.allocations[i];
        try{logFile->event("private_detail_native_allocation","\"ordinal\":"+std::to_string(scope.eye)+
            ",\"allocation\":"+std::to_string(i)+",\"allocator\":"+hex(a.allocator)+",\"caller\":"+hex(a.caller)+
            ",\"size\":"+std::to_string(a.size)+",\"alignment\":"+std::to_string(a.alignment)+
            ",\"tag_before\":"+std::to_string(a.tagBefore)+",\"tag_after\":"+std::to_string(a.tagAfter)+
            ",\"flags\":"+std::to_string(a.flags)+",\"result\":"+hex(a.result)+",\"actual_size\":"+std::to_string(a.actualSize)+
            ",\"before\":"+allocatorJson(a.before)+",\"after\":"+allocatorJson(a.after));}catch(...){}
    }
    try{logFile->event("private_native_detail_factory_return","\"ordinal\":"+std::to_string(scope.eye)+
        ",\"manager\":"+hex(reinterpret_cast<uintptr_t>(manager))+",\"object\":"+hex(reinterpret_cast<uintptr_t>(result))+
        ",\"observed_body\":"+hex(scope.body)+",\"reserve_observations\":"+std::to_string(scope.observations)+
        ",\"failed_reserves\":"+std::to_string(scope.failedReserves)+",\"allocator_forwarding\":false"+
        ",\"allocation_observations\":"+std::to_string(scope.allocationCount)+",\"allocation_overflow\":"+(scope.allocationOverflow?"true":"false"));}catch(...){}
    return result;
}
void* __fastcall allocate(void* opaque,uint64_t size,uint64_t alignment,uint8_t* tag,uint8_t flags) {
    auto& facade=*static_cast<AllocatorFacade*>(opaque);
    auto& body=(*facade.bodies)[facade.eye];
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(caller==baseAddress+0x477fe&&!body.consumed&&size==0x210&&alignment==8&&tag&&*tag==8&&flags==0){
        body.consumed=true;*tag=body.returnedTag;SetLastError(body.returnedError);
        return reinterpret_cast<void*>(body.address);
    }
    auto* result=facade.allocate(reinterpret_cast<void*>(facade.original),size,alignment,tag,flags);
    if(result){bool recorded=false;
        for(auto& allocation:facade.forwarded)if(!allocation.active){allocation={reinterpret_cast<uintptr_t>(result),true};recorded=true;break;}
        if(!recorded)facade.contractViolation=true;
    }
    return result;
}
void __fastcall free(void* opaque,void* allocation,uint8_t* tag) {
    auto& facade=*static_cast<AllocatorFacade*>(opaque);
    // A failed factory can free its body before the next eye's arrays allocate.
    // Native TLSF may reuse that address for a legitimate forwarded allocation.
    // Match the active allocation before consulting the old body's history.
    for(auto& forwarded:facade.forwarded)if(forwarded.active&&forwarded.address==reinterpret_cast<uintptr_t>(allocation)){
        forwarded.active=false;facade.free(reinterpret_cast<void*>(facade.original),allocation,tag);return;
    }
    for(auto& body:*facade.bodies)if(body.address==reinterpret_cast<uintptr_t>(allocation)){
        if(body.freed){facade.contractViolation=true;return;}
        body.freed=true;break;
    }
    facade.free(reinterpret_cast<void*>(facade.original),allocation,tag);
}
void validate(){
    const std::pair<uintptr_t,const char*> sites[]={
        {0x477c0,"48 89 5C 24 10 55 56 57 41 54 41 55 41 56 41 57"},
        {0x49400,"40 53 55 56 57 41 55 48 83 EC 20"},
        {0x4eb50,"40 56 57 41 57 48 83 EC 30 4C 8B 79 10"},
        {0x87aee0,"48 83 EC 38 41 0F B6 01 4C 8B 09"},
        {0xa8760,"40 53 48 83 EC 40 8B 84 24 80 00 00 00 48 8B D9 C6 44 24 38 01 89 44 24 30 8B 44 24"},
        {0xa9840,"48 83 EC 28 85 D2"},
        {0x11ca840,"48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ? ? ? ? 45 33 E4 48 8D 05 CF"},
        {0x11cb340,"48 89 5C 24 10 57 48 83 EC 40 48 89 11 48"},
        {0x11d21d0,"4C 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 55 53 57 41 56 48"},
        {0x11cb620,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20"},
        {0x11d0db0,"48 8B C4 55 53 57 41 57 48 8D 68 A1"},
        {0x109200,"48 89 5C 24 10 55 56 57 48 83 EC 50 48 8B 79"}};
    for(const auto& site:sites){const auto pattern=parsePattern(site.second);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+site.first),pattern.size(),pattern)!=std::vector<size_t>{0})
            throw std::runtime_error("detail diagnostic signature changed at "+hex(site.first));}
    for(const auto caller:{0x4790e,0x47933,0x47962,0x47987}){
        const auto call=baseAddress+caller-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+0x4eb50)
            throw std::runtime_error("detail reserve caller changed");
    }
}
void install(){
    validate();
    auto target=reinterpret_cast<void*>(baseAddress+0x4eb50);
    const auto created=MH_CreateHook(target,reinterpret_cast<void*>(&reserve),reinterpret_cast<void**>(&originalReserve));
    if(created!=MH_OK)throw std::runtime_error("detail reserve hook create failed");
    const auto enabled=MH_EnableHook(target);
    if(enabled!=MH_OK)throw std::runtime_error("detail reserve hook enable failed");
    reserveEnabled=true;
}
void disable() noexcept {
    if(factoryTraceEnabled){
        const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0x477c0));
        factoryTraceEnabled=status!=MH_OK;
        try{logFile->event("detail_factory_trace_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
    }
    if(allocatorTraceEnabled){
        const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0x8b0b30));
        allocatorTraceEnabled=status!=MH_OK;
        try{logFile->event("detail_allocator_trace_hook_removed","\"status\":"+std::to_string(status));}catch(...){}
    }
    if(!reserveEnabled)return;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0x4eb50));
    reserveEnabled=status!=MH_OK;
    try{logFile->event("detail_reserve_hook_removed","\"status\":"+std::to_string(status)+",\"trampoline_retained\":true");}catch(...){}
}
// Call from the bounded render callback after the original game's call returns.
// This never calls View feature/resolution setup or draws a private View.
void run(uintptr_t view){
    if(!reserveEnabled||current)throw std::runtime_error("detail diagnostic hook/scope state invalid");
    const auto scene=read<uintptr_t>(view+8),renderer=read<uintptr_t>(scene+8),wrapper=read<uintptr_t>(view);
    if(read<uintptr_t>(scene+layout::sceneView)!=view||read<uintptr_t>(wrapper+0x70)!=scene||read<uintptr_t>(wrapper+0x78)!=view)
        throw std::runtime_error("detail diagnostic original view ownership mismatch");
    using Wait=bool(__fastcall*)(void*,int64_t);
    if(!reinterpret_cast<Wait>(baseAddress+0x109200)(reinterpret_cast<void*>(scene),0))
        throw std::runtime_error("scene busy; no detail allocation attempted");
    const auto manager=read<uintptr_t>(renderer+0x14398),resolution=read<uintptr_t>(wrapper+0xc0);
    const auto originalDetail=read<uintptr_t>(resolution);
    const auto managerBefore=read<std::array<uintptr_t,5>>(manager);
    const auto detailBefore=read<std::array<uint8_t,0x210>>(originalDetail);
    const auto resolutionBefore=read<std::array<uint8_t,24>>(resolution);
    const auto allocator=managerBefore[0],vtable=read<uintptr_t>(allocator);
    const auto allocateAddress=read<uintptr_t>(vtable+8),freeAddress=read<uintptr_t>(vtable+0x10);
    const auto counterPool=managerBefore[4],counterTable=read<uintptr_t>(counterPool+0x48);
    const auto freeBefore=read<uint64_t>(counterPool+0x60),freeCapacity=read<uint64_t>(counterPool+0x68);
    if(freeBefore<6||freeBefore>freeCapacity||freeCapacity>65536||!counterTable)
        throw std::runtime_error("detail counter pool lacks bounded capacity for two objects");
    std::array<uintptr_t,3> originalCounterEntries{};
    std::array<std::array<uint8_t,64>,3> originalCounters{};
    for(size_t i=0;i<3;++i){const auto handle=read<uint32_t>(originalDetail+8+i*4);
        if(!handle)throw std::runtime_error("original detail counter missing");
        originalCounterEntries[i]=counterTable+uintptr_t(handle&0xffff)*64;
        originalCounters[i]=read<std::array<uint8_t,64>>(originalCounterEntries[i]);
        if(read<uint16_t>(originalCounterEntries[i]+0x38)!=uint16_t((handle>>16)-1))
            throw std::runtime_error("original detail counter generation mismatch");
    }
    const auto freeStack=read<uintptr_t>(counterPool+0x58);
    const auto nextCounters=read<std::array<uint16_t,6>>(freeStack+(freeBefore-6)*2);
    for(size_t i=0;i<nextCounters.size();++i){
        const auto entry=counterTable+uintptr_t(nextCounters[i])*64;
        if(read<uint8_t>(entry+0x3a)!=0||std::find(originalCounterEntries.begin(),originalCounterEntries.end(),entry)!=originalCounterEntries.end()||
           std::find(nextCounters.begin(),nextCounters.begin()+i,nextCounters[i])!=nextCounters.begin()+i)
            throw std::runtime_error("detail free-counter entries active, duplicated or aliased to original");
    }
    const auto pool=read<uint32_t>(renderer+0x19c);
    if(!pool)throw std::runtime_error("detail's original transient GPU pool missing");
    logFile->event("detail_factory_probe_begin","\"scene\":"+hex(scene)+",\"manager\":"+hex(manager)+
        ",\"original_detail\":"+hex(originalDetail)+",\"allocator\":"+hex(allocator)+",\"allocator_vtable\":"+hex(vtable)+
        ",\"allocate_function\":"+hex(allocateAddress)+",\"free_function\":"+hex(freeAddress)+
        ",\"allocate_rva\":"+(allocateAddress>=baseAddress&&allocateAddress<baseAddress+evr::gameBuild::sizeOfImage?hex(allocateAddress-baseAddress):"null")+
        ",\"free_rva\":"+(freeAddress>=baseAddress&&freeAddress<baseAddress+evr::gameBuild::sizeOfImage?hex(freeAddress-baseAddress):"null")+
        ",\"counter_free_before\":"+std::to_string(freeBefore)+",\"gpu_pool\":"+std::to_string(pool)+
        ",\"drawn\":false,\"private_descriptor_bytes\":40,\"native_tag8_zero_required\":true,\"preallocate_body_count\":2");
    std::array<Body,2> bodies{};
    void* forwardingVtable[]{nullptr,reinterpret_cast<void*>(&allocate),reinterpret_cast<void*>(&free)};
    AllocatorFacade facade{forwardingVtable,allocator,reinterpret_cast<Allocate>(allocateAddress),
        reinterpret_cast<Free>(freeAddress),&bodies};
    alignas(16) auto privateManager=managerBefore;
    privateManager[0]=reinterpret_cast<uintptr_t>(&facade);
    std::array<uintptr_t,2> objects{};
    struct BuilderStorage{alignas(16) std::array<uint8_t,0x200> bytes{};std::array<uint8_t,32> guard{};} builder;
    builder.guard.fill(0xa5);
    alignas(16) std::array<uint8_t,32> gpuAllocator{};
    bool builderInitialized=false;uint32_t group=0;unsigned failures=0;
    auto cleanup=[&](){
        for(size_t i=2;i-->0;)if(objects[i]){
            reinterpret_cast<Dispose>(baseAddress+0x49400)(privateManager.data(),reinterpret_cast<void*>(objects[i]));objects[i]=0;
        }
        if(builderInitialized){reinterpret_cast<void(__fastcall*)(void*)>(baseAddress+0x11cb620)(builder.bytes.data());builderInitialized=false;}
        if(group)reinterpret_cast<void(__fastcall*)(void*,uint32_t*)>(baseAddress+0x11d0db0)(
            reinterpret_cast<void*>(read<uintptr_t>(renderer+0x50)),&group);
        for(auto& body:bodies)if(body.address&&!body.freed){
            uint8_t tag=0;facade.free(reinterpret_cast<void*>(allocator),reinterpret_cast<void*>(body.address),&tag);body.freed=true;
        }
    };
    try{
        // Original factory does not safely handle its initial allocation returning
        // NULL. Make both exact requests before any factory/counter/GPU work.
        for(size_t i=0;i<2;++i){uint8_t tag=8;
            auto pointer=facade.allocate(reinterpret_cast<void*>(allocator),0x210,8,&tag,0);
            const auto returnedError=GetLastError();const auto address=reinterpret_cast<uintptr_t>(pointer);
            if(!address)throw std::runtime_error("native detail body allocation returned NULL before factory");
            const auto overlaps=[&](uintptr_t other,size_t length){return address<=UINTPTR_MAX-0x210&&other<=UINTPTR_MAX-length&&
                address<other+length&&other<address+0x210;};
            if(address>UINTPTR_MAX-0x210||overlaps(originalDetail,0x210)||overlaps(resolution,24)||overlaps(manager,40)||
               overlaps(scene,0xa00)||overlaps(view,0xbc0)||overlaps(wrapper,0xd0)||overlaps(allocator,16)||
               overlaps(counterPool,0xb0)||(i&&overlaps(bodies[0].address,0x210)))
                throw std::runtime_error("native detail allocation overlaps existing owner; returned alias not freed");
            bodies[i].address=address;bodies[i].returnedTag=tag;bodies[i].returnedError=returnedError;
            const auto raw=read<std::array<uint8_t,0x210>>(address);
            bodies[i].nativeZero=std::all_of(raw.begin(),raw.end(),[](uint8_t b){return b==0;});
            // Actual allocator7e3840 honors tag bit8 by zeroing requested bytes.
            // Enforce that native contract before the original factory starts;
            // never compensate for a changed allocator by modifying the object.
            if(!bodies[i].nativeZero)throw std::runtime_error("native tag8 zero-initialization contract failed before factory");
            logFile->event("detail_owned_body_preallocated","\"eye\":"+std::to_string(i)+",\"object\":"+hex(address)+
                ",\"size\":528,\"alignment\":8,\"input_tag\":8,\"output_tag\":"+std::to_string(tag)+
                ",\"fifth_argument\":0,\"native_was_zero\":"+(bodies[i].nativeZero?"true":"false")+
                ",\"diagnostic_zeroing\":false");
        }
        using BuilderInit=void*(__fastcall*)(void*,void*,uint8_t);
        reinterpret_cast<BuilderInit>(baseAddress+0x11ca840)(builder.bytes.data(),reinterpret_cast<void*>(read<uintptr_t>(renderer+0x14a70)),read<uint8_t>(wrapper+8));
        builderInitialized=true;
        using PoolInit=void*(__fastcall*)(void*,void*,uint32_t,uintptr_t,uint8_t);
        reinterpret_cast<PoolInit>(baseAddress+0x11cb340)(gpuAllocator.data(),builder.bytes.data(),pool,0,1);
        std::array<void*,1> context{gpuAllocator.data()};
        for(size_t i=0;i<2;++i){
            facade.eye=i;Scope scope;scope.body=bodies[i].address;scope.eye=i;
            uintptr_t returned=0;
            {ScopeLease lease(scope);returned=reinterpret_cast<uintptr_t>(reinterpret_cast<Factory>(baseAddress+0x477c0)(privateManager.data(),context.data()));}
            if(returned&&returned!=bodies[i].address)throw std::runtime_error("detail factory returned foreign object; foreign destructor refused");
            objects[i]=returned;
            logFile->event("detail_factory_return","\"eye\":"+std::to_string(i)+",\"object\":"+hex(objects[i])+
                ",\"reserve_observations\":"+std::to_string(scope.observations)+",\"failed_reserves\":"+std::to_string(scope.failedReserves)+
                ",\"body_consumed\":"+(bodies[i].consumed?"true":"false")+",\"body_freed\":"+(bodies[i].freed?"true":"false"));
            if(!bodies[i].consumed||(objects[i]&&objects[i]!=bodies[i].address)||
               (!objects[i]&&!bodies[i].freed)||(objects[i]&&bodies[i].freed))
                throw std::runtime_error("detail factory owned-body contract mismatch");
            if(!objects[i])++failures;
        }
        std::array<uint8_t,32> status{};status[0]=0xff;
        using Commit=void*(__fastcall*)(void*,void*,uint32_t*,void*);
        reinterpret_cast<Commit>(baseAddress+0x11d21d0)(builder.bytes.data(),status.data(),&group,reinterpret_cast<void*>(read<uintptr_t>(renderer+0x50)));
        logFile->event("detail_factory_group_commit","\"status\":"+std::to_string(status[0])+",\"group\":"+std::to_string(group));
        if(status[0])throw std::runtime_error("detail diagnostic GPU commit failed");
        if(!std::all_of(builder.guard.begin(),builder.guard.end(),[](uint8_t b){return b==0xa5;}))
            throw std::runtime_error("detail diagnostic builder guard changed");
        cleanup();
        bool originalCountersUnchanged=true;
        for(size_t i=0;i<3;++i)originalCountersUnchanged&=read<std::array<uint8_t,64>>(originalCounterEntries[i])==originalCounters[i];
        const auto freeAfter=read<uint64_t>(counterPool+0x60);
        const bool unchanged=read<std::array<uintptr_t,5>>(manager)==managerBefore&&
            read<std::array<uint8_t,0x210>>(originalDetail)==detailBefore&&read<std::array<uint8_t,24>>(resolution)==resolutionBefore&&
            read<uintptr_t>(renderer+0x14398)==manager&&read<uintptr_t>(wrapper+0xc0)==resolution&&read<uintptr_t>(scene+layout::sceneView)==view;
        logFile->event("detail_factory_probe_complete","\"factory_failures\":"+std::to_string(failures)+
            ",\"original_owners_unchanged\":"+(unchanged?"true":"false")+",\"original_counters_unchanged\":"+(originalCountersUnchanged?"true":"false")+
            ",\"counter_free_after\":"+std::to_string(freeAfter)+",\"owned_group_released\":"+(!group?"true":"false")+
            ",\"allocator_contract_violation\":"+(facade.contractViolation?"true":"false")+",\"drawn\":false");
        const bool forwardedReleased=std::none_of(facade.forwarded.begin(),facade.forwarded.end(),[](const auto& a){return a.active;});
        if(!unchanged||!originalCountersUnchanged||freeAfter!=freeBefore||group||facade.contractViolation||!forwardedReleased)
            throw std::runtime_error("detail diagnostic ownership postcheck failed");
        if(failures)throw std::runtime_error("native detail array reserve failed; isolated cleanup completed");
    }catch(...){cleanup();throw;}
}
} // namespace detailFactoryDiagnostic
void validateDetailFactoryProbe(){detailFactoryDiagnostic::validate();}
void installDetailFactoryReserveHook(){detailFactoryDiagnostic::install();}
void installNativeDetailFactoryTrace(){
    detailFactoryDiagnostic::install();
    const auto call=baseAddress+0xf260d;
    if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+0x477c0)
        throw std::runtime_error("native detail factory caller changed");
    auto target=reinterpret_cast<void*>(baseAddress+0x477c0);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&detailFactoryDiagnostic::traceFactory),
        reinterpret_cast<void**>(&detailFactoryDiagnostic::originalTracedFactory))!=MH_OK||MH_EnableHook(target)!=MH_OK)
        throw std::runtime_error("native detail factory trace hook failed");
    detailFactoryDiagnostic::factoryTraceEnabled=true;
    const auto pattern=parsePattern("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 20 4C 89 44 24 18 57 41 56 41 57 48 83 EC 60 48 8D");
    auto allocatorTarget=reinterpret_cast<void*>(baseAddress+0x8b0b30);
    if(scan(reinterpret_cast<const uint8_t*>(allocatorTarget),pattern.size(),pattern)!=std::vector<size_t>{0})
        throw std::runtime_error("native allocator trace signature changed");
    if(MH_CreateHook(allocatorTarget,reinterpret_cast<void*>(&detailFactoryDiagnostic::traceAllocateResult),
        reinterpret_cast<void**>(&detailFactoryDiagnostic::originalAllocateResult))!=MH_OK||MH_EnableHook(allocatorTarget)!=MH_OK)
        throw std::runtime_error("native allocator trace hook failed");
    detailFactoryDiagnostic::allocatorTraceEnabled=true;
}
void disableDetailFactoryReserveHook() noexcept {detailFactoryDiagnostic::disable();}
void runUnusedDetailFactoryProbe(uintptr_t view){detailFactoryDiagnostic::run(view);}
