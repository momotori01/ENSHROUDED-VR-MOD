#pragma once
#include <evr/motion_contact_lifetime.hpp>
namespace evr::motion {
enum class WorldRetirementResult {Retired,InvalidBoundary,StaleToken,Faulted,OwnershipMismatch};
// Caller holds all three ledger locks, keeps the exact VALUE lease, and excludes
// new callbacks for this owner until completion. Preflight performs no mutations;
// the following synchronous value-only erases cannot fail after these checks.
// This path never calls game code, accesses an Actor, or frees a native collider.
template<size_t P,size_t E,size_t C>
WorldRetirementResult retireContactAfterWorldEnd(ContactLifetimePool<P>& pool,
 ContactLifetimeToken token,EventRetimeLedger<E>& events,ColliderLeaseLedger<C>& colliders,
 const WorldEndBoundary& boundary){
 const auto owned=pool.snapshot(token);
 if(!owned.used)return WorldRetirementResult::StaleToken;
 if(!boundary.permits(owned.authority.action.epoch))return WorldRetirementResult::InvalidBoundary;
 if(pool.faulted(token))return WorldRetirementResult::Faulted;
 const bool hasCollider=owned.kind==ContactLifetimeKind::Collider;
 if(!pool.canRetireEnded(token,boundary)||!events.owns(owned.authority.action)||
    (hasCollider?!colliders.owns(owned.collider):!events.blocked(owned.authority.action)))
  return WorldRetirementResult::OwnershipMismatch;
 events.retireAfterQuiescence(owned.authority.action);
 if(hasCollider)colliders.retireAfterQuiescence(owned.collider);
 pool.retireEnded(token,boundary);
 return WorldRetirementResult::Retired;
}
}
