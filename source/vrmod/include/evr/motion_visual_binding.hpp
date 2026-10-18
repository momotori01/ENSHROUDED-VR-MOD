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
// Slots never evict a live GPU resource. Native Scene disposal releases them.
struct RetainedVisual {VisualIdentity identity{};uint32_t clone=0;};
constexpr size_t retainedVisualCapacity=24;
struct VisualSelection {bool valid=false;size_t slot=0;bool create=false;};
inline VisualSelection selectRetainedVisual(const std::array<RetainedVisual,retainedVisualCapacity>& owned,
                                           const VisualIdentity& source,bool sourceValidated){
 if(!sourceValidated||!source.item||!source.source||!source.child||!source.resource||source.guid==std::array<uint32_t,4>{})return {};
 size_t empty=owned.size();
 for(size_t i=0;i<owned.size();++i){
  const auto& e=owned[i];if(!e.clone){if(empty==owned.size())empty=i;continue;}
  if(e.identity.item==source.item&&e.identity.resource==source.resource&&e.identity.guid==source.guid)return {true,i,false};
 }
 return empty<owned.size()?VisualSelection{true,empty,true}:VisualSelection{};
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
