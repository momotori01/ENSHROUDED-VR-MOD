// Read-only DLSS evaluation observation (diagnostic, build 23966345, 2026-10-05).
// 0xdbe9f0 is the game's inlined NGX_VULKAN_EVALUATE_DLSS_EXT wrapper, called only from the
// upscaler command executor at 0xe11d51. Arguments: rcx command buffer, rdx NGX feature handle
// (context+0xe8), r8 NGX parameter object (context+0xe0), r9 NVSDK_NGX_VK_DLSS_Eval_Params (NGX SDK layout: color +0, output +8,
// depth +0x18, motion vectors +0x20, jitter +0x28/+0x2c, render subrect +0x30/+0x34, reset +0x38,
// MV scale +0x3c/+0x40). NVSDK_NGX_Resource_VK: view +0, image +8, format +0x24, width +0x28,
// height +0x2c, type +0x30. Nothing is written; the original call always runs.
namespace dlssEval {
using Evaluate=int(__fastcall*)(void*,void*,void*,void*);
inline Evaluate original=nullptr;inline bool enabled=false,requested=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},calls{0},failures{0},logged{0};
constexpr uintptr_t rva=0xfac380,caller=0x1007534;
constexpr const char* pattern="48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 40 49 8B F8 49";
constexpr unsigned logLimit=4000;
inline bool registerDiagnostics=false;inline VkDevice diagDevice{};inline std::mutex diagMutex;inline std::array<uintptr_t,2> diagOutputs{};inline unsigned diagCount=0;
// Alternate-eye warping (EVR_EYE_WARP=1 or 2, layer/native_xr.hpp evrNoteEyeDepth): every evaluation of an eye-sized
// picture tells the layer which depth image the game gave DLSS for it (the output image names the picture). Read only.
inline const bool depthNotes=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_WARP",v,sizeof v);return n==1&&(v[0]=='1'||v[0]=='2');}();
inline std::atomic<unsigned> depthNoted{0},depthNoteFailures{0};
inline void noteDepth(uintptr_t p){
    if(!depthNotes||!diagDevice)return;
    try{
        const auto out=read<uintptr_t>(p+8),dep=read<uintptr_t>(p+0x18);if(!out||!dep)return;
        const auto w=read<uint32_t>(out+0x28),h=read<uint32_t>(out+0x2c);
        if(!((w==eyeWidth(0)&&h==eyeHeight(0))||(w==eyeWidth(1)&&h==eyeHeight(1))))return;
        // With DLSS's jitter of this evaluation (+0x28 / +0x2c), so that the warp can read the depth where the colour stands.
        using Note=int(*)(VkDevice,VkImage,VkImage,uint32_t,uint32_t,VkFormat,float,float);
        static const Note note=[]{const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");return layer?reinterpret_cast<Note>(GetProcAddress(layer,"evrNoteEyeDepthJitter")):nullptr;}();
        if(!note){depthNoteFailures.fetch_add(1);return;}
        const int r=note(diagDevice,reinterpret_cast<VkImage>(read<uintptr_t>(out+8)),reinterpret_cast<VkImage>(read<uintptr_t>(dep+8)),read<uint32_t>(dep+0x28),read<uint32_t>(dep+0x2c),VkFormat(read<uint32_t>(dep+0x24)),read<float>(p+0x28),read<float>(p+0x2c));
        const auto n=depthNoted.fetch_add(1)+1;
        if(n<=2||r)logFile->event("dlss_depth_noted","\"count\":"+std::to_string(n)+",\"result\":"+std::to_string(r)+",\"output\":"+hex(read<uintptr_t>(out+8))+",\"depth\":"+hex(read<uintptr_t>(dep+8))+",\"depth_size\":["+std::to_string(read<uint32_t>(dep+0x28))+","+std::to_string(read<uint32_t>(dep+0x2c))+"],\"depth_format\":"+std::to_string(read<uint32_t>(dep+0x24)));
    }catch(...){depthNoteFailures.fetch_add(1);}
}
// Alternate-eye rendering (2026-10-23): an eye drawn one frame in two has DLSS's history two frames back, the game's motion
// vectors of moving things one (layer/eye_motion_fix.hpp). Just before such an evaluation the layer writes corrected motion
// vectors into its own image from the game's, the depth and the matrix from this frame's clip space to the eye's last
// drawing's - the two game cameras the mod gave the game (view_resource_probe.cpp alternateEyes::current / drawn) -, and
// the evaluation's motion vector resource (+0x20) points at a copy naming that image for the call. EVR_AFW_MOTION_FIX=0:
// off. EVR_AFW_MOTION_YSIGN=1: clip y and the image's rows the same way (default -1). EVR_AFW_MOTION_LAYOUT=general: the
// game's motion and depth sampled in GENERAL (default SHADER_READ_ONLY_OPTIMAL).
inline const bool motionFix=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_AFW_MOTION_FIX",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<unsigned> motionFixed{0},motionFailures{0};
inline std::array<unsigned char,0x38> motionResource{};   // NVSDK_NGX_Resource_VK: view +0, image +8, range +0x10, format +0x24, size +0x28/+0x2c
using Matrix=std::array<double,16>;
inline Matrix rowsOf(const game::MatrixRows& m){Matrix r{};for(int i=0;i<16;i++)r[i]=m.values[i];return r;}
inline Matrix multiply(const Matrix& a,const Matrix& b){Matrix r{};for(int i=0;i<4;i++)for(int j=0;j<4;j++){double s=0;for(int k=0;k<4;k++)s+=a[i*4+k]*b[k*4+j];r[i*4+j]=s;}return r;}
inline bool invert(Matrix m,Matrix& out){
    Matrix r{};for(int i=0;i<4;i++)r[i*4+i]=1;
    for(int c=0;c<4;c++){
        int pivot=c;for(int i=c+1;i<4;i++)if(std::abs(m[i*4+c])>std::abs(m[pivot*4+c]))pivot=i;
        if(std::abs(m[pivot*4+c])<1e-12)return false;
        if(pivot!=c)for(int j=0;j<4;j++){std::swap(m[c*4+j],m[pivot*4+j]);std::swap(r[c*4+j],r[pivot*4+j]);}
        const double inv=1/m[c*4+c];for(int j=0;j<4;j++){m[c*4+j]*=inv;r[c*4+j]*=inv;}
        for(int i=0;i<4;i++)if(i!=c){const double f=m[i*4+c];if(f!=0)for(int j=0;j<4;j++){m[i*4+j]-=f*m[c*4+j];r[i*4+j]-=f*r[c*4+j];}}
    }
    out=r;return true;
}
// Returns the evaluation's own motion vector resource pointer when it was swapped for the layer's (0: left as it was).
inline uintptr_t fixMotion(void* command,uintptr_t p){
    const auto out=read<uintptr_t>(p+8),motion=read<uintptr_t>(p+0x20),depth=read<uintptr_t>(p+0x18);
    if(!out||!motion||!depth)return 0;
    const uint32_t eye=liveFrame.eyeMask==1?0u:1u;
    if(read<uint32_t>(out+0x28)!=eyeWidth(eye)||read<uint32_t>(out+0x2c)!=eyeHeight(eye))return 0;
    const auto& before=alternateEyes::drawn[eye];const auto& now=alternateEyes::current.eyes[eye];
    if(!before.valid||before.session!=liveFrame.sessionGeneration||alternateEyes::currentFrame!=liveFrame.frame||!before.frame||liveFrame.frame<=before.frame)return 0;
    const uint64_t frames=liveFrame.frame-before.frame;if(frames<2||frames>4)return 0;
    const auto projection=rowsOf(game::projection(now.fov,alternateEyes::nearZ,alternateEyes::farZ));
    const Matrix nowClip=multiply(rowsOf(game::cameraRelativeView(now.position,now.orientation,now.position)),projection);
    const Matrix beforeClip=multiply(rowsOf(game::cameraRelativeView(before.position,before.orientation,now.position)),projection);
    Matrix back{};if(!invert(nowClip,back))return 0;
    const Matrix reproject=multiply(back,beforeClip);   // row vectors: this frame's clip -> the last drawing's clip
    std::array<float,16> r{};for(int i=0;i<16;i++){r[i]=float(reproject[i]);if(!std::isfinite(r[i]))return 0;}
    using Fix=int(*)(VkCommandBuffer,uint32_t,VkImageView,VkImageView,uint32_t,uint32_t,const float*,float,float,uint32_t,VkImage*,VkImageView*);
    static const Fix fix=[]{const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");return layer?reinterpret_cast<Fix>(GetProcAddress(layer,"evrFixEyeMotion")):nullptr;}();
    if(!fix)return 0;
    static const float ySign=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_AFW_MOTION_YSIGN",v,sizeof v);return n==1&&v[0]=='1'?1.f:-1.f;}();
    static const uint32_t layoutIn=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_AFW_MOTION_LAYOUT",v,sizeof v);
        return n&&std::string(v,n)=="general"?uint32_t(VK_IMAGE_LAYOUT_GENERAL):uint32_t(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);}();
    VkImage image{};VkImageView view{};
    const int result=fix(reinterpret_cast<VkCommandBuffer>(command),eye,read<VkImageView>(motion),read<VkImageView>(depth),read<uint32_t>(motion+0x28),read<uint32_t>(motion+0x2c),
        r.data(),float(frames),ySign,layoutIn,&image,&view);
    if(result||!image||!view){++motionFailures;return 0;}
    std::memcpy(motionResource.data(),reinterpret_cast<const void*>(motion),motionResource.size());
    std::memcpy(motionResource.data(),&view,sizeof view);std::memcpy(motionResource.data()+8,&image,sizeof image);
    const uint32_t format=VK_FORMAT_R16G16_SFLOAT;std::memcpy(motionResource.data()+0x24,&format,sizeof format);
    *reinterpret_cast<uintptr_t*>(p+0x20)=reinterpret_cast<uintptr_t>(motionResource.data());
    const auto n=++motionFixed;
    if(n<=3||n%3600==0)logFile->event("afw_motion_fix","\"count\":"+std::to_string(n)+",\"eye\":"+std::to_string(eye)+",\"frames\":"+std::to_string(frames)+
        ",\"frame\":"+std::to_string(liveFrame.frame)+",\"motion_format\":"+std::to_string(read<uint32_t>(motion+0x24))+",\"failures\":"+std::to_string(motionFailures.load()));
    return motion;
}
// The water's left/right difference of the main mode (2026-10-24; the right eye's far water reflects a brighter sky; gone after
// a fast travel). Everything checked feeds both eyes' DLSS alike (colour, motion and the reactive mask at the 10-20 spot, jitter,
// no reset, the same pre-exposure and exposure scale: Mock game-20261024-101422), so either the right eye's DLSS history holds
// it or the picture before DLSS differs where no capture looked. EVR_DLSS_RESET_RIGHT=<seconds> (diagnostic, for the headset):
// every so many seconds the right eye's evaluation (its feature handle, rightFeature, view_resource_probe.cpp) is made with
// reset set (+0x38) for that call only - the right eye loses its history for a frame (a visible pop). If the difference goes
// with each pop and grows back, the history carries it; if it stays, the picture before DLSS does.
inline const double resetRightEvery=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_DLSS_RESET_RIGHT",v,sizeof v);const double s=n&&n<sizeof v?std::strtod(v,nullptr):0;return std::isfinite(s)&&s>=0.2&&s<=600?s:0.0;}();
inline std::atomic<uintptr_t> rightFeature{0};inline std::atomic<uint64_t> rightResets{0};inline uint64_t lastRightReset=0;
struct CallbackScope {CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
inline std::string resource(uintptr_t r){
    if(!r)return "null";
    std::ostringstream o;o<<"{\"image\":"<<hex(read<uintptr_t>(r+8))<<",\"w\":"<<read<uint32_t>(r+0x28)<<",\"h\":"<<read<uint32_t>(r+0x2c)
        <<",\"format\":"<<read<uint32_t>(r+0x24)<<",\"type\":"<<read<uint32_t>(r+0x30)<<'}';return o.str();
}
__declspec(noinline) int __fastcall evaluate(void* command,void* handle,void* parameters,void* eval){
    const CallbackScope callback;
    const auto from=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    std::string fields;
    if(accepting&&from==caller&&logged.load()<logLimit){
        const ObserverLastError preserve;
        try{
            const auto p=reinterpret_cast<uintptr_t>(eval),h=reinterpret_cast<uintptr_t>(handle),params=reinterpret_cast<uintptr_t>(parameters);
            std::ostringstream o;o<<std::setprecision(6)<<"\"call\":"<<(calls.load()+1)<<",\"thread\":"<<GetCurrentThreadId()
                <<",\"command\":"<<hex(reinterpret_cast<uintptr_t>(command))<<",\"feature\":"<<hex(h)<<",\"feature_id\":"<<(h?read<uint32_t>(h):0)<<",\"params\":"<<hex(params)
                <<",\"color\":"<<resource(read<uintptr_t>(p))<<",\"output\":"<<resource(read<uintptr_t>(p+8))
                <<",\"depth\":"<<resource(read<uintptr_t>(p+0x18))<<",\"motion\":"<<resource(read<uintptr_t>(p+0x20))
                <<",\"jitter\":["<<read<float>(p+0x28)<<','<<read<float>(p+0x2c)<<"],\"subrect\":["<<read<uint32_t>(p+0x30)<<','<<read<uint32_t>(p+0x34)
                <<"],\"reset\":"<<read<int32_t>(p+0x38)<<",\"mv_scale\":["<<read<float>(p+0x3c)<<','<<read<float>(p+0x40)<<']'
                // 2026-10-24 (the water's left/right difference: each eye's own exposure in the upscaler?): the transparency mask +0x48,
                // the exposure texture +0x50, the bias mask +0x58, the pre-exposure +0x90 and the exposure scale +0x94 (NGX SDK layout)
                <<",\"transparency\":"<<resource(read<uintptr_t>(p+0x48))<<",\"exposure\":"<<resource(read<uintptr_t>(p+0x50))<<",\"bias\":"<<resource(read<uintptr_t>(p+0x58))
                <<",\"pre_exposure\":"<<read<float>(p+0x90)<<",\"exposure_scale\":"<<read<float>(p+0x94);
            fields=o.str();
        }catch(...){++failures;}
    }
    // Register the first two eye-sized DLSS outputs and their color inputs with the layer's
    // diagnostic readback (slots 0/1 output, 2/3 input), once per image. Read-only.
    if(accepting&&from==caller&&registerDiagnostics){
        const ObserverLastError preserve;
        try{
            const auto p=reinterpret_cast<uintptr_t>(eval);const auto out=read<uintptr_t>(p+8),color=read<uintptr_t>(p);
            if(out&&color&&read<uint32_t>(out+0x28)==eyeWidth(0)&&read<uint32_t>(out+0x2c)==eyeHeight(0)){
                const auto image=read<uintptr_t>(out+8);
                std::lock_guard<std::mutex> lock(diagMutex);
                bool known=false;for(auto i:diagOutputs)known|=i==image;
                if(!known&&diagCount<2){
                    const unsigned slot=diagCount++;diagOutputs[slot]=image;
                    const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
                    using Track=int(*)(VkDevice,VkImage,uint32_t);
                    const auto track=layer?reinterpret_cast<Track>(GetProcAddress(layer,"evrTrackDiagnosticImage")):nullptr;
                    const auto device=diagDevice;
                    const int a=track&&device?track(device,reinterpret_cast<VkImage>(image),slot):-1;
                    const int b=track&&device?track(device,reinterpret_cast<VkImage>(read<uintptr_t>(color+8)),slot+2):-1;
                    logFile->event("dlss_diagnostic_images","\"slot\":"+std::to_string(slot)+",\"output\":"+hex(image)+",\"color\":"+hex(read<uintptr_t>(color+8))+",\"output_result\":"+std::to_string(a)+",\"color_result\":"+std::to_string(b));
                }
            }
        }catch(...){++failures;}
    }
    if(from==caller)noteDepth(reinterpret_cast<uintptr_t>(eval));
    ++calls;
    // EVR_AFW_MV_SCALE=<f> (2026-10-21, a test of the alternate-eye warp's "everything shook"): with one eye drawn a frame,
    // an eye's last picture - DLSS's history - is two frames old while the game's motion vectors may span one. For the
    // call only, an eye-sized evaluation's motion vector scale (+0x3c / +0x40) is multiplied by f; put back after it.
    static const float mvScale=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_AFW_MV_SCALE",v,sizeof v);const float f=n&&n<sizeof v?std::strtof(v,nullptr):1.f;return std::isfinite(f)&&f>0&&f<8?f:1.f;}();
    float savedScale[2]{};bool scaled=false;
    if(mvScale!=1&&from==caller&&liveFrame.eyeMask!=3)try{
        const auto p=reinterpret_cast<uintptr_t>(eval);const auto out=read<uintptr_t>(p+8);const auto w=out?read<uint32_t>(out+0x28):0,h=out?read<uint32_t>(out+0x2c):0;
        if((w==eyeWidth(0)&&h==eyeHeight(0))||(w==eyeWidth(1)&&h==eyeHeight(1))){savedScale[0]=read<float>(p+0x3c);savedScale[1]=read<float>(p+0x40);
            *reinterpret_cast<float*>(p+0x3c)=savedScale[0]*mvScale;*reinterpret_cast<float*>(p+0x40)=savedScale[1]*mvScale;scaled=true;}
    }catch(...){scaled=false;}
    // The motion vectors of an eye drawn one frame in two (2026-10-23, layer/eye_motion_fix.hpp): for the call only, the
    // evaluation reads the layer's corrected copy.
    uintptr_t savedMotion=0;
    if(motionFix&&from==caller&&(liveFrame.eyeMask==1||liveFrame.eyeMask==2))try{savedMotion=fixMotion(command,reinterpret_cast<uintptr_t>(eval));}catch(...){savedMotion=0;++motionFailures;}
    int32_t savedReset=0;bool resetSet=false;
    if(resetRightEvery>0&&from==caller&&handle&&reinterpret_cast<uintptr_t>(handle)==rightFeature.load()&&liveFrame.eyeMask==3)try{
        const auto now=GetTickCount64();
        if(!lastRightReset)lastRightReset=now;
        else if(now-lastRightReset>=uint64_t(resetRightEvery*1000)){lastRightReset=now;const auto p=reinterpret_cast<uintptr_t>(eval);savedReset=read<int32_t>(p+0x38);
            *reinterpret_cast<int32_t*>(p+0x38)=1;resetSet=true;const auto n=++rightResets;
            if(n<=3||n%100==0)logFile->event("dlss_right_reset","\"count\":"+std::to_string(n)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"every_s\":"+std::to_string(resetRightEvery)+",\"feature\":"+hex(reinterpret_cast<uintptr_t>(handle)));}
    }catch(...){resetSet=false;}
    const int result=original(command,handle,parameters,eval);
    if(resetSet)*reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(eval)+0x38)=savedReset;
    if(savedMotion)*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(eval)+0x20)=savedMotion;
    if(scaled){const auto p=reinterpret_cast<uintptr_t>(eval);*reinterpret_cast<float*>(p+0x3c)=savedScale[0];*reinterpret_cast<float*>(p+0x40)=savedScale[1];}
    if(!fields.empty()){
        const ObserverLastError preserve;
        try{++logged;logFile->event("dlss_eval",fields+",\"result\":"+hex(uint32_t(result))+",\"read_only\":true");}catch(...){++failures;}
    }
    return result;
}
// NGX parameter setter thunks (NVSDK_NGX_Parameter_SetUI / SetI): params->vtable[+0x20] / [+0x18].
// Observed only for size/quality/subrect names so creation (Width/Height/OutWidth/OutHeight) and
// per-evaluation values can be attributed to the shared parameter object.
using SetValue=void(__fastcall*)(void*,const char*,uint32_t);
inline SetValue originalSetUI=nullptr,originalSetI=nullptr;inline bool setEnabled=false;
inline std::atomic<unsigned> setLogged{0};
constexpr uintptr_t setUIRva=0x12d2b20,setIRva=0x12d2ac0;
constexpr const char* setUIPattern="48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 20 48 8B 01 4C 8B F1 41 8B F8 48 8B F2 48 8B 58 20";
constexpr const char* setIPattern="48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 20 48 8B 01 4C 8B F1 41 8B F8 48 8B F2 48 8B 58 18";
inline bool interesting(const char* name){
    if(!name)return false;
    for(const char* key:{"Width","Height","PerfQuality","Feature.Create","Subrect","Output.Subrects","Reset","Preset"})if(std::strstr(name,key))return true;
    return false;
}
inline void logSet(const char* kind,void* params,const char* name,uint32_t value){
    if(!accepting||setLogged.load()>=3000)return;
    const ObserverLastError preserve;
    try{
        std::string n;for(size_t i=0;i<80&&name[i];++i)n.push_back(name[i]);
        if(!interesting(n.c_str()))return;
        ++setLogged;logFile->event("ngx_param_set","\"kind\":\""+std::string(kind)+"\",\"params\":"+hex(reinterpret_cast<uintptr_t>(params))+",\"name\":"+jsonString(n)+
            ",\"value\":"+std::to_string(value)+",\"caller\":"+hex(reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress)+",\"thread\":"+std::to_string(GetCurrentThreadId()));
    }catch(...){++failures;}
}
__declspec(noinline) void __fastcall setUI(void* params,const char* name,uint32_t value){const CallbackScope callback;logSet("ui",params,name,value);originalSetUI(params,name,value);}
__declspec(noinline) void __fastcall setI(void* params,const char* name,uint32_t value){const CallbackScope callback;logSet("i",params,name,value);originalSetI(params,name,value);}
inline void verify(){
    for(const auto& [r,pat]:{std::pair<uintptr_t,const char*>{setUIRva,setUIPattern},{setIRva,setIPattern}}){
        const auto bytes=parsePattern(pat);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+r),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("NGX parameter setter signature changed");
    }
    const auto bytes=parsePattern(pattern);const auto site=baseAddress+caller-5;
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(site)!=0xe8||uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=baseAddress+rva)
        throw std::runtime_error("DLSS evaluate signature/caller changed");
}
inline void install(){
    verify();
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&evaluate),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("DLSS evaluate observer hook creation failed");
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("DLSS evaluate observer enable failed");
    enabled=true;
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+setUIRva),reinterpret_cast<void*>(&setUI),reinterpret_cast<void**>(&originalSetUI))!=MH_OK||
       MH_CreateHook(reinterpret_cast<void*>(baseAddress+setIRva),reinterpret_cast<void*>(&setI),reinterpret_cast<void**>(&originalSetI))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+setUIRva))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+setIRva))!=MH_OK)
        throw std::runtime_error("NGX parameter setter observer hook failed");
    setEnabled=true;accepting=true;
    logFile->event("dlss_eval_observer_enabled","\"rva\":\"0xfac380\",\"caller\":\"0x100752f\",\"log_limit\":"+std::to_string(logLimit)+",\"read_only\":true");
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("DLSS evaluate observer disable failed");
    enabled=false;
    if(setEnabled){
        if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+setUIRva))!=MH_OK||MH_DisableHook(reinterpret_cast<void*>(baseAddress+setIRva))!=MH_OK)
            throw std::runtime_error("NGX parameter setter observer disable failed");
        setEnabled=false;
    }
    const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    verify();
    logFile->event("dlss_eval_observer_removed","\"restored\":true,\"callbacks\":"+std::to_string(callbacks.load())+
        ",\"calls\":"+std::to_string(calls.load())+",\"logged\":"+std::to_string(logged.load())+",\"param_sets_logged\":"+std::to_string(setLogged.load())+",\"failures\":"+std::to_string(failures.load()));
    if(callbacks.load())throw std::runtime_error("DLSS evaluate callbacks still active");
}
}
