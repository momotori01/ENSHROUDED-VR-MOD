#include <evr/culling_history.hpp>
#include <iostream>
int main(){
    unsigned failures=0;auto check=[&](bool v,const char* s){std::cout<<(v?"PASS ":"FAIL ")<<s<<'\n';if(!v)++failures;};
    using evr::culling::resetRanges;
    check(resetRanges({1,5,7},8).empty(),"moving existing instances preserves history");
    auto r=resetRanges({0x80000005,0x80000002,0x80000003,0x80000002,6},8);
    check(r.size()==2&&r[0].first==2&&r[0].count==2&&r[1].first==5&&r[1].count==1,"new or replaced instances reset once; order independent");
    r=resetRanges({0x80000000,0x8000ffff},65536);
    check(r.size()==2&&r.front().first==0&&r.back().first==65535,"first and last native slots included");
    bool rejected=false;try{resetRanges({0x80010000},65536);}catch(const std::exception&){rejected=true;}
    check(rejected,"out of range update rejected");
    std::vector<uint32_t> many;for(unsigned i=0;i<2048;i++)many.push_back(0x80000000u+i*2);
    r=resetRanges(many,65536);check(r.size()==1&&r[0].first==0&&r[0].count==65536,"mass update uses bounded conservative reset");
    check(resetRanges({},65536).empty(),"empty upload leaves history intact");
    return failures?1:0;
}
