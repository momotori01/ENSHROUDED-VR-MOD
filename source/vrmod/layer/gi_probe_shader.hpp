#pragma once
#include <fstream>
// Opt-in local experiment. Only the exact reviewed GI sampling shader is
// replaced; source and replacement hashes are logged. No game asset is written.
namespace giProbe {
inline constexpr const char* sourceSha="5d79570aa82b8a4247a11985893317fe908d1105e32a63244880e984957fd01c";
inline std::vector<uint32_t> load(const VkShaderModuleCreateInfo& source) {
    const auto* path=_wgetenv(L"EVR_GI_PROBE_SHADER");
    if(!path||!*path||source.codeSize!=33400||source.flags||source.pNext||
       head::sha256(source.pCode,source.codeSize)!=sourceSha)return {};
    const auto* expected=std::getenv("EVR_GI_PROBE_SHADER_SHA256");
    if(!expected||std::strlen(expected)!=64)throw std::runtime_error("missing GI probe replacement hash");
    std::ifstream stream(std::filesystem::path(path),std::ios::binary|std::ios::ate);
    if(!stream)throw std::runtime_error("GI probe replacement open failed");
    const auto size=stream.tellg();
    if(size<20||size>262144||static_cast<size_t>(size)%4)throw std::runtime_error("GI probe replacement size");
    std::vector<uint32_t> code(static_cast<size_t>(size)/4);stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(code.data()),static_cast<std::streamsize>(size))||code[0]!=0x07230203||
       head::sha256(code.data(),code.size()*4)!=expected)throw std::runtime_error("GI probe replacement contents/hash");
    return code;
}
}
