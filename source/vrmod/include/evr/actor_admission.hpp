#pragma once
#include <evr/combat_worlds.hpp>
namespace evr::motion {
// One finite combat binding. This closes NEW admission, not native callbacks or
// ECS ownership. The caller serializes access and still drains in-flight work.
// No reset/rebind: a respawn needs a separately proven actor lifetime.
class ActorAdmission {
public:
 enum class Stop {Rejected,Stopped,AlreadyStopped};
 bool open(const CombatWorlds& worlds){
  if(opened_||!worlds.valid())return false;
  worlds_=worlds;opened_=true;return true;
 }
 bool bind(const WorldStamp& server,uint32_t entity){
  if(!opened_||stopped_||!entity||!(server==worlds_.contact))return false;
  if(actor_&&actor_!=entity)return false;
  actor_=entity;return true;
 }
 Stop stop(const WorldStamp& server,uint32_t entity,uint64_t ownerCall){
  if(!opened_||!actor_||entity!=actor_||!(server==worlds_.contact)||!ownerCall)return Stop::Rejected;
  if(stopped_)return Stop::AlreadyStopped;
  stopped_=true;ownerCall_=ownerCall;return Stop::Stopped;
 }
 bool permitsNew(const CombatWorlds& worlds)const{return opened_&&!stopped_&&worlds==worlds_;}
 bool permitsFence(const WorldStamp& server)const{return opened_&&!stopped_&&server==worlds_.contact;}
 bool stopped()const{return stopped_;}
 uint32_t actor()const{return actor_;}
 uint64_t ownerCall()const{return ownerCall_;}
private:
 CombatWorlds worlds_{};uint32_t actor_=0;uint64_t ownerCall_=0;bool opened_=false,stopped_=false;
};
}
