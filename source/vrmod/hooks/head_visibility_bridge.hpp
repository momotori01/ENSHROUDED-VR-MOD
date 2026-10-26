// VR-only culling payload. No Player/Scene model field is modified.
namespace headVisibility {
inline bool requested=false,enabled=false,failed=false,handsOnly=false;
inline void* target=nullptr;
inline constexpr const char* pattern="E8 ?? ?? ?? ?? 0F 11 74 24 38 C6 44 24 30 07";
inline std::array<uint64_t,2> written{},skipped{},bowPayloadSerial{};
inline std::atomic<uint64_t> originalCalls{0};
struct FrameSample {game::BodyMotionSample motion;uint64_t renderEntryMs=0;bool motionOverride=false;equipmentObserver::Snapshot equipment;uint64_t serial=0,entryAge=0;uint8_t cameraId=255;bool nativeCamera=false,syntheticPresentation=false;};
inline thread_local const FrameSample* frame=nullptr;
struct FrameScope {
    const FrameSample* previous=frame;FrameSample value;
    FrameScope(bool use) noexcept {
        if(!use)return;
        try{const auto root=rootCamera::snapshot();value.cameraId=root.cameraId;value.syntheticPresentation=root.syntheticPresentation;value.serial=root.renderSerial;value.renderEntryMs=root.renderEntryMilliseconds;
            const bool fp=rootCamera::firstPerson(root);
            if(inputActor::motionObserverRequested){
                {std::lock_guard<std::mutex> actorLock(inputActor::motionMutex);value.motion=inputActor::motionLatest;}
                value.motionOverride=game::bodyMotionOverride(value.motion,root.entity,value.renderEntryMs,fp,root.syntheticPresentation);
                logFile->event("body_motion_frame","\"serial\":"+std::to_string(value.serial)+",\"root_sample\":"+std::to_string(root.number)+
                    ",\"root_entity\":"+std::to_string(root.entity)+",\"render_entry_ms\":"+std::to_string(value.renderEntryMs)+
                    ",\"actor_sample\":"+std::to_string(value.motion.number)+",\"actor_ms\":"+std::to_string(value.motion.milliseconds)+
                    ",\"actor_entity\":"+std::to_string(value.motion.entity)+",\"actor_state\":"+std::to_string(value.motion.state)+
                    ",\"known\":"+(value.motion.known?"true":"false")+",\"override\":"+(value.motionOverride?"true":"false"));
            }
            if(!fp){value.nativeCamera=true;frame=&value;return;}std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);
            const auto& s=equipmentObserver::latest;const auto now=GetTickCount64();
            // While the game is paused (the pause menu and the settings, screen 2) the equipment is not sampled any
            // more - and does not change: the last sample stays good. Without this the head came back a second after
            // the pause menu opened, and the eyes looked at the inside of the player's own face behind the menu
            // (Mock runs of 2026-10-13, e.g. game-20261013-033851).
            const bool paused=menuPointer::screen.load()==2;
            if(s.number&&s.milliseconds&&(now-s.milliseconds<=1000||paused)&&s.entity==root.entity&&s.scene==provenWorldScene){
                value.equipment=s;value.serial=root.renderSerial;value.entryAge=now-s.milliseconds;}}
        catch(...){}frame=&value;
    }
    ~FrameScope(){frame=previous;}
};
inline void onCopy(uintptr_t rsp) noexcept {
    const ObserverLastError preserve;
    if(!requested||!enabled)return;
    if(!cullingHistory::current||cullingHistory::currentEye<0||!cullingHistory::prepareGraph){++originalCalls;return;}
    auto& pair=*cullingHistory::current;const unsigned eye=unsigned(cullingHistory::currentEye);
    try{
        if(eye>1||failed||pair.history.failed||pair.renderThread!=GetCurrentThreadId()||cullingHistory::fillOnly)
            throw std::runtime_error("head payload outside validated private culling scope");
        const auto args=observerEntryArguments(rsp);const auto address=args[1];
        ULONG_PTR stackLow=0,stackHigh=0;GetCurrentThreadStackLimits(&stackLow,&stackHigh);
        if(args[2]!=head::constantBytes||!args[0]||address<stackLow||address>stackHigh-head::constantBytes)
            throw std::runtime_error("head culling constants are not the bounded native stack array");
        auto before=read<std::array<uint8_t,head::constantBytes>>(address);
        if(read<head::Payload>(address+head::offset)!=head::Payload{})throw std::runtime_error("native culling padding is not zero");
        const auto subviews=read<uint32_t>(address+0xfe0),passes=read<uint32_t>(address+0xfe4),cpuCount=read<uint32_t>(address+0xff4);
        if(subviews>11||passes>12||cpuCount>65536)throw std::runtime_error("head culling parameter layout differs");
        sceneryCulling::observe(address,pair,eye);
        sceneryCulling::disableHzb(address,pair,eye,before);
        if(frame&&frame->nativeCamera&&frame->serial==pair.serial){
            logFile->event("head_visibility_native_camera","\"eye\":"+std::to_string(eye)+",\"serial\":"+std::to_string(pair.serial)+
                ",\"culling_flags\":"+std::to_string(read<uint32_t>(address+0xfec))+",\"payload_unchanged\":true");return;
        }
        if(!frame||!frame->equipment.number||frame->serial!=pair.serial){
            if(++skipped[eye]<=2)logFile->event("head_visibility_fallback","\"eye\":"+std::to_string(eye)+",\"reason\":\"no current equipment snapshot\",\"payload_unchanged\":true");return;}
        const auto& s=frame->equipment;
        if(s.scene!=pair.scene)throw std::runtime_error("head equipment belongs to another Scene");
        const auto objects=read<uintptr_t>(pair.scene+0x1b0),generations=read<uintptr_t>(pair.scene+0x1c8);
        const auto objectBytes=read<uint64_t>(pair.scene+0x1b8),generationCount=read<uint64_t>(pair.scene+0x1d0);
        if(objectBytes%0x1b0||!objects||!generations)throw std::runtime_error("head model tables unavailable");
        std::vector<unsigned> parts{0,1,2,3,15};
        // Body IK experiment: the body is drawn on the mod's pose, so only the head parts (and the bare-hand
        // model, which stays on the game's pose under the private hands) are kept out of the eyes.
        const bool bodyShown=motionBodyIk::shown(pair.serial);
        if(bodyShown)parts.push_back(11);
        // First person with the pad (2026-10-25): the native body is shown (no tracked hands stand for it), but the head, the
        // neck, the collar, the chest and the upper armour - the shoulder pieces and the breastplate, right before the eyes
        // (Mock 20261025-115157) - are left out; the arms, the hands with what they hold, and the legs stay. More than the
        // head payload's five: the hands-only payload (body::packAppearance) carries them, with no private model of the mod's.
        const bool padFirst=game::padStyle.load()==1;
        if(handsOnly&&!bodyShown&&padFirst)parts={0,1,2,3,4,5,6,15,16};
        else if(handsOnly&&!bodyShown){parts.clear();for(unsigned n=0;n<(motionVisual::requested?s.handles.size():22);n++)if(motionVisual::requested||n!=11)parts.push_back(n);}
        std::vector<uint32_t> indices;
        for(auto part:parts)if(s.handles[part]){
            const auto handle=s.handles[part],index=handle&0xffffff;
            if(index>=cpuCount||index>=objectBytes/0x1b0||index>=generationCount||read<uint8_t>(generations+index)!=(handle>>24))
                throw std::runtime_error("head handle stale at culling boundary");
            const auto object=objects+uintptr_t(index)*0x1b0,resource=read<uintptr_t>(object+layout::modelResource);
            if(!resource||s.ready[part]!=1||read<uint8_t>(resource+0x14)!=1||read<uint16_t>(resource+layout::resourceModel)!=s.models[part]||read<uint32_t>(object+layout::modelMask)!=s.masks[part])
                throw std::runtime_error("head model changed since equipment snapshot");
            for(unsigned body=0;body<s.handles.size();body++)if(std::find(parts.begin(),parts.end(),body)==parts.end()&&s.handles[body]==handle)
                throw std::runtime_error("head and body share a model handle");
            indices.push_back(index);
        }
        head::Payload payload{};uint32_t hand=UINT32_MAX,extra=UINT32_MAX,shield=UINT32_MAX,handMask=0;bool arrowShown=false,offhandShown=false,consumableShown=false,modBowShown=false;
        // Gloves: where the equipment covers a body part the game does not draw that part - the part's model has
        // the mask 0 (run game-20261013-084345: the parts with mask 0 are exactly the bits of the equipment's hide
        // mask 0x407fda at +300 - hair, beard, neck, chest .. feet, the hands among them - and the parts that are
        // seen, head, collar and the armour, have ffffffff). With the body shown on the mod's pose the gloves are on
        // the hands, and the copy of the bare hands inside them is left out as the game leaves its original out:
        // the pads of its fingertips came through the gloves (headset run game-20261013-005905). The copy's pose
        // is still made - the body takes the fingers from it. EVR_HANDS_UNDER_GLOVES=1 draws the copy as before.
        static const bool underGloves=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_HANDS_UNDER_GLOVES",value,sizeof value);return n&&n<sizeof value&&value[0]=='1';}();
        const bool handsCovered=bodyShown&&s.handles[11]&&s.masks[11]==0&&!underGloves;
        // Headset run game-20261013-105615: "something at the gloves showed double and flickered at times". The record of
        // every payload ends after the first seconds, so each change of this decision is put on record by itself: the
        // bare hands' copy and the gloves are both drawn where it turns false while the gloves stay on.
        {static int lastCovered=-1;static unsigned coveredLogs=0;
            if(int(handsCovered)!=lastCovered&&coveredLogs<300){lastCovered=int(handsCovered);++coveredLogs;
                logFile->event("hands_covered_changed","\"serial\":"+std::to_string(pair.serial)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"hands_covered\":"+(handsCovered?"true":"false")+
                    ",\"body_shown\":"+(bodyShown?"true":"false")+",\"hand_part_handle\":"+(s.handles[11]?"true":"false")+",\"hand_part_mask\":"+std::to_string(s.masks[11])+",\"read_only\":true");}}
        if(handsOnly){
            if(motionVisual::requested){
                // Hide only ownership-validated appearance instances. Effects,
                // projectiles and unclassified child entities are not swept in.
                // The fishing rod while its float is out: the game's own rod stays in, the copy goes out (motion_visual_bridge.hpp).
                const bool nativeRod=motionVisual::nativeRodShown(pair.serial)&&motionVisual::poseReady&&motionVisual::updatedSerial==pair.serial&&motionVisual::updatedFrame==liveFrame.frame&&motionVisual::handle;
                if(!nativeRod&&motionVisual::contextReady&&motionVisual::contextSerial==pair.serial&&motionVisual::contextFrame==liveFrame.frame&&motionVisual::hiddenSource.handle){
                    const auto& hidden=motionVisual::hiddenSource;const auto model=motionVisual::object(hidden.handle),res=read<uintptr_t>(model+layout::modelResource);
                    if(hidden.parent!=s.entity||!res||read<uint8_t>(res+0x14)!=1||read<uint16_t>(res+layout::resourceModel)!=hidden.model||
                       read<std::array<uint32_t,4>>(res+0x18)!=hidden.modelGuid)
                        throw std::runtime_error("motion hidden source changed at culling boundary");
                    indices.push_back(hidden.handle&0xffffff);
                }
                if(motionVisual::hideHeldAttachments){
                    unsigned added=0;
                    for(const auto held:attachmentObserver::heldHandles(s.entity,pair.scene,GetTickCount64(),true)){
                        const auto index=held&0xffffff;
                        if(held==motionVisual::handle||(nativeRod&&held==motionVisual::sourceHandle)||index>=cpuCount||index>=objectBytes/0x1b0||index>=generationCount||read<uint8_t>(generations+index)!=(held>>24))continue;
                        const auto object=objects+uintptr_t(index)*0x1b0;if(!read<uintptr_t>(object+layout::modelResource))continue;
                        if(indices.size()>=body::maximumModels)break; // payload capacity (24); body parts come first
                        if(std::find(indices.begin(),indices.end(),index)==indices.end()){indices.push_back(index);++added;}
                    }
                    static unsigned lastAdded=UINT_MAX;
                    if(added!=lastAdded){lastAdded=added;logFile->event("held_attachments_hidden","\"count\":"+std::to_string(added)+",\"serial\":"+std::to_string(pair.serial));}
                }
                hand=UINT32_MAX;
                if(motionVisual::poseReady&&motionVisual::updatedSerial==pair.serial&&motionVisual::updatedFrame==liveFrame.frame&&motionVisual::handle){
                    const auto object=motionVisual::object(motionVisual::handle);hand=motionVisual::handle&0xffffff;
                    if(hand>=cpuCount){hand=UINT32_MAX;}
                    else if(read<uint32_t>(object+layout::modelMask)||read<uint32_t>(object+layout::modelSdf)||read<uintptr_t>(object+layout::modelResource)!=motionVisual::resource)
                        throw std::runtime_error("motion private culling invariant changed");
                    else if(nativeRod){if(indices.size()<body::maximumModels&&std::find(indices.begin(),indices.end(),hand)==indices.end())indices.push_back(hand);hand=UINT32_MAX;}
                }
            }else if(padFirst){
                // The first-person pad style (2026-10-25 evening, user: "the cape covers the eyes while gliding - hide it"): what
                // hangs on the body (the glider, what is carried on the back) is left out; what the hands hold stays.
                unsigned added=0;
                for(const auto held:attachmentObserver::heldHandles(s.entity,pair.scene,GetTickCount64(),true,true)){
                    const auto index=held&0xffffff;
                    if(index>=cpuCount||index>=objectBytes/0x1b0||index>=generationCount||read<uint8_t>(generations+index)!=(held>>24))continue;
                    const auto object=objects+uintptr_t(index)*0x1b0;if(!read<uintptr_t>(object+layout::modelResource))continue;
                    if(indices.size()>=body::maximumModels)break; // payload capacity (24); body parts come first
                    if(std::find(indices.begin(),indices.end(),index)==indices.end()){indices.push_back(index);++added;}
                }
                static unsigned lastAdded=UINT_MAX;static unsigned addedLogs=0;
                if(added!=lastAdded&&addedLogs<200){lastAdded=added;++addedLogs;logFile->event("pad_first_attachments_hidden","\"count\":"+std::to_string(added)+",\"serial\":"+std::to_string(pair.serial));}
            }else{
            const auto handle=s.handles[11];hand=handle&0xffffff;
            if(!handle||hand>=cpuCount||hand>=objectBytes/0x1b0||hand>=generationCount||read<uint8_t>(generations+hand)!=(handle>>24))throw std::runtime_error("local hand ownership/generation unavailable");
            const auto object=objects+uintptr_t(hand)*0x1b0,resource=read<uintptr_t>(object+layout::modelResource);handMask=read<uint32_t>(object+layout::modelMask);
            if(!resource||s.ready[11]!=1||read<uint8_t>(resource+0x14)!=1||read<uint16_t>(resource+layout::resourceModel)!=s.models[11]||handMask!=s.masks[11])throw std::runtime_error("local hand resource changed");
            }
            // The mod's bow (motion_mod_bow_visual.hpp) takes the hand item's private slot: the game's hand is bare meanwhile.
            if(hand==UINT32_MAX&&motionModBow::shown(pair.serial)){
                const auto model=motionVisual::object(motionModBow::handle);const auto index=motionModBow::handle&0xffffff;
                if(read<uintptr_t>(model+layout::modelResource)!=motionModBow::resource||read<uint32_t>(model+layout::modelMask)||read<uint32_t>(model+layout::modelSdf))
                    throw std::runtime_error("private mod bow culling invariant differs");
                if(index<cpuCount){hand=index;modBowShown=true;}
            }
            // ... and the game's shield is out of the eyes meanwhile (motion_shield_visual.hpp stowed).
            if(motionShieldVisual::requested&&motionShieldVisual::stowed(pair.serial)){
                const auto sourceIndex=motionShieldVisual::stowedSource&0xffffff;
                if(motionVisual::currentObject(motionShieldVisual::stowedSource)&&sourceIndex<cpuCount&&indices.size()<body::maximumModels-1&&
                   std::find(indices.begin(),indices.end(),sourceIndex)==indices.end())indices.push_back(sourceIndex);
            }
            // The consumable held in the right hand (motion_consumable_visual.hpp) takes the weapon's private slot - the hand
            // item is put away while one is held - or the third one while the weapon is still in the hand.
            uint32_t consumable=UINT32_MAX;
            if(motionConsumableVisual::shown(pair.serial)){
                const auto model=motionVisual::object(motionConsumableVisual::handle);consumable=motionConsumableVisual::handle&0xffffff;
                if(read<uintptr_t>(model+layout::modelResource)!=motionConsumableVisual::resource||read<uint32_t>(model+layout::modelMask)||read<uint32_t>(model+layout::modelSdf))
                    throw std::runtime_error("private consumable culling invariant differs");
                if(consumable>=cpuCount)consumable=UINT32_MAX;
                else if(hand==UINT32_MAX){hand=consumable;consumableShown=true;}
            }
            if(!handsCovered&&motionHandVisual::requested&&motionHandVisual::ready&&motionHandVisual::serial==pair.serial&&motionHandVisual::frame==liveFrame.frame&&motionHandVisual::handle){
                const auto object=motionVisual::object(motionHandVisual::handle);extra=motionHandVisual::handle&0xffffff;
                if(extra>=cpuCount)extra=UINT32_MAX;
                else if(read<uint32_t>(object+layout::modelMask)||read<uint32_t>(object+layout::modelSdf)||read<uint32_t>(object+layout::modelPose)!=(motionHandVisual::lentPose?motionHandVisual::lentPose:motionHandVisual::poseHandle)||read<uintptr_t>(object+layout::modelResource)!=motionHandVisual::resource)
                    throw std::runtime_error("private hand culling invariant changed");
            }
            if(motionShieldVisual::requested&&motionShieldVisual::ready&&motionShieldVisual::serial==pair.serial&&motionShieldVisual::frame==liveFrame.frame){
                const auto model=motionVisual::object(motionShieldVisual::handle);shield=motionShieldVisual::handle&0xffffff;
                if(shield>=cpuCount||read<uintptr_t>(model+layout::modelResource)!=motionShieldVisual::resource||read<uint32_t>(model+layout::modelMask)||read<uint32_t>(model+layout::modelSdf))
                    throw std::runtime_error("private shield culling invariant differs");
                motionVisual::object(motionShieldVisual::sourceHandle);
                const auto sourceIndex=motionShieldVisual::sourceHandle&0xffffff;
                if(std::find(indices.begin(),indices.end(),sourceIndex)==indices.end())indices.push_back(sourceIndex);
            }
            // The left-hand part of a weapon pair takes the third private slot (no shield is shown with a pair).
            if(shield==UINT32_MAX&&motionOffhandVisual::requested&&motionOffhandVisual::ready&&motionOffhandVisual::serial==pair.serial&&motionOffhandVisual::frame==liveFrame.frame){
                const auto model=motionVisual::object(motionOffhandVisual::handle);const auto index=motionOffhandVisual::handle&0xffffff;
                if(read<uintptr_t>(model+layout::modelResource)!=motionOffhandVisual::resource||read<uint32_t>(model+layout::modelMask)||read<uint32_t>(model+layout::modelSdf))
                    throw std::runtime_error("private off-hand culling invariant differs");
                const auto sourceIndex=motionOffhandVisual::sourceHandle&0xffffff;
                if(motionVisual::currentObject(motionOffhandVisual::sourceHandle)&&sourceIndex<cpuCount&&indices.size()<body::maximumModels-1&&
                   std::find(indices.begin(),indices.end(),sourceIndex)==indices.end())indices.push_back(sourceIndex);
                if(index<cpuCount&&indices.size()<body::maximumModels){shield=index;offhandShown=true;}
            }
            // The things this guest broke on its own PC, until the host's break (live_melee_targets.hpp guestBreak, 2026-10-22).
            {std::array<uint32_t,4> broken{};const auto n=liveTargets::hiddenThingHandles(broken.data(),broken.size(),GetTickCount64());
                for(size_t k=0;k<n;++k){const auto h=broken[k];const auto i=h&0xffffff;
                    if(motionVisual::currentObject(h)&&i<cpuCount&&indices.size()<body::maximumModels-2&&std::find(indices.begin(),indices.end(),i)==indices.end())indices.push_back(i);}}
            // The game's copies of the own shot (motion_arrow_visual.hpp foreignArrows, 2026-10-22): out of the eyes.
            if(motionArrowVisual::requested&&motionArrowVisual::foreignSerial==pair.serial&&motionArrowVisual::foreignFrame==liveFrame.frame)
                for(size_t k=0;k<motionArrowVisual::foreignCount;++k){const auto h=motionArrowVisual::foreign[k];const auto i=h&0xffffff;
                    if(motionVisual::currentObject(h)&&i<cpuCount&&indices.size()<body::maximumModels-2&&std::find(indices.begin(),indices.end(),i)==indices.end())indices.push_back(i);}
            // The game's nocked arrow for a second after it stopped being the arrow at the game's bow (motion_arrow_visual.hpp lostSource,
            // 2026-10-26: after a release it stays on the original model's hand about 0.3 s, and showed there).
            if(motionArrowVisual::requested&&motionArrowVisual::lostHiddenSerial==pair.serial&&motionArrowVisual::lostHiddenFrame==liveFrame.frame&&motionArrowVisual::lostSource){
                const auto h=motionArrowVisual::lostSource;const auto i=h&0xffffff;
                if(motionVisual::currentObject(h)&&i<cpuCount&&indices.size()<body::maximumModels-2&&std::find(indices.begin(),indices.end(),i)==indices.end())indices.push_back(i);}
            // The nocked arrow takes the third private slot while no shield is shown (a bow fills the shield hand).
            // The game's own arrow stays out of the eyes for as long as it is at the game's bow.
            if(motionArrowVisual::requested&&motionArrowVisual::hiddenSerial==pair.serial&&motionArrowVisual::hiddenFrame==liveFrame.frame&&motionArrowVisual::source){
                const auto sourceIndex=motionArrowVisual::source&0xffffff;
                if(motionVisual::currentObject(motionArrowVisual::source)&&sourceIndex<cpuCount&&indices.size()<body::maximumModels-1&&
                   std::find(indices.begin(),indices.end(),sourceIndex)==indices.end())indices.push_back(sourceIndex);
                if(shield==UINT32_MAX&&motionArrowVisual::ready&&motionArrowVisual::serial==pair.serial&&motionArrowVisual::frame==liveFrame.frame){
                    const auto model=motionVisual::object(motionArrowVisual::handle);const auto index=motionArrowVisual::handle&0xffffff;
                    if(read<uintptr_t>(model+layout::modelResource)!=motionArrowVisual::resource||read<uint32_t>(model+layout::modelMask)||read<uint32_t>(model+layout::modelSdf))
                        throw std::runtime_error("private arrow culling invariant differs");
                    if(index<cpuCount&&indices.size()<body::maximumModels){shield=index;arrowShown=true;}
                }
            }
            if(!consumableShown&&consumable!=UINT32_MAX&&shield==UINT32_MAX&&consumable!=hand&&consumable!=extra){shield=consumable;consumableShown=true;}
            const auto owned=body::packAppearance(indices,hand==UINT32_MAX?0:hand+1,extra==UINT32_MAX?0:extra+1,shield==UINT32_MAX?0:shield+1);uint64_t gpuAddress=0;
            using GetPayload=int(*)(VkDevice,const body::Payload*,uint64_t*);
            const auto module=GetModuleHandleW(L"evr_diagnostic.dll");const auto get=module?reinterpret_cast<GetPayload>(GetProcAddress(module,"evrBodyClipPayload")):nullptr;
            const auto device=read<VkDevice>(read<uintptr_t>(pair.graphics+0x278)+layout::backendDevice);
            // No payload (2026-10-25: the layer's store full in a player's boss fight, "body immutable payload budget exhausted"):
            // nothing is written yet - this eye keeps the game's own constants this frame (the head may show for a moment) and
            // the eyes go on, as without an equipment snapshot above. Before, it stopped the eyes for good.
            if(!get||get(device,&owned,&gpuAddress)||!gpuAddress){
                if(++skipped[eye]<=20||skipped[eye]%600==0)logFile->event("head_visibility_fallback","\"eye\":"+std::to_string(eye)+",\"reason\":\"no appearance payload\",\"payload_unchanged\":true,\"skips\":"+std::to_string(skipped[eye]));
                return;}
            payload={uint32_t(gpuAddress),uint32_t(gpuAddress>>32),owned.tag};
        }else payload=head::pack(indices);
        auto expected=before;
        std::memcpy(expected.data()+head::offset,payload.data(),sizeof(payload));
        std::memcpy(reinterpret_cast<void*>(address+head::offset),payload.data(),sizeof(payload));
        if(read<std::array<uint8_t,head::constantBytes>>(address)!=expected)throw std::runtime_error("head constants changed outside payload");
        ++written[eye];
        // Bounded diagnostic log: every update supports same-frame/eye and
        // equipment-transition checks; no proprietary shader bytes are logged.
        std::ostringstream out;out<<"\"eye\":"<<eye<<",\"serial\":"<<pair.serial<<",\"equipment_sample\":"<<s.number
            <<",\"entity\":"<<s.entity<<",\"hands_only\":"<<(handsOnly?"true":"false")<<",\"motion_visual\":"<<(motionVisual::requested?"true":"false")<<",\"hand_instance\":"<<hand<<",\"extra_instance\":"<<extra<<",\"shield_instance\":"<<shield<<",\"hands_covered\":"<<(handsCovered?"true":"false")<<",\"hand_mask_before\":"<<handMask<<",\"snapshot_entry_age_ms\":"<<frame->entryAge<<",\"culling_flags\":"<<read<uint32_t>(address+0xfec)
            <<",\"payload\":["<<payload[0]<<','<<payload[1]<<','<<payload[2]<<"],\"indices\":[";
        for(size_t n=0;n<indices.size();n++){if(n)out<<',';out<<indices[n];}
        out<<"],\"only_private_stack_padding_changed\":true,\"handles_revalidated\":true";
        logFile->event("head_visibility_payload",out.str());
        if(arrowShown&&motionArrowVisual::payloadSerial[eye]!=pair.serial){
            motionArrowVisual::payloadSerial[eye]=pair.serial;
            logFile->event("motion_arrow_eye_payload",out.str()+",\"frame\":"+std::to_string(liveFrame.frame));
        }
        if(consumableShown&&motionConsumableVisual::payloadSerial[eye]!=pair.serial){
            motionConsumableVisual::payloadSerial[eye]=pair.serial;
            static unsigned told=0;if(told++<12)logFile->event("consumable_model_eye_payload",out.str()+",\"frame\":"+std::to_string(liveFrame.frame)+",\"consumable_instance\":"+std::to_string(motionConsumableVisual::handle&0xffffff));
        }
        if(modBowShown&&motionModBow::payloadSerial[eye]!=pair.serial){
            motionModBow::payloadSerial[eye]=pair.serial;
            static unsigned told=0;if(told++<12)logFile->event("mod_bow_eye_payload",out.str()+",\"frame\":"+std::to_string(liveFrame.frame)+",\"bow_instance\":"+std::to_string(motionModBow::handle&0xffffff));
        }
        if(offhandShown&&motionOffhandVisual::payloadSerial[eye]!=pair.serial){
            motionOffhandVisual::payloadSerial[eye]=pair.serial;
            logFile->event("motion_offhand_eye_payload",out.str()+",\"frame\":"+std::to_string(liveFrame.frame));
        }
        if(shield!=UINT32_MAX&&!arrowShown&&!offhandShown&&motionShieldVisual::payloadSerial[eye]!=pair.serial){
            motionShieldVisual::payloadSerial[eye]=pair.serial;
            logFile->event("motion_shield_eye_payload",out.str()+",\"frame\":"+std::to_string(liveFrame.frame));
        }
        // Keep the opt-in bow evidence separate: startup can exhaust the general
        // culling log before the bow appears. This does not change the payload.
        if(motionVisual::poseReady&&
           motionVisual::updatedSerial==pair.serial&&motionVisual::updatedFrame==liveFrame.frame&&
           motionBowVisual::profile()){
            const auto model=motionVisual::object(motionVisual::handle);
            const auto pose=read<uint32_t>(model+layout::modelPose);
            bool owned=false;
            for(const auto& entry:motionBowVisual::owned)if(entry.model==motionVisual::handle&&entry.pose==pose&&pose)owned=true;
            if(!owned||hand!=(motionVisual::handle&0xffffff)||
               std::find(indices.begin(),indices.end(),motionVisual::sourceHandle&0xffffff)==indices.end())
                throw std::runtime_error("private bow culling ownership differs");
            // Native graph may upload this payload twice for one eye. Validate
            // every call, but record one same-frame proof per eye/serial.
            if(bowPayloadSerial[eye]!=pair.serial){
                bowPayloadSerial[eye]=pair.serial;
                logFile->event((motion::reviewedBowFacing(motionVisual::item,motionVisual::guid)||(motionVisual::rangedAnimationRequested&&motion::catalogBowAsset(motionVisual::item,motionVisual::guid)))?"motion_bow_eye_payload":"motion_ranged_eye_payload",out.str()+",\"frame\":"+std::to_string(liveFrame.frame)+
                    ",\"pose_handle\":"+std::to_string(pose));
            }
        }
    }catch(const std::exception& e){failed=true;pair.history.failed=true;
        try{logFile->event("head_visibility_failure","\"reason\":"+jsonString(e.what()));}catch(...){} }
}
inline void install(){
    using Status=uint32_t(*)();auto module=GetModuleHandleW(L"evr_diagnostic.dll");
    auto status=module?reinterpret_cast<Status>(GetProcAddress(module,handsOnly?"evrHandsOnlyShaderStatus":"evrHeadVisibilityShaderStatus")):nullptr;
    if(!status||!status())throw std::runtime_error("dynamic head shader was not prepared at process launch");
    target=reinterpret_cast<void*>(baseAddress+0xae57e);const auto bytes=parsePattern(pattern);
    if(scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||
       uintptr_t(int64_t(baseAddress+0xae583)+read<int32_t>(baseAddress+0xae57f))!=baseAddress+0x13ad610)   // the call's rel32 (the matcher took 0x80db9 to the call itself)
        throw std::runtime_error("head constant upload signature/target differs");
    // Native 7fd10 contains legacy SSE, with no live upper AVX registers.
    // Reuse camera_stub.asm's tested full GPR/flags/x87/XMM preservation.
    if(MH_CreateHook(target,reinterpret_cast<void*>(&evr_camera_stub),&evr_camera_trampoline)!=MH_OK||MH_EnableHook(target)!=MH_OK)
        throw std::runtime_error("head constant upload hook failed");
    enabled=true;logFile->event("head_visibility_enabled","\"rva\":\"0xae57e\",\"pattern\":"+jsonString(pattern)+",\"payload_offset\":4132,\"payload_bytes\":12,\"shader_modules\":"+std::to_string(status()));
}
inline void disable(){
    if(!enabled)return;
    if(MH_DisableHook(target)!=MH_OK)throw std::runtime_error("head constant hook disable failed");enabled=false;
    const auto bytes=parsePattern(pattern);const bool restored=scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("head_visibility_removed","\"original_bytes_restored\":"+std::string(restored?"true":"false")+
        ",\"writes\":["+std::to_string(written[0])+","+std::to_string(written[1])+"],\"skips\":["+std::to_string(skipped[0])+","+std::to_string(skipped[1])+"],\"original_calls_untouched\":"+std::to_string(originalCalls.load()));
    if(!restored)throw std::runtime_error("head constant upload bytes not restored");
}
}
