#pragma once
// Diagnostic (2026-11-11, the stick-turn motion vector bursts - docs/analysis/motion_blur_20261111.md). The View update
// (0x12d7e9..) builds two matrices from the previous camera (View+0xfc0, +0x1000.., all checked right in a burst) and
// this one: A (rbp+0x57a0) = the previous camera's rotation-only view x its projection, B (rbp+0x5860) = this frame's
// inverse projection x this camera's world matrix placed at its offset from the previous camera; 0x124e60 (called at
// 0x137eed with the camera record +0x38 as its 4th argument, A 5th, B 6th) packs them for the GPU. EVR_LOG_MOTION_MATRICES=
// <first>-<last> (XR frames) logs A, B and the camera of every call (motion_matrices) - read only, the game's values pass.
namespace motionMatrixWatch {
using Pack=uintptr_t(__fastcall*)(void*,void*,void*,const uint8_t*,const float*,const float*);
inline Pack original=nullptr;inline bool enabled=false;inline std::atomic<unsigned> logged{0};
// The eyes take the other consumer (0x137eb0's test skips 0x124e60 for them): 0xe2310, called at 0x137f46 (history valid)
// with View+0x5b0 2nd, A 6th and B 7th.
using Second=uintptr_t(__fastcall*)(void*,void*,void*,void*,void*,const float*,const float*);
inline Second originalSecond=nullptr;
inline const std::pair<uint64_t,uint64_t> range=[]{char v[48]{};const auto n=GetEnvironmentVariableA("EVR_LOG_MOTION_MATRICES",v,sizeof v);
    uint64_t a=0,b=0;if(n&&n<sizeof v&&std::sscanf(v,"%llu-%llu",&a,&b)==2&&a<=b)return std::pair<uint64_t,uint64_t>{a,b};return std::pair<uint64_t,uint64_t>{0,0};}();
inline bool wanted(){return range.second!=0;}
inline uintptr_t __fastcall pack(void* graph,void* view,void* third,const uint8_t* camera,const float* a,const float* b){
    if(liveFrame.frame>=range.first&&liveFrame.frame<=range.second&&logged.fetch_add(1)<12000)try{
        std::ostringstream o;o<<std::setprecision(9)<<"\"frame\":"<<liveFrame.frame<<",\"view\":"<<hex(reinterpret_cast<uintptr_t>(view));
        o<<",\"camera_q32\":["<<read<int64_t>(reinterpret_cast<uintptr_t>(camera))<<','<<read<int64_t>(reinterpret_cast<uintptr_t>(camera)+8)<<','<<read<int64_t>(reinterpret_cast<uintptr_t>(camera)+16)<<']';
        o<<",\"camera_q\":["<<read<float>(reinterpret_cast<uintptr_t>(camera)+0x18)<<','<<read<float>(reinterpret_cast<uintptr_t>(camera)+0x1c)<<','<<read<float>(reinterpret_cast<uintptr_t>(camera)+0x20)<<','<<read<float>(reinterpret_cast<uintptr_t>(camera)+0x24)<<']';
        o<<",\"a\":[";for(int i=0;i<16;i++)o<<(i?",":"")<<a[i];o<<"],\"b\":[";for(int i=0;i<16;i++)o<<(i?",":"")<<b[i];o<<']';
        logFile->event("motion_matrices",o.str());
    }catch(...){}
    return original(graph,view,third,camera,a,b);
}
inline uintptr_t __fastcall second(void* graph,void* viewPart,void* third,void* fourth,void* fifth,const float* a,const float* b){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    if(liveFrame.frame>=range.first&&liveFrame.frame<=range.second&&logged.fetch_add(1)<12000)try{
        std::ostringstream o;o<<std::setprecision(9)<<"\"frame\":"<<liveFrame.frame<<",\"caller\":"<<hex(caller)<<",\"view\":"<<hex(reinterpret_cast<uintptr_t>(viewPart)-0x5b0);
        o<<",\"a\":[";for(int i=0;i<16;i++)o<<(i?",":"")<<a[i];o<<"],\"b\":[";for(int i=0;i<16;i++)o<<(i?",":"")<<b[i];o<<']';
        logFile->event("motion_matrices",o.str());
    }catch(...){}
    return originalSecond(graph,viewPart,third,fourth,fifth,a,b);
}
inline void install(){
    if(!wanted())return;
    static const std::vector<uint8_t> pattern{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    if(std::memcmp(reinterpret_cast<const void*>(baseAddress+0x124e60),pattern.data(),pattern.size()))throw std::runtime_error("motion pack signature differs");
    const auto* call=reinterpret_cast<const uint8_t*>(baseAddress+0x137eed);int32_t delta=0;std::memcpy(&delta,call+1,4);
    if(*call!=0xe8||reinterpret_cast<uintptr_t>(call+5)+delta!=baseAddress+0x124e60)throw std::runtime_error("motion pack callsite differs");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+0x124e60),reinterpret_cast<void*>(&pack),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+0x124e60))!=MH_OK)throw std::runtime_error("motion pack hook failed");
    static const std::vector<uint8_t> secondPattern{0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    if(std::memcmp(reinterpret_cast<const void*>(baseAddress+0xe2310),secondPattern.data(),secondPattern.size()))throw std::runtime_error("motion second signature differs");
    const auto* call2=reinterpret_cast<const uint8_t*>(baseAddress+0x137f46);std::memcpy(&delta,call2+1,4);
    if(*call2!=0xe8||reinterpret_cast<uintptr_t>(call2+5)+delta!=baseAddress+0xe2310)throw std::runtime_error("motion second callsite differs");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+0xe2310),reinterpret_cast<void*>(&motionMatrixWatch::second),reinterpret_cast<void**>(&originalSecond))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+0xe2310))!=MH_OK)throw std::runtime_error("motion second hook failed");
    enabled=true;logFile->event("motion_matrix_watch_enabled","\"first\":"+std::to_string(range.first)+",\"last\":"+std::to_string(range.second));
}
} // namespace motionMatrixWatch
