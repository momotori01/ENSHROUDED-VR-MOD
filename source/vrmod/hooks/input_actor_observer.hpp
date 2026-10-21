// Observe completed ActorInput immediately before the ECS iterator advances.
// No input/state writes and no component pointer escapes this callback.
#include "action_sequence_observer.hpp"
#include "native_impact_components.hpp"
#include "input_ranged_observer.hpp"
#include "item_catalog_observer.hpp"
#include "attachment_observer.hpp"
#include "held_vfx_observer.hpp"
#include "sequence_collider_observer.hpp"
#include "impact_hierarchy_observer.hpp"
// Body heading = view heading for the local actor (include/evr/camera_mode.hpp, bodyHeadingCamera). For that actor's
// iteration the query slot of the look mode points at a private byte holding 3 (camera); the slot gets its own pointer
// back before the iterator advances. Nothing the game owns is written.
namespace bodyHeading {
inline bool requested=false;
inline bool guardOnly=false;   // only while the live guard holds the block button (view_resource_probe.cpp, 2026-10-14)
// For the guard, the look mode itself is written (EVR_GUARD_MODE_WRITE=0: the slot is swapped for the look selection only,
// as for the always-on heading). Headset run 20261014-230255 (user: "while guarding I walk only where the shield faces, I
// cannot step back"): with the swapped slot only the look selection (2873a0) saw mode 3 - and a copy of the component in
// which every byte but the mode was 0 - while the rest of the player's update saw mode 1 and the game walked toward the
// look, as with the always-on heading of 10-11. Written, every reader sees the camera mode for the time of the guard, the
// other bytes of the component unchanged; the byte goes back to mode 1 when the guard ends.
inline const bool writeMode=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUARD_MODE_WRITE",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline uintptr_t written=0;inline std::atomic<unsigned> writes{0},restores{0};
inline void restoreWritten() noexcept {
 if(!written)return;
 try{if(read<uint8_t>(written)==game::lookModeCamera)*reinterpret_cast<uint8_t*>(written)=game::lookModeByState;const auto n=restores.fetch_add(1)+1;
  if(n<=3||n%200==0)logFile->event("guard_mode_restored","\"count\":"+std::to_string(n)+",\"mode\":1");}catch(...){}
 written=0;
}
// The local actor is usually the first entity of the iteration: it is reached by the advance before the loop (2873d6),
// every later one by the advance at the loop's end (2894f6). Both use the same stack query.
constexpr uintptr_t firstCallerRva=0x2dc2d7;
inline std::atomic<unsigned> outstanding{0},applied{0},skippedCamera{0},skippedMode{0},failures{0},lost{0};
inline thread_local uintptr_t savedSlot=0,slotAddress=0;inline thread_local std::array<uint8_t,248> modeCopy{};inline thread_local bool active=false;
inline void finish(void* query) noexcept {
 if(!active)return;active=false;
 // The same stack query of the same call: only then is the slot still ours to give back.
 const auto address=reinterpret_cast<uintptr_t>(query)+7*sizeof(uintptr_t);
 try{if(address==slotAddress&&read<uintptr_t>(address)==reinterpret_cast<uintptr_t>(modeCopy.data()))*reinterpret_cast<uintptr_t*>(address)=savedSlot;else ++lost;}catch(...){++lost;}
 --outstanding;
}
inline void begin(uintptr_t sp,void* context,void* query,uint32_t bytes) noexcept {
 if(!requested||active)return;
 // 1.0 (2026-10-16): the look selection reads the mode byte from slot 7 as in June (2ddd7f: [rbp-0x48] = the query at
 // rbp-0x80 + 0x38; June 289115: [rbp-0x18] = rbp-0x50 + 0x38), with the same values (1 by state, 3 and 4 the camera).
 // The query census names that slot's component ServerFishing (248 bytes); the private copy below is that size.
 // the guard, or the time after a stroke's press (controllerUiFixture::attackFacingUntil: the attack goes where the head looks)
 const bool held=!guardOnly||controllerUiFixture::liveGuardHeld.load()||GetTickCount64()<controllerUiFixture::attackFacingUntil.load();
 if(guardOnly&&writeMode&&!held){restoreWritten();return;}
 if(!held)return;
 try{
  // 1.0: the same 0x170-byte query at rsp+0x80 as the final actor query below (the look mode stays slot 7).
  if(bytes!=0x170||reinterpret_cast<uintptr_t>(query)!=sp+0x80)throw std::runtime_error("body heading query ABI differs");
  const auto slots=read<std::array<uintptr_t,46>>(reinterpret_cast<uintptr_t>(query));
  if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[7])return;
  const auto state=read<uintptr_t>(slots[0]);const auto index=read<uint32_t>(slots[0]+8);
  if(!state||!index||index>0x1000000)return;
  const auto entities=read<uintptr_t>(state+0x828);if(!entities)return;
  const auto entity=read<uint32_t>(entities+uintptr_t(index-1)*16);
  rootCamera::Sample local;{std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
  if(!local.number||GetTickCount64()-local.milliseconds>1000||entity!=local.entity)return;
  if(!rootCamera::firstPerson(local)||!game::bodyHeadingCamera(local.cameraId)){++skippedCamera;if(guardOnly&&writeMode)restoreWritten();return;}
  if(guardOnly&&writeMode){
   if(written&&written!=slots[7])restoreWritten();
   if(written)return;
   if(read<uint8_t>(slots[7])!=game::lookModeByState){++skippedMode;return;}
   *reinterpret_cast<uint8_t*>(slots[7])=game::lookModeCamera;written=slots[7];const auto n=writes.fetch_add(1)+1;
   if(n<=3||n%200==0)logFile->event("guard_mode_written","\"count\":"+std::to_string(n)+",\"entity\":"+std::to_string(entity)+",\"camera_id\":"+std::to_string(unsigned(local.cameraId))+",\"mode\":3");
   return;
  }
  if(read<uint8_t>(slots[7])!=game::lookModeByState){++skippedMode;return;}
  modeCopy=read<std::array<uint8_t,248>>(slots[7]);modeCopy[0]=game::lookModeCamera;
  slotAddress=reinterpret_cast<uintptr_t>(query)+7*sizeof(uintptr_t);savedSlot=slots[7];
  *reinterpret_cast<uintptr_t*>(slotAddress)=reinterpret_cast<uintptr_t>(modeCopy.data());active=true;++outstanding;
  const auto n=applied.fetch_add(1)+1;
  if(n<=3||n%2000==0)logFile->event("body_heading_applied","\"count\":"+std::to_string(n)+",\"entity\":"+std::to_string(entity)+",\"camera_id\":"+std::to_string(unsigned(local.cameraId))+",\"native_mode\":1,\"presented_mode\":3,\"component_written\":false");
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)try{logFile->event("body_heading_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void drain(){
 if(!requested)return;requested=false;
 restoreWritten();
 const auto deadline=GetTickCount64()+1000;while(outstanding.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 logFile->event("body_heading_removed","\"applied\":"+std::to_string(applied.load())+",\"skipped_camera\":"+std::to_string(skippedCamera.load())+",\"skipped_mode\":"+std::to_string(skippedMode.load())+",\"lost_scopes\":"+std::to_string(lost.load())+",\"outstanding\":"+std::to_string(outstanding.load())+",\"failures\":"+std::to_string(failures.load()));
 if(outstanding.load())throw std::runtime_error("body heading query slot not returned; iterator hook retained");
}
}
namespace inputActor {
using Advance=bool(__fastcall*)(void*,void*,uint32_t);
inline Advance original=nullptr;
inline bool enabled=false;
inline std::atomic<bool> accepting{false},appearanceOnly{false};
inline std::atomic<unsigned> callbacks{0},calls{0},samples{0},failures{0},waitingForHeight{0},guardSamples{0};
inline unsigned sampleInterval=16;
// EVR_ACTOR_SAMPLES=<n> (2026-10-14, the guard's direction: world_look late in a session): more records than 256.
inline const unsigned sampleLimit=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_ACTOR_SAMPLES",v,sizeof v);const unsigned k=n&&n<sizeof v?unsigned(std::strtoul(v,nullptr,10)):0u;return k>=256&&k<=20000?k:256u;}();
inline std::atomic<unsigned> airSamples{0};
inline bool motionObserverRequested=false;inline uint64_t motionNumber=0;
// The player's own ticks on a client (2026-10-19, online: the friend's throwables "could not be thrown", every stroke's
// press "lost"): the targets of the live melee, the action bar (the held item: throwables), the equipment and the fog
// timer tick in motion_event_selection_observer.hpp select, for the Actor of the player's World - which a client never
// finds. There, with no Actor bound in any World, they tick here at the player's final actor query with the World the
// root camera resolved (the player's render component's). Set by view_resource_probe.cpp.
inline void (*clientWorldTicks)(uintptr_t world,uint32_t entity)=nullptr;inline std::atomic<uint64_t> clientTicks{0};
inline std::mutex motionMutex;inline game::BodyMotionSample motionLatest;
constexpr uintptr_t rva=0x9b6e60,callerRva=0x2de765;
// 1.0: the final actor query is 0x170 bytes at the caller's rsp+0x80 (0xb0 at +0xb0). PlayerInput (slot 4) and Actor (9) as
// before, ActorInput 15 (11), PlayerState 33 (17), gravity 2 (query census, Mock game-20261016-074759: the ECS type names).
constexpr uint32_t actorQueryBytes=0x170;constexpr uintptr_t actorQueryAt=0x80;
constexpr size_t actorQuerySlots=46,slotInput=4,slotBaseActor=9,slotActor=15,slotPlayerState=33,slotGravity=2;
// 1.0: the two server-slot loops (June 2d07d0 / 2d0860 around 2b4d10) are not found yet; without them the rest stays.
inline bool serverSlotsVerified=false;
constexpr const char* pattern="40 57 48 83 EC 20 4C 8B 01 48 8B FA 8B 41 08 4C 8B C9";
struct CallbackScope {CallbackScope(){callbacks.fetch_add(1);}~CallbackScope(){callbacks.fetch_sub(1);}};
// 1.0 layout search (EVR_LAYOUT_DUMP=1): what each slot of a watched query points at, named through the ECS type
// registry (native_impact_components.hpp), three iterations per caller; the query bytes go to layout-query-<caller>-<n>.bin.
namespace queryCensus {
inline const bool wanted=[]{char v[8]{};const auto k=GetEnvironmentVariableA("EVR_LAYOUT_DUMP",v,sizeof v);return k==1&&v[0]=='1';}();
// Every caller (EVR_LAYOUT_DUMP=2, 2026-10-17): after the advance found an entity, the first three of each caller with the
// types in its slots - to find where 1.0 iterates the attack's colliders (the June callsite 0x3f02dd is never reached
// during an attack: Mock game-20261017-031508, sequence_collider calls 0 for eight strokes).
inline const bool all=[]{char v[8]{};const auto k=GetEnvironmentVariableA("EVR_LAYOUT_DUMP",v,sizeof v);return k==1&&v[0]=='2';}();
inline std::mutex mutex;inline std::map<uintptr_t,unsigned> counts;inline std::atomic<uintptr_t> knownWorld{0};
inline void record(uintptr_t caller,uintptr_t callerRsp,void* context,void* query,uint32_t bytes,bool localOnly=true) noexcept {
    try{
        const auto q=reinterpret_cast<uintptr_t>(query),ctx=reinterpret_cast<uintptr_t>(context);
        {const auto st=read<uintptr_t>(ctx);const auto ix=read<uint32_t>(ctx+8);const auto table=st?read<uintptr_t>(st+0x828):0;
         const uint32_t id=table&&ix?read<uint32_t>(table+uintptr_t(ix-1)*16):0;rootCamera::Sample local;
         if(localOnly&&(!rootCamera::trySnapshot(local)||!local.entity||id!=local.entity))return;}   // the local player's iteration only
        if(!read<uintptr_t>(read<uintptr_t>(ctx)+0x30)&&!knownWorld.load())return;   // wait for a world to name the types with
        unsigned n=0;{std::lock_guard<std::mutex> lock(mutex);n=counts[caller]++;}if(n>=3)return;
        std::ostringstream out;out<<"\"caller\":"<<hex(caller-baseAddress)<<",\"sample\":"<<n<<",\"bytes\":"<<bytes<<",\"query_minus_rsp\":"<<hex(q-callerRsp);
        const auto state=read<uintptr_t>(ctx);const auto index=read<uint32_t>(ctx+8);uint32_t entity=0;uintptr_t world=0;
        try{const auto entities=read<uintptr_t>(state+0x828);if(entities&&index)entity=read<uint32_t>(entities+uintptr_t(index-1)*16);}catch(...){}
        try{world=read<uintptr_t>(state+0x30);}catch(...){}
        if(world)knownWorld=world;else world=knownWorld.load();   // a task without the world capability: the one seen elsewhere
        out<<",\"entity\":"<<entity<<",\"task_world\":"<<hex(world);
        nativeImpactComponents::Entity e;bool have=false;
        try{if(world&&entity){e=nativeImpactComponents::entityAtWorld(world,entity);have=true;}}catch(const std::exception& x){out<<",\"entity_lookup\":"<<jsonString(x.what());}
        const auto count=std::min<uint32_t>(bytes/8,160u);std::vector<uint8_t> raw(bytes);std::memcpy(raw.data(),query,bytes);
        std::ostringstream name;name<<"layout-query-"<<std::hex<<(caller-baseAddress)<<std::dec<<"-"<<n<<".bin";
        std::ofstream(calibrationDirectory/name.str(),std::ios::binary)
            .write(reinterpret_cast<const char*>(raw.data()),std::streamsize(raw.size()));
        out<<",\"slots\":[";bool first=true;
        for(uint32_t i=0;i<count;i++){uintptr_t p=0;std::memcpy(&p,raw.data()+i*8,8);if(p<0x10000)continue;
            out<<(first?"":",")<<"{\"at\":"<<i*8<<",\"p\":"<<hex(p);first=false;
            {auto reader=[](uint64_t a,void* o,size_t k){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(a),o,k,&got)&&got==k;};
             game::ItemMapHeader h;if(game::readItemMap(reader,p,h)&&game::validItemMap(h)&&h.count)out<<",\"item_map\":{\"capacity\":"<<h.capacity<<",\"count\":"<<h.count<<'}';}
            if(have)try{const auto bits=read<std::array<uint64_t,10>>(e.layout);
                for(unsigned id=0;id<nativeImpactComponents::componentCapacity;++id){if(!(bits[id/64]&(1ull<<(id%64))))continue;
                    const auto stride=read<uint16_t>(e.layout+0xa84+id*2),offset=read<uint16_t>(e.layout+0x84+id*2);
                    if(e.data+offset+uintptr_t(e.row)*stride==p){const auto t=nativeImpactComponents::componentType(world,id);
                        out<<",\"id\":"<<id<<",\"type\":"<<jsonString(t.qualifiedName)<<",\"size\":"<<t.bytes;break;}}}catch(const std::exception& x){out<<",\"lookup\":"<<jsonString(x.what());}
            out<<'}';}
        out<<']';logFile->event("query_census",out.str());
    }catch(const std::exception& x){try{logFile->event("query_census_failure","\"caller\":"+hex(caller-baseAddress)+",\"reason\":"+jsonString(x.what()));}catch(...){}}
}
}
// The local player's jump (2026-10-18, user after the headset run of 13:30: "in the double jump the feet show in the camera
// for a moment - the double jump without the legs' animation"): DynamicLocomotion's jumpType, jumpCount and state
// (docs/analysis/airborne_skills_20261015.md 1.3: the game itself tells the double jump by jumpCount), read at the final actor
// query through the entity's component table. 1.0 (the reflection of build 25988036): 456 bytes, state +0x59, jumpType +0x154,
// jumpCount +0x155 (June: 312 bytes, +0x3d / +0xcc / +0xcd); the World's type registry names it keen::ecs::Locomotion (Mock
// game-20261018-141428, id 325), so it is found there by that name and its size, once per World. Read-only. Used by the body's IK
// (hooks/motion_body_ik.hpp): the legs hang under the hips while jumpCount is 2 or more.
namespace localJump {
inline std::atomic<uintptr_t> world{0};inline std::atomic<int> id{-1};
inline std::atomic<uint8_t> type{0},count{0},state{0};inline std::atomic<uint64_t> milliseconds{0};inline std::atomic<unsigned> logs{0};
inline void sample(uintptr_t w,uint32_t entity) noexcept {
    if(!w||!entity)return;
    try{
        if(world.load()!=w){int found=-1;
            for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){nativeImpactComponents::ComponentType t;
                try{t=nativeImpactComponents::componentType(w,i);}catch(const std::exception&){break;}
                if((t.qualifiedName=="keen::ecs::Locomotion"||t.qualifiedName=="keen::ecs::DynamicLocomotion")&&t.bytes==456){found=int(i);break;}}
            id=found;world=w;
            logFile->event("local_jump_component","\"world\":"+hex(w)+",\"component_id\":"+std::to_string(found)+",\"read_only\":true");}
        const int i=id.load();if(i<0)return;
        const auto e=nativeImpactComponents::entityAtWorld(w,entity);const auto p=nativeImpactComponents::component(e,unsigned(i),456);if(!p)return;
        const auto t=read<uint8_t>(p+0x154),c=read<uint8_t>(p+0x155),s=read<uint8_t>(p+0x59);
        const bool changed=t!=type.load()||c!=count.load()||s!=state.load();
        type=t;count=c;state=s;milliseconds=GetTickCount64();
        if(changed&&logs.fetch_add(1)<2000)logFile->event("local_jump","\"jump_type\":"+std::to_string(t)+",\"jump_count\":"+std::to_string(c)+",\"locomotion_state\":"+std::to_string(s));
    }catch(...){}
}
// The double jump (or more) going on: a fresh sample (a quarter second) with jumpCount 2 or more.
inline bool doubleJump(uint64_t now){const auto ms=milliseconds.load();return ms&&now-ms<=250&&count.load()>=2;}
}
__declspec(noinline) bool __fastcall advance(void* context,void* query,uint32_t bytes){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto callerRsp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+8;
    const CallbackScope callback;
    if(queryCensus::wanted){const auto c=caller-baseAddress;
        if(c==callerRva||c==bodyHeading::firstCallerRva||c==inputRanged::callerRva||c==attachmentObserver::callers[0])queryCensus::record(caller,callerRsp,context,query,bytes);
        else if(c==impactHierarchyObserver::callerRva||c==sequenceColliderObserver::callerRva||c==0x3f0236||c==0x2d9b8b||c==0x368b27||c==attachmentObserver::callers[1]||c==attachmentObserver::callers[2]||c==attachmentObserver::callers[3])
            queryCensus::record(caller,callerRsp,context,query,bytes,false);}
    if(caller==baseAddress+callerRva)bodyHeading::finish(query);
    {const ObserverLastError preserve;
     if(accepting.load()&&sequenceColliderObserver::requested&&serverSlotsVerified&&(caller==baseAddress+0x2d0d67||caller==baseAddress+0x2d0dfb))sequenceColliderObserver::serverSlot(callerRsp,context,query,bytes);
     if(accepting.load()&&impactHierarchyObserver::requested&&caller==baseAddress+impactHierarchyObserver::callerRva){impactHierarchyObserver::capture(callerRsp,context,query,bytes);
        if(queryCensus::wanted&&impactHierarchyObserver::actorImpact){impactHierarchyObserver::actorImpact=false;queryCensus::record(caller+1,callerRsp,context,query,bytes,false);}}
     if(accepting.load()&&sequenceColliderObserver::requested&&caller==baseAddress+sequenceColliderObserver::callerRva){sequenceColliderObserver::capture(callerRsp,context,query,bytes);}
     if(caller==baseAddress+sequenceColliderObserver::callerRva){motionColliderApply::finish(query);sequenceColliderIdentity::finish(query);}
     if(sequenceColliderObserver::colliderSweep::site(caller))sequenceColliderObserver::colliderSweep::close(query);
     if((accepting.load()||appearanceOnly.load())&&heldVfxObserver::requested)heldVfxObserver::capture(caller-baseAddress,callerRsp,context,query,bytes);
     if((accepting.load()||appearanceOnly.load())&&attachmentObserver::requested)attachmentObserver::capture(caller-baseAddress,callerRsp,context,query,bytes);
     if(accepting.load()&&caller==baseAddress+inputRanged::callerRva)inputRanged::capture(callerRsp,context,query,bytes);
     if(accepting.load()&&caller==baseAddress+callerRva&&((calls.fetch_add(1)%sampleInterval==0&&samples.load()<sampleLimit)||controllerUiFixture::motionRequested||(controllerUiFixture::verticalRequested||controllerUiFixture::wheelRequested)||motionObserverRequested||actionSequenceObserver::requested||inputHeadIdle::vrLocomotion)){
        try{
          [&]{
            if(bytes!=actorQueryBytes||reinterpret_cast<uintptr_t>(query)!=callerRsp+actorQueryAt)
                throw std::runtime_error("final actor query ABI changed");
            const auto slots=read<std::array<uintptr_t,actorQuerySlots>>(reinterpret_cast<uintptr_t>(query));
            if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[slotInput]||!slots[slotBaseActor]||!slots[slotActor]||!slots[slotPlayerState])
                throw std::runtime_error("final actor query identity changed");
            const auto state=read<uintptr_t>(slots[0]);const auto index=read<uint32_t>(slots[0]+8);
            if(!state||!index||index>0x1000000)throw std::runtime_error("final actor query bounds");
            const auto entities=read<uintptr_t>(state+0x828),ticks=read<uintptr_t>(state+0x818);
            if(!entities)throw std::runtime_error("final actor entity table absent");
            const auto entity=read<uint32_t>(entities+uintptr_t(index-1)*16);
            rootCamera::Sample local;{std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
            if(!local.number||GetTickCount64()-local.milliseconds>1000||entity!=local.entity)return;
            const auto input=slots[slotInput],baseActor=slots[slotBaseActor],actor=slots[slotActor];
            // Native queries can lack the optional prediction tick table.
            // Entity identity remains mandatory; zero never proves tick identity.
            const auto tick=ticks?read<uint32_t>(ticks+uintptr_t(index-1)*4):0;
            const auto actorWorld=nativeImpactComponents::resolveWorld(entity,baseActor,3104);
            // after 5 s without any Actor bound: a host binds the server's World within the first ticks
            static uint64_t firstQuery=0;if(!firstQuery)firstQuery=GetTickCount64();
            const bool forced=nativeImpactComponents::forceClientTicks;
            if((!actorWorld||forced)&&clientWorldTicks&&local.world&&GetTickCount64()-firstQuery>5000&&(forced||!nativeImpactComponents::actorBound())){
                if(clientTicks.fetch_add(1)==0)logFile->event("client_world_ticks","\"world\":"+hex(local.world)+",\"entity\":"+std::to_string(entity)+
                    ",\"reason\":"+(forced?"\"forced (EVR_FORCE_CLIENT_TICKS=1)\"":"\"no World has the player's Actor (a client)\""));
                nativeImpactComponents::clientTickMs.store(GetTickCount64());clientWorldTicks(local.world,entity);}
            actionSequenceObserver::capture(baseActor,entity,local.entity,tick,input,actor,actorWorld,slots);
            // A guest has no World with its Actor (the host's server moves it); its client World's Locomotion is read then
            // (2026-10-21, user as a guest: "the spinning legs of the double jump show" - the legs never hung, no jump was read).
            localJump::sample(actorWorld?actorWorld:local.world,entity);
            const auto position=read<WorldPositionQ32>(input+0x28);const auto q=read<Quat>(input+0x40);normalized(q);
            const auto desired=read<Vec3>(actor+0x18c),move=read<Vec3>(actor+0x198),look=read<Vec3>(actor+game::actorInputLook);
            if(!finite(desired)||!finite(move)||!finite(look))throw std::runtime_error("nonfinite final actor vectors");
            const auto gravity=slots[slotGravity]; // query RBP-40, native cc3e70 checks +10
            const auto gravityDirection=gravity?read<Vec3>(gravity):Vec3{};
            const auto gravityValue=gravity?read<float>(gravity+0xc):0.f;
            const bool gravityActive=gravity&&read<uint8_t>(gravity+0x10)!=0;
            if(!finite(gravityDirection)||!std::isfinite(gravityValue))throw std::runtime_error("nonfinite gravity state");
            const auto raw=read<uint64_t>(baseActor+0xbf8),add=read<uint64_t>(baseActor+0xbd0),remove=read<uint64_t>(baseActor+0xbd8);
            const auto sequence=read<uint8_t>(baseActor+0x1b1);
            const auto effective=sequence&1?(raw|add)&~remove:raw; // native 1dfa50
            // The player's state; its changes of Blocking, ParryOportunity and EnemyPrepareAttack are put on record
            // (headset runs keep few lines: changes only). The BaseActor names the local player for the swing parry.
            controllerUiFixture::playerActorState=effective;controllerUiFixture::playerActorStateMs=GetTickCount64();controllerUiFixture::playerBaseActor=baseActor;
            if(effective>>5&1)controllerUiFixture::blockOpportunityMs=GetTickCount64();
            // Walking while guarding (headset run 20261014-230255, user: "while guarding I walk only where the shield faces;
            // I cannot step back"): the game strafes while Blocking only if the input component's byte +51a is 0 (237de8 ->
            // 336420), and it was 1 all through the guard (Mock game-20261014-233556) - so the character walked only along
            // its look, which the guard holds on the shield. While the live guard holds the block the byte is 0
            // (EVR_GUARD_STRAFE=0 leaves it).
            {static const bool strafe=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUARD_STRAFE",v,sizeof v);return !(n==1&&v[0]=='0');}();
             if(strafe&&controllerUiFixture::liveGuardHeld.load()&&read<uint8_t>(input+game::playerInputSettings)==1){*reinterpret_cast<uint8_t*>(input+game::playerInputSettings)=0;
                static std::atomic<unsigned> n{0};if(n.fetch_add(1)<3)logFile->event("guard_strafe_input","\"byte_51a\":0,\"was\":1");}}
            {static uint64_t seen=~0ull;static std::atomic<unsigned> lines{0};constexpr uint64_t watched=(1ull<<2)|(1ull<<4)|(1ull<<5)|(1ull<<51);   // 1.0: RangedCombatShoot 51 (48)
             if((effective&watched)!=seen&&lines.load()<2000){++lines;seen=effective&watched;
                logFile->event("player_guard_state","\"tick\":"+std::to_string(tick)+",\"blocking\":"+std::to_string(effective>>2&1)+",\"parry_opportunity\":"+std::to_string(effective>>4&1)+
                    ",\"block_opportunity\":"+std::to_string(effective>>5&1)+",\"enemy_prepare_attack\":"+std::to_string(effective>>48&1)+",\"guard_held\":"+(controllerUiFixture::liveGuardHeld.load()?"true":"false"));}}
            glideLaunch::capture(baseActor,input,entity,tick,effective,read<uint32_t>(slots[slotPlayerState]+0x34),gravityActive&&finite(gravityDirection)&&length(gravityDirection-Vec3{0,-1,0})<1e-5f);
            if(motionObserverRequested){
                std::lock_guard<std::mutex> lock(motionMutex);
                game::BodyMotionSample sample{++motionNumber,GetTickCount64(),effective,entity,true};
                logFile->event("body_motion_actor_observation","\"sample\":"+std::to_string(sample.number)+",\"milliseconds\":"+std::to_string(sample.milliseconds)+
                    ",\"entity\":"+std::to_string(entity)+",\"raw\":"+std::to_string(raw)+",\"add\":"+std::to_string(add)+",\"remove\":"+std::to_string(remove)+
                    ",\"sequence\":"+std::to_string(sequence)+",\"state\":"+std::to_string(sample.state)+",\"tick\":"+std::to_string(tick)+",\"source\":\"final_actor_query\",\"read_only\":true");
                motionLatest=sample;
            }
            // The ordinary movement quaternion branches are skipped while gliding.
            // Publish from the completed local Actor query too, before the bounded
            // logging budget, so the 200 ms guard remains strict during flight.
            if(inputHeadIdle::vrLocomotion)inputHeadIdle::observeState(entity,read<uint32_t>(slots[slotPlayerState]+0x34),baseActor,gravity,true);
            if(controllerUiFixture::motionRequested||(controllerUiFixture::verticalRequested||controllerUiFixture::wheelRequested))controllerUiFixture::publishMotionActor(entity,effective,read<uint64_t>(input+game::playerInputDigital));
            // Ordinary records are exhausted before the delayed glide fixture.
            // Keep a separate bounded airborne record window for actual flight
            // direction, without changing any native Actor or input bytes.
            const bool airRecord=inputHeadIdle::vrLocomotion&&rootCamera::heightReady.load()&&
                !(effective&1ull)&&airSamples.load()<512;
            // While the live guard holds the block button: a record window of its own (2026-10-14, the guard's direction).
            const bool guardRecord=controllerUiFixture::liveGuardHeld.load()&&samples.load()>=sampleLimit&&guardSamples.load()<8000;
            if(samples.load()>=sampleLimit&&!airRecord&&!guardRecord)return;
            // First-person HMD runs: the bounded record window starts once the eye height is
            // settled and head input is published, so it observes the HMD-driven camera path.
            if(rootCamera::mode==2&&!rootCamera::heightReady.load()){waitingForHeight.fetch_add(1);return;}
            std::ostringstream out;out<<std::setprecision(10)<<"\"sample\":"<<(airRecord?airSamples.fetch_add(1):guardRecord?guardSamples.fetch_add(1):samples.fetch_add(1))+1<<",\"guard_window\":"<<(guardRecord?"true":"false")
                <<",\"entity\":"<<entity<<",\"local_entity\":"<<local.entity<<",\"tick\":"<<tick
                <<",\"tick_available\":"<<(ticks?"true":"false")<<",\"query_state\":"<<hex(state)<<",\"thread\":"<<GetCurrentThreadId()
                <<",\"caller\":"<<hex(callerRva)<<",\"input_mode\":"<<unsigned(read<uint8_t>(input+game::playerInputMode))<<",\"input_51a\":"<<unsigned(read<uint8_t>(input+game::playerInputSettings))
                <<",\"client_state_mask\":"<<unsigned(read<uint8_t>(input+game::playerInputMask))
                <<",\"player_flags\":"<<read<uint32_t>(slots[slotPlayerState]+0x34)
                <<",\"gravity_present\":"<<(gravity?"true":"false")<<",\"gravity_active\":"<<(gravityActive?"true":"false")
                <<",\"gravity_value\":"<<gravityValue
                <<",\"actor_state_raw\":"<<raw<<",\"actor_state_add\":"<<add<<",\"actor_state_remove\":"<<remove
                <<",\"actor_sequence_flags\":"<<unsigned(sequence)<<",\"actor_state_effective\":"<<effective
                <<",\"camera_q32\":["<<position.x<<','<<position.y<<','<<position.z<<"]"
                <<",\"camera_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"]";
            auto vector=[&](const char* name,Vec3 v){out<<",\""<<name<<"\":["<<v.x<<','<<v.y<<','<<v.z<<']';};
            vector("desired_local",desired);vector("world_move",move);vector("world_look",look);
            vector("gravity_direction",gravityDirection);
            if(airRecord){const auto& bq=local.stable.orientation;
                out<<",\"root_frame\":"<<local.number<<",\"root_tick\":"<<local.tick<<",\"camera_id\":"<<unsigned(local.cameraId)
                   <<",\"root_age_ms\":"<<GetTickCount64()-local.milliseconds
                   <<",\"body_orientation\":["<<bq.x<<','<<bq.y<<','<<bq.z<<','<<bq.w<<']';}
            out<<",\"read_only\":true";logFile->event(airRecord?"input_actor_air_sample":"input_actor_final_sample",out.str());
          }();
        }catch(const std::exception& e){{std::lock_guard<std::mutex> lock(motionMutex);motionLatest={};}if(failures.fetch_add(1)==0)logFile->event("input_actor_final_failure","\"reason\":"+jsonString(e.what()));}
     }}
    const auto result=original(context,query,bytes);
    if(queryCensus::all&&result){const ObserverLastError preserve;queryCensus::record(caller,callerRsp,context,query,bytes,false);}
    if(result&&accepting.load()&&sequenceColliderObserver::requested&&sequenceColliderObserver::colliderSweep::site(caller)){const ObserverLastError preserve;sequenceColliderObserver::colliderSweep::opened(callerRsp,context,query,bytes);}
    {const ObserverLastError preserve;if(result&&accepting.load()&&(caller==baseAddress+callerRva||caller==baseAddress+bodyHeading::firstCallerRva))bodyHeading::begin(callerRsp,context,query,bytes);}
    {const ObserverLastError preserve;if(result&&accepting.load()&&sequenceColliderObserver::requested&&(caller==baseAddress+0x3f0236||caller==baseAddress+sequenceColliderObserver::callerRva))sequenceColliderObserver::ready(callerRsp,context,query,bytes);}
    return result;
}
inline void install(){
    if(nativeImpactComponents::requested)rootCamera::worldResolver=&nativeImpactComponents::resolveWorld;
    inputPipeline::verify(rva,pattern,callerRva);
    if(bodyHeading::requested)inputPipeline::verify(rva,pattern,bodyHeading::firstCallerRva);
    if(impactHierarchyObserver::requested){inputPipeline::verify(rva,pattern,impactHierarchyObserver::callerRva);logFile->event("impact_hierarchy_enabled","\"caller_rva\":\"0x3ef3c6\",\"query_bytes\":1216,\"read_only\":true");}
    if(sequenceColliderObserver::requested){inputPipeline::verify(rva,pattern,sequenceColliderObserver::colliderSweep::firstReturn);inputPipeline::verify(rva,pattern,sequenceColliderObserver::colliderSweep::nextReturn);
        inputPipeline::verify(rva,pattern,sequenceColliderObserver::callerRva);inputPipeline::verify(rva,pattern,0x3f0236);try{inputPipeline::verify(rva,pattern,0x2d0d67);inputPipeline::verify(rva,pattern,0x2d0dfb);serverSlotsVerified=true;}catch(const std::exception& e){serverSlotsVerified=false;logFile->event("optional_hook_skipped","\"hook\":\"sequence_collider_server_slots\",\"reason\":"+jsonString(e.what()));}logFile->event("sequence_collider_enabled","\"caller_rva\":\"0x3f065d\",\"query_bytes\":80,\"read_only\":true");}
    if(attachmentObserver::requested){for(auto caller:attachmentObserver::callers)inputPipeline::verify(rva,pattern,caller);logFile->event("attachment_observer_enabled","\"read_only\":true,\"max_records\":256");}
    inputPipeline::verify(rva,pattern,inputRanged::callerRva);
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&advance),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("final actor observer creation failed");
    accepting=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("final actor observer enable failed");}
    enabled=true;logFile->event("input_actor_final_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern));
    logFile->event("input_ranged_enabled","\"shared_iterator_rva\":"+hex(rva)+",\"caller\":"+hex(inputRanged::callerRva)+",\"query_bytes\":312,\"item_metadata_version\":1,\"read_only\":true");
}
// Appearance and combat observations share this native iterator. The renderer
// still needs FRESH attachment values after gameplay readers have stopped.
// Keep only those original callback-owned attachment arguments; no saved ECS read.
inline void retainAppearanceOnly(){
    if(!enabled||!attachmentObserver::requested)throw std::runtime_error("appearance handoff requires installed attachment observer");
    appearanceOnly=true;accepting=false;
    bodyHeading::drain();motionColliderApply::drain();sequenceColliderIdentity::drain();
    logFile->event("input_actor_appearance_only","\"gameplay_accepting\":false,\"attachment_callback_only\":true,\"saved_actor_dereference\":false,\"iterator_hook_retained\":true");
}
inline void disable(){
    appearanceOnly=false;
    bodyHeading::drain();
    motionColliderApply::drain();
    sequenceColliderIdentity::drain();
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("final actor observer disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto bytes=parsePattern(pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("input_actor_final_removed","\"restored\":"+std::string(restored?"true":"false")+",\"samples\":"+std::to_string(samples.load())+",\"waiting_for_height\":"+std::to_string(waitingForHeight.load())+
        ",\"calls\":"+std::to_string(calls.load())+",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    logFile->event("input_ranged_removed","\"restored\":"+std::string(restored?"true":"false")+",\"samples\":"+std::to_string(inputRanged::samples.load())+
        ",\"calls\":"+std::to_string(inputRanged::calls.load())+",\"failures\":"+std::to_string(inputRanged::failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    if(attachmentObserver::requested)logFile->event("attachment_observer_removed","\"restored\":"+std::string(restored?"true":"false")+",\"failures\":"+std::to_string(attachmentObserver::failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    actionSequenceObserver::removed(restored,callbacks.load());
    sequenceColliderObserver::removed(restored,callbacks.load());
    impactHierarchyObserver::removed(restored,callbacks.load());
    motionColliderShadow::removed();
    const bool intentionallyBlocked=controllerUiFixture::expectedMotionCancellation()&&motionColliderChannel::eventBlocked.load();
    // Live play may end without a single attack; only the scope invariant is enforced there.
    if(motionColliderApply::live&&motionColliderApply::opened.load()!=motionColliderApply::closed.load())throw std::runtime_error("live tracked collider scopes unbalanced; iterator restored");
    if(motionColliderApply::requested&&!motionColliderApply::live&&((!motionColliderApply::opened.load()&&!intentionallyBlocked)||motionColliderApply::failures.load()||(motionColliderApply::cancelAfter&&!motionColliderApply::cancelled.load())||motionColliderApply::opened.load()!=motionColliderApply::closed.load()))throw std::runtime_error("tracked collider diagnostic failed; iterator restored");
    if(motionColliderChannel::requested&&!motionColliderChannel::live&&((!motionColliderShadow::accepted.load()&&!intentionallyBlocked)||motionColliderShadow::failures.load()))throw std::runtime_error("collider shadow did not validate; iterator restored");
    if(!restored||callbacks.load())throw std::runtime_error("final actor observer did not quiesce/restore");
    if(attachmentObserver::requested&&attachmentObserver::failures.load())throw std::runtime_error("attachment observation failed; hook already restored");
}
}
