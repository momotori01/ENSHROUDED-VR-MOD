#include <evr/head_visibility.hpp>
#include <iostream>
int main(){try{
    auto require=[](bool b){if(!b)throw std::runtime_error("head visibility invariant");};
    const std::vector<uint32_t> ids{0,3,17,40000,65535};const auto p=evr::head::pack(ids);
    for(unsigned v=0;v<11;v++)for(unsigned i=0;i<65536;i++)
        require(evr::head::excluded(p,i,v)==(v==0&&std::find(ids.begin(),ids.end(),i)!=ids.end()));
    for(unsigned n=0;n<=5;n++){
        const std::vector<uint32_t> selected(ids.begin(),ids.begin()+n);const auto value=evr::head::pack(selected);
        for(auto id:ids)require(evr::head::excluded(value,id,0)==(std::find(selected.begin(),selected.end(),id)!=selected.end()));
    }
    require(evr::head::pack({3,3,3})==evr::head::pack({3}));
    for(auto tag:{0u,1u,0xffffu,0xa5d0u,0xa5d6u,0xa5d7u})require(!evr::head::excluded({0,0,tag<<16},0,0));
    require(!evr::head::excluded(p,65536,0));
    for(auto bad:std::vector<std::vector<uint32_t>>{{65536},{0,1,2,3,4,5}}){bool rejected=false;try{evr::head::pack(bad);}catch(...){rejected=true;}require(rejected);}
    std::cout<<"PASS 720896 instance/subview cases; all 0..5 counts, malformed tags, deduplication, boundary IDs\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
