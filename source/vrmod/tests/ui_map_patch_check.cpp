// Manual check (2026-10-06): patch the three reviewed world-map fragment shaders the way the UI pipeline probe does
// and write the results for inspection (spirv-cross). Usage: evr_ui_map_patch_check out-dir map1.spv map2.spv map3.spv
#include <evr/ui_spirv_patch.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace evr;
static std::vector<uint32_t> words(const std::filesystem::path& p){
    std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("open "+p.string());
    const auto n=size_t(f.tellg());f.seekg(0);std::vector<uint32_t> w(n/4);f.read(reinterpret_cast<char*>(w.data()),std::streamsize(n));return w;
}
int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"usage: out-dir map1.spv map2.spv map3.spv\n";return 2;}
    unsigned passed=0,failed=0;auto check=[&](bool v,const std::string& n){std::cout<<(v?"PASS ":"FAIL ")<<n<<'\n';v?++passed:++failed;};
    const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    for(int i=2;i<5;i++){
        auto w=words(argv[i]);const auto bytes=w.size()*4;
        const auto* map=ui::reviewedMapFragment(bytes,head::sha256(w.data(),bytes));
        check(map!=nullptr,std::string("reviewed map shader ")+argv[i]);if(!map)continue;
        const auto mode=map->straightAlpha?ui::FragmentOutput::Premultiply:ui::FragmentOutput::Duplicate;
        auto p=ui::duplicateFragmentOutput(w.data(),bytes,mode);
        check(p.size()>w.size()&&p[0]==0x07230203,"patched shader grew and keeps the SPIR-V header");
        std::ofstream f(out/(std::filesystem::path(argv[i]).stem().string()+"-mrt.spv"),std::ios::binary);
        f.write(reinterpret_cast<const char*>(p.data()),std::streamsize(p.size()*4));
        bool rejected=false;try{ui::duplicateFragmentOutput(w.data(),bytes,map->straightAlpha?ui::FragmentOutput::Duplicate:ui::FragmentOutput::Premultiply);}catch(const std::exception&){rejected=true;}
        check(!map->straightAlpha?rejected:!rejected,"premultiply only for the straight-alpha map shader");
        auto bad=w;bad.back()^=1;rejected=false;try{ui::duplicateFragmentOutput(bad.data(),bytes,mode);}catch(const std::exception&){rejected=true;}
        check(rejected,"modified map shader rejected");
    }
    std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
