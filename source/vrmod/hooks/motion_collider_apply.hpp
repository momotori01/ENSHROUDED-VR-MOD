#pragma once
#include "contact_lifetime_observer.hpp"
#include "impact_lifetime.hpp"
#include <evr/live_contact.hpp>
// Bounded per-Impact query borrowing plus native removal of a previously leased collider.
namespace motionColliderApply {
inline bool requested=false;inline bool drained=false;inline unsigned cancelAfter=0;
// Unlimited play: time-stamped records instead of the finite leases, no cancellation (motionColliderChannel::live).
inline bool live=false;inline motion::LiveColliderLedger<16> liveLease;inline std::atomic<unsigned> liveLogged{0},liveContinuous{0};
// The hit volume a little ahead of the tracked blade (2026-10-15, user: "the hit and the reaction come late - as soon as
// the weapon touches the enemy"): the pose the game takes is the last drawn frame's, and the game looks about 18 ms
// later (headset run 10: the volume moved every 18 ms in the middle); the blade of a 4 m/s stroke is 7 cm further by
// then. The position is moved on by the blade's speed over the last two poses times EVR_MELEE_COLLIDER_LEAD_MS
// (18; 0 none), at most 15 cm; the sweep from the previous pose then covers the predicted way. Orientation as tracked.
inline const unsigned colliderLeadMs=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_COLLIDER_LEAD_MS",v,sizeof v);if(!n||n>=sizeof v)return 18u;const auto x=std::strtoul(v,nullptr,10);return x>60?60u:unsigned(x);}();
// The hit volume a little larger than the weapon (2026-10-19, user: "can the weapon's hit be slightly larger than the model?
// a hit feels better than a miss"): the scale the transform keeps from the game (bytes 40..51, 1.0 each - the tracked pose
// gives the place and the turn only) times EVR_MELEE_HIT_SCALE (1.15; 1 the game's own size, at most 1.5).
inline const float hitScale=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_HIT_SCALE",v,sizeof v);if(!n||n>=sizeof v)return 1.15f;
 const float x=std::strtof(v,nullptr);return std::isfinite(x)?std::clamp(x,1.f,1.5f):1.15f;}();
