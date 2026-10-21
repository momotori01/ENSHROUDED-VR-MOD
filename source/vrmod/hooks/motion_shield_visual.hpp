// Opt-in measured shield: a separate Scene-owned representation, mask/SDF0.
// Original shield/hand models and native shield gameplay remain untouched.
namespace motionShieldVisual {
inline bool requested=false,ready=false;
// Live held items: every shield of the model catalog (42), and the clone follows a change of shield.
inline bool catalogShields=false;inline unsigned retired=0;
inline uint32_t handle=0,sourceHandle=0,item=0;inline uintptr_t resource=0,scene=0;
inline uint64_t serial=0,frame=0,updates=0;inline DWORD thread=0;
inline std::array<uint64_t,2> payloadSerial{};
inline void prepare(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested)return;ready=false;
    if(!motionVisual::contextReady||motionVisual::contextSerial!=currentSerial||motionVisual::contextFrame!=liveFrame.frame||
       !motionHandVisual::ready||motionHandVisual::serial!=currentSerial||!shownHands.focused||!shownHands.hands[0].tracked())return;
    const auto root=rootCamera::snapshot();
    const auto current=attachmentObserver::shieldAppearance(root.entity,motionVisual::scene,motionVisual::view,root.world,GetTickCount64());
    if(!current.entity||!(motion::reviewedShieldMount(current.item,current.modelGuid,current.slot)||(catalogShields&&motion::catalogShieldMount(current.item,current.modelGuid,current.slot))))return;
    const auto source=motionVisual::currentObject(current.handle);if(!source)return;
    const auto res=read<uintptr_t>(source+layout::modelResource);
    if(!res||read<uint8_t>(res+0x14)!=1||read<uint16_t>(res+layout::resourceModel)!=current.model||read<std::array<uint32_t,4>>(res+24)!=current.modelGuid||
       !read<uint32_t>(source+layout::modelMask)||read<uint32_t>(source+layout::modelPose))return;
    if(scene&&(scene!=motionVisual::scene||thread!=GetCurrentThreadId()||(resource!=res&&!catalogShields)))throw std::runtime_error("shield Scene/thread/resource changed");
    if(handle&&resource!=res){
        // Another shield: the clone of the previous one goes, as at Scene disposal, and a new one is made below.
        const auto old=motionVisual::object(handle);
        if(read<uintptr_t>(old+layout::modelResource)!=resource||read<uint32_t>(old+layout::modelMask)||read<uint32_t>(old+layout::modelSdf))throw std::runtime_error("shield retirement invariant differs");
        const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
        reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),ids.data());handle=0;
        const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
        const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
        logFile->event("motion_shield_retired","\"handle\":"+std::to_string(id)+",\"previous_item\":"+std::to_string(item)+",\"next_item\":"+std::to_string(current.item)+",\"generation_invalidated\":"+(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false"));
        if(!invalid||!released)throw std::runtime_error("shield retirement failed");
        ++retired;
    }
    scene=motionVisual::scene;thread=GetCurrentThreadId();
    const auto original=read<std::array<uint8_t,432>>(source);
    auto transform=read<std::array<uint8_t,56>>(source+0x38);const auto scale=read<Vec3>(source+0x60);
    if(anchor.worldUnitsPerMeter!=1||!finite(scale))throw std::runtime_error("shield observed scale differs");
    // The game scales the character a little while it places something (summoning staff: 0.944 in Mock run
    // game065355); the clone takes the source's scale with its transform. A shield shrunk away is not shown.
    static bool shrunk=false;const bool small=scale.x<.5f||scale.y<.5f||scale.z<.5f||scale.x>1.5f||scale.y>1.5f||scale.z>1.5f;
    if(small!=shrunk){shrunk=small;std::ostringstream s;s<<std::setprecision(6)<<"\"frame\":"<<liveFrame.frame<<",\"item\":"<<current.item<<",\"hidden\":"<<(small?"true":"false")
        <<",\"scale\":["<<scale.x<<','<<scale.y<<','<<scale.z<<"]";logFile->event("motion_shield_native_scale",s.str());}
    if(small)return;
    const auto tracked=motion::mountNativeShield(shownHands.hands[0].grip);
    const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,tracked));
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    const auto position=cameraPosition(origin,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
    const auto rotation=normalized(multiply(facing,tracking.orientation));
    std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&rotation,16);
    if(!handle){
        motionVisual::validate();const auto refs=read<uint32_t>(res+16);if(!refs||refs==UINT32_MAX)throw std::runtime_error("shield resource reference bound");
        resource=res;item=current.item;
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(scene),current.modelGuid.data(),transform.data());
        if(!handle||handle==current.handle)throw std::runtime_error("shield creation failed/aliased");
        auto clone=motionVisual::object(handle);
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        clone=motionVisual::object(handle);
        if(read<uintptr_t>(clone+layout::modelResource)!=res||read<uint32_t>(res+16)!=refs+1||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(clone+layout::modelPose))throw std::runtime_error("shield clone resource/pose differs");
        const auto sdfId=read<uint32_t>(clone+layout::modelSdf);
        if(sdfId){
            if(sdfId==read<uint32_t>(motionVisual::object(current.handle)+layout::modelSdf))throw std::runtime_error("shield SDF aliases source");
            const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=read<uint64_t>(sdf+0x78);if(!count)throw std::runtime_error("shield SDF count");
            using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a640)(reinterpret_cast<void*>(sdf),sdfId);
            *reinterpret_cast<uint32_t*>(motionVisual::object(handle)+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("shield SDF removal failed");
        }
        logFile->event("motion_shield_created","\"handle\":"+std::to_string(handle)+",\"source_handle\":"+std::to_string(current.handle)+
            ",\"item\":"+std::to_string(item)+",\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(read<uint32_t>(res+16))+",\"mask\":0,\"sdf\":0,\"combat_authorized\":false");
    }else{
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    }
    const auto clone=motionVisual::object(handle);
    if(read<std::array<uint8_t,432>>(motionVisual::object(current.handle))!=original||read<uintptr_t>(clone+layout::modelResource)!=resource||
       read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelPose)||
       std::memcmp(read<std::array<uint8_t,56>>(clone+0x38).data(),transform.data(),52))throw std::runtime_error("shield source/pose invariant failed");
    sourceHandle=current.handle;serial=currentSerial;frame=liveFrame.frame;ready=true;++updates;
    attachmentObserver::shownItem[0].serial=serial;attachmentObserver::shownItem[0].frame=frame;std::memcpy(attachmentObserver::shownItem[0].transform.data(),transform.data(),40);
    std::ostringstream out;out<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"time\":"<<liveFrame.time
        <<",\"handle\":"<<handle<<",\"source_handle\":"<<sourceHandle<<",\"item\":"<<item<<",\"q32\":["<<position.x<<','<<position.y<<','<<position.z
        <<"],\"orientation\":["<<rotation.x<<','<<rotation.y<<','<<rotation.z<<','<<rotation.w<<"],\"source_unchanged\":true,\"mask\":0,\"sdf\":0,\"grip_hand\":0,\"native_slot\":12";
    logFile->event("motion_shield_pose",out.str());
}
inline void dispose(uintptr_t s){
    if(!handle||s!=scene)return;
    if(thread!=GetCurrentThreadId())throw std::runtime_error("shield disposal thread differs");
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf))throw std::runtime_error("shield disposal invariant differs");
    const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(s),ids.data());handle=0;ready=false;
    const bool invalid=read<uint8_t>(read<uintptr_t>(s+0x160)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
    logFile->event("motion_shield_disposed","\"handle\":"+std::to_string(id)+",\"updates\":"+std::to_string(updates)+
        ",\"generation_invalidated\":"+(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false")+",\"scene_disposal\":true");
    if(!invalid||!released)throw std::runtime_error("shield disposal failed");
}
}
