#pragma once
#include <cstdlib>
#include <cstring>
// The water's screen-space reflection (2026-10-20, user: "the water reflection differs between the eyes"; report
// 20261019-140137: a far pond reflects the trees in the left eye and only the sky in the right).
// The game's reflection search (RenderDoc rd-20261020-ssr, events 8791 left / 13650 right, the same module) is the
// hierarchical-depth march of a hybrid reflection: a hit whose screen-edge fade is below 0.99 is not taken from the
// picture but sent to the hardware ray pass (8800, RayQuery against the simplified world). The fade band is
// 0.05 x (height / width) of the screen - the water buffer is half width (523 x 1046), so the band is 10 % of the width
// on each side in VR (a flat screen's buffer gives about 5.6 %). A pond near one eye's edge (8 % in the right eye,
// 24 % in the left in that report) then takes the simplified world in one eye only (guess: without the trees, the sky).
// The one constant 0.05 (only used for that band) is scaled: EVR_SSR_EDGE_PERCENT of the game's band, default 25
// (2.5 % of the width, 1.25 % of the height), 1..100; 100 keeps the game's shader.
namespace ssrEdge {
inline constexpr const char* sourceSha="8026df3c9cfa016f58a6d0d99f68e2cfd34c8a3cd04310b65399a6fcc662a2f2";   // 1.0 build 25988036
inline constexpr size_t sourceBytes=24952;
inline constexpr size_t constantWord=1055;   // OpConstant %float(0x64) %0xa4 0.05
inline float percent() {
    const char* v=std::getenv("EVR_SSR_EDGE_PERCENT");
    if(!v||!*v)return 25.f;
    char* end=nullptr;const float p=std::strtof(v,&end);
    if(end==v||!(p>=1.f)||p>1000.f)return 25.f;   // 0 would make smoothstep(0, 0, x) undefined; above 100 only to test (1000: every hit to the ray pass)
    return p;
}
inline std::vector<uint32_t> patch(const VkShaderModuleCreateInfo& source) {
    if(source.codeSize!=sourceBytes||source.flags||source.pNext)return {};
    const float p=percent();if(p==100.f)return {};
    if(head::sha256(source.pCode,source.codeSize)!=sourceSha)return {};
    std::vector<uint32_t> code(source.pCode,source.pCode+sourceBytes/4);
    if(code[0]!=0x07230203||code[constantWord-3]!=0x0004002bu||code[constantWord-2]!=0x64u||code[constantWord-1]!=0xa4u||code[constantWord]!=0x3d4ccccdu)
        throw std::runtime_error("SSR edge constant not where reviewed");
    const float value=.05f*p/100.f;std::memcpy(&code[constantWord],&value,4);
    return code;
}
}
