#include <evr/recorded_xr.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc,char**argv){
    int failures=0,passes=0;
    auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passes:++failures;};
    const std::string body="800 900 1000 1100\n0 -1.6 0 0 0 0 1\n1 111\n-.032 1.6 0 0 0 0 1 -.88 .82 .8 -.85\n.032 1.6 0 0 0 0 1 -.82 .88 .8 -.85\n";
    auto parse=[&](std::string s){std::istringstream in(s);return evr::diagnostic::RecordedXr::read(in);};
    const auto valid=parse("EVR_RECORDED_XR_1 1 1.25\n"+body);
    check(valid.width[0]==1000&&valid.height[0]==1125&&valid.width[1]==1250&&valid.height[1]==1375,"runtime-specific extents independently scaled");
    const auto& f=valid.frames[0];evr::game::StableAnchor anchor;
    const auto cameras=evr::game::stereoCameras(anchor,valid.trackingToAnchor,f.views,3,f.time,f.frame,anchor.root,.01f,1000);
    check(std::abs(evr::relativeQ32(cameras.eyes[0].position.y,0))<1e-6&&std::abs(evr::relativeQ32(cameras.eyes[1].position.x,cameras.eyes[0].position.x)-.064)<1e-6,"recorded recenter preserves complete eye baseline");
    check(cameras.eyes[0].projection.values[8]*cameras.eyes[1].projection.values[8]<0,"recorded independent FOVs retain asymmetry");
    auto reject=[&](std::string s,const char*name){try{parse(s);check(false,name);}catch(const std::invalid_argument&){check(true,name);}};
    reject("EVR_RECORDED_XR_1 1 0\n"+body,"zero scale refused");
    reject("EVR_RECORDED_XR_1 1 100\n"+body,"oversized diagnostic allocation refused");
    reject("EVR_RECORDED_XR_1 1001 1\n"+body,"unbounded frame count refused");
    reject("EVR_RECORDED_XR_1 1 1\n"+body+"garbage","trailing garbage refused");
    reject("EVR_RECORDED_XR_1 2 1\n"+body,"truncated right/frame data refused");
    if(argc==2){std::ifstream in(argv[1]);try{auto recording=evr::diagnostic::RecordedXr::read(in);check(!recording.frames.empty(),"exported OpenXR recording accepted");}
        catch(const std::exception&e){check(false,e.what());}}
    std::cout<<passes<<" PASS / "<<failures<<" FAIL\n";return failures?1:0;
}
