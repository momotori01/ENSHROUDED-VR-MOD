#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace evr::water
{
// Build-specific native readback layout, unrelated to HMD image dimensions.
constexpr size_t capacity = 0x8000, packetBytes = 0x8000c;
constexpr std::array<size_t, 3> offsets{0xc, 0x4000c, 0x6000c};
struct Split { uint32_t priority, node; };
struct Packet
{
    uint64_t serial = 0;
    std::array<uint32_t, 3> rawCounts{};
    std::vector<Split> split;
    std::vector<uint32_t> coarsen, retire;
};
inline uint32_t word(const uint8_t* p) { uint32_t v; std::memcpy(&v,p,4); return v; }
inline Packet decode(const uint8_t* data, size_t bytes, uint64_t serial, uint32_t nodes)
{
    if(!data || bytes != packetBytes || !serial || !nodes || nodes > 0x10000000u)
        throw std::invalid_argument("invalid water feedback envelope");
    Packet p; p.serial=serial;
    for(size_t n=0;n<3;n++)p.rawCounts[n]=word(data+n*4);
    for(size_t n=0;n<std::min<size_t>(p.rawCounts[0],capacity);n++) {
        Split s{word(data+offsets[0]+n*8),word(data+offsets[0]+n*8+4)};
        if(s.node>=nodes)throw std::invalid_argument("water split index outside native table");
        p.split.push_back(s);
    }
    for(size_t list=1;list<3;list++)for(size_t n=0;n<std::min<size_t>(p.rawCounts[list],capacity);n++) {
        const auto v=word(data+offsets[list]+n*4);
        if((list==1?v:v&0xfffffffu)>=nodes)throw std::invalid_argument("water feedback index outside native table");
        (list==1?p.coarsen:p.retire).push_back(v);
    }
    return p;
}
struct Merged
{
    uint64_t serial = 0;
    std::array<uint32_t,3> counts{}, overflowSources{};
    size_t omittedSplits = 0;
    std::vector<uint8_t> bytes = std::vector<uint8_t>(packetBytes);
};
inline Merged merge(const std::array<Packet,3>& input)
{
    Merged out;out.serial=input[0].serial;
    std::map<uint32_t,uint32_t> splits;
    for(const auto& p:input) {
        if(!out.serial || p.serial!=out.serial || p.split.size()>capacity || p.coarsen.size()>capacity || p.retire.size()>capacity)
            throw std::invalid_argument("water feedback must share one completed game serial");
        for(size_t n=0;n<3;n++)out.overflowSources[n]+=p.rawCounts[n]>capacity;
        for(const auto& s:p.split)splits[s.node]=std::max(splits[s.node],s.priority);
    }
    std::vector<Split> ordered;ordered.reserve(splits.size());
    for(const auto& s:splits)ordered.push_back({s.second,s.first});
    // Native consumer sorts unsigned priority ascending, then traverses backwards.
    std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.priority!=b.priority?a.priority>b.priority:a.node<b.node;});
    out.counts[0]=uint32_t(std::min(ordered.size(),capacity));
    out.omittedSplits=ordered.size()-out.counts[0];
    for(size_t n=0;n<out.counts[0];n++)std::memcpy(out.bytes.data()+offsets[0]+n*8,&ordered[n],8);
    for(size_t list=1;list<3;list++) {
        auto values=[&](size_t eye)->const std::vector<uint32_t>& {return list==1?input[eye].coarsen:input[eye].retire;};
        std::set<uint32_t> common(values(0).begin(),values(0).end());
        for(size_t eye=1;eye<3;eye++) {
            std::set<uint32_t> next(values(eye).begin(),values(eye).end()), both;
            std::set_intersection(common.begin(),common.end(),next.begin(),next.end(),std::inserter(both,both.end()));
            common.swap(both);
        }
        for(uint32_t value:common) {
            // Generation-tagged retire handles remain exact. Never retire or
            // coarsen a node requested for refinement by any of the three views.
            if(splits.count(list==1?value:value&0xfffffffu))continue;
            std::memcpy(out.bytes.data()+offsets[list]+out.counts[list]*4,&value,4);++out.counts[list];
        }
    }
    std::memcpy(out.bytes.data(),out.counts.data(),12);
    return out;
}
}
