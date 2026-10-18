#include <MinHook.h>
#include <evr/observer_error.hpp>
#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>
extern "C" {void* evr_ui_state_trampoline=nullptr;void evr_ui_state_stub();unsigned evr_ui_dispatch_fixture(void*);extern unsigned char evr_ui_state_marker;}
std::atomic<unsigned> hits{0},bad{0};thread_local const void* expected=nullptr;
extern "C" void evr_observe_ui_state(uintptr_t rsp) noexcept {
 const evr::ObserverLastError error;uintptr_t source=0;std::memcpy(&source,reinterpret_cast<void*>(rsp-120),8);
 if(source!=reinterpret_cast<uintptr_t>(expected))++bad;++hits;SetLastError(ERROR_PARTIAL_COPY);
}
int main(){try{
 auto require=[](bool b){if(!b)throw std::runtime_error("UI relocated dispatch/observer invariant");};
 std::array<unsigned char,5> before{};std::memcpy(before.data(),&evr_ui_state_marker,5);
 require(before==std::array<unsigned char,5>{0x41,0x0f,0xb6,0x4e,0x0c});
 require(MH_Initialize()==MH_OK);require(MH_CreateHook(&evr_ui_state_marker,&evr_ui_state_stub,&evr_ui_state_trampoline)==MH_OK);require(MH_EnableHook(&evr_ui_state_marker)==MH_OK);
 std::vector<std::thread> workers;
 for(unsigned t=0;t<4;t++)workers.emplace_back([]{std::array<unsigned char,16> data{};expected=data.data();for(unsigned i=0;i<4096;i++){
  unsigned char state=i%256;data[12]=state;const auto copy=data;SetLastError(ERROR_SUCCESS);
  const unsigned want=state>=1&&state<=5?11*state:999;
  if(evr_ui_dispatch_fixture(data.data())!=want||data!=copy||GetLastError()!=ERROR_SUCCESS)++bad;
 }});
 for(auto& t:workers)t.join();require(hits==16384&&bad==0);require(MH_DisableHook(&evr_ui_state_marker)==MH_OK);
 require(std::memcmp(&evr_ui_state_marker,before.data(),5)==0);std::array<unsigned char,16> d{};d[12]=4;require(evr_ui_dispatch_fixture(d.data())==44&&hits==16384);
 std::cout<<"PASS 16384 calls: all 256 state bytes, four threads, R14 saved register, relocated MOVZX, unchanged data/result/last-error and restored bytes\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
