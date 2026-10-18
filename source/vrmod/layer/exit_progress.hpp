#pragma once
// Finite opt-in timing evidence only. A Vulkan return is not a GPU fence.
namespace exitProgress {
inline std::atomic<uint64_t> deadline{0},sequence{0};
inline bool armed(){const auto d=deadline.load(std::memory_order_acquire);return d&&GetTickCount64()<d;}
inline uint32_t arm(uint32_t milliseconds) noexcept {
 const ObserverLastError preserve;
 if(milliseconds!=5000)return 1;
 uint64_t empty=0;const auto end=GetTickCount64()+milliseconds;
 if(!deadline.compare_exchange_strong(empty,end))return 2;
 note("exit_progress_armed","\"duration_ms\":5000,\"event_budget\":8192,\"read_only\":true,\"gpu_completion_proven\":false");return 0;
}
struct Scope {
 const char* operation;uint64_t object=0,id=0,started=0;
 Scope(const char* name,uint64_t value):operation(name),object(value){
  if(!armed())return;
  const ObserverLastError preserve;id=++sequence;
  if(id>8192){if(id==8193)note("exit_progress_overflow");id=0;return;}
  started=GetTickCount64();write("begin",0);
 }
 void write(const char* phase,int64_t result) noexcept {
  try{note("exit_progress_gpu_call","\"call\":"+std::to_string(id)+",\"operation\":"+jsonString(operation)+",\"phase\":"+jsonString(phase)+",\"object\":"+std::to_string(object)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"result\":"+std::to_string(result)+",\"elapsed_ms\":"+std::to_string(GetTickCount64()-started)+",\"gpu_completion_proven\":false");}catch(...){}
 }
 void end(int64_t result){if(id){const ObserverLastError preserve;write("end",result);}}
};
}
