#pragma once
#include <evr/motion_event_retime.hpp>
#include <evr/live_contact.hpp>
#include <evr/attack_catalog.hpp>
#include <evr/observation_budget.hpp>
// Read-only native active-event selection at actor_spawn_entity's exact caller.
namespace motionEventSelection {
using Select=uintptr_t(__fastcall*)(void*,const void*,void*,uint8_t,const void*);
inline bool retimeRequested=false;inline std::mutex retimeMutex;inline evr::motion::EventRetimeLedger<8> retimeLedger;inline unsigned appended=0,suppressed=0,blocked=0;
inline evr::diagnostic::ObservationBudget recordingBudget{256};
inline std::atomic<unsigned> recordedSamples{0},witnessSamples{0},omittedSamples{0};
inline motion::RetainedEventScope retainedScope;
inline std::atomic<bool> retainedOnly{false};
inline std::atomic<uint64_t> retainedCalls{0},retainedEpochSamples{0};
// Caller owns retimeMutex at a proven normal-idle fence, or after the exact
// native Actor returned absent in the explicit interrupted trial. Preserve
// suppression without any saved-Actor or camera lookup.
inline void restrictToRetained(const motion::EventActionKey& key){
 if(!retainedScope.valid()||!(retainedScope.action==key)||!retimeLedger.owns(key))throw std::runtime_error("retained event scope identity missing");
 retainedOnly=true;
 logFile->event("motion_event_retained_scope","\"actor_argument\":"+hex(retainedScope.actorArgument)+",\"native_registry_argument\":"+hex(retainedScope.registryArgument)+",\"world\":"+hex(key.world)+",\"entity\":"+std::to_string(key.entity)+",\"sequence\":"+std::to_string(key.sequence)+",\"trigger\":"+std::to_string(key.trigger)+",\"native_arguments_only\":true,\"reads_camera\":false,\"dereferences_saved_actor\":false"+combatEpochGate::stampFields(key.epoch));
}
inline bool shadowRequested=false;inline std::atomic<unsigned> shadowCalls{0};
inline Select original=nullptr;inline bool requested=false,enabled=false;inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},samples{0},failures{0};
constexpr uintptr_t rva=0x213b50,callerRva=0x224524;
constexpr const char* pattern="48 89 5c 24 18 48 89 6c 24 20 56 48 83 ec 50 f6 82 b1 01";
inline evr::motion::EventActionKey actionKey(const std::array<uint8_t,3104>& b,uintptr_t world,uint32_t entity){
 evr::motion::EventActionKey k;k.world=world;k.entity=entity;std::memcpy(&k.sequence,b.data(),4);std::memcpy(&k.item,b.data()+0x38,4);std::memcpy(&k.trigger,b.data()+8,8);k.call=b[20];k.sub=b[0x170];return combatEpochGate::action(k);
}
constexpr uint32_t spawnImpactEvent=0x64b5dc20,spawnEntityEvent=0x51e20bf3,sfxNotifierEvent=0x306db875,vfxNotifierEvent=0x08e653f0,lerpMovementFactorEvent=0x65ab3720,lerpRotationFactorEvent=0x43cf5bc5;
// An Impact is told apart by its event guid as well; other events (a tool's SpawnEntityEvent is shorter than that field's offset) by their place in the asset alone.
inline evr::motion::EventIdentity identity(uintptr_t variant){evr::motion::EventIdentity e;e.variant=variant;if(read<uint32_t>(variant)==spawnImpactEvent){const auto payload=sequenceAssetObserver::relative(variant+4);e.guid=read<std::array<uint8_t,16>>(payload+0x158);}return e;}
inline void retime(void* output,const void* actor,const std::array<uint8_t,3104>& before,uintptr_t world,uint32_t entity,uintptr_t candidate=0,uintptr_t registryArgument=0){
 if(!retimeRequested)return;std::lock_guard<std::mutex> lock(retimeMutex);
 combatWorldBridge::observe(combatWorldBridge::Stage::Event,0,world);
 const auto key=actionKey(before,world,entity);if(!key.valid())return;const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
 if(h[1]>256||h[1]>h[2]||!h[0])throw std::runtime_error("event retime bounds");
 std::vector<uintptr_t> src;for(size_t i=0;i<h[1];++i)src.push_back(read<uintptr_t>(h[0]+i*8));auto dst=src;
 const bool early=candidate!=0;bool changed=false,adding=false;
 if(early){
  const auto root=rootCamera::snapshot();const auto now=GetTickCount64();const auto visual=attachmentObserver::meleeAppearance(root.entity,now);const auto ui=inputUi::snapshot();
  uint32_t triggerEntity=0;std::memcpy(&triggerEntity,before.data()+16,4);
  const auto ticket=controllerUiFixture::ticketSnapshot();
  bool originValid=!ticket.motion;
  const auto published=motionColliderChannel::snapshotFrame();
  if(ticket.motion){const auto& h=published.hands;const auto ms=published.handMs;const auto& pose=published.pose;
   const bool supported=!controllerUiFixture::twoHandAttackRequested||controllerUiFixture::twoHandSupport(h);
   originValid=published.coherent()&&supported&&controllerUiFixture::motionAttackRequested&&ticket.binding==((uint64_t(visual.child)<<32)|visual.item)&&h.frame>=ticket.frame&&h.sessionGeneration==ticket.session&&h.originGeneration==ticket.origin&&h.focused&&h.hands[1].tracked()&&h.hands[1].grabActive&&h.hands[1].grab&&ms&&now>=ms&&now-ms<=200&&rootCamera::firstPerson(root)&&pose.valid&&pose.session==ticket.session&&pose.origin==ticket.origin&&pose.clientWorld==root.world&&pose.root==entity&&pose.item==key.item&&pose.child==visual.child&&pose.milliseconds&&now>=pose.milliseconds&&now-pose.milliseconds<=200&&h.frame>=pose.frame&&h.frame-pose.frame<=2;
  }
  if(ticket.motion)logFile->event("motion_origin_frame_snapshot","\"coherent\":"+std::string(published.coherent()?"true":"false")+",\"hand_frame\":"+std::to_string(published.hands.frame)+",\"pose_frame\":"+std::to_string(published.pose.frame)+",\"hand_ms\":"+std::to_string(published.handMs)+",\"pose_ms\":"+std::to_string(published.pose.milliseconds)+",\"now_ms\":"+std::to_string(now)+",\"age_limit_ms\":200,\"origin_valid\":"+(originValid?"true":"false"));
  const bool qualified=(controllerUiFixture::attackRequested||controllerUiFixture::motionAttackRequested)&&originValid&&controllerUiFixture::attackButtons==XINPUT_GAMEPAD_X&&combatEpochGate::impact(ticket,key,now)&&root.entity==entity&&triggerEntity==entity&&visual.valid&&visual.item==key.item&&inputUi::boundNormal(ui,ui.senderContext,entity,now);
  const auto id=identity(candidate);int64_t authored=0,sourceEnd=0;const auto payload=sequenceAssetObserver::relative(candidate+4);authored=read<int64_t>(payload);std::memcpy(&sourceEnd,before.data()+0x1c0,8);
  logFile->event("motion_event_retime_qualification",std::string("\"motion\":")+(ticket.motion?"true":"false")+",\"origin_valid\":"+(originValid?"true":"false")+",\"ticket_serial\":"+std::to_string(ticket.serial)+",\"ticket_age_ms\":"+std::to_string(now>=ticket.issued?now-ticket.issued:UINT64_MAX)+",\"ticket_matches\":"+(combatEpochGate::impact(ticket,key,now)?"true":"false")+",\"qualified\":"+(qualified?"true":"false")+",\"native_trigger\":"+std::to_string(key.trigger)+",\"previous_trigger\":"+std::to_string(ticket.previousTrigger));
  const bool belongs=ticket.motion&&ticket.identifies(key,now)&&root.entity==entity&&triggerEntity==entity;
  if(belongs&&!qualified){
   if(!retimeLedger.block(key,id))throw std::runtime_error("event cancellation identity already consumed");++blocked;motionColliderChannel::eventBlocked=true;blockedActionLifetime::block(retimeLedger,key,id);
   logFile->event("motion_event_blocked",std::string("\"world\":")+hex(world)+",\"entity\":"+std::to_string(entity)+",\"item\":"+std::to_string(key.item)+",\"sequence\":"+std::to_string(key.sequence)+",\"trigger\":"+std::to_string(key.trigger)+",\"variant\":"+hex(candidate)+",\"guid\":"+jsonString(nativeImpactComponents::asHex(id.guid))+",\"ticket_identified\":true,\"ticket_revoked\":"+(ticket.revoked?"true":"false")+",\"origin_valid\":"+(originValid?"true":"false")+",\"latched\":true,\"asset_written\":false");
   dst.erase(std::remove(dst.begin(),dst.end(),candidate),dst.end());changed=dst!=src; // Includes an already-due native occurrence.
  }
  else if(authored>sourceEnd&&std::find(src.begin(),src.end(),candidate)==src.end()&&retimeLedger.consider(key,id,true,qualified,src.size(),h[2])==evr::motion::EventRetime::Decision::Append){dst.push_back(candidate);changed=adding=true;}

 }else if(combatEpochGate::suppress(key,retimeLedger.owns(key))){
  dst.clear();for(auto p:src){if(read<uint32_t>(p)==0x64b5dc20&&retimeLedger.consider(key,identity(p),false,false,src.size(),h[2])==evr::motion::EventRetime::Decision::Suppress){blockedActionLifetime::suppressed(key,identity(p));changed=true;continue;}dst.push_back(p);}
 }
 if(adding&&!combatEpochGate::newAllowed(combatEpochGate::Stage::Append,0,world)){
  const auto id=identity(candidate);if(!retimeLedger.block(key,id))throw std::runtime_error("epoch cancelled event identity already consumed");++blocked;motionColliderChannel::eventBlocked=true;blockedActionLifetime::block(retimeLedger,key,id);
  dst=src;dst.erase(std::remove(dst.begin(),dst.end(),candidate),dst.end());adding=false;changed=dst!=src;
  logFile->event("motion_event_epoch_cancelled","\"world\":"+hex(world)+",\"trigger\":"+std::to_string(key.trigger)+",\"latched\":true");
 }
 if(!changed)return;
 // Only the caller's stack-owned array and count change. Its storage/capacity/
 // allocator, Actor, and authored event asset remain native and untouched.
 std::memcpy(reinterpret_cast<void*>(h[0]),dst.data(),dst.size()*8);const uint64_t count=dst.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
 if(adding){if(!retimeLedger.commit(key,identity(candidate)))throw std::runtime_error("event retime duplicate commit");++appended;retainedScope={key,reinterpret_cast<uintptr_t>(actor),registryArgument};const auto ticket=controllerUiFixture::ticketSnapshot();motionColliderChannel::authorize({key,identity(candidate).guid,ticket.motion});}else suppressed+=unsigned(src.size()-dst.size());
 auto after=read<std::array<uintptr_t,4>>(address);bool stable=before==read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor))&&after[0]==h[0]&&after[2]==h[2]&&after[3]==h[3]&&after[1]==dst.size();
 for(size_t i=0;i<dst.size();++i)stable&=read<uintptr_t>(h[0]+i*8)==dst[i];
 std::ostringstream o;o<<"\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"sequence\":"<<key.sequence<<",\"item\":"<<key.item<<",\"trigger\":"<<key.trigger<<",\"call\":"<<unsigned(key.call)<<",\"sub\":"<<unsigned(key.sub)<<",\"operation\":"<<jsonString(adding?"append":"suppress")<<",\"before\":[";
 for(size_t i=0;i<src.size();++i){if(i)o<<',';o<<hex(src[i]);}o<<"],\"after\":[";for(size_t i=0;i<dst.size();++i){if(i)o<<',';o<<hex(dst[i]);}
 o<<"]"<<combatEpochGate::stampFields(key.epoch)<<",\"actor_and_storage_unchanged\":"<<(stable?"true":"false")<<",\"asset_written\":false,\"applied\":true";logFile->event("motion_event_retime",o.str());
 if(!stable)throw std::runtime_error("event retime postcondition");
}
inline void shadow(void* output,const void* actor,void* registry,const std::array<uint8_t,3104>& before,uintptr_t world,uint32_t entity){
 int64_t sourceStart=0,sourceEnd=0,length=0;std::memcpy(&sourceStart,before.data()+0x1b8,8);std::memcpy(&sourceEnd,before.data()+0x1c0,8);std::memcpy(&length,before.data()+0x188,8);
 if(!shadowRequested||sourceStart!=0)return;
 if(shadowCalls.fetch_add(1)>=controllerUiFixture::motionAttackLimit)throw std::runtime_error("shadow diagnostic action limit exceeded");
 if(length<=0||length>60000000000ll)throw std::runtime_error("shadow native sequence length bound");
 const auto header=read<std::array<uintptr_t,4>>(reinterpret_cast<uintptr_t>(output));
 if(header[1]>256||header[1]>header[2])throw std::runtime_error("shadow original array bound");
 std::vector<uintptr_t> saved;for(uint64_t i=0;i<header[1];++i)saved.push_back(read<uintptr_t>(header[0]+i*8));
 auto copy=before;const int64_t zero=0;std::memcpy(copy.data()+0x1b8,&zero,8);std::memcpy(copy.data()+0x1c0,&length,8);copy[0xbf0]=0;const auto expectedCopy=copy;
 std::array<uintptr_t,16> storage{};std::array<uintptr_t,4> result{reinterpret_cast<uintptr_t>(storage.data()),0,storage.size(),0};
 const std::array<uint8_t,8> filter{0x20,0xdc,0xb5,0x64,1,0,0,0};const auto expectedFilter=filter;
 // Execute original native filtering with private Actor times/output only.
 const auto status=original(result.data(),copy.data(),registry,0,filter.data());
 if(result[0]!=reinterpret_cast<uintptr_t>(storage.data())||result[1]>storage.size()||result[2]!=storage.size()||result[3]||!(status&255))throw std::runtime_error("private event selection failed bounds or allocation");
 bool outputSame=header==read<std::array<uintptr_t,4>>(reinterpret_cast<uintptr_t>(output));for(size_t i=0;i<saved.size();++i)outputSame&=saved[i]==read<uintptr_t>(header[0]+i*8);
 const bool actorSame=before==read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor)),copySame=copy==expectedCopy,filterSame=filter==expectedFilter;
 std::ostringstream o;o<<"\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"source_range\":["<<sourceStart<<','<<sourceEnd<<"],\"candidate_range\":[0,"<<length<<"],\"events\":[";
 for(uint64_t i=0;i<result[1];++i){if(i)o<<',';const auto variant=storage[i],payload=sequenceAssetObserver::relative(variant+4);const auto type=read<uint32_t>(variant);
  if(!payload||type!=0x64b5dc20)throw std::runtime_error("native type filter failed");
  o<<"{\"type_hash\":"<<type<<",\"variant\":"<<hex(variant)<<",\"payload\":"<<hex(payload)<<",\"spawn_impact\":"<<jsonString(sequenceAssetObserver::blob<488>(payload))<<'}';
 }
 o<<"],\"actor_unchanged\":"<<(actorSame?"true":"false")<<",\"output_unchanged\":"<<(outputSame?"true":"false")<<",\"private_actor_unchanged\":"<<(copySame?"true":"false")<<",\"input_filter_unchanged\":"<<(filterSame?"true":"false")<<",\"return_success\":true,\"applied\":false";
 logFile->event("motion_event_selection_shadow",o.str());
 if(!actorSame||!outputSame||!copySame||!filterSame)throw std::runtime_error("shadow event selection changed sources");
 if(retimeRequested){if(result[1]!=1)throw std::runtime_error("event retime requires exactly one qualified Impact");retime(output,actor,before,world,entity,storage[0],reinterpret_cast<uintptr_t>(registry));}
}
// Unlimited play (world-live-melee-contact.txt, word "early"): the single Impact of an attack that a swing started is
// handed to the game on the action's first tick and withheld when its authored time comes, so the hit volume exists
// while the real weapon is still moving through the target (include/evr/live_contact.hpp). The look ahead is the
// private native selection that shadow() above proved; only the caller's stack-owned result array changes. An action
// with no Impact or with several keeps the game's timing.
// "still" (same policy file): such an attack also plays no attack animation and neither turns nor moves the character.
// Its SetAnimationEvent (the animation with its root motion, 1.1 m forward for a sword stroke, game161403), SnapEvent
// (turn to the target) and RootMotionWarpEvent (step to the target) are withheld like the authored Impact: in VR the
// player's own arm is the swing. State, ability, action-opportunity, noise, sound and effect events stay.
// An attack starts with every ability off and enables walking, turning and jumping late (EnableAbilityEvent at 0.7 s
// for a sword stroke, game163200). Those EnableAbilityEvents are handed over as soon as the early Impact has run its
// authored duration (0.15 s for the sword) plus one tick. Not on the first tick: with the ability to move, stick input
// cancels the attack at once and the hit volume lived for a single update (game164930). Their authored occurrence
// stays (enabling twice changes nothing).
inline bool liveStill=false;inline std::atomic<unsigned> liveStilled{0},liveMovedEarly{0},liveAttacksEarly{0};
inline unsigned stillKeep=0;   // EVR_LIVE_STILL_KEEP, see movesCharacter
constexpr uint32_t enableAbilityEvent=0x0c04bc9a,movementAbilities=0x06000100; // CanJump (8), CanMove (25), CanRotate (26)
struct StillAction{evr::motion::EventActionKey key;uint64_t until=0,pressSerial=0;int64_t moveAt=0,attackAt=0;bool walkAborts=false;};inline StillAction stillAction; // guarded by retimeMutex
constexpr uint32_t setAbortByMovementEvent=0xc9ead48d;
// The owned attack once more, for the lock-free look at other Actor instances (stillReplica).
inline std::atomic<uint32_t> ownedSequence{0},ownedItem{0};inline std::atomic<int64_t> ownedTrigger{0},ownedMoveAt{0};inline std::atomic<uint64_t> ownedUntil{0},ownedSerial{0};inline std::atomic<bool> ownedWalkAborts{false};inline std::atomic<int64_t> ownedAttackAt{0};
inline void publishOwned(){ownedSerial=stillAction.pressSerial;ownedSequence=stillAction.key.sequence;ownedItem=stillAction.key.item;ownedTrigger=int64_t(stillAction.key.trigger);ownedMoveAt=stillAction.moveAt;ownedAttackAt=stillAction.attackAt;ownedWalkAborts=stillAction.walkAborts;ownedUntil=stillAction.until;}
// The action's events of one type (the private native look ahead, as everywhere here): visit(payload) for each one.
template<class Visit> inline size_t actionEvents(const std::array<uint8_t,3104>& before,void* registry,int64_t length,uint32_t hash,Visit visit){
 auto copy=before;const int64_t zero=0;std::memcpy(copy.data()+0x1b8,&zero,8);std::memcpy(copy.data()+0x1c0,&length,8);copy[0xbf0]=0;const auto expected=copy;
 std::array<uintptr_t,64> storage{};std::array<uintptr_t,4> result{reinterpret_cast<uintptr_t>(storage.data()),0,storage.size(),0};
 std::array<uint8_t,8> filter{};std::memcpy(filter.data(),&hash,4);filter[4]=1;
 original(result.data(),copy.data(),registry,0,filter.data());
 if(result[0]!=reinterpret_cast<uintptr_t>(storage.data())||result[1]>storage.size()||result[2]!=storage.size()||result[3]||copy!=expected)throw std::runtime_error("action look ahead failed bounds");
 for(size_t i=0;i<result[1];++i){const auto event=storage[i];const auto data=sequenceAssetObserver::relative(event+4);
  if(read<uint32_t>(event)!=hash||!data)throw std::runtime_error("action look ahead returned another event type");visit(data);}
 return result[1];
}
// What an owned action says about walking (see movesNow):
//  walkAborts  it carries SetAbortByMovement: the strokes of the felling axe and of the pickaxe (both with 0, at
//              0 ms). The pickaxe's keeps ability 25 all along, and the character stands for its 0.87 s all the same
//              (Mock run game191229, the stick forward from 0.05 s to 0.7 s: 0.00 m) - during an action a walk is
//              always the action being broken off.
//  payAt       its last PayUsageCost (felling axe 250 ms, pickaxe 417 ms; the attacks carry none). A walk must not
//              break the action off before the tool's use is paid for.
//  attackAt    when it lets the next attack begin: its first EnableAbilityEvent with ability 0 (a request to attack
//              passes 1d8a20 only with it; one-hand sword 550 ms, great axe 700 ms, daggers 600 ms, fists 700 ms,
//              felling axe 1200 ms, pickaxe 500 ms). The cooldown of the stroke that started it is this long
//              (include/evr/live_melee.hpp LiveCooldown).
// Actions that move the character on purpose (2026-10-15, user: "the jump attack skill falls on the enemy with a
// shock wave; such a skill's fall or warp must really move"): an action with a jump, a teleport, an impulse or a
// gravity change among its events (the jump attacks: JumpEvent, SetGravityEvent; the weapon skills' leaps) is left to
// the game as authored - nothing of it is withheld or brought forward, on any Actor instance, and its hit is the
// game's own. The lunge of an ordinary attack (SnapEvent, RootMotionWarpEvent) is no such move: those stay withheld.
// EVR_MOVING_SEQUENCES=<number,...> names further sequences that are left to the game as a whole (none by default; until
// 2026-10-17 the five dodge attacks, which the catalog below now knows as skill attacks).
// The 1.0 attack spec (STATUS 10-16 03:25, user 2026-10-17): a normal attack does not move; a skill attack (the skill
// tree's: slash up, crash down, dodge, whirl, kick ...; the items' special abilities) moves as the game's animation does,
// the direction from where the head looks; no swing animation is seen (the body is the IK's); a shock wave comes as
// authored; the hit itself is the player's own weapon, which deals it whenever it touches for the length of the skill's
// action (no second swing for a move-then-strike skill); no double hit by shock wave and weapon unless the attack hits
// several times by itself. Which action is which comes from the game data (include/evr/attack_catalog.hpp, generated
// from docs/analysis/attacks_1_0_20261017): Normal - the still route below; Skill with a weapon hit - skillAction:
// nothing withheld but the authored weapon hits, which are handed over on the first tick and live until the next attack
// may begin (impact_lifetime.hpp); LeftToGame - a skill without a weapon hit (crash down: shock waves only; the kicks:
// the foot; the wands' and the bow's) and any action the catalog does not know that carries a moving event (June's rule).
constexpr std::array<uint32_t,9> movingEvents{0x75dd70a9,0x5c77582c,0xb9d17d1c,0xaacc8de0,0x81f6f852,0xfbef2dd9,0x0e70900f,0x0ee2d5dc,0x790aaee4};
   // JumpEvent, TeleportToTargetEvent, StoreEnemyMarkerPositionAsTargetEvent, ApplyImpulseEvent, ImpulseEvent, PushEvent,
   // SetGravityEvent, ResetGravityEvent, ResetFallAltitudeOnApexEvent (keen::actor, type-hash-index.json)
