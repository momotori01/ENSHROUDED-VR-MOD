#include <evr/vfx_writeback_patch.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: evr_vfx_writeback_patch local-original.spv output.spv");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if(!input||input.tellg()!=2720)throw std::runtime_error("input shader extent");
    std::vector<uint32_t> source(680);input.seekg(0);input.read(reinterpret_cast<char*>(source.data()),2720);
    if(!input)throw std::runtime_error("input shader read");
    const auto result=evr::vfx::patchWriteback(source.data(),2720);
    std::filesystem::create_directories(std::filesystem::absolute(argv[2]).parent_path());
    std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(result.data()),result.size()*4);
    if(!file)throw std::runtime_error("output shader write");
    std::cout<<"PASS guarded VFX writeback shader "<<result.size()*4<<" bytes SHA256 "<<evr::head::sha256(result.data(),result.size()*4)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
