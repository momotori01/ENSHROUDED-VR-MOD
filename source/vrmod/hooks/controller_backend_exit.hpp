#pragma once
// Native input-owner handoff. Optional joined proof reads completed MOD values only.
namespace controllerBackendExit {
using Destroy=void(__fastcall*)(void*,void*);
using Disconnect=uintptr_t(__fastcall*)(void*,uint32_t); // Preserve opaque RAX.
inline Destroy originalDestroy=nullptr;inline Disconnect originalDisconnect=nullptr;
inline bool(*joinedFinalizer)()=nullptr; // Optional nonblocking, MOD-value-only final proof.
inline bool enabled=false;inline std::atomic<unsigned> callbacks{0},failures{0},completion{0};
inline std::atomic<unsigned> destroys{0},disconnects{0};
inline thread_local bool insideDestroy=false;inline thread_local uintptr_t expectedManager=0;inline thread_local uint32_t expectedDevice=0;
constexpr uintptr_t destroyRva=0xf96db0,disconnectRva=0xf976b0;
constexpr const char* destroyPattern="48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 33 ED 48 8B DA 48 8B F1";
constexpr const char* disconnectPattern="48 89 5C 24 08 48 89 7C 24 18 89 54 24 10 55 48 8D 6C";
inline void fail(const char* reason) noexcept {++failures;try{logFile->event("controller_owner_exit_failure","\"reason\":"+jsonString(reason));}catch(...){} }
inline void disable(){
 if(!enabled)return;
 bool okay=true;
 for(const auto rva:{destroyRva,disconnectRva}){auto result=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));okay=okay&&(result==MH_OK||result==MH_ERROR_DISABLED);}
 enabled=false;
 const auto d=parsePattern(destroyPattern),c=parsePattern(disconnectPattern);
 const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+destroyRva),d.size(),d)==std::vector<size_t>{0}&&scan(reinterpret_cast<const uint8_t*>(baseAddress+disconnectRva),c.size(),c)==std::vector<size_t>{0};
 logFile->event("controller_owner_exit_removed","\"restored\":"+std::string(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load())+",\"failures\":"+std::to_string(failures.load())+",\"trampolines_retained\":true");
 if(!okay||!restored||callbacks.load())throw std::runtime_error("native input owner hook restoration failed");
}
__declspec(noinline) uintptr_t __fastcall disconnect(void* manager,uint32_t device){
 ++callbacks;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 const bool owned=insideDestroy&&reinterpret_cast<uintptr_t>(manager)==expectedManager&&device==expectedDevice;
 if(owned){const ObserverLastError preserve;try{++disconnects;logFile->event("controller_owner_native_disconnect_begin","\"caller\":"+hex(caller)+",\"manager\":"+hex(expectedManager)+",\"device\":"+std::to_string(device)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"read_only\":true");}catch(...){fail("native disconnect begin log");}}
 const auto result=originalDisconnect(manager,device);const ObserverLastError preserve;
 if(owned)try{logFile->event("controller_owner_native_disconnect_end","\"manager\":"+hex(expectedManager)+",\"device\":"+std::to_string(device)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"arguments_unchanged\":true,\"return_unchanged\":true");}catch(...){fail("native disconnect end log");}
 --callbacks;return result;
}
__declspec(noinline) void __fastcall destroy(void* allocator,void* object){
 ++callbacks;const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 controllerUiFixture::BackendExitToken token;
 {const ObserverLastError preserve;try{
  token=controllerUiFixture::beginBackendExit(reinterpret_cast<uintptr_t>(object),caller);
  if(token.valid()){
   if(insideDestroy||++destroys!=1)throw std::runtime_error("reentrant/duplicate finite backend destruction");
   expectedManager=read<uintptr_t>(token.backend+0x30);expectedDevice=read<uint32_t>(token.backend+0x38);insideDestroy=true;
   if(!expectedManager||expectedManager!=token.manager )throw std::runtime_error("native connected device owner differs");
   logFile->event("controller_owner_native_original_begin","\"backend\":"+hex(token.backend)+",\"allocator\":"+hex(reinterpret_cast<uintptr_t>(allocator))+",\"manager\":"+hex(expectedManager)+",\"device\":"+std::to_string(expectedDevice)+",\"thread\":"+std::to_string(GetCurrentThreadId()));
  }
 }catch(const std::exception& e){fail(e.what());}}
 // Always preserve the genuine destructor even if diagnostics fail.
 originalDestroy(allocator,object);const ObserverLastError preserve;insideDestroy=false;
 if(token.valid())try{
  logFile->event("controller_owner_native_original_end","\"backend\":"+hex(token.backend)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"arguments_unchanged\":true,\"reads_destroyed_memory\":false");
  if(!controllerUiFixture::finishBackendExit(token))throw std::runtime_error("native owner handoff did not complete");
 }catch(const std::exception& e){fail(e.what());}
 --callbacks;
 if(token.valid()){
  // Complete on this native owner thread; ExitProcess can outpace a worker.
  try{if(inputUi::enabled||inputUi::callbacks.load()||inputUi::failures.load()||destroys!=1||disconnects!=1||failures.load())throw std::runtime_error("owner handoff prerequisites did not quiesce");
   controllerUiFixture::drain();disable();
   if(joinedFinalizer&&!joinedFinalizer())throw std::runtime_error("joined consumers not complete at actual input owner return");
   logFile->event("controller_owner_exit_complete","\"result\":0,\"synchronous_owner_thread\":true,\"thread\":"+std::to_string(GetCurrentThreadId())+",\"world_reads_after_handoff\":false,\"claims_xr_combat_exit\":false");completion=2;
  }catch(const std::exception& e){fail(e.what());try{disable();}catch(...){}completion=3;}
 }
}
inline void install(){
 for(const auto& p:std::array<std::pair<uintptr_t,const char*>,2>{{{destroyRva,destroyPattern},{disconnectRva,disconnectPattern}}}){const auto bytes=parsePattern(p.second);if(scan(reinterpret_cast<const uint8_t*>(baseAddress+p.first),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("input owner signature differs");}
 for(const auto& p:std::array<std::pair<uintptr_t,uintptr_t>,2>{{{0xdad468,destroyRva},{0xdace21,disconnectRva}}})if(read<uint8_t>(baseAddress+p.first)!=0xe8||uintptr_t(int64_t(p.first+5)+read<int32_t>(baseAddress+p.first+1))!=p.second)throw std::runtime_error("input owner direct call differs");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+destroyRva),reinterpret_cast<void*>(&destroy),reinterpret_cast<void**>(&originalDestroy))!=MH_OK||MH_CreateHook(reinterpret_cast<void*>(baseAddress+disconnectRva),reinterpret_cast<void*>(&disconnect),reinterpret_cast<void**>(&originalDisconnect))!=MH_OK)throw std::runtime_error("input owner hook create failed");
 enabled=true;
 if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+destroyRva))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+disconnectRva))!=MH_OK)throw std::runtime_error("input owner hook enable failed");
 logFile->event("controller_owner_exit_enabled","\"destroy_rva\":\"0xf96db0\",\"disconnect_rva\":\"0xf976b0\",\"expected_caller\":\"0xf97414\",\"neutral_only\":"+std::string(controllerUiFixture::motionAttackRequested?"false":"true")+",\"system_settings_unchanged\":true");
}
}
