#pragma once
#include <evr/head_spirv_patch.hpp>
#include <cmath>
namespace evr::body {
constexpr uint32_t marker=0x45564243,maximumModels=24;
struct Payload {uint32_t tag=marker,count=0;float distance=0;uint32_t reserved=0;std::array<uint32_t,maximumModels> models{};};
static_assert(sizeof(Payload)==112);
inline Payload pack(std::vector<uint32_t> ids,float distance){
    if(!std::isfinite(distance)||distance<.01f||distance>1.f)throw std::runtime_error("body experimental distance bounds");
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    if(ids.empty()||ids.size()>maximumModels||ids.back()>=0x1000000)throw std::runtime_error("body model index bounds");
    Payload p;p.distance=distance;p.count=uint32_t(ids.size());std::copy(ids.begin(),ids.end(),p.models.begin());return p;
}
// Appearance uses the same fixed immutable allocation layout, a distinct tag,
// word2/word3 hold two owned instance+1 values (zero means absent).
// The distance field is only a byte container under this distinct tag; no clip.
constexpr uint32_t appearanceMarker=0x4556484f,appearanceShieldMarker=0x45564850;
inline bool appearanceTag(uint32_t tag){return tag==appearanceMarker||tag==appearanceShieldMarker;}
inline uint32_t appearanceShield(const Payload& p){return p.tag==appearanceShieldMarker?p.models.back():0;}
inline uint32_t appearanceExtra(const Payload& p){uint32_t value=0;std::memcpy(&value,&p.distance,4);return value;}
inline Payload packAppearance(std::vector<uint32_t> ids,uint32_t handPlusOne,uint32_t extraPlusOne=0,uint32_t shieldPlusOne=0){
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    if(ids.empty()||ids.size()>(shieldPlusOne?maximumModels-1:maximumModels)||ids.back()>=0x1000000||handPlusOne>0x1000000||extraPlusOne>0x1000000||
       (extraPlusOne&&(extraPlusOne==handPlusOne||std::find(ids.begin(),ids.end(),extraPlusOne-1)!=ids.end()))||
       (handPlusOne&&std::find(ids.begin(),ids.end(),handPlusOne-1)!=ids.end())||shieldPlusOne>0x1000000||
       (shieldPlusOne&&(shieldPlusOne==handPlusOne||shieldPlusOne==extraPlusOne||std::find(ids.begin(),ids.end(),shieldPlusOne-1)!=ids.end())))throw std::runtime_error("appearance ownership bounds/overlap");
    Payload p;p.tag=shieldPlusOne?appearanceShieldMarker:appearanceMarker;p.count=uint32_t(ids.size());std::memcpy(&p.distance,&extraPlusOne,4);p.reserved=handPlusOne;std::copy(ids.begin(),ids.end(),p.models.begin());if(shieldPlusOne)p.models.back()=shieldPlusOne;return p;
}
inline constexpr const char* depthSha="e7ddf2d6f02a752e57e06f9a9ca780421c30c711ccfe0dcbadab6364313d8934";
inline constexpr const char* colorSha="fd8ea5dffef32fd659442ec9fce7011f4ede8703e1a34380c10eac39c361ff8a";
// Only an original transformation recipe. Native bytecode is supplied locally.
// The two verified native writers zero their padding before every upload. Zero
// means inactive and branches around all physical-pointer memory accesses.
inline std::vector<uint32_t> patch(const uint32_t* source,size_t bytes,bool color){
    const auto expectedBytes=color?55756u:31688u,originalBound=color?2109u:1203u;
    if(bytes!=expectedBytes||head::sha256(source,bytes)!=(color?colorSha:depthSha))throw std::runtime_error("unreviewed body vertex shader");
    std::vector<uint32_t> w(source,source+bytes/4);
    if(w[0]!=0x07230203||w[1]!=0x10500||w[3]!=originalBound)throw std::runtime_error("body SPIR-V header");
    uint32_t u=0,f=0,b=0,u2=0,position=0,uniformUint=0;
    for(size_t at=5;at<w.size();at+=w[at]>>16){const auto n=w[at]>>16,op=w[at]&65535;
        if(!n||at+n>w.size())throw std::runtime_error("body malformed instruction");
        if(op==21&&n==4&&w[at+2]==32&&w[at+3]==0)u=w[at+1];
        if(op==22&&n==3&&w[at+2]==32)f=w[at+1];
        if(op==20)b=w[at+1];
        if(op==71&&n==4&&w[at+2]==11&&w[at+3]==0)position=w[at+1];
    }
    for(size_t at=5;at<w.size();at+=w[at]>>16){const auto op=w[at]&65535;
        if(op==23&&w[at+2]==u&&w[at+3]==2)u2=w[at+1];
        if(op==32&&w[at+2]==2&&w[at+3]==u)uniformUint=w[at+1];
    }
    if(!u||!f||!b||!u2||!position||!uniformUint)throw std::runtime_error("body native types absent");
    uint32_t next=w[3];std::vector<uint32_t> annotations,declarations,body;
    auto emit=[](std::vector<uint32_t>& to,uint32_t op,std::initializer_list<uint32_t> values){to.push_back((uint32_t(values.size()+1)<<16)|op);to.insert(to.end(),values.begin(),values.end());};
    auto constant=[&](uint32_t type,uint32_t bits){auto id=next++;emit(declarations,43,{type,id,bits});return id;};
    std::array<uint32_t,33> c{};for(uint32_t n=0;n<c.size();n++)c[n]=constant(u,n);
    const auto one=constant(f,0x3f800000),minimum=constant(f,0x3c23d70a),tag=constant(u,marker);
    const auto u64=next++;emit(declarations,21,{u64,64,0});
    const auto words=next++;emit(declarations,28,{words,u,c[28]});emit(annotations,71,{words,6,4});
    const auto payload=next++;emit(declarations,30,{payload,words});emit(annotations,71,{payload,2});emit(annotations,72,{payload,0,35,0});
    const auto pointer=next++;emit(declarations,32,{pointer,5349,payload});
    const auto wordPointer=next++;emit(declarations,32,{wordPointer,5349,u});
    const auto uniformPair=next++;emit(declarations,32,{uniformPair,2,u2});
    const auto clipArray=next++;emit(declarations,28,{clipArray,f,c[1]});
    const auto clipPointer=next++;emit(declarations,32,{clipPointer,3,clipArray});
    const auto clip=next++;emit(declarations,59,{clipPointer,clip,3});emit(annotations,71,{clip,11,3});
    if(color)emit(annotations,72,{81,32,35,632});
    auto unary=[&](uint32_t op,uint32_t type,uint32_t x){auto id=next++;emit(body,op,{type,id,x});return id;};
    auto binary=[&](uint32_t op,uint32_t type,uint32_t x,uint32_t y){auto id=next++;emit(body,op,{type,id,x,y});return id;};
    auto extract=[&](uint32_t type,uint32_t value,uint32_t member){auto id=next++;emit(body,81,{type,id,value,member});return id;};
    uint32_t pair=0,lo=0,hi=0;
    if(color){auto ptr=next++;emit(body,65,{uniformPair,ptr,24,c[32]});pair=unary(61,u2,ptr);lo=extract(u,pair,0);hi=extract(u,pair,1);}
    else{std::array<uint32_t,2> halves{};for(unsigned n=0;n<2;n++){auto ptr=next++;emit(body,65,{uniformUint,ptr,15,c[22+n]});halves[n]=unary(61,u,ptr);}lo=halves[0];hi=halves[1];pair=next++;emit(body,80,{u2,pair,lo,hi});}
    const auto present=binary(166,b,binary(171,b,lo,c[0]),binary(171,b,hi,c[0]));
    const auto load=next++,end=next++;emit(body,247,{end,0});emit(body,250,{present,load,end});emit(body,248,{load});
    const auto address=unary(124,u64,pair),ptr=unary(120,pointer,address);std::array<uint32_t,28> values{};
    for(unsigned n=0;n<values.size();n++){auto word=next++;emit(body,65,{wordPointer,word,ptr,c[0],c[n]});values[n]=next++;emit(body,61,{u,values[n],word,2,4});}
    auto valid=binary(167,b,binary(170,b,values[0],tag),binary(167,b,binary(172,b,values[1],c[0]),binary(178,b,values[1],c[24])));
    const auto distance=unary(124,f,values[2]);valid=binary(167,b,valid,binary(167,b,binary(190,b,distance,minimum),binary(188,b,distance,one)));
    uint32_t any=0;for(unsigned n=0;n<maximumModels;n++){
        const auto hit=binary(167,b,binary(170,b,color?898:541,values[4+n]),binary(172,b,values[1],c[n]));any=n?binary(166,b,any,hit):hit;
    }
    const auto selected=binary(167,b,valid,any),nativePosition=color?2108u:1202u;
    const auto plane=binary(131,f,extract(f,nativePosition,3),distance),value=next++;
    emit(body,169,{f,value,selected,plane,one});emit(body,249,{end});emit(body,248,{end});
    const auto result=next++,array=next++;emit(body,245,{f,result,one,color?2057u:1177u,value,load});
    emit(body,80,{clipArray,array,result});emit(body,62,{clip,array});
    std::vector<uint32_t> out(w.begin(),w.begin()+5);out[3]=next;
    emit(out,17,{32});emit(out,17,{11});emit(out,17,{5347});
    bool annotated=false,declared=false,inserted=false,appended=false,interface=false;uint32_t label=0;
    for(size_t at=5;at<w.size();at+=w[at]>>16){const auto n=w[at]>>16,op=w[at]&65535;
        if(op==71&&!annotated){out.insert(out.end(),annotations.begin(),annotations.end());annotated=true;}
        if(op==54&&!declared){out.insert(out.end(),declarations.begin(),declarations.end());declared=true;}
        if(op==248)label=w[at+1];
        if(op==62&&n==3&&w[at+1]==position&&w[at+2]==nativePosition){
            if(inserted||label!=(color?2057u:1177u))throw std::runtime_error("body Position/CFG changed");
            out.insert(out.end(),body.begin(),body.end());inserted=true;
        }
        if(op==15&&w[at+1]==0){out.push_back(((n+1)<<16)|op);out.insert(out.end(),w.begin()+at+1,w.begin()+at+n);out.push_back(clip);interface=true;}
        else if(op==14){if(w[at+1]!=0||w[at+2]!=1)throw std::runtime_error("body memory model differs");emit(out,14,{5348,1});}
        else if(color&&op==30&&w[at+1]==81){if(n!=34)throw std::runtime_error("color native members changed");out.push_back(((n+1)<<16)|op);out.insert(out.end(),w.begin()+at+1,w.begin()+at+n);out.push_back(u2);appended=true;}
        else out.insert(out.end(),w.begin()+at,w.begin()+at+n);
    }
    if(!annotated||!declared||!inserted||!interface||(color&&!appended))throw std::runtime_error("body transform boundary absent");return out;
}
}
