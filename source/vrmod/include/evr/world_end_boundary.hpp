#pragma once
#include <evr/combat_worlds.hpp>
namespace evr::motion {
// VALUE evidence only. Native adapter must read the retained broker lease AFTER
// the original server-root destructor returned, stop admission to its consumers,
// drain their callbacks, and hold all action-ledger locks through the transaction.
// Neither a zero callback sample nor this struct pins any native/ECS allocation.
struct WorldEndBoundary {
 epoch::Response observed{};WorldStamp owner{};uint64_t leaseHandle=0;
 bool consumersStopped=false;
 uint64_t selectors=0,queries=0,destructors=0,flushes=0;
 bool permits(const WorldStamp& saved)const {
  return saved.valid()&&saved.kind==0&&owner==saved&&worldStamp(observed)==saved&&
   observed.phase==uint32_t(epoch::Phase::Ended)&&observed.handle==0&&
   observed.pins&&leaseHandle&&
   (observed.flags==epoch::flagActive||observed.flags==(epoch::flagActive|epoch::flagAdmissionClosed))&&
   consumersStopped&&!selectors&&!queries&&!destructors&&!flushes;
 }
};
}
