#pragma once
// Read-only trial: propose the rendered weapon transform at the native owned
// collision boundary. Neither stack query nor ECS components are changed.
namespace motionColliderShadow {
inline thread_local motion::ColliderPose acceptedPose;
inline thread_local bool ownedIntent=false;
inline std::atomic<unsigned> samples{0},accepted{0},rejected{0},failures{0};
// 1.0 (2026-10-17): a collider from the sweep 0x32c230 (sequenceColliderObserver::colliderSweep). 1.0's impacts carry no
// item config (the impact query's filled slots are 2, 3, 4 and 8 only - headset runs of 2026-10-17, impact_hierarchy_slots)
// and the server's attachment slot is not bound, so June's proof of the owner (the impact's item and its pide, the stable
// owner, the server slot's attachment) cannot be made: ownership was refused for every collider (Mock game-20261017-050209,
// 52 of 52, reason "ownership") and no stroke ever moved the hit volume. For these colliders the owner is proved by what
// 1.0 has: the collider's action sequence is the local actor's running one (ready() returns before otherwise), the impact
// hierarchy's root - when the collider has one - is the local entity, the item is the actor's used item and the shown
// weapon's; the attachment is the shown weapon's (its slot and calibration are still checked).
inline thread_local bool fromSweep=false;inline std::atomic<unsigned> sweepOwners{0};
inline void inspect(uintptr_t state,uint32_t entity,void* query,const actionSequenceObserver::Status& actor,unsigned readySample){
 acceptedPose={};ownedIntent=false;if(!motionColliderChannel::requested)return;
 try{
  const auto published=motionColliderChannel::snapshotFrame();const auto& p=published.pose;motion::ColliderEvidence e;e.now=GetTickCount64();
  const auto root=rootCamera::snapshot();const auto visual=attachmentObserver::meleeAppearance(root.entity,e.now,motionColliderChannel::live);
  const auto ui=inputUi::snapshot();const auto& hands=published.hands;e.handMs=published.handMs;
  const auto owner=nativeImpactComponents::owner(state,entity);
  e.frame=hands.frame;e.session=hands.sessionGeneration;e.origin=hands.originGeneration;
  // Live play holds the weapon without a grip button (the right grip is free, 2026-10-11).
  e.tracked=hands.focused&&hands.hands[1].tracked()&&(motionColliderChannel::live||(hands.hands[1].grabActive&&hands.hands[1].grab));
  const bool bothRequired=controllerUiFixture::twoHandAttackRequested;
  const bool supportValid=!bothRequired||controllerUiFixture::twoHandSupport(hands);
  e.tracked=e.tracked&&supportValid;
  e.root=root.entity;e.clientWorld=root.world;e.actorWorld=actor.world;e.impactWorld=owner.world;
  combatWorldBridge::observe(combatWorldBridge::Stage::Collider,e.clientWorld,e.impactWorld);
  e.actorEntity=actor.entity;e.actorMs=actor.milliseconds;e.active=actor.active;e.alive=actor.alive;e.ready=actor.ready;e.item=actor.usedItem;
  e.normalUi=inputUi::boundNormal(ui,ui.senderContext,root.entity,e.now);e.firstPerson=rootCamera::firstPerson(root);
  e.visualMs=visual.milliseconds;e.visualChild=visual.child;e.visualItem=visual.item;e.visualAttachment=visual.slotBytes;
  e.ownerStable=owner.ready&&owner.stable;
  std::memcpy(&e.impactRoot,owner.hierarchy.data()+4,4);std::memcpy(&e.weaponPide,owner.hierarchy.data()+8,4);
  std::memcpy(&e.impactItem,owner.item.data()+8,4);std::memcpy(&e.itemPide,owner.item.data()+12,4);
  const auto ent=nativeImpactComponents::entityAt(state,entity);
  const auto slot=owner.binding.serverSlot<0?0:nativeImpactComponents::component(ent,owner.binding.serverSlot,112);
  if(slot){const auto first=read<std::array<uint8_t,112>>(slot);e.slotStable=first==read<std::array<uint8_t,112>>(slot);std::memcpy(e.impactAttachment.data(),first.data(),48);}
  if(visual.world!=root.world||!visual.valid)e.slotStable=false;
  if(fromSweep&&!owner.ready){
   uint32_t hierarchyRoot=0;
   if(owner.binding.hierarchy>=0)if(const auto h=nativeImpactComponents::component(ent,owner.binding.hierarchy,16))hierarchyRoot=read<uint32_t>(h+4);
   e.impactRoot=hierarchyRoot?hierarchyRoot:(actor.active?actor.entity:0);
   e.impactItem=actor.usedItem;e.weaponPide=e.itemPide=1;e.ownerStable=true;
   e.slotStable=visual.valid&&visual.world==root.world;e.impactAttachment=visual.slotBytes;
   if(sweepOwners.fetch_add(1)<3)logFile->event("motion_collider_sweep_owner","\"entity\":"+std::to_string(entity)+",\"hierarchy_root\":"+std::to_string(hierarchyRoot)+",\"actor\":"+std::to_string(actor.entity)+",\"item\":"+std::to_string(actor.usedItem)+",\"visual_item\":"+std::to_string(visual.item));
  }
  const auto q=read<std::array<uintptr_t,10>>(reinterpret_cast<uintptr_t>(query));
  const auto event=read<std::array<uint8_t,24>>(q[4]);
  const auto authority=motionColliderChannel::authorized();
  ownedIntent=owner.ready&&owner.stable&&e.actorWorld==owner.world&&e.impactRoot==actor.entity&&e.impactItem==actor.usedItem&&e.weaponPide&&e.weaponPide==e.itemPide&&authority.matches(owner.world,e.impactRoot,e.impactItem,actor.triggerTime,event,combatEpochGate::serverStamp(owner.world));
  // Live play: melee weapons only (native categories as in the live melee swing: one hand 0..3 and 13, two hands 7..10).
  // A staff's or wand's cast also owns an Impact on the weapon slot; that one stays where the game puts it.
  const bool meleeKind=visual.tool||(visual.weaponCategoryKnown&&(visual.weaponType<=3||visual.weaponType==13||(visual.weaponType>=7&&visual.weaponType<=10)));
  const char* reason=motionColliderChannel::requireAuthority&&!ownedIntent?"authority":combatEpochGate::poseRejection(p,e,motionColliderChannel::live);
  if(!reason&&motionColliderChannel::live&&!meleeKind)reason="weapon_kind";
  if(reason)++rejected;else{++accepted;acceptedPose=p;}
  // Live play: the first samples and every rejection among the first 200 are logged in full, then one in 50.
  if(motionColliderChannel::live&&samples.load()>=64&&(!reason||rejected.load()>200)&&samples.load()%50){++samples;return;}
  const auto original=read<std::array<uint8_t,56>>(q[1]);const auto proposed=motion::colliderTransform(p,original);
  std::ostringstream o;o<<"\"sample\":"<<++samples<<",\"ready_sample\":"<<readySample<<",\"entity\":"<<entity<<",\"root\":"<<e.root<<",\"item\":"<<e.item
   <<",\"frame\":"<<p.frame<<",\"serial\":"<<p.serial<<",\"time\":"<<p.predictedTime<<",\"session\":"<<p.session<<",\"origin\":"<<p.origin
   <<",\"pose_ms\":"<<p.milliseconds<<",\"milliseconds\":"<<e.now<<",\"hand_ms\":"<<e.handMs<<",\"actor_ms\":"<<e.actorMs<<",\"visual_ms\":"<<e.visualMs
   <<",\"hand_frame\":"<<e.frame<<",\"hand_session\":"<<e.session<<",\"hand_origin\":"<<e.origin
   <<",\"client_world\":"<<hex(p.clientWorld)<<",\"camera_world\":"<<hex(root.world)<<",\"actor_world\":"<<hex(actor.world)<<",\"impact_world\":"<<hex(owner.world)
   <<",\"impact_root\":"<<e.impactRoot<<",\"impact_item\":"<<e.impactItem<<",\"weapon_pide\":"<<e.weaponPide<<",\"item_pide\":"<<e.itemPide
   <<",\"normal_ui\":"<<(e.normalUi?"true":"false")<<",\"first_person\":"<<(e.firstPerson?"true":"false")<<",\"tracked\":"<<(e.tracked?"true":"false")
   <<",\"two_hand_required\":"<<(bothRequired?"true":"false")<<",\"two_hand_support_valid\":"<<(supportValid?"true":"false")
   <<",\"actor_active\":"<<(e.active?"true":"false")<<",\"actor_alive\":"<<(e.alive?"true":"false")<<",\"actor_ready\":"<<(e.ready?"true":"false")
   <<",\"owned_intent\":"<<(ownedIntent?"true":"false")<<",\"eligible\":"<<(reason?"false":"true")<<",\"reason\":"<<jsonString(reason?reason:"accepted")
   <<",\"proposed_transform\":"<<jsonString(nativeImpactComponents::asHex(proposed))<<",\"render_transform\":"<<jsonString(nativeImpactComponents::asHex(p.transform))<<",\"original_transform\":"<<jsonString(nativeImpactComponents::asHex(original))
   <<",\"visual_attachment\":"<<jsonString(nativeImpactComponents::asHex(e.visualAttachment))<<",\"impact_attachment\":"<<jsonString(nativeImpactComponents::asHex(e.impactAttachment))
   <<",\"thread\":"<<GetCurrentThreadId()<<",\"applied\":false,\"ecs_written\":false";
  logFile->event("motion_collider_shadow",o.str());
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("motion_collider_shadow_failure","\"reason\":"+jsonString(e.what()));}
}
inline void removed(){if(!motionColliderChannel::requested)return;motionColliderChannel::invalidate();logFile->event("motion_collider_shadow_removed","\"samples\":"+std::to_string(samples.load())+",\"accepted\":"+std::to_string(accepted.load())+",\"rejected\":"+std::to_string(rejected.load())+",\"failures\":"+std::to_string(failures.load())+",\"channel_invalidated\":true,\"applied\":false");}
}
