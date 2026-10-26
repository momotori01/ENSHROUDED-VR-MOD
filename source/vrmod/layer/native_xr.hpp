#pragma once
#include <evr/native_xr_bridge.hpp>
#include <evr/capture_burst.hpp>
#include <evr/stereo_transfer.hpp>
#include <evr/ui_transfer.hpp>
#include <evr/hud_fade.hpp>
#include <evr/edge_tint.hpp>
#include <evr/eye_mark.hpp>
#include <evr/eye_panel.hpp>
#include <evr/eye_history.hpp>
#include <evr/eye_warp.hpp>
#include <evr/aim_mark.hpp>
// Explicit native-renderer diagnostic entry points. Caller holds all native queue
// gates and uses one render thread for every entry. No automatic session creation.
struct LiveNativeXr
{
    DWORD thread = GetCurrentThreadId();
    VkDevice device{};
    uint64_t beginPresent = 0;
    uint32_t frameLimit = 1;
    uint32_t submittedPairs = 0;
    uint64_t generation = 0;
    uint64_t capturedFrame = 0; // successfully saved eye pair; queried on the same render thread
    std::shared_ptr<diagnostic::XrPreflight> owner;
    std::unique_ptr<Log> log;
    std::unique_ptr<OpenXrClient> xr;
    std::unique_ptr<motion::Actions> hands;
    // Declared before transfer: destroyed after the transfer fence retired, before the XR client/device.
    std::unique_ptr<EdgeTint> edgeTint;
    std::unique_ptr<EyeMark> eyeMark; // the aim mark's dot (evrSetAimMark), like the tint
    std::unique_ptr<EyePanel> eyePanel; uint64_t aimPanelSerial=0; // the picture beside the mark (evrSetAimPanel)
    // Alternate-eye warping (eye_warp.hpp; EVR_EYE_WARP=1 with EVR_ALTERNATE_EYES=1): one per eye, as the eyes' extents
    // differ. Declared before the transfer: the transfer's keep copies retire first.
    std::unique_ptr<EyeWarp> warp;   // one for both eyes (the third stage draws one eye from the other's picture)
    bool warpWanted=false,warpFailed=false,warpFromOther=false,warpCheck=false;uint64_t warpFrames=0,warpFollowed=0,warpCaptures=0,warpCheckCaptures=0,warpKeepMisses=0,warpChecks=0,warpCheckFrames=0,warpCheckEvery=120;
    std::array<uint64_t,2> depthNoteUsed{};   // the depth note each eye last kept (its sequence number)
    // The bug report's last 10 seconds of both eyes (eye_history.hpp; EVR_BUG_HISTORY=0 leaves it out). Declared before the
    // transfer: its blits retire with the transfer's fence first.
    std::unique_ptr<EyeHistory> history;bool historyFailed=false;
    std::unique_ptr<StereoTransfer> transfer;
    std::unique_ptr<UiTransfer> uiTransfer;
    uint64_t desktopUiFrames=0;
    bool uiDisabled=false;
    bool alternateRefused=false;   // the runtime refused a frame with one eye's earlier picture: both eyes from then on
    // The mod's own queue (layer/diagnostic_layer.cpp evrCreateDevice, EVR_AFW_PACING=2 at the start; 2026-10-23): the XR
    // binding's queue beside the game's, for the refresh thread's warps (include/evr/afw_pacing.hpp pacing 2). What the game
    // drew is then read on another queue than it was drawn on: each frame's copy waits for the game's queue (fromGame), and
    // the game's queue for the copy (toGame) - semaphores, each wait followed by a barrier (ownQueueIn / ownQueueOut).
    struct OwnQueue {VkQueue game{};VkSemaphore fromGame{},toGame{};VkCommandPool pool{};VkCommandBuffer xrBarrier{},gameBarrier{};
        PFN_vkCreateSemaphore createSemaphore=nullptr;PFN_vkDestroySemaphore destroySemaphore=nullptr;} own;bool ownQueue=false;
    // A game frame while the refresh thread shows the eyes: its eyes are copied (with what is drawn over them) into these
    // instead of the runtime's images, and kept from them for the warps (StereoTransfer keepDrawn).
    struct Slot {VkImage image{};VkDeviceMemory memory{};uint32_t width=0,height=0;} slots[2];
    // The world's motion under the tracking space per game frame (the hooks' evrSetNativeXrWorldMotion), for carrying the
    // kept pictures on to each refresh (drawRefresh); written by the game's thread and read by the refresh thread under
    // the queue lock.
    struct Motion {uint64_t frame=0;XrPosef perFrame{{0,0,0,1},{0,0,0}};int64_t dt=0;XrTime time=0;bool valid=false;};
    Motion pendingMotion{};std::array<Motion,16> motions{};
    XrTime lastHandTime=0;   // the time of the hands' last sample (evrBeginNativeXr)
    bool refreshGaveUp=false,shotsDone=false;uint64_t refreshCaptures=0,refreshLogs=0;
    ~LiveNativeXr(){if(xr)xr->stopRefreshing();}   // before the warp and the transfers go
    // HUD auto-hide (hud_fade.hpp): what changed in the UI, and the opacity of the HUD outside the pins.
    HudChangeDetector hudChanges;HudFade hudFade;uint64_t hudProbeSeen=0;int hudLoggedMode=-1;bool hudLoggedHidden=false;unsigned hudLogs=0;
    uint64_t uiOverlaySerial=0;unsigned uiOverlayLogs=0;   // the overlay picture last given to the UI transfer (evrSetUiOverlay)
    bool uiPlateLogged=false;unsigned uiPlateLogs=0; // the menu's plate (evrSetUiPlate): what the record last told
    bool aimClusterLogged=false;unsigned aimClusterLogs=0; // the aim cluster's quads: what the record last told
    int targetHudState=-1;unsigned targetHudStateLogs=0;   // the health bar's part: what the record last told (target_hud_state)
    // Opt-in (EVR_XR_ASYNC_COMPLETION=1): release after queue-ordered submission instead of
    // a per-frame device idle. Only when the binding family has exactly one created queue,
    // so the eye writer's family proves the same VkQueue as the XR binding queue.
    bool asyncCompletion=false;
    // The plate (layer/screen_xr.hpp, EVR_SCREEN_XR=1). worldSession: the hooks' world frames use this session since
    // their first begin, until they close it. screenWanted: the last world frame ended without eyes (or there was
    // none yet), so the picture of each present is kept for the plate; a submitted pair of eyes clears it.
    // worldPairs: pairs of eyes submitted since this world session's first frame (0 = its eyes still warm up).
    bool worldSession=false,screenWanted=true;uint64_t lastWorldBegin=0,worldPairs=0;
};
std::unique_ptr<LiveNativeXr> liveNativeXr;
// The depth the game hands DLSS for an eye-sized picture (hooks/dlss_eval_observer.hpp, EVR_EYE_WARP=1): the output
// image names the eye's picture, the depth image is kept for the warp of the frames that leave the eye out.
// jitter: DLSS's jitter of that evaluation (its parameters +0x28 / +0x2c, the game's depth pixels; 2026-10-21).
struct EyeDepthNote {VkImage output{},depth{};uint32_t width=0,height=0;VkFormat format=VK_FORMAT_UNDEFINED;uint64_t at=0;float jitterX=0,jitterY=0;};
std::array<EyeDepthNote,8> eyeDepthNotes{};uint64_t eyeDepthNoteCount=0;std::mutex eyeDepthNoteLock;
extern "C" __declspec(dllexport) int evrNoteEyeDepthJitter(VkDevice device,VkImage output,VkImage depth,uint32_t width,uint32_t height,VkFormat format,float jitterX,float jitterY) noexcept {
    try{
        if(!output||!depth||!width||!height)return 2;
        if(!std::isfinite(jitterX)||!std::isfinite(jitterY)||std::abs(jitterX)>1||std::abs(jitterY)>1)jitterX=jitterY=0;
        {std::lock_guard<std::mutex> lock(mutex);uint64_t generation=0;auto d=trackedImages.find(key(device));
            if(d!=trackedImages.end()){auto found=d->second.find(depth);if(found!=d->second.end())generation=found->second;}
            registerNativeImageLocked(device,depth,generation,8,true);}
        std::lock_guard<std::mutex> lock(eyeDepthNoteLock);
        auto& n=eyeDepthNotes[eyeDepthNoteCount%eyeDepthNotes.size()];n={output,depth,width,height,format,++eyeDepthNoteCount,jitterX,jitterY};
        return 0;
    }catch(...){return 1;}
}
extern "C" __declspec(dllexport) int evrNoteEyeDepth(VkDevice device,VkImage output,VkImage depth,uint32_t width,uint32_t height,VkFormat format) noexcept {
    return evrNoteEyeDepthJitter(device,output,depth,width,height,format,0,0);
}
// EVR_EYE_WARP_UNJITTER (2026-10-21; the user's headset run of the warp: "everything shook"): the kept depth is read
// shifted by the jitter - 1 (+x,+y), 2 (-x,-y), 3 (+x,-y), 4 (-x,+y); 0 / unset as drawn. Which sign is right is
// measured with EVR_EYE_WARP_CHECK (the eye made from the other against the eye drawn). EVR_EYE_WARP_UNJITTER=cycle: 0..4
// in turn, 200 frames each (one Mock run compares them all; the check's record names the mode).
inline int eyeWarpUnjitter(uint64_t frame){static const int mode=[]{const char* v=std::getenv("EVR_EYE_WARP_UNJITTER");if(v&&!std::strcmp(v,"cycle"))return -1;return v&&v[0]>='1'&&v[0]<='4'&&!v[1]?v[0]-'0':0;}();
    return mode<0?int((frame/200)%5):mode;}