inline const std::vector<uint32_t> movingSequences=[]{
 std::vector<uint32_t> v;
 char buf[512]{};const auto n=GetEnvironmentVariableA("EVR_MOVING_SEQUENCES",buf,sizeof buf);
 if(n&&n<sizeof buf){std::string s(buf,n);size_t p=0;
  while(p<=s.size()){const auto q=s.find(',',p);const auto part=s.substr(p,q==std::string::npos?std::string::npos:q-p);
   if(!part.empty())v.push_back(uint32_t(std::strtoul(part.c_str(),nullptr,10)));if(q==std::string::npos)break;p=q+1;}}
 return v;}();
// EVR_SKILL_ATTACKS=0 in the game's environment: every skill attack left to the game as before 2026-10-17 (a comparison).
inline const bool skillRoute=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SKILL_ATTACKS",v,sizeof v);return !(n==1&&v[0]=='0');}();
enum class ActionKind {Normal,Skill,LeftToGame};
inline std::atomic<unsigned> movingSeen{0};
inline ActionKind actionKind(const std::array<uint8_t,3104>& before,void* registry,int64_t length,uint32_t sequence,uint32_t& by){
 by=0;for(auto s:movingSequences)if(s==sequence){by=s;return ActionKind::LeftToGame;}
 if(const auto a=evr::attacks::find(sequence)){by=sequence;if(!a->skill)return ActionKind::Normal;return skillRoute&&a->weaponHits?ActionKind::Skill:ActionKind::LeftToGame;}
 for(auto h:movingEvents)if(actionEvents(before,registry,length,h,[](auto){})){by=h;return ActionKind::LeftToGame;}
 return ActionKind::Normal;
}
inline bool movingAction(const std::array<uint8_t,3104>& before,void* registry,int64_t length,uint32_t sequence,uint32_t& by){
 return actionKind(before,registry,length,sequence,by)!=ActionKind::Normal;}
// The skill attack a swing started (liveEvents): its key, until when it plays.
struct SkillAction{evr::motion::EventActionKey key;uint64_t until=0;};inline SkillAction skillAction;   // guarded by retimeMutex
inline std::atomic<unsigned> skillsStarted{0},skillsLogged{0};
// EVR_SPECIAL_ABILITY_OWNED=0: an item's special ability (begun by its button) keeps the game's own hits.
constexpr uint16_t specialAbilityTrigger=15;   // keen::actor::ActionSequenceTrigger::SpecialAbility (1.0 executable)
inline const bool specialAbilityOwned=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SPECIAL_ABILITY_OWNED",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline bool skillOwned(const evr::motion::EventActionKey& key,uint64_t now){return skillAction.until>=now&&skillAction.key==key;}
// An action of the wand that a swing started (LiveAttackPress::spawnsOnly): its spawns came on the first tick, the rest stays the game's.
inline SkillAction wandAction;   // guarded by retimeMutex
inline bool wandOwned(const evr::motion::EventActionKey& key,uint64_t now){return wandAction.until>=now&&wandAction.key==key;}
inline std::atomic<unsigned> wandShotsEarly{0};
// The bow's arrow at the hand's release (2026-10-21, the user: "bring it forward"). The game's shoot action
// (Attack_Player_Ranged_Bow_Shoot, 800 ms; game data) loops 200..400 ms while the string is drawn and makes its arrow at
// 400 ms (three SpawnEntityEvents, three more at 433.3 ms; one arrow came of them on each shot, headset log
// 20261021-123610); the arrow came 151 .. 162 ms after the hand let go. The game takes the shot when RT is let go
// (the own ranged weapon's state 2 -> 3, Mock game-20261021-134251: 111 / 126 ms after the hand with the fixed 80 ms tap;
// handed at the hand's release, before that, the arrow was refused - bow_spawn_observer wants the state 3 - and none
// came). Once this player has let go (controllerUiFixture::bowReleasedAt, not older than bowEarlyMs), the game has taken
// the shot (bowFired) and the action is past its nocking (200 ms) but short of its arrow, the action's spawns (from the
// first one up to 50 ms after it) and the sounds authored with the first are handed at once (the private look ahead, as
// for a swing) and withheld when their authored time comes - for that release only. Nothing else of the action changes.
// Not the ledger: the action's key may change when it leaves its loop (not checked). EVR_BOW_EARLY=0: as before.
// Headset run 20261021-150238 (with the state-3 rule and a two-poll tap): the arrow came 100 .. 110 ms after the hand, and
// the state 3 came with the 400 ms every time (49 looks, none handed). So (15:30) the arrow is handed at the release
// itself - the bow's spawn is let through before the state 3 for it (controllerUiFixture::bowEarlySpawnUntil) - and the
// strong shot's (Attack_Player_SpecialAbility_Strong_Shot_Bow, 1963645007: spawns from 400 ms) at that action's start.
inline const bool bowEarly=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_BOW_EARLY",v,sizeof v);return !(n==1&&v[0]=='0');}();
constexpr uint32_t bowShootSequence=2886385501u,strongShotSequence=1963645007u;constexpr int64_t bowNocked=200000000;constexpr uint64_t bowEarlyMs=400,strongEarlyMs=2000;
// The game has taken this player's shot: the own ranged weapon's state is 3 (2 while drawn; input_ranged_observer, fresh).
inline bool bowFired(uintptr_t world,uint64_t now){
 std::lock_guard<std::mutex> lock(inputRanged::bowMutex);
 for(const auto& v:inputRanged::bowIdentities)if(v.world==world&&v.milliseconds&&now>=v.milliseconds&&now-v.milliseconds<=100)return v.bytes[0]==3;
 return false;
}
struct BowShot{uint64_t released=0,until=0;uint32_t sequence=0;std::array<uintptr_t,8> variants{};std::array<bool,8> gone{};size_t count=0;};
inline BowShot bowShot;   // guarded by retimeMutex
inline std::atomic<unsigned> bowShotsEarly{0},bowWithheld{0},bowLogged{0};
// The action's events of one type with their authored times (the private native look ahead over the whole action).
inline void bowLook(const std::array<uint8_t,3104>& before,void* registry,int64_t length,uint32_t hash,std::vector<std::pair<uintptr_t,int64_t>>& out){
 auto copy=before;const int64_t zero=0;std::memcpy(copy.data()+0x1b8,&zero,8);std::memcpy(copy.data()+0x1c0,&length,8);copy[0xbf0]=0;const auto expected=copy;
 std::array<uintptr_t,64> storage{};std::array<uintptr_t,4> result{reinterpret_cast<uintptr_t>(storage.data()),0,storage.size(),0};
 std::array<uint8_t,8> filter{};std::memcpy(filter.data(),&hash,4);filter[4]=1;
 const auto status=original(result.data(),copy.data(),registry,0,filter.data());
 if(result[0]!=reinterpret_cast<uintptr_t>(storage.data())||result[1]>storage.size()||result[2]!=storage.size()||result[3]||!(status&255)||copy!=expected)throw std::runtime_error("bow look ahead failed bounds or allocation");
 for(size_t i=0;i<result[1];++i){const auto p=sequenceAssetObserver::relative(storage[i]+4);
  if(read<uint32_t>(storage[i])!=hash||!p)throw std::runtime_error("bow look ahead returned another event type");out.push_back({storage[i],read<int64_t>(p)});}
}
inline void movingNoted(uint32_t sequence,uint32_t item,int64_t trigger,uint32_t by,const char* where){
 if(movingSeen.fetch_add(1)<60)try{logFile->event("live_event_moving_action","\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+",\"trigger\":"+std::to_string(trigger)+",\"by\":"+hex(by)+",\"where\":\""+where+"\",\"left_to_the_game\":true");}catch(...){}
}
struct StillTraits {bool walkAborts=false;int64_t payAt=0,attackAt=0;};
constexpr uint32_t payUsageCostEvent=0x4e5c0937,attackAbilities=0x1f;
inline StillTraits stillTraits(const std::array<uint8_t,3104>& before,void* registry,int64_t length){
 StillTraits t;
 t.walkAborts=actionEvents(before,registry,length,setAbortByMovementEvent,[](uintptr_t){})>0;
 actionEvents(before,registry,length,payUsageCostEvent,[&](uintptr_t data){t.payAt=std::max(t.payAt,read<int64_t>(data));});
 actionEvents(before,registry,length,enableAbilityEvent,[&](uintptr_t data){const auto at=read<int64_t>(data);
  if((read<uint32_t>(data+96)&1u)&&at>0&&at<=length&&(!t.attackAt||at<t.attackAt))t.attackAt=at;});
 return t;
}
// The user's rule for the cooldown (2026-10-12): the next stroke attacks when 80 % of it has passed. The stroke's
// press reaches the game a little later than it was made, and not always by the same time, so the abilities to
// attack are handed over a tenth of a second before that point of the action; no stroke presses before it (the
// cooldown is kept where the strokes are judged, controller_ui_fixture.hpp).
constexpr int64_t attackEarlyMargin=100000000;
inline int64_t attackEarlyAt(int64_t attackAt){
 return attackAt>0?std::max<int64_t>(16666667,int64_t(double(attackAt)*double(controllerUiFixture::liveCooldown.share))-attackEarlyMargin):0;}
inline bool attacksNow(const evr::motion::EventActionKey& key,uint64_t now,int64_t start,int64_t end){
 return stillAction.until>=now&&stillAction.key==key&&stillAction.attackAt>0&&end>=attackEarlyAt(stillAction.attackAt)&&start<stillAction.attackAt;}
