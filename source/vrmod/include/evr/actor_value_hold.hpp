#pragma once
#include <array>
#include <cstdint>
namespace evr::motion {
// Only permits retaining MOD values after the original Actor callback returns.
// It cannot retire a ledger, dereference an old Actor, or prove a native drain.
// Other entity destructors can overlap. Actual erasure still requires the exact
// WorldEndBoundary, stopped hooks and ALL destruction callbacks at zero.
struct ActorValueHoldBoundary {
 std::array<uint64_t,3> before{},after{};
 bool stable=false,admissionStopped=false;
 uint64_t selectors=0,queries=0,lookups=0,flushes=0,destructors=0;
 bool permitsHold()const noexcept {
  return stable&&admissionStopped&&before==after&&before==std::array<uint64_t,3>{}&&
   !selectors&&!queries&&!lookups&&!flushes&&destructors>=1;
 }
};
}
