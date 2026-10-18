#pragma once
#include <evr/present_retirement_abi.hpp>
namespace presentRetirement {
// One-shot finite diagnostic slot; cannot be replaced while native work owns it.
inline std::atomic<bool> registered{false};
inline std::atomic<diagnostic::PresentRetirementCallback> callback{nullptr};
inline void* context=nullptr;inline DWORD thread=0;inline uint64_t polls=0;
inline uint32_t install(const diagnostic::PresentRetirementRequest* r) noexcept {
 const ObserverLastError preserve;
 if(!r||r->size!=sizeof(*r)||r->version!=1||r->reserved||!r->callback||!r->context||r->thread!=GetCurrentThreadId())return 1;
 bool empty=false;if(!registered.compare_exchange_strong(empty,true))return 2;
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(r->callback),&pinned))return 3;
 context=r->context;thread=r->thread;
 note("present_retirement_registered","\"thread\":"+std::to_string(thread)+",\"callback_module_pinned\":true,\"capacity\":1,\"finite_diagnostic\":true");
 callback.store(r->callback,std::memory_order_release);return 0;
}
inline void pump(uint32_t event) noexcept {
 const auto fn=callback.load(std::memory_order_acquire);if(!fn)return;
 // Only this registered thread may invoke or mutate the callback state.
 if(GetCurrentThreadId()!=thread)return;
 const ObserverLastError preserve;const auto n=++polls;uint32_t result=2;
 try{result=fn(context,event);}catch(...){}
 if(n<=8||result||event)note("present_retirement_poll","\"poll\":"+std::to_string(n)+",\"thread\":"+std::to_string(thread)+",\"device_ending\":"+(event?"true":"false")+",\"result\":"+std::to_string(result));
 if(result||event)callback.store(nullptr,std::memory_order_release);
}
}
