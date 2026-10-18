#pragma once
#include <evr/head_gesture.hpp>
#include <evr/motion_intent.hpp>
namespace evr::motion {
// This gate cancels only an unacknowledged local intent and clears swing history.
// It never changes native action state, accepted costs, movement or cooldown.
// The caller still publishes native recovery feedback independently of UI input.
inline bool gestureInhibitsCombat(const HandFrame& hands,const HeadGestureResult& gesture){
 const bool current=gesture.frame==hands.frame&&gesture.session==hands.sessionGeneration&&
     gesture.origin==hands.originGeneration&&gesture.time==hands.predictedTime;
 return !current||!gesture.valid||gesture.active||gesture.resetSwing;
}
inline Intent gestureCombat(IntentGate& gate,const HandFrame& hands,const Weapon& weapon,
                            NativeCombat native,const HeadGestureResult& gesture){
 native.uiOrEvent|=gestureInhibitsCombat(hands,gesture);
 return gate.update(hands,weapon,native);
}
}
