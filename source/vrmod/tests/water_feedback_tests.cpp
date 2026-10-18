#include <evr/water_feedback.hpp>
#include <iostream>
using namespace evr::water;
int main()
{
    unsigned passed=0,failed=0;
    auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passed:++failed;};
    std::array<Packet,3> p;for(auto& v:p)v.serial=17;
    p[0].split={{2,10},{3,11}};p[1].split={{8,10},{1,12}};p[2].split={{4,13},{7,10}};
    p[0].coarsen={10,20,21,20};p[1].coarsen={10,20,22};p[2].coarsen={10,20,23};
    p[0].retire={0xa000001e,0xa000001f,0xa000000a};p[1].retire={0xa000001e,0xb000001f,0xa000000a};p[2].retire={0xa000001e,0xa000001f,0xa000000a};
    auto m=merge(p);auto d=decode(m.bytes.data(),m.bytes.size(),m.serial,100);
    check(d.split.size()==4&&d.split[0].node==10&&d.split[0].priority==8,"all views refine with maximum native priority");
    check(d.coarsen==std::vector<uint32_t>{20},"coarsening requires all views and excludes refinement conflicts");
    check(d.retire==std::vector<uint32_t>{0xa000001e},"retirement requires exact generation agreement and no refinement");
    check(p[0].coarsen.size()==4&&p[0].split[0].priority==2,"input packets unchanged");
    p[2].serial=18;
    try{merge(p);check(false,"mismatched serial refused");}catch(const std::invalid_argument&){check(true,"mismatched serial refused");}
    try{decode(m.bytes.data(),m.bytes.size()-1,17,100);check(false,"short packet refused");}catch(const std::invalid_argument&){check(true,"short packet refused");}
    try{decode(m.bytes.data(),m.bytes.size(),17,12);check(false,"node table bounds enforced");}catch(const std::invalid_argument&){check(true,"node table bounds enforced");}
    for(auto& v:p){v=Packet{};v.serial=24;}
    for(size_t eye=0;eye<3;eye++)for(uint32_t n=0;n<capacity;n++)p[eye].split.push_back({uint32_t(eye),uint32_t(n+eye*capacity)});
    m=merge(p);d=decode(m.bytes.data(),m.bytes.size(),24,uint32_t(capacity*3));
    check(m.omittedSplits==capacity*2&&d.split.size()==capacity&&d.split.front().priority==2&&d.split.back().priority==2,"oversubscribed union preserves strongest requests within native capacity");
    auto raw=m.bytes;const uint32_t huge=0xffffffffu;std::memcpy(raw.data(),&huge,4);
    d=decode(raw.data(),raw.size(),24,uint32_t(capacity*3));
    check(d.rawCounts[0]==huge&&d.split.size()==capacity,"GPU counter overflow follows bounded native reads");
    for(auto& v:p){v=d;v.coarsen.clear();v.retire.clear();}
    check(merge(p).overflowSources[0]==3,"overflow provenance retained");
    for(auto& v:p){v=Packet{};v.serial=30;}
    m=merge(p);check(m.counts==std::array<uint32_t,3>{0,0,0}&&m.bytes.size()==packetBytes,"empty packets preserve complete native layout");
    std::cout<<"RESULT "<<passed<<" PASS "<<failed<<" FAIL\n";return failed?1:0;
}
