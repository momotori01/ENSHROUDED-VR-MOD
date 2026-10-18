#pragma once
#include <evr/combat_worlds.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace evr::motion {
struct EventActionKey {
 uint64_t world=0;uint32_t entity=0,sequence=0,item=0;int64_t trigger=0;uint8_t call=0,sub=0;WorldStamp epoch{};
 bool valid()const{return world&&entity&&sequence&&item&&(epoch.empty()||(epoch.valid()&&epoch.world==world&&epoch.kind==0));}
 bool operator==(const EventActionKey& b)const{return world==b.world&&entity==b.entity&&sequence==b.sequence&&item==b.item&&trigger==b.trigger&&call==b.call&&sub==b.sub&&epoch==b.epoch;}
};
struct NativeAttackTicket {
 uint32_t entity=0,item=0;int64_t previousTrigger=0;uint64_t issued=0,serial=0;bool valid=false;bool motion=false;uint64_t binding=0,frame=0,session=0,origin=0;bool revoked=false;CombatWorlds worlds{};
 bool identifies(const EventActionKey& k,uint64_t now)const{return valid&&serial&&issued&&now>=issued&&now-issued<=1000&&k.valid()&&k.entity==entity&&k.item==item&&k.trigger!=previousTrigger&&(worlds.empty()?k.epoch.empty():(worlds.valid()&&worlds.contact==k.epoch));}
 bool permits(const EventActionKey& k,uint64_t now)const{return worlds.empty()&&!revoked&&identifies(k,now);}
};
struct EventIdentity {uintptr_t variant=0;std::array<uint8_t,16> guid{};bool operator==(const EventIdentity& b)const{return variant==b.variant&&guid==b.guid;}};
// A published native event authorization contains no live ECS/resource pointers.
struct EventAuthority {
 EventActionKey action{};std::array<uint8_t,16> guid{};bool motion=false;
 bool matches(uint64_t world,uint32_t root,uint32_t item,int64_t trigger,const std::array<uint8_t,24>& event,const WorldStamp& epoch={})const{
  uint32_t sequence=0;std::memcpy(&sequence,event.data(),4);
  return motion&&action.valid()&&action.epoch==epoch&&action.world==world&&action.entity==root&&action.item==item&&action.trigger==trigger&&action.sequence==sequence&&action.sub==event[4]&&!std::memcmp(guid.data(),event.data()+8,16);
 }
};
// Bounded one-attack diagnostic. An owned event remains consumed until teardown;
// no timeout can accidentally re-enable it at its authored start time.
class EventRetime {
 EventActionKey owner_{};EventIdentity event_{};bool owned_=false,blocked_=false;
public:
 enum class Decision {Ignore,Append,Suppress};
 bool owns(const EventActionKey& k)const{return owned_&&owner_==k;}
 Decision consider(const EventActionKey& k,const EventIdentity& e,bool early,bool qualified,size_t count,size_t capacity)const{
  if(!k.valid()||!e.variant)return Decision::Ignore;
  if(owned_)return owner_==k&&event_==e&&!early?Decision::Suppress:Decision::Ignore;
  return early&&qualified&&count<capacity?Decision::Append:Decision::Ignore;
 }
 bool blocked()const{return blocked_;}
 bool block(const EventActionKey& k,const EventIdentity& e){if(!commit(k,e))return false;blocked_=true;return true;}
 bool commit(const EventActionKey& k,const EventIdentity& e){if(owned_||!k.valid()||!e.variant)return false;owner_=k;event_=e;owned_=true;return true;}
};
// Bounded multi-action diagnostic. Old consumed events are retained for the
// whole run; starting another action cannot replay its predecessor's Impact.
// Each action still selects only its independently verified single Impact.
template<size_t Capacity=8> class EventRetimeLedger {
 struct Entry{EventActionKey key{};EventRetime value{};bool used=false;};
 std::array<Entry,Capacity> entries_{};size_t limit_=Capacity;bool started_=false;
public:
 size_t size()const{size_t n=0;for(const auto& e:entries_)n+=e.used;return n;}
 size_t limit()const{return limit_;}
 bool configureLimit(size_t n){if(started_||!n||n>Capacity)return false;limit_=n;return true;}
 // Caller must hold the adapter locks and an independently verified native
 // quiescent lifetime boundary. Never retire from a timer or log budget.
 bool retireAfterQuiescence(const EventActionKey& key){for(auto& e:entries_)if(e.used&&e.key==key){e={};return true;}return false;}
 using Decision=EventRetime::Decision;
 bool blocked(const EventActionKey& key)const{for(const auto& e:entries_)if(e.used&&e.key==key)return e.value.blocked();return false;}
 bool owns(const EventActionKey& key)const{for(const auto& e:entries_)if(e.used&&e.key==key)return true;return false;}
 Decision consider(const EventActionKey& key,const EventIdentity& event,bool early,bool qualified,size_t count,size_t capacity)const{
  for(const auto& e:entries_)if(e.used&&e.key==key)return e.value.consider(key,event,early,qualified,count,capacity);
  for(size_t i=0;i<limit_;++i)if(!entries_[i].used)return entries_[i].value.consider(key,event,early,qualified,count,capacity);
  return Decision::Ignore;
 }
 bool commit(const EventActionKey& key,const EventIdentity& event,bool blocked=false){
  if(owns(key))return false;
  for(size_t i=0;i<limit_;++i){auto& e=entries_[i];if(e.used)continue;const bool ok=blocked?e.value.block(key,event):e.value.commit(key,event);if(ok){e.key=key;e.used=true;started_=true;}return ok;}
  return false;
 }
 bool block(const EventActionKey& key,const EventIdentity& event){return commit(key,event,true);}
};

}
