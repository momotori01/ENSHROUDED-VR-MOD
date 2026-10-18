#pragma once
#include <evr/motion_event_retime.hpp>
#include <evr/motion_collider_lease.hpp>
namespace evr::motion {
// These values must be captured at the validated native post-task/post-command
// boundary. This policy does not claim that arbitrary caller-supplied zeroes
// establish native quiescence, or that address equality proves a World epoch.
struct ContactBoundary {
 uint64_t world=0,registry=0,taskList=0;uint32_t actor=0,tick=0,entityCounter=0;uint8_t worldKind=255;
 EventActionKey action{};bool active=false;
 uint64_t running=0,queued=0,completed=0,commandBytes=0,commandCount=0,selectors=0,queries=0;WorldStamp epoch{};
 bool quiet()const{return !running&&!queued&&!completed&&!commandBytes&&!commandCount&&!selectors&&!queries;}
 bool valid()const{return world&&registry&&taskList&&actor&&worldKind==0&&(epoch.empty()||(epoch.valid()&&epoch.world==world&&epoch.registry==registry&&epoch.kind==0));}
 bool cleared()const{return !active&&!action.sequence&&!action.item&&!action.trigger;}
};
// A continuity guard is shared by all observed actions during this diagnostic.
// A changed owner/task-list/World or backward tick/counter permanently closes it.
// A new game session must construct a new adapter after draining old callbacks.
class ContactBoundaryContinuity {
 ContactBoundary previous_{};bool seen_=false,faulted_=false;
public:
 bool observe(const ContactBoundary& b){
  if(faulted_)return false;
  if(!b.valid()||(seen_&&(!(b.epoch==previous_.epoch)||b.world!=previous_.world||b.registry!=previous_.registry||b.taskList!=previous_.taskList||b.actor!=previous_.actor||b.tick<previous_.tick||b.entityCounter<previous_.entityCounter))){faulted_=true;return false;}
  previous_=b;seen_=true;return true;
 }
 bool faulted()const{return faulted_;}
};
// One verified action + its independently destroyed native Impact. Event and
// collider entries remain consumed through the late authored selection window.
// The guard emits once only, at a later quiet, fully cleared Actor boundary.
class ContactRetirementGate {
 EventAuthority authority_{};ColliderKey collider_{};uint32_t activeTick_=0;
 bool initialized_=false,activeSeen_=false,retired_=false,faulted_=false,permitted_=false;
 bool blockedOnly_=false,suppressionSeen_=false;EventIdentity blockedIdentity_{};
public:
 bool initialize(const EventAuthority& a,const ColliderKey& k){
  if(initialized_||!k.valid()||!a.matches(k.world,a.action.entity,a.action.item,a.action.trigger,k.event,k.epoch))return false;
  authority_=a;collider_=k;initialized_=true;return true;
 }
 // Event-only cancellation uses an explicit finite native adapter.
 // The adapter must register immediately after the exact blocked commit,
 // forward every matching creation, and report the real late suppression.
 template<size_t N> bool initializeBlocked(const EventRetimeLedger<N>& ledger,const EventActionKey& key,const EventIdentity& event){
  if(initialized_||!ledger.blocked(key)||ledger.consider(key,event,false,false,0,0)!=EventRetime::Decision::Suppress)return false;
  authority_.action=key;authority_.guid=event.guid;authority_.motion=true;blockedIdentity_=event;blockedOnly_=initialized_=true;return true;
 }
 bool suppressed(const EventActionKey& key,const EventIdentity& event){
  if(!initialized_||!blockedOnly_||retired_||faulted_||!(key==authority_.action)||!(event==blockedIdentity_))return false;
  suppressionSeen_=true;return true;
 }
 bool impactCreated(const EventActionKey& key){
  if(!initialized_||!blockedOnly_||retired_||!(key==authority_.action))return false;
  // A blocked event unexpectedly spawning an Impact invalidates the event-only
  // proof. It cannot be treated as an ordinary cancelled collider by this gate.
  faulted_=true;permitted_=false;return true;
 }
 bool ready(const ContactBoundary& b,bool destroyed){
  permitted_=false;
  if(!initialized_||retired_||faulted_)return false;
  if(!b.valid()||!(b.epoch==authority_.action.epoch)||b.world!=authority_.action.world||b.actor!=authority_.action.entity){faulted_=true;return false;}
  if(b.active){
   if(!(b.action==authority_.action)){faulted_=true;return false;}
   if(b.quiet()){activeSeen_=true;activeTick_=b.tick;}
   return false;
  }
  return permitted_=(!blockedOnly_||suppressionSeen_)&&destroyed&&activeSeen_&&b.cleared()&&b.quiet()&&b.tick>activeTick_;
 }
 bool commit(){if(!initialized_||retired_||faulted_||!permitted_)return false;retired_=true;permitted_=false;return true;}
 bool retired()const{return retired_;}bool faulted()const{return faulted_;}
};
}
