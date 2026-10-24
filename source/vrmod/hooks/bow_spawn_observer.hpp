// Mock-only Crosshair witness; opt-in tracked aim uses a private context/input
// for a single authorized release. Original game components and event stay intact.
namespace bowSpawnObserver {
using Fn=bool(__fastcall*)(void*,void*,const void*,const void*);
inline Fn original=nullptr;inline bool enabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},calls{0},samples{0},failures{0};
using DispatchFn=bool(__fastcall*)(void*,void*,const void*,uint8_t,const void*,const void*);
inline DispatchFn dispatchOriginal=nullptr;inline bool dispatchEnabled=false;
constexpr uintptr_t dispatchRva=0x21e810,spawnParentRva=0x2252cd;
constexpr const char* dispatchPattern="44 88 4c 24 20 55 53 56 41 54 41 55 41 56 41 57 48 8d 6c 24 80 48 81 ec";
struct Parent {uintptr_t caller=0;const void* context=nullptr;const void* event=nullptr;unsigned sample=0;bool crosshairOk=false;std::array<uint8_t,56> transform{};inputRanged::BowNativeIdentity identity{};uint8_t mode=0;};
struct PendingSubmit {const void* params=nullptr;unsigned sample=0;std::array<uint8_t,56> transform{};inputRanged::BowNativeIdentity identity{};};
inline thread_local PendingSubmit pendingSubmit;
using SubmitFn=bool(__fastcall*)(void*,const void*);inline SubmitFn submitOriginal=nullptr;inline bool submitEnabled=false;
constexpr uintptr_t submitRva=0x2213d0,submitCaller=0x2252f7;
constexpr const char* submitPattern="40 55 56 41 54 41 56 48 8d ac 24 28 9a fe ff b8 d8 66 01 00 e8 c7 c1 10";   // the call of the stack probe: its rel32 moves with every build
inline std::atomic<unsigned> dispatchSamples{0},submitSamples{0};
inline thread_local Parent parent;
constexpr uintptr_t rva=0x21f6f0,callerRva=0x21ed52;
constexpr const char* pattern="48 8b c4 48 81 ec d8 00 00 00 f3 0f 10 25 ? ? ? ? 4c 8b d1 f3 0f 10";
struct Scope{Scope(){callbacks.fetch_add(1);}~Scope(){callbacks.fetch_sub(1);}};
inline void failure(const std::exception& e);
// The bow flies where the hand aims while locked on too (2026-10-24, user after the headset: "while locked on, the bow flies to the
// locked target - make it always fly where aimed; the wand and the staff do the same, and those are fine as they are"). Headset log
// 20261024-151525: 24 bow shots, the tracked aim applied on all 11 without a lock (camera 7) and was never asked on the 13 locked
// ones (camera 5) - no call of the crosshair placement (0x21f6f0, from 0x21ed52) at all. The spawn dispatch (0x21e810) takes the
// placement by the ranged weapon's shallPlayerTurnTowardsTarget (+0xc4, inputRanged::rangedTurn): 0x21ed78 cmp [weapon+0xc4],0 ->
// the crosshair (0x21f6f0) when clear, the locked target's (0x21f9c0) when set. For a bow's shot of the own player (or another
// player's, whose mod sends its aim) the flag is cleared for the dispatch alone and put back right after it: the crosshair placement
// runs, with the hand's aim (observe below). EVR_LOCKED_BOW_TO_HAND=0: the game's way (to the locked target).
struct TargetFlagAside {uintptr_t at=0;uint8_t was=0;~TargetFlagAside(){if(at)*reinterpret_cast<volatile uint8_t*>(at)=was;}};
inline void lockedBowAside(uintptr_t callerRva,const void* context,TargetFlagAside& aside) noexcept;
__declspec(noinline) bool __fastcall dispatchObserve(void* out,void* nativeContext,const void* context,uint8_t mode,const void* event,const void* other){
 const Scope scope;const auto saved=parent;
 parent={reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress,context,event};parent.mode=mode;
 bool result=false;
 {TargetFlagAside aside;lockedBowAside(parent.caller,context,aside);
  result=dispatchOriginal(out,nativeContext,context,mode,event,other);}
 {const ObserverLastError preserve;if(parent.sample){try{
  const auto value=read<std::array<uint8_t,240>>(reinterpret_cast<uintptr_t>(out));
  const bool same=parent.crosshairOk&&!std::memcmp(value.data()+0x18,parent.transform.data(),52);
  logFile->event("bow_spawn_dispatch_result","\"sample\":"+std::to_string(parent.sample)+",\"params\":"+hex(reinterpret_cast<uintptr_t>(out))+
   ",\"result\":"+(result?"true":"false")+",\"same_transform\":"+(same?"true":"false")+",\"params_bytes\":"+jsonString(nativeImpactComponents::asHex(value))+",\"read_only\":true");
  ++dispatchSamples;if(result&&same)pendingSubmit={out,parent.sample,parent.transform,parent.identity};
 }catch(const std::exception& e){failure(e);}}}
 parent=saved;return result;
}
__declspec(noinline) bool __fastcall submitObserve(void* context,const void* params){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const Scope scope;
 const auto pending=pendingSubmit;pendingSubmit={};bool record=false;std::array<uint8_t,240> before{};
 uintptr_t eventContext=0,actor=0;uint32_t oldEntity=0;bool actorMatched=false;uint64_t identityAge=UINT64_MAX;

 {const ObserverLastError preserve;if(accepting&&caller==baseAddress+submitCaller&&pending.sample&&pending.params==params){try{
  before=read<decltype(before)>(reinterpret_cast<uintptr_t>(params));record=!std::memcmp(before.data()+0x18,pending.transform.data(),52);
  if(record){
   std::memcpy(&eventContext,before.data(),8);
   // Dereference only the context passed into this live native call, never a cached World.
   actor=eventContext?read<uintptr_t>(eventContext+0x108):0;
   const auto& id=pending.identity;const auto current=inputRanged::bowIdentity(id.input,id.weapon);const auto now=GetTickCount64();
   identityAge=now>=current.milliseconds?now-current.milliseconds:UINT64_MAX;
   actorMatched=actor&&actor==id.actor&&actor==current.actor&&id.world&&id.world==current.world&&id.entity&&id.entity==current.entity&&
       id.input==current.input&&id.weapon==current.weapon&&current.milliseconds&&identityAge<=100;
   if(actorMatched)oldEntity=read<uint32_t>(actor+0xd2c);
  }

 }catch(const std::exception& e){failure(e);}}}
 const bool result=submitOriginal(context,params);
 {const ObserverLastError preserve;if(record){try{
  const bool same=before==read<decltype(before)>(reinterpret_cast<uintptr_t>(params));++submitSamples;
  const bool contextStable=eventContext&&read<uintptr_t>(reinterpret_cast<uintptr_t>(params))==eventContext;
  const bool actorStable=actorMatched&&contextStable&&read<uintptr_t>(eventContext+0x108)==actor;
  const auto newEntity=actorStable?read<uint32_t>(actor+0xd2c):0;
  if(result&&actorStable&&newEntity&&newEntity!=oldEntity)inputRanged::publishSpawn(pending.identity.world,actor,pending.identity.entity,newEntity,pending.sample);
  logFile->event("bow_spawn_entity_observed","\"sample\":"+std::to_string(pending.sample)+",\"params\":"+hex(reinterpret_cast<uintptr_t>(params))+
    ",\"event_context\":"+hex(eventContext)+",\"actor\":"+hex(actor)+",\"observed_actor\":"+hex(pending.identity.actor)+
    ",\"native_world\":"+hex(pending.identity.world)+",\"native_entity\":"+std::to_string(pending.identity.entity)+
    ",\"input\":"+hex(pending.identity.input)+",\"weapon\":"+hex(pending.identity.weapon)+",\"identity_age_ms\":"+std::to_string(identityAge)+
    ",\"actor_matched\":"+(actorMatched?"true":"false")+",\"actor_stable\":"+(actorStable?"true":"false")+
    ",\"previous_spawned_entity\":"+std::to_string(oldEntity)+",\"spawned_entity\":"+std::to_string(newEntity)+
    ",\"native_result\":"+(result?"true":"false")+",\"source_unchanged\":"+(same?"true":"false")+
    ",\"read_only\":true,\"field_offset\":3352");

  logFile->event("bow_spawn_submit_result","\"sample\":"+std::to_string(pending.sample)+",\"params\":"+hex(reinterpret_cast<uintptr_t>(params))+
   ",\"result\":"+(result?"true":"false")+",\"source_unchanged\":"+(same?"true":"false")+",\"read_only\":true");
 }catch(const std::exception& e){failure(e);}}}
 return result;
}
inline void failure(const std::exception& e){if(failures.fetch_add(1)==0)logFile->event("bow_spawn_failure","\"reason\":"+jsonString(e.what()));}
// Staff casts reach the Crosshair placement from another event branch of the same
// dispatch (survey game233325: return address 1e8f28, event without the projectile flag).
constexpr uintptr_t staffParentRva=0x224c68;
// A tracked shot must start near the player's own native aim focus. Measured in Mock: bow
// 1.15 m, staff emitter 1.25 m, wand 0.72 m. An outstretched arm with a long staff, or standing
// off-centre in the room, adds to that, so play uses 3 m; the one-shot diagnostic keeps 1.5 m.
constexpr float aimFocusLimit=3.f;
// Other players' shots (observe below). EVR_REMOTE_SHOTS=0: as before (a guest's arrow went the tracked branch).
inline const bool remoteShotsWanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_SHOTS",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline const bool lockedBowToHand=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_LOCKED_BOW_TO_HAND",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<unsigned> lockedBowShots{0};
inline void lockedBowAside(uintptr_t callerRva,const void* context,TargetFlagAside& aside) noexcept {
 if(!lockedBowToHand||!accepting.load()||callerRva!=spawnParentRva||!context)return;
 try{
  const auto c=reinterpret_cast<uintptr_t>(context);const auto weapon=read<uintptr_t>(c+0xb8);if(!weapon)return;
  const auto flag=weapon+inputRanged::rangedTurn;const uint8_t was=read<uint8_t>(flag);if(!was)return;
  const uint32_t item=read<uint32_t>(weapon+inputRanged::rangedItem);
  if(!(item==327157552u||motion::catalogBowItem(item)))return;   // bows only: the wand and the staff keep the game's way
  const bool own=inputRanged::bowIdentity(read<uintptr_t>(c+0x88),weapon).world!=0;
  if(!(own?controllerUiFixture::bowTrackedAimRequested:remoteShotsWanted))return;
  aside.at=flag;aside.was=was;*reinterpret_cast<volatile uint8_t*>(flag)=0;   // put back by aside's end, right after the dispatch
  const auto n=++lockedBowShots;
  try{if(n<=20||n%50==0)logFile->event("bow_locked_shot_to_hand","\"shots\":"+std::to_string(n)+",\"item\":"+std::to_string(item)+",\"own\":"+(own?"true":"false")+",\"target_flag\":"+std::to_string(unsigned(was)));}catch(...){}
 }catch(...){}
}
// A host without VR (view_resource_probe.cpp onlineFlat, 2026-10-22): every shot is looked at for the other players' aims,
// and the staff's casts too - the own ones keep the game's aim (no tracked hands).
inline bool remoteAlways=false;
inline constexpr uint64_t remoteShotMs=1000,remoteHeldMs=300;inline unsigned remoteLogs=0;
// The nearest new aim another player's mod sent (controllerUiFixture::remoteShots) whose start lies within aimFocusLimit
// of this input's camera focus; a shot is used up, a held aim stays. Returns why none was taken, or "applied".
inline const char* remoteShot(const std::array<uint8_t,44>& camera,uint64_t now,motion::BowWorldAim& aim,float& nearest,uint64_t& from,uint64_t& age,uint16_t& id,unsigned& fresh,bool& held,bool heldOnly=false){
 WorldPositionQ32 nativePosition;Quat nativeQ;float nativeDistance;
 std::memcpy(&nativePosition,camera.data(),24);std::memcpy(&nativeQ,camera.data()+24,16);std::memcpy(&nativeDistance,camera.data()+40,4);
 const auto focus=cameraPosition(nativePosition,rotate(normalized(nativeQ),{0,0,std::isfinite(nativeDistance)?nativeDistance:0.f}),1);
 std::lock_guard<std::mutex> lock(controllerUiFixture::remoteShotMutex);controllerUiFixture::RemoteShot* best=nullptr;float limit=aimFocusLimit;
 for(auto& s:controllerUiFixture::remoteShots){
  if(s.used||now<s.milliseconds||now-s.milliseconds>(s.held?remoteHeldMs:remoteShotMs)||(heldOnly&&!s.held))continue;++fresh;
  WorldPositionQ32 at;std::memcpy(&at,s.aim.data(),24);
  const Vec3 offset{float(relativeQ32(at.x,focus.x)),float(relativeQ32(at.y,focus.y)),float(relativeQ32(at.z,focus.z))};
  const float d=length(offset);if(finite(offset)&&d<=limit){limit=d;best=&s;}}
 if(!best)return fresh?"origin_range":"none";
 if(!best->held)best->used=true;
 std::memcpy(&aim.origin,best->aim.data(),24);std::memcpy(&aim.orientation,best->aim.data()+24,16);aim.orientation=normalized(aim.orientation);aim.direction=rotate(aim.orientation,{0,0,1});
 nearest=limit;from=best->from;age=now-best->milliseconds;id=best->id;held=best->held;return "applied";
}
// VHVR staff/wand: the newest same-frame weapon pose and pointer ray, accepted only for
// the local player's own reviewed weapon while it is in its native firing state.
// twoHanded: the staff was held with both hands on that frame and the shot follows the weapon axis.
inline bool rangedAim(uint32_t nativeItem,const inputRanged::BowNativeIdentity& identity,uint8_t nativeState,const std::array<uint8_t,44>& camera,motion::BowWorldAim& aim,uint64_t& frame,const char*& reason,bool& twoHanded){
 twoHanded=false;
 const auto* weapon=motion::rangedAimModel(nativeItem);if(!weapon){reason="item";return false;}
 const auto now=GetTickCount64();uint32_t cachedItem=0;std::memcpy(&cachedItem,identity.bytes.data()+inputRanged::rangedItem,4);
 // The game's shot state (3); a wand's shot the mod handed early (motion_event_selection_observer.hpp) comes before it, in the
 // state the game had (as the bow's and the throw's early ones).
 const bool early=now<=controllerUiFixture::rangedEarlySpawnUntil.load()&&nativeState==identity.bytes[0];
 if(!identity.world||!identity.entity||!identity.milliseconds||now<identity.milliseconds||now-identity.milliseconds>100||cachedItem!=nativeItem||(nativeState!=3&&!early)){reason="native_identity";return false;}
 motion::HandFrame hands;motion::BowPhysicalGeometry g;uint32_t item=0;uint64_t received=0;bool both=false;
 {std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);hands=controllerUiFixture::gestureHands;g=controllerUiFixture::gestureRangedGeometry;item=controllerUiFixture::gestureRangedItem;received=controllerUiFixture::gestureRangedMs;both=controllerUiFixture::gestureRangedTwoHanded;}
 frame=hands.frame;const auto& right=hands.hands[1];
 // One hand needs the pointer ray; two hands need both hands tracked (the pose already follows them).
 if(item!=nativeItem||!g.binding||g.entity!=identity.entity||g.frame!=hands.frame||!received||now<received||now-received>200||!hands.focused||!right.tracked()||
    (both?!hands.hands[0].tracked():!right.aimActive||(right.aimFlags&15)!=15)){reason="tracked_scope";return false;}
 twoHanded=both;
 aim=both?motion::rangedAxisAim(*weapon,g.model,g.scale,g.worldPosition,g.worldOrientation):
     motion::rangedPointerAim(*weapon,g.model,g.scale,g.worldPosition,g.worldOrientation,right.aim);
 // As for the bow: the shot must start near the player's own native aim focus.
 WorldPositionQ32 nativePosition;Quat nativeQ;float nativeDistance;
 std::memcpy(&nativePosition,camera.data(),24);std::memcpy(&nativeQ,camera.data()+24,16);std::memcpy(&nativeDistance,camera.data()+40,4);
 const auto focus=cameraPosition(nativePosition,rotate(normalized(nativeQ),{0,0,nativeDistance}),1);
 const Vec3 offset{float(relativeQ32(aim.origin.x,focus.x)),float(relativeQ32(aim.origin.y,focus.y)),float(relativeQ32(aim.origin.z,focus.z))};
 if(!finite(offset)||length(offset)>aimFocusLimit){reason="origin_range";return false;}
 reason="applied";return true;
}
inline void logRangedAim(unsigned sample,uint32_t item,bool applied,const char* reason,uint64_t frame,const motion::BowWorldAim& aim,const char* path,bool twoHanded){
 std::ostringstream s;s<<std::setprecision(10)<<"\"sample\":"<<sample<<",\"item\":"<<item<<",\"path\":\""<<path<<"\",\"applied\":"<<(applied?"true":"false")<<",\"reason\":\""<<reason<<"\",\"frame\":"<<frame
  <<",\"two_handed\":"<<(twoHanded?"true":"false")
  <<",\"aim_q32\":["<<aim.origin.x<<','<<aim.origin.y<<','<<aim.origin.z<<"],\"aim_direction\":["<<aim.direction.x<<','<<aim.direction.y<<','<<aim.direction.z<<']';
 logFile->event("ranged_aim_call",s.str());
}
__declspec(noinline) bool __fastcall observe(void* output,void* nativeContext,const void* context,const void* event){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const Scope scope;
 std::string pending,survey,cast;uintptr_t input=0;std::array<uint8_t,44> camera{};
 std::array<uint8_t,192> ctx{},privateContext{};std::array<uint8_t,336> evt{};
 std::array<uint8_t,1320> sourceInput{},privateInput{};std::array<uint8_t,44> effectiveCamera{};
 bool aimed=false,rejected=false;const void* callContext=context;
 {const ObserverLastError preserve;
  // The one-shot witness stops after 64 calls. The repeatable draw route must keep
  // aiming every shot of a session, so it is not counted out.
  if(accepting.load()&&caller==baseAddress+callerRva&&(calls.fetch_add(1)<64||controllerUiFixture::bowDrawRequested||controllerUiFixture::rangedAimRequested||controllerUiFixture::liveHandUseRequested||remoteAlways)){try{
   const auto c=reinterpret_cast<uintptr_t>(context),v=reinterpret_cast<uintptr_t>(event);
   // Type hash belongs to the outer BlobVariant, not this resolved payload.
   // 1e951d is exclusively the 51e20bf3 branch (native byte signatures checked).
   if(parent.caller==spawnParentRva&&parent.context==context&&parent.event==event){
    ctx=read<decltype(ctx)>(c);evt=read<decltype(evt)>(v);input=read<uintptr_t>(c+0x88);
    camera=read<decltype(camera)>(input+0x28);
    const auto weapon=read<uintptr_t>(c+0xb8);const auto identity=inputRanged::bowIdentity(input,weapon);
    const auto nativeWeapon=read<std::array<uint8_t,inputRanged::rangedWeaponBytes>>(weapon);const auto now=GetTickCount64();
    uint32_t nativeItem=0,cachedItem=0;std::memcpy(&nativeItem,nativeWeapon.data()+inputRanged::rangedItem,4);std::memcpy(&cachedItem,identity.bytes.data()+inputRanged::rangedItem,4);
    // The reviewed bow; with the draw route every bow of the catalog (shared skeleton, weapon_mount.hpp).
    // The shot still needs this frame's published bow geometry, which exists only for a shown, bound bow.
    const bool bowItem=nativeItem==327157552u||(controllerUiFixture::bowDrawRequested&&motion::catalogBowItem(nativeItem));
    // The game's shot state (3); an arrow the mod handed early (motion_event_selection_observer.hpp bowEarly) comes before it.
    const bool early=now<=controllerUiFixture::bowEarlySpawnUntil.load();
    const bool freshIdentity=identity.world&&identity.entity&&identity.milliseconds&&now>=identity.milliseconds&&now-identity.milliseconds<=100&&
      bowItem&&cachedItem==nativeItem&&((nativeWeapon[0]==3&&identity.bytes[0]==3)||(early&&nativeWeapon[0]==identity.bytes[0]));
    parent.identity=freshIdentity?identity:inputRanged::BowNativeIdentity{};
    parent.sample=samples.fetch_add(1)+1;
    effectiveCamera=camera;
    // Another player's shot (2026-10-21): an input of this server without the own player's record (input_ranged_observer:
    // it is kept for the own character only) - a guest's arrow or thrown item. It is never dropped (before, the tracked
    // branch below took it for an own shot whose aim was missing: 8 and 15 arrows of the guest were not made in the
    // session of 10-21 00:25). The camera the guest's mod sent at its release (online_sync.hpp shotAction) is given to
    // it when one came within the last second from near this input's camera (a wand's held aim within remoteHeldMs);
    // otherwise the guest's game's own camera.
    const bool otherInput=!identity.world&&evt[0x148]==1&&remoteShotsWanted;
    if(otherInput){
     motion::BowWorldAim remote;float nearest=-1;uint64_t from=0,age=0;uint16_t id=0;unsigned fresh=0;bool held=false;
     const char* reason=remoteShot(camera,now,remote,nearest,from,age,id,fresh,held);
     // live_melee_targets.hpp steerArrows looks for the new arrow - a bow's only, and only when that guest's mod sent the
     // release's aim (a mod that flies its arrows itself and reports their hits; an older one does not, and its arrows
     // fly here as they are): not a wand's shot or a thrown item (those the host's server makes and hits as they come).
     if(from&&!held&&(nativeItem==327157552u||motion::catalogBowItem(nativeItem))){
      controllerUiFixture::remoteSpawnOrigin[0]=remote.origin.x;controllerUiFixture::remoteSpawnOrigin[1]=remote.origin.y;controllerUiFixture::remoteSpawnOrigin[2]=remote.origin.z;
      controllerUiFixture::remoteSpawnDirection[0]=remote.direction.x;controllerUiFixture::remoteSpawnDirection[1]=remote.direction.y;controllerUiFixture::remoteSpawnDirection[2]=remote.direction.z;
      controllerUiFixture::remoteSpawnAt=now;}
     if(from)try{effectiveCamera=motion::bowAimCamera(remote,evt);}catch(const std::exception&){from=0;reason="event";}
     if(from){
      sourceInput=read<decltype(sourceInput)>(input);privateInput=sourceInput;privateContext=ctx;
      std::memcpy(privateInput.data()+0x28,effectiveCamera.data(),44);
      const auto pointer=reinterpret_cast<uintptr_t>(privateInput.data());std::memcpy(privateContext.data()+0x88,&pointer,8);
      callContext=privateContext.data();aimed=true;}
     if(remoteLogs++<400)logFile->event("bow_remote_aim","\"sample\":"+std::to_string(parent.sample)+",\"item\":"+std::to_string(nativeItem)+",\"applied\":"+(aimed?"true":"false")+
      ",\"reason\":\""+reason+"\",\"held\":"+(held?"true":"false")+",\"age_ms\":"+std::to_string(age)+",\"id\":"+std::to_string(id)+
      ",\"distance\":"+std::to_string(nearest)+",\"fresh\":"+std::to_string(fresh));
    }else if(controllerUiFixture::bowTrackedAimRequested&&bowItem&&evt[0x148]==1){
     motion::HandFrame hands;motion::BowPhysicalGeometry geometry;uint64_t received=0;
     {std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);hands=controllerUiFixture::gestureHands;geometry=controllerUiFixture::gestureBowGeometry;received=controllerUiFixture::gestureMs;}
     uint32_t ammo=0,cachedAmmo=0;uint16_t missing=0;std::memcpy(&ammo,nativeWeapon.data()+inputRanged::rangedAmmoInstance+8,4);std::memcpy(&cachedAmmo,identity.bytes.data()+inputRanged::rangedAmmoInstance+8,4);std::memcpy(&missing,nativeWeapon.data()+inputRanged::rangedUsageMask,2);   // 1.0: +0x70 / +0x7c (June +0x60 / +0x6c)
     motion::BowTrackedShotValue shot;bool consumed=false;
     // The one-shot diagnostic was reviewed with one arrow type; the draw route takes any loaded arrow.
     const bool draw=controllerUiFixture::bowDrawRequested;
     if(freshIdentity&&(draw?ammo!=0:ammo==1454438654)&&ammo==cachedAmmo&&!missing&&received&&now>=received&&now-received<=200){
      std::lock_guard<std::mutex> lock(controllerUiFixture::bowShotMutex);
      consumed=controllerUiFixture::bowTrackedShot.consume(hands,geometry,now,identity.entity,shot,draw?controllerUiFixture::bowDrawShotMs:200,draw?controllerUiFixture::bowDrawShotFrames:8);
     }
     // Rejected tracked shots fail instead of silently using the old camera.
     rejected=true;
     if(consumed){
      WorldPositionQ32 nativePosition;Quat nativeQ;float nativeDistance;
      std::memcpy(&nativePosition,camera.data(),24);std::memcpy(&nativeQ,camera.data()+24,16);std::memcpy(&nativeDistance,camera.data()+40,4);
      const auto focus=cameraPosition(nativePosition,rotate(normalized(nativeQ),{0,0,nativeDistance}),1);
      const Vec3 offset{float(relativeQ32(shot.aim.origin.x,focus.x)),float(relativeQ32(shot.aim.origin.y,focus.y)),float(relativeQ32(shot.aim.origin.z,focus.z))};
      if(finite(offset)&&length(offset)<=(draw?aimFocusLimit:1.5f)){
       effectiveCamera=motion::bowAimCamera(shot.aim,evt);
       sourceInput=read<decltype(sourceInput)>(input);privateInput=sourceInput;privateContext=ctx;
       std::memcpy(privateInput.data()+0x28,effectiveCamera.data(),44);
       const auto pointer=reinterpret_cast<uintptr_t>(privateInput.data());std::memcpy(privateContext.data()+0x88,&pointer,8);
       callContext=privateContext.data();aimed=true;rejected=false;
      }
     }
     logFile->event("bow_tracked_aim_call","\"sample\":"+std::to_string(parent.sample)+",\"applied\":"+(aimed?"true":"false")+
      ",\"consumed\":"+(consumed?"true":"false")+",\"release_frame\":"+std::to_string(shot.frame)+",\"current_frame\":"+std::to_string(hands.frame)+
      ",\"age_ms\":"+std::to_string(now>=shot.milliseconds?now-shot.milliseconds:UINT64_MAX)+",\"entity\":"+std::to_string(identity.entity)+
      ",\"effective_camera_bytes\":"+jsonString(nativeImpactComponents::asHex(effectiveCamera))+
      ",\"original_input_bytes\":"+jsonString(nativeImpactComponents::asHex(sourceInput))+
      ",\"private_input_bytes\":"+jsonString(nativeImpactComponents::asHex(privateInput)));
    }else if(controllerUiFixture::rangedAimRequested&&motion::rangedAimModel(nativeItem)&&(evt[0x148]==1||motion::rangedAimModel(nativeItem)->type!=6)){
     // Wand: same spawn event as the bow. No token: the pointer ray of the moment the game spawns the shot.
     // A staff's cast comes this way in 1.0 too, without the projectile flag (headset run 20261024-151525: its spawn by this
     // parent, isProjectile 0; the staff's own parent 0x224c68 never came) - "the spells do not fly to the white dot" (user,
     // 2026-10-24): it takes the hand's aim as well, its event read as the staff's (bowAimCamera without the flag).
     motion::BowWorldAim aim;uint64_t aimFrame=0;const char* reason="";bool bothHands=false;
     bool usable=rangedAim(nativeItem,identity,nativeWeapon[0],camera,aim,aimFrame,reason,bothHands);
     if(usable)try{effectiveCamera=motion::bowAimCamera(aim,evt,evt[0x148]==1);}catch(const std::exception&){usable=false;reason="event";}
     if(usable){
      sourceInput=read<decltype(sourceInput)>(input);privateInput=sourceInput;privateContext=ctx;
      std::memcpy(privateInput.data()+0x28,effectiveCamera.data(),44);
      const auto pointer=reinterpret_cast<uintptr_t>(privateInput.data());std::memcpy(privateContext.data()+0x88,&pointer,8);
      callContext=privateContext.data();aimed=true;
     }
     logRangedAim(parent.sample,nativeItem,aimed,reason,aimFrame,aim,"spawn_event",bothHands);
    }else if(controllerUiFixture::liveHandUseRequested&&evt[0x148]==1&&motion::catalogThrowable(nativeItem)){
     // A thrown item (VHVR ThrowableManager): the same spawn event; it leaves the hand that threw it, along that
     // hand's movement. Without a throw of the hand (the attack button alone) the game's own aim stands.
     controllerUiFixture::ThrowWorldAim w;const char* reason="applied";
     {std::lock_guard<std::mutex> lock(controllerUiFixture::throwMutex);w=controllerUiFixture::throwWorldAim;
      if(w.serial&&!w.used&&now>=w.milliseconds&&now-w.milliseconds<=1500)controllerUiFixture::throwWorldAim.used=true;else w.serial=0;}
     // The game's thrown state (3); a throw the mod handed early (controllerUiFixture::throwEarlySpawnUntil) comes before it
     // (Mock game-20261023-181633: the early spawn 31 ms after the throw, refused here for the state - the game's aim stood).
     const bool early=now<=controllerUiFixture::throwEarlySpawnUntil.load();
     const bool own=identity.world&&identity.entity&&identity.milliseconds&&now>=identity.milliseconds&&now-identity.milliseconds<=100&&cachedItem==nativeItem&&(nativeWeapon[0]==3||early);
     if(!w.serial)reason="no_throw";else if(!own)reason="native_identity";
     else{
      WorldPositionQ32 nativePosition;Quat nativeQ;float nativeDistance;
      std::memcpy(&nativePosition,camera.data(),24);std::memcpy(&nativeQ,camera.data()+24,16);std::memcpy(&nativeDistance,camera.data()+40,4);
      const auto focus=cameraPosition(nativePosition,rotate(normalized(nativeQ),{0,0,nativeDistance}),1);
      const Vec3 offset{float(relativeQ32(w.aim.origin.x,focus.x)),float(relativeQ32(w.aim.origin.y,focus.y)),float(relativeQ32(w.aim.origin.z,focus.z))};
      if(!finite(offset)||length(offset)>aimFocusLimit)reason="origin_range";
      else{
       effectiveCamera=motion::bowAimCamera(w.aim,evt);
       sourceInput=read<decltype(sourceInput)>(input);privateInput=sourceInput;privateContext=ctx;
       std::memcpy(privateInput.data()+0x28,effectiveCamera.data(),44);
       const auto pointer=reinterpret_cast<uintptr_t>(privateInput.data());std::memcpy(privateContext.data()+0x88,&pointer,8);
       callContext=privateContext.data();aimed=true;
      }
     }
     std::ostringstream s;s<<std::setprecision(10)<<"\"sample\":"<<parent.sample<<",\"item\":"<<nativeItem<<",\"applied\":"<<(aimed?"true":"false")<<",\"reason\":\""<<reason<<"\",\"serial\":"<<w.serial
      <<",\"age_ms\":"<<(w.serial?now-w.milliseconds:0)<<",\"aim_q32\":["<<w.aim.origin.x<<','<<w.aim.origin.y<<','<<w.aim.origin.z<<"],\"aim_direction\":["<<w.aim.direction.x<<','<<w.aim.direction.y<<','<<w.aim.direction.z
      <<"],\"native_camera_bytes\":"<<jsonString(nativeImpactComponents::asHex(camera))<<",\"effective_camera_bytes\":"<<jsonString(nativeImpactComponents::asHex(effectiveCamera));
     logFile->event("throw_aim_call",s.str());
    }
    rootCamera::Sample local;{std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
    uint64_t frame=0;{std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);frame=controllerUiFixture::gestureHands.frame;}
    std::ostringstream row;row<<"\"sample\":"<<parent.sample<<",\"last_xr_frame\":"<<frame<<",\"caller\":"<<hex(callerRva)
     <<",\"parent_caller\":"<<hex(parent.caller)<<",\"event_type_hash\":"<<0x51e20bf3u<<",\"typed_dispatch_path\":true"
     <<",\"native_identity_observed\":"<<(freshIdentity?"true":"false")<<",\"native_world\":"<<hex(identity.world)
     <<",\"native_actor\":"<<hex(identity.actor)<<",\"native_entity\":"<<identity.entity<<",\"identity_age_ms\":"<<(now>=identity.milliseconds?now-identity.milliseconds:UINT64_MAX)
     <<",\"native_weapon_bytes\":"<<jsonString(nativeImpactComponents::asHex(nativeWeapon))
     <<",\"observed_weapon_bytes\":"<<jsonString(nativeImpactComponents::asHex(identity.bytes))
     <<",\"thread\":"<<GetCurrentThreadId()<<",\"native_context\":"<<hex(reinterpret_cast<uintptr_t>(nativeContext))
     <<",\"input\":"<<hex(input)<<",\"context\":"<<hex(c)<<",\"event_address\":"<<hex(v)
     <<",\"local_entity\":"<<local.entity<<",\"local_world\":"<<hex(local.world)
     <<",\"context_bytes\":"<<jsonString(nativeImpactComponents::asHex(ctx))<<",\"event_bytes\":"<<jsonString(nativeImpactComponents::asHex(evt))
     <<",\"camera_bytes\":"<<jsonString(nativeImpactComponents::asHex(camera))
     <<",\"spawn_kind\":"<<unsigned(evt[0x90])<<",\"is_projectile\":"<<(evt[0x148]?"true":"false");pending=row.str();
   }else if((controllerUiFixture::rangedAimRequested||remoteAlways)&&parent.caller==staffParentRva&&parent.context==context&&parent.event==event){
    // Staff cast. Unreviewed items and failed checks keep the native camera aim.
    ctx=read<decltype(ctx)>(c);evt=read<decltype(evt)>(v);input=read<uintptr_t>(c+0x88);camera=read<decltype(camera)>(input+0x28);
    const auto weapon=read<uintptr_t>(c+0xb8);const auto identity=inputRanged::bowIdentity(input,weapon);
    const auto nativeWeapon=read<std::array<uint8_t,inputRanged::rangedWeaponBytes>>(weapon);uint32_t nativeItem=0;std::memcpy(&nativeItem,nativeWeapon.data()+inputRanged::rangedItem,4);
    effectiveCamera=camera;motion::BowWorldAim aim;uint64_t aimFrame=0;const char* reason="item";bool bothHands=false;
    bool remote=false;
    if(!identity.world&&remoteShotsWanted&&motion::rangedAimModel(nativeItem)){
     float nearest=-1;uint64_t from=0,age=0;uint16_t id=0;unsigned fresh=0;bool held=false;
     reason=remoteShot(camera,GetTickCount64(),aim,nearest,from,age,id,fresh,held,true);
     if(from)try{effectiveCamera=motion::bowAimCamera(aim,evt,false);remote=true;}catch(const std::exception&){reason="event";}
     if(remoteLogs++<400)logFile->event("staff_remote_aim","\"item\":"+std::to_string(nativeItem)+",\"applied\":"+(remote?"true":"false")+",\"reason\":\""+reason+"\",\"age_ms\":"+std::to_string(age)+",\"distance\":"+std::to_string(nearest));}
    if(remote){
     sourceInput=read<decltype(sourceInput)>(input);privateInput=sourceInput;privateContext=ctx;
     std::memcpy(privateInput.data()+0x28,effectiveCamera.data(),44);
     const auto pointer=reinterpret_cast<uintptr_t>(privateInput.data());std::memcpy(privateContext.data()+0x88,&pointer,8);
     callContext=privateContext.data();aimed=true;
    }else if(controllerUiFixture::rangedAimRequested&&motion::rangedAimModel(nativeItem)&&rangedAim(nativeItem,identity,nativeWeapon[0],camera,aim,aimFrame,reason,bothHands)){
     effectiveCamera=motion::bowAimCamera(aim,evt,false);
     sourceInput=read<decltype(sourceInput)>(input);privateInput=sourceInput;privateContext=ctx;
     std::memcpy(privateInput.data()+0x28,effectiveCamera.data(),44);
     const auto pointer=reinterpret_cast<uintptr_t>(privateInput.data());std::memcpy(privateContext.data()+0x88,&pointer,8);
     callContext=privateContext.data();aimed=true;
    }
    logRangedAim(0,nativeItem,aimed,reason,aimFrame,aim,"cast_event",bothHands);
    std::ostringstream row;row<<"\"last_xr_frame\":"<<aimFrame<<",\"parent_caller\":"<<hex(parent.caller)<<",\"item\":"<<nativeItem<<",\"weapon_state\":"<<unsigned(nativeWeapon[0])
     <<",\"native_entity\":"<<identity.entity<<",\"spawn_kind\":"<<unsigned(evt[0x90])
     <<",\"camera_bytes\":"<<jsonString(nativeImpactComponents::asHex(camera))<<",\"event_bytes\":"<<jsonString(nativeImpactComponents::asHex(evt));cast=row.str();
   }else if(controllerUiFixture::rangedSurveyRequested&&parent.context==context&&parent.event==event){
    // Read-only survey: other event types that reach the same Crosshair placement (staff casts).
    const auto in=read<uintptr_t>(c+0x88);uint32_t item=0;uint8_t state=255;
    try{const auto w=read<uintptr_t>(c+0xb8);item=read<uint32_t>(w+inputRanged::rangedItem);state=read<uint8_t>(w);}catch(const std::exception&){}
    uint64_t frame=0;{std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);frame=controllerUiFixture::gestureHands.frame;}
    std::ostringstream row;row<<"\"last_xr_frame\":"<<frame<<",\"parent_caller\":"<<hex(parent.caller)<<",\"mode\":"<<unsigned(parent.mode)
     <<",\"item\":"<<item<<",\"weapon_state\":"<<unsigned(state)<<",\"thread\":"<<GetCurrentThreadId()
     <<",\"camera_bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,44>>(in+0x28)))
     <<",\"event_bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,336>>(v)));survey=row.str();
   }
  }catch(const std::exception& e){failure(e);}}
 }
 const bool result=rejected?false:original(output,nativeContext,callContext,event);
 {const ObserverLastError preserve;if(!survey.empty()){try{
  logFile->event("crosshair_survey_sample",survey+",\"result\":"+(result?"true":"false")+",\"transform_bytes\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,56>>(reinterpret_cast<uintptr_t>(output))))+",\"read_only\":true");
 }catch(const std::exception& e){failure(e);}}}
 // The game's own input/context/event are never written; only the private copies differ, and only in the camera value.
 const auto sourcesUnchanged=[&]{return camera==read<std::array<uint8_t,44>>(input+0x28)&&ctx==read<std::array<uint8_t,192>>(reinterpret_cast<uintptr_t>(context))&&evt==read<std::array<uint8_t,336>>(reinterpret_cast<uintptr_t>(event))&&(!aimed||(sourceInput==read<std::array<uint8_t,1320>>(input)&&!std::memcmp(privateInput.data()+0x28,effectiveCamera.data(),44)&&!std::memcmp(privateInput.data(),sourceInput.data(),0x28)&&!std::memcmp(privateInput.data()+0x54,sourceInput.data()+0x54,sourceInput.size()-0x54)&&!std::memcmp(privateContext.data(),ctx.data(),0x88)&&!std::memcmp(privateContext.data()+0x90,ctx.data()+0x90,ctx.size()-0x90)&&read<uintptr_t>(reinterpret_cast<uintptr_t>(privateContext.data()+0x88))==reinterpret_cast<uintptr_t>(privateInput.data())));};
 {const ObserverLastError preserve;if(!cast.empty()){try{
  const bool unchanged=sourcesUnchanged();
  logFile->event("ranged_cast_sample",cast+",\"result\":"+(result?"true":"false")+",\"transform_bytes\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,56>>(reinterpret_cast<uintptr_t>(output))))+
   ",\"sources_unchanged\":"+(unchanged?"true":"false")+",\"tracked_aim\":"+(aimed?"true":"false")+",\"effective_camera_bytes\":"+jsonString(nativeImpactComponents::asHex(effectiveCamera)));
  if(!unchanged)throw std::runtime_error("Crosshair source changed during staff cast");
 }catch(const std::exception& e){failure(e);}}}
 {const ObserverLastError preserve;if(!pending.empty()){try{
  const bool unchanged=sourcesUnchanged();
  const auto out=rejected?std::array<uint8_t,56>{}:read<std::array<uint8_t,56>>(reinterpret_cast<uintptr_t>(output));parent.transform=out;parent.crosshairOk=result;
  logFile->event("bow_spawn_sample",pending+",\"result\":"+(result?"true":"false")+",\"transform_bytes\":"+jsonString(nativeImpactComponents::asHex(out))+
   ",\"sources_unchanged\":"+(unchanged?"true":"false")+",\"read_only\":"+(aimed?"false":"true")+",\"tracked_aim\":"+(aimed?"true":"false")+",\"effective_camera_bytes\":"+jsonString(nativeImpactComponents::asHex(effectiveCamera)));
  if(!unchanged)throw std::runtime_error("Crosshair source changed during original call");
 }catch(const std::exception& e){failure(e);}}}
 return result;
}
inline void verifyBytes(uintptr_t at,const char* expected){
 const auto bytes=parsePattern(expected);
 if(scan(reinterpret_cast<const uint8_t*>(baseAddress+at),bytes.size(),bytes)!=std::vector<size_t>{0})
  throw std::runtime_error("bow spawn typed path changed: "+hex(at));
}
inline void install(){
 inputPipeline::verify(rva,pattern,callerRva);
 inputPipeline::verify(submitRva,submitPattern,submitCaller);
 inputPipeline::verify(dispatchRva,dispatchPattern,spawnParentRva);
 // Successful native submit stores r15d in Actor.currentServerAction.lastSpawnedEntityId.
 // 1.0: Actor.currentServerAction (+0xc20) .lastSpawnedEntityId moved +0xf8 -> +0x10c (reflection), written at 223452 after the
 // write access call; the type hash at 22404e; the parent's call of the dispatch at 224db3 (June 1e81ec / 1e88eb / 1e9513).
 verifyBytes(0x22394c,"44 89 b8 2c 0d 00 00");
 verifyBytes(0x22455e,"8b 03 3d f3 0b e2 51");
 verifyBytes(0x2252c3,"48 89 5c 24 20 e8 43 95 ff ff");   // build 26082781: the call to the dispatch 0x21e810 (25988036: 0x225123 -> 0x21e670, the same bytes; 1.0.0.1: e8 63 95 ff ff)
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+dispatchRva),reinterpret_cast<void*>(&dispatchObserve),reinterpret_cast<void**>(&dispatchOriginal))!=MH_OK)throw std::runtime_error("bow spawn dispatch observer creation failed");
 if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+dispatchRva))!=MH_OK)throw std::runtime_error("bow spawn dispatch observer enable failed");
 dispatchEnabled=true;
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+submitRva),reinterpret_cast<void*>(&submitObserve),reinterpret_cast<void**>(&submitOriginal))!=MH_OK)throw std::runtime_error("bow spawn submit observer creation failed");
 if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+submitRva))!=MH_OK)throw std::runtime_error("bow spawn submit observer enable failed");
 submitEnabled=true;
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&observe),reinterpret_cast<void**>(&original))!=MH_OK)throw std::runtime_error("bow spawn observer creation failed");
 accepting=true;if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("bow spawn observer enable failed");}
 enabled=true;logFile->event("bow_spawn_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"read_only\":true");
}
inline void disable(){
 accepting=false;if(!enabled&&!dispatchEnabled&&!submitEnabled)return;
 if(enabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("bow spawn observer disable failed");
 if(dispatchEnabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+dispatchRva))!=MH_OK)throw std::runtime_error("bow spawn dispatch observer disable failed");
 if(submitEnabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+submitRva))!=MH_OK)throw std::runtime_error("bow spawn submit observer disable failed");
 submitEnabled=false;dispatchEnabled=false;
 enabled=false;const auto until=GetTickCount64()+500;
 while(callbacks.load()&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 const auto bytes=parsePattern(pattern),dispatchBytes=parsePattern(dispatchPattern),submitBytes=parsePattern(submitPattern);
 const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0}
  &&scan(reinterpret_cast<const uint8_t*>(baseAddress+dispatchRva),dispatchBytes.size(),dispatchBytes)==std::vector<size_t>{0}
  &&scan(reinterpret_cast<const uint8_t*>(baseAddress+submitRva),submitBytes.size(),submitBytes)==std::vector<size_t>{0};
 logFile->event("bow_spawn_removed","\"restored\":"+std::string(restored?"true":"false")+",\"calls\":"+std::to_string(calls.load())+",\"samples\":"+std::to_string(samples.load())+",\"dispatch_samples\":"+std::to_string(dispatchSamples.load())+",\"submit_samples\":"+std::to_string(submitSamples.load())+",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
 if(!restored||callbacks.load()||failures.load())throw std::runtime_error("bow spawn observer did not restore/validate");
}
}
