#pragma once
#include <atomic>
#include <cstdint>
namespace evr::diagnostic {
// One request by default; an explicit manual budget allows at most four.
// Completed requests retain the session budget. Frontend publishes counters; the render
// thread performs readback after native completion. No pointers cross threads.
class CaptureBurst {
 std::atomic<uint64_t> state_{0};
public:
 struct Frame {uint32_t index=0,total=0;explicit operator bool()const{return index!=0;}};
 int request(uint32_t count,uint32_t requestLimit=1){
  if(count<1||count>32||requestLimit<1||requestLimit>4)return 2;
  auto value=state_.load();for(;;){
   if((value&65535)||(value>>32)>=requestLimit)return 1;
   const uint64_t next=(((value>>32)+1)<<32)|(uint64_t(count)<<16)|count;
   if(state_.compare_exchange_weak(value,next))return 0;
  }
 }
 Frame take(){auto value=state_.load();while(value&65535){if(state_.compare_exchange_weak(value,value-1))return {uint32_t((value>>16)&65535)-uint32_t(value&65535)+1,uint32_t((value>>16)&65535)};}return {};}
 uint32_t reset(){return uint32_t(state_.exchange(0)&65535);}
 bool pending()const{return (state_.load()&65535)!=0;}
};
}
