#include <evr/root_interpolation.hpp>
#include <fstream>
#include <iostream>
using namespace evr;
int main(int argc,char** argv){
    if(argc==4&&std::string(argv[1])=="--vectors"){
        // Binary rows: RootHistory176, tick4, fraction4 -> RootTransform56.
        std::ifstream input(argv[2],std::ios::binary);std::ofstream output(argv[3],std::ios::binary);
        if(!input||!output)return 2;
        game::RootHistory history;uint32_t tick;float fraction;unsigned count=0;
        while(input.read(reinterpret_cast<char*>(&history),sizeof(history))){
            if(!input.read(reinterpret_cast<char*>(&tick),4)||!input.read(reinterpret_cast<char*>(&fraction),4))return 2;
            const auto result=game::stableRoot(history,tick,fraction);output.write(reinterpret_cast<const char*>(&result),sizeof(result));++count;
        }
        if(input.gcount()||!output)return 2;
        std::cout<<"PASS root vectors "<<count<<'\n';return count?0:2;
    }
    unsigned passed=0,failed=0;auto check=[&](bool value,const char* text){std::cout<<(value?"PASS ":"FAIL ")<<text<<'\n';value?++passed:++failed;};
    check(!game::rootSampleUsable(0,100,100,true),"UI hold does not invent a sample");
    check(game::rootSampleUsable(1,100,1100,false),"1000 ms boundary is usable");
    check(!game::rootSampleUsable(1,100,1101,false),"startup stale sample is unavailable");
    check(game::rootSampleUsable(2,1101,1101,false),"fresh update after startup is usable");
    check(game::rootSampleUsable(1,100,10000,true),"existing UI hold accepts stored sample");
    check(!game::rootSampleUsable(1,100,99,true),"backwards clock rejected even during UI hold");
    game::RootHistory h;
    h.source={{int64_t(1e6*4294967296.),0,0},{0,0,0,1},{1,1,1}};
    h.target=h.source;h.target.position=cameraPosition(h.source.position,{.007f,-.013f,.019f},1);h.sourceTick=100;h.newestTick=101;
    const auto before=h;
    auto root=game::stableRoot(h,101,.5f);
    check(std::abs(relativeQ32(root.position.x,h.source.position.x)-.0035)<1e-8,"millimeter movement preserved at large world coordinate");
    h.rendered.position={INT64_MAX,INT64_MIN,INT64_MAX};h.rendered.orientation={1,0,0,0};
    const auto shaken=game::stableRoot(h,101,.5f);
    check(shaken.position.x==root.position.x&&shaken.position.y==root.position.y&&shaken.position.z==root.position.z,"rendered animation offsets do not enter stable root");
    check(h.source.position.x==before.source.position.x&&h.target.position.x==before.target.position.x&&h.sourceTick==100&&h.newestTick==101,"native interpolation history not advanced");
    h.target.orientation={0,1,0,0};root=game::stableRoot(h,101,.5f);
    check(std::abs(root.orientation.y-std::sqrt(.5f))<1e-6&&std::abs(root.orientation.w-std::sqrt(.5f))<1e-6,"native shortest quaternion arc");
    h.source=h.target;root=game::stableRoot(h,101,.1f);
    check(root.position.x==h.target.position.x&&root.position.y==h.target.position.y,"teleport-reset identical endpoints stay exact");
    try{game::stableRoot(h,101,1.1f);check(false,"invalid fraction refused");}catch(const std::invalid_argument&){check(true,"invalid fraction refused");}
    try{game::interpolateRootAxis(INT64_MIN,INT64_MAX,.5f);check(false,"extreme coordinate difference refused");}catch(const std::invalid_argument&){check(true,"extreme coordinate difference refused");}
    h.target.orientation={0,0,0,0};
    try{game::stableRoot(h,101,.5f);check(false,"invalid quaternion refused");}catch(const std::invalid_argument&){check(true,"invalid quaternion refused");}
    std::cout<<"RESULT "<<passed<<" PASS "<<failed<<" FAIL\n";return failed?1:0;
}
