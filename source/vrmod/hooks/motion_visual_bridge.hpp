// Native weapon representations retained until Scene disposal. Native Scene mask stays 0.
// Only private eye culling may expose it. No original model/ECS/attack writes.
namespace motionVisual {
inline bool requested=false,validated=false,failed=false,poseReady=false,contextReady=false;
inline uintptr_t scene=0,view=0,resource=0;
inline bool equipmentSwitchRequested=false,suspendOnEquipmentChange=false;
inline bool bowAnimationRequested=false,rangedAnimationRequested=false;
inline motion::BowPhysicalGeometry bowGeometry;
// VHVR rotateBowOnPulling: set while this frame's bow was posed from both hands.
inline bool bowDrawHeld=false;
// The watering can's mount (weapon_mount.hpp mountCarriedTool): form 3, the can hangs under the fist with its spout
// where the hand points. The earlier forms were turns of the weapon mount (1: the headset runs; 0 and 2: Mock only).
// What put the can off the hand in all of them was something else: the offset of its attachment was misread as
// 0.77 m (weapon_mount.hpp nativeHandAttachment).
// For a comparison, EVR_CARRIED_TOOL_TURN in the game's environment: x (form 1), h (form 0) or y (form 2).
inline unsigned carriedToolForm(){static const unsigned form=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_CARRIED_TOOL_TURN",v,sizeof v);
    return !n||n>=sizeof v?3u:(v[0]=='h'||v[0]=='H')?0u:(v[0]=='x'||v[0]=='X')?1u:(v[0]=='y'||v[0]=='Y')?2u:3u;}();return form;}