// After a stroke of the felling axe (Mine_Chop_Tree_Interaction1: DisableAbilityEvent 0x0600041f at 0 and 0x06000000
// at 67 ms, movement factors 0 at 400 ms) the character did not walk until the action was over, with the stick
// forward from 0.3 s on (Mock runs game160101 and game162110). Withholding the movement factors did not change that,
// and neither did handing these abilities over on every tick from moveAt for 0.4 s here (game162110, taken back).
// The abilities are read on two sides: the type switches at 222b20 and at 224650 both take EnableAbilityEvent
// (0c04bc9a), the second next to the effect and sound notifiers - the presentation side, which plays the action on
// another Actor instance (stillReplica below). That side kept the action's own DisableAbilityEvents and got its
// EnableAbilityEvents only at their authored time; the sword's attack never disables walking by an event, which is
// why it did not show there. stillReplica now hands the movement abilities over on that side as well.
// What the Mock runs of 2026-10-12 showed for the felling axe, the stick forward from 0.3 s after the stroke:
//   this Actor's abilities for 0.4 s, nothing on the other side (game162110): no step until the action is over
//   this Actor on the one tick of moveAt, the other side for 0.4 s with its factors withheld (game173256): the
//     action is broken off by the walk at 1.36 s - when the authored EnableAbilityEvent of 1333 ms comes
//   both sides for 0.4 s (game175028): no step, and the action is not broken off at all (worse)
// So the second form stays. The early events do not open the walk on either side, and on this Actor, offered for
// longer, they even keep the authored one from working. How the abilities are held (the handlers at 222d06 and
// 2247b4 or their state at Actor+bc8) is not understood yet; the stroke of the axe still holds the character 1.4 s.
// (EVR_LIVE_STILL_KEEP bit 32 brings the third form back for a trial.)
// What holds the character is not the ability mask. Trace of Actor+bc8 (game182153, EVR_STILL_EXPERIMENT 128): on
// that run the early events did arrive - 0x011e2000 became 0x071e257f at 117 ms on this Actor and on the other
// instances - and the character still stood for the whole action, and the action was not broken off either. The
// game's own stroke (game181830, no early route at all) holds the character for the whole action too, 1.95 s with
// the stick forward throughout. A tool's action is one that a walk breaks off (SetAbortByMovement at 0 ms), and it
// seems to be broken off when an EnableAbilityEvent for walking arrives while the stick is pushed: in game173256 the
// early events had found no array, and the authored one at 1333 ms broke the action off. Abilities that are already
// set when it comes take that moment away.
// Tried on that reading (game182717): the abilities handed over only while the walk stick is pushed. The mask did
// change at 333 ms with the stick down, on every instance - and the character stood all the same. Taken out again.
// What it is (the executable, read 2026-10-12 evening): the start of an action sets Actor+1b1 bits 0 and 4 (1db123:
// playing, and "a walk breaks it off"). A request of kind 21 passes 1d8a20 while an action plays only with ability
// 25 and with bit 4. SetAbortByMovement (c9ead48d, handler 2299e0) sets or clears bit 4 by its byte at +96, and the
// actions of the felling axe and of the pickaxe carry it at 0 ms with 0: they are made so that a walk does not break
// them off (the trace of game182153 shows the byte going from 17 to 1 at 16 ms). The sword's attacks carry no such
// event: there the walk breaks the action off as soon as the abilities are back, which is why they never showed
// this. So for an action that carries the event (StillTraits), the early stroke done, bit 4 is put back as the
// action's start had it (walkAbort below), and the abilities are offered for 0.4 s here too: a tick without an
// array must not lose them. Mock runs game185142 and game191229: the stick forward 0.27 s after the felling axe's
// stroke, the action is broken off at 0.3 s and the character walks (before: it stood for 1.8 s); without the stick
// the action plays to its end as before.
// EVR_LIVE_STILL_KEEP bit 64 leaves the bit as the action set it and keeps the one tick.
inline bool movesNow(const evr::motion::EventActionKey& key,uint64_t now,int64_t start,int64_t end){
 if(!(stillAction.until>=now&&stillAction.key==key))return false;
 if((stillKeep&32)||(stillAction.walkAborts&&!(stillKeep&64)))return stillAction.moveAt>0&&stillAction.moveAt<=end&&start<stillAction.moveAt+400000000;
 return stillAction.moveAt>start&&stillAction.moveAt<=end;}
// Appends the action's EnableAbilityEvents with these abilities that are not yet due. Private native look ahead.
inline unsigned earlyAbilities(std::vector<uintptr_t>& dst,const std::array<uint8_t,3104>& before,void* registry,int64_t length,int64_t sourceEnd,uintptr_t capacity,uint32_t abilities){
 auto copy=before;const int64_t zero=0;std::memcpy(copy.data()+0x1b8,&zero,8);std::memcpy(copy.data()+0x1c0,&length,8);copy[0xbf0]=0;const auto expected=copy;
 std::array<uintptr_t,64> storage{};std::array<uintptr_t,4> found{reinterpret_cast<uintptr_t>(storage.data()),0,storage.size(),0};const std::array<uint8_t,8> filter{0x9a,0xbc,0x04,0x0c,1,0,0,0};
 original(found.data(),copy.data(),registry,0,filter.data());
 if(found[0]!=reinterpret_cast<uintptr_t>(storage.data())||found[1]>storage.size()||found[2]!=storage.size()||found[3]||copy!=expected)throw std::runtime_error("live ability look ahead failed bounds");
 unsigned added=0;for(size_t i=0;i<found[1];++i){const auto event=storage[i];const auto data=sequenceAssetObserver::relative(event+4);
  if(read<uint32_t>(event)!=enableAbilityEvent||!data)throw std::runtime_error("live ability look ahead returned another event type");
  if(!(read<uint32_t>(data+96)&abilities)||read<int64_t>(data)<=sourceEnd||std::find(dst.begin(),dst.end(),event)!=dst.end()||dst.size()+1>capacity)continue;
  dst.push_back(event);++added;}
 return added;
}
// For moving, turning and jumping.
inline unsigned earlyMovementAbilities(std::vector<uintptr_t>& dst,const std::array<uint8_t,3104>& before,void* registry,int64_t length,int64_t sourceEnd,uintptr_t capacity){
 return earlyAbilities(dst,before,registry,length,sourceEnd,capacity,movementAbilities);}
