#pragma once
#include <evr/head_visibility.hpp>
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <string>
#pragma comment(lib,"bcrypt.lib")
namespace evr::head {
// 1.0 (build 25936191, 2026-10-16): the culling compute shader is shader_5ee5962523096f50 (22228 bytes, bound 817;
// June 24704 bytes, SHA 04ceb2a4..., bound 913). Its constant block is unchanged (19 members, 16..18 the unused
// padding at 0x1024..0x102f, CB 0x1030 bytes written by acff0 at ae47e); the SSA IDs moved (June in brackets).
inline constexpr const char* sourceSha="79a390a311bb3964bd92d0bafb70f924895afb3675997f4ee80b6b43588afc49";
inline constexpr size_t sourceBytes=22228;
inline constexpr uint32_t sourceBound=817;
namespace ids {
constexpr uint32_t u=208/*229*/,b=240/*259*/,zero=223/*243*/,one=213/*233*/,uniformWord=276/*298*/,constants=4/*4*/,
    invocation=304/*331*/,subview=640/*745*/,visibility=682/*780*/,visibleSelect=683/*781*/,
    // hands-only (appearance) form: the instance record's block after the invocation bound check, its mask word, the
    // CB mask and their AND (June 337, 345, 360, 361; the record lost one member in front: mask 17 -> 16)
    instanceBlock=310,instanceMask=318,cbMask=332,maskAnd=333;
}
inline std::string sha256(const void* bytes,size_t size){
    if(size>UINT32_MAX)throw std::runtime_error("shader hash size");
    BCRYPT_ALG_HANDLE alg=nullptr;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider");
    struct Close{BCRYPT_ALG_HANDLE a;~Close(){BCryptCloseAlgorithmProvider(a,0);}}close{alg};
    std::array<uint8_t,32> digest{};
    if(BCryptHash(alg,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<void*>(bytes)),ULONG(size),digest.data(),ULONG(digest.size()))<0)
        throw std::runtime_error("SHA256 hash");
    std::string result;constexpr char hex[]="0123456789abcdef";
    for(auto c:digest){result+=hex[c>>4];result+=hex[c&15];}return result;
}
// Own transformation recipe only. The caller supplies its local game shader;
// never embed or distribute the original/generated proprietary SPIR-V.
inline std::vector<uint32_t> patch(const uint32_t* source,size_t bytes){
    if(bytes!=sourceBytes||sha256(source,bytes)!=sourceSha)throw std::runtime_error("unreviewed culling shader");
    std::vector<uint32_t> w(source,source+bytes/4);
    if(w[0]!=0x07230203||w[1]!=0x10500||w[3]!=sourceBound)throw std::runtime_error("culling SPIR-V header");
    using namespace ids;
    // Pinned SSA IDs: invocation, subview, native visibility (and the select of it).
    // Verify the exact def/use sites as well as the full module hash (June words 2103, 2108, 5141, 5330, 5337, 2076).
    auto matches=[&](size_t at,std::initializer_list<uint32_t> values){return at+values.size()<=w.size()&&std::equal(values.begin(),values.end(),w.begin()+at);};
    constexpr size_t selectAt=4714,functionAt=2010;
    if(!matches(2029,{0x50041,297,303,3,206})||!matches(2034,{0x4003d,u,invocation,303})||
       !matches(4480,{0x50051,u,subview,638,1})||!matches(4707,{0x700f5,b,visibility,651,655,681,659})||
       !matches(selectAt,{0x600a9,u,visibleSelect,visibility,one,zero})||!matches(functionAt,{0x50036,269,2,0,270}))
        throw std::runtime_error("culling SSA boundary differs");
    uint32_t next=w[3];std::vector<uint32_t> declarations,body;
    auto emit=[](std::vector<uint32_t>& dest,uint32_t opcode,std::initializer_list<uint32_t> operands){
        dest.push_back((uint32_t(operands.size()+1)<<16)|opcode);dest.insert(dest.end(),operands.begin(),operands.end());};
    auto constant=[&](uint32_t value){auto id=next++;emit(declarations,43,{u,id,value});return id;};
    auto unary=[&](uint32_t op,uint32_t type,uint32_t a){auto id=next++;emit(body,op,{type,id,a});return id;};
    auto binary=[&](uint32_t op,uint32_t type,uint32_t a,uint32_t c){auto id=next++;emit(body,op,{type,id,a,c});return id;};
    const auto shift=constant(16),mask=constant(0xffff),tagMask=constant(0xfff8),tagValue=constant(marker),countMask=constant(7);
    std::array<uint32_t,3> words{};
    for(unsigned n=0;n<3;n++){
        const auto member=constant(16+n),ptr=next++;
        emit(body,65,{uniformWord,ptr,constants,member});words[n]=unary(61,u,ptr);
    }
    const auto tag=binary(194,u,words[2],shift),count=binary(199,u,tag,countMask);
    auto enabled=binary(170,b,binary(199,u,tag,tagMask),tagValue);
    enabled=binary(167,b,enabled,binary(172,b,count,zero));
    enabled=binary(167,b,enabled,binary(174,b,constant(5),count));
    uint32_t any=0;
    for(unsigned n=0;n<5;n++){
        const auto value=binary(199,u,n%2?binary(194,u,words[n/2],shift):words[n/2],mask);
        const auto hit=binary(167,b,binary(170,b,invocation,value),binary(172,b,count,constant(n)));
        any=n?binary(166,b,any,hit):hit;
    }
    const auto exclude=binary(167,b,binary(167,b,enabled,any),binary(170,b,subview,zero));
    const auto visible=binary(167,b,visibility,unary(168,b,exclude));
    emit(body,169,{u,visibleSelect,visible,one,zero});
    w.erase(w.begin()+selectAt,w.begin()+selectAt+6);w.insert(w.begin()+selectAt,body.begin(),body.end());
    w.insert(w.begin()+functionAt,declarations.begin(),declarations.end());w[3]=next;return w;
}
}
