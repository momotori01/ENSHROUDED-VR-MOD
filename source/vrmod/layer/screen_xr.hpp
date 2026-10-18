#pragma once
#include <evr/menu_pointer.hpp>
#include <evr/screen_keys.hpp>
#include <evr/tiny_text.hpp>
// The game's picture on a plate in the headset, from the first frames the game presents (user, 2026-10-14: "from the
// start, consistently operable in VR is a must"; "the first menu does not have to be 2D - whichever is simpler and
// leads on smoothly"; before: "nothing in the headset for 30 seconds after entering, and the menu on the monitor, will
// not do: the play area and the keyboard are apart"). EVR_SCREEN_XR=1 in the game's environment (the loader sets it for
// a VR start):
//  - After the first presents the OpenXR session of the eyes is made here - the same LiveNativeXr the hooks' world
//    session then uses - and every present without a world frame under way becomes a screen frame: the picture the
//    game presents, on a plate fixed in the room ahead of where the player looked (VHVR's title screen: the GUI plate
//    fixed ahead of the rig; docs/analysis/vhvr_spec_20261010/ui_hud.md 1.3).
//  - A world frame that the hooks end without eyes (the first seconds in a world: the eyes' Views warm up, the
//    player's height settles) shows the same plate instead of nothing.
//  - While only the plate is shown the controllers work the game's menus: a hand's ray is the mouse and its trigger
//    the left button (as hooks/menu_pointer.hpp does in the world), a stick the arrow keys, A / X the game's "confirm"
//    (SPACE), B / Y its "back" (ESC) - include/evr/screen_keys.hpp. Mouse and keys go to the game's own window as
//    messages (the keys as raw input, hooks/key_script.hpp's way); nothing is injected system wide.
// The picture is copied as it is: the game's swapchain holds display values in an 8 bit RGBA image, the plate's
// swapchain is the same bytes read as sRGB. Another swapchain format (HDR) switches the plate off, with a note.
// Everything here runs on the thread that presents - the render thread, which is also the one the hooks' world frames
// use - inside the game's own present call, so the queue is the game's to use at that moment.
namespace screenXr {
inline bool requested(){static const bool on=[]{const auto f=std::getenv("EVR_SCREEN_XR");return f&&!std::strcmp(f,"1");}();return on;}
inline float eyeScale(){static const float s=[]{const auto f=std::getenv("EVR_SCREEN_XR_SCALE");const float v=f?float(std::atof(f)):1.f;return std::isfinite(v)&&v>=.25f&&v<=2.f?v:1.f;}();return s;}
constexpr float plateDistance=2.f,plateWidth=3.f;   // metres: about 74 degrees wide (VHVR: 5.33 m at 3 m, 83 degrees)
struct Slot {VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};VkSemaphore done{};};
struct State {
    Vulkan* v=nullptr;VkDevice device{};                       // the session's own binding (LiveNativeXr::xr->vk)
    PFN_vkCreateSemaphore createSemaphore=nullptr;PFN_vkDestroySemaphore destroySemaphore=nullptr;PFN_vkCmdCopyBufferToImage copyBufferToImage=nullptr;
    // The picture of the last present, kept in TRANSFER_SRC_OPTIMAL between its uses.
    VkImage saved{};VkDeviceMemory savedMemory{};VkFormat savedFormat=VK_FORMAT_UNDEFINED;uint32_t width=0,height=0;bool written=false;uint64_t savedPresent=0;
    std::array<Slot,3> slots{};uint32_t next=0;
    VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};  // the copy onto the plate
    VkBuffer dot{};VkDeviceMemory dotMemory{};uint8_t* dotBytes=nullptr;int dotLook=-1;
    VkBuffer text{};VkDeviceMemory textMemory{};uint32_t textWidth=0,textHeight=0;   // "VR Now Loading..." as pixels
    bool failed=false,formatTold=false,threadTold=false;unsigned failures=0;uint64_t frames=0,saves=0,worldPlates=0;
    bool plateSet=false;Pose origin=identityPose();evr::screen::Recentre recentre;uint64_t lastFrameTick=0;
    evr::menu::Buttons buttons;evr::menu::ClickSlack slack;POINT lastClient{-1,-1};HWND window{};uint64_t windowLooked=0;int pointerHand=-1;
    evr::screen::Keys keys;uint64_t moves=0,clicks=0,keysPosted=0;int lastState=-1;uint64_t lastLog=0;
};
inline State state;
constexpr uint32_t dotSize=14;

