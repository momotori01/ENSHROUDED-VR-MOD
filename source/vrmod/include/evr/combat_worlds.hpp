#pragma once
#include <evr/world_epoch_abi.hpp>
namespace evr::motion {
// Saved identity only. This is neither a native memory pin nor a current-state
// proof; obtain a fresh broker response again at each native publication boundary.
struct WorldStamp {
 uint64_t provider=0,serial=0,world=0,registry=0,root=0;uint32_t slot=0,kind=0;
 bool empty()const{return !provider&&!serial&&!world&&!registry&&!root&&!slot&&!kind;}
 bool valid()const{return provider&&serial&&world&&registry&&root&&kind<=2;}
 bool operator==(const WorldStamp& b)const{return provider==b.provider&&serial==b.serial&&world==b.world&&registry==b.registry&&root==b.root&&slot==b.slot&&kind==b.kind;}
};
inline bool knownWorldResponse(const epoch::Response& r){return r.size==sizeof(r)&&r.version==epoch::abiVersion&&r.status==uint32_t(epoch::Status::Ok)&&r.phase>=uint32_t(epoch::Phase::Alive)&&r.phase<=uint32_t(epoch::Phase::Ended);}
inline WorldStamp worldStamp(const epoch::Response& r){return knownWorldResponse(r)?WorldStamp{r.provider,r.serial,r.world,r.registry,r.root,r.slot,r.kind}:WorldStamp{};}
struct CombatWorlds {
 WorldStamp input{},contact{};
 bool empty()const{return input.empty()&&contact.empty();}
 bool valid()const{return input.valid()&&contact.valid()&&input.kind==2&&contact.kind==0&&input.provider==contact.provider&&input.world!=contact.world&&input.root!=contact.root&&input.registry!=contact.registry;}
 bool operator==(const CombatWorlds& b)const{return input==b.input&&contact==b.contact;}
};
inline CombatWorlds combatWorlds(const epoch::Response& input,const epoch::Response& contact){CombatWorlds result{worldStamp(input),worldStamp(contact)};return result.valid()&&epoch::admitsNew(input)&&epoch::admitsNew(contact)?result:CombatWorlds{};}
inline bool permitsNewCombat(const CombatWorlds& saved,const epoch::Response& input,const epoch::Response& contact){return saved.valid()&&saved==combatWorlds(input,contact);}
// An Actor idle fence is readable only before SERVER admission closes. Input
// admission can close independently while that server still finishes an attack.
// This is a necessary identity condition; native flush scheduling still owns ECS.
inline bool permitsActorFence(const WorldStamp& saved,const epoch::Response& contact){
 return saved.valid()&&saved.kind==0&&saved==worldStamp(contact)&&contact.pins&&epoch::admitsNew(contact);
}
// A consumed Impact remains suppressed if its client has already ended but its
// exact server generation is still running/closing. Never apply it after native
// server teardown or to a replacement World, even with identical raw addresses.
inline bool permitsOwnedSuppression(const WorldStamp& saved,const epoch::Response& contact,bool alreadyConsumed){
 return alreadyConsumed&&saved.valid()&&saved.kind==0&&saved==worldStamp(contact)&&(contact.phase==uint32_t(epoch::Phase::Alive)||contact.phase==uint32_t(epoch::Phase::Closing));
}
}
