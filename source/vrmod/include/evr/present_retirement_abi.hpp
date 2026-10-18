#pragma once
#include <cstdint>
#include <type_traits>
namespace evr::diagnostic {
// Finite diagnostic callback. No native resources or C++ ownership cross this ABI.
// event=0: later native present on the registered thread; event=1: device ending.
// callback return: 0 pending, 1 completed, 2 failed (retain owned storage).
using PresentRetirementCallback=uint32_t(*)(void*,uint32_t);
struct PresentRetirementRequest {
 uint32_t size=sizeof(PresentRetirementRequest),version=1,thread=0,reserved=0;
 PresentRetirementCallback callback=nullptr;void* context=nullptr;
};
static_assert(sizeof(PresentRetirementRequest)==32&&std::is_trivially_copyable_v<PresentRetirementRequest>);
using RegisterPresentRetirement=uint32_t(*)(const PresentRetirementRequest*);
}
