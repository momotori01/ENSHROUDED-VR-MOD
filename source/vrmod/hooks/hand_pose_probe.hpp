// TEST ONLY: an unbound private pose is created/updated/deleted in one CPU
// boundary before the native renderer. Never bind it to any original model.
namespace handPoseProbe {
using Create=uint32_t(__fastcall*)(void*,const void*);
using Update=void(__fastcall*)(void*,uint32_t,const void*);
using Remove=void(__fastcall*)(void*,uint32_t);
inline Render original=nullptr;
inline std::atomic<bool> accepting{false},claimed{false},complete{false};
inline std::atomic<unsigned> callbacks{0};
inline unsigned result=1;inline uintptr_t scene=0,nativeView=0;inline std::array<uint32_t,4> guid{};
inline std::vector<uint8_t> bytes(uintptr_t p,size_t count){
    if(!count||count>16384)throw std::runtime_error("pose snapshot bound");
    std::vector<uint8_t> b(count);SIZE_T n=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),b.data(),b.size(),&n)||n!=b.size())
        throw std::runtime_error("pose snapshot unreadable");
    return b;
}
inline uintptr_t jointData(uintptr_t record){
    const auto first=read<uint32_t>(record+8),count=read<uint32_t>(record+12);
    const auto data=read<uintptr_t>(scene+layout::sceneJoints),capacity=read<uint64_t>(scene+layout::sceneJointCapacity);
    if(!data||!count||count>256||capacity>1000000||uint64_t(first)+count>capacity)throw std::runtime_error("pose joint storage bound");
    return data+size_t(first)*48;
}
inline uint32_t findSource(){
    const auto pages=read<uintptr_t>(scene+layout::scenePosePages),count=read<uint64_t>(scene+layout::scenePoseCount),shift=read<uint64_t>(scene+layout::scenePoseShift);
    if(!pages||!count||count>256||shift>16)throw std::runtime_error("pose source table bound");
    for(uint64_t p=0;p<count;++p){
        const auto page=pages+p*64,records=read<uintptr_t>(page+8),size=read<uint64_t>(page+16);
        if(size>65536||(!records&&size))throw std::runtime_error("pose source page bound");
        for(uint64_t i=0;i<size;++i){
            const auto entry=records+i*layout::poseEntryBytes;const auto generation=read<uint32_t>(entry);
            if(generation&0x80000000u)continue;
            const auto owner=read<uintptr_t>(entry+8);
            if(owner&&read<std::array<uint32_t,4>>(owner)==guid){
                const auto index=(p<<shift)|i;if(index>65535||generation>65535)throw std::runtime_error("pose source handle overflow");
                const auto h=uint32_t(generation<<16)|uint32_t(index);handPoseMetadata::pose(scene,h);return h;
            }
        }
    }
    throw std::runtime_error("loaded hierarchy not found; no resource loading attempted");
}
inline void exercise(){
    uint32_t owned=0;
    try{
        const auto sourceHandle=findSource();const auto source=handPoseMetadata::pose(scene,sourceHandle);
        const auto resource=read<uintptr_t>(source),hierarchy=read<uintptr_t>(resource+24);
        const auto count=read<uint32_t>(source+12),refs=read<uint32_t>(resource+16);
        if(!hierarchy||!refs||refs==UINT32_MAX||!count||count>256||read<uint32_t>(hierarchy+48)!=count)
            throw std::runtime_error("pose source resource not ready");
        const auto sourceJoints=bytes(jointData(source),count*48);const auto sourceVersion=read<uint64_t>(source+24);
        const auto liveBefore=read<uint64_t>(scene+layout::scenePoseLive);
        const auto defaultOffset=read<int32_t>(hierarchy+64);const auto defaultCount=read<uint32_t>(hierarchy+68);
        if(!defaultOffset||defaultOffset < -16777216||defaultOffset>16777216||defaultCount!=((count+3u)&~3u))
            throw std::runtime_error("pose defaults alignment mismatch");
        const auto defaults=bytes(uintptr_t(int64_t(hierarchy+64)+defaultOffset),count*48);
        logFile->event("hand_pose_source","\"handle\":"+std::to_string(sourceHandle)+",\"joints\":"+std::to_string(count)+
            ",\"references\":"+std::to_string(refs)+",\"scene_live\":"+std::to_string(liveBefore)+",\"unbound_test\":true");
        owned=reinterpret_cast<Create>(baseAddress+0xfb850)(reinterpret_cast<void*>(scene),guid.data());
        if(!owned||owned==sourceHandle){owned=0;throw std::runtime_error("private pose creation failed/aliased");}
        const auto record=handPoseMetadata::pose(scene,owned);
        if(read<uintptr_t>(record)!=resource||read<uint32_t>(resource+16)!=refs+1||read<uint64_t>(scene+layout::scenePoseLive)!=liveBefore+1||
           read<uint32_t>(record+12)!=count||read<uint64_t>(record+0x58)||bytes(jointData(record),count*48)!=defaults)
            throw std::runtime_error("private pose creation accounting/defaults differ");
        logFile->event("hand_pose_created","\"handle\":"+std::to_string(owned)+",\"references_incremented\":true,\"live_incremented\":true,\"defaults_match\":true,\"bound_models\":0");
        auto changed=defaults;float old=0;std::memcpy(&old,changed.data()+16,4);
        if(!std::isfinite(old))throw std::runtime_error("nonfinite default root");const float moved=old+0.125f;
        std::memcpy(changed.data()+16,&moved,4);const auto beforeVersion=read<uint64_t>(record+24);
        std::array<uintptr_t,2> span{reinterpret_cast<uintptr_t>(changed.data()),count};
        reinterpret_cast<Update>(baseAddress+0x110f80)(reinterpret_cast<void*>(scene),owned,span.data());
        if(bytes(jointData(record),count*48)!=changed||read<uint64_t>(record+24)!=beforeVersion+1||
           bytes(jointData(source),count*48)!=sourceJoints||read<uint64_t>(source+24)!=sourceVersion||read<uint64_t>(record+0x58))
            throw std::runtime_error("private pose update/source isolation differs");
        handPoseMetadata::save(calibrationDirectory,"pose-original-joints.bin",reinterpret_cast<uintptr_t>(sourceJoints.data()),sourceJoints.size());
        handPoseMetadata::save(calibrationDirectory,"pose-owned-joints.bin",jointData(record),count*48);
        logFile->event("hand_pose_updated","\"native_update\":true,\"source_unchanged\":true,\"version_incremented\":true,\"delta\":0.125,\"bound_models\":0");
        const auto retired=owned;reinterpret_cast<Remove>(baseAddress+0x10e180)(reinterpret_cast<void*>(scene),owned);owned=0;
        bool invalid=false;try{handPoseMetadata::pose(scene,retired);}catch(...){invalid=true;}
        if(!invalid||read<uint32_t>(resource+16)!=refs||read<uint64_t>(scene+layout::scenePoseLive)!=liveBefore||
           bytes(jointData(source),count*48)!=sourceJoints||read<uint64_t>(source+24)!=sourceVersion)
            throw std::runtime_error("private pose cleanup accounting/source differs");
        logFile->event("hand_pose_retired","\"generation_invalidated\":true,\"references_restored\":true,\"live_restored\":true,\"source_unchanged\":true");result=0;
    }catch(const std::exception& e){
        if(owned){try{handPoseMetadata::pose(scene,owned);reinterpret_cast<Remove>(baseAddress+0x10e180)(reinterpret_cast<void*>(scene),owned);
            logFile->event("hand_pose_failure_cleanup","\"native_remove_called\":true");}catch(...){logFile->event("hand_pose_failure_cleanup","\"native_remove_called\":false");}}
        logFile->event("hand_pose_failure","\"reason\":"+jsonString(e.what()));
    }
    complete=true;
}
inline void* __fastcall render(void* out,void* context,void* selected,void* params,void* target){
    callbacks.fetch_add(1);
    if(accepting.load()&&!complete.load())try{
        const auto v=reinterpret_cast<uintptr_t>(selected),owner=read<uintptr_t>(v);
        if(v==nativeView&&read<uintptr_t>(v+8)==scene&&read<uintptr_t>(scene+layout::sceneView)==v&&
           read<uintptr_t>(owner+layout::ownerScene)==scene&&read<uintptr_t>(owner+layout::ownerView)==v&&finishEyeCpuTasks(scene)&&!claimed.exchange(true))exercise();
    }catch(const std::exception& e){logFile->event("hand_pose_failure","\"reason\":"+jsonString(e.what()));complete=true;}
    auto returned=original(out,context,selected,params,target);callbacks.fetch_sub(1);return returned;
}
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeHandPose(void* directory){
    if(!directory||started.exchange(true))return 2;HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ProbeHandPose),&pinned))return 3;
    using namespace handPoseProbe;bool installed=false;void* target=nullptr;
    auto cleanup=[&](){accepting=false;if(installed){require(MH_DisableHook(target),"pose hook disable");installed=false;}
        const auto until=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto p=parsePattern(renderPattern);const bool restored=target&&scan(static_cast<const uint8_t*>(target),p.size(),p)==std::vector<size_t>{0};
        logFile->event("hand_pose_hook_removed","\"restored\":"+std::string(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load()));
        if(!restored||callbacks.load())throw std::runtime_error("pose hook cleanup incomplete");};
    try{
        calibrationDirectory=std::filesystem::path(static_cast<wchar_t*>(directory));logFile=new Log((calibrationDirectory/"view-resource-probe.jsonl").string(),true);
        std::ifstream f(calibrationDirectory/"hand-pose.txt");std::string magic;f>>magic>>std::hex>>scene>>nativeView>>std::dec;for(auto& word:guid)f>>word;f>>std::ws;
        if(!f||!f.eof()||magic!="EVR_UNUSED_HAND_POSE_1"||!scene||!nativeView||!(guid[0]|guid[1]|guid[2]|guid[3]))throw std::runtime_error("invalid hand pose policy");
        baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("pose PE header");
        const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
        if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)throw std::runtime_error("pose unsupported build");
        for(auto e:std::array<std::pair<uintptr_t,const char*>,4>{{
          {renderRva,renderPattern},
          {0xfb850,"40 55 53 56 41 57 48 8d 6c 24 c1 48 81 ec 88 00 00 00 48 8b 42 08 33 db"},
          {0x110f80,"85 d2 0f 84 ? ? ? ? 48 89 74 24 20 41 56 48 83 ec 30 48 8b f1 44 0f"},
          {0x10e180,"85 d2 0f 84 ? ? ? ? 48 89 5c 24 20 57 48 83 ec 20 48 8b f9 8b da 48"}}}){
            const auto p=parsePattern(e.second);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+e.first),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("pose signature mismatch");
            logFile->event("hand_pose_signature","\"rva\":"+hex(e.first)+",\"pattern\":"+jsonString(e.second)+",\"matched\":true");}
        worldNativeDiagnostic=true;target=reinterpret_cast<void*>(baseAddress+renderRva);require(MH_Initialize(),"pose MH initialize");
        require(MH_CreateHook(target,reinterpret_cast<void*>(&handPoseProbe::render),reinterpret_cast<void**>(&handPoseProbe::original)),"pose hook create");
        accepting=true;require(MH_EnableHook(target),"pose hook enable");installed=true;
        const auto until=GetTickCount64()+15000;while(!complete.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        cleanup();if(!complete.load())throw std::runtime_error("pose no idle render boundary");
        logFile->event("hand_pose_complete","\"result\":"+std::to_string(result)+",\"drawn\":false");return result;
    }catch(const std::exception& e){if(installed)try{cleanup();}catch(...){}if(logFile)logFile->event("hand_pose_failure","\"reason\":"+jsonString(e.what()));return 1;}
}
