#pragma once
#include <evr/motion_actions.hpp>
namespace evr::motion {
struct Feedback {uint64_t frame=0,session=0,origin=0,milliseconds=0;std::array<float,2> amplitude{};};
inline std::array<float,2> feedbackForFrame(const Feedback& f,const HandFrame& current,uint64_t now){
 std::array<float,2> out{};
 if(!current.focused||!f.frame||!f.session||!f.origin||!f.milliseconds||f.session!=current.sessionGeneration||f.origin!=current.originGeneration||
    f.frame>current.frame||current.frame-f.frame>2||now<f.milliseconds||now-f.milliseconds>200)return out;
 for(auto a:f.amplitude)if(!std::isfinite(a)||a<0||a>1)return out;
 for(unsigned h=0;h<2;h++)if(current.hands[h].tracked()&&current.hands[h].grabActive&&current.hands[h].grab)out[h]=f.amplitude[h];
 return out;
}
}
