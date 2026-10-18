// Completed-render-boundary witness; no native writes.
namespace heldVfxBridge {
inline uint64_t observed=0;
inline void observe(){
    if(!heldVfxObserver::requested||!motionVisual::poseReady||!motionVisual::contextReady||motionVisual::updatedFrame!=liveFrame.frame||liveFrame.frame%15)return;
    const auto& held=motionVisual::hiddenSource;
    if(!held.entity||!held.weaponCategoryKnown||held.weaponType!=3||held.entity!=motionVisual::child)return;
    const auto s=heldVfxObserver::snapshot(held.entity);const auto now=GetTickCount64();
    if(!s.at||now<s.at||now-s.at>100||s.scene!=motionVisual::scene||s.item!=held.item||s.attachment!=held.entity||s.raw[49]||s.raw[50])return;
    const auto source=motionVisual::object(motionVisual::sourceHandle);if(read<uint32_t>(source+layout::modelPose))return;
    const auto record=heldVfxObserver::effect(s.controller,s.key);if(!record||read<uint32_t>(record+0x180)!=s.handle||read<std::array<uint8_t,52>>(s.component)!=s.raw)return;
    const auto emitter=heldVfxObserver::emitter(s.scene,s.handle);if(!emitter)return;
    const auto native=read<std::array<uint8_t,56>>(record+0x60),render=read<std::array<uint8_t,56>>(emitter+0x40);
    logFile->event("held_vfx_render_witness","\"frame\":"+std::to_string(liveFrame.frame)+",\"entity\":"+std::to_string(held.entity)+",\"item\":"+std::to_string(held.item)+
        ",\"key\":"+std::to_string(s.key)+",\"handle\":"+std::to_string(s.handle)+",\"source_model\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,56>>(source+0x38)))+
        ",\"clone_model\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,56>>(motionVisual::object(motionVisual::handle)+0x38)))+
        ",\"native_effect\":"+jsonString(nativeImpactComponents::asHex(native))+",\"render_effect\":"+jsonString(nativeImpactComponents::asHex(render))+
        ",\"offset_component\":"+jsonString(nativeImpactComponents::asHex(s.raw))+",\"resource\":"+hex(read<uintptr_t>(emitter+0xf0))+",\"age_ms\":"+std::to_string(now-s.at)+",\"read_only\":true");++observed;
}

inline bool requested=false,validated=false;
inline uint64_t moved=0,restored=0,rejected=0,failures=0;
inline void validate(){
 if(validated)return;
 for(const auto& e:std::array<std::pair<uintptr_t,const char*>,2>{{
    {0xf5330,"85 d2 ? ? 48 8b 81 98 00 00 00 48 8b 88 50 09 00 00 48 85 c9 0f 85 15 c9"},
    {0x17dc00,"4c 8b d1 85 d2 0f 84 11 01 00 00 48 8b 49 68 44 0f b7 ca 41 8b c1 48 d3 e8 49 8b"}}}){
    const auto p=parsePattern(e.second);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+e.first),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("held VFX update signature differs");
    logFile->event("held_vfx_mount_signature","\"rva\":"+hex(e.first)+",\"matched\":true");
 }validated=true;
}
using Update=void(__fastcall*)(void*,uint32_t,const void*);
inline void change(uintptr_t scene,uint32_t handle,const std::array<uint8_t,56>& pose){reinterpret_cast<Update>(baseAddress+0xf5330)(reinterpret_cast<void*>(scene),handle,pose.data());}
struct Scope {
 heldVfxObserver::Snapshot sample{};uintptr_t emitter=0,resource=0,record=0,source=0;bool active=false;
 std::array<uint8_t,56> before{},target{},sourceBefore{},nativeBefore{};
 void begin(){
    if(!requested||!motionVisual::contextReady||!motionVisual::poseReady||motionVisual::updatedFrame!=liveFrame.frame)return;
    const auto& held=motionVisual::hiddenSource;
    if(!held.entity||held.entity!=motionVisual::child||!held.weaponCategoryKnown||held.weaponType!=3||held.itemSlot!=1||held.slot!=8)return;
    sample=heldVfxObserver::snapshot(held.entity);const auto now=GetTickCount64();
    if(!sample.at||now<sample.at||now-sample.at>100||sample.scene!=motionVisual::scene||sample.item!=held.item||sample.attachment!=held.entity||
       sample.typeId<0||sample.raw[48]!=8||sample.raw[49]||sample.raw[50]||sample.data[0x9c]!=8)return;
    // A completed same-frame weapon preparation already drained native CPU tasks.
    if(!motionVisual::contextReady||motionVisual::contextFrame!=liveFrame.frame||sample.scene!=provenWorldScene||GetCurrentThreadId()!=motionVisual::thread)
        throw std::runtime_error("held VFX mutation boundary differs");
    const auto owner=nativeImpactComponents::entityAtWorld(sample.world,held.entity);
    if(nativeImpactComponents::component(owner,unsigned(sample.typeId),52)!=sample.component||read<std::array<uint8_t,52>>(sample.component)!=sample.raw)return;
    record=heldVfxObserver::effect(sample.controller,sample.key);
    if(!record||read<uint32_t>(record+0x180)!=sample.handle||read<uint32_t>(record+0x98)!=held.entity||
       read<std::array<uint8_t,16>>(record+0x170)!=read<std::array<uint8_t,16>>(reinterpret_cast<uintptr_t>(sample.data.data())+0x170))return;
    source=motionVisual::object(motionVisual::sourceHandle);
    if(read<uint32_t>(source+layout::modelPose)||read<uintptr_t>(source+layout::modelResource)!=motionVisual::resource)return;
    sourceBefore=read<std::array<uint8_t,56>>(source+0x38);nativeBefore=read<std::array<uint8_t,56>>(record+0x60);
    motion::EffectTransform model{},native{};motion::EffectOffset offset{};
    std::memcpy(&model,sourceBefore.data(),56);std::memcpy(&native,nativeBefore.data(),56);std::memcpy(&offset,sample.raw.data()+8,40);
    const auto witness=motion::mountHeldEffect(model,offset);const double error=motion::effectPositionError(witness,native);
    if(error>0.0002||motion::effectRotationError(witness.orientation,native.orientation)>0.0002f){
        if(++rejected<=4)logFile->event("held_vfx_mount_deferred","\"reason\":\"source_offset_witness_differs\",\"error_m\":"+std::to_string(error));return;
    }
    emitter=heldVfxObserver::emitter(sample.scene,sample.handle);if(!emitter)return;
    resource=read<uintptr_t>(emitter+0xf0);if(!resource)return;
    before=read<std::array<uint8_t,56>>(emitter+0x40);
    const auto clone=read<motion::EffectTransform>(motionVisual::object(motionVisual::handle)+0x38);
    auto mounted=motion::mountHeldEffect(clone,offset);
    // The effect owns its particle/light scale. Only its attachment pose follows the model.
    std::memcpy(&mounted.scale,before.data()+40,12);std::memcpy(&mounted.padding,before.data()+52,4);std::memcpy(target.data(),&mounted,56);
    validate();active=true;change(sample.scene,sample.handle,target);
    if(read<std::array<uint8_t,56>>(emitter+0x40)!=target)throw std::runtime_error("held VFX mount write not observed");
    ++moved;logFile->event("held_vfx_mounted","\"frame\":"+std::to_string(liveFrame.frame)+",\"entity\":"+std::to_string(held.entity)+",\"item\":"+std::to_string(held.item)+
        ",\"effect_key\":"+std::to_string(sample.key)+",\"handle\":"+std::to_string(sample.handle)+",\"before\":"+jsonString(nativeImpactComponents::asHex(before))+
        ",\"target\":"+jsonString(nativeImpactComponents::asHex(target))+",\"clone\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,56>>(motionVisual::object(motionVisual::handle)+0x38)))+
        ",\"offset_component\":"+jsonString(nativeImpactComponents::asHex(sample.raw))+",\"native_offset_error_m\":"+std::to_string(error)+",\"native_entity_unchanged\":true,\"cpu_complete\":true");
 }
 bool restore()noexcept{
    if(!active)return true;
    try{
        // 1.0 (2026-10-16): the View's streaming can stay busy for frames while the world streams in; the mount is put
        // back all the same (cpu_complete false in the record) instead of ending the session.
        const bool cpuComplete=finishEyeCpuTasks(sample.scene,4);
        if(heldVfxObserver::emitter(sample.scene,sample.handle)!=emitter||read<uintptr_t>(emitter+0xf0)!=resource||heldVfxObserver::effect(sample.controller,sample.key)!=record||read<uint32_t>(record+0x180)!=sample.handle)
            throw std::runtime_error("held VFX restore ownership changed");
        const auto after=read<std::array<uint8_t,56>>(emitter+0x40);const bool targetStable=after==target;
        if(!targetStable)logFile->event("held_vfx_scope_change","\"frame\":"+std::to_string(liveFrame.frame)+",\"handle\":"+std::to_string(sample.handle)+",\"target\":"+jsonString(nativeImpactComponents::asHex(target))+",\"after\":"+jsonString(nativeImpactComponents::asHex(after))+",\"native\":"+jsonString(nativeImpactComponents::asHex(nativeBefore)));
        const bool nativeStable=read<std::array<uint8_t,56>>(source+0x38)==sourceBefore&&read<std::array<uint8_t,56>>(record+0x60)==nativeBefore&&read<std::array<uint8_t,52>>(sample.component)==sample.raw;
        change(sample.scene,sample.handle,before);active=false;
        if(read<std::array<uint8_t,56>>(emitter+0x40)!=before)throw std::runtime_error("held VFX restore differs");
        ++restored;logFile->event("held_vfx_mount_restored","\"frame\":"+std::to_string(liveFrame.frame)+",\"handle\":"+std::to_string(sample.handle)+
            ",\"target_stable\":"+(targetStable?"true":"false")+",\"native_unchanged\":"+(nativeStable?"true":"false")+",\"restored\":true,\"cpu_complete\":"+(cpuComplete?"true":"false"));
        if(!targetStable||!nativeStable)throw std::runtime_error("held VFX changed inside render scope");return true;
    }catch(const std::exception& e){active=false;++failures;motionVisual::failed=true;logFile->event("held_vfx_mount_failure","\"reason\":"+jsonString(e.what()));return false;}
 }
 ~Scope(){restore();}
 Scope()=default;Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline void report(){if(requested)logFile->event("held_vfx_mount_summary","\"moved\":"+std::to_string(moved)+",\"restored\":"+std::to_string(restored)+",\"deferred\":"+std::to_string(rejected)+",\"failures\":"+std::to_string(failures));}
}
