// Verified native constant boundary. Observation is default; explicit body clip
// changes only zeroed padding in private first-person eye uploads.
namespace bodyUpload {
inline bool requested=false,failed=false,clipRequested=false;
inline float clipDistance=0,glideClipDistance=0,motionClipDistance=0;inline std::vector<unsigned> parts;
inline std::array<bool,2> enabled{};
inline std::array<std::array<uint64_t,3>,2> counts{};
inline std::atomic<uint64_t> ignored{0};
constexpr std::array<uintptr_t,2> sites{0x10bcbb,0x10e284};
constexpr std::array<size_t,2> sourceOffsets{0xf60,0x1ae0},descriptorOffsets{0xf30,0xea0},sizes{992,480};
constexpr std::array<const char*,2> patterns{
    "48 8B 85 50 0F 00 00 48 8D 8D 60 0F 00 00",
    "48 8B 85 C0 0E 00 00 48 8D 8D E0 1A 00 00"};
inline void applyClip(const PairResources& pair,int eye,unsigned kind,uintptr_t address,const std::array<uint8_t,992>& before){
    const size_t offset=kind?472:632;
    if(read<uint64_t>(address+offset))throw std::runtime_error("native body padding is not zero");
    if(eye<0)return;
    const auto* frame=headVisibility::frame;
    if(!frame||frame->nativeCamera||!frame->equipment.number||frame->serial!=pair.serial){
        logFile->event("body_clip_inactive","\"eye\":"+std::to_string(eye)+",\"kind\":"+jsonString(kind?"depth":"color")+",\"serial\":"+std::to_string(pair.serial)+",\"reason\":\"native camera or absent current equipment\"");return;
    }
    const auto& s=frame->equipment;if(s.scene!=pair.scene)throw std::runtime_error("body equipment Scene differs");
    const auto objects=read<uintptr_t>(pair.scene+0x1b0),generations=read<uintptr_t>(pair.scene+0x1c8);
    const auto bytes=read<uint64_t>(pair.scene+0x1b8),generationCount=read<uint64_t>(pair.scene+0x1d0);
    if(!objects||!generations||bytes%0x1b0)throw std::runtime_error("body model tables unavailable");
    std::vector<uint32_t> indices;
    for(auto part:parts)if(s.handles[part]){
        const auto handle=s.handles[part],index=handle&0xffffff;
        if(index>=bytes/0x1b0||index>=generationCount||read<uint8_t>(generations+index)!=(handle>>24))throw std::runtime_error("body handle generation changed");
        const auto object=objects+uintptr_t(index)*0x1b0,resource=read<uintptr_t>(object+layout::modelResource);
        if(!resource||s.ready[part]!=1||read<uint8_t>(resource+0x14)!=1||read<uint16_t>(resource+layout::resourceModel)!=s.models[part]||read<uint32_t>(object+layout::modelMask)!=s.masks[part])
            throw std::runtime_error("body model changed since snapshot");
        indices.push_back(index);
    }
    if(indices.empty())return;
    // One copied render-entry camera decision for both eyes and both stages.
    // This changes owned visibility only; it never changes tracking or Root.
    const bool glideOverride=glideClipDistance>0&&frame->cameraId==25&&!frame->syntheticPresentation;
    const bool motionOverride=motionClipDistance>0&&frame->motionOverride;
    const float distance=motionOverride?motionClipDistance:glideOverride?glideClipDistance:clipDistance;
    const auto payload=body::pack(indices,distance);
    using GetPayload=int(*)(VkDevice,const body::Payload*,uint64_t*);
    const auto module=GetModuleHandleW(L"evr_diagnostic.dll");const auto get=module?reinterpret_cast<GetPayload>(GetProcAddress(module,"evrBodyClipPayload")):nullptr;
    const auto device=read<VkDevice>(read<uintptr_t>(pair.graphics+0x278)+layout::backendDevice);uint64_t gpuAddress=0;
    if(!get||get(device,&payload,&gpuAddress)||!gpuAddress){logFile->event("body_clip_inactive","\"eye\":"+std::to_string(eye)+",\"kind\":"+jsonString(kind?"depth":"color")+",\"serial\":"+std::to_string(pair.serial)+",\"reason\":\"payload unavailable\"");return;}
    auto expected=before;std::memcpy(expected.data()+offset,&gpuAddress,8);
    std::memcpy(reinterpret_cast<void*>(address+offset),&gpuAddress,8);
    if(std::memcmp(reinterpret_cast<const void*>(address),expected.data(),sizes[kind]))throw std::runtime_error("body write escaped private padding");
    std::ostringstream out;out<<"\"eye\":"<<eye<<",\"kind\":"<<jsonString(kind?"depth":"color")<<",\"serial\":"<<pair.serial<<",\"equipment_sample\":"<<s.number
        <<",\"entity\":"<<s.entity<<",\"address\":"<<hex(gpuAddress)<<",\"offset\":"<<offset<<",\"distance\":"<<distance<<",\"camera_id\":"<<unsigned(frame->cameraId)<<",\"glide_override\":"<<(glideOverride?"true":"false")<<",\"motion_override\":"<<(motionOverride?"true":"false")<<",\"actor_sample\":"<<frame->motion.number<<",\"indices\":[";
    for(uint32_t n=0;n<payload.count;n++){if(n)out<<',';out<<payload.models[n];}
    out<<"],\"only_private_padding_changed\":true,\"handles_revalidated\":true";logFile->event("body_clip_payload",out.str());
}
inline void onUpload(uintptr_t rsp,unsigned kind) noexcept {
    const ObserverLastError preserve;
    if(!requested||kind>1||!enabled[kind])return;
    if(!cullingHistory::current){++ignored;return;}
    try{
        const auto& pair=*cullingHistory::current;const int eye=cullingHistory::currentEye;
        if(eye < -1||eye>1||pair.renderThread!=GetCurrentThreadId()||pair.scene!=provenWorldScene)
            throw std::runtime_error("body upload scope differs");
        const auto rbp=read<uintptr_t>(rsp-48);ULONG_PTR low=0,high=0;GetCurrentThreadStackLimits(&low,&high);
        if(rbp<low||rbp>high||sourceOffsets[kind]+sizes[kind]>high-rbp)
            throw std::runtime_error("body constants outside native stack");
        const auto address=rbp+sourceOffsets[kind];const auto destination=read<uintptr_t>(rbp+descriptorOffsets[kind]+0x20);
        if(!destination)throw std::runtime_error("body constant allocation missing");
        const auto number=++counts[kind][eye+1];if(number>4&&!clipRequested)return;
        std::array<uint8_t,992> bytes{};std::memcpy(bytes.data(),reinterpret_cast<const void*>(address),sizes[kind]);
        if(number<=4){
        const auto name="body_constants_"+std::to_string(kind)+"_"+std::to_string(eye+1)+"_"+std::to_string(number)+".bin";
        {std::ofstream f(calibrationDirectory/name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),sizes[kind]);if(!f)throw std::runtime_error("body constants evidence write");}
        if(std::memcmp(bytes.data(),reinterpret_cast<const void*>(address),sizes[kind]))throw std::runtime_error("body source changed during observation");
        uint64_t candidate=0;std::memcpy(&candidate,bytes.data()+(kind?472:632),8);
        std::ostringstream out;out<<"\"kind\":"<<jsonString(kind?"depth":"color")<<",\"eye\":"<<eye<<",\"serial\":"<<pair.serial
            <<",\"number\":"<<number<<",\"bytes\":"<<sizes[kind]<<",\"source\":"<<hex(address)<<",\"destination\":"<<hex(destination)
            <<",\"candidate_padding\":"<<hex(candidate)<<",\"candidate_offset\":"<<(kind?472:632)<<",\"file\":"<<jsonString(name)
            <<",\"read_only\":true,\"source_unchanged\":true";
        logFile->event("body_upload_observation",out.str());
        }
        if(clipRequested)applyClip(pair,eye,kind,address,bytes);
    }catch(const std::exception& e){failed=true;try{logFile->event("body_upload_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void install(){
    std::array<void*,2> stubs{reinterpret_cast<void*>(&evr_body_color_stub),reinterpret_cast<void*>(&evr_body_depth_stub)};
    std::array<void**,2> trampolines{&evr_body_color_trampoline,&evr_body_depth_trampoline};
    for(unsigned n=0;n<2;n++){
        auto* target=reinterpret_cast<void*>(baseAddress+sites[n]);const auto pattern=parsePattern(patterns[n]);
        if(scan(static_cast<const uint8_t*>(target),pattern.size(),pattern)!=std::vector<size_t>{0})throw std::runtime_error("body upload signature differs");
        if(MH_CreateHook(target,stubs[n],trampolines[n])!=MH_OK||MH_EnableHook(target)!=MH_OK)throw std::runtime_error("body upload observer hook failed");
        enabled[n]=true;logFile->event("body_upload_hook_enabled","\"kind\":"+jsonString(n?"depth":"color")+",\"rva\":"+hex(sites[n])+",\"pattern\":"+jsonString(patterns[n])+",\"read_only\":"+(clipRequested?"false":"true"));
    }
}
inline void disable(){
    for(unsigned n=0;n<2;n++)if(enabled[n]){
        auto* target=reinterpret_cast<void*>(baseAddress+sites[n]);
        if(MH_DisableHook(target)!=MH_OK)throw std::runtime_error("body upload observer disable failed");enabled[n]=false;
        const auto bytes=parsePattern(patterns[n]);const bool restored=scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)==std::vector<size_t>{0};
        logFile->event("body_upload_hook_removed","\"kind\":"+jsonString(n?"depth":"color")+",\"original_bytes_restored\":"+(restored?"true":"false")+
            ",\"original_count\":"+std::to_string(counts[n][0])+",\"eye_counts\":["+std::to_string(counts[n][1])+","+std::to_string(counts[n][2])+"]");
        if(!restored)throw std::runtime_error("body upload original bytes not restored");
    }
}
}
