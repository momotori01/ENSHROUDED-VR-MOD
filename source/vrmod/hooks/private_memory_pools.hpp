// Build23966345 native GPU pool ownership. Included after read()/hex().
// A memory group does not reserve disjoint offsets in another owner's pool.
namespace privateMemoryPools {
// [eye][0] feature (wrapper+b0; +88 before 1.0), [1] persistent resolution (wrapper+b4; +8c),
// [2] transient resolution (renderer+27c; +19c, opt-in privateTransient; 0 when shared). 1.0: the renderer is
// scene+18 (+8), its memory manager renderer+160 (+50), the fallback pool renderer+278 (+198); the table is as before.
using EyePools=std::array<std::array<uint32_t,3>,2>;
inline bool privateTransient=false; // EVR_EYE_PRIVATE_TRANSIENT_1
uintptr_t record(uintptr_t manager,uint32_t id){
    if(!id)throw std::runtime_error("native memory pool ID is zero");
    // 1.0: the slot table is manager+0x20 (+0x18 before; 11ceb00 against f9f1e0), its layout unchanged (11d09b0 = fa0ed0).
    const auto shift=read<uint64_t>(manager+0x58),mask=read<uint64_t>(manager+0x68);
    if(shift>16||mask>0xffff)throw std::runtime_error("native memory pool table format changed");
    const uint64_t slot=id&0xffffu,page=slot>>shift,column=slot&mask;
    if(page>=read<uint64_t>(manager+0x38))throw std::runtime_error("native memory pool page outside table");
    const auto chunk=read<uintptr_t>(manager+0x30)+page*0x40;
    if(column>=read<uint64_t>(chunk+0x10))throw std::runtime_error("native memory pool column outside page");
    const auto result=read<uintptr_t>(chunk+8)+column*0x40;
    if(read<uint32_t>(result)!=(id>>16))throw std::runtime_error("native memory pool generation/free state differs");
    // fa0ed0 returns slot+8; the leading eight bytes are the table header.
    return result+8;
}
void create(uintptr_t scene,uintptr_t wrapper,unsigned eye,bool resolution,EyePools& pools){
    const auto renderer=read<uintptr_t>(scene+layout::sceneRenderer),manager=read<uintptr_t>(renderer+layout::rendererMemory);
    using Create=uint32_t (__fastcall *)(void*,const void*);
    // c02a0 (build 23966345): feature builder pool wrapper+88, persistent resolution wrapper+8c
    // (fallback renderer+198), transient resolution builder renderer+19c (c09af, f8aff0 flag 1).
    // The transient pool is renderer-wide, so the original View and both eyes otherwise share it.
    const unsigned kinds=resolution?(privateTransient?3u:2u):1u;
    for(unsigned kind=0;kind<kinds;++kind){
        auto source=kind<2?read<uint32_t>(wrapper+layout::ownerPools+8+kind*4):read<uint32_t>(renderer+0x27c);
        if(!source)source=read<uint32_t>(renderer+0x278);
        const auto sourceRecord=record(manager,source);
        auto descriptor=read<std::array<uint8_t,32>>(sourceRecord);
        // +0 is not an input to f9f1e0. The native configuration is
        // payload+4/+8/+c/+10/+18; preserve those runtime values.
        std::fill_n(descriptor.data(),4,uint8_t(0));
        const auto created=reinterpret_cast<Create>(baseAddress+0x11d1b10)(reinterpret_cast<void*>(manager),descriptor.data());
        if(!created||created==source)throw std::runtime_error("private native memory pool allocation failed/aliased");
        for(const auto& owned:pools)for(auto other:owned)if(other&&other==created)
            throw std::runtime_error("private memory pool aliases an already owned pool");
        pools[eye][kind]=created; // Own before subsequent validation can throw.
        const auto newRecord=record(manager,created);
        for(const auto field:{std::pair<size_t,size_t>{4,4},{8,4},{0xc,1},{0x10,4},{0x18,8}}){
            uint64_t expected=0,actual=0;
            std::memcpy(&expected,descriptor.data()+field.first,field.second);
            for(size_t n=0;n<field.second;n++)actual|=uint64_t(read<uint8_t>(newRecord+field.first+n))<<(n*8);
            logFile->event("private_native_memory_pool_field","\"eye\":"+std::to_string(eye)+
                ",\"kind\":"+std::to_string(kind)+",\"pool\":"+std::to_string(created)+
                ",\"offset\":"+std::to_string(field.first)+",\"expected\":"+std::to_string(expected)+",\"actual\":"+std::to_string(actual));
        }
        for(const auto field:{std::pair<size_t,size_t>{4,4},{8,4},{0xc,1},{0x10,4},{0x18,8}})
            for(size_t n=0;n<field.second;n++)if(read<uint8_t>(newRecord+field.first+n)!=descriptor[field.first+n])
                throw std::runtime_error("private native memory pool configuration differs");
        logFile->event("private_native_memory_pool_created","\"eye\":"+std::to_string(eye)+
            ",\"kind\":"+jsonString(kind==2?"transient":kind?"resolution":"feature")+",\"source_pool\":"+std::to_string(source)+
            ",\"pool\":"+std::to_string(created)+",\"block_bytes\":"+std::to_string(read<uint64_t>(newRecord+0x18))+
            ",\"configuration_copied_from_native\":true");
    }
}
void release(uintptr_t scene,EyePools& pools){
    const auto manager=read<uintptr_t>(read<uintptr_t>(scene+layout::sceneRenderer)+layout::rendererMemory);
    using Release=void (__fastcall *)(void*,uint32_t*);
    // Call only after both Views, resolution groups and feature groups are gone.
    for(int eye=1;eye>=0;--eye)for(int kind=2;kind>=0;--kind)if(pools[eye][kind]){
        const auto id=pools[eye][kind];record(manager,id);
        reinterpret_cast<Release>(baseAddress+0x11d2bb0)(reinterpret_cast<void*>(manager),&pools[eye][kind]);
        if(pools[eye][kind])throw std::runtime_error("private native memory pool release did not clear ID");
        logFile->event("private_native_memory_pool_released","\"eye\":"+std::to_string(eye)+
            ",\"kind\":"+jsonString(kind==2?"transient":kind?"resolution":"feature")+",\"pool\":"+std::to_string(id));
    }
}
// Points the renderer-wide transient resolution pool at the eye's private pool only while that
// eye's c02a0 runs on the render thread, then restores the original ID (also on exceptions).
struct TransientScope {
    uintptr_t renderer=0;uint32_t original=0,replacement=0;bool active=false;
    TransientScope(uintptr_t scene,uint32_t pool,unsigned eye){
        if(!privateTransient||!pool)return;
        renderer=read<uintptr_t>(scene+layout::sceneRenderer);original=read<uint32_t>(renderer+0x27c);
        if(!original)throw std::runtime_error("native transient resolution pool missing");
        replacement=pool;*reinterpret_cast<volatile uint32_t*>(renderer+0x27c)=pool;active=true;
        logFile->event("private_transient_pool_scope","\"eye\":"+std::to_string(eye)+",\"original\":"+std::to_string(original)+",\"private\":"+std::to_string(pool)+",\"open\":true");
    }
    ~TransientScope(){
        if(!active)return;
        const auto current=read<uint32_t>(renderer+0x27c);
        *reinterpret_cast<volatile uint32_t*>(renderer+0x27c)=original;
        try{logFile->event("private_transient_pool_scope","\"original\":"+std::to_string(original)+",\"private\":"+std::to_string(replacement)+
            ",\"open\":false,\"unchanged_during_scope\":"+(current==replacement?"true":"false"));}catch(...){}
    }
};
}
