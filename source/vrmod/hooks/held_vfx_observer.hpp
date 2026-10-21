// Opt-in read-only evidence at the completed update_vfx query. No ECS writes.
namespace heldVfxObserver {
inline bool requested=false;
inline std::atomic<unsigned> samples{0},failures{0};
inline std::mutex mutex;
struct Seen {uint32_t entity=0,key=0;uint64_t at=0;};
inline std::array<Seen,64> seen{};
struct Snapshot {
    uint32_t entity=0,item=0,key=0,handle=0,attachment=0;uint64_t at=0;
    uintptr_t scene=0,world=0,controller=0,component=0;
    std::array<uint8_t,52> raw{};std::array<uint8_t,0x1c8> data{};int typeId=-1;
};
inline std::array<Snapshot,64> latest{};
inline Snapshot snapshot(uint32_t child){std::lock_guard<std::mutex> lock(mutex);for(const auto& s:latest)if(s.entity==child)return s;return {};}
inline uintptr_t emitter(uintptr_t scene,uint32_t id){
    const auto vfx=read<uintptr_t>(scene+layout::sceneVfx);if(!vfx||!id)return 0;   // 1.0: the pool fields below are not re-checked yet (bounds-checked)
    const auto shift=read<uint64_t>(vfx+0x68),mask=read<uint64_t>(vfx+0x78);
    const auto pages=read<uintptr_t>(vfx+0x40),count=read<uint64_t>(vfx+0x48);
    if(shift>16||mask>0xffff||count>65536||!pages)throw std::runtime_error("held VFX render pool bounds");
    const uint64_t index=id&0xffff,page=index>>shift,within=index&mask;if(page>=count)return 0;
    const auto block=pages+page*64,n=read<uint64_t>(block+0x10),data=read<uintptr_t>(block+8);
    if(n>65536||!data)throw std::runtime_error("held VFX render page bounds");if(within>=n)return 0;
    const auto entry=data+within*0x118;const auto generation=read<uint32_t>(entry);
    if((generation&0x80000000u)||(generation&0x7fffffffu)!=(id>>16))return 0;return entry+8;
}

inline uintptr_t effect(uintptr_t controller,uint32_t key){
    if(!key)return 0;
    const auto pool=controller+0x90;
    const auto shift=read<uint64_t>(pool+0x38),mask=read<uint64_t>(pool+0x48);
    const auto pages=read<uintptr_t>(pool+0x10),count=read<uint64_t>(pool+0x18);
    if(shift>24||mask>0xffffff||count>65536||!pages)throw std::runtime_error("held VFX pool bounds");
    const uint64_t index=key&0xffffffu,page=index>>shift,within=index&mask;
    if(page>=count)return 0;
    const auto block=pages+page*64,n=read<uint64_t>(block+0x10),data=read<uintptr_t>(block+8);
    if(n>0x1000000||!data)throw std::runtime_error("held VFX page bounds");
    if(within>=n)return 0;
    const auto entry=data+within*0x1d0;const auto generation=read<uint32_t>(entry);
    if((generation&0x80000000u)||(generation&0x7fffffffu)!=(key>>24))return 0;
    return entry+8;
}
inline void capture(uintptr_t caller,uintptr_t sp,void* context,void* query,uint32_t bytes){
    if(!requested||caller!=0x2b06ba)return;
    try{
        rootCamera::Sample local;if(!rootCamera::trySnapshot(local)||!local.entity)return;
        if(bytes!=0x68||reinterpret_cast<uintptr_t>(query)!=sp+0x30||read<uintptr_t>(sp+0x30)!=reinterpret_cast<uintptr_t>(context))
            throw std::runtime_error("held VFX final query ABI differs");
        const auto entity=attachmentObserver::entityFor(context);
        const auto ref=read<uintptr_t>(sp+0x78);
        const auto weapon=ref?read<std::array<uint32_t,2>>(ref):std::array<uint32_t,2>{};
        attachmentObserver::Child held;
        {std::lock_guard<std::mutex> lock(attachmentObserver::mutex);
         for(const auto& c:attachmentObserver::children)if(c.parent==local.entity&&(c.entity==entity||c.entity==weapon[0])){held=c;break;}}
        if(entity!=local.entity&&weapon[1]!=local.entity&&!held.entity)return;
        const auto component=read<uintptr_t>(sp+0x80),controller=read<uintptr_t>(sp+0x90);
        if(!component||!controller)throw std::runtime_error("held VFX component/controller missing");
        const auto raw=read<std::array<uint8_t,52>>(component);
        uint32_t key=0;std::memcpy(&key,raw.data(),4);if(!key)return;
        const auto now=GetTickCount64();
        if(!held.entity){std::lock_guard<std::mutex> lock(mutex);if(samples.load()>=256)return;for(const auto& x:seen)if(x.entity==entity&&x.key==key&&now>=x.at&&now-x.at<1000)return;}
        const auto wrapper=read<uintptr_t>(controller+8),scene=wrapper?read<uintptr_t>(wrapper+0x70):0;
        if(!provenWorldScene||scene!=provenWorldScene)return;
        const auto state=read<uintptr_t>(reinterpret_cast<uintptr_t>(context));
        const auto queryWorld=read<uintptr_t>(state+0x30),ecsRoot=read<uintptr_t>(controller+0x20);
        // Native 863800 uses root+cc4360's entity map: the World member starts
        // at cc4218 (also proven by the existing World lifecycle observer).
        const uintptr_t world=ecsRoot?ecsRoot+0xcc4218:0;
        // A task may omit +30's lookup capability. Prove the controller's World
        // against this completed query's exact entity/component pointer instead.
        if(samples.load()<4)logFile->event("held_vfx_world_candidate","\"query_world\":"+hex(queryWorld)+",\"controller_world\":"+hex(world)+",\"local_world\":"+hex(local.world)+",\"entity\":"+std::to_string(entity));
        if(!world||(queryWorld&&queryWorld!=world)||(local.world&&world!=local.world))throw std::runtime_error("held VFX world differs");
        const auto owner=nativeImpactComponents::entityAtWorld(world,entity);
        const auto prior=snapshot(entity);int typeId=prior.scene==scene&&prior.world==world?prior.typeId:-1;
        if(typeId<0){typeId=nativeImpactComponents::identify(owner,component,52);
            const auto type=nativeImpactComponents::componentType(world,unsigned(typeId));
            if(type.bytes!=52||type.qualifiedName!="keen::ecs::VfxComponent")throw std::runtime_error("held VFX component type witness differs");
        }else if(nativeImpactComponents::component(owner,unsigned(typeId),52)!=component)throw std::runtime_error("held VFX cached type query pointer differs");
        const auto record=effect(controller,key);if(!record)return;
        const auto data=read<std::array<uint8_t,0x1c8>>(record);
        uint32_t attachment=0,handle=0;std::memcpy(&attachment,data.data()+0x98,4);std::memcpy(&handle,data.data()+0x180,4);
        if(raw!=read<std::array<uint8_t,52>>(component)||record!=effect(controller,key)||data!=read<std::array<uint8_t,0x1c8>>(record))
            throw std::runtime_error("held VFX observation changed");
        {std::lock_guard<std::mutex> lock(mutex);
         Snapshot* value=nullptr;for(auto& x:latest)if(x.entity==entity){value=&x;break;}
         if(!value)for(auto& x:latest)if(!x.entity){value=&x;break;}
         if(value)*value={entity,held.item,key,handle,attachment,now,scene,world,controller,component,raw,data,typeId};
         if(samples.load()>=256)return;
         Seen* entry=nullptr;for(auto& x:seen)if(x.entity==entity){entry=&x;break;}
         if(!entry)for(auto& x:seen)if(!x.entity){entry=&x;break;}
         if(!entry)return;if(entry->key==key&&now>=entry->at&&now-entry->at<1000)return;
         *entry={entity,key,now};}
        const auto asHex=[](const uint8_t* b,size_t n){static constexpr char h[]="0123456789abcdef";std::string out;for(size_t i=0;i<n;i++){out+=h[b[i]>>4];out+=h[b[i]&15];}return out;};
        logFile->event("held_vfx_observation","\"entity\":"+std::to_string(entity)+",\"local_entity\":"+std::to_string(local.entity)+
            ",\"weapon_entity\":"+std::to_string(weapon[0])+",\"weapon_owner\":"+std::to_string(weapon[1])+",\"appearance_entity\":"+std::to_string(held.entity)+
            ",\"item\":"+std::to_string(held.item)+",\"effect_key\":"+std::to_string(key)+",\"render_handle\":"+std::to_string(handle)+
            ",\"attachment_entity\":"+std::to_string(attachment)+",\"slot\":"+std::to_string(data[0x9c])+",\"scene\":"+hex(scene)+
            ",\"effect_guid\":"+jsonString(asHex(data.data()+0x170,16))+",\"transform_bytes\":"+jsonString(asHex(data.data()+0x60,56))+
            ",\"component_type_id\":"+std::to_string(typeId)+",\"world\":"+hex(world)+",\"exact_component_witness\":true"+
            ",\"component_bytes\":"+jsonString(asHex(raw.data(),raw.size()))+",\"read_only\":true,\"pool_generation_verified\":true");++samples;
    }catch(const std::exception& e){if(failures.fetch_add(1)<4)logFile->event("held_vfx_observer_failure","\"reason\":"+jsonString(e.what()));}
}
}
