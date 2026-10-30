#pragma once
#include <algorithm>
#include <unordered_set>
#include <vector>
// The pickaxe's blue ring (2026-10-30, user: "the pickaxe feels good - hide the blue ring"). The game draws it as a VFX of a
// render material type of its own, TerraformingCursor (keen::VfxTerraformingCursorInstance with a
// TerraformingCursorRfxRenderMaterial; the material types Billboard 0 .. TerraformingCursor 9 .. SkyTexture 10 at rva
// 0x1abb000), so it has graphics pipelines of its own. Moving the cursor's ray away from the ground took the ring away but
// the digs too (the game digs where that ray meets the ground) - so the ring is left out where it is drawn.
// A pipeline is known by its shader stages: each module's code hashed (FNV-1a 64 over its words, as the "shader" record),
// the pipeline's key a hash of its stages and their modules' hashes - the same in every run of the same game build.
// EVR_PIPELINE_CENSUS=1 (diagnostic): the draws of every key counted, written every 0.5 s (pipeline_census); with the ring
// shown and not, its pipelines are the keys drawn only while it shows.
// The skip: draws while a pipeline of a hidden key is bound are not passed on, while the mod asks for it
// (evrSetToolRingHidden: a tool in the right hand). EVR_HIDE_PIPELINES=<hex>,<hex>: other keys (a diagnostic).
namespace pipelineCensus {
inline const bool census = [] { const char *v = std::getenv("EVR_PIPELINE_CENSUS"); return v && v[0] == '1' && !v[1]; }();
inline std::vector<uint64_t> hiddenKeys = [] {
    // The ring's pipeline (2026-10-30, Mock 20261030-101235 with EVR_HIDE_BITS=3000,60: of the 55 blended pipelines the ring
    // went with bits 0 2 3 4 5 6 8 10 12 13 14 and stayed with 1 7 9 11 15 16 - one key's 17 low bits, 0x0757d; the next
    // nearest key differs in 4). A game build with other shaders has another key: the ring is shown then.
    std::vector<uint64_t> keys{0x93c95027d2d6757dull};
    if (const char *v = std::getenv("EVR_HIDE_PIPELINES")) {
        keys.clear();
        std::string s(v);
        size_t at = 0;
        while (at < s.size()) {
            const size_t end = std::min(s.find(',', at), s.size());
            const std::string item = s.substr(at, end - at);
            if (!item.empty()) try { keys.push_back(std::stoull(item, nullptr, 16)); } catch (...) {}
            at = end + 1;
        }
    }
    return keys;
}();
inline std::atomic<bool> hideWanted{false};
// Diagnostic bit test (EVR_HIDE_BITS=<first frame>,<frames a block>): from that XR frame on, 34 blocks; in the odd block 2k+1
// the blended pipelines (a colour attachment with blending - the ring is see-through) whose key has bit k set are left out,
// the even blocks leave nothing out. Pictures of the blocks tell the ring's key bit by bit (one Mock run).
struct BitTest { uint64_t from = 0; uint64_t period = 0; };
inline const BitTest bitTest = [] {
    BitTest t;
    if (const char *v = std::getenv("EVR_HIDE_BITS")) {
        try { std::string s(v); const auto c = s.find(','); t.from = std::stoull(s.substr(0, c)); if (c != std::string::npos) t.period = std::stoull(s.substr(c + 1)); } catch (...) { t = BitTest{}; }
    }
    return t;
}();
inline std::atomic<uint64_t> frameNow{0};   // the XR frame begun last (native_xr.hpp)
inline std::unordered_set<uint64_t> blendedPipelines;
inline int lastBlock = -1;
inline int bitNow() noexcept
{
    if (!bitTest.period) return -1;
    const uint64_t f = frameNow.load();
    if (f < bitTest.from) return -1;
    const uint64_t b = (f - bitTest.from) / bitTest.period;
    if (b >= 34) return -1;
    return (b & 1) ? int(b / 2) : -1;
}
inline std::mutex m;
inline std::unordered_map<uint64_t, uint64_t> moduleHash, pipelineKey;
inline std::unordered_set<uint64_t> hiddenPipelines;
inline std::atomic<unsigned> hiddenCount{0};
inline std::unordered_map<uint64_t, uint32_t> counts;
inline uint64_t windowAt = 0;
inline std::atomic<uint64_t> skipped{0}, windows{0};
struct Bound { VkCommandBuffer c = VK_NULL_HANDLE; uint64_t key = 0; bool hidden = false; };
inline thread_local Bound bound;   // a command buffer is recorded on one thread at a time
inline uint64_t fnvWords(const uint32_t *p, size_t n, uint64_t h = 14695981039346656037ull)
{
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
inline bool active() { return census || !hiddenKeys.empty() || bitTest.period; }
inline void shaderCreated(const VkShaderModuleCreateInfo *info, VkShaderModule module) noexcept
{
    if (!active() || !info || !info->pCode || !module) return;
    try { const auto h = fnvWords(info->pCode, info->codeSize / 4); std::lock_guard<std::mutex> l(m); moduleHash[uint64_t(module)] = h; } catch (...) {}
}
inline void shaderDestroyed(VkShaderModule module) noexcept
{
    if (!active() || !module) return;
    try { std::lock_guard<std::mutex> l(m); moduleHash.erase(uint64_t(module)); } catch (...) {}
}
inline void pipelineCreated(const VkGraphicsPipelineCreateInfo &info, VkPipeline pipeline) noexcept
{
    if (!active() || !pipeline) return;
    try {
        std::lock_guard<std::mutex> l(m);
        uint64_t key = 14695981039346656037ull;
        for (uint32_t j = 0; j < info.stageCount; j++) {
            const auto &s = info.pStages[j];
            uint64_t h = 0;
            if (s.module) { const auto it = moduleHash.find(uint64_t(s.module)); if (it != moduleHash.end()) h = it->second; }
            else for (auto n = static_cast<const VkBaseInStructure *>(s.pNext); n; n = n->pNext)
                if (n->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO) {
                    const auto ci = reinterpret_cast<const VkShaderModuleCreateInfo *>(n);
                    if (ci->pCode) h = fnvWords(ci->pCode, ci->codeSize / 4);
                }
            const uint32_t words[4]{uint32_t(s.stage), 0, uint32_t(h), uint32_t(h >> 32)};
            key = fnvWords(words, 4, key);
        }
        pipelineKey[uint64_t(pipeline)] = key;
        if (bitTest.period) {
            bool blended = false;
            if (info.pColorBlendState)
                for (uint32_t a = 0; a < info.pColorBlendState->attachmentCount; a++)
                    if (info.pColorBlendState->pAttachments && info.pColorBlendState->pAttachments[a].blendEnable) blended = true;
            if (blended) {
                blendedPipelines.insert(uint64_t(pipeline));
                std::ostringstream s;
                s << "\"key\":\"" << std::hex << key << '"';
                note("pipeline_blended", s.str());
            }
        }
        if (std::find(hiddenKeys.begin(), hiddenKeys.end(), key) != hiddenKeys.end()) {
            hiddenPipelines.insert(uint64_t(pipeline));
            hiddenCount = unsigned(hiddenPipelines.size());
            std::ostringstream s;
            s << "\"pipeline\":\"0x" << std::hex << uint64_t(pipeline) << "\",\"key\":\"" << key << '"';
            note("pipeline_hidden_known", s.str());
        }
    } catch (...) {}
}
inline void pipelineDestroyed(VkPipeline pipeline) noexcept
{
    if (!active() || !pipeline) return;
    try {
        std::lock_guard<std::mutex> l(m);
        pipelineKey.erase(uint64_t(pipeline));
        blendedPipelines.erase(uint64_t(pipeline));
        if (hiddenPipelines.erase(uint64_t(pipeline))) hiddenCount = unsigned(hiddenPipelines.size());
    } catch (...) {}
}
inline void bind(VkCommandBuffer c, VkPipelineBindPoint point, VkPipeline pipeline) noexcept
{
    if (point != VK_PIPELINE_BIND_POINT_GRAPHICS) return;
    if (!census && !hiddenCount.load() && !bitTest.period) { bound = Bound{c, 0, false}; return; }
    try {
        std::lock_guard<std::mutex> l(m);
        const auto it = pipelineKey.find(uint64_t(pipeline));
        const uint64_t key = it != pipelineKey.end() ? it->second : 0;
        bool hidden = hiddenPipelines.count(uint64_t(pipeline)) != 0;
        if (bitTest.period) {
            const int bit = bitNow();
            const int block = frameNow.load() >= bitTest.from ? int((frameNow.load() - bitTest.from) / bitTest.period) : -1;
            if (block != lastBlock) { lastBlock = block; note("pipeline_bit_block", "\"block\":" + std::to_string(block) + ",\"bit\":" + std::to_string(bit) + ",\"frame\":" + std::to_string(frameNow.load())); }
            hidden = bit >= 0 && blendedPipelines.count(uint64_t(pipeline)) && ((key >> bit) & 1);
        }
        bound = Bound{c, key, hidden};
    } catch (...) {}
}
// A draw: counted (census); true when it is to be left out.
inline bool draw(VkCommandBuffer c) noexcept
{
    if (bound.c != c) return false;   // (its pipeline bound on another thread: as it comes)
    if (census) try {
        std::lock_guard<std::mutex> l(m);
        ++counts[bound.key];
        const uint64_t now = GetTickCount64();
        if (!windowAt) windowAt = now;
        if (now - windowAt >= 500) {
            std::ostringstream s;
            s << "\"window\":" << windows.fetch_add(1) << ",\"ms\":" << (now - windowAt) << ",\"hide_wanted\":" << (hideWanted.load() ? "true" : "false")
              << ",\"skipped\":" << skipped.load() << ",\"draws\":{";
            bool first = true;
            for (const auto &[k, n] : counts) { if (!first) s << ','; first = false; s << "\"" << std::hex << k << std::dec << "\":" << n; }
            s << '}';
            note("pipeline_census", s.str());
            counts.clear();
            windowAt = now;
        }
    } catch (...) {}
    if (bound.hidden && (hideWanted.load() || bitTest.period)) { ++skipped; return true; }
    return false;
}
}
// The mod: hide the ring's pipelines now (a tool in the right hand) or not.
extern "C" __declspec(dllexport) void evrSetToolRingHidden(int hidden) noexcept { pipelineCensus::hideWanted = hidden != 0; }
