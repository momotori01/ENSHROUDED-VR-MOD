// Explicit diagnostic: native bare hands bound to a private RenderPose.
// Both branch roots follow one same-frame OpenXR sample; original bones stay read-only.
namespace motionHandVisual {
inline bool requested=false,ready=false;inline uint32_t handle=0,poseHandle=0,sourceHandle=0,sourcePose=0;
inline uintptr_t resource=0,poseResource=0;inline uint64_t serial=0,frame=0,updates=0;
inline std::unique_ptr<hand::Rig> rig;
inline std::vector<hand::Joint> lastOutput; // this frame's joints of the private hands (the body IK copies the fingers)
inline std::array<hand::FingerCurl,2> lastCurls{}; // ... and the curl amounts they were made from (what other players would be sent)
// Finger curl (2026-10-09): closed-hand pose from the grip calibration (world-hand-curl.txt + hand-curl-closed-pose.bin).
inline std::vector<hand::Joint> closedPose;inline float curlRelaxed=.15f;inline uint64_t curlLogged=0;
struct ThumbSeen {bool active=false,touch=false;};inline std::array<ThumbSeen,2> thumbSeen{};inline uint64_t thumbChanges=0;
using PoseCreate=uint32_t(__fastcall*)(void*,const void*);
using PoseUpdate=void(__fastcall*)(void*,uint32_t,const void*);
using PoseRemove=void(__fastcall*)(void*,uint32_t);
using Bind=void(__fastcall*)(void*,const void*,uint32_t);
inline uintptr_t s(){return motionVisual::scene;}
inline std::vector<uint8_t> snapshot(uintptr_t p,size_t size){
    if(!p||!size||size>65536)throw std::runtime_error("hand visual read bound");
    std::vector<uint8_t> out(size);SIZE_T n=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out.data(),size,&n)||n!=size)throw std::runtime_error("hand visual read failed");return out;
}
template<class T> inline std::vector<T> array(uintptr_t field,size_t expected){
    const auto offset=read<int32_t>(field);const auto count=read<uint32_t>(field+4);
    if(!offset||offset < -16777216||offset>16777216||count!=expected||expected>256)throw std::runtime_error("hand visual blob shape differs");
    const auto b=snapshot(uintptr_t(int64_t(field)+offset),count*sizeof(T));std::vector<T> out(count);std::memcpy(out.data(),b.data(),b.size());return out;
}
inline uintptr_t joints(uintptr_t pose){
    const auto first=read<uint32_t>(pose+8),count=read<uint32_t>(pose+12);
    const auto data=read<uintptr_t>(s()+layout::sceneJoints),capacity=read<uint64_t>(s()+layout::sceneJointCapacity);
    if(!data||count!=102||capacity>1000000||uint64_t(first)+count>capacity)throw std::runtime_error("hand visual joint storage differs");return data+size_t(first)*48;
}
inline void validate(){
    for(auto e:std::array<std::pair<uintptr_t,const char*>,4>{{
      {0xfb8e0,"40 55 53 56 41 57 48 8d 6c 24 c1 48 81 ec 88 00 00 00 48 8b 42 08 33 db"},
      {0x111060,"85 d2 0f 84 ? ? ? ? 48 89 74 24 20 41 56 48 83 ec 30 48 8b f1 44 0f"},
      {0x10e260,"85 d2 0f 84 ? ? ? ? 48 89 5c 24 20 57 48 83 ec 20 48 8b f9 8b da 48"},
      {0x110960,"48 89 5c 24 18 48 89 6c 24 20 56 41 54 41 57 48 83 ec 20 45 33 e4 41 8b"}}}){
        const auto p=parsePattern(e.second);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+e.first),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("hand visual signature mismatch");
        logFile->event("motion_hand_signature","\"rva\":"+hex(e.first)+",\"pattern\":"+jsonString(e.second)+",\"matched\":true");}
}
}
#include "native_grip_observer.hpp"
namespace motionHandVisual {
// While the player's own model is driven (motion_body_ik.hpp) the copy of the bare hands is bound to that pose, at
// the body's transform, instead of this file's private pose: 0, or the pose it is on. The private pose is still
// updated every frame - the body takes its finger joints from it - but then no model is bound to it.
inline uint32_t lentPose=0;
// The character's colours (the skin among them) belong to the model instance, not to the model: four values at +0x1a0
// of the model record, which the game sets through dc340(scene, handle, index, value) - that also marks the record as
// changed. A copy made from the model alone has none, and skin without its colour is black (user, 2026-10-14: "the
// bare hands' skin was black", and that black skin is what showed double with the gauntlets; naked save,
// game-20261014-021053. Both records side by side, game-20261014-022938: the game's 2f0e0600 0c060000 3a1e0c00 0,
// the copy's all zero). The copy takes the source's whenever they differ, through the game's own setter.
inline unsigned colourCopies=0;
inline void takeColours(uintptr_t clone){
    const auto wanted=read<std::array<uint32_t,layout::modelColourCount>>(motionVisual::object(sourceHandle)+layout::modelColours);
    if(read<std::array<uint32_t,layout::modelColourCount>>(clone+layout::modelColours)==wanted)return;
    static const bool known=[]{const auto p=parsePattern("48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 41 0f b6 f0 41 8b d9 41 b0 01 48 8b f9");
        const bool matched=scan(reinterpret_cast<const uint8_t*>(baseAddress+0x110350),p.size(),p)==std::vector<size_t>{0};
        logFile->event("motion_hand_signature","\"rva\":\"0x110350\",\"use\":\"model instance colour\",\"matched\":"+std::string(matched?"true":"false"));return matched;}();
    if(!known)return; // the hands stay as they were, without the colours
    using Colour=void(__fastcall*)(void*,uint32_t,uint8_t,uint32_t);
    for(uint8_t n=0;n<layout::modelColourCount;n++)reinterpret_cast<Colour>(baseAddress+0x110350)(reinterpret_cast<void*>(s()),handle,n,wanted[n]);
    if(read<std::array<uint32_t,layout::modelColourCount>>(clone+layout::modelColours)!=wanted)throw std::runtime_error("hand copy did not take the colours");
    if(colourCopies++<8){std::ostringstream o;o<<"\"frame\":"<<liveFrame.frame<<",\"handle\":"<<handle<<",\"source_handle\":"<<sourceHandle<<",\"colours\":[";
        for(unsigned n=0;n<layout::modelColourCount;n++)o<<(n?",":"")<<wanted[n];
        o<<"],\"source_unchanged\":true";logFile->event("motion_hand_colours",o.str());}
}
// The hand rig alone, without the copy of the hands: for the mod without VR (view_resource_probe.cpp onlineFlat), where
// other players' bodies are solved on the player's skeleton and no hands of the own are shown. From the own character's
// bare-hand model, as prepare() below makes it. True: the rig is there.
inline bool rigFromSource(uint32_t source){
    if(rig)return true;
    if(!source)return false;
    const auto object=motionVisual::object(source),res=read<uintptr_t>(object+layout::modelResource);
    if(!res||read<uint8_t>(res+0x14)!=1||read<uint32_t>(res+layout::resourceAuxiliary))return false;
    const auto sp=read<uint32_t>(object+layout::modelPose);if(!sp)return false;
    const auto pr=handPoseMetadata::pose(s(),sp),owner=read<uintptr_t>(pr);
    const auto hierarchy=read<uintptr_t>(owner+layout::poseOwnerHierarchy),model=read<uintptr_t>(res+layout::resourceDescriptor);
    if(!hierarchy||!model||read<uint32_t>(hierarchy+48)!=102||read<uint16_t>(model+layout::renderModelHierarchyJoints)!=102||
       read<std::array<uint32_t,4>>(hierarchy)!=read<std::array<uint32_t,4>>(owner)||
       read<std::array<uint32_t,4>>(model+layout::renderModelHierarchy)!=read<std::array<uint32_t,4>>(owner))throw std::runtime_error("hand hierarchy identity differs");
    auto names=array<uint32_t>(hierarchy+16,102);auto parents=array<uint16_t>(hierarchy+24,102);
    const std::array<std::pair<unsigned,uint32_t>,6> profile{{{43,0xc613924f},{44,0x8d954c5f},{64,0xc5733365},{73,0xffcff1a4},{74,0xe4a46f46},{94,0x3f7c0e06}}};
    for(auto [index,name]:profile)if(names[index]!=name)throw std::runtime_error("unreviewed hand skeleton profile");
    auto defaults=array<hand::Joint>(hierarchy+64,104);defaults.resize(102);const auto skin=array<uint16_t>(model+layout::renderModelSkinningMap,42);
    rig=std::make_unique<hand::Rig>(defaults,parents,skin,std::array<hand::Branch,2>{{{43,44,64},{73,74,94}}});
    bool curl=false;
    if(!closedPose.empty())try{rig->enableCurl(closedPose,{{{49,53,61},{79,83,91}}});curl=true;}catch(...){}
    poseResource=owner;sourceHandle=source;
    logFile->event("motion_hand_rig_only","\"source_handle\":"+std::to_string(source)+",\"joints\":102,\"curl\":"+(curl?"true":"false")+",\"copy_of_the_hands\":false");
    return true;
}
inline void prepare(const game::StableAnchor& anchor,uint64_t renderSerial){
    if(!requested||serial==renderSerial)return;ready=false;
    // Hands need the current native mutation boundary, not a supported weapon.
    if(!motionVisual::contextReady||motionVisual::contextSerial!=renderSerial||motionVisual::contextFrame!=liveFrame.frame)return;
    const auto& hands=shownHands;
    if(!hands.focused||!hands.hands[0].tracked()||!hands.hands[1].tracked())return;
    equipmentObserver::Snapshot equipment;{std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);equipment=equipmentObserver::latest;}
    const auto root=rootCamera::snapshot();const auto now=GetTickCount64();
    if(equipment.entity!=root.entity||equipment.scene!=s()||!pauseClock::fresh(equipment.milliseconds,now,500)||!equipment.handles[11])return;   // the age stands still while the game is paused
    if(handle&&equipment.handles[11]!=sourceHandle)throw std::runtime_error("hand equipment changed; diagnostic ends");
    const auto source=motionVisual::object(equipment.handles[11]),res=read<uintptr_t>(source+layout::modelResource);
    if(!res||read<uint8_t>(res+0x14)!=1||read<uint32_t>(res+layout::resourceAuxiliary))throw std::runtime_error("hand model resource not supported");
    const auto sourceBytes=read<std::array<uint8_t,0x1b0>>(source);
    const auto sp=read<uint32_t>(source+layout::modelPose);const auto pr=handPoseMetadata::pose(s(),sp),owner=read<uintptr_t>(pr);
    const auto sourceJoints=snapshot(joints(pr),102*48);const auto version=read<uint64_t>(pr+24);
    if(handle&&(resource!=res||sourcePose!=sp||poseResource!=owner))throw std::runtime_error("hand source pose/resource changed");
    nativeGripObserver::capture(equipment.handles[11],sp,pr,sourceBytes,sourceJoints,version,renderSerial);
    const auto facing=normalized(anchor.orientation);const auto position=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    std::array<uint8_t,56> transform{};const Vec3 scale{anchor.worldUnitsPerMeter,anchor.worldUnitsPerMeter,anchor.worldUnitsPerMeter};
    if(!finite(scale)||scale.x<=0)throw std::runtime_error("hand world scale invalid");
    std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&facing,16);std::memcpy(transform.data()+40,&scale,12);
    if(!handle){
        validate();const auto hierarchy=read<uintptr_t>(owner+layout::poseOwnerHierarchy),model=read<uintptr_t>(res+layout::resourceDescriptor);
        if(!hierarchy||!model||read<uint32_t>(hierarchy+48)!=102||read<uint16_t>(model+layout::renderModelHierarchyJoints)!=102||
           read<std::array<uint32_t,4>>(hierarchy)!=read<std::array<uint32_t,4>>(owner)||
           read<std::array<uint32_t,4>>(model+layout::renderModelHierarchy)!=read<std::array<uint32_t,4>>(owner))throw std::runtime_error("hand hierarchy identity differs");
        auto names=array<uint32_t>(hierarchy+16,102);auto parents=array<uint16_t>(hierarchy+24,102);
        const std::array<std::pair<unsigned,uint32_t>,6> profile{{{43,0xc613924f},{44,0x8d954c5f},{64,0xc5733365},{73,0xffcff1a4},{74,0xe4a46f46},{94,0x3f7c0e06}}};
        for(auto [index,name]:profile)if(names[index]!=name)throw std::runtime_error("unreviewed hand skeleton profile");
        auto defaults=array<hand::Joint>(hierarchy+64,104);defaults.resize(102);const auto skin=array<uint16_t>(model+layout::renderModelSkinningMap,42);
        if(weaponGripPreview::requested){
            if(read<std::array<uint32_t,4>>(res+24)!=weaponGripPreview::handGuid)throw std::runtime_error("grip preview hand asset changed");
            defaults=weaponGripPreview::handPose;
        }
        rig=std::make_unique<hand::Rig>(defaults,parents,skin,std::array<hand::Branch,2>{{{43,44,64},{73,74,94}}});
        if(!closedPose.empty()&&!weaponGripPreview::requested){
            // Knuckles measured by the grip calibration (index 49/79, middle 53/83, little 61/91).
            try{rig->enableCurl(closedPose,{{{49,53,61},{79,83,91}}});std::string sizes;
                for(unsigned h=0;h<2;h++)for(unsigned f=0;f<hand::FingerCount;f++)sizes+=std::string(sizes.empty()?"":",")+std::to_string(rig->fingerJoints(h)[f].size());
                logFile->event("motion_hand_curl_enabled","\"relaxed\":"+std::to_string(curlRelaxed)+",\"chain_lengths\":["+sizes+"],\"source\":\"grip calibration closed hand\"");
            }catch(const std::exception& e){
                // Record the hand hierarchy (joint:parent) so the finger chains can be mapped from evidence.
                std::string tree;const auto& par=rig->parents();
                for(uint16_t j=40;j<par.size()&&j<100;j++)tree+=std::string(tree.empty()?"":",")+"["+std::to_string(j)+","+std::to_string(par[j]==UINT16_MAX?-1:int(par[j]))+"]";
                logFile->event("motion_hand_curl_disabled","\"reason\":"+jsonString(e.what())+",\"parents_40_99\":["+tree+"]");}
        }
        const auto modelGuid=read<std::array<uint32_t,4>>(res+24),hierarchyGuid=read<std::array<uint32_t,4>>(owner);
        const auto refs=read<uint32_t>(res+16),poseRefs=read<uint32_t>(owner+16);
        if(!refs||refs==UINT32_MAX||!poseRefs||poseRefs==UINT32_MAX)throw std::runtime_error("hand reference bounds");
        sourceHandle=equipment.handles[11];resource=res;sourcePose=sp;poseResource=owner;
        poseHandle=reinterpret_cast<PoseCreate>(baseAddress+0xfb8e0)(reinterpret_cast<void*>(s()),hierarchyGuid.data());
        if(!poseHandle||poseHandle==sourcePose){poseHandle=0;throw std::runtime_error("hand private pose failed/aliased");}
        if(read<uintptr_t>(handPoseMetadata::pose(s(),poseHandle))!=owner||read<uint32_t>(owner+16)!=poseRefs+1)throw std::runtime_error("hand private pose reference mismatch");
        // Create at the native source transform, then move to the VR origin
        // through c77d0 below. An identical first transform is a native no-op
        // and never activates dynamic skinning/+12d or its42-joint palette.
        const auto initialTransform=read<std::array<uint8_t,56>>(source+0x38);
        if(!std::memcmp(initialTransform.data(),transform.data(),52))throw std::runtime_error("hand initial/source origin coincides; dynamic initialization unproven");
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(s()),modelGuid.data(),initialTransform.data());
        if(!handle||handle==sourceHandle){handle=0;throw std::runtime_error("hand clone failed/aliased");}
        auto clone=motionVisual::object(handle);reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(s()),reinterpret_cast<void*>(clone),0);
        if(read<uintptr_t>(clone+layout::modelResource)!=res||read<uint32_t>(res+16)!=refs+1||read<uint32_t>(clone+layout::modelAuxiliary))throw std::runtime_error("hand clone resource/auxiliary mismatch");
        const auto sdfId=read<uint32_t>(clone+layout::modelSdf);if(sdfId){
            if(sdfId==read<uint32_t>(source+layout::modelSdf))throw std::runtime_error("hand SDF aliases source");
            const auto sdf=read<uintptr_t>(s()+layout::sceneSdf),count=read<uint64_t>(sdf+0x78);if(!count)throw std::runtime_error("hand SDF count");
            reinterpret_cast<PoseRemove>(baseAddress+0x15a640)(reinterpret_cast<void*>(sdf),sdfId);*reinterpret_cast<uint32_t*>(clone+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("hand SDF release failed");}
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1};reinterpret_cast<Bind>(baseAddress+0x110960)(reinterpret_cast<void*>(s()),ids.data(),poseHandle);
        if(read<uint32_t>(clone+layout::modelPose)!=poseHandle||read<uint64_t>(handPoseMetadata::pose(s(),poseHandle)+0x58)!=1)throw std::runtime_error("hand pose binding failed");
        logFile->event("motion_hand_created","\"handle\":"+std::to_string(handle)+",\"source_handle\":"+std::to_string(sourceHandle)+
            ",\"pose_handle\":"+std::to_string(poseHandle)+",\"source_pose\":"+std::to_string(sourcePose)+",\"joints\":102,\"skin_joints\":42,\"mask\":0,\"sdf\":0,\"private_pose\":true,\"socket_calibration\":\"native_candidate_64_94\"");
    }
    std::array<Pose,2> grips;
    for(unsigned h=0;h<2;h++){auto grip=hands.hands[h].grip;
        // VHVR keeps the drawing hand on the string while the draw is held back.
        if(h==1&&motionVisual::bowDrawHandValid)grip.position=motionVisual::bowDrawHand;
        grips[h]=game::trackingPose(compose(liveTrackingToAnchor,grip));}
    std::array<hand::FingerCurl,2> curls{};
    for(unsigned h=0;h<2;h++){const auto& v=hands.hands[h];
        // Fourth real-device run ("スティックの親指可動は可能なら静電気式に"): with a touch sensor on the stick the thumb
        // follows that sensor alone; only controllers without one fall back to a click or tilt, as in EDF6VR.
        const bool thumb=v.thumbTouchActive?v.thumbTouch:(v.stickClickActive&&v.stickClick)||(v.stickActive&&std::abs(v.stick.x)+std::abs(v.stick.y)>.15f);
        if(v.thumbTouchActive!=thumbSeen[h].active||v.thumbTouch!=thumbSeen[h].touch){thumbSeen[h]={v.thumbTouchActive,v.thumbTouch};
            if(thumbChanges++<200)logFile->event("motion_thumb_touch","\"frame\":"+std::to_string(liveFrame.frame)+",\"hand\":"+std::to_string(h)+
                ",\"sensor\":"+(v.thumbTouchActive?"true":"false")+",\"touch\":"+(v.thumbTouch?"true":"false")+",\"stick\":["+std::to_string(v.stick.x)+","+std::to_string(v.stick.y)+"]");}
        curls[h]=hand::curlFromControls(v.triggerActive?v.trigger:0.f,v.grabActive?v.squeeze:0.f,thumb,curlRelaxed);}
    if(rig->curlEnabled()&&(++curlLogged%600)==1)logFile->event("motion_hand_curl_sample","\"frame\":"+std::to_string(liveFrame.frame)+
        ",\"left\":["+std::to_string(curls[0].amount[0])+","+std::to_string(curls[0].amount[1])+","+std::to_string(curls[0].amount[2])+"],\"right\":["+
        std::to_string(curls[1].amount[0])+","+std::to_string(curls[1].amount[1])+","+std::to_string(curls[1].amount[2])+"]");
    const auto output=rig->locate(grips,rig->curlEnabled()?&curls:nullptr);lastOutput=output;lastCurls=curls;std::array<uintptr_t,2> jointSpan{reinterpret_cast<uintptr_t>(output.data()),output.size()};
    reinterpret_cast<PoseUpdate>(baseAddress+0x111060)(reinterpret_cast<void*>(s()),poseHandle,jointSpan.data());
    std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},transforms{reinterpret_cast<uintptr_t>(transform.data()),1};
    motionVisual::change(reinterpret_cast<void*>(s()),ids.data(),transforms.data());
    const auto clone=motionVisual::object(handle),owned=handPoseMetadata::pose(s(),poseHandle);
    if(read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(clone+layout::modelPose)!=(lentPose?lentPose:poseHandle)||
       read<uint32_t>(clone+layout::modelPaletteCount)!=42||!read<uint32_t>(clone+layout::modelPaletteHandle)||read<uint64_t>(owned+0x58)!=(lentPose?0u:1u)||
       snapshot(joints(owned),output.size()*48)!=snapshot(reinterpret_cast<uintptr_t>(output.data()),output.size()*48)||
       read<std::array<uint8_t,0x1b0>>(motionVisual::object(sourceHandle))!=sourceBytes||snapshot(joints(pr),102*48)!=sourceJoints||read<uint64_t>(pr+24)!=version)
        {logFile->event("motion_hand_invariant_failure","\"palette_count\":"+std::to_string(read<uint32_t>(clone+layout::modelPaletteCount))+
          ",\"palette_handle\":"+std::to_string(read<uint32_t>(clone+layout::modelPaletteHandle))+",\"dynamic\":"+std::to_string(read<uint8_t>(clone+layout::modelDynamic))+
          ",\"source_model_unchanged\":"+(read<std::array<uint8_t,0x1b0>>(motionVisual::object(sourceHandle))==sourceBytes?"true":"false")+
          ",\"source_pose_unchanged\":"+(snapshot(joints(pr),102*48)==sourceJoints&&read<uint64_t>(pr+24)==version?"true":"false"));
         throw std::runtime_error("hand pose/palette/source isolation invariant failed");}
    takeColours(clone);
    ready=true;serial=renderSerial;frame=liveFrame.frame;++updates;
    std::ostringstream o;o<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"time\":"<<liveFrame.time<<",\"handle\":"<<handle<<",\"pose_handle\":"<<poseHandle<<",\"source_unchanged\":true,\"mask\":0,\"sdf\":0,\"palette_count\":42,\"grips\":[";
    for(unsigned h=0;h<2;h++){if(h)o<<',';auto p=grips[h];o<<"["<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','<<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w<<']';}
    o<<']';logFile->event("motion_hand_pose",o.str());
    if(updates==1){handPoseMetadata::save(calibrationDirectory,"hand-owned-joints.bin",joints(owned),102*48);}
}
inline void dispose(uintptr_t scene){
    if(scene!=s())return;
    if(handle){const auto id=handle;const auto refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
        reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),ids.data());handle=0;ready=false;
        const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
        const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
        logFile->event("motion_hand_model_disposed","\"generation_invalidated\":"+std::string(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false"));
        if(!invalid||!released)throw std::runtime_error("hand model retirement failed");}
    if(poseHandle){const auto id=poseHandle;const auto record=handPoseMetadata::pose(scene,id);const auto refs=read<uint32_t>(poseResource+16);
        if(read<uint64_t>(record+0x58))throw std::runtime_error("private hand pose still bound");
        reinterpret_cast<PoseRemove>(baseAddress+0x10e260)(reinterpret_cast<void*>(scene),id);poseHandle=0;
        bool invalid=false;try{handPoseMetadata::pose(scene,id);}catch(...){invalid=true;}
        const bool released=refs==1||(refs>1&&read<uint32_t>(poseResource+16)==refs-1);
        logFile->event("motion_hand_pose_disposed","\"generation_invalidated\":"+std::string(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false")+",\"updates\":"+std::to_string(updates));
        if(!invalid||!released)throw std::runtime_error("hand pose retirement failed");}
}
}
