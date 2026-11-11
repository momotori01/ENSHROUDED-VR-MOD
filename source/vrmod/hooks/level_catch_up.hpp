// The level catch-up for guests (2026-11-10; user, 11-09: "to close the level gap with people who play now and then: when you
// join a host's world three or more levels below the host, you are raised to the host's level - 1; switched in the exe, off
// by default" and "if the experience is the host's business, put it on the host's side"). [Online] LevelCatchUp; the loader
// runs EVR_LevelCatchUp once at the start (VR or not), and these hooks stay until the game closes.
//
// The host's game runs the server. Its task admin_player (0x2d0110, build 26430625) calls the admin check 0x3baaf0 (session
// entry +0x9d bit 0) for every player each tick, at 0x2d0238, in the task's frame: rbp = rsp + 0x100. The host is admin,
// the guests are not. What the task does for the host's own input is used: the game's admin action AdminSetCharacterLevel
// in PlayerInput ([rbp+0x170]) .fromAdminClient (+0x3c0) .adminSetCharacterLevel (+0xe4: version +0x4a4, targetEntityId
// +0x4a8, level +0x4ac). With the target and the level written there and ServerConsumedPlayerInput ([rbp+0x178]) +0xc0 made
// to differ from the input's version, the task applies it in the same call - the target's Level component gets the level,
// its experience 0 and what the next level needs (0xe976e0, called at 0x2d141e) - and marks the version consumed again.
// Mock 11-10 (logs 20261110-012913 / -013236): the own character 25 -> 20 that way, still 20 ten seconds later. The client's
// own admin field does not reach the server (docs/analysis/level_gap_20261110.md), so a guest can not do it himself.
//
// A level is read as the task finds it: the Level component (its type [rbp+0x1f8]) of a player's entity by 0x9b2900
// (out, [[ctx]+0x30], [[ctx]+0x1c0], type, entity, byte [[ctx]+0xbb]; ctx = [rbp+0x1700], the task's argument), and its
// attribute 0x724bf568 found by 0x123a930 (out, hash, registry [rbp+0x1c0]): the value at component + word[component+4] +
// index * 4 (component byte +6 bit 0 set). A player's entity is the one the admin check is asked about.
//
// A guest is decided once a stay: 10 s after first seen, two equal reads 3 s apart; raised when guest + 3 <= host, to host - 1,
// one guest a tick; the level read back a second later. A guest not seen for 10 s is forgotten (a new join is decided again).
// EVR_LEVEL_CATCH_UP_TEST=<n> (Mock, no guests there): the host is also taken as a guest of a host n levels higher.
#include <map>
namespace levelCatchUp {
constexpr uintptr_t adminRva=0x3baaf0,adminReturn=0x2d023d,lookupRva=0x9b2900,attributeRva=0x123a930,xpRva=0xe976e0,xpReturn=0x2d1423;
constexpr uintptr_t taskRva=0x2d0110,readsRva=0x2d0245,levelReadRva=0x2d12ca;
constexpr const char* adminPattern="8D 41 FF 4C 8B C2 3D FE 03 00 00 77";
constexpr const char* lookupPattern="40 53 48 83 EC 30 0F B6 44 24 68";
constexpr const char* attributePattern="89 54 24 10 53 48 83 EC 20 48 8B D9 48 8D 54 24 38 49 8D 48 68";
constexpr const char* xpPattern="4C 8B 4A 20 44 8B D1 4D 85 C9 75 06 B8 01 00 00 00 C3";
// The task's start (push rbp; push r12; lea rbp,[rsp-0x16e8]: the frame), its reads right after the check, and its level action.
constexpr const char* taskPattern="48 89 4C 24 08 55 41 54 48 8D AC 24 18 E9 FF FF B8 E8 17 00 00";
constexpr const char* readsPattern="48 8B 8D A0 01 00 00 4C 8B BD 70 01 00 00";
constexpr const char* levelReadPattern="41 8B 87 A4 04 00 00 41 39 85 C0 00 00 00 0F 84";
constexpr size_t frameRbp=0x100,inputAt=0x170,consumedAt=0x178,registryAt=0x1c0,levelTypeAt=0x1f8,contextAt=0x1700;
constexpr size_t versionAt=0x4a4,targetAt=0x4a8,levelAt=0x4ac,consumedVersionAt=0xc0;
constexpr uint32_t levelHash=0x724bf568;
constexpr uint64_t settleMs=10000,confirmMs=3000,retryMs=1000,goneMs=10000;
constexpr int gap=3,levelLow=1,levelHigh=200;
using Admin=uint8_t(__fastcall*)(uint32_t,void*);
using Lookup=void*(__fastcall*)(void*,void*,void*,void*,uint32_t,uint8_t);
using Attribute=void*(__fastcall*)(void*,uint32_t,void*);
using Xp=uint32_t(__fastcall*)(uint32_t,void*);
inline Admin originalAdmin=nullptr;inline Xp originalXp=nullptr;
inline unsigned testPlus=0;
struct Guest{uint64_t first=0,last=0,next=0;int level=-1;unsigned reads=0;bool decided=false,self=false;};
// Touched on the server task's thread only.
inline std::map<uint32_t,Guest> guests;inline Guest selfTest;inline uint32_t host=0,verifyTarget=0;inline int verifyLevel=0;inline uint64_t verifyAt=0;
inline std::atomic<unsigned> failures{0},raised{0},applied{0};
inline bool pointer(uintptr_t p){return p>=0x10000&&p<0x7ff000000000ull;}
// The level of a player's entity, as the task reads it (false: not readable now).
inline bool levelOf(uintptr_t rbp,uint32_t entity,int& level){
    const auto ctx=read<uintptr_t>(rbp+contextAt),type=read<uintptr_t>(rbp+levelTypeAt),registry=read<uintptr_t>(rbp+registryAt);
    if(!pointer(ctx)||!type||!pointer(registry))return false;
    const auto inner=read<uintptr_t>(ctx);if(!pointer(inner))return false;
    alignas(16) uintptr_t out[4]{};
    reinterpret_cast<Lookup>(baseAddress+lookupRva)(out,reinterpret_cast<void*>(read<uintptr_t>(inner+0x30)),reinterpret_cast<void*>(read<uintptr_t>(inner+0x1c0)),
        reinterpret_cast<void*>(type),entity,read<uint8_t>(inner+0xbb));
    const auto component=out[0];if(!pointer(component)||!(read<uint8_t>(component+6)&1))return false;
    struct Out{uint8_t status;uint8_t pad[3];uint32_t x;uintptr_t desc;} found{};
    reinterpret_cast<Attribute>(baseAddress+attributeRva)(&found,levelHash,reinterpret_cast<void*>(registry));
    if(found.status||!pointer(found.desc))return false;
    level=int(read<uint32_t>(component+read<uint16_t>(component+4)+size_t(read<uint16_t>(found.desc+0xc))*4));
    return level>=levelLow&&level<=levelHigh;
}
// The admin action into the host's own input of this tick.
inline void raise(uintptr_t rbp,uint32_t target,int level){
    const auto input=read<uintptr_t>(rbp+inputAt),consumed=read<uintptr_t>(rbp+consumedAt);
    if(!pointer(input)||!pointer(consumed))throw std::runtime_error("no input in the task's frame");
    const auto version=read<uint32_t>(input+versionAt);
    *reinterpret_cast<volatile uint32_t*>(input+targetAt)=target;
    *reinterpret_cast<volatile uint8_t*>(input+levelAt)=uint8_t(level);
    *reinterpret_cast<volatile uint32_t*>(consumed+consumedVersionAt)=version+1;
}
// One guest's turn (true: the host's input was used this tick).
inline bool decide(uintptr_t rbp,uint32_t id,Guest& g,uint64_t now,int plus){
    if(g.decided||now-g.first<settleMs||now<g.next)return false;
    int level=0;
    if(!levelOf(rbp,id,level)){g.next=now+retryMs;if(++g.reads>60){g.decided=true;logFile->event("level_catch_up_unreadable","\"guest\":"+std::to_string(id));}return false;}
    if(g.level!=level){g.level=level;g.next=now+confirmMs;if(++g.reads>60)g.decided=true;return false;}   // two equal reads, 3 s apart
    int own=0;if(!levelOf(rbp,host,own)){g.next=now+retryMs;return false;}
    own+=plus;g.decided=true;
    const bool raiseIt=level+gap<=own;
    logFile->event("level_catch_up_guest","\"guest\":"+std::to_string(id)+",\"guest_level\":"+std::to_string(level)+",\"host\":"+std::to_string(host)+
        ",\"host_level\":"+std::to_string(own)+(plus?",\"test_plus\":"+std::to_string(plus):"")+",\"raised_to\":"+(raiseIt?std::to_string(own-1):"null"));
    if(!raiseIt)return false;
    raise(rbp,id,own-1);raised.fetch_add(1);verifyTarget=id;verifyLevel=own-1;verifyAt=now+1000;return true;
}
inline void step(uintptr_t rbp,uint32_t id,bool admin){
    const auto now=GetTickCount64();
    if(!admin){auto& g=guests[id];if(!g.first){g.first=now;logFile->event("level_catch_up_seen","\"guest\":"+std::to_string(id));}g.last=now;return;}
    if(host!=id){host=id;logFile->event("level_catch_up_host","\"host\":"+std::to_string(id));}
    for(auto it=guests.begin();it!=guests.end();)
        if(now-it->second.last>goneMs){logFile->event("level_catch_up_gone","\"guest\":"+std::to_string(it->first));it=guests.erase(it);}else ++it;
    if(verifyAt&&now>=verifyAt){verifyAt=0;int level=0;const bool ok=levelOf(rbp,verifyTarget,level);
        logFile->event("level_catch_up_check","\"guest\":"+std::to_string(verifyTarget)+",\"wanted\":"+std::to_string(verifyLevel)+",\"read\":"+(ok?std::to_string(level):"null"));}
    for(auto& [guest,g]:guests)if(decide(rbp,guest,g,now,0))return;   // one a tick
    if(testPlus){if(!selfTest.first)selfTest.first=now;decide(rbp,id,selfTest,now,int(testPlus));}
}
__declspec(noinline) uint8_t __fastcall admin(uint32_t id,void* table){
    const auto r=originalAdmin(id,table);
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+adminReturn){
        const ObserverLastError preserve;
        try{step(reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+8+frameRbp,id,(r&1)!=0);}
        catch(const std::exception& e){if(failures.fetch_add(1)<5)logFile->event("level_catch_up_failure","\"reason\":"+jsonString(e.what()));}
    }
    return r;
}
__declspec(noinline) uint32_t __fastcall xp(uint32_t next,void* balancing){
    const auto r=originalXp(next,balancing);
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+xpReturn){
        const ObserverLastError preserve;
        if(applied.fetch_add(1)<64)logFile->event("level_catch_up_applied","\"level\":"+std::to_string(next-1)+",\"experience_of_next_level\":"+std::to_string(r));}
    return r;
}
inline void install(){
    {char v[16]{};const auto n=GetEnvironmentVariableA("EVR_LEVEL_CATCH_UP_TEST",v,sizeof v);if(n&&n<sizeof v){const auto x=std::strtoul(v,nullptr,10);if(x>=1&&x<=100)testPlus=unsigned(x);}}
    for(auto [rva,pattern]:{std::pair<uintptr_t,const char*>{adminRva,adminPattern},{lookupRva,lookupPattern},{attributeRva,attributePattern},{xpRva,xpPattern},
                            {taskRva,taskPattern},{readsRva,readsPattern},{levelReadRva,levelReadPattern}}){
        const auto bytes=parsePattern(pattern);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0})
            throw std::runtime_error("the game's code is not as expected at "+std::to_string(rva));}
    for(auto r:{adminReturn,xpReturn})if(read<uint8_t>(baseAddress+r-5)!=0xe8)throw std::runtime_error("the game's calls are not as expected");
    require(MH_CreateHook(reinterpret_cast<void*>(baseAddress+xpRva),reinterpret_cast<void*>(&xp),reinterpret_cast<void**>(&originalXp)),"level catch-up experience hook");
    require(MH_CreateHook(reinterpret_cast<void*>(baseAddress+adminRva),reinterpret_cast<void*>(&admin),reinterpret_cast<void**>(&originalAdmin)),"level catch-up admin hook");
    require(MH_EnableHook(reinterpret_cast<void*>(baseAddress+xpRva)),"level catch-up experience hook enable");
    require(MH_EnableHook(reinterpret_cast<void*>(baseAddress+adminRva)),"level catch-up admin hook enable");
    logFile->event("level_catch_up_enabled","\"gap\":"+std::to_string(gap)+",\"settle_ms\":"+std::to_string(settleMs)+(testPlus?",\"test_plus\":"+std::to_string(testPlus):""));
}
}
