// Local capture replay experiment. The source capture and live game are never modified.
static void compareCulling(IReplayController* controller,uint32_t event,
                           const std::filesystem::path& replacementPath,const std::filesystem::path& out,
                           uint32_t finalEvent,const std::vector<uint32_t>& previewTextures={},bool rawPreviews=false,
                           uint32_t secondaryEvent=0,const std::filesystem::path& secondaryPath={}) {
    controller->SetFrameEvent(event,true);
    const auto* pipe=controller->GetVulkanPipelineState();
    if(!pipe->computeShader.reflection)throw std::runtime_error("compute shader absent");
    const auto original=pipe->computeShader.resourceId;
    const auto entry=pipe->computeShader.reflection->entryPoint;
    const auto& source=pipe->computeShader.reflection->rawBytes;
    {std::ofstream file(out/"original.spv",std::ios::binary);file.write(reinterpret_cast<const char*>(source.data()),source.size());}
    std::ifstream input(replacementPath,std::ios::binary|std::ios::ate);
    if(!input)throw std::runtime_error("replacement input missing");
    const auto size=input.tellg();if(size<=0||size>1024*1024||size%4)throw std::runtime_error("replacement shader size");
    input.seekg(0);bytebuf bytes;bytes.resize(size_t(size));input.read(reinterpret_cast<char*>(bytes.data()),size);
    if(!input)throw std::runtime_error("replacement shader read failed");
    {std::ofstream file(out/"replacement.spv",std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("replacement evidence write failed");}
    std::ofstream report(out/"buffers.jsonl");
    std::ofstream images(out/"comparison-images.jsonl");
    auto snapshot=[&](const char* name){
        controller->SetFrameEvent(event,true);
        const auto* state=controller->GetVulkanPipelineState();uint64_t total=0;unsigned count=0;
        if(rawPreviews)for(auto index:previewTextures){
            const auto& textures=controller->GetTextures();
            if(index>=textures.size())throw std::runtime_error("background texture index");
            const auto& t=textures[index];
            if(t.format.Name()!="R8G8B8A8_UNORM"||t.width>4096||t.height>4096||t.depth!=1||t.arraysize!=1||t.msSamp!=1)
                throw std::runtime_error("background requires bounded RGBA8 UNORM");
            const auto data=controller->GetTextureData(t.resourceId,{});
            if(data.size()!=uint64_t(t.width)*t.height*4)throw std::runtime_error("background byte extent");
            const auto label=std::string(name)+"-background",filename=label+"_texture_"+std::to_string(index)+".bin";
            std::ofstream file(out/filename,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());
            if(!file)throw std::runtime_error("background write failed");
            images<<"{\"case\":"<<quote(label.c_str())<<",\"event\":"<<event<<",\"texture\":"<<index
                <<",\"format\":\"R8G8B8A8_UNORM\",\"width\":"<<t.width<<",\"height\":"<<t.height
                <<",\"file\":"<<quote(filename.c_str())<<"}\n";images.flush();
        }
        for(const auto& access:controller->GetDescriptorAccess()){
            if(access.stage!=ShaderStage::Compute||access.staticallyUnused||access.type==DescriptorType::Sampler)continue;
            const auto values=controller->GetDescriptors(access.descriptorStore,{DescriptorRange(access)});
            if(values.size()!=1)throw std::runtime_error("descriptor lookup mismatch");
            auto value=values[0];
            if(value.resource==ResourceId()||value.byteSize==0)continue;
            bool isBuffer=false;for(const auto& buffer:controller->GetBuffers())if(buffer.resourceId==value.resource){isBuffer=true;break;}
            if(!isBuffer)continue;
            for(const auto& set:state->compute.descriptorSets)if(set.descriptorSetResourceId==access.descriptorStore)
                for(const auto& offset:set.dynamicOffsets)if(offset.descriptorByteOffset==access.byteOffset)value.byteOffset+=offset.dynamicBufferByteOffset;
            total+=value.byteSize;
            if(++count>32||value.byteSize>32*1024*1024||total>128*1024*1024)throw std::runtime_error("buffer snapshot budget");
            auto data=controller->GetBufferData(value.resource,value.byteOffset,value.byteSize);
            if(data.size()!=value.byteSize)throw std::runtime_error("buffer snapshot short read");
            const auto filename=std::string(name)+"_"+std::to_string(uint32_t(access.type))+"_"+std::to_string(access.index)+".bin";
            std::ofstream file(out/filename,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());
            if(!file)throw std::runtime_error("buffer snapshot write failed");
            report<<"{\"case\":"<<quote(name)<<",\"event\":"<<event<<",\"type\":"<<uint32_t(access.type)
                  <<",\"index\":"<<access.index<<",\"offset\":"<<value.byteOffset<<",\"size\":"<<value.byteSize
                  <<",\"file\":"<<quote(filename.c_str())<<"}\n";
        }
        report.flush();std::cout<<"PASS "<<name<<" snapshot "<<count<<" buffers"<<std::endl;
        if(!previewTextures.empty()){
            controller->SetFrameEvent(finalEvent,true);const auto& textures=controller->GetTextures();
            for(auto index:previewTextures){
                if(index>=textures.size())throw std::runtime_error("comparison texture index");
                const auto& t=textures[index];
                if(t.width>4096||t.height>4096||t.depth!=1||t.arraysize!=1||t.msSamp!=1)throw std::runtime_error("comparison texture extent");
                TextureSave save;save.resourceId=t.resourceId;save.destType=FileType::PNG;save.mip=0;
                const auto filename=std::string(name)+"_texture_"+std::to_string(index)+".png";
                if(!controller->SaveTexture(save,(out/filename).string().c_str()).OK())throw std::runtime_error("comparison texture export");
                if(!rawPreviews&&t.format.Name()=="R32_UINT"){
                    const auto data=controller->GetTextureData(t.resourceId,{});
                    if(data.size()!=uint64_t(t.width)*t.height*4)throw std::runtime_error("integer comparison raw extent mismatch");
                    const auto rawName=std::string(name)+"_texture_"+std::to_string(index)+".bin";
                    std::ofstream raw(out/rawName,std::ios::binary);raw.write(reinterpret_cast<const char*>(data.data()),data.size());
                    if(!raw)throw std::runtime_error("integer comparison raw write failed");
                    images<<"{\"case\":"<<quote(name)<<",\"event\":"<<finalEvent<<",\"texture\":"<<index
                        <<",\"format\":\"R32_UINT\",\"width\":"<<t.width<<",\"height\":"<<t.height
                        <<",\"file\":"<<quote(rawName.c_str())<<"}\n";images.flush();
                }
                if(rawPreviews){
                    if(t.format.Name()!="R8G8B8A8_UNORM")throw std::runtime_error("raw comparison requires RGBA8 UNORM");
                    const auto data=controller->GetTextureData(t.resourceId,{});
                    if(data.size()!=uint64_t(t.width)*t.height*4)throw std::runtime_error("comparison raw extent mismatch");
                    const auto rawName=std::string(name)+"_texture_"+std::to_string(index)+".bin";
                    std::ofstream raw(out/rawName,std::ios::binary);raw.write(reinterpret_cast<const char*>(data.data()),data.size());
                    if(!raw)throw std::runtime_error("comparison raw write failed");
                    images<<"{\"case\":"<<quote(name)<<",\"event\":"<<finalEvent<<",\"texture\":"<<index
                        <<",\"format\":\"R8G8B8A8_UNORM\",\"width\":"<<t.width<<",\"height\":"<<t.height
                        <<",\"file\":"<<quote(rawName.c_str())<<",\"png\":"<<quote(filename.c_str())<<"}\n";images.flush();
                }
            }
        }
    };
    snapshot("before");
    if(rawPreviews)snapshot("control");
    const auto built=controller->BuildTargetShader(entry,ShaderEncoding::SPIRV,bytes,{},ShaderStage::Compute);
    if(built.first==ResourceId())throw std::runtime_error("replay shader build failed: "+std::string(built.second.c_str()));
    struct Replacement {
        IReplayController* controller;ResourceId original,replacement;bool active=false;
        ~Replacement(){if(active)controller->RemoveReplacement(original);if(replacement!=ResourceId())controller->FreeTargetResource(replacement);}
    } owner{controller,original,built.first};
    Replacement secondary{controller,{},{} }; // Removed even if either snapshot throws.
    if(secondaryEvent){
        controller->SetFrameEvent(secondaryEvent,true);
        const auto* state=controller->GetVulkanPipelineState();
        if(!state->computeShader.reflection)throw std::runtime_error("secondary compute shader absent");
        secondary.original=state->computeShader.resourceId;
        if(secondary.original==original)throw std::runtime_error("secondary shader aliases primary");
        std::ifstream input(secondaryPath,std::ios::binary|std::ios::ate);const auto size=input.tellg();
        if(!input||size<=0||size>1024*1024||size%4)throw std::runtime_error("secondary shader size");
        input.seekg(0);bytebuf data;data.resize(size_t(size));input.read(reinterpret_cast<char*>(data.data()),size);
        if(!input)throw std::runtime_error("secondary shader read");
        {std::ofstream file(out/"secondary-replacement.spv",std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());}
        const auto result=controller->BuildTargetShader(state->computeShader.reflection->entryPoint,ShaderEncoding::SPIRV,data,{},ShaderStage::Compute);
        if(result.first==ResourceId())throw std::runtime_error("secondary shader build failed");
        secondary.replacement=result.first;secondary.active=true;controller->ReplaceResource(secondary.original,secondary.replacement);
    }
    owner.active=true;controller->ReplaceResource(original,built.first);
    snapshot("after");
    controller->RemoveReplacement(original);owner.active=false;
    if(secondary.active){controller->RemoveReplacement(secondary.original);secondary.active=false;}
    snapshot("restored");
    if(rawPreviews)snapshot("restored_again");
    std::cout<<"PASS offline shader replacement removed; capture unmodified"<<std::endl;
}
