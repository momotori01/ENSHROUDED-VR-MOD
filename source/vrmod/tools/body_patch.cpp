// Local native shader transformation utility; does not load the game.
#include <evr/body_clip_spirv_patch.hpp>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=4)throw std::runtime_error("usage: evr_body_patch depth|color input.spv output.spv");
    const std::string mode=argv[1];if(mode!="depth"&&mode!="color")throw std::runtime_error("unknown mode");
    std::ifstream input(argv[2],std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("missing local shader");
    const auto bytes=input.tellg();if(bytes<=0||bytes%4||bytes>1024*1024)throw std::runtime_error("shader extent");
    std::vector<uint32_t> code(size_t(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),bytes);if(!input)throw std::runtime_error("short shader");
    const auto patched=evr::body::patch(code.data(),size_t(bytes),mode=="color");
    std::ofstream output(argv[3],std::ios::binary);output.write(reinterpret_cast<const char*>(patched.data()),patched.size()*4);if(!output)throw std::runtime_error("output write");
    std::cout<<"PASS "<<mode<<" "<<patched.size()*4<<" bytes "<<evr::head::sha256(patched.data(),patched.size()*4)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
