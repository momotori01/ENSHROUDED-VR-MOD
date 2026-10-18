#pragma once
#include <evr/melee_session.hpp>
#include <evr/world_lifetime.hpp>
namespace evr::motion {
// Caller holds the World book's mutex while binding, advancing the coordinator,
// and publishing its values. Recheck the saved token at a later consumer.
// This makes a COPY; it never changes the native Actor or grants permissions.
// This adapter is CPU-tested; the current finite native probe still uses serial0.
template<size_t N> MeleeNative bindMeleeWorld(const WorldLifetimes<N>& worlds,WorldToken token,MeleeNative native,uint8_t expectedKind){
 const auto state=worlds.snapshot(token);
 if(!worlds.alive(token)||expectedKind>2||state.identity.kind!=expectedKind||state.identity.world!=native.world){native.known=false;native.worldSerial=0;return native;}
 native.worldSerial=token.serial;return native;
}
}
