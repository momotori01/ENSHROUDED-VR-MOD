#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace evr::head {
// Reviewed native culling CB padding at 0x1024..0x102f. Native construction
// zeroes the entire 4144-byte CB. No binding or constant-buffer size changes.
using Payload=std::array<uint32_t,3>;
constexpr uint32_t marker=0xa5d0; // low three bits carry 1..5 entries
constexpr size_t offset=0x1024,constantBytes=0x1030;
inline Payload pack(std::vector<uint32_t> indices){
    std::sort(indices.begin(),indices.end());indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    if(indices.size()>5||(!indices.empty()&&indices.back()>=65536))throw std::runtime_error("head instance bounds");
    Payload p{};if(indices.empty())return p;
    for(size_t n=0;n<indices.size();n++)p[n/2]|=indices[n]<<(16*(n%2));
    p[2]|=(marker|uint32_t(indices.size()))<<16;return p;
}
inline bool excluded(Payload p,uint32_t instance,uint32_t subview){
    const auto tag=p[2]>>16,count=tag&7;
    if(subview||instance>=65536||(tag&0xfff8)!=marker||!count||count>5)return false;
    for(uint32_t n=0;n<count;n++)if(((p[n/2]>>(16*(n%2)))&65535)==instance)return true;
    return false;
}
}
