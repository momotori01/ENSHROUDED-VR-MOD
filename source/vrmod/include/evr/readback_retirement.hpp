#pragma once
#include <array>
#include <cstdint>
namespace evr::water {
struct RetirementSlot {uint64_t inFlight=0,ready=0;};
struct RetirementEvidence {
 bool ownerStopped=false,sceneReturned=false,sameThread=false;
 uint64_t completed=0,pending=0,capacity=0;
 std::array<RetirementSlot,8> slots{};
};
// Discard private feedback only after its native completion serial. Never apply
// to shared gameplay data or use this predicate to advance an engine counter.
inline bool permitsPrivateDiscard(const RetirementEvidence& e){
 if(!e.ownerStopped||!e.sceneReturned||!e.sameThread||!e.completed||!e.capacity||e.capacity>e.slots.size()||e.pending>e.capacity)return false;
 uint64_t occupied=0;
 for(uint64_t i=0;i<e.capacity;++i){
  if(e.slots[i].inFlight>e.completed||e.slots[i].ready>e.completed)return false;
  occupied+=bool(e.slots[i].inFlight||e.slots[i].ready);
 }
 return occupied==e.pending;
}
}
