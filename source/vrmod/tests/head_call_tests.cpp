#include <MinHook.h>
#include <evr/head_visibility.hpp>
#include <evr/observer_stack.hpp>
#include <evr/observer_error.hpp>
#include <atomic>
#include <iostream>
#include <thread>
#include <windows.h>
extern "C" {void* evr_camera_trampoline=nullptr;void evr_camera_stub();void* evr_head_copy_fixture(void*,const void*,size_t);extern uint8_t evr_head_copy_marker;}
std::atomic<unsigned> copies{0},hooks{0},bad{0};
thread_local void* expectedSource=nullptr;
const auto payload=evr::head::pack({0,17,40000,65535});
extern "C" void* evr_fixture_copy(void* destination,const void* source,size_t size){++copies;return std::memcpy(destination,source,size);}
extern "C" void evr_observe_camera(uintptr_t rsp) noexcept {
    const evr::ObserverLastError error;const auto args=evr::observerEntryArguments(rsp);
    if(args[1]!=reinterpret_cast<uintptr_t>(expectedSource)||args[2]!=evr::head::constantBytes){++bad;return;}
    std::memcpy(reinterpret_cast<void*>(args[1]+evr::head::offset),payload.data(),sizeof(payload));++hooks;
    SetLastError(ERROR_PARTIAL_COPY);
}
int main(){try{
    auto require=[](bool value){if(!value)throw std::runtime_error("head memcpy callsite invariant");};
    std::array<uint8_t,5> before{};std::memcpy(before.data(),&evr_head_copy_marker,5);require(before[0]==0xe8);
    require(MH_Initialize()==MH_OK);require(MH_CreateHook(&evr_head_copy_marker,&evr_camera_stub,&evr_camera_trampoline)==MH_OK);
    require(MH_EnableHook(&evr_head_copy_marker)==MH_OK);
    std::vector<std::thread> workers;
    for(unsigned t=0;t<4;t++)workers.emplace_back([]{
        std::array<uint8_t,evr::head::constantBytes> source{},destination{},expected{};source.fill(0x42);expected=source;
        std::memcpy(expected.data()+evr::head::offset,payload.data(),sizeof(payload));expectedSource=source.data();
        for(unsigned i=0;i<5000;i++){
            source.fill(0x42);SetLastError(ERROR_SUCCESS);
            if(evr_head_copy_fixture(destination.data(),source.data(),source.size())!=destination.data()||destination!=expected||source!=expected||GetLastError()!=ERROR_SUCCESS)++bad;
        }});
    for(auto& thread:workers)thread.join();require(hooks==20000&&copies==20000&&bad==0);
    require(MH_DisableHook(&evr_head_copy_marker)==MH_OK);require(MH_RemoveHook(&evr_head_copy_marker)==MH_OK);
    require(std::memcmp(before.data(),&evr_head_copy_marker,5)==0);
    std::array<uint8_t,32> source{},destination{};source.fill(0x18);
    require(evr_head_copy_fixture(destination.data(),source.data(),source.size())==destination.data()&&source==destination);
    std::cout<<"PASS 20000 relocated CALL copies, four threads, private 12-byte payload only, return value/last-error and code restoration\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
