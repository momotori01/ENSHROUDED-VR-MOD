#include <evr/combat_world_binding.hpp>
#include <evr/world_epoch_broker.hpp>
#include <evr/motion_contact_lifetime.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr;using namespace evr::motion;
int main(){unsigned checks=0;try{
 auto ck=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 epoch::Broker<> broker;const auto provider=broker.call(epoch::request(epoch::Operation::Register));
 auto call=[&](epoch::Operation op,const epoch::Response& r){return broker.call(epoch::request(op,r));};
 auto create=[&](uint64_t world,uint64_t registry,uint64_t root,uint32_t kind){auto q=epoch::request(epoch::Operation::Create,provider);q.world=world;q.registry=registry;q.root=root;q.kind=kind;auto r=broker.call(q);ck(epoch::admitsNew(r),"valid native-created generation");return r;};
 for(unsigned cycle=0;cycle<5000;++cycle){
  auto server=create(100,101,102,0),client=create(200,201,202,2),prediction=create(300,201,202,1);
  const auto worlds=combatWorlds(client,server);ck(worlds.valid()&&permitsNewCombat(worlds,client,server),"client input and server contact use separate exact Worlds");
  ck(!combatWorlds(prediction,server).valid()&&!combatWorlds(server,client).valid(),"prediction and reversed roles cannot grant combat");
  auto foreign=server;foreign.provider++;ck(!combatWorlds(client,foreign).valid(),"cross-provider pair refused");
  MeleeNative native;native.world=client.world;native.entity=11;native.item=99;native.known=true;auto bound=bindCombatWorlds(native,worlds,client,server);
  ck(bound.known&&bound.worldSerial==client.serial&&native.worldSerial==0&&native.worlds.empty(),"generation is added only to private native snapshot");
  auto unavailable=native;unavailable.known=false;ck(!bindCombatWorlds(unavailable,worlds,client,server).known,"World proof cannot grant native readiness");
  EventActionKey raw{100,11,22,99,123,0,0};auto key=scopeAction(raw,worlds);EventIdentity id{1000,{1,2,3}};
  NativeAttackTicket ticket{11,99,100,200,1,true,true,5,1,1,1};ticket.worlds=worlds;
  ck(permitsNewImpact(ticket,key,250,client,server),"new scoped native Impact permits fresh live pair");
  auto legacy=ticket;legacy.worlds={};ck(!legacy.permits(key,250)&&!ticket.permits(raw,250),"unscoped and scoped tickets cannot silently mix");
  EventRetimeLedger<2> events;ck(events.commit(key,id),"consume one scoped event");
  std::array<uint8_t,24> nativeEvent{};std::memcpy(nativeEvent.data(),&key.sequence,4);std::memcpy(nativeEvent.data()+8,id.guid.data(),16);
  auto collider=scopeCollider({100,77,nativeEvent},worlds);EventAuthority authority{key,id.guid,true};ContactRetirementGate retirement;
  ck(retirement.initialize(authority,collider),"contact retirement requires matching event/collider epochs");
  auto wrongCollider=collider;wrongCollider.epoch.serial++;ContactRetirementGate invalidRetirement;ck(!invalidRetirement.initialize(authority,wrongCollider),"identical event payload cannot retire a different generation");
  auto cp=call(epoch::Operation::Acquire,client),sp=call(epoch::Operation::Acquire,server);
  auto closeClient=call(epoch::Operation::BeginRootClose,client);auto closingClient=call(epoch::Operation::Read,client);
  ck(!permitsNewImpact(ticket,key,250,closingClient,server)&&!bindCombatWorlds(native,worlds,closingClient,server).known,"client close blocks new attack even while server remains alive");
  ck(permitsOwnedSuppression(key.epoch,server,events.owns(key)),"consumed server event survives client closure");
  call(epoch::Operation::EndRootClose,closeClient);auto endedClient=call(epoch::Operation::Read,client);
  ck(!permitsNewCombat(worlds,endedClient,server)&&permitsOwnedSuppression(key.epoch,server,events.owns(key)),"client destruction cannot erase late server suppression history");
  auto newClient=create(200,201,202,2),newPrediction=create(300,201,202,1);auto nextPair=combatWorlds(newClient,server);
  ck(!permitsNewCombat(worlds,newClient,server)&&!bindCombatWorlds(bound,nextPair,newClient,server).known,"client address reuse cannot retag an old native snapshot");
  auto closeServer=call(epoch::Operation::BeginRootClose,server),closingServer=call(epoch::Operation::Read,server);
  ck(!permitsNewCombat(nextPair,newClient,closingServer)&&permitsOwnedSuppression(key.epoch,closingServer,events.owns(key)),"server closing blocks appends but retains exact consumed suppression");
  ck(events.consider(key,id,false,false,0,0)==EventRetime::Decision::Suppress,"owned authored event remains consumed regardless of ticket age");
  ck(!permitsOwnedSuppression(key.epoch,closingServer,false),"unconsumed event never gains suppression authority");
  call(epoch::Operation::EndRootClose,closeServer);auto endedServer=call(epoch::Operation::Read,server);ck(!permitsOwnedSuppression(key.epoch,endedServer,true),"ended server cannot authorize new native calls");
  auto newServer=create(100,101,102,0);auto replacement=combatWorlds(newClient,newServer);auto replacementKey=scopeAction(raw,replacement);
  ck(!events.owns(replacementKey)&&!permitsOwnedSuppression(key.epoch,newServer,true),"all addresses/Actor/item/event reused without old suppression entering new World");
  ck(!scopeAction(key,replacement).valid()&&!scopeCollider(collider,replacement).valid(),"already scoped action/collider cannot be retagged with replacement generation");
  auto replacementCollider=scopeCollider({100,77,nativeEvent},replacement);ColliderLeaseLedger<2> colliders;
  ck(colliders.decide(collider,true,0)==ColliderDecision::Track&&colliders.returned(collider,5),"old collider lease acquired");
  ck(!colliders.owns(replacementCollider)&&colliders.decide(replacementCollider,true,0)==ColliderDecision::Track,"identical Collider ID and payload require distinct epoch lease");
  ContactBoundary oldBoundary;oldBoundary.world=100;oldBoundary.registry=101;oldBoundary.taskList=9;oldBoundary.actor=11;oldBoundary.worldKind=0;oldBoundary.epoch=key.epoch;oldBoundary.tick=1;oldBoundary.entityCounter=1;
  ContactBoundaryContinuity continuity;ck(continuity.observe(oldBoundary),"initial task boundary captured");auto newBoundary=oldBoundary;newBoundary.epoch=replacementKey.epoch;ck(!continuity.observe(newBoundary)&&continuity.faulted(),"idle native boundary cannot recycle another World epoch even with all counters equal");
  call(epoch::Operation::Release,cp);call(epoch::Operation::Release,sp);
  call(epoch::Operation::EndRootClose,call(epoch::Operation::BeginRootClose,newClient));call(epoch::Operation::EndRootClose,call(epoch::Operation::BeginRootClose,newServer));
  auto stats=call(epoch::Operation::Stats,provider);ck(stats.worlds==stats.closures&&stats.worlds==0&&stats.leases==0,"all held generations drain after cycle");
 }
 // Coordinator must revoke an in-flight request when only contact World changes.
 auto server=create(100,101,102,0),client=create(200,201,202,2),prediction=create(300,201,202,1);auto worlds=combatWorlds(client,server);
 HandFrame f;f.focused=true;f.sessionGeneration=f.originGeneration=1;for(auto& h:f.hands){h.profile=1;h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;}
 Weapon weapon;weapon.binding=5;MeleeNative n;n.world=client.world;n.entity=11;n.item=99;n.binding=5;n.known=n.local=n.alive=n.gameplay=n.canPrimary=true;
 MeleeSession session{{2,.3f,.6f,2,100000000,12,30},{120,1000,200}};uint64_t now=1000;float x=.2f;MeleeCommand out;
 auto step=[&](float dx){now+=20;x+=dx;++f.frame;f.predictedTime+=20000000;f.hands[1].grip.position={x,1,-.4f};n.milliseconds=now;out=session.update(now,f,now,weapon,bindCombatWorlds(n,worlds,client,server),{true,true,false});};
 for(unsigned i=0;i<4;++i)step(0);step(.06f);const auto issued=out.issued;ck(issued&&out.level==255,"scoped physical swing requests original input");
 call(epoch::Operation::EndRootClose,call(epoch::Operation::BeginRootClose,server));server=create(100,101,102,0);worlds=combatWorlds(client,server);step(0);
 ck(out.revoked&&out.retired==issued&&!out.level&&out.reason==MeleeStop::EpochChanged,"server-only generation change neutralizes existing input and ticket");

 ColliderPose pose;pose.valid=true;pose.frame=30;pose.session=2;pose.origin=3;pose.milliseconds=1000;pose.serial=40;pose.clientWorld=200;pose.predictedTime=2000;pose.root=11;pose.item=99;pose.child=50;
 std::array<float,7> transform{0,0,0,1,1,1,1};std::memcpy(pose.transform.data()+24,transform.data(),28);
 float one=1;std::memcpy(pose.attachment.data()+36,&one,4);pose.attachment[41]=9;std::memcpy(pose.attachment.data()+44,&pose.root,4);
 ColliderEvidence e;e.now=1010;e.frame=30;e.session=2;e.origin=3;e.handMs=e.actorMs=e.visualMs=1000;e.clientWorld=200;e.actorWorld=e.impactWorld=100;
 e.root=e.actorEntity=e.impactRoot=11;e.item=e.visualItem=e.impactItem=99;e.visualChild=50;e.weaponPide=e.itemPide=4321;
 e.normalUi=e.firstPerson=e.tracked=e.active=e.alive=e.ready=e.ownerStable=e.slotStable=true;e.visualAttachment=e.impactAttachment=pose.attachment;
 auto captured=bindColliderWorlds(pose,worlds,client,server);ck(!scopedColliderPoseRejection(captured,e,client,server),"live paired pose retains native geometry validation");
 ck(colliderPoseRejection(captured,e)!=nullptr,"legacy pose gate cannot bypass required live generation proof");
 for(unsigned fault=0;fault<12;++fault){auto p=captured;auto proof=e;auto input=client,contact=server;
  switch(fault){case 0:input.phase=uint32_t(epoch::Phase::Closing);break;case 1:contact.phase=uint32_t(epoch::Phase::Closing);break;case 2:contact.serial++;break;case 3:input.provider++;break;case 4:contact.status=uint32_t(epoch::Status::WrongToken);break;case 5:input.flags=epoch::flagFaulted;break;case 6:p.worlds={};break;case 7:proof.normalUi=false;break;case 8:proof.slotStable=false;break;case 9:proof.item++;break;case 10:p.milliseconds=1;break;case 11:proof.impactWorld++;break;}
  ck(scopedColliderPoseRejection(p,proof,input,contact)!=nullptr,"World guard does not bypass pose freshness, UI, item, or geometry guards");
 }
 auto closed=call(epoch::Operation::BeginRootClose,server);auto contactClosing=call(epoch::Operation::Read,server);
 ck(!bindColliderWorlds(pose,worlds,client,contactClosing).valid,"no new pose publication after root close");
 call(epoch::Operation::EndRootClose,closed);auto replacementServer=create(100,101,102,0);auto replaced=combatWorlds(client,replacementServer);
 ck(!bindColliderWorlds(captured,replaced,client,replacementServer).valid,"captured pose cannot be retagged into replacement server World");
 NativeAttackTicket scopedTicket{11,99,100,200,1,true,true,5,1,1,1};scopedTicket.worlds=replaced;auto scopedKey=scopeAction({100,11,22,99,123,0,0},replaced);
 ck(scopedTicket.identifies(scopedKey,250)&&!scopedTicket.permits(scopedKey,250)&&permitsNewImpact(scopedTicket,scopedKey,250,client,replacementServer),"scoped ticket cannot use legacy permit API without live pair proof");
 call(epoch::Operation::Fault,provider);auto faultedServer=call(epoch::Operation::Read,replacementServer),faultedClient=call(epoch::Operation::Read,client);
 ck(!permitsNewImpact(scopedTicket,scopedKey,250,faultedClient,faultedServer)&&permitsOwnedSuppression(scopedKey.epoch,faultedServer,true),"fault disables appends but preserves exact consumed suppression before native teardown");
 std::cout<<"PASS "<<checks<<" two-World combat generation checks; 5000 native-value lifecycle cycles, gameplay adapter not connected\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
