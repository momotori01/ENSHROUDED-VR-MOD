#pragma once
#include <evr/motion_contact_retirement.hpp>
#include <evr/world_end_boundary.hpp>
#include <limits>
namespace evr::motion {
// Token scope is one non-copyable pool instance. Reconstruct it only after the
// native adapter has drained all callbacks; this is not a native World epoch.
struct ContactLifetimeToken {
 size_t slot=0;uint64_t serial=0;
 explicit operator bool()const{return serial!=0;}
 bool operator==(const ContactLifetimeToken& b)const{return slot==b.slot&&serial==b.serial;}
};
enum class ContactLifetimeKind {Collider,BlockedEvent};
struct ContactLifetimeSnapshot {
 ContactLifetimeToken token{};EventAuthority authority{};ColliderKey collider{};
 ContactLifetimeKind kind=ContactLifetimeKind::Collider;
 bool used=false,destroying=false,ended=false;
};
// Caller serializes every operation. All callback completion and retirement
// operations require the saved token, never a freshly looked-up slot index.
// Normal and post-spawn-cancelled colliders share the same native-end proof;
// a pre-spawn blocked action additionally requires its real late suppression.
template<size_t Capacity=8> class ContactLifetimePool {
 struct Entry {ContactLifetimeSnapshot state{};ContactRetirementGate gate{};bool permit=false;};
 std::array<Entry,Capacity> entries_{};size_t limit_=Capacity;uint64_t serial_=0;
 Entry* get(ContactLifetimeToken t){return t&&t.slot<limit_&&entries_[t.slot].state.used&&entries_[t.slot].state.token==t?&entries_[t.slot]:nullptr;}
 ContactLifetimeToken insert(const EventAuthority& a,const ColliderKey& k,ContactLifetimeKind kind,const ContactRetirementGate& gate){
  if(serial_==std::numeric_limits<uint64_t>::max())return {}; // never wrap into a stale token
  for(const auto& e:entries_)if(e.state.used&&(e.state.authority.action==a.action||(k.valid()&&e.state.collider.valid()&&e.state.collider.world==k.world&&e.state.collider.entity==k.entity)))return {};
  for(size_t i=0;i<limit_;++i)if(!entries_[i].state.used){auto& e=entries_[i];e={};e.state={{i,++serial_},a,k,kind,true,false,false};e.gate=gate;return e.state.token;}
  return {};
 }
public:
 ContactLifetimePool()=default;
 ContactLifetimePool(const ContactLifetimePool&)=delete;ContactLifetimePool& operator=(const ContactLifetimePool&)=delete;
 bool configureLimit(size_t n){if(serial_||!n||n>Capacity)return false;limit_=n;return true;}
 size_t size()const{size_t n=0;for(const auto& e:entries_)n+=e.state.used;return n;}
 size_t limit()const{return limit_;}
 uint64_t issued()const{return serial_;}
 ContactLifetimeSnapshot snapshot(ContactLifetimeToken t)const{return t&&t.slot<limit_&&entries_[t.slot].state.used&&entries_[t.slot].state.token==t?entries_[t.slot].state:ContactLifetimeSnapshot{};}
 ContactLifetimeToken acquire(const EventAuthority& a,const ColliderKey& k){ContactRetirementGate g;if(!g.initialize(a,k))return {};return insert(a,k,ContactLifetimeKind::Collider,g);}
 template<size_t N> ContactLifetimeToken acquireBlocked(const EventRetimeLedger<N>& ledger,const EventActionKey& key,const EventIdentity& id){
  ContactRetirementGate g;if(!g.initializeBlocked(ledger,key,id))return {};return insert({key,id.guid,true},{},ContactLifetimeKind::BlockedEvent,g);
 }
 bool suppressed(ContactLifetimeToken t,const EventActionKey& key,const EventIdentity& id){auto* e=get(t);if(!e)return false;e->permit=false;return e->gate.suppressed(key,id);}
 bool unexpectedImpact(ContactLifetimeToken t,const EventActionKey& key){auto* e=get(t);if(!e)return false;e->permit=false;return e->gate.impactCreated(key);}
 bool beginDestroy(ContactLifetimeToken t,const ColliderKey& key){auto* e=get(t);if(!e||e->state.kind!=ContactLifetimeKind::Collider||!(e->state.collider==key)||e->state.destroying||e->state.ended)return false;e->permit=false;e->state.destroying=true;return true;}
 bool finishDestroy(ContactLifetimeToken t,const ColliderKey& key,bool absent){auto* e=get(t);if(!e||e->state.kind!=ContactLifetimeKind::Collider||!(e->state.collider==key)||!e->state.destroying||e->state.ended)return false;e->permit=false;e->state.destroying=false;e->state.ended=absent;return true;}
 bool ready(ContactLifetimeToken t,const ContactBoundary& b){auto* e=get(t);if(!e)return false;return e->permit=e->gate.ready(b,e->state.kind==ContactLifetimeKind::BlockedEvent||(e->state.ended&&!e->state.destroying));}
 // Whole-World destruction is a distinct terminal proof: no Actor read, late
 // authored event, or individual Impact destructor is required after its exact
 // native root ended. A fault or unfinished observer callback still retains data.
 bool canRetireEnded(ContactLifetimeToken t,const WorldEndBoundary& b)const {
  const auto s=snapshot(t);
  return s.used&&!s.destroying&&!faulted(t)&&s.authority.motion&&s.authority.action.valid()&&
   b.permits(s.authority.action.epoch)&&
   (s.kind==ContactLifetimeKind::BlockedEvent||
    (s.collider.valid()&&s.collider.epoch==s.authority.action.epoch));
 }
 bool retireEnded(ContactLifetimeToken t,const WorldEndBoundary& b){
  if(!canRetireEnded(t,b))return false;*get(t)={};return true;
 }
 bool faulted(ContactLifetimeToken t)const{return t&&t.slot<limit_&&entries_[t.slot].state.used&&entries_[t.slot].state.token==t&&entries_[t.slot].gate.faulted();}
 // Caller holds event/collider/pool locks and rechecks native absence, callback
 // counts and exact ledger ownership, then retires MOD ledgers and this slot.
 bool retire(ContactLifetimeToken t){auto* e=get(t);if(!e||!e->permit||!e->gate.commit())return false;*e={};return true;}
};
}
