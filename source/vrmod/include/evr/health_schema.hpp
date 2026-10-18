#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
namespace evr::motion {
// Copied evidence only; no native lookup, lifetime ownership or damage writes.
// Attribute index is learned from the native metadata, never hardcoded to zero.
struct HealthSchema {
 std::array<uint8_t,20> container{};
 std::array<uint8_t,32> attribute{};
 std::string_view componentName{},attributeName{};
 uint32_t componentId=0,componentBytes=0;
};
inline bool verifiedHealthSchema(const HealthSchema& s){
 constexpr uint32_t healthKey=0x8eb84995;
 uint32_t root=0,offset=0,count=0,metaRoot=0,metaKey=0;uint16_t index=0;
 std::memcpy(&root,s.container.data(),4);std::memcpy(&offset,s.container.data()+8,4);std::memcpy(&count,s.container.data()+12,4);
 std::memcpy(&metaRoot,s.attribute.data(),4);std::memcpy(&metaKey,s.attribute.data()+4,4);std::memcpy(&index,s.attribute.data()+12,2);
 return s.componentId<640&&s.componentBytes==68&&s.componentName=="keen::ecs::Health"&&s.attributeName=="Health"&&
   root==healthKey&&metaRoot==healthKey&&metaKey==healthKey&&(s.container[16]&1)&&offset==36&&count>0&&count<=8&&index<count;
}
}
