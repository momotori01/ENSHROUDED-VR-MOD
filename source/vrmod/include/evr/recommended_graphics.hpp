#pragma once
// The recommended graphics for VR in one press (2026-10-20, user: "a button that makes the recommended settings at once,
// in the game and in the exe - 'change to the recommended graphics settings'; if possible read the PC and change the
// recommendation, e.g. lower the textures with only 10 GB of VRAM"; and "the detail scattering goes straight to the
// looks - Medium even if it costs a little"; then "distant objects, deco objects, tessellation, voxels act heavily on the
// looks - not to be cut; take this PC's settings for those. Decide yourself the ones hard to tell apart at a glance -
// SSAO, anisotropic filtering, DLSS"). So what is seen stays at the user's own choice of 2026-10-20 (local_settings.json:
// shadows Low, reflections and water Balance, fog Medium, distant / deco objects Medium, detail scattering Medium,
// voxels Low, tessellation Medium, point light shadows Low), and the mod decides SSAO and contact shadows (off: screen
// space, drawn by each eye on its own, a left / right difference besides the cost), the texture filtering (up: little
// cost, a sharper ground at a slant), DLSS / FSR 3 (Quality: no cost measured between the modes), vertical sync and HDR
// (off: no meaning in VR).
// What is measured (docs/STATUS.md 10-20 18:30, docs/analysis/performance_20261014/findings.md): the GPU is the limit
// (85..98 %); a frame is a shared part of about 6 ms and about 15 ms for each eye (pond capture, 36.8 ms both eyes in
// replay), spread over thousands of small commands; the eyes' picture size moves it most (0.8 -> 0.6: about 5 ms less);
// the DLSS mode does not (Quality, Balanced, Performance alike - the work hardly depends on the resolution, so Quality);
// GI Performance + fog Low + contact shadows off about 1 ms; distant objects Low + deco objects off + tessellation off
// about 0.4 ms; raising contact shadows, detail scattering, distant objects, GI and fog together cost about a fifth of
// the pairs on the headset (2026-10-13); the shadow map is shared by the eyes (0.16 ms a pair); vertical sync made no
// difference in VR (the headset's timing is the mod's). The game 1.0 has no texture quality setting any more (its
// local_settings.json: textureAnisotropy and models instead; the values below are the executable's own enum names,
// read from its reflection 2026-10-20): less video memory is met with a smaller eye picture.
// Game settings written for good (the player may change them again in the game): render.<name> and the two flags.
// DLSS (antiAliasing) and GI (dynamicGi) are written here into the game's settings (2026-10-25, user: "the recommended
// button is there - everything but HDR the player switches themselves": a VR start keeps the player's own; before, it set
// DLSS and GI High at each start). A card that is not NVIDIA's gets FSR 3 instead (2026-10-25, user: "and FSR 3"), with
// the fsr3 preset written below like the DLSS mode; a card not known gets DLSS (the game itself falls back to FSR 3 on a
// card without DLSS). The DLSS mode (dlss, and fsr3 for
// cards without DLSS) is written here into the game's settings (2026-10-23: the VR start keeps the game's own mode now).
#include <evr/auto_start.hpp>
#include <evr/vr_settings.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
namespace evr::recommend {
// The graphics card the game draws with (the one with the most dedicated video memory; gpu_info.hpp reads it).
struct Machine {std::string gpu;uint32_t vendor=0;uint64_t videoMemory=0;bool known=false;};
constexpr uint32_t nvidia=0x10de,amd=0x1002,intel=0x8086;
struct Recommendation {
    std::vector<std::pair<std::string,std::string>> render;   // graphics.render.<name> = value
    bool vsync=false,hdr=false;
    std::string dlss="Quality",renderScale="0.8",giQuality="High";
    std::string tier;                                          // for the record and the player: "16 GB", "10 GB" ...
};
inline double gigabytes(uint64_t bytes){return double(bytes)/(1024.0*1024.0*1024.0);}
// The recommendation for this machine. Video memory stands for the card's class too (the eyes' pictures are its largest
// use in VR; a card with less of it is mostly a slower one - a guess): 11 GB and more the usual size (0.8) and GI High -
// the eyes' indirect light agrees sooner (include/evr/vr_settings.hpp giQuality); less a smaller picture (0.7, the
// smallest size the setting exe offers) and GI Performance; texture filtering High / Medium / Low (under 7.5 GB). What
// is seen (distance, deco, tessellation, detail, voxels, fog) is not lowered for any card (18:55, the user). Unknown
// machine: as 11 GB and more.
inline Recommendation recommend(const Machine& m){
    Recommendation r;
    const double gb=m.known?gigabytes(m.videoMemory):16.;
    const int tier=gb>=11.?2:gb>=7.5?1:0;
    char text[32];std::snprintf(text,sizeof text,"%.0f GB",gb);r.tier=m.known?text:"unknown";
    r.renderScale=tier==2?"0.8":"0.7";
    r.giQuality=tier==2?"High":"Performance";
    r.render={{"antiAliasing",m.vendor&&m.vendor!=nvidia?"Fsr3":"Dlss"},{"preset","Custom"},{"shadows","Low"},{"contactShadows","Disabled"},{"reflections","Balance"},{"water","Balance"},
        {"fog","Medium"},{"ssao","Disabled"},{"distantObjects","Medium"},{"decoObjects","Medium"},{"detailScattering","Medium"},
        {"voxelMeshQuality","Low"},{"tessellation","Medium"},{"pointLightShadows","Low"},{"textureAnisotropy",tier==2?"High":tier==1?"Medium":"Low"},
        {"models","Balance"}};   // every quality item (2026-10-25, user: "the button writes every item"); this PC's value as for the looks
    r.render.push_back({"dynamicGi",r.giQuality});   // into the game's settings (2026-10-25)
    return r;
}
// "name": true|false, where it stands exactly once.
inline bool flag(const std::string& text,const std::string& name,size_t& begin,size_t& end){
    const std::string key="\""+name+"\"";size_t found=0;
    for(size_t at=text.find(key);at!=std::string::npos;at=text.find(key,at+1)){
        size_t i=at+key.size();while(i<text.size()&&(text[i]==' '||text[i]=='\t'))++i;
        if(i>=text.size()||text[i]!=':')continue;
        ++i;while(i<text.size()&&(text[i]==' '||text[i]=='\t'||text[i]=='\r'||text[i]=='\n'))++i;
        if(text.compare(i,4,"true")==0){++found;begin=i;end=i+4;}else if(text.compare(i,5,"false")==0){++found;begin=i;end=i+5;}
    }
    return found==1;
}
struct Applied {std::vector<std::string> changed,missing;};
// The game's settings text with the recommendation in; every other byte as it was. A field that is not there exactly
// once is left (and named in `missing`).
inline std::string applyGame(const std::string& text,const Recommendation& r,Applied& out){
    std::string s=text;
    for(const auto& [name,value]:r.render){size_t b=0,e=0;
        if(!autostart::field(s,name,b,e)){out.missing.push_back(name);continue;}
        const auto now=s.substr(b,e-b);if(now==value)continue;
        out.changed.push_back(name+" "+now+"->"+value);s=s.substr(0,b)+value+s.substr(e);}
    // The DLSS mode, and FSR 3's like it where the file has one (a card without DLSS).
    {size_t b=0,e=0;if(!autostart::field(s,"dlss",b,e))out.missing.push_back("dlss");else if(s.substr(b,e-b)!=r.dlss){out.changed.push_back("dlss "+s.substr(b,e-b)+"->"+r.dlss);s=s.substr(0,b)+r.dlss+s.substr(e);}
     const auto fsr=autostart::fsr3Preset(r.dlss);if(!fsr.empty()&&autostart::field(s,"fsr3",b,e)&&s.substr(b,e-b)!=fsr){out.changed.push_back("fsr3 "+s.substr(b,e-b)+"->"+fsr);s=s.substr(0,b)+fsr+s.substr(e);}}
    for(const auto& [name,value]:std::vector<std::pair<std::string,bool>>{{"vsync",r.vsync},{"enableHdr",r.hdr}}){size_t b=0,e=0;
        if(!flag(s,name,b,e)){out.missing.push_back(name);continue;}
        const std::string want=value?"true":"false",now=s.substr(b,e-b);if(now==want)continue;
        out.changed.push_back(name+" "+now+"->"+want);s=s.substr(0,b)+want+s.substr(e);}
    return s;
}
// The game's settings the recommendation writes (2026-10-25): the record of an earlier VR start (what it changed, put back
// at the next start without VR - include/evr/auto_start.hpp) loses them, so that it does not take the player's choice back.
inline std::vector<std::string> fields(const Recommendation& r){
    std::vector<std::string> f;for(const auto& kv:r.render)f.push_back(kv.first);
    for(const char* n:{"dlss","fsr3","vsync","enableHdr"})f.push_back(n);
    return f;
}
inline bool dropFromRecord(autostart::RenderRecord& record,const Recommendation& r){
    const auto names=fields(r);const auto count=record.changes.size();
    record.changes.erase(std::remove_if(record.changes.begin(),record.changes.end(),[&](const autostart::Change& c){return std::find(names.begin(),names.end(),c.name)!=names.end();}),record.changes.end());
    return record.changes.size()!=count;
}
// The mod's settings (EnshroudedVR.ini) with the VR side of it: the eyes' picture size (GI goes into the game's settings).
inline std::string applyIni(const std::string& ini,const Recommendation& r,std::vector<std::string>* changed=nullptr){
    auto out=ini;
    for(const auto& [key,value]:std::vector<std::pair<std::string,std::string>>{{"RenderScale",r.renderScale}}){
        const auto now=settings::get(out,"Picture",key);if(now==value)continue;
        if(changed)changed->push_back(key+" "+(now.empty()?"(unset)":now)+"->"+value);
        out=settings::set(out,"Picture",key,value);}
    return out;
}
// Asked in the game (its start menu): done by the loader at the next start, before the game reads its settings - the
// game has its settings in memory already and writes them back when it closes ([Graphics] Recommended=1). The loader
// runs inside DllMain, where DXGI must not be asked: the card's video memory is written with the request
// ([Graphics] VideoMemoryMB, read in the game) and the loader makes the recommendation from that.
inline bool pending(const std::string& ini){return settings::get(ini,"Graphics","Recommended")=="1";}
inline std::string setPending(const std::string& ini,bool on,const Machine& m={}){
    auto out=settings::set(ini,"Graphics","Recommended",on?"1":"0");
    if(on&&m.known)out=settings::set(out,"Graphics","VideoMemoryMB",std::to_string(m.videoMemory/(1024ull*1024ull)));
    if(on&&m.vendor)out=settings::set(out,"Graphics","VideoVendor",std::to_string(m.vendor));   // DLSS or FSR 3 (2026-10-25)
    return out;
}
inline Machine pendingMachine(const std::string& ini){
    Machine m;const auto v=settings::get(ini,"Graphics","VideoMemoryMB");char* end=nullptr;const auto mb=std::strtoull(v.c_str(),&end,10);
    if(!v.empty()&&end&&!*end&&mb>0&&mb<1024ull*1024ull){m.known=true;m.videoMemory=mb*1024ull*1024ull;}
    {const auto id=settings::get(ini,"Graphics","VideoVendor");char* stop=nullptr;const auto n=std::strtoul(id.c_str(),&stop,10);
     if(!id.empty()&&stop&&!*stop&&n>0&&n<=0xffff)m.vendor=uint32_t(n);}
    return m;
}
}
