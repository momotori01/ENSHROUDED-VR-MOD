#include <evr/hook_disable_batch.hpp>
#include <iostream>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <string>
using Call=int(*)();
Call originals[4]{};
__declspec(noinline) int a(){return originals[0]()+100;}
__declspec(noinline) int b(){return originals[1]()+100;}
__declspec(noinline) int c(){return originals[2]()+100;}
__declspec(noinline) int d(){return originals[3]()+100;}
int main(){unsigned checks=0;unsigned char* code=nullptr;try{
 auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("hook batch check "+std::to_string(checks));};
 check(MH_Initialize()==MH_OK);
 code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));check(code!=nullptr);
 std::memset(code,0x90,4096);std::array<void*,4> targets{};std::array<Call,4> calls{};const Call detours[]{a,b,c,d};
 for(size_t i=0;i<4;++i){auto* p=code+i*128;const int value=int(i+1)*10;p[0]=0xb8;std::memcpy(p+1,&value,4);p[5]=0xc3;targets[i]=p;calls[i]=reinterpret_cast<Call>(p);check(FlushInstructionCache(GetCurrentProcess(),p,128)!=0);check(calls[i]()==value);check(MH_CreateHook(p,reinterpret_cast<void*>(detours[i]),reinterpret_cast<void**>(&originals[i]))==MH_OK);check(MH_EnableHook(p)==MH_OK);check(calls[i]()==value+100);}
 using evr::diagnostic::HookDisableBatch;
 HookDisableBatch invalid;check(invalid.prepare({})!=MH_OK);check(invalid.prepare({nullptr})!=MH_OK);check(invalid.prepare({targets[0],targets[0]})!=MH_OK);check(!invalid.complete());
 check(invalid.prepare({targets[0],reinterpret_cast<void*>(1)})==MH_ERROR_NOT_CREATED);
 check(MH_ApplyQueued()==MH_OK);check(calls[0]()==110); // Failed queue rolled back.
 HookDisableBatch batch;check(batch.prepare({targets[0],targets[1],targets[2]})==MH_OK);check(batch.pending()==3&&!batch.complete());
 for(size_t i=0;i<3;++i)check(calls[i]()==int(i+1)*10);
 check(calls[3]()==140); // Foreign live hook (analogous to input owner) untouched.
 MH_STATUS foreign=MH_OK;std::thread t([&]{foreign=batch.disable(targets[0]);});t.join();check(foreign!=MH_OK&&batch.pending()==3);
 check(batch.prepare({targets[3]})!=MH_OK);check(calls[3]()==140);
 for(size_t i=0;i<3;++i){check(batch.disable(targets[i])==MH_OK);check(code[i*128]==0xb8&&code[i*128+5]==0xc3);check(batch.disable(targets[i])==MH_ERROR_DISABLED);}
 check(batch.complete()&&batch.pending()==0);check(batch.disable(targets[3])==MH_OK);check(calls[3]()==40);
 // A receipt is not permission to ignore someone re-enabling the hook.
 check(MH_EnableHook(targets[0])==MH_OK);HookDisableBatch conflict;check(conflict.prepare({targets[0]})==MH_OK);check(MH_EnableHook(targets[0])==MH_OK);check(conflict.disable(targets[0])==MH_ERROR_ENABLED);check(!conflict.complete());check(conflict.disable(targets[0])==MH_OK);check(conflict.complete());
 check(MH_Uninitialize()==MH_OK);check(VirtualFree(code,0,MEM_RELEASE)!=0);code=nullptr;
 std::cout<<"PASS "<<checks<<" real MinHook batch/receipt checks\n";return 0;
 }catch(const std::exception& e){MH_Uninitialize();if(code)VirtualFree(code,0,MEM_RELEASE);std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