// VHVR "Full" draw restriction. bowDrawProgress is the native bow's own draw (0..1)
// read from its source pose on the previous frame; while the string is held back,
// bowDrawHand is where the drawing hand is shown (on the string, as VHVR's hand IK).
inline float bowDrawProgress=0;inline bool bowDrawHandValid=false;inline Vec3 bowDrawHand{};
// Reviewed staff or wand shown this frame (ranged_aim.hpp); item 0 when none.
inline motion::BowPhysicalGeometry rangedGeometry;inline uint32_t rangedItem=0;inline bool rangedTwoHanded=false;
// Melee weapon shown this frame for the live swing detector (live_melee.hpp); binding 0 when none.
inline motion::LiveMeleeWeapon meleeWeapon;
inline float meleeTipY=0;inline uint32_t meleeTipItem=0;   // the blade's tip of the shown weapon for its hit volume (ColliderPose::tipY)
// Fishing rod shown this frame (item category FishingRod, 5: the five rods of the game data, held-models.json), for
// casting by a swing (live_swing_cast.hpp); binding 0 when none.
inline motion::LiveSwingRod heldRod;
// The rod while its float is out (user after the headset run game-20261013-141010: "the float and the hook of the
// rod's model do not move when I cast; it feels wrong - take them away or move them"). The copy in the hand is drawn
// as its vertices are stored, float and hook at the tip; the line, the float and the hook are joints the game
// simulates for its own instance of the rod (they go out to the float in the water, the rod bends). That instance
// already follows the tracked hand (attachment_observer.hpp, the "follow" of live held items: 2 mm from the tracked
// rod in the median of the headset run, a frame behind in a swing). So while the float is out, the game's own rod is
// shown and the copy is left out (head_visibility_bridge.hpp); before the cast and after reeling in it is the copy
// again. Whether the float is out: controllerUiFixture::fishingFloatOut (the player's keen::ecs::NetworkFishing).
// EVR_ROD_NATIVE=0 keeps the copy throughout.
inline uint64_t nativeRodSerial=0,nativeRodFrame=0;
inline bool nativeRodWanted(){static const bool wanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ROD_NATIVE",v,sizeof v);return !(n&&n<sizeof v&&v[0]=='0');}();return wanted;}
inline bool nativeRodShown(uint64_t serial){return nativeRodSerial==serial&&nativeRodFrame==liveFrame.frame&&serial!=0;}
inline bool twoHandWieldRequested=false;inline motion::LiveWield wieldState;inline motion::LiveWieldPose wieldPose;
inline bool twoHandCatalogOnly=false; // diagnostic: measured assets also take the catalog group's mount
inline bool liveWeaponMount=false;inline float weaponPitchDegrees=motion::vhvrWeaponPitchDegrees;
inline bool hideHeldAttachments=false; // live play: hide every held item's native instance (head_visibility_bridge)
inline bool canTransition(){return equipmentSwitchRequested||suspendOnEquipmentChange;}inline std::string dormantReason;
inline DWORD thread=0;
inline uint32_t handle=0,sourceHandle=0,item=0,child=0;
inline uint64_t updatedSerial=0,updatedFrame=0,updates=0,contextSerial=0,contextFrame=0;
inline attachmentObserver::Child hiddenSource{};
inline motion::ColliderPose pendingColliderPose;
inline std::array<uint8_t,0x1b0> hiddenSourceBytes{};
inline std::array<uint32_t,4> guid{};
inline std::array<motion::RetainedVisual,motion::retainedVisualCapacity> retainedVisuals{};
// The hand items as shown this frame: attachmentObserver::shownItem (2026-10-15, online 2.1).
inline bool liveEquipment(){return liveWeaponMount&&!weaponGripPreview::requested;}
using Create=uint32_t(__fastcall*)(void*,const void*,const void*);
// 1.0 (2026-10-16): the batch transform change is 1150c0 (June c77d0) and takes a fourth byte: not 0 = a jump (the motion
// history of the model starts again, 1150c0 skips its distance test); 0 = as c77d0 did.
using Change=void(__fastcall*)(void*,const void*,const void*,uint8_t);
constexpr uintptr_t changeRva=0x115160;
inline void change(void* scene,const void* ids,const void* transforms){reinterpret_cast<Change>(baseAddress+changeRva)(scene,ids,transforms,0);}
using Mask=void(__fastcall*)(void*,void*,uint32_t);
using Remove=void(__fastcall*)(void*,const void*);
inline uintptr_t object(uint32_t id){
    const auto objects=read<uintptr_t>(scene+0x1b0),gens=read<uintptr_t>(scene+0x1c8);
    const auto bytes=read<uint64_t>(scene+0x1b8),count=read<uint64_t>(scene+0x1d0);const auto index=id&0xffffff;
    if(!id||!objects||!gens||bytes%0x1b0||index>=bytes/0x1b0||index>=count||read<uint8_t>(gens+index)!=(id>>24))
        throw std::runtime_error("motion visual stale/out-of-range handle");
    return objects+uintptr_t(index)*0x1b0;
}
inline uintptr_t currentObject(uint32_t id){
    const auto objects=read<uintptr_t>(scene+0x1b0),gens=read<uintptr_t>(scene+0x1c8);
    const auto bytes=read<uint64_t>(scene+0x1b8),count=read<uint64_t>(scene+0x1d0);const auto index=id&0xffffff;
    if(!id||!objects||!gens||bytes%0x1b0||index>=bytes/0x1b0||index>=count||read<uint8_t>(gens+index)!=(id>>24))return 0;
    return objects+uintptr_t(index)*0x1b0;
}
inline void dormant(const char* reason,uint32_t selected){
    if(dormantReason==reason)return;dormantReason=reason;
    logFile->event("motion_visual_dormant","\"reason\":"+jsonString(reason)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"selected_item\":"+std::to_string(selected)+",\"retained_handle\":"+std::to_string(handle)+",\"native_mask\":0,\"resource_retained\":true,\"combat_authorized\":false");
}
inline void validate(){
    if(validated)return;
    for(auto e:std::array<std::pair<uintptr_t,const char*>,5>{{
        {0xfb450,"40 55 57 41 54 41 56 41 57 48 83 ec 30 48 8b 42 08 4d 8b f8"},
        {0x115160,"48 8b c4 44 88 48 20 4c 89 40 18 55 56 41 55 41 57 48 81 ec 98 00 00 00"},
        {0x10df20,"48 89 6c 24 20 57 41 54 41 56 48 83 ec 30 45 33 e4 4c 8b f2"},
        {0xf7030,"44 89 82 14 01 00 00 4c 8b ca 4c 8b c2 48 b8 5f 42 7b 09 ed"},
        {0x15a540,"85 d2 0f 84 fa 04 00 00 89 54 24 10 4c 8b dc 49 89 4b 08 55"}}}){
        const auto p=parsePattern(e.second);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+e.first),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("motion visual signature mismatch");
        logFile->event("motion_visual_signature","\"rva\":"+hex(e.first)+",\"pattern\":"+jsonString(e.second)+",\"matched\":true");
    }
    validated=true;
}
// The model's own extent along its long axis (+Y for every melee weapon and tool of the game data; audit_held_models.py),
// from the descriptor the offline catalog reads: resource+0x30, minimum at +16, maximum at +32.
inline bool modelExtentY(uintptr_t res,float& rear,float& tip){
    static uintptr_t known=0;static float knownRear=0,knownTip=0;static bool valid=false;
    if(res!=known){known=res;valid=false;
        try{const auto model=read<uintptr_t>(res+layout::resourceDescriptor);const auto lo=read<Vec3>(model+16),hi=read<Vec3>(model+32);
            if(finite(lo)&&finite(hi)&&lo.y<hi.y&&hi.y-lo.y<4.f&&hi.y-lo.y>=hi.x-lo.x&&hi.y-lo.y>=hi.z-lo.z){knownRear=lo.y;knownTip=hi.y;valid=true;}
        }catch(const std::exception&){}
    }
    rear=knownRear;tip=knownTip;return valid;
}
// Set by motion_offhand_visual.hpp: replaces the transform by the left-hand weapon's when the left hand struck the
// attack that is running.
inline bool (*offhandStrike)(std::array<uint8_t,56>&)=nullptr;
inline attachmentObserver::Child appearance(uint32_t local,bool meleeOnly=true){
    const auto now=GetTickCount64();
    // A pair of weapons: its right-hand part is the shown weapon (the left-hand part: motion_offhand_visual.hpp).
    if(!meleeOnly){const auto pair=attachmentObserver::dualWield(local,now);if(pair.valid)return pair.right;}
    std::lock_guard<std::mutex> lock(attachmentObserver::mutex);
    attachmentObserver::Child out;
    for(const auto& c:attachmentObserver::children){
        auto fresh=[&](uint64_t t){return pauseClock::fresh(t,now,500);};   // the age stands still while the game is paused
        // Live held items: a tool in a hand slot is the shown item as a weapon is.
        const bool tool=!meleeOnly&&attachmentObserver::liveTools&&c.category!=2&&attachmentObserver::toolItemSlot(c.itemSlot)&&(c.slot==8||c.slot==9);
        if(c.parent!=local||(c.category!=2&&!tool)||(meleeOnly&&c.itemSlot!=1)||!c.item||!c.handle||!c.slotObserved||c.target!=0||(meleeOnly&&c.slot!=9)||
           !fresh(c.itemSeen)||!fresh(c.slotSeen)||!fresh(c.modelSeen)||c.slotSeen<c.itemChangedAt||c.modelSeen<c.itemChangedAt)continue;
        if(out.entity){const auto* kept=attachmentObserver::survivor(out,c);if(!kept)return {};if(kept==&c)out=c;continue;}   // the game replaced the visual: attachment_observer.hpp
        out=c;
    }
    return out;
}
// Shared native mutation boundary. Its readiness proves current World/XR/CPU
// ownership; it does not prove that any equipped weapon can be rendered or used.
inline void prepareContext(uintptr_t s,uintptr_t v,uint64_t serial){
    contextReady=false;hiddenSource={};
    const auto& hands=shownHands;
    if(hands.frame!=liveFrame.frame||hands.predictedTime!=liveFrame.time||hands.sessionGeneration!=liveFrame.sessionGeneration)
        throw std::runtime_error("motion visual requires same-frame OpenXR hand sample");
    const auto root=rootCamera::snapshot();
    if(!rootCamera::firstPerson(root))return;
    if(scene&&(s!=scene||v!=view||thread!=GetCurrentThreadId()))throw std::runtime_error("motion visual Scene/View/thread changed");
    scene=s;view=v;thread=GetCurrentThreadId();
    if(scene!=provenWorldScene||view!=provenWorldView||read<uintptr_t>(scene+layout::sceneView)!=view)
        throw std::runtime_error("motion visual not at original world boundary");
    // Do not call d5870 here: its 136550 discards pending world SDF results.
    // Only these five proven collectors precede the ORIGINAL native upload.
    validate();
    // 1.0 (2026-10-16): the View's streaming can stay busy for frames while the world streams in; that frame goes without
    // the hands (contextReady stays false) instead of ending the session.
    if(!finishEyeCpuTasks(scene,4)){contextReady=false;return;}
    contextReady=true;contextSerial=serial;contextFrame=liveFrame.frame;
    logFile->event("motion_visual_context","\"serial\":"+std::to_string(serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
        ",\"time\":"+std::to_string(liveFrame.time)+",\"scene\":"+hex(scene)+",\"view\":"+hex(view)+
        ",\"thread\":"+std::to_string(thread)+",\"same_frame_hand\":true,\"cpu_complete\":true,\"first_person\":true");
    // Visibility ownership is broader than the melee representation. A bow,
    // for example, remains the local equipped item's appearance but has no
    // reviewed tracked weapon pose yet. Never include effects/unclassified ECS.
    const auto held=appearance(root.entity,false);
    if(held.entity&&held.modelScene==scene&&held.modelView==view&&(!root.world||held.world==root.world)){
        const auto source=canTransition()?currentObject(held.handle):object(held.handle);if(!source)return;
        const auto res=read<uintptr_t>(source+layout::modelResource);
        if(!res||read<uint8_t>(res+0x14)!=1||read<uint16_t>(res+layout::resourceModel)!=held.model||
           read<std::array<uint32_t,4>>(res+0x18)!=held.modelGuid){
            if(canTransition())return; // Streaming/switch snapshot has no current visibility authority.
            throw std::runtime_error("motion hidden appearance identity changed");
        }
        hiddenSource=held;hiddenSourceBytes=read<std::array<uint8_t,0x1b0>>(source);
    }
}
// The original stays untouched: every byte of its Scene object, except six bookkeeping bytes (layout::modelBookkeeping) that the
// Scene itself rewrites for a skinned model with simulated joints when another instance of it appears (fishing rods;
// game051008, game051329).
inline bool sourceUnchanged(std::array<uint8_t,0x1b0> now,const std::array<uint8_t,0x1b0>& before){
    uint32_t pose=0;std::memcpy(&pose,before.data()+layout::modelPose,4);
    constexpr auto at=layout::modelBookkeeping;if(pose)std::copy(before.begin()+at,before.begin()+at+6,now.begin()+at);
    return now==before;
}
inline void verifyContextSource(){
    if(!contextReady)return;
    if(hiddenSource.entity&&!sourceUnchanged(read<std::array<uint8_t,0x1b0>>(object(hiddenSource.handle)),hiddenSourceBytes))
        throw std::runtime_error("motion original appearance changed during private preparation");
    logFile->event("motion_visual_selection","\"serial\":"+std::to_string(contextSerial)+",\"frame\":"+std::to_string(contextFrame)+
        ",\"source_handle\":"+std::to_string(hiddenSource.handle)+",\"item\":"+std::to_string(hiddenSource.item)+
        ",\"child\":"+std::to_string(hiddenSource.entity)+",\"parent\":"+std::to_string(hiddenSource.parent)+
        ",\"slot\":"+std::to_string(hiddenSource.slot)+",\"category\":"+std::to_string(hiddenSource.category)+
        ",\"item_slot\":"+std::to_string(hiddenSource.itemSlot)+",\"target\":"+std::to_string(hiddenSource.target)+
        ",\"world\":"+hex(hiddenSource.world)+",\"source_unchanged\":true,\"weapon_ready\":"+(poseReady?"true":"false"));
}
inline void prepare(uintptr_t s,uintptr_t v,const game::StableAnchor& anchor,uint64_t serial){
    if(!requested||failed||updatedSerial==serial)return;
    poseReady=false;pendingColliderPose={};wieldPose={};bowGeometry={};bowDrawHeld=false;bowDrawHandValid=false;rangedGeometry={};rangedItem=0;rangedTwoHanded=false;meleeWeapon={};heldRod={};
    struct RejectIncomplete {~RejectIncomplete(){if(!poseReady){motionColliderChannel::invalidate();wieldState.reset();}}} rejectIncomplete;
    prepareContext(s,v,serial);
    if(!contextReady)return;
    const auto& hands=shownHands;
    const auto root=rootCamera::snapshot();const auto current=appearance(root.entity,!liveEquipment());
    const bool nativeBow=liveEquipment()&&current.weaponCategoryKnown&&motion::nativeLeftBow(current.weaponType,current.itemSlot,current.slot);
    const unsigned gripHand=nativeBow?0:1;
    const bool tool=liveEquipment()&&attachmentObserver::liveTools&&current.category!=2&&attachmentObserver::toolItemSlot(current.itemSlot);
    const bool swungTool=tool&&attachmentObserver::swungTool(current.itemSlot,current.slot);
    if(liveEquipment()&&current.weaponCategoryKnown&&current.weaponType==4&&!nativeBow){dormant("unsupported_bow_attachment",current.item);return;}
    if(!hands.focused||!hands.hands[gripHand].tracked()||!current.entity||current.modelScene!=scene||current.modelView!=view||(root.world&&current.world!=root.world)){
        if(canTransition())dormant("tracking_or_no_calibrated_appearance",hiddenSource.item);return;
    }
    if(canTransition()&&(hiddenSource.entity!=current.entity||hiddenSource.handle!=current.handle||hiddenSource.item!=current.item)){
        // The broad visibility selection can be ambiguous while both old and
        // new attachment observations are still fresh. Do not show a clone
        // until the same-frame original can be hidden with matching ownership.
        dormant("appearance_transition_ambiguous",current.item);return;
    }
    const auto source=canTransition()?currentObject(current.handle):object(current.handle);
    if(!source){dormant("stale_original_handle",current.item);return;}
    const auto res=read<uintptr_t>(source+layout::modelResource);const auto originalBytes=read<std::array<uint8_t,0x1b0>>(source);
    const bool ready=res&&read<uint8_t>(res+0x14)==1&&!read<uint32_t>(res+layout::resourceAuxiliary);
    motion::VisualSelection liveSelection;
    if(liveEquipment()){
        // Main-hand item slots proven by VisualEquipment metadata. Effects,
        // body attachments and unclassified resources never enter this pool.
        const bool supported=((current.weaponCategoryKnown&&(current.itemSlot==1||current.itemSlot==4||current.itemSlot==5))||tool)&&
            (current.slot==8||current.slot==9)&&ready;
        const auto sourceGuid=ready?read<std::array<uint32_t,4>>(res+0x18):std::array<uint32_t,4>{};
        const motion::VisualIdentity selected{current.item,current.handle,current.entity,res,sourceGuid};
        liveSelection=motion::selectRetainedVisual(retainedVisuals,selected,supported);
        if(!liveSelection.valid){dormant(supported?"retained_visual_capacity":"unsupported_live_appearance",current.item);return;}
        const auto next=liveSelection.create?0:retainedVisuals[liveSelection.slot].clone;
        if(next!=handle){
            logFile->event("motion_visual_equipment_switch","\"frame\":"+std::to_string(liveFrame.frame)+",\"previous_handle\":"+std::to_string(handle)+
                ",\"next_handle\":"+std::to_string(next)+",\"item\":"+std::to_string(current.item)+",\"cache_slot\":"+std::to_string(liveSelection.slot)+
                ",\"reused\":"+(liveSelection.create?"false":"true")+",\"retained_until_scene_disposal\":true,\"combat_authorized\":false");
        }
        handle=next;resource=res;guid=sourceGuid;item=current.item;sourceHandle=current.handle;child=current.entity;
        if(handle&&read<uintptr_t>(object(handle)+layout::modelResource)!=resource)throw std::runtime_error("cached live weapon resource differs");
        dormantReason.clear();
    }else if(canTransition()){
        const auto sourceGuid=ready?read<std::array<uint32_t,4>>(res+0x18):std::array<uint32_t,4>{};
        const motion::VisualIdentity selected{current.item,current.handle,current.entity,res,sourceGuid},retained{item,sourceHandle,child,resource,guid};
        const bool calibrated=ready&&weaponGripPreview::requested&&current.weaponCategoryKnown&&current.weaponType==weaponGripPreview::type&&current.slot==weaponGripPreview::slot;
        // Explicit uncalibrated wheel diagnostic may retain/rebind only the
        // first verified resource. It never invents a grip for another item.
        const bool diagnostic=suspendOnEquipmentChange&&!weaponGripPreview::requested;
        const auto expectedItem=diagnostic?(handle?item:current.item):weaponGripPreview::item;
        const auto expectedGuid=diagnostic?(handle?guid:sourceGuid):weaponGripPreview::weaponGuid;
        const auto decision=motion::visualBinding(selected,retained,expectedItem,expectedGuid,diagnostic?ready:calibrated,handle!=0);
        if(decision==motion::VisualBinding::Suspend){dormant("unreviewed_or_different_resource",current.item);return;}
        if(decision==motion::VisualBinding::Rebind){
            const auto clone=object(handle);
            if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelAuxiliary))throw std::runtime_error("retained weapon clone identity changed");
            logFile->event("motion_visual_rebound","\"frame\":"+std::to_string(liveFrame.frame)+",\"handle\":"+std::to_string(handle)+",\"previous_source\":"+std::to_string(sourceHandle)+",\"source\":"+std::to_string(current.handle)+",\"previous_child\":"+std::to_string(child)+",\"child\":"+std::to_string(current.entity)+",\"item\":"+std::to_string(item)+",\"resource\":"+hex(resource)+",\"references\":"+std::to_string(read<uint32_t>(resource+0x10))+",\"clone_retained\":true,\"combat_authorized\":false");
            sourceHandle=current.handle;child=current.entity;
        }
        if(!dormantReason.empty()){logFile->event("motion_visual_resumed","\"frame\":"+std::to_string(liveFrame.frame)+",\"source\":"+std::to_string(current.handle)+",\"item\":"+std::to_string(current.item));dormantReason.clear();}
    }else{
        if(handle&&(current.handle!=sourceHandle||current.item!=item||current.entity!=child))throw std::runtime_error("motion visual equipment changed; bounded single-item trial ends");
        if(!ready)throw std::runtime_error("motion visual resource not supported/ready");
        if(handle&&(res!=resource||read<std::array<uint32_t,4>>(res+0x18)!=guid))throw std::runtime_error("motion visual resource identity changed");
    }
    auto transform=read<std::array<uint8_t,56>>(source+0x38);
    const bool bowAnimated=nativeBow&&bowAnimationRequested&&(motion::reviewedBowFacing(current.item,current.modelGuid)||(rangedAnimationRequested&&motion::catalogBowAsset(current.item,current.modelGuid)));
    // Every catalog bow shares the skeleton, so the string is on the same side as on the reviewed bow.
    // Headset run game100334: the second bow (left trigger with the wand) pointed its string away.
    const bool bowFacing=nativeBow&&!bowAnimated&&(motion::reviewedBowFacing(current.item,current.modelGuid)||motion::catalogBowAsset(current.item,current.modelGuid));
    const auto scale=weaponGripPreview::requested?weaponGripPreview::scale:read<Vec3>(source+0x38+40);
    // Measured assets first; every other left-hand two-hand melee weapon of the catalog takes the group's mount.
    const bool measuredMount=liveEquipment()&&!twoHandCatalogOnly&&motion::nativeMeleeMount(current.item,current.weaponType,current.modelGuid).known;
    const auto meleeMount=tool?motion::toolMount(current.itemSlot,current.slot):!liveEquipment()||!current.weaponCategoryKnown?motion::NativeMeleeMount{}:
        twoHandCatalogOnly?motion::catalogFamilyMount(current.item,current.weaponType,current.modelGuid,current.slot):
        motion::catalogMeleeMount(current.item,current.weaponType,current.modelGuid,current.slot);
    if(nativeBow&&anchor.worldUnitsPerMeter!=1)throw std::runtime_error("native bow world scale differs");
    const bool carriedTool=tool&&!weaponGripPreview::requested&&!nativeBow&&!meleeMount.known&&liveWeaponMount&&current.slot==8&&motion::hangingTool(current.item,current.modelGuid);
    auto trackedWeapon=weaponGripPreview::requested?weaponGripPreview::locate(hands,current,res,serial):
        nativeBow?motion::mountNativeBow(hands.hands[0].grip,current.slotBytes,root.entity,bowFacing):
        meleeMount.known?motion::mountNativeMelee(motion::mountLiveWeapon(hands.hands[1].grip,weaponPitchDegrees),meleeMount,scale):
        liveWeaponMount?(carriedTool?motion::mountCarriedTool(hands.hands[1].grip,weaponPitchDegrees,carriedToolForm()):motion::mountLiveWeapon(hands.hands[1].grip,weaponPitchDegrees)):hands.hands[1].grip;
    // The plain right-grip route puts the hand slot at the grip; the game's own attachment offset, where an item has
    // one, goes between the slot and the model as in the game.
    const auto handAttachment=!weaponGripPreview::requested&&!nativeBow&&!meleeMount.known&&liveWeaponMount&&current.slot==8?motion::nativeHandAttachment(current.slotBytes):motion::NativeAttachment{};
    if(!handAttachment.identity)trackedWeapon=compose(trackedWeapon,handAttachment.local);
    const char* mountKind=weaponGripPreview::requested?"measured_local_grip":bowAnimated?"native_left_bow_animated":bowFacing?"native_left_bow_string_back":nativeBow?"native_left_bow":measuredMount?"native_melee_right_grip":tool&&meleeMount.known?"tool_left_slot_right_grip":tool?"tool_right_grip":meleeMount.known?"catalog_melee_right_grip":liveWeaponMount?"live_vhvr_pitch":"diagnostic_identity";
    if(!handAttachment.identity)mountKind=carriedTool?"tool_right_grip_carried":tool?"tool_right_grip_attached":"live_vhvr_pitch_attached";
    if(controllerUiFixture::bowDrawRequested&&bowAnimated&&motion::bowDrawAsset(current.item,current.modelGuid,rangedAnimationRequested)){
        // While the frontend holds the string, the bow looks along the line between
        // both hands (VHVR). The same decision drives the private string pose.
        controllerUiFixture::BowPhysicalPublished state;
        {std::lock_guard<std::mutex> lock(controllerUiFixture::bowPhysicalMutex);state=controllerUiFixture::bowPhysicalPublished;}
        // The frame on which the trigger comes up is still posed from both hands:
        // the frontend reads this frame's geometry when it turns the release into a shot.
        const auto now=GetTickCount64();const auto& right=hands.hands[1];
        bowDrawHeld=state.held&&state.binding==((uint64_t(current.item)<<32)|current.handle)&&state.frame<=hands.frame&&hands.frame-state.frame<=8&&
            state.milliseconds&&now>=state.milliseconds&&now-state.milliseconds<=200&&right.tracked();
        if(bowDrawHeld){
            trackedWeapon=motion::bowDrawnPose(trackedWeapon,right.grip.position,motion::BowPhysicalTrial::restString.y*scale.y);mountKind="native_left_bow_drawn";
            const auto local=rotate(inverse(trackedWeapon.orientation),right.grip.position-trackedWeapon.position);
            // The first part of the draw is free (2026-10-21, user: "the start of the draw was slow; as a guest too"): the game
            // starts drawing about 0.35 s after its LT (its nocking; Valheim, which VHVR's restriction was made for, has no
            // such wait), and until then the hand stood held at the string. Up to freePull of the game's progress (0.25:
            // half the travel) the hand goes as it is; beyond that it follows the game's draw. EVR_BOW_FREE_PULL=<0..1>.
            static const float freePull=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_BOW_FREE_PULL",v,sizeof v);
                if(!n||n>=sizeof v)return .25f;const float f=std::strtof(v,nullptr);return std::isfinite(f)?std::clamp(f,0.f,1.f):.25f;}();
            const float brace=motion::BowPhysicalTrial::restString.z*scale.z,limit=brace+motion::bowPullLimit(std::max(bowDrawProgress,freePull),brace,motion::bowFullDrawLength);
            if(local.z>limit){bowDrawHand=trackedWeapon.position+rotate(trackedWeapon.orientation,{local.x,local.y,limit});bowDrawHandValid=true;}
        }
    }
    // Left-hand two-hand melee weapons hold by their mount. With the VHVR entry the weapons on the
    // plain right-hand mount join: great swords, and staffs when the staff aim route is on (the
    // shot then flies along the line between the hands).
    const auto wieldProfile=meleeMount.known?meleeMount:
        liveEquipment()&&liveWeaponMount&&!nativeBow&&wieldState.vhvrEntry()&&current.weaponCategoryKnown&&(current.weaponType!=5||controllerUiFixture::rangedAimRequested)?
            motion::twoHandHoldProfile(current.item,current.weaponType,current.modelGuid,current.slot):motion::NativeMeleeMount{};
    if(twoHandWieldRequested&&wieldProfile.known&&liveEquipment()){
        const uint64_t wieldBinding=(uint64_t(current.item)<<32)|current.handle;
        wieldPose=wieldState.update(hands,wieldBinding,wieldProfile,scale,trackedWeapon,!controllerUiFixture::liveWieldBusy.load());
        trackedWeapon=wieldPose.pose;
    }else wieldState.reset();
    const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,trackedWeapon));
    const auto facing=normalized(anchor.orientation);
    const auto rootCamera=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    const auto position=cameraPosition(rootCamera,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
    const auto orientation=normalized(multiply(facing,tracking.orientation));
    std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&orientation,16);
    // Keep native scale. Known local rigid assets use their measured grip;
    // unknown equipment remains on the explicitly uncalibrated mount.
    if(weaponGripPreview::requested)std::memcpy(transform.data()+40,&scale,12);
    if(!finite(scale)||scale.x<=0||scale.y<=0||scale.z<=0)throw std::runtime_error("motion visual native scale invalid");
    if(!handle){
        const auto refs=read<uint32_t>(res+0x10);if(!refs||refs==UINT32_MAX)throw std::runtime_error("motion resource reference bound");
        resource=res;guid=read<std::array<uint32_t,4>>(res+0x18);sourceHandle=current.handle;item=current.item;child=current.entity;
        handle=reinterpret_cast<Create>(baseAddress+0xfb450)(reinterpret_cast<void*>(scene),guid.data(),transform.data());
        if(!handle||handle==sourceHandle)throw std::runtime_error("motion clone creation failed/aliased");
        auto clone=object(handle);
        reinterpret_cast<Mask>(baseAddress+0xf7030)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        clone=object(handle);
        if(read<uintptr_t>(clone+layout::modelResource)!=res||read<uint32_t>(res+0x10)!=refs+1||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelAuxiliary))
            throw std::runtime_error("motion clone resource/mask/auxiliary mismatch");
        const auto sdfId=read<uint32_t>(clone+layout::modelSdf);
        if(sdfId){
            if(sdfId==read<uint32_t>(object(sourceHandle)+layout::modelSdf))throw std::runtime_error("motion clone SDF aliases original");
            const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=read<uint64_t>(sdf+0x78);
            if(!count)throw std::runtime_error("motion clone SDF count invalid");
            using SdfRemove=void(__fastcall*)(void*,uint32_t);
            reinterpret_cast<SdfRemove>(baseAddress+0x15a540)(reinterpret_cast<void*>(sdf),sdfId);
            *reinterpret_cast<uint32_t*>(object(handle)+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("motion clone SDF removal failed");
        }
        logFile->event("live_weapon_attachment_observation","\"item\":"+std::to_string(item)+",\"weapon_type\":"+std::to_string(current.weaponType)+
            ",\"slot\":"+std::to_string(current.slot)+",\"item_slot\":"+std::to_string(current.itemSlot)+",\"source_pose\":"+std::to_string(read<uint32_t>(object(sourceHandle)+layout::modelPose))+
            ",\"attachment_bytes\":"+jsonString(nativeImpactComponents::asHex(current.slotBytes))+",\"asset_pivot_calibrated\":false,\"read_only\":true");
        logFile->event("motion_visual_created","\"handle\":"+std::to_string(handle)+",\"source_handle\":"+std::to_string(sourceHandle)+
            ",\"item\":"+std::to_string(item)+",\"child\":"+std::to_string(child)+",\"native_mask\":0,\"sdf_handle\":0,\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(read<uint32_t>(res+0x10))+
            ",\"native_scene_lifetime\":true,\"weapon_calibration\":\""+std::string(mountKind)+"\"");
    }else{
        if(read<uint32_t>(object(handle)+layout::modelMask)||read<uint32_t>(object(handle)+layout::modelSdf)||read<uint32_t>(object(handle)+layout::modelAuxiliary))throw std::runtime_error("motion clone mask/auxiliary changed");
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},transforms{reinterpret_cast<uintptr_t>(transform.data()),1};
        change(reinterpret_cast<void*>(scene),ids.data(),transforms.data());
    }
    {auto sourceAfter=read<std::array<uint8_t,0x1b0>>(object(sourceHandle));const auto cloneAfter=read<std::array<uint8_t,56>>(object(handle)+0x38);
     // A skinned model with simulated joints (the fishing rods; game051008): when its copy is made, the Scene itself
     // rewrites six bookkeeping bytes (layout::modelBookkeeping, 1.0 +0xf0; June +0xe0) of the original. Transform, resource, mask, pose and SDF stay.
     // Those six bytes are left out of the proof for skinned originals, and the change is put on record.
     if(read<uint32_t>(object(sourceHandle)+layout::modelPose)&&!std::equal(sourceAfter.begin()+layout::modelBookkeeping,sourceAfter.begin()+layout::modelBookkeeping+6,originalBytes.begin()+layout::modelBookkeeping)){
        static unsigned sceneFieldEvents=0;
        constexpr auto at=layout::modelBookkeeping;
        if(sceneFieldEvents++<8)logFile->event("motion_visual_source_scene_field","\"item\":"+std::to_string(item)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"offset\":"+std::to_string(at)+",\"before\":"+jsonString(nativeImpactComponents::asHex(std::array<uint8_t,6>{originalBytes[at],originalBytes[at+1],originalBytes[at+2],originalBytes[at+3],originalBytes[at+4],originalBytes[at+5]}))+
            ",\"after\":"+jsonString(nativeImpactComponents::asHex(std::array<uint8_t,6>{sourceAfter[at],sourceAfter[at+1],sourceAfter[at+2],sourceAfter[at+3],sourceAfter[at+4],sourceAfter[at+5]}))+",\"written_by\":\"scene\"");
        std::copy(originalBytes.begin()+at,originalBytes.begin()+at+6,sourceAfter.begin()+at);
     }
     if(sourceAfter!=originalBytes||std::memcmp(cloneAfter.data(),transform.data(),52)){
        std::string where;unsigned listed=0;
        for(size_t i=0;i<sourceAfter.size()&&listed<16;++i)if(sourceAfter[i]!=originalBytes[i]){where+=(listed++?",":"")+std::to_string(i);}
        logFile->event("motion_visual_invariant","\"item\":"+std::to_string(item)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"source_changed_offsets\":["+where+"],\"clone_pose_differs\":"+(std::memcmp(cloneAfter.data(),transform.data(),52)?"true":"false")+
            ",\"source_pose\":"+std::to_string(read<uint32_t>(object(sourceHandle)+layout::modelPose))+",\"clone_bytes\":"+jsonString(nativeImpactComponents::asHex(cloneAfter))+",\"wanted_bytes\":"+jsonString(nativeImpactComponents::asHex(transform)));
        throw std::runtime_error("motion visual original changed or clone pose differs");}}
    if(liveEquipment())retainedVisuals[liveSelection.slot]={{item,sourceHandle,child,resource,guid},handle};
    poseReady=true;updatedSerial=serial;updatedFrame=liveFrame.frame;++updates;
    attachmentObserver::shownItem[1].serial=serial;attachmentObserver::shownItem[1].frame=liveFrame.frame;std::memcpy(attachmentObserver::shownItem[1].transform.data(),transform.data(),40);
    // Experiment: the game's own entity of the shown hand item follows the tracked item (attachment_observer.hpp).
    // The bow stays on the game's hand: its nocked arrow is found next to it (motion_arrow_visual.hpp).
    if(attachmentObserver::followRequested&&liveEquipment()&&!nativeBow&&(current.slot==8||current.slot==9)&&current.target==0){
        attachmentObserver::Follow f;f.entity=current.entity;f.parent=root.entity;f.world=current.world;f.at=GetTickCount64();f.frame=liveFrame.frame;
        std::memcpy(f.transform.data(),transform.data(),40);attachmentObserver::stampAnchor(f,anchor.root);attachmentObserver::publishFollow(f);
        // How far the game's own model stands from the tracked one this frame (the animated hand, or one frame of lag).
        static uint64_t lastFollowState=0;
        if(f.at-lastFollowState>=1000||attachmentObserver::followTrace){lastFollowState=f.at;WorldPositionQ32 native;std::memcpy(&native,originalBytes.data()+0x38,24);
            const double dx=relativeQ32(native.x,position.x),dy=relativeQ32(native.y,position.y),dz=relativeQ32(native.z,position.z);
            WorldPositionQ32 written;uint64_t writtenAt=0,writtenCount=0;{std::lock_guard<std::mutex> lock(attachmentObserver::writtenMutex);written=attachmentObserver::lastWritten;writtenAt=attachmentObserver::lastWrittenAt;writtenCount=attachmentObserver::lastWrittenCount;}
            const double wx=relativeQ32(native.x,written.x),wy=relativeQ32(native.y,written.y),wz=relativeQ32(native.z,written.z);
            const double tx=relativeQ32(written.x,position.x),ty=relativeQ32(written.y,position.y),tz=relativeQ32(written.z,position.z);
            // The anchor worked out at the attachment (attachmentObserver::lastPredictedRoot) against the one this frame is drawn with.
            std::string predicted;{std::lock_guard<std::mutex> lock(attachmentObserver::predictedMutex);
                if(attachmentObserver::lastPredictedValid)predicted=",\"predicted_minus_anchor\":["+std::to_string(relativeQ32(attachmentObserver::lastPredictedRoot.x,anchor.root.x))+","+
                    std::to_string(relativeQ32(attachmentObserver::lastPredictedRoot.y,anchor.root.y))+","+std::to_string(relativeQ32(attachmentObserver::lastPredictedRoot.z,anchor.root.z))+
                    "],\"predicted_turn_minus_anchor\":"+std::to_string(attachmentObserver::lastPredictedTurn-f.anchorTurn)+",\"anchor_sample\":"+std::to_string(f.anchorSample)+
                    ",\"predicted_tick\":["+std::to_string(attachmentObserver::lastPredictedTick)+","+std::to_string(attachmentObserver::lastPredictedFraction)+","+std::to_string(attachmentObserver::lastPredictedSourceTick)+","+std::to_string(attachmentObserver::lastPredictedNewestTick)+"]";
                try{rootCamera::Sample cs;if(rootCamera::trySnapshot(cs)){
                    auto yaw=[](Quat q){return std::atan2(2.*(double(q.w)*q.y+double(q.x)*q.z),1.-2.*(double(q.x)*q.x+double(q.y)*q.y));};
                    predicted+=",\"camera_tick\":["+std::to_string(cs.tick)+","+std::to_string(cs.fraction)+","+std::to_string(cs.history.sourceTick)+","+std::to_string(cs.history.newestTick)+"],\"camera_sample\":"+std::to_string(cs.number)+
                        ",\"yaw_anchor\":"+std::to_string(yaw(anchor.orientation))+",\"yaw_stable_root\":"+std::to_string(yaw(cs.stable.orientation))+",\"yaw_unshaken\":"+std::to_string(yaw(cs.unshaken.orientation))+
                        ",\"turn_sample\":"+std::to_string(cs.controls.gameplayTurn);}}catch(...){}}
            // The three apart (metres, x y z): drawn native model - last written, last written - tracked now. If the first is
            // not ~0, the game draws something else than what was written (an interpolated transform, say).
            logFile->event("held_follow_state","\"frame\":"+std::to_string(liveFrame.frame)+",\"item\":"+std::to_string(item)+",\"native_model_to_tracked_metres\":"+std::to_string(std::sqrt(dx*dx+dy*dy+dz*dz))+
                ",\"native_minus_written\":["+std::to_string(wx)+","+std::to_string(wy)+","+std::to_string(wz)+"],\"written_minus_tracked\":["+std::to_string(tx)+","+std::to_string(ty)+","+std::to_string(tz)+
                "],\"written_age_ms\":"+std::to_string(f.at>=writtenAt?f.at-writtenAt:0)+",\"written_count\":"+std::to_string(writtenCount)+predicted+
                ",\"writes\":"+std::to_string(attachmentObserver::followWrites.load())+",\"stale\":"+std::to_string(attachmentObserver::followStale.load())+",\"rejected\":"+std::to_string(attachmentObserver::followRejected.load()));}
    }
    if(bowAnimated&&motion::bowDrawAsset(item,guid,rangedAnimationRequested&&controllerUiFixture::bowDrawRequested))bowGeometry={(uint64_t(item)<<32)|sourceHandle,hands.frame,trackedWeapon,scale,position,orientation,current.world,root.entity};
    // Native weapon categories (weapon_category.hpp): one-hand melee 0..3 and 13, two-hand melee 7..10.
    // The two-hand group takes VHVR's heavier swing requirement. Weapon models point along +Y.
    // A long tool is swung like a one-hand weapon: the swing presses the game's use button (mining, felling).
    if(controllerUiFixture::liveMeleeRequested&&liveEquipment()&&!nativeBow&&(swungTool||(current.weaponCategoryKnown&&!tool&&
       (current.weaponType<=3||current.weaponType==13||(current.weaponType>=7&&current.weaponType<=10))))){
        meleeWeapon.binding=(uint64_t(item)<<32)|sourceHandle;meleeWeapon.heavy=!tool&&current.weaponType>=7&&current.weaponType<=10;
        meleeWeapon.twoHanded=wieldPose.active;meleeWeapon.forward=rotate(trackedWeapon.orientation,{0,1,0});
        // The weapon in the world for the target gate: from the end of the hilt to the tip along the model's +Y (0.9 m
        // past the grip when the model's extent is not known, and 0.15 m behind it for the hilt). The hilt counts since
        // the headset run 20261014-124515 (user: "is there a hit only on the blade? make the hilt hit too"); the game's
        // own hit boxes of a weapon start behind the grip as well. A tool aims with the view and needs no target.
        {float rear=0,tip=0;const bool extent=modelExtentY(resource,rear,tip);const char* reach=meleeMount.known&&meleeMount.tipY>meleeMount.gripInModel.y+.3f?"catalog":extent&&tip*scale.y>.15f?"model_extent":"default";
         const float grip=meleeMount.known?meleeMount.gripInModel.y:0.f,to=reach[0]=='c'?meleeMount.tipY:reach[0]=='m'?std::max(tip*scale.y,grip+.15f):grip+.9f;
         const float from=extent&&rear*scale.y<grip&&rear*scale.y>grip-.6f?rear*scale.y:grip-.15f;
         static uint64_t bladeBinding=0;
         if(bladeBinding!=meleeWeapon.binding){bladeBinding=meleeWeapon.binding;std::ostringstream row;row<<std::setprecision(5)<<"\"item\":"<<item<<",\"grip_y\":"<<grip<<",\"hilt_y\":"<<from<<",\"tip_y\":"<<to<<",\"source\":\""<<reach<<"\",\"model_rear_y\":"<<rear<<",\"model_tip_y\":"<<tip<<",\"tool\":"<<(tool?"true":"false");logFile->event("live_melee_blade",row.str());}
         const auto a=cameraPosition(position,rotate(orientation,Vec3{0,from,0}),anchor.worldUnitsPerMeter),b=cameraPosition(position,rotate(orientation,Vec3{0,to,0}),anchor.worldUnitsPerMeter);
         meleeWeapon.bladeFrom={a.x,a.y,a.z};meleeWeapon.bladeTo={b.x,b.y,b.z};meleeWeapon.blade=true;meleeWeapon.needsTarget=!tool;
         meleeWeapon.weaponType=tool||!current.weaponCategoryKnown?uint8_t(255):current.weaponType;meleeWeapon.reach=to-grip;meleeTipY=to;meleeTipItem=item;}
    }
    // The rod's model points along +Y; its tip is the far end of the model (1.89 m for the basic rod).
    if(controllerUiFixture::liveHandUseRequested&&liveEquipment()&&!nativeBow&&current.category==5){
        {const bool native=nativeRodWanted()&&attachmentObserver::followRequested&&current.slot==8&&current.target==0&&controllerUiFixture::fishingFloatOut(GetTickCount64());
         static int shown=-1;
         if(int(native)!=shown){shown=int(native);logFile->event("fishing_rod_shown","\"frame\":"+std::to_string(liveFrame.frame)+",\"item\":"+std::to_string(item)+",\"shown\":\""+(native?"the_game_s_own_rod":"copy_in_the_hand")+"\",\"float_out\":"+(native?"true":"false"));}
         if(native){nativeRodSerial=serial;nativeRodFrame=liveFrame.frame;}}
        float rear=0,tip=0;
        if(modelExtentY(resource,rear,tip)&&tip*scale.y>.3f){
            heldRod={(uint64_t(item)<<32)|sourceHandle,rotate(trackedWeapon.orientation,{0,1,0}),tip*scale.y};
            static uint64_t rodBinding=0;
            if(rodBinding!=heldRod.binding){rodBinding=heldRod.binding;std::ostringstream row;row<<std::setprecision(5)<<"\"item\":"<<item<<",\"tip_m\":"<<heldRod.tip<<",\"model_rear_y\":"<<rear<<",\"model_tip_y\":"<<tip<<",\"scale_y\":"<<scale.y;logFile->event("live_fishing_rod",row.str());}
        }
    }
    if(controllerUiFixture::rangedAimRequested&&liveEquipment()&&!nativeBow&&motion::rangedAimModel(item)){
        rangedGeometry={(uint64_t(item)<<32)|sourceHandle,hands.frame,trackedWeapon,scale,position,orientation,current.world,root.entity};rangedItem=item;
        rangedTwoHanded=wieldPose.active;
    }
    std::ostringstream fields;fields<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<liveFrame.frame<<",\"time\":"<<liveFrame.time
        <<",\"handle\":"<<handle<<",\"source_handle\":"<<sourceHandle<<",\"entity\":"<<root.entity<<",\"item\":"<<item
        <<",\"grip_hand\":"<<gripHand<<",\"mount_kind\":"<<jsonString(mountKind)
        <<",\"scale\":["<<scale.x<<','<<scale.y<<','<<scale.z<<']' 
        <<",\"wield_active\":"<<(wieldPose.active?"true":"false")<<",\"wield_main_y\":"<<wieldPose.mainY<<",\"wield_off_y\":"<<wieldPose.offY<<",\"wield_distance\":"<<wieldPose.distance
        <<",\"grip_position\":["<<hands.hands[gripHand].grip.position.x<<','<<hands.hands[gripHand].grip.position.y<<','<<hands.hands[gripHand].grip.position.z
        <<"],\"q32\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"orientation\":["<<orientation.x<<','<<orientation.y<<','<<orientation.z<<','<<orientation.w
        <<"],\"native_mask\":0,\"sdf_handle\":0,\"source_unchanged\":true,\"same_frame_hand\":true,\"auxiliary138\":"<<read<uint32_t>(object(handle)+layout::modelPaletteHandle);
    if(motionColliderChannel::requested){
        motion::ColliderPose p;p.valid=true;p.frame=hands.frame;p.session=hands.sessionGeneration;p.origin=hands.originGeneration;
        p.predictedTime=hands.predictedTime;p.milliseconds=GetTickCount64();p.serial=serial;p.clientWorld=current.world;
        p.root=root.entity;p.item=item;p.child=child;p.transform=transform;p.attachment=current.slotBytes;p.tipY=meleeTipItem==item?meleeTipY:0.f;
        // A weapon pair: the hit volume of an attack that the left hand struck goes to the left-hand weapon (its pose
        // of the previous frame; that part is prepared after this one).
        if(offhandStrike&&offhandStrike(p.transform))fields<<",\"collider_hand\":0";
        combatWorldBridge::observe(combatWorldBridge::Stage::Pose,p.clientWorld,0);
        combatEpochGate::pose(p);
        pendingColliderPose=p;
        fields<<",\"world_transform_bytes\":"<<jsonString(nativeImpactComponents::asHex(transform))<<",\"client_world\":"<<hex(p.clientWorld)
            <<",\"session\":"<<p.session<<",\"origin\":"<<p.origin<<",\"pose_ms\":"<<p.milliseconds;
    }
    logFile->event("motion_visual_pose",fields.str());
}
inline void disposeActive(){
    if(!handle)return;
    if(thread!=GetCurrentThreadId())throw std::runtime_error("motion visual Scene disposal thread differs");
    const auto clone=object(handle);if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf))throw std::runtime_error("motion visual disposal invariant differs");
    const auto refs=read<uint32_t>(resource+0x10),id=handle;
    std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<Remove>(baseAddress+0x10df20)(reinterpret_cast<void*>(scene),ids.data());handle=0;poseReady=false;
    const auto gens=read<uintptr_t>(scene+0x1c8);
    const bool invalid=read<uint8_t>(gens+(id&0xffffff))!=uint8_t(id>>24);
    // Resource can be freed if the original item was already unequipped. Read
    // its count only while another native reference was present.
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+0x10)==refs-1);
    logFile->event("motion_visual_disposed","\"handle\":"+std::to_string(id)+",\"generation_invalidated\":"+(invalid?"true":"false")+
        ",\"reference_released\":"+(released?"true":"false")+",\"scene_disposal\":true,\"updates\":"+std::to_string(updates));
    if(!invalid||!released)throw std::runtime_error("motion visual disposal failed");
}
inline void dispose(uintptr_t s){
    motionColliderChannel::invalidate();
    if(s!=scene)return;
    // Include an incomplete active creation when preparation failed before it
    // could be registered. Each successful clone is removed exactly once.
    const auto activeHandle=handle;bool registered=false;
    for(const auto& e:retainedVisuals)if(e.clone&&e.clone==activeHandle)registered=true;
    if(activeHandle&&!registered)disposeActive();
    for(auto& e:retainedVisuals)if(e.clone){handle=e.clone;resource=e.identity.resource;disposeActive();e={};}
    handle=0;poseReady=false;
}
}
