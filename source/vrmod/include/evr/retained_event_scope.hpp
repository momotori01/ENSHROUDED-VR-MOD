#pragma once
#include <evr/motion_event_retime.hpp>
namespace evr::motion {
// Numeric native argument identities only. Never dereference this saved Actor
// address. Only an original native call carrying it, the same registry and live
// retained World may supply readable Actor bytes for the second action check.
struct RetainedEventScope {
 EventActionKey action{};uintptr_t actorArgument=0,registryArgument=0;
 bool valid()const{return actorArgument&&registryArgument&&action.valid()&&action.epoch.valid()&&action.epoch.kind==0;}
 bool acceptsArguments(uintptr_t actor,uintptr_t registry)const {
  return valid()&&actor==actorArgument&&registry==registryArgument;
 }
 bool acceptsCall(uintptr_t actor,uintptr_t registry,const WorldStamp& current)const {
  return acceptsArguments(actor,registry)&&current==action.epoch;
 }
 // Foreign native calls must not sample the retained World, which may already
 // have ended. Matching arguments still require a fresh sample on EVERY call;
 // cached pointer equality never substitutes for the retained generation.
 template<class Sample> bool acceptsSampledCall(uintptr_t actor,uintptr_t registry,Sample&& sample)const {
  return acceptsArguments(actor,registry)&&sample(action.world)==action.epoch;
 }
 bool matchesAction(const EventActionKey& candidate,uint32_t triggerEntity)const {
  return valid()&&candidate==action&&triggerEntity==action.entity;
 }
};
}
