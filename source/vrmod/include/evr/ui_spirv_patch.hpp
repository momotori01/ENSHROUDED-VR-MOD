#pragma once
#include <evr/head_spirv_patch.hpp>
#include <vector>
#include <string>
#include <stdexcept>

namespace evr::ui {
inline constexpr const char* vertexSha="a5adbece8bd91ca9e005dcacb0afe2ca2412bf9c70d6979ea56513ff3bf154ef";
inline constexpr const char* fragmentSha="d97899fedc56ecf5d2c436d700b7c7cf2e0979707a29c6cb2fc93b7fc242fc55";
inline constexpr const char* fontSha="87bc6f1b3ca5bd485620a760ae0d7dc903ba16d11d7c366b8b4ba389a369d41d";
// 1.0 (2026-10-16): the same three, rebuilt - vertex 1632, ordinary 2704 (texture x premultiplied colour, a 1/255 dither),
// font 3100 bytes (the SDF text, premultiplied); captured in game-20261016-061640 as shader_8abb09cd21b975ca /
// shader_d2e5010b0829ae7 / shader_a2b2e43844b532be. Unreviewed, every HUD and menu frame had unknown draws only and the
// headset showed no UI at all (headset run 20261016-134128: "the menu did not come"; every Mock since 1.0).
inline constexpr const char* vertexSha1_0="778bcacb9c12390aa5adc0671ed3bfc0699e533a3dee659ef221be7b9cc8b6f9";
inline constexpr const char* fragmentSha1_0="2a5fa9d3d31b3705d2a7b2e0623065fe4082fe93a9daa12488b87b9d20858d3b";
inline constexpr const char* fontSha1_0="92652bd01005f0ff230204af00fc44fb87c001a3b2d3a9b281309f274719b467";
inline bool reviewedVertex(const std::string& sha){return sha==vertexSha||sha==vertexSha1_0;}
inline bool reviewedFragment(size_t bytes,const std::string& sha){
    return ((bytes==2640||bytes==3072)&&(sha==fragmentSha||sha==fontSha))||(bytes==2704&&sha==fragmentSha1_0)||(bytes==3100&&sha==fontSha1_0);
}
// World map tab (2026-10-06): three fragment shaders drawn inside the HUD pass (entry fs_main, their own vertex shaders).
// The first two use the reviewed premultiplied source-over blend; the third uses straight alpha and is cloned with
// a premultiplied output (Premultiply) and source-over blend, which keeps the native colour result.
struct MapFragment {size_t bytes;const char* sha;bool straightAlpha;};
inline constexpr MapFragment mapFragments[]={
    {1784,"0b8567817c4ac31ac4ad58c908e4e565427f1d3d8edaaadd11f03c4ea8782d39",false},
    {18648,"0abcf0c894e64c81854ee114211980b22d0bbe5a43b1d0bd22e2ff49222d4b69",false},
    {2308,"07521e3aa2dcdc881b39dd4a1cccb2573f0a8e0d627d4c4030f15113cdd37b4d",true},
    // 1.0: a gradient fill of the HUD (premultiplied vertex colour x a smoothstep ramp, a dither), with its own vertex shader
    // (1420 bytes); shader_67da6760e6488d85 of game-20261016-061640.
    {1832,"6f049a72cd1b5280381c02ca9f39577c3a39c03c54e6face974de76a338a6ea4",false},
    // 1.0: the map's shader (June's 18648 above, rebuilt), with its own vertex shader (1456 bytes, 5e12e9bf...) and the
    // reviewed premultiplied source-over blend; shader_69fb582b295414e0 of game-20261017-035316. Headset report 21 of
    // 2026-10-17 (the menu could not be seen): one of the unknown pipelines of that menu.
    {18612,"147d11eda00c683ae80c5c3723bee7f9caa5f2cc465e6a9d32566bad76df8d76",false},
    // 1.0: the other unknown pipeline of that menu (2026-10-17): a shadow of the menu's shapes - the colour of a storage
    // buffer times the sum of two offset samples of a mask, alpha 1 - a second mask, times the vertex alpha, a 1/255
    // dither; straight alpha, its blend keeps the target's alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA for the colour, ZERO /
    // ONE for alpha). Its own vertex shader (1632 bytes, 6ce6526b...); shader_641297d477370798 of game-20261017-035316.
    {2400,"edb19a2ece2da179167a6dceb1833a46543958608ae575ed6232cce6a8ca34c2",true}};
inline const MapFragment* reviewedMapFragment(size_t bytes,const std::string& sha){
    for(const auto& m:mapFragments)if(m.bytes==bytes&&sha==m.sha)return &m;
    return nullptr;
}
enum class FragmentOutput {Duplicate,AdditiveAlphaZero,Premultiply};

// A transformation of a verified local shader, not a bundled game asset.
// Preserve native location 0 and duplicate its final value at location 1.
inline std::vector<uint32_t> duplicateFragmentOutput(const uint32_t* source,size_t bytes,FragmentOutput mode){
    const bool additive=mode==FragmentOutput::AdditiveAlphaZero,premultiply=mode==FragmentOutput::Premultiply;
    if(!source||!bytes||bytes%4)throw std::runtime_error("UI shader size");
    const auto hash=head::sha256(source,bytes);
    const auto* map=reviewedMapFragment(bytes,hash);
    if(!map&&!reviewedFragment(bytes,hash))throw std::runtime_error("unreviewed UI shader");
    if(premultiply&&!(map&&map->straightAlpha))throw std::runtime_error("premultiply is reviewed only for the straight-alpha map shader");
    const size_t count=bytes/4;
    if(source[0]!=0x07230203||source[1]!=0x10500||!source[3]||source[3]>=0x3fffff)
        throw std::runtime_error("UI SPIR-V header");
    uint32_t output=0,pointerType=0;unsigned entries=0,outputs=0,stores=0,locations=0;
    for(size_t p=5;p<count;){
        const auto n=source[p]>>16,op=source[p]&0xffff;
        if(!n||n>count-p)throw std::runtime_error("UI SPIR-V instruction");
        if(op==15){if(n<5||source[p+1]!=4)throw std::runtime_error("UI entry point");++entries;}
        if(op==59&&n==4&&source[p+3]==3){pointerType=source[p+1];output=source[p+2];++outputs;}
        p+=n;
    }
    if(entries!=1||outputs!=1)throw std::runtime_error("UI output interface");
    for(size_t p=5;p<count;){
        const auto n=source[p]>>16,op=source[p]&0xffff;
        if(op==71&&n==4&&source[p+1]==output&&source[p+2]==30&&source[p+3]==0)++locations;
        if(op==62&&n>=3&&source[p+1]==output){if(n!=3)throw std::runtime_error("UI store operands");++stores;}
        p+=n;
    }
    if(locations!=1||stores!=1)throw std::runtime_error("UI final output store");
    uint32_t vectorType=0,floatType=0,zero=0;
    for(size_t p=5;p<count;){
        const auto n=source[p]>>16,op=source[p]&0xffff;
        if(op==32&&n==4&&source[p+1]==pointerType&&source[p+2]==3)vectorType=source[p+3];
        p+=n;
    }
    for(size_t p=5;p<count;){
        const auto n=source[p]>>16,op=source[p]&0xffff;
        if(op==23&&n==4&&source[p+1]==vectorType&&source[p+3]==4)floatType=source[p+2];
        p+=n;
    }
    bool float32=false;
    for(size_t p=5;p<count;){
        const auto n=source[p]>>16,op=source[p]&0xffff;
        if(op==22&&n==3&&source[p+1]==floatType&&source[p+2]==32)float32=true;
        if(op==43&&n==4&&source[p+1]==floatType&&source[p+3]==0)zero=source[p+2];
        p+=n;
    }
    if(!vectorType||!float32||!zero)throw std::runtime_error("UI float4 output/zero constant");
    const uint32_t extra=source[3],alphaZeroValue=extra+1,alphaValue=extra+1,scaledValue=extra+2,premultipliedValue=extra+3;
    std::vector<uint32_t> result(source,source+5);result[3]=extra+(additive?2:premultiply?4:1);
    bool decorated=false,declared=false;
    for(size_t p=5;p<count;){
        const auto n=source[p]>>16,op=source[p]&0xffff;
        if(!decorated&&op>=19&&op<=39){result.insert(result.end(),{0x40047,extra,30,1});decorated=true;}
        if(!declared&&op==54){result.insert(result.end(),{0x4003b,pointerType,extra,3});declared=true;}
        if(premultiply&&op==62&&n==3&&source[p+1]==output){
            // P = vec4(V.rgb*V.a, V.a), stored at both outputs (blend becomes premultiplied source-over).
            const auto v=source[p+2];
            result.insert(result.end(),{0x50051,floatType,alphaValue,v,3});              // OpCompositeExtract
            result.insert(result.end(),{0x5008e,vectorType,scaledValue,v,alphaValue});   // OpVectorTimesScalar
            result.insert(result.end(),{0x60052,vectorType,premultipliedValue,alphaValue,scaledValue,3}); // OpCompositeInsert
            result.insert(result.end(),{0x3003e,output,premultipliedValue,0x3003e,extra,premultipliedValue});
            p+=n;continue;
        }
        const auto start=result.size();result.insert(result.end(),source+p,source+p+n);
        if(op==15){result[start]=((n+1)<<16)|15;result.push_back(extra);}
        if(op==62&&n==3&&source[p+1]==output){
            // Native additive RGB/alpha stays unchanged at output 0. At output 1,
            // ONE+ONE adds RGB but preserves accumulated coverage (src alpha=0).
            if(additive)result.insert(result.end(),{0x60052,vectorType,alphaZeroValue,zero,source[p+2],3});
            result.insert(result.end(),{0x3003e,extra,additive?alphaZeroValue:source[p+2]});
        }
        p+=n;
    }
    if(!decorated||!declared)throw std::runtime_error("UI declaration boundary");
    return result;
}
inline std::vector<uint32_t> duplicateFragmentOutput(const uint32_t* source,size_t bytes,bool additive=false){
    return duplicateFragmentOutput(source,bytes,additive?FragmentOutput::AdditiveAlphaZero:FragmentOutput::Duplicate);
}
}
