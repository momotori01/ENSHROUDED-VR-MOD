#pragma once
#include <fstream>
// Opt-in local experiment. Only the exact reviewed sun-visibility shader is
// replaced; source and replacement hashes are logged. No game asset is written.
namespace shadowReceiver {
inline constexpr const char* sourceSha="b57c41ce794e40677ae687f44db16a84ffa1fa45a72e865832aa3eab3c3312f2";   // build 26320373 (2026-11-02, 22816 bytes)
inline constexpr size_t sourceBytes=22816;
// 1.0 to build 26082781: bb450d00434dca964a841dcc9995582282ce69bf49260e6d5e153ba87483177b, 20244 bytes.
// June: e0d6de57a4db980fcdcfd7ba61640f0db21be193ef57ffde91b43dd6efca2427, 21228 bytes (tools/prepare_shadow_receiver_shader.py).
inline std::vector<uint32_t> load(const VkShaderModuleCreateInfo& source) {
    const auto* path=_wgetenv(L"EVR_SHADOW_RECEIVER_SHADER");
    if(!path||!*path||source.codeSize!=sourceBytes||source.flags||source.pNext||
       head::sha256(source.pCode,source.codeSize)!=sourceSha)return {};
    const auto* expected=std::getenv("EVR_SHADOW_RECEIVER_SHADER_SHA256");
    if(!expected||std::strlen(expected)!=64)throw std::runtime_error("missing shadow replacement hash");
    std::ifstream stream(std::filesystem::path(path),std::ios::binary|std::ios::ate);
    if(!stream)throw std::runtime_error("shadow replacement open failed");
    const auto size=stream.tellg();
    if(size<20||size>262144||static_cast<size_t>(size)%4)throw std::runtime_error("shadow replacement size");
    std::vector<uint32_t> code(static_cast<size_t>(size)/4);stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(code.data()),static_cast<std::streamsize>(size))||code[0]!=0x07230203||
       head::sha256(code.data(),code.size()*4)!=expected)throw std::runtime_error("shadow replacement contents/hash");
    return code;
}
}
