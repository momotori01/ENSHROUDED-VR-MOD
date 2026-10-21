// Read-only scoped UI dispatch observation. No UI/save flags or input changes.
namespace uiStateObserver {
inline bool requested=false,enabled=false;
inline std::atomic<bool> accepting{false};inline std::atomic<unsigned> callbacks{0},failures{0},calls{0};
inline std::atomic<uint8_t> previousState{255};
constexpr uintptr_t rva=0xbc230b;
// Hotfix 1.0.0.1 (2026-10-16): the switch is a jump table of seven states now, its object in r15 (1.0: a chain of six in r14).
constexpr const char* pattern="41 0F B6 47 0C FF C8 83 F8 06 0F 87";
struct CallbackScope{CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
inline void onDispatch(uintptr_t rsp) noexcept {
 const CallbackScope callback;const ObserverLastError preserve;
 if(!accepting.load())return;
 try{
  const auto rbp=read<uintptr_t>(rsp-48),object=read<uintptr_t>(rsp-128); // saved RBP/R15, stub-owned slots
  ULONG_PTR low=0,high=0;GetCurrentThreadStackLimits(&low,&high);
  if(rbp!=rsp+0x100||rbp<low||rbp>high-16||!object)throw std::runtime_error("UI dispatch stack ABI differs");
  const auto data=read<uintptr_t>(rbp+8);
  if(!data||read<uintptr_t>(data)!=object)throw std::runtime_error("UI dispatch scoped owner differs");
  const auto before=read<std::array<uint8_t,16>>(object);const auto state=before[12];
  if(read<std::array<uint8_t,16>>(object)!=before||read<uintptr_t>(data)!=object)throw std::runtime_error("UI dispatch state changed during observation");
  const auto number=++calls;const auto previous=previousState.exchange(state);
  if(number<=4||state!=previous||number%120==0){
   logFile->event("ui_state_dispatch_sample","\"sample\":"+std::to_string(number)+",\"state\":"+std::to_string(state)+
     ",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"milliseconds\":"+std::to_string(GetTickCount64())+
     ",\"owner\":"+hex(object)+",\"known_branch\":"+(state>=1&&state<=5?"true":"false")+",\"scoped_owner_matches\":true,\"source_unchanged\":true,\"read_only\":true");
  }
 }catch(const std::exception& e){if(failures.fetch_add(1)==0)try{logFile->event("ui_state_dispatch_failure","\"reason\":"+jsonString(e.what()));}catch(...){} }
}
inline void install(){
 auto site=reinterpret_cast<void*>(baseAddress+rva);const auto p=parsePattern(pattern);
 if(scan(static_cast<const uint8_t*>(site),p.size(),p)!=std::vector<size_t>{0}||read<uint8_t>(baseAddress+0xbc239d)!=0xe8||   // 1.0: the call to the slide at bc017d (the matcher took aac491 to bc011a, a call of c565e0)
    uintptr_t(int64_t(baseAddress+0xbc23a2)+read<int32_t>(baseAddress+0xbc239e))!=baseAddress+0xca8da0)
  throw std::runtime_error("native UI dispatch signature/slide target differs");
 if(MH_CreateHook(site,reinterpret_cast<void*>(&evr_ui_state_stub),&evr_ui_state_trampoline)!=MH_OK)throw std::runtime_error("UI dispatch observer hook creation");
 accepting=true;
 if(MH_EnableHook(site)!=MH_OK){accepting=false;throw std::runtime_error("UI dispatch observer hook enable");}
 enabled=true;logFile->event("ui_state_dispatch_enabled","\"rva\":"+hex(rva)+",\"pattern\":"+jsonString(pattern)+",\"read_only\":true");
}
inline void disable(){
 accepting=false;if(!enabled)return;
 const auto site=reinterpret_cast<void*>(baseAddress+rva);
 if(MH_DisableHook(site)!=MH_OK)throw std::runtime_error("UI dispatch observer disable");enabled=false;
 const auto deadline=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 const auto p=parsePattern(pattern);const bool restored=scan(static_cast<const uint8_t*>(site),p.size(),p)==std::vector<size_t>{0};
 logFile->event("ui_state_dispatch_removed","\"calls\":"+std::to_string(calls.load())+",\"failures\":"+std::to_string(failures.load())+
   ",\"callbacks\":"+std::to_string(callbacks.load())+",\"original_bytes_restored\":"+(restored?"true":"false")+",\"trampoline_retained\":true");
 if(!restored||callbacks.load())throw std::runtime_error("UI dispatch restoration/quiescence failed");
}
}