// ---- the keys: raw input records answered for handles of our own range (hooks/key_script.hpp) ----
namespace keys {
using RawFn=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
inline RawFn nativeRaw=nullptr;inline uintptr_t slot=0;inline bool installed=false,refused=false;
inline std::mutex ringMutex;inline std::array<RAWKEYBOARD,64> ring{};inline std::atomic<uint64_t> posted{0},served{0};
constexpr uintptr_t handleBase=0x4556530000ull;   // not a handle the system gives out; another range than the key script's
inline UINT WINAPI raw(HRAWINPUT handle,UINT command,LPVOID data,PUINT size,UINT header){
    const auto value=reinterpret_cast<uintptr_t>(handle);
    if((value&~uintptr_t(0xffff))==handleBase&&command==RID_INPUT&&size&&header==sizeof(RAWINPUTHEADER)){
        const UINT need=UINT(sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD));
        if(!data){*size=need;return 0;}
        if(*size<need){*size=need;return UINT(-1);}
        RAWINPUT input{};input.header.dwType=RIM_TYPEKEYBOARD;input.header.dwSize=need;
        {std::lock_guard<std::mutex> lock(ringMutex);input.data.keyboard=ring[(value&0xffff)%ring.size()];}
        std::memcpy(data,&input,need);++served;return need;
    }
    return nativeRaw(handle,command,data,size,header);
}
// The executable's own import slot of GetRawInputData (its import table; nothing outside the executable's image).
inline uintptr_t importSlot(){
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 0;
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE)return 0;
    const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!directory.VirtualAddress)return 0;
    for(auto d=reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base+directory.VirtualAddress);d->Name;++d){
        if(_stricmp(reinterpret_cast<const char*>(base+d->Name),"user32.dll")||!d->OriginalFirstThunk)continue;
        auto names=reinterpret_cast<const IMAGE_THUNK_DATA64*>(base+d->OriginalFirstThunk);auto slots=reinterpret_cast<const IMAGE_THUNK_DATA64*>(base+d->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
            if(!std::strcmp(reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData)->Name,"GetRawInputData"))return reinterpret_cast<uintptr_t>(&slots->u1.Function);
        }
    }
    return 0;
}
inline bool install(){
    if(installed)return true;if(refused)return false;
    try{
        const auto user=GetModuleHandleW(L"user32.dll");const auto native=reinterpret_cast<uintptr_t>(user?GetProcAddress(user,"GetRawInputData"):nullptr);
        slot=importSlot();
        if(!slot||!native||*reinterpret_cast<const uintptr_t*>(slot)!=native)throw std::runtime_error("the executable's import of GetRawInputData is not as expected");
        nativeRaw=reinterpret_cast<RawFn>(native);
        DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(slot),8,PAGE_READWRITE,&old))throw std::runtime_error("import slot not writable");
        const bool done=InterlockedCompareExchangePointer(reinterpret_cast<void**>(slot),reinterpret_cast<void*>(&raw),reinterpret_cast<void*>(native))==reinterpret_cast<void*>(native);
        DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(slot),8,old,&ignored);
        if(!done)throw std::runtime_error("import slot changed while it was written");
        installed=true;
        note("screen_keys_enabled","\"import_slot_rva\":"+std::to_string(slot-reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)))+",\"messages_to_the_game_window_only\":true,\"system_input_injected\":false");
        return true;
    }catch(const std::exception& e){refused=true;note("screen_keys_failed","\"reason\":"+jsonString(e.what()));return false;}
}
inline bool post(HWND window,evr::screen::Key key,bool down){
    struct Code{uint16_t make,vkey;bool extended;};
    static const Code codes[]={{0x48,VK_UP,true},{0x50,VK_DOWN,true},{0x4b,VK_LEFT,true},{0x4d,VK_RIGHT,true},{0x39,VK_SPACE,false},{0x01,VK_ESCAPE,false}};
    if(!window||!install())return false;
    const auto& c=codes[size_t(key)];const auto index=posted.fetch_add(1);
    RAWKEYBOARD k{};k.MakeCode=c.make;k.Flags=USHORT((down?RI_KEY_MAKE:RI_KEY_BREAK)|(c.extended?RI_KEY_E0:0));k.VKey=c.vkey;k.Message=down?WM_KEYDOWN:WM_KEYUP;
    {std::lock_guard<std::mutex> lock(ringMutex);ring[index%ring.size()]=k;}
    return PostMessageW(window,WM_INPUT,RIM_INPUT,LPARAM(handleBase+(index%ring.size())))!=0;
}
}

