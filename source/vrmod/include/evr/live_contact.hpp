#pragma once
#include <evr/motion_collider_lease.hpp>
#include <evr/motion_event_retime.hpp>
namespace evr::motion {
// Unlimited play (2026-10-11, real-device feedback "hit with the weapon in the hand, as VHVR does; the game's
// animation may only set the cooldown"). The finite contact trials of 10-04/05 keep their own ledgers of eight
// entries that are never retired from a timer. Live play cannot keep every attack for ever; its records are
// plain last-seen stamps and expire by time. Nothing here authorises an attack: the hooks apply these records only
// to an Impact that the native ownership checks have already tied to the local player's weapon.

// The game's attack collider follows the tracked weapon. Each native update needs the pose it had on the previous
// update (the game sweeps from there); a collider seen again within `continuityMs` continues its own sweep, any
// other starts from where the weapon is now. Entries that have not been seen for `forgetMs` are free again; when
// all are in use the one seen longest ago is taken.
template<size_t Capacity=16> class LiveColliderLedger {
 struct Entry{ColliderKey key{};uint64_t seen=0,nativeId=0;std::array<uint8_t,56> applied{};bool used=false;};
 std::array<Entry,Capacity> entries_{};
public:
 uint64_t continuityMs=250,forgetMs=2000;
 struct Decision {bool track=false,continuous=false;std::array<uint8_t,56> previous{};};
 size_t size(uint64_t now)const{size_t n=0;for(const auto& e:entries_)n+=e.used&&now>=e.seen&&now-e.seen<=forgetMs;return n;}
 // A collider that was tracked a moment ago stays the mod's while the game's action sequence moves on.
 bool owns(const ColliderKey& key,uint64_t now)const{
  for(const auto& e:entries_)if(e.used&&e.key==key)return now>=e.seen&&now-e.seen<=continuityMs;return false;
 }
 Decision apply(const ColliderKey& key,bool eligible,uint64_t now,const std::array<uint8_t,56>& current){
  Decision d;if(!key.valid()||!eligible||!now)return d;
  Entry* slot=nullptr;
  for(auto& e:entries_)if(e.used&&e.key==key){slot=&e;break;}
  if(slot){d.continuous=now>=slot->seen&&now-slot->seen<=continuityMs;if(d.continuous)d.previous=slot->applied;}
  else{
   for(auto& e:entries_)if(!e.used||now<e.seen||now-e.seen>forgetMs){slot=&e;break;}
   if(!slot){slot=&entries_[0];for(auto& e:entries_)if(e.seen<slot->seen)slot=&e;}
   *slot={};slot->key=key;slot->used=true;
  }
  if(!d.continuous)d.previous=current;
  slot->seen=now;slot->applied=current;d.track=true;return d;
 }
 bool returned(const ColliderKey& key,uint64_t nativeId){for(auto& e:entries_)if(e.used&&e.key==key){e.nativeId=nativeId;return true;}return false;}
};

// The swing that pressed the game's attack button. The action it starts is the next one of that entity and item
// whose trigger time differs from the one seen at the press, within `windowMs`.
struct LiveAttackPress {
 uint32_t entity=0,item=0;int64_t previousTrigger=0;uint64_t pressed=0,serial=0;
 bool starts(const EventActionKey& k,uint64_t now,uint64_t windowMs=1000)const{
  return serial&&pressed&&now>=pressed&&now-pressed<=windowMs&&k.valid()&&k.entity==entity&&k.item==item&&k.trigger!=previousTrigger;
 }
};
// What the first tick of an action that a swing started hands to the game at once: of the events that are the
// stroke itself (the lasting Impacts of an attack; for a tool without one, what its stroke spawns), those authored
// first. Events authored together with the first go with it: a pair of daggers strikes with two Impacts at one time
// (game data, Attack_Player_Double_Daggers05: two SpawnImpacts at 533 ms; headset run game145015, where such attacks
// kept the game's timing, animation and step because only attacks with exactly one Impact were taken). Later events
// of the action (the second hit of a flurry) keep their authored time.
struct EarlyEvent {uintptr_t variant=0;int64_t time=0,duration=0;};
constexpr size_t maximumEarlyEvents=4;
inline size_t selectEarly(const EarlyEvent* found,size_t count,std::array<EarlyEvent,maximumEarlyEvents>& out,int64_t together=1000000){
 out={};if(!found||!count)return 0;
 int64_t first=0;bool any=false;
 for(size_t i=0;i<count;++i)if(found[i].variant&&found[i].time>=0&&(!any||found[i].time<first)){first=found[i].time;any=true;}
 if(!any)return 0;
 size_t n=0;
 for(size_t i=0;i<count;++i)if(found[i].variant&&found[i].time>=first&&found[i].time-first<=together){
  if(n==out.size())return 0;                 // more at once than the route holds: the action keeps the game's timing
  bool twice=false;for(size_t j=0;j<n;++j)twice=twice||out[j].variant==found[i].variant;
  if(!twice)out[n++]=found[i];}
 return n;
}
// Early events for live play: on the first tick of an attack started by a swing, the events selectEarly() names
// are handed to the game at once, and the same events are withheld when their authored time comes. An entry lives
// for the length of its action plus `marginMs`, so the authored occurrence can never come back.
template<size_t Capacity=32> class LiveEventLedger {
 struct Entry{EventActionKey key{};std::array<EventIdentity,maximumEarlyEvents> events{};size_t count=0;uint64_t until=0;bool used=false;};
 std::array<Entry,Capacity> entries_{};
public:
 uint64_t marginMs=3000;
 enum class Decision {Ignore,Append,Suppress};
 size_t size(uint64_t now)const{size_t n=0;for(const auto& e:entries_)n+=e.used&&now<=e.until;return n;}
 bool owns(const EventActionKey& key,uint64_t now)const{for(const auto& e:entries_)if(e.used&&now<=e.until&&e.key==key)return true;return false;}
 // early: the first tick's private look ahead found this event; otherwise the event is due now in the native list.
 Decision consider(const EventActionKey& key,const EventIdentity& event,bool early,bool qualified,size_t count,size_t capacity,uint64_t now)const{
  if(!key.valid()||!event.variant||!now)return Decision::Ignore;
  for(const auto& e:entries_)if(e.used&&now<=e.until&&e.key==key){
   if(!early)for(size_t i=0;i<e.count;++i)if(e.events[i]==event)return Decision::Suppress;
   return Decision::Ignore;}
  if(!early||!qualified||count>=capacity)return Decision::Ignore;
  for(const auto& e:entries_)if(!e.used||now>e.until)return Decision::Append;
  return Decision::Ignore; // every entry still guards a running action: this attack keeps its authored timing
 }
 bool commit(const EventActionKey& key,const EventIdentity& event,uint64_t now,uint64_t actionMs){return commit(key,&event,1,now,actionMs);}
 bool commit(const EventActionKey& key,const EventIdentity* events,size_t count,uint64_t now,uint64_t actionMs){
  if(!key.valid()||!events||!count||count>maximumEarlyEvents||!now||owns(key,now))return false;
  for(size_t i=0;i<count;++i)if(!events[i].variant)return false;
  for(auto& e:entries_)if(!e.used||now>e.until){e={};e.key=key;e.count=count;for(size_t i=0;i<count;++i)e.events[i]=events[i];e.until=now+actionMs+marginMs;e.used=true;return true;}
  return false;
 }
};
}
