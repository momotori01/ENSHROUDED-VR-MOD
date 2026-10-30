#pragma once
#include <evr/ui_spirv_patch.hpp>
namespace uiDraw {
void reset(VkCommandBuffer) noexcept;
void bind(VkCommandBuffer,VkPipelineBindPoint,VkPipeline) noexcept;
void submitted(VkQueue,VkCommandBuffer,VkResult) noexcept;
}
// Explicit, bounded diagnostic. Merely builds private compatible pipelines;
// this phase does not bind them, change game shaders or alter attachments.
namespace uiCapture {
std::mutex stateMutex;
struct Shader {bool vertex=false,map=false,straightAlpha=false;std::vector<uint32_t> fragment;};
// seen: the size and SHA-256 of every small module (up to 64 KiB), named in ui_shaped_pipeline, so that UI pipelines of
// a new game build can be told apart (2026-10-16: 1.0 rebuilt the UI shaders and nothing named them).
struct Device {std::unordered_map<VkShaderModule,Shader> shaders;std::unordered_map<VkPipeline,VkPipeline> pipelines;
    std::unordered_map<VkShaderModule,std::pair<size_t,std::string>> seen;};
std::unordered_map<VkDevice,Device> state;
bool enabled(){static const bool value=[](){const auto* p=std::getenv("EVR_UI_PIPELINE_PROBE");return p&&std::strcmp(p,"1")==0;}();return value;}
inline std::string h(uint64_t value){std::ostringstream s;s<<"\"0x"<<std::hex<<value<<'"';return s.str();}
void shaderCreated(VkDevice device,const VkShaderModuleCreateInfo* info,VkShaderModule module) noexcept {
    // 2026-10-06: module handles are reused after destruction; forget whatever this handle meant before, so a
    // later pipeline never pairs the new module with an earlier reviewed shader's code.
    if(enabled()&&module){std::lock_guard<std::mutex> lock(stateMutex);auto d=state.find(device);if(d!=state.end()){d->second.shaders.erase(module);d->second.seen.erase(module);}}
    if(enabled()&&module&&info&&info->pCode&&info->codeSize<=65536)try{
        auto hash=evr::head::sha256(info->pCode,info->codeSize);
        std::lock_guard<std::mutex> lock(stateMutex);auto& d=state[device];if(d.seen.size()<8192)d.seen[module]={info->codeSize,std::move(hash)};}catch(...){}
    if(!enabled()||!module||!info||info->flags||info->pNext||
       (info->codeSize!=1488&&info->codeSize!=2640&&info->codeSize!=3072&&info->codeSize!=1784&&info->codeSize!=18648&&info->codeSize!=2308&&
        info->codeSize!=1632&&info->codeSize!=2704&&info->codeSize!=3100&&info->codeSize!=1832&&info->codeSize!=18612&&info->codeSize!=2400))return;   // 1.0: the last six
    try{
        const auto hash=evr::head::sha256(info->pCode,info->codeSize);
        const auto* map=evr::ui::reviewedMapFragment(info->codeSize,hash);
        if(!map&&!evr::ui::reviewedVertex(hash)&&!evr::ui::reviewedFragment(info->codeSize,hash))return;
        Shader s;s.vertex=evr::ui::reviewedVertex(hash);s.map=map!=nullptr;s.straightAlpha=map&&map->straightAlpha;
        if(!s.vertex)s.fragment.assign(info->pCode,info->pCode+info->codeSize/4);
        {std::lock_guard<std::mutex> lock(stateMutex);auto& d=state[device];
            if(d.shaders.size()>=128)throw std::runtime_error("UI shader registry cap");d.shaders[module]=std::move(s);}
        note("ui_shader_verified","\"module\":"+h(uint64_t(module))+
            ",\"sha256\":"+jsonString(hash)+",\"bytes\":"+std::to_string(info->codeSize));
    }catch(const std::exception& e){note("ui_shader_probe_failed","\"reason\":"+jsonString(e.what()));}
}
bool additive(const VkPipelineColorBlendAttachmentState& b){
    return b.blendEnable&&b.srcColorBlendFactor==VK_BLEND_FACTOR_ONE&&b.dstColorBlendFactor==VK_BLEND_FACTOR_ONE&&
        b.colorBlendOp==VK_BLEND_OP_ADD&&b.srcAlphaBlendFactor==VK_BLEND_FACTOR_ONE&&
        b.dstAlphaBlendFactor==VK_BLEND_FACTOR_ONE&&b.alphaBlendOp==VK_BLEND_OP_ADD&&b.colorWriteMask==15;
}
// Straight-alpha source-over; or the same for the colour with the target's alpha kept (ZERO / ONE: the 1.0 menu shadow).
bool alphaKept(const VkPipelineColorBlendAttachmentState& b){return b.srcAlphaBlendFactor==VK_BLEND_FACTOR_ZERO&&b.dstAlphaBlendFactor==VK_BLEND_FACTOR_ONE;}
bool straightAlpha(const VkPipelineColorBlendAttachmentState& b){
    return b.blendEnable&&b.srcColorBlendFactor==VK_BLEND_FACTOR_SRC_ALPHA&&b.dstColorBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA&&
        b.colorBlendOp==VK_BLEND_OP_ADD&&((b.srcAlphaBlendFactor==VK_BLEND_FACTOR_SRC_ALPHA&&b.dstAlphaBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)||alphaKept(b))&&
        b.alphaBlendOp==VK_BLEND_OP_ADD&&b.colorWriteMask==15;
}
const VkPipelineRenderingCreateInfo* shape(const VkGraphicsPipelineCreateInfo& p,bool allowStraight=false){
    if(p.flags||p.renderPass||p.subpass||p.stageCount!=2||p.basePipelineHandle||p.basePipelineIndex!=-1||
       !p.pColorBlendState||!p.pMultisampleState||!p.pRasterizationState||!p.pDynamicState)
        throw std::runtime_error("pipeline envelope");
    const auto* r=static_cast<const VkPipelineRenderingCreateInfo*>(p.pNext);
    if(!r||r->sType!=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO||r->pNext||r->viewMask||
       r->colorAttachmentCount!=1||!r->pColorAttachmentFormats||r->pColorAttachmentFormats[0]!=VK_FORMAT_R8G8B8A8_UNORM||
       r->depthAttachmentFormat!=VK_FORMAT_UNDEFINED||r->stencilAttachmentFormat!=VK_FORMAT_UNDEFINED)
        throw std::runtime_error("pipeline rendering format/chain");
    for(uint32_t i=0;i<p.stageCount;i++)if(p.pStages[i].pNext||p.pStages[i].flags||p.pStages[i].pSpecializationInfo)
        throw std::runtime_error("shader stage options");
    const auto& blend=*p.pColorBlendState;
    if(blend.pNext||blend.flags||blend.logicOpEnable||blend.attachmentCount!=1||!blend.pAttachments)
        throw std::runtime_error("blend envelope");
    const auto& b=blend.pAttachments[0];
    if(!additive(b)&&!(allowStraight&&straightAlpha(b))&&(!b.blendEnable||b.srcColorBlendFactor!=VK_BLEND_FACTOR_ONE||b.dstColorBlendFactor!=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA||
       b.colorBlendOp!=VK_BLEND_OP_ADD||b.srcAlphaBlendFactor!=VK_BLEND_FACTOR_ONE||
       b.dstAlphaBlendFactor!=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA||b.alphaBlendOp!=VK_BLEND_OP_ADD||b.colorWriteMask!=15))
        throw std::runtime_error("not reviewed source-over/additive blend");
    const auto& ms=*p.pMultisampleState;const auto& rs=*p.pRasterizationState;
    if(ms.pNext||ms.flags||ms.rasterizationSamples!=VK_SAMPLE_COUNT_1_BIT||ms.sampleShadingEnable||ms.alphaToCoverageEnable||
       ms.alphaToOneEnable||rs.pNext||rs.flags||rs.rasterizerDiscardEnable||rs.depthBiasEnable)
        throw std::runtime_error("rasterization/multisampling options");
    if(p.pDepthStencilState&&(p.pDepthStencilState->pNext||p.pDepthStencilState->flags||
       p.pDepthStencilState->depthTestEnable||p.pDepthStencilState->depthWriteEnable||
       p.pDepthStencilState->depthBoundsTestEnable||p.pDepthStencilState->stencilTestEnable))
        throw std::runtime_error("depth/stencil");
    if(p.pDynamicState->pNext||p.pDynamicState->flags)throw std::runtime_error("dynamic state options");
    for(uint32_t i=0;i<p.pDynamicState->dynamicStateCount;i++){
        const auto s=p.pDynamicState->pDynamicStates[i];
        if(s!=VK_DYNAMIC_STATE_VIEWPORT&&s!=VK_DYNAMIC_STATE_SCISSOR&&s!=VK_DYNAMIC_STATE_BLEND_CONSTANTS&&
           s!=VK_DYNAMIC_STATE_STENCIL_REFERENCE&&s!=VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK&&s!=VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)
            throw std::runtime_error("unreviewed dynamic state "+std::to_string(s));
    }
    return r;
}
void pipelineCreated(VkDevice device,const VkGraphicsPipelineCreateInfo& source,VkPipeline original) noexcept {
    if(!enabled()||!original||source.stageCount!=2||!source.pStages)return;
    // 2026-10-06 (map tab): record every UI-shaped pipeline (dynamic rendering, one RGBA8 UNORM target) with its shader
    // modules and blend, so pipelines outside the reviewed UI shaders can be identified when they are drawn.
    try{const auto* r=static_cast<const VkPipelineRenderingCreateInfo*>(source.pNext);static std::atomic<unsigned> shaped{0};
        if(r&&r->sType==VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO&&r->colorAttachmentCount==1&&r->pColorAttachmentFormats&&
           r->pColorAttachmentFormats[0]==VK_FORMAT_R8G8B8A8_UNORM&&source.pColorBlendState&&source.pColorBlendState->attachmentCount==1&&
           source.pColorBlendState->pAttachments&&shaped++<256){
            const auto& b=source.pColorBlendState->pAttachments[0];
            std::string shaders;
            {std::lock_guard<std::mutex> lock(stateMutex);auto d=state.find(device);
                for(uint32_t i=0;i<source.stageCount&&i<2;i++){const char* name=i?"fragment":"vertex";
                    if(d!=state.end()){auto s=d->second.seen.find(source.pStages[i].module);
                        if(s!=d->second.seen.end())shaders+=std::string(",\"")+name+"_bytes\":"+std::to_string(s->second.first)+",\""+name+"_sha256\":"+jsonString(s->second.second);}}}
            note("ui_shaped_pipeline","\"pipeline\":"+h(uint64_t(original))+",\"vertex_module\":"+h(uint64_t(source.pStages[0].module))+",\"fragment_module\":"+h(uint64_t(source.pStages[1].module))+shaders+
                ",\"blend\":["+std::to_string(b.blendEnable)+","+std::to_string(b.srcColorBlendFactor)+","+std::to_string(b.dstColorBlendFactor)+","+std::to_string(b.colorBlendOp)+","+
                std::to_string(b.srcAlphaBlendFactor)+","+std::to_string(b.dstAlphaBlendFactor)+","+std::to_string(b.alphaBlendOp)+","+std::to_string(b.colorWriteMask)+"]");
        }}catch(...){}
    VkPipeline clone=VK_NULL_HANDLE;VkShaderModule patchedModule=VK_NULL_HANDLE;
    try{
        std::vector<uint32_t> code;bool vertex=false,otherVertex=false,map=false,straight=false;uint32_t fragment=UINT32_MAX;
        {std::lock_guard<std::mutex> lock(stateMutex);auto d=state.find(device);if(d==state.end())return;
            for(uint32_t i=0;i<source.stageCount;i++){
                const auto& stage=source.pStages[i];auto s=d->second.shaders.find(stage.module);
                if(stage.stage==VK_SHADER_STAGE_VERTEX_BIT){
                    // Map fragments run with their own vertex shader, which the clone keeps unchanged.
                    if(s!=d->second.shaders.end()&&s->second.vertex&&stage.pName&&std::strcmp(stage.pName,"vs_main")==0)vertex=true;else otherVertex=true;
                }else if(stage.stage==VK_SHADER_STAGE_FRAGMENT_BIT&&s!=d->second.shaders.end()&&!s->second.fragment.empty()&&stage.pName&&
                        std::strcmp(stage.pName,"fs_main")==0){
                    code=s->second.fragment;fragment=i;map=s->second.map;straight=s->second.straightAlpha;
                }else return;
            }
            if(d->second.pipelines.size()>=128)throw std::runtime_error("UI pipeline registry cap");
        }
        if(fragment==UINT32_MAX||!(vertex||(map&&otherVertex)))return;
        const auto* nativeRendering=shape(source,straight);
        const bool isAdditive=additive(source.pColorBlendState->pAttachments[0]);
        const auto mode=straight?evr::ui::FragmentOutput::Premultiply:isAdditive?evr::ui::FragmentOutput::AdditiveAlphaZero:evr::ui::FragmentOutput::Duplicate;
        const auto changed=evr::ui::duplicateFragmentOutput(code.data(),code.size()*4,mode);
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=changed.size()*4;sm.pCode=changed.data();
        evr::vkCheck(deviceFn<PFN_vkCreateShaderModule>(device,"vkCreateShaderModule")(device,&sm,nullptr,&patchedModule),"UI clone shader");
        std::array<VkPipelineShaderStageCreateInfo,2> stages{source.pStages[0],source.pStages[1]};stages[fragment].module=patchedModule;
        auto rendering=*nativeRendering;std::array<VkFormat,2> formats{nativeRendering->pColorAttachmentFormats[0],nativeRendering->pColorAttachmentFormats[0]};
        rendering.colorAttachmentCount=2;rendering.pColorAttachmentFormats=formats.data();
        auto blend=*source.pColorBlendState;std::array<VkPipelineColorBlendAttachmentState,2> attachments{blend.pAttachments[0],blend.pAttachments[0]};
        if(straight&&alphaKept(attachments[0])){
            // The game's target (output 0) keeps its alpha as natively; the VR plane (output 1) is coverage, so there the
            // shape covers what lies behind as it covers the scene on the flat screen.
            attachments[0].srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
            attachments[1].srcColorBlendFactor=attachments[1].srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;attachments[1].dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        }else if(straight)for(auto& a:attachments){a.srcColorBlendFactor=a.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;} // shader now outputs premultiplied colour
        blend.attachmentCount=2;blend.pAttachments=attachments.data();
        auto ci=source;ci.pNext=&rendering;ci.pStages=stages.data();ci.pColorBlendState=&blend;
        const auto result=deviceFn<PFN_vkCreateGraphicsPipelines>(device,"vkCreateGraphicsPipelines")(device,VK_NULL_HANDLE,1,&ci,nullptr,&clone);
        deviceFn<PFN_vkDestroyShaderModule>(device,"vkDestroyShaderModule")(device,patchedModule,nullptr);patchedModule=VK_NULL_HANDLE;
        evr::vkCheck(result,"UI pipeline clone");
        {std::lock_guard<std::mutex> lock(stateMutex);state[device].pipelines[original]=clone;}
        note("ui_pipeline_clone_ready","\"original\":"+h(uint64_t(original))+",\"clone\":"+h(uint64_t(clone))+
            ",\"source_sha256\":"+jsonString(evr::head::sha256(code.data(),code.size()*4))+",\"additive_alpha_zero\":"+std::string(isAdditive?"true":"false")+",\"map\":"+std::string(map?"true":"false")+",\"premultiplied\":"+std::string(straight?"true":"false")+",\"bound\":false,\"color_attachments\":2");
    }catch(const std::exception& e){
        if(patchedModule)deviceFn<PFN_vkDestroyShaderModule>(device,"vkDestroyShaderModule")(device,patchedModule,nullptr);
        if(clone)deviceFn<PFN_vkDestroyPipeline>(device,"vkDestroyPipeline")(device,clone,nullptr);
        // 2026-10-06: menus bind pipelines outside the reviewed blends; record their blend so they can be reviewed.
        std::string blend;
        if(source.pColorBlendState&&source.pColorBlendState->attachmentCount&&source.pColorBlendState->pAttachments){
            const auto& b=source.pColorBlendState->pAttachments[0];
            blend=",\"blend\":["+std::to_string(b.blendEnable)+","+std::to_string(b.srcColorBlendFactor)+","+std::to_string(b.dstColorBlendFactor)+","+std::to_string(b.colorBlendOp)+","+
                std::to_string(b.srcAlphaBlendFactor)+","+std::to_string(b.dstAlphaBlendFactor)+","+std::to_string(b.alphaBlendOp)+","+std::to_string(b.colorWriteMask)+"]";
        }
        note("ui_pipeline_clone_rejected","\"original\":"+h(uint64_t(original))+",\"reason\":"+jsonString(e.what())+blend);
    }
}
void pipelineDestroyed(VkDevice device,VkPipeline original) noexcept {
    if(!enabled())return;VkPipeline clone=VK_NULL_HANDLE;
    {std::lock_guard<std::mutex> lock(stateMutex);auto d=state.find(device);if(d!=state.end()){
        auto p=d->second.pipelines.find(original);if(p!=d->second.pipelines.end()){clone=p->second;d->second.pipelines.erase(p);}
    }}
    if(clone){deviceFn<PFN_vkDestroyPipeline>(device,"vkDestroyPipeline")(device,clone,nullptr);
        note("ui_pipeline_clone_destroyed","\"original\":"+h(uint64_t(original))+",\"clone\":"+h(uint64_t(clone)));}
}
void shaderDestroyed(VkDevice device,VkShaderModule original){
    if(!enabled())return;std::lock_guard<std::mutex> lock(stateMutex);auto d=state.find(device);if(d!=state.end()){d->second.shaders.erase(original);d->second.seen.erase(original);}
}
void destroyDevice(VkDevice device) noexcept {
    if(!enabled())return;Device retired;
    {std::lock_guard<std::mutex> lock(stateMutex);auto d=state.find(device);if(d==state.end())return;retired=std::move(d->second);state.erase(d);}
    for(const auto& p:retired.pipelines)deviceFn<PFN_vkDestroyPipeline>(device,"vkDestroyPipeline")(device,p.second,nullptr);
    note("ui_pipeline_probe_cleanup","\"remaining_clones\":"+std::to_string(retired.pipelines.size())+",\"remaining_shader_records\":"+std::to_string(retired.shaders.size()));
}
}
VKAPI_ATTR void VKAPI_CALL evrDestroyPipeline(VkDevice device,VkPipeline pipeline,const VkAllocationCallbacks* allocator){
    uiCapture::pipelineDestroyed(device,pipeline);meshDrawGuard::destroyed(pipeline);giReprojectPipelineDestroyed(pipeline);pipelineCensus::pipelineDestroyed(pipeline);
    deviceFn<PFN_vkDestroyPipeline>(device,"vkDestroyPipeline")(device,pipeline,allocator);
}
VKAPI_ATTR void VKAPI_CALL evrDestroyShaderModule(VkDevice device,VkShaderModule module,const VkAllocationCallbacks* allocator){
    uiCapture::shaderDestroyed(device,module);giReprojectModuleDestroyed(module);pipelineCensus::shaderDestroyed(module);
    deviceFn<PFN_vkDestroyShaderModule>(device,"vkDestroyShaderModule")(device,module,allocator);
}
