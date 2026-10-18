// Opt-in, bounded same-render-boundary copies of ORIGINAL hands and weapon.
// No source writes; no calibration constants are accepted by this observer.
namespace nativeGripObserver {
inline bool requested=false;inline unsigned samples=0;
inline std::array<std::pair<uint32_t,unsigned>,motion::retainedVisualCapacity> itemSamples{};
// Animated weapon anatomy is evidence only. Copy at the same completed CPU
// boundary as the hand survey, with bounded arrays and a second byte read.
struct WeaponPoseCopy {
    uint32_t handle=0,count=0,skinCount=0;uint64_t version=0;
    uintptr_t scene=0,record=0,owner=0,hierarchy=0,jointData=0;
    struct Part {std::string name;uintptr_t address;std::vector<uint8_t> bytes;};
    std::vector<Part> parts;
    void part(const char* name,uintptr_t address,size_t bytes){parts.push_back({name,address,motionHandVisual::snapshot(address,bytes)});}
    void blob(const char* name,uintptr_t field,size_t stride,uint32_t expected){
        const auto offset=read<int32_t>(field);const auto n=read<uint32_t>(field+4);
        if(!offset||offset < -16777216||offset>16777216||n!=expected||!n||n>256)
            throw std::runtime_error("weapon pose survey relative array differs");
        part(name,uintptr_t(int64_t(field)+offset),size_t(n)*stride);
    }
    void capture(uintptr_t s,uintptr_t modelObject,uintptr_t model){
        handle=read<uint32_t>(modelObject+layout::modelPose);if(!handle)return;
        scene=s;record=handPoseMetadata::pose(scene,handle);owner=read<uintptr_t>(record);
        if(!owner)throw std::runtime_error("weapon pose survey owner missing");
        hierarchy=read<uintptr_t>(owner+layout::poseOwnerHierarchy);
        if(!hierarchy||read<std::array<uint32_t,4>>(hierarchy)!=read<std::array<uint32_t,4>>(owner)||
           read<std::array<uint32_t,4>>(model+layout::renderModelHierarchy)!=read<std::array<uint32_t,4>>(owner))
            throw std::runtime_error("weapon pose survey hierarchy identity differs");
        count=read<uint32_t>(hierarchy+48);skinCount=read<uint32_t>(model+layout::renderModelSkinningMap+4);   // the skinning map's count (1.0 +0xd8; June +0x108)
        const auto first=read<uint32_t>(record+8),active=read<uint32_t>(record+12);
        const auto data=read<uintptr_t>(scene+layout::sceneJoints),capacity=read<uint64_t>(scene+layout::sceneJointCapacity);
        if(!count||count>256||read<uint16_t>(model+layout::renderModelHierarchyJoints)!=count||active!=count||
           !skinCount||skinCount>count||!data||capacity>1000000||uint64_t(first)+count>capacity)
            throw std::runtime_error("weapon pose survey storage/count differs");
        jointData=data+size_t(first)*48;version=read<uint64_t>(record+24);
        part("weapon-pose-record.bin",record,104);part("weapon-hierarchy-owner.bin",owner,32);
        part("weapon-hierarchy.bin",hierarchy,72);part("weapon-current-joints.bin",jointData,count*48);
        blob("weapon-joint-names.bin",hierarchy+16,4,count);blob("weapon-joint-parents.bin",hierarchy+24,2,count);
        blob("weapon-skin-to-joint.bin",hierarchy+40,64,count);blob("weapon-default-joints.bin",hierarchy+64,48,(count+3u)&~3u);
        blob("weapon-skinning-map.bin",model+layout::renderModelSkinningMap,2,skinCount);
    }
    void verify()const{
        if(!handle)return;
        if(handPoseMetadata::pose(scene,handle)!=record||read<uintptr_t>(record)!=owner||
           read<uintptr_t>(owner+layout::poseOwnerHierarchy)!=hierarchy||read<uint64_t>(record+24)!=version)
            throw std::runtime_error("weapon pose survey identity changed");
        for(const auto& p:parts)if(motionHandVisual::snapshot(p.address,p.bytes.size())!=p.bytes)
            throw std::runtime_error("weapon pose survey source changed");
    }
};
// Shield uses attachment slot12 in the current home save, rather than the
// weapon sockets8/9. Measure its relation to all original bones before choosing
// a tracked mount. This is a separate read-only budget, never a new clone.
inline unsigned shieldSamples=0;
inline void captureShield(uint32_t handHandle,uint32_t poseHandle,uintptr_t record,
                          const std::array<uint8_t,432>& handBytes,const std::vector<uint8_t>& jointBytes,
                          uint64_t version,uint64_t serial){
    if(shieldSamples>=6||!motionVisual::liveEquipment()||jointBytes.size()!=102*48)return;
    const auto root=rootCamera::snapshot();
    const auto shield=attachmentObserver::shieldAppearance(root.entity,motionVisual::scene,motionVisual::view,root.world,GetTickCount64());
    if(!shield.entity)return;
    const auto object=motionVisual::currentObject(shield.handle);if(!object)return;
    const auto resource=read<uintptr_t>(object+layout::modelResource);
    if(!resource||read<uint8_t>(resource+0x14)!=1||read<uint16_t>(resource+layout::resourceModel)!=shield.model||
       read<std::array<uint32_t,4>>(resource+24)!=shield.modelGuid)return;
    const auto model=read<uintptr_t>(resource+layout::resourceDescriptor);if(!model)return;
    const auto shieldBytes=read<std::array<uint8_t,432>>(object);const auto poseBytes=read<std::array<uint8_t,104>>(record);
    const auto metadata=read<std::array<uint8_t,320>>(model);
    if(!read<uint32_t>(object+layout::modelMask))return; // do not calibrate a hidden/stowed source
    if(read<std::array<uint8_t,432>>(motionVisual::object(handHandle))!=handBytes||
       read<std::array<uint8_t,432>>(motionVisual::object(shield.handle))!=shieldBytes||
       read<std::array<uint8_t,104>>(record)!=poseBytes||read<uint64_t>(record+24)!=version||
       motionHandVisual::snapshot(motionHandVisual::joints(record),102*48)!=jointBytes||
       read<std::array<uint8_t,320>>(model)!=metadata)
        throw std::runtime_error("shield survey original changed at completed CPU boundary");
    const auto relative=std::string("native-shield/frame-")+std::to_string(liveFrame.frame);
    const auto folder=calibrationDirectory/relative;std::filesystem::create_directories(folder);
    const auto save=[&](const char* name,const auto& bytes){std::ofstream file(folder/name,std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("shield survey write failed");};
    save("hand-model.bin",handBytes);save("shield-model.bin",shieldBytes);save("joints.bin",jointBytes);
    save("pose-record.bin",poseBytes);save("attachment.bin",shield.slotBytes);save("shield-render-model.bin",metadata);
    logFile->event("native_shield_grip_sample","\"serial\":"+std::to_string(serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
        ",\"time\":"+std::to_string(liveFrame.time)+",\"parent\":"+std::to_string(shield.parent)+",\"child\":"+std::to_string(shield.entity)+
        ",\"item\":"+std::to_string(shield.item)+",\"item_slot\":18,\"attachment_slot\":"+std::to_string(shield.slot)+
        ",\"hand_handle\":"+std::to_string(handHandle)+",\"pose_handle\":"+std::to_string(poseHandle)+",\"pose_version\":"+std::to_string(version)+
        ",\"shield_handle\":"+std::to_string(shield.handle)+",\"shield_guid_words\":["+std::to_string(shield.modelGuid[0])+","+std::to_string(shield.modelGuid[1])+","+
        std::to_string(shield.modelGuid[2])+","+std::to_string(shield.modelGuid[3])+"],\"folder\":"+jsonString(relative)+
        ",\"read_only\":true,\"source_unchanged\":true,\"engine_phase\":\"after_cpu_collectors_before_original_render\"");
    ++shieldSamples;
}
inline void capture(uint32_t handHandle,uint32_t poseHandle,uintptr_t record,
                    const std::array<uint8_t,432>& handBytes,const std::vector<uint8_t>& jointBytes,
                    uint64_t version,uint64_t renderSerial){
    const bool live=motionVisual::liveEquipment();
    if(!requested||motionHandVisual::updates%30)return;
    if(live)captureShield(handHandle,poseHandle,record,handBytes,jointBytes,version,renderSerial);
    if(samples>=(live?60u:20u))return;
    // Generic live survey keeps independent per-item budgets so startup cannot
    // consume the bow/staff samples. Legacy calibrated-axe evidence stays at 20.
    if(live&&!motionVisual::poseReady)return;
    const auto current=motionVisual::appearance(rootCamera::snapshot().entity,!live);
    if(!current.weaponCategoryKnown||current.handle!=motionVisual::sourceHandle||current.item!=motionVisual::item||
       current.entity!=motionVisual::child||!current.world||jointBytes.size()!=102*48){
        if(live)return; // Switching observations are not a coherent source sample.
        throw std::runtime_error("grip observer source/category identity not proven");
    }
    unsigned* itemCount=nullptr;
    if(live){
        for(auto& entry:itemSamples)if(entry.first==current.item){itemCount=&entry.second;break;}
        if(!itemCount)for(auto& entry:itemSamples)if(!entry.first){entry.first=current.item;itemCount=&entry.second;break;}
        if(!itemCount||*itemCount>=6)return;
    }
    const auto weapon=motionVisual::object(current.handle);
    const auto weaponBytes=read<std::array<uint8_t,432>>(weapon);
    const auto poseBytes=read<std::array<uint8_t,104>>(record);
    const auto hr=read<uintptr_t>(motionVisual::object(handHandle)+layout::modelResource),wr=read<uintptr_t>(weapon+layout::modelResource);
    if(!hr||!wr||wr!=motionVisual::resource||handPoseMetadata::pose(motionVisual::scene,poseHandle)!=record)
        throw std::runtime_error("grip observer resource/pose identity changed");
    const auto handGuid=read<std::array<uint8_t,16>>(hr+24),weaponGuid=read<std::array<uint8_t,16>>(wr+24);
    // Read-only RenderModel320 metadata. BoundingBox16, Sphere48 and the
    // position decoder64/76 are reflection-backed asset data, not a blade
    // calibration. Preserve this distinction in downstream analysis.
    const auto weaponModel=read<uintptr_t>(wr+layout::resourceDescriptor);
    const auto weaponModelBytes=motionHandVisual::snapshot(weaponModel,320);
    const auto lo=read<Vec3>(weaponModel+16),hi=read<Vec3>(weaponModel+32);
    if(!finite(lo)||!finite(hi)||lo.x>=hi.x||lo.y>=hi.y||lo.z>=hi.z)
        throw std::runtime_error("grip observer weapon bounds invalid");
    WeaponPoseCopy animated;
    if(live&&(current.weaponType==4||current.weaponType==5||current.weaponType==6))animated.capture(motionVisual::scene,weapon,weaponModel);
    animated.verify();
    // Re-read all originals before file IO: these bytes belong to this one
    // original render boundary after the proven CPU collectors have completed.
    if(read<std::array<uint8_t,432>>(motionVisual::object(handHandle))!=handBytes||
       read<std::array<uint8_t,432>>(motionVisual::object(current.handle))!=weaponBytes||
       read<std::array<uint8_t,104>>(record)!=poseBytes||read<uint64_t>(record+24)!=version||
       motionHandVisual::snapshot(motionHandVisual::joints(record),102*48)!=jointBytes||
       read<uintptr_t>(wr+layout::resourceDescriptor)!=weaponModel||motionHandVisual::snapshot(weaponModel,320)!=weaponModelBytes)
        throw std::runtime_error("grip observer originals changed during capture");
    const auto relative=std::string("native-grip/frame-")+std::to_string(liveFrame.frame);
    const auto folder=calibrationDirectory/relative;std::filesystem::create_directories(folder);
    const auto save=[&](const char* name,const auto& bytes){
        std::ofstream file(folder/name,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(!file)throw std::runtime_error("grip observer evidence write failed");};
    save("hand-model.bin",handBytes);save("weapon-model.bin",weaponBytes);save("joints.bin",jointBytes);save("pose-record.bin",poseBytes);save("attachment.bin",current.slotBytes);save("weapon-render-model.bin",weaponModelBytes);
    for(const auto& p:animated.parts)save(p.name.c_str(),p.bytes);
    std::ostringstream row;row<<"\"serial\":"<<renderSerial<<",\"frame\":"<<liveFrame.frame<<",\"time\":"<<liveFrame.time
      <<",\"session\":"<<liveFrame.sessionGeneration<<",\"scene\":"<<hex(motionVisual::scene)<<",\"world\":"<<hex(current.world)
      <<",\"parent\":"<<current.parent<<",\"child\":"<<current.entity<<",\"item\":"<<current.item
      <<",\"weapon_type\":"<<unsigned(current.weaponType)<<",\"weapon_hash\":"<<current.weaponHash<<",\"attachment_target\":"<<unsigned(current.target)<<",\"attachment_slot\":"<<unsigned(current.slot)
      <<",\"hand_handle\":"<<handHandle<<",\"weapon_handle\":"<<current.handle<<",\"pose_handle\":"<<poseHandle<<",\"pose_version\":"<<version
      <<",\"hand_guid\":"<<jsonString(nativeImpactComponents::asHex(handGuid))<<",\"weapon_guid\":"<<jsonString(nativeImpactComponents::asHex(weaponGuid))
      <<",\"weapon_render_model_bytes\":320,\"weapon_render_model_stable\":true"
      <<",\"weapon_pose_copied\":"<<(animated.handle?"true":"false")<<",\"weapon_pose_handle\":"<<animated.handle
      <<",\"weapon_joint_count\":"<<animated.count<<",\"weapon_skin_count\":"<<animated.skinCount<<",\"weapon_pose_version\":"<<animated.version
      <<",\"folder\":"<<jsonString(relative)<<",\"source_unchanged\":true,\"read_only\":true,\"engine_phase\":\"after_cpu_collectors_before_original_render\"";
    logFile->event("native_grip_sample",row.str());++samples;if(itemCount)++*itemCount;
}
}
