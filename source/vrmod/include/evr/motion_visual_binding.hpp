#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace evr::motion {
// Rendering-only identity selection. Never authorizes an attack, releases a
// GPU resource or treats a same GUID at a new resource address as interchangeable.
struct VisualIdentity {
 uint32_t item=0,source=0,child=0;uintptr_t resource=0;std::array<uint32_t,4> guid{};
};
// Slots are released by the native Scene disposal, and otherwise only when all are taken (2026-11-23, a player's four-hour
// session: 19 slots went to the survivors chosen while placing them at the base, the pickaxe found none and stayed out of the
// hand - mining fell back to the flat game's): then the least recently used slot that is not the one shown is handed back
// (motion_visual_bridge.hpp evictRetained removes its clone as the Scene disposal does) and taken anew. used: when the slot
// was last selected (a counter of the bridge).
struct RetainedVisual {VisualIdentity identity{};uint32_t clone=0;uint64_t used=0;};
constexpr size_t retainedVisualCapacity=24;
struct VisualSelection {bool valid=false;size_t slot=0;bool create=false;bool evict=false;};
inline VisualSelection selectRetainedVisual(const std::array<RetainedVisual,retainedVisualCapacity>& owned,
                                           const VisualIdentity& source,bool sourceValidated,uint32_t shownClone=0,
                                           size_t usable=retainedVisualCapacity){   // usable: the first slots only (a test)
 if(!sourceValidated||!source.item||!source.source||!source.child||!source.resource||source.guid==std::array<uint32_t,4>{})return {};
 const size_t count=usable<1?1:usable>owned.size()?owned.size():usable;
 size_t empty=count;
 for(size_t i=0;i<count;++i){
  const auto& e=owned[i];if(!e.clone){if(empty==count)empty=i;continue;}
  if(e.identity.item==source.item&&e.identity.resource==source.resource&&e.identity.guid==source.guid)return {true,i,false};
 }
 if(empty<count)return VisualSelection{true,empty,true,false};
 size_t victim=count;
 for(size_t i=0;i<count;++i)if(owned[i].clone&&owned[i].clone!=shownClone&&(victim==count||owned[i].used<owned[victim].used))victim=i;
 return victim<count?VisualSelection{true,victim,true,true}:VisualSelection{};
}
enum class VisualBinding {Suspend,Create,Update,Rebind};
inline VisualBinding visualBinding(const VisualIdentity& source,const VisualIdentity& retained,
                                  uint32_t calibratedItem,const std::array<uint32_t,4>& calibratedGuid,
                                  bool sourceValidated,bool cloneOwned){
 if(!sourceValidated||!calibratedItem||source.item!=calibratedItem||!source.source||!source.child||!source.resource||
    source.guid!=calibratedGuid||calibratedGuid==std::array<uint32_t,4>{})return VisualBinding::Suspend;
 if(!cloneOwned)return VisualBinding::Create;
 if(source.item!=retained.item||source.resource!=retained.resource||source.guid!=retained.guid)return VisualBinding::Suspend;
 return source.source==retained.source&&source.child==retained.child?VisualBinding::Update:VisualBinding::Rebind;
}
}
