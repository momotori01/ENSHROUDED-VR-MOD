// Headless, local-only RenderDoc 1.46 API client. No layer registration or global hook calls.
#include <windows.h>
#include <dbghelp.h>
#include <renderdoc_replay.h>
#include <vk_pipestate.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <functional>
#include <chrono>
#include <thread>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cmath>
REPLAY_PROGRAM_MARKER();
static std::wstring dumpPath;
static LONG WINAPI crashDump(EXCEPTION_POINTERS* exception){
    HANDLE file=CreateFileW(dumpPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(),exception,FALSE};
        MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),file,MiniDumpNormal,&info,nullptr,nullptr);CloseHandle(file);}
    return EXCEPTION_EXECUTE_HANDLER;
}

static std::string quote(const char* p) {
    std::string s="\"";
    for (;*p;p++) { unsigned char c=*p; if(c=='"'||c=='\\') {s+='\\';s+=c;} else if(c=='\n')s+="\\n"; else if(c=='\r')s+="\\r"; else if(c=='\t')s+="\\t"; else if(c>=32)s+=c; }
    return s+'"';
}
struct ReplayLifetime { ReplayLifetime(){ RENDERDOC_InitialiseReplay({},{}); } ~ReplayLifetime(){RENDERDOC_ShutdownReplay();} };
#include "renderdoc_culling.hpp"
template<class T> struct Owner { T* p=nullptr; ~Owner(){if(p)p->Shutdown();} };
#include "renderdoc_ui.hpp"
#include "renderdoc_shader_compare.hpp"
static void debugVariable(std::ostream& out,const ShaderVariable& value,unsigned depth=0){
    if(depth>16)throw std::runtime_error("debug variable nesting budget exceeded");
    out<<"{\"name\":"<<quote(value.name.c_str())<<",\"type\":"<<uint32_t(value.type)
       <<",\"rows\":"<<uint32_t(value.rows)<<",\"columns\":"<<uint32_t(value.columns)<<",\"words\":[";
    for(unsigned n=0;n<16;n++){if(n)out<<',';out<<value.value.u32v[n];}out<<"],\"members\":[";
    for(size_t n=0;n<value.members.size();n++){if(n)out<<',';debugVariable(out,value.members[n],depth+1);}out<<"]}";
}
static void structuredObject(std::ostream& out,const SDObject& object,unsigned& budget,unsigned depth=0){
    if(depth>24||!budget--)throw std::runtime_error("structured API export exceeds node budget");
    out<<"{\"name\":"<<quote(object.name.c_str())<<",\"type\":"<<quote(object.type.name.c_str())
       <<",\"basic\":"<<uint32_t(object.type.basetype);
    switch(object.type.basetype){
    case SDBasic::UnsignedInteger:case SDBasic::Enum:case SDBasic::Resource:case SDBasic::GPUAddress:
        out<<",\"value\":"<<quote(std::to_string(object.AsUInt64()).c_str());break;
    case SDBasic::SignedInteger:out<<",\"value\":"<<quote(std::to_string(object.AsInt64()).c_str());break;
    case SDBasic::Float:out<<",\"value\":"<<quote(std::to_string(object.AsDouble()).c_str());break;
    case SDBasic::Boolean:case SDBasic::Character:out<<",\"value\":"<<unsigned(object.AsUInt8());break;
    case SDBasic::String:out<<",\"value\":"<<quote(object.AsString().c_str());break;
    default:break; // Opaque buffers are intentionally not exported.
    }
    out<<",\"children\":[";
    for(size_t n=0;n<object.NumChildren();n++){if(n)out<<',';structuredObject(out,*object.GetChild(n),budget,depth+1);}
    out<<"]}";
}

