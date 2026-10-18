#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace evr::motion {
// Value-only identities. Adapter verifies native layouts/callers; pointers are
// never dereferenced here. One book belongs to one MOD session, and is not copied.
struct WorldIdentity {
 uintptr_t world=0,registry=0,root=0;uint8_t kind=255;
 bool valid()const{return world&&registry&&root&&kind<=2;}
 bool operator==(const WorldIdentity& b)const{return world==b.world&&registry==b.registry&&root==b.root&&kind==b.kind;}
};
struct WorldToken {
 size_t slot=0;uint64_t serial=0;
 explicit operator bool()const{return serial!=0;}
 bool operator==(const WorldToken& b)const{return slot==b.slot&&serial==b.serial;}
};
enum class WorldProvenance {Adopted,NativeCreation};
enum class WorldPhase {Empty,Alive,Closing,Ended};
struct WorldLifetime {
 WorldToken token{};WorldIdentity identity{};WorldProvenance provenance=WorldProvenance::Adopted;
 WorldPhase phase=WorldPhase::Empty;uint32_t pins=0;
};
// Caller holds one mutex for ALL methods and checks token validity while using
// MOD state. Pins prevent reuse of this value slot only, never native ECS free
// or GPU-resource release. A token is not permission to dereference freed ECS.
template<size_t Capacity=8> class WorldLifetimes {
 static_assert(Capacity>0,"nonzero World storage");
 std::array<WorldLifetime,Capacity> entries_{};uint64_t serial_=0;bool faulted_=false,adoptionSealed_=false;
 WorldLifetime* get(WorldToken t){return t&&t.slot<Capacity&&entries_[t.slot].phase!=WorldPhase::Empty&&entries_[t.slot].token==t?&entries_[t.slot]:nullptr;}
 void fault(){faulted_=true;for(auto& e:entries_)if(e.phase==WorldPhase::Alive)e.phase=WorldPhase::Closing;}
 WorldToken insert(WorldIdentity id,WorldProvenance provenance){
  if(faulted_||!id.valid()||serial_==std::numeric_limits<uint64_t>::max())return {};
  for(auto& e:entries_)if(e.phase==WorldPhase::Alive||e.phase==WorldPhase::Closing){
   if(e.identity.world==id.world){
    if(provenance==WorldProvenance::Adopted&&e.phase==WorldPhase::Alive&&e.identity==id)return e.token;
    // Closing cannot be resurrected by another observation; a new native
    // creation before observed completion is a lifecycle coverage failure.
    if(provenance==WorldProvenance::NativeCreation||!(e.identity==id))fault();return {};
   }
   if(e.identity.root==id.root&&e.phase==WorldPhase::Closing){if(provenance==WorldProvenance::NativeCreation)fault();return {};}
   if(e.identity.root==id.root&&(e.identity.registry!=id.registry||e.identity.kind==id.kind||e.identity.kind==0||id.kind==0)){fault();return {};}
  }
  if(provenance==WorldProvenance::Adopted&&adoptionSealed_)return {};
  for(size_t i=0;i<Capacity;++i)if(entries_[i].phase==WorldPhase::Empty){entries_[i]={{i,++serial_},id,provenance,WorldPhase::Alive,0};return entries_[i].token;}
  return {};
 }
public:
 WorldLifetimes()=default;WorldLifetimes(const WorldLifetimes&)=delete;WorldLifetimes& operator=(const WorldLifetimes&)=delete;
 // Seal after the initial verified World inventory. Later raw snapshots may
 // resolve an existing live identity, but cannot resurrect an ended World.
 void sealAdoption(){adoptionSealed_=true;}
 bool adoptionSealed()const{return adoptionSealed_;}
 WorldToken adopt(WorldIdentity id){return insert(id,WorldProvenance::Adopted);}
 WorldToken created(WorldIdentity id){return insert(id,WorldProvenance::NativeCreation);}
 bool faulted()const{return faulted_;}
 uint64_t issued()const{return serial_;}
 size_t size()const{size_t n=0;for(const auto& e:entries_)n+=e.phase!=WorldPhase::Empty;return n;}
 WorldLifetime snapshot(WorldToken t)const{return t&&t.slot<Capacity&&entries_[t.slot].phase!=WorldPhase::Empty&&entries_[t.slot].token==t?entries_[t.slot]:WorldLifetime{};}
 bool alive(WorldToken t)const{return !faulted_&&snapshot(t).phase==WorldPhase::Alive;}
 bool pin(WorldToken t){auto* e=get(t);if(!e||!alive(t)||e->pins==std::numeric_limits<uint32_t>::max())return false;++e->pins;return true;}
 bool unpin(WorldToken t){auto* e=get(t);if(!e||!e->pins)return false;--e->pins;return true;}
 // Saved tokens, not a later lookup by raw pointer, must cross native calls.
 bool beginClose(WorldToken t){auto* e=get(t);if(!e||e->phase==WorldPhase::Ended)return false;e->phase=WorldPhase::Closing;return true;}
 struct Batch {std::array<WorldToken,Capacity> tokens{};size_t count=0;};
 Batch beginRootClose(uintptr_t root){Batch result;if(!root)return result;for(auto& e:entries_)if(e.identity.root==root&&(e.phase==WorldPhase::Alive||e.phase==WorldPhase::Closing)){e.phase=WorldPhase::Closing;result.tokens[result.count++]=e.token;}return result;}
 bool finishClose(WorldToken t){auto* e=get(t);if(!e||e->phase!=WorldPhase::Closing)return false;e->phase=WorldPhase::Ended;return true;}
 bool reclaim(WorldToken t){auto* e=get(t);if(!e||e->phase!=WorldPhase::Ended||e->pins)return false;*e={};return true;}
};
}
