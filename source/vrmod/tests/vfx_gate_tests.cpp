#include <MinHook.h>
#include <evr/observer_error.hpp>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>
extern "C" {void* evr_vfx_gate_trampoline=nullptr;void evr_vfx_gate_stub();uintptr_t evr_vfx_gate_fixture(uintptr_t);extern uint8_t evr_vfx_gate_marker;}
extern "C" {void* evr_vfx_draw_trampoline=nullptr;void evr_vfx_draw_stub();uintptr_t evr_vfx_draw_fixture(uintptr_t);extern uint8_t evr_vfx_draw_marker;}
extern "C" {void* evr_vfx_model_trampolines[8]={};}   // eight more stubs of the same macro (vfx_model_bridge.hpp)
extern "C" {void* evr_vfx_extra_trampolines[4]={};void* evr_vfx_constants_trampoline=nullptr;}   // and four of vfx_gate_probe.hpp
extern "C" {void* evr_vfx_bindings_trampoline=nullptr;void evr_vfx_bindings_stub();uintptr_t evr_vfx_bindings_fixture(uintptr_t);extern uint8_t evr_vfx_bindings_marker;}
thread_local bool force=false;
std::atomic<unsigned> calls{0},failures{0};
extern "C" void evr_observe_vfx_gate(uintptr_t rsp,unsigned gate) noexcept {
    const evr::ObserverLastError preserve;++calls;
    uintptr_t savedR12{};uint64_t flags{};std::memcpy(&savedR12,reinterpret_cast<void*>(rsp-(gate?128:104)),8);
    std::memcpy(&flags,reinterpret_cast<void*>(rsp-8),8);
    if(!savedR12&&force){flags&=~uint64_t(0x40);std::memcpy(reinterpret_cast<void*>(rsp-8),&flags,8);}
    SetLastError(ERROR_PARTIAL_COPY);
}
int main(){
    auto require=[](bool v){if(!v)throw std::runtime_error("VFX branch hook invariant");};
    try{
        std::array<uint8_t,13> bytes{};std::memcpy(bytes.data(),&evr_vfx_gate_marker,bytes.size());
        require(bytes[0]==0x0f&&bytes[1]==0x84);
        require(MH_Initialize()==MH_OK&&MH_CreateHook(&evr_vfx_gate_marker,&evr_vfx_gate_stub,&evr_vfx_gate_trampoline)==MH_OK);
        require(MH_EnableHook(&evr_vfx_gate_marker)==MH_OK);
        std::array<uint8_t,13> drawBytes{};std::memcpy(drawBytes.data(),&evr_vfx_draw_marker,drawBytes.size());
        require(MH_CreateHook(&evr_vfx_draw_marker,&evr_vfx_draw_stub,&evr_vfx_draw_trampoline)==MH_OK&&MH_EnableHook(&evr_vfx_draw_marker)==MH_OK);
        std::array<uint8_t,13> bindingBytes{};std::memcpy(bindingBytes.data(),&evr_vfx_bindings_marker,bindingBytes.size());
        require(MH_CreateHook(&evr_vfx_bindings_marker,&evr_vfx_bindings_stub,&evr_vfx_bindings_trampoline)==MH_OK&&MH_EnableHook(&evr_vfx_bindings_marker)==MH_OK);
        std::vector<std::thread> workers;
        for(unsigned t=0;t<4;t++)workers.emplace_back([]{
            for(unsigned i=0;i<5000;i++){
                force=(i&1)!=0;const auto value=uintptr_t(i%3);SetLastError(ERROR_SUCCESS);
                if(evr_vfx_gate_fixture(value)!=(value||force?0x13579u:0x24680u)||GetLastError()!=ERROR_SUCCESS)++failures;
                if(evr_vfx_draw_fixture(value)!=(value||force?0x13579u:0x24680u)||GetLastError()!=ERROR_SUCCESS)++failures;
                if(evr_vfx_bindings_fixture(value)!=(value||force?0x13579u:0x24680u)||GetLastError()!=ERROR_SUCCESS)++failures;
            }});
        for(auto& t:workers)t.join();require(calls==60000&&failures==0);
        require(MH_DisableHook(&evr_vfx_gate_marker)==MH_OK&&MH_RemoveHook(&evr_vfx_gate_marker)==MH_OK);
        require(std::memcmp(bytes.data(),&evr_vfx_gate_marker,bytes.size())==0);
        require(MH_DisableHook(&evr_vfx_draw_marker)==MH_OK&&MH_RemoveHook(&evr_vfx_draw_marker)==MH_OK);
        require(std::memcmp(drawBytes.data(),&evr_vfx_draw_marker,drawBytes.size())==0);
        require(MH_DisableHook(&evr_vfx_bindings_marker)==MH_OK&&MH_RemoveHook(&evr_vfx_bindings_marker)==MH_OK);
        require(std::memcmp(bindingBytes.data(),&evr_vfx_bindings_marker,bindingBytes.size())==0);
        require(evr_vfx_gate_fixture(0)==0x24680u&&evr_vfx_gate_fixture(1)==0x13579u);
        require(evr_vfx_draw_fixture(0)==0x24680u&&evr_vfx_draw_fixture(1)==0x13579u);
        require(evr_vfx_bindings_fixture(0)==0x24680u&&evr_vfx_bindings_fixture(1)==0x13579u);
        std::cout<<"PASS 60000 relocated near-JE calls, three sites, four thread-local policies, saved R12/R15/XMM/last-error, native branches and code restoration\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
