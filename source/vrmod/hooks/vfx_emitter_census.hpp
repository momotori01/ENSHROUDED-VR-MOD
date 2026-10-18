// Diagnostic, read-only: which effect emitters appear in the Scene and when (the pool of held_vfx_observer.hpp,
// Scene+0x908). For finding where an effect comes from (the white arc of the one-hand sword). Enabled only by
// EVR_VFX_CENSUS=1 in the game's environment; no launch flag sets it.
namespace vfxEmitterCensus {
inline int state=-1;inline bool primed=false;inline unsigned logged=0;inline std::unordered_map<uint32_t,uint32_t> known;
inline void observe(uintptr_t scene,const game::StableAnchor& anchor){
    if(state<0){char value[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_CENSUS",value,sizeof value);state=n&&n<sizeof value&&value[0]=='1';
        if(state)logFile->event("vfx_emitter_census_enabled","\"diagnostic\":true,\"read_only\":true");}
    if(state!=1||!scene)return;
    try{
        const auto vfx=read<uintptr_t>(scene+layout::sceneVfx);if(!vfx)return;   // 1.0: the pool fields below are not re-checked yet (bounds-checked)
        const auto shift=read<uint64_t>(vfx+0x68),mask=read<uint64_t>(vfx+0x78);const auto pages=read<uintptr_t>(vfx+0x40),count=read<uint64_t>(vfx+0x48);
        if(shift>16||mask>0xffff||count>65536||!pages)throw std::runtime_error("emitter pool bounds");
        unsigned live=0;
        for(uint64_t page=0;page<count;++page){
            const auto block=pages+page*64,n=read<uint64_t>(block+0x10);const auto data=read<uintptr_t>(block+8);
            if(n>65536)throw std::runtime_error("emitter page bounds");if(!data)continue;
            for(uint64_t i=0;i<n;++i){
                const auto entry=data+i*0x118;const auto generation=read<uint32_t>(entry);const auto index=uint32_t((page<<shift)|i);
                const auto found=known.find(index);
                if(generation&0x80000000u){
                    if(found!=known.end()){if(primed&&logged<6000){++logged;logFile->event("vfx_emitter_gone","\"frame\":"+std::to_string(liveFrame.frame)+",\"index\":"+std::to_string(index)+",\"generation\":"+std::to_string(found->second));}known.erase(found);}
                    continue;}
                ++live;if(found!=known.end()&&found->second==generation)continue;
                known[index]=generation;if(!primed||logged>=6000)continue;++logged;
                const auto emitter=entry+8;const auto resource=read<uintptr_t>(emitter+0xf0);const auto position=read<WorldPositionQ32>(emitter+0x40);
                std::ostringstream s;s<<std::setprecision(6)<<"\"frame\":"<<liveFrame.frame<<",\"index\":"<<index<<",\"generation\":"<<generation<<",\"resource\":"<<hex(resource)
                    <<",\"from_root_m\":["<<double(position.x-anchor.root.x)/4294967296.0<<','<<double(position.y-anchor.root.y)/4294967296.0<<','<<double(position.z-anchor.root.z)/4294967296.0
                    <<"],\"emitter_bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,0x110>>(emitter)));
                if(resource)s<<",\"resource_bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,96>>(resource)));
                logFile->event("vfx_emitter_new",s.str());
            }
        }
        if(!primed){primed=true;logFile->event("vfx_emitter_census_primed","\"frame\":"+std::to_string(liveFrame.frame)+",\"live\":"+std::to_string(live));}
    }catch(const std::exception& e){state=0;logFile->event("vfx_emitter_census_failure","\"reason\":"+jsonString(e.what()));}
}
}
