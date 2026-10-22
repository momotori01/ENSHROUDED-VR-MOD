#pragma once
// The game's ray test against the world, for the guest's own arrows (2026-10-22, user 23:30: "online, an arrow that hits
// a wall shows no effect"). A guest's arrow flies on its PC (motion_arrow_visual.hpp) and knew the creatures and the
// level ground only, so it went through walls; the host keeps the game's arrow parked until a hit is reported
// (live_melee_targets.hpp steerArrows), so a wall showed nothing anywhere.
// 0xe71eb0 (36 callers; the ray_cast_spawner system 0x342360 calls it at 0x342748): (result, from, to, parameters), from
// and to float3 in metres of the world (the spawner makes them from the Q32 places times 2^-32, constant 0x13b565c).
// Result: +0 a status byte, 0 for a hit (the spawner reads the place only then: 0x34274d), the hit's place at +0x10.
// Parameters (read in 0xe71eb0): +0x20 the kinds; bit 0 the physics colliders, with their world at +0x10 (0xe71f38;
// +0x00 and +0x33 two bytes of that test, +0x30/+0x31 layer masks over a list at +0x18, skipped when both are 0); any
// other bit the voxels (terrain, buildings: 0x9fff60 at 0xe72200 with the voxel world at +0x08, +0x28, +0x2c, the byte
// +0x24 and the kinds; not for kinds == 1).
// Census on a guest (Mock game-20261022-234508, EVR_RAYCAST_CENSUS=1): all calls on one thread of the game, 1000 a
// second, from 4-6 callers; kinds 0x09 (0x32bca8, 0x307024) and 0x0a (0x3ce215) return places on the ground and walls.
// The first try cast the arrow's ray inside the call returning to 0x307024 with its parameters - that caller did not run
// at all in a bow session (game-20261022-235707: 0 rays). So now the two worlds are taken from the game's own calls
// that ask the voxels (kinds with bit 3, +0x2c 0, both worlds set; seen within the last second), and the arrow's ray is
// cast with parameters of its own (the two worlds, kinds 0x09, +0x28 as the game's) right after any of the game's calls
// returns on that same thread: one more ray of the game's own kind, on its thread, between its own. Nothing is cast
// when nobody asked within 0.3 s. EVR_ARROW_WALLS=0: not installed (unless the census).
namespace worldRay {
using Ray=void*(__fastcall*)(void*,const float*,const float*,const uint8_t*);
inline Ray original=nullptr;inline bool enabled=false,census=false;
constexpr uintptr_t rva=0xe71eb0;
struct Worlds{uintptr_t voxels=0,colliders=0;uint32_t extra=0,thread=0;uint64_t ms=0;};
inline Worlds worlds;   // only on the game's thread that casts (no lock: written and read there alone)
inline std::atomic<uint64_t> worldChanges{0};
constexpr const char* pattern="48 8b c4 48 89 58 08 4c 89 40 18 48 89 50 10 55 56 57 41 54 41 55 41 56 41 57 48 8d 6c 24 90 48";
struct Request{uint64_t serial=0,ms=0;float from[3]{},to[3]{};};
struct Answer{uint64_t serial=0,ms=0;bool hit=false;float at[3]{};};
inline std::mutex askMutex;inline Request asked;inline Answer answered;inline std::atomic<uint64_t> askSerial{0},cast{0},castHits{0};
// Any thread: the newest ask replaces an older one not cast yet.
inline uint64_t ask(const float from[3],const float to[3]){
    std::lock_guard<std::mutex> lock(askMutex);asked.serial=++askSerial;asked.ms=GetTickCount64();for(int k=0;k<3;k++){asked.from[k]=from[k];asked.to[k]=to[k];}return asked.serial;
}
inline Answer answer(){std::lock_guard<std::mutex> lock(askMutex);return answered;}
// The census (EVR_RAYCAST_CENSUS=1 in the game's environment): callers, threads, sample parameters, read only.
struct Caller{uintptr_t rva=0;uint64_t calls=0,misses=0;uint32_t thread=0;unsigned samples=0;};
inline std::mutex censusMutex;inline std::array<Caller,64> callers{};inline std::atomic<uint64_t> total{0};inline uint64_t startMs=0,lastSummary=0;inline unsigned summaries=0;
inline std::string floats(const float* p,unsigned n){std::string s="[";for(unsigned i=0;i<n;i++){char b[32];std::snprintf(b,sizeof b,"%s%.3f",i?",":"",double(p[i]));s+=b;}return s+"]";}
inline void summary(uint64_t now){
    std::vector<Caller> list;for(const auto& c:callers)if(c.rva)list.push_back(c);
    std::sort(list.begin(),list.end(),[](const Caller& a,const Caller& b){return a.calls>b.calls;});
    std::string s;for(size_t i=0;i<list.size()&&i<16;i++)s+=std::string(i?",":"")+"{\"caller\":"+hex(list[i].rva)+",\"calls\":"+std::to_string(list[i].calls)+",\"status_not_0\":"+std::to_string(list[i].misses)+",\"thread\":"+std::to_string(list[i].thread)+"}";
    logFile->event("raycast_census","\"seconds\":"+std::to_string((now-startMs)/1000)+",\"total\":"+std::to_string(total.load())+",\"arrow_rays\":"+std::to_string(cast.load())+",\"callers\":["+s+"]");
}
inline void censusBefore(uintptr_t caller,const uint8_t* parameters,bool& sample,std::array<uint8_t,0x40>& before){
    std::lock_guard<std::mutex> lock(censusMutex);++total;
    Caller* slot=nullptr;for(auto& c:callers){if(c.rva==caller){slot=&c;break;}if(!c.rva){c.rva=caller;c.thread=GetCurrentThreadId();slot=&c;break;}}
    if(slot){++slot->calls;if(slot->samples<3){++slot->samples;sample=true;}}
    if(sample)before=read<std::array<uint8_t,0x40>>(reinterpret_cast<uintptr_t>(parameters));
}
inline void censusAfter(uintptr_t caller,void* result,const float* from,const float* to,bool sample,const std::array<uint8_t,0x40>& before){
    const auto status=read<uint8_t>(reinterpret_cast<uintptr_t>(result));const auto now=GetTickCount64();
    std::lock_guard<std::mutex> lock(censusMutex);
    if(status)for(auto& c:callers)if(c.rva==caller){++c.misses;break;}
    if(sample){const auto out=read<std::array<uint8_t,0x40>>(reinterpret_cast<uintptr_t>(result));
        logFile->event("raycast_sample","\"caller\":"+hex(caller)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"from\":"+floats(from,3)+",\"to\":"+floats(to,3)+
            ",\"parameters\":"+jsonString(nativeImpactComponents::asHex(before))+",\"result\":"+jsonString(nativeImpactComponents::asHex(out))+",\"status\":"+std::to_string(status));}
    if(!startMs)startMs=lastSummary=now;
    if(now-lastSummary>=5000&&summaries<24){lastSummary=now;++summaries;summary(now);}
}
inline unsigned castLogs=0;
__declspec(noinline) void* __fastcall ray(void* result,const float* from,const float* to,const uint8_t* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    std::array<uint8_t,0x40> before{};bool sample=false;
    if(census){const ObserverLastError preserve;try{censusBefore(caller,parameters,sample,before);}catch(...){sample=false;}}
    auto* r=original(result,from,to,parameters);
    const ObserverLastError preserve;
    if(census)try{censusAfter(caller,result,from,to,sample,before);}catch(...){}
    try{
        const auto p=reinterpret_cast<uintptr_t>(parameters);const auto now=GetTickCount64();const auto thread=GetCurrentThreadId();
        // the two worlds, from the game's own voxel rays
        const auto kinds=read<uint32_t>(p+0x20);
        if((kinds&8)&&!read<uint32_t>(p+0x2c)){const auto v=read<uintptr_t>(p+8),c=read<uintptr_t>(p+0x10);
            if(v&&c){if(v!=worlds.voxels||c!=worlds.colliders||thread!=worlds.thread)++worldChanges;worlds={v,c,read<uint32_t>(p+0x28),thread,now};}}
        // the arrow's ray, right after the game's own on its thread
        if(worlds.ms&&thread==worlds.thread&&now>=worlds.ms&&now-worlds.ms<=1000){
            Request q;uint64_t done=0;{std::lock_guard<std::mutex> lock(askMutex);q=asked;done=answered.serial;}
            if(q.serial&&q.serial>done&&now>=q.ms&&now-q.ms<=300){
                alignas(16) std::array<uint8_t,0x40> mine{};alignas(16) std::array<uint8_t,0x200> own{};
                std::memcpy(mine.data()+8,&worlds.voxels,8);std::memcpy(mine.data()+0x10,&worlds.colliders,8);
                const uint32_t asking=9;std::memcpy(mine.data()+0x20,&asking,4);std::memcpy(mine.data()+0x28,&worlds.extra,4);
                original(own.data(),q.from,q.to,mine.data());++cast;
                Answer a;a.serial=q.serial;a.ms=now;a.hit=own[0]==0;std::memcpy(a.at,own.data()+0x10,12);
                // a place off the asked segment (or not a number) is not taken as a hit
                if(a.hit){float len2=0,d2=0;for(int k=0;k<3;k++){len2+=(q.to[k]-q.from[k])*(q.to[k]-q.from[k]);d2+=(a.at[k]-q.from[k])*(a.at[k]-q.from[k]);}
                    if(!std::isfinite(a.at[0])||!std::isfinite(a.at[1])||!std::isfinite(a.at[2])||d2>len2*1.0201f+.01f)a.hit=false;}
                if(a.hit)++castHits;
                {std::lock_guard<std::mutex> lock(askMutex);answered=a;}
                if(castLogs<60){++castLogs;logFile->event("arrow_ray_cast","\"serial\":"+std::to_string(q.serial)+",\"after_caller\":"+hex(caller)+",\"from\":"+floats(q.from,3)+",\"to\":"+floats(q.to,3)+",\"status\":"+std::to_string(own[0])+
                    ",\"hit\":"+(a.hit?"true":"false")+",\"at\":"+floats(a.at,3)+",\"asked_ms_ago\":"+std::to_string(now-q.ms)+",\"world_changes\":"+std::to_string(worldChanges.load()));}
            }
        }
    }catch(...){}
    return r;
}
inline void install(){
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_RAYCAST_CENSUS",value,sizeof value);census=n==1&&value[0]=='1';}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_ARROW_WALLS",value,sizeof value);if(n==1&&value[0]=='0'&&!census){logFile->event("arrow_walls_off","\"reason\":\"EVR_ARROW_WALLS=0\"");return;}}
    const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("ray test signature changed");
    if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&ray),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("ray test hook failed");
    enabled=true;logFile->event("world_ray_enabled","\"rva\":"+hex(rva)+",\"census\":"+(census?"true":"false"));
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{if(census){std::lock_guard<std::mutex> lock(censusMutex);summary(GetTickCount64());}
        logFile->event("world_ray_removed","\"status\":"+std::to_string(status)+",\"arrow_rays\":"+std::to_string(cast.load())+",\"arrow_ray_hits\":"+std::to_string(castHits.load())+",\"world_changes\":"+std::to_string(worldChanges.load()));}catch(...){}
}
}
