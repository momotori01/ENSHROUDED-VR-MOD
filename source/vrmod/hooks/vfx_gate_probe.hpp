// Diagnostic (2026-10-16 evening): four more "does this View have a VfxView" branches of the render function, none of them
// taken by a bridge yet. The eye without the VFX skipped the particles' composite after the third particle pass (RenderDoc
// game-20261016-193923: the owner's eye runs a 131x131 dispatch 7727 that reads the particle targets 14482 / 14484, the
// other eye goes on with 33x33 at 11870) - the flame of the held staff in one eye only, although its particles are drawn
// (11855 paints it). This counts, per eye, how often each branch is reached and closed (ZF set: the View has no VfxView),
// and EVR_VFX_OPEN_GATES=<mask> opens the chosen ones for the eye that borrows the owner's effects (the saved ZF cleared,
// as the other bridges do). By default the one after the particle passes is opened (openMask below).
namespace vfxGateProbe {
struct Site {uintptr_t rva;int viewSlot;const char* pattern;const char* name;};
// viewSlot: the saved register holding the View, as an offset below the stub's original rsp (vfx_gate_stub.asm):
// rbx -40, rdi -64, r13 -112.
inline const std::array<Site,4> sites{{
    {0x12a363,-64,"74 13 4C 8D 85 58 01 00 00","rdx_vfx_12a223"},
    {0x12a62f,-64,"74 4C 44 8B 0D","cmp_rdi_12a4ef"},
    {0x13557e,-40,"0F 84 1D 01 00 00 8B 83 D0 00 00 00","fog_injection_13543e"},
    {0x13a5cb,-40,"0F 84 C8 08 00 00 48 8B 44 24 58","after_particles_13a48b"}}};
// Default 12 = after_particles_13a48b (8; Mock game-20261016-194804: opened for the eye without the VFX, the flame of the
// held staff and the glowing motes in both eyes; no failure) and the fog's VFX injection 13543e (4; 2026-10-17, headset
// reports 11 and 22: "the mist of the valley in the left eye only" - the gate was closed for the right eye every time,
// Mock: right eye 1149 of 1149). Its block only builds the fog stage's bindings from the View's +0xb60 / +0xb64 / +0xb78
// (no VfxView read), which for the other eye are the owner's lent counters (shared_point_lights.hpp). Opened in five Mock
// runs (game-20261017-034249, -034533, -040118, -040820, -042900): no failure, no visible fault; the mist's own effect is
// for the headset to show (no mist where the Mock stands). The other two are counted only. EVR_VFX_OPEN_GATES=8: as before.
inline const unsigned openMask=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_VFX_OPEN_GATES",v,sizeof v);
    return n&&n<sizeof v?unsigned(std::strtoul(v,nullptr,0)):12u;}();
inline std::array<bool,4> enabled{};inline bool installed=false;
inline std::array<std::array<std::atomic<unsigned>,3>,4> closed{},open{},opened{};   // [site][eye 0 / eye 1 / other View]
inline std::atomic<unsigned> lines{0};
inline void onGate(uintptr_t rsp,unsigned site) noexcept {
    if(site>=sites.size())return;const ObserverLastError preserve;
    try{
        const auto view=read<uintptr_t>(rsp+sites[site].viewSlot);const auto flags=read<uint64_t>(rsp-8);
        auto* c=sharedPointLights::current;int eye=2;
        if(c&&c->pair&&c->eye>=0&&c->eye<=1&&c->pair->created[size_t(c->eye)]==view)eye=c->eye;
        const bool zf=(flags&0x40)!=0;
        if(!zf){++open[site][size_t(eye)];return;}
        ++closed[site][size_t(eye)];
        bool take=false;
        if((openMask&(1u<<site))&&eye<=1&&eye!=worldSharedStages::vfxOwnerEye&&!read<uintptr_t>(view+layout::viewVfx)){
            const auto changed=flags&~uint64_t(0x40);std::memcpy(reinterpret_cast<void*>(rsp-8),&changed,8);++opened[site][size_t(eye)];take=true;}
        if(lines.fetch_add(1)<48)logFile->event("vfx_gate_probe_hit","\"site\":\""+std::string(sites[site].name)+"\",\"eye\":"+std::to_string(eye)+
            ",\"owner_eye\":"+std::to_string(worldSharedStages::vfxOwnerEye)+",\"serial\":"+std::to_string(c&&c->pair?c->pair->serial:0)+
            ",\"closed\":true,\"opened\":"+(take?"true":"false"));
    }catch(...){}
}
inline void summary(const char* when) noexcept {
    try{std::string s;
        for(size_t i=0;i<sites.size();++i){s+=std::string(i?",":"")+"{\"site\":\""+sites[i].name+"\",\"enabled\":"+(enabled[i]?"true":"false")+",\"closed\":[";
            for(size_t e=0;e<3;++e)s+=(e?",":"")+std::to_string(closed[i][e].load());
            s+="],\"open\":[";for(size_t e=0;e<3;++e)s+=(e?",":"")+std::to_string(open[i][e].load());
            s+="],\"opened\":[";for(size_t e=0;e<3;++e)s+=(e?",":"")+std::to_string(opened[i][e].load());s+="]}";}
        logFile->event("vfx_gate_probe_summary","\"when\":\""+std::string(when)+"\",\"open_mask\":"+std::to_string(openMask)+",\"sites\":["+s+"],\"eyes\":\"[eye 0, eye 1, other View]\"");
    }catch(...){}
}
inline void install(void* const* stubs,void** trampolines){
    for(size_t i=0;i<sites.size();++i){const auto& s=sites[i];const auto bytes=parsePattern(s.pattern);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+s.rva),bytes.size(),bytes)!=std::vector<size_t>{0}){
            logFile->event("vfx_gate_probe_skipped","\"site\":\""+std::string(s.name)+"\",\"reason\":\"signature\"");continue;}
        if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+s.rva),stubs[i],&trampolines[i])!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(baseAddress+s.rva))!=MH_OK){logFile->event("vfx_gate_probe_skipped","\"site\":\""+std::string(s.name)+"\",\"reason\":\"hook\"");continue;}
        enabled[i]=true;}
    installed=true;summary("installed");
}
inline void disable() noexcept {
    if(!installed)return;summary("removed");
    for(size_t i=0;i<sites.size();++i)if(enabled[i]){MH_DisableHook(reinterpret_cast<void*>(baseAddress+sites[i].rva));MH_RemoveHook(reinterpret_cast<void*>(baseAddress+sites[i].rva));enabled[i]=false;}
    installed=false;
}
}
