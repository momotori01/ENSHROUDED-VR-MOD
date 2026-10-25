#pragma once
// Diagnostic (2026-10-24, the bow's strong shot with the equipment bow - user: "make the strong shot come out", then
// "rewrite the game's rule"). Read in the executable (build 26082781): the ranged weapon's input step 46f980 asks
// 46ef20 for a special-ability request (in AimAndCharge, with no action playing, of the ranged weapon's own item: input
// type 9 checked by 470c10), then 46d1a0 decides it (the item's SpecialAbility row, 3afcf0 with 15, and its
// requirements); the fourth flag it returns makes the state machine 4710a0 go to Fire, and the ranged system 2e69f5
// starts the item's special ability 2ba6b0 with RangedWeapon+0x54 (the bow). Which of these refuses while the bow is
// aimed is not known; this records each one's result when it changes, per actor (the first 3000 records). Nothing is
// written. Opt-in: EVR_RANGED_SPECIAL_PROBE=1.
namespace rangedSpecialProbe {
using Reader=uintptr_t(__fastcall*)(void*,void*);
using Check=uintptr_t(__fastcall*)(void*,void*,uint64_t,uint64_t);
using Decide=uint64_t(__fastcall*)(void*,void*);
inline Reader readerOriginal=nullptr;inline Check checkOriginal=nullptr;inline Decide decideOriginal=nullptr,startOriginal=nullptr;
inline bool enabled=false;
constexpr uintptr_t readerRva=0x46ef20,checkRva=0x470c10,decideRva=0x46d1a0,startRva=0x2ba6b0;
constexpr const char* readerPattern="48 89 5C 24 08 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 C9 48 81 EC";
constexpr const char* checkPattern="48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 41 0F B6 F1 48 8B D9 8B CE 48 8B FA 45 84 C9 0F 84";
constexpr const char* decidePattern="48 89 5C 24 20 57 41 54 41 55 41 56 41 57 48 81 EC B0 00 00 00 4C 8B E2 4C 8B F1 E8 80 03 00 00";
constexpr const char* startPattern="40 55 53 56 41 54 41 55 41 56 48 8D AC 24 68 FF FF FF 48 81 EC 98 01 00 00 4C 8B E2 4C 8B F1";
inline std::atomic<unsigned> logged{0},inside{0};inline std::atomic<uint64_t> calls{0};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
inline std::mutex mutex;inline std::map<std::tuple<int,uintptr_t,uint64_t>,std::string> last;
inline std::string bytesOf(uintptr_t at,size_t n){static const char* d="0123456789abcdef";std::string s;
 try{for(size_t i=0;i<n;++i){const auto b=read<uint8_t>(at+i);s+=d[b>>4];s+=d[b&15];}}catch(...){s+="??";}return s;}
inline uint8_t stateOf(uintptr_t ctx){try{const auto w=read<uintptr_t>(ctx+0x138);return w?read<uint8_t>(w):0xff;}catch(...){return 0xfe;}}
inline uintptr_t actorOf(uintptr_t ctx){try{return read<uintptr_t>(ctx+0x78);}catch(...){return 0;}}
inline void note(int which,uintptr_t actor,const std::string& key,const std::string& text,uint64_t slot=0){
 if(logged.load()>=3000)return;
 {std::lock_guard<std::mutex> lock(mutex);auto& l=last[{which,actor,slot}];if(l==key)return;l=key;}
 ++logged;const ObserverLastError preserve;
 try{logFile->event("ranged_special_probe","\"step\":"+std::to_string(which)+",\"actor\":"+hex(actor)+",\"frame\":"+std::to_string(liveFrame.frame)+","+text);}catch(...){}
}
uintptr_t __fastcall reader(void* out,void* ctx){
 const Inside counted;++calls;const auto result=readerOriginal(out,ctx);
 try{const auto c=reinterpret_cast<uintptr_t>(ctx);const auto o=reinterpret_cast<uintptr_t>(out);const auto rec=bytesOf(o,32);const auto st=stateOf(c);
  note(1,actorOf(c),rec.substr(0,2)+rec.substr(16,2)+std::to_string(st),"\"what\":\"special_input\",\"state\":"+std::to_string(st)+",\"record\":\""+rec+"\"");}catch(...){}
 return result;
}
uintptr_t __fastcall check(void* out,void* ctx,uint64_t item,uint64_t type){
 const Inside counted;const auto result=checkOriginal(out,ctx,item,type);
 try{const auto c=reinterpret_cast<uintptr_t>(ctx);const auto rec=bytesOf(reinterpret_cast<uintptr_t>(out),32);
  note(2,actorOf(c),rec.substr(0,2)+rec.substr(16,2)+std::to_string(uint32_t(item))+std::to_string(uint8_t(type)),"\"what\":\"item_input\",\"item\":"+std::to_string(uint32_t(item))+",\"type\":"+std::to_string(unsigned(uint8_t(type)))+",\"record\":\""+rec+"\"",(uint64_t(uint32_t(item))<<8)|uint8_t(type));}catch(...){}
 return result;
}
uint64_t __fastcall decide(void* ctx,void* request){
 const Inside counted;const auto req=bytesOf(reinterpret_cast<uintptr_t>(request),24);const auto result=decideOriginal(ctx,request);
 try{const auto c=reinterpret_cast<uintptr_t>(ctx);const auto st=stateOf(c);
  note(3,actorOf(c),std::to_string(result&0xff)+std::to_string(st),"\"what\":\"special_decide\",\"state\":"+std::to_string(st)+",\"result\":"+std::to_string(result&0xff)+",\"request\":\""+req+"\"");}catch(...){}
 return result;
}
uint64_t __fastcall start(void* ctx,void* request){
 const Inside counted;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;const auto req=bytesOf(reinterpret_cast<uintptr_t>(request),24);
 const auto result=startOriginal(ctx,request);
 try{note(4,caller,req+std::to_string(result&0xff)+std::to_string(GetTickCount64()/50),"\"what\":\"special_start\",\"caller\":"+hex(caller)+",\"result\":"+std::to_string(result&0xff)+",\"request\":\""+req+"\"");}catch(...){}
 return result;
}
inline bool hook(uintptr_t rva,const char* pattern,void* detour,void** original){
 const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva;
 if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("ranged special probe signature changed at "+hex(rva));
 return MH_CreateHook(reinterpret_cast<void*>(site),detour,original)==MH_OK&&MH_EnableHook(reinterpret_cast<void*>(site))==MH_OK;
}
inline void install() noexcept {
 try{
  char value[8]{};const auto n=GetEnvironmentVariableA("EVR_RANGED_SPECIAL_PROBE",value,sizeof value);
  if(!(n==1&&value[0]=='1'))return;
  if(!hook(readerRva,readerPattern,reinterpret_cast<void*>(&reader),reinterpret_cast<void**>(&readerOriginal))||
     !hook(checkRva,checkPattern,reinterpret_cast<void*>(&check),reinterpret_cast<void**>(&checkOriginal))||
     !hook(decideRva,decidePattern,reinterpret_cast<void*>(&decide),reinterpret_cast<void**>(&decideOriginal))||
     !hook(startRva,startPattern,reinterpret_cast<void*>(&start),reinterpret_cast<void**>(&startOriginal)))throw std::runtime_error("ranged special probe hook failed");
  enabled=true;logFile->event("ranged_special_probe_enabled","\"read_only\":true");
 }catch(const std::exception& e){try{logFile->event("ranged_special_probe_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void disable() noexcept {
 if(!enabled)return;enabled=false;
 for(const auto rva:{readerRva,checkRva,decideRva,startRva})MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));
 const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 try{logFile->event("ranged_special_probe_removed","\"calls\":"+std::to_string(calls.load())+",\"records\":"+std::to_string(logged.load()));}catch(...){}
}
}