inline void enlarge(std::array<uint8_t,56>& current){
 if(hitScale==1.f)return;
 for(size_t at=40;at<52;at+=4){float s=0;std::memcpy(&s,current.data()+at,4);if(std::isfinite(s)&&s>0&&s<100){s*=hitScale;std::memcpy(current.data()+at,&s,4);}}
}
// The sweep from the tip (2026-10-19, user after the headset run of 15:22: "strokes miss at a distance where they surely
// hit - when I swing compactly, the hand moving little and the wrist much"): the game sweeps the hit volume from its previous
// transform to the current one, and the tracked transform is the weapon model's, whose origin is the grip. A stroke of the
// wrist turns the blade about the grip: the grip moves a few centimetres an update, the tip tens of centimetres, and what
// the blade passed between two updates is not swept (presumed: the sweep moves the shape along a line, it does not turn it).
// The sweep therefore starts with the current orientation, placed where its tip stands at the previous tip: the swept way
// is the tip's. A stroke without turning keeps its sweep as it was (the tip moved as the grip did). EVR_MELEE_TIP_SWEEP=0: off.
inline const bool tipSweep=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_TIP_SWEEP",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<unsigned> tipSweeps{0};inline double lastTipSweepMetres=0;
inline void sweepFromTip(const motion::ColliderPose& pose,const std::array<uint8_t,56>& current,std::array<uint8_t,72>& previous){
 lastTipSweepMetres=0;
 if(!tipSweep||!(pose.tipY>.2f&&pose.tipY<4.f))return;
 WorldPositionQ32 cp,pp;Quat cq,pq;std::memcpy(&cp,current.data(),24);std::memcpy(&cq,current.data()+24,16);std::memcpy(&pp,previous.data(),24);std::memcpy(&pq,previous.data()+24,16);
 {const float a=cq.x*cq.x+cq.y*cq.y+cq.z*cq.z+cq.w*cq.w,b=pq.x*pq.x+pq.y*pq.y+pq.z*pq.z+pq.w*pq.w;if(!(a>.5f&&a<1.5f&&b>.5f&&b<1.5f))return;}
 const Vec3 tip{0,pose.tipY,0};const Vec3 ct=rotate(normalized(cq),tip),pt=rotate(normalized(pq),tip);
 const Vec3 grip{float(relativeQ32(pp.x,cp.x)),float(relativeQ32(pp.y,cp.y)),float(relativeQ32(pp.z,cp.z))};   // previous grip - current grip
 const Vec3 shift=grip+pt-ct;                                                                                 // previous tip - current tip
 const float tipWay=length(shift),gripWay=length(grip);
 if(!std::isfinite(tipWay)||tipWay>1.5f||tipWay<gripWay+.02f)return;                                           // no turn worth it, or a jump
 WorldPositionQ32 start=cp;start.x+=int64_t(double(shift.x)*4294967296.);start.y+=int64_t(double(shift.y)*4294967296.);start.z+=int64_t(double(shift.z)*4294967296.);
 std::memcpy(previous.data(),&start,24);std::memcpy(previous.data()+24,current.data()+24,16);
 lastTipSweepMetres=tipWay;++tipSweeps;
}
// The sweep from the press (2026-10-20, user: "strokes at pots miss although I swung close - like the fix for the enemies,
// can't everything breakable the blade passed during the attack be hit?"). Headset run 20261020-100726: the game's hit
// volume came 80 ms after a stroke's press and lasted 150 ms; its first update stood at the blade without a sweep (the
// previous transform the same as the current), so where the blade went in those 80 ms - with a fast stroke 0.4..0.8 m,
// past a pot 0.5 m across - was never in the volume. The first two updates of a hit volume now sweep from where the tip
// was when the stroke pressed the attack (the shown weapon's pose then, motionColliderChannel::past) to where it is,
// as sweepFromTip does between two updates: what the blade passed from the press on is in the sweep. At most 300 ms
// back and 2 m. EVR_MELEE_CATCH_UP=0: off.
inline const bool catchUp=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_CATCH_UP",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline motion::ColliderKey catchKey{};inline unsigned catchUpdates=0;inline double lastCatchUpMetres=0;inline uint64_t lastCatchUpMs=0;inline std::atomic<unsigned> catchUps{0};
inline void sweepFromPress(const motion::ColliderPose& pose,const motion::ColliderKey& key,bool continuous,const std::array<uint8_t,56>& native,const std::array<uint8_t,56>& current,std::array<uint8_t,72>& previous){
 lastCatchUpMetres=0;lastCatchUpMs=0;
 if(!continuous){catchKey=key;catchUpdates=0;}else if(catchKey==key)++catchUpdates;else return;
 if(!catchUp||catchUpdates>1||!(pose.tipY>.2f&&pose.tipY<4.f))return;
 const auto stroke=controllerUiFixture::strokeMs.load();
 if(!stroke||pose.milliseconds<stroke||pose.milliseconds-stroke>300)return;
 motion::ColliderPose then;if(!motionColliderChannel::pastAt(stroke>10?stroke-10:stroke,pose.item,then)||then.milliseconds>=pose.milliseconds)return;
 const auto was=motion::colliderTransform(then,native);
 WorldPositionQ32 cp,pp;Quat cq,pq;std::memcpy(&cp,current.data(),24);std::memcpy(&cq,current.data()+24,16);std::memcpy(&pp,was.data(),24);std::memcpy(&pq,was.data()+24,16);
 {const float a=cq.x*cq.x+cq.y*cq.y+cq.z*cq.z+cq.w*cq.w,b=pq.x*pq.x+pq.y*pq.y+pq.z*pq.z+pq.w*pq.w;if(!(a>.5f&&a<1.5f&&b>.5f&&b<1.5f))return;}
 const Vec3 tip{0,pose.tipY,0};const Vec3 ct=rotate(normalized(cq),tip),pt=rotate(normalized(pq),tip);
 const Vec3 grip{float(relativeQ32(pp.x,cp.x)),float(relativeQ32(pp.y,cp.y)),float(relativeQ32(pp.z,cp.z))};
 const Vec3 shift=grip+pt-ct;const float way=length(shift);   // the tip then - the tip now
 if(!std::isfinite(way)||way>2.f||way<.05f)return;
 // only when it reaches further back than the sweep the update has already
 WorldPositionQ32 ps;std::memcpy(&ps,previous.data(),24);const Vec3 had{float(relativeQ32(ps.x,cp.x)),float(relativeQ32(ps.y,cp.y)),float(relativeQ32(ps.z,cp.z))};
 if(length(had)>=way)return;
 WorldPositionQ32 start=cp;start.x+=int64_t(double(shift.x)*4294967296.);start.y+=int64_t(double(shift.y)*4294967296.);start.z+=int64_t(double(shift.z)*4294967296.);
 std::memcpy(previous.data(),&start,24);std::memcpy(previous.data()+24,current.data()+24,16);
 lastCatchUpMetres=way;lastCatchUpMs=pose.milliseconds-then.milliseconds;++catchUps;
}
struct PoseSeen {uint32_t item=0;uint64_t serial=0;int64_t predictedTime=0;WorldPositionQ32 position{};bool valid=false;};
inline PoseSeen poseSeen;inline double lastLeadMetres=0;inline std::atomic<unsigned> leadApplied{0};
inline void leadPosition(const motion::ColliderPose& pose,std::array<uint8_t,56>& current){
 lastLeadMetres=0;WorldPositionQ32 at;std::memcpy(&at,current.data(),24);
 const bool continuous=poseSeen.valid&&poseSeen.item==pose.item&&pose.serial>poseSeen.serial&&pose.predictedTime>poseSeen.predictedTime&&pose.predictedTime-poseSeen.predictedTime<=100000000;
 if(continuous&&colliderLeadMs){
  const double dt=double(pose.predictedTime-poseSeen.predictedTime)*1e-9,lead=colliderLeadMs*1e-3;
  const double vx=relativeQ32(at.x,poseSeen.position.x)/dt,vy=relativeQ32(at.y,poseSeen.position.y)/dt,vz=relativeQ32(at.z,poseSeen.position.z)/dt;
  const double speed=std::sqrt(vx*vx+vy*vy+vz*vz);
  if(std::isfinite(speed)&&speed>.5&&speed<15){double metres=speed*lead;const double cap=.15;const double k=metres>cap?cap/metres:1.;metres*=k;
   at.x+=int64_t(vx*lead*k*4294967296.);at.y+=int64_t(vy*lead*k*4294967296.);at.z+=int64_t(vz*lead*k*4294967296.);
   std::memcpy(current.data(),&at,24);lastLeadMetres=metres;++leadApplied;}
 }
 WorldPositionQ32 seen;std::memcpy(&seen,pose.transform.data(),24);poseSeen={pose.item,pose.serial,pose.predictedTime,seen,true};
}
inline std::mutex mutex;inline bool accepting=true;inline motion::ColliderLeaseLedger<8> lease;
inline std::atomic<unsigned> outstanding{0},opened{0},closed{0},applied{0},cancelled{0},failures{0},ignored{0};
inline thread_local game::ScopedQueryTransforms borrow;
inline thread_local game::ScopedQueryTransforms::Query sourceQuery{};
inline thread_local std::array<uint8_t,56> sourceCurrent;
inline thread_local std::array<uint8_t,72> sourcePrevious;
inline thread_local motion::ColliderKey scopeKey;
inline thread_local unsigned readySample=0;
inline thread_local bool activeScope=false,cancelScope=false,lookupSeen=false;
using Lookup=void*(__fastcall*)(void*,uint32_t,uint8_t,const void*);
inline Lookup originalLookup=nullptr;inline bool lookupEnabled=false;inline std::atomic<unsigned> lookupCallbacks{0};
constexpr uintptr_t lookupRva=0x213930;
constexpr const char* lookupPattern="40 53 48 83 ec 20 49 8b d9 e8 ? ? ? ?";
inline bool owns(uintptr_t world,uint32_t entity,const std::array<uint8_t,24>& event){std::lock_guard<std::mutex> lock(mutex);const auto key=combatEpochGate::collider({world,entity,event});return requested&&(live?liveLease.owns(key,GetTickCount64()):lease.owns(key));}
__declspec(noinline) void* __fastcall lookup(void* registry,uint32_t sequence,uint8_t sub,const void* guid){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 struct Guard {Guard(){++lookupCallbacks;}~Guard(){--lookupCallbacks;}} guard;
 auto* result=originalLookup(registry,sequence,sub,guid);const ObserverLastError preserve;
 if(!activeScope||!cancelScope||caller!=0x3eff42)return result;
 try{
  uint32_t expected=0;std::memcpy(&expected,scopeKey.event.data(),4);
  const auto bytes=read<std::array<uint8_t,16>>(reinterpret_cast<uintptr_t>(guid));
  const bool matched=expected==sequence&&scopeKey.event[4]==sub&&!std::memcmp(scopeKey.event.data()+8,bytes.data(),16);
  if(!matched)++failures;lookupSeen=true;
  logFile->event("motion_collider_lookup_suppressed","\"ready_sample\":"+std::to_string(readySample)+",\"entity\":"+std::to_string(scopeKey.entity)+",\"world\":"+hex(scopeKey.world)+",\"caller\":\"0x3eff42\",\"original_resource_present\":"+(result?"true":"false")+",\"event_matches\":"+(matched?"true":"false")+",\"returned_null\":true");
 }catch(...){++failures;}
 // This exact caller belongs to the owned entity iteration even on log/read
 // failure; returning null asks native code to remove only that query's collider.
 return nullptr;
}
inline void install(){
 const auto bytes=parsePattern(lookupPattern);const auto call=baseAddress+0x3eff3d;
 if(scan(reinterpret_cast<const uint8_t*>(baseAddress+lookupRva),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+lookupRva)throw std::runtime_error("collider cancellation lookup signature/caller differs");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+lookupRva),reinterpret_cast<void*>(&lookup),reinterpret_cast<void**>(&originalLookup))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+lookupRva))!=MH_OK)throw std::runtime_error("collider cancellation lookup hook failed");
 lookupEnabled=true;logFile->event("motion_collider_lookup_enabled","\"rva\":\"0x213930\",\"pattern\":"+jsonString(lookupPattern)+",\"caller\":\"0x3eff42\",\"cancel_after\":"+std::to_string(cancelAfter));
}
// remote: another player's weapon as their mod sent it (sequence_collider_observer.hpp remoteMelee) - no lead from the
// local weapon's last poses and no sweep from the tip (its length is not sent).
inline void begin(uintptr_t world,uint32_t entity,void* query,unsigned sample,const motion::ColliderPose& pose,bool ownedIntent=false,bool remote=false){
 if(!requested)return;std::lock_guard<std::mutex> lock(mutex);if(!accepting)return;
 if(activeScope||borrow.active())throw std::runtime_error("nested tracked collider scope");
 auto& q=*reinterpret_cast<game::ScopedQueryTransforms::Query*>(query);const auto id=read<uint64_t>(q[5]+8);
 const auto key=combatEpochGate::collider({world,entity,read<std::array<uint8_t,24>>(q[4])});
 if(live){
  if(!pose.valid||!combatEpochGate::newAllowed(combatEpochGate::Stage::Collider,pose.clientWorld,world)){++ignored;return;}
  sourceCurrent=read<std::array<uint8_t,56>>(q[1]);sourcePrevious=q[3]?read<std::array<uint8_t,72>>(q[3]):std::array<uint8_t,72>{};
  auto current=motion::colliderTransform(pose,sourceCurrent);if(!remote)leadPosition(pose,current);enlarge(current);
  const auto decision=liveLease.apply(key,true,GetTickCount64(),current);
  if(!decision.track){++ignored;return;}
  impactLifetime::tracked(world,entity,key.event);
  sourceQuery=q;scopeKey=key;cancelScope=false;lookupSeen=false;readySample=sample;
  // The game sweeps from the previous transform: the tracked weapon's own previous pose, or no sweep on the first update.
  auto previous=sourcePrevious;if(q[3]){std::memcpy(previous.data(),decision.previous.data(),52);if(decision.continuous&&!remote)sweepFromTip(pose,current,previous);
   if(!remote)sweepFromPress(pose,key,decision.continuous,sourceCurrent,current,previous);}
  borrow.open(q,current,previous);activeScope=true;++outstanding;++opened;++applied;if(decision.continuous)++liveContinuous;
  if(liveLogged.fetch_add(1)<400||!(liveLogged.load()%20))logFile->event("motion_collider_apply_begin","\"ready_sample\":"+std::to_string(sample)+",\"entity\":"+std::to_string(entity)+",\"frame\":"+std::to_string(pose.frame)+",\"collider_before\":"+std::to_string(id)+
   ",\"original_current\":"+jsonString(nativeImpactComponents::asHex(sourceCurrent))+",\"applied_current\":"+jsonString(nativeImpactComponents::asHex(current))+
   ",\"original_previous\":"+jsonString(nativeImpactComponents::asHex(sourcePrevious))+",\"applied_previous\":"+jsonString(nativeImpactComponents::asHex(previous))+
   ",\"previous_present\":"+(q[3]?"true":"false")+",\"live\":true,\"continuous\":"+(decision.continuous?"true":"false")+",\"item\":"+std::to_string(pose.item)+",\"hit_scale\":"+std::to_string(hitScale)+",\"tip_y\":"+std::to_string(pose.tipY)+",\"tip_sweep_metres\":"+std::to_string(lastTipSweepMetres)+",\"from_press_metres\":"+std::to_string(remote?0.:lastCatchUpMetres)+",\"from_press_ms\":"+std::to_string(remote?0:lastCatchUpMs)+",\"lead_ms\":"+std::to_string(colliderLeadMs)+",\"lead_metres\":"+std::to_string(remote?0.:lastLeadMetres)+",\"remote\":"+(remote?"true":"false")+",\"ecs_written_by_mod\":false");
  return;
 }
 const bool forced=cancelAfter&&applied.load()>=cancelAfter;
 const auto decision=lease.decide(key,pose.valid&&!forced&&combatEpochGate::newAllowed(combatEpochGate::Stage::Collider,pose.clientWorld,world),id,ownedIntent);
 if(decision==motion::ColliderDecision::Ignore){++ignored;return;}
 contactLifetimeObserver::track(key,sample);
 sourceQuery=q;scopeKey=key;sourceCurrent=read<std::array<uint8_t,56>>(q[1]);sourcePrevious=q[3]?read<std::array<uint8_t,72>>(q[3]):std::array<uint8_t,72>{};
 cancelScope=decision==motion::ColliderDecision::Cancel;lookupSeen=false;readySample=sample;
 if(!cancelScope){
  const auto current=motion::colliderTransform(pose,sourceCurrent);auto previous=sourcePrevious;
  if(!id&&q[3])std::memcpy(previous.data(),current.data(),52);
  borrow.open(q,current,previous);activeScope=true;++outstanding;++opened;++applied;
  logFile->event("motion_collider_apply_begin","\"ready_sample\":"+std::to_string(sample)+",\"entity\":"+std::to_string(entity)+",\"frame\":"+std::to_string(pose.frame)+",\"collider_before\":"+std::to_string(id)+
   ",\"original_current\":"+jsonString(nativeImpactComponents::asHex(sourceCurrent))+",\"applied_current\":"+jsonString(nativeImpactComponents::asHex(current))+
   ",\"original_previous\":"+jsonString(nativeImpactComponents::asHex(sourcePrevious))+",\"applied_previous\":"+jsonString(nativeImpactComponents::asHex(previous))+
   ",\"previous_present\":"+(q[3]?"true":"false")+",\"initial_sweep_reset\":"+(!id?"true":"false")+",\"ecs_written_by_mod\":false"+combatEpochGate::stampFields(key.epoch));
 }else{
  activeScope=true;++outstanding;++opened;++cancelled;
  logFile->event("motion_collider_cancel_begin","\"ready_sample\":"+std::to_string(sample)+",\"entity\":"+std::to_string(entity)+",\"world\":"+hex(world)+",\"collider_before\":"+std::to_string(id)+",\"owned_intent\":"+(ownedIntent?"true":"false")+",\"forced_fixture\":"+(forced?"true":"false")+",\"pose_eligible\":"+(pose.valid?"true":"false")+",\"latched\":true,\"ecs_written_by_mod\":false"+combatEpochGate::stampFields(key.epoch));
 }
}
inline void finish(void* query) noexcept {
 if(!activeScope)return;
 game::QueryReturnCompletion completion(outstanding,closed);
 try{
  auto& actual=*reinterpret_cast<game::ScopedQueryTransforms::Query*>(query);
  bool unchanged=false;try{unchanged=sourceCurrent==read<std::array<uint8_t,56>>(sourceQuery[1])&&(!sourceQuery[3]||sourcePrevious==read<std::array<uint8_t,72>>(sourceQuery[3]));}catch(...){}
  const bool restored=borrow.active()?borrow.close(actual):actual==sourceQuery;
  completion.markReturned();
  const bool wasCancelled=cancelScope,observed=lookupSeen;
  const auto collider=read<uint64_t>(sourceQuery[5]+8);{std::lock_guard<std::mutex> lock(mutex);if(!(live?liveLease.returned(scopeKey,collider):lease.returned(scopeKey,collider)))++failures;}
  if(!restored||!unchanged||(wasCancelled&&(!observed||collider)))++failures;
  if(!live||liveLogged.load()<=400||!(liveLogged.load()%20)||!restored||!unchanged)logFile->event(wasCancelled?"motion_collider_cancel_end":"motion_collider_apply_end","\"ready_sample\":"+std::to_string(readySample)+",\"collider_after\":"+std::to_string(collider)+",\"query_restored\":"+(restored?"true":"false")+",\"original_components_unchanged\":"+(unchanged?"true":"false")+",\"lookup_suppressed\":"+(observed?"true":"false")+",\"ecs_written_by_mod\":false");
 }catch(...){++failures;}
 if(completion.returned()){activeScope=false;cancelScope=false;}
 // completion publishes zero only after every native read and failure report.
}
inline void drain(){
 if(!requested)return;{std::lock_guard<std::mutex> lock(mutex);if(drained)return;accepting=false;}
 const auto deadline=GetTickCount64()+2000;while(outstanding.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 logFile->event("motion_collider_apply_drained","\"opened\":"+std::to_string(opened.load())+",\"closed\":"+std::to_string(closed.load())+",\"outstanding\":"+std::to_string(outstanding.load())+",\"failures\":"+std::to_string(failures.load())+",\"fallbacks\":0,\"applied\":"+std::to_string(applied.load())+",\"cancelled\":"+std::to_string(cancelled.load())+",\"ignored\":"+std::to_string(ignored.load())+",\"live\":"+(live?"true":"false")+",\"continuous_sweeps\":"+std::to_string(liveContinuous.load()));
 if(outstanding.load())throw std::runtime_error("tracked collider query not returned; iterator hook retained");
 if(lookupEnabled){
  if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+lookupRva))!=MH_OK)throw std::runtime_error("collider lookup disable failed");lookupEnabled=false;
  const auto until=GetTickCount64()+500;while(lookupCallbacks.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  const auto pattern=parsePattern(lookupPattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+lookupRva),pattern.size(),pattern)==std::vector<size_t>{0};
  logFile->event("motion_collider_lookup_removed",std::string("\"restored\":")+(restored?"true":"false")+",\"callbacks\":"+std::to_string(lookupCallbacks.load()));
  if(!restored||lookupCallbacks.load())throw std::runtime_error("collider lookup did not restore/quiesce");
 }
 {std::lock_guard<std::mutex> lock(mutex);drained=true;}
}
}
