// Read-only native culling constants at the already validated private upload boundary.
namespace sceneryCulling {
inline bool requested=false,noHzb=false;
inline std::array<uint64_t,2> bypassed{};
inline std::array<uint64_t,2> samples{};
inline void disableHzb(uintptr_t address,PairResources& pair,unsigned eye,std::array<uint8_t,4144>& before){
    if(!noHzb)return;
    if(eye>1||GetCurrentThreadId()!=pair.renderThread)throw std::runtime_error("scenery HZB diagnostic scope differs");
    const auto base=reinterpret_cast<uintptr_t>(before.data());
    const auto subviews=read<uint32_t>(base+4064),passes=read<uint32_t>(base+4068);
    if(!subviews||subviews>11||!passes||passes>12)throw std::runtime_error("scenery HZB subview bounds differ");
    unsigned mainPasses=0;
    for(unsigned i=0;i<passes;++i)if(read<uint32_t>(base+3872+i*16+8)==0){
        if(read<uint32_t>(base+3872+i*16+4)!=0)throw std::runtime_error("scenery HZB main subview differs");++mainPasses;}
    if(mainPasses!=1||read<uint32_t>(base+336)!=0)throw std::runtime_error("scenery HZB expected one perspective main subview");
    const auto d=pair.created[eye]+0x6a0;
    if(read<uint32_t>(d)!=3)throw std::runtime_error("scenery HZB requires the native asymmetric descriptor");
    const float l=read<float>(d+4),r=read<float>(d+8),b=read<float>(d+12),t=read<float>(d+16);
    if(!(l<r&&b<t))throw std::runtime_error("scenery HZB descriptor bounds differ");
    // Explicit diagnostic also covers the centered compatibility FOV. Owner/thread,
    // main-pass identity and exact byte-range guards above remain unchanged.
    const bool centered=std::abs((r+l)/(r-l))<1e-5f&&std::abs((t+b)/(t-b))<1e-5f;
    const auto flags=read<uint32_t>(base);if(!(flags&16u))return;
    const uint32_t replacement=flags&~16u;
    std::memcpy(before.data(),&replacement,4);std::memcpy(reinterpret_cast<void*>(address),&replacement,4);
    if(read<std::array<uint8_t,4144>>(address)!=before)throw std::runtime_error("scenery HZB changed bytes outside the main flag");
    ++bypassed[eye];
    if(bypassed[eye]<=3||liveFrame.frame%60==0)logFile->event("scenery_hzb_bypassed","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(pair.serial)+
        ",\"eye\":"+std::to_string(eye)+",\"native_flags\":"+std::to_string(flags)+",\"flags\":"+std::to_string(replacement)+
        ",\"applications\":"+std::to_string(bypassed[eye])+",\"projection_centered\":"+(centered?"true":"false")+",\"main_subview\":0,\"other_bytes_unchanged\":true,\"diagnostic_only\":true");
}
inline void observe(uintptr_t address,PairResources& pair,unsigned eye){
    if(!requested)return;
    if(eye>1||GetCurrentThreadId()!=pair.renderThread)throw std::runtime_error("scenery culling observer scope differs");
    ++samples[eye];
    if(samples[eye]>3&&liveFrame.frame%60!=0)return;
    const auto bytes=read<std::array<uint8_t,4144>>(address);
    const auto descriptor=read<std::array<uint8_t,80>>(pair.created[eye]+0x6a0);
    auto hexArray=[](const auto& data){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto b:data)s<<std::setw(2)<<unsigned(b);return s.str();};
    std::ostringstream f;f<<"\"frame\":"<<liveFrame.frame<<",\"serial\":"<<pair.serial<<",\"eye\":"<<eye
        <<",\"sample\":"<<samples[eye]<<",\"constants\":"<<jsonString(hexArray(bytes))<<",\"descriptor\":"<<jsonString(hexArray(descriptor))
        <<",\"read_only\":true";
    logFile->event("scenery_culling_constants",f.str());
}
}
