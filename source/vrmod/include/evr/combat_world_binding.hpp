#pragma once
#include <evr/melee_session.hpp>
#include <evr/motion_event_retime.hpp>
#include <evr/motion_collider_lease.hpp>
#include <evr/motion_collider_pose.hpp>
namespace evr::motion {
// Strict opt-in bridge for the process broker. Legacy finite diagnostics have
// empty contexts; never treat those as verified generations in these functions.
inline MeleeNative bindCombatWorlds(MeleeNative value,const CombatWorlds& saved,const epoch::Response& input,const epoch::Response& contact){
 if(!value.worlds.empty()&&!(value.worlds==saved)){value.known=false;return value;}
 value.worlds=saved;value.worldSerial=0;
 if(!permitsNewCombat(saved,input,contact)||value.world!=saved.input.world){value.known=false;return value;}
 value.worldSerial=saved.input.serial;return value;
}
inline EventActionKey scopeAction(EventActionKey key,const CombatWorlds& saved){
 if(!saved.valid()||key.world!=saved.contact.world||(!key.epoch.empty()&&!(key.epoch==saved.contact)))return {};
 key.epoch=saved.contact;return key;
}
inline ColliderKey scopeCollider(ColliderKey key,const CombatWorlds& saved){
 if(!saved.valid()||key.world!=saved.contact.world||(!key.epoch.empty()&&!(key.epoch==saved.contact)))return {};
 key.epoch=saved.contact;return key;
}
inline bool permitsNewImpact(const NativeAttackTicket& ticket,const EventActionKey& key,uint64_t now,const epoch::Response& input,const epoch::Response& contact){
 return permitsNewCombat(ticket.worlds,input,contact)&&!ticket.revoked&&ticket.identifies(key,now);
}
inline ColliderPose bindColliderWorlds(ColliderPose value,const CombatWorlds& saved,const epoch::Response& input,const epoch::Response& contact){
 if((!value.worlds.empty()&&!(value.worlds==saved))||!permitsNewCombat(saved,input,contact)||value.clientWorld!=saved.input.world){value.valid=false;return value;}
 value.worlds=saved;return value;
}
inline const char* scopedColliderPoseRejection(const ColliderPose& pose,const ColliderEvidence& evidence,const epoch::Response& input,const epoch::Response& contact){
 const auto& captured=pose.worlds;
 if(!permitsNewCombat(captured,input,contact)||pose.clientWorld!=captured.input.world||evidence.clientWorld!=captured.input.world||evidence.actorWorld!=captured.contact.world||evidence.impactWorld!=captured.contact.world)return "world_epoch";
 // The original geometry/UI/ownership guards still run after the live proof.
 auto unscoped=pose;unscoped.worlds={};return colliderPoseRejection(unscoped,evidence);
}
}
