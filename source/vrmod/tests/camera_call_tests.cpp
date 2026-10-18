#include <MinHook.h>
#include <evr/observer_error.hpp>
#include <intrin.h>
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
extern "C" {
void evr_camera_call_fixture(uintptr_t*,const uintptr_t*,uintptr_t);
void evr_camera_build_fixture(uintptr_t*,const uintptr_t*);
extern uint8_t evr_camera_fixture_return;
}
using Build=void(__fastcall*)(uintptr_t*,const uintptr_t*);
Build original=nullptr;
std::atomic<unsigned> hits{0},bad{0};
__declspec(noinline) void __fastcall observe(uintptr_t* out,const uintptr_t* controller){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto rsp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+sizeof(uintptr_t);
    original(out,controller);
    const evr::ObserverLastError lastError;
    SetLastError(ERROR_PARTIAL_COPY);
    if(caller!=reinterpret_cast<uintptr_t>(&evr_camera_fixture_return)||
       *reinterpret_cast<const uintptr_t*>(rsp+0x50)!=*controller||
       *reinterpret_cast<const uintptr_t*>(rsp+0x78)!=reinterpret_cast<uintptr_t>(controller)||
       *reinterpret_cast<const uintptr_t*>(rsp+0x80)!=reinterpret_cast<uintptr_t>(out)||*out!=*controller)++bad;
    ++hits;
}
int main(){
    auto target=reinterpret_cast<void*>(&evr_camera_build_fixture);
    if(MH_Initialize()!=MH_OK||MH_CreateHook(target,reinterpret_cast<void*>(&observe),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(target)!=MH_OK)return 2;
    std::vector<std::thread> threads;
    for(unsigned t=0;t<4;t++)threads.emplace_back([t]{for(unsigned n=0;n<10000;n++){
        uintptr_t out=0,value=(uintptr_t(t)<<32)|n;SetLastError(1234);
        evr_camera_call_fixture(&out,&value,value);
        if(out!=value||GetLastError()!=1234)++bad;
    }});
    for(auto& thread:threads)thread.join();
    if(MH_DisableHook(target)!=MH_OK)return 3;
    uintptr_t out=0,value=567;evr_camera_call_fixture(&out,&value,value);
    if(out!=value||hits!=40000)++bad;
    std::cout<<(bad?"FAIL":"PASS")<<" camera caller frame: "<<hits<<" callbacks, "<<bad<<" errors; original restored\n";
    return bad?1:0;
}
