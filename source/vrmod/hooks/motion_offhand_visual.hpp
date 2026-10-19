// The left-hand part of a weapon held as a pair (VHVR DualKnives / DualAxes: one in each hand). The game's own part
// stays untouched and out of the eyes; a Scene-owned copy of the same model is drawn in the tracked left hand
// (mask/SDF 0), in the third private slot, which the shield does not use while a pair is held.
namespace motionOffhandVisual {
inline bool requested=false,ready=false;
inline uint32_t handle=0,sourceHandle=0,item=0,child=0;inline uintptr_t resource=0,scene=0,world=0;inline unsigned retired=0;
inline uint64_t serial=0,frame=0,updates=0;inline DWORD thread=0;
inline std::array<uint64_t,2> payloadSerial{};
// The part in the tracked left hand this frame: tracking-space pose, world transform and its attachment (for the stroke
// of the left hand and for where that stroke's hit volume belongs).
inline Pose tracked=identityPose();inline std::array<uint8_t,56> worldTransform{};inline std::array<uint8_t,48> attachment{};inline Vec3 scale{1,1,1};
inline motion::LiveMeleeWeapon meleeWeapon;   // empty unless the part is shown this frame
inline void retire(const char* event,const std::string& detail){
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf))throw std::runtime_error("off-hand retirement invariant differs");
    const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10df20)(reinterpret_cast<void*>(scene),ids.data());handle=0;ready=false;
    const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
    logFile->event(event,"\"handle\":"+std::to_string(id)+",\"item\":"+std::to_string(item)+",\"updates\":"+std::to_string(updates)+",\"generation_invalidated\":"+(invalid?"true":"false")+
        ",\"reference_released\":"+(released?"true":"false")+detail);
    if(!invalid||!released)throw std::runtime_error("off-hand retirement failed");
}
// The caught fish in the tracked left hand (2026-10-19 19:15, user: "after the catch, let the caught fish stick to the left
// hand"). From the catch (35 Victory) until the fish is taken or let go (36..38): where the game has put the fish on the
// character (its SlotAttachment, as the attachment system read it), the same attachment from the tracked left grip -
// as the left part of a pair is mounted - in the game's world, published for the fish's own entity;
// attachment_observer.hpp writes it right after the game attaches the fish. EVR_FISH_IN_HAND=0: off.
inline void fishInHand(const game::StableAnchor& anchor){
    static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_IN_HAND",v,sizeof v);return !(n==1&&v[0]=='0');}();
    if(!on)return;
    const auto now=GetTickCount64();const auto fish=controllerUiFixture::fishingFishId.load();
    const int state=controllerUiFixture::fishingState.load();const auto stateAt=controllerUiFixture::fishingStateMs.load();
    if(!fish||state<35||state>38||!stateAt||now<stateAt||now-stateAt>500)return;
    if(!shownHands.focused||!shownHands.hands[0].tracked())return;
    std::array<uint8_t,48> slot{};uintptr_t world=0;uint32_t parent=0;bool seen=false;
    {std::lock_guard<std::mutex> lock(attachmentObserver::followMutex);const auto& c=attachmentObserver::fishAttach;
     if(c.entity==fish&&c.at&&now>=c.at&&now-c.at<=500){slot=c.slot;world=c.world;parent=c.parent;seen=true;}}
    static uint32_t toldMissing=0;
    if(!seen){
        // Attached to nothing (headset runs 20261019-191819 / -193001: the game places the caught fish itself, every
        // frame): its render object is moved instead, right before the eyes are drawn, as the body's parts are
        // (motion_body_ik.hpp) - its origin to the tracked left grip, its orientation the game's.
        // (19:40..19:55 its render object's origin went to the tracked grip, the game's orientation kept - user: "the fish
        // is held on the line of the wrist"; the body's solve now holds it as the game's animation does: motion_body_ik.hpp.)
        if(toldMissing!=fish){toldMissing=fish;logFile->event("fishing_fish_not_attached","\"fish\":"+std::to_string(fish)+",\"state\":"+std::to_string(state)+",\"held_by\":\"body_ik\"");}
        return;}
    const auto root=rootCamera::snapshot();if(!root.entity)return;
    const auto held=motion::mountPairPart(shownHands.hands[0].grip,slot,motionVisual::weaponPitchDegrees);
    const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,held));
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    const auto position=cameraPosition(origin,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
    const auto rotation=normalized(multiply(facing,tracking.orientation));
    attachmentObserver::Follow f;f.entity=fish;f.parent=parent;f.world=world;f.at=now;f.frame=liveFrame.frame;
    std::memcpy(f.transform.data(),&position,24);std::memcpy(f.transform.data()+24,&rotation,16);
    attachmentObserver::stampAnchor(f,anchor.root);attachmentObserver::publishFishFollow(f);
    static unsigned published=0;if(published++<4)logFile->event("fishing_fish_in_hand","\"fish\":"+std::to_string(fish)+",\"state\":"+std::to_string(state)+",\"slot\":"+std::to_string(slot[41]));
}
inline void prepare(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested)return;ready=false;meleeWeapon={};
    if(!motionVisual::contextReady||motionVisual::contextSerial!=currentSerial||motionVisual::contextFrame!=liveFrame.frame||
       !motionHandVisual::ready||motionHandVisual::serial!=currentSerial||!shownHands.focused||!shownHands.hands[0].tracked())return;
    const auto root=rootCamera::snapshot();
    const auto pair=attachmentObserver::dualWield(root.entity,GetTickCount64());
    if(!pair.valid||pair.left.modelScene!=motionVisual::scene||pair.left.modelView!=motionVisual::view||(root.world&&pair.left.world!=root.world))return;
    const auto& current=pair.left;
    const auto source=motionVisual::currentObject(current.handle);if(!source)return;
    const auto res=read<uintptr_t>(source+layout::modelResource);
    if(!res||read<uint8_t>(res+0x14)!=1||read<uint16_t>(res+layout::resourceModel)!=current.model||read<std::array<uint32_t,4>>(res+24)!=current.modelGuid||
       !read<uint32_t>(source+layout::modelMask)||read<uint32_t>(source+layout::modelPose))return;
    if(scene&&(scene!=motionVisual::scene||thread!=GetCurrentThreadId()))throw std::runtime_error("off-hand Scene/thread changed");
    if(handle&&resource!=res){retire("motion_offhand_retired",",\"next_item\":"+std::to_string(current.item));++retired;}
    scene=motionVisual::scene;thread=GetCurrentThreadId();
    const auto original=read<std::array<uint8_t,432>>(source);
    auto transform=read<std::array<uint8_t,56>>(source+0x38);scale=read<Vec3>(source+0x60);
    if(anchor.worldUnitsPerMeter!=1||!finite(scale))throw std::runtime_error("off-hand observed scale differs");
    if(scale.x<=0||scale.y<=0||scale.z<=0||scale.x>4||scale.y>4||scale.z>4)return;   // shrunk away by the game: nothing to show
    tracked=motion::mountPairPart(shownHands.hands[0].grip,current.slotBytes,motionVisual::weaponPitchDegrees);
    const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,tracked));
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    const auto position=cameraPosition(origin,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
    const auto rotation=normalized(multiply(facing,tracking.orientation));
    std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&rotation,16);
    if(!handle){
        motionVisual::validate();const auto refs=read<uint32_t>(res+16);if(!refs||refs==UINT32_MAX)throw std::runtime_error("off-hand resource reference bound");
        resource=res;item=current.item;
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb450)(reinterpret_cast<void*>(scene),current.modelGuid.data(),transform.data());
        if(!handle||handle==current.handle)throw std::runtime_error("off-hand creation failed/aliased");
        auto clone=motionVisual::object(handle);
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf7030)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        clone=motionVisual::object(handle);
        if(read<uintptr_t>(clone+layout::modelResource)!=res||read<uint32_t>(res+16)!=refs+1||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(clone+layout::modelPose))throw std::runtime_error("off-hand clone resource/pose differs");
        const auto sdfId=read<uint32_t>(clone+layout::modelSdf);
        if(sdfId){
            if(sdfId==read<uint32_t>(motionVisual::object(current.handle)+layout::modelSdf))throw std::runtime_error("off-hand SDF aliases source");
            const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=read<uint64_t>(sdf+0x78);if(!count)throw std::runtime_error("off-hand SDF count");
            using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a540)(reinterpret_cast<void*>(sdf),sdfId);
            *reinterpret_cast<uint32_t*>(motionVisual::object(handle)+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("off-hand SDF removal failed");
        }
        logFile->event("motion_offhand_created","\"handle\":"+std::to_string(handle)+",\"source_handle\":"+std::to_string(current.handle)+",\"item\":"+std::to_string(item)+",\"child\":"+std::to_string(current.entity)+
            ",\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(read<uint32_t>(res+16))+",\"mask\":0,\"sdf\":0,\"grip_hand\":0,\"native_slot\":9");
    }else{
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    }
    const auto clone=motionVisual::object(handle);
    if(read<std::array<uint8_t,432>>(motionVisual::object(current.handle))!=original||read<uintptr_t>(clone+layout::modelResource)!=resource||
       read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelPose)||
       std::memcmp(read<std::array<uint8_t,56>>(clone+0x38).data(),transform.data(),52))throw std::runtime_error("off-hand source/pose invariant failed");
    sourceHandle=current.handle;child=current.entity;world=current.world;worldTransform=transform;attachment=current.slotBytes;
    serial=currentSerial;frame=liveFrame.frame;ready=true;++updates;
    attachmentObserver::shownItem[0].serial=serial;attachmentObserver::shownItem[0].frame=frame;std::memcpy(attachmentObserver::shownItem[0].transform.data(),transform.data(),40);
    // Its own stroke (live_melee.hpp, hand 0): the blade from the grip to the tip of the model's long axis.
    if(controllerUiFixture::liveMeleeRequested){
        meleeWeapon.binding=(uint64_t(item)<<32)|sourceHandle;meleeWeapon.hand=0;meleeWeapon.forward=rotate(tracked.orientation,{0,1,0});
        const auto* asset=motion::catalogDualWieldItem(item);const float tip=(asset?asset->tipY:.5f)*scale.y;
        const auto a=position,b=cameraPosition(position,rotate(rotation,Vec3{0,tip,0}),anchor.worldUnitsPerMeter);
        meleeWeapon.bladeFrom={a.x,a.y,a.z};meleeWeapon.bladeTo={b.x,b.y,b.z};meleeWeapon.blade=true;meleeWeapon.needsTarget=true;
        meleeWeapon.weaponType=13;meleeWeapon.reach=tip;   // the left part of a pair is a dagger (dual_wield_catalog.hpp)
    }
    // The game's own left-hand part follows the tracked one, as the right-hand item does (attachment_observer.hpp).
    if(attachmentObserver::followRequested){attachmentObserver::Follow f;f.entity=current.entity;f.parent=root.entity;f.world=current.world;f.at=GetTickCount64();f.frame=liveFrame.frame;
        std::memcpy(f.transform.data(),transform.data(),40);attachmentObserver::stampAnchor(f,anchor.root);attachmentObserver::publishFollowLeft(f);}
    std::ostringstream out;out<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"handle\":"<<handle<<",\"source_handle\":"<<sourceHandle<<",\"item\":"<<item
        <<",\"q32\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"orientation\":["<<rotation.x<<','<<rotation.y<<','<<rotation.z<<','<<rotation.w
        <<"],\"world_transform_bytes\":"<<jsonString(nativeImpactComponents::asHex(transform))<<",\"source_unchanged\":true,\"mask\":0,\"sdf\":0,\"grip_hand\":0,\"native_slot\":9";
    logFile->event("motion_offhand_pose",out.str());
}
// The left hand struck the attack that is running (the latest stroke, no older than an attack lasts) and the part
// was shown on the previous frame: its world transform stands for the weapon's.
inline bool strike(std::array<uint8_t,56>& transform){
    const auto left=controllerUiFixture::liveStrikeLeftMs.load(),right=controllerUiFixture::liveStrikeRightMs.load();const auto now=GetTickCount64();
    if(!left||left<right||now<left||now-left>1500||!handle||!updates||liveFrame.frame<frame||liveFrame.frame-frame>2)return false;
    std::memcpy(transform.data(),worldTransform.data(),40);return true;
}
inline void dispose(uintptr_t s){
    if(!handle||s!=scene)return;
    if(thread!=GetCurrentThreadId())throw std::runtime_error("off-hand disposal thread differs");
    retire("motion_offhand_disposed",",\"scene_disposal\":true");
}
}
