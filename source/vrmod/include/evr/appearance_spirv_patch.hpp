#pragma once
#include <evr/body_clip_spirv_patch.hpp>
namespace evr::appearance {
// Native hash + SSA checks; original bytecode is supplied locally, never embedded.
// Only main subview appearance changes. Hidden-by-armour hands use original
// frustum/LOD/occlusion calculations, and retain their original hidden shadows.
inline std::vector<uint32_t> patch(const uint32_t* source,size_t bytes){
 if(bytes!=head::sourceBytes||head::sha256(source,bytes)!=head::sourceSha)throw std::runtime_error("unreviewed appearance culling shader");
 std::vector<uint32_t> w(source,source+bytes/4);if(w[3]!=head::sourceBound)throw std::runtime_error("appearance bound changed");
 // 1.0 (2026-10-16): SSA IDs of the reviewed shader in head_spirv_patch.hpp (June 229, 259, 243, 233, 298, 331, 337, 345, 360, 361, 745, 780, 781).
 using namespace head::ids;
 uint32_t next=w[3];std::vector<uint32_t> ann,decl,begin,mask,finish;
 auto emit=[](std::vector<uint32_t>& a,uint32_t op,std::initializer_list<uint32_t> args){a.push_back((uint32_t(args.size()+1)<<16)|op);a.insert(a.end(),args.begin(),args.end());};
 auto constant=[&](uint32_t v){auto id=next++;emit(decl,43,{u,id,v});return id;};
 std::array<uint32_t,29> c{};for(unsigned n=0;n<c.size();n++)c[n]=constant(n);
 const auto tag=constant(body::appearanceMarker),shieldTag=constant(body::appearanceShieldMarker),handMax=constant(0x1000000),falseId=next++;emit(decl,42,{b,falseId});
 const auto u64=next++;emit(decl,21,{u64,64,0});const auto u2=next++;emit(decl,23,{u2,u,2});
 const auto words=next++;emit(decl,28,{words,u,c[28]});emit(ann,71,{words,6,4});
 const auto structure=next++;emit(decl,30,{structure,words});emit(ann,71,{structure,2});emit(ann,72,{structure,0,35,0});
 const auto ptrType=next++;emit(decl,32,{ptrType,5349,structure});const auto wordType=next++;emit(decl,32,{wordType,5349,u});
 auto unary=[&](std::vector<uint32_t>& a,uint32_t op,uint32_t type,uint32_t x){auto id=next++;emit(a,op,{type,id,x});return id;};
 auto binary=[&](std::vector<uint32_t>& a,uint32_t op,uint32_t type,uint32_t x,uint32_t y){auto id=next++;emit(a,op,{type,id,x,y});return id;};
 std::array<uint32_t,3> cb{};for(unsigned n=0;n<3;n++){auto p=next++;emit(begin,65,{uniformWord,p,constants,c[16+n]});cb[n]=unary(begin,61,u,p);}
 auto present=binary(begin,167,b,binary(begin,166,b,binary(begin,170,b,cb[2],tag),binary(begin,170,b,cb[2],shieldTag)),binary(begin,171,b,binary(begin,197,u,cb[0],cb[1]),zero));
 const auto load=next++,end=next++;emit(begin,247,{end,0});emit(begin,250,{present,load,end});emit(begin,248,{load});
 const auto pair=next++;emit(begin,80,{u2,pair,cb[0],cb[1]});const auto address=unary(begin,124,u64,pair),ptr=unary(begin,120,ptrType,address);
 std::array<uint32_t,28> values{};for(unsigned n=0;n<values.size();n++){auto p=next++;emit(begin,65,{wordType,p,ptr,c[0],c[n]});values[n]=next++;emit(begin,61,{u,values[n],p,2,4});}
 const auto multi=binary(begin,170,b,values[0],shieldTag),limit=next++,shield=next++;
 emit(begin,169,{u,limit,multi,c[23],c[24]});emit(begin,169,{u,shield,multi,values[27],zero});
 auto valid=binary(begin,167,b,binary(begin,170,b,values[0],cb[2]),binary(begin,167,b,binary(begin,172,b,values[1],zero),binary(begin,178,b,values[1],limit)));
 valid=binary(begin,167,b,valid,binary(begin,178,b,shield,handMax));
 valid=binary(begin,167,b,valid,binary(begin,167,b,binary(begin,178,b,values[2],handMax),binary(begin,178,b,values[3],handMax)));
 uint32_t any=falseId;for(unsigned n=0;n<24;n++){
  auto hit=binary(begin,167,b,binary(begin,170,b,invocation,values[4+n]),binary(begin,172,b,values[1],c[n]));any=binary(begin,166,b,any,hit);
 }
 const auto instancePlusOne=binary(begin,128,u,invocation,one);
 auto hiddenLoaded=binary(begin,167,b,valid,any),handLoaded=binary(begin,167,b,valid,
     binary(begin,166,b,binary(begin,170,b,shield,instancePlusOne),binary(begin,166,b,binary(begin,170,b,values[3],instancePlusOne),binary(begin,170,b,values[2],instancePlusOne))));
 emit(begin,249,{end});emit(begin,248,{end});const auto hidden=next++,hand=next++;
 emit(begin,245,{b,hidden,falseId,instanceBlock,hiddenLoaded,load});emit(begin,245,{b,hand,falseId,instanceBlock,handLoaded,load});
 const auto nativeMask=next++;emit(mask,199,{u,nativeMask,instanceMask,cbMask});emit(mask,169,{u,maskAnd,hand,cbMask,nativeMask});
 const auto handBypassed=binary(mask,167,b,hand,binary(mask,170,b,nativeMask,zero));
 const auto main=binary(finish,170,b,subview,zero);
 const auto hideMain=binary(finish,167,b,hidden,main);
 const auto suppressShadow=binary(finish,167,b,handBypassed,unary(finish,168,b,main));
 const auto visible=binary(finish,167,b,visibility,unary(finish,168,b,binary(finish,166,b,hideMain,suppressShadow)));
 emit(finish,169,{u,visibleSelect,visible,one,zero});
 std::vector<uint32_t> out(w.begin(),w.begin()+5);out[3]=next;emit(out,17,{11});emit(out,17,{5347});
 bool annotated=false,declared=false,inserted=false,masked=false,finished=false;
 for(size_t at=5;at<w.size();at+=w[at]>>16){auto n=w[at]>>16,op=w[at]&65535;if(!n||at+n>w.size())throw std::runtime_error("appearance malformed module");
  if(op==71&&!annotated){out.insert(out.end(),ann.begin(),ann.end());annotated=true;}
  if(op==54&&!declared){out.insert(out.end(),decl.begin(),decl.end());declared=true;}
  if(op==14){if(w[at+1]!=0||w[at+2]!=1)throw std::runtime_error("appearance memory model");emit(out,14,{5348,1});}
  else if(op==199&&n==5&&w[at+2]==maskAnd){if(w[at+3]!=instanceMask||w[at+4]!=cbMask)throw std::runtime_error("appearance native mask SSA");out.insert(out.end(),mask.begin(),mask.end());masked=true;}
  else if(op==169&&n==6&&w[at+2]==visibleSelect){if(w[at+3]!=visibility||w[at+4]!=one||w[at+5]!=zero)throw std::runtime_error("appearance native visibility SSA");out.insert(out.end(),finish.begin(),finish.end());finished=true;}
  else{out.insert(out.end(),w.begin()+at,w.begin()+at+n);if(op==248&&n==2&&w[at+1]==instanceBlock){out.insert(out.end(),begin.begin(),begin.end());inserted=true;}}
 }
 if(!annotated||!declared||!inserted||!masked||!finished)throw std::runtime_error("appearance insertion boundary missing");return out;
}
}
