#include <evr/appearance_spirv_patch.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
    if((argc!=3&&argc!=4))throw std::runtime_error("usage: evr_head_patch local-original.spv output.spv");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);if(!input||input.tellg()!=std::streamoff(evr::head::sourceBytes))throw std::runtime_error("input shader size");
    std::vector<uint32_t> source(evr::head::sourceBytes/4);input.seekg(0);input.read(reinterpret_cast<char*>(source.data()),evr::head::sourceBytes);
    if(!input)throw std::runtime_error("input read");const auto output=argc==4&&std::string(argv[3])=="--hands-only"?evr::appearance::patch(source.data(),evr::head::sourceBytes):evr::head::patch(source.data(),evr::head::sourceBytes);
    std::filesystem::create_directories(std::filesystem::absolute(argv[2]).parent_path());
    std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(output.data()),output.size()*4);
    if(!file)throw std::runtime_error("output write");
    std::cout<<"PASS guarded dynamic head shader "<<output.size()*4<<" bytes SHA256 "<<evr::head::sha256(output.data(),output.size()*4)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
