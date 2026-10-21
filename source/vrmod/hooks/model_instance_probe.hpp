// Explicit unused-native-model lifecycle proof. Never connected to VR visuals.
namespace modelInstanceProbe {
using Create=uint32_t(__fastcall*)(void*,const void*,const void*);
using Change=void(__fastcall*)(void*,const void*,const void*);
using Remove=void(__fastcall*)(void*,const void*);
using Mask=void(__fastcall*)(void*,void*,uint32_t);
inline Render original=nullptr;
inline std::atomic<bool> accepting{false},complete{false},claimed{false};
inline std::atomic<unsigned> callbacks{0};
inline unsigned result=1,holdFrames=0;
inline std::atomic<uint32_t> heldHandle{0};inline std::atomic<bool> stopHold{false};
inline void beginHold(uint32_t handle,uintptr_t view,uintptr_t resource);
inline void holdTick(uintptr_t view);
inline uintptr_t scene=0;inline uint32_t sourceHandle=0;inline std::array<uint32_t,4> guid{};
struct Scope {Scope(){++callbacks;}~Scope(){--callbacks;}};
inline uintptr_t object(uint32_t handle){
    const auto objects=read<uintptr_t>(scene+0x1b0),gens=read<uintptr_t>(scene+0x1c8);
    const auto bytes=read<uint64_t>(scene+0x1b8),count=read<uint64_t>(scene+0x1d0);const auto index=handle&0xffffffu;
    if(!handle||!objects||!gens||bytes%0x1b0||index>=bytes/0x1b0||index>=count||read<uint8_t>(gens+index)!=(handle>>24))
        throw std::runtime_error("model lifecycle stale/out-of-range handle");
    return objects+index*0x1b0;
}
inline void remove(uint32_t handle){
    object(handle); // Never call native lookup on an unbounded or stale handle.
    std::array<uintptr_t,2> span{reinterpret_cast<uintptr_t>(&handle),1};
    reinterpret_cast<Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),span.data());
}
inline void exercise(uintptr_t view){
    uint32_t created=0;
    try{
        const auto owner=read<uintptr_t>(view);
        if(!owner||read<uintptr_t>(owner+layout::ownerScene)!=scene||read<uintptr_t>(owner+layout::ownerView)!=view||read<uintptr_t>(scene+layout::sceneView)!=view||!(read<uint32_t>(owner+0x10)&0x40u))
            throw std::runtime_error("model lifecycle world Scene/View ownership differs");
        const auto source=object(sourceHandle),resource=read<uintptr_t>(source+layout::modelResource);
        const auto sourceBytes=read<std::array<uint8_t,0x1b0>>(source);
        if(!resource||read<uint8_t>(resource+0x14)!=1||read<std::array<uint32_t,4>>(resource+0x18)!=guid)
            throw std::runtime_error("model lifecycle source resource identity differs");
        const auto refs=read<uint32_t>(resource+0x10);
        logFile->event("model_instance_source","\"scene\":"+hex(scene)+",\"view\":"+hex(view)+",\"source_handle\":"+std::to_string(sourceHandle)+
            ",\"references\":"+std::to_string(refs)+",\"auxiliary64\":"+std::to_string(read<uint32_t>(resource+layout::resourceSdf))+",\"auxiliary68\":"+std::to_string(read<uint32_t>(resource+layout::resourceAuxiliary))+
            ",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"drawn\":false");
        if(!refs||refs==UINT32_MAX)throw std::runtime_error("model resource refcount invalid");
        // +64 is SDF, paired native ownership 123300/128f30. Auxiliary +68
        // remains unsupported. This proof creates/removes SDF in one idle CPU
        // boundary and verifies the live instance count returns to baseline.
        if(read<uint32_t>(resource+layout::resourceAuxiliary))throw std::runtime_error("model auxiliary68 ownership unresolved; creation refused");
        const bool hasSdf=read<uint32_t>(resource+layout::resourceSdf)!=0;const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);
        if(hasSdf&&!sdf)throw std::runtime_error("SDF owner absent");
        const auto sdfCount=hasSdf?read<uint64_t>(sdf+0x78):0;
        auto transform=read<std::array<uint8_t,56>>(source+0x38);
        const auto beforeTransform=transform;
        created=reinterpret_cast<Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(scene),guid.data(),transform.data());
        if(!created||created==sourceHandle){created=0;throw std::runtime_error("native clone creation failed/aliased source");}
        const auto clone=object(created);
        if(read<uint32_t>(clone+layout::modelMask)!=1)throw std::runtime_error("native constructor default mask changed");
        reinterpret_cast<Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        if(hasSdf&&(!read<uint32_t>(clone+layout::modelSdf)||read<uint64_t>(sdf+0x78)!=sdfCount+1))throw std::runtime_error("native SDF instance accounting mismatch");
        logFile->event("model_instance_sdf_created","\"present\":"+std::string(hasSdf?"true":"false")+",\"count_before\":"+std::to_string(sdfCount)+",\"count_after\":"+std::to_string(hasSdf?read<uint64_t>(sdf+0x78):0));
        if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)!=0||read<uint32_t>(resource+0x10)!=refs+1)
            throw std::runtime_error("native clone visibility/resource/refcount mismatch");
        if(read<std::array<uint8_t,56>>(clone+0x38)!=transform||read<std::array<uint8_t,56>>(clone)!=transform)
            throw std::runtime_error("native clone initial transform history mismatch");
        logFile->event("model_instance_created","\"handle\":"+std::to_string(created)+",\"mask\":0,\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(refs+1)+",\"drawn\":false");
        auto x=read<int64_t>(reinterpret_cast<uintptr_t>(transform.data()));
        constexpr int64_t delta=536870912; // Exactly 0.125m Q32, unused diagnostic only.
        if(x>INT64_MAX-delta)throw std::runtime_error("diagnostic model position overflow");
        x+=delta;std::memcpy(transform.data(),&x,8);
        std::array<uintptr_t,2> handles{reinterpret_cast<uintptr_t>(&created),1},transforms{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),handles.data(),transforms.data());
        const auto moved=read<std::array<uint8_t,56>>(object(created)+0x38);
        if(std::memcmp(moved.data(),transform.data(),24)||read<uint32_t>(object(created)+layout::modelMask)!=0)
            throw std::runtime_error("native unused model movement/mask mismatch");
        if(read<std::array<uint8_t,56>>(object(created))!=beforeTransform)
            throw std::runtime_error("native previous transform unexpectedly changed");
        if(read<std::array<uint8_t,0x1b0>>(object(sourceHandle))!=sourceBytes)throw std::runtime_error("native original model changed during clone exercise");
        logFile->event("model_instance_moved","\"native_transform_api\":true,\"delta_q32\":536870912,\"source_unchanged\":true,\"previous_preserved\":true,\"mask\":0");
        if(holdFrames){beginHold(created,view,resource);created=0;return;}
        const auto retired=created;remove(created);created=0;
        const auto gens=read<uintptr_t>(scene+0x1c8);
        if(read<uint8_t>(gens+(retired&0xffffff))==uint8_t(retired>>24)||read<uint32_t>(resource+0x10)!=refs||read<std::array<uint8_t,0x1b0>>(object(sourceHandle))!=sourceBytes)
            throw std::runtime_error("native model retirement/refcount/source restoration mismatch");
        if(hasSdf&&read<uint64_t>(sdf+0x78)!=sdfCount)throw std::runtime_error("native SDF live count not restored");
        logFile->event("model_instance_sdf_retired","\"present\":"+std::string(hasSdf?"true":"false")+",\"count_restored\":true");
        logFile->event("model_instance_retired","\"handle\":"+std::to_string(retired)+",\"generation_invalidated\":true,\"references_restored\":true,\"source_unchanged\":true,\"gpu_retirement_verified\":false");
        result=0;
    }catch(const std::exception& e){
        if(created){try{remove(created);logFile->event("model_instance_failure_cleanup","\"handle\":"+std::to_string(created)+",\"native_remove_called\":true");}catch(...){logFile->event("model_instance_failure_cleanup","\"handle\":"+std::to_string(created)+",\"native_remove_called\":false");}}
        logFile->event("model_instance_failure","\"reason\":"+jsonString(e.what()));result=1;
    }
    complete=true;
}
#include "model_instance_persistence.hpp"
inline void* __fastcall render(void* out,void* context,void* view,void* params,void* target){
    const Scope callback;const auto returned=original(out,context,view,params,target);
    const ObserverLastError preserve;
    if(accepting.load()&&!complete.load())try{
        const auto v=reinterpret_cast<uintptr_t>(view);
        if(read<uintptr_t>(v+8)==scene&&reinterpret_cast<Wait>(baseAddress+0x1092e0)(reinterpret_cast<void*>(scene),0)){
            if(heldHandle.load())holdTick(v);else if(!claimed.exchange(true))exercise(v);
        }
    }catch(const std::exception& e){logFile->event("model_instance_failure","\"reason\":"+jsonString(e.what()));complete=true;}
    return returned;
}
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeModelInstance(void* directory){
    if(!directory||started.exchange(true))return 2;
    HMODULE pinned{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ProbeModelInstance),&pinned))return 3;
    using namespace modelInstanceProbe;bool installed=false;void* target=nullptr;
    auto cleanup=[&](){accepting=false;if(installed){require(MH_DisableHook(target),"model render hook disable");installed=false;}
        const auto until=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto p=parsePattern(renderPattern);const bool restored=target&&scan(static_cast<const uint8_t*>(target),p.size(),p)==std::vector<size_t>{0};
        logFile->event("model_instance_hook_removed","\"restored\":"+std::string(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load()));
        if(!restored||callbacks.load())throw std::runtime_error("model hook cleanup incomplete");};
    try{
        const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));logFile=new Log((path/"view-resource-probe.jsonl").string(),true);
        std::ifstream f(path/"model-instance.txt");std::string magic;f>>magic>>std::hex>>scene>>std::dec>>sourceHandle;
        for(auto& word:guid)f>>word;
        if(magic=="EVR_UNUSED_MODEL_HOLD_1")f>>holdFrames;f>>std::ws;
        if(!f||!f.eof()||(magic!="EVR_UNUSED_MODEL_1"&&magic!="EVR_UNUSED_MODEL_HOLD_1")||!scene||!sourceHandle||!(guid[0]|guid[1]|guid[2]|guid[3]))throw std::runtime_error("invalid model lifecycle policy");
        if((magic=="EVR_UNUSED_MODEL_HOLD_1"&&(!holdFrames||holdFrames>600))||(magic=="EVR_UNUSED_MODEL_1"&&holdFrames))throw std::runtime_error("invalid bounded hold count");
        baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("model PE header");
        const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
        if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)throw std::runtime_error("model unsupported build");
        for(auto e:std::array<std::pair<uintptr_t,const char*>,8>{{
            {0x129fc0,"4c 89 4c 24 20 4c 89 44 24 18 48 89 54 24 10 48 89 4c 24 08 55 53 56 57 41 54 48 8d"},
            {0x1092e0,"48 89 5c 24 10 55 56 57 48 83 ec 50 48 8b 79 40 48 8b da 48"},
            {0xfb4e0,"40 55 57 41 54 41 56 41 57 48 83 ec 30 48 8b 42 08 4d 8b f8"},
            {0x115240,"48 8b c4 44 88 48 20 4c 89 40 18 55 56 41 55 41 57 48 81 ec 98 00 00 00"},
            {0x10e000,"48 89 6c 24 20 57 41 54 41 56 48 83 ec 30 45 33 e4 4c 8b f2"},
            {0xf70c0,"44 89 82 14 01 00 00 4c 8b ca 4c 8b c2 48 b8 5f 42 7b 09 ed"},
            {0x16f9e0,"48 89 6c 24 10 48 89 74 24 18 48 89 7c 24 20 41 56 48 83 ec"},
            {0x15a640,"85 d2 0f 84 ? ? ? ? 89 54 24 10 4c 8b dc 49 89 4b 08 55"}}}){
            const auto p=parsePattern(e.second);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+e.first),p.size(),p)!=std::vector<size_t>{0})throw std::runtime_error("model lifecycle signature mismatch");
            logFile->event("model_instance_signature","\"rva\":"+hex(e.first)+",\"pattern\":"+jsonString(e.second)+",\"matched\":true");}
        target=reinterpret_cast<void*>(baseAddress+renderRva);require(MH_Initialize(),"model MH initialize");
        require(MH_CreateHook(target,reinterpret_cast<void*>(&modelInstanceProbe::render),reinterpret_cast<void**>(&modelInstanceProbe::original)),"model hook create");
        accepting=true;require(MH_EnableHook(target),"model hook enable");installed=true;
        const auto until=GetTickCount64()+(holdFrames?30000:15000);while(!complete.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if(!complete.load()&&heldHandle.load()){stopHold=true;const auto grace=GetTickCount64()+3000;while(!complete.load()&&GetTickCount64()<grace)std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        if(heldHandle.load())logFile->event("model_instance_failure","\"reason\":\"owned model retained until normal game exit: no idle cleanup boundary\"");
        cleanup();if(!complete.load())throw std::runtime_error("model lifecycle no idle world render boundary");
        logFile->event("model_instance_complete","\"result\":"+std::to_string(result)+",\"drawn\":false");return result;
    }catch(const std::exception& e){if(installed)try{cleanup();}catch(...){}if(logFile)logFile->event("model_instance_failure","\"reason\":"+jsonString(e.what()));return 1;}
}
