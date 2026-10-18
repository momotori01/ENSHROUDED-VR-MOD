#pragma once
#include <evr/combat_worlds.hpp>
#include <evr/world_end_boundary.hpp>
#include <array>
namespace evr::motion {
enum class CombatEpochState : uint8_t {Open,Closed,Unavailable};
// Serialized owner of two VALUE leases. The caller supplies synchronization and
// drains its native callbacks before release(); this never pins ECS memory.
// One binding per instance: a failed/ended instance cannot adopt a later World.
class CombatEpochClient {
public:
 struct Sample {
  epoch::Response input{},contact{};bool inputValid=false,contactValid=false,newCombat=false,consumerFault=false;
  bool suppresses(const WorldStamp& owner,bool consumed)const{return contactValid&&permitsOwnedSuppression(owner,contact,consumed);}
  CombatEpochState state()const {
   if(consumerFault||!inputValid||!contactValid)return CombatEpochState::Unavailable;
   for(const auto* r:{&input,&contact})if(!knownWorldResponse(*r)||!r->pins||!(r->flags&epoch::flagActive)||
       (r->flags&~(epoch::flagActive|epoch::flagAdmissionClosed)))return CombatEpochState::Unavailable;
   if(input.phase!=uint32_t(epoch::Phase::Alive)||contact.phase!=uint32_t(epoch::Phase::Alive)||
      ((input.flags|contact.flags)&epoch::flagAdmissionClosed))return CombatEpochState::Closed;
   return newCombat?CombatEpochState::Open:CombatEpochState::Unavailable;
  }
 };
private:
 epoch::Call call_=nullptr;CombatWorlds worlds_{};std::array<epoch::Response,2> leases_{};
 bool attempted_=false,faulted_=false,released_=false;
 static bool checked(uint32_t status,const epoch::Response& r){return status==0&&r.status==status&&r.size==sizeof(r)&&r.version==epoch::abiVersion;}
 bool invoke(const epoch::Request& q,epoch::Response& r){
  if(!call_)return false;try{return checked(call_(&q,&r),r);}catch(...){return false;}
 }
 bool exact(const epoch::Response& r,size_t role)const{return worldStamp(r)==(role==0?worlds_.input:worlds_.contact);}
public:
 CombatEpochClient()=default;CombatEpochClient(const CombatEpochClient&)=delete;CombatEpochClient& operator=(const CombatEpochClient&)=delete;
 // Deliberately no destructor release: implicit unwinding cannot prove native
 // callback quiescence. Exceptional cleanup must explicitly drain then release.
 const CombatWorlds& worlds()const{return worlds_;}
 bool faulted()const{return faulted_;}
 std::array<epoch::Response,2> leaseIdentities()const{return leases_;}
 size_t retained()const{return size_t(leases_[0].handle!=0)+size_t(leases_[1].handle!=0);}
 bool open(epoch::Call fn,const epoch::Response& client,const epoch::Response& server){
  if(attempted_||released_)return false;attempted_=true;call_=fn;worlds_=combatWorlds(client,server);
  if(!fn||!worlds_.valid()||client.handle||server.handle){faulted_=true;return false;}
  const std::array<epoch::Response,2> expected{client,server};
  for(size_t i=0;i<2;++i){
   epoch::Response resolved;
   if(!invoke(epoch::request(epoch::Operation::Resolve,expected[i]),resolved)||!exact(resolved,i)||!epoch::admitsNew(resolved)){faulted_=true;release();return false;}
   epoch::Response acquired;const bool valid=invoke(epoch::request(epoch::Operation::Acquire,expected[i]),acquired);
   // The request's exact identity owns any returned handle. Retain it even if
   // the response is malformed so explicit cleanup can attempt the exact token.
   if(acquired.handle){leases_[i]=expected[i];leases_[i].handle=acquired.handle;}
   if(!valid||!exact(acquired,i)||!epoch::admitsNew(acquired)||!acquired.handle||!acquired.pins){faulted_=true;release();return false;}
  }
  if(!sample().newCombat){faulted_=true;release();return false;}return true;
 }
 Sample sample(){
  Sample out;
  if(!attempted_||released_||retained()!=2)return out;
  out.inputValid=invoke(epoch::request(epoch::Operation::Read,leases_[0]),out.input)&&exact(out.input,0)&&out.input.pins;
  out.contactValid=invoke(epoch::request(epoch::Operation::Read,leases_[1]),out.contact)&&exact(out.contact,1)&&out.contact.pins;
  // Read both roles again. This detects observed transitions but is not a lock
  // over subsequent native publication. Native scheduling fences remain needed.
  epoch::Response clientAfter,serverAfter;
  const bool ci=invoke(epoch::request(epoch::Operation::Read,leases_[0]),clientAfter)&&exact(clientAfter,0)&&clientAfter.pins;
  const bool si=invoke(epoch::request(epoch::Operation::Read,leases_[1]),serverAfter)&&exact(serverAfter,1)&&serverAfter.pins;
  out.inputValid=out.inputValid&&ci;out.contactValid=out.contactValid&&si;
  const bool liveBefore=out.inputValid&&out.contactValid&&permitsNewCombat(worlds_,out.input,out.contact);
  out.input=clientAfter;out.contact=serverAfter;
  if(!out.inputValid||!out.contactValid)faulted_=true;
  out.consumerFault=faulted_;out.newCombat=!faulted_&&liveBefore&&permitsNewCombat(worlds_,out.input,out.contact);
  return out;
 }
 // Fresh terminal identity from our OWN retained lease. Does not establish
 // callback quiescence: the adapter must supply that independently, under locks.
 bool endedContact(WorldEndBoundary& out){
  out={};const auto current=sample();
  if(faulted_||current.consumerFault||!current.contactValid||!leases_[1].handle)return false;
  WorldEndBoundary candidate;candidate.observed=current.contact;candidate.owner=worlds_.contact;
  candidate.leaseHandle=leases_[1].handle;
  auto probe=candidate;probe.consumersStopped=true;
  if(!probe.permits(worlds_.contact))return false;
  out=candidate;return true;
 }
 // Stop external callbacks first. A transient failed release retains its exact
 // handle for retry; it never reports success or silently resets the provider.
 bool release(){
  released_=true;bool ok=true;
  for(auto& lease:leases_)if(lease.handle){epoch::Response result;
   if(invoke(epoch::request(epoch::Operation::Release,lease),result)&&result.handle==lease.handle&&worldStamp(result)==worldStamp(lease))lease={};
   else{faulted_=true;ok=false;}
  }
  return ok&&retained()==0;
 }
};
}
