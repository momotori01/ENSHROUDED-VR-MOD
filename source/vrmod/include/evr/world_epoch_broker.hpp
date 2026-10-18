#pragma once
#include <evr/world_epoch_abi.hpp>
#include <evr/world_lifetime.hpp>
#include <mutex>
namespace evr::epoch {
// One instance in the pinned process-lifetime provider. All operations hold its
// mutex briefly; no native/game/GPU function or caller callback executes inside.
// External lease handles are unique per acquisition, so a duplicate release
// cannot consume another callback's pin. Pins own MOD values only, not native ECS.
template<size_t Worlds=8,size_t Leases=64,size_t Closures=8> class Broker {
 using Token=motion::WorldToken;using Life=motion::WorldLifetime;using Book=motion::WorldLifetimes<Worlds>;
 struct Lease {uint64_t handle=0;Token token{};};
 struct Closure {uint64_t handle=0;Life primary{};typename Book::Batch batch{};};
 std::mutex mutex_;Book book_;std::array<Token,Worlds> records_{};std::array<bool,Worlds> admissionClosed_{};std::array<Lease,Leases> leases_{};std::array<Closure,Closures> closures_{};
 uint64_t provider_=0,nextLease_=0,nextClose_=0;bool active_=false,faulted_=false;
 static Token token(const Request& r){return {r.slot,r.serial};}
 static bool same(const Life& s,const Request& r){return s.token&&s.token==token(r)&&s.identity.world==r.world&&s.identity.registry==r.registry&&s.identity.root==r.root&&s.identity.kind==r.kind;}
 Life find(uint64_t world,uint32_t kind)const{for(auto t:records_){auto s=book_.snapshot(t);if((s.phase==motion::WorldPhase::Alive||s.phase==motion::WorldPhase::Closing)&&s.identity.world==world&&s.identity.kind==kind)return s;}return {};}
 void fill(Response& out,const Life& s){out.provider=provider_;out.serial=s.token.serial;out.slot=uint32_t(s.token.slot);out.world=s.identity.world;out.registry=s.identity.registry;out.root=s.identity.root;out.kind=s.identity.kind;out.phase=uint32_t(s.phase);out.pins=s.pins;if(s.token&&admissionClosed_[s.token.slot])out.flags|=flagAdmissionClosed;}
 void reclaim(Token t){if(book_.reclaim(t)){if(records_[t.slot]==t){records_[t.slot]={};admissionClosed_[t.slot]=false;}}}
 Status dispatch(const Request& r,Response& out){
  const auto op=Operation(r.operation);
  if(op==Operation::Register){if(provider_)return Status::Duplicate;provider_=1;active_=true;book_.sealAdoption();return Status::Ok;}
  if(op==Operation::Stats)return Status::Ok;
  if(!provider_||(r.provider&&r.provider!=provider_))return Status::Unavailable;
  if(op==Operation::Resolve){if(!r.world||r.kind>2)return Status::Invalid;auto s=find(r.world,r.kind);if(!s.token)return Status::Unknown;fill(out,s);return Status::Ok;}
  if(r.provider!=provider_)return Status::Unavailable;
  if(op==Operation::Fault){faulted_=true;active_=false;return Status::Ok;}
  if(op==Operation::Read){auto s=book_.snapshot(token(r));if(!same(s,r))return Status::WrongToken;fill(out,s);return Status::Ok;}
  if(op==Operation::Release){
   for(auto& ref:leases_)if(ref.handle&&ref.handle==r.handle){auto s=book_.snapshot(ref.token);if(!same(s,r))return Status::WrongToken;
    if(!book_.unpin(ref.token)){faulted_=true;active_=false;return Status::Fault;}fill(out,book_.snapshot(ref.token));out.handle=ref.handle;auto t=ref.token;ref={};reclaim(t);return Status::Ok;}
   return Status::Unknown;
  }
  if(op==Operation::EndRootClose){
   for(auto& c:closures_)if(c.handle&&c.handle==r.handle){if(!same(c.primary,r))return Status::WrongToken;
    for(size_t i=0;i<c.batch.count;++i){const auto t=c.batch.tokens[i];if(!book_.finishClose(t)||!book_.unpin(t)){faulted_=true;active_=false;return Status::Fault;}if(t==c.primary.token)fill(out,book_.snapshot(t));reclaim(t);}
    out.handle=c.handle;c={};return Status::Ok;}
   return Status::Unknown;
  }
  if(op==Operation::Stop){if(book_.size())return Status::Busy;for(const auto& c:closures_)if(c.handle)return Status::Busy;for(const auto& p:leases_)if(p.handle)return Status::Busy;active_=false;return Status::Ok;}
  if(!active_||faulted_||book_.faulted())return Status::Unavailable;
  if(op==Operation::Create){
   if(r.kind>2||!r.world||!r.registry||!r.root)return Status::Invalid;
   auto t=book_.created({r.world,r.registry,r.root,uint8_t(r.kind)});
   if(!t){if(book_.faulted()){faulted_=true;active_=false;return Status::Fault;}return Status::Capacity;}
   records_[t.slot]=t;admissionClosed_[t.slot]=false;fill(out,book_.snapshot(t));return Status::Ok;
  }
  if(op==Operation::Acquire){
   auto s=book_.snapshot(token(r));if(!same(s,r))return Status::WrongToken;if(!book_.alive(s.token)||admissionClosed_[s.token.slot])return Status::Closing;
   if(r.handle)return Status::Invalid;if(nextLease_==UINT64_MAX)return Status::Capacity;
   for(auto& ref:leases_)if(!ref.handle){if(!book_.pin(s.token))return Status::Capacity;ref={++nextLease_,s.token};fill(out,book_.snapshot(s.token));out.handle=ref.handle;return Status::Ok;}return Status::Capacity;
  }
  // Native owner may delete entities over several frames BEFORE destruction.
  // Stop only new admission; existing leases/suppression retain exact identity.
  // No blocking wait and no native/ECS function runs under this mutex.
  if(op==Operation::CloseAdmission){
   auto s=book_.snapshot(token(r));if(!same(s,r))return Status::WrongToken;if(s.identity.kind==1||r.handle)return Status::Invalid;
   if(!book_.alive(s.token))return Status::Closing;
   size_t count=0;for(auto t:records_){auto v=book_.snapshot(t);if(v.phase==motion::WorldPhase::Alive&&v.identity.root==s.identity.root)++count;}
   if(count!=(s.identity.kind==2?2u:1u))return Status::Invalid;
   for(auto t:records_){auto v=book_.snapshot(t);if(v.phase==motion::WorldPhase::Alive&&v.identity.root==s.identity.root)admissionClosed_[t.slot]=true;}
   fill(out,book_.snapshot(s.token));return Status::Ok;
  }
  if(op==Operation::BeginRootClose){
   auto s=book_.snapshot(token(r));if(!same(s,r))return Status::WrongToken;if(s.identity.kind==1)return Status::Invalid;if(!book_.alive(s.token))return Status::Closing;
   size_t count=0;for(auto t:records_){auto v=book_.snapshot(t);if(v.phase==motion::WorldPhase::Alive&&v.identity.root==s.identity.root)++count;}
   if(count!=(s.identity.kind==2?2u:1u)){faulted_=true;active_=false;return Status::Fault;}
   if(nextClose_==UINT64_MAX)return Status::Capacity;
   for(auto& c:closures_)if(!c.handle){
    for(auto t:records_){auto v=book_.snapshot(t);if(v.phase==motion::WorldPhase::Alive&&v.identity.root==s.identity.root&&!book_.pin(t)){faulted_=true;active_=false;return Status::Fault;}}
    c.handle=++nextClose_;c.primary=s;c.batch=book_.beginRootClose(s.identity.root);fill(out,book_.snapshot(s.token));out.handle=c.handle;return Status::Ok;
   }return Status::Capacity;
  }
  return Status::Invalid;
 }
public:
 Broker()=default;Broker(const Broker&)=delete;Broker& operator=(const Broker&)=delete;
 Response call(const Request& r) noexcept {
  Response out;
  if(r.size!=sizeof(Request)||r.version!=abiVersion){out.status=uint32_t(Status::AbiMismatch);return out;}
  if(r.reserved||r.operation<uint32_t(Operation::Register)||r.operation>uint32_t(Operation::CloseAdmission)){out.status=uint32_t(Status::Invalid);return out;}
  try{
   std::lock_guard<std::mutex> lock(mutex_);out.status=uint32_t(dispatch(r,out));out.provider=provider_;out.flags|=(active_?flagActive:0)|(faulted_||book_.faulted()?flagFaulted:0);out.worlds=uint32_t(book_.size());
   for(const auto& c:closures_)out.closures+=c.handle!=0;for(const auto& p:leases_)out.leases+=p.handle!=0;
  }catch(...){out={};out.status=uint32_t(Status::Fault);}
  return out;
 }
};
}
