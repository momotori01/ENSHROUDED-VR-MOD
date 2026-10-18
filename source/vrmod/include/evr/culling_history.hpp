#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace evr::culling {
struct ResetRange {uint32_t first,count;};
// The native upload shader resets history to all-visible only for bit31-tagged
// instance updates. Ordinary transform changes preserve temporal history.
inline std::vector<ResetRange> resetRanges(const std::vector<uint32_t>& updates,uint32_t slots){
    if(!slots||slots>65536||updates.size()>65536)throw std::runtime_error("culling reset bounds");
    std::vector<uint32_t> indices;
    for(auto value:updates){const auto index=value&0x7fffffffu;
        if(index>=slots)throw std::runtime_error("culling update index outside history");
        if(value&0x80000000u)indices.push_back(index);
    }
    std::sort(indices.begin(),indices.end());indices.erase(std::unique(indices.begin(),indices.end()),indices.end());
    std::vector<ResetRange> ranges;
    for(auto index:indices){if(!ranges.empty()&&ranges.back().first+ranges.back().count==index)++ranges.back().count;
        else ranges.push_back({index,1});}
    // Conservative all-visible reset bounds graph command growth during mass
    // streaming. It loses one frame of history, not current frustum/LOD tests.
    if(ranges.size()>1024)return {{0,slots}};
    return ranges;
}
}