// Also withheld: VfxNotifierEvent (0x08e653f0), the swing trail. It is authored for the attack animation's arc, which
// is not what the player's arm does (headset run game180455: "the swing effect follows the original animation; remove
// it and keep the hit effect"). Hit effects come from the Impact, not from the sequence, and stay.
// Diagnostic for the white arc of the one-hand sword (EVR_STILL_EXPERIMENT in the game's environment, bit field; no
// launch flag sets it): 1 also withholds noise, sound and foley notifiers; 2 the authored Impact at the other call
// sites; 4 action opportunities, combat stance, velocity and hole avoidance; 8 everything but state and abilities;
// 16 the state event as well; 32 the ability events as well; 64 logs other Actor instances that play the owned
// attack's sequence (read-only); 128 logs, for every Actor instance that plays it, the ability mask the game keeps
// (Actor+bc8: 2247b4 ORs an EnableAbilityEvent's mask into it, 2249d1 clears a DisableAbilityEvent's) once a tick
// (read-only; event live_still_abilities).
inline unsigned stillExperiment=0;
// Headset run game145015 (2026-10-12), with the hit handed over on the first tick:
//  - "the swing's sound comes after the hit's": SfxNotifierEvent keeps its authored time (sword 250 ms, daggers up to
//    700 ms) while the Impact is early. The game has no way to play it before the stroke that starts the action, so
//    it is withheld (user: "if it cannot come earlier, no swing sound is fine"). The hit's own sound is the Impact's.
//  - "after a stroke of the felling axe I cannot move for a moment": Mine_Chop_Tree_Interaction1 sets the movement
//    factors to 0 at 400 ms (LerpMovementFactorEvent) and nothing sets them back before the action ends at 1.8 s.
//    The factors slow the character for the animation; a still action withholds them (and the rotation factors).
// EVR_LIVE_STILL_KEEP in the game's environment (bit field, no launch flag) takes single parts back for a comparison:
// 1 the swing's sound stays, 2 only attacks with exactly one Impact are taken (as before 2026-10-12), 4 a tool's
// stroke without an Impact keeps the game's timing, 8 the movement and rotation factors stay, 16 the presentation
// side gets its movement abilities at their authored time, 32 this Actor is offered its movement abilities for
// 0.4 s for every action, 64 a tool's action stays one that a walk does not break off (see movesNow).
// The turn to the target: SnapEvent (20c70f55) until the June data; 1.0 has none and RotationLerpEvent (d102d6dc, the
// type's name from the 1.0 executable, vrmod/tools/reflection_type_by_hash.py) stands at the same time in 542 events
// of the sequences both versions share (docs/analysis/attacks_1_0_20261017).
constexpr uint32_t snapEvent=0x20c70f55,rotationLerpEvent=0xd102d6dc;
// A normal attack does not move (1.0 spec): its pushes as well (the great axe's third stroke, Attack_Player_2H_Greataxe_03_
// Diagonal: ImpulseEvent 0.5 forward at 417 ms; ApplyImpulseEvent and PushEvent likewise).
constexpr uint32_t impulseEvent=0x81f6f852,applyImpulseEvent=0xaacc8de0,pushEvent=0xfbef2dd9;
inline bool movesCharacter(uint32_t type){
 if(type==0x4d94aa27||type==snapEvent||type==rotationLerpEvent||type==0xd3080aa1||type==0x08e653f0)return true;
 if(type==impulseEvent||type==applyImpulseEvent||type==pushEvent)return true;
 if(!(stillKeep&1)&&type==sfxNotifierEvent)return true;
 if(!(stillKeep&8)&&(type==lerpMovementFactorEvent||type==lerpRotationFactorEvent))return true;
 if((stillExperiment&1)&&(type==0xed2e6db7||type==0x306db875||type==0x592a72d6))return true;
 if((stillExperiment&4)&&(type==0xf6c3e0b8||type==0xfc30e4d6||type==0x88115e14||type==0xb1c5e63d||type==0xa194d94d||type==0x65ab3720))return true;
 if((stillExperiment&8)&&type!=0x45ba5932&&type!=0x0c04bc9a&&type!=0xbbac411f&&type!=0x64b5dc20)return true;
 if((stillExperiment&16)&&type==0x45ba5932)return true;
 if((stillExperiment&32)&&(type==0x0c04bc9a||type==0xbbac411f))return true;
 return false;
}
inline bool liveRetime=false;inline evr::motion::LiveEventLedger<32> liveLedger;inline evr::motion::LiveAttackPress(*livePress)()=nullptr;
// Bare hands have no shown weapon; their attack is the game's unarmed item while the fists are active (motion_fist.hpp).
inline bool (*bareHands)(uint32_t item,uint64_t now)=nullptr;
inline std::atomic<unsigned> liveAppended{0},liveSuppressed{0},liveLookAheads{0},liveNotSingle{0},liveUnqualified{0};
// The events of an action that are its stroke, by the private native look ahead over the whole action that shadow()
// proved: the lasting Impacts (sword 150 ms, great axe 233 ms, daggers 167 ms, fists 117 ms; the bare hands' attack
// also carries three Impacts without a length, run game073332 - those keep their authored time). An action without
// any Impact may be a tool's stroke: the pickaxe digs with one SpawnEntityEvent at 417 ms (game data,
// Mine_Terraforming_*), which then is the stroke. Of these, selectEarly() takes the ones authored first.
// weaponOnly (a skill attack): only the hits on a hand (SpawnEntityBaseEvent.attachmentSlot, payload +145: HandR 8, HandL 9,
// HandPalmR 14, HandPalmL 15); a shock wave (no slot, 36) or a kick (FootR 5) keeps its authored time and place.
inline bool weaponSlot(uint8_t slot){return slot==8||slot==9||slot==14||slot==15;}
inline size_t earlyCandidates(const std::array<uint8_t,3104>& before,void* registry,int64_t length,std::array<evr::motion::EarlyEvent,evr::motion::maximumEarlyEvents>& early,uint32_t& type,size_t& impacts,size_t& lasting,bool weaponOnly=false,int64_t together=1000000){
 std::array<uintptr_t,16> storage{};
 auto look=[&](uint32_t hash)->size_t{
  auto copy=before;const int64_t zero=0;std::memcpy(copy.data()+0x1b8,&zero,8);std::memcpy(copy.data()+0x1c0,&length,8);copy[0xbf0]=0;const auto expected=copy;
  storage={};std::array<uintptr_t,4> result{reinterpret_cast<uintptr_t>(storage.data()),0,storage.size(),0};
  std::array<uint8_t,8> filter{};std::memcpy(filter.data(),&hash,4);filter[4]=1;
  const auto status=original(result.data(),copy.data(),registry,0,filter.data());
  if(result[0]!=reinterpret_cast<uintptr_t>(storage.data())||result[1]>storage.size()||result[2]!=storage.size()||result[3]||!(status&255)||copy!=expected)throw std::runtime_error("live look ahead failed bounds or allocation");
  for(size_t i=0;i<result[1];++i)if(read<uint32_t>(storage[i])!=hash||!sequenceAssetObserver::relative(storage[i]+4))throw std::runtime_error("live look ahead returned another event type");
  return size_t(result[1]);};
 std::array<evr::motion::EarlyEvent,32> found{};size_t count=0;
 type=spawnImpactEvent;impacts=look(spawnImpactEvent);lasting=0;
 for(size_t i=0;i<impacts;++i){const auto p=sequenceAssetObserver::relative(storage[i]+4);
  if(weaponOnly&&!weaponSlot(read<uint8_t>(p+145)))continue;
  if(impacts==1||weaponOnly||read<int64_t>(p+8)>0){found[count++]={storage[i],read<int64_t>(p),read<int64_t>(p+8)};++lasting;}}
 if(weaponOnly)return evr::motion::selectEarly(found.data(),count,early);
 if((stillKeep&2)&&lasting!=1)return 0;
 if(!impacts&&!(stillKeep&4)){type=spawnEntityEvent;const auto spawns=look(spawnEntityEvent);
  for(size_t i=0;i<spawns;++i){const auto p=sequenceAssetObserver::relative(storage[i]+4);found[count++]={storage[i],read<int64_t>(p),read<int64_t>(p+8)};}}
 return evr::motion::selectEarly(found.data(),count,early,together);
}
inline void liveEvents(void* output,const void* actor,void* registry,const std::array<uint8_t,3104>& before,uintptr_t world,uint32_t entity){
 if(!liveRetime)return;std::lock_guard<std::mutex> lock(retimeMutex);
 const auto key=actionKey(before,world,entity);if(!key.valid())return;
 const auto now=GetTickCount64();const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
 if(h[1]>256||h[1]>h[2]||(!h[0]&&h[1]))throw std::runtime_error("live event array bounds");
 std::vector<uintptr_t> src;for(size_t i=0;i<h[1];++i)src.push_back(read<uintptr_t>(h[0]+i*8));auto dst=src;
 int64_t sourceStart=0,sourceEnd=0,length=0,authored=0;std::memcpy(&sourceStart,before.data()+0x1b8,8);std::memcpy(&sourceEnd,before.data()+0x1c0,8);std::memcpy(&length,before.data()+0x188,8);
 using Decision=evr::motion::LiveEventLedger<32>::Decision;evr::motion::LiveAttackPress press;unsigned impacts=0,stilled=0,moved=0,attacks=0;int64_t attackAt=0;
 std::array<evr::motion::EarlyEvent,evr::motion::maximumEarlyEvents> early{};size_t earlyCount=0;uint32_t earlyType=spawnImpactEvent;
 bool skill=false,bow=false;
 if(liveLedger.owns(key,now)){
  // A skill attack keeps its animation, turn, step, pushes, sounds and effects: only its authored weapon hits go.
  skill=skillOwned(key,now)||wandOwned(key,now);   // the wand's swing: nothing of its motion withheld either
  dst.clear();for(auto p:src){const auto type=read<uint32_t>(p);
   if((type==spawnImpactEvent||type==spawnEntityEvent)&&liveLedger.consider(key,identity(p),false,false,src.size(),h[2],now)==Decision::Suppress){++impacts;continue;}
   if(liveStill&&!skill&&movesCharacter(type)){++stilled;continue;}
   dst.push_back(p);}
  if(liveStill&&!skill&&h[0]&&movesNow(key,now,sourceStart,sourceEnd))moved=earlyMovementAbilities(dst,before,registry,length,sourceEnd,h[2]);
  if(liveStill&&!skill&&h[0]&&attacksNow(key,now,sourceStart,sourceEnd))attacks=earlyAbilities(dst,before,registry,length,sourceEnd,h[2],attackAbilities);
 }else if(bowEarly&&((key.sequence==bowShootSequence&&sourceStart>0)||key.sequence==strongShotSequence)){
  // The bow's arrow at the release, the strong shot's at its start (bowEarly above).
  bow=true;const auto root=rootCamera::snapshot();uint32_t triggerEntity=0;std::memcpy(&triggerEntity,before.data()+16,4);
  if(root.entity!=entity||triggerEntity!=entity||!h[0])return;
  const bool strongAction=key.sequence==strongShotSequence;
  const auto released=strongAction?controllerUiFixture::strongShotAt.load():controllerUiFixture::bowReleasedAt.load();
  if(strongAction&&sourceStart==0){static std::atomic<unsigned> told{0};uint16_t actionTrigger=0;std::memcpy(&actionTrigger,before.data()+0x18,2);
   if(told.fetch_add(1)<100)logFile->event("strong_shot_begun","\"frame\":"+std::to_string(liveFrame.frame)+",\"action_trigger\":"+std::to_string(actionTrigger)+
    ",\"release_age_ms\":"+std::to_string(released&&now>=released?now-released:0));}
  if(bowShot.count&&bowShot.released==released&&bowShot.sequence==key.sequence&&now<=bowShot.until){
   dst.clear();for(auto p:src){bool handed=false;for(size_t i=0;i<bowShot.count&&!handed;++i)if(!bowShot.gone[i]&&bowShot.variants[i]==p){bowShot.gone[i]=true;handed=true;}
    if(handed){++impacts;continue;}dst.push_back(p);}
   bowWithheld+=impacts;
  }else if(released&&released!=bowShot.released&&now>=released&&now-released<=(strongAction?strongEarlyMs:bowEarlyMs)&&sourceStart>=(strongAction?0:bowNocked)&&length>0&&length<=60000000000ll){
   const bool fired=bowFired(world,now);
   bowShot={};bowShot.released=released;bowShot.sequence=key.sequence;   // one look ahead per release
   std::vector<std::pair<uintptr_t,int64_t>> spawns,sounds;bowLook(before,registry,length,spawnEntityEvent,spawns);bowLook(before,registry,length,sfxNotifierEvent,sounds);++liveLookAheads;
   bool same=h==read<std::array<uintptr_t,4>>(address)&&before==read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));
   for(size_t i=0;i<src.size();++i)same=same&&src[i]==read<uintptr_t>(h[0]+i*8);
   if(!same)throw std::runtime_error("bow look ahead changed its sources");
   int64_t first=-1;for(const auto& s:spawns)if(s.second>=(strongAction?0:bowNocked)&&(first<0||s.second<first))first=s.second;
   std::array<uintptr_t,8> handed{};size_t n=0,arrows=0;bool fits=first>=0;
   for(const auto& s:spawns)if(first>=0&&s.second>=first&&s.second-first<=50000000){if(n==handed.size()){fits=false;break;}handed[n++]=s.first;++arrows;}
   for(const auto& s:sounds)if(fits&&first>=0&&(s.second>first?s.second-first:first-s.second)<=1000000){if(n==handed.size()){fits=false;break;}handed[n++]=s.first;}
   bool present=false;for(size_t i=0;i<n;++i)present=present||std::find(src.begin(),src.end(),handed[i])!=src.end();
   const char* why=!fits?"\"no_arrow_or_too_many\"":first<=sourceEnd||present?"\"already_due\"":nullptr;
   if(bowLogged.fetch_add(1)<200)logFile->event("bow_arrow_look","\"sequence\":"+std::to_string(key.sequence)+",\"state3\":"+(fired?"true":"false")+",\"release_age_ms\":"+std::to_string(now-released)+",\"range\":["+std::to_string(sourceStart)+","+std::to_string(sourceEnd)+"],\"length\":"+std::to_string(length)+
    ",\"loop_flag\":"+std::to_string(before[0xbf0])+",\"spawns\":"+std::to_string(spawns.size())+",\"sounds\":"+std::to_string(sounds.size())+",\"first\":"+std::to_string(first)+",\"arrows\":"+std::to_string(arrows)+
    ",\"handed\":"+std::to_string(why?0:n)+",\"not_handed\":"+(why?why:"null"));
   if(why)return;
   bowShot.until=now+(strongAction?2000:1000);bowShot.count=n;bowShot.variants=handed;
   controllerUiFixture::bowEarlySpawnUntil=now+100;   // the spawn comes within this tick; before the game's shot state
   for(size_t i=0;i<n;++i)dst.push_back(handed[i]);
   earlyCount=n;earlyType=spawnEntityEvent;authored=first;++bowShotsEarly;
  }else return;
 }else if(sourceStart==0&&livePress){
  press=livePress();const auto root=rootCamera::snapshot();uint32_t triggerEntity=0;std::memcpy(&triggerEntity,before.data()+16,4);
  // An item's special ability (the twin blades' spin, the great sword's spin, the felling axe's whirl, ...) begins by
  // its button, not by a swing: the action's trigger (Actor+0x18) is SpecialAbility. It is a skill attack all the same,
  // and its weapon hits are the real weapon's as well when the catalog gives it one.
  uint16_t actionTrigger=0;std::memcpy(&actionTrigger,before.data()+0x18,2);
  // The bow's strong shot begun by this player (Attack_Player_SpecialAbility_Strong_Shot_Bow, 1 s; the 1.0 attack table).
  if(key.sequence==1963645007u&&root.entity==entity&&triggerEntity==entity){controllerUiFixture::strongShotAt=now;
   static std::atomic<unsigned> told{0};if(told.fetch_add(1)<100)logFile->event("strong_shot_begun","\"frame\":"+std::to_string(liveFrame.frame)+",\"action_trigger\":"+std::to_string(actionTrigger));}
  const auto* listed=evr::attacks::find(key.sequence);const bool stroke=press.starts(key,now);
  const bool button=!stroke&&specialAbilityOwned&&actionTrigger==specialAbilityTrigger&&listed&&listed->skill&&listed->weaponHits;
  if(!(stroke||button)||root.entity!=entity||triggerEntity!=entity||!rootCamera::firstPerson(root)||length<=0||length>60000000000ll||!h[0])return;
  uint32_t by=0;const auto kind=actionKind(before,registry,length,key.sequence,by);skill=kind==ActionKind::Skill;
  // The wand's swing: all of the first spawns within 100 ms (its normal attack spawns at 350 and 400 ms) come at once.
  const bool wandSwing=stroke&&press.spawnsOnly;
  size_t lasting=0,found=0;earlyCount=kind==ActionKind::LeftToGame?0:earlyCandidates(before,registry,length,early,earlyType,found,lasting,skill&&!wandSwing,wandSwing?100000000:1000000);++liveLookAheads;
  if(wandSwing&&earlyType!=spawnEntityEvent)earlyCount=0;   // only what a wand spawns, never an Impact
  bool same=h==read<std::array<uintptr_t,4>>(address)&&before==read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));
  for(size_t i=0;i<src.size();++i)same=same&&src[i]==read<uintptr_t>(h[0]+i*8);
  if(!same)throw std::runtime_error("live look ahead changed its sources");
  if(kind==ActionKind::LeftToGame){movingNoted(key.sequence,key.item,int64_t(key.trigger),by,"main");return;}
  if(!earlyCount){if(liveNotSingle.fetch_add(1)<50)logFile->event("live_event_not_retimed","\"reason\":\"impact_count\",\"impacts\":"+std::to_string(found)+",\"lasting\":"+std::to_string(lasting)+",\"sequence\":"+std::to_string(key.sequence)+",\"item\":"+std::to_string(key.item)+",\"trigger\":"+std::to_string(key.trigger));return;}
  authored=early[0].time;
  if(stroke&&!wandSwing&&controllerUiFixture::liveCooldownEnabled())controllerUiFixture::liveAttackBegan=press.serial;   // the press started an action with a stroke
  const auto weapon=attachmentObserver::meleeAppearance(root.entity,now,true);const auto ui=inputUi::snapshot();
  const bool qualified=(wandSwing||(weapon.valid&&weapon.item==key.item)||(bareHands&&bareHands(key.item,now)))&&inputUi::boundNormal(ui,ui.senderContext,entity,now);
  bool due=false,present=false;for(size_t i=0;i<earlyCount;++i){due=due||early[i].time<=sourceEnd;present=present||std::find(src.begin(),src.end(),early[i].variant)!=src.end();}
  if(due||present||liveLedger.consider(key,identity(early[0].variant),true,qualified,src.size()+earlyCount-1,h[2],now)!=Decision::Append){
   if(liveUnqualified.fetch_add(1)<50)logFile->event("live_event_not_retimed","\"reason\":"+std::string(!qualified?"\"not_the_shown_weapon_or_ui\"":due||present?"\"already_due\"":"\"ledger\"")+",\"sequence\":"+std::to_string(key.sequence)+",\"item\":"+std::to_string(key.item)+",\"trigger\":"+std::to_string(key.trigger));
   earlyCount=0;return;}
  const auto traits=stillTraits(before,registry,length);const bool cooldown=controllerUiFixture::liveCooldownEnabled();
  // The cooldown of the stroke that started this action is as long as the action takes to let the next attack begin.
  if(stroke&&!wandSwing&&cooldown&&traits.attackAt>0)controllerUiFixture::measureLiveCooldown(press.serial,uint64_t(traits.attackAt/1000000),key.sequence);
  attackAt=traits.attackAt;
  if(wandSwing){
   // The wand keeps the game's cooldown: no swing shoots again before this action lets the next attack begin (its first
   // EnableAbilityEvent with the ability to attack after the start, 600 ms for the normal attack) - the time from one shot to
   // the next is the game's own, the shots only come 350 ms sooner.
   wandAction.key=key;wandAction.until=now+uint64_t(length/1000000)+3000;++wandShotsEarly;
   controllerUiFixture::wandNextShotMs=now+uint64_t((traits.attackAt>0&&traits.attackAt<length?traits.attackAt:length)/1000000);
  }else if(skill){
   // The weapon hits live from this first tick until the next attack may begin (the action's first EnableAbilityEvent
   // with the ability to attack; else its end): a move-then-strike skill hits whenever the weapon touches, and no hit
   // of this action outlives it into the next one. The hits' own lengths grow by this factor (impact_lifetime.hpp).
   const int64_t end=traits.attackAt>0&&traits.attackAt<length?traits.attackAt:length;
   std::array<std::array<uint8_t,16>,evr::motion::maximumEarlyEvents> guids{};std::array<double,evr::motion::maximumEarlyEvents> factors{};
   for(size_t i=0;i<earlyCount;++i){guids[i]=identity(early[i].variant).guid;factors[i]=early[i].duration>0?std::max(1.0,double(end-sourceEnd)/double(early[i].duration)):1.0;}
   impactLifetime::own(key.sequence,guids.data(),factors.data(),earlyCount,now+uint64_t(length/1000000)+500);
   skillAction.key=key;skillAction.until=now+uint64_t(length/1000000)+3000;++skillsStarted;
   controllerUiFixture::skillBegan(now);
   if(skillsLogged.fetch_add(1)<60)logFile->event("live_skill_attack","\"sequence\":"+std::to_string(key.sequence)+",\"item\":"+std::to_string(key.item)+",\"length_ms\":"+std::to_string(length/1000000)+
    ",\"next_attack_at_ms\":"+std::to_string(traits.attackAt/1000000)+",\"weapon_hits\":"+std::to_string(earlyCount)+",\"authored_ms\":"+std::to_string(early[0].time/1000000)+",\"hit_length_ms\":"+std::to_string(early[0].duration/1000000)+
    ",\"lengthened_by\":"+std::to_string(factors[0])+",\"action_trigger\":"+std::to_string(actionTrigger)+",\"by_button\":"+(button?"true":"false")+",\"stick_forward\":"+(controllerUiFixture::skillFacing?"true":"false"));
  }else if(liveStill){
   dst.clear();for(auto p:src){if(movesCharacter(read<uint32_t>(p))){++stilled;continue;}dst.push_back(p);}
   stillAction.key=key;stillAction.until=now+uint64_t(length/1000000)+3000;stillAction.moveAt=std::max(early[0].duration,traits.payAt)+16666667;
   stillAction.attackAt=cooldown?traits.attackAt:0;stillAction.pressSerial=cooldown?press.serial:0;stillAction.walkAborts=traits.walkAborts;publishOwned();
  }
  for(size_t i=0;i<earlyCount;++i)dst.push_back(early[i].variant);
 }else return;
 if(dst==src)return;
 // As retime(): only the caller's stack-owned array and count change; Actor and the authored asset stay native.
 std::memcpy(reinterpret_cast<void*>(h[0]),dst.data(),dst.size()*8);const uint64_t count=dst.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
 const bool adding=earlyCount!=0;
 if(adding&&!bow){std::array<evr::motion::EventIdentity,evr::motion::maximumEarlyEvents> ids{};for(size_t i=0;i<earlyCount;++i)ids[i]=identity(early[i].variant);
  if(!liveLedger.commit(key,ids.data(),earlyCount,now,uint64_t(length/1000000)))throw std::runtime_error("live event duplicate commit");++liveAppended;}liveSuppressed+=impacts;liveStilled+=stilled;liveMovedEarly+=moved;liveAttacksEarly+=attacks;
 const auto after=read<std::array<uintptr_t,4>>(address);bool stable=before==read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor))&&after[0]==h[0]&&after[2]==h[2]&&after[3]==h[3]&&after[1]==dst.size();
 for(size_t i=0;i<dst.size();++i)stable=stable&&read<uintptr_t>(h[0]+i*8)==dst[i];
 std::ostringstream o;o<<"\"operation\":"<<jsonString(adding?"append":impacts?"suppress":"still")<<",\"movement_events_withheld\":"<<stilled<<",\"movement_abilities_early\":"<<moved<<",\"attack_abilities_early\":"<<attacks<<",\"next_attack_at\":"<<attackAt<<",\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"sequence\":"<<key.sequence<<",\"item\":"<<key.item<<",\"trigger\":"<<key.trigger
  <<",\"range\":["<<sourceStart<<','<<sourceEnd<<"],\"length\":"<<length<<",\"authored\":"<<authored<<",\"move_at\":"<<(adding?stillAction.moveAt:0)<<",\"walk_breaks_off\":"<<(adding&&stillAction.walkAborts?"true":"false")<<",\"skill\":"<<(skill?"true":"false")<<",\"bow\":"<<(bow?"true":"false")<<",\"early_events\":"<<earlyCount<<",\"early_type\":"<<(earlyType==spawnEntityEvent?"\"spawn_entity\"":"\"impact\"")<<",\"press_serial\":"<<press.serial<<",\"press_age_ms\":"<<(press.pressed&&now>=press.pressed?now-press.pressed:0)
  <<",\"events_before\":"<<src.size()<<",\"events_after\":"<<dst.size()<<",\"ledger\":"<<liveLedger.size(now)<<",\"milliseconds\":"<<now<<",\"actor_and_storage_unchanged\":"<<(stable?"true":"false")<<",\"asset_written\":false";
 logFile->event("live_event_retime",o.str());
 if(!stable)throw std::runtime_error("live event retime postcondition");
}
// The native selection has about forty call sites: each system (animation, turning, abilities, ...) selects the
// events of the running action for itself (game164357). The attack animation therefore kept playing while its
// SetAnimationEvent was withheld at callerRva only. With "still" the same events are withheld, and the movement
// abilities handed over when the early Impact is done, at every call site, for the local player's Actor and only for an action that a
// swing started. As everywhere here only the caller's own result array changes.
inline std::atomic<uintptr_t> stillActor{0},stillWorld{0};inline std::atomic<uint32_t> stillEntity{0};
inline std::atomic<unsigned> stillElsewhereCalls{0},stillElsewhereWithheld{0},stillElsewhereAbilities{0},stillElsewhereFailures{0};
inline bool stillImpactElsewhere=false; // tried 2026-10-11 21:25 (game212551): the white arc stayed; off, hits not verified with it
inline void stillElsewhere(void* output,const void* actor,void* registry,const void* filter){
 try{
  const auto before=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));if(!(before[0x1b1]&1))return;
  const auto world=stillWorld.load();const auto entity=stillEntity.load();const auto key=actionKey(before,world,entity);if(!key.valid())return;
  const auto now=GetTickCount64();int64_t sourceStart=0,sourceEnd=0,length=0;std::memcpy(&sourceStart,before.data()+0x1b8,8);std::memcpy(&sourceEnd,before.data()+0x1c0,8);std::memcpy(&length,before.data()+0x188,8);
  std::lock_guard<std::mutex> lock(retimeMutex);
  if(skillOwned(key,now)||wandOwned(key,now))return;   // a skill attack's motion stays the game's at every call site (the wand's swing too)
  if(!liveLedger.owns(key,now)&&!(stillAction.until>=now&&stillAction.key==key)){
   if(sourceStart!=0||!livePress)return;const auto press=livePress();const auto root=rootCamera::snapshot();uint32_t triggerEntity=0;std::memcpy(&triggerEntity,before.data()+16,4);
   if(!press.starts(key,now)||root.entity!=entity||triggerEntity!=entity||!rootCamera::firstPerson(root)||length<=0||length>60000000000ll)return;
   // Only an action of the shown weapon or tool (or of the bare hands) that has a stroke to hand over early, as in
   // liveEvents: other actions keep the game's motion.
   {std::array<evr::motion::EarlyEvent,evr::motion::maximumEarlyEvents> early{};uint32_t type=0;size_t impacts=0,lasting=0;
    const auto weapon=attachmentObserver::meleeAppearance(root.entity,now,true);
    if(!earlyCandidates(before,registry,length,early,type,impacts,lasting)||!((weapon.valid&&weapon.item==key.item)||(bareHands&&bareHands(key.item,now))))return;}
   {uint32_t by=0;if(movingAction(before,registry,length,key.sequence,by)){movingNoted(key.sequence,key.item,int64_t(key.trigger),by,"elsewhere");return;}}
   stillAction.key=key;stillAction.until=now+uint64_t(length/1000000)+3000;stillAction.moveAt=0;stillAction.attackAt=0;stillAction.pressSerial=0;stillAction.walkAborts=false;publishOwned();
  }
  ++stillElsewhereCalls;const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
  if(h[1]>256||h[1]>h[2]||(!h[0]&&h[1]))throw std::runtime_error("still event array bounds");
  std::vector<uintptr_t> src,dst;unsigned withheld=0,abilities=0;
  // The attack's authored Impact as well: the hit volume was handed over on the first tick by the main call site
  // (early), and what the other call sites make of the authored one is the swing's white arc (user, 2026-10-11:
  // "a white swing effect shows with the one-hand sword"; passed-event census game212139, SpawnImpact 216..366 ms).
  for(size_t i=0;i<h[1];++i){const auto p=read<uintptr_t>(h[0]+i*8);src.push_back(p);const auto type=read<uint32_t>(p);if(movesCharacter(type)||(stillImpactElsewhere&&type==0x64b5dc20)){++withheld;continue;}dst.push_back(p);}
  bool unfiltered=!filter;if(filter){const auto f=read<std::array<uint8_t,5>>(reinterpret_cast<uintptr_t>(filter));unfiltered=!f[4];}
  if(unfiltered&&h[0]&&movesNow(key,now,sourceStart,sourceEnd))abilities=earlyMovementAbilities(dst,before,registry,length,sourceEnd,h[2]);
  if(unfiltered&&h[0]&&attacksNow(key,now,sourceStart,sourceEnd))abilities+=earlyAbilities(dst,before,registry,length,sourceEnd,h[2],attackAbilities);
  if(dst==src)return;
  if(before!=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor))||h!=read<std::array<uintptr_t,4>>(address))throw std::runtime_error("still selection changed its sources");
  std::memcpy(reinterpret_cast<void*>(h[0]),dst.data(),dst.size()*8);const uint64_t count=dst.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
  stillElsewhereWithheld+=withheld;stillElsewhereAbilities+=abilities;
 }catch(const std::exception& e){if(stillElsewhereFailures.fetch_add(1)<3)logFile->event("live_still_failure","\"reason\":"+jsonString(e.what()));}
}
// The same attack plays a second time on the presentation side: another Actor instance of the local player (client
// world; Mock run game081443: same sequence, item and trigger time, at every call site, nothing withheld). Its
// VfxNotifierEvents are the white arc of a swing (user, 2026-10-11; the emitter appears at the root 0.2 s into the
// attack, run game081049). Only those are withheld there: animation and sound of that side stay as they were.
inline std::atomic<unsigned> replicaWithheld{0},replicaFailures{0},replicaAbilities{0};
// The owned attack on another Actor instance: the same sequence, item and triggering entity - and the trigger's time,
// which was asked to be the very same until 2026-10-13. For the first attack of a chain it is (both sides start it
// from one input tick); a later step is started by each side from its own tick, and the two can lie a tick or more
// apart. Then nothing was withheld on that side and the swing effect of the second strike showed - in some runs and
// not in others (Mock runs game-20261013-095147 with the effect, 095623 without; the headset run of 2026-10-13: "the
// swing effect sometimes shows"). The same sequence cannot begin twice within the slack: an attack lasts longer.
constexpr int64_t replicaTriggerSlack=150000000;   // ns
inline std::atomic<unsigned> replicaApartLines{0};
inline bool ownedElsewhere(uint32_t sequence,uint32_t item,int64_t trigger,uint32_t triggerEntity,int64_t* apart=nullptr){
 if(sequence!=ownedSequence.load()||item!=ownedItem.load()||triggerEntity!=stillEntity.load())return false;
 const int64_t difference=trigger-ownedTrigger.load();if(apart)*apart=difference;
 const bool same=difference>=-replicaTriggerSlack&&difference<=replicaTriggerSlack;
 if(difference){static std::atomic<uint64_t> told{0};const auto serial=ownedSerial.load();
  if(told.exchange(serial)!=serial&&replicaApartLines.fetch_add(1)<80)
   logFile->event("live_still_replica_trigger_apart","\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+",\"owned_serial\":"+std::to_string(serial)+
    ",\"trigger\":"+std::to_string(trigger)+",\"owned_trigger\":"+std::to_string(ownedTrigger.load())+",\"later_by_ms\":"+std::to_string(double(difference)/1e6)+",\"taken_for_the_same_attack\":"+(same?"true":"false"));}
 return same;
}
// A guest's own attack on its client (2026-10-22, user as a guest: "striking a tree or a thing walks me forward the way of the
// old model"; guest log 20261022-140736: live_event_retime 0 - a client has no World with its own Actor, so no action of
// its own is ever owned here). The action this guest's stroke began (the pending one below) is held as owned on this
// side instead: its movement and turn withheld at every call site, as the host withholds them on its server
// (remoteActions). EVR_GUEST_STILL=0: off.
inline const bool guestStillWanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_GUEST_STILL",v,sizeof v);return !(n==1&&v[0]=='0');}();
struct GuestOwned{uint32_t sequence=0,item=0,entity=0;int64_t trigger=0;uint64_t until=0;};inline GuestOwned guestOwned;inline std::mutex guestOwnedMutex;
inline std::atomic<unsigned> guestStilled{0},guestOwnedActions{0};
inline void stillReplica(void* output,const void* actor,void* registry,const void* filter){
 const auto now=GetTickCount64();const auto until=ownedUntil.load();const bool owning=until&&now<=until;
 if(!owning&&!livePress)return;
 try{
  const auto head=read<std::array<uint8_t,0x40>>(reinterpret_cast<uintptr_t>(actor));uint32_t sequence=0,item=0,triggerEntity=0;int64_t trigger=0;
  std::memcpy(&sequence,head.data(),4);std::memcpy(&trigger,head.data()+8,8);std::memcpy(&triggerEntity,head.data()+16,4);std::memcpy(&item,head.data()+0x38,4);
  // Not the owned attack - but perhaps the one a stroke has just asked for, begun on this side before the side that
  // owns attacks has told of it: this side starts it from the press at once, the other a few ticks later (Mock run
  // game-20261013-101131: this side was seen in the next attack while the one before was still the owned one). With
  // few frames a second the effect's time, 0.2 s into the attack, can come inside that gap. Until the attack is
  // owned, only the effect is withheld here; sound, factors and abilities wait for the owner's word.
  int64_t apart=0;bool pending=false,guestOwn=false;uint64_t pressSerial=0;
  if(!(owning&&ownedElsewhere(sequence,item,trigger,triggerEntity,&apart))){
   if(guestStillWanted){std::lock_guard<std::mutex> lock(guestOwnedMutex);
    guestOwn=guestOwned.until>=now&&guestOwned.sequence==sequence&&guestOwned.item==item&&guestOwned.entity==triggerEntity&&guestOwned.trigger==trigger;}
   if(!guestOwn){
   if(!livePress||!sequence||!(read<uint8_t>(reinterpret_cast<uintptr_t>(actor)+0x1b1)&1))return;
   const auto press=livePress();
   if(!(press.serial&&press.serial>ownedSerial.load()&&press.pressed&&now>=press.pressed&&now-press.pressed<=1000&&press.entity&&triggerEntity==press.entity&&item==press.item&&trigger!=press.previousTrigger))return;
   pending=true;pressSerial=press.serial;
   {const auto before=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));int64_t length=0;std::memcpy(&length,before.data()+0x188,8);uint32_t by=0;
    if(length>0&&length<=60000000000ll&&movingAction(before,registry,length,sequence,by)){movingNoted(sequence,item,trigger,by,"replica");return;}
    // a guest: no owner will come - this side holds the action (only a normal attack: movingAction returned above)
    if(guestStillWanted&&(liveTargets::guestWorld||nativeImpactComponents::clientNow(now))&&length>0&&length<=60000000000ll){
     {std::lock_guard<std::mutex> lock(guestOwnedMutex);guestOwned={sequence,item,triggerEntity,trigger,now+uint64_t(length/1000000)+1000};}guestOwn=true;
     if(guestOwnedActions.fetch_add(1)<200)logFile->event("guest_action_held","\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+",\"trigger\":"+std::to_string(trigger)+
      ",\"length_ms\":"+std::to_string(length/1000000)+",\"press_serial\":"+std::to_string(press.serial)+",\"press_age_ms\":"+std::to_string(now-press.pressed));}}
   }
  }
  const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
  if(h[1]>256||h[1]>h[2]||(!h[0]&&h[1]))throw std::runtime_error("replica event array bounds");
  std::vector<uintptr_t> dst;unsigned withheld=0;
  // The swing's sound too (SfxNotifierEvent): this side plays it, at its authored time, after the early hit.
  // And the factors that slow the character for the animation (LerpMovementFactorEvent, LerpRotationFactorEvent).
  for(size_t i=0;i<h[1];++i){const auto p=read<uintptr_t>(h[0]+i*8);const auto type=read<uint32_t>(p);
   if(type==vfxNotifierEvent||(guestOwn&&movesCharacter(type))||(!pending&&!(stillKeep&1)&&type==sfxNotifierEvent)||(!pending&&!(stillKeep&8)&&(type==lerpMovementFactorEvent||type==lerpRotationFactorEvent))){++withheld;if(guestOwn&&type!=vfxNotifierEvent)++guestStilled;continue;}dst.push_back(p);}
  // The abilities to walk, turn and jump, as soon as the early stroke is done: on this side too (see movesNow). A call
  // site can take them only on a tick that gave it an array, so they are offered for 0.4 s from that time on; enabling
  // twice changes nothing.
  // The abilities to attack again, when the cooldown's 80 % are near (attackEarlyAt): on this side as well.
  unsigned abilities=0;const int64_t moveAt=ownedMoveAt.load(),attackAt=ownedAttackAt.load();
  bool unfiltered=!filter;if(filter){const auto f=read<std::array<uint8_t,5>>(reinterpret_cast<uintptr_t>(filter));unfiltered=!f[4];}
  if(!pending&&(moveAt>0||attackAt>0)&&unfiltered&&h[0]&&!(stillKeep&16)){
   const auto before=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));int64_t start=0,end=0,length=0;
   std::memcpy(&start,before.data()+0x1b8,8);std::memcpy(&end,before.data()+0x1c0,8);std::memcpy(&length,before.data()+0x188,8);
   const bool playing=(before[0x1b1]&1)&&length>0&&length<=60000000000ll;
   if(playing&&moveAt>0&&end>=moveAt&&start<moveAt+400000000)abilities=earlyMovementAbilities(dst,before,registry,length,end,h[2]);
   if(playing&&attackAt>0&&end>=attackEarlyAt(attackAt)&&start<attackAt)abilities+=earlyAbilities(dst,before,registry,length,end,h[2],attackAbilities);
  }
  if(!withheld&&!abilities)return;
  if(h!=read<std::array<uintptr_t,4>>(address))throw std::runtime_error("replica selection changed its sources");
  std::memcpy(reinterpret_cast<void*>(h[0]),dst.data(),dst.size()*8);const uint64_t count=dst.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
  const auto abilitiesBefore=replicaAbilities.fetch_add(abilities);
  // The first of each owned attack is always on record (which attacks were reached on this side).
  static std::atomic<uint64_t> withheldSerial{0},pendingSerial{0};
  const bool firstOfAttack=withheld&&(pending?pendingSerial.exchange(pressSerial)!=pressSerial:withheldSerial.exchange(ownedSerial.load())!=ownedSerial.load());
  if(pending){if(firstOfAttack)logFile->event("live_still_replica_vfx_before_owned","\"actor\":"+hex(reinterpret_cast<uintptr_t>(actor))+",\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+",\"trigger\":"+std::to_string(trigger)+
    ",\"press_serial\":"+std::to_string(pressSerial)+",\"owned_serial\":"+std::to_string(ownedSerial.load())+",\"vfx_notifiers_withheld\":"+std::to_string(withheld)+",\"events_after\":"+std::to_string(dst.size())+",\"only_the_effect\":true,\"asset_written\":false");}
  else if(replicaWithheld.fetch_add(withheld)<24||(abilities&&abilitiesBefore<24)||firstOfAttack)logFile->event("live_still_replica_vfx","\"actor\":"+hex(reinterpret_cast<uintptr_t>(actor))+",\"owned_actor\":"+hex(stillActor.load())+",\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+",\"movement_abilities_early\":"+std::to_string(abilities)+
   ",\"trigger\":"+std::to_string(trigger)+",\"trigger_later_by_ms\":"+std::to_string(double(apart)/1e6)+",\"owned_serial\":"+std::to_string(ownedSerial.load())+",\"vfx_notifiers_withheld\":"+std::to_string(withheld)+",\"swing_sounds_included\":"+((stillKeep&1)?"false":"true")+",\"events_after\":"+std::to_string(dst.size())+",\"asset_written\":false");
 }catch(const std::exception& e){if(replicaFailures.fetch_add(1)<3)logFile->event("live_still_failure","\"reason\":"+jsonString(e.what()));}
}
// Other players' strokes on this host (2026-10-22, user as a VR guest on the friend's host: "attacks still lag", "striking a
// tree or a thing walks me forward the way of the old model - the animation is still used"). A VR guest's hit is decided on
// the guest's PC (online_sync.hpp hitAction) and laid here on that player's attack collider (sequence_collider_observer.hpp
// remoteMelee) - which the server made only at the attack's authored time: the hits waited 250 ms (median of 39, host log
// 20261022-140712 remote_melee_confirmed hit_age_ms 203..687). And the attack's animation moved and turned the guest's
// character on the server as a desktop player's. For the characters of players whose mod sends its body (remoteMelee::
// peerEntities, within 5 s), a normal attack with an Impact (the attack table: no skill; no moving events) is handled as
// the local player's: its Impacts on the first tick (the authored ones suppressed when they come; liveLedger, as
// liveEvents) and its movement and turn withheld at every call site (movesCharacter). Skills keep the game's. The guest's
// client holds the same action likewise (stillReplica, guestOwned). A swing of the wand (user: "the magic's shot lags"):
// its shots, 350 and 400 ms into the wand's normal attack, on the first tick (as liveEvents' wandSwing), its motion the
// game's. EVR_REMOTE_STILL=0: off.
namespace remoteActions {
inline const bool wanted=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_STILL",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<uintptr_t> serverWorld{0};   // the World the local player's Actor ticks in (select)
struct Owned{evr::motion::EventActionKey key;uint64_t until=0;bool still=true;};inline std::array<Owned,8> owned{};   // guarded by retimeMutex
inline std::atomic<unsigned> started{0},withheld{0},earlyImpacts{0},suppressed{0},failures{0},logs{0};
// The characters of the players whose mod sends its body, refreshed every 250 ms (select is called for every Actor at about
// forty call sites: the filter below reads the game's own Actor directly and takes no lock).
inline std::array<std::atomic<uint32_t>,8> ids{};inline std::atomic<uint64_t> idsMs{0};
// EVR_REMOTE_STILL_SELF=1 (a test for one player, without -LiveMeleeEarly/-LiveMeleeStill): the own character as another player's.
inline const bool self=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_STILL_SELF",v,sizeof v);return n==1&&v[0]=='1';}();
inline bool remotePlayer(uint32_t entity,uint64_t now){
 if(self){rootCamera::Sample r;return rootCamera::trySnapshot(r)&&r.entity==entity;}
 if(now-idsMs.load()>250){idsMs=now;std::array<uint32_t,8> fresh{};size_t n=0;
  {std::lock_guard<std::mutex> lock(remoteMelee::mutex);for(const auto& [peer,e]:remoteMelee::peerEntities)if(n<fresh.size()&&e.first&&now>=e.second&&now-e.second<=5000)fresh[n++]=e.first;}
  for(size_t i=0;i<ids.size();++i)ids[i]=fresh[i];}
 for(const auto& id:ids)if(id.load()==entity)return true;
 return false;
}
inline void apply(void* output,const void* actor,void* registry,bool mainSite) noexcept {
 try{
  const auto a=reinterpret_cast<uintptr_t>(actor);if(!a)return;
  // the game's own Actor of this call (BaseActor, 3104 bytes): playing an action (+0x1b1 bit 0), the actor's entity (+0x10)
  if(!(*reinterpret_cast<const volatile uint8_t*>(a+0x1b1)&1))return;
  const uint32_t entity=*reinterpret_cast<const volatile uint32_t*>(a+16);const auto now=GetTickCount64();
  if(!entity||!remotePlayer(entity,now))return;
  const auto world=serverWorld.load();if(!world)return;
  const auto before=read<std::array<uint8_t,3104>>(a);const auto key=actionKey(before,world,entity);if(!key.valid())return;
  int64_t sourceStart=0,sourceEnd=0,length=0;std::memcpy(&sourceStart,before.data()+0x1b8,8);std::memcpy(&sourceEnd,before.data()+0x1c0,8);std::memcpy(&length,before.data()+0x188,8);
  const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
  if(h[1]>256||h[1]>h[2]||(!h[0]&&h[1]))throw std::runtime_error("remote event array bounds");
  std::vector<uintptr_t> src;for(size_t i=0;i<h[1];++i)src.push_back(read<uintptr_t>(h[0]+i*8));
  std::vector<uintptr_t> dst;unsigned stilled=0,dropped=0;size_t earlyCount=0;
  std::array<evr::motion::EarlyEvent,evr::motion::maximumEarlyEvents> ev{};
  std::lock_guard<std::mutex> lock(retimeMutex);
  Owned* own=nullptr;for(auto& o:owned)if(o.until>=now&&o.key==key){own=&o;break;}
  if(!own){
   if(!mainSite||sourceStart!=0||length<=0||length>60000000000ll||!h[0])return;
   uint32_t by=0;if(actionKind(before,registry,length,key.sequence,by)!=ActionKind::Normal)return;
   const bool wand=motion::rangedAimWand(key.item);
   uint32_t type=spawnImpactEvent;size_t found=0,lasting=0;
   earlyCount=earlyCandidates(before,registry,length,ev,type,found,lasting,false,wand?100000000:1000000);
   if(!earlyCount||type!=(wand?spawnEntityEvent:spawnImpactEvent))return;   // an action without a hit (a tool's use, an emote): the game's
   bool due=false,present=false;for(size_t i=0;i<earlyCount;++i){due=due||ev[i].time<=sourceEnd;present=present||std::find(src.begin(),src.end(),ev[i].variant)!=src.end();}
   if(due||present)return;
   for(auto p:src){if(!wand&&movesCharacter(read<uint32_t>(p))){++stilled;continue;}dst.push_back(p);}
   for(size_t i=0;i<earlyCount;++i)dst.push_back(ev[i].variant);
   if(dst.size()>h[2]||liveLedger.consider(key,identity(ev[0].variant),true,true,src.size()+earlyCount-1,h[2],now)!=evr::motion::LiveEventLedger<32>::Decision::Append)return;
   Owned* slot=&owned[0];for(auto& o:owned)if(o.until<slot->until)slot=&o;*slot={key,now+uint64_t(length/1000000)+3000,!wand};
   if(!wand){   // the hits live until the next attack may begin (as a skill's; impact_lifetime.hpp ownRemote)
    const auto traits=stillTraits(before,registry,length);const int64_t end=traits.attackAt>0&&traits.attackAt<length?traits.attackAt:length;
    std::array<std::array<uint8_t,16>,evr::motion::maximumEarlyEvents> guids{};std::array<double,evr::motion::maximumEarlyEvents> factors{};
    for(size_t i=0;i<earlyCount;++i){guids[i]=identity(ev[i].variant).guid;factors[i]=ev[i].duration>0?std::max(1.0,double(end-sourceEnd)/double(ev[i].duration)):1.0;}
    impactLifetime::ownRemote(key.sequence,guids.data(),factors.data(),earlyCount,now+uint64_t(length/1000000)+500);}
  }else{
   for(auto p:src){const auto type=read<uint32_t>(p);
    if(mainSite&&(type==spawnImpactEvent||type==spawnEntityEvent)&&liveLedger.consider(key,identity(p),false,false,src.size(),h[2],now)==evr::motion::LiveEventLedger<32>::Decision::Suppress){++dropped;continue;}
    if(own->still&&movesCharacter(type)){++stilled;continue;}dst.push_back(p);}
  }
  if(dst==src)return;
  if(before!=read<std::array<uint8_t,3104>>(a)||h!=read<std::array<uintptr_t,4>>(address))throw std::runtime_error("remote selection changed its sources");
  std::memcpy(reinterpret_cast<void*>(h[0]),dst.data(),dst.size()*8);const uint64_t count=dst.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
  withheld+=stilled;suppressed+=dropped;
  if(earlyCount){
   std::array<evr::motion::EventIdentity,evr::motion::maximumEarlyEvents> idsOf{};for(size_t i=0;i<earlyCount;++i)idsOf[i]=identity(ev[i].variant);
   const bool committed=liveLedger.commit(key,idsOf.data(),earlyCount,now,uint64_t(length/1000000));
   ++started;earlyImpacts+=unsigned(earlyCount);
   if(logs.fetch_add(1)<300)logFile->event("remote_action_held","\"entity\":"+std::to_string(entity)+",\"sequence\":"+std::to_string(key.sequence)+",\"item\":"+std::to_string(key.item)+
    ",\"length_ms\":"+std::to_string(length/1000000)+",\"wand\":"+(motion::rangedAimWand(key.item)?"true":"false")+",\"early_events\":"+std::to_string(earlyCount)+",\"authored_ms\":"+std::to_string(ev[0].time/1000000)+",\"movement_events_withheld\":"+std::to_string(stilled)+
    ",\"committed\":"+(committed?"true":"false")+",\"events_before\":"+std::to_string(src.size())+",\"events_after\":"+std::to_string(dst.size()));
  }
 }catch(const std::exception& e){if(failures.fetch_add(1)<5)try{logFile->event("remote_action_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
 catch(...){failures.fetch_add(1);}
}
inline void summary(){
 if(!started.load()&&!withheld.load()&&!failures.load()&&!guestOwnedActions.load())return;
 logFile->event("remote_actions_summary","\"held\":"+std::to_string(started.load())+",\"early_impacts\":"+std::to_string(earlyImpacts.load())+",\"authored_suppressed\":"+std::to_string(suppressed.load())+
  ",\"movement_events_withheld\":"+std::to_string(withheld.load())+",\"failures\":"+std::to_string(failures.load())+",\"guest_actions_held\":"+std::to_string(guestOwnedActions.load())+",\"guest_movement_withheld\":"+std::to_string(guestStilled.load()));
}
}
// "A walk breaks the action off" (Actor+1b1 bit 4), put back once the early stroke is done and the tool's use is
// paid for, for an owned action that was made not to be broken off by a walk (StillTraits, see movesNow). On every Actor instance that plays it: the byte is the instance's own,
// and this is the thread that works on that instance. One bit of one byte, only while bit 0 (playing) is set; the
// next action's start sets the byte's bits itself.
inline std::atomic<unsigned> walkAbortWrites{0};
inline void walkAbort(const void* actor){
 const auto until=ownedUntil.load();const int64_t moveAt=ownedMoveAt.load();
 if(!until||moveAt<=0||!ownedWalkAborts.load()||GetTickCount64()>until)return;
 try{
  const auto a=reinterpret_cast<uintptr_t>(actor);const auto head=read<std::array<uint8_t,0x40>>(a);uint32_t sequence=0,item=0,triggerEntity=0;int64_t trigger=0;
  std::memcpy(&sequence,head.data(),4);std::memcpy(&trigger,head.data()+8,8);std::memcpy(&triggerEntity,head.data()+16,4);std::memcpy(&item,head.data()+0x38,4);
  if(!ownedElsewhere(sequence,item,trigger,triggerEntity))return;
  const auto flags=read<uint8_t>(a+0x1b1);const auto end=read<int64_t>(a+0x1c0);
  if(!(flags&1)||(flags&0x10)||end<moveAt)return;
  *reinterpret_cast<uint8_t*>(a+0x1b1)=uint8_t(flags|0x10);
  if(walkAbortWrites.fetch_add(1)<12)logFile->event("live_still_walk_abort","\"actor\":"+hex(a)+",\"owned_actor\":"+(a==stillActor.load()?"true":"false")+",\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+
   ",\"action_ms\":"+std::to_string(end/1000000)+",\"flags_before\":"+std::to_string(flags)+",\"flags_after\":"+std::to_string(flags|0x10)+",\"one_bit_of_the_actor_written\":true");
 }catch(...){}
}
// The next attack of a chain. An action tells with SetActionOportunity (f6c3e0b8) what may follow it: the handler
// (223054) keeps each in one of the Actor's six slots (Actor+1d8, 1a8 bytes each: the kind of request at +18 - 1 the
// attack, 4 the heavy one -, the event's guid at +174, and at +188 the time of the action at which the next one
// starts; the one-hand sword's first attack: 550 ms, or 450 ms by a condition of the character) and sets the
// request's ability at once. A request made in time waits in the slot until that time. Mock run game205954: with
// the cooldown's wait over at 0.44 s the stroke pressed, and the game began the chain's next attack at its own time.
// So that the stroke attacks when it is made, the slot's time is brought forward by the same rule as the abilities
// (attackEarlyAt: 80 % of the game's time, less a tenth of a second for the way the press takes), once per slot and
// action, on every Actor instance that plays the owned action. The game's own time is what the cooldown is as long
// as (the first slot of an attack that is found tells controller_ui_fixture.hpp); an action without such a slot
// keeps the time of its EnableAbilityEvent (stillTraits).
constexpr uintptr_t opportunitySlots=0x1d8,opportunitySize=0x1a8;constexpr unsigned opportunityCount=6;
// The jump attacks (2026-10-15, user: "in VR both: the ordinary stroke in the air attacks, the heavy stroke in the air
// is the jump attack"). The game's jump actions (Ability_Movement_JumpSingle, _Skill_JumpDoubleFirst/Second) carry
// SetActionOportunity events: at 0 ms, Primary with the "slash up" skill -> Attack_Player_Melee_Special_JumpAttack_
// SlashUp_<weapon>, and at 216.7 ms, Primary with the "crash down" skill (override) -> ..._CrashDown_<weapon> (the fall
// with the shock wave). The handler (223054) keeps the one of the held weapon in a slot (kind 1, the skill's id at
// +190); the Primary press then takes that slot (1d9e30: the first slot of its kind, marked at +1a4) instead of the
// ordinary attack - which is why the skill takes the ordinary attack in the air away. For the ordinary stroke in the
// air, the skilled Primary slots of the local player's Actor instances are emptied (kind 0 - what the game's own
// ClearActionOportunity does), so that the press goes its ordinary way (275110: the Primary row of the item); the
// heavy stroke in the air is sent as a short press, which the slot takes. The slots are filled again by the next
// jump's events. Nothing else of the Actor is written.
inline std::mutex recentMutex;struct RecentActor {uintptr_t actor=0;uint64_t seen=0;};inline std::array<RecentActor,8> recentActors{};
inline void noteActor(const void* actor){
 const auto a=reinterpret_cast<uintptr_t>(actor);const auto now=GetTickCount64();std::lock_guard<std::mutex> lock(recentMutex);
 size_t slot=0;for(size_t i=0;i<recentActors.size();++i){if(recentActors[i].actor==a){slot=i;break;}if(recentActors[i].seen<recentActors[slot].seen)slot=i;}
 recentActors[slot]={a,now};
}
inline std::atomic<unsigned> airClears{0};
inline unsigned clearSkilledPrimaryOpportunities(){
 std::array<RecentActor,8> actors{};{std::lock_guard<std::mutex> lock(recentMutex);actors=recentActors;}
 const auto now=GetTickCount64();unsigned slots=0,instances=0;std::ostringstream rows;
 for(const auto& r:actors){
  if(!r.actor||now-r.seen>2000)continue;
  try{
   const auto a=r.actor;const auto flags=read<uint8_t>(a+0x1b1);if(!(flags&1))continue;
   const auto sequence=read<uint32_t>(a);unsigned here=0;
   for(unsigned i=0;i<opportunityCount;++i){
    const auto slot=a+opportunitySlots+i*opportunitySize;
    if(read<uint16_t>(slot+0x18)!=1||!read<uint32_t>(slot+0x190)||read<uint8_t>(slot+0x1a4))continue;
    *reinterpret_cast<uint16_t*>(slot+0x18)=0;++here;
   }
   if(here){++instances;slots+=here;rows<<(rows.tellp()>0?",":"")<<"{\"actor\":"<<hex(a)<<",\"sequence\":"<<sequence<<",\"slots\":"<<here<<"}";}
  }catch(...){}
 }
 if(airClears.fetch_add(1)<40)try{logFile->event("live_air_opportunities_cleared","\"instances\":"+std::to_string(instances)+",\"slots\":"+std::to_string(slots)+",\"rows\":["+rows.str()+"],\"kind_of_the_slot_written\":true");}catch(...){}
 return slots;
}
// The two airborne attacks by the kind of stroke (2026-10-15 afternoon, user: "the new save has the double jump and both
// airborne attack skills"; headset run 11: every airborne stroke gave the rising slash, the crash-down never came, since
// the game takes the FIRST Primary slot and the jump registers the slash-up (0 ms) before the crash-down (216.7 ms)).
// The stroke's direction chooses (user 2026-10-15 afternoon: "swing up in the air for the rising attack, down for the
// crash-down"). The game's own rule (1d9e30): the FIRST Primary slot is taken - the rising slash, registered at the
// jump's start, while its conditions hold (rising); the crash-down registered at 216.7 ms does not replace it (Mock
// game-20261015-131620: a heavy stroke 0.46 s into the jump still gave the slash-up), so the crash-down comes only once
// the slash-up's conditions fail (falling). Here the unused Primary slots that hold a jump attack get the SEQUENCE of
// the wanted kind written (+0x170, the follow-up the take copies to +0; the slot's skill, guid and conditions stay the
// slot's own): kind +1 (the hand going up) the rising slash, kind -1 (going down, or a heavy stroke) the crash-down with
// its shockwave, kind 0 (level) nothing. The sequence of the wanted kind is the weapon group's (the slot's own sequence
// names the group; a one-handed group's crash-down is the sword's for a sword, else the mace's). A slot the player has
// no skill for was never filled, and the press then goes its ordinary way. docs/analysis/airborne_skills_20261015.md.
// The slots of every recent Actor instance are put on record the first times (live_air_opportunities_shaped).
constexpr std::array<uint32_t,4> slashUpSequences{2251110514u,1990608986u,1548769672u,1253034539u};   // Sword_new, Daggers_new, GreatSword, Hammer
constexpr std::array<uint32_t,5> crashDownSequences{3198880426u,1061916186u,2045364040u,2614964733u,755063106u};   // Sword, Mace, Daggers, Greatsword, Hammer
inline std::atomic<unsigned> airShapes{0};
// The Actor whose slots the game's take (1d9e30) searched last, kept by opportunity_take_observer.hpp.
namespace opportunityHolder {inline std::atomic<uintptr_t> actor{0};inline std::atomic<uint64_t> seen{0};}
inline uint32_t wantedJumpAttack(uint32_t current,int kind,uint8_t weaponType){
 // the group of the slot's sequence -> the wanted kind's sequence of that group; 0: not a jump attack
 const bool oneHand=current==2251110514u||current==3198880426u||current==1061916186u,daggers=current==1990608986u||current==2045364040u;
 const bool greatSword=current==1548769672u||current==2614964733u,hammer=current==1253034539u||current==755063106u;
 if(kind>0)return oneHand?2251110514u:daggers?1990608986u:greatSword?1548769672u:hammer?1253034539u:0u;
 if(kind<0)return oneHand?(weaponType==2?3198880426u:1061916186u):daggers?2045364040u:greatSword?2614964733u:hammer?755063106u:0u;
 return 0u;
}
// 1.0 (2026-10-17): the holder of the slots, found by its components. For the local player's entity in the camera's world
// and every world the actions were seen in, each component whose type name has "Actor" and is large enough for the slots (+0x1d8, six of
// 0x1a8) is looked at; the one with an unused Primary slot holding a jump attack is the holder. Only at an airborne
// stroke's press. The names are read from the world's type registry once per world (live_air_holder_search, first 20).
struct ActorComponent {unsigned id=0,bytes=0;std::string name;};
inline std::map<uintptr_t,std::vector<ActorComponent>> actorComponents;inline std::atomic<unsigned> holderSearches{0};
// Where the slots went when no Actor holds them (2026-10-17, 1.0: both Actor components of the local player were idle
// through the jump, Mock game-20261017-093355): every component of the player's entity in the searched worlds is read
// whole and the jump attacks' numbers looked for as 32-bit values; each hit is logged with the component's name and the
// offset (live_air_slot_value_search, first 6 searches). Read-only. Mock game-20261017-093735: 428 components, no hit -
// in 1.0 the airborne attacks come from the item's rows (controller_ui_fixture.hpp, airDownHeavy), not from slots. Opt-in
// since: EVR_AIR_SLOT_SEARCH=1.
inline std::atomic<unsigned> valueSearches{0};
inline const bool slotValueSearch=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_AIR_SLOT_SEARCH",v,sizeof v);return n==1&&v[0]=='1';}();
inline void searchSlotValues(const std::vector<std::pair<uintptr_t,uint32_t>>& keys){
 if(!slotValueSearch||valueSearches.fetch_add(1)>=6)return;
 std::ostringstream hits;unsigned components=0;
 for(const auto& [world,entity]:keys){
  try{
   const auto e=nativeImpactComponents::entityAtWorld(world,entity);
   const auto bits=read<std::array<uint64_t,10>>(e.layout);
   for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){
    if(!(bits[i/64]&(1ull<<(i%64))))continue;
    unsigned bytes=0;std::string name;
    try{const auto t=nativeImpactComponents::componentType(world,i);bytes=t.bytes;name=t.qualifiedName;}catch(...){continue;}
    if(!bytes||bytes>65536)continue;
    const auto a=nativeImpactComponents::component(e,i,bytes);if(!a)continue;++components;
    for(size_t o=0;o+4<=bytes;o+=4){uint32_t v=0;try{v=read<uint32_t>(a+o);}catch(...){break;}
     bool jump=false;for(auto s:slashUpSequences)jump|=s==v;for(auto s:crashDownSequences)jump|=s==v;
     if(jump)hits<<(hits.tellp()>0?",":"")<<"{\"world\":"<<hex(world)<<",\"component\":"<<jsonString(name)<<",\"bytes\":"<<bytes<<",\"offset\":"<<o<<",\"value\":"<<v<<"}";}
   }
  }catch(...){}
 }
 try{logFile->event("live_air_slot_value_search","\"components\":"+std::to_string(components)+",\"hits\":["+hits.str()+"]");}catch(...){}
}
inline uintptr_t findOpportunityHolder(){
 std::vector<std::pair<uintptr_t,uint32_t>> keys;
 {std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);for(const auto& q:actionSequenceObserver::qualified)keys.push_back(q.first);
  if(actionSequenceObserver::latest.world)keys.push_back({actionSequenceObserver::latest.world,actionSequenceObserver::latest.entity});}
 // The camera's world: the client's, where the local player's own actions play (Mock game-20261017-093007: the server
 // world's Actor of entity 1 was idle through the jump while the jump attack came).
 {const auto root=rootCamera::snapshot();if(root.world&&root.entity)keys.insert(keys.begin(),{root.world,root.entity});}
 std::ostringstream log;uintptr_t found=0;
 for(const auto& [world,entity]:keys){
  if(!world||!entity||found)continue;
  try{
   auto& list=actorComponents[world];
   if(list.empty()&&actorComponents.size()<16){
    for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){
     try{const auto t=nativeImpactComponents::componentType(world,i);
      if(t.qualifiedName.find("Actor")!=std::string::npos&&t.bytes>=opportunitySlots+opportunityCount*opportunitySize)list.push_back({i,t.bytes,t.qualifiedName});}catch(const std::exception&){break;}
    }
    if(list.empty())list.push_back({UINT_MAX,0,""});
   }
   const auto e=nativeImpactComponents::entityAtWorld(world,entity);
   for(const auto& c:list){
    if(c.id==UINT_MAX)continue;
    const auto a=nativeImpactComponents::component(e,c.id,c.bytes);if(!a)continue;
    unsigned jump=0;
    for(unsigned i=0;i<opportunityCount;++i){const auto slot=a+opportunitySlots+i*opportunitySize;const auto sequence=read<uint32_t>(slot+0x170);
     bool known=false;for(auto s:slashUpSequences)known|=s==sequence;for(auto s:crashDownSequences)known|=s==sequence;
     if(known&&read<uint16_t>(slot+0x18)==1&&!read<uint8_t>(slot+0x1a4))++jump;}
    log<<(log.tellp()>0?",":"")<<"{\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"component\":"<<jsonString(c.name)<<",\"bytes\":"<<c.bytes<<",\"address\":"<<hex(a)<<",\"flags\":"<<unsigned(read<uint8_t>(a+0x1b1))<<",\"sequence\":"<<read<uint32_t>(a)<<",\"jump_slots\":"<<jump<<"}";
    if(jump&&!found)found=a;
   }
  }catch(...){}
 }
 if(!found)searchSlotValues(keys);
 if(holderSearches.fetch_add(1)<20)try{logFile->event("live_air_holder_search","\"found\":"+hex(found)+",\"candidates\":["+log.str()+"]");}catch(...){}
 return found;
}
inline unsigned shapeAirborneOpportunities(int kind,uint8_t weaponType){
 // The holder the game's own take searched last (opportunity_take_observer.hpp) first, then the player's base Actor
 // (input_actor_observer.hpp) and the recent instances (Mock game-20261015-132051: those carried no slots).
 std::array<RecentActor,10> actors{};{std::lock_guard<std::mutex> lock(recentMutex);for(size_t i=0;i<recentActors.size();++i)actors[i+2]=recentActors[i];}
 const auto now=GetTickCount64();unsigned cleared=0,scanned=0,kept=0;std::ostringstream rows;
 // 1.0: the holder found by its components first (the take's observer finds none in 1.0).
 {uintptr_t holder=0;try{holder=findOpportunityHolder();}catch(...){}
  actors[0]={holder?holder:(now-opportunityHolder::seen.load()<=600000?opportunityHolder::actor.load():0),now};}
 actors[1]={controllerUiFixture::playerBaseActor.load(),now};
 for(size_t ai=0;ai<actors.size();++ai){const auto& r=actors[ai];
  bool twice=false;for(size_t j=0;j<ai;++j)if(actors[j].actor==r.actor)twice=true;if(twice)continue;
  if(!r.actor||now-r.seen>2000)continue;
  try{
   const auto a=r.actor;const auto flags=read<uint8_t>(a+0x1b1);++scanned;
   rows<<(rows.tellp()>0?",":"")<<"{\"actor\":"<<hex(a)<<",\"flags\":"<<unsigned(flags)<<",\"sequence\":"<<read<uint32_t>(a)<<",\"slots\":[";
   for(unsigned i=0;i<opportunityCount;++i){
    const auto slot=a+opportunitySlots+i*opportunitySize;
    const uint16_t trigger=read<uint16_t>(slot+0x18);const uint8_t used=read<uint8_t>(slot+0x1a4);const uint32_t sequence=read<uint32_t>(slot+0x170),skill=read<uint32_t>(slot+0x190);
    bool slashUp=false,crashDown=false;for(auto s:slashUpSequences)if(s==sequence)slashUp=true;for(auto s:crashDownSequences)if(s==sequence)crashDown=true;
    const auto wanted=(flags&1)&&trigger==1&&!used&&(slashUp||crashDown)?wantedJumpAttack(sequence,kind,weaponType):0u;
    const bool other=wanted&&wanted!=sequence;
    if(other){*reinterpret_cast<uint32_t*>(slot+0x170)=wanted;*reinterpret_cast<uint32_t*>(slot)=wanted;++cleared;}else if(wanted)++kept;
    rows<<(i?",":"")<<"{\"trigger\":"<<trigger<<",\"sequence\":"<<sequence<<",\"skill\":"<<skill<<",\"used\":"<<unsigned(used)<<(other?",\"written\":"+std::to_string(wanted):"")<<"}";
   }
   rows<<"]}";
  }catch(...){}
 }
 if(airShapes.fetch_add(1)<40)try{logFile->event("live_air_opportunities_shaped","\"kind\":"+std::to_string(kind)+",\"weapon_type\":"+std::to_string(weaponType)+",\"actors\":"+std::to_string(scanned)+",\"cleared\":"+std::to_string(cleared)+",\"kept\":"+std::to_string(kept)+",\"rows\":["+rows.str()+"]");}catch(...){}
 return cleared;
}
struct ChainWrite {uintptr_t actor=0;int64_t trigger=0;std::array<int64_t,opportunityCount> written{};};
inline std::array<ChainWrite,4> chainWrites;inline std::mutex chainMutex;inline std::atomic<unsigned> chainWritten{0};
inline void chainEarly(const void* actor){
 const auto until=ownedUntil.load();const auto serial=ownedSerial.load();
 if(!until||!serial||GetTickCount64()>until)return;
 try{
  const auto a=reinterpret_cast<uintptr_t>(actor);const auto head=read<std::array<uint8_t,0x40>>(a);uint32_t sequence=0,item=0,triggerEntity=0;int64_t trigger=0;
  std::memcpy(&sequence,head.data(),4);std::memcpy(&trigger,head.data()+8,8);std::memcpy(&triggerEntity,head.data()+16,4);std::memcpy(&item,head.data()+0x38,4);
  if(!ownedElsewhere(sequence,item,trigger,triggerEntity))return;
  const auto flags=read<uint8_t>(a+0x1b1);const auto length=read<int64_t>(a+0x188);
  if(!(flags&1)||length<=0||length>60000000000ll)return;
  std::lock_guard<std::mutex> lock(chainMutex);
  ChainWrite* mine=nullptr;for(auto& w:chainWrites)if(w.actor==a){mine=&w;break;}
  if(!mine){for(auto& w:chainWrites)if(!w.actor){mine=&w;break;}if(!mine)mine=&chainWrites[0];*mine={};mine->actor=a;}
  if(mine->trigger!=trigger){mine->trigger=trigger;mine->written={};}
  bool told=false;
  for(unsigned i=0;i<opportunityCount;++i){
   const auto slot=a+opportunitySlots+i*opportunitySize;const auto kind=read<uint16_t>(slot+0x18);const auto time=read<int64_t>(slot+0x188);
   if(kind!=1&&kind!=4)continue;
   if(kind==1&&time==mine->written[i])told=true;            // this action's attack slot is already brought forward
   if(time<=0||time>length||time==mine->written[i])continue;
   const auto guid=read<std::array<uint64_t,2>>(slot+0x174);if(!guid[0]&&!guid[1])continue;
   const int64_t early=attackEarlyAt(time);if(early<=0||early>=time)continue;
   *reinterpret_cast<int64_t*>(slot+0x188)=early;mine->written[i]=early;
   if(kind==1&&!told){told=true;controllerUiFixture::measureLiveCooldown(serial,uint64_t(time/1000000),sequence);}
   if(chainWritten.fetch_add(1)<36)logFile->event("live_still_chain_early","\"actor\":"+hex(a)+",\"owned_actor\":"+(a==stillActor.load()?"true":"false")+",\"sequence\":"+std::to_string(sequence)+",\"slot\":"+std::to_string(i)+",\"kind\":"+std::to_string(kind)+
    ",\"game_ms\":"+std::to_string(time/1000000)+",\"written_ms\":"+std::to_string(early/1000000)+",\"action_ms\":"+std::to_string(read<int64_t>(a+0x1c0)/1000000)+",\"one_time_of_the_actor_written\":true");
  }
 }catch(...){}
}
// The chain's request stays open to the end of the action. Some attacks close it before they end: the one-hand
// sword's first attack is 1667 ms long, takes the request for the next attack from 550 ms on and clears it at 1133 ms
// (ClearActionOportunity a194d94d; its handler 223c7f empties the request's slot - one kind, or all six with ffff -
// and takes the kind's ability away); from then until the action is over a press starts nothing. The fists do the
// same (1383 of 1683 ms), the sword's second attack at 1533 of 1667 ms; great axe and daggers never clear. With the
// pad that part punishes a late press. A stroke of the hand comes when the hand gets there: in the headset run
// game-20261013-005905 the strokes were 1.2..1.3 s apart, and all eight presses that started nothing were sword
// strokes 1035..1220 ms into the first attack - after the vibration of the cooldown had ended (the user: "the
// vibration had ended and the second strike did not come").
// So for an action that a stroke started the clearing events are withheld, at every call site and on every Actor
// instance that plays it: the request stays until the action ends. The slots need no clearing: every start and end
// of an action empties the Actor's first bf8 bytes, slots included (1d8cb0, called at 1db108, 224ad2, 22a237).
// EVR_LIVE_STILL_KEEP bit 128 leaves the clearing as authored (for a comparison).
constexpr uint32_t clearActionOportunityEvent=0xa194d94d;
inline std::atomic<unsigned> chainKept{0},chainKeepFailures{0};
inline void chainStays(void* output,const void* actor){
 const auto until=ownedUntil.load();
 if(!until||!ownedSerial.load()||GetTickCount64()>until)return;
 try{
  const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
  if(!h[0]||!h[1])return;
  if(h[1]>256||h[1]>h[2])throw std::runtime_error("chain event array bounds");
  std::vector<uintptr_t> dst;unsigned withheld=0;int64_t at=0;
  for(size_t i=0;i<h[1];++i){const auto p=read<uintptr_t>(h[0]+i*8);if(read<uint32_t>(p)==clearActionOportunityEvent){++withheld;continue;}dst.push_back(p);}
  if(!withheld)return;
  const auto a=reinterpret_cast<uintptr_t>(actor);const auto head=read<std::array<uint8_t,0x40>>(a);uint32_t sequence=0,item=0,triggerEntity=0;int64_t trigger=0;
  std::memcpy(&sequence,head.data(),4);std::memcpy(&trigger,head.data()+8,8);std::memcpy(&triggerEntity,head.data()+16,4);std::memcpy(&item,head.data()+0x38,4);
  if(!ownedElsewhere(sequence,item,trigger,triggerEntity))return;
  if(!(read<uint8_t>(a+0x1b1)&1))return;at=read<int64_t>(a+0x1c0);
  if(h!=read<std::array<uintptr_t,4>>(address))throw std::runtime_error("chain selection changed its sources");
  if(!dst.empty())std::memcpy(reinterpret_cast<void*>(h[0]),dst.data(),dst.size()*8);const uint64_t count=dst.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
  if(chainKept.fetch_add(withheld)<36)logFile->event("live_still_chain_kept","\"actor\":"+hex(a)+",\"owned_actor\":"+(a==stillActor.load()?"true":"false")+",\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+
   ",\"action_ms\":"+std::to_string(at/1000000)+",\"clearing_events_withheld\":"+std::to_string(withheld)+",\"events_after\":"+std::to_string(dst.size())+",\"asset_written\":false,\"actor_written\":false");
 }catch(const std::exception& e){if(chainKeepFailures.fetch_add(1)<3)logFile->event("live_still_failure","\"reason\":"+jsonString(e.what()));}
}
// Diagnostic: which events still reach the game's systems during a swing the still route owns (by type and by where
// in the action they start). Headset note 2026-10-11: a white swing effect shows with the one-hand sword.
struct PassedEvent {uint32_t type=0,caller=0;unsigned count=0;int64_t first=0,last=0;};
inline std::array<PassedEvent,96> passedEvents{};inline std::mutex passedMutex;
inline void censusPassed(void* output,const void* actor,uint32_t caller){
 try{
  const auto now=GetTickCount64();
  {std::lock_guard<std::mutex> lock(retimeMutex);if(stillAction.until<now)return;}
  const auto before=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));if(!(before[0x1b1]&1))return;
  const auto key=actionKey(before,stillWorld.load(),stillEntity.load());if(!key.valid())return;
  {std::lock_guard<std::mutex> lock(retimeMutex);if(!(stillAction.until>=now&&stillAction.key==key))return;}
  const auto h=read<std::array<uintptr_t,4>>(reinterpret_cast<uintptr_t>(output));if(!h[0]||h[1]>256)return;
  // Where in the action this selection looks (the Actor's current window), per event type.
  int64_t start=0;std::memcpy(&start,before.data()+0x1b8,8);
  std::lock_guard<std::mutex> lock(passedMutex);
  for(size_t i=0;i<h[1];++i){const auto e=read<uintptr_t>(h[0]+i*8);const auto type=read<uint32_t>(e);
   for(auto& row:passedEvents)if(!row.count||row.type==type){
    if(!row.count){row.type=type;row.caller=caller;row.first=row.last=start;}
    ++row.count;row.first=std::min(row.first,start);row.last=std::max(row.last,start);break;}
  }
 }catch(const std::exception&){}
}
// With the hand-drawn bow (-BowDraw) the game's own arrow, which ShowAmmunition puts on the animated hand of the
// original model, appeared below the bow that the player holds (headset run game180455: "the arrow is at the lower
// left and flies from there"). The event is withheld for the local player's Actor at every call site, whatever
// the action; the shot itself (the projectile spawn at the tracked bow) does not depend on it.
inline bool hideAmmunition=false;inline std::atomic<unsigned> ammunitionWithheld{0},ammunitionFailures{0};
constexpr uint32_t showAmmunitionEvent=0x2f729879;
inline void withholdAmmunition(void* output){
 try{
  const auto address=reinterpret_cast<uintptr_t>(output);const auto h=read<std::array<uintptr_t,4>>(address);
  if(h[1]>256||h[1]>h[2]||(!h[0]&&h[1]))throw std::runtime_error("ammunition event array bounds");
  std::vector<uintptr_t> kept;unsigned removed=0;
  for(size_t i=0;i<h[1];++i){const auto p=read<uintptr_t>(h[0]+i*8);if(read<uint32_t>(p)==showAmmunitionEvent){++removed;continue;}kept.push_back(p);}
  if(!removed)return;
  if(!kept.empty())std::memcpy(reinterpret_cast<void*>(h[0]),kept.data(),kept.size()*8);const uint64_t count=kept.size();std::memcpy(reinterpret_cast<void*>(address+8),&count,8);
  ammunitionWithheld+=removed;
 }catch(const std::exception& e){if(ammunitionFailures.fetch_add(1)<3)logFile->event("ammunition_withhold_failure","\"reason\":"+jsonString(e.what()));}
}
// Mock evidence (policy word "observe" with "early"): every call site of the native selection and the event type it filters
// for, counted. The attack animation keeps playing when its SetAnimationEvent is withheld at callerRva (game163720),
// so another system selects that event for itself.
inline bool callerCensus=false;inline std::array<std::atomic<uint64_t>,64> censusKeys{};inline std::array<std::atomic<uint32_t>,64> censusCounts{};
inline void census(uintptr_t caller,const void* filter){
 uint32_t hash=0xffffffffu;try{if(filter){const auto f=read<std::array<uint8_t,5>>(reinterpret_cast<uintptr_t>(filter));if(f[4])std::memcpy(&hash,f.data(),4);else hash=0;}}catch(...){hash=0xfffffffeu;}
 const uint64_t key=(uint64_t(uint32_t(caller))<<32)|hash;
 for(size_t i=0;i<censusKeys.size();++i){uint64_t seen=censusKeys[i].load();
  if(!seen&&censusKeys[i].compare_exchange_strong(seen,key))seen=key;
  if(seen==key){++censusCounts[i];return;}}
}
__declspec(noinline) uintptr_t __fastcall select(void* output,const void* actor,void* registry,uint8_t overlap,const void* filter){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
 if(callerCensus)census(caller,filter);
 struct Guard {Guard(){++callbacks;}~Guard(){--callbacks;}} guard;
 std::array<uint8_t,3104> before{};uintptr_t world=0;uint32_t entity=0;bool observe=false;
 {const ObserverLastError preserve;
  if(accepting.load()&&caller==callerRva)try{
   if(retainedOnly.load()||combatEpochGate::actorStopped()){
    motion::RetainedEventScope scope;{std::lock_guard<std::mutex> lock(retimeMutex);scope=retainedScope;}
    ++retainedCalls;
    if(scope.acceptsSampledCall(reinterpret_cast<uintptr_t>(actor),reinterpret_cast<uintptr_t>(registry),[](uintptr_t world){++retainedEpochSamples;return combatEpochGate::serverStamp(world);})){
     world=scope.action.world;entity=scope.action.entity;
     before=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));
     uint32_t triggerEntity=0;std::memcpy(&triggerEntity,before.data()+16,4);
     observe=(before[0x1b1]&1)!=0&&scope.matchesAction(actionKey(before,world,entity),triggerEntity);
    }
   }else{
    const auto root=rootCamera::snapshot();entity=root.entity;
    world=nativeImpactComponents::resolveWorld(entity,reinterpret_cast<uintptr_t>(actor),3104);
    if(world){before=read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor));observe=(before[0x1b1]&1)!=0;
     if(liveStill||hideAmmunition){stillWorld=world;stillEntity=entity;stillActor=reinterpret_cast<uintptr_t>(actor);}
     noteActor(actor);   // every call (headset run 11: with the still experiment alone the slots were never found)
     remoteActions::serverWorld=world;
     if(!nativeImpactComponents::forceClientTicks){liveTargets::tick(world,entity);liveActionbars::tick(world,entity);liveEquipment::tick(world,entity);fogTimer::tick(world,entity);}}
   }
  }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("motion_event_selection_failure","\"reason\":"+jsonString(e.what()));}
 }
 const auto result=original(output,actor,registry,overlap,filter);const ObserverLastError preserve;
 if(hideAmmunition&&accepting.load()&&reinterpret_cast<uintptr_t>(actor)==stillActor.load())withholdAmmunition(output);
 if(liveStill&&caller!=callerRva&&accepting.load()&&reinterpret_cast<uintptr_t>(actor)==stillActor.load())stillElsewhere(output,actor,registry,filter);
 if(accepting.load()&&reinterpret_cast<uintptr_t>(actor)!=stillActor.load())noteActor(actor);
 if(liveStill&&accepting.load()&&reinterpret_cast<uintptr_t>(actor)!=stillActor.load()&&!(stillExperiment&64))stillReplica(output,actor,registry,filter);
 if(liveStill&&accepting.load()&&!(stillKeep&64))walkAbort(actor);
 if(liveStill&&accepting.load()&&controllerUiFixture::liveCooldownEnabled())chainEarly(actor);
 if(liveStill&&accepting.load()&&controllerUiFixture::liveCooldownEnabled()&&!(stillKeep&128))chainStays(output,actor);
 if(remoteActions::wanted&&accepting.load()&&reinterpret_cast<uintptr_t>(actor)!=stillActor.load()){const ObserverLastError keep;remoteActions::apply(output,actor,registry,caller==callerRva);}
 if((stillExperiment&64)&&liveStill&&accepting.load()&&reinterpret_cast<uintptr_t>(actor)!=stillActor.load())try{
  static std::atomic<unsigned> logged{0};const auto head=read<std::array<uint8_t,0x40>>(reinterpret_cast<uintptr_t>(actor));uint32_t sequence=0,item=0;std::memcpy(&sequence,head.data(),4);std::memcpy(&item,head.data()+0x38,4);
  uint32_t owned=0;{std::lock_guard<std::mutex> lock(retimeMutex);if(stillAction.until>=GetTickCount64())owned=stillAction.key.sequence;}
  if(owned&&sequence==owned&&logged.fetch_add(1)<60){const auto h=read<std::array<uintptr_t,4>>(reinterpret_cast<uintptr_t>(output));std::string types;
   for(size_t i=0;i<h[1]&&i<24;++i)types+=(i?",":"")+std::to_string(read<uint32_t>(read<uintptr_t>(h[0]+i*8)));
   const auto full=read<std::array<uint8_t,0x1c8>>(reinterpret_cast<uintptr_t>(actor));int64_t start=0,end=0;std::memcpy(&start,full.data()+0x1b8,8);std::memcpy(&end,full.data()+0x1c0,8);
   logFile->event("live_still_other_actor","\"actor\":"+hex(reinterpret_cast<uintptr_t>(actor))+",\"owned_actor\":"+hex(stillActor.load())+",\"caller\":"+hex(caller)+",\"sequence\":"+std::to_string(sequence)+",\"item\":"+std::to_string(item)+
    ",\"head\":"+jsonString(nativeImpactComponents::asHex(head))+",\"range\":["+std::to_string(start)+","+std::to_string(end)+"],\"flag\":"+std::to_string(full[0x1b1])+",\"event_types\":["+types+"],\"read_only\":true");}
 }catch(...){}
 if((stillExperiment&128)&&liveStill&&accepting.load())try{
  const auto until=ownedUntil.load();
  if(until&&GetTickCount64()<=until){
   const auto a=reinterpret_cast<uintptr_t>(actor);const auto head=read<std::array<uint8_t,0x40>>(a);uint32_t sequence=0,item=0;std::memcpy(&sequence,head.data(),4);std::memcpy(&item,head.data()+0x38,4);
   if(sequence==ownedSequence.load()&&item==ownedItem.load()){
    static std::mutex guard;static std::array<std::pair<uintptr_t,int64_t>,4> seen{};static unsigned logged=0;std::lock_guard<std::mutex> lock(guard);
    const auto window=read<std::array<int64_t,2>>(a+0x1b8);size_t slot=seen.size();
    for(size_t i=0;i<seen.size();++i)if(seen[i].first==a||!seen[i].first){slot=i;break;}
    if(slot<seen.size()&&(seen[slot].first!=a||seen[slot].second!=window[0])&&logged<1200){seen[slot]={a,window[0]};++logged;
     logFile->event("live_still_abilities","\"actor\":"+hex(a)+",\"owned_actor\":"+(a==stillActor.load()?"true":"false")+",\"sequence\":"+std::to_string(sequence)+",\"range_ms\":["+std::to_string(window[0]/1000000)+","+std::to_string(window[1]/1000000)+
      "],\"abilities\":"+hex(read<uint32_t>(a+0xbc8))+",\"bc0\":"+hex(read<uint64_t>(a+0xbc0))+",\"bd0\":"+hex(read<uint64_t>(a+0xbd0))+",\"bd8\":"+hex(read<uint64_t>(a+0xbd8))+",\"bf8\":"+hex(read<uint64_t>(a+0xbf8))+",\"flag_1b1\":"+std::to_string(read<uint8_t>(a+0x1b1))+",\"read_only\":true");}
   }
  }
 }catch(...){}
 if(liveStill&&callerCensus&&accepting.load()&&reinterpret_cast<uintptr_t>(actor)==stillActor.load())censusPassed(output,actor,uint32_t(caller));
 if(!observe)return result;
 try{
  const auto address=reinterpret_cast<uintptr_t>(output);const auto header=read<std::array<uintptr_t,4>>(address);
  if(header[1]>256||header[1]>header[2]||header[2]>0x100000||(!header[0]&&header[1]))throw std::runtime_error("native event array bounds");
  const auto f=read<std::array<uint8_t,5>>(reinterpret_cast<uintptr_t>(filter));uint32_t hash=0;if(f[4])std::memcpy(&hash,f.data(),4);
  const bool stable=before==read<std::array<uint8_t,3104>>(reinterpret_cast<uintptr_t>(actor))&&header==read<std::array<uintptr_t,4>>(address)&&f==read<std::array<uint8_t,5>>(reinterpret_cast<uintptr_t>(filter));
  if(!stable)throw std::runtime_error("event selection changed Actor or source descriptor");
  const auto observation=recordingBudget.observe();++samples;
  if(observation.record||observation.witness)try{
  uint32_t sequence=0,item=0;int64_t start=0,end=0,start2=0,end2=0,trigger=0;std::memcpy(&sequence,before.data(),4);std::memcpy(&item,before.data()+0x38,4);std::memcpy(&trigger,before.data()+8,8);
  std::memcpy(&start,before.data()+0x1b8,8);std::memcpy(&end,before.data()+0x1c0,8);std::memcpy(&start2,before.data()+0x1c8,8);std::memcpy(&end2,before.data()+0x1d0,8);
  std::ostringstream o;o<<"\"sample\":"<<observation.ordinal<<",\"world\":"<<hex(world)<<",\"entity\":"<<entity<<",\"sequence\":"<<sequence<<",\"item\":"<<item<<",\"trigger\":"<<trigger<<",\"range\":["<<start<<','<<end<<"],\"range2\":["<<start2<<','<<end2<<"],\"second_range\":"<<(before[0xbf0]?"true":"false")<<",\"include_overlap\":"<<unsigned(overlap)<<",\"filter_present\":"<<(f[4]?"true":"false")<<",\"filter_hash\":"<<hash<<",\"capacity\":"<<header[2]<<",\"events\":[";
  for(uint64_t i=0;i<header[1];++i){
   if(i)o<<',';const auto event=read<uintptr_t>(header[0]+i*8);const auto data=sequenceAssetObserver::relative(event+4);const auto type=read<uint32_t>(event);
   if(!data)throw std::runtime_error("native selected event payload null");
   o<<"{\"type_hash\":"<<type<<",\"variant\":"<<hex(event)<<",\"payload\":"<<hex(data)<<",\"variant_bytes\":"<<jsonString(sequenceAssetObserver::blob<12>(event))<<",\"base_event\":"<<jsonString(sequenceAssetObserver::blob<96>(data));
   // The first bytes of the derived event (ability and state masks, snap type, movement factors) for the event catalogue.
   if(const auto size=read<uint32_t>(event+8);size>=120&&size<=4096)o<<",\"tail\":"<<jsonString(sequenceAssetObserver::blob<24>(data+96));else if(size>=104&&size<=4096)o<<",\"tail\":"<<jsonString(sequenceAssetObserver::blob<8>(data+96));
   if(type==0x64b5dc20)o<<",\"spawn_impact\":"<<jsonString(sequenceAssetObserver::blob<488>(data));o<<'}';
  }
  o<<"],\"state_tags\":"<<jsonString(sequenceAssetObserver::blob<128>(reinterpret_cast<uintptr_t>(before.data()+0xec)))<<",\"actor_unchanged\":"<<(stable?"true":"false")<<",\"milliseconds\":"<<GetTickCount64()<<",\"thread\":"<<GetCurrentThreadId()<<",\"caller\":\"0x224524\",\"read_only\":true,\"return_unchanged\":true";
  logFile->event(observation.record?"motion_event_selection":"motion_event_selection_budget_witness",o.str());
  if(observation.record)++recordedSamples;else ++witnessSamples;
  }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("motion_event_recording_failure","\"reason\":"+jsonString(e.what()));}
  else ++omittedSamples;
  // Optional evidence exhaustion never skips suppression or native candidate selection.
  retime(output,actor,before,world,entity);
  liveEvents(output,actor,registry,before,world,entity);
  if(!retainedOnly.load()&&!combatEpochGate::actorStopped())shadow(output,actor,registry,before,world,entity);
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("motion_event_selection_failure","\"reason\":"+jsonString(e.what()));}
 return result;
}
inline void install(){
 {char value[16]{};const auto n=GetEnvironmentVariableA("EVR_STILL_EXPERIMENT",value,sizeof value);
  if(n&&n<sizeof value){stillExperiment=unsigned(std::strtoul(value,nullptr,10))&255;stillImpactElsewhere=stillExperiment&2;
   logFile->event("live_still_experiment","\"bits\":"+std::to_string(stillExperiment)+",\"diagnostic\":true");}}
 {char value[16]{};const auto n=GetEnvironmentVariableA("EVR_LIVE_STILL_KEEP",value,sizeof value);
  if(n&&n<sizeof value){stillKeep=unsigned(std::strtoul(value,nullptr,10))&255;
   logFile->event("live_still_keep","\"bits\":"+std::to_string(stillKeep)+",\"swing_sound_kept\":"+((stillKeep&1)?"true":"false")+",\"single_impact_only\":"+((stillKeep&2)?"true":"false")+",\"tool_stroke_native\":"+((stillKeep&4)?"true":"false")+",\"movement_factors_kept\":"+((stillKeep&8)?"true":"false"));}}
 const auto bytes=parsePattern(pattern);const auto call=baseAddress+callerRva-5;
 if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)throw std::runtime_error("native event selection signature differs");
 if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&select),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("native event selection hook failed");
 enabled=true;accepting=true;logFile->event("motion_event_selection_enabled","\"rva\":\"0x213b50\",\"caller\":\"0x224524\",\"pattern\":"+jsonString(pattern)+",\"read_only\":"+(retimeRequested?"false":"true")+",\"one_impact_retime\":"+(retimeRequested?"true":"false"));
}
inline void disable(){
 accepting=false;if(!enabled)return;
 if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("native event selection hook disable failed");enabled=false;
 const auto deadline=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 logFile->event("motion_event_recording_summary","\"processed\":"+std::to_string(samples.load())+",\"recorded\":"+std::to_string(recordedSamples.load())+",\"witness\":"+std::to_string(witnessSamples.load())+",\"omitted\":"+std::to_string(omittedSamples.load())+",\"limit\":"+std::to_string(recordingBudget.limit()));
 logFile->event("motion_event_retained_filter_summary","\"calls\":"+std::to_string(retainedCalls.load())+",\"fresh_epoch_samples\":"+std::to_string(retainedEpochSamples.load())+",\"foreign_skipped\":"+std::to_string(retainedCalls.load()-retainedEpochSamples.load())+",\"callbacks\":"+std::to_string(callbacks.load())+",\"cached_epoch\":false,\"dereferences_saved_actor\":false");
 if(liveRetime)logFile->event("live_event_retime_removed","\"appended\":"+std::to_string(liveAppended.load())+",\"suppressed\":"+std::to_string(liveSuppressed.load())+",\"movement_events_withheld\":"+std::to_string(liveStilled.load())+",\"movement_abilities_early\":"+std::to_string(liveMovedEarly.load())+",\"look_aheads\":"+std::to_string(liveLookAheads.load())+",\"not_single_impact\":"+std::to_string(liveNotSingle.load())+",\"not_qualified\":"+std::to_string(liveUnqualified.load())+",\"failures\":"+std::to_string(failures.load()));
 if(retimeRequested)logFile->event("motion_event_retime_removed","\"appended\":"+std::to_string(appended)+",\"suppressed\":"+std::to_string(suppressed)+",\"blocked\":"+std::to_string(blocked));
 const auto bytes=parsePattern(pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
 liveTargets::summary();remoteActions::summary();
 if(hideAmmunition)logFile->event("ammunition_withhold_removed","\"show_ammunition_events_withheld\":"+std::to_string(ammunitionWithheld.load())+",\"failures\":"+std::to_string(ammunitionFailures.load()));
 if(liveStill&&callerCensus){std::ostringstream rows;std::lock_guard<std::mutex> lock(passedMutex);bool first=true;
  for(const auto& row:passedEvents)if(row.count){rows<<(first?"":",")<<"{\"type\":"<<row.type<<",\"caller\":\"0x"<<std::hex<<row.caller<<std::dec<<"\",\"count\":"<<row.count<<",\"first_start_ms\":"<<row.first/1000000<<",\"last_start_ms\":"<<row.last/1000000<<"}";first=false;}
  logFile->event("live_still_passed_events","\"rows\":["+rows.str()+"]");}
 if(liveStill)logFile->event("live_still_removed","\"other_call_site_calls\":"+std::to_string(stillElsewhereCalls.load())+",\"movement_events_withheld\":"+std::to_string(stillElsewhereWithheld.load())+",\"movement_abilities_early\":"+std::to_string(stillElsewhereAbilities.load())+",\"failures\":"+std::to_string(stillElsewhereFailures.load()));
 if(callerCensus){std::ostringstream o;o<<"\"callers\":[";bool first=true;
  for(size_t i=0;i<censusKeys.size();++i){const auto key=censusKeys[i].load();if(!key)break;if(!first)o<<',';first=false;
   o<<"{\"caller\":"<<hex(uintptr_t(key>>32))<<",\"filter_hash\":"<<uint32_t(key)<<",\"calls\":"<<censusCounts[i].load()<<'}';}
  o<<"],\"filter_hash_4294967295\":\"no filter argument\",\"filter_hash_0\":\"filter absent\"";logFile->event("motion_event_selection_callers",o.str());}
 logFile->event("motion_event_selection_removed",std::string("\"restored\":")+(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load())+",\"samples\":"+std::to_string(samples.load())+",\"failures\":"+std::to_string(failures.load()));
 // Live play may end without any action, and a rejected read there is reported, not fatal.
 if(liveRetime){if(!restored||callbacks.load())throw std::runtime_error("native event selection did not restore/quiesce");return;}
 if(!restored||callbacks.load()||failures.load()||!samples.load()||(shadowRequested&&shadowCalls.load()!=controllerUiFixture::expectedNativeActions())||(retimeRequested&&(appended+blocked!=controllerUiFixture::expectedNativeActions()||suppressed!=controllerUiFixture::expectedNativeActions())))throw std::runtime_error("native event selection observation failed");
}
}
