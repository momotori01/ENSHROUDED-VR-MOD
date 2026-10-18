#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace evr::vfx {
// The game's "vfx_write_decals_sort_data_back" compute shader (June 187d10) and what stands in for it
// (shaders/vfx_decal_writeback.comp): the same copy, and for a payload of five words the copy moved into another
// camera's space. keen::ForwardDecalData is 112 bytes: decalTransform (three rows of four floats) at +0,
// worldBoundingSphere (x, y, z, radius) at +48 (in 1.0 a world place: left as it is); the rest is left as it is.
// 1.0 (shader capture game-20261016-061640, shader_5c2f916ec78fc508): 2016 bytes, the sort data 8 bytes {key, index},
// groups of 64, the same decal record. Its pass sits inside the VFX stage's writeback function 0x1aed70 (the payload's
// allocation at 0x1af08a, the target at parameters+0x1c8, the count +0x248, the counter's id +0x24c), which the eye runs
// again with only that pass open (hooks/vfx_decal_rebase.hpp). June's: 2072 bytes, 4da2e9ad....
inline constexpr const char* decalWritebackSha="a4627b86574893748bbebf365ddae87962c7f80da8b24cf729585413930c02b2";
inline constexpr uint32_t decalWritebackBytes=2016;
inline constexpr uint32_t decalWords=28;
// 'EVRD': the transform takes a camera-relative position to the decal's space. 'EVRd': its fourth column is the
// decal's camera-relative place.
inline constexpr uint32_t decalMarkerInto=0x45565244u,decalMarkerPlace=0x45565264u;
using DecalRecord=std::array<uint32_t,decalWords>;
inline float decalFloat(uint32_t bits){float f;std::memcpy(&f,&bits,4);return f;}
inline uint32_t decalBits(float f){uint32_t bits;std::memcpy(&bits,&f,4);return bits;}
// delta = the camera position the record was written for - the camera position it is read with, in metres.
inline DecalRecord rebaseDecal(DecalRecord d,const std::array<float,3>& delta,uint32_t marker){
    if(marker!=decalMarkerInto&&marker!=decalMarkerPlace)return d;
    for(unsigned row=0;row<3;++row){
        const float w=decalFloat(d[row*4+3]);
        const float moved=marker==decalMarkerInto
            ?w-(decalFloat(d[row*4])*delta[0]+decalFloat(d[row*4+1])*delta[1]+decalFloat(d[row*4+2])*delta[2])
            :w+delta[row];
        d[row*4+3]=decalBits(moved);
    }
    // 1.0: the bounding sphere (+48) is a world place - its centre is the camera plus the decal's camera-relative centre
    // (RenderDoc ha, 2026-10-17) - and stays. June's was camera-relative and moved with delta.
    return d;
}
// The payload of the eye's writeback; the game writes the first word (the counter's byte offset) itself.
inline std::array<uint32_t,5> decalPayload(const std::array<float,3>& delta,uint32_t marker){
    return {0u,marker,decalBits(delta[0]),decalBits(delta[1]),decalBits(delta[2])};
}
}
