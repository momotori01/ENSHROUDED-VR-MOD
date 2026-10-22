#pragma once
#include "native_impact_components.hpp"
// How long the hit volume of an action event lives (2026-10-17, the 1.0 attack spec: "for the length of a skill's own
// animation, the weapon deals the hit whenever it touches - so a move-then-strike skill needs no second swing").
// Fact (Mock game-20261017-062409 and -054608, sixteen strokes of the felling axe): a hit handed to the game on the
// action's first tick lives the event's authored length from then on (82..103 ms for the axe's 100 ms Impact), not to
// the event's authored end in the action. The ECS has keen::ecs::LifeTime (24 bytes: lifeTime, endOfLife, then the bytes
// enforceDestroy, preventDestroy, triggeredDestroy; June reflection) - read here for each new tracked hit volume of the
// local player (motion_collider_apply.hpp, live), and lengthened for the hits an owned skill gave (own()).
// The length is changed in the units the game keeps: lifeTime and endOfLife grow by lifeTime x (factor - 1).
// EVR_IMPACT_LIFETIME_TEST=<factor> in the game's environment lengthens every tracked hit by that factor (a trial).
namespace impactLifetime {
inline std::mutex mutex;
struct Owned {uint32_t sequence=0;std::array<std::array<uint8_t,16>,8> guids{};std::array<double,8> factors{};size_t count=0;uint64_t until=0;};
inline Owned owned;
inline const double testFactor=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_IMPACT_LIFETIME_TEST",v,sizeof v);if(!n||n>=sizeof v)return 1.0;const double f=std::strtod(v,nullptr);return f>1&&f<=40?f:1.0;}();
inline std::atomic<unsigned> seen{0},lengthened{0},failures{0};
inline std::map<uintptr_t,std::pair<int,int>> ids;   // world -> (LifeTime, SpawnTime) component ids, -1 when absent
inline std::array<std::pair<uintptr_t,uint32_t>,64> recent{};inline size_t recentAt=0;
// The hits of an action the swing started that should live this many times their authored length (until the action's end).
inline void own(uint32_t sequence,const std::array<uint8_t,16>* guids,const double* factors,size_t count,uint64_t until){
 std::lock_guard<std::mutex> lock(mutex);owned=Owned{};owned.sequence=sequence;owned.count=std::min(count,owned.guids.size());
 for(size_t i=0;i<owned.count;++i){owned.guids[i]=guids[i];owned.factors[i]=factors[i];}owned.until=until;
}
// Other players' attacks handed over on their first tick on this host (motion_event_selection_observer.hpp remoteActions,
// 2026-10-22): their hits live until the next attack may begin, so that the hit their mod sent (online_sync.hpp hitAction),
// which comes over another path than the game's own input, still finds the collider.
inline std::array<Owned,8> remote{};inline size_t remoteAt=0;
inline void ownRemote(uint32_t sequence,const std::array<uint8_t,16>* guids,const double* factors,size_t count,uint64_t until){
 std::lock_guard<std::mutex> lock(mutex);auto& o=remote[remoteAt++%remote.size()];o=Owned{};o.sequence=sequence;o.count=std::min(count,o.guids.size());
 for(size_t i=0;i<o.count;++i){o.guids[i]=guids[i];o.factors[i]=factors[i];}o.until=until;
}
inline std::pair<int,int> componentIds(uintptr_t world){
 if(const auto it=ids.find(world);it!=ids.end())return it->second;
 std::pair<int,int> r{-1,-1};
 for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){
  try{const auto t=nativeImpactComponents::componentType(world,i);
   if(t.qualifiedName=="keen::ecs::LifeTime"&&t.bytes==24)r.first=int(i);
   if(t.qualifiedName=="keen::ecs::SpawnTime"&&t.bytes==8)r.second=int(i);}catch(const std::exception&){break;}
 }
 if(ids.size()<16)ids[world]=r;return r;
}
// A tracked hit volume of the local player, on its first sweep.
inline void tracked(uintptr_t world,uint32_t entity,const std::array<uint8_t,24>& event) noexcept {
 try{
  std::lock_guard<std::mutex> lock(mutex);
  for(const auto& r:recent)if(r.first==world&&r.second==entity)return;
  recent[recentAt++%recent.size()]={world,entity};
  const auto [lifeId,spawnId]=componentIds(world);
  const auto e=nativeImpactComponents::entityAtWorld(world,entity);
  const auto life=lifeId>=0?nativeImpactComponents::component(e,unsigned(lifeId),24):0;
  const auto spawn=spawnId>=0?nativeImpactComponents::component(e,unsigned(spawnId),8):0;
  uint32_t sequence=0;std::memcpy(&sequence,event.data(),4);std::array<uint8_t,16> guid{};std::memcpy(guid.data(),event.data()+8,16);
  double factor=testFactor;bool skill=false;
  if(owned.sequence==sequence&&GetTickCount64()<=owned.until)for(size_t i=0;i<owned.count;++i)if(owned.guids[i]==guid){factor=owned.factors[i];skill=true;}
  if(!skill)for(const auto& o:remote)if(o.sequence==sequence&&GetTickCount64()<=o.until)for(size_t i=0;i<o.count;++i)if(o.guids[i]==guid&&o.factors[i]>factor){factor=o.factors[i];skill=true;}
  int64_t lifeTime=0,endOfLife=0,spawnTime=0,newLife=0,newEnd=0;std::array<uint8_t,3> flags{};
  if(life){lifeTime=read<int64_t>(life);endOfLife=read<int64_t>(life+8);flags=read<std::array<uint8_t,3>>(life+16);}
  if(spawn)spawnTime=read<int64_t>(spawn);
  bool written=false;
  if(life&&factor>1&&lifeTime>0&&endOfLife>0&&!flags[0]&&!flags[2]){
   const auto extra=int64_t(double(lifeTime)*(factor-1));newLife=lifeTime+extra;newEnd=endOfLife+extra;
   *reinterpret_cast<int64_t*>(life)=newLife;*reinterpret_cast<int64_t*>(life+8)=newEnd;written=true;++lengthened;
  }
  if(seen.fetch_add(1)<200||written&&lengthened.load()<200){
   std::ostringstream o;o<<"\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"sequence\":"<<sequence<<",\"sub\":"<<unsigned(event[4])<<",\"life_time_component\":"<<lifeId<<",\"spawn_time_component\":"<<spawnId
    <<",\"life_time\":"<<lifeTime<<",\"end_of_life\":"<<endOfLife<<",\"spawn_time\":"<<spawnTime<<",\"enforce_destroy\":"<<unsigned(flags[0])<<",\"prevent_destroy\":"<<unsigned(flags[1])<<",\"triggered_destroy\":"<<unsigned(flags[2])
    <<",\"owned_skill_hit\":"<<(skill?"true":"false")<<",\"factor\":"<<factor<<",\"lengthened\":"<<(written?"true":"false")<<",\"new_life_time\":"<<newLife<<",\"new_end_of_life\":"<<newEnd<<",\"milliseconds\":"<<GetTickCount64();
   logFile->event("impact_lifetime",o.str());
  }
 }catch(const std::exception& ex){if(failures.fetch_add(1)<3)try{logFile->event("impact_lifetime_failure","\"reason\":"+jsonString(ex.what()));}catch(...){}}
}
}
