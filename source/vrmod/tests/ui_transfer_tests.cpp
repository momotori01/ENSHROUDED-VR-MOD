// Actual Vulkan UI pixels -> acquired OpenXR image, alongside real fixture stereo geometry.
#include <evr/ui_transfer.hpp>
#include <evr/test_renderer.hpp>
#include <iostream>
#include <set>
using namespace evr;
struct UiSource {
    Vulkan& v;VkImage image{};VkDeviceMemory memory{},bufferMemory{};VkBuffer buffer{};
    VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};uint32_t width,height;std::vector<uint8_t> pixels;
    PFN_vkCmdCopyBufferToImage upload{};
    UiSource(Vulkan& d,uint32_t w,uint32_t h):v(d),width(w),height(h),pixels(size_t(w)*h*4){
        try{
            upload=reinterpret_cast<PFN_vkCmdCopyBufferToImage>(v.vkGetDeviceProcAddr(v.device,"vkCmdCopyBufferToImage"));
            if(!upload)throw std::runtime_error("upload missing");
            VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=VK_FORMAT_R8G8B8A8_UNORM;i.extent={w,h,1};i.mipLevels=i.arrayLayers=1;i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            vkCheck(v.vkCreateImage(v.device,&i,nullptr,&image),"fixture source");VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,image,&req);memory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,image,memory,0),"fixture image memory");
            VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=pixels.size();b.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&buffer),"fixture staging");
            v.vkGetBufferMemoryRequirements(v.device,buffer,&req);bufferMemory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(v.device,buffer,bufferMemory,0),"fixture staging memory");
            VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=v.family;vkCheck(v.vkCreateCommandPool(v.device,&cp,nullptr,&pool),"fixture pool");
            VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vkCheck(v.vkAllocateCommandBuffers(v.device,&ca,&cmd),"fixture command");
            VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkCheck(v.vkCreateFence(v.device,&f,nullptr,&fence),"fixture fence");
        }catch(...){destroy();throw;}
    }
    void destroy(){v.vkDeviceWaitIdle(v.device);if(fence)v.vkDestroyFence(v.device,fence,nullptr);if(pool)v.vkDestroyCommandPool(v.device,pool,nullptr);if(buffer)v.vkDestroyBuffer(v.device,buffer,nullptr);if(bufferMemory)v.vkFreeMemory(v.device,bufferMemory,nullptr);if(image)v.vkDestroyImage(v.device,image,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);}
    ~UiSource(){destroy();}
    void begin(){vkCheck(v.vkResetCommandBuffer(cmd,0),"fixture reset");VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};vkCheck(v.vkBeginCommandBuffer(cmd,&b),"fixture begin");}
    void complete(){vkCheck(v.vkEndCommandBuffer(cmd),"fixture end");vkCheck(v.vkResetFences(v.device,1,&fence),"fixture reset fence");VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&cmd;vkCheck(v.vkQueueSubmit(v.queue,1,&s,fence),"fixture submit");vkCheck(v.vkWaitForFences(v.device,1,&fence,VK_TRUE,5000000000),"fixture wait");}
    void transition(VkImageLayout old,VkImageLayout next,VkAccessFlags src,VkAccessFlags dst){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=old;b.newLayout=next;b.srcAccessMask=src;b.dstAccessMask=dst;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);}
    VkBufferImageCopy region(){VkBufferImageCopy r{};r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};r.imageExtent={width,height,1};return r;}
    void fill(unsigned phase){
        for(size_t i=0;i<pixels.size()/4;++i){
            const unsigned a=(unsigned(i)*53+phase*11)%256;
            pixels[i*4+3]=uint8_t(a);for(unsigned c=0;c<3;++c)pixels[i*4+c]=uint8_t((i*29+c*47+phase*7)%(a+1));
            if(i%17==0){pixels[i*4]=25;pixels[i*4+1]=50;pixels[i*4+2]=99;pixels[i*4+3]=0;}
            if(i%19==0){pixels[i*4]=128;pixels[i*4+1]=64;pixels[i*4+2]=32;pixels[i*4+3]=128;}
            if(i%23==0){pixels[i*4]=200;pixels[i*4+1]=128;pixels[i*4+2]=64;pixels[i*4+3]=90;}
        }
        void* p{};vkCheck(v.vkMapMemory(v.device,bufferMemory,0,pixels.size(),0,&p),"fixture map");std::memcpy(p,pixels.data(),pixels.size());v.vkUnmapMemory(v.device,bufferMemory);
        begin();VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&host,0,nullptr,0,nullptr);
        transition(VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
        auto r=region();upload(cmd,buffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&r);
        transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);complete();
    }
    bool unchanged(){begin();auto r=region();v.vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&r);VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);complete();void* p{};vkCheck(v.vkMapMemory(v.device,bufferMemory,0,pixels.size(),0,&p),"source compare map");bool same=!std::memcmp(p,pixels.data(),pixels.size());v.vkUnmapMemory(v.device,bufferMemory);return same;}
    TransferImage descriptor()const{return {image,width,height,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,v.family};}
};
double decode(double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}
double encode(double x){return x<=.0031308?x*12.92:1.055*std::pow(x,1./2.4)-.055;}
int main(int argc,char** argv){unsigned checks=0;auto require=[&](bool v,const char* name){if(!v)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';};
    try{
        const std::filesystem::path output=argc>1?argv[1]:"ui-transfer-validation";std::filesystem::create_directories(output);Log log((output/"openxr.jsonl").string(),true);
        OpenXrClient xr(log);xr.init(.25f);xr.poll();
        {   // What the desktop shows of the eye's picture: the sums of the headset run and of the Mock, and the two shapes.
            const auto same=[](CropRect r,int32_t x0,int32_t y0,int32_t x1,int32_t y1){return r.x0==x0&&r.y0==y0&&r.x1==x1&&r.y1==y1;};
            require(same(centreCrop(2243,2243,1920,1080),0,490,2243,1752)&&same(centreCrop(1963,1963,1920,1080),0,429,1963,1533),"square eye picture: the middle rows");
            require(same(centreCrop(1920,1080,1920,1080),0,0,1920,1080)&&same(centreCrop(4000,1000,1920,1080),1111,0,2889,1000),"same shape whole, wide picture: the middle columns");
            require(same(centreCrop(0,10,16,9),0,0,0,10)&&same(centreCrop(10,10,0,9),0,0,10,10),"nothing to cut without a size");
        }
        {
            TestRenderer eyes(xr);
            for(auto format:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM})for(auto extent:{std::array<uint32_t,2>{16,8},{128,64},{33,15}}){
                xr.configureUi(extent[0],extent[1],format);UiTransfer transfer(xr.vk,xr.uiSwapchain(),format);UiSource source(xr.vk,extent[0],extent[1]);
                // The pictures behind the UI for the desktop's picture: their middle in the UI's shape is one texel for one.
                UiSource tall(xr.vk,extent[0],extent[1]+6),wide(xr.vk,extent[0]+10,extent[1]);tall.fill(40);wide.fill(41);
                std::set<uint32_t> indices;
                for(unsigned f=0;f<6;++f){
                    source.fill(f);const bool screen=f==2||f==3;require(screen?xr.beginScreenFrame():xr.beginFrame(),"GPU fixture frame begins");const auto snapshot=xr.pendingFrame();auto index=xr.acquireUi(snapshot.frame);indices.insert(index);
                    if(!f)for(unsigned test=0;test<7;++test){auto invalid=source.descriptor();switch(test){case 0:invalid.image=VK_NULL_HANDLE;break;case 1:++invalid.width;break;case 2:invalid.format=VK_FORMAT_R8G8B8A8_SRGB;break;case 3:invalid.layout=VK_IMAGE_LAYOUT_UNDEFINED;break;case 4:invalid.queueFamily=VK_QUEUE_FAMILY_IGNORED;break;case 5:invalid.usage=0;break;case 6:invalid.image=xr.uiSwapchain().images[index].image;break;}bool refused=false;try{transfer.draw(invalid,index);}catch(const std::invalid_argument&){refused=true;}require(refused&&!transfer.gpuPending(),"unproven/aliased UI source refused before GPU submit");}
                    const auto bytes=transfer.draw(source.descriptor(),index,f!=5);require(!transfer.gpuPending(),"UI GPU fence completes before release");
                    if(f!=5){
                        require(bytes.size()==source.pixels.size(),"XR UI image dimensions match native surface");int maxError=0;bool alpha=true;
                        for(size_t i=0;i<bytes.size();i+=4){double a=source.pixels[i+3]/255.;alpha &= bytes[i+3]==source.pixels[i+3];for(unsigned c=0;c<3;++c){double x=source.pixels[i+c]/255.;double linear=a>0?decode(x/a)*a:decode(x);linear=std::clamp(linear,0.,1.);double expected=format==VK_FORMAT_R8G8B8A8_SRGB?encode(linear):linear;maxError=std::max(maxError,std::abs(int(bytes[i+c])-int(std::lround(expected*255))));}}
                        require(alpha,"coverage preserved including zero-alpha additive UI");require(maxError<=2,"GPU linear-premultiplied RGB matches independent CPU reference");
                        std::ofstream file(output/(std::to_string(format)+"-"+std::to_string(extent[0])+"-"+std::to_string(f)+".rgba8"),std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                        if(format==VK_FORMAT_R8G8B8A8_SRGB)require(bytes[19*4]>source.pixels[19*4]+40,"negative control rejects raw sRGB copy of premultiplied UI");
                    }else require(bytes.empty(),"no CPU readback on ordinary submission");
                    if(f!=5){
                        // The desktop's picture: the UI over the middle of the picture behind, both as display values.
                        for(unsigned k=0;k<3;++k){
                            UiSource& back=k==1?wide:tall;UiFade fade;if(k==2)fade.outside=.5f;const auto behind=back.descriptor();
                            transfer.draw(source.descriptor(),index,true,true,fade,false,&behind);
                            const auto view=transfer.desktopView();const auto& desk=transfer.desktopCapture();
                            require(view.image&&view.image==transfer.desktopImage()&&view.width==extent[0]&&view.height==extent[1]&&desk.size()==source.pixels.size(),"desktop picture has the UI's size");
                            const auto crop=centreCrop(back.width,back.height,extent[0],extent[1]);int worst=0;
                            require(crop.x1-crop.x0==int32_t(extent[0])&&crop.y1-crop.y0==int32_t(extent[1])&&(k==1?crop.x0==5&&crop.y0==0:crop.x0==0&&crop.y0==3),"fixture crop is one texel for one");
                            for(uint32_t y=0;y<extent[1];++y)for(uint32_t x=0;x<extent[0];++x){
                                const size_t i=(size_t(y)*extent[0]+x)*4,b=(size_t(y+crop.y0)*back.width+x+crop.x0)*4;const double scale=k==2?.5:1.,a=source.pixels[i+3]/255.*scale;
                                for(unsigned c=0;c<4;++c){const double expected=std::min(255.,source.pixels[i+c]*scale+back.pixels[b+c]*(1-a));worst=std::max(worst,std::abs(int(desk[i+c])-int(std::lround(expected))));}
                            }
                            require(worst<=2,"desktop picture is the UI over the middle of the picture behind");
                            require(back.unchanged(),"the picture behind is only read");
                        }
                        // Without a picture behind, or with one of another kind, there is none - and the headset's image is as before.
                        auto other=tall.descriptor();other.format=VK_FORMAT_R8G8B8A8_SRGB;transfer.draw(source.descriptor(),index,true,true,{},false,&other);
                        require(!transfer.desktopView().image&&transfer.desktopCapture().empty(),"a picture of another kind is left alone");
                        const auto again=transfer.draw(source.descriptor(),index,true);
                        require(!transfer.desktopView().image&&transfer.desktopCapture().empty(),"no desktop picture without the picture behind");
                        require(again==bytes,"the headset's UI image does not depend on the desktop's picture");
                    }
                    require(source.unchanged(),"source pixels and transfer layout preserved");
                    UiQuadPlacement placement;placement.localSpace=screen;placement.origin={euler(.3f,0,0),{0,1.5f,0}};
                    const auto before=xr.draws[0];xr.markUiRendered(snapshot.frame,placement);
                    if(!screen)for(unsigned eye=0;eye<2;++eye){eyes.draw(eye,snapshot.imageIndices[eye],snapshot.views[eye],"");xr.markEyeRendered(eye,snapshot.frame);}
                    xr.finishFrame();
                    require(xr.draws[0]==before+(screen?0:1),"actual GPU screen content and world eye rendering remain separate");
                }
                require(indices.size()==3,"all Runtime UI swapchain slots exercised");
            }
        }
        xr.shutdown();require(!xr.hasPendingFrame() && !xr.uiSwapchain().handle,"GPU/UI resources destroyed before session shutdown");
        std::cout<<"RESULT "<<checks<<" PASS 0 FAIL (actual GPU, synthetic UI/world)\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