inline int eyeWarpUnjitterUsed=0;
// The note of this frame for the eye: the eye picture given to the layer is not the DLSS output (the game's later
// passes write another image; Mock game-20261015-043919), so the notes are taken in order - the eyes are drawn left
// then right, each with one eye-sized evaluation, so with both eyes drawn the left eye's note is the second newest
// and the right eye's the newest; with one eye drawn, the newest. A note older than the one the eye last used is
// refused (no evaluation this frame).
inline bool eyeDepthByOrder(uint32_t eyes,uint32_t eye,uint64_t usedBefore,EyeDepthNote& out){
    std::lock_guard<std::mutex> lock(eyeDepthNoteLock);
    EyeDepthNote newest,second;
    for(const auto& n:eyeDepthNotes){if(!n.at)continue;if(n.at>newest.at){second=newest;newest=n;}else if(n.at>second.at)second=n;}
    const bool both=(eyes&3)==3;const EyeDepthNote& pick=both&&eye==0?second:newest;
    if(!pick.at||pick.at<=usedBefore)return false;
    out=pick;return true;
}
namespace screenXr {bool requested();void release() noexcept;bool insteadOfEyes(LiveNativeXr&) noexcept;}
// VR status tint (2026-10-06): the probe sets the current rim colour/coverage once per eye pair; 0 coverage = none.
std::mutex edgeTintLock;EdgeTintParams edgeTintCurrent;std::atomic<bool> edgeTintFailed{false};std::atomic<uint64_t> edgeTintFrames{0};
// HUD auto-hide: the probe tells once per eye pair what kind of screen the game shows (0 off, 1 the HUD of ordinary
// play, 2 anything that has to stay visible: menus, the quick wheel, building mode). Without a call for a second the
// HUD is shown.
std::atomic<int> uiHudMode{0};std::atomic<uint64_t> uiHudAt{0};
extern "C" __declspec(dllexport) int evrSetUiHud(int mode) noexcept {
    if(mode<0||mode>2)return 2;
    uiHudMode=mode;uiHudAt=GetTickCount64();return 0;
}
// The alternate eyes' pacing (include/evr/afw_pacing.hpp), from the game's VR settings (hooks/bug_report.hpp): 0 every
// refresh the frame can make, 1 a steady two refreshes a frame, 2 the mod's own frame for every refresh - only in a session
// started with it (afwFillPrepared: the queue of its own, made with the device). 0 taken, 1 refused (2 not prepared), 2 bad.
std::atomic<bool> afwFillPrepared{false};
extern "C" __declspec(dllexport) int evrSetAfwPacing(int pacing) noexcept {
    if(pacing<0||pacing>2)return 2;
    if(pacing==2&&!afwFillPrepared.load())return 1;
    afw::pacing=pacing;note("afw_pacing_set","\"pacing\":"+std::to_string(pacing));return 0;
}
extern "C" __declspec(dllexport) int evrAfwPacing() noexcept {return afw::pacing.load()|(afwFillPrepared.load()?4:0);}
// The menu pointer (hooks/menu_pointer.hpp): where the hand's ray meets the UI image, as parts of its width and of its
// height from the top left, and what to show (0 nothing, 1 the dot, 2 the dot of a held button). The dot is drawn for
// a quarter of a second after the last call.
std::atomic<uint64_t> uiCursorAt{0};std::atomic<uint32_t> uiCursorPlace{0};std::atomic<int> uiCursorShown{0};
extern "C" __declspec(dllexport) int evrSetUiCursor(float u,float v,int shown) noexcept {
    if(!(u>=0&&u<=1)||!(v>=0&&v<=1)||shown<0||shown>2)return 2;
    uiCursorPlace=(uint32_t(u*65535.f+.5f)<<16)|uint32_t(v*65535.f+.5f);uiCursorShown=shown;uiCursorAt=GetTickCount64();return 0;
}
// The plate of an open menu (hooks/menu_pointer.hpp, VHVR LockGuiPositionWhenMenuOpen): fixed 1 = the UI image stands
// 1.5 m ahead of `pose` (the LOCAL space; orientation x y z w, then position x y z) instead of ahead of the head, 0 =
// ahead of the head again. Without a call for two seconds it is ahead of the head.
std::mutex uiPlateLock;Pose uiPlateOrigin=identityPose();std::atomic<int> uiPlateFixed{0};std::atomic<uint64_t> uiPlateAt{0};
// fixed 2 (the third-person pad style while playing, hooks/pad_settings.hpp): fixed as 1, and the locked enemy's health bar is still
// taken out of the image to the enemy, the rest of the plate staying where it is fixed.
extern "C" __declspec(dllexport) int evrSetUiPlate(int fixed,const float* pose) noexcept {
    if(fixed<0||fixed>2||(fixed&&!pose))return 2;
    if(fixed){
        float norm=0;for(unsigned i=0;i<7;i++)if(!std::isfinite(pose[i]))return 2;
        for(unsigned i=0;i<4;i++)norm+=pose[i]*pose[i];
        if(std::abs(norm-1)>.02f||std::abs(pose[4])>1000||std::abs(pose[5])>1000||std::abs(pose[6])>1000)return 2;
        std::lock_guard<std::mutex> l(uiPlateLock);uiPlateOrigin={{pose[0],pose[1],pose[2],pose[3]},{pose[4],pose[5],pose[6]}};
    }
    uiPlateFixed=fixed;uiPlateAt=GetTickCount64();return 0;
}
// HUD auto-hide: 1 = the compass stays shown with the pins (the Shroud's timer is out; hooks/live_actionbar_pages.hpp
// fogTimer), for three seconds after the last call.
std::atomic<int> uiCompassKept{0};std::atomic<uint64_t> uiCompassAt{0};
// The mod's own picture over the UI plate (the VR bug report, hooks/bug_report.hpp): with rgba, a new picture (RGBA8,
// display values, straight alpha, w x h, rows from the top); without, the last one. u0 v0 u1 v1: its place on the UI
// image (0..1). It is shown while the caller keeps asking (within a second); u1 <= u0 takes it away at once.
std::mutex uiOverlayLock;std::vector<uint8_t> uiOverlayBytes;uint32_t uiOverlayWidth=0,uiOverlayHeight=0;uint64_t uiOverlaySerialNow=0,uiOverlayAt=0;std::array<float,4> uiOverlayRect{};
// onlyWhereEmpty: shown only while the game draws nothing under it (the UI's small copy, ui_transfer.hpp probe) - the
// start menu's empty right half, not the settings page that the same screen opens over it. evrUiOverlayShown: whether it
// was drawn in the last 300 ms (the pointer's click on it, hooks/menu_pointer.hpp).
std::atomic<int> uiOverlayOnlyEmpty{0};std::atomic<uint64_t> uiOverlayDrawnAt{0};
// The mod's own stamina ring (2026-10-26, user: "cutting the UI out goes wrong somewhere - take stamina out of the UI and draw
// it yourself"; hooks/own_stamina_ring.hpp). evrStaminaStrip: the hooks, on every frame of the play screen while the game's
// floating ring is held off (hooks/floating_stamina.hpp). evrSetStamina: the stamina now and the most, every frame. The ring
// is drawn into each eye where VHVR has its stamina (StaminaPanelPlacement CameraLocked: 1 m ahead, 0.33 m left, 0.1 m down),
// while the stamina is below the most and for a moment after it is full again (as the game's floating ring), red from running
// out until full. Before (2026-10-26 night): a copy of the HUD's stamina bar there. Since 2026-10-26 13:20 where the game's own ring
// stood on the HUD plate (afterCopy below).
std::atomic<uint64_t> staminaStripAt{0},staminaAt{0};std::atomic<int> staminaNow{0},staminaMost{0};
extern "C" __declspec(dllexport) void evrStaminaStrip() noexcept {staminaStripAt=GetTickCount64();}
extern "C" __declspec(dllexport) void evrSetStamina(int now,int most) noexcept {if(most>0&&now>=0&&now<=most){staminaNow=now;staminaMost=most;staminaAt=GetTickCount64();}}
std::atomic<bool> staminaRingFailed{false};std::atomic<uint64_t> staminaRingFrames{0};
extern "C" __declspec(dllexport) int evrUiOverlayShown() noexcept {const auto at=uiOverlayDrawnAt.load();const auto now=GetTickCount64();return at&&now>=at&&now-at<=300?1:0;}
// The pictures of the VR bug report: count consecutive pairs of both eyes (1..32), without the diagnostic budget of
// evrRequestNativeXrBurst; 1 while pictures of an earlier request are still being taken. evrSetCaptureHalf: 1 half size,
// 0 full, -1 as the environment says (EVR_CAPTURE_HALF).
diagnostic::CaptureBurst bugCaptureBurst;
// The bug report's first picture (one pair) also writes the UI image (2026-10-19: where the game draws the locked target's
// health bar on the HUD, for moving it above the enemy - native-xr/ui-frame-<n>-native.rgba8 and .json, made into a JPEG
// of the report by the settings program).
std::atomic<bool> bugUiWanted{false};
extern "C" __declspec(dllexport) int evrRequestBugCapture(uint32_t count) noexcept {if(bugCaptureBurst.pending())return 1;bugCaptureBurst.reset();const int r=bugCaptureBurst.request(count);if(!r&&count==1)bugUiWanted=true;return r;}
extern "C" __declspec(dllexport) int evrSetCaptureHalf(int half) noexcept {if(half<-1||half>1)return 2;captureHalfOverride=half;return 0;}
extern "C" __declspec(dllexport) int evrSetUiOverlay(const uint8_t* rgba,uint32_t w,uint32_t h,float u0,float v0,float u1,float v1,int onlyWhereEmpty) noexcept {
    try{
        for(const float f:{u0,v0,u1,v1})if(!std::isfinite(f)||f<0||f>1)return 2;
        std::lock_guard<std::mutex> l(uiOverlayLock);uiOverlayOnlyEmpty=onlyWhereEmpty==2?2:onlyWhereEmpty?1:0;
        if(rgba){if(!w||!h||w>4096||h>4096)return 2;uiOverlayBytes.assign(rgba,rgba+size_t(w)*h*4);uiOverlayWidth=w;uiOverlayHeight=h;++uiOverlaySerialNow;}
        uiOverlayRect={u0,v0,u1,v1};uiOverlayAt=GetTickCount64();return 0;
    }catch(...){return 1;}
}
extern "C" __declspec(dllexport) int evrSetUiCompass(int kept) noexcept {
    if(kept<0||kept>1)return 2;
    uiCompassKept=kept;uiCompassAt=GetTickCount64();return 0;
}
// The aim mark (hooks/aim_mark.hpp, include/evr/aim_mark.hpp): `point` is the aimed place in the LOCAL space (x y z);
// shown 1 = a dot is drawn into both eyes' images where that place shows in them, and the middle of the UI image - the
// game's own reticle - is left out. For a quarter of a second after the last call.
// EVR_AIM_MARK_HOLE in the game's environment: the radius of what is left out, in pixels of a 1080 rows high UI image.
std::mutex aimMarkLock;Vec3 aimMarkPoint{};std::atomic<int> aimMarkShown{0};std::atomic<uint64_t> aimMarkAt{0},aimMarkFrames{0};std::atomic<bool> aimMarkFailed{false};
extern "C" __declspec(dllexport) int evrSetAimMark(const float* point,int shown) noexcept {
    if(shown<0||shown>1||(shown&&!point))return 2;
    if(shown){
        for(unsigned i=0;i<3;i++)if(!std::isfinite(point[i])||std::abs(point[i])>100000.f)return 2;
        std::lock_guard<std::mutex> l(aimMarkLock);aimMarkPoint={point[0],point[1],point[2]};
    }
    aimMarkShown=shown;aimMarkAt=GetTickCount64();return 0;
}
// The mod's own picture beside the aim mark (include/evr/eye_panel.hpp, hooks/spell_panel.hpp - the staff's spells; 2026-10-26, user:
// "cutting the UI always goes wrong somewhere; take the spell switching out of the UI and draw what is needed at the reticle"). rgba
// (w x h RGBA8, display values, straight alpha, rows from the top): a new picture; null keeps the last one. degreesWide: its width
// as seen (its height follows the picture's); anchor: the picture's row (0..1 from the top) at the mark's height; gap: its left edge
// this many degrees right of the mark; strength 0..1 (0 hides it). Drawn into both eyes at the mark's place, while the mark is
// shown and for a quarter of a second after the last call.
std::mutex aimPanelLock;std::vector<uint8_t> aimPanelBytes;uint32_t aimPanelWidth=0,aimPanelHeight=0;uint64_t aimPanelSerialNow=0;
float aimPanelDegrees=0,aimPanelAnchor=.5f,aimPanelGap=0,aimPanelStrength=0;
std::atomic<uint64_t> aimPanelAt{0},aimPanelFrames{0};std::atomic<bool> aimPanelFailed{false};
extern "C" __declspec(dllexport) int evrSetAimPanel(const uint8_t* rgba,uint32_t w,uint32_t h,float degreesWide,float anchor,float gap,float strength) noexcept {
    try{
        if(!std::isfinite(degreesWide)||degreesWide<=0||degreesWide>90||!std::isfinite(anchor)||anchor<-1||anchor>2||!std::isfinite(gap)||std::abs(gap)>45||
           !std::isfinite(strength)||strength<0||strength>1)return 2;
        std::lock_guard<std::mutex> l(aimPanelLock);
        if(rgba){if(!w||!h||w>2048||h>2048)return 2;aimPanelBytes.assign(rgba,rgba+size_t(w)*h*4);aimPanelWidth=w;aimPanelHeight=h;++aimPanelSerialNow;}
        else if(!aimPanelWidth&&strength>0)return 3;
        aimPanelDegrees=degreesWide;aimPanelAnchor=anchor;aimPanelGap=gap;aimPanelStrength=strength;aimPanelAt=GetTickCount64();return 0;
    }catch(...){return 1;}
}
// The guide of a throw (hooks: controller_ui_fixture.hpp, include/evr/live_throw.hpp ThrowGuide): `count` points (2..16)
// of the LOCAL space, a small dot drawn into both eyes' images at each - a dotted line of about 30 cm from the left hand.
// For a quarter of a second after the last call; shown 0 takes it away.
std::mutex throwGuideLock;std::array<Vec3,16> throwGuidePoints{};std::atomic<uint32_t> throwGuideCount{0};std::atomic<uint64_t> throwGuideAt{0},throwGuideFrames{0};std::atomic<bool> throwGuideFailed{false};
extern "C" __declspec(dllexport) int evrSetThrowGuide(const float* points,uint32_t count,int shown) noexcept {
    if(shown<0||shown>1||(shown&&(!points||count<2||count>16)))return 2;
    if(shown){
        for(uint32_t i=0;i<count*3;i++)if(!std::isfinite(points[i])||std::abs(points[i])>100000.f)return 2;
        std::lock_guard<std::mutex> l(throwGuideLock);for(uint32_t i=0;i<count;i++)throwGuidePoints[i]={points[i*3],points[i*3+1],points[i*3+2]};
    }
    throwGuideCount=shown?count:0;throwGuideAt=GetTickCount64();return 0;
}
// The locked combat target's mark (hooks/target_mark.hpp): `point` is the place just above the target's head in the LOCAL
// space; shown 1 = a downward chevron is drawn into both eyes' images there, as large to the eye at any distance. For a
// quarter of a second after the last call; shown 0 takes it away.
std::mutex targetMarkLock;Vec3 targetMarkPoint{};std::atomic<int> targetMarkShown{0};std::atomic<uint64_t> targetMarkAt{0},targetMarkFrames{0};std::atomic<bool> targetMarkFailed{false};
extern "C" __declspec(dllexport) int evrSetTargetMark(const float* point,int shown) noexcept {
    if(shown<0||shown>1||(shown&&!point))return 2;
    if(shown){
        for(unsigned i=0;i<3;i++)if(!std::isfinite(point[i])||std::abs(point[i])>100000.f)return 2;
        std::lock_guard<std::mutex> l(targetMarkLock);targetMarkPoint={point[0],point[1],point[2]};
    }
    targetMarkShown=shown;targetMarkAt=GetTickCount64();return 0;
}
// The locked enemy's health bar and lock-on arrow (the game's spatial HUD, FbUiHUDSpatial) as the game drew them into the HUD
// image: values = the anchor's place in the LOCAL space (3), where the game's own third-person camera sees it as slopes
// (x / z, y / z of its camera space, +z ahead), the tangent of that camera's half field of view and 1 when that field is
// the vertical one. The HUD image's part about it is taken out of the plate and set at the anchor (aim_mark.hpp partAt).
// Mock game-20261019-142512: with the head alone turning, the spatial HUD stayed put in the HUD image - the game's own
// camera projects it, not the eyes'. For a quarter of a second after the last call; shown 0 takes it away.
std::mutex targetHudLock;std::array<float,7> targetHudValues{};std::atomic<int> targetHudShown{0};std::atomic<uint64_t> targetHudAt{0},targetHudFrames{0};
extern "C" __declspec(dllexport) int evrSetTargetHud(const float* values,int shown) noexcept {
    if(shown<0||shown>1||(shown&&!values))return 2;
    if(shown){
        for(unsigned i=0;i<7;i++)if(!std::isfinite(values[i])||std::abs(values[i])>100000.f)return 2;
        if(!(values[5]>.05f&&values[5]<20.f))return 2;
        std::lock_guard<std::mutex> l(targetHudLock);std::copy(values,values+7,targetHudValues.begin());
    }
    targetHudShown=shown;targetHudAt=GetTickCount64();return 0;
}
inline bool targetHudNow(std::array<float,7>& values) {
    if(!targetHudShown.load())return false;
    const auto now=GetTickCount64();const auto at=targetHudAt.load();if(now<at||now-at>250)return false;
    std::lock_guard<std::mutex> l(targetHudLock);values=targetHudValues;return true;
}
inline bool targetMarkNow(Vec3& point) {
    if(!targetMarkShown.load())return false;
    const auto now=GetTickCount64();const auto at=targetMarkAt.load();if(now<at||now-at>250)return false;
    std::lock_guard<std::mutex> l(targetMarkLock);point=targetMarkPoint;return true;
}
// The bug report's last seconds (eye_history.hpp): asked from the hooks (hooks/bug_report.hpp) when its button is pressed
// and when its window ends; written before the next pair. evrEyeHistorySaved: the saves done so far.
std::atomic<uint64_t> eyeHistoryAsked{0},eyeHistoryDone{0};
extern "C" __declspec(dllexport) int evrSaveEyeHistory() noexcept {++eyeHistoryAsked;return 0;}
extern "C" __declspec(dllexport) uint64_t evrEyeHistorySaved() noexcept {return eyeHistoryDone.load();}
inline uint32_t throwGuideNow(std::array<Vec3,16>& points) {
    const auto count=throwGuideCount.load();if(!count)return 0;
    const auto now=GetTickCount64();const auto at=throwGuideAt.load();if(now<at||now-at>250)return 0;
    std::lock_guard<std::mutex> l(throwGuideLock);points=throwGuidePoints;return count;
}
inline bool aimMarkNow(Vec3& point) {
    if(!aimMarkShown.load())return false;
    const auto now=GetTickCount64();const auto at=aimMarkAt.load();if(now<at||now-at>250)return false;
    std::lock_guard<std::mutex> l(aimMarkLock);point=aimMarkPoint;return true;
}
// The reticle's part of the HUD goes to the aimed point with the mark (aim_mark.hpp, the cluster). EVR_AIM_CLUSTER=0
// in the game's environment keeps the HUD whole and only takes the game's dot out of its middle (the hole below).
inline bool aimCluster() {
    static const bool on=[]{const auto v=std::getenv("EVR_AIM_CLUSTER");return !(v&&v[0]=='0');}();
    return on;
}
inline float aimMarkHole() {
    static const float radius=[]{const auto v=std::getenv("EVR_AIM_MARK_HOLE");const float r=v?float(std::atof(v)):14.f;return std::isfinite(r)&&r>=0&&r<=200?r:14.f;}();
    return radius;
}
extern "C" __declspec(dllexport) int evrSetEdgeTint(float r,float g,float b,float a,float band) noexcept {
    if(!(a>=0&&a<=1)||!(band>0&&band<=0.5f)||!(r>=0&&r<=1)||!(g>=0&&g<=1)||!(b>=0&&b<=1))return 2;
    std::lock_guard<std::mutex> l(edgeTintLock);edgeTintCurrent={r,g,b,a,band};return 0;
}
// Bounded diagnostic request from the frontend thread. No renderer access there.
std::atomic<uint32_t> nativeFixtureCapture{0};
diagnostic::CaptureBurst nativeCaptureBurst;
extern "C" __declspec(dllexport) int evrRequestNativeXrBurst(uint32_t count) noexcept {return nativeCaptureBurst.request(count);}
extern "C" __declspec(dllexport) int evrRequestNativeXrCapture(uint32_t tag) noexcept {
    if(tag<1||tag>5)return 2;uint32_t empty=0;
    return nativeFixtureCapture.compare_exchange_strong(empty,tag)?0:1;
}
uint64_t nextNativeXrGeneration=0; // same native render thread as session creation
extern "C" __declspec(dllexport) int evrGetPreparedGameQueueFamily(VkInstance instance,VkDevice device,uint32_t* family) noexcept
{
    if(!family)return 2;
    try {
        std::lock_guard<std::mutex> lock(mutex);
        auto child=devices.find(key(device));auto parent=child==devices.end()?instances.end():instances.find(key(child->second.instance));
        if(parent==instances.end()||child==devices.end()||parent->second.fixture||!parent->second.preflight||
           !child->second.fixtureBinding||child->second.instance!=parent->second.instance)return 2;
        *family=child->second.fixtureBinding->queueFamily;return 0;
    }catch(...){return 1;}
}
// keepForScreen (the hooks' world session ends, EVR_SCREEN_XR=1): the world's eyes stop and the session stays for the
// plate - the game goes on at its menu or in the next world's loading, and the headset keeps showing it.
// deviceEnds (the game destroys its Vulkan device - it is closing): the session that was kept for the plate goes with
// it, whichever thread that is; nothing draws any more. Game start 20261014-115547: without this the kept session
// outlived the device and the game ended with an access violation.
int closeLiveNativeXr(bool keepForScreen=false,bool deviceEnds=false) noexcept
{
    if(!liveNativeXr)return 0;
    try {
        auto &state=*liveNativeXr;
        if(!deviceEnds&&state.thread!=GetCurrentThreadId())throw std::runtime_error("native XR teardown thread changed");
        QueueCallScope context("openxr_native_teardown");
        if(state.xr)state.xr->stopRefreshing();   // its warps use what goes below
        if(keepForScreen&&state.xr&&state.xr->vk.device&&state.xr->active()){
            desktopMirrorSource(state.device,VK_NULL_HANDLE); // the desktop shows the game's own picture again
            vkCheck(state.xr->vk.vkDeviceWaitIdle(state.xr->vk.device),"native XR world end idle");
            state.xr->abortFrame("world_session_ended");
            state.worldSession=false;state.screenWanted=true;state.worldPairs=0;
            state.log->event("native_xr_kept_for_screen","\"submitted_pairs\":"+std::to_string(state.submittedPairs)+",\"generation\":"+std::to_string(state.generation));
            return 0;
        }
        screenXr::release();
        if(state.xr&&state.xr->vk.device){
            // All native queue gates are held by the caller. Never call engine retirement.
            const bool transferPending=state.transfer&&state.transfer->gpuPending(),uiPending=state.uiTransfer&&state.uiTransfer->gpuPending();
            desktopMirrorSource(state.device,VK_NULL_HANDLE); // no new desktop blit of the transfer-owned mirror image
            vkCheck(state.xr->vk.vkDeviceWaitIdle(state.xr->vk.device),"native XR teardown idle");
            if(state.asyncCompletion)state.log->event("native_xr_async_drained","\"transfer_pending\":"+std::string(transferPending?"true":"false")+",\"ui_pending\":"+(uiPending?"true":"false")+",\"device_idle_complete\":true,\"submitted_pairs\":"+std::to_string(state.submittedPairs));
            if(state.uiTransfer){state.uiTransfer.reset();state.log->event("native_ui_transfer_destroyed");}
            if(state.hands){const bool closed=state.hands->close();state.hands.reset();state.log->event("native_motion_closed","\"success\":"+std::string(closed?"true":"false"));}
            state.transfer.reset();
            {auto& vk=state.xr->vk;auto& o=state.own;   // the device is idle (above)
             for(auto& sl:state.slots){if(sl.image)vk.vkDestroyImage(vk.device,sl.image,nullptr);if(sl.memory)vk.vkFreeMemory(vk.device,sl.memory,nullptr);sl={};}
             if(o.destroySemaphore){if(o.fromGame)o.destroySemaphore(vk.device,o.fromGame,nullptr);if(o.toGame)o.destroySemaphore(vk.device,o.toGame,nullptr);}
             if(o.pool)vk.vkDestroyCommandPool(vk.device,o.pool,nullptr);o={};}
            if(state.edgeTint){state.edgeTint.reset();state.log->event("edge_tint_destroyed","\"frames\":"+std::to_string(edgeTintFrames.load()));}
            if(state.eyeMark){state.eyeMark.reset();state.log->event("aim_mark_destroyed","\"frames\":"+std::to_string(aimMarkFrames.load()));}
            if(state.eyePanel){state.eyePanel.reset();state.aimPanelSerial=0;state.log->event("aim_panel_destroyed","\"frames\":"+std::to_string(aimPanelFrames.load()));}
            state.xr->abortFrame("native_diagnostic_stopped");state.xr->shutdown();
        }
        state.xr.reset();const auto pendingBurst=nativeCaptureBurst.reset();
        if(pendingBurst)state.log->event("native_burst_capture_cancelled","\"remaining\":"+std::to_string(pendingBurst));
        state.log->event("native_xr_closed");liveNativeXr.reset();return 0;
    }catch(const std::exception &e){note("native_xr_close_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
extern "C" __declspec(dllexport) int evrCloseNativeXr() noexcept {return closeLiveNativeXr(screenXr::requested());}
// Read-only query after evrFinishNativeXr; no ABI change to the existing finish function.
extern "C" __declspec(dllexport) int evrNativeFrameCaptured(uint64_t frame) noexcept {
    return liveNativeXr&&liveNativeXr->thread==GetCurrentThreadId()&&frame&&liveNativeXr->capturedFrame==frame?1:0;
}
// Read-only (2026-10-18): how often the runtime has announced a new LOCAL space (its view reset: SteamVR's or Meta's
// system button held) and the time of the last one; the hooks take the view's origin again from that time on.
extern "C" __declspec(dllexport) int evrNativeLocalSpaceChange(uint64_t* count,int64_t* changeTime) noexcept {
    if(!count||!changeTime||!liveNativeXr||!liveNativeXr->xr||liveNativeXr->thread!=GetCurrentThreadId())return 0;
    *count=liveNativeXr->xr->localSpaceChanges;*changeTime=liveNativeXr->xr->localSpaceChangeTime;return 1;
}
extern "C" __declspec(dllexport) int evrSkipNativeXr(uint64_t frame) noexcept {
    try {
        if(!liveNativeXr)return 2;auto& state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId()||state.xr->pendingFrame().frame!=frame||presents.load()<=state.beginPresent)
            throw std::runtime_error("native deferred frame identity/present boundary mismatch");
        QueueCallScope context("openxr_native_deferred_frame");
        state.screenWanted=true;
        if(!screenXr::insteadOfEyes(state))state.xr->skipFrame("world_view_resources_pending");
        return 0;
    }catch(const std::exception& e){note("native_xr_skip_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
// The same for a frame that was begun early in the game's frame (hooks: world_early_xr_frame) when the game then
// renders no world for it: there is no present between the begin and this end. Seen when the game rebuilds its
// renderer after a change of the graphics settings - SSAO to "disabled" ended the session with "native deferred
// frame identity/present boundary mismatch" (headset run game-20261013-005905, Mock run game-20261013-031050). The
// frame is ended empty, as above; the thread and the frame's identity are still required.
extern "C" __declspec(dllexport) int evrSkipNativeXrEarly(uint64_t frame) noexcept {
    try {
        if(!liveNativeXr)return 2;auto& state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId()||state.xr->pendingFrame().frame!=frame||presents.load()<state.beginPresent)
            throw std::runtime_error("native early deferred frame identity mismatch");
        QueueCallScope context("openxr_native_deferred_frame");
        state.screenWanted=true;
        if(!screenXr::insteadOfEyes(state))state.xr->skipFrame("world_original_view_not_rendered");
        state.log->event("native_frame_skipped_early","\"frame\":"+std::to_string(frame)+",\"presents_since_begin\":"+std::to_string(presents.load()-state.beginPresent)+",\"layers\":0");
        return 0;
    }catch(const std::exception& e){note("native_xr_skip_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
// The mod's own queue: the game's queue's work so far before the binding queue's next (ownQueueIn), and the binding queue's
// so far before the game's next (ownQueueOut) - around everything that reads what the game drew or that the game reads
// back (the eyes' copy, the UI's, the plate). The caller holds the client's queue lock.
inline void ownQueueIn(LiveNativeXr& st){
    if(!st.ownQueue)return;
    auto& vk=st.xr->vk;auto& o=st.own;
    VkSubmitInfo signal{VK_STRUCTURE_TYPE_SUBMIT_INFO};signal.signalSemaphoreCount=1;signal.pSignalSemaphores=&o.fromGame;
    vkCheck(vk.vkQueueSubmit(o.game,1,&signal,VK_NULL_HANDLE),"own queue: the game's queue signals");
    const VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo wait{VK_STRUCTURE_TYPE_SUBMIT_INFO};wait.waitSemaphoreCount=1;wait.pWaitSemaphores=&o.fromGame;wait.pWaitDstStageMask=&stage;wait.commandBufferCount=1;wait.pCommandBuffers=&o.xrBarrier;
    vkCheck(vk.vkQueueSubmit(vk.queue,1,&wait,VK_NULL_HANDLE),"own queue: waits for the game's");
}
inline void ownQueueOut(LiveNativeXr& st){
    if(!st.ownQueue)return;
    auto& vk=st.xr->vk;auto& o=st.own;
    VkSubmitInfo signal{VK_STRUCTURE_TYPE_SUBMIT_INFO};signal.signalSemaphoreCount=1;signal.pSignalSemaphores=&o.toGame;
    vkCheck(vk.vkQueueSubmit(vk.queue,1,&signal,VK_NULL_HANDLE),"own queue: signals the game's");
    const VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo wait{VK_STRUCTURE_TYPE_SUBMIT_INFO};wait.waitSemaphoreCount=1;wait.pWaitSemaphores=&o.toGame;wait.pWaitDstStageMask=&stage;wait.commandBufferCount=1;wait.pCommandBuffers=&o.gameBarrier;
    vkCheck(vk.vkQueueSubmit(o.game,1,&wait,VK_NULL_HANDLE),"own queue: the game's queue waits");
}
// In at its start, out at its end (also when it throws; a failure there is only logged).
struct OwnQueueScope {
    LiveNativeXr& st;bool in=false;
    explicit OwnQueueScope(LiveNativeXr& s):st(s){ownQueueIn(st);in=st.ownQueue;}
    ~OwnQueueScope(){if(in)try{ownQueueOut(st);}catch(const std::exception& e){try{st.log->event("own_queue_out_failed","\"reason\":"+jsonString(e.what()));}catch(...){}}}
};
// The slot of an eye (Slot above), made at the eyes' size on first use.
inline TransferImage slotFor(LiveNativeXr& st,uint32_t eye,uint32_t width,uint32_t height){
    auto& vk=st.xr->vk;auto& sl=st.slots[eye];
    if(sl.image&&(sl.width!=width||sl.height!=height)){vkCheck(vk.vkDeviceWaitIdle(vk.device),"slot resize");vk.vkDestroyImage(vk.device,sl.image,nullptr);vk.vkFreeMemory(vk.device,sl.memory,nullptr);sl={};}
    const VkImageUsageFlags usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    if(!sl.image){
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=st.xr->format;i.extent={width,height,1};i.mipLevels=i.arrayLayers=1;
        i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=usage;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        vkCheck(vk.vkCreateImage(vk.device,&i,nullptr,&sl.image),"eye slot image");VkMemoryRequirements req{};vk.vkGetImageMemoryRequirements(vk.device,sl.image,&req);
        try{sl.memory=vk.allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(vk.vkBindImageMemory(vk.device,sl.image,sl.memory,0),"eye slot memory");}
        catch(...){vk.vkDestroyImage(vk.device,sl.image,nullptr);sl={};throw;}
        sl.width=width;sl.height=height;
        st.log->event("afw_fill_slot","\"eye\":"+std::to_string(eye)+",\"size\":["+std::to_string(width)+','+std::to_string(height)+"],\"format\":"+std::to_string(st.xr->format));
    }
    return {sl.image,width,height,st.xr->format,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,usage,vk.family};
}
// A part s of a rigid motion (its rotation's angle and its move times s): the motion carried on for a part of a frame.
inline XrPosef motionPart(const XrPosef& m,float s){
    auto q=normalized(m.orientation);if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
    const float half=std::acos(std::clamp(q.w,-1.f,1.f)),sinHalf=std::sin(half);
    XrPosef out{{0,0,0,1},{m.position.x*s,m.position.y*s,m.position.z*s}};
    if(sinHalf>1e-6f){const float k=std::sin(half*s)/sinHalf;out.orientation=normalized(Quat{q.x*k,q.y*k,q.z*k,std::cos(half*s)});}
    return out;
}
inline const LiveNativeXr::Motion* motionOf(const LiveNativeXr& st,uint64_t frame){
    const auto& m=st.motions[frame%st.motions.size()];return m.frame==frame?&m:nullptr;
}
// The world's path over the last game frames, smoothed (2026-10-24; the mod's own frames for every refresh while walking, Mock
// game-20261024-040231 at the real pace: the world moved 0.2 px in one refresh and 2.7 px in the next, its speed per millisecond
// spread by 41 % - the game's step of a frame and the frame's display time do not go together (a frame's speed read 2.8 .. 6.3 m/s
// walking evenly), and the last frame's step carried on made the world stand and jump). The places of the frames since r (up to 7
// motions) at their display times: a straight line in time through the places and the turn about the up axis (least squares, the
// later frames weighted more); ahead = where the line has the world at `time` (kept within -1 .. 2.5 frames of the last), from the
// last frame's place. False when the frames do not allow it (fewer than 2 motions, a turn not about the up axis): the caller keeps
// the last frame's motion carried on. speed / turn: the line's speed (m/s) and turn (degrees/s), for the log.
inline bool smoothAhead(const LiveNativeXr& st,uint64_t latest,XrTime time,XrPosef& ahead,float& speed,float& turn,float& shortShare){
    // EVR_AFW_SMOOTH_FRAMES=<2..15> motions in the line (15; with the last 5 blended in at a change of speed, below), EVR_AFW_SMOOTH_AHEAD=<frames> the most carried past the last frame (2.5).
    // Mock (shots at the real pace, the spread of the world's speed per millisecond): walking, the last frame's step 0.39 - the line of 5
    // within 1.5 frames 0.15 - of 7 within 2.5 frames 0.07; turning by the stick 0.30 - of 7 within 2.5 0.155.
    static const int frames=[]{const auto f=std::getenv("EVR_AFW_SMOOTH_FRAMES");const int v=f?std::atoi(f):15;return v>=2&&v<=15?v:15;}();
    static const double most=[]{const auto f=std::getenv("EVR_AFW_SMOOTH_AHEAD");const double v=f?std::atof(f):2.5;return v>=0.5&&v<=4.0?v:2.5;}();
    // EVR_AFW_SMOOTH_FLAT=1: the frames weighted alike (else the later more)
    static const bool flat=[]{const auto f=std::getenv("EVR_AFW_SMOOTH_FLAT");return f&&f[0]=='1';}();
    const auto weight=[](size_t i){return flat?1.0:double(i+1);};
    uint64_t r=latest;
    while(r+uint64_t(frames)>latest&&r>0){const auto* m=motionOf(st,r);if(!m||!m->valid||m->dt<=0)break;--r;}
    if(latest<r+2)return false;
    const auto* next=motionOf(st,r+1);const auto* own=motionOf(st,r);
    const XrTime base=own&&own->time>0?own->time:next->time-next->dt;
    std::array<double,16> t{},yaw{},px{},py{},pz{};size_t n=0;
    XrPosef M{{0,0,0,1},{0,0,0}};
    auto put=[&](double at,const XrPosef& p){
        const auto q=normalized(p.orientation);
        if(std::abs(q.x)>0.02f||std::abs(q.z)>0.02f)return false;
        double a=2.0*std::atan2(double(q.y),double(q.w));
        if(n){while(a-yaw[n-1]>3.14159265358979)a-=6.28318530717959;while(a-yaw[n-1]<-3.14159265358979)a+=6.28318530717959;}
        t[n]=at;yaw[n]=a;px[n]=p.position.x;py[n]=p.position.y;pz[n]=p.position.z;++n;return true;
    };
    if(!put(0.0,M))return false;
    for(uint64_t k=r+1;k<=latest;++k){const auto* m=motionOf(st,k);M=compose(m->perFrame,M);if(!put(double(m->time-base)*1e-9,M))return false;}
    if(n<3)return false;
    const double tLast=t[n-1],step=tLast/double(n-1);if(!(step>1e-4))return false;
    const double at=std::clamp(double(time-base)*1e-9,tLast-step,tLast+most*step);
    // a weighted line through the samples from `from` on: its value at `at` and its slope, per channel (yaw, x, y, z)
    struct Fit {double v[4]{},s[4]{};bool ok=false;};
    const std::array<const std::array<double,16>*,4> ys{&yaw,&px,&py,&pz};
    const auto fitFrom=[&](size_t from){Fit out;double sw=0,sx=0;
        for(size_t i=from;i<n;++i){const double w=weight(i-from);sw+=w;sx+=w*t[i];}
        const double mx=sx/sw;double sxx=0;for(size_t i=from;i<n;++i)sxx+=weight(i-from)*(t[i]-mx)*(t[i]-mx);
        if(!(sxx>0))return out;
        for(size_t c=0;c<4;++c){const auto& y=*ys[c];double sy=0;for(size_t i=from;i<n;++i)sy+=weight(i-from)*y[i];const double my=sy/sw;double sxy=0;
            for(size_t i=from;i<n;++i)sxy+=weight(i-from)*(t[i]-mx)*(y[i]-my);out.s[c]=sxy/sxx;out.v[c]=my+out.s[c]*(at-mx);}
        out.ok=true;return out;};
    const Fit longFit=fitFrom(0);if(!longFit.ok)return false;
    // A change of speed (a start, a stop, a turn begun or ended). Facts (Mock 2026-10-24): the game's step of a frame is about the same
    // from frame to frame while its time is not (a stick turn 1.8 .. 2.2 degrees a frame over 10.7 .. 21.5 ms) - the line of 15 motions
    // made the turn smooth (spread 0.176 -> 0.077) but at a stop the world ran on and came back by up to 2.5 px (game-20261024-062702);
    // a released stick halves the turn's step frame by frame (2.24, 1.15, 0.56, 0.28 degrees, -063503). So the last frame's step against
    // the mean step of the frames before it tells the change at once: within 15 % the line, from 15 % on more and more (all from 45 %)
    // the last step carried on from the last frame (as before the line). The walk's step is not that even (0.047 .. 0.099 m a frame,
    // -040231): its change is told by the line of the last 5 motions against the long line's speed - none within 15 %, all from 40 %
    // (Mock -063126: the walk's spread 0.047, no step back at the stop) - and that line taken in.
    Fit f=longFit;shortShare=0;
    if(n>=4){
        const auto turnStep=[&](size_t i){return std::abs(yaw[i]-yaw[i-1]);};
        double turnMean=0;for(size_t i=1;i+1<n;++i)turnMean+=turnStep(i);turnMean/=double(n-2);
        const double turnLast=turnStep(n-1),least=0.0017;   // 0.1 degree a frame
        const double wTurn=std::max(turnLast,turnMean)<least?0.0:std::clamp((std::abs(turnLast/std::max(turnMean,least)-1.0)-0.15)/0.30,0.0,1.0);
        const double dtLast=t[n-1]-t[n-2],partLast=dtLast>1e-5?(at-t[n-1])/dtLast:0.0;
        f.v[0]+=(yaw[n-1]+(yaw[n-1]-yaw[n-2])*partLast-longFit.v[0])*wTurn;
        double wMove=0;
        if(n>7){const Fit shortFit=fitFrom(n-6);
            if(shortFit.ok){
                const auto norm=[](double a,double b,double c){return std::sqrt(a*a+b*b+c*c);};
                const double vl=norm(longFit.s[1],longFit.s[2],longFit.s[3]),vs=norm(shortFit.s[1],shortFit.s[2],shortFit.s[3]);
                const double dv=norm(longFit.s[1]-shortFit.s[1],longFit.s[2]-shortFit.s[2],longFit.s[3]-shortFit.s[3]);
                wMove=std::clamp((dv/std::max(std::max(vl,vs),0.3)-0.15)/0.25,0.0,1.0);   // 0.3 m/s
                for(size_t c=1;c<4;++c){f.v[c]+=(shortFit.v[c]-longFit.v[c])*wMove;f.s[c]+=(shortFit.s[c]-longFit.s[c])*wMove;}}}
        shortShare=float(std::max(wTurn,wMove));}
    const double a=f.v[0],x=f.v[1],y=f.v[2],z=f.v[3];
    const XrPosef F{{0,float(std::sin(a/2)),0,float(std::cos(a/2))},{float(x),float(y),float(z)}};
    ahead=compose(F,inverse(M));speed=float(std::sqrt(f.s[1]*f.s[1]+f.s[2]*f.s[2]+f.s[3]*f.s[3]));turn=float(f.s[0]*57.2957795);
    return finite(ahead.position)&&length(ahead.position)<5.f;
}
// The refresh thread's pictures (OpenXrClient refreshLoop, the queue lock held): each eye from the kept pictures - the
// other eye's when it is the newer (with this eye's own in its holes), else this eye's own - each from where the world now
// has it: the world's motion of the frames since it was drawn, and of the last frame carried on to this refresh's time
// (EVR_AFW_FILL_CARRY=0: from the poses they were drawn at, the head's pose alone).
bool drawRefresh(LiveNativeXr& st,const std::array<XrView,2>& views,XrTime time,const std::array<uint32_t,2>& indices,uint64_t refresh){
    if(!st.warp||st.warpFailed||!st.warp->canWarp(0)||!st.warp->canWarp(1))return false;
    auto& w=*st.warp;auto& xr=*st.xr;
    static const bool carry=[]{const auto f=std::getenv("EVR_AFW_FILL_CARRY");return !(f&&!std::strcmp(f,"0"));}();
    // EVR_AFW_FILL_OBJECTS=0: the things' own motion left out (the hands and the weapon then go with the world's motion)
    static const bool objects=[]{const auto f=std::getenv("EVR_AFW_FILL_OBJECTS");return !(f&&!std::strcmp(f,"0"));}();
    const uint64_t latest=std::max(w.keptFrame(0),w.keptFrame(1));
    const auto* last=motionOf(st,latest);
    float part=0;if(last&&last->valid&&last->dt>0)part=std::clamp(float(double(time-last->time)/double(last->dt)),-1.f,1.5f);
    XrPosef ahead=last&&last->valid?motionPart(last->perFrame,part):XrPosef{{0,0,0,1},{0,0,0}};
    // the world on the smoothed path of the last frames (smoothAhead); EVR_AFW_FILL_SMOOTH=0: the last frame's motion carried on
    static const bool smooth=[]{const auto f=std::getenv("EVR_AFW_FILL_SMOOTH");return !(f&&!std::strcmp(f,"0"));}();
    float smoothSpeed=-1,smoothTurn=0,smoothShort=0;
    if(smooth&&carry&&last&&last->valid){XrPosef a{};float sp=0,tu=0;if(smoothAhead(st,latest,time,a,sp,tu,smoothShort)){ahead=a;smoothSpeed=sp;smoothTurn=tu;}}
    // where a kept picture is now, and by how many game frames its world was carried on (the frames since it, and the part)
    auto carried=[&](uint32_t eye,float& frames){
        XrPosef p=w.keptView(eye).pose;frames=last&&last->valid?part:0.f;
        for(uint64_t f=w.keptFrame(eye)+1;f<=latest&&f<w.keptFrame(eye)+8;++f)if(const auto* m=motionOf(st,f);m&&m->valid){p=compose(m->perFrame,p);frames+=1;}
        return compose(ahead,p);
    };
    // A kept picture's things are carried on by their own motion (one game frame's) as many frames as its world: a thing that
    // moves with the player (the hands, the weapon) then keeps its place - two different spans moved the staff in the hand by
    // up to 50 pixels between refreshes (Mock 2026-10-23).
    auto framesSince=[&](uint32_t eye){float frames=0;carried(eye,frames);return objects?std::max(frames,0.f):0.f;};
    std::array<EyeWarp::PairEye,2> eyes{};
    for(uint32_t eye=0;eye<2;++eye){
        auto& e=eyes[eye];const auto& target=xr.targets[eye];
        e.runtime={target.images[indices[eye]].image,target.width,target.height,xr.format,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,xr.vk.family};
        e.now=views[eye];e.fromOther=w.keptFrame(1-eye)>w.keptFrame(eye);
        e.moved=carry;if(carry){float unused=0;e.keptAt=carried(eye,unused);e.otherAt=carried(1-eye,unused);}
        e.keptMotion=framesSince(eye);e.otherMotion=framesSince(1-eye);
    }
    // EVR_AFW_FILL_CAPTURE=N: N refreshes' pictures of one eye (EVR_AFW_FILL_CAPTURE_EYE, 0) as native-xr/refresh-<n>-eye-<e>.ppm,
    // from the refresh EVR_AFW_FILL_CAPTURE_FROM on.
    static const unsigned captureWanted=[]{const auto f=std::getenv("EVR_AFW_FILL_CAPTURE");return f?unsigned(std::strtoul(f,nullptr,10)):0u;}();
    static const uint64_t captureFrom=[]{const auto f=std::getenv("EVR_AFW_FILL_CAPTURE_FROM");return f?uint64_t(std::strtoull(f,nullptr,10)):0ull;}();
    static const int captureEye=[]{const auto f=std::getenv("EVR_AFW_FILL_CAPTURE_EYE");return f&&f[0]=='1'?1:0;}();
    const bool capture=st.refreshCaptures<captureWanted&&refresh>=captureFrom;
    // EVR_AFW_FILL_SHOTS=N: N refreshes in a row from EVR_AFW_FILL_CAPTURE_FROM, a quarter of the eye's size each way (EVR_AFW_FILL_CAPTURE_EYE),
    // copied on the GPU without waiting and written afterwards as native-xr/shot-<refresh>.ppm (the refreshes are not held up)
    static const unsigned shotsWanted=[]{const auto f=std::getenv("EVR_AFW_FILL_SHOTS");return f?unsigned(std::min(std::strtoul(f,nullptr,10),400ul)):0u;}();
    w.shotEye=-1;
    if(shotsWanted&&refresh>=captureFrom&&!st.shotsDone){w.prepareShots(shotsWanted);if(!w.shotsFull()){w.shotEye=captureEye;w.shotTag=refresh;}}
    const bool shot=w.shotEye>=0;
    const auto bytes=w.drawPair(eyes,capture?captureEye:-1);
    w.shotEye=-1;
    if(shotsWanted&&!st.shotsDone&&w.shotsFull()){
        st.shotsDone=true;auto shots=w.collectShots();const uint32_t sw=w.shotWidth(),sh=w.shotHeight();
        std::error_code ec;std::filesystem::create_directories(std::filesystem::path(output)/"native-xr",ec);
        st.log->event("afw_fill_shots","\"refresh\":"+std::to_string(refresh)+",\"shots\":"+std::to_string(shots.size())+",\"size\":["+std::to_string(sw)+','+std::to_string(sh)+"]");
        std::thread([shots=std::move(shots),sw,sh,dir=output+"/native-xr/"]{try{for(const auto& [tag,px]:shots){std::ofstream out(dir+"shot-"+std::to_string(tag)+".ppm",std::ios::binary);
            out<<"P6\n"<<sw<<' '<<sh<<"\n255\n";std::vector<char> row(size_t(sw)*3);
            for(uint32_t y=0;y<sh;++y){const auto* p=px.data()+size_t(y)*sw*4;for(uint32_t x=0;x<sw;++x){row[x*3]=char(p[x*4]);row[x*3+1]=char(p[x*4+1]);row[x*3+2]=char(p[x*4+2]);}out.write(row.data(),std::streamsize(row.size()));}}}catch(...){}}).detach();
    }
    if(capture){++st.refreshCaptures;const auto path=output+"/native-xr/refresh-"+std::to_string(refresh)+"-eye-"+std::to_string(captureEye)+".ppm";
        std::error_code ec;std::filesystem::create_directories(std::filesystem::path(output)/"native-xr",ec);
        // written on a thread of its own: the refresh thread holds the queue lock, and the game's thread waits for it
        const uint32_t pw=xr.targets[0].width,ph=xr.targets[0].height;
        std::thread([path,pw,ph,pixels=bytes]{try{if(pixels.size()!=size_t(pw)*ph*4)return;std::ofstream out(path,std::ios::binary);out<<"P6\n"<<pw<<' '<<ph<<"\n255\n";
            std::vector<char> row(size_t(pw)*3);for(uint32_t y=0;y<ph;++y){const auto* p=pixels.data()+size_t(y)*pw*4;for(uint32_t x=0;x<pw;++x){row[x*3]=char(p[x*4]);row[x*3+1]=char(p[x*4+1]);row[x*3+2]=char(p[x*4+2]);}
            out.write(row.data(),std::streamsize(row.size()));}}catch(...){}}).detach();}
    const auto n=++st.refreshLogs;
    if(n<=3||n%900==0||capture||shot){std::ostringstream o;o<<std::setprecision(6)<<"\"refresh\":"<<refresh<<",\"time\":"<<time<<",\"kept_frames\":["<<w.keptFrame(0)<<','<<w.keptFrame(1)<<"],\"from_other\":["
        <<(eyes[0].fromOther?"true":"false")<<','<<(eyes[1].fromOther?"true":"false")<<"],\"carry\":"<<(carry?"true":"false")<<",\"part\":"<<part<<",\"smooth_speed\":"<<smoothSpeed<<",\"smooth_turn\":"<<smoothTurn<<",\"smooth_short\":"<<smoothShort<<",\"last_turn_deg\":"<<(last?2.f*std::atan2(last->perFrame.orientation.y,last->perFrame.orientation.w)*57.29578f:0.f)<<",\"motion_valid\":"<<(last&&last->valid?"true":"false")
        <<",\"motion_move\":["<<(last?last->perFrame.position.x:0.f)<<','<<(last?last->perFrame.position.y:0.f)<<','<<(last?last->perFrame.position.z:0.f)<<"],\"motion_dt\":"<<(last?last->dt:0)
        <<",\"object_frames\":["<<eyes[0].keptMotion<<','<<eyes[1].keptMotion<<"],\"object_kept\":["<<(w.keptMotion(0)?"true":"false")<<','<<(w.keptMotion(1)?"true":"false")<<"],\"pairs\":"<<w.pairs<<",\"previous_wait_ms\":"<<w.waitMs<<",\"previous_wait_max_ms\":"<<w.waitMaxMs<<",\"waits_held\":"<<w.waitsHeld<<",\"gpu_ms\":"<<w.gpuMs<<",\"gpu_mean_ms\":"<<(w.gpuCount?w.gpuSum/double(w.gpuCount):-1.0)<<",\"gpu_max_ms\":"<<w.gpuMax<<",\"captured\":"<<(capture||shot?"true":"false")<<",\"shot\":"<<(shot?"true":"false");
        st.log->event("afw_fill",o.str());}
    return true;
}
// The warp of the alternate eyes (eye_warp.hpp), at the eyes' extent now (their runtime images' used part).
void makeEyeWarp(LiveNativeXr& state){
    auto* st=&state;auto& xr=*state.xr;
    if(xr.targets[0].width!=xr.targets[1].width||xr.targets[0].height!=xr.targets[1].height)throw std::runtime_error("the eyes differ in size");
    state.warp.reset();
    state.warp=std::make_unique<EyeWarp>(xr.vk,xr.format,xr.targets[0].width,xr.targets[0].height);
    if(state.warpFromOther)state.warp->cells=320;   // fine cells: the holes at the depth steps are a cell wide (6 px here; 144 cells left a jagged 14 px band along the sword)
    if(const auto c=std::getenv("EVR_EYE_WARP_CELLS")){const auto n=std::strtoul(c,nullptr,10);if(n>=8&&n<=512)state.warp->cells=unsigned(n);}
    if(const auto n=std::getenv("EVR_EYE_WARP_NEAR")){const auto f=std::strtof(n,nullptr);if(f>0&&f<10)state.warp->nearPlane=f;}
    state.log->event("eye_warp_enabled","\"cells\":"+std::to_string(state.warp->cells)+",\"near\":"+std::to_string(state.warp->nearPlane)+",\"far\":"+std::to_string(state.warp->farDistance)+
        ",\"from_other_eye\":"+std::string(state.warpFromOther?"true":"false")+",\"self_check\":"+(state.warpCheck?"true":"false")+
        ",\"sweep_env\":["+std::string(std::getenv("EVR_EYE_WARP_PARALLAX_SWEEP")?"1":"0")+","+[]{char v[8]{};return GetEnvironmentVariableA("EVR_EYE_WARP_PARALLAX_SWEEP",v,sizeof v)?"1":"0";}()+"]"+
        ",\"format\":"+std::to_string(xr.format)+",\"extent\":["+std::to_string(xr.targets[0].width)+','+std::to_string(xr.targets[0].height)+"]");
    // The eye left out gets the edge tint too, after its warped picture (the copy's afterCopy gives it to a drawn eye).
    state.warp->afterWarp=[st](VkCommandBuffer cmd,uint32_t eye,const TransferImage& dst)->bool{
        EdgeTintParams p;{std::lock_guard<std::mutex> l(edgeTintLock);p=edgeTintCurrent;}
        if(p.a<0.002f||edgeTintFailed.load())return false;
        try{if(!st->edgeTint){st->edgeTint=std::make_unique<EdgeTint>(st->xr->vk,st->xr->format);st->log->event("edge_tint_enabled","\"format\":"+std::to_string(st->xr->format)+",\"by\":\"warp\"");}
            st->edgeTint->record(cmd,dst,eye,p);return true;}catch(const std::exception& e){edgeTintFailed=true;st->log->event("edge_tint_failed","\"stage\":\"warp\",\"reason\":"+jsonString(e.what()));return false;}
    };
}
// The session of the eyes: made on the hooks' first world frame, or - with EVR_SCREEN_XR=1 - by the game's first
// presents for the plate (layer/screen_xr.hpp), on the same render thread. 0 = made, 2 = not possible here; throws.
int createLiveNativeXr(VkInstance instance,VkPhysicalDevice physical,VkDevice device,uint32_t family,float scale,uint32_t frames)
{
        {
            auto state=std::make_unique<LiveNativeXr>();BorrowedVulkan binding;
            if(!frames||frames>diagnostic::maximumNativeDiagnosticFrames)return 2;
            state->frameLimit=frames;
            {std::lock_guard<std::mutex> lock(mutex);auto child=devices.find(key(device));auto parent=child==devices.end()?instances.end():instances.find(key(child->second.instance));
                if(parent==instances.end()||child==devices.end()||parent->second.fixture||!parent->second.preflight||
                   !child->second.fixtureBinding||child->second.instance!=parent->second.instance)return 2;
                state->owner=parent->second.preflight;binding=*child->second.fixtureBinding;
            }
            if(family!=binding.queueFamily)return 2;
            state->owner->validateGpu(instance,binding.physical);
            auto module=GetModuleHandleW(L"vulkan-1.dll");
            binding.getInstanceProcAddr=module?reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module,"vkGetInstanceProcAddr")):nullptr;
            binding.instance=instance;binding.physical=physical;binding.device=device;state->device=device;
            std::filesystem::create_directories(output+"/native-xr");
            state->log=std::make_unique<Log>(output+"/native-xr/openxr.jsonl",true);
            if(const auto cap=std::getenv("EVR_LOG_NAME_CAP")){const auto n=std::strtoul(cap,nullptr,10);if(n)state->log->setNameCap(unsigned(n));}
            state->xr=std::make_unique<OpenXrClient>(*state->log,state->owner->loaderEntryPoint());
            state->xr->diagnosticCameraMode="native_render_camera_diagnostic";
            const auto shared=state->owner->borrowInstance();
            state->xr->init(scale,&binding,&shared,XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT);
            if(!rgba8(state->xr->format))throw std::runtime_error("native GPU copy requires RGBA8 runtime format");
            state->transfer=std::make_unique<StereoTransfer>(state->xr->vk);
            // The mod's own queue (the binding's, added by evrCreateDevice for pacing 2): the game's queue, the semaphores between
            // them and a barrier for after each wait (ownQueueIn / ownQueueOut); the frames for every refresh may run then.
            if(binding.ownQueue)try{
                auto& vk=state->xr->vk;auto& o=state->own;
                o.createSemaphore=reinterpret_cast<PFN_vkCreateSemaphore>(vk.vkGetDeviceProcAddr(vk.device,"vkCreateSemaphore"));
                o.destroySemaphore=reinterpret_cast<PFN_vkDestroySemaphore>(vk.vkGetDeviceProcAddr(vk.device,"vkDestroySemaphore"));
                if(!o.createSemaphore||!o.destroySemaphore)throw std::runtime_error("no semaphore functions");
                vk.vkGetDeviceQueue(vk.device,binding.queueFamily,0,&o.game);
                if(!o.game||o.game==vk.queue)throw std::runtime_error("the game's queue is the binding's");
                VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                vkCheck(o.createSemaphore(vk.device,&si,nullptr,&o.fromGame),"own queue semaphore");vkCheck(o.createSemaphore(vk.device,&si,nullptr,&o.toGame),"own queue semaphore");
                VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=vk.family;vkCheck(vk.vkCreateCommandPool(vk.device,&pi,nullptr,&o.pool),"own queue pool");
                VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=o.pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=2;
                std::array<VkCommandBuffer,2> barriers{};vkCheck(vk.vkAllocateCommandBuffers(vk.device,&ai,barriers.data()),"own queue barriers");o.xrBarrier=barriers[0];o.gameBarrier=barriers[1];
                for(const auto c:barriers){VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;vkCheck(vk.vkBeginCommandBuffer(c,&bi),"own queue barrier begin");
                    VkMemoryBarrier m{VK_STRUCTURE_TYPE_MEMORY_BARRIER};m.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;m.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
                    vk.vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&m,0,nullptr,0,nullptr);vkCheck(vk.vkEndCommandBuffer(c),"own queue barrier end");}
                state->ownQueue=true;afwFillPrepared=true;uiGlyph::modOwnQueue=vk.queue;eyeMotion::keepObjectMotion=true;
                state->log->event("afw_own_queue","\"queue_index\":"+std::to_string(binding.queueIndex)+",\"family\":"+std::to_string(binding.queueFamily)+",\"created_queue_count\":"+std::to_string(binding.createdQueueCount)+
                    ",\"game_queue\":"+handleText(o.game)+",\"xr_queue\":"+handleText(vk.queue));
            }catch(const std::exception& e){state->log->event("afw_own_queue_failed","\"reason\":"+jsonString(e.what()));throw;}
            {static const bool historyWanted=[]{const auto f=std::getenv("EVR_BUG_HISTORY");return !(f&&!std::strcmp(f,"0"));}();
             if(historyWanted)try{
                state->history=std::make_unique<EyeHistory>(state->xr->vk);
                state->transfer->history=[st=state.get()](VkCommandBuffer cmd,size_t eye,const TransferImage& src){
                    if(st->historyFailed||!st->history)return;
                    try{st->history->record(cmd,eye,src);}catch(const std::exception& e){st->historyFailed=true;st->log->event("eye_history_failed","\"stage\":\"record\",\"reason\":"+jsonString(e.what()));}
                };
                state->log->event("eye_history_enabled","\"slots\":"+std::to_string(state->history->slots)+",\"every_ms\":"+std::to_string(state->history->everyMs)+",\"half_size\":true");
             }catch(const std::exception& e){state->history.reset();state->log->event("eye_history_failed","\"stage\":\"create\",\"reason\":"+jsonString(e.what()));}}
            {const auto m=std::getenv("EVR_DESKTOP_MIRROR");state->transfer->mirrorRight=m&&!std::strcmp(m,"1");}
            // EVR_EYE_WARP=1: the eye left out shows its last picture warped (second stage); 2: made from the other eye's
            // picture of the frame, its last picture in the holes (third stage, PureDark's way).
            {const auto w=std::getenv("EVR_EYE_WARP");state->warpWanted=w&&(!std::strcmp(w,"1")||!std::strcmp(w,"2"));state->warpFromOther=w&&!std::strcmp(w,"2");
             const auto c=std::getenv("EVR_EYE_WARP_CHECK");const auto every=c?std::strtoul(c,nullptr,10):0;state->warpCheck=state->warpWanted&&every>=1;state->warpCheckEvery=every>=1?every:120;}   // EVR_EYE_WARP_CHECK=n: every n-th frame that draws both eyes
            if(state->warpWanted)try{
                makeEyeWarp(*state);
                // Every eye copied keeps its colour and the game's depth of that frame for the warp: the game's picture (keepEye),
                // or - a game frame of the refresh thread's time - the picture with what was drawn over it (keepDrawn).
                auto keepPicture=[st=state.get()](VkCommandBuffer cmd,size_t eye,const TransferImage& src){
                    if(st->warpFailed||eye>=2||!st->warp)return;
                    try{
                        EyeDepthNote note;const uint32_t drawnEyes=st->xr->pendingFrame().eyeMask;
                        if(!eyeDepthByOrder(drawnEyes,uint32_t(eye),st->depthNoteUsed[eye],note)){if(++st->warpKeepMisses<=3)st->log->event("eye_warp_keep_missed","\"eye\":"+std::to_string(eye)+",\"picture\":"+handleText(src.image)+",\"drawn_eyes\":"+std::to_string(drawnEyes)+",\"reason\":\"no new depth note for this eye\"");return;}
                        st->depthNoteUsed[eye]=note.at;
                        diagnostic::NativeImageEvidence e;
                        if(evrGetNativeImageEvidence(st->device,note.depth,&e)||!e.ready){if(++st->warpKeepMisses<=3)st->log->event("eye_warp_keep_missed","\"eye\":"+std::to_string(eye)+",\"depth\":"+handleText(note.depth)+",\"reason\":\"depth layout unproven\"");return;}
                        if(!(e.usage&VK_IMAGE_USAGE_SAMPLED_BIT)){if(++st->warpKeepMisses<=3)st->log->event("eye_warp_keep_missed","\"eye\":"+std::to_string(eye)+",\"depth\":"+handleText(note.depth)+",\"usage\":"+std::to_string(e.usage)+",\"reason\":\"depth has no sampled usage\"");return;}
                        const auto& snapshot=st->xr->pendingFrame();
                        {const int m=eyeWarpUnjitter(snapshot.frame);eyeWarpUnjitterUsed=m;const float sx=(m==1||m==3)?1.f:(m==2||m==4)?-1.f:0.f,sy=(m==1||m==4)?1.f:(m==2||m==3)?-1.f:0.f;
                         st->warp->depthShift={sx*note.jitterX,sy*note.jitterY};}
                        VkImage objectImage{};uint32_t objectWidth=0,objectHeight=0;
                        const bool object=eyeMotion::takeObjectMotion(uint32_t(eye),objectImage,objectWidth,objectHeight);
                        st->warp->keep(cmd,uint32_t(eye),src,note.depth,e.layout,note.width,note.height,note.format,snapshot.views[eye],snapshot.frame,
                            object?objectImage:VK_NULL_HANDLE,objectWidth,objectHeight);
                        const auto n=st->warp->keeps;
                        if(n<=2||n%1200==0)st->log->event("eye_warp_kept","\"frame\":"+std::to_string(snapshot.frame)+",\"eye\":"+std::to_string(eye)+",\"keeps\":"+std::to_string(n)+",\"depth\":"+handleText(note.depth)+
                            ",\"depth_size\":["+std::to_string(note.width)+','+std::to_string(note.height)+"],\"depth_format\":"+std::to_string(note.format)+",\"depth_layout\":"+std::to_string(e.layout)+",\"depth_usage\":"+std::to_string(e.usage));
                    }catch(const std::exception& x){st->warpFailed=true;st->log->event("eye_warp_failed","\"stage\":\"keep\",\"eye\":"+std::to_string(eye)+",\"reason\":"+jsonString(x.what()));}
                };
                state->transfer->keepEye=[st=state.get(),keepPicture](VkCommandBuffer cmd,size_t eye,const TransferImage& src){if(!st->xr->pendingAsync())keepPicture(cmd,eye,src);};
                state->transfer->keepDrawn=[st=state.get(),keepPicture](VkCommandBuffer cmd,size_t eye,const TransferImage& drawn){if(st->xr->pendingAsync())keepPicture(cmd,eye,drawn);};
            }catch(const std::exception& e){state->warpWanted=false;state->warp.reset();state->log->event("eye_warp_unavailable","\"reason\":"+jsonString(e.what()));}
            state->transfer->afterCopy=[st=state.get()](VkCommandBuffer cmd,size_t eye,const TransferImage& dst){
                bool drawn=false; // true once something left the image in COLOR_ATTACHMENT_OPTIMAL
                EdgeTintParams p;{std::lock_guard<std::mutex> l(edgeTintLock);p=edgeTintCurrent;}
                if(p.a>=0.002f&&!edgeTintFailed.load()&&!st->xr->pendingAsync()){
                    try{
                        if(!st->edgeTint){st->edgeTint=std::make_unique<EdgeTint>(st->xr->vk,st->xr->format);st->log->event("edge_tint_enabled","\"format\":"+std::to_string(st->xr->format));}
                        st->edgeTint->record(cmd,dst,eye,p);drawn=true;
                        const auto n=eye==0?++edgeTintFrames:edgeTintFrames.load();
                        if(eye==0&&(n<=3||n%600==0)){std::ostringstream o;o<<std::setprecision(4)<<"\"frames\":"<<n<<",\"rgb\":["<<p.r<<','<<p.g<<','<<p.b<<"],\"strength\":"<<p.a<<",\"band\":"<<p.band;st->log->event("edge_tint_frame",o.str());}
                    }catch(const std::exception& e){
                        // Creation/target failures happen before any command is recorded; the eye stays untinted.
                        edgeTintFailed=true;st->log->event("edge_tint_failed","\"reason\":"+jsonString(e.what()));
                    }
                }
                // The mod's own stamina ring (evrSetStamina above): head-fixed where VHVR has its stamina, at that point's depth
                // in both eyes. Shown while the stamina is below the most and 0.7 s after (fading), red from 0 until full.
                {const auto t=GetTickCount64();const auto at=staminaAt.load(),play=staminaStripAt.load();
                 static uint64_t belowAt=0;static bool runOut=false;
                 if(eye<2&&!staminaRingFailed.load()&&at&&t>=at&&t-at<=600&&play&&t>=play&&t-play<=300){
                    const int most=staminaMost.load(),now=std::min(staminaNow.load(),most);const float share=most>0?float(now)/float(most):1.f;
                    if(eye==0){if(share<.999f)belowAt=t;if(now<=0)runOut=true;else if(share>=.999f)runOut=false;}
                    const float strength=share<.999f?1.f:belowAt&&t>=belowAt&&t-belowAt<700?1.f-float(t-belowAt)/700.f:0.f;
                    if(strength>0)try{
                        const auto& views=st->xr->pendingFrame().views;const auto& view=views[eye];
                        const Vec3 middle{(views[0].pose.position.x+views[1].pose.position.x)*.5f,(views[0].pose.position.y+views[1].pose.position.y)*.5f,(views[0].pose.position.z+views[1].pose.position.z)*.5f};
                        // Where the game's own floating ring stood on the HUD plate (2026-10-26, user after the headset: "the stamina's place is
                        // hard to see - it may follow as before; mind the focal distance"; the report pictures 200115/200206: about 230 left of
                        // the middle and 80 below, of a 1080 rows HUD, radius about 25) - on the plate's plane, 1.5 m ahead (2 m wide): 0.24 m
                        // left, 0.083 m down, radius 0.026 m. The head's turn: both views' (the Index's views turn 6 degrees outwards each).
                        Quat turn=views[0].pose.orientation;{const auto& b=views[1].pose.orientation;const float sgn=turn.x*b.x+turn.y*b.y+turn.z*b.z+turn.w*b.w<0?-1.f:1.f;
                            turn=normalized(Quat{turn.x+b.x*sgn,turn.y+b.y*sgn,turn.z+b.z*sgn,turn.w+b.w*sgn});}
                        const Vec3 point=middle+rotate(turn,Vec3{-.24f,-.083f,-1.5f});
                        const float metres=length(point-middle);
                        const auto place=aim::eyePlace(view.pose,view.fov.angleLeft,view.fov.angleRight,view.fov.angleUp,view.fov.angleDown,point,dst.width,dst.height,.026f/std::max(metres,.1f));
                        if(place.on){
                            if(!st->eyeMark){st->eyeMark=std::make_unique<EyeMark>(st->xr->vk,st->xr->format);st->log->event("aim_mark_enabled","\"format\":"+std::to_string(st->xr->format)+",\"by\":\"stamina\"");}
                            st->eyeMark->recordRing(cmd,dst,{place.x,place.y,place.radius,std::max(2.f,place.radius*.28f),share,strength,runOut},drawn);drawn=true;
                            const auto n=eye==0?++staminaRingFrames:staminaRingFrames.load();
                            if(n<=4||n%600==0){std::ostringstream o;o<<std::setprecision(6)<<"\"frames\":"<<n<<",\"eye\":"<<eye<<",\"pixel\":["<<place.x<<','<<place.y<<"],\"radius\":"<<place.radius<<",\"share\":"<<share<<",\"run_out\":"<<(runOut?"true":"false")<<",\"strength\":"<<strength;st->log->event("stamina_ring_frame",o.str());}
                        }
                    }catch(const std::exception& e){staminaRingFailed=true;st->log->event("stamina_ring_failed","\"reason\":"+jsonString(e.what()));}
                 }}
                // The aim mark: the aimed place as this eye sees it (the pose and field of view the eye image is
                // submitted with), a dot there.
                Vec3 aimed{};
                if(eye<2&&!aimMarkFailed.load()&&aimMarkNow(aimed)){
                    try{
                        const auto& view=st->xr->pendingFrame().views[eye];
                        const auto place=aim::eyePlace(view.pose,view.fov.angleLeft,view.fov.angleRight,view.fov.angleUp,view.fov.angleDown,aimed,dst.width,dst.height);
                        if(place.on){
                            if(!st->eyeMark){st->eyeMark=std::make_unique<EyeMark>(st->xr->vk,st->xr->format);st->log->event("aim_mark_enabled","\"format\":"+std::to_string(st->xr->format));}
                            st->eyeMark->record(cmd,dst,{place.x,place.y,place.radius,1.f},drawn);drawn=true;
                            const auto n=eye==0?++aimMarkFrames:aimMarkFrames.load();
                            if(n<=4||n%300==0){std::ostringstream o;o<<std::setprecision(7)<<"\"frames\":"<<n<<",\"frame\":"<<st->xr->pendingFrame().frame<<",\"eye\":"<<eye<<",\"pixel\":["<<place.x<<','<<place.y<<"],\"radius\":"<<place.radius
                                <<",\"extent\":["<<dst.width<<','<<dst.height<<"],\"point\":["<<aimed.x<<','<<aimed.y<<','<<aimed.z<<']';st->log->event("aim_mark_frame",o.str());}
                        }
                    }catch(const std::exception& e){
                        // As above: nothing was recorded for the mark; the eye stays without it.
                        aimMarkFailed=true;st->log->event("aim_mark_failed","\"reason\":"+jsonString(e.what()));
                    }
                }
                // The mod's own picture beside the mark (evrSetAimPanel): its left edge `gap` degrees right of the dot, the anchor row at
                // the dot's height, in each eye at the dot's place - so at the aimed point's depth, as the dot.
                if(eye<2&&!aimPanelFailed.load()){
                    Vec3 at{};const auto t=GetTickCount64();const auto when=aimPanelAt.load();
                    if(when&&t>=when&&t-when<=250)try{
                        const bool marked=aimMarkNow(at);
                        std::vector<uint8_t> picture;uint32_t w=0,h=0;float degrees=0,anchor=.5f,gap=0,strength=0;uint64_t serial=0;
                        {std::lock_guard<std::mutex> l(aimPanelLock);serial=aimPanelSerialNow;degrees=aimPanelDegrees;anchor=aimPanelAnchor;gap=aimPanelGap;strength=aimPanelStrength;
                         if(serial!=st->aimPanelSerial){picture=aimPanelBytes;w=aimPanelWidth;h=aimPanelHeight;}}
                        if(strength>0){
                            if(!st->eyePanel){st->eyePanel=std::make_unique<EyePanel>(st->xr->vk,st->xr->format);st->log->event("aim_panel_enabled","\"format\":"+std::to_string(st->xr->format));}
                            if(serial!=st->aimPanelSerial&&!picture.empty()){st->eyePanel->setPicture(picture,w,h);st->aimPanelSerial=serial;}
                            if(st->eyePanel->hasPicture()){
                                // a quad in the room where the cluster stands (aim_mark.hpp panelQuad: towards the aimed point, at most 20 m),
                                // seen by each eye through its own view (2026-10-26: the same pixel shift in both eyes was not one depth on the
                                // Index, whose views turn 6 degrees outwards)
                                const auto& views=st->xr->pendingFrame().views;const auto& view=views[eye];
                                const Vec3 head{(views[0].pose.position.x+views[1].pose.position.x)*.5f,(views[0].pose.position.y+views[1].pose.position.y)*.5f,(views[0].pose.position.z+views[1].pose.position.z)*.5f};
                                if(!marked){   // no mark (a bow not drawn): where the game's list stood - the HUD plate's middle, 1.5 m ahead
                                    Quat turn=views[0].pose.orientation;const auto& o=views[1].pose.orientation;const float sgn=turn.x*o.x+turn.y*o.y+turn.z*o.z+turn.w*o.w<0?-1.f:1.f;
                                    turn=normalized(Quat{turn.x+o.x*sgn,turn.y+o.y*sgn,turn.z+o.z*sgn,turn.w+o.w*sgn});at=head+rotate(turn,Vec3{0,0,-1.5f});}
                                const auto quad=aim::panelQuad(head,at,degrees,float(st->eyePanel->pictureHeight())/float(st->eyePanel->pictureWidth()),anchor,gap);
                                EyePanelParams params;std::array<float,4> box{};
                                if(aim::panelInEye(quad,view.pose,view.fov.angleLeft,view.fov.angleRight,view.fov.angleUp,view.fov.angleDown,dst.width,dst.height,params.h,box)){
                                    params.x0=box[0];params.y0=box[1];params.x1=box[2];params.y1=box[3];params.strength=strength;
                                    st->eyePanel->record(cmd,dst,params,drawn);drawn=true;
                                    const auto n=eye==0?++aimPanelFrames:aimPanelFrames.load();
                                    if(n<=4||n%600==0){std::ostringstream o;o<<std::setprecision(6)<<"\"frames\":"<<n<<",\"eye\":"<<eye<<",\"box\":["<<box[0]<<','<<box[1]<<','<<box[2]<<','<<box[3]<<"],\"picture\":["
                                        <<st->eyePanel->pictureWidth()<<','<<st->eyePanel->pictureHeight()<<"],\"degrees\":"<<degrees<<",\"strength\":"<<strength
                                        <<",\"quad_origin\":["<<quad.origin.x<<','<<quad.origin.y<<','<<quad.origin.z<<"],\"distance_m\":"<<length(quad.origin-head);st->log->event("aim_panel_frame",o.str());}
                                }
                            }
                        }
                    }catch(const std::exception& e){aimPanelFailed=true;st->log->event("aim_panel_failed","\"reason\":"+jsonString(e.what()));}
                }
                // The throw's guide: a short white beam from its first point to its last (user 2026-10-18 12:40: "a short laser
                // rather than a dotted line", 13:20 "white"), about 5 mm thick near the hand, fainter towards its end.
                std::array<Vec3,16> guide{};
                if(eye<2&&!throwGuideFailed.load())if(const auto count=throwGuideNow(guide)){
                    try{
                        const auto& view=st->xr->pendingFrame().views[eye];unsigned on=0;
                        const auto& from=guide[0];const auto& to=guide[count-1];
                        const float away=length(from-view.pose.position);
                        if(std::isfinite(away)&&away>=.05f){
                            const float radians=std::clamp(.0025f/away,.0012f,.02f);
                            const auto a=aim::eyePlace(view.pose,view.fov.angleLeft,view.fov.angleRight,view.fov.angleUp,view.fov.angleDown,from,dst.width,dst.height,radians);
                            const auto b=aim::eyePlace(view.pose,view.fov.angleLeft,view.fov.angleRight,view.fov.angleUp,view.fov.angleDown,to,dst.width,dst.height,radians);
                            if(a.radius>0&&b.radius>0&&(a.on||b.on)){   // both ends in front of the eye (radius 0: behind it)
                                if(!st->eyeMark){st->eyeMark=std::make_unique<EyeMark>(st->xr->vk,st->xr->format);st->log->event("aim_mark_enabled","\"format\":"+std::to_string(st->xr->format)+",\"by\":\"throw_guide\"");}
                                st->eyeMark->recordBeam(cmd,dst,{a.x,a.y,b.x,b.y,std::max(2.5f,a.radius*2.f),1.f},drawn);drawn=true;on=2;
                            }
                        }
                        const auto n=eye==0?++throwGuideFrames:throwGuideFrames.load();
                        if(n<=4||n%300==0){std::ostringstream o;o<<std::setprecision(6)<<"\"frames\":"<<n<<",\"frame\":"<<st->xr->pendingFrame().frame<<",\"eye\":"<<eye<<",\"points\":"<<count<<",\"drawn\":"<<on
                            <<",\"first\":["<<guide[0].x<<','<<guide[0].y<<','<<guide[0].z<<"],\"last\":["<<guide[count-1].x<<','<<guide[count-1].y<<','<<guide[count-1].z<<']';st->log->event("throw_guide_frame",o.str());}
                    }catch(const std::exception& e){throwGuideFailed=true;st->log->event("throw_guide_failed","\"reason\":"+jsonString(e.what()));}
                }
                // The locked target's mark: a downward chevron (two beams to its tip) above the target's head, 0.8 degrees
                // half wide to the eye at any distance (0.7 at first: 13 pixels in a Mock eye image of 1570, lost in the view;
                // then 1.6, and the user after the headset run of 2026-10-19 14:00: "half of the present size is right").
                Vec3 targeted{};
                if(eye<2&&!targetMarkFailed.load()&&targetMarkNow(targeted)){
                    try{
                        const auto& view=st->xr->pendingFrame().views[eye];
                        const auto tip=aim::eyePlace(view.pose,view.fov.angleLeft,view.fov.angleRight,view.fov.angleUp,view.fov.angleDown,targeted,dst.width,dst.height,.014f);
                        if(tip.on&&tip.radius>0){
                            if(!st->eyeMark){st->eyeMark=std::make_unique<EyeMark>(st->xr->vk,st->xr->format);st->log->event("aim_mark_enabled","\"format\":"+std::to_string(st->xr->format)+",\"by\":\"target_mark\"");}
                            const float w=tip.radius,h=w*1.3f,thick=std::max(2.5f,w*.26f);
                            st->eyeMark->recordBeam(cmd,dst,{tip.x,tip.y,tip.x-w,tip.y-h,thick,1.f},drawn);drawn=true;
                            st->eyeMark->recordBeam(cmd,dst,{tip.x,tip.y,tip.x+w,tip.y-h,thick,1.f},drawn);
                            const auto n=eye==0?++targetMarkFrames:targetMarkFrames.load();
                            if(n<=4||n%300==0){std::ostringstream o;o<<std::setprecision(6)<<"\"frames\":"<<n<<",\"frame\":"<<st->xr->pendingFrame().frame<<",\"eye\":"<<eye<<",\"pixel\":["<<tip.x<<','<<tip.y<<"],\"half_width\":"<<w
                                <<",\"point\":["<<targeted.x<<','<<targeted.y<<','<<targeted.z<<']';st->log->event("target_mark_frame",o.str());}
                        }
                    }catch(const std::exception& e){targetMarkFailed=true;st->log->event("target_mark_failed","\"reason\":"+jsonString(e.what()));}
                }
                return drawn;
            };
            state->generation=++nextNativeXrGeneration;
            {
                const auto asyncFlag=std::getenv("EVR_XR_ASYNC_COMPLETION");
                const bool requested=asyncFlag&&!std::strcmp(asyncFlag,"1");
                state->asyncCompletion=requested&&(binding.createdQueueCount==1||state->ownQueue)&&state->xr->vk.family==binding.queueFamily;
                state->log->event("native_xr_completion_policy","\"async\":"+std::string(state->asyncCompletion?"true":"false")+
                    ",\"requested\":"+(requested?"true":"false")+",\"queue_family\":"+std::to_string(binding.queueFamily)+
                    ",\"created_queue_count\":"+std::to_string(binding.createdQueueCount)+",\"binding_queue_family\":"+std::to_string(state->xr->vk.family)+
                    ",\"reason\":"+jsonString(!requested?"strict_default":state->asyncCompletion?(state->ownQueue?"own_queue_semaphores":"single_queue_family_ordered"):"family_queue_identity_unproven"));
            }
            const auto motionFlag=std::getenv("EVR_NATIVE_MOTION_HANDS");
            if(motionFlag&&!std::strcmp(motionFlag,"1")){
                state->hands=std::make_unique<motion::Actions>();
                state->hands->open(state->owner->loaderEntryPoint(),state->xr->instance,state->xr->session,state->generation);
                // EVR_GRIP_TRIM="yaw,pitch,roll" degrees (launcher -PrepareRealVr default, 2026-10-09); absent = identity.
                std::string trim="none";
                if(const auto t=std::getenv("EVR_GRIP_TRIM");t&&*t){float y=0,p=0,r=0;char extra=0;
                    if(std::sscanf(t,"%f,%f,%f%c",&y,&p,&r,&extra)!=3)throw std::runtime_error("EVR_GRIP_TRIM must be yaw,pitch,roll degrees");
                    state->hands->setGripTrim(y,p,r);trim=t;}
                state->log->event("native_motion_opened","\"generation\":"+std::to_string(state->generation)+",\"grip_trim_degrees\":"+jsonString(trim));
            }
            liveNativeXr=std::move(state);
            // Opt-in ordering diagnostic (EVR_QUEUE_TRACE_LIVE=N, N<=4096): arm the bounded queue
            // submit trace when the first live XR frame begins, so game vs MOD copy order is visible.
            if(const auto armFlag=std::getenv("EVR_QUEUE_TRACE_LIVE")){
                const auto arm=std::strtoul(armFlag,nullptr,10);
                if(arm>0&&arm<=4096){queueTraceBudget=uint32_t(arm);liveNativeXr->log->event("native_queue_trace_armed","\"budget\":"+std::to_string(arm));}
            }
        }
        return 0;
}
extern "C" __declspec(dllexport) int evrBeginNativeXr(VkInstance instance,VkPhysicalDevice physical,VkDevice device,
    uint32_t family,float scale,diagnostic::NativeXrFrame* frame) noexcept
{
    try {
        const auto enabled=std::getenv("EVR_LIVE_NATIVE_XR");
        if(!enabled||std::strcmp(enabled,"1")||!frame||frame->structSize!=sizeof(*frame)||frame->abiVersion!=diagnostic::nativeXrFrameAbiVersion)return 2;
        const auto requestedFrames=frame->diagnosticFrames;const auto requestedEyes=frame->eyeMask;
        *frame=diagnostic::NativeXrFrame{};frame->diagnosticFrames=requestedFrames;
        QueueCallScope context("openxr_native_begin");
        if(!liveNativeXr){if(const int made=createLiveNativeXr(instance,physical,device,family,scale,frame->diagnosticFrames))return made;}
        auto &state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId()||state.device!=device)throw std::runtime_error("native XR thread/device changed");
        if(!state.worldSession){state.worldPairs=0;
            // A world after the first (2026-10-25: a player's second world had no records of the UI left - every kind had reached its
            // limit of the first world): the per-name limits start again, for this log and the layer's.
            if(state.submittedPairs){if(const auto cap=state.log->nameCap())state.log->setNameCap(cap);
            if(logger){if(const auto cap=logger->nameCap())logger->setNameCap(cap);}
            state.log->event("native_world_session_again","\"generation\":"+std::to_string(state.generation)+",\"submitted_pairs\":"+std::to_string(state.submittedPairs));}}
        state.worldSession=true;state.lastWorldBegin=GetTickCount64();
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        while(!state.xr->active()){
            if(!state.xr->poll()||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("native XR session not ready");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        {
            // One eye a frame (alternate-eye rendering) unless a picture of both eyes is to be taken in this frame: the
            // requests that evrFinishNativeXr takes (a tag, a burst, a request file on every 15th pair, the evidence pairs).
            uint32_t eyes=(requestedEyes&3)?(requestedEyes&3):3u;
            if(state.alternateRefused)eyes=3;
            // EVR_CAPTURE_SINGLE=1 (comparisons, 2026-10-14): pictures are taken of the one eye drawn - each eye then
            // drawn wholly by itself, none of it borrowed from the other - so frames of one eye stay as they are.
            static const bool singleCapture=[]{const auto f=std::getenv("EVR_CAPTURE_SINGLE");return f&&!std::strcmp(f,"1");}();
            if(eyes!=3&&!singleCapture){
                static const bool playLog=[]{const auto f=std::getenv("EVR_PLAY_LOG");return f&&!std::strcmp(f,"1");}();
                const auto pairNumber=state.submittedPairs+1;std::error_code ec;
                if(nativeFixtureCapture.load()||nativeCaptureBurst.pending()||bugCaptureBurst.pending()||(!playLog&&(pairNumber==1||pairNumber==state.frameLimit/2||pairNumber==state.frameLimit))||
                   (state.submittedPairs%15==0&&(std::filesystem::exists(std::filesystem::path(output)/"xr-capture.request",ec)||std::filesystem::exists(std::filesystem::path(output)/"xr-burst.request",ec))))
                    eyes=3;
            }
            // The mod's own frame for every refresh (afw_pacing.hpp, pacing 2): started once both eyes have kept pictures,
            // stopped when another pacing is chosen; after a failure of its thread the game's thread ends the frames for the
            // rest of the session.
            {
                if(state.xr->refreshFailed()&&!state.refreshGaveUp){state.refreshGaveUp=true;afw::pacing=0;state.log->event("afw_fill_gave_up","\"frame\":"+std::to_string(state.xr->frames));}
                const bool want=afw::pacing.load()==2&&state.ownQueue&&!state.refreshGaveUp&&state.warp&&!state.warpFailed&&state.warpFromOther&&!state.alternateRefused&&
                    state.warp->canWarp(0)&&state.warp->canWarp(1);
                if(want&&!state.xr->refreshing()){
                    try{state.xr->startRefreshing([st=&state](const std::array<XrView,2>& views,XrTime time,const std::array<uint32_t,2>& indices,uint64_t refresh){return drawRefresh(*st,views,time,indices,refresh);});}
                    catch(const std::exception& e){state.refreshGaveUp=true;state.log->event("afw_fill_start_failed","\"reason\":"+jsonString(e.what()));}
                }else if(!want&&state.xr->refreshing())state.xr->stopRefreshing();
            }
            state.xr->requestEyes(eyes);
            // The eye left out gets an image for its warped picture when its last drawn one is kept with its depth.
            uint32_t warpMask=0;
            if(state.warpWanted&&!state.warpFailed&&eyes!=3&&state.warp)for(uint32_t eye=0;eye<2;++eye)if(!(eyes>>eye&1)&&state.warp->canWarp(eye,state.warpFromOther))warpMask|=1u<<eye;
            state.xr->requestWarp(warpMask);
            if(!state.xr->beginFrame())throw std::runtime_error("native XR session stopped");
            const auto snapshot=state.xr->pendingFrame();
            {static int loggedPacing=-1;static uint64_t loggedAt=0;const int pacing=afw::pacing.load();
             if(pacing!=loggedPacing||snapshot.frame>=loggedAt+900){loggedPacing=pacing;loggedAt=snapshot.frame;
                state.log->event("afw_pacing","\"frame\":"+std::to_string(snapshot.frame)+",\"pacing\":"+std::to_string(pacing)+",\"begun_again\":"+std::to_string(afw::steadyBegunAgain.load())+
                    ",\"predicted_time\":"+std::to_string(snapshot.predictedDisplayTime)+",\"eyes\":"+std::to_string(snapshot.eyeMask)+",\"fill_prepared\":"+(afwFillPrepared.load()?"true":"false"));}}
            const auto flags=state.xr->headLocation.locationFlags;
            // the hands' sample needs a time after the last one; a game frame begun right after the refresh thread's own frames,
            // or the runtime's time after them, may not be (2026-10-23: that ended the session on the headset)
            if(state.hands){const XrTime handTime=std::max<XrTime>(snapshot.predictedDisplayTime,state.lastHandTime+1);state.lastHandTime=handTime;
                frame->hands=state.hands->sample(state.xr->local,handTime,snapshot.frame,1);}
            frame->frame=snapshot.frame;frame->time=snapshot.predictedDisplayTime;
            frame->headFlags=flags;frame->sessionGeneration=state.generation;
            if(snapshot.shouldRender&&(flags&3)==3){
                frame->frame=snapshot.frame;frame->time=snapshot.predictedDisplayTime;frame->views=snapshot.views;frame->eyeMask=snapshot.eyeMask;
                frame->head=state.xr->headLocation.pose;
                frame->headFlags=flags;frame->sessionGeneration=state.generation;
                for(size_t eye=0;eye<2;++eye){
                    frame->width[eye]=state.xr->targets[eye].width;frame->height[eye]=state.xr->targets[eye].height;
                    const auto& view=frame->views[eye];const auto& p=view.pose;const auto& f=view.fov;
                    std::ostringstream fields;fields<<std::setprecision(9)<<"\"frame\":"<<frame->frame<<",\"time\":"<<frame->time<<",\"eye\":"<<eye
                        <<",\"position\":["<<p.position.x<<','<<p.position.y<<','<<p.position.z<<"],\"orientation\":["<<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w
                        <<"],\"fov\":["<<f.angleLeft<<','<<f.angleRight<<','<<f.angleUp<<','<<f.angleDown<<']';
                    state.log->event("runtime_eye_pose",fields.str());
                }
                state.beginPresent=presents.load();
                state.log->event("native_frame_begin","\"frame\":"+std::to_string(frame->frame)+",\"time\":"+std::to_string(frame->time)+
                    ",\"head_flags\":"+std::to_string(flags)+",\"session_generation\":"+std::to_string(frame->sessionGeneration));
                return 0;
            }
            if(snapshot.shouldRender)state.xr->skipFrame("native_head_tracking_invalid");
            else state.xr->finishFrame();
            state.log->event("native_frame_deferred","\"frame\":"+std::to_string(frame->frame)+",\"time\":"+std::to_string(frame->time)+
                ",\"head_flags\":"+std::to_string(flags)+",\"session_generation\":"+std::to_string(state.generation)+",\"empty_frame_ended\":true");
            return diagnostic::nativeXrFrameDeferred;
        }
    }catch(const std::exception& e){note("native_xr_begin_failed","\"reason\":"+jsonString(e.what()));closeLiveNativeXr();return 1;}
}
// Called on the existing XR render thread, never from the frontend consumer.
// Finite100ms pulses bound any stale effect even if the renderer stops entirely.
extern "C" __declspec(dllexport) int evrNativeMotionHaptic(uint64_t frame,uint64_t generation,const float* amplitude) noexcept {
    try {
        if(!liveNativeXr||!amplitude)return 2;auto& state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId()||generation!=state.generation||!state.hands||state.xr->pendingFrame().frame!=frame)return 2;
        for(unsigned h=0;h<2;h++)if(!std::isfinite(amplitude[h])||amplitude[h]<0||amplitude[h]>1)return 2;
        std::array<bool,2> delivered{};
        for(unsigned h=0;h<2;h++)delivered[h]=amplitude[h]>0?state.hands->vibrate(h,amplitude[h],100000000):state.hands->stop(h);
        std::ostringstream o;o<<std::setprecision(9)<<"\"frame\":"<<frame<<",\"generation\":"<<generation<<",\"amplitude\":["<<amplitude[0]<<','<<amplitude[1]
            <<"],\"delivered\":["<<(delivered[0]?"true":"false")<<','<<(delivered[1]?"true":"false")<<"],\"duration\":100000000";
        state.log->event("native_motion_haptic",o.str());return 0;
    }catch(const std::exception& e){note("native_motion_haptic_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
extern "C" __declspec(dllexport) int evrNativeGestureHaptic(uint64_t frame,uint64_t generation,const float* amplitude,const XrDuration* duration) noexcept {
    try {
        if(!liveNativeXr||!amplitude||!duration)return 2;auto& state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId()||generation!=state.generation||!state.hands||state.xr->pendingFrame().frame!=frame)return 2;
        for(unsigned h=0;h<2;h++)if(!std::isfinite(amplitude[h])||amplitude[h]<0||amplitude[h]>1||duration[h]<=0||duration[h]>100000000)return 2;
        std::array<bool,2> delivered{};
        for(unsigned h=0;h<2;h++)delivered[h]=amplitude[h]>0?state.hands->vibrate(h,amplitude[h],duration[h]):state.hands->stop(h);
        std::ostringstream o;o<<std::setprecision(9)<<"\"frame\":"<<frame<<",\"generation\":"<<generation<<",\"amplitude\":["<<amplitude[0]<<','<<amplitude[1]
            <<"],\"delivered\":["<<(delivered[0]?"true":"false")<<','<<(delivered[1]?"true":"false")<<"],\"duration\":["<<duration[0]<<','<<duration[1]<<']';
        state.log->event("native_gesture_haptic",o.str());return 0;
    }catch(const std::exception& e){note("native_gesture_haptic_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
// Alternate-eye rendering: the pose (LOCAL space) the eye left out in this frame is shown at (hooks: where its game
// camera was, seen from this frame's anchor). 0 taken; 1 refused (no such frame or eye); 2 no session.
extern "C" __declspec(dllexport) int evrSetNativeXrStalePose(uint64_t frame,uint32_t eye,const XrPosef* pose) noexcept
{
    try {
        if(!liveNativeXr||!pose)return 2;auto &state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId()||!state.xr->hasPendingFrame()||state.xr->pendingFrame().frame!=frame)return 1;
        return state.xr->showStaleEyeAt(eye,*pose)?0:1;
    }catch(const std::exception& e){note("native_xr_stale_pose_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
// The world's motion under the tracking space from the last game frame to this one (hooks: alternateEyes::worldMotion),
// for the refresh thread (drawRefresh). 0 taken, 1 refused, 2 no session.
extern "C" __declspec(dllexport) int evrSetNativeXrWorldMotion(uint64_t frame,const XrPosef* motion,int64_t dt) noexcept
{
    if(!liveNativeXr||!motion)return 2;auto& state=*liveNativeXr;
    if(state.thread!=GetCurrentThreadId()||!finite(motion->position)||dt<=0)return 1;
    state.pendingMotion={frame,*motion,dt,0,true};return 0;
}
extern "C" __declspec(dllexport) int evrFinishNativeXr(uint64_t frame,const VkImage* images) noexcept
{
    try {
        if(!liveNativeXr||!images)return 2;auto &state=*liveNativeXr;
        if(state.thread!=GetCurrentThreadId())throw std::runtime_error("native XR completion thread changed");
        QueueCallScope context("openxr_native_copy_end");
        const auto completionStart=std::chrono::steady_clock::now();
        auto &xr=*state.xr;const auto snapshot=xr.pendingFrame();
        if(snapshot.frame!=frame||!snapshot.shouldRender||presents.load()<=state.beginPresent)
            throw std::runtime_error("native frame identity/present boundary not established");
        // The eyes drawn in this frame (alternate-eye rendering: one of them). Pictures of the eyes wait for a frame of
        // both (evrBeginNativeXr asks for one when they are waiting).
        const uint32_t eyes=snapshot.eyeMask;
        static const bool singleCapture=[]{const auto f=std::getenv("EVR_CAPTURE_SINGLE");return f&&!std::strcmp(f,"1");}();
        const bool both=eyes==3||singleCapture;   // with EVR_CAPTURE_SINGLE the picture of the eye drawn is taken alone
        // A game frame of the refresh thread's time (afw_pacing.hpp, pacing 2): its eyes go into the slots and are kept from
        // there; the refresh thread shows them. With the mod's own queue the queue lock is held to the end, and the copy waits
        // for the game's queue and the game's queue for it (OwnQueueScope).
        const bool asyncFrame=xr.pendingAsync();
        std::unique_lock<std::recursive_mutex> queueHold(xr.queueLock,std::defer_lock);if(state.ownQueue)queueHold.lock();
        OwnQueueScope ownQueue(state);
        const VkQueue gameQueue=state.ownQueue?state.own.game:xr.vk.queue;
        // The eyes' extent shrinks after the session's start (the lens' visible area: openxr_client.hpp decideFovCrop, the canted
        // frustums). The warp made at the full extent refused every picture after it ("eye warp colour source": 2243 x 2243 against
        // the game's 2002 x 2148 on the user's headset) and stopped - every AFW start on the headset since at least 10-22 showed
        // the eye left out a frame late (2026-10-23 10:50, user: "a delayed frame drawn with the normal one, chasing after it").
        // It is made again at the extent the eyes have now (not while the refresh thread draws with it).
        if(state.warp&&!state.warpFailed&&!xr.refreshing()&&(state.warp->extentWidth()!=xr.targets[0].width||state.warp->extentHeight()!=xr.targets[0].height)){
            const auto oldWidth=state.warp->extentWidth(),oldHeight=state.warp->extentHeight();
            try{makeEyeWarp(state);state.log->event("eye_warp_resized","\"frame\":"+std::to_string(frame)+",\"from\":["+std::to_string(oldWidth)+','+std::to_string(oldHeight)+
                "],\"to\":["+std::to_string(xr.targets[0].width)+','+std::to_string(xr.targets[0].height)+"]");}
            catch(const std::exception& e){state.warpFailed=true;state.warp.reset();state.log->event("eye_warp_failed","\"stage\":\"resize\",\"reason\":"+jsonString(e.what()));}
        }
        auto fixtureCapture=both?nativeFixtureCapture.exchange(0):0u;auto burstCapture=both?nativeCaptureBurst.take():decltype(nativeCaptureBurst.take()){};bool bugBurst=false;if(both&&!burstCapture){burstCapture=bugCaptureBurst.take();bugBurst=bool(burstCapture);}
        // F8: pictures of both eyes where the player stands (EVR_CAPTURE_KEY=1; for Mock sessions that the user moves by
        // the keyboard - 2026-10-14: "I move with the keyboard; take the fog and brightness differences"). A beep says
        // the key was taken; on a frame of one eye the picture comes with the next frame of both.
        {static const bool keyWanted=[]{const auto f=std::getenv("EVR_CAPTURE_KEY");return f&&!std::strcmp(f,"1");}();static bool keyBefore=false;
         if(keyWanted){const bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
            if(down&&!keyBefore){if(both&&!fixtureCapture)fixtureCapture=9;else nativeFixtureCapture.store(9);MessageBeep(MB_OK);
                state.log->event("manual_key_capture","\"frame\":"+std::to_string(frame)+",\"key\":\"F8\"");}
            keyBefore=down;}}
        // Manual one-shot capture (2026-10-06): <output>/xr-capture.request -> eyes + UI of this pair, file removed.
        // Checked every 15th pair only, to keep the per-frame cost negligible.
        if(both&&!fixtureCapture&&state.submittedPairs%15==0){std::error_code ec;const auto request=std::filesystem::path(output)/"xr-capture.request";
            if(std::filesystem::exists(request,ec)&&std::filesystem::remove(request,ec)){fixtureCapture=9;state.log->event("manual_xr_capture","\"frame\":"+std::to_string(frame));}}
        // Manual burst (2026-10-09, flicker detection): <output>/xr-burst.request holding N (1..32, default 8) captures the
        // next N consecutive pairs. Optional second field is an explicit session request limit (1..4, default 1).
        if(both&&!fixtureCapture&&state.submittedPairs%15==0){std::error_code ec;const auto request=std::filesystem::path(output)/"xr-burst.request";
            if(std::filesystem::exists(request,ec)){uint32_t count=8,limit=1;{std::ifstream in(request);uint32_t n=0;if(in>>n){count=n;if(in>>n)limit=n;}}
                if(std::filesystem::remove(request,ec)){const int r=nativeCaptureBurst.request(count,limit);
                    state.log->event("manual_xr_burst","\"frame\":"+std::to_string(frame)+",\"count\":"+std::to_string(count)+",\"request_limit\":"+std::to_string(limit)+",\"accepted\":"+(r==0?"true":"false"));}}}
        // UI statistics only (2026-10-11, headset feedback "the UI blinks at times"): <output>/xr-ui-stat.request holding N
        // (1..3000) records the UI coverage of the next N pairs (native_ui_burst_stat), without eye images. Those pairs complete
        // synchronously, as captures do.
        static uint32_t uiStatRemaining=0;
        if(!fixtureCapture&&!uiStatRemaining&&state.submittedPairs%15==0){std::error_code ec;const auto request=std::filesystem::path(output)/"xr-ui-stat.request";
            if(std::filesystem::exists(request,ec)){uint32_t count=300;{std::ifstream in(request);uint32_t n=0;if(in>>n)count=n;}
                if(std::filesystem::remove(request,ec)){uiStatRemaining=std::clamp<uint32_t>(count,1,3000);
                    state.log->event("manual_ui_stat","\"frame\":"+std::to_string(frame)+",\"count\":"+std::to_string(uiStatRemaining));}}}
        const bool uiStatFrame=uiStatRemaining>0;if(uiStatFrame)--uiStatRemaining;
        // One manual raw diagnostic request, Mock navigation sessions only. V1 captures one pair;
        // V2 explicitly requests 2..4 consecutive pairs, each still bounded to 16 images / 256 MiB.
        std::vector<size_t> dumpImages;
        static bool rawDumpConsumed=false;static uint32_t rawDumpRemaining=0,rawDumpCount=0;static std::vector<size_t> rawDumpSelection;
        if(both&&!rawDumpConsumed&&diagnosticReadback::sweepRequested()&&std::getenv("EVR_MOCK_NAV_FILE")&&state.submittedPairs>30&&state.submittedPairs%15==0){
            std::error_code ec;const auto request=std::filesystem::path(output)/"eye-dump.request";
            if(std::filesystem::exists(request,ec)){
                rawDumpConsumed=true;std::ifstream in(request);std::string magic;size_t index=0;uint32_t count=1;in>>magic;
                const bool batch=magic=="EVR_EYE_IMAGE_DUMP_2";if(batch)in>>count;
                while(in>>index){dumpImages.push_back(index);if(dumpImages.size()>16)break;}
                const bool valid=(magic=="EVR_EYE_IMAGE_DUMP_1"||(batch&&count>=2&&count<=4))&&in.eof()&&!dumpImages.empty()&&dumpImages.size()<=16;
                in.close();std::filesystem::rename(request,std::filesystem::path(output)/("eye-dump-request-"+std::to_string(frame)+".txt"),ec);
                if(!valid||ec)dumpImages.clear();
                if(!dumpImages.empty()){rawDumpSelection=dumpImages;rawDumpCount=rawDumpRemaining=count;}
                state.log->event("manual_eye_dump","\"frame\":"+std::to_string(frame)+",\"accepted\":"+(dumpImages.empty()?"false":"true"));
            }
        }
        if(both&&rawDumpRemaining){dumpImages=rawDumpSelection;--rawDumpRemaining;fixtureCapture=10;}
        const auto pairNumber=state.submittedPairs+1;
        // A manual burst also reads the UI back, as statistics only (2026-10-11, headset feedback "the UI blinks at times"):
        // coverage per frame and per screen cell shows whether, where and in which stage (game image or XR copy) it drops out.
        const bool uiBurst=(bool(burstCapture)||uiStatFrame)&&!fixtureCapture;
        // EVR_PLAY_LOG=1 (the start by itself, for play): no evidence pictures of the first, middle and last pair.
        static const bool playLog=[]{const auto f=std::getenv("EVR_PLAY_LOG");return f&&!std::strcmp(f,"1");}();
        const bool evidencePair=both&&!playLog&&(pairNumber==1||pairNumber==state.frameLimit/2||pairNumber==state.frameLimit);
        const bool uiReadback=fixtureCapture||uiBurst||evidencePair;
        std::array<TransferImage,2> src{},dst{};std::array<std::string,2> captures;
        for(size_t eye=0;eye<2;++eye){
            if(!(eyes>>eye&1))continue;   // not drawn in this frame: the headset keeps its last picture
            diagnostic::NativeImageEvidence evidence;
            if(evrGetNativeImageEvidence(state.device,images[eye],&evidence)||!evidence.ready)
                throw std::runtime_error("native eye image layout/submission unproven");
            src[eye]={images[eye],evidence.width,evidence.height,evidence.format,evidence.layout,evidence.usage,evidence.family};
            const auto &target=xr.targets[eye];
            if(asyncFrame)dst[eye]=slotFor(state,uint32_t(eye),target.width,target.height);
            else dst[eye]={target.images[snapshot.imageIndices[eye]].image,target.width,target.height,xr.format,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,xr.vk.family};
            if(fixtureCapture||burstCapture||evidencePair)
                captures[eye]=output+"/native-xr/frame-"+std::to_string(frame)+"-eye-"+std::to_string(eye)+".ppm";
            state.log->event("native_eye_source","\"frame\":"+std::to_string(frame)+",\"eye\":"+std::to_string(eye)+
                ",\"image\":"+handleText(images[eye])+",\"generation\":"+std::to_string(evidence.generation)+
                ",\"layout\":"+std::to_string(evidence.layout)+",\"family\":"+std::to_string(evidence.family)+
                ",\"usage\":"+std::to_string(src[eye].usage)+",\"format\":"+std::to_string(src[eye].format)+
                ",\"width\":"+std::to_string(src[eye].width)+",\"height\":"+std::to_string(src[eye].height)+
                ",\"submission\":"+std::to_string(evidence.submission)+",\"xr_image\":"+handleText(dst[eye].image));
        }
        if(uiDraw::enabled())state.log->event("native_ui_frame_evidence","\"frame\":"+std::to_string(frame)+","+uiDraw::frameEvidence(state.device,state.beginPresent,gameQueue));
        uiDraw::FrameSource uiSource;
        const auto uiFlag=std::getenv("EVR_UI_QUAD");
        bool submitUi=uiFlag && !std::strcmp(uiFlag,"1") && !state.uiDisabled && uiDraw::sourceForFrame(state.device,state.beginPresent,gameQueue,uiSource);
        if(submitUi){
            // Resource/format setup can fail before acquisition without poisoning the world frame.
            try{
                if(state.uiTransfer&&(xr.uiSwapchain().width!=uiSource.width || xr.uiSwapchain().height!=uiSource.height)){
                    // The desktop may be reading this transfer's picture (desktop UI): no new blit of it, and those under way end first.
                    if(state.uiTransfer->desktopImage()){desktopMirrorSource(state.device,VK_NULL_HANDLE);vkCheck(xr.vk.vkDeviceWaitIdle(xr.vk.device),"desktop UI picture retired");}
                    state.uiTransfer.reset();
                }
                xr.configureUi(uiSource.width,uiSource.height,VK_FORMAT_R8G8B8A8_SRGB);
                if(!state.uiTransfer)state.uiTransfer=std::make_unique<UiTransfer>(xr.vk,xr.uiSwapchain(),xr.uiSwapchainFormat());
            }catch(const std::exception& e){state.uiDisabled=true;submitUi=false;state.log->event("native_ui_disabled","\"reason\":"+jsonString(e.what()));}
        }
        if(uiFlag && !std::strcmp(uiFlag,"1") && !submitUi)
            state.log->event("native_ui_omitted","\"frame\":"+std::to_string(frame)+",\"begin_present\":"+std::to_string(state.beginPresent)+",\"reason\":\"unproven_frame_or_disabled\"");
        validateStereoTransfer(src,dst,xr.vk.family,eyes);
        const auto evidenceComplete=std::chrono::steady_clock::now();
        // Capture/readback frames keep the strict path. Async frames rely on the eye writer's
        // family == binding family with a single queue, plus the copy's ALL_COMMANDS barriers.
        const bool async=state.asyncCompletion&&captures[0].empty()&&captures[1].empty()&&!uiReadback;
        // Opt-in black-frame diagnostic (EVR_EYE_LUMA_EVERY=N): read back both XR eye images every Nth
        // pair without a device idle, so the measured path is the ordinary (async) submission path.
        const auto lumaFlag=std::getenv("EVR_EYE_LUMA_EVERY");
        const unsigned lumaEvery=lumaFlag?unsigned(std::strtoul(lumaFlag,nullptr,10)):0u;
        const bool lumaFrame=eyes==3&&lumaEvery&&captures[0].empty()&&captures[1].empty()&&pairNumber%lumaEvery==0;
        // Diagnostic synchronization only. This does not flush/retire native allocators.
        if(!async)vkCheck(xr.vk.vkDeviceWaitIdle(xr.vk.device),"native game pair GPU completion");
        const auto idleComplete=std::chrono::steady_clock::now();
        auto copyDone=idleComplete,uiDone=idleComplete;   // where a capture's time goes (2026-10-22: 130 of 160 ms were not the eyes')
        if(queueTraceBudget.load())note("native_copy_begin","\"frame\":"+std::to_string(frame)+",\"pair\":"+std::to_string(pairNumber)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"last_present\":"+std::to_string(presents.load()));
        // The bug report's history: a save asked for (evrSaveEyeHistory: the button pressed, the report's window ended) is
        // written before this pair; a picture of this pair goes into the ring if one is due.
        if(state.history&&!state.historyFailed){
            // two slots a frame (about ten frames for the twenty), the frame held some milliseconds each instead of 90..236 ms once
            static uint64_t savingAsked=0,savingFrame=0;static double savingMs=0;
            const auto asked=eyeHistoryAsked.load();
            if(state.history->saving||asked!=eyeHistoryDone.load())try{
                const auto start=std::chrono::steady_clock::now();
                state.transfer->finish();
                if(!state.history->saving){std::error_code ec;std::filesystem::create_directories(std::filesystem::path(output)/"native-xr",ec);
                    state.history->startSave(output+"/native-xr");savingAsked=asked;savingFrame=frame;savingMs=0;}
                const bool done=state.history->step(xr.vk.queue,2);
                savingMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(done){eyeHistoryDone=savingAsked;const auto& frames=state.history->savedFrames();
                    std::string list;for(const auto f:frames)list+=(list.empty()?"":",")+std::to_string(f);
                    state.log->event("eye_history_saved","\"frame\":"+std::to_string(frame)+",\"asked_at_frame\":"+std::to_string(savingFrame)+",\"pairs\":"+std::to_string(frames.size())+",\"frames\":["+list+"],\"ms\":"+
                        std::to_string(int(savingMs))+",\"spread_over_frames\":"+std::to_string(frame-savingFrame+1));}
            }catch(const std::exception& e){state.historyFailed=true;eyeHistoryDone=eyeHistoryAsked.load();state.log->event("eye_history_failed","\"stage\":\"save\",\"reason\":"+jsonString(e.what()));}
            state.history->begin(frame,GetTickCount64(),eyes==3);
        }
        // The headset-view fit ([Picture] FitView, include/evr/fov_fit.hpp; ALVR): the part of each picture the runtime's own
        // field of view covers, taken now that the game has drawn them; the frame's field of view becomes the runtime's.
        {std::array<evr::fovfit::Rect,2> parts{};std::array<StereoTransfer::Fit,2> fit{};
         if(xr.takeRuntimeFovFit(parts,eyes))for(size_t eye=0;eye<2;++eye)if((eyes>>eye&1)&&parts[eye].valid())fit[eye]={true,parts[eye].x0,parts[eye].y0,parts[eye].x1,parts[eye].y1};
         state.transfer->fit=fit;}
        const auto fitFailuresBefore=state.transfer->fitFailures;
        state.transfer->copy(src,dst,captures,!async,lumaFrame,eyes);
        if(state.transfer->fitFailures!=fitFailuresBefore){static unsigned fitFailureLogs=0;
            if(fitFailureLogs++<20)state.log->event("runtime_fov_fit_failed","\"frame\":"+std::to_string(frame)+",\"failures\":"+std::to_string(state.transfer->fitFailures)+",\"plain_copy\":true");}
        if(state.history)state.history->end();
        copyDone=std::chrono::steady_clock::now();uiDone=copyDone;
        // Desktop UI (2026-10-13, user: "put the UI on the desktop side, for streaming"): with the UI of this frame the
        // desktop's picture - the right eye's copy with the UI over it - is made by the UI's draw below and the desktop
        // shows that; until then it keeps the picture it has. Without a UI in this frame, with a right eye of another
        // format, or with EVR_DESKTOP_UI=0 the desktop shows the right eye alone, as before.
        const auto mirrorCopy=state.transfer->mirrorView();
        static const bool desktopUiWanted=[]{const auto f=std::getenv("EVR_DESKTOP_UI");return !f||std::strcmp(f,"0");}();
        const bool desktopUi=desktopUiWanted&&submitUi&&mirrorCopy.image&&mirrorCopy.format==VK_FORMAT_R8G8B8A8_UNORM;
        // If the UI's draw does not come to it (an exception on the way), the desktop falls back to the right eye alone.
        struct DesktopFallback {bool armed;VkDevice device;VkImage image;uint32_t width,height;
            ~DesktopFallback(){if(armed)desktopMirrorOwnedSource(device,image,width,height);}} desktopFallback{desktopUi,state.device,mirrorCopy.image,mirrorCopy.width,mirrorCopy.height};
        if(!desktopUi){
            if(mirrorCopy.image)desktopMirrorOwnedSource(state.device,mirrorCopy.image,mirrorCopy.width,mirrorCopy.height);
            else desktopMirrorSource(state.device,images[1]);
        }
        if(lumaFrame){const auto& l=state.transfer->cpuTiming().luma;std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<frame<<",\"pair\":"<<pairNumber
            <<",\"luma\":["<<l[0]<<','<<l[1]<<"],\"async_path\":"<<(async?"true":"false")<<",\"every\":"<<lumaEvery;
            // Registered diagnostic images (DLSS inputs/outputs) read back in the same frame boundary.
            for(size_t slot=0;slot<diagnosticReadback::slots.size();++slot){
                const auto d=diagnosticReadback::slots[slot];if(!d.image)continue;
                const auto r=diagnosticReadback::read(xr.vk,d.image,d.device);
                o<<",\"slot"<<slot<<"\":{\"image\":"<<handleText(d.image)<<",\"ok\":"<<(r.ok?"true":"false")<<",\"mean\":"<<r.mean<<",\"non_finite\":"<<r.nonFinite
                 <<",\"layout\":"<<r.layout<<",\"w\":"<<r.width<<",\"h\":"<<r.height<<",\"format\":"<<r.format<<",\"error\":"<<jsonString(r.error)<<'}';
            }
            // Aliasing evidence once per session (bindings are recorded at vkBindImageMemory*).
            static unsigned aliasReports=0;
            if(aliasReports++<3){
                std::string fields;{std::lock_guard<std::mutex> lock(mutex);
                    fields="\"frame\":"+std::to_string(frame)+",\"eye0\":{\"image\":"+handleText(src[0].image)+","+imageAliasReportLocked(src[0].image)+"},\"eye1\":{\"image\":"+handleText(src[1].image)+","+imageAliasReportLocked(src[1].image)+'}';
                    for(size_t slot=0;slot<diagnosticReadback::slots.size();++slot)if(diagnosticReadback::slots[slot].image)
                        fields+=",\"slot"+std::to_string(slot)+"\":{\"image\":"+handleText(diagnosticReadback::slots[slot].image)+","+imageAliasReportLocked(diagnosticReadback::slots[slot].image)+'}';}
                state.log->event("native_eye_alias_report",fields);
            }
            state.log->event("native_eye_luma",o.str());
            if(diagnosticReadback::sweepRequested()){
                static bool inventoryLogged=false;
                const auto sweep=diagnosticReadback::readSweep(xr.vk,!inventoryLogged);
                if(!inventoryLogged){inventoryLogged=true;state.log->event("native_eye_sweep_inventory","\"frame\":"+std::to_string(frame)+",\"images\":["+sweep.inventory+"]");}
                state.log->event("native_eye_sweep","\"frame\":"+std::to_string(frame)+",\"pair\":"+std::to_string(pairNumber)+","+sweep.frame);
            }}
        if(!dumpImages.empty()){
            const auto dir=std::filesystem::path(output)/"native-xr"/("eye-dump-"+std::to_string(frame));
            const auto dump=diagnosticReadback::readSweep(xr.vk,true,dir,dumpImages);
            const std::string fields="\"frame\":"+std::to_string(frame)+",\"pair\":"+std::to_string(pairNumber)+",\"dump_index\":"+std::to_string(rawDumpCount-rawDumpRemaining)+",\"dump_count\":"+std::to_string(rawDumpCount)+",\"images\":["+dump.inventory+"],"+dump.frame;
            state.log->event("native_eye_dump",fields);
            std::filesystem::create_directories(dir);std::ofstream manifest(dir/"manifest.json");manifest<<'{'<<fields<<'}'<<std::endl;
        }
        // Depth-op window for the next pair (recorded before its evrFinishNativeXr): luma pairs only.
        if(diagnosticReadback::sweepRequested()){const bool next=lumaEvery&&(pairNumber+1)%lumaEvery==0;
            diagnosticReadback::depthLogPair=next?pairNumber+1:0;diagnosticReadback::depthLogWindow=next;}
        gpuTimingPair(state.device,pairNumber,*state.log);
        if(submitUi){
            const auto uiIndex=xr.acquireUi(frame);
            // Motion burst needs eye images; UI remains GPU-submitted each frame. A bug report's pictures take no read back of the UI's
            // XR and desktop copies (2026-10-22: the UI's part held a picture's frame 55..121 ms; only its one UI picture is kept, below).
            const bool capture=uiReadback&&!bugBurst;
            UiFade uiFade;bool uiProbe=false;
            if(const int requested=uiHudMode.load()){
                // The look at the previous frame's UI decides this frame's opacity (one frame late).
                const auto now=GetTickCount64();const auto at=uiHudAt.load();
                const auto mode=requested==1&&now>=at&&now-at<=1000?HudMode::Gameplay:HudMode::Forced;
                state.uiTransfer->finishPrevious();HudChange change;
                if(state.uiTransfer->probes()!=state.hudProbeSeen&&!state.uiTransfer->probe().empty()){
                    state.hudProbeSeen=state.uiTransfer->probes();
                    change=state.hudChanges.update(state.uiTransfer->probe().data(),state.uiTransfer->probeCellsWidth(),state.uiTransfer->probeCellsHeight(),now);
                }
                const float before=state.hudFade.opacity();uiFade.outside=state.hudFade.update(now,mode,change.changed);
                const bool compassKept=uiCompassKept.load()&&now>=uiCompassAt.load()&&now-uiCompassAt.load()<=3000;
                const auto pins=state.hudChanges.layout().kept(compassKept);const auto& wide=state.hudChanges.layout().pinsWide;const float sx=uiSource.width/1920.f,sy=uiSource.height/1080.f;
                {static int told=-1;if(told!=int(compassKept)){told=int(compassKept);state.log->event("native_ui_compass","\"frame\":"+std::to_string(frame)+",\"kept_with_the_pins\":"+(compassKept?"true":"false"));}}
                uiFade.keep={pins.x0*sx,pins.y0*sy,pins.x1*sx,pins.y1*sy};uiFade.keepWide={wide.x0*sx,wide.y0*sy,wide.x1*sx,wide.y1*sy};uiProbe=true;
                const bool hidden=uiFade.outside<=0;
                if((int(mode)!=state.hudLoggedMode||hidden!=state.hudLoggedHidden||(change.changed&&before<1))&&state.hudLogs<2000){
                    ++state.hudLogs;state.hudLoggedMode=int(mode);state.hudLoggedHidden=hidden;
                    state.log->event("native_ui_hud","\"frame\":"+std::to_string(frame)+",\"mode\":"+std::to_string(int(mode))+",\"opacity\":"+std::to_string(uiFade.outside)+",\"hidden\":"+(hidden?"true":"false")+
                        ",\"changed\":"+(change.changed?"true":"false")+",\"vital_cells\":"+std::to_string(change.vitalCells)+",\"other_cells\":"+std::to_string(change.otherCells)+
                        ",\"ticker_cells\":"+std::to_string(change.tickerCells)+",\"fading_cells\":"+std::to_string(change.fadingCells)+",\"ignored_cells\":"+std::to_string(change.ignoredCells)+
                        ",\"other_box\":["+std::to_string(change.otherX0)+','+std::to_string(change.otherY0)+','+std::to_string(change.otherX1)+','+std::to_string(change.otherY1)+"]"+
                        ",\"cells\":["+std::to_string(state.uiTransfer->probeCellsWidth())+','+std::to_string(state.uiTransfer->probeCellsHeight())+"],\"hold_ms\":5000");
                }
                // What keeps a shown HUD shown, once a second at the most (2026-10-18; headset report 12: "the UI does not go
                // away" - the record above leaves out changes while it is fully shown, so the reason was not on record).
                {static uint64_t keptAt=0;static unsigned keptLogs=0;
                 if(change.changed&&!hidden&&before>=1&&now>=keptAt+1000&&keptLogs<3600){keptAt=now;++keptLogs;
                    state.log->event("native_ui_hud_kept","\"frame\":"+std::to_string(frame)+",\"vital_cells\":"+std::to_string(change.vitalCells)+",\"other_cells\":"+std::to_string(change.otherCells)+
                        ",\"ticker_cells\":"+std::to_string(change.tickerCells)+",\"other_box\":["+std::to_string(change.otherX0)+','+std::to_string(change.otherY0)+','+std::to_string(change.otherX1)+','+std::to_string(change.otherY1)+"]"+
                        ",\"compass_kept\":"+(compassKept?"true":"false"));}}
            }else if(state.hudLoggedMode!=0){state.hudLoggedMode=0;state.hudFade.update(GetTickCount64(),HudMode::Off,false);state.hudChanges.reset();}
            if(const int shown=uiCursorShown.load()){
                const auto now=GetTickCount64();const auto at=uiCursorAt.load();const auto place=uiCursorPlace.load();
                if(now>=at&&now-at<=250){const float size=uiSource.height/1080.f;
                    uiFade.cursor={float(place>>16)/65535.f*uiSource.width,float(place&0xffff)/65535.f*uiSource.height,(shown==2?9.f:6.f)*size,float(shown)};}
            }
            {   // the mod's own picture over the plate (evrSetUiOverlay)
                std::lock_guard<std::mutex> l(uiOverlayLock);
                if(uiOverlaySerialNow!=state.uiOverlaySerial&&uiOverlayWidth){
                    state.uiTransfer->setOverlay(uiOverlayBytes,uiOverlayWidth,uiOverlayHeight);state.uiOverlaySerial=uiOverlaySerialNow;
                    if(state.uiOverlayLogs++<40)state.log->event("native_ui_overlay","\"frame\":"+std::to_string(frame)+",\"picture\":["+std::to_string(uiOverlayWidth)+','+std::to_string(uiOverlayHeight)+"],\"serial\":"+std::to_string(uiOverlaySerialNow));
                }
                const auto now=GetTickCount64();
                if(uiOverlayWidth&&uiOverlayRect[2]>uiOverlayRect[0]&&now>=uiOverlayAt&&now-uiOverlayAt<=1000){
                    bool empty=true;
                    if(uiOverlayOnlyEmpty.load()){
                        // the game's UI under the place, from the small copy of the last draws (cells of about 16 pixels)
                        uiProbe=true;const auto& cells=state.uiTransfer->probe();const auto cw=state.uiTransfer->probeCellsWidth(),ch=state.uiTransfer->probeCellsHeight();
                        if(cells.empty()||!cw||!ch){
                            // 2026-10-17 evening (headset: "the bug report button is gone"; shown in both Mock starts): without the
                            // small copy of the UI the button (mode 2) is shown rather than hidden - at worst over the settings page
                            empty=uiOverlayOnlyEmpty.load()==2;
                            static unsigned noProbe=0;if(noProbe++<8)state.log->event("native_ui_overlay_decision","\"frame\":"+std::to_string(frame)+
                                ",\"shown\":"+(empty?"true":"false")+",\"probe\":\"none\",\"mode\":"+std::to_string(uiOverlayOnlyEmpty.load()));}
                        else{const auto x0=uint32_t(uiOverlayRect[0]*cw),x1=std::min(cw,uint32_t(uiOverlayRect[2]*cw)+1),y0=uint32_t(uiOverlayRect[1]*ch),y1=std::min(ch,uint32_t(uiOverlayRect[3]*ch)+1);
                            // "nothing drawn" = even: the start menu lays a dark veil of half cover over the whole picture (game-20261015-192035:
                            // alpha 128 everywhere on its right half); text, frames and pictures make the cells differ
                            if(uiOverlayOnlyEmpty.load()==2){
                                // 2 (2026-10-17, the bug report's button; user: "hide it once the settings are open"): hidden only when the
                                // game covers more than a fifth of the place - the settings page reaches over about 40 % of it (Mock
                                // game-20261017-025851, frame 1061), the start menu's veil none; something small (user 2026-10-15: "show it
                                // even over other things") keeps it shown. A cell counts as covered when a channel is 18 off the place's median.
                                std::array<std::vector<uint8_t>,4> channel;
                                for(uint32_t y=y0;y<y1;++y)for(uint32_t x=x0;x<x1;++x)for(unsigned c=0;c<4;++c)channel[c].push_back(cells[(size_t(y)*cw+x)*4+c]);
                                std::array<uint8_t,4> median{};
                                for(unsigned c=0;c<4;++c){auto v=channel[c];std::nth_element(v.begin(),v.begin()+v.size()/2,v.end());median[c]=v.empty()?0:v[v.size()/2];}
                                size_t covered=0;const size_t total=channel[0].size();
                                // 2026-10-18 (headset: the button still missing; record: covered 665 of 2204, median alpha 0): the start
                                // menu's veil on the headset is not even (it fades across the place), so differing from the median no
                                // longer tells. What the settings page has and the veil has not is edges - text, rows and frames: a cell
                                // counts when a channel is 18 off its right or lower neighbour (Mock game-20261018-030005: the start menu
                                // 0 %, the settings page 15 % of the place, its panels only half opaque); more than 5 % hides the button.
                                for(uint32_t y=y0;y<y1;++y)for(uint32_t x=x0;x<x1;++x){
                                    auto at=[&](uint32_t cx,uint32_t cy,unsigned c){return int(cells[(size_t(std::min(cy,ch-1))*cw+std::min(cx,cw-1))*4+c]);};
                                    bool edge=false;for(unsigned c=0;c<4&&!edge;++c){const int v=at(x,y,c);if(std::abs(v-at(x+1,y,c))>18||std::abs(v-at(x,y+1,c))>18)edge=true;}
                                    if(edge)++covered;}
                                empty=total&&covered*20<=total;
                                // 2026-10-17 evening (user: "the bug report button is gone"): each change of the decision on record
                                {static int lastShown=-1;static unsigned changes=0;
                                 if(int(empty)!=lastShown&&changes<40){lastShown=int(empty);++changes;
                                    state.log->event("native_ui_overlay_decision","\"frame\":"+std::to_string(frame)+",\"shown\":"+(empty?"true":"false")+
                                        ",\"covered\":"+std::to_string(covered)+",\"cells\":"+std::to_string(total)+",\"median\":["+std::to_string(median[0])+','+
                                        std::to_string(median[1])+','+std::to_string(median[2])+','+std::to_string(median[3])+"]");}}
                            }else{
                            uint8_t lo[4]{255,255,255,255},hi[4]{0,0,0,0};
                            for(uint32_t y=y0;y<y1;++y)for(uint32_t x=x0;x<x1;++x)for(unsigned c=0;c<4;++c){const auto v=cells[(size_t(y)*cw+x)*4+c];lo[c]=std::min(lo[c],v);hi[c]=std::max(hi[c],v);}
                            for(unsigned c=0;c<4;++c)if(hi[c]-lo[c]>18)empty=false;}}
                    }
                    if(empty){uiFade.overlay={uiOverlayRect[0]*uiSource.width,uiOverlayRect[1]*uiSource.height,uiOverlayRect[2]*uiSource.width,uiOverlayRect[3]*uiSource.height};uiOverlayDrawnAt=now;}
                }
            }
            Vec3 aimedPoint{};const bool aiming=aimMarkNow(aimedPoint);
            // The cluster needs the projection and five quads; a runtime that takes fewer layers keeps the HUD whole.
            // (2026-10-26 11:40-14:00 none while the mod drew its spell list; back since the game's own stamina ring follows the
            // reticle again - user: "as the old reticle-following way".)
            const bool cluster=aiming&&aimCluster()&&xr.maxLayers()>=6&&!(uiPlateFixed.load()&&GetTickCount64()-uiPlateAt.load()<=2000);
            if(aiming&&!cluster&&aimMarkHole()>0)uiFade.hole={uiSource.width*.5f,uiSource.height*.5f,aimMarkHole()*uiSource.height/1080.f,0};
            const TransferImage behindUi{mirrorCopy.image,mirrorCopy.width,mirrorCopy.height,mirrorCopy.format,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,xr.vk.family};
            const auto bytes=state.uiTransfer->draw({uiSource.image,uiSource.width,uiSource.height,VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,xr.vk.family},uiIndex,capture,!async,uiFade,uiProbe,desktopUi?&behindUi:nullptr);
            if(const auto desk=state.uiTransfer->desktopView();desk.image){
                desktopFallback.armed=false;desktopMirrorOwnedSource(state.device,desk.image,desk.width,desk.height);
                const auto n=++state.desktopUiFrames;
                if(n<=3||n%600==0){const auto crop=centreCrop(mirrorCopy.width,mirrorCopy.height,desk.width,desk.height);
                    state.log->event("native_desktop_ui","\"frame\":"+std::to_string(frame)+",\"frames\":"+std::to_string(n)+",\"picture\":"+handleText(desk.image)+
                        ",\"size\":["+std::to_string(desk.width)+','+std::to_string(desk.height)+"],\"behind\":"+handleText(mirrorCopy.image)+",\"behind_size\":["+std::to_string(mirrorCopy.width)+','+std::to_string(mirrorCopy.height)+
                        "],\"crop\":["+std::to_string(crop.x0)+','+std::to_string(crop.y0)+','+std::to_string(crop.x1)+','+std::to_string(crop.y1)+"],\"outside\":"+std::to_string(uiFade.outside));}
            }else if(desktopFallback.armed){desktopFallback.armed=false;desktopMirrorOwnedSource(state.device,mirrorCopy.image,mirrorCopy.width,mirrorCopy.height);}
            UiQuadPlacement placement;
            if(uiPlateFixed.load()){
                const auto now=GetTickCount64();const auto at=uiPlateAt.load();
                if(now>=at&&now-at<=2000){std::lock_guard<std::mutex> l(uiPlateLock);placement.localSpace=true;placement.origin=uiPlateOrigin;}
            }
            if(placement.localSpace!=state.uiPlateLogged&&state.uiPlateLogs<200){
                ++state.uiPlateLogs;state.uiPlateLogged=placement.localSpace;std::ostringstream o;o<<std::setprecision(7)<<"\"frame\":"<<frame<<",\"fixed\":"<<(placement.localSpace?"true":"false")
                    <<",\"origin_position\":["<<placement.origin.position.x<<','<<placement.origin.position.y<<','<<placement.origin.position.z<<"],\"origin_orientation\":["<<placement.origin.orientation.x<<','<<placement.origin.orientation.y<<','<<placement.origin.orientation.z<<','<<placement.origin.orientation.w
                    <<"],\"distance_m\":"<<placement.distance<<",\"space\":"<<(placement.localSpace?"\"LOCAL\"":"\"VIEW\"");
                state.log->event("native_ui_plate",o.str());
            }
            // The locked enemy's health bar and lock-on arrow: where the game's camera put them into the HUD image (the
            // anchor pixel), a part 380 by 180 (of 1080 rows) about it, the anchor 50 rows above its bottom - the bar and
            // the level badge stand above the arrow (bug report 20261019-141055, ui-031117: badge 341, bar 353, arrow 372).
            std::array<float,7> hud{};const bool hudKnown=targetHudNow(hud);bool targetHud=hudKnown&&!(uiPlateFixed.load()==1&&GetTickCount64()-uiPlateAt.load()<=2000);
            const bool playPlate=placement.localSpace&&uiPlateFixed.load()==2;   // the third-person pad style's plate ahead of the camera
            // Why the bar is or is not taken out (2026-10-19 18:45, user: "the health bar shows in the UI when the enemy leaves
            // my view"): 0 no target from the hooks, 1 the plate fixed in the room (a menu), 2 the anchor off the image,
            // 3 the plate in the room's space, 4 no part (too near, or overlapping the cluster), 5 moved. Logged on each change.
            int hudState=!hudKnown?0:!targetHud?1:5;
            aim::PixelRect hudCut{};float hudX=0,hudY=0;
            if(targetHud){
                const float w=float(uiSource.width),h=float(uiSource.height),aspect=w/h,tanHalf=hud[5];const bool vertical=hud[6]>.5f;
                hudX=w*.5f*(1+hud[3]/(vertical?tanHalf*aspect:tanHalf));hudY=h*.5f*(1-hud[4]/(vertical?tanHalf:tanHalf/aspect));
                const float s=h/1080.f;
                // 380 by 180 since 2026-10-19 18:20 (user after the headset run of 17:52: "the health bar sometimes goes out of
                // the area and is cut"): wider and taller round the anchor, and near the image's edge the part is the image's
                // side of it (before, a part over the edge was not taken out at all and the bar went back to the HUD).
                hudCut={int32_t(hudX-190*s),int32_t(hudY-130*s),int32_t(hudX+190*s),int32_t(hudY+50*s)};
                // The anchor may be off the image while the bar, beside and above it, still shows at the edge (2026-10-19 19:25,
                // user: "the bar stays in the UI only when it sticks out of the cut"; headset run 20261019-191819: every frame
                // the part was not taken out had the anchor just off the image - x 1958, 1987, y -36, 1140): the part is taken
                // out as long as any of it is on the image.
                targetHud=std::isfinite(hudX)&&std::isfinite(hudY)&&hudCut.x1>1&&hudCut.y1>1&&hudCut.x0<int32_t(uiSource.width)-1&&hudCut.y0<int32_t(uiSource.height)-1;if(!targetHud)hudState=2;
                if(targetHud){hudCut.x0=std::max(hudCut.x0,int32_t(1));hudCut.y0=std::max(hudCut.y0,int32_t(1));
                    hudCut.x1=std::min(hudCut.x1,int32_t(uiSource.width)-1);hudCut.y1=std::min(hudCut.y1,int32_t(uiSource.height)-1);
                    // Not the HUD's fixed bands (2026-10-26, user after the headset run 20261026-030628: "something cuts the UI out up to
                    // above the quick slot, and the quick slot flickers a little" - target parts such as 452,879 380x180 took in the action
                    // bar whenever an enemy stood low in the view, and the hole came and went with the enemies): the top band (health,
                    // mana, stamina, compass, quest) and the bottom one (the held item's name and hints, the action bar) stay on the plate.
                    hudCut.y0=std::max(hudCut.y0,int32_t(100*s));hudCut.y1=std::min(hudCut.y1,int32_t(878*s));
                    targetHud=hudCut.x1-hudCut.x0>=int32_t(40*s)&&hudCut.y1-hudCut.y0>=int32_t(40*s);}
            }
            if((cluster||targetHud)&&(!placement.localSpace||playPlate)){
                // The plate ahead of the head as the parts around the cluster (and the health bar), and those at their points.
                std::array<aim::PixelRect,2> cuts{};std::array<aim::QuadPart,2> moved{};size_t count=0;
                // The locked enemy's widget stands near the middle of the game's lock camera, so it nearly always overlaps the
                // cluster's rows (headset run 20261019-144734: anchor rows 430, the cluster 420..660 - the plate fell back to
                // the cluster alone and the health bar went with it to the aimed point). The cluster gives up the rows on the
                // widget's side; with less than 80 rows (of 1080) left it is not taken out this frame.
                auto clusterCut=aim::clusterRect(uiSource.width,uiSource.height);bool clusterFits=true;
                if(cluster&&targetHud&&clusterCut.x0<hudCut.x1&&hudCut.x0<clusterCut.x1&&clusterCut.y0<hudCut.y1&&hudCut.y0<clusterCut.y1){
                    if(hudCut.y0+hudCut.y1<clusterCut.y0+clusterCut.y1)clusterCut.y0=hudCut.y1;else clusterCut.y1=hudCut.y0;
                    clusterFits=clusterCut.y1-clusterCut.y0>=int32_t(80.f*float(uiSource.height)/1080.f);
                }
                if(cluster&&clusterFits){const auto cut=clusterCut;
                    const auto at=aim::clusterAt(cut,uiSource.width,uiSource.height,placement.width,placement.distance,xr.headLocation.pose.position,aimedPoint);
                    if(at.width>0){cuts[count]=cut;moved[count++]=at;}}
                if(targetHud){const Vec3 anchor{hud[0],hud[1],hud[2]};
                    const auto at=aim::partAt(hudCut,hudX,hudY,uiSource.width,uiSource.height,placement.width,placement.distance,xr.headLocation.pose.position,anchor);
                    if(at.width>0){cuts[count]=hudCut;moved[count++]=at;}else hudState=4;}
                std::array<aim::QuadPart,10> pieces{};size_t n=0;
                if(count&&!aim::plateWithout(cuts.data(),count,uiSource.width,uiSource.height,placement.width,pieces,n)&&count==2){   // overlapping: the first alone
                    count=1;aim::plateWithout(cuts.data(),count,uiSource.width,uiSource.height,placement.width,pieces,n);if(targetHud&&cluster)hudState=4;}
                if(count&&n&&n+count<=placement.parts.size()){
                    for(size_t i=0;i<n;++i){const auto& p=pieces[i];auto& part=placement.parts[placement.partCount++];
                        const Pose onPlate{{0,0,0,1},{p.pose.position.x,p.pose.position.y,-placement.distance}};
                        // the plate fixed in the room (playPlate): its pieces where it is fixed
                        part={p.rect.x0,p.rect.y0,p.rect.x1-p.rect.x0,p.rect.y1-p.rect.y0,placement.localSpace,placement.localSpace?compose(placement.origin,onPlate):onPlate,p.width,p.height};}
                    for(size_t i=0;i<count;++i)placement.parts[placement.partCount++]={cuts[i].x0,cuts[i].y0,cuts[i].x1-cuts[i].x0,cuts[i].y1-cuts[i].y0,true,moved[i].pose,moved[i].width,moved[i].height};
                }
                if(targetHud){const auto k=++targetHudFrames;
                    if(k<=4||k%300==0){std::ostringstream o;o<<std::setprecision(6)<<"\"frames\":"<<k<<",\"frame\":"<<frame<<",\"anchor_pixel\":["<<hudX<<','<<hudY<<"],\"cut\":["<<hudCut.x0<<','<<hudCut.y0<<','<<hudCut.x1<<','<<hudCut.y1
                        <<"],\"anchor\":["<<hud[0]<<','<<hud[1]<<','<<hud[2]<<"],\"slopes\":["<<hud[3]<<','<<hud[4]<<"],\"tan_half_fov\":"<<hud[5]<<",\"vertical\":"<<(hud[6]>.5f?"true":"false")
                        <<",\"with_cluster\":"<<(cluster&&clusterFits?"true":"false")<<",\"cluster_rows\":["<<clusterCut.y0<<','<<clusterCut.y1<<"],\"parts\":"<<placement.partCount;state.log->event("target_hud_part",o.str());}}
            }
            if(targetHud&&placement.localSpace&&!playPlate)hudState=3;
            if(hudState!=state.targetHudState&&state.targetHudStateLogs<2000){++state.targetHudStateLogs;state.targetHudState=hudState;
                static const char* names[]={"no_target","plate_fixed","anchor_off_image","plate_in_room","no_part","moved"};
                std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<frame<<",\"state\":\""<<names[std::clamp(hudState,0,5)]<<"\",\"anchor_pixel\":["<<hudX<<','<<hudY<<"],\"cluster\":"<<(cluster?"true":"false");
                if(hudKnown)o<<",\"slopes\":["<<hud[3]<<','<<hud[4]<<"],\"anchor\":["<<hud[0]<<','<<hud[1]<<','<<hud[2]<<']';
                state.log->event("target_hud_state",o.str());}
            // Diagnostic (2026-10-20, the enemy health bar left in the HUD while the enemy is out of view; where the game draws it
            // then is not known): EVR_TARGET_HUD_OFF_CAPTURE=<n> captures the HUD and the eyes 3 and 20 frames into each time the
            // anchor is off the game's picture, n captures at the most.
            {static const unsigned offCaptures=[]{const char* v=std::getenv("EVR_TARGET_HUD_OFF_CAPTURE");return v?unsigned(std::strtoul(v,nullptr,10)):0u;}();
                static unsigned offFrames=0,offTaken=0;offFrames=hudState==2?offFrames+1:0;
                if(offCaptures&&offTaken<offCaptures&&(offFrames==3||offFrames==20)){uint32_t empty=0;if(nativeFixtureCapture.compare_exchange_strong(empty,9u)){++offTaken;
                    state.log->event("target_hud_off_capture","\"frame\":"+std::to_string(frame)+",\"off_frames\":"+std::to_string(offFrames));}}}
            if((placement.partCount!=0)!=state.aimClusterLogged&&state.aimClusterLogs<200){
                ++state.aimClusterLogs;state.aimClusterLogged=placement.partCount!=0;std::ostringstream o;o<<std::setprecision(7)<<"\"frame\":"<<frame<<",\"on\":"<<(placement.partCount?"true":"false")<<",\"parts\":"<<placement.partCount;
                if(placement.partCount){const auto& c=placement.parts[placement.partCount-1];
                    o<<",\"cluster_pixels\":["<<c.x<<','<<c.y<<','<<c.pixelsX<<','<<c.pixelsY<<"],\"cluster_position\":["<<c.pose.position.x<<','<<c.pose.position.y<<','<<c.pose.position.z
                     <<"],\"cluster_metres\":["<<c.width<<','<<c.height<<"],\"aimed\":["<<aimedPoint.x<<','<<aimedPoint.y<<','<<aimedPoint.z<<"],\"space\":\"LOCAL\"";}
                state.log->event("native_aim_cluster",o.str());
            }
            xr.markUiRendered(frame,placement);
            std::string fields="\"fixture_capture_tag\":"+std::to_string(fixtureCapture)+",\"frame\":"+std::to_string(frame)+",\"begin_present\":"+std::to_string(state.beginPresent)+
                ",\"source_present\":"+std::to_string(uiSource.present)+",\"source_version\":"+std::to_string(uiSource.version)+
                ",\"source_image\":"+handleText(uiSource.image)+",\"xr_image\":"+handleText(xr.uiSwapchain().images[uiIndex].image)+
                ",\"image_index\":"+std::to_string(uiIndex)+",\"width\":"+std::to_string(uiSource.width)+",\"height\":"+std::to_string(uiSource.height)+
                ",\"ui_readback_captured\":"+std::string(capture?"true":"false")+",\"gpu_fence_complete\":"+((capture||!async)?"true":"false")+
                ",\"async_completion\":"+((capture||!async)?"false":"true")+",\"previous_ui_pending\":"+(state.uiTransfer->previousPending?"true":"false")+
                ",\"format\":\"RGBA8_SRGB\",\"color_policy\":\"unpremultiply_srgb_decode_premultiply\"";
            // A bug report's picture reads the game's UI back only for its one UI picture (2026-10-22: the copy out of the UI's
            // mapped memory and the statistics of every picture took about 130 of the 160 ms a picture held the frame).
            if(bugBurst&&uiReadback&&bugUiWanted.exchange(false))try{   // the bug report's UI picture: the game's UI alone, no statistics
                const auto native=uiDraw::captureSource(state.device,uiSource);
                if(native.size()==size_t(uiSource.width)*uiSource.height*4){const auto prefix=output+"/native-xr/ui-frame-"+std::to_string(frame);
                    {std::ofstream file(prefix+"-native.rgba8",std::ios::binary);file.write(reinterpret_cast<const char*>(native.data()),std::streamsize(native.size()));}
                    {std::ofstream json(prefix+".json");json<<"{\"frame\":"<<frame<<",\"width\":"<<uiSource.width<<",\"height\":"<<uiSource.height<<",\"by\":\"bug_report\"}\n";}
                    state.log->event("bug_report_ui_frame","\"frame\":"+std::to_string(frame)+",\"width\":"+std::to_string(uiSource.width)+",\"height\":"+std::to_string(uiSource.height));}
            }catch(const std::exception& e){state.log->event("bug_report_ui_frame_failed","\"reason\":"+jsonString(e.what()));}
            if(capture&&uiBurst){
                const auto native=uiDraw::captureSource(state.device,uiSource);
                if(bugUiWanted.exchange(false)&&native.size()==size_t(uiSource.width)*uiSource.height*4)try{
                    const auto prefix=output+"/native-xr/ui-frame-"+std::to_string(frame);
                    {std::ofstream file(prefix+"-native.rgba8",std::ios::binary);file.write(reinterpret_cast<const char*>(native.data()),std::streamsize(native.size()));}
                    {std::ofstream json(prefix+".json");json<<"{\"frame\":"<<frame<<",\"width\":"<<uiSource.width<<",\"height\":"<<uiSource.height<<",\"by\":\"bug_report\"}\n";}
                    state.log->event("bug_report_ui_frame","\"frame\":"+std::to_string(frame)+",\"width\":"+std::to_string(uiSource.width)+",\"height\":"+std::to_string(uiSource.height));
                }catch(const std::exception& e){state.log->event("bug_report_ui_frame_failed","\"reason\":"+jsonString(e.what()));}
                const auto stat=[&](const std::vector<uint8_t>& p){
                    const unsigned w=uiSource.width,h=uiSource.height;std::array<uint32_t,144> cells{};uint64_t alpha=0,color=0;uint32_t covered=0;
                    if(p.size()!=size_t(w)*h*4)return std::string("null");
                    for(unsigned y=0;y<h;y+=4)for(unsigned x=0;x<w;x+=4){const auto* q=p.data()+(size_t(y)*w+x)*4;
                        if(q[3]>8){++covered;++cells[(y*9/h)*16+(x*16/w)];}alpha+=q[3];color+=q[0]+q[1]+q[2];}
                    std::string out="{\"covered\":"+std::to_string(covered)+",\"alpha\":"+std::to_string(alpha)+",\"color\":"+std::to_string(color)+",\"cells\":[";
                    for(size_t i=0;i<cells.size();i++)out+=(i?",":"")+std::to_string(cells[i]);
                    return out+"]}";};
                state.log->event("native_ui_burst_stat","\"frame\":"+std::to_string(frame)+",\"source_present\":"+std::to_string(uiSource.present)+",\"source_version\":"+std::to_string(uiSource.version)+
                    ",\"image_index\":"+std::to_string(uiIndex)+",\"grid\":[16,9],\"stride\":4,\"native\":"+stat(native)+",\"xr\":"+stat(bytes));
            }else if(capture){
                const auto prefix=output+"/native-xr/ui-frame-"+std::to_string(frame);
                if(!captureHalfSize()){   // EVR_CAPTURE_HALF: the eyes only (stereo_transfer.hpp)
                const auto native=uiDraw::captureSource(state.device,uiSource);
                for(const auto& pair:std::array<std::pair<std::string,const std::vector<uint8_t>*>,2>{{{"-native.rgba8",&native},{"-xr.rgba8",&bytes}}}){
                    std::ofstream file(prefix+pair.first,std::ios::binary);file.write(reinterpret_cast<const char*>(pair.second->data()),std::streamsize(pair.second->size()));
                    if(!file)throw std::runtime_error("native XR UI capture write failed");
                }}
                if(const auto& desk=state.uiTransfer->desktopCapture();!desk.empty()&&!captureHalfSize()){
                    std::ofstream file(prefix+"-desktop.rgba8",std::ios::binary);file.write(reinterpret_cast<const char*>(desk.data()),std::streamsize(desk.size()));
                    if(!file)throw std::runtime_error("native XR desktop UI capture write failed");
                }
                std::ofstream meta(prefix+".json");meta<<'{'<<fields<<"}\n";if(!meta)throw std::runtime_error("native XR UI metadata write failed");
            }
            state.log->event("native_ui_gpu_complete",fields);
        }
        uiDone=std::chrono::steady_clock::now();
        // The eye left out: its kept picture warped to this frame's pose, into the image acquired for it.
        for(uint32_t eye=0;eye<2;++eye){
            if((eyes>>eye&1)||!xr.eyeWarpable(eye)||!state.warp||state.warpFailed)continue;
            try{
                // EVR_EYE_WARP_CAPTURE=N: the first N warped pictures as native-xr/warp-<frame>-eye-<eye>.ppm (from the frame
                // EVR_EYE_WARP_CAPTURE_FROM, when given), for a look at the warp beside the drawn eye of the same frames.
                static const unsigned captureWanted=[]{const auto f=std::getenv("EVR_EYE_WARP_CAPTURE");return f?unsigned(std::strtoul(f,nullptr,10)):0u;}();
                static const uint64_t captureFrom=[]{const auto f=std::getenv("EVR_EYE_WARP_CAPTURE_FROM");return f?uint64_t(std::strtoull(f,nullptr,10)):0ull;}();
                const bool capture=state.warpCaptures<captureWanted&&frame>=captureFrom;
                TransferImage runtime;runtime.image=xr.targets[eye].images[snapshot.imageIndices[eye]].image;runtime.width=xr.targets[eye].width;runtime.height=xr.targets[eye].height;
                runtime.format=xr.format;runtime.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;runtime.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                // The eye's own last picture (the holes' fill) starts from where the world now has it (the hooks' stale
                // pose: a step or a turn by the stick moved the world under the tracking space); EVR_EYE_WARP_FOLLOW=0:
                // from the pose it was drawn at, as before 2026-10-23.
                static const bool follow=[]{const auto f=std::getenv("EVR_EYE_WARP_FOLLOW");return !(f&&!std::strcmp(f,"0"));}();
                const XrPosef* keptAt=follow?xr.staleEyePose(eye):nullptr;
                const auto bytes=state.warp->draw(eye,runtime,snapshot.views[eye],frame,state.warpFromOther,capture,!async,keptAt);
                if(keptAt)++state.warpFollowed;
                xr.markEyeWarped(eye,frame);
                const auto n=++state.warpFrames;
                if(capture){++state.warpCaptures;const auto path=output+"/native-xr/warp-"+std::to_string(frame)+"-eye-"+std::to_string(eye)+".ppm";
                    std::filesystem::create_directories(std::filesystem::path(output)/"native-xr");state.warp->writePpm(path,bytes);}
                if(n<=3||n%600==0||capture){const auto& k=state.warp->keptView(eye);std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<frame<<",\"eye\":"<<eye<<",\"warped_frames\":"<<n
                    <<",\"from_self\":"<<(state.warp->lastFromSelf?"true":"false")<<",\"from_other\":"<<(state.warp->lastFromOther?"true":"false")<<",\"other_frame\":"<<state.warp->lastOtherFrame<<",\"gpu_ms\":"<<state.warp->gpuMs<<",\"gpu_mean_ms\":"<<(state.warp->gpuCount?state.warp->gpuSum/state.warp->gpuCount:-1.0)<<",\"gpu_max_ms\":"<<state.warp->gpuMax<<",\"kept_followed\":"<<state.warpFollowed<<",\"draws_from_other\":"<<state.warp->drawsFromOther<<",\"fills\":"<<state.warp->fills<<",\"own_draws\":"<<state.warp->ownDraws
                    <<",\"kept_frame\":"<<state.warp->keptFrame(eye)<<",\"kept_position\":["<<k.pose.position.x<<','<<k.pose.position.y<<','<<k.pose.position.z<<"],\"position\":["<<snapshot.views[eye].pose.position.x<<','<<snapshot.views[eye].pose.position.y<<','<<snapshot.views[eye].pose.position.z
                    <<"],\"captured\":"<<(capture?"true":"false");state.log->event("eye_warp",o.str());}
            }catch(const std::exception& e){state.warpFailed=true;state.log->event("eye_warp_failed","\"stage\":\"draw\",\"frame\":"+std::to_string(frame)+",\"eye\":"+std::to_string(eye)+",\"reason\":"+jsonString(e.what()));}
        }
        // The self-check of the third stage: on a frame that drew both eyes, every 120th, eye 1 made from eye 0 against eye 1 as drawn.
        if(state.warpCheck&&eyes==3&&state.warp&&!state.warpFailed&&++state.warpCheckFrames%state.warpCheckEvery==0&&state.warp->keptThisFrame(0,frame)&&state.warp->keptThisFrame(1,frame)){
            try{// EVR_EYE_WARP_CHECK_CAPTURE=N: the first N checks from EVR_EYE_WARP_CAPTURE_FROM also write the made and the drawn picture and their difference (x4, holes blue) as native-xr/check-<frame>-{made,drawn,diff}.ppm.
                static const unsigned checkCaptureWanted=[]{const auto f=std::getenv("EVR_EYE_WARP_CHECK_CAPTURE");return f?unsigned(std::strtoul(f,nullptr,10)):0u;}();
                static const uint64_t checkCaptureFrom=[]{const auto f=std::getenv("EVR_EYE_WARP_CAPTURE_FROM");return f?uint64_t(std::strtoull(f,nullptr,10)):0ull;}();
                const bool checkCapture=state.warpCheckCaptures<checkCaptureWanted&&frame>=checkCaptureFrom;std::vector<uint8_t> made,drawn;
                const auto c=state.warp->check(1,frame,checkCapture?&made:nullptr,checkCapture?&drawn:nullptr);++state.warpChecks;
                if(checkCapture){++state.warpCheckCaptures;const auto prefix=output+"/native-xr/check-"+std::to_string(frame);std::filesystem::create_directories(std::filesystem::path(output)/"native-xr");
                    state.warp->writePpm(prefix+"-made.ppm",made);state.warp->writePpm(prefix+"-drawn.ppm",drawn);std::vector<uint8_t> diff(made.size());
                    for(size_t i=0;i+3<made.size();i+=4){if(made[i+3]==0){diff[i]=0;diff[i+1]=0;diff[i+2]=255;}else for(unsigned ch=0;ch<3;++ch){const int d=std::abs(int(made[i+ch])-int(drawn[i+ch]))*4;diff[i+ch]=uint8_t(d>255?255:d);}diff[i+3]=255;}
                    state.warp->writePpm(prefix+"-diff.ppm",diff);}
                std::ostringstream o;o<<std::setprecision(5)<<"\"frame\":"<<frame<<",\"eye_made\":1,\"from_eye\":0,\"mean_abs_diff\":"<<c.meanDifference<<",\"holes\":"<<c.holes<<",\"compared\":"<<c.compared<<",\"checks\":"<<state.warpChecks<<",\"cells\":"<<state.warp->cells
                    <<",\"unjitter\":"<<eyeWarpUnjitterUsed<<",\"depth_shift\":["<<state.warp->depthShift[0]<<','<<state.warp->depthShift[1]<<']';
                state.log->event("eye_warp_check",o.str());
                // EVR_EYE_WARP_PARALLAX_SWEEP=1 (2026-10-23, the user: "max still shakes"; Mock: the made eye stood 0.3 .. 4 px to the
                // side of the drawn one, more the nearer the thing, none on the clouds): the same check with the eyes' distance
                // taken 0.7 .. 1.3 times, each one's mean difference - where it is least, the parallax the game drew.
                static const bool parallaxSweep=[]{char v[8]{};return GetEnvironmentVariableA("EVR_EYE_WARP_PARALLAX_SWEEP",v,sizeof v)==1&&v[0]=='1';}();
                if(parallaxSweep){std::ostringstream p;p<<std::setprecision(5)<<"\"frame\":"<<frame<<",\"parallax\":[";std::ostringstream d;d<<"],\"mean_abs_diff\":[";
                    const float factors[]{.7f,.8f,.9f,.95f,1.f,1.05f,1.1f,1.2f,1.3f};bool first=true;
                    for(const float f:factors){const auto r=state.warp->check(1,frame,nullptr,nullptr,f);p<<(first?"":",")<<f;d<<(first?"":",")<<r.meanDifference;first=false;}
                    state.log->event("eye_warp_parallax_sweep",p.str()+d.str()+"]");}
                // EVR_EYE_WARP_HOLE_SWEEP=1 (2026-10-23; Mock: the made eye's holes lay all over the grass, far wider than what the
                // other eye cannot see - in the alternate eyes those are the eye's own picture of two frames before): the same
                // check with a hole only where the walk that found nothing was at least 0.75 .. 24 pixels long (shorter: the
                // stretched place), each one's holes and mean difference.
                static const bool holeSweep=[]{char v[8]{};return GetEnvironmentVariableA("EVR_EYE_WARP_HOLE_SWEEP",v,sizeof v)==1&&v[0]=='1';}();
                if(holeSweep){std::ostringstream p;p<<std::setprecision(5)<<"\"frame\":"<<frame<<",\"hole_pixels\":[";std::ostringstream d,h;d<<"],\"mean_abs_diff\":[";h<<"],\"holes\":[";
                    const float sizes[]{.75f,1.5f,3.f,6.f,12.f,24.f};bool first=true;
                    for(const float s:sizes){const auto r=state.warp->check(1,frame,nullptr,nullptr,1.f,s);p<<(first?"":",")<<s;d<<(first?"":",")<<r.meanDifference;h<<(first?"":",")<<r.holes;first=false;}
                    state.log->event("eye_warp_hole_sweep",p.str()+d.str()+h.str()+"]");}
                // EVR_EYE_WARP_THIN_SWEEP=1 (2026-10-23): the same check with the walk without and with thin things met.
                static const bool thinSweep=[]{char v[8]{};return GetEnvironmentVariableA("EVR_EYE_WARP_THIN_SWEEP",v,sizeof v)==1&&v[0]=='1';}();
                if(thinSweep){const auto off=state.warp->check(1,frame,nullptr,nullptr,1.f,0.f,0),on=state.warp->check(1,frame,nullptr,nullptr,1.f,0.f,1);
                    std::ostringstream o;o<<std::setprecision(5)<<"\"frame\":"<<frame<<",\"mean_abs_diff\":["<<off.meanDifference<<','<<on.meanDifference<<"],\"holes\":["<<off.holes<<','<<on.holes<<"]";
                    state.log->event("eye_warp_thin_sweep",o.str());}}
            catch(const std::exception& e){state.warpCheck=false;state.log->event("eye_warp_check_failed","\"frame\":"+std::to_string(frame)+",\"reason\":"+jsonString(e.what()));}
        }
        // EVR_AFW_SHOTS=N (2026-10-23; the user after the headset: "every pacing shakes the same" - the shake is not the pacing):
        // N frames in a row of eye 0 as it is shown - drawn, or made from the other eye - a quarter of the size each way, from the
        // frame EVR_AFW_SHOTS_FROM, without holding the frames up; written afterwards as native-xr/afw-shot-<frame>-<d|m>.ppm.
        {static const unsigned afwShots=[]{const auto f=std::getenv("EVR_AFW_SHOTS");return f?unsigned(std::min(std::strtoul(f,nullptr,10),400ul)):0u;}();
         static const uint64_t afwShotsFrom=[]{const auto f=std::getenv("EVR_AFW_SHOTS_FROM");return f?uint64_t(std::strtoull(f,nullptr,10)):0ull;}();
         static bool afwShotsDone=false;
         if(afwShots&&!asyncFrame&&state.warp&&!afwShotsDone&&frame>=afwShotsFrom&&((eyes&1)||xr.eyeWarpable(0))){
            try{
                auto& w=*state.warp;w.prepareShots(afwShots);
                const auto& target=xr.targets[0];
                const TransferImage shown{target.images[snapshot.imageIndices[0]].image,target.width,target.height,xr.format,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,xr.vk.family};
                w.shotOfImage(shown,frame*2+((eyes&1)?1:0));
                if(w.shotsFull()){
                    afwShotsDone=true;auto shots=w.collectShots();const uint32_t sw=w.shotWidth(),sh=w.shotHeight();
                    std::error_code ec;std::filesystem::create_directories(std::filesystem::path(output)/"native-xr",ec);
                    state.log->event("afw_shots","\"frame\":"+std::to_string(frame)+",\"shots\":"+std::to_string(shots.size()));
                    std::thread([shots=std::move(shots),sw,sh,dir=output+"/native-xr/"]{try{for(const auto& [tag,px]:shots){
                        std::ofstream out(dir+"afw-shot-"+std::to_string(tag/2)+((tag&1)?"-d":"-m")+".ppm",std::ios::binary);out<<"P6\n"<<sw<<' '<<sh<<"\n255\n";std::vector<char> row(size_t(sw)*3);
                        for(uint32_t y=0;y<sh;++y){const auto* p=px.data()+size_t(y)*sw*4;for(uint32_t x=0;x<sw;++x){row[x*3]=char(p[x*4]);row[x*3+1]=char(p[x*4+1]);row[x*3+2]=char(p[x*4+2]);}out.write(row.data(),std::streamsize(row.size()));}}}catch(...){}}).detach();
                }
            }catch(const std::exception& e){afwShotsDone=true;state.log->event("afw_shots_failed","\"reason\":"+jsonString(e.what()));}
         }}
        const auto transferComplete=std::chrono::steady_clock::now();
        // The world's motion of this frame for the refresh thread (an identity when the hooks gave none).
        {const auto& m=state.pendingMotion;const bool ours=m.valid&&m.frame==frame;
         state.motions[frame%state.motions.size()]={frame,ours?m.perFrame:XrPosef{{0,0,0,1},{0,0,0}},ours?m.dt:0,snapshot.predictedDisplayTime,ours};}
        if(!asyncFrame)for(uint32_t eye=0;eye<2;++eye)if(eyes>>eye&1)xr.markEyeRendered(eye,frame);
        if(eyes==3)xr.finishFrame();
        else try{xr.finishFrame();}catch(const std::exception& e){
            // The runtime did not take a frame with one eye's earlier picture: that frame is dropped (finishFrame
            // ended it empty), and every later frame draws both eyes. The session goes on.
            state.alternateRefused=true;
            state.log->event("alternate_eyes_refused","\"frame\":"+std::to_string(frame)+",\"reason\":"+jsonString(e.what())+",\"both_eyes_from_now\":true");
            return 0;
        }
        if(!captures[0].empty()&&!captures[1].empty()){
            state.capturedFrame=frame;
            // Capture-specific names stay below the session log cap, even after the per-frame pose log ends.
            const auto& h=xr.headLocation.pose;std::ostringstream f;f<<std::setprecision(10)
                <<"\"frame\":"<<frame<<",\"time\":"<<snapshot.predictedDisplayTime<<",\"head_flags\":"<<xr.headLocation.locationFlags
                <<",\"position\":["<<h.position.x<<','<<h.position.y<<','<<h.position.z<<"],\"orientation\":["
                <<h.orientation.x<<','<<h.orientation.y<<','<<h.orientation.z<<','<<h.orientation.w
                <<"],\"strict_capture\":true,\"regular_async_policy\":"<<(state.asyncCompletion?"true":"false")<<",\"eyes\":[";
            for(size_t eye=0;eye<2;++eye){const auto& v=snapshot.views[eye];const auto& p=v.pose;const auto& a=v.fov;if(eye)f<<',';
                f<<"{\"eye\":"<<eye<<",\"position\":["<<p.position.x<<','<<p.position.y<<','<<p.position.z
                 <<"],\"orientation\":["<<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w
                 <<"],\"fov\":["<<a.angleLeft<<','<<a.angleRight<<','<<a.angleUp<<','<<a.angleDown<<"],\"width\":"<<src[eye].width<<",\"height\":"<<src[eye].height<<'}';}
            f<<']';state.log->event("native_capture_pose",f.str());
        }
        if(fixtureCapture)state.log->event("native_fixture_capture_complete","\"tag\":"+std::to_string(fixtureCapture)+",\"frame\":"+std::to_string(frame)+",\"ui_submitted\":"+(submitUi?"true":"false"));
        if(burstCapture)state.log->event("native_burst_capture_complete","\"index\":"+std::to_string(burstCapture.index)+
            ",\"total\":"+std::to_string(burstCapture.total)+",\"frame\":"+std::to_string(frame)+",\"ui_submitted\":"+(submitUi?"true":"false"));
        const auto finishComplete=std::chrono::steady_clock::now();
        const auto& transferTiming=state.transfer->cpuTiming();
        const auto micros=[](auto start,auto end){return std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();};
        state.log->event("native_completion_cpu_timing","\"frame\":"+std::to_string(frame)+
            ",\"evidence_us\":"+std::to_string(micros(completionStart,evidenceComplete))+
            ",\"device_idle_us\":"+std::to_string(micros(evidenceComplete,idleComplete))+
            ",\"transfer_and_capture_us\":"+std::to_string(micros(idleComplete,transferComplete))+",\"eye_copy_us\":"+std::to_string(micros(idleComplete,copyDone))+",\"ui_us\":"+std::to_string(micros(copyDone,uiDone))+
            ",\"end_frame_us\":"+std::to_string(micros(transferComplete,finishComplete))+
            ",\"total_us\":"+std::to_string(micros(completionStart,finishComplete))+
            ",\"transfer_free_us\":"+std::to_string(transferTiming.freeUs)+
            ",\"transfer_allocate_us\":"+std::to_string(transferTiming.allocateUs)+
            ",\"transfer_record_us\":"+std::to_string(transferTiming.recordUs)+
            ",\"transfer_submit_wait_us\":"+std::to_string(transferTiming.submitWaitUs)+
            ",\"transfer_map_us\":"+std::to_string(transferTiming.mapUs)+
            ",\"transfer_write_us\":"+std::to_string(transferTiming.writeUs)+
            ",\"readback_memory_types\":["+std::to_string(transferTiming.memoryTypes[0])+','+std::to_string(transferTiming.memoryTypes[1])+']'+
            ",\"readback_memory_flags\":["+std::to_string(transferTiming.memoryFlags[0])+','+std::to_string(transferTiming.memoryFlags[1])+']'+
            ",\"capture\":"+std::string(captures[0].empty()&&captures[1].empty()?"false":"true")+
            ",\"async\":"+std::string(async?"true":"false")+",\"previous_pending\":"+(transferTiming.previousPending?"true":"false")+
            ",\"previous_wait_us\":"+std::to_string(transferTiming.previousWaitUs));
        ++state.submittedPairs;++state.worldPairs;state.screenWanted=false;
        state.log->event("native_game_pair_submitted","\"frame\":"+std::to_string(frame)+
            ",\"time\":"+std::to_string(snapshot.predictedDisplayTime)+",\"pair\":"+std::to_string(state.submittedPairs)+",\"eyes\":"+std::to_string(eyes==3?2:1)+",\"eye_mask\":"+std::to_string(eyes)+",\"gpu_fence_complete\":"+(async?"false":"true")+
            ",\"async_completion\":"+(async?"true":"false")+",\"queue_ordered_release\":true");
        return 0;
    }catch(const std::exception& e){note("native_xr_submit_failed","\"reason\":"+jsonString(e.what()));return 1;}
}
