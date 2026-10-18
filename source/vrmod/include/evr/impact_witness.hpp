#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace evr::motion {
struct ImpactWitnessKey {
 uint64_t world=0;uint32_t root=0,instance=0;
 bool valid()const{return world&&root&&instance;}
 bool operator==(const ImpactWitnessKey& b)const{return world==b.world&&root==b.root&&instance==b.instance;}
};
enum class WitnessRecord {Stored,Invalid,ClockRegression,Conflict,Full};
// Read-only attribution evidence, not an attack permission/cooldown mechanism.
// Callers serialize access. A conflicting identity is quarantined until reset.
// Live records are never evicted merely because another Impact is observed.
template<class Payload,size_t Capacity=16> class ImpactWitnessLedger {
 static_assert(Capacity>0);
public:
 struct Witness {Payload value{};uint64_t first=0,last=0;};
 explicit ImpactWitnessLedger(uint64_t maximumAge):maximumAge_(maximumAge){}
 template<class SameIdentity> WitnessRecord remember(ImpactWitnessKey key,const Payload& value,uint64_t now,SameIdentity same){
  if(!key.valid()||!now||!maximumAge_)return WitnessRecord::Invalid;
  if(now<latest_)return WitnessRecord::ClockRegression;
  latest_=now;
  for(auto& e:entries_)if(e.used&&e.key==key){
   if(e.conflict||!same(e.witness.value,value)){e.conflict=true;return WitnessRecord::Conflict;}
   e.witness.value=value;e.witness.last=now;return WitnessRecord::Stored;
  }
  for(auto& e:entries_)if(!e.used||(!e.conflict&&now>=e.witness.last&&now-e.witness.last>maximumAge_)){
   e={key,{value,now,now},true,false};return WitnessRecord::Stored;
  }
  return WitnessRecord::Full;
 }
 std::optional<Witness> find(ImpactWitnessKey key,uint64_t now)const{
  if(!key.valid())return {};
  for(const auto& e:entries_)if(e.used&&e.key==key){
   if(e.conflict||now<e.witness.last||now-e.witness.last>maximumAge_)return {};
   return e.witness;
  }
  return {};
 }
 void clear(){entries_={};latest_=0;}
private:
 struct Entry{ImpactWitnessKey key{};Witness witness{};bool used=false,conflict=false;};
 std::array<Entry,Capacity> entries_{};uint64_t maximumAge_=0,latest_=0;
};
}
