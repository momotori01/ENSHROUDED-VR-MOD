#pragma once
// Finite event-only cancellation evidence. Values only; no native pointers
// are dereferenced here. The World flush adapter alone retires the MOD entry.
namespace blockedActionLifetime {
inline std::atomic<bool> requested{false};inline bool registered=false;inline std::mutex mutex;
inline motion::EventActionKey key{};inline motion::EventIdentity identity{};
inline motion::ContactLifetimePool<1> lifetimes;inline motion::ContactLifetimeToken token;
inline unsigned registrations=0,suppressions=0,colliders=0,retired=0;
inline std::atomic<unsigned> failures{0};
inline void failure(const char* why){if(failures.fetch_add(1)<3)logFile->event("blocked_action_lifetime_failure","\"reason\":"+jsonString(why));}
inline motion::EventActionKey snapshot(){std::lock_guard<std::mutex> lock(mutex);return registered?key:motion::EventActionKey{};}
template<size_t N> inline void block(const motion::EventRetimeLedger<N>& ledger,const motion::EventActionKey& k,const motion::EventIdentity& id){
 if(!requested)return;
 try{std::lock_guard<std::mutex> lock(mutex);
  if(registered)throw std::runtime_error("event-only registration still occupied");
  token=lifetimes.acquireBlocked(ledger,k,id);if(!token)throw std::runtime_error("event-only registration lacks unique blocked identity");
  registered=true;key=k;identity=id;++registrations;
  std::ostringstream o;o<<"\"watch_slot\":"<<token.slot<<",\"watch_serial\":"<<token.serial<<",\"world\":"<<hex(k.world)<<",\"actor\":"<<k.entity<<",\"sequence\":"<<k.sequence<<",\"item\":"<<k.item<<",\"trigger\":"<<k.trigger<<",\"call\":"<<unsigned(k.call)<<",\"sub\":"<<unsigned(k.sub)<<",\"variant\":"<<hex(id.variant)<<",\"blocked_ledger_verified\":true,\"writes_gameplay\":false";
  logFile->event("blocked_action_lifetime_watch",o.str());
 }catch(const std::exception& e){failure(e.what());}
}
inline void suppressed(const motion::EventActionKey& k,const motion::EventIdentity& id){
 if(!requested)return;
 try{std::lock_guard<std::mutex> lock(mutex);if(!registered||!(key==k))return;
  if(!lifetimes.suppressed(token,k,id))throw std::runtime_error("event-only suppression identity differs");++suppressions;
  logFile->event("blocked_action_lifetime_suppressed","\"world\":"+hex(k.world)+",\"actor\":"+std::to_string(k.entity)+",\"trigger\":"+std::to_string(k.trigger)+",\"variant\":"+hex(id.variant)+",\"writes_gameplay\":false");
 }catch(const std::exception& e){failure(e.what());}
}
inline void collider(uintptr_t world,uint32_t entity,const std::array<uint8_t,24>& event){
 if(!requested)return;
 try{std::lock_guard<std::mutex> lock(mutex);if(!registered||world!=key.world||(combatEpochGate::requested&&!(key.epoch==combatEpochGate::serverStamp(world))))return;
  uint32_t sequence=0;std::memcpy(&sequence,event.data(),4);
  if(sequence!=key.sequence||event[4]!=key.sub||std::memcmp(event.data()+8,identity.guid.data(),16))return;
  // Conservatively reject any matching asset event in this finite World trial,
  // before recording quotas or local actor filters can hide it.
  ++colliders;(void)lifetimes.unexpectedImpact(token,key);
  logFile->event("blocked_action_unexpected_collider","\"world\":"+hex(world)+",\"entity\":"+std::to_string(entity)+",\"actor\":"+std::to_string(key.entity)+",\"retires_ledger_entries\":false");
 }catch(const std::exception& e){failure(e.what());}
}
}
