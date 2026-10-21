#pragma once
// Status vignettes in VR (2026-10-06, user decisions: keep them but never as a frame floating in the HUD; then
// "vignettes are unpleasant in VR, a slightly red or blue rim is enough").
// Game paths (game130721/game133508):
//  - Low health: post pass of every View (post_upscale shader 20714976): edge-distance mask * render parameter
//    +268..+277 (rgb, coverage w); +278..+287 is an additive full-screen colour (flash) when its w > 0.
//  - Freezing: HUD widget aa9270 ("freezing vignette": frost borderLayer + darkening glassLayer) drawn into the HUD
//    pass, i.e. onto the HUD quad in VR. It reads the amount at UI state (*(273f588)+c8) +50980+214 when +50cd8 is
//    non-zero, returns early when |amount| <= eps, and draws only while |+508b8 * +508b0| > eps: smoothed strengths
//    that rise only while the player status flag ([[state]+28]+150 & 0x100000) is set, with opacity +508ac.
//  - Blur: HUD widget a45e00 ("Blur Vignette", strength in xmm0), called from a2169d.
// VR tint mode (world-edge-tint.txt, default for live world series): both HUD widgets are not drawn; the probe turns
// the game's in-eye vignette/flash off and the layer draws a faint rim on the outer side of each eye instead
// (layer/native_xr.hpp, include/evr/edge_tint.hpp). The freezing rim follows the game's freezing flag.
namespace statusVignette {
using Fn=void(__fastcall*)();
using BlurFn=void(__fastcall*)(float);
inline Fn freezingOriginal=nullptr;inline BlurFn blurOriginal=nullptr;
inline bool freezingEnabled=false,blurEnabled=false;
inline bool vrTint=false;           // world-edge-tint.txt
inline float forceFreezing=-1;      // world-force-freezing.txt (diagnostic); <0 = game value
inline std::atomic<uint64_t> freezingCalls{0},blurCalls{0},blurSuppressed{0};
inline std::atomic<float> frost{0};  // smoothed freezing rim strength 0..1 (UI thread writes, render thread reads)
inline std::chrono::steady_clock::time_point lastFreezingCall{};
constexpr uintptr_t freezingRva=0xbbf330,hudCallerRva=0xc50a52,uiGlobalRva=0x281e498;
constexpr uintptr_t blurRva=0xb52df0,blurCallerRva=0xb32898;
constexpr const char* freezingPattern="48 8B C4 55 53 56 41 54 48 8D A8 38 FF FF FF 48 81 EC A8 01 00 00 48 8B 1D";
constexpr const char* blurPattern="48 8B C4 48 89 58 20 55 48 8D 68 88 48 81 EC 70 01 00 00";
inline uintptr_t freezingBlock(){
    const auto ui=read<uintptr_t>(baseAddress+uiGlobalRva);if(!ui)return 0;
    const auto state=read<uintptr_t>(ui+0xc8);if(!state)return 0;
    if(!read<uint64_t>(state+0x50cd8))return 0; // aa9296: cmp [r14+50cd8],0 ; cmovbe -> no block
    return state+0x50980;
}
void __fastcall freezing(){
    const auto block=freezingBlock();
    float amount=0;uint32_t flags=0;
    if(block){
        const auto state=block-0x50980;
        if(forceFreezing>=0){
            *reinterpret_cast<float*>(block+0x214)=forceFreezing;
            // +508ac: widget opacity (aa9641, aa96f0), +508b0/+508b4/+508b8: smoothed strengths (aa94d8..aa963b).
            if(!vrTint)for(const uintptr_t o:{0x508acu,0x508b0u,0x508b4u,0x508b8u})*reinterpret_cast<float*>(state+o)=forceFreezing;
        }
        amount=read<float>(block+0x214);
        if(const auto owner=read<uintptr_t>(state))if(const auto status=read<uintptr_t>(owner+0x28))flags=read<uint32_t>(status+0x150);
    }
    const bool freezingFlag=(flags&0x100000)!=0;
    if(vrTint){
        // Rim target: the game's freezing flag (or the forced diagnostic value); about 0.7 s to fade in/out.
        const float target=forceFreezing>=0?std::min(forceFreezing,1.0f):(block&&freezingFlag?1.0f:0.0f);
        const auto now=std::chrono::steady_clock::now();
        float dt=lastFreezingCall.time_since_epoch().count()?std::chrono::duration<float>(now-lastFreezingCall).count():0.0f;lastFreezingCall=now;
        dt=std::clamp(dt,0.0f,0.25f);
        auto f=frost.load();f+=(target-f)*std::min(1.0f,dt*1.5f);frost=f;
    }
    const auto n=++freezingCalls;
    if(n<=3||n%600==0)try{
        std::ostringstream o;o<<std::setprecision(4)<<"\"calls\":"<<n<<",\"block\":"<<(block?"true":"false")<<",\"amount\":"<<amount
            <<",\"forced\":"<<(forceFreezing>=0?"true":"false")<<",\"freezing_flag\":"<<(freezingFlag?"true":"false")
            <<",\"vr_tint\":"<<(vrTint?"true":"false")<<",\"rim\":"<<frost.load();
        logFile->event("status_vignette_freezing",o.str());
    }catch(...){}
    if(!vrTint)freezingOriginal();  // VR tint mode: the frost widget would float on the HUD quad
}
void __fastcall blur(float strength){
    const auto n=++blurCalls;
    if(vrTint)++blurSuppressed;
    if(n<=3||n%600==0)try{
        std::ostringstream o;o<<std::setprecision(4)<<"\"calls\":"<<n<<",\"strength\":"<<strength<<",\"suppressed\":"<<blurSuppressed.load();
        logFile->event("status_vignette_blur",o.str());
    }catch(...){}
    if(!vrTint)blurOriginal(strength);
}
inline void hookAt(uintptr_t rva,uintptr_t callerRva,const char* pattern,void* detour,void** original,bool checkGlobal,const char* name){
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva,call=baseAddress+callerRva;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=target||
       (checkGlobal&&uintptr_t(int64_t(target+0x1d)+read<int32_t>(target+0x19))!=baseAddress+uiGlobalRva))
        throw std::runtime_error(std::string(name)+" vignette signature/caller mismatch");
    if(MH_CreateHook(reinterpret_cast<void*>(target),detour,original)!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)
        throw std::runtime_error(std::string(name)+" vignette hook failed");
}
inline void install(){
    hookAt(freezingRva,hudCallerRva,freezingPattern,reinterpret_cast<void*>(&freezing),reinterpret_cast<void**>(&freezingOriginal),true,"freezing");
    freezingEnabled=true;
    if(vrTint){hookAt(blurRva,blurCallerRva,blurPattern,reinterpret_cast<void*>(&blur),reinterpret_cast<void**>(&blurOriginal),false,"blur");blurEnabled=true;}
    std::ostringstream f;f<<std::setprecision(4)<<"\"freezing_rva\":"<<hex(freezingRva)<<",\"blur_rva\":"<<hex(blurRva)<<",\"blur_hooked\":"<<(blurEnabled?"true":"false")
        <<",\"vr_tint\":"<<(vrTint?"true":"false")<<",\"force\":"<<forceFreezing;
    logFile->event("status_vignette_hook_enabled",f.str());
}
inline void uninstall() noexcept {
    MH_STATUS freezingStatus=MH_OK,blurStatus=MH_OK;
    if(freezingEnabled){freezingStatus=MH_DisableHook(reinterpret_cast<void*>(baseAddress+freezingRva));if(freezingStatus==MH_OK){MH_RemoveHook(reinterpret_cast<void*>(baseAddress+freezingRva));freezingEnabled=false;}}
    if(blurEnabled){blurStatus=MH_DisableHook(reinterpret_cast<void*>(baseAddress+blurRva));if(blurStatus==MH_OK){MH_RemoveHook(reinterpret_cast<void*>(baseAddress+blurRva));blurEnabled=false;}}
    frost=0;
    try{logFile->event("status_vignette_hook_removed","\"freezing_status\":"+std::to_string(freezingStatus)+",\"blur_status\":"+std::to_string(blurStatus)+
        ",\"freezing_calls\":"+std::to_string(freezingCalls.load())+",\"blur_calls\":"+std::to_string(blurCalls.load())+",\"blur_suppressed\":"+std::to_string(blurSuppressed.load()));}catch(...){}
}
}
