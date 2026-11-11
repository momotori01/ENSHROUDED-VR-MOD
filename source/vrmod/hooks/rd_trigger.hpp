#pragma once
// Test only (2026-11-11, the stick-turn motion vector bursts - docs/analysis/motion_blur_20261111.md): RenderDoc captures of
// a player-style start with RenderDoc's layer under the mod's (loader [Environment] EVR_TEST_RENDERDOC). Each capture is the
// game frame after the one asked in (RenderDoc's TriggerCapture: present to present), written to <probe folder>/rd/.
// EVR_RENDERDOC_CAPTURE_FRAME=<XR frame>: one capture there (a turn without the fault, to compare with).
// EVR_RENDERDOC_ON_BAD_MOTION=<n>: up to n captures (at least 4 frames apart) when the layer's motion check finds the game's
// motion off from the eyes' cameras (eye_motion_fix.hpp evrMotionBadChecks; with EVR_MOTION_CHECK_BOTH and
// EVR_AFW_MOTION_CHECK) - the fault is then a few frames old and lasts ~40 frames.
namespace rdTrigger {
inline RENDERDOC_API_1_6_0* api=nullptr;inline bool looked=false;
inline const uint64_t atFrame=[]{char v[24]{};return GetEnvironmentVariableA("EVR_RENDERDOC_CAPTURE_FRAME",v,sizeof v)?std::strtoull(v,nullptr,10):0ull;}();
inline const unsigned onBad=[]{char v[8]{};return GetEnvironmentVariableA("EVR_RENDERDOC_ON_BAD_MOTION",v,sizeof v)?unsigned(std::strtoul(v,nullptr,10)):0u;}();
inline bool frameDone=false;inline unsigned badCaptures=0;inline uint64_t lastBadSeen=0,lastCaptureFrame=0;
// EVR_RENDERDOC_AT_HEADING=<deg>[/<deg>...]: one capture each time eye 0's heading (yaw of its orientation, the motion
// check's metric, -180..180) first comes within 1.5 degrees of the value after XR frame 2500 (the turn of the Mock script).
inline std::vector<double> headings=[]{std::vector<double> out;char v[64]{};const auto n=GetEnvironmentVariableA("EVR_RENDERDOC_AT_HEADING",v,sizeof v);
    if(!n||n>=sizeof v)return out;std::string t(v);size_t at=0;while(at<t.size()){auto e=t.find_first_of("/,",at);if(e==std::string::npos)e=t.size();out.push_back(std::atof(t.substr(at,e-at).c_str()));at=e+1;}return out;}();
inline void trigger(uint64_t frame,const char* why){
    api->TriggerCapture();lastCaptureFrame=frame;
    logFile->event("rd_capture_triggered","\"frame\":"+std::to_string(frame)+",\"why\":"+jsonString(why)+",\"captures\":"+std::to_string(api->GetNumCaptures()));
}
inline void tick(uint64_t frame,const std::filesystem::path& folder,const XrQuaternionf& eye0){
    if(!atFrame&&!onBad&&headings.empty())return;
    if(!looked){looked=true;
        const auto module=GetModuleHandleW(L"renderdoc.dll");const auto get=module?reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module,"RENDERDOC_GetAPI")):nullptr;
        if(!get||!get(eRENDERDOC_API_Version_1_6_0,reinterpret_cast<void**>(&api))){api=nullptr;logFile->event("rd_capture_unavailable","\"module\":"+std::string(module?"true":"false"));return;}
        std::error_code ec;std::filesystem::create_directories(folder/"rd",ec);
        api->SetCaptureFilePathTemplate((folder/"rd"/"eyes").string().c_str());
        logFile->event("rd_capture_ready","\"frame\":"+std::to_string(frame));}
    if(!api||api->IsFrameCapturing())return;
    if(atFrame&&!frameDone&&frame>=atFrame){frameDone=true;trigger(frame,"frame");return;}
    if(!headings.empty()&&frame>2500){
        const double yaw=std::atan2(2.0*(eye0.w*eye0.y+eye0.x*eye0.z),1.0-2.0*(eye0.y*eye0.y+eye0.x*eye0.x))*57.29577951308232;
        for(auto it=headings.begin();it!=headings.end();++it){double d=yaw-*it;while(d>180)d-=360;while(d<-180)d+=360;
            if(std::abs(d)<1.5&&frame>=lastCaptureFrame+4){trigger(frame,("heading "+std::to_string(yaw)).c_str());headings.erase(it);return;}}
    }
    if(onBad&&badCaptures<onBad){
        using Count=uint64_t(*)();
        static const Count count=[]{const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");return layer?reinterpret_cast<Count>(GetProcAddress(layer,"evrMotionBadChecks")):nullptr;}();
        if(!count)return;
        const auto bad=count();
        if(bad>lastBadSeen&&frame>=lastCaptureFrame+4){lastBadSeen=bad;++badCaptures;trigger(frame,"bad_motion");}
    }
}
} // namespace rdTrigger
