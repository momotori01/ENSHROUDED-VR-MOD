#include <evr/held_effect_mount.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){try{
 auto require=[](bool b){if(!b)throw std::runtime_error("held effect mount invariant");};
 const EffectTransform model{{int64_t(1)<<60,-(int64_t(1)<<58),int64_t(1)<<56},euler(0,0,pi/2),{2,3,4},91};
 const EffectOffset offset{{0,.57f,0},{.1f,.2f,.3f},{0,0,0,1}};
 const auto result=mountHeldEffect(model,offset);
 require(std::abs(relativeQ32(result.position.x,model.position.x)+.47)<1e-6&&std::abs(relativeQ32(result.position.y,model.position.y)-.2)<1e-6&&std::abs(relativeQ32(result.position.z,model.position.z)-.3)<1e-6);
 require(result.scale.y==3&&result.padding==91&&effectRotationError(result.orientation,model.orientation)<1e-7);
 auto unscaled=model;unscaled.scale={1,1,1};require(effectPositionError(result,mountHeldEffect(unscaled,offset))==0);
 // Independent captured torch witness from game-20261009-170154, frame495.
 const auto decode=[](const char* h){EffectTransform r{};auto* b=reinterpret_cast<unsigned char*>(&r);for(size_t i=0;i<56;++i){const auto nib=[](char c){return c<='9'?c-'0':c-'a'+10;};b[i]=uint8_t(nib(h[i*2])*16+nib(h[i*2+1]));}return r;};
 const auto source=decode("000030c0b30d000000005c1199030000000050a79b0700007bb0a23d15932bbffb44353ffda3543e503d803f9e54803f7318803f00000000");
 const auto native=decode("00f4bc85b30d0000003e1d0f990300000008a1219b0700007bb0a23d15932bbffb44353ffda3543e503d803f9e54803f7318803f00000000");
 const auto measured=mountHeldEffect(source,{{0,.57f,0},{0,0,0},{0,0,0,1}});
 require(effectPositionError(measured,native)<2e-7&&effectRotationError(measured.orientation,native.orientation)==0);
 for(int fault=0;fault<4;++fault){auto m=model;auto o=offset;if(fault==0)o.local.x=NAN;if(fault==1)o.local.y=11;if(fault==2)m.orientation={0,0,0,0};if(fault==3)m.position.x=INT64_MIN;
 bool threw=false;try{mountHeldEffect(m,o);}catch(const std::invalid_argument&){threw=true;}require(threw);}
 std::cout<<"PASS declared offset, raw native witness, scale independence, large Q32 coordinates and invalid inputs\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
