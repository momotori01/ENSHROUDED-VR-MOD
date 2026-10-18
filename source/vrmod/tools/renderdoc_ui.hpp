// Local capture-only UI isolation. Uses RenderDoc's supported ClearBeforePass
// overlay after native UI-atlas preparation; no live game or capture mutation.
static void isolateUiPass(IReplayController* controller,const ActionDescription& lastDraw,
                         uint32_t begin,const std::filesystem::path& out){
    const auto id=lastDraw.outputs[0];const auto& textures=controller->GetTextures();
    const TextureDescription* texture=nullptr;
    for(const auto& t:textures)if(t.resourceId==id)texture=&t;
    if(!texture||texture->format.Name()!="R8G8B8A8_UNORM"||texture->width>4096||texture->height>4096||
       texture->depth!=1||texture->arraysize!=1||texture->msSamp!=1)
        throw std::runtime_error("UI pass requires bounded RGBA8 UNORM attachment");
    std::ofstream report(out/"ui-pass-images.jsonl");
    auto save=[&](const char* label,uint32_t event){
        const auto bytes=controller->GetTextureData(id,{});
        if(bytes.size()!=uint64_t(texture->width)*texture->height*4)throw std::runtime_error("UI texture extent mismatch");
        const auto raw=std::string(label)+".bin",png=std::string(label)+".png";
        std::ofstream file(out/raw,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(!file)throw std::runtime_error("UI snapshot write failed");
        TextureSave config;config.resourceId=id;config.destType=FileType::PNG;config.mip=0;
        if(!controller->SaveTexture(config,(out/png).string().c_str()).OK())throw std::runtime_error("UI preview write failed");
        report<<"{\"case\":"<<quote(label)<<",\"event\":"<<event<<",\"width\":"<<texture->width
            <<",\"height\":"<<texture->height<<",\"format\":\"R8G8B8A8_UNORM\",\"file\":"<<quote(raw.c_str())
            <<",\"png\":"<<quote(png.c_str())<<"}\n";report.flush();
    };
    controller->SetFrameEvent(begin,true);save("background",begin);
    controller->SetFrameEvent(lastDraw.eventId,true);save("before",lastDraw.eventId);
    controller->SetFrameEvent(lastDraw.eventId,true);save("control",lastDraw.eventId);
    Owner<IReplayOutput> output{controller->CreateOutput({},ReplayOutputType::Texture)};
    if(!output.p)throw std::runtime_error("headless UI replay output unavailable");
    TextureDisplay display;display.resourceId=id;display.backgroundColor={0,0,0,0};
    display.overlay=DebugOverlay::ClearBeforePass;output.p->SetTextureDisplay(display);
    // For ClearBeforePass RenderDoc modifies the replay target, not a separate
    // overlay texture (ReplayOutput::SetTextureDisplay/GetDebugOverlayTexID).
    output.p->GetDebugOverlayTexID();save("isolated",lastDraw.eventId);
    display.overlay=DebugOverlay::NoOverlay;output.p->SetTextureDisplay(display);
    controller->SetFrameEvent(lastDraw.eventId,true);save("restored",lastDraw.eventId);
    std::cout<<"PASS exported UI pass baseline/control/transparent/restored; numerical verification required\n";
}