// ---- our own device objects, made with the session's binding and released before it goes ----
inline void release() noexcept {
    try{
        if(!state.v){state.saved={};state.written=false;return;}
        auto& v=*state.v;v.vkDeviceWaitIdle(v.device);
        for(auto& s:state.slots){if(s.fence)v.vkDestroyFence(v.device,s.fence,nullptr);if(s.done&&state.destroySemaphore)state.destroySemaphore(v.device,s.done,nullptr);if(s.pool)v.vkDestroyCommandPool(v.device,s.pool,nullptr);s={};}
        if(state.fence)v.vkDestroyFence(v.device,state.fence,nullptr);if(state.pool)v.vkDestroyCommandPool(v.device,state.pool,nullptr);
        if(state.dotBytes)v.vkUnmapMemory(v.device,state.dotMemory);if(state.dot)v.vkDestroyBuffer(v.device,state.dot,nullptr);if(state.dotMemory)v.vkFreeMemory(v.device,state.dotMemory,nullptr);
        if(state.text)v.vkDestroyBuffer(v.device,state.text,nullptr);if(state.textMemory)v.vkFreeMemory(v.device,state.textMemory,nullptr);
        if(state.saved)v.vkDestroyImage(v.device,state.saved,nullptr);if(state.savedMemory)v.vkFreeMemory(v.device,state.savedMemory,nullptr);
        note("screen_xr_released","\"frames\":"+std::to_string(state.frames)+",\"world_frames_with_the_plate\":"+std::to_string(state.worldPlates)+",\"pictures_saved\":"+std::to_string(state.saves));
    }catch(...){}
    state.v=nullptr;state.device={};state.saved={};state.savedMemory={};state.written=false;state.width=state.height=0;state.fence={};state.pool={};state.cmd={};
    state.dot={};state.dotMemory={};state.dotBytes=nullptr;state.dotLook=-1;state.plateSet=false;
    state.text={};state.textMemory={};state.textWidth=state.textHeight=0;
}
inline void ensure(LiveNativeXr& live,const SwapData& swap){
    auto& v=live.xr->vk;
    if(state.v==&v&&state.saved&&state.width==swap.width&&state.height==swap.height&&state.savedFormat==swap.format)return;
    if(state.v)release();
    state.v=&v;state.device=v.device;
#define SCREEN_LOAD(member,name) state.member=reinterpret_cast<PFN_##name>(v.vkGetDeviceProcAddr(v.device,#name));if(!state.member)throw std::runtime_error("missing " #name);
    SCREEN_LOAD(createSemaphore,vkCreateSemaphore) SCREEN_LOAD(destroySemaphore,vkDestroySemaphore) SCREEN_LOAD(copyBufferToImage,vkCmdCopyBufferToImage)
#undef SCREEN_LOAD
    VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=swap.format;i.extent={swap.width,swap.height,1};i.mipLevels=i.arrayLayers=1;
    i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    vkCheck(v.vkCreateImage(v.device,&i,nullptr,&state.saved),"screen picture image");VkMemoryRequirements req{};v.vkGetImageMemoryRequirements(v.device,state.saved,&req);
    state.savedMemory=v.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(v.vkBindImageMemory(v.device,state.saved,state.savedMemory,0),"screen picture memory");
    state.savedFormat=swap.format;state.width=swap.width;state.height=swap.height;state.written=false;
    const auto pool=[&](VkCommandPool& p,VkCommandBuffer& c,VkFence& f,bool signaled){
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pc.queueFamilyIndex=v.family;
        vkCheck(v.vkCreateCommandPool(v.device,&pc,nullptr,&p),"screen command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=p;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkCheck(v.vkAllocateCommandBuffers(v.device,&ca,&c),"screen command buffer");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fc.flags=signaled?VK_FENCE_CREATE_SIGNALED_BIT:0;vkCheck(v.vkCreateFence(v.device,&fc,nullptr,&f),"screen fence");};
    for(auto& s:state.slots){pool(s.pool,s.cmd,s.fence,true);VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};vkCheck(state.createSemaphore(v.device,&sc,nullptr,&s.done),"screen semaphore");}
    pool(state.pool,state.cmd,state.fence,true);
    // The pointer's mark: a small block of pixels in a buffer, copied onto the plate's image where the ray meets it.
    VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=dotSize*dotSize*4;b.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;b.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&state.dot),"screen pointer buffer");VkMemoryRequirements breq{};v.vkGetBufferMemoryRequirements(v.device,state.dot,&breq);
    state.dotMemory=v.allocate(breq,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(v.device,state.dot,state.dotMemory,0),"screen pointer memory");
    void* mapped=nullptr;vkCheck(v.vkMapMemory(v.device,state.dotMemory,0,VK_WHOLE_SIZE,0,&mapped),"screen pointer map");state.dotBytes=static_cast<uint8_t*>(mapped);state.dotLook=-1;
    // "VR Now Loading...": white on a dark band, about 1/22 of the picture high, shown while a world's eyes warm up.
    {const uint32_t scale=std::max<uint32_t>(2,swap.height/180);
     const auto picture=evr::text::render("VR Now Loading...",scale,scale*3,{245,245,245,255},{18,18,18,255});
     if(picture.width&&picture.width<=swap.width&&picture.height<=swap.height){
        VkBufferCreateInfo t{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};t.size=picture.rgba.size();t.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;t.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(v.vkCreateBuffer(v.device,&t,nullptr,&state.text),"screen text buffer");VkMemoryRequirements treq{};v.vkGetBufferMemoryRequirements(v.device,state.text,&treq);
        state.textMemory=v.allocate(treq,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(v.device,state.text,state.textMemory,0),"screen text memory");
        void* bytes=nullptr;vkCheck(v.vkMapMemory(v.device,state.textMemory,0,VK_WHOLE_SIZE,0,&bytes),"screen text map");
        std::memcpy(bytes,picture.rgba.data(),picture.rgba.size());v.vkUnmapMemory(v.device,state.textMemory);
        state.textWidth=picture.width;state.textHeight=picture.height;}}
    note("screen_xr_ready","\"width\":"+std::to_string(swap.width)+",\"height\":"+std::to_string(swap.height)+",\"format\":"+std::to_string(swap.format)+",\"plate_m\":["+std::to_string(plateWidth)+","+std::to_string(plateWidth*float(swap.height)/float(swap.width))+
        "],\"distance_m\":"+std::to_string(plateDistance));
}
inline void barrier(Vulkan& v,VkCommandBuffer cmd,VkImage image,VkImageLayout from,VkImageLayout to,VkAccessFlags src,VkAccessFlags dst){
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    b.image=image;b.oldLayout=from;b.newLayout=to;b.srcAccessMask=src;b.dstAccessMask=dst;
    v.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
}
// The picture that is about to be presented, copied into our own image on the present queue: the copy waits for the
// game's rendering (the present's semaphores) and the present waits for the copy, as the desktop mirror does.
inline bool save(VkQueue queue,const VkPresentInfoKHR* present,VkPresentInfoKHR& replacement,VkSemaphore& waitOut,const SwapData& swap){
    auto& v=*state.v;auto& slot=state.slots[state.next];state.next=(state.next+1)%uint32_t(state.slots.size());
    if(v.vkWaitForFences(v.device,1,&slot.fence,VK_TRUE,1000000000ull)!=VK_SUCCESS)return false;
    v.vkResetFences(v.device,1,&slot.fence);v.vkResetCommandBuffer(slot.cmd,0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkCheck(v.vkBeginCommandBuffer(slot.cmd,&bi),"screen save begin");
    const auto source=swap.images[present->pImageIndices[0]];
    barrier(v,slot.cmd,source,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    barrier(v,slot.cmd,state.saved,state.written?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,state.written?VK_ACCESS_TRANSFER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={state.width,state.height,1};
    v.vkCmdCopyImage(slot.cmd,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,state.saved,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier(v,slot.cmd,state.saved,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    barrier(v,slot.cmd,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_MEMORY_READ_BIT);
    vkCheck(v.vkEndCommandBuffer(slot.cmd),"screen save end");
    std::vector<VkPipelineStageFlags> stages(replacement.waitSemaphoreCount,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.waitSemaphoreCount=replacement.waitSemaphoreCount;s.pWaitSemaphores=replacement.pWaitSemaphores;s.pWaitDstStageMask=stages.data();
    s.commandBufferCount=1;s.pCommandBuffers=&slot.cmd;s.signalSemaphoreCount=1;s.pSignalSemaphores=&slot.done;
    vkCheck(v.vkQueueSubmit(queue,1,&s,slot.fence),"screen save submit");
    waitOut=slot.done;replacement.waitSemaphoreCount=1;replacement.pWaitSemaphores=&waitOut;
    state.written=true;state.savedPresent=presents.load();++state.saves;return true;
}
struct Mark {bool shown=false;int x=0,y=0,look=0;};   // the pointer's place in the picture; look 1 = a button is down
// While a world's eyes warm up (the first seconds of a world session, before its first pair) the plate keeps the last
// picture from before: the game's own view is then the first-person camera inside the player's body (game start
// 20261014-115547, screen-plate-world). "VR Now Loading..." blinks near the lower edge (half a second on, half off)
// to show that it goes on - the walking green block until 2026-10-14 (user: "make it a blinking VR Now Loading...").
inline bool loadingShown(uint64_t now){return state.text&&(now/500)%2==0;}
// The saved picture onto the plate's image, the pointer's mark over it; done before the frame is handed over.
inline void copyToPlate(OpenXrClient& xr,uint32_t index,const Mark& mark,bool loading=false){
    auto& v=*state.v;const auto target=xr.uiSwapchain().images[index].image;
    vkCheck(v.vkWaitForFences(v.device,1,&state.fence,VK_TRUE,2000000000ull),"screen plate previous copy");
    v.vkResetFences(v.device,1,&state.fence);v.vkResetCommandBuffer(state.cmd,0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkCheck(v.vkBeginCommandBuffer(state.cmd,&bi),"screen plate begin");
    barrier(v,state.cmd,target,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={state.width,state.height,1};
    v.vkCmdCopyImage(state.cmd,state.saved,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    if(mark.shown&&state.dotBytes){
        if(state.dotLook!=mark.look){   // a white block with a dark rim; green while a button is down
            for(uint32_t y=0;y<dotSize;y++)for(uint32_t x=0;x<dotSize;x++){auto* p=state.dotBytes+(size_t(y)*dotSize+x)*4;
                const bool rim=x<2||y<2||x>=dotSize-2||y>=dotSize-2;
                p[0]=rim?0:mark.look?60:255;p[1]=rim?0:mark.look?230:255;p[2]=rim?0:mark.look?60:255;p[3]=255;}
            state.dotLook=mark.look;}
        const int half=int(dotSize/2),x0=std::clamp(mark.x-half,0,int(state.width)-int(dotSize)),y0=std::clamp(mark.y-half,0,int(state.height)-int(dotSize));
        if(state.width>=dotSize&&state.height>=dotSize){
            VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageOffset={x0,y0,0};region.imageExtent={dotSize,dotSize,1};
            state.copyBufferToImage(state.cmd,state.dot,target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);}
    }
    if(loading&&state.text&&state.textWidth<=state.width&&state.textHeight<=state.height){
        const int x0=int(state.width-state.textWidth)/2,y0=std::clamp(int(state.height)-int(state.height)/12-int(state.textHeight)/2,0,int(state.height-state.textHeight));
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageOffset={x0,y0,0};region.imageExtent={state.textWidth,state.textHeight,1};
        state.copyBufferToImage(state.cmd,state.text,target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);}
    barrier(v,state.cmd,target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    vkCheck(v.vkEndCommandBuffer(state.cmd),"screen plate end");
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&state.cmd;
    vkCheck(v.vkQueueSubmit(v.queue,1,&s,state.fence),"screen plate submit");
    vkCheck(v.vkWaitForFences(v.device,1,&state.fence,VK_TRUE,2000000000ull),"screen plate copy");
}
// Test only (EVR_SCREEN_XR_DUMP=<n>,<m> in the game's environment): the plate's image of the n-th screen frame and of
// the m-th world frame with the plate, read back into <output>/native-xr/screen-plate-<tag>.rgba8 - what the headset
// is given cannot be seen from outside otherwise. Nothing of it runs without the variable.
inline uint64_t dumpAt(bool own){
    static const std::array<uint64_t,2> at=[]{std::array<uint64_t,2> v{};const auto f=std::getenv("EVR_SCREEN_XR_DUMP");
        if(f){char* end=nullptr;v[0]=std::strtoull(f,&end,10);if(end&&*end==',')v[1]=std::strtoull(end+1,nullptr,10);}return v;}();
    return at[own?0:1];
}
inline void dumpPlate(LiveNativeXr& live,uint32_t index,const char* tag){
    auto& v=*state.v;auto& xr=*live.xr;const auto target=xr.uiSwapchain().images[index].image;const VkDeviceSize bytes=VkDeviceSize(state.width)*state.height*4;
    VkBuffer buffer{};VkDeviceMemory memory{};
    try{
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;b.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(v.vkCreateBuffer(v.device,&b,nullptr,&buffer),"plate dump buffer");VkMemoryRequirements req{};v.vkGetBufferMemoryRequirements(v.device,buffer,&req);
        memory=v.allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);vkCheck(v.vkBindBufferMemory(v.device,buffer,memory,0),"plate dump memory");
        v.vkResetFences(v.device,1,&state.fence);v.vkResetCommandBuffer(state.cmd,0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkCheck(v.vkBeginCommandBuffer(state.cmd,&bi),"plate dump begin");
        barrier(v,state.cmd,target,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={state.width,state.height,1};
        v.vkCmdCopyImageToBuffer(state.cmd,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&region);
        barrier(v,state.cmd,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        vkCheck(v.vkEndCommandBuffer(state.cmd),"plate dump end");
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&state.cmd;
        vkCheck(v.vkQueueSubmit(v.queue,1,&s,state.fence),"plate dump submit");vkCheck(v.vkWaitForFences(v.device,1,&state.fence,VK_TRUE,5000000000ull),"plate dump wait");
        void* mapped=nullptr;vkCheck(v.vkMapMemory(v.device,memory,0,VK_WHOLE_SIZE,0,&mapped),"plate dump map");
        const auto* p=static_cast<const uint8_t*>(mapped);uint64_t sum[3]{};for(VkDeviceSize i=0;i<bytes;i+=4*97){sum[0]+=p[i];sum[1]+=p[i+1];sum[2]+=p[i+2];}
        const auto samples=double((bytes+4*97-1)/(4*97));const auto name=output+"/native-xr/screen-plate-"+tag+".rgba8";
        {std::ofstream file(name,std::ios::binary);file.write(static_cast<const char*>(mapped),std::streamsize(bytes));}
        v.vkUnmapMemory(v.device,memory);
        live.log->event("screen_plate_dump","\"tag\":"+jsonString(tag)+",\"width\":"+std::to_string(state.width)+",\"height\":"+std::to_string(state.height)+",\"mean_rgb\":["+std::to_string(sum[0]/samples)+","+std::to_string(sum[1]/samples)+","+std::to_string(sum[2]/samples)+
            "],\"file\":"+jsonString(name));
    }catch(const std::exception& e){live.log->event("screen_plate_dump_failed","\"reason\":"+jsonString(e.what()));}
    if(buffer)v.vkDestroyBuffer(v.device,buffer,nullptr);if(memory)v.vkFreeMemory(v.device,memory,nullptr);
}
inline BOOL CALLBACK findWindow(HWND window,LPARAM out){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId()||!IsWindowVisible(window)||GetWindow(window,GW_OWNER))return TRUE;
    *reinterpret_cast<HWND*>(out)=window;return FALSE;
}
inline HWND gameWindow(){
    const auto now=GetTickCount64();
    if(state.window&&IsWindow(state.window))return state.window;
    if(now-state.windowLooked<1000)return nullptr;
    state.windowLooked=now;state.window=nullptr;EnumWindows(findWindow,reinterpret_cast<LPARAM>(&state.window));return state.window;
}
// A button or key that is still down gets its release when the plate stops being the only thing shown.
inline void letGo() noexcept {
    try{
        const HWND h=state.window&&IsWindow(state.window)?state.window:nullptr;
        if(h&&state.lastClient.x>=0&&state.buttons.left)PostMessageW(h,WM_LBUTTONUP,0,MAKELPARAM(state.lastClient.x,state.lastClient.y));
        state.buttons={};state.slack={};
        for(const auto& e:state.keys.release())if(h)keys::post(h,e.key,e.down);
    }catch(...){}
}
// The controllers at the plate: the mouse and the keys the game is told, and where the mark goes.
inline Mark controllers(LiveNativeXr& live,const OpenXrFrameSnapshot& snapshot){
    Mark mark;if(!live.hands)return mark;
    const auto hands=live.hands->sample(live.xr->local,snapshot.predictedDisplayTime,snapshot.frame,1);
    const HWND h=gameWindow();RECT client{};
    if(!hands.focused||!h||!GetClientRect(h,&client)||client.right<=0||client.bottom<=0){letGo();return mark;}
    // No look at the system's cursor here. The game hides it while the pad is its device - which it is after a VR
    // session, the controllers being the pad there - so back at the main menu nothing was sent (headset run
    // 20261014-124515: "after going back to the main menu the controllers cannot choose anything; the keyboard can").
    // The pointer's first posted move turns the game back to the mouse (hooks/menu_pointer.hpp, note above).
    const auto now=GetTickCount64();
    const auto plate=evr::menu::fixedPlate(state.origin,plateDistance,plateWidth,plateWidth*float(state.height)/float(state.width),uint32_t(client.right),uint32_t(client.bottom));
    // The hand whose ray is on the plate; the right one first, and the hand that holds a click keeps it.
    evr::menu::Spot spot;int hand=-1;
    const auto rayOf=[&](int i){const auto& s=hands.hands[size_t(i)];
        return s.aimActive&&(s.aimFlags&3)==3?evr::menu::spot(plate,s.aim.position,rotate(s.aim.orientation,{0,0,-1})):evr::menu::Spot{};};
    const int order[2]={state.buttons.left&&state.pointerHand>=0?state.pointerHand:1,state.buttons.left&&state.pointerHand>=0?1-state.pointerHand:0};
    for(const int i:order){const auto s=rayOf(i);if(s.on){spot=s;hand=i;break;}}
    const bool ray=spot.on,pull=hand>=0&&hands.hands[size_t(hand)].triggerActive&&hands.hands[size_t(hand)].trigger>.5f;
    if(hand>=0)state.pointerHand=hand;
    const auto change=state.buttons.update(ray,pull,false);
    const auto held=[&]{return WPARAM(state.buttons.left?MK_LBUTTON:0);};
    auto shown=spot;
    // As in hooks/menu_pointer.hpp: the place on every frame while the ray is on the plate, and once more right
    // before a button (the game makes its button events at the place of the last WM_MOUSEMOVE).
    if(ray){shown=state.slack.filter(spot,state.buttons.left);state.lastClient={shown.pixelX,shown.pixelY};PostMessageW(h,WM_MOUSEMOVE,held(),MAKELPARAM(state.lastClient.x,state.lastClient.y));++state.moves;}
    else state.slack.filter(spot,false);
    if(state.lastClient.x>=0){
        const auto place=MAKELPARAM(state.lastClient.x,state.lastClient.y);
        if(change.any()&&!ray){PostMessageW(h,WM_MOUSEMOVE,held(),place);++state.moves;}
        if(change.leftDown){PostMessageW(h,WM_LBUTTONDOWN,held(),place);++state.clicks;}
        if(change.leftUp){PostMessageW(h,WM_LBUTTONUP,held(),place);++state.clicks;}
    }
    const auto& l=hands.hands[0];const auto& r=hands.hands[1];
    const auto pad=evr::screen::pad(l.stick.x,l.stick.y,l.stickActive,r.stick.x,r.stick.y,r.stickActive,
        (l.primaryActive&&l.primaryPressed)||(r.primaryActive&&r.primaryPressed),(l.secondaryActive&&l.secondaryPressed)||(r.secondaryActive&&r.secondaryPressed));
    const auto events=state.keys.update(pad,now);
    for(const auto& e:events){keys::post(h,e.key,e.down);++state.keysPosted;}
    if(ray){mark.shown=true;mark.look=state.buttons.left?1:0;
        mark.x=int(float(shown.pixelX)*float(state.width)/float(client.right));mark.y=int(float(shown.pixelY)*float(state.height)/float(client.bottom));}
    const int told=(ray?1:0)|(state.buttons.left?2:0)|(state.keys.anyDown()?4:0)|((hand+1)<<4);
    if(told!=state.lastState||change.any()||!events.empty()||now>=state.lastLog+5000){
        std::ostringstream o;o<<"\"frame\":"<<snapshot.frame<<",\"ray_on_plate\":"<<(ray?"true":"false")<<",\"hand\":"<<hand<<",\"pixel\":["<<shown.pixelX<<','<<shown.pixelY<<"],\"client\":["<<client.right<<','<<client.bottom
            <<"],\"left_button\":"<<(state.buttons.left?"true":"false")<<",\"left_down\":"<<(change.leftDown?"true":"false")<<",\"left_up\":"<<(change.leftUp?"true":"false")<<",\"keys\":[";
        for(size_t i=0;i<events.size();i++)o<<(i?",":"")<<'['<<int(events[i].key)<<','<<(events[i].down?1:0)<<']';
        o<<"],\"stick\":["<<std::setprecision(3)<<pad.x<<','<<pad.y<<"],\"confirm\":"<<(pad.confirm?"true":"false")<<",\"back\":"<<(pad.back?"true":"false")
            <<",\"moves_posted\":"<<state.moves<<",\"clicks_posted\":"<<state.clicks<<",\"keys_posted\":"<<state.keysPosted<<",\"keys_served\":"<<keys::served.load();
        live.log->event("screen_controllers",o.str());state.lastState=told;state.lastLog=now;
    }
    return mark;
}
// The pending frame - a screen frame of our own, or a world frame without eyes that became one - shows the plate.
inline void frame(LiveNativeXr& live,bool own){
    auto& xr=*live.xr;const auto snapshot=xr.pendingFrame();
    if(!snapshot.shouldRender){xr.finishFrame();return;}
    const auto now=GetTickCount64();const auto& head=xr.headLocation;const bool headValid=(head.locationFlags&3)==3;
    // Where the plate stands: ahead of where the player looked when it was first shown, or shown again after the
    // world's eyes, or after they turned away from it for a while.
    if(headValid){
        const auto ahead=rotate(head.pose.orientation,{0,0,-1});const auto centre=compose(state.origin,Pose{{0,0,0,1},{0,0,-plateDistance}}).position;
        const bool again=!state.plateSet||(own&&now-state.lastFrameTick>3000)||(own&&state.recentre.update(centre.x-head.pose.position.x,centre.z-head.pose.position.z,ahead.x,ahead.z,now));
        if(again){state.origin=evr::menu::plateOrigin(head.pose);state.plateSet=true;
            std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<snapshot.frame<<",\"origin_position\":["<<state.origin.position.x<<','<<state.origin.position.y<<','<<state.origin.position.z<<"],\"origin_orientation\":["<<state.origin.orientation.x<<','
                <<state.origin.orientation.y<<','<<state.origin.orientation.z<<','<<state.origin.orientation.w<<"],\"distance_m\":"<<plateDistance<<",\"width_m\":"<<plateWidth<<",\"space\":\"LOCAL\"";
            live.log->event("screen_plate_placed",o.str());}
    }
    if(own)state.lastFrameTick=now;
    if(!state.plateSet||!state.written){xr.skipFrame(state.written?"screen_waiting_for_the_head":"screen_without_a_picture");return;}
    Mark mark;const bool loading=!own&&live.worldSession&&!live.worldPairs;
    if(own)try{mark=controllers(live,snapshot);}
        catch(const std::exception& e){static unsigned told=0;if(told++<3)live.log->event("screen_controllers_failed","\"reason\":"+jsonString(e.what()));letGo();}
    if(live.uiTransfer&&(xr.uiSwapchain().width!=state.width||xr.uiSwapchain().height!=state.height||xr.uiSwapchainFormat()!=VK_FORMAT_R8G8B8A8_SRGB)){
        // The HUD's transfer of the last world session was made for another image: it goes before the image does.
        if(live.uiTransfer->desktopImage())desktopMirrorSource(live.device,VK_NULL_HANDLE);
        vkCheck(xr.vk.vkDeviceWaitIdle(xr.vk.device),"screen plate: HUD transfer retired");live.uiTransfer.reset();live.log->event("native_ui_transfer_destroyed","\"reason\":\"screen_plate_size\"");}
    xr.configureUi(state.width,state.height,VK_FORMAT_R8G8B8A8_SRGB);
    const auto index=xr.acquireUi(snapshot.frame);
    copyToPlate(xr,index,mark,loading&&loadingShown(now));
    if(dumpAt(own)&&(own?state.frames+1:state.worldPlates+1)==dumpAt(own))dumpPlate(live,index,own?"screen":"world");
    UiQuadPlacement placement;placement.localSpace=true;placement.origin=state.origin;placement.distance=plateDistance;placement.width=plateWidth;placement.opaque=true;
    xr.markUiRendered(snapshot.frame,placement);xr.finishFrame();
    const auto n=own?++state.frames:++state.worldPlates;
    if(n<=3||n%1800==0)live.log->event(own?"screen_plate_frame":"screen_plate_world_frame","\"frame\":"+std::to_string(snapshot.frame)+",\"count\":"+std::to_string(n)+",\"picture_of_present\":"+std::to_string(state.savedPresent)+
        ",\"pointer_shown\":"+(mark.shown?"true":"false"));
}
// A world frame that the hooks end without eyes: the plate instead of nothing. True = the frame is ended here.
inline bool insteadOfEyes(LiveNativeXr& live) noexcept {
    if(!requested()||state.failed||!state.v||!state.written||!live.xr->hasPendingFrame())return false;
    try{
        const bool warming=live.worldSession&&!live.worldPairs;               // the picture from before the session stays
        if(!warming&&presents.load()-state.savedPresent>4)return false;          // no picture of this moment
        if(!live.xr->pendingFrame().shouldRender)return false;
        live.xr->screenInsteadOfEyes();frame(live,false);return true;
    }catch(const std::exception& e){
        if(++state.failures<=5)note("screen_xr_failed","\"where\":\"world frame without eyes\",\"reason\":"+jsonString(e.what()));
        if(state.failures>=5)state.failed=true;
        try{if(live.xr->hasPendingFrame())live.xr->skipFrame("screen_plate_failure");}catch(...){live.xr->abortFrame("screen_plate_failure");}
        return true;
    }
}
// Once per present, before the desktop mirror. May replace what the present waits for (as the mirror does).
inline void present(VkQueue queue,const VkPresentInfoKHR* p,VkPresentInfoKHR& replacement,VkSemaphore& waitOut) noexcept {
    if(!requested()||state.failed||!p||p->swapchainCount!=1)return;
    try{
        DeviceDispatch dd{};uint32_t family=UINT32_MAX;SwapData swap;
        {std::lock_guard<std::mutex> lock(mutex);
            auto found=devices.find(key(queue));if(found==devices.end())return;dd=found->second;
            auto f=queueFamilies.find(queue);if(f!=queueFamilies.end())family=f->second;
            auto s=swapData.find(p->pSwapchains[0]);if(s==swapData.end())return;swap=s->second;}
        if(family==UINT32_MAX||!dd.fixtureBinding||p->pImageIndices[0]>=swap.images.size())return;
        if(!(swap.usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)||(swap.format!=VK_FORMAT_R8G8B8A8_UNORM&&swap.format!=VK_FORMAT_R8G8B8A8_SRGB)){
            if(!state.formatTold){state.formatTold=true;note("screen_xr_off","\"reason\":\"the game's swapchain cannot be copied as it is\",\"format\":"+std::to_string(swap.format)+",\"usage\":"+std::to_string(swap.usage));}
            return;}
        if(!liveNativeXr){
            if(presents.load()<30)return;                                          // the game's window first
            // The session's binding wants the GPU's application-level handle (what the hooks hand over from the game).
            std::shared_ptr<diagnostic::XrPreflight> preflight;
            {std::lock_guard<std::mutex> lock(mutex);auto parent=instances.find(key(dd.instance));if(parent!=instances.end()&&!parent->second.fixture)preflight=parent->second.preflight;}
            if(!preflight){state.failed=true;note("screen_xr_failed","\"where\":\"session\",\"reason\":\"OpenXR was not prepared for this instance\"");return;}
            const auto gpu=preflight->runtimeGpu(dd.instance);
            const int made=createLiveNativeXr(dd.instance,gpu,dd.device,dd.fixtureBinding->queueFamily,eyeScale(),diagnostic::maximumNativeDiagnosticFrames);
            if(made||!liveNativeXr){state.failed=true;note("screen_xr_failed","\"where\":\"session\",\"result\":"+std::to_string(made));return;}
            liveNativeXr->log->event("screen_xr_session","\"eye_scale\":"+std::to_string(eyeScale())+",\"present\":"+std::to_string(presents.load())+",\"thread\":"+std::to_string(GetCurrentThreadId()));
        }
        auto& live=*liveNativeXr;
        if(live.thread!=GetCurrentThreadId()||live.device!=dd.device||queue!=live.xr->vk.queue){
            if(!state.threadTold){state.threadTold=true;note("screen_xr_off","\"reason\":\"another thread, device or queue presents than the session's\",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"session_thread\":"+std::to_string(live.thread));}
            return;}
        const bool worldFrame=live.xr->hasPendingFrame();
        const bool hooksDriving=live.worldSession&&GetTickCount64()-live.lastWorldBegin<=500;
        if((worldFrame||hooksDriving)&&!live.screenWanted){if(state.buttons.left||state.keys.anyDown())letGo();return;}   // the eyes are drawn
        ensure(live,swap);
        const bool warming=(worldFrame||hooksDriving)&&live.worldSession&&!live.worldPairs&&state.written;
        if(!warming&&!save(queue,p,replacement,waitOut,swap))return;
        if(worldFrame||hooksDriving){if(state.buttons.left||state.keys.anyDown())letGo();return;}                           // its end shows the plate (insteadOfEyes)
        if(!live.xr->beginScreenFrame())return;                                    // the session is not running (yet)
        frame(live,true);
    }catch(const std::exception& e){
        if(++state.failures<=5)note("screen_xr_failed","\"where\":\"present\",\"reason\":"+jsonString(e.what()));
        if(state.failures>=5||!liveNativeXr)state.failed=true;   // no session could be made: not tried on every present
        try{if(liveNativeXr&&liveNativeXr->xr&&liveNativeXr->xr->hasPendingFrame())liveNativeXr->xr->abortFrame("screen_frame_failure");}catch(...){}
    }
}
}
