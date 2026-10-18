#pragma once
#include <cstdint>
namespace evr::game {
// Copied metadata only: no game pointer, Root or HMD Pose is modified.
struct BodyMotionSample {uint64_t number=0,milliseconds=0,state=0;uint32_t entity=0;bool known=false;};
inline bool bodyMotionOverride(const BodyMotionSample& s,uint32_t entity,uint64_t entryMs,bool firstPerson,bool synthetic) noexcept {
 if(!firstPerson||synthetic||!s.known||!s.number||!s.entity||s.entity!=entity||!s.milliseconds||entryMs<s.milliseconds||entryMs-s.milliseconds>200)return false;
 if(s.state&(1ull<<7))return false; // dead is not a validated motion override
 return (s.state&((1ull<<1)|(1ull<<3)|(1ull<<32)|(1ull<<37)))!=0;
}
}
