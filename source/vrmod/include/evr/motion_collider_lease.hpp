#pragma once
#include <evr/combat_worlds.hpp>
#include <array>
#include <cstdint>
#include <cstddef>
namespace evr::motion {
struct ColliderKey {
 uint64_t world=0;uint32_t entity=0;std::array<uint8_t,24> event{};WorldStamp epoch{};
 bool operator==(const ColliderKey& b)const{return world==b.world&&entity==b.entity&&event==b.event&&epoch==b.epoch;}
 bool valid()const{return world&&entity&&(epoch.empty()||(epoch.valid()&&epoch.world==world&&epoch.kind==0));}
};
enum class ColliderDecision {Ignore,Track,Cancel};
// A one-attack lease cannot acquire an existing animated collider or migrate
// to another World/entity/event. Cancellation stays latched for this lifetime.
class ColliderLease {
 bool active_=false,cancelled_=false;ColliderKey key_{};uint64_t id_=0;
public:
 bool owns(const ColliderKey& k)const{return active_&&k==key_;}
 ColliderDecision decide(const ColliderKey& k,bool eligible,uint64_t nativeId,bool ownedIntent=false){
  if(!k.valid())return ColliderDecision::Ignore;
  if(!active_){if((!eligible&&!ownedIntent)||nativeId)return ColliderDecision::Ignore;key_=k;active_=true;}
  if(!owns(k))return ColliderDecision::Ignore;
  if(!eligible||nativeId!=id_)cancelled_=true;
  return cancelled_?ColliderDecision::Cancel:ColliderDecision::Track;
 }
 void returned(uint64_t nativeId){id_=nativeId;}
 bool cancelled()const{return cancelled_;}
};
// A separate lease per native Impact entity/event. Cancellation remains latched
// for older keys while a later attack acquires a new zero-ID native collider.
template<size_t Capacity=8> class ColliderLeaseLedger {
 struct Entry{ColliderKey key{};ColliderLease value{};bool used=false;};
 std::array<Entry,Capacity> entries_{};size_t limit_=Capacity;bool started_=false;
public:
 size_t size()const{size_t n=0;for(const auto& e:entries_)n+=e.used;return n;}
 size_t limit()const{return limit_;}
 bool configureLimit(size_t n){if(started_||!n||n>Capacity)return false;limit_=n;return true;}
 // Caller must hold the adapter locks and an independently verified native
 // quiescent lifetime boundary. Never retire from a timer or log budget.
 bool retireAfterQuiescence(const ColliderKey& key){for(auto& e:entries_)if(e.used&&e.key==key){e={};return true;}return false;}
 bool owns(const ColliderKey& key)const{for(const auto& e:entries_)if(e.used&&e.key==key)return true;return false;}
 ColliderDecision decide(const ColliderKey& key,bool eligible,uint64_t nativeId,bool ownedIntent=false){
  for(auto& e:entries_)if(e.used&&e.key==key)return e.value.decide(key,eligible,nativeId,ownedIntent);
  for(size_t i=0;i<limit_;++i){auto& e=entries_[i];if(e.used)continue;const auto d=e.value.decide(key,eligible,nativeId,ownedIntent);if(d!=ColliderDecision::Ignore){e.key=key;e.used=true;started_=true;}return d;}
  return ColliderDecision::Ignore;
 }
 bool returned(const ColliderKey& key,uint64_t nativeId){for(auto& e:entries_)if(e.used&&e.key==key){e.value.returned(nativeId);return true;}return false;}
};

}
