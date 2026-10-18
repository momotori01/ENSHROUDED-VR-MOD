#include <evr/world_epoch_abi.hpp>
#include <windows.h>
// Compiled twice as separate DLLs. Neither one owns a World book.
extern "C" __declspec(dllexport) uint32_t evrTestEpochCall(const evr::epoch::Request* r,evr::epoch::Response* out){
 const auto saved=GetLastError();const auto provider=GetModuleHandleW(L"evr_diagnostic.dll");
 const auto fn=provider?reinterpret_cast<evr::epoch::Call>(GetProcAddress(provider,"evrWorldEpochCall")):nullptr;
 SetLastError(saved);return fn?fn(r,out):uint32_t(evr::epoch::Status::Unavailable);
}
