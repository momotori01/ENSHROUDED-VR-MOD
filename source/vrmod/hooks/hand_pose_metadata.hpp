// Read-only bounded capture of native hand resources; not redistributable assets.
namespace handPoseMetadata {
inline void save(const std::filesystem::path& folder,const char* name,uintptr_t address,size_t size){
    if(!size||size>65536)throw std::runtime_error("hand metadata size outside bound");
    std::vector<uint8_t> data(size);SIZE_T copied=0;
    if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),data.data(),size,&copied)||copied!=size)
        throw std::runtime_error("hand metadata unreadable");
    std::ofstream file(folder/name,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());
    if(!file)throw std::runtime_error("hand metadata file failed");
}
inline uint32_t blob(const std::filesystem::path& folder,const char* name,uintptr_t field,size_t stride,uint32_t maximum){
    const auto offset=read<int32_t>(field);const auto count=read<uint32_t>(field+4);
    if(!count)return 0;
    if(!offset||offset < -16777216||offset>16777216||count>maximum)
        throw std::runtime_error("hand metadata relative array outside bound");
    save(folder,name,uintptr_t(int64_t(field)+offset),size_t(count)*stride);return count;
}
inline uintptr_t pose(uintptr_t scene,uint32_t handle){
    const auto shift=read<uint64_t>(scene+layout::scenePoseShift),mask=read<uint64_t>(scene+layout::scenePoseMask);
    const auto pages=read<uintptr_t>(scene+layout::scenePosePages),count=read<uint64_t>(scene+layout::scenePoseCount);
    if(!handle||shift>16||mask>65535||!pages||!count||count>65536)
        throw std::runtime_error("hand pose table invalid");
    const uint64_t index=handle&65535u,page=index>>shift,slot=index&mask;
    if(page>=count)throw std::runtime_error("hand pose page outside table");
    const auto p=pages+page*64,records=read<uintptr_t>(p+8),size=read<uint64_t>(p+16);
    if(!records||size>65536||slot>=size)throw std::runtime_error("hand pose slot outside page");
    const auto entry=records+slot*layout::poseEntryBytes;const auto generation=read<uint32_t>(entry);
    if((generation&0x80000000u)||(generation&0x7fffffffu)!=(handle>>16))
        throw std::runtime_error("hand pose generation differs");
    return entry+8;
}
inline void capture(uintptr_t scene,uintptr_t object){
    const auto resource=read<uintptr_t>(object+layout::modelResource);const auto handle=read<uint32_t>(object+layout::modelPose);
    if(!resource||read<uint8_t>(resource+0x14)!=1)throw std::runtime_error("hand resource not ready");
    const auto model=read<uintptr_t>(resource+layout::resourceDescriptor),record=pose(scene,handle),owner=read<uintptr_t>(record);
    if(!model||!owner)throw std::runtime_error("hand model/pose resource absent");
    const auto hierarchy=read<uintptr_t>(owner+layout::poseOwnerHierarchy);
    if(!hierarchy||read<std::array<uint32_t,4>>(model+layout::renderModelHierarchy)!=read<std::array<uint32_t,4>>(owner)||
       read<std::array<uint32_t,4>>(hierarchy)!=read<std::array<uint32_t,4>>(owner))
        throw std::runtime_error("hand hierarchy GUID differs");
    const auto count=read<uint32_t>(hierarchy+48);
    if(!count||count>256||read<uint16_t>(model+layout::renderModelHierarchyJoints)!=count)
        throw std::runtime_error("hand hierarchy joint count differs");
    const auto folder=calibrationDirectory/"hand-pose";std::filesystem::create_directories(folder);
    save(folder,"model.bin",model,320);save(folder,"hierarchy.bin",hierarchy,72);
    save(folder,"pose-record.bin",record,0x68);save(folder,"hierarchy-owner.bin",owner,32);
    const auto names=blob(folder,"joint-names.bin",hierarchy+16,4,256);
    const auto parents=blob(folder,"joint-parents.bin",hierarchy+24,2,256);
    const auto matrices=blob(folder,"skin-to-joint.bin",hierarchy+40,64,256);
    const auto defaults=blob(folder,"default-joints.bin",hierarchy+64,48,256);
    const auto skin=blob(folder,"skinning-map.bin",model+layout::renderModelSkinningMap,2,256);
    const auto activeCount=read<uint32_t>(record+0xc),first=read<uint32_t>(record+8);
    const auto jointData=read<uintptr_t>(scene+layout::sceneJoints),capacity=read<uint64_t>(scene+layout::sceneJointCapacity);
    if(names!=count||parents!=count||matrices!=count||defaults!=((count+3u)&~3u)||activeCount!=count||
       !jointData||capacity>1000000||uint64_t(first)+count>capacity)
        throw std::runtime_error("hand pose joint arrays differ");
    save(folder,"current-joints.bin",jointData+size_t(first)*48,count*48);
    logFile->event("hand_pose_metadata","\"read_only\":true,\"handle\":"+std::to_string(handle)+
        ",\"joint_count\":"+std::to_string(count)+",\"skin_count\":"+std::to_string(skin)+
        ",\"palette_count\":"+std::to_string(read<uint32_t>(object+layout::modelPaletteCount))+",\"pose\":"+hex(record)+
        ",\"hierarchy\":"+hex(hierarchy)+",\"source_unchanged\":true");
}
}
