#include <evr/test_renderer.hpp>
#include <iostream>
using namespace evr;
int main(){
    try{
        std::filesystem::create_directories("tracking-recovery-validation");
        Log log("tracking-recovery-validation/openxr.jsonl",true);OpenXrClient xr(log);xr.init(1);xr.poll();
        unsigned visible=0,empty=0;bool valid=true;
        {TestRenderer renderer(xr);
         for(unsigned i=0;i<94;++i){
            if(!xr.beginFrame())throw std::runtime_error("session stopped during temporary tracking loss");
            const auto frame=xr.pendingFrame();bool wanted=i<2||i>=92;
            valid &= frame.shouldRender==wanted&&frame.frame==i+1;
            if(frame.shouldRender){++visible;for(uint32_t eye=0;eye<2;++eye){renderer.draw(eye,frame.imageIndices[eye],frame.views[eye],"");xr.markEyeRendered(eye,frame.frame);}}
            else{++empty;valid &= xr.headLocation.locationFlags==0;}
            xr.finishFrame();valid &= !xr.hasPendingFrame()&&xr.active();
         }}
        valid &= visible==4&&empty==90&&xr.draws[0]==4&&xr.draws[1]==4&&xr.emptyFrames==90;
        xr.shutdown();std::cout<<(valid?"PASS":"FAIL")<<" one session: paired GPU frames, 90 empty frames, paired GPU recovery, balanced frame closure\n";return valid?0:1;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