int main(int argc,char** argv) {
    if(argc<3) {std::cerr<<"usage: evr_renderdoc <absolute renderdoc.dll> version|replay <capture.rdc> <output>\n";return 2;}
    auto dll=std::filesystem::absolute(argv[1]);
    // Delay imports resolve to this explicit local DLL, not a system-wide installation.
    if(!LoadLibraryExW(dll.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH)){std::cerr<<"FAIL load "<<GetLastError()<<'\n';return 1;}
    try {
        ReplayLifetime lifetime;
        dumpPath=(std::filesystem::current_path()/"vrmod/output/renderdoc-helper-crash.dmp").wstring();
        SetUnhandledExceptionFilter(crashDump);
        if(std::string(argv[2])=="version") {std::cout<<"PASS RenderDoc replay initialized "<<RENDERDOC_GetCommitHash()<<'\n';return 0;}
        const bool control=std::string(argv[2])=="control"&&argc==6;
        if((std::string(argv[2])=="capture"&&argc==8)||control){
            const int delay=std::stoi(argv[control?5:7]);if(delay<0||delay>120)return 2;
            std::filesystem::path out=argv[control?4:6];std::filesystem::create_directories(out);
            uint32_t ident=0;
            if(control)ident=uint32_t(std::stoul(argv[3]));else{
                rdcarray<EnvironmentModification> environment;
                for(const char* key:{"VK_LAYER_PATH","VK_INSTANCE_LAYERS","ENABLE_VULKAN_RENDERDOC_CAPTURE","EVR_LAYER_OUTPUT","SteamAppId","SteamGameId","EVR_PREPARE_OPENXR","EVR_PREPARE_SESSION_FIXTURE","VK_LOADER_DEBUG"}){
                    auto name=std::string("EVR_RD_CHILD_")+key;const char* value=std::getenv(name.c_str());
                    if(value)environment.push_back(EnvironmentModification(EnvMod::Set,EnvSep::NoSep,key,value));
                }
                auto launched=RENDERDOC_ExecuteAndInject(argv[3],argv[4],argv[5],environment,(out/"game").string().c_str(),{},false);
                if(!launched.result.OK()){std::cerr<<"FAIL ExecuteAndInject "<<int(launched.result.code)<<'\n';return 1;}ident=launched.ident;
            }
            Owner<ITargetControl> target{RENDERDOC_CreateTargetControl("",ident,"Enshrouded VR local diagnostics",false)};
            if(!target.p||!target.p->Connected()){std::cerr<<"FAIL target connection\n";return 1;}
            {std::ofstream launch(out/"renderdoc-target.json");launch<<"{\"pid\":"<<target.p->GetPID()<<",\"ident\":"<<ident<<"}\n";}
            std::cout<<"Connected PID "<<target.p->GetPID()<<std::endl;
            auto start=std::chrono::steady_clock::now();bool requested=false;
            while(target.p->Connected()&&std::chrono::steady_clock::now()-start<std::chrono::seconds(delay+120)){
                if(!requested&&std::chrono::steady_clock::now()-start>=std::chrono::seconds(delay)){target.p->TriggerCapture(1);requested=true;}
                auto msg=target.p->ReceiveMessage(nullptr);
                if(msg.type==TargetControlMessageType::NewCapture){
                    std::ofstream data(out/"capture.json");data<<"{\"path\":"<<quote(msg.newCapture.path.c_str())<<",\"frame\":"<<msg.newCapture.frameNumber<<",\"bytes\":"<<msg.newCapture.byteSize<<"}\n";
                    std::cout<<"PASS captured frame "<<msg.newCapture.frameNumber<<std::endl;return 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            std::cerr<<"FAIL capture timeout or disconnected; target left running for diagnosis\n";return 1;
        }
        const bool layoutsOnly=std::string(argv[2])=="layouts";
        const bool shaderImagesMode=std::string(argv[2])=="shader-images";
        const bool eventMode=std::string(argv[2])=="events";
        const bool historyMode=std::string(argv[2])=="history";
        const bool bufferSnapshots=std::string(argv[2])=="buffers";
        const bool rasterOnly=std::string(argv[2])=="raster";
        const bool bindingsMode=std::string(argv[2])=="bindings"||bufferSnapshots||rasterOnly;
        const bool texelsMode=std::string(argv[2])=="texels";
        const bool texturesMode=std::string(argv[2])=="textures";
        const bool debugGiPixelMode=std::string(argv[2])=="debug-gi-pixel";
        const bool debugGiRayMode=std::string(argv[2])=="debug-gi-ray";
        const bool debugThreadMode=std::string(argv[2])=="debug-thread"||debugGiPixelMode||debugGiRayMode;
        const bool usageOnly=std::string(argv[2])=="usage";
        const bool bufferUsageMode=std::string(argv[2])=="buffer-usage";
        const bool bufferDataMode=std::string(argv[2])=="buffer-data";
        const bool apiMode=std::string(argv[2])=="api";
        const bool memoryMode=std::string(argv[2])=="memory";
        // 2026-10-16: every event's GPU time on replay (EventGPUDuration) - which command made a frame of the Mock take
        // 300 ms (`durations <capture> <out>`, durations.jsonl).
        const bool durationsMode=std::string(argv[2])=="durations";
        const bool computeCompare=std::string(argv[2])=="compute-image-compare";
        const bool uiPassMode=std::string(argv[2])=="clear-ui-pass";
        const bool pairedCompute=std::string(argv[2])=="compute-pair-compare";
        const bool cullingMode=std::string(argv[2])=="culling-compare"||computeCompare||pairedCompute;
        if((std::string(argv[2])!="replay"&&!durationsMode&&!layoutsOnly&&!eventMode&&!historyMode&&!bindingsMode&&!texelsMode&&!texturesMode&&!debugThreadMode&&!usageOnly&&!bufferUsageMode&&!bufferDataMode&&!apiMode&&!memoryMode&&!cullingMode&&!uiPassMode&&!shaderImagesMode)||(shaderImagesMode?argc!=7:cullingMode?(pairedCompute?argc!=10:computeCompare?argc!=9:(argc!=7&&argc!=8)):argc!=((eventMode||historyMode||bindingsMode||texelsMode||texturesMode||debugThreadMode||apiMode||bufferUsageMode||bufferDataMode||uiPassMode)?6:5)))return 2;
        std::filesystem::path out=argv[4];std::filesystem::create_directories(out);
        Owner<ICaptureFile> capture{RENDERDOC_OpenCaptureFile()};
        auto status=capture.p->OpenFile(argv[3],"rdc",nullptr);
        if(!status.OK()){std::cerr<<"FAIL OpenFile "<<int(status.code)<<'\n';return 1;}
        auto result=capture.p->OpenCapture({},nullptr);
        if(!result.first.OK()){std::cerr<<"FAIL OpenCapture "<<int(result.first.code)<<'\n';return 1;}
        Owner<IReplayController> controller{result.second};
        const auto& textures=controller.p->GetTextures();
        // Indices are controller-local; export the actual inventory for every
        // inspection rather than reusing an index from an earlier capture.
        {std::ofstream inventory(out/"buffers.jsonl");const auto& buffers=controller.p->GetBuffers();
            // id: the numeric ResourceId that the API export prints (2026-10-19, to find an indirect draw's argument buffer)
            for(size_t n=0;n<buffers.size();n++){uint64_t id=0;static_assert(sizeof(buffers[n].resourceId)==8,"ResourceId size");std::memcpy(&id,&buffers[n].resourceId,8);
                inventory<<"{\"index\":"<<n<<",\"bytes\":"<<buffers[n].length<<",\"id\":"<<id<<"}\n";}
            if(!inventory)throw std::runtime_error("buffer inventory export failed");}
        auto index=[&](ResourceId id)->int {for(size_t i=0;i<textures.size();++i)if(textures[i].resourceId==id)return int(i);return -1;};
        std::ofstream actions(out/"actions.jsonl");uint32_t last=0,draws=0,dispatches=0;
        std::unordered_map<uint32_t,const ActionDescription*> actionById;
        std::function<void(const rdcarray<ActionDescription>&)> walk=[&](const auto& array){for(const auto& a:array){
            actionById[a.eventId]=&a;
            last=std::max(last,a.eventId);if(a.flags&ActionFlags::Drawcall)draws++;if(a.flags&ActionFlags::Dispatch)dispatches++;
            actions<<"{\"event\":"<<a.eventId<<",\"flags\":"<<uint32_t(a.flags)<<",\"name\":"<<quote(a.customName.c_str())<<",\"num_indices\":"<<a.numIndices<<",\"num_instances\":"<<a.numInstances<<",\"base_vertex\":"<<a.baseVertex<<",\"index_offset\":"<<a.indexOffset<<",\"instance_offset\":"<<a.instanceOffset<<",\"outputs\":[";
            for(size_t n=0;n<8;n++){if(n)actions<<',';actions<<index(a.outputs[n]);}actions<<"],\"copy_source\":"<<index(a.copySource)<<",\"copy_destination\":"<<index(a.copyDestination)
                <<",\"dispatch\":["<<a.dispatchDimension[0]<<','<<a.dispatchDimension[1]<<','<<a.dispatchDimension[2]<<"]}\n";walk(a.children);
        }};walk(controller.p->GetRootActions());
        if(durationsMode){
            const auto results=controller.p->FetchCounters({GPUCounter::EventGPUDuration});
            std::ofstream report(out/"durations.jsonl");double total=0;
            for(const auto& r:results){const double s=r.value.d;total+=s;report<<"{\"event\":"<<r.eventId<<",\"ms\":"<<s*1000.0<<"}\n";}
            if(!report)throw std::runtime_error("durations export failed");
            std::cout<<"PASS local GPU durations "<<results.size()<<" total_ms "<<total*1000.0<<'\n';return 0;
        }
        if(shaderImagesMode){
            std::vector<uint32_t> previews;std::istringstream input(argv[6]);std::string token;
            while(std::getline(input,token,',')){size_t n=0;auto i=std::stoul(token,&n);
                if(n!=token.size()||i>=textures.size()||previews.size()>=4)throw std::runtime_error("bounded valid image indices required");previews.push_back(uint32_t(i));}
            compareVertexShaders(controller.p,argv[5],out,last,previews);return 0;
        }
        if(uiPassMode){
            size_t n=0;const auto event=std::stoul(argv[5],&n);
            if(n!=std::string(argv[5]).size()||event>UINT32_MAX||!actionById.count(uint32_t(event)))throw std::runtime_error("invalid UI event");
            const auto& draw=*actionById.at(uint32_t(event));
            if(!(draw.flags&ActionFlags::Drawcall)||index(draw.outputs[0])<0)throw std::runtime_error("UI selection must be a color draw");
            uint32_t begin=0;
            for(const auto& a:actionById)if(a.first<event&&(a.second->flags&ActionFlags::BeginPass)&&
                !(a.second->flags&ActionFlags::CommandBufferBoundary)&&a.second->outputs[0]==draw.outputs[0])begin=std::max(begin,a.first);
            if(!begin)throw std::runtime_error("UI pass begin missing");
            for(const auto& a:actionById)if(a.first>begin&&a.first<event&&(a.second->flags&ActionFlags::EndPass))throw std::runtime_error("UI pass already ended");
            isolateUiPass(controller.p,draw,begin,out);return 0;
        }
        if(bufferDataMode){
            const auto& buffers=controller.p->GetBuffers();
            std::istringstream input(argv[5]);std::string token;unsigned count=0;uint64_t totalBytes=0;uint32_t lastBufferEvent=0;bool haveBufferEvent=false;
            std::ofstream report(out/"buffer-data.jsonl");
            while(std::getline(input,token,',')){
                if(++count>16)throw std::runtime_error("at most 16 buffer ranges");
                std::replace(token.begin(),token.end(),':',' ');std::istringstream fields(token);
                uint64_t event=0,buffer=0,offset=0,size=0;std::string trailing;
                if(!(fields>>event>>buffer>>offset>>size)||(fields>>trailing)||event>UINT32_MAX||
                   !actionById.count(uint32_t(event))||buffer>=buffers.size()||!size||size>32*1024*1024||
                   offset>buffers[size_t(buffer)].length||size>buffers[size_t(buffer)].length-offset||
                   (totalBytes+=size)>128*1024*1024)throw std::runtime_error("bounded event:buffer:offset:bytes required");
                // Adjacent ranges at the same event must share one replay: an
                // additional SetFrameEvent can reshuffle native atomic queues.
                if(!haveBufferEvent||lastBufferEvent!=uint32_t(event)){
                    controller.p->SetFrameEvent(uint32_t(event),true);lastBufferEvent=uint32_t(event);haveBufferEvent=true;
                }
                const auto bytes=controller.p->GetBufferData(buffers[size_t(buffer)].resourceId,offset,size);
                if(bytes.size()!=size)throw std::runtime_error("short buffer range read");
                const auto name="event_"+std::to_string(event)+"_buffer_"+std::to_string(buffer)+"_offset_"+std::to_string(offset)+".bin";
                std::ofstream file(out/name,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                if(!file)throw std::runtime_error("buffer range write failed");
                report<<"{\"event\":"<<event<<",\"buffer\":"<<buffer<<",\"offset\":"<<offset<<",\"bytes\":"<<size<<",\"file\":"<<quote(name.c_str())<<"}\n";
                report.flush();
            }
            if(!count||!report)throw std::runtime_error("buffer range export failed");
            std::cout<<"PASS local buffer ranges exported "<<count<<'\n';return 0;
        }
        if(bufferUsageMode){
            const auto& buffers=controller.p->GetBuffers();
            std::istringstream input(argv[5]);std::string token;unsigned count=0;
            std::ofstream report(out/"buffer-usage.jsonl");
            while(std::getline(input,token,',')){
                size_t consumed=0;const auto idx=std::stoul(token,&consumed);
                if(++count>16||consumed!=token.size()||idx>=buffers.size())throw std::runtime_error("at most 16 valid buffer usage indices");
                const auto& b=buffers[idx];
                for(const auto& u:controller.p->GetUsage(b.resourceId))
                    report<<"{\"buffer\":"<<idx<<",\"bytes\":"<<b.length<<",\"event\":"<<u.eventId<<",\"usage\":"<<uint32_t(u.usage)
                        <<",\"compute_read_write\":"<<(u.usage==ResourceUsage::CS_RWResource?"true":"false")<<"}\n";
            }
            if(!count||!report)throw std::runtime_error("buffer usage export failed");
            std::cout<<"PASS local buffer usage exports "<<count<<'\n';return 0;
        }
        if(cullingMode){
            size_t consumed=0;const auto event=uint32_t(std::stoul(argv[5],&consumed));
            if(consumed!=std::string(argv[5]).size()||!actionById.count(event)||!(actionById.at(event)->flags&ActionFlags::Dispatch))
                throw std::runtime_error("culling comparison requires a captured dispatch event");
            std::vector<uint32_t> previews;
            if(computeCompare){
                if(std::string(argv[7])!="present")throw std::runtime_error("compute image comparison requires present selector");
                int resolved=-1;
                for(const auto& a:actionById)if(a.second->flags&ActionFlags::Present){
                    const int i=index(a.second->copyDestination);
                    if(i<0)throw std::runtime_error("present image missing");
                    // RenderDoc's End of Capture repeats the same present image.
                    if(resolved>=0&&resolved!=i)throw std::runtime_error("multiple different present textures");resolved=i;
                }
                size_t n=0;const auto selected=std::stoul(argv[8],&n);
                if(resolved<0||n!=std::string(argv[8]).size()||selected>UINT32_MAX||!actionById.count(uint32_t(selected))||selected<event)
                    throw std::runtime_error("invalid comparison event or ambiguous present");
                previews.push_back(uint32_t(resolved));
                last=uint32_t(selected);
            }else if(argc==8||pairedCompute){std::istringstream input(argv[7]);std::string token;
                while(std::getline(input,token,',')){size_t n=0;auto i=std::stoul(token,&n);
                    if(n!=token.size()||i>=textures.size()||previews.size()>=4)throw std::runtime_error("at most four valid comparison texture indices");previews.push_back(uint32_t(i));}}
            uint32_t secondary=0;
            if(pairedCompute){size_t n=0;secondary=uint32_t(std::stoul(argv[8],&n));
                if(n!=std::string(argv[8]).size()||!actionById.count(secondary)||!(actionById.at(secondary)->flags&ActionFlags::Dispatch))
                    throw std::runtime_error("secondary event must be a captured dispatch");}
            compareCulling(controller.p,event,argv[6],out,last,previews,computeCompare,secondary,pairedCompute?argv[9]:"");return 0;
        }
        if(memoryMode){
            const auto& file=controller.p->GetStructuredFile();std::ofstream report(out/"memory-api.jsonl");unsigned count=0;
            for(size_t n=0;n<file.chunks.size();n++){
                const auto& chunk=*file.chunks[n];const std::string name=chunk.name.c_str();
                if(name!="vkBindImageMemory"&&name!="vkBindImageMemory2"&&name!="vkBindImageMemory2KHR"&&
                   name!="vkBindBufferMemory"&&name!="vkBindBufferMemory2"&&name!="vkBindBufferMemory2KHR"&&
                   name!="vkAllocateMemory"&&name!="vkFreeMemory"&&name!="vkDestroyImage")continue;
                if(++count>100000)throw std::runtime_error("memory chunk budget exceeded");
                report<<"{\"chunk_index\":"<<n<<",\"chunk\":";unsigned budget=65536;structuredObject(report,chunk,budget);report<<"}\n";
            }
            std::ofstream ids(out/"texture-ids.jsonl");
            for(const auto& resource:controller.p->GetResources()){
                int i=index(resource.resourceId);if(i<0)continue;
                ids<<"{\"texture\":"<<i<<",\"name\":"<<quote(resource.name.c_str())<<",\"initialisation_chunks\":[";
                for(size_t n=0;n<resource.initialisationChunks.size();n++){if(n)ids<<',';ids<<resource.initialisationChunks[n];}ids<<"]}\n";
            }
            std::cout<<"PASS local memory API exports "<<count<<'\n';return 0;
        }
        if(apiMode){
            std::unordered_map<uint32_t,uint32_t> chunks;
            for(const auto& item:actionById)for(const auto& e:item.second->events)chunks[e.eventId]=e.chunkIndex;
            const auto& file=controller.p->GetStructuredFile();
            std::ofstream report(out/"api.jsonl");std::istringstream input(argv[5]);std::string token;unsigned count=0;
            while(std::getline(input,token,',')){
                if(++count>32)throw std::runtime_error("at most 32 API events");
                size_t consumed=0;auto event=std::stoul(token,&consumed);
                if(consumed!=token.size()||event>UINT32_MAX||!chunks.count(uint32_t(event))||chunks.at(uint32_t(event))>=file.chunks.size())throw std::runtime_error("API event has no captured chunk");
                report<<"{\"event\":"<<event<<",\"chunk\":";unsigned budget=65536;
                const auto& chunk=*file.chunks[chunks.at(uint32_t(event))];
                structuredObject(report,chunk,budget);report<<"}\n";
                // 2026-10-16: the mesh-task draws too (a runaway group count made a Mock frame take 170 ms).
                if(chunk.name=="vkCmdDrawIndexedIndirectCount"||chunk.name=="vkCmdDrawMeshTasksIndirectCountEXT"||chunk.name=="vkCmdDrawIndexedIndirect"){
                    const auto* source=chunk.FindChild("buffer");const auto* offset=chunk.FindChild("offset");
                    const auto* stride=chunk.FindChild("stride");const auto* maximum=chunk.FindChild("maxDrawCount");
                    if(!maximum)maximum=chunk.FindChild("count");   // vkCmdDrawIndexedIndirect (the VFX particles, 2026-10-16)
                    if(!source||source->type.basetype!=SDBasic::Resource||!offset||!stride||!maximum||
                       !stride->AsUInt64()||stride->AsUInt64()>256||maximum->AsUInt64()>65536)
                        throw std::runtime_error("bounded indirect API parameters required");
                    const auto& buffers=controller.p->GetBuffers();int resolved=-1;
                    for(size_t i=0;i<buffers.size();i++)if(buffers[i].resourceId==source->AsResourceId())resolved=int(i);
                    const auto bytes=maximum->AsUInt64()*stride->AsUInt64();
                    if(resolved<0||!bytes||bytes>16*1024*1024||offset->AsUInt64()>buffers[resolved].length||
                       bytes>buffers[resolved].length-offset->AsUInt64())throw std::runtime_error("indirect buffer bounds");
                    controller.p->SetFrameEvent(uint32_t(event),true);
                    const auto data=controller.p->GetBufferData(source->AsResourceId(),offset->AsUInt64(),bytes);
                    if(data.size()!=bytes)throw std::runtime_error("short indirect source read");
                    const auto stem="indirect_"+std::to_string(event);
                    std::ofstream binary(out/(stem+".bin"),std::ios::binary);
                    binary.write(reinterpret_cast<const char*>(data.data()),data.size());
                    std::ofstream metadata(out/(stem+".json"));
                    int countResolved=-1;uint64_t countOffset=0;
                    if(const auto* count=chunk.FindChild("countBuffer");count&&count->type.basetype==SDBasic::Resource){
                        for(size_t i=0;i<buffers.size();i++)if(buffers[i].resourceId==count->AsResourceId())countResolved=int(i);
                        if(const auto* co=chunk.FindChild("countBufferOffset"))countOffset=co->AsUInt64();}
                    metadata<<"{\"event\":"<<event<<",\"buffer\":"<<resolved<<",\"offset\":"<<offset->AsUInt64()
                        <<",\"stride\":"<<stride->AsUInt64()<<",\"max_draw_count\":"<<maximum->AsUInt64()
                        <<",\"count_buffer\":"<<countResolved<<",\"count_offset\":"<<countOffset<<"}\n";
                    if(!binary||!metadata)throw std::runtime_error("indirect source evidence write");
                }
            }
            std::cout<<"PASS local structured API exports "<<count<<'\n';return 0;
        }
        if(debugThreadMode){
            std::istringstream input(argv[5]);std::string token;unsigned count=0;
            while(std::getline(input,token,',')){
                if(++count>4)throw std::runtime_error("at most four debug invocations");
                std::replace(token.begin(),token.end(),':',' ');std::istringstream fields(token);
                uint32_t event;rdcfixedarray<uint32_t,3> group{},thread{};std::string trailing;
                uint32_t pixelX=0,pixelY=0,probeId=0,rayIndex=0;
                bool observedProbe=false,observedRayResult=false;
                std::string observedPixelName="_519";
                struct DebugReplacement {
                    IReplayController* controller;ResourceId original,replacement;bool active=false;
                    ~DebugReplacement(){if(active)controller->RemoveReplacement(original);if(replacement!=ResourceId())controller->FreeTargetResource(replacement);}
                } replacement{controller.p};
                if(debugGiPixelMode){
                    // DebugThread replays the atomic queue itself, so queue indices
                    // cannot identify a stable pixel, even in the same controller.
                    // Override only the decoded queue entry of this verified shader.
                    if(!(fields>>event>>pixelX>>pixelY)||(fields>>trailing)||!actionById.count(event)||pixelX>=4096||pixelY>=4096)
                        throw std::runtime_error("expected event:x:y");
                    controller.p->SetFrameEvent(event,true);
                    const auto* state=controller.p->GetVulkanPipelineState();
                    if(!state->computeShader.reflection)throw std::runtime_error("GI compute reflection missing");
                    const auto& source=state->computeShader.reflection->rawBytes;uint64_t hash=14695981039346656037ull;
                    for(auto byte:source)hash=(hash^byte)*1099511628211ull;
                    const bool nativeGi=source.size()==33400&&hash==0xf1155b221aa595b2ull;
                    const bool fallbackGi=source.size()==71716&&hash==0x082af85977299a1eull;
                    if(fallbackGi)observedPixelName="_108"; // Verified OpCompositeConstruct feeding named _519.
                    if(!nativeGi&&!fallbackGi)
                        throw std::runtime_error("GI pixel override requires a verified sampling shader");
                    std::vector<uint32_t> words(source.size()/4);std::memcpy(words.data(),source.data(),source.size());
                    const uint32_t id=words[3]++;uint32_t type=0,queueVariable=0;size_t function=0;unsigned changed=0;
                    if(fallbackGi){
                        for(size_t i=5;i<words.size();){
                            const uint32_t n=words[i]>>16,op=words[i]&65535;
                            if(!n||n>words.size()-i)throw std::runtime_error("GI SPIRV instruction bounds");
                            if(op==5&&n==4&&words[i+2]==0x3031355fu&&words[i+3]==0u)queueVariable=words[i+1];
                            i+=n;
                        }
                        if(!queueVariable)throw std::runtime_error("GI fallback queue variable missing");
                    }
                    for(size_t i=5;i<words.size();){
                        const uint32_t n=words[i]>>16,op=words[i]&65535;
                        if(!n||n>words.size()-i)throw std::runtime_error("GI SPIRV instruction bounds");
                        if(op==54&&!function)function=i;
                        if(nativeGi&&op==245&&n>=7&&words[i+2]==510){
                            type=words[i+1];for(size_t j=3;j<n;j+=2)words[i+j]=id;++changed;
                        }
                        if(fallbackGi&&op==21&&n==4&&words[i+2]==32&&words[i+3]==0)type=words[i+1];
                        if(fallbackGi&&op==62&&n==3&&words[i+1]==queueVariable){words[i+2]=id;++changed;}
                        i+=n;
                    }
                    if(changed!=(nativeGi?1u:2u)||!function||!type)throw std::runtime_error("GI decoded queue entry not found");
                    const std::vector<uint32_t> constant{(4u<<16)|43u,type,id,(pixelX<<16)|(pixelY<<1)|1u};
                    words.insert(words.begin()+function,constant.begin(),constant.end());bytebuf bytes;bytes.resize(words.size()*4);
                    std::memcpy(bytes.data(),words.data(),bytes.size());
                    const auto built=controller.p->BuildTargetShader(state->computeShader.reflection->entryPoint,ShaderEncoding::SPIRV,bytes,{},ShaderStage::Compute);
                    if(built.first==ResourceId())throw std::runtime_error("GI debug shader build failed");
                    replacement.original=state->computeShader.resourceId;replacement.replacement=built.first;replacement.active=true;
                    controller.p->ReplaceResource(replacement.original,replacement.replacement);
                    controller.p->SetFrameEvent(event,true);group={0,0,0};thread={0,0,0};
                    std::ofstream evidence(out/("pixel_override_"+std::to_string(event)+"_"+std::to_string(count)+".spv"),std::ios::binary);
                    evidence.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                    std::ofstream resolved(out/("resolved_"+std::to_string(event)+"_"+std::to_string(count)+".json"));
                    resolved<<"{\"event\":"<<event<<",\"requested\":["<<pixelX<<','<<pixelY<<"],\"diagnostic_queue_entry_override\":true}\n";
                }else if(debugGiRayMode){
                    // Resolve the ray from its probe's CURRENT atomic allocation on
                    // each replay. Never reuse an exported atomic queue index.
                    if(!(fields>>event>>probeId>>rayIndex)||(fields>>trailing)||!actionById.count(event)||probeId>=16384||rayIndex>=512)
                        throw std::runtime_error("expected event:probe:ray (bounded)");
                    controller.p->SetFrameEvent(event,true);
                    const auto* state=controller.p->GetVulkanPipelineState();
                    if(!state->computeShader.reflection)throw std::runtime_error("GI ray compute reflection missing");
                    const auto& source=state->computeShader.reflection->rawBytes;uint64_t hash=14695981039346656037ull;
                    for(auto byte:source)hash=(hash^byte)*1099511628211ull;
                    if(source.size()!=29464||hash!=0x1121e5fb6466ccdaull)
                        throw std::runtime_error("GI ray override requires reviewed ray shader");
                    std::vector<uint32_t> words(source.size()/4);std::memcpy(words.data(),source.data(),source.size());
                    const uint32_t first=words[3];words[3]+=6;
                    size_t function=0,site=0;unsigned matches=0;
                    for(size_t i=5;i<words.size();){
                        const uint32_t n=words[i]>>16,op=words[i]&65535;
                        if(!n||n>words.size()-i)throw std::runtime_error("GI ray SPIRV bounds");
                        if(op==54&&!function)function=i;
                        if(op==245&&n==7&&words[i+1]==252&&words[i+2]==417){site=i;++matches;}
                        i+=n;
                    }
                    if(matches!=1||!function||site<=function)throw std::runtime_error("GI ray queue shape changed");
                    // AccessChain(record[probe]), Load, extract allocation offset,
                    // mask its flags, add the requested relative ray. Other math
                    // and the native range checks remain unchanged.
                    const std::vector<uint32_t> ops{
                        (6u<<16)|65u,356,first+2,12,246,first,
                        (4u<<16)|61u,65,first+3,first+2,
                        (5u<<16)|81u,252,first+4,first+3,4,
                        (5u<<16)|199u,252,first+5,first+4,263,
                        (5u<<16)|128u,252,417,first+5,first+1};
                    words.erase(words.begin()+site,words.begin()+site+7);words.insert(words.begin()+site,ops.begin(),ops.end());
                    const std::vector<uint32_t> constants{(4u<<16)|43u,252,first,probeId,(4u<<16)|43u,252,first+1,rayIndex};
                    words.insert(words.begin()+function,constants.begin(),constants.end());
                    bytebuf bytes;bytes.resize(words.size()*4);std::memcpy(bytes.data(),words.data(),bytes.size());
                    const auto built=controller.p->BuildTargetShader(state->computeShader.reflection->entryPoint,ShaderEncoding::SPIRV,bytes,{},ShaderStage::Compute);
                    if(built.first==ResourceId())throw std::runtime_error("GI ray debug shader build failed");
                    replacement.original=state->computeShader.resourceId;replacement.replacement=built.first;replacement.active=true;
                    controller.p->ReplaceResource(replacement.original,replacement.replacement);controller.p->SetFrameEvent(event,true);
                    group={0,0,0};thread={0,0,0};
                    std::ofstream evidence(out/("ray_override_"+std::to_string(event)+"_"+std::to_string(count)+".spv"),std::ios::binary);
                    evidence.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                    std::ofstream resolved(out/("resolved_"+std::to_string(event)+"_"+std::to_string(count)+".json"));
                    resolved<<"{\"event\":"<<event<<",\"probe\":"<<probeId<<",\"ray\":"<<rayIndex<<",\"diagnostic_allocation_override\":true}\n";
                }else if(!(fields>>event>>group[0]>>group[1]>>group[2]>>thread[0]>>thread[1]>>thread[2])||(fields>>trailing)||!actionById.count(event))
                    throw std::runtime_error("expected event:gx:gy:gz:tx:ty:tz");
                const auto& action=*actionById.at(event);
                if(!(action.flags&ActionFlags::Dispatch))throw std::runtime_error("debug event is not compute");
                for(unsigned n=0;n<3;n++)if(group[n]>=action.dispatchDimension[n]||thread[n]>=1024)throw std::runtime_error("debug invocation outside group budget");
                if(!debugGiPixelMode&&!debugGiRayMode)controller.p->SetFrameEvent(event,true);
                bool observedPixel=false;
                struct TraceOwner{IReplayController* controller;ShaderDebugTrace* trace;~TraceOwner(){if(trace)controller->FreeTrace(trace);}} trace{controller.p,controller.p->DebugThread(group,thread)};
                if(!trace.trace||!trace.trace->debugger)throw std::runtime_error("shader debugging unavailable");
                std::ofstream report(out/("debug_"+std::to_string(event)+"_"+std::to_string(count)+".jsonl"));
                for(const auto& value:trace.trace->constantBlocks){report<<"{\"constant\":";debugVariable(report,value);report<<"}\n";}
                const auto start=std::chrono::steady_clock::now();unsigned steps=0;bool finished=false;
                while(steps<20000&&std::chrono::steady_clock::now()-start<std::chrono::seconds(120)){
                    const auto states=controller.p->ContinueDebug(trace.trace->debugger);
                    if(states.empty()){finished=true;break;}
                    for(const auto& state:states){
                        if(++steps>20000)break;
                        report<<"{\"step\":"<<state.stepIndex<<",\"next\":"<<state.nextInstruction<<",\"changes\":[";
                        for(size_t n=0;n<state.changes.size();n++){
                            const auto& value=state.changes[n].after;
                            if(debugGiPixelMode&&value.name==observedPixelName.c_str()){
                                if(value.columns!=2||value.value.u32v[0]!=pixelX||value.value.u32v[1]!=pixelY)
                                    throw std::runtime_error("GI debug invocation differs from resolved pixel");
                                observedPixel=true;
                            }
                            if(debugGiRayMode&&value.name=="_424"){
                                if(value.columns!=1||value.value.u32v[0]!=probeId)throw std::runtime_error("GI ray resolved to wrong probe");
                                observedProbe=true;
                            }
                            if(debugGiRayMode&&value.name=="_1062")observedRayResult=true;
                            if(n)report<<',';debugVariable(report,value);
                        }report<<"]}\n";
                    }report.flush();
                }
                report<<"{\"finished\":"<<(finished?"true":"false")<<",\"steps\":"<<steps<<"}\n";
                if(!finished)throw std::runtime_error("shader debug trace budget exhausted");
                if(debugGiPixelMode&&!observedPixel)throw std::runtime_error("GI debug pixel was never observed");
                if(debugGiRayMode&&(!observedProbe||!observedRayResult))throw std::runtime_error("GI ray lacked matching probe/result (possibly unavailable ray)");
            }
            std::cout<<"PASS local shader invocations debugged "<<count<<'\n';return 0;
        }
        if(texelsMode||texturesMode){
            std::istringstream input(argv[5]);std::string token;unsigned count=0;
            std::ofstream report(out/(texturesMode?"texture-previews.jsonl":"texels.jsonl"));
            while(std::getline(input,token,',')){
                if(++count>16)throw std::runtime_error("at most 16 texture inspections");
                std::replace(token.begin(),token.end(),':',' ');std::istringstream fields(token);uint32_t event,texture,slice=0;std::string selection,sliceText,trailing;bool explicitSlice=false;
                if(!(fields>>event>>selection)||!actionById.count(event))
                    throw std::runtime_error("invalid event:texture[:slice] selection");
                if(fields>>sliceText){size_t used=0;const auto parsed=std::stoul(sliceText,&used);
                    if(used!=sliceText.size()||parsed>UINT32_MAX||(fields>>trailing))throw std::runtime_error("invalid texture slice");
                    slice=uint32_t(parsed);explicitSlice=true;}
                if(selection=="present"){
                    // Resolve in this controller; inventory indices from another replay
                    // are evidence, not stable resource identifiers.
                    int resolved=-1;
                    for(const auto& a:actionById)if(a.second->flags&ActionFlags::Present){
                        const int i=index(a.second->copyDestination);
                        if(i<0||(resolved>=0&&resolved!=i))throw std::runtime_error("missing or multiple present textures");resolved=i;
                    }
                    if(resolved<0)throw std::runtime_error("present selector needs a captured present texture");
                    texture=uint32_t(resolved);
                }else{
                    size_t consumed=0;const auto parsed=std::stoul(selection,&consumed);
                    if(consumed!=selection.size()||parsed>=textures.size())throw std::runtime_error("invalid texture index");
                    texture=uint32_t(parsed);
                }
                const auto& t=textures[texture];
                if(t.arraysize>16||slice>=t.arraysize||(!explicitSlice&&t.arraysize!=1))
                    throw std::runtime_error("array texture needs explicit bounded slice");
                const auto sliceSuffix=explicitSlice?"_slice_"+std::to_string(slice):std::string();
                if(texturesMode){
                    if(t.width>8192||t.height>8192||t.depth!=1||t.msSamp>1)
                        throw std::runtime_error("unsupported texture preview extent");
                    controller.p->SetFrameEvent(event,true);
                    const auto name="event_"+std::to_string(event)+"_texture_"+std::to_string(texture)+sliceSuffix+".png";
                    TextureSave save;save.resourceId=t.resourceId;save.destType=FileType::PNG;save.mip=0;save.slice.sliceIndex=int32_t(slice);
                    if(!controller.p->SaveTexture(save,(out/name).string().c_str()).OK())throw std::runtime_error("texture preview save failed");
                    report<<"{\"event\":"<<event<<",\"texture\":"<<texture<<",\"file\":"<<quote(name.c_str())
                        <<",\"format\":"<<quote(t.format.Name().c_str())<<",\"width\":"<<t.width<<",\"height\":"<<t.height
                        <<",\"array\":"<<t.arraysize<<",\"slice\":"<<slice<<",\"raw_data\":false,\"conversion\":\"RenderDoc default PNG; float values can be clipped\"}\n";
                    report.flush();continue;
                }
                // Packed 32-bit formats retain their original words and are explicitly
                // tagged. Never present them as tightly packed float components.
                const bool packed=t.format.type==ResourceFormatType::R10G10B10A2||t.format.type==ResourceFormatType::R9G9B9E5;
                const uint64_t expected=uint64_t(t.width)*t.height*t.depth*(packed?4:t.format.compCount*t.format.compByteWidth);
                if((t.format.type!=ResourceFormatType::Regular&&!packed)||!expected||expected>64*1024*1024||t.depth>512||t.msSamp>1)
                    throw std::runtime_error("unsupported texture format or diagnostic byte budget");
                controller.p->SetFrameEvent(event,true);
                const auto data=controller.p->GetTextureData(t.resourceId,{0,slice,0});
                if(data.size()!=expected)throw std::runtime_error("raw texture byte extent mismatch");
                const auto name="event_"+std::to_string(event)+"_texture_"+std::to_string(texture)+sliceSuffix+".bin";
                std::ofstream file(out/name,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());
                if(!file)throw std::runtime_error("raw texture write failed");
                report<<"{\"event\":"<<event<<",\"texture\":"<<texture<<",\"file\":"<<quote(name.c_str())
                    <<",\"format\":"<<quote(t.format.Name().c_str())<<",\"width\":"<<t.width<<",\"height\":"<<t.height<<",\"depth\":"<<t.depth
                    <<",\"array\":"<<t.arraysize<<",\"slice\":"<<slice<<",\"components\":"<<uint32_t(t.format.compCount)<<",\"component_bytes\":"<<uint32_t(t.format.compByteWidth)
                    <<",\"component_type\":"<<uint32_t(t.format.compType)<<",\"packed_32\":"<<(packed?"true":"false")<<",\"bytes\":"<<data.size()<<"}\n";
                report.flush();
            }
            std::cout<<"PASS local "<<(texturesMode?"preview":"raw")<<" texture exports "<<count<<'\n';return 0;
        }
        if(bindingsMode){
            std::istringstream input(argv[5]);std::string token;unsigned count=0;
            uint64_t snapshotBytes=0;
            std::ofstream report(out/"bindings.jsonl");
            std::ofstream raster(out/"raster-state.jsonl");
            while(std::getline(input,token,',')){
                if(++count>(rasterOnly?256u:16u))throw std::runtime_error("binding/raster inspection budget");
                const auto event=uint32_t(std::stoul(token));if(!actionById.count(event))throw std::runtime_error("binding event is not an action");
                controller.p->SetFrameEvent(event,true);const auto* pipe=controller.p->GetVulkanPipelineState();
                if(actionById.at(event)->flags&ActionFlags::Drawcall){
                    raster<<"{\"event\":"<<event<<",\"depth_test\":"<<(pipe->depthStencil.depthTestEnable?"true":"false")
                        <<",\"depth_write\":"<<(pipe->depthStencil.depthWriteEnable?"true":"false")<<",\"blends\":[";
                    for(size_t n=0;n<pipe->colorBlend.blends.size();n++){
                        if(n)raster<<',';const auto& b=pipe->colorBlend.blends[n];
                        raster<<"{\"enabled\":"<<(b.enabled?"true":"false")<<",\"write_mask\":"<<unsigned(b.writeMask)
                            <<",\"logic\":"<<(b.logicOperationEnabled?"true":"false")<<",\"color\":["<<uint32_t(b.colorBlend.source)<<','
                            <<uint32_t(b.colorBlend.destination)<<','<<uint32_t(b.colorBlend.operation)<<"],\"alpha\":["
                            <<uint32_t(b.alphaBlend.source)<<','<<uint32_t(b.alphaBlend.destination)<<','<<uint32_t(b.alphaBlend.operation)<<"]}";
                    }
                    raster<<"],\"attachments\":[";
                    for(size_t n=0;n<pipe->currentPass.framebuffer.attachments.size();n++){
                        if(n)raster<<',';const auto& a=pipe->currentPass.framebuffer.attachments[n];
                        raster<<"{\"texture\":"<<index(a.resource)<<",\"format\":"<<quote(a.format.Name().c_str())<<"}";
                    }
                    raster<<"]}\n";
                }
                if(rasterOnly)continue;
                // 2026-10-18: the tessellation stages too (the terrain is drawn in four-point patches).
                for(auto stage:{ShaderStage::Vertex,ShaderStage::Tess_Control,ShaderStage::Tess_Eval,ShaderStage::Fragment,ShaderStage::Compute}){
                    if(bool(actionById.at(event)->flags&ActionFlags::Dispatch)!=(stage==ShaderStage::Compute))continue;
                    const auto* shader=stage==ShaderStage::Compute?pipe->computeShader.reflection:stage==ShaderStage::Vertex?pipe->vertexShader.reflection:
                        stage==ShaderStage::Tess_Control?pipe->tessControlShader.reflection:stage==ShaderStage::Tess_Eval?pipe->tessEvalShader.reflection:pipe->fragmentShader.reflection;if(!shader)continue;
                    const auto prefix="event_"+std::to_string(event)+"_stage_"+std::to_string(uint32_t(stage));
                    {std::ofstream file(out/(prefix+".spv"),std::ios::binary);file.write(reinterpret_cast<const char*>(shader->rawBytes.data()),shader->rawBytes.size());}
                    auto descriptors=[&](const char* category,const rdcarray<UsedDescriptor>& descriptors){
                        if(descriptors.size()>512)throw std::runtime_error("descriptor inspection budget exceeded");
                        unsigned ordinal=0;
                        for(const auto& d:descriptors){
                            const auto& value=d.descriptor;const int texture=index(value.resource);
                            int buffer=-1;const auto& buffers=controller.p->GetBuffers();
                            for(size_t n=0;n<buffers.size();n++)if(buffers[n].resourceId==value.resource){buffer=int(n);break;}
                            report<<"{\"event\":"<<event<<",\"stage\":"<<uint32_t(stage)<<",\"category\":"<<quote(category)
                                <<",\"index\":"<<d.access.index<<",\"array_element\":"<<d.access.arrayElement<<",\"descriptor_type\":"<<uint32_t(d.access.type)<<",\"texture\":"<<texture<<",\"buffer\":"<<buffer
                                <<",\"offset\":"<<value.byteOffset<<",\"size\":"<<value.byteSize<<",\"format\":"<<quote(value.format.Name().c_str());
                            if(texture>=0)report<<",\"width\":"<<textures[texture].width<<",\"height\":"<<textures[texture].height;
                            const bool constant=std::string(category)=="constant";
                            if((constant||(bufferSnapshots&&buffer>=0))&&value.resource!=ResourceId()&&value.byteSize){
                                const uint64_t limit=constant?65536:32*1024*1024;
                                if(value.byteSize>limit||(snapshotBytes+=value.byteSize)>128*1024*1024)
                                    throw std::runtime_error("binding buffer snapshot byte budget exceeded");
                                const auto bytes=controller.p->GetBufferData(value.resource,value.byteOffset,value.byteSize);
                                if(bytes.size()!=value.byteSize)throw std::runtime_error("binding buffer snapshot short read");
                                const auto fileName=prefix+"_"+category+"_"+std::to_string(ordinal)+".bin";
                                std::ofstream file(out/fileName,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                                if(!file)throw std::runtime_error("binding buffer snapshot write failed");
                                report<<",\"file\":"<<quote(fileName.c_str());
                            }
                            report<<"}\n";++ordinal;
                        }
                    };
                    rdcarray<UsedDescriptor> constants,resources;
                    for(const auto& access:controller.p->GetDescriptorAccess()){
                        if(access.stage!=stage||access.staticallyUnused)continue;
                        if(access.type==DescriptorType::Sampler){
                            const auto samplers=controller.p->GetSamplerDescriptors(access.descriptorStore,{DescriptorRange(access)});
                            if(samplers.size()!=1)throw std::runtime_error("sampler descriptor range result mismatch");
                            const auto& v=samplers[0];
                            report<<"{\"event\":"<<event<<",\"stage\":"<<uint32_t(stage)<<",\"category\":\"sampler\",\"index\":"<<access.index
                                <<",\"compare\":"<<uint32_t(v.compareFunction)<<",\"address\":["<<uint32_t(v.addressU)<<','<<uint32_t(v.addressV)<<','<<uint32_t(v.addressW)
                                <<"],\"filter\":["<<uint32_t(v.filter.minify)<<','<<uint32_t(v.filter.magnify)<<','<<uint32_t(v.filter.mip)<<','<<uint32_t(v.filter.filter)<<"]}\n";
                            continue;
                        }
                        const auto values=controller.p->GetDescriptors(access.descriptorStore,{DescriptorRange(access)});
                        if(values.size()!=1)throw std::runtime_error("descriptor range result mismatch");
                        UsedDescriptor value;value.access=access;value.descriptor=values[0];
                        const auto& pipeline=stage==ShaderStage::Compute?pipe->compute:pipe->graphics;
                        for(const auto& set:pipeline.descriptorSets)if(set.descriptorSetResourceId==access.descriptorStore)
                            for(const auto& offset:set.dynamicOffsets)if(offset.descriptorByteOffset==access.byteOffset)value.descriptor.byteOffset+=offset.dynamicBufferByteOffset;
                        (access.type==DescriptorType::ConstantBuffer?constants:resources).push_back(value);
                    }
                    descriptors("constant",constants);descriptors("resource",resources);
                }
            }
            std::cout<<"PASS local shaders and bindings exported "<<count<<'\n';return 0;
        }
        if(eventMode){
            std::istringstream input(argv[5]);std::string token;std::vector<uint32_t> selected;
            while(std::getline(input,token,',')){selected.push_back(uint32_t(std::stoul(token)));if(selected.size()>16)throw std::runtime_error("at most 16 event inspections");}
            std::ofstream report(out/"event-meshes.jsonl");
            for(auto event:selected){
                if(!actionById.count(event))throw std::runtime_error("event not an action");
                controller.p->SetFrameEvent(event,true);const auto& action=*actionById.at(event);
                for(auto id:action.outputs)if(id!=ResourceId()){
                    auto idx=index(id);if(idx<0)continue;const auto& t=textures[size_t(idx)];
                    if(t.width>8192||t.height>8192||t.depth!=1||t.arraysize!=1)continue;
                    TextureSave save;save.resourceId=id;save.destType=FileType::PNG;save.mip=0;
                    auto saved=controller.p->SaveTexture(save,(out/("event_"+std::to_string(event)+"_texture_"+std::to_string(idx)+".png")).string().c_str());
                    if(!saved.OK())throw std::runtime_error("event texture save failed");
                }
                const auto mesh=controller.p->GetPostVSData(0,0,MeshDataStage::VSOut);
                uint64_t maxVertex=mesh.numIndices?mesh.numIndices-1:0;bool indexed=mesh.indexResourceId!=ResourceId();
                if(indexed&&mesh.numIndices&&mesh.indexByteStride){
                    const uint64_t bytes=uint64_t(mesh.numIndices)*mesh.indexByteStride;
                    if(bytes>32*1024*1024)throw std::runtime_error("mesh index diagnostic budget exceeded");
                    const auto data=controller.p->GetBufferData(mesh.indexResourceId,mesh.indexByteOffset,bytes);
                    std::ofstream file(out/("event_"+std::to_string(event)+"_indices.bin"),std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());
                    maxVertex=0;
                    for(size_t n=0;n+mesh.indexByteStride<=data.size();n+=mesh.indexByteStride){
                        uint32_t value=0;std::memcpy(&value,data.data()+n,std::min(mesh.indexByteStride,4u));
                        if(value!=UINT32_MAX&&int64_t(value)+mesh.baseVertex>=0)maxVertex=std::max(maxVertex,uint64_t(int64_t(value)+mesh.baseVertex));
                    }
                }
                uint64_t vertexBytes=0;
                if(mesh.vertexResourceId!=ResourceId()&&mesh.vertexByteStride){
                    vertexBytes=(maxVertex+1)*mesh.vertexByteStride;if(vertexBytes>32*1024*1024)throw std::runtime_error("mesh vertex diagnostic budget exceeded");
                    const auto data=controller.p->GetBufferData(mesh.vertexResourceId,mesh.vertexByteOffset,vertexBytes);vertexBytes=data.size();
                    std::ofstream file(out/("event_"+std::to_string(event)+"_vertices.bin"),std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());
                }
                report<<"{\"event\":"<<event<<",\"indices\":"<<mesh.numIndices<<",\"index_stride\":"<<mesh.indexByteStride<<",\"base_vertex\":"<<mesh.baseVertex
                    <<",\"vertex_stride\":"<<mesh.vertexByteStride<<",\"vertex_bytes\":"<<vertexBytes<<",\"topology\":"<<uint32_t(mesh.topology)
                    <<",\"component_count\":"<<uint32_t(mesh.format.compCount)<<",\"component_bytes\":"<<uint32_t(mesh.format.compByteWidth)
                    <<",\"component_type\":"<<uint32_t(mesh.format.compType)<<"}\n";
            }
            std::cout<<"PASS local event textures/post-VS geometry exported "<<selected.size()<<'\n';return 0;
        }
        if(!usageOnly)controller.p->SetFrameEvent(last,true);
        if(historyMode){
            std::istringstream input(argv[5]);std::string token;unsigned count=0;
            std::ofstream history(out/"pixel-history.jsonl");
            while(std::getline(input,token,',')){
                if(++count>16)throw std::runtime_error("at most 16 pixel inspections");
                // texture:x:y[:slice] (2026-10-17: the slice of an array image, e.g. one cascade of the sun shadow map)
                std::replace(token.begin(),token.end(),':',' ');std::istringstream fields(token);uint32_t texture,x,y,slice=0;
                if(!(fields>>texture>>x>>y)||texture>=textures.size()||x>=textures[texture].width||y>=textures[texture].height)
                    throw std::runtime_error("pixel inspection coordinate/index invalid");
                if(fields>>slice&&slice>=textures[texture].arraysize)throw std::runtime_error("pixel inspection slice invalid");
                Subresource sub{};sub.slice=slice;
                const auto changes=controller.p->PixelHistory(textures[texture].resourceId,x,y,sub,CompType::Typeless);
                history<<"{\"texture\":"<<texture<<",\"x\":"<<x<<",\"y\":"<<y<<",\"slice\":"<<slice<<",\"changes\":[";
                for(size_t n=0;n<changes.size();++n){const auto& c=changes[n];if(n)history<<',';
                    history<<"{\"event\":"<<c.eventId<<",\"primitive\":"<<c.primitiveID<<",\"passed\":"<<(c.Passed()?"true":"false")
                        <<",\"direct_write\":"<<(c.directShaderWrite?"true":"false")<<",\"discarded\":"<<(c.shaderDiscarded?"true":"false")
                        <<",\"backface_culled\":"<<(c.backfaceCulled?"true":"false")<<",\"depth_failed\":"<<(c.depthTestFailed?"true":"false")<<",\"post\":[";
                    for(unsigned j=0;j<4;++j){if(j)history<<',';float f=c.postMod.col.floatValue[j];if(std::isfinite(f))history<<f;else history<<"null";}history<<"]}";
                }
                history<<"]}\n";history.flush();
            }
            std::cout<<"PASS local pixel history exported "<<count<<'\n';return 0;
        }
        std::ofstream layouts(out/"image-layouts.jsonl");
        if(!usageOnly)for(const auto& image:controller.p->GetVulkanPipelineState()->images){
            std::string name;for(const auto& resource:controller.p->GetResources())if(resource.resourceId==image.resourceId){name=resource.name.c_str();break;}
            for(const auto& layout:image.layouts)layouts<<"{\"texture\":"<<index(image.resourceId)<<",\"name\":"<<quote(name.c_str())
                <<",\"layout\":"<<quote(layout.name.c_str())<<",\"base_mip\":"<<layout.baseMip<<",\"mips\":"<<layout.numMip
                <<",\"base_layer\":"<<layout.baseLayer<<",\"layers\":"<<layout.numLayer<<"}\n";
        }
        if(layoutsOnly){std::cout<<"PASS final-event image layouts recorded\n";return 0;}
        std::ofstream inventory(out/"textures.jsonl");unsigned saved=0,failures=0;
        std::ofstream usages(out/"usage.jsonl");
        for(size_t i=0;i<textures.size();++i){const auto& t=textures[i];
            std::string name;for(const auto& resource:controller.p->GetResources())if(resource.resourceId==t.resourceId){name=resource.name.c_str();break;}
            const bool selected=t.width>=256&&t.height>=256&&t.depth==1&&t.arraysize==1&&(t.creationFlags&TextureCategory::ColorTarget);
            int code=-1;
            if(selected||usageOnly)for(const auto& usage:controller.p->GetUsage(t.resourceId))usages<<"{\"texture\":"<<i<<",\"event\":"<<usage.eventId<<",\"usage\":"<<uint32_t(usage.usage)<<",\"compute_write\":"<<(usage.usage==ResourceUsage::CS_RWResource?"true":"false")<<",\"copy_destination\":"<<(usage.usage==ResourceUsage::CopyDst?"true":"false")<<"}\n";
            if(selected&&!usageOnly){TextureSave save;save.resourceId=t.resourceId;save.destType=FileType::PNG;save.mip=0;
                auto st=controller.p->SaveTexture(save,(out/("texture_"+std::to_string(i)+".png")).string().c_str());code=int(st.code);if(st.OK())saved++;else failures++;}
            inventory<<"{\"index\":"<<i<<",\"name\":"<<quote(name.c_str())<<",\"format\":"<<quote(t.format.Name().c_str())<<",\"width\":"<<t.width<<",\"height\":"<<t.height<<",\"array\":"<<t.arraysize<<",\"flags\":"<<uint32_t(t.creationFlags)<<",\"selected\":"<<(selected?"true":"false")<<",\"save_result\":"<<code<<"}\n";
        }
        std::ofstream report(out/"result.json");report<<"{\"event\":"<<last<<",\"draws\":"<<draws<<",\"dispatches\":"<<dispatches<<",\"textures\":"<<textures.size()<<",\"saved\":"<<saved<<",\"save_failures\":"<<failures<<"}\n";
        std::cout<<"PASS replay events="<<last<<" draws="<<draws<<" dispatches="<<dispatches<<" saved="<<saved<<" failures="<<failures<<'\n';
        return failures?1:0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
