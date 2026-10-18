// Camera-control observation. The native weighted rotation input of the camera update is sampled before
// follow/glider/combat modifiers. Identity addresses are compared only; no ECS pointer is dereferenced after a callback.
// One write (2026-10-13, heading realignment, include/evr/camera_mode.hpp headingCorrection): the requested amount is
// added to that update's weighted yaw sum, so the native camera turns by it and the VR view does not (the amount is not
// counted in gameplayTurn).
// 1.0 (2026-10-16, Hotfix 1.0.0.1 addresses): the camera update is 0xeaa940 (one caller, the call at 0x2b25b5, as June's
// 0xce99d0 from 0x2661e6), its rotation part 0xea65a0 (June 0xce3a00, called with the controller +0x30, June +0x28). June
// called an orbit function (0xca5040) per camera layer that added input * sensitivity * weight to a local; 1.0 asks
// 0xe850f0 for each layer's input and sums input * weight in xmm14 (yaw) / xmm15 (pitch) - the remaining weight's layer
// last - and the two paths meet at 0xea67ac, before the layers use the sums (stored at rbp+0x198 on three paths). The
// observer there (camera_sum_stub.asm: every register and the SSE state saved; the callback may change the saved xmm14)
// reads the player's weighted turn and adds the realignment. The parameters' entity is +0x28 (June +0x2c), the camera id
// +0x20, the controller's camera id +0x1460 (June +0xea8; stored at 0xeab0e1, June 0xcea9ab), in 1.0's numbering.
namespace cameraControl {
using Update=void(__fastcall*)(void*,const void*);
inline Update originalUpdate=nullptr;
inline bool updateEnabled=false,sumEnabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},calls{0},failures{0};
constexpr uintptr_t updateRva=0xeaaf60,updateCaller=0x2b28da,sumRva=0xea6dcc,modeStoreRva=0xeab701,rotationCall=0xeab399;
constexpr uintptr_t stateOffset=0x30,cameraOffset=0x1460,parameterEntity=0x28,parameterCamera=0x20,parameterRaw=0xf0;
constexpr const char* updatePattern="48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 56 41 57 48 8D A8 88 FE FF FF 48 81 EC 60 02 00 00";
// movss xmm7,[rip+..] ; xor r13d,r13d ; test r12,r12 - the join after the remaining weight's layer
constexpr const char* sumPattern="F3 0F 10 3D ? ? ? ? 45 33 ED 4D 85 E4";
struct Sample {
    uint64_t number=0,milliseconds=0;uintptr_t identity=0;
    uint32_t entity=0;uint8_t camera=255,inputMode=255,context=255;
    std::array<float,2> raw{},weighted{};unsigned orbitCalls=0;
    bool valid=false;double gameplayTurn=0;
};
struct Pending {Sample value;bool failed=false;float added=0;};
inline std::atomic<float> realignRequest{0},disturbRequest{0};inline std::atomic<bool> realignEnabled{false};
inline std::atomic<unsigned> realignApplied{0};
inline thread_local Pending* current=nullptr;
inline std::mutex mutex;inline Sample latest;inline double gameplayTurn=0;inline uint32_t turnEntity=0;
struct CallbackScope {CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
// regs: the stub's pushes from r15 (index 0) to the flags (15): rdi 8, rsi 9. fx: the FXSAVE64 area, XMM i at 0xa0 + 16 i.
inline void onSum(const uintptr_t* regs,uint8_t* fx) noexcept {
    const CallbackScope callback;
    if(!accepting||!current)return;
    const ObserverLastError preserve;
    try{
        auto& p=*current;const auto state=regs[8],parameters=regs[9];
        if(state!=p.value.identity+stateOffset||!parameters||p.value.orbitCalls)throw std::runtime_error("camera rotation sum scope differs");
        float yaw=0,pitch=0;std::memcpy(&yaw,fx+0xa0+14*16,4);std::memcpy(&pitch,fx+0xa0+15*16,4);
        if(!std::isfinite(yaw)||!std::isfinite(pitch))throw std::runtime_error("nonfinite weighted rotation");
        p.value.weighted={yaw,pitch};p.value.raw=read<std::array<float,2>>(parameters+parameterRaw);
        p.value.inputMode=0;p.value.context=0;++p.value.orbitCalls;   // 1.0: mouse and controller input are summed alike
        // realignRequest: the correction. disturbRequest: a diagnostic turn "by the game" (EVR_HEADING_REALIGN_TEST).
        float add=disturbRequest.exchange(0);
        if(realignEnabled.load()){const float c=realignRequest.exchange(0);if(c!=0&&game::headingRealignCamera(p.value.camera)){add+=c;++realignApplied;}}
        if(add!=0&&std::isfinite(add)&&std::abs(add)<=1.f){yaw+=add;std::memcpy(fx+0xa0+14*16,&yaw,4);p.added=add;}
    }catch(...){current->failed=true;++failures;}
}
inline bool dumped=false;
__declspec(noinline) void __fastcall update(void* controller,const void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    const CallbackScope callback;Pending p;auto* previous=current;
    {const ObserverLastError preserve;
     if(accepting&&caller==updateCaller)try{
        const auto args=reinterpret_cast<uintptr_t>(parameters);
        p.value.number=++calls;p.value.milliseconds=GetTickCount64();p.value.identity=reinterpret_cast<uintptr_t>(controller);
        p.value.entity=read<uint32_t>(args+parameterEntity);p.value.camera=game::legacyCameraId(read<uint8_t>(args+parameterCamera));
        if(!dumped){dumped=true;const auto raw=read<std::array<uint8_t,0x40>>(args);std::string text;char b[4];
            for(auto v:raw){std::snprintf(b,sizeof b,"%02x",v);text+=b;}
            logFile->event("camera_control_parameters","\"bytes\":\""+text+"\",\"entity\":"+std::to_string(p.value.entity)+",\"camera_id\":"+std::to_string(unsigned(p.value.camera)));}
        if(!p.value.entity||p.value.camera>=45||current)throw std::runtime_error("camera update scope/entity/mode invalid");
        current=&p;
     }catch(...){p.failed=true;++failures;current=nullptr;}}
    originalUpdate(controller,parameters);
    {const ObserverLastError preserve;current=previous;
     if(p.value.number){
        try{p.value.valid=!p.failed&&p.value.orbitCalls&&
                game::legacyCameraId(read<uint8_t>(p.value.identity+cameraOffset))==p.value.camera;
            {std::lock_guard<std::mutex> lock(mutex);
                if(turnEntity!=p.value.entity){turnEntity=p.value.entity;gameplayTurn=0;}
                if(p.value.valid&&game::cameraPresentation(p.value.camera)==game::CameraPresentation::FirstPerson)
                    gameplayTurn+=p.value.weighted[0];
                p.value.gameplayTurn=gameplayTurn;latest=p.value;}
            if(p.value.number<=8||p.value.number%120==0||!p.value.valid){
                const auto& s=p.value;std::ostringstream out;out<<std::setprecision(10)
                    <<"\"number\":"<<s.number<<",\"entity\":"<<s.entity<<",\"camera_id\":"<<unsigned(s.camera)
                    <<",\"native_input_mode\":"<<unsigned(s.inputMode)<<",\"input_context\":"<<unsigned(s.context)
                    <<",\"raw_rotation\":["<<s.raw[0]<<','<<s.raw[1]<<"],\"weighted_rotation\":["<<s.weighted[0]<<','<<s.weighted[1]
                    <<"],\"added\":"<<p.added<<",\"orbit_calls\":"<<s.orbitCalls<<",\"valid\":"<<(s.valid?"true":"false")<<",\"read_only\":true";
                logFile->event("camera_control_sample",out.str());
            }
        }catch(...){++failures;std::lock_guard<std::mutex> lock(mutex);latest={};}
     }}
}
inline Sample snapshot(uintptr_t identity,uint32_t entity,uint8_t camera,uint64_t now){
    std::lock_guard<std::mutex> lock(mutex);auto s=latest;
    s.valid=s.valid&&s.identity==identity&&s.entity==entity&&s.camera==camera&&now>=s.milliseconds&&now-s.milliseconds<=200;
    s.identity=0;return s;
}
inline void verify(){
    const auto update=parsePattern(updatePattern),sum=parsePattern(sumPattern),store=parsePattern("88 83 60 14 00 00");
    const auto site=baseAddress+updateCaller-5,rotation=baseAddress+rotationCall-5;
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+updateRva),update.size(),update)!=std::vector<size_t>{0}||
       read<uint8_t>(site)!=0xe8||uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=baseAddress+updateRva)
        throw std::runtime_error("camera update signature/caller changed");
    // the rotation part is called from the update with the controller +0x30, and its sums meet at sumRva
    if(read<uint8_t>(rotation)!=0xe8||uintptr_t(int64_t(rotation+5)+read<int32_t>(rotation+1))!=baseAddress+0xea6bc0||
       read<uint32_t>(rotation-9)!=0x304b8d48u||scan(reinterpret_cast<const uint8_t*>(baseAddress+sumRva),sum.size(),sum)!=std::vector<size_t>{0})
        throw std::runtime_error("camera rotation sum signature changed");
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+modeStoreRva),store.size(),store)!=std::vector<size_t>{0})
        throw std::runtime_error("camera mode store signature changed");
}
inline void install(){
    verify();
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+updateRva),reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&originalUpdate))!=MH_OK||
       MH_CreateHook(reinterpret_cast<void*>(baseAddress+sumRva),reinterpret_cast<void*>(&evr_camera_sum_stub),&evr_camera_sum_trampoline)!=MH_OK)
        throw std::runtime_error("camera control observer hook creation failed");
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+sumRva))!=MH_OK)throw std::runtime_error("camera rotation sum observer enable failed");
    sumEnabled=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+updateRva))!=MH_OK)throw std::runtime_error("camera update observer enable failed");
    updateEnabled=true;accepting=true;
    logFile->event("camera_control_observer_enabled","\"update_rva\":"+hex(updateRva)+",\"sum_rva\":"+hex(sumRva)+",\"camera_offset\":"+std::to_string(cameraOffset)+",\"read_only\":false");
}
inline void disable(){
    accepting=false;const bool had=updateEnabled||sumEnabled;
    if(updateEnabled){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+updateRva))!=MH_OK)throw std::runtime_error("camera update disable failed");updateEnabled=false;}
    if(sumEnabled){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+sumRva))!=MH_OK)throw std::runtime_error("camera rotation sum disable failed");sumEnabled=false;}
    if(!had)return;
    const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    verify();
    logFile->event("camera_control_observer_removed","\"restored\":true,\"callbacks\":"+std::to_string(callbacks.load())+
        ",\"calls\":"+std::to_string(calls.load())+",\"failures\":"+std::to_string(failures.load()));
    if(callbacks.load())throw std::runtime_error("camera control callbacks still active");
}
}
