// Included inside modelInstanceProbe after the single-callback lifecycle proof.
// Only owned model fields are written. Shared source models/resources are read.
inline uintptr_t heldView=0,heldResource=0;inline DWORD heldThread=0;
inline unsigned holdSamples=0;inline uint64_t heldBeginMs=0;
inline std::array<uint8_t,56> heldTransform{};
inline void beginHold(uint32_t handle,uintptr_t view,uintptr_t resource){
    const auto clone=object(handle),source=object(sourceHandle);
    const auto sourceBytes=read<std::array<uint8_t,0x1b0>>(source);
    const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto id=read<uint32_t>(clone+layout::modelSdf);
    if(read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(resource+layout::resourceAuxiliary))throw std::runtime_error("hold unknown auxiliary present");
    if(id){
        if(!sdf||id==read<uint32_t>(source+layout::modelSdf))throw std::runtime_error("owned SDF aliases original/has no owner");
        const auto count=read<uint64_t>(sdf+0x78);if(!count)throw std::runtime_error("owned SDF count underflow");
        // Same native destructor as d9f40; clear only our lifetime-owned handle
        // so native movement/retirement sees the documented no-SDF sentinel.
        using SdfRemove=void(__fastcall*)(void*,uint32_t);
        reinterpret_cast<SdfRemove>(baseAddress+0x15a540)(reinterpret_cast<void*>(sdf),id);
        *reinterpret_cast<uint32_t*>(clone+layout::modelSdf)=0;
        if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("owned SDF detach count mismatch");
        logFile->event("model_instance_sdf_detached","\"handle\":"+std::to_string(id)+",\"count_before\":"+std::to_string(count)+",\"count_after\":"+std::to_string(count-1)+",\"only_owned_handle_cleared\":true");
    }
    if(read<std::array<uint8_t,0x1b0>>(object(sourceHandle))!=sourceBytes)throw std::runtime_error("source changed during owned SDF detach");
    heldView=view;heldResource=resource;heldThread=GetCurrentThreadId();holdSamples=0;heldBeginMs=GetTickCount64();
    heldTransform=read<std::array<uint8_t,56>>(object(handle)+0x38);heldHandle=handle;
    logFile->event("model_instance_hold_started","\"handle\":"+std::to_string(handle)+",\"frames_requested\":"+std::to_string(holdFrames)+",\"sdf_handle\":0,\"mask\":0,\"drawn\":false");
}
inline void retireHeld(uint32_t handle){
    const auto source=object(sourceHandle),clone=object(handle);
    const auto sourceBytes=read<std::array<uint8_t,0x1b0>>(source);
    if(read<uintptr_t>(clone+layout::modelResource)!=heldResource||read<uintptr_t>(source+layout::modelResource)!=heldResource)throw std::runtime_error("held retirement resource differs");
    const auto refs=read<uint32_t>(heldResource+0x10);if(refs<2)throw std::runtime_error("held resource missing reference");
    const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=sdf?read<uint64_t>(sdf+0x78):0;
    if(read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelAuxiliary))throw std::runtime_error("held auxiliary unexpectedly reappeared");
    remove(handle);heldHandle=0;
    const auto gens=read<uintptr_t>(scene+0x1c8);
    if(read<uint8_t>(gens+(handle&0xffffff))==uint8_t(handle>>24)||read<uint32_t>(heldResource+0x10)!=refs-1||
       read<std::array<uint8_t,0x1b0>>(object(sourceHandle))!=sourceBytes||(sdf&&read<uint64_t>(sdf+0x78)!=count))
        throw std::runtime_error("held native retirement did not restore current reference/count/source");
    logFile->event("model_instance_hold_retired","\"handle\":"+std::to_string(handle)+",\"frames\":"+std::to_string(holdSamples)+",\"elapsed_ms\":"+std::to_string(GetTickCount64()-heldBeginMs)+
        ",\"generation_invalidated\":true,\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(refs-1)+",\"source_unchanged\":true,\"sdf_unchanged\":true,\"drawn\":false,\"gpu_retirement_verified\":false");
}
inline void holdTick(uintptr_t view){
    const auto handle=heldHandle.load();
    try{
        if(view!=heldView||GetCurrentThreadId()!=heldThread)throw std::runtime_error("held Scene/View/thread changed");
        const auto clone=object(handle),source=object(sourceHandle),owner=read<uintptr_t>(view);
        if(!owner||read<uintptr_t>(owner+layout::ownerScene)!=scene||read<uintptr_t>(owner+layout::ownerView)!=view||read<uintptr_t>(scene+layout::sceneView)!=view)
            throw std::runtime_error("held world ownership changed");
        if(read<uintptr_t>(clone+layout::modelResource)!=heldResource||read<uintptr_t>(source+layout::modelResource)!=heldResource||read<uint8_t>(heldResource+0x14)!=1||read<std::array<uint32_t,4>>(heldResource+0x18)!=guid)
            throw std::runtime_error("held resource/generation changed");
        if(read<uint32_t>(clone+layout::modelMask)!=0||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelAuxiliary))throw std::runtime_error("held native model became visible or recreated auxiliary");
        const auto sourceBytes=read<std::array<uint8_t,0x1b0>>(source);
        auto transform=heldTransform;int64_t x=0;std::memcpy(&x,transform.data(),8);
        const int64_t delta=int64_t(std::sin(double(holdSamples)*.1)*double(536870912));
        if((delta>0&&x>INT64_MAX-delta)||(delta<0&&x<INT64_MIN-delta))throw std::runtime_error("held diagnostic position overflow");
        x+=delta;std::memcpy(transform.data(),&x,8);
        std::array<uintptr_t,2> handles{reinterpret_cast<uintptr_t>(&handle),1},transforms{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),handles.data(),transforms.data());
        if(read<std::array<uint8_t,0x1b0>>(object(sourceHandle))!=sourceBytes||std::memcmp(read<std::array<uint8_t,56>>(object(handle)+0x38).data(),transform.data(),24)||read<uint32_t>(object(handle)+layout::modelSdf))
            throw std::runtime_error("held transform/source/SDF mismatch");
        ++holdSamples;
        logFile->event("model_instance_hold_sample","\"sample\":"+std::to_string(holdSamples)+",\"handle\":"+std::to_string(handle)+",\"mask\":0,\"sdf_handle\":0,\"source_unchanged\":true,\"source_handle\":"+std::to_string(sourceHandle)+",\"native_transform\":true,\"auxiliary138\":"+std::to_string(read<uint32_t>(object(handle)+layout::modelPaletteHandle)));
        if(holdSamples>=holdFrames||stopHold.load()){
            retireHeld(handle);result=stopHold.load()?1:0;
            if(result)logFile->event("model_instance_failure","\"reason\":\"bounded hold timed out and retired\"");
            complete=true;
        }
    }catch(const std::exception& e){
        if(heldHandle.load())try{remove(handle);heldHandle=0;logFile->event("model_instance_failure_cleanup","\"handle\":"+std::to_string(handle)+",\"native_remove_called\":true");}catch(...){logFile->event("model_instance_failure_cleanup","\"handle\":"+std::to_string(handle)+",\"native_remove_called\":false");}
        result=1;logFile->event("model_instance_failure","\"reason\":"+jsonString(e.what()));complete=true;
    }
}
