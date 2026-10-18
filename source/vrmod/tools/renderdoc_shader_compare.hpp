// Bounded offline vertex-shader experiments. The capture remains immutable.
#include <iomanip>
static void compareVertexShaders(IReplayController* controller,const std::filesystem::path& planPath,
                                 const std::filesystem::path& out,uint32_t finalEvent,
                                 const std::vector<uint32_t>& textures){
    struct Entry{ResourceId original{},replacement{};bool active=false;};
    std::vector<Entry> entries;std::vector<uint32_t> meshEvents,indirectEvents;
    struct BufferRequest{uint32_t event,index;};std::vector<BufferRequest> bufferRequests;
    struct Cleanup{IReplayController* c;std::vector<Entry>& entries;~Cleanup(){for(auto& e:entries){if(e.active)c->RemoveReplacement(e.original);if(e.replacement!=ResourceId())c->FreeTargetResource(e.replacement);}}}cleanup{controller,entries};
    std::ifstream plan(planPath);if(!plan)throw std::runtime_error("shader plan absent");
    std::ofstream metadata(out/"shader-comparison.jsonl");std::string line;unsigned count=0;
    while(std::getline(plan,line)){
        if(line.empty()||line[0]=='#')continue;
        if(line.rfind("indirect ",0)==0){
            std::istringstream row(line.substr(9));uint32_t event=0;std::string trailing;
            if(!(row>>event)||(row>>trailing)||indirectEvents.size()>=4)throw std::runtime_error("bounded indirect count plan syntax");
            indirectEvents.push_back(event);continue;
        }
        if(line.rfind("buffer ",0)==0){
            std::istringstream row(line.substr(7));BufferRequest r{};std::string trailing;
            if(!(row>>r.event>>r.index)||(row>>trailing)||bufferRequests.size()>=4)throw std::runtime_error("bounded vertex buffer plan syntax");
            bufferRequests.push_back(r);continue;
        }
        if(line.rfind("mesh ",0)==0){
            std::istringstream row(line.substr(5));uint32_t event=0;std::string trailing;
            if(!(row>>event)||(row>>trailing)||meshEvents.size()>=4)throw std::runtime_error("bounded mesh plan syntax");
            meshEvents.push_back(event);continue;
        }
        std::istringstream row(line);uint32_t event=0;std::string filename,trailing;
        if(!(row>>event>>std::quoted(filename))||(row>>trailing)||++count>4)throw std::runtime_error("bounded shader plan syntax");
        controller->SetFrameEvent(event,true);const auto* pipe=controller->GetVulkanPipelineState();
        if(!pipe->vertexShader.reflection||pipe->geometryShader.reflection||pipe->tessEvalShader.reflection||pipe->tessControlShader.reflection)
            throw std::runtime_error("experiment requires vertex as final geometry stage");
        const auto& shader=*pipe->vertexShader.reflection;
        const auto original=pipe->vertexShader.resourceId;
        for(const auto& e:entries)if(e.original==original)throw std::runtime_error("duplicate shader replacement");
        std::ifstream input(filename,std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("replacement shader missing");
        const auto size=input.tellg();if(size<=0||size>1024*1024||size%4)throw std::runtime_error("replacement shader bounds");
        input.seekg(0);bytebuf bytes;bytes.resize(size_t(size));input.read(reinterpret_cast<char*>(bytes.data()),size);if(!input)throw std::runtime_error("short shader read");
        const auto prefix=std::to_string(count);
        {std::ofstream f(out/("original_"+prefix+".spv"),std::ios::binary);f.write(reinterpret_cast<const char*>(shader.rawBytes.data()),shader.rawBytes.size());if(!f)throw std::runtime_error("source shader evidence write");}
        {std::ofstream f(out/("replacement_"+prefix+".spv"),std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)throw std::runtime_error("replacement shader evidence write");}
        const auto built=controller->BuildTargetShader(shader.entryPoint,ShaderEncoding::SPIRV,bytes,{},ShaderStage::Vertex);
        if(built.first==ResourceId())throw std::runtime_error("vertex shader build failed: "+std::string(built.second.c_str()));
        entries.push_back({original,built.first,false});
        metadata<<"{\"entry\":"<<count<<",\"event\":"<<event<<",\"stage\":\"vertex\",\"file\":"<<quote(filename.c_str())<<"}\n";metadata.flush();
    }
    if(entries.empty()||textures.empty()||textures.size()>4)throw std::runtime_error("empty/bounded shader image experiment");
    auto snapshot=[&](const char* label){
        controller->SetFrameEvent(finalEvent,true);const auto& all=controller->GetTextures();
        for(auto index:textures){
            if(index>=all.size())throw std::runtime_error("unknown texture index");const auto& t=all[index];
            if(t.width>4096||t.height>4096||t.depth!=1||t.arraysize!=1||t.msSamp!=1||t.format.Name()!="R8G8B8A8_UNORM")
                throw std::runtime_error("shader image experiment requires bounded RGBA8 UNORM");
            const auto data=controller->GetTextureData(t.resourceId,{});if(data.size()!=uint64_t(t.width)*t.height*4)throw std::runtime_error("texture byte extent");
            const auto stem=std::string(label)+"_texture_"+std::to_string(index);
            std::ofstream f(out/(stem+".bin"),std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());if(!f)throw std::runtime_error("texture evidence write");
            TextureSave save;save.resourceId=t.resourceId;save.destType=FileType::PNG;save.mip=0;
            if(!controller->SaveTexture(save,(out/(stem+".png")).string().c_str()).OK())throw std::runtime_error("texture PNG export");
            metadata<<"{\"case\":"<<quote(label)<<",\"texture\":"<<index<<",\"width\":"<<t.width<<",\"height\":"<<t.height<<",\"file\":"<<quote((stem+".bin").c_str())<<"}\n";
        }
        for(auto event:meshEvents){
            controller->SetFrameEvent(event,true);const auto mesh=controller->GetPostVSData(0,0,MeshDataStage::VSOut);
            if(mesh.vertexResourceId==ResourceId()||!mesh.vertexByteStride||!mesh.numIndices)throw std::runtime_error("mesh data absent");
            uint64_t maxVertex=mesh.numIndices-1;bytebuf indices;
            if(mesh.indexResourceId!=ResourceId()&&mesh.indexByteStride){
                const uint64_t size=uint64_t(mesh.numIndices)*mesh.indexByteStride;
                if(size>32*1024*1024||mesh.indexByteStride>4)throw std::runtime_error("mesh index budget");
                indices=controller->GetBufferData(mesh.indexResourceId,mesh.indexByteOffset,size);if(indices.size()!=size)throw std::runtime_error("short mesh indices");
                maxVertex=0;
                for(size_t n=0;n<indices.size();n+=mesh.indexByteStride){uint32_t index=0;std::memcpy(&index,indices.data()+n,mesh.indexByteStride);
                    if(index!=UINT32_MAX&&int64_t(index)+mesh.baseVertex>=0)maxVertex=std::max(maxVertex,uint64_t(int64_t(index)+mesh.baseVertex));}
            }
            const uint64_t size=(maxVertex+1)*mesh.vertexByteStride;if(size>32*1024*1024)throw std::runtime_error("mesh vertex budget");
            const auto vertices=controller->GetBufferData(mesh.vertexResourceId,mesh.vertexByteOffset,size);if(vertices.size()!=size)throw std::runtime_error("short mesh vertices");
            const auto stem=std::string(label)+"_mesh_"+std::to_string(event);
            {std::ofstream f(out/(stem+"_vertices.bin"),std::ios::binary);f.write(reinterpret_cast<const char*>(vertices.data()),vertices.size());if(!f)throw std::runtime_error("mesh vertex write");}
            {std::ofstream f(out/(stem+"_indices.bin"),std::ios::binary);f.write(reinterpret_cast<const char*>(indices.data()),indices.size());if(!f)throw std::runtime_error("mesh index write");}
            metadata<<"{\"case\":"<<quote(label)<<",\"mesh_event\":"<<event<<",\"vertex_stride\":"<<mesh.vertexByteStride<<",\"index_stride\":"<<mesh.indexByteStride<<",\"indices\":"<<mesh.numIndices<<",\"base_vertex\":"<<mesh.baseVertex<<",\"component_count\":"<<unsigned(mesh.format.compCount)<<",\"component_bytes\":"<<unsigned(mesh.format.compByteWidth)<<",\"component_type\":"<<unsigned(mesh.format.compType)<<",\"vertices_file\":"<<quote((stem+"_vertices.bin").c_str())<<"}\n";
        }
        uint64_t totalBytes=0;
        for(const auto& r:bufferRequests){
            controller->SetFrameEvent(r.event,true);const auto* pipe=controller->GetVulkanPipelineState();unsigned matches=0;
            for(const auto& access:controller->GetDescriptorAccess()){
                if(access.stage!=ShaderStage::Vertex||access.staticallyUnused||uint32_t(access.type)!=9||access.index!=r.index||access.arrayElement)continue;
                if(++matches!=1)throw std::runtime_error("ambiguous requested vertex buffer");
                const auto descriptors=controller->GetDescriptors(access.descriptorStore,{DescriptorRange(access)});
                if(descriptors.size()!=1)throw std::runtime_error("vertex buffer descriptor result");auto d=descriptors[0];
                for(const auto& set:pipe->graphics.descriptorSets)if(set.descriptorSetResourceId==access.descriptorStore)
                    for(const auto& offset:set.dynamicOffsets)if(offset.descriptorByteOffset==access.byteOffset)d.byteOffset+=offset.dynamicBufferByteOffset;
                if(d.resource==ResourceId()||!d.byteSize||d.byteSize>2*1024*1024||(totalBytes+=d.byteSize)>8*1024*1024)throw std::runtime_error("bounded vertex buffer extent");
                const auto bytes=controller->GetBufferData(d.resource,d.byteOffset,d.byteSize);if(bytes.size()!=d.byteSize)throw std::runtime_error("short vertex buffer");
                const auto name=std::string(label)+"_buffer_"+std::to_string(r.event)+"_"+std::to_string(r.index)+".bin";
                std::ofstream f(out/name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)throw std::runtime_error("vertex buffer evidence write");
                metadata<<"{\"case\":"<<quote(label)<<",\"buffer_event\":"<<r.event<<",\"vertex_resource_index\":"<<r.index<<",\"offset\":"<<d.byteOffset<<",\"size\":"<<d.byteSize<<",\"file\":"<<quote(name.c_str())<<"}\n";
            }
            if(matches!=1)throw std::runtime_error("requested vertex buffer absent");
        }
        for(auto event:indirectEvents){
            uint32_t chunk=UINT32_MAX;unsigned matches=0;
            std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const rdcarray<ActionDescription>& actions){for(const auto& a:actions){for(const auto& e:a.events)if(e.eventId==event){chunk=e.chunkIndex;++matches;}visit(a.children);}};
            visit(controller->GetRootActions());const auto& file=controller->GetStructuredFile();
            if(matches!=1||chunk>=file.chunks.size())throw std::runtime_error("indirect event/chunk absent or ambiguous");
            const auto& call=*file.chunks[chunk];
            if(call.name!="vkCmdDrawIndexedIndirectCount")throw std::runtime_error("expected native indexed indirect-count action");
            const auto* countBuffer=call.FindChild("countBuffer");const auto* offset=call.FindChild("countBufferOffset");const auto* maximum=call.FindChild("maxDrawCount");
            if(!countBuffer||countBuffer->type.basetype!=SDBasic::Resource||!offset||!maximum||maximum->AsUInt64()>65536)throw std::runtime_error("indirect count structured parameters changed");
            controller->SetFrameEvent(event,true);
            const auto bytes=controller->GetBufferData(countBuffer->AsResourceId(),offset->AsUInt64(),4);
            if(bytes.size()!=4)throw std::runtime_error("short indirect GPU count read");uint32_t count=0;std::memcpy(&count,bytes.data(),4);
            metadata<<"{\"case\":"<<quote(label)<<",\"indirect_event\":"<<event<<",\"gpu_count\":"<<count<<",\"max_draw_count\":"<<maximum->AsUInt64()<<",\"count_offset\":"<<offset->AsUInt64()<<"}\n";
        }
        metadata.flush();std::cout<<"PASS "<<label<<" image snapshot\n"<<std::flush;
    };
    snapshot("before");snapshot("control");
    for(auto& e:entries){e.active=true;controller->ReplaceResource(e.original,e.replacement);}
    snapshot("after");
    for(auto& e:entries){controller->RemoveReplacement(e.original);e.active=false;}
    snapshot("restored");snapshot("restored_again");
    std::cout<<"PASS vertex replacements removed; capture unmodified\n";
}
