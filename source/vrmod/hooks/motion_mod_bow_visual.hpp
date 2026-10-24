// The mod's bow (2026-10-24; user: "the action bar is full in ordinary play - the mod's way"; "not slowed while the
// string is not drawn, and drawing it in the air has to float"). Taken from the left shoulder with no slot of the action
// bar holding it (holster_actions.hpp, controllerUiFixture::modBowItem), the equipment's bow is shown here in the tracked
// left hand while the game's hand is bare: the game is not aiming, so the walk is not slowed. The draw starts from this
// bow's geometry (bow_draw.hpp: the trigger near the string presses the game's LT); the LT brings the game's own bow and
// its aim, and from the frame the game's bow is shown (motion_visual_bridge.hpp) this one steps aside. Both carry the
// same binding (bowBinding), so a draw goes on through the change of model.
// The model: the bow's own (ranged_asset_catalog.hpp), made by its GUID as the hand items' copies are (0xfb4e0) with
// mask/SDF 0, at the rest pose of its skeleton (as the consumables', motion_consumable_visual.hpp), and drawn through the
// eyes' private hand slot (head_visibility_bridge.hpp). Placed as the game's bow is on the draw route.
namespace motionModBow {
inline bool requested=false,ready=false;
inline uint32_t handle=0,item=0,failedItem=0;inline uintptr_t resource=0,scene=0;inline DWORD thread=0;
inline uint64_t serial=0,frame=0,updates=0,createdMs=0,hiddenFrames=0;inline bool loaded=false;inline unsigned remasked=0,sdfRemoved=0;
inline uint32_t poseHandle=0;inline bool posed=false;
inline std::array<uint64_t,2> payloadSerial{};
// This frame's bow for the draw (its binding: bowBinding), when shown.
inline motion::BowPhysicalGeometry geometry;
inline bool shown(uint64_t s){return requested&&ready&&handle&&serial==s&&frame==liveFrame.frame;}
// The draw may start from it this frame - also while its model is still loading (the triggers stay the bow's).
inline bool drawable(uint64_t s){return requested&&handle&&geometry.binding&&serial==s&&frame==liveFrame.frame;}
inline std::array<uint32_t,4> modelOf(uint32_t id){
    for(const auto& a:motion::catalogRangedAssets){if(a.item==id)return motion::catalogRangedRigs[a.rig].type==4?a.guid:std::array<uint32_t,4>{};if(a.item>id)break;}
    return {};
}
inline void retire(const char* event,const std::string& detail){
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource)throw std::runtime_error("mod bow retirement invariant differs");
    const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),ids.data());handle=0;ready=false;
    motionConsumableVisual::unpose(scene,poseHandle,posed);
    const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
    logFile->event(event,"\"handle\":"+std::to_string(id)+",\"item\":"+std::to_string(item)+",\"updates\":"+std::to_string(updates)+",\"loaded\":"+(loaded?"true":"false")+
        ",\"references_before\":"+std::to_string(refs)+",\"generation_invalidated\":"+(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false")+detail);
    item=0;resource=0;loaded=false;
    if(!invalid||!released)throw std::runtime_error("mod bow retirement failed");
}
// The game's own bow is shown this frame (the LT of a draw brought it).
inline bool gameBowShown(uint64_t s){
    return motionVisual::poseReady&&motionVisual::updatedSerial==s&&motionVisual::updatedFrame==liveFrame.frame&&motionVisual::handle&&motion::catalogBowItem(motionVisual::item);
}
inline void prepare(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested)return;ready=false;geometry={};
    const auto now=GetTickCount64();
    uint32_t bow=controllerUiFixture::modBowItem.load();
    {const auto held=liveActionbars::hand(now);if(held.valid&&held.item)bow=0;}   // a slot selected: that is in the hand
    if(handle&&(scene!=motionVisual::scene||thread!=GetCurrentThreadId()))throw std::runtime_error("mod bow Scene/thread changed");
    if(handle&&bow!=item)retire("mod_bow_retired",",\"wanted\":"+std::to_string(bow));
    if(!bow||bow==failedItem)return;
    if(gameBowShown(currentSerial)){if(hiddenFrames++==0)logFile->event("mod_bow_aside","\"frame\":"+std::to_string(liveFrame.frame)+",\"item\":"+std::to_string(bow)+",\"game_bow\":"+std::to_string(motionVisual::item));return;}
    if(hiddenFrames){logFile->event("mod_bow_back","\"frame\":"+std::to_string(liveFrame.frame)+",\"item\":"+std::to_string(bow)+",\"frames_aside\":"+std::to_string(hiddenFrames));hiddenFrames=0;}
    if(!motionVisual::contextReady||motionVisual::contextSerial!=currentSerial||motionVisual::contextFrame!=liveFrame.frame||
       !motionHandVisual::ready||motionHandVisual::serial!=currentSerial||!shownHands.focused||!shownHands.hands[0].tracked()||anchor.worldUnitsPerMeter!=1)return;
    const auto root=rootCamera::snapshot();if(!root.entity||!rootCamera::firstPerson(root))return;
    // Where the game's bow is on the draw route: its identity attachment at the left grip (motion_visual_bridge.hpp
    // mountNativeBow without the half turn of the display route - the model's +Z, the string, is the grip's +Z, toward
    // the player; bow_draw.hpp finds the nock there).
    auto tracked=shownHands.hands[0].grip;
    // While the string is held before the game's bow has come, this one turns onto the line between the hands too.
    const auto binding=motionVisual::bowBinding(bow,0);
    {controllerUiFixture::BowPhysicalPublished state;
     {std::lock_guard<std::mutex> lock(controllerUiFixture::bowPhysicalMutex);state=controllerUiFixture::bowPhysicalPublished;}
     const auto& right=shownHands.hands[1];
     if(state.held&&state.binding==binding&&state.frame<=shownHands.frame&&shownHands.frame-state.frame<=8&&state.milliseconds&&now>=state.milliseconds&&now-state.milliseconds<=200&&right.tracked())
        tracked=motion::bowDrawnPose(tracked,right.grip.position,motion::BowPhysicalTrial::restString.y);}
    const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,tracked));
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    const auto position=cameraPosition(origin,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
    const auto rotation=normalized(multiply(facing,tracking.orientation));
    // The model is drawn half round about its limbs (2026-10-24, user after the headset: "the bow from the shoulder faces the
    // wrong way"; Mock -162821: its string stood on the other side than the game's bow's). It stands at the rest pose of its
    // skeleton, and the game's own pose of a bow in the hand carries that half turn (the display route's
    // native_left_bow_string_back, weapon_mount.hpp). The draw's geometry keeps the draw route's pose (the nock at +Z).
    const auto shownRotation=normalized(multiply(rotation,Quat{0,1,0,0}));
    // Until its resource is loaded: at the hand, a thousandth of its size.
    const bool resourceIn=handle&&read<uint8_t>(resource+0x14)==1;
    const float s=resourceIn?1.f:.001f;const Vec3 scale{s,s,s};
    std::array<uint8_t,56> transform{};std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&shownRotation,16);std::memcpy(transform.data()+40,&scale,12);
    if(!handle){
        const auto g=modelOf(bow);
        if(g==std::array<uint32_t,4>{}){failedItem=bow;logFile->event("mod_bow_none","\"item\":"+std::to_string(bow)+",\"reason\":\"not_a_bow_of_the_catalog\"");return;}
        motionVisual::validate();scene=motionVisual::scene;thread=GetCurrentThreadId();
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(scene),g.data(),transform.data());
        if(!handle){failedItem=bow;logFile->event("mod_bow_failed","\"item\":"+std::to_string(bow));return;}
        auto clone=motionVisual::object(handle);resource=read<uintptr_t>(clone+layout::modelResource);
        if(!resource||read<std::array<uint32_t,4>>(resource+0x18)!=g)throw std::runtime_error("mod bow resource differs");
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        item=bow;createdMs=now;updates=0;loaded=false;
        logFile->event("mod_bow_created","\"handle\":"+std::to_string(handle)+",\"item\":"+std::to_string(item)+",\"model\":"+motionConsumableVisual::guidText(g)+
            ",\"resource_state\":"+std::to_string(read<uint8_t>(resource+0x14))+",\"references\":"+std::to_string(read<uint32_t>(resource+16)));
    }else{
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    }
    auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource)throw std::runtime_error("mod bow resource changed");
    // The game finishes a model when its resource has come: its mask and SDF are put back to the copy's own each time.
    if(read<uint32_t>(clone+layout::modelMask)){
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);clone=motionVisual::object(handle);
        if(remasked++<4)logFile->event("mod_bow_remasked","\"handle\":"+std::to_string(handle)+",\"frame\":"+std::to_string(liveFrame.frame));
    }
    if(const auto sdfId=read<uint32_t>(clone+layout::modelSdf)){
        const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=sdf?read<uint64_t>(sdf+0x78):0;if(!count)throw std::runtime_error("mod bow SDF count");
        using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a640)(reinterpret_cast<void*>(sdf),sdfId);
        *reinterpret_cast<uint32_t*>(motionVisual::object(handle)+layout::modelSdf)=0;
        if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("mod bow SDF removal failed");
        if(sdfRemoved++<4)logFile->event("mod_bow_sdf_removed","\"handle\":"+std::to_string(handle)+",\"frame\":"+std::to_string(liveFrame.frame));
    }
    const bool withPose=resourceIn&&motionConsumableVisual::pose(scene,handle,resource,poseHandle,posed);clone=motionVisual::object(handle);
    const bool nowLoaded=resourceIn&&withPose;
    if(nowLoaded&&!loaded)logFile->event("mod_bow_loaded","\"handle\":"+std::to_string(handle)+",\"item\":"+std::to_string(item)+",\"frame\":"+std::to_string(liveFrame.frame)+
        ",\"after_ms\":"+std::to_string(now-createdMs)+",\"pose\":"+std::to_string(read<uint32_t>(clone+layout::modelPose)));
    loaded=nowLoaded;
    if(!loaded&&now-createdMs>5000){static uint32_t told=0;if(told!=handle){told=handle;logFile->event("mod_bow_not_loaded","\"handle\":"+std::to_string(handle)+",\"item\":"+std::to_string(item)+
        ",\"resource_state\":"+std::to_string(read<uint8_t>(resource+0x14))+",\"pose\":"+std::to_string(poseHandle));}}
    serial=currentSerial;frame=liveFrame.frame;ready=loaded&&!read<uint32_t>(clone+layout::modelMask)&&!read<uint32_t>(clone+layout::modelSdf);++updates;
    geometry={binding,shownHands.frame,tracked,{1,1,1},position,rotation,root.world,root.entity};
    if(updates<=3||updates%300==0){std::ostringstream out;out<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"handle\":"<<handle<<",\"item\":"<<item
        <<",\"loaded\":"<<(loaded?"true":"false")<<",\"ready\":"<<(ready?"true":"false")<<",\"q32\":["<<position.x<<','<<position.y<<','<<position.z<<"]";logFile->event("mod_bow_pose",out.str());}
}
inline void dispose(uintptr_t s){
    if(!handle||s!=scene)return;
    if(thread!=GetCurrentThreadId())throw std::runtime_error("mod bow disposal thread differs");
    retire("mod_bow_disposed",",\"scene_disposal\":true");
}
}
