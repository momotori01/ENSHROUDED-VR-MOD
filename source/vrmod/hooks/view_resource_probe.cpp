#include <evr/actor_value_hold.hpp>
#include <evr/game_build.hpp>
#include <evr/vfx_culling_union.hpp>
#include <evr/query_return_completion.hpp>
#include <evr/readback_retirement.hpp>
#include <evr/present_retirement_abi.hpp>
#include <evr/world_epoch_abi.hpp>
#include <evr/combat_epoch_client.hpp>
#include <evr/actor_admission.hpp>
#include <evr/epoch_attack_packet.hpp>
#include <evr/combat_world_binding.hpp>
#include <evr/world_lifetime.hpp>
#include <evr/melee_session_fixture.hpp>
#include <evr/observation_budget.hpp>
#include <evr/motion_contact_retirement.hpp>
#include <evr/motion_contact_lifetime.hpp>
#include <evr/motion_world_retirement.hpp>
#include <evr/motion_collider_frame.hpp>
#include <evr/retained_event_scope.hpp>
#include <evr/motion_guard_fixture.hpp>
#include <evr/controller_guard_fixture.hpp>
#include <evr/controller_equipment_fixture.hpp>
#include <evr/motion_visual_binding.hpp>
#include <evr/weapon_mount.hpp>
#include <evr/live_wield.hpp>
#include <evr/held_effect_mount.hpp>
#include <evr/weapon_category.hpp>
#include <evr/head_gesture_feedback.hpp>
#include <evr/vertical_stick_mapping.hpp>
#include <evr/live_pad.hpp>
#include <evr/left_trigger_tools.hpp>
#include <evr/staff_trigger.hpp>
#include <evr/bow_input_trial.hpp>
#include <evr/bow_physical_trial.hpp>
#include <evr/bow_physical_release.hpp>
#include <evr/bow_tracked_shot.hpp>
#include <evr/bow_draw.hpp>
#include <evr/ranged_aim.hpp>
#include <evr/button_glyph_labels.hpp>
#include <evr/live_melee.hpp>
#include <evr/live_throw.hpp>
#include <evr/live_swing_cast.hpp>
#include <evr/status_rim.hpp>
#include <evr/pose_smoothing.hpp>
#include <evr/hit_stop.hpp>
#include <evr/breakable_models.hpp>
#include <evr/damage_calc.hpp>
#include <evr/damage_tables.hpp>
#include <evr/gpu_info.hpp>
#include <evr/vfx_decal_rebase.hpp>
#include <evr/live_secondary.hpp>
#include <evr/throwable_catalog.hpp>
#include <evr/bow_speed_catalog.hpp>
#include <evr/live_guard.hpp>
#include <evr/quick_slot.hpp>
#include <evr/body_ik.hpp>
#include <evr/player_sync.hpp>
#include <evr/guard_rule.hpp>
#include <evr/live_contact.hpp>
#include <evr/attack_catalog.hpp>
#include <evr/live_targets.hpp>
#include <evr/bow_draw_pose.hpp>
#include <evr/quick_wheel_schedule.hpp>
#include <evr/height_settle.hpp>
#include <evr/grip_router.hpp>
#include <evr/holster.hpp>
#include <evr/consumable_hold.hpp>
#include <evr/consumable_model.hpp>
#include <evr/consumable_hand_catalog.hpp>
#include <evr/room_sneak.hpp>
#include <evr/swim_stroke.hpp>
#include <evr/equip_planner.hpp>
#include <evr/health_schema.hpp>
#include <evr/impact_witness.hpp>
#include <evr/motion_event_retime.hpp>
#include <evr/motion_collider_lease.hpp>
#include <evr/motion_collider_pose.hpp>
#include <evr/scoped_query_transforms.hpp>
#include <evr/motion_feedback.hpp>
#include <evr/motion_attack_fixture.hpp>
#include <evr/hand_rig.hpp>
#include <evr/controller_attack_fixture.hpp>
#include <evr/body_motion.hpp>
#include <evr/controller_motion_fixture.hpp>
#include <evr/controller_menu_fixture.hpp>
#include <evr/camera_mode.hpp>
#include <evr/body_clip_spirv_patch.hpp>
// Bounded diagnostics: unused View lifecycle or native stereo calibration.
// Private camera inputs, plus an explicit bounded test-only analog fixture.
// Shared player/input components are never written by these diagnostics.
#include <MinHook.h>
#include <evr/hook_disable_batch.hpp>
#include <evr/log.hpp>
#include <evr/observer_error.hpp>
#include <evr/signature.hpp>
#include <evr/math.hpp>
#include <evr/recorded_xr.hpp>
#include <evr/native_xr_bridge.hpp>
#include <evr/water_feedback.hpp>
#include <evr/root_interpolation.hpp>
#include <evr/culling_history.hpp>
#include <evr/head_visibility.hpp>
#include <evr/observer_stack.hpp>
#include <evr/shared_culling_camera.hpp>
#include <evr/shadow_projection.hpp>
#include <evr/stable_frustum.hpp>
#include <evr/input_pose_channel.hpp>
#include <evr/idle_head_camera.hpp>
#include <evr/walking_head_camera.hpp>
#include <evr/pointer_aim_camera.hpp>
#include <evr/glide_launch.hpp>
#include <evr/walking_input_fixture.hpp>
#include <evr/item_registry.hpp>
#include <evr/item_catalog.hpp>
#include <evr/menu_pointer.hpp>
#include <evr/bug_report.hpp>
#include <evr/mod_update.hpp>
#include <evr/vr_settings_panel.hpp>
#include <evr/aim_mark.hpp>
#include <evr/pause_clock.hpp>
#include <iomanip>
#include <fstream>
#include <intrin.h>
#include <renderdoc_app.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <array>
#include <atomic>
#include <deque>
#include <chrono>
#include <filesystem>
#include <thread>
#include <windows.h>
#include <Xinput.h>
using namespace evr;
extern "C" {void* evr_ui_state_trampoline=nullptr;void evr_ui_state_stub();}
extern "C" {void* evr_camera_trampoline=nullptr;void evr_camera_stub();}
extern "C" {void* evr_camera_sum_trampoline=nullptr;void evr_camera_sum_stub();}
extern "C" {void* evr_vfx_gate_trampoline=nullptr;void evr_vfx_gate_stub();void* evr_vfx_draw_trampoline=nullptr;void evr_vfx_draw_stub();void evr_observe_vfx_gate(uintptr_t,unsigned) noexcept;}
extern "C" {void* evr_vfx_bindings_trampoline=nullptr;void evr_vfx_bindings_stub();}
extern "C" {void* evr_vfx_model_trampolines[8]={};void evr_vfx_model_stub0();void evr_vfx_model_stub1();void evr_vfx_model_stub2();void evr_vfx_model_stub3();void evr_vfx_model_stub4();void evr_vfx_model_stub5();void evr_vfx_model_stub6();void evr_vfx_model_stub7();}
extern "C" {void* evr_vfx_extra_trampolines[4]={};void evr_vfx_extra_stub0();void evr_vfx_extra_stub1();void evr_vfx_extra_stub2();void evr_vfx_extra_stub3();}
extern "C" {void* evr_body_color_trampoline=nullptr;void* evr_body_depth_trampoline=nullptr;void evr_body_color_stub();void evr_body_depth_stub();}
namespace {
// Structure layout of the game's render objects, 1.0 (build 25936191, 2026-10-16); the 2026-06 build's value after each.
// Read off the matched functions of both builds (docs/analysis/rematch_1_0_layout_20261016.md): the View constructor
// fa450 (c8880), its factory 103460 (d0060), the feature setup 103a20 (d05a0) and 107d80 (d43e0), the releases 107b20 /
// 107830 / 107570 (d4170 / d3ef0 / d3bf0), the owner's factory f2cb0 (c11c0) and configure f1b90 (c02a0).
namespace layout {
constexpr size_t viewBytes=0x1140;           // 0xbc0
constexpr size_t ownerBytes=0xf8;            // 0xd0  (the render scene's owner: View+0)
constexpr size_t parameterBytes=0x600;       // 0x400 (the render camera arguments; 1.0 reads them up to +0x41e)
constexpr size_t sceneRenderer=0x18;         // 0x8
constexpr size_t sceneView=0x40;             // 0x28  (the selected View: the render back-pointer)
constexpr size_t sceneVfx=0x950;             // 0x908 (the VFX scene)
constexpr size_t vfxSceneView=0x80;          // the VFX View lives inside the VFX scene (the old one pointed at it: +0xe0)
constexpr size_t viewConfig=0xc4;            // 0x7c  (the factory's config +8..+0x1c, 20 bytes; the old one 17)
constexpr size_t viewSelf=0x240,viewSelfTarget=0x268; // 0x150 -> 0x178
constexpr size_t viewSerial=0x8c8;           // 0x660
constexpr size_t viewCamera=0x8d0;           // 0x668 (the camera record: the pose 0x38 bytes, then the projection)
constexpr size_t viewProjection=0x908;       // 0x6a0
constexpr size_t viewPingPong=0x850;         // 0x5f0
constexpr size_t viewStreaming=0x978;        // 0x710
constexpr size_t viewLastPositionValid=0x970;// 0x708
constexpr size_t viewReady=0x980;            // 0x718
constexpr size_t viewVfx=0x9a8;              // 0x738
constexpr size_t viewFog=0xb40;              // 0x8c0
constexpr size_t viewWater=0xb48;            // 0x8c8
constexpr size_t viewGi=0xed0;               // 0x950
constexpr size_t viewLightCache=0xee0;       // 0x958
constexpr size_t viewSsr=0xf18;              // 0x990 (two slots)
constexpr size_t viewUpscaler=0xf38;         // 0x9b0 (kind byte; FSR3 +8, DLSS +0x10, XeSS +0x18)
constexpr size_t viewLights=0xf98;           // 0xa08 (array, count +8, capacity +0x10, grow +0x18)
constexpr size_t viewHistoryValid=0x1110;    // 0xb90
constexpr size_t viewPost=0x1120;            // 0xba0
constexpr size_t viewDepthOfField=0x1130;    // 0xbb0
// The owner (0xf8): the settings that configure (f1b90) takes are +0x10..+0x94 (0x84 bytes; the old ones 0x60).
constexpr size_t ownerSettings=0x10,ownerSettingsBytes=0x84;
constexpr size_t ownerWidth=0x1c,ownerHeight=0x20,ownerRenderScale=0x24; // 0x18, 0x1c, 0x20
constexpr size_t ownerScene=0x98,ownerView=0xa0;                          // 0x70, 0x78
constexpr size_t ownerPools=0xa8;            // 0x80 (four pool ids; the feature and resolution pools are the 3rd and 4th)
constexpr size_t ownerMarker=0xd8,ownerFeatureGroup=0xe0;                 // 0xb0, 0xb8
constexpr size_t ownerResolution=0xe8,ownerResolutionGroup=0xf0;          // 0xc0, 0xc8
// The renderer (scene+0x18).
constexpr size_t rendererMemory=0x160;       // 0x50
constexpr size_t rendererGraphics=0x120;     // 0x10
constexpr size_t rendererBuilder=0x128;      // 0x14a70
constexpr size_t rendererSsrSetting2=0x168;  // 0x58
constexpr size_t rendererSsrSetting3=0x16480;// 0x143a0
constexpr size_t rendererSsr=0x16548;        // 0x14470 (two)
// The render camera arguments (the render function's 4th): the head is as before; the tail moved.
constexpr size_t parameterPost=0x250;        // 0x258 (48 bytes; the low health vignette colour +0x10, its alpha +0x1c, the flash +0x2c)
constexpr size_t parameterSharpness=0x308;   // 0x2f8 (then +4, +8)
// The render graph (the render target's +0): arrays of {allocator, data, capacity}.
constexpr size_t graphTextures=0x178;        // 0x68
constexpr size_t graphDescriptors=0x190;     // 0x80 (48-byte descriptors)
constexpr size_t graphCapacity=0x198;        // 0x88
constexpr size_t graphFreeCount=0x238;       // 0xe0
// The graphics backend (graphics+0x278): the Vulkan handles (the dump of game-20261016-040701: the layer's own
// instance and device handles sit there; the physical device is the loader's).
constexpr size_t backendInstance=0x778,backendPhysical=0x780,backendDevice=0x788; // 0x2c8, 0x2d0, 0x2d8
// The device context {device, physical, -, platform} (backend+0x1100 before 1.0): the dump of game-20261016-041440 has
// the device at +0x788, +0x15e8 and +0x1e28, the physical device at +0x780, +0x15f0 and +0x1e20.
constexpr size_t backendContext=0x15e8;
constexpr size_t platformQueues=0xdd0,platformQueueCount=0xdd8; // 0xd98, 0xda0 (0xfe51c0 against 0xdfc530)
// The Scene's model objects (the table at scene+0x1b0, stride 0x1b0 as before). Their head (placement, +0..+0x6f) is
// unchanged; from +0xe8 the fields are 0x10 later (equipment-models/ of Mock runs game-20261015-202041 and
// game-20261016-061640 side by side: the chest's two pointers 0x360 apart at +0xe8/+0xf0 -> +0xf8/+0x100, the hands'
// palette count 42 at +0x134 -> +0x144, the hidden-under-armour mask 0 / the head's ffffffff at +0x104 -> +0x114).
constexpr size_t modelResource=0x108;        // 0xf8
constexpr size_t modelMask=0x114;            // 0x104
constexpr size_t modelPose=0x130;            // 0x120
constexpr size_t modelSdf=0x134;             // 0x124
constexpr size_t modelAuxiliary=0x138;       // 0x128
// Six bookkeeping bytes the Scene itself rewrites in the original of a skinned model with simulated joints when another
// instance of it appears (fishing rods). 1.0 moved them with the rest (+0x10): headset run 2026-10-17 01:55, the fishing
// rod's copy changed bytes 240..245 and the check for June's +0xe0 ended the VR session (motion_visual_failure).
constexpr size_t modelBookkeeping=0xf0;       // 0xe0
constexpr size_t modelDynamic=0x13d;         // 0x12d
constexpr size_t modelPaletteCount=0x144;    // 0x134
constexpr size_t modelPaletteHandle=0x148;   // 0x138
constexpr size_t modelColours=0x1a4;         // 0x1a0: 1.0 keeps three (1101d0 against dc340: +0x1a4+4n; June four from +0x1a0)
constexpr size_t modelColourCount=3;         // 4
constexpr size_t sceneSdf=0x9f8;             // 0x940 (the model SDF owner: 1150c0 reads it where c77d0 read +0x940)
// The model resource a model object points at (modelResource). +0x10 references, +0x14 ready, +0x18 GUID as before;
// a pointer shared by every model came in at +0x30, the rest is 8 later (Mock run game-20261016-064254 resources, and
// fb3b0 reading the SDF / auxiliary ids at +0x6c / +0x70 where c97c0 read +0x64 / +0x68). The body parts' model numbers
// at +0x68 run on one by one as at +0x60 before (3207, 3208 ..; June 930, 931 ..).
constexpr size_t resourceDescriptor=0x38;    // 0x30
constexpr size_t resourceModel=0x68;         // 0x60
constexpr size_t resourceSdf=0x6c;           // 0x64
constexpr size_t resourceAuxiliary=0x70;     // 0x68
// The Scene's pose records (hand poses): pages +0x3c0, count +0x3c8, shift +0x3e8, mask +0x3f8 (0x358 / 0x360 / 0x380 /
// 0x390: f68c0 against c5824), the live count +0x408 (0x3a0, taken with the block), 0x98 bytes an entry (0x70); the record
// in it (entry+8) keeps first joint +8 and count +0xc. The joints (48 bytes each) +0x8c0, capacity +0x8c8 (0x858 / 0x860:
// 102340 against cf360); the scene from +0x148 to about +0x8f0 is 0x68 later throughout.
constexpr size_t scenePosePages=0x3c0,scenePoseCount=0x3c8,scenePoseShift=0x3e8,scenePoseMask=0x3f8,scenePoseLive=0x408,poseEntryBytes=0x98;
constexpr size_t sceneJoints=0x8c0,sceneJointCapacity=0x8c8;
constexpr size_t poseOwnerHierarchy=0x20;     // 0x18: the pose record's owner keeps its hierarchy here (f68c0 against c5824; +0x2c as before)
// RenderModel (a resource's descriptor), from the game's reflection: the hierarchy GUID +0x80 (0x94), the skinning map
// +0xd4 (0x104), the joint count +0x108 (0x138), the hierarchy's joint count +0x10a (0x13a).
constexpr size_t renderModelHierarchy=0x80,renderModelSkinningMap=0xd4,renderModelJointCount=0x108,renderModelHierarchyJoints=0x10a;
}
using Render = void *(__fastcall *)(void *,void *,void *,void *,void *);
using Create = void *(__fastcall *)(void *,const void *);
using Destroy = void (__fastcall *)(void *);
using Wait = bool (__fastcall *)(void *,int64_t);
Render original=nullptr;
uintptr_t baseAddress=0;
Log *logFile=nullptr;
std::atomic<bool> started{false},active{false},claimed{false},done{false};
std::atomic<unsigned> resultCode{4};
bool initializeFeatures=false;
bool initializeResolution=false;
bool renderCalibration=false;
bool retainCalibration=false;
bool sceneCleanup=false;
bool activeXrExitTrial=false,activeContactOwnerTrial=false,earlyInputStopped=false,renderBatchRequested=false;
namespace renderHookRetirement { inline diagnostic::HookDisableBatch batch;inline MH_STATUS disable(void* target)noexcept{return batch.disable(target);} }
std::atomic<bool> activeXrExitOwnerReturned{false},terminalOwnerReady{false};
std::atomic<bool> activeXrExitReadbackRetained{false};
uint32_t calibrationFrames=1;
uint32_t configuredLiveFrames=1; // bounded capture target, preserved when a play session removes the pair limit
bool worldEndurance=false; // explicit separate policy, 3000 pairs / 420 seconds maximum
// Play session (2026-10-06): EVR_WORLD_SESSION_1 <minutes> on top of endurance. No pair limit; the
// series ends at `stop.request` in the calibration directory or at the minute limit, through the
// same final-pair path as a finite series. Logs are name-capped and a heartbeat is written.
bool worldSession=false;unsigned sessionMinutes=0;std::atomic<bool> sessionStop{false};
constexpr uint32_t sessionUnlimitedPairs=0x7fffffffu;
constexpr uint32_t worldWarmupLimit=600;
// Interactive sessions (2026-10-06): when VR starts right after the user moved or was hit, the native camera is still
// blending PlayerCameraDefault -> PlayerCameraIdle (game130115: 1.749 -> 1.724 m over 324 warmup frames). Warm up
// (no layers) until the idle height settled, at most this many extra frames, instead of failing at once.
constexpr uint32_t heightWaitLimit=2400;
bool useRecordedXr=false;
bool preparedSessionProbe=false;
bool detailFactoryProbe=false;
unsigned worldResourceStage=0; // Explicit unused-object diagnostics only; never live stereo.
bool worldNativeDiagnostic=false;
bool privateWaterFeedback=false;
bool earlyWorldXr=false;
bool sharedCullingCamera=false;
unsigned sharedRenderDeferredFrames=0;
// Opt-in desktop skip (2026-10-06, world-desktop-skip.txt): on frames where the eye pair renders, the original
// View's world render is replaced by f7c50's own empty result (the path it takes when Scene+1c is set:
// eb080 copies a zero 0x38 result whose last 32 bytes come from .rdata 12af6e8; c3ab7 then skips its copy).
// Eye 0 executes the shared world updates on those frames. 0 = skip every frame, N>=2 = render 1 of N.
bool desktopSkip=false;unsigned desktopSkipEvery=0;
// Alternate-eye rendering (2026-10-14, user: "try the PureDark way"; docs/analysis/performance_20261014/findings.md 6):
// on the frames that leave the desktop's View out, one eye is drawn - left and right in turn - and the other keeps
// the picture it was drawn with last, at its own pose (layer/native_xr.hpp, openxr_client.hpp eyeMask). The eye that
// is drawn owns the frame's shared updates and the effects, as eye 0 did. EVR_ALTERNATE_EYES=1; first stage: the eye
// left out is not warped to the new pose by the mod (the runtime turns it with the head).
namespace alternateEyes {
inline bool requested=false;inline unsigned parity=0;inline uint64_t asked=0,single=0,turned=0;
constexpr uint32_t warmupPairs=150;   // after the desktop-skip warm-up (120): both eyes have pictures and histories
// The game camera each eye was last drawn with (world).
struct Drawn {bool valid=false;uint64_t session=0;WorldPositionQ32 position{};Quat orientation{0,0,0,1};uint64_t frame=0;};
// This frame's eye cameras as the mod gave them to the game, with the game's near and far (hooks/dlss_eval_observer.hpp:
// the motion vectors of an eye drawn one frame in two, layer/eye_motion_fix.hpp).
inline game::StereoCameras current{};inline float nearZ=0,farZ=0;inline uint64_t currentFrame=0;
inline std::array<Drawn,2> drawn{},pendingDrawn{};   // pendingDrawn: drawn in this frame, taken once it is submitted
// The eye left out in this frame, at the pose in the tracking space (LOCAL) that its picture now belongs to: where its
// game camera was, seen from this frame's anchor. Without that the runtime would show the picture at the pose it was
// drawn at - right while the head alone moves, but a turn by the stick or a step of the character would leave that
// eye one frame behind the other.
struct Stale {uint64_t frame=0;uint32_t eye=0;bool valid=false;XrPosef pose{{0,0,0,1},{0,0,0}};};
inline Stale stale{};
// The world's motion under the tracking space from the last frame to this one (2026-10-23, the mod's own frame for every
// refresh - include/evr/afw_pacing.hpp pacing 2 - carries the walk and the turn by the stick on between the game's frames):
// where a fixed world point goes in the tracking space, from the last frame's anchor to this frame's, as one rigid motion,
// and the time between the two frames' display times (ns).
struct WorldMotion {uint64_t frame=0;bool valid=false;XrPosef motion{{0,0,0,1},{0,0,0}};int64_t dt=0;};
inline WorldMotion worldMotion{};
}
// Mock-only A/B: retain the normal skip policy and temporarily request the
// original View on the next complete frame. No GI/shadow state is reset.
bool desktopCompare=false,desktopCompareRender=false;
unsigned desktopCompareSequence=0;

// Synthetic controller fixtures arm after this many submitted pairs (default 30; world-controller-fixture-arm.txt, 2026-10-09:
// menu tests after desktop skip has started at 120 pairs).
uint32_t controllerFixtureArmPair=30;
constexpr uint32_t desktopSkipWarmupPairs=120;
constexpr uintptr_t emptyRenderResultRva=0x13b9650;
uint64_t desktopSkipCandidates=0,desktopSkipped=0,desktopRendered=0;
// Opt-in desktop discard (world-desktop-discard.txt): the original View renders normally (shared updates, VFX
// simulation, borrowed lights/particles stay intact) but its result is replaced by the empty result, so c3ab7 does
// not consume the desktop image. Measures whether the frame graph drops the unconsumed desktop passes.
bool desktopDiscard=false;uint64_t desktopDiscarded=0;
// Desktop skip renders the original View once in N frames to keep it fresh. With the UI quad that frame's desktop image
// (world behind the HUD panels) became the UI source: the HUD blinked about once a second on the headset (2026-10-11,
// Mock game134955: coverage of the HUD regions jumped on one frame in 30). The refresh render now runs and its result
// is dropped, as on the skipped frames.
bool desktopRefreshDiscard=false;uint64_t desktopRefreshDiscarded=0;
bool giDump=false; // world-gi-dump.txt: read-only GI object dump on pairs 300/301
// VR post effects (2026-10-06, user request: depth of field looks bad in VR, default off). f7c50 applies depth of field
// (40470, 'depth_of_field', incl. near-field blur) only when View+bb0 (DoF state) is set and the byte View+58 is
// non-zero (106b49/106b58); both fields are used for nothing else in f7c50. Eye renders clear View+58 for the call.
bool keepDepthOfField=false;uint64_t dofSuppressed=0;
// VR sharpness (2026-10-06, user request: crisper image). f7c50's post pass 963640 runs the game's NIS sharpen
// (95e4f0) when render parameter +2f8 > 0.01 (106e0b). The game fills +2f8 from renderPreferences.sharpness (c5fb0),
// but with DLSS selected it passes 0 there (the slider goes to DLSS, which has no sharpening since 2.5.1; runtime
// log render_preferences_seen). The eyes get the VR value written into their copy of the parameters instead.
float vrSharpness=-1;
// Experiment (world-desktop-skip-vfx.txt): on skipped frames the left eye View temporarily holds the singleton
// VFX View (original View+738 == VFX scene+e0) so f7c50 runs the VFX stages with the eye camera. Restored to 0
// after the eye call, including exceptions. The right eye has no VFX in this experiment.
bool desktopSkipVfx=false;uint64_t desktopSkipVfxFrames=0,refreshVfxAside=0;
// The VFX owner's culling covers the other eye from this distance outward (parallax included).
constexpr float vfxUnionMinDistance=.5f;uint64_t vfxUnionApplied=0,vfxUnionFailures=0;
void writeEmptyRenderResult(void* out){
    auto* o=static_cast<uint8_t*>(out);
    std::memset(o,0,0x18);
    std::memcpy(o+0x18,reinterpret_cast<const void*>(baseAddress+emptyRenderResultRva),16);
    std::memcpy(o+0x28,reinterpret_cast<const void*>(baseAddress+emptyRenderResultRva),16);
}
uintptr_t provenWorldScene=0,provenWorldView=0;
diagnostic::RecordedXr recordedXr;
bool liveXr=false;
bool matchCameraFov=false;
bool liveOriginInitialized=false;
uint64_t liveOriginSession=0,liveOriginEpoch=0;
// The view's origin taken again (2026-10-18): the runtime's LOCAL space changes seen (its view reset) and one waiting
// for its time, and the reset pose's requests seen (controllerUiFixture::recenterRequests).
uint64_t liveLocalChangesSeen=0,liveRecenterRequestsSeen=0;bool liveRuntimeRecenterPending=false;int64_t liveRuntimeRecenterTime=0;
float liveRenderScale=1;
// Eye View game RenderScale (resolution owner +0x24, wrapper+0x24 is the game's RenderScale; +0x20 before 1.0).
// 1.0 keeps the historical native-resolution eyes; lower values let the game's own temporal
// upscaler (FSR3/DLSS/XeSS per the user's graphics setting) reconstruct the XR target size.
float eyeRenderScale=1.f;
bool eyeDealias=false; // EVR_EYE_DEALIAS_1: dedicated memory for eye View resources (layer side)
// Original (desktop) View game RenderScale while live XR runs: the original View still provides
// the shared stages for both eyes, but its desktop pixels are only a monitor mirror (to be
// replaced by the right-eye mirror, user requirement 2026-10-05 19:53). 0 = leave the game's value.
float desktopRenderScale=0.f;unsigned desktopScaleWrites=0;float desktopScaleBefore=-1.f;uintptr_t desktopScaleWrapper=0;
diagnostic::NativeXrFrame liveFrame;
// The hands of liveFrame as they are shown (include/evr/pose_smoothing.hpp): what draws the hands, the held item and
// the body reads these; strokes, aim, gestures and the pointer read liveFrame.hands as tracked.
// EVR_HAND_SMOOTHING=0 shows the hands as tracked; another number is the cutoff in Hz (default 4; lower is smoother).
evr::motion::HandFrame shownHands;evr::motion::HandSmoothing handSmoothing;bool handSmoothingRead=false;
evr::motion::HitStop hitStop;   // the shown hands stopped a moment when a stroke hits (include/evr/hit_stop.hpp)
// A hand whose tracking drops stays where it was shown (include/evr/pose_smoothing.hpp HandHold; VHVR keeps the hands
// where SteamVR last put them). EVR_HAND_HOLD=0 in the game's environment: as before, hands and body go while a hand
// is not tracked.
evr::motion::HandHold handHold;bool handHoldWanted=true;std::array<evr::motion::HandHold::State,2> handHoldSeen{};unsigned handHoldLogs=0;
diagnostic::NativeMotionHaptic nativeMotionHaptic=nullptr;
diagnostic::NativeGestureHaptic nativeGestureHaptic=nullptr;
Pose liveTrackingToAnchor=identityPose();
// The tracking origin as taken at the start or the view reset; a pad style's tilt (game::padTiltDegrees) turns it every
// frame (2026-10-25: playing lying down).
Pose liveTrackingBase=identityPose();
bool sceneryReplayRequested=false;
game::StableAnchor sceneryReplayReference{};
// EVR_SCENERY_REPLAY_FROM=<frame> (2026-10-21, a Mock of "entering a building": the shadows differ between the eyes for
// a while after it): the camera stays the player's own until that frame, then jumps to the reference.
game::StableAnchor sceneryAnchor(game::StableAnchor native){
    static const uint64_t from=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_SCENERY_REPLAY_FROM",v,sizeof v);return n&&n<sizeof v?std::strtoull(v,nullptr,10):0ull;}();
    // EVR_SCENERY_REPLAY_UNTIL=<frame>: the reference until that frame, the player's own camera after it (from outside in).
    static const uint64_t until=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_SCENERY_REPLAY_UNTIL",v,sizeof v);return n&&n<sizeof v?std::strtoull(v,nullptr,10):~0ull;}();
    return sceneryReplayRequested&&liveFrame.frame>=from&&liveFrame.frame<until?game::replayAnchor(sceneryReplayReference,liveTrackingToAnchor):native;
}
bool runtimeEyes(){return useRecordedXr||liveXr;}
uint32_t eyeWidth(size_t eye){return liveXr?liveFrame.width[eye]:recordedXr.width[eye];}
uint32_t eyeHeight(size_t eye){return liveXr?liveFrame.height[eye]:recordedXr.height[eye];}
using ConvertProjection=void* (__fastcall *)(void*,const void*,float);
ConvertProjection originalProjection=nullptr;
void* projectionTarget=nullptr;
bool projectionEnabled=false;
thread_local const game::ProjectionDescriptor* eyeProjection=nullptr;
thread_local unsigned projectionCalls=0;
namespace fogProjection { void convert(void*,const void*,uintptr_t) noexcept; }
void* __fastcall convertEyeProjection(void* output,const void* camera,float aspect){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto result=originalProjection(output,camera,aspect);
    if(eyeProjection&&caller==baseAddress+0x12af4d){std::memcpy(output,eyeProjection->bytes.data(),80);++projectionCalls;}
    fogProjection::convert(output,camera,caller);
    return result;
}
std::filesystem::path calibrationDirectory;
using DestroyOwner=void (__fastcall *)(void*);
DestroyOwner originalDestroyOwner=nullptr;
void* destroyOwnerTarget=nullptr;
std::atomic<bool> destroyOwnerEnabled{false};
RENDERDOC_API_1_6_0* captureApi=nullptr;
bool captureOpen=false;
bool captureLiveFinal=false,capturedLiveFinal=false,captureOnRequest=false,captureGlide=false,capturedGlide=false;
// Captures on request (the VR bug report button on the headset): up to this many in a session (2026-10-18 night: the
// grass, the trees and the rocks whose right-eye picture differs, one capture each). The second and later go to
// world_final_<n> (RenderDoc adds its own ending).
constexpr unsigned maxRequestedCaptures=3;unsigned requestedCaptures=0;
bool captureInitialFogSdf=false,capturedInitialFogSdf=false;
bool initialFogUploadObserved=false;
unsigned initialFogCaptureFrames=0;
struct PrivateSsr {
    uintptr_t object=0;
    // 152ce0 does not initialize resolution handles. Set only after the native
    // 152f00 call returns, clear after 153fe0; never infer this from allocation.
    bool resolutionInitialized=false;
};
struct WaterFeedbackState {
    uintptr_t source=0,owner=0;
    std::array<uintptr_t,2> pools{};
    DWORD thread=0;
    uint64_t firstSerial=0,lastMerged=0;
    unsigned packetCaptures=0;
    std::array<uint64_t,2> lastProduced{};
    std::map<uint64_t,uint8_t> producedEyes;
    bool failed=false;
    water::Merged merged;
};
struct PairResources {
    uintptr_t scene=0,originalView=0,graph=0,graphics=0;
    uint64_t serial=0;
    uint64_t xrAttemptSerial=0;
    bool pairPending=false;
    DWORD renderThread=0;
    uint32_t pairedFrames=0;
    bool sessionFinal=false; // play session: this pair closed native XR after a stop request
    uint32_t warmupFrames=0,heightWaitFrames=0;
    std::array<uintptr_t,2> created{};
    std::array<uint32_t,2> groups{};
    std::array<std::array<uint32_t,3>,2> memoryPools{}; // privateMemoryPools::EyePools (feature, resolution, transient)
    std::unique_ptr<WaterFeedbackState> water;
    struct CullingHistory {
        uintptr_t manager=0,graph=0;DWORD thread=0;uint32_t slots=0;
        std::array<uint8_t,16> source{};
        std::array<uint8_t,16> generatedCountSource{};
        uint64_t generatedResetSerial=0;int generatedOwnerEye=-2;bool generatedResetValid=false;
        std::array<uint64_t,3> generatedCalls{};uint64_t generatedResets=0,generatedBorrows=0,generatedAuditedEyes=0;
        std::array<std::array<uint8_t,16>,2> handles{};
        std::array<bool,2> initialized{};
        std::array<bool,2> fillOnlyObserved{};
        std::array<uint64_t,2> substitutions{},resets{};
        bool failed=false;
    } history;
    std::array<std::array<PrivateSsr,2>,2> privateSsr{};
    alignas(16) std::array<std::array<uint8_t,layout::ownerBytes>,2> owners{};
    alignas(16) std::array<std::array<uint8_t,layout::parameterBytes>,2> parameters{};
    alignas(16) std::array<uint8_t,layout::parameterBytes> sharedParameters{};
    std::array<std::array<uint8_t,16>,2> targets{},aliases{};
    // The render target argument: 1.0 {handle 16, +0x10 an optional object the render waits on, +0x18 the output kind
    // (0: the handle itself), +0x20} = 40 bytes (24 before: the kind at +0x10). The eyes pass their image, kind 0, no wait.
    std::array<std::array<uint8_t,40>,2> targetArguments{};
    std::array<std::array<uint8_t,56>,2> results{};
    bool recorded=false,failed=false;
    bool frameReady=true;
};
std::atomic<PairResources*> retainedPair{nullptr};
std::atomic<uint32_t> renderCallbacks{0};
struct RenderCallbackScope {
    RenderCallbackScope(){renderCallbacks.fetch_add(1,std::memory_order_acq_rel);}
    ~RenderCallbackScope(){renderCallbacks.fetch_sub(1,std::memory_order_acq_rel);}
};
bool awaitRenderCallbacks() noexcept {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    while(renderCallbacks.load(std::memory_order_acquire)){
        if(std::chrono::steady_clock::now()>=deadline)return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}
constexpr uintptr_t renderRva=0x129fc0;
// Bit 15 is native FSR3. Its resolution object at View+9b8 is individually
// constructed by 52960 and destroyed by 537b0, like the per-View DLSS context.
// 1.0 (2026-10-16): the world View carries bits 6 and 16 as well (the creation log names OcclusionCulling, Shadows, Vfx,
// Fog, Bloom, Sdf, DetailScattering, Gi, GrassMovement, DynamicHeightmap, ContactShadows2, Fsr3, StochasticScreenSpace-
// Reflections, ManyLightsShadows, GpuBvh, SnowDisplacement, Water, BrickRenderer, FrostedGlass, WorldSdf); the game's own
// feature setup (0x103a20) allocates for the mask, so they are accepted - which bit is which is not yet pinned down.
constexpr uint32_t worldKnownFeatures=0x1cfa7ffu;
constexpr const char *renderPattern="4C 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 48 8D AC";
template<class T> T read(uintptr_t address) {
    T value{}; SIZE_T count=0;
    if(!address || !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void *>(address),&value,sizeof(value),&count) || count!=sizeof(value))
        throw std::runtime_error("unreadable view metadata at 0x"+[&]{char t[24];snprintf(t,sizeof t,"%llx",static_cast<unsigned long long>(address));return std::string(t);}());
    return value;
}
std::string hex(uintptr_t value) { std::ostringstream out; out<<"\"0x"<<std::hex<<value<<'"'; return out.str(); }
// Clears View+58 (depth of field request) for one eye render, restored after the eye's CPU tasks.
struct DofScope {
    uintptr_t view=0;uint8_t saved=0;
    DofScope(uintptr_t v,bool off){
        if(!off||!read<uintptr_t>(v+layout::viewDepthOfField))return;saved=read<uint8_t>(v+0x6c);if(!saved)return;
        view=v;*reinterpret_cast<uint8_t*>(v+0x6c)=0;
        if(++dofSuppressed<=3||dofSuppressed%1000==0)
            logFile->event("vr_depth_of_field_suppressed","\"frames\":"+std::to_string(dofSuppressed)+",\"flag\":"+std::to_string(saved));
    }
    ~DofScope(){if(view)*reinterpret_cast<uint8_t*>(view+0x6c)=saved;}
    DofScope(const DofScope&)=delete;DofScope& operator=(const DofScope&)=delete;
};
struct VfxTakeoverScope {
    uintptr_t view=0;
    VfxTakeoverScope(uintptr_t scene,uintptr_t originalView,uintptr_t eyeView,bool use){
        if(!use)return;
        const auto vfx=read<uintptr_t>(originalView+layout::viewVfx),vfxScene=read<uintptr_t>(scene+layout::sceneVfx);
        if(!vfx||!vfxScene||vfxScene+layout::vfxSceneView!=vfx||read<uintptr_t>(eyeView+layout::viewVfx))
            throw std::runtime_error("VFX takeover topology differs");
        *reinterpret_cast<uintptr_t*>(eyeView+layout::viewVfx)=vfx;view=eyeView;++desktopSkipVfxFrames;
        // The eye Views are made without the render feature Vfx (bit 2, keen::RenderFeature), so the owner eye - with the
        // VfxView, but without the feature - drew none of the VFX models after its lighting (headset reports of 2026-10-17:
        // 4 "the stars in the right eye only", 15 "the tool's circle differs", 19 "the building's blue in one eye";
        // RenderDoc game-20261017-034858: the emissive draw and the area's volume draw in the other, bridged eye only).
        // For the owner's frame the bit is set as well as the VfxView, and cleared again with it (Mock game-20261017-062743:
        // the stars in both eyes, the emissive pass 3 / 3 and the transparent pass 2 / 2 draws in the two eyes).
        // EVR_VFX_OWNER_FEATURE=0 leaves the bit off.
        if(ownerFeature&&!(read<uint32_t>(eyeView+0x10)&4u)){*reinterpret_cast<uint32_t*>(eyeView+0x10)|=4u;featureSet=true;}
    }
    inline static const bool ownerFeature=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VFX_OWNER_FEATURE",v,sizeof v);return !(n==1&&v[0]=='0');}();
    bool featureSet=false;
    ~VfxTakeoverScope(){if(view){*reinterpret_cast<uintptr_t*>(view+layout::viewVfx)=0;if(featureSet)*reinterpret_cast<uint32_t*>(view+0x10)&=~4u;}}
    VfxTakeoverScope(const VfxTakeoverScope&)=delete;VfxTakeoverScope& operator=(const VfxTakeoverScope&)=delete;
};
#include "world_lifecycle_observer.hpp"
#include "combat_world_bridge.hpp"
#include "combat_epoch_gate.hpp"
#include "private_memory_pools.hpp"
#include "shared_resolution.hpp"
#include "detail_factory_probe.hpp"
#include "detail_binding_bridge.hpp"
#include "shadow_projection_bridge.hpp"
#include "fog_projection_bridge.hpp"
#include "world_ready_probe.hpp"
#include "world_shared_probe.hpp"
#include "world_shared_stages.hpp"
#include "terrain_ready_probe.hpp"
#include "water_feedback_bridge.hpp"
#include "download_feedback_probe.hpp"
#include "camera_control_observer.hpp"
extern "C" void evr_observe_camera_sum(const uintptr_t* regs,uint8_t* fx) noexcept {cameraControl::onSum(regs,fx);}
#include "dlss_eval_observer.hpp"
#include "root_camera_bridge.hpp"
// When the other eye may run its own terrain update (hooks/world_shared_stages.hpp terrain::eyeOwnAllowed): 120 XR frames
// after the first ask, and again 120 after the game camera jumped 20 m or more between two asks (a teleport, a load) - the
// times the bricks around a camera are asked for all at once.
namespace terrainEyeGate {
inline uint64_t quietUntil=0;inline bool started=false,haveLast=false;inline WorldPositionQ32 last{};inline unsigned jumps=0;
constexpr uint64_t settleFrames=120;constexpr double jumpMeters=20;
inline bool allow(){
    const uint64_t frame=liveFrame.frame;
    if(!started){started=true;quietUntil=frame+settleFrames;}
    try{rootCamera::Sample s;
        if(rootCamera::trySnapshot(s)){const auto p=s.stable.position;
            if(haveLast){const double dx=relativeQ32(p.x,last.x),dy=relativeQ32(p.y,last.y),dz=relativeQ32(p.z,last.z);
                if(dx*dx+dy*dy+dz*dz>jumpMeters*jumpMeters){quietUntil=frame+settleFrames;
                    if(jumps++<20)logFile->event("terrain_eye_gate_jump","\"frame\":"+std::to_string(frame)+",\"metres\":"+std::to_string(std::sqrt(dx*dx+dy*dy+dz*dz))+",\"held_until\":"+std::to_string(quietUntil));}}
            last=p;haveLast=true;}
    }catch(...){return false;}
    return frame>=quietUntil;
}
inline const bool installed=(worldSharedStages::terrain::eyeOwnAllowed=&allow,true);
}
#include "input_pose_bridge.hpp"
#include "motion_collider_channel.hpp"
#include "blocked_action_lifetime.hpp"
#include "input_direction_observer.hpp"
// The pad's turn watch reads the camera update's sample (controller_ui_fixture.hpp cameraTurn, 2026-10-24).
const bool cameraTurnWired=[]{controllerUiFixture::cameraTurn=[]{controllerUiFixture::CameraTurn t;std::lock_guard<std::mutex> lock(cameraControl::mutex);
    const auto& s=cameraControl::latest;t.valid=s.valid;t.camera=s.camera;t.yaw=s.weighted[0];t.ms=s.milliseconds;t.heldUntil=cameraControl::turnHeldUntil.load();return t;};return true;}();
#include "bow_spawn_observer.hpp"
#include "controller_backend_exit.hpp"
#include "ui_state_observer.hpp"
#include "equipment_observer.hpp"
#include "culling_history_bridge.hpp"
bool finishEyeCpuTasks(uintptr_t scene,unsigned milliseconds=100);
#include "weapon_grip_preview.hpp"
#include "motion_visual_bridge.hpp"
#include "motion_hand_visual.hpp"
#include "motion_bow_visual.hpp"
#include "motion_shield_visual.hpp"
#include "motion_arrow_visual.hpp"
#include "motion_offhand_visual.hpp"
#include "motion_consumable_visual.hpp"
#include "live_throw_aim.hpp"
#include "motion_fist.hpp"
#include "vfx_emitter_census.hpp"
#include "motion_body_ik.hpp"
#include "steam_link.hpp"
#include "remote_body.hpp"
#include "remote_consumable_visual.hpp"
#include "online_sync.hpp"
#include "held_vfx_bridge.hpp"
#include "motion_contact_observer.hpp"
#include "motion_hit_observer.hpp"
#include "native_attribute_registry.hpp"
#include "motion_target_observer.hpp"
#include "live_melee_targets.hpp"
#include "effect_entity_census.hpp"
#include "target_mark.hpp"
#include "live_actionbar_pages.hpp"
#include "live_equipment.hpp"
#include "holster_actions.hpp"
#include "motion_mod_bow_visual.hpp"
#include "motion_event_selection_observer.hpp"
namespace inputActor {inline const bool clientTicksSet=[]{clientWorldTicks=[](uintptr_t w,uint32_t e){liveTargets::tick(w,e);liveActionbars::tick(w,e);liveEquipment::tick(w,e);fogTimer::tick(w,e);};return true;}();}
#include "contact_quiescence_observer.hpp"
#include "scenery_culling_observer.hpp"
#include "head_visibility_bridge.hpp"
#include "body_upload_observer.hpp"
#include "vfx_transparent_bridge.hpp"
#include "vfx_cpu_draw_probe.hpp"
#include "vfx_decal_rebase.hpp"
#include "grass_movement_bridge.hpp"
#include "snow_displacement_bridge.hpp"
#include "parry_observer.hpp"
#include "melee_trigger_observer.hpp"
#include "opportunity_take_observer.hpp"
#include "sequence_request_observer.hpp"
#include "ranged_special_probe.hpp"
#include "water_displacement_bridge.hpp"
#include "hit_event_intake.hpp"
#include "world_ray.hpp"
#include "shadow_history_probe.hpp"
#include "shared_point_lights.hpp"
#include "stable_frustum_bridge.hpp"
#include "vfx_spring_rebase.hpp"
#include "vfx_model_bridge.hpp"
#include "vfx_gate_probe.hpp"
#include "gi_share_bridge.hpp"
#include "object_watch.hpp"
#include "status_vignette.hpp"
#include "combat_camera_override.hpp"
#include "bug_report.hpp"
#include "pad_settings.hpp"
#include <tlhelp32.h>
#include "data_watch.hpp"
// VR status tint (2026-10-06): colour/coverage handed to the layer once per eye pair (evrSetEdgeTint).
namespace edgeTintBridge {
using SetFn=int(*)(float,float,float,float,float);
inline SetFn set=nullptr;inline bool resolved=false;
inline float maxStrength=0.5f;constexpr float band=0.12f; // "a slightly red or blue rim is enough"; strength from the policy
// Channel multipliers at full strength: low health keeps red and dims green/blue, freezing keeps blue.
constexpr std::array<float,3> lowHealth{1.0f,0.2f,0.2f},freezing{0.45f,0.7f,1.0f};
inline void apply(float r,float g,float b,float a){
    if(!resolved){resolved=true;if(auto m=GetModuleHandleW(L"evr_diagnostic.dll"))set=reinterpret_cast<SetFn>(GetProcAddress(m,"evrSetEdgeTint"));
        logFile->event("edge_tint_bridge",std::string("\"resolved\":")+(set?"true":"false"));}
    if(set&&set(r,g,b,a,band))throw std::runtime_error("edge tint parameters rejected");
}
}
// HUD auto-hide (include/evr/hud_fade.hpp, user 2026-10-12): the kind of screen the game shows goes to the layer once
// per eye pair (evrSetUiHud); the layer fades the HUD outside the middle of the UI image.
namespace uiHudBridge {
using SetFn=int(*)(int);
inline bool requested=false,resolved=false;inline SetFn set=nullptr;inline int last=-1;
inline void apply(){
    if(!requested)return;
    if(!resolved){resolved=true;if(auto m=GetModuleHandleW(L"evr_diagnostic.dll"))set=reinterpret_cast<SetFn>(GetProcAddress(m,"evrSetUiHud"));
        logFile->event("ui_hud_bridge",std::string("\"resolved\":")+(set?"true":"false"));}
    const auto now=GetTickCount64();const auto at=controllerUiFixture::liveScreenAt.load();
    // While the game loads (cameras 1..3, or no sample of the player yet - as for the desktop skip) the loading
    // screen is not a HUD to hide: Mock run game203326 faded it after five seconds.
    bool loading=false;try{rootCamera::Sample root;loading=!rootCamera::trySnapshot(root)||root.cameraId<=3;}catch(const std::exception&){loading=true;}
    const int mode=!loading&&at&&now>=at&&now-at<=500&&controllerUiFixture::liveScreenGameplay.load()?1:2;
    if(set&&set(mode))throw std::runtime_error("UI HUD mode rejected");
    if(mode!=last){last=mode;logFile->event("ui_hud_mode","\"mode\":"+std::to_string(mode)+",\"gameplay_hud\":"+(mode==1?"true":"false"));}
}
}
// The pad styles' online (2026-10-25, user: "whichever mode is chosen, no problem online"): as the mod without VR
// (onlineFlat::frame) - the own character's place goes out (the others' VR bodies are sent here and shown on their
// characters), no body of its own (no tracked hands: the native animation, as any player's). Once per eye pair, at
// the original world boundary (as motionVisual::prepareContext).
namespace padOnline {
inline unsigned frames=0,skipped=0;
inline void frame(uintptr_t scene,uintptr_t view,uint64_t serial) noexcept {
    const ObserverLastError preserve;
    try{
        if(!onlineSync::requested)return;
        if(!finishEyeCpuTasks(scene,4)){++skipped;return;}
        motionVisual::scene=scene;motionVisual::view=view;motionVisual::thread=GetCurrentThreadId();
        motionVisual::contextReady=true;motionVisual::contextSerial=serial;motionVisual::contextFrame=liveFrame.frame;
        equipmentObserver::Snapshot own;{std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);own=equipmentObserver::latest;}
        const auto local=rootCamera::snapshot().entity;
        if(own.number&&own.entity==local&&own.scene==scene&&GetTickCount64()-own.milliseconds<=1500){
            std::array<double,3> at{};if(onlineSync::placeOf(own.handles,at)){onlineSync::flatPlace=sync::place(at);onlineSync::flatPlaced=true;}
            motionHandVisual::rigFromSource(own.handles[11]);
        }
        motionBodyIk::published.have=false;
        onlineSync::frame(serial);++frames;
    }catch(const std::exception& e){static unsigned logs=0;if(logs++<5)try{logFile->event("pad_online_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
    catch(...){}
}
}
void configureControllerFixture(const std::filesystem::path& path,unsigned duration,bool world){
        controllerUiFixture::requested=std::filesystem::exists(path/"controller-ui-fixture.txt");
        if(controllerUiFixture::requested){
            std::ifstream flag(path/"controller-ui-fixture.txt");std::string value;flag>>value>>std::ws;
            controllerUiFixture::motionGuardRequested=value=="EVR_MOTION_GUARD_1";
            controllerUiFixture::guardRequested=value=="EVR_CONTROLLER_GUARD_1"||controllerUiFixture::motionGuardRequested;
            if(controllerUiFixture::guardRequested){
                if(!world||!motionHandVisual::requested||!worldEndurance||!weaponGripPreview::requested||nativeGripObserver::requested||motionVisual::equipmentSwitchRequested)
                    throw std::runtime_error("guard fixture needs isolated measured hand/weapon rendering");
                actionSequenceObserver::requested=true;actionSequenceObserver::recording=false;
                logFile->event("action_sequence_enabled","\"bytes\":3104,\"max_samples\":4096,\"read_only\":true,\"time_unit_known\":false");
                controllerUiFixture::observeGuard=[](){
                    actionSequenceObserver::Status a;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);a=actionSequenceObserver::latest;}
                    game::GuardObservation o;const auto root=rootCamera::snapshot();const auto now=GetTickCount64();
                    const auto m=attachmentObserver::meleeAppearance(a.entity,now);
                    o.root=a.entity;o.item=m.item;o.canBlock=a.canBlock;o.blocking=a.blocking;o.digital=a.digital;o.binding=(uint64_t(m.child)<<32)|m.item;o.attacking=a.active&&a.trigger!=8&&a.trigger!=9;
                    o.valid=a.alive&&a.ready&&m.valid&&m.item==weaponGripPreview::item&&a.entity==root.entity&&rootCamera::firstPerson(root)&&
                        now>=a.milliseconds&&now-a.milliseconds<=200&&now>=root.milliseconds&&now-root.milliseconds<=200;
                    if(controllerUiFixture::motionGuardRequested)o.valid=o.valid&&m.weaponCategoryKnown&&m.weaponType==weaponGripPreview::type&&m.modelGuid==weaponGripPreview::weaponGuid;
                    return o;
                };
                if(controllerUiFixture::motionGuardRequested){
                    std::ifstream file(path/"world-motion-guard.txt");std::string magic;Vec3 offset;uint64_t id;motion::GuardCalibration c;float tolerance;
                    file>>magic>>offset.x>>offset.y>>offset.z>>id>>c.shieldEnterFront>>c.shieldExitFront>>c.twoHandEnterFront>>c.twoHandExitFront
                        >>c.releaseHeightMargin>>c.overheadHeight>>c.overheadBackAllowance>>c.maximumHandSpeed>>c.maximumHandAngularSpeed>>c.maximumGap>>c.rearmOutsideTime>>tolerance>>std::ws;
                    if(!file||!file.eof()||magic!="EVR_SYNTHETIC_GUARD_1"||!c.valid()||!std::isfinite(tolerance)||tolerance<=0||tolerance>=.1f)throw std::runtime_error("explicit synthetic body calibration required");
                    auto w=weaponGripPreview::weapon;w.twoHanded=true;w.offHandTolerance=tolerance;
                    controllerUiFixture::motionGuard=std::make_unique<game::MotionGuardFixture>(c,w);
                    controllerUiFixture::guardBodyTracker=std::make_unique<motion::GuardBodyTracker>(offset,id);
                    logFile->event("motion_guard_enabled","\"synthetic_calibration\":true,\"holds\":2,\"native_button\":256,\"parry_state_writes\":false,\"startup_requires_lowering\":true");
                }
                logFile->event("guard_fixture_enabled","\"buttons_only\":[256],\"holds\":2,\"attack_commands\":false,\"native_state_writes\":false");
            }
            controllerUiFixture::equipmentRequested=value=="EVR_CONTROLLER_EQUIPMENT_1";
            if(controllerUiFixture::equipmentRequested){
                if(!world||!motionHandVisual::requested||!worldEndurance||!motionVisual::equipmentSwitchRequested||!weaponGripPreview::requested||nativeGripObserver::requested)
                    throw std::runtime_error("equipment fixture needs isolated measured hand/weapon rendering");
                controllerUiFixture::equipment=game::ControllerEquipmentFixture(weaponGripPreview::item);
                controllerUiFixture::observeEquipment=[](){
                    game::EquipmentObservation o;const auto root=rootCamera::snapshot();const auto now=GetTickCount64();
                    o.valid=rootCamera::firstPerson(root)&&now>=root.milliseconds&&now-root.milliseconds<=200;o.root=root.entity;
                    if(o.valid){const auto held=motionVisual::appearance(root.entity,false);o.item=held.item;o.child=held.entity;o.handle=held.handle;}return o;
                };
                logFile->event("equipment_fixture_enabled","\"initial_item\":"+std::to_string(weaponGripPreview::item)+",\"buttons_only\":[4,8],\"attack_commands\":false,\"selection_writes\":false");
            }
            const auto* wheelSchedule=evr::motion::quickWheelSchedule(value.c_str());
            controllerUiFixture::wheelRequested=wheelSchedule!=nullptr;
            if(wheelSchedule){controllerUiFixture::wheelProgress=evr::motion::QuickWheelProgress(*wheelSchedule);motionVisual::suspendOnEquipmentChange=true;}
            controllerUiFixture::routerEquipRequested=value=="EVR_GRIP_ROUTER_EQUIP_1"||value=="EVR_GRIP_ROUTER_EQUIP_2";
            controllerUiFixture::routerRequested=value=="EVR_GRIP_ROUTER_WHEEL_1"||controllerUiFixture::routerEquipRequested;
            if(controllerUiFixture::routerEquipRequested)controllerUiFixture::observeActionbar=[](){std::lock_guard<std::mutex> lock(inputRanged::actionbarMutex);return inputRanged::actionbar;};
            if(controllerUiFixture::routerRequested){
                std::ifstream file(path/"world-grip-router.txt");std::string magic;Vec3 offset;uint64_t id;motion::GripRouterCalibration c;
                file>>magic>>offset.x>>offset.y>>offset.z>>id
                    >>c.rightShoulder.right>>c.rightShoulder.up>>c.rightShoulder.forward>>c.leftShoulder.right>>c.leftShoulder.up>>c.leftShoulder.forward
                    >>c.rightHip.right>>c.rightHip.up>>c.rightHip.forward>>c.leftHip.right>>c.leftHip.up>>c.leftHip.forward
                    >>c.enterRadius>>c.exitRadius>>c.calmSpeed>>c.calmTime>>c.longPress>>c.maximumGap;
                // v2 appends the head-relative shoulder reach: below above lateral forward keepMargin.
                const bool reach=magic=="EVR_SYNTHETIC_GRIP_ROUTER_2";
                if(reach)file>>c.shoulderReach.below>>c.shoulderReach.above>>c.shoulderReach.lateral>>c.shoulderReach.forward>>c.shoulderReach.keepMargin;
                file>>std::ws;
                if(!file||!file.eof()||(magic!="EVR_SYNTHETIC_GRIP_ROUTER_1"&&!reach)||reach!=c.shoulderReach.enabled()||!c.valid())throw std::runtime_error("explicit synthetic grip router calibration required");
                if((value=="EVR_GRIP_ROUTER_EQUIP_2")!=reach)throw std::runtime_error("overlap equip fixture requires the v2 shoulder reach profile");
                controllerUiFixture::gripRouter=std::make_unique<motion::GripRouter>(c);
                controllerUiFixture::routerBody=std::make_unique<motion::GuardBodyTracker>(offset,id);
                logFile->event("grip_router_fixture_enabled","\"synthetic_calibration\":true,\"rb_source\":\"grip_router_only\",\"equip_executed\":false,\"long_press_ns\":"+std::to_string(c.longPress)+
                    ",\"shoulder_reach\":"+(reach?"["+std::to_string(c.shoulderReach.below)+","+std::to_string(c.shoulderReach.above)+","+std::to_string(c.shoulderReach.lateral)+","+std::to_string(c.shoulderReach.forward)+","+std::to_string(c.shoulderReach.keepMargin)+"]":std::string("null"))+
                    ",\"gesture_blocks_grip\":false");
            }
            // 2026-10-09 real-device phase 1: continuous controller input (live_pad.hpp), no fixture schedule.
            controllerUiFixture::liveRequested=value=="EVR_CONTROLLER_LIVE_1";
            if(controllerUiFixture::liveRequested){
                if(!world||!motionHandVisual::requested||!worldEndurance)throw std::runtime_error("live controller input needs tracked hands in a world session");
                const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
                nativeGestureHaptic=layer?reinterpret_cast<diagnostic::NativeGestureHaptic>(GetProcAddress(layer,"evrNativeGestureHaptic")):nullptr;
                if(!nativeGestureHaptic)throw std::runtime_error("live input gesture haptic bridge missing");
                // Real-device run game-20261009-103033 stopped when the user picked another item on the quick wheel:
                // the tracked weapon must go dormant on an equipment change (and rebind on return), as in the wheel trials.
                motionVisual::suspendOnEquipmentChange=true;
                motionVisual::liveWeaponMount=true;
                if(std::filesystem::exists(calibrationDirectory/"world-two-hand-wield.txt")){
                    std::ifstream wf(calibrationDirectory/"world-two-hand-wield.txt");std::string wm;wf>>wm>>std::ws;
                    // _1: the measured-contact trial (support hand on the shaft, right grip pressed again).
                    // _2: VHVR's entry rule (left grip, hands along the weapon); see live_wield.hpp.
                    // _3: the same, and measured assets also take the catalog group's mount (diagnostic).
                    const bool catalogOnly=wm=="EVR_LIVE_TWO_HAND_WIELD_3",vhvrEntry=catalogOnly||wm=="EVR_LIVE_TWO_HAND_WIELD_2";
                    if(!wf||!wf.eof()||(wm!="EVR_LIVE_TWO_HAND_WIELD_1"&&!vhvrEntry)||weaponGripPreview::requested)throw std::runtime_error("invalid live wield policy");
                    motionVisual::twoHandWieldRequested=true;motionVisual::twoHandCatalogOnly=catalogOnly;
                    if(vhvrEntry)motionVisual::wieldState=motion::LiveWield(true);
                    logFile->event("live_two_hand_wield_enabled","\"display_only\":true,\"measured_assets_only\":"+std::string(vhvrEntry?"false":"true")+",\"combat_authorized\":false,\"entry\":"+
                        std::string(vhvrEntry?"\"vhvr\"":"\"contact\"")+",\"left_grip_only\":"+(vhvrEntry?"true":"false")+",\"catalog_only\":"+(catalogOnly?"true":"false"));
                }
                if(std::filesystem::exists(calibrationDirectory/"world-body-ik.txt")){
                    std::ifstream bf(calibrationDirectory/"world-body-ik.txt");std::string bm;bf>>bm>>std::ws;
                    if(!bf||!bf.eof()||bm!="EVR_BODY_IK_1")throw std::runtime_error("invalid body ik policy");
                    motionBodyIk::requested=true;
                    // keen::actor::StateFlag FloatingOnWater (43), UnderWater (44) of the local actor, as the input patch sees it.
                    motionBodyIk::swimming=[](uint64_t now){
                        inputHeadIdle::State state;{std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);state=inputHeadIdle::latest;}
                        return state.actorKnown&&state.milliseconds&&now>=state.milliseconds&&now-state.milliseconds<=500&&(state.actorState&game::swimmingStates)!=0;};
                    controllerUiFixture::jumpPressed=&inputActor::localJump::own::pressSeen;   // a guest's double jump (2026-10-22)
                    controllerUiFixture::clientNowFn=&nativeImpactComponents::clientNow;
                    logFile->event("motion_body_ik_enabled","\"experiment\":true,\"native_pose_written\":false,\"rebinds_local_body_parts\":true,\"solver\":\"evr/body_ik.hpp\"");
                    onlineSync::configure();
                }
                if(std::filesystem::exists(path/"world-weapon-mount.txt")){
                    std::ifstream mount(path/"world-weapon-mount.txt");std::string magic;
                    mount>>magic>>motionVisual::weaponPitchDegrees;mount>>std::ws;
                    if(!mount||!mount.eof()||magic!="EVR_WEAPON_MOUNT_1")throw std::runtime_error("invalid live weapon mount policy");
                    motion::mountLiveWeapon(identityPose(),motionVisual::weaponPitchDegrees);
                }
                logFile->event("live_weapon_mount_enabled","\"reference\":\"VHVR equipped/unequipped target delta\",\"pitch_degrees\":"+std::to_string(motionVisual::weaponPitchDegrees)+",\"hand_pose_unchanged\":true,\"asset_pivot_calibrated\":false");
                // Real-device feedback (10-09): jump/dodge/air used the body heading; live play steers them with the head.
                inputHeadIdle::vrLocomotion=true;
                // Weapons other than the calibrated axe floated on the hidden native body: hide every held item.
                motionVisual::hideHeldAttachments=true;
                // game-20261009-103959: a full-screen UI (map, frontend screen 2) stops the player camera update.
                // The camera update resumes a few frames after the HUD returns (Mock game-20261009-104954: 17 ms gap after
                // closing the system screen), so the hold also covers 1.5 s after the last non-HUD screen.
                rootCamera::uiHold=[]()->bool{const auto last=inputUi::lastNonHudMs.load();const auto now=GetTickCount64();
                    return last&&now>=last&&now-last<=1500;};
                logFile->event("controller_live_enabled","\"phase\":1,\"table\":\"gameplay A/Y/RB, menus A/B/X/Y/RB/LB, right stick up L3 down B, temple D-pad/Start/Select\""
                    ",\"motion_attack\":false,\"posture_guard\":false,\"equip_zones\":false,\"time_limit\":false");
            }
            controllerUiFixture::verticalRequested=value=="EVR_VERTICAL_STICK_1";
            const bool leftMenu=value=="EVR_HEAD_GESTURE_MENU_LEFT_1";
            controllerUiFixture::gestureRequested=value=="EVR_HEAD_GESTURE_MENU_1"||leftMenu||controllerUiFixture::verticalRequested||controllerUiFixture::wheelRequested;
            if(controllerUiFixture::gestureRequested){
                if(!world||!motionHandVisual::requested||!worldEndurance)throw std::runtime_error("gesture needs tracked hands endurance");
                const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
                nativeGestureHaptic=layer?reinterpret_cast<diagnostic::NativeGestureHaptic>(GetProcAddress(layer,"evrNativeGestureHaptic")):nullptr;
                if(!nativeGestureHaptic)throw std::runtime_error("gesture haptic bridge missing");
                if(controllerUiFixture::wheelRequested){
                    std::string cycles;for(size_t i=0;i<wheelSchedule->count;i++)cycles+=std::string(i?",":"")+"["+std::to_string(wheelSchedule->cycles[i].open)+","+std::to_string(wheelSchedule->cycles[i].close)+"]";
                    logFile->event("quick_wheel_fixture_enabled","\"buttons_only\":[512],\"native_state_writes\":false,\"end_frame\":"+std::to_string(wheelSchedule->end)+
                        ",\"schedule\":"+jsonString(wheelSchedule->token)+",\"open_cycles\":["+cycles+"],\"close_grace\":"+std::to_string(wheelSchedule->closeGrace));
                }
                else if(controllerUiFixture::verticalRequested)logFile->event("vertical_stick_fixture_enabled","\"buttons_only\":[64,8192],\"native_state_writes\":false,\"end_frame\":750");
                else logFile->event("head_gesture_fixture_enabled","\"menu_only\":true,\"automatic_confirmation\":false,\"end_frame\":650,\"hand\":"+std::string(leftMenu?"\"left\"":"\"right\""));
            }
            controllerUiFixture::motionAttackRequested=value=="EVR_MOTION_ATTACK_1"||value=="EVR_MOTION_ATTACK_REPEAT_2";
            controllerUiFixture::motionAttackLimit=value=="EVR_MOTION_ATTACK_REPEAT_2"?2u:1u;
            if(controllerUiFixture::motionAttackRequested)controllerUiFixture::swing.setAttackLimit(controllerUiFixture::motionAttackLimit);
            if(std::filesystem::exists(path/"world-head-combat.txt")){
                std::ifstream hf(path/"world-head-combat.txt");std::string hm;hf>>hm>>std::ws;
                if(!hf||!hf.eof()||hm!="EVR_HEAD_COMBAT_1"||!controllerUiFixture::motionAttackRequested||controllerUiFixture::gestureRequested||!world)
                    throw std::runtime_error("head combat policy requires explicit bounded motion diagnostic");
                controllerUiFixture::combatGestureRequested=true;controllerUiFixture::swing.requireHeadGesture();
                const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
                nativeGestureHaptic=layer?reinterpret_cast<diagnostic::NativeGestureHaptic>(GetProcAddress(layer,"evrNativeGestureHaptic")):nullptr;
                if(!nativeGestureHaptic)throw std::runtime_error("combined gesture haptic bridge missing");
                logFile->event("head_combat_fixture_enabled","\"same_frame_policy\":true,\"menu_commands\":false,\"native_action_unchanged\":true");
            }
            if(controllerUiFixture::motionAttackRequested){
                if(!world||!motionHandVisual::requested||!worldEndurance)throw std::runtime_error("motion attack requires explicit tracked hands and world endurance");
                if(std::filesystem::exists(path/"world-two-hand-attack.txt")){
                    std::ifstream tf(path/"world-two-hand-attack.txt");std::string tm;float tolerance=0;tf>>tm>>tolerance>>std::ws;
                    if(!tf||!tf.eof()||tm!="EVR_TWO_HAND_ATTACK_1"||!weaponGripPreview::requested)throw std::runtime_error("two-hand attack needs measured preview");
                    auto w=weaponGripPreview::weapon;w.twoHanded=true;w.offHandTolerance=tolerance;
                    controllerUiFixture::swing.setTwoHandWeapon(w,weaponGripPreview::item);controllerUiFixture::twoHandAttackRequested=true;
                    controllerUiFixture::twoHandSupportWeapon=w;
                    logFile->event("two_hand_attack_enabled","\"item\":"+std::to_string(weaponGripPreview::item)+",\"type\":7,\"tolerance\":"+std::to_string(tolerance)+",\"tracked_hit_geometry_enabled\":"+(std::filesystem::exists(path/"world-motion-collider-apply.txt")?"true":"false"));
                }
                if(std::filesystem::exists(path/"world-melee-session.txt")){
                    std::ifstream file(path/"world-melee-session.txt");std::string magic;file>>magic>>std::ws;
                    if(!file||!file.eof()||(magic!="EVR_MELEE_SESSION_1"&&magic!="EVR_MELEE_SESSION_REPEAT_2")||!controllerUiFixture::twoHandAttackRequested||controllerUiFixture::motionAttackLimit!=(magic=="EVR_MELEE_SESSION_REPEAT_2"?2u:1u))
                        throw std::runtime_error("session adapter needs measured finite attack test");
                    const bool contact=std::filesystem::exists(path/"world-melee-session-contact.txt");
                    if(contact){
                        std::ifstream cp(path/"world-melee-session-contact.txt");std::string cm;cp>>cm>>std::ws;
                        if(!cp||!cp.eof()||cm!="EVR_MELEE_SESSION_CONTACT_2"||magic!="EVR_MELEE_SESSION_REPEAT_2"||!std::filesystem::exists(path/"world-motion-collider-apply.txt")||!std::filesystem::exists(path/"world-motion-event-retime.txt")||!std::filesystem::exists(path/"world-motion-collider-shadow.txt"))throw std::runtime_error("measured session contact policy incomplete");
                        logFile->event("melee_session_contact_enabled","\"expected_attacks\":2,\"measured_geometry\":true,\"native_costs_recovery_unchanged\":true");
                    }else if(std::filesystem::exists(path/"world-motion-collider-shadow.txt")||std::filesystem::exists(path/"world-motion-collider-apply.txt")||std::filesystem::exists(path/"world-motion-event-retime.txt"))throw std::runtime_error("session contact requires explicit qualified mode");
                    controllerUiFixture::meleeSession=std::make_unique<game::MeleeSessionFixture>(controllerUiFixture::twoHandSupportWeapon,controllerUiFixture::motionAttackLimit,controllerUiFixture::combatGestureRequested);
                    logFile->event("melee_session_enabled","\"coordinator_unlimited\":true,\"fixture_attack_limit\":"+std::to_string(controllerUiFixture::motionAttackLimit)+",\"direct_damage_writes\":false");
                }
                actionSequenceObserver::requested=true;actionSequenceObserver::recording=false;
                logFile->event("action_sequence_enabled","\"bytes\":3104,\"max_samples\":4096,\"read_only\":true,\"time_unit_known\":false");
                const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
                nativeMotionHaptic=layer?reinterpret_cast<diagnostic::NativeMotionHaptic>(GetProcAddress(layer,"evrNativeMotionHaptic")):nullptr;
                if(!nativeMotionHaptic)throw std::runtime_error("motion feedback render-thread export absent");
                inputHeadIdle::meleeEvidence=[](uint32_t entity,uint64_t now){
                    const auto m=attachmentObserver::meleeAppearance(entity,now);const auto root=rootCamera::snapshot();
                    return m.valid&&root.entity==entity&&rootCamera::firstPerson(root)&&now>=root.milliseconds&&now-root.milliseconds<=200;
                };
                logFile->event("melee_head_policy_enabled","\"camera_only\":true,\"native_digital_mask\":15,\"extra_actor_state_bit\":5,\"fresh_melee_appearance_required\":true");
                controllerUiFixture::observeCombat=[](){
                    actionSequenceObserver::Status a;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);a=actionSequenceObserver::latest;}
                    const auto now=GetTickCount64();const auto m=attachmentObserver::meleeAppearance(a.entity,now);const auto root=rootCamera::snapshot();
                    game::MotionCombatObservation out;out.entity=a.entity;out.item=m.item;out.binding=(uint64_t(m.child)<<32)|m.item;
                    out.usedItem=a.usedItem;out.sequence=a.sequence;out.triggerTime=a.triggerTime;out.playedTime=a.playedTime;out.currentTime=a.currentTime;out.length=a.length;
                    out.active=a.active;out.alive=a.alive;out.ready=a.ready;out.world=m.world;out.nativeMs=a.milliseconds;out.trigger=a.trigger;out.canPrimary=a.canPrimary;
                    out.valid=m.valid&&a.entity==root.entity&&rootCamera::firstPerson(root)&&now>=a.milliseconds&&now-a.milliseconds<=200&&now>=root.milliseconds&&now-root.milliseconds<=200;
                    if(controllerUiFixture::twoHandAttackRequested)out.valid=out.valid&&m.weaponCategoryKnown&&m.weaponType==weaponGripPreview::type&&m.item==weaponGripPreview::item&&m.modelGuid==weaponGripPreview::weaponGuid;
                    if(combatEpochGate::requested){out.worlds=combatEpochGate::capture(combatEpochGate::Stage::Input,m.world,a.world);out.valid=out.valid&&out.worlds.valid();}
                    if(out.valid)combatWorldBridge::observe(combatWorldBridge::Stage::Input,m.world,a.world);
                    return out;};
            }
            controllerUiFixture::attackButtons=controllerUiFixture::motionAttackRequested?XINPUT_GAMEPAD_X:value=="EVR_CONTROLLER_ATTACK_RB_1"?XINPUT_GAMEPAD_RIGHT_SHOULDER:value=="EVR_CONTROLLER_ATTACK_X_1"?XINPUT_GAMEPAD_X:0;
            controllerUiFixture::attackRequested=!controllerUiFixture::motionAttackRequested&&(value=="EVR_CONTROLLER_ATTACK_1"||controllerUiFixture::attackButtons!=0);
            if(controllerUiFixture::attackRequested){
                if(world||!actionSequenceObserver::requested||!attachmentObserver::requested||!attachmentObserver::allowCandidateScene)throw std::runtime_error("attack fixture requires standalone action and melee appearance evidence");
                controllerUiFixture::observeAttack=[](){
                    actionSequenceObserver::Status a;
                    {std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);a=actionSequenceObserver::latest;}
                    const auto now=GetTickCount64();const auto m=attachmentObserver::meleeAppearance(a.entity,now);
                    const bool valid=m.valid&&a.entity&&now>=a.milliseconds&&now-a.milliseconds<=200;
                    return game::AttackObservation{a.entity,m.item,valid,a.active,a.activeSamples,a.triggerTime};};
            }
            controllerUiFixture::glideRequested=value=="EVR_CONTROLLER_GLIDE_1";
            controllerUiFixture::doubleJumpRequested=value=="EVR_CONTROLLER_DOUBLE_JUMP_1";
            controllerUiFixture::motionRequested=value=="EVR_CONTROLLER_JUMP_1"||controllerUiFixture::doubleJumpRequested||controllerUiFixture::glideRequested;
            controllerUiFixture::motion=game::ControllerMotionFixture(controllerUiFixture::doubleJumpRequested,controllerUiFixture::glideRequested);
            controllerUiFixture::observeMotion=[](){
                auto r=rootCamera::snapshot();controllerUiFixture::MotionActor a;
                {std::lock_guard<std::mutex> lock(controllerUiFixture::motionActorMutex);a=controllerUiFixture::motionActor;}
                const auto now=GetTickCount64();const bool valid=a.entity==r.entity&&a.milliseconds&&now>=a.milliseconds&&now-a.milliseconds<=200&&now>=r.milliseconds&&now-r.milliseconds<=200;
                return game::MotionObservation{r.entity,r.cameraId,double(r.stable.position.x)*0x1p-32,double(r.stable.position.y)*0x1p-32,double(r.stable.position.z)*0x1p-32,valid,a.state,r.number,now>=r.milliseconds?now-r.milliseconds:UINT64_MAX,now>=a.milliseconds?now-a.milliseconds:UINT64_MAX};};
            controllerUiFixture::tabsRequested=value=="EVR_CONTROLLER_TABS_1";
            controllerUiFixture::altarRequested=value=="EVR_CONTROLLER_ALTAR_1";
            const bool tourRequested=value=="EVR_CONTROLLER_TOUR_1"; // 2026-10-06: every menu tab, VR UI check
            controllerUiFixture::menuRequested=value=="EVR_CONTROLLER_MENU_1"||controllerUiFixture::tabsRequested||controllerUiFixture::altarRequested||tourRequested;
            controllerUiFixture::menu=game::ControllerMenuFixture(controllerUiFixture::tabsRequested,controllerUiFixture::altarRequested,tourRequested);
            // Live play (2026-10-19 01:04, real device): a RenderDoc capture held the frame ~1 s, the root sample went stale, the
            // throw here failed the whole controller and the player could not move for the rest of the session. Live input
            // takes an unknown camera (-1, as with no observer) for that tick; the fixtures keep the strict check.
            controllerUiFixture::observeCamera=[]()->int {
                if(!controllerUiFixture::liveRequested)return int(rootCamera::snapshot().cameraId);
                rootCamera::Sample s;try{if(rootCamera::trySnapshot(s))return int(s.cameraId);}catch(...){}return -1;};
            if(!flag||!flag.eof()||(value!="EVR_CONTROLLER_NEUTRAL_1"&&!controllerUiFixture::menuRequested&&!controllerUiFixture::motionRequested&&!controllerUiFixture::attackRequested&&!controllerUiFixture::motionAttackRequested&&!controllerUiFixture::gestureRequested&&!controllerUiFixture::equipmentRequested&&!controllerUiFixture::guardRequested&&!controllerUiFixture::liveRequested)||duration<((controllerUiFixture::motionRequested||controllerUiFixture::attackRequested)?20u:controllerUiFixture::tabsRequested?30u:controllerUiFixture::menuRequested?15u:10u))throw std::runtime_error("invalid controller fixture");
            for(const auto& check:std::array<std::pair<uintptr_t,const char*>,3>{{
                // 1.0: the rescan's prologue was compiled differently; the store of the rescan flag ([[rcx+48]+28] = 1) is the same.
                {0xf9f7e0,"40 55 56 57 48 8D 6C 24 B9 48 81 EC 00 01 00 00"},
                {0xfa00e5,"48 8B 41 48 C6 40 28 01 B0 01"},
                {0xf97acb,"80 79 28 00 48 8B E9"}}}){
                const auto bytes=parsePattern(check.second);
                if(scan(reinterpret_cast<const uint8_t*>(baseAddress+check.first),bytes.size(),bytes)!=std::vector<size_t>{0})
                    throw std::runtime_error("native hotplug/rescan signature differs");
            }
            /* 1.0: the table is at 0xfa0108 (the matcher took 0xfa00f9) */ if(read<uint32_t>(baseAddress+0xfa0108+15*4)!=0xfa00e5)throw std::runtime_error("native hotplug event20 dispatch changed");
            controllerUiFixture::nativeDeviceEvent=reinterpret_cast<controllerUiFixture::RescanFn>(baseAddress+0xf9f7e0);
        }

    controllerUiFixture::armed=!world;
    if(world&&controllerUiFixture::requested){
        if((!controllerUiFixture::menuRequested&&!controllerUiFixture::motionRequested&&!controllerUiFixture::motionAttackRequested&&!controllerUiFixture::gestureRequested&&!controllerUiFixture::equipmentRequested&&!controllerUiFixture::guardRequested&&!controllerUiFixture::liveRequested)||!inputDirection::requested||rootCamera::mode!=2||rootCamera::modeFixture||inputHeadIdle::fixtureRequested)
            throw std::runtime_error("world menu fixture needs native first-person/input and excludes other fixtures");
        const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
        controllerUiFixture::requestXrCapture=layer?reinterpret_cast<controllerUiFixture::CaptureFn>(GetProcAddress(layer,"evrRequestNativeXrCapture")):nullptr;
        if(!controllerUiFixture::requestXrCapture)throw std::runtime_error("world menu capture export absent");
        if(std::filesystem::exists(path/"controller-motion-burst.txt")){
            std::ifstream input(path/"controller-motion-burst.txt");std::string magic;input>>magic>>controllerUiFixture::motionBurstFrames>>std::ws;
            if(!input||!input.eof()||magic!="EVR_CONTROLLER_MOTION_BURST_1"||!controllerUiFixture::motionRequested||captureGlide||captureLiveFinal||captureInitialFogSdf||
               !controllerUiFixture::motionBurstFrames||controllerUiFixture::motionBurstFrames>32)throw std::runtime_error("invalid bounded motion burst request");
            controllerUiFixture::requestXrBurst=reinterpret_cast<controllerUiFixture::CaptureFn>(GetProcAddress(layer,"evrRequestNativeXrBurst"));
            if(!controllerUiFixture::requestXrBurst)throw std::runtime_error("motion burst export absent");
        }
    }
}
// Read-only comparison of native exposure state. View+d0 is a texture handle,
// not a light-factor vector; the post-processing object is View+ba0.
// One exposure for both eyes (2026-10-14, user: "the brightness differs between the eyes - that is what feels wrong";
// Mock self-test game-20261014-210046 in the user's room: the right eye's whole picture 23 % brighter, its adapted
// luminance 5.72 against the left eye's 6.50). Each View adapts its own exposure to what it sees - its post-processing
// object (View+ba0): +40 the adapted luminance, +44 its reciprocal, +50 the one before .. - moved by 962760 and read by
// 962590 into the frame's constants (f7c50 fa1ef / fa1fb), so an eye that sees a bright doorway the other one does not
// drew the whole picture darker or brighter. Copying the left eye's state before the right eye drew was not enough
// (game-20261014-211053: the eye's own update moved it 2 % in one step). So the right eye's update itself is followed
// by the left eye's state of this frame: both eyes draw with one exposure. 962760 is called at fa1ef only, a large
// function (its callers load their registers again). EVR_SHARE_EXPOSURE=0 leaves each eye its own.
namespace eyeExposure {
// 1.0: the update takes a fifth (stack) argument (the call at 0x12c82a); a four-argument detour crashed the game inside
// it (Mock game-20261016-043950, access violation at 0xa4c5cc).
using Update=uint64_t(__fastcall*)(void*,void*,void*,void*,void*);
inline Update original=nullptr;inline bool enabled=false;
inline std::atomic<uintptr_t> source{0},target{0};inline std::atomic<uint64_t> shared{0};inline std::atomic<bool> failed{false};
constexpr uintptr_t rva=0xa4e550,caller=0x12c9cf;
constexpr const char* pattern="48 8B C4 57 48 81 EC D0 00 00 00 80 B9 60 04 00 00 00";
// 1.0 (Hotfix 1.0.0.1, 0xa4c450 against June's 0x962760): the result is eight values from +0x3c - the exposure (+0x3c,
// and +0x40, the same value when the game adapts), its inverse +0x44, +0x48..+0x50 (written before), and the previous
// frame's +0x3c / +0x40 at +0x54 / +0x58 (June: six from +0x40 - exposure, inverse, two, the previous exposure +0x50).
constexpr uintptr_t stateAt=0x3c;constexpr size_t stateFloats=8;
uint64_t __fastcall update(void* post,void* b,void* c,void* d,void* e){
    const auto result=original(post,b,c,d,e);
    const auto to=target.load(),from=source.load();
    if(to&&from&&reinterpret_cast<uintptr_t>(post)==to&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+caller){
        std::array<float,stateFloats> left{},right{};std::memcpy(left.data(),reinterpret_cast<void*>(from+stateAt),sizeof left);std::memcpy(right.data(),reinterpret_cast<void*>(to+stateAt),sizeof right);
        bool sane=std::isfinite(left[0])&&left[0]>0;for(const float v:left)sane=sane&&std::isfinite(v);
        if(sane){std::memcpy(reinterpret_cast<void*>(to+stateAt),left.data(),sizeof left);++shared;}
        const auto n=shared.load();
        if(n<=4||n%600==0||!sane)try{std::ostringstream f;f<<std::setprecision(6)<<"\"shared\":"<<(sane?"true":"false")<<",\"frames\":"<<n
            <<",\"left\":["<<left[0]<<','<<left[2]<<','<<left[6]<<"],\"right_own\":["<<right[0]<<','<<right[2]<<','<<right[6]<<']';
            logFile->event("eye_exposure_shared",f.str());}catch(...){}
    }
    return result;
}
inline void install(){
    char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SHARE_EXPOSURE",v,sizeof v);
    if(n==1&&v[0]=='0'){logFile->event("eye_exposure_share_off","\"reason\":\"EVR_SHARE_EXPOSURE=0\"");return;}
    const auto bytes=parsePattern(pattern);const auto at=baseAddress+rva,site=baseAddress+caller-5;
    if(scan(reinterpret_cast<const uint8_t*>(at),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(site)!=0xe8||uintptr_t(int64_t(site+5)+read<int32_t>(site+1))!=at)
        throw std::runtime_error("exposure update signature/caller mismatch");
    if(MH_CreateHook(reinterpret_cast<void*>(at),reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(at))!=MH_OK)throw std::runtime_error("exposure update hook failed");
    enabled=true;logFile->event("eye_exposure_share_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(caller));
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("eye_exposure_share_removed","\"status\":"+std::to_string(status)+",\"frames\":"+std::to_string(shared.load()));}catch(...){}
}
}
void logWorldColorState(uintptr_t view,int eye,uint64_t serial,void* parameters,void* result) noexcept {
    if(!worldNativeDiagnostic)return;
    try {
        const auto post=read<uintptr_t>(view+layout::viewPost);
        std::ostringstream fields;
        fields<<"\"eye\":"<<eye<<",\"serial\":"<<serial<<",\"view\":"<<hex(view)
            <<",\"post\":"<<hex(post)<<",\"post_40_54\":[";
        for(unsigned n=0;n<6;n++){if(n)fields<<',';if(post){const auto v=read<float>(post+0x40+n*4);if(std::isfinite(v))fields<<v;else fields<<"null";}else fields<<"null";}
        const auto input=reinterpret_cast<uintptr_t>(parameters);
        fields<<"],\"render_mode_120\":"<<read<uint32_t>(input+0x120)
            <<",\"parameter_300\":"<<read<float>(input+0x300)
            <<",\"view_flags\":"<<read<uint32_t>(view+0x10);
        logFile->event("world_color_state",fields.str());
        // f7c50:fad2c clears this native per-view light array count; the
        // fad60..fade3 loop filters Scene lights and 10aa80 appends survivors.
        // Compare CPU selection with captured GPU light data without copying
        // another view's list, changing culling, or writing light parameters.
        const auto lightCount=read<uint64_t>(view+layout::viewLights+8);
        const auto lightArray=read<uintptr_t>(view+layout::viewLights);
        std::ostringstream lights;
        lights<<"\"eye\":"<<eye<<",\"serial\":"<<serial<<",\"view\":"<<hex(view)
            <<",\"cpu_light_count\":"<<lightCount<<",\"cpu_light_array\":"<<hex(lightArray)
            <<",\"cpu_light_capacity\":"<<read<uint64_t>(view+layout::viewLights+0x10)
            <<",\"cpu_light_grow\":"<<hex(read<uintptr_t>(view+layout::viewLights+0x18))
            <<",\"light_mask\":"<<read<uint32_t>(input+0x8c)
            <<",\"gpu_light_upload\":"<<read<uint32_t>(view+0xb68)
            <<",\"array_valid\":"<<((!lightCount||lightArray)?"true":"false");
        logFile->event("world_light_state",lights.str());
        // f7c50:107205 compares native result bits 7,8,9 before setting View+718.
        // Observe why a private View remains unready; never copy or force flags.
        std::ostringstream pipeline;
        const auto resultFlags=read<uint32_t>(reinterpret_cast<uintptr_t>(result));
        const auto streaming=read<uintptr_t>(view+layout::viewStreaming);
        pipeline<<"\"eye\":"<<eye<<",\"serial\":"<<serial<<",\"view\":"<<hex(view)
            <<",\"result_flags\":"<<resultFlags<<",\"missing_ready_bits\":"<<((~resultFlags)&0x380u)
            <<",\"ready\":"<<unsigned(read<uint8_t>(view+layout::viewReady))
            <<",\"last_position_valid\":"<<unsigned(read<uint8_t>(view+layout::viewLastPositionValid))
            <<",\"invalidate_ready_input\":"<<unsigned(read<uint8_t>(input))
            <<",\"view_streaming\":"<<hex(streaming)
            <<",\"terrain_ready\":"<<(streaming?unsigned(read<uint8_t>(streaming+0xc1bb)):0)
            <<",\"terrain_requests_pending\":"<<(streaming?read<uint32_t>(streaming+0xc1cc):0)
            <<",\"gi_context\":"<<hex(read<uintptr_t>(view+layout::viewGi))
            <<",\"light_cache\":"<<hex(read<uintptr_t>(view+layout::viewLightCache));
        logFile->event("world_pipeline_state",pipeline.str());
        const auto fog=read<uintptr_t>(view+layout::viewFog);
        if(fog){
            std::ostringstream state;state<<"\"eye\":"<<eye<<",\"serial\":"<<serial<<",\"view\":"<<hex(view)<<",\"fog\":"<<hex(fog)
                <<",\"frame\":"<<read<uint32_t>(fog+0x18)<<",\"sdf_active\":"<<unsigned(read<uint8_t>(fog+0x83a0))
                <<",\"cpu_queued\":"<<unsigned(read<uint8_t>(fog+0xb90))<<",\"cpu_completed\":"<<unsigned(read<uint8_t>(fog+0xb91))
                <<",\"pending_regions\":"<<read<uint16_t>(fog+0x8398)<<",\"staging_valid\":"<<unsigned(read<uint8_t>(fog+0xb88))
                <<",\"sdf_graph\":"<<hex(read<uintptr_t>(fog+0x990))<<",\"sdf_handle\":"<<read<uint64_t>(fog+0x998)
                <<",\"origin\":["<<read<int32_t>(fog+0x944)<<','<<read<int32_t>(fog+0x948)<<','<<read<int32_t>(fog+0x94c)<<']';
            logFile->event("fog_sdf_state",state.str());
        }
    }catch(...){} // Diagnostic observation must not change native behavior.
}
// The only explicit data write is the scene's renderer back-pointer. The game
// factory and destructor write this same field. Restore it on every C++ path.
void selectView(uintptr_t scene,uintptr_t view) { *reinterpret_cast<uintptr_t *>(scene+layout::sceneView)=view; }
bool finishEyeCpuTasks(uintptr_t scene,unsigned milliseconds){
    // d5870 includes 136550, which discards finished CPU SDF output and marks
    // it for retry. During world rendering, leave that queue to ca2f0/136a90:
    // the native frame stage consumes its results and records the GPU bake.
    // Preserve the other five native collectors and their busy status contract.
    using Collect=void*(__fastcall*)(void*,void*,int64_t);
    struct Collector {uintptr_t rva;size_t offset;bool perView;};
    // 1.0: the wait (0x1091c0) has four collectors and no SDF one: scene+0xa80 (0x53280; June scene+0x9c8, 0x2afb0),
    // scene+0xa88 (0x1fb370; +0x9d8, 0x1ce370), View+0x978 (0x1ddf60; +0x710, 0x1b26d0), View+0xb40 (0x1b8bc0; the fog,
    // +0x8c0, 0x1912a0); June's View+0x740 (0x1275e0) is gone.
    // 1.0 (2026-10-16, both real-device starts of package-20261016-124304 and the Mock auto start crashed on entering the
    // world): the two scene collectors are not called here any more. 0x53280 (scene+0xa80, the terrain bricks) is not a
    // status query but the brick system's step: a call takes the finished batch and starts the next one (the jobs
    // 0x58b00 -> 0x58540). The game steps it once a frame after its own preparation (0x5ac83) and runs it to the end only
    // when a View goes (0xfd1b0 / 0x106c4f, infinite time, after the cancel 0x4f000). Called every millisecond after each
    // eye while the world was still streaming in (the first frames after loading), it was stepped 62 times in 0.5 s and a
    // job it had started wrote through a null brick buffer (enshrouded.exe+0x13aa839 <- 0x10952f5 <- 0x5872b <- 0x58bb0 on a
    // worker thread; dumps enshrouded_20261016_034944 / _035041 / _041052). Mock runs reached the eyes only after the world
    // had settled, where the step had nothing to do (status 0). 0x1fb370 (scene+0xa88, the water bricks) is stepped by the
    // game the same way (0x1faa40, 0x1fdcb1) and is left to it too. The two per-View collectors stay (the selected View's
    // own streaming and fog), with the status read where the game reads it: the byte the returned pointer points at.
    constexpr std::array<Collector,2> collectors{{
        {0x1de3b0,layout::viewStreaming,true},{0x1b9250,layout::viewFog,true}}};
    const auto start=std::chrono::steady_clock::now(),deadline=start+std::chrono::milliseconds(milliseconds);
    const auto selectedView=read<uintptr_t>(scene+layout::sceneView);
    std::array<unsigned,2> statuses{};
    unsigned polls=0;bool complete=false;
    do {++polls;
        if(!worldNativeDiagnostic)complete=reinterpret_cast<Wait>(baseAddress+0x1092e0)(reinterpret_cast<void*>(scene),0);
        else {
            if(read<uintptr_t>(scene+layout::sceneView)!=selectedView)throw std::runtime_error("selected View changed during CPU completion");
            complete=true;
            for(size_t i=0;i<collectors.size();++i){const auto& c=collectors[i];
                const auto object=read<uintptr_t>((c.perView?selectedView:scene)+c.offset);
                alignas(16) std::array<uint8_t,64> result{};
                const uint8_t* status=result.data();
                if(object)if(const auto r=reinterpret_cast<Collect>(baseAddress+c.rva)(result.data(),reinterpret_cast<void*>(object),0))status=static_cast<const uint8_t*>(r);
                statuses[i]=*status;
                if(statuses[i]>=1&&statuses[i]<=3)complete=false;
            }
        }
        if(complete)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }while(std::chrono::steady_clock::now()<deadline);
    static std::atomic<uint64_t> calls{0},incomplete{0};
    const auto number=++calls;if(!complete)++incomplete;
    // every call until 300, then the first of each 600 and every incomplete one up to 2000 of them
    if(number<=300||number%600==0||(!complete&&incomplete<=2000))
        logFile->event("eye_cpu_tasks_completion","\"complete\":"+std::string(complete?"true":"false")+",\"call\":"+std::to_string(number)+
            ",\"incomplete_calls\":"+std::to_string(incomplete.load())+",\"deadline_ms\":"+std::to_string(milliseconds)+
            ",\"polls\":"+std::to_string(polls)+",\"elapsed_us\":"+std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count())+
            ",\"sdf_results_preserved\":"+(worldNativeDiagnostic?"true":"false")+",\"scene_collectors_stepped\":false"+
            ",\"collector_statuses\":["+std::to_string(statuses[0])+','+std::to_string(statuses[1])+']');
    return complete;
}
void dispose(uintptr_t scene,uintptr_t &view,std::array<PrivateSsr,2>& ssr) {
    if(!view)return;
    SharedResolutionScope privateScope(scene,&ssr);
    selectView(scene,view);
    // A feature-only or failed feature-commit path never ran 152f00. Detach
    // those private objects before d4170, which otherwise calls 153fe0 on
    // uninitialized resolution handles. Their creation resources remain owned.
    for(size_t i=0;i<ssr.size();++i)if(ssr[i].object){
        const auto slot=view+layout::viewSsr+i*8;
        if(read<uintptr_t>(slot)!=ssr[i].object)throw std::runtime_error("private SSR slot ownership changed during disposal");
        if(!ssr[i].resolutionInitialized)*reinterpret_cast<uintptr_t*>(slot)=0;
    }
    reinterpret_cast<Destroy>(baseAddress+0x107ca0)(reinterpret_cast<void *>(view));
    for(const auto& ssrObject:ssr)if(ssrObject.resolutionInitialized)
        throw std::runtime_error("private SSR resolution cleanup did not complete");
    reinterpret_cast<Destroy>(baseAddress+0x1079b0)(reinterpret_cast<void *>(view));
    for(auto& ssrObject:ssr)if(ssrObject.object){
        reinterpret_cast<Destroy>(baseAddress+0x17b340)(reinterpret_cast<void*>(ssrObject.object));ssrObject={};
    }
    reinterpret_cast<Destroy>(baseAddress+0x1076f0)(reinterpret_cast<void *>(view));
    view=0;
}
// The eyes' render features: the original View's (keen::RenderOldFeatureFlag bits - with the user's settings Dlss 19 is on and
// Xess 20, Ssao 9 and the contact shadows 12 are off) without the singleton VFX (bit 2). 2026-10-22: EVR_EYE_SSR=0 also leaves
// out StochasticScreenSpaceReflections (bit 17) - a test of the water reflection that shows in the right eye only (the game's
// settings cannot turn the reflections off: their lowest step is Performance, still SSR). (Bit 16 is the BrickRenderer: left
// out, the terrain was not drawn in the eyes - Mock game-20261022-041547.)
inline uint32_t eyeFeatures(uint32_t original){
    static const bool ssrOff=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_SSR",v,sizeof v);return n==1&&v[0]=='0';}();
    return (original&~4u)&~(ssrOff?(1u<<17):0u);
}
void setupFeatures(uintptr_t scene,uintptr_t wrapper,uintptr_t view,uint32_t features,uint32_t pool,uint32_t &group,
                   std::array<PrivateSsr,2>& ssr,const std::array<std::array<PrivateSsr,2>,2>& allSsr) {
    using BuilderInit=void *(__fastcall *)(void *,void *,uint8_t);
    using PoolInit=void *(__fastcall *)(void *,void *,uint32_t,uintptr_t,uint8_t);
    // 1.0: the setup takes the configure settings as well (its WorldSdf part reads settings+0x58).
    using Setup=void (__fastcall *)(void *,const uint32_t *,const void *,const void *);
    using Commit=void *(__fastcall *)(void *,void *,uint32_t *,void *);
    const auto renderer=read<uintptr_t>(scene+layout::sceneRenderer);
    struct Scratch { alignas(16) std::array<uint8_t,0x400> bytes{}; std::array<uint8_t,32> guard; } builder; // 1.0 uses up to +0x178 (+0x150 before), the game gives it 0x210
    builder.guard.fill(0xa5);
    alignas(16) std::array<uint8_t,32> allocator{};
    bool initialized=false;
    try {
        reinterpret_cast<BuilderInit>(baseAddress+0x11ccaf0)(builder.bytes.data(),reinterpret_cast<void *>(read<uintptr_t>(renderer+layout::rendererBuilder)),read<uint8_t>(wrapper+8));
        initialized=true;
        if(!pool)throw std::runtime_error("private feature memory pool missing");
        reinterpret_cast<PoolInit>(baseAddress+0x11cd5f0)(allocator.data(),builder.bytes.data(),pool,0,0);
        std::array<void *,2> context{allocator.data(),nullptr};
        reinterpret_cast<Setup>(baseAddress+0x103ba0)(reinterpret_cast<void *>(view),&features,reinterpret_cast<const void*>(wrapper+layout::ownerSettings),context.data());
        // d43e0 borrows these two SSR objects from the global Renderer. Replace
        // them only in our new View, using the original factory and this group's
        // allocator. Per-eye resolution/history then has a private owner.
        for(size_t i=0;i<2;++i){
            const auto slot=view+layout::viewSsr+i*8;
            const auto borrowed=read<uintptr_t>(slot);
            if(!borrowed)continue;
            if(borrowed!=read<uintptr_t>(renderer+layout::rendererSsr+i*8))throw std::runtime_error("unknown SSR ownership");
            using Factory=void* (__fastcall*)(void*,const void*);
            const std::array<uintptr_t,5> settings{read<uintptr_t>(renderer+layout::rendererGraphics),read<uintptr_t>(renderer+layout::rendererBuilder),
                read<uintptr_t>(renderer+layout::rendererSsrSetting2),read<uintptr_t>(renderer+layout::rendererSsrSetting3),reinterpret_cast<uintptr_t>(allocator.data())};
            const auto candidate=reinterpret_cast<uintptr_t>(reinterpret_cast<Factory>(baseAddress+0x17a3c0)(
                reinterpret_cast<void*>(read<uintptr_t>(renderer)),settings.data()));
            // Do not publish an unvalidated pointer into our destruction list.
            // An alias rejection must never free another eye or a borrowed SSR.
            if(!candidate||candidate==read<uintptr_t>(renderer+layout::rendererSsr)||candidate==read<uintptr_t>(renderer+layout::rendererSsr+8))
                throw std::runtime_error("SSR private factory alias/failure");
            for(const auto& eyeObjects:allSsr)for(const auto& other:eyeObjects)if(candidate==other.object)
                throw std::runtime_error("SSR private factory aliases another owned object");
            ssr[i].object=candidate;
            *reinterpret_cast<uintptr_t*>(slot)=candidate;
            logFile->event("private_ssr_created","\"view\":"+hex(view)+",\"slot\":"+std::to_string(i)+
                ",\"object\":"+hex(candidate)+",\"original\":"+hex(borrowed));
        }
        std::array<uint8_t,32> status{}; status[0]=0xff;
        reinterpret_cast<Commit>(baseAddress+0x11d4480)(builder.bytes.data(),status.data(),&group,reinterpret_cast<void *>(read<uintptr_t>(renderer+layout::rendererMemory)));
        if(status[0])throw std::runtime_error("native feature allocation error "+std::to_string(status[0]));
        if(!std::all_of(builder.guard.begin(),builder.guard.end(),[](uint8_t v){return v==0xa5;}) || read<uint32_t>(view+0x10)!=features)
            throw std::runtime_error("feature allocator guard/mask mismatch");
        reinterpret_cast<Destroy>(baseAddress+0x11cd8d0)(builder.bytes.data());initialized=false;
        logFile->event("native_view_features_initialized","\"view\":"+hex(view)+",\"features\":"+std::to_string(features)+",\"memory_group\":"+std::to_string(group));
    } catch(...) { if(initialized)reinterpret_cast<Destroy>(baseAddress+0x11cd8d0)(builder.bytes.data());throw; }
}
void releaseGroup(uintptr_t scene,uint32_t &group) {
    if(!group)return;
    using ReleaseGroup=void (__fastcall *)(void *,uint32_t *);
    const auto renderer=read<uintptr_t>(scene+layout::sceneRenderer);
    reinterpret_cast<ReleaseGroup>(baseAddress+0x11d3060)(reinterpret_cast<void *>(read<uintptr_t>(renderer+layout::rendererMemory)),&group);
}
template<class T,size_t N> void put(std::array<uint8_t,N> &bytes,size_t offset,T value) {
    if(offset+sizeof(T)>N)throw std::runtime_error("private owner field range");
    std::memcpy(bytes.data()+offset,&value,sizeof(T));
}
uintptr_t allocateFeatureMarker(uintptr_t scene) {
    const auto allocator=read<uintptr_t>(read<uintptr_t>(scene+layout::sceneRenderer));
    // 1.0: the allocator's tag goes by value (it went by pointer).
    using Allocate=void *(__fastcall *)(void *,uint64_t,uint64_t,uint8_t,uint8_t);
    auto fn=reinterpret_cast<Allocate>(read<uintptr_t>(read<uintptr_t>(allocator)+8));
    auto pointer=fn(reinterpret_cast<void *>(allocator),1,1,8,0);
    if(!pointer)throw std::runtime_error("private feature marker allocation failed");
    return reinterpret_cast<uintptr_t>(pointer);
}
void releasePrivateOwner(uintptr_t scene,std::array<uint8_t,layout::ownerBytes> &owner) {
    const auto address=reinterpret_cast<uintptr_t>(owner.data());
    const auto renderer=read<uintptr_t>(scene+layout::sceneRenderer);
    const auto resolution=read<uintptr_t>(address+layout::ownerResolution);
    if(resolution) {
        using FreeResolution=void (__fastcall *)(void *,void *);
        reinterpret_cast<FreeResolution>(baseAddress+0xf3970)(reinterpret_cast<void *>(resolution),reinterpret_cast<void *>(renderer));
        put<uintptr_t>(owner,layout::ownerResolution,0);
    }
    auto group=read<uint32_t>(address+layout::ownerResolutionGroup); releaseGroup(scene,group);put<uint32_t>(owner,layout::ownerResolutionGroup,group);
    const auto marker=read<uintptr_t>(address+layout::ownerMarker);
    if(marker) {
        const auto allocator=read<uintptr_t>(renderer);
        using Free=void (__fastcall *)(void *,void *,uint8_t);
        reinterpret_cast<Free>(read<uintptr_t>(read<uintptr_t>(allocator)+0x10))(reinterpret_cast<void *>(allocator),reinterpret_cast<void *>(marker),0);
        put<uintptr_t>(owner,layout::ownerMarker,0);
    }
}
void setupResolution(std::array<uint8_t,layout::ownerBytes> &owner,std::array<PrivateSsr,2>& ssr) {
    SharedResolutionScope privateScope(read<uintptr_t>(reinterpret_cast<uintptr_t>(owner.data())+layout::ownerScene),&ssr);
    // 1.0: the control goes by value (it went by pointer).
    using Configure=void (__fastcall *)(void *,const void *,uint32_t);
    const uint32_t control=0; // Force native resolution setup, without device-wide idle request.
    std::array<uint8_t,layout::ownerSettingsBytes> settings{};
    std::memcpy(settings.data(),owner.data()+layout::ownerSettings,settings.size());
    reinterpret_cast<Configure>(baseAddress+0xf1cb0)(owner.data(),settings.data(),control);
    const auto address=reinterpret_cast<uintptr_t>(owner.data());
    if(!read<uintptr_t>(address+layout::ownerResolution))throw std::runtime_error("native resolution setup did not commit");
    logFile->event("native_view_resolution_initialized","\"view\":"+hex(read<uintptr_t>(address+layout::ownerView))+
        ",\"width\":"+std::to_string(read<uint32_t>(address+layout::ownerWidth))+",\"height\":"+std::to_string(read<uint32_t>(address+layout::ownerHeight))+
        ",\"memory_group\":"+std::to_string(read<uint32_t>(address+layout::ownerResolutionGroup))+",\"render_scale\":"+std::to_string(read<float>(address+layout::ownerRenderScale))+",\"rendered\":false");
}
template<class Function> auto withNativeQueues(uintptr_t view,Function function){
    const auto leaseStart=std::chrono::steady_clock::now();
    const auto scene=read<uintptr_t>(view+8),renderer=read<uintptr_t>(scene+layout::sceneRenderer),graphics=read<uintptr_t>(renderer+layout::rendererGraphics);
    const auto backend=read<uintptr_t>(graphics+0x278),platform=read<uintptr_t>(backend+layout::backendContext+0x18);
    const auto instance=read<VkInstance>(backend+layout::backendInstance);
    const auto physical=read<VkPhysicalDevice>(backend+layout::backendPhysical);
    const auto device=read<VkDevice>(backend+layout::backendDevice);
    if(device!=read<VkDevice>(backend+layout::backendContext)||physical!=read<VkPhysicalDevice>(backend+layout::backendContext+8))throw std::runtime_error("native Vulkan binding copies differ");
    const auto count=read<uint64_t>(platform+layout::platformQueueCount),table=read<uintptr_t>(platform+layout::platformQueues);
    if(count<1||count>16||table%8)throw std::runtime_error("native queue table invalid");
    using TryLock=bool (__fastcall *)(void*);
    const auto tryLock=reinterpret_cast<TryLock>(baseAddress+0x8ae210);
    struct Lease{std::vector<volatile LONG*> held;
        void release(){for(auto i=held.rbegin();i!=held.rend();++i)InterlockedExchange(*i,0);held.clear();}
        ~Lease(){release();}} lease;
    lease.held.reserve(size_t(count));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
    for(;;){
        for(uint64_t i=0;i<count;i++){
            const auto entry=table+i*16;
            if(!read<VkQueue>(entry))throw std::runtime_error("null native queue");
            read<uint32_t>(entry+8);
            if(!tryLock(reinterpret_cast<void*>(entry+8)))break;
            lease.held.push_back(reinterpret_cast<volatile LONG*>(entry+8));
            if(read<uint32_t>(entry+8)!=0x80000000u)throw std::runtime_error("native queue lock state mismatch");
        }
        if(lease.held.size()==count)break;
        // Release only our own gates before retrying. Never wait while holding a
        // partial set, or reset a lock owned by another game/overlay worker.
        lease.release();
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("native queues busy after bounded retry; no XR call attempted");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // The game's own queue gates cover main/compute/transfer/overlay submissions.
    // Runtime calls do not reacquire these private gates; no new Vulkan-layer mutex.
    const auto leaseMicros=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-leaseStart).count();
    logFile->event("game_session_queue_lease","\"queues\":"+std::to_string(count)+",\"device\":"+hex(reinterpret_cast<uintptr_t>(device))+
        ",\"native_lock_value\":2147483648,\"acquire_cpu_us\":"+std::to_string(leaseMicros));
    return function(instance,physical,device);
}
uint32_t preparedQueueFamily(HMODULE module,VkInstance instance,VkDevice device){
    using GetFamily=int (*)(VkInstance,VkDevice,uint32_t*);
    auto get=module?reinterpret_cast<GetFamily>(GetProcAddress(module,"evrGetPreparedGameQueueFamily")):nullptr;
    uint32_t family=UINT32_MAX;
    if(!get||get(instance,device,&family)||family==UINT32_MAX)
        throw std::runtime_error("prepared game queue family unavailable");
    return family;
}
// Where the time of a frame goes on the render thread (user, 2026-10-14: "I want more FPS"; the GPU's own marks could
// not tell - lowering the eyes' internal size by 44 % changed nothing, runs game-20261014-080526 / -080951): the
// stretches of every eye pair are summed and go on record every 120 pairs. A handful of clock reads a frame.
//   begin   the runtime's frame wait and begin        eye0/eye1    the game's own render call for that eye
//   tasks   waiting for the eye's CPU tasks           record       all of recordPair (both eyes and the mod's work between)
//   submit  handing the pair to the runtime           hook         everything inside the render hook
//   period  from one pair to the next; period - hook is the game's own frame outside the hook
namespace cpuTiming {
enum Part:unsigned{Begin,Eye0,Eye1,Tasks0,Tasks1,Record,Submit,Hook,Count};
inline std::array<double,Count> sum{},worst{};inline uint64_t pairs=0;inline double lastPair=0,periodSum=0,periodWorst=0;inline unsigned periods=0;
inline double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// A part of the frame that took long (2026-10-23, the dashboard with SteamVR's motion smoothing: a frame every 3 s or so):
// which part, how long, which frame - 150 ms and longer, 300 at most, at once (the averages above come every 120 pairs).
inline std::atomic<unsigned> slowSpans{0};
inline void noteSlowSpan(Part part,double ms) noexcept;
struct Span{Part part;double start=now();explicit Span(Part p):part(p){}~Span(){const double d=now()-start;sum[part]+=d;worst[part]=std::max(worst[part],d);if(d>=150)noteSlowSpan(part,d);}Span(const Span&)=delete;Span& operator=(const Span&)=delete;};
inline void noteSlowSpan(Part part,double ms) noexcept {
    if(slowSpans.fetch_add(1)>=300)return;
    try{static const char* const names[Count]{"begin","eye0","eye1","tasks0","tasks1","record","submit","hook"};
        std::ostringstream o;o<<std::fixed<<std::setprecision(1)<<"\"part\":\""<<names[part]<<"\",\"ms\":"<<ms<<",\"frame\":"<<liveFrame.frame<<",\"focused\":"<<(liveFrame.hands.focused?"true":"false");
        logFile->event("cpu_slow_span",o.str());}catch(...){}
}
inline void pairDone() noexcept {
    const double t=now();if(lastPair>0){const double p=t-lastPair;periodSum+=p;periodWorst=std::max(periodWorst,p);++periods;}lastPair=t;
    if(++pairs%120)return;
    try{static const char* const names[Count]{"begin","eye0","eye1","tasks0","tasks1","record","submit","hook"};
        std::ostringstream o;o<<std::fixed<<std::setprecision(2)<<"\"pairs\":"<<pairs<<",\"frame\":"<<liveFrame.frame<<",\"period_ms\":"<<(periods?periodSum/periods:0)<<",\"period_worst_ms\":"<<periodWorst;
        for(unsigned n=0;n<Count;n++)o<<",\""<<names[n]<<"_ms\":"<<sum[n]/120<<",\""<<names[n]<<"_worst_ms\":"<<worst[n];
        logFile->event("cpu_frame_timing",o.str());}catch(...){}
    sum={};worst={};periodSum=periodWorst=0;periods=0;
}
}
void testGameSession(uintptr_t view){
    withNativeQueues(view,[&](VkInstance instance,VkPhysicalDevice physical,VkDevice device){
        auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        using Probe=int (*)(VkInstance,VkPhysicalDevice,VkDevice,uint32_t);
        auto run=module?reinterpret_cast<Probe>(GetProcAddress(module,"evrTestPreparedGameSession")):nullptr;
        if(!run)throw std::runtime_error("prepared game session entry unavailable");
        if(run(instance,physical,device,preparedQueueFamily(module,instance,device)))throw std::runtime_error("prepared game session failed");
        logFile->event("game_session_probe_complete","\"empty_frames\":3,\"game_eye_pixels_submitted\":false");
    });
}
// The eyes to ask the runtime frame for: one in turn after the warm-up when alternate-eye rendering is on.
uint32_t alternateEyesWanted(const PairResources& pair){
    if(!alternateEyes::requested||pair.pairedFrames<alternateEyes::warmupPairs)return 3;
    alternateEyes::parity^=1u;++alternateEyes::asked;return 1u<<alternateEyes::parity;
}
void alternateEyesLog(uint64_t serial){
    if(liveFrame.eyeMask!=3&&(++alternateEyes::single<=3||alternateEyes::single%1000==0))
        logFile->event("alternate_eye_frame","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(serial)+",\"eye_mask\":"+std::to_string(liveFrame.eyeMask)+
            ",\"single_eye_frames\":"+std::to_string(alternateEyes::single)+",\"asked\":"+std::to_string(alternateEyes::asked)+",\"stale_pose_turned\":"+std::to_string(alternateEyes::turned));
}
bool beginLiveFrame(uintptr_t view,uint32_t eyes=3){
    const cpuTiming::Span timed(cpuTiming::Begin);
    liveFrame.eyeMask=eyes;
    inputPose::channel.invalidate(); // before any blocking Runtime call
    bool ready=false;
    withNativeQueues(view,[&](VkInstance instance,VkPhysicalDevice physical,VkDevice device){
        auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        auto begin=module?reinterpret_cast<diagnostic::BeginNativeXr>(GetProcAddress(module,"evrBeginNativeXr")):nullptr;
        liveFrame.diagnosticFrames=worldSession?diagnostic::maximumNativeDiagnosticFrames:calibrationFrames;
        if(!begin)throw std::runtime_error("live native XR begin unavailable");
        const auto result=begin(instance,physical,device,preparedQueueFamily(module,instance,device),liveRenderScale,&liveFrame);
        if(result==diagnostic::nativeXrFrameDeferred){
            logFile->event("live_native_frame_deferred","\"frame\":"+std::to_string(liveFrame.frame)+",\"time\":"+std::to_string(liveFrame.time)+
                ",\"head_flags\":"+std::to_string(liveFrame.headFlags)+",\"input_pose_invalidated\":true");return;
        }
        if(result)throw std::runtime_error("live native XR begin failed");
        // The view reset (user, 2026-10-18 morning; VHVR player_ik.md 3.5): the headset's own (the runtime announces a new
        // LOCAL space: SteamVR's or Meta's system button held), from the time it gives on, or the pose of both hands in
        // front of the face. The origin is taken again as when the world began: the headset's place and heading now are
        // the character's eye and forward from then on, its height the standing height (the crouch's reference).
        const char* recenter=nullptr;
        {using LocalChange=int(*)(uint64_t*,int64_t*);const auto change=reinterpret_cast<LocalChange>(GetProcAddress(module,"evrNativeLocalSpaceChange"));
         uint64_t count=0;int64_t at=0;
         if(change&&change(&count,&at)){
             if(!liveOriginInitialized||count<liveLocalChangesSeen)liveLocalChangesSeen=count;
             else if(count!=liveLocalChangesSeen){liveLocalChangesSeen=count;liveRuntimeRecenterPending=true;liveRuntimeRecenterTime=at;
                 logFile->event("view_reset_runtime_announced","\"frame\":"+std::to_string(liveFrame.frame)+",\"time\":"+std::to_string(liveFrame.time)+",\"change_time\":"+std::to_string(at)+",\"changes\":"+std::to_string(count));}}
         if(liveRuntimeRecenterPending&&liveFrame.time>=liveRuntimeRecenterTime){liveRuntimeRecenterPending=false;recenter="runtime";}
         const auto asked=controllerUiFixture::recenterRequests.load();
         if(!liveOriginInitialized)liveRecenterRequestsSeen=asked;
         else if(asked!=liveRecenterRequestsSeen){liveRecenterRequestsSeen=asked;if(!recenter)recenter="pose";}}
        if(!liveOriginInitialized||liveOriginSession!=liveFrame.sessionGeneration||recenter){
            if(rootCamera::mode==2&&game::padStyle.load()){
                // A pad style: forward is taken from the head's right side, which stays level lying on the back as well (its
                // face looks up there); the tilt is turned on every frame below.
                const auto right=rotate(liveFrame.head.orientation,Vec3{1,0,0});
                const float yaw=right.x*right.x+right.z*right.z>1e-6f?std::atan2(-right.z,right.x):0.f;
                const auto q=euler(-yaw,0,0);liveTrackingBase={q,rotate(q,liveFrame.head.position)*-1};
                // The pad panel's view reset (LT, pad_settings.hpp): the tilt becomes the head's angle now, so where the face
                // looks - up and down as well - is ahead (the panel's slider shows it and keeps it in the settings).
                if(recenter&&game::padTiltFromHead.exchange(false)){const int tilt=game::padTiltOf(liveFrame.head.orientation);game::padTiltDegrees=tilt;
                    logFile->event("pad_tilt_from_head","\"frame\":"+std::to_string(liveFrame.frame)+",\"tilt\":"+std::to_string(tilt));}
            }else if(rootCamera::mode==2){TrackingOrigin origin;origin.recenter(liveFrame.head);liveTrackingBase=origin.trackingToAnchor;}
            else liveTrackingBase=inverse(liveFrame.head);
            liveTrackingToAnchor=liveTrackingBase;
            liveOriginInitialized=true;
            liveOriginSession=liveFrame.sessionGeneration;++liveOriginEpoch;
            const float standing=rootCamera::calibratedHeight>1.f&&rootCamera::calibratedHeight<2.5f?float(rootCamera::calibratedHeight):1.65f;
            controllerUiFixture::publishOrigin(liveFrame.head.position.y,standing,liveOriginEpoch);
            if(recenter)logFile->event("view_reset","\"reason\":\""+std::string(recenter)+"\",\"frame\":"+std::to_string(liveFrame.frame)+",\"epoch\":"+std::to_string(liveOriginEpoch)+
                ",\"head\":["+std::to_string(liveFrame.head.position.x)+","+std::to_string(liveFrame.head.position.y)+","+std::to_string(liveFrame.head.position.z)+"],\"standing_m\":"+std::to_string(standing));
        }
        for(size_t eye=0;eye<2;++eye)if(!liveFrame.width[eye]||!liveFrame.height[eye]||liveFrame.width[eye]>8192||liveFrame.height[eye]>8192)
            throw std::runtime_error("live native XR extent exceeds diagnostic budget");
        logFile->event("live_native_frame_begin","\"frame\":"+std::to_string(liveFrame.frame)+",\"time\":"+std::to_string(liveFrame.time));
        // Motion intents share the camera tracking-origin epoch so a recenter
        // cannot keep an old pending attack/feedback alive.
        liveFrame.hands.originGeneration=liveOriginEpoch;
        if(!handSmoothingRead){handSmoothingRead=true;char value[16]{};const auto n=GetEnvironmentVariableA("EVR_HAND_SMOOTHING",value,sizeof value);
            if(n&&n<sizeof value){char* end=nullptr;const float hz=std::strtof(value,&end);if(end!=value&&std::isfinite(hz)&&hz>=0&&hz<=60)handSmoothing.settings.cutoff=hz;}
            logFile->event("hand_smoothing","\"cutoff_hz\":"+std::to_string(handSmoothing.settings.cutoff)+",\"position_beta\":"+std::to_string(handSmoothing.settings.positionBeta)+
                ",\"orientation_beta\":"+std::to_string(handSmoothing.settings.orientationBeta)+",\"only_what_is_shown\":true");}
        if(handSmoothingRead&&!handHoldLogs&&handHoldWanted){char value[8]{};const auto n=GetEnvironmentVariableA("EVR_HAND_HOLD",value,sizeof value);
            if(n&&n<sizeof value&&value[0]=='0')handHoldWanted=false;
            ++handHoldLogs;logFile->event("hand_tracking_hold","\"enabled\":"+std::string(handHoldWanted?"true":"false")+",\"only_what_is_shown\":true");}
        shownHands=handSmoothing.update(handHoldWanted?handHold.update(liveFrame.hands):liveFrame.hands);
        // The hit stop (include/evr/hit_stop.hpp; asked by controller_ui_fixture.hpp when a stroke's hit is seen).
        {static uint32_t seenSerial=0;const auto serial=controllerUiFixture::hitStopSerial.load();
         if(serial!=seenSerial){seenSerial=serial;hitStop.stop(controllerUiFixture::hitStopHands.load(),controllerUiFixture::hitStopMs.load()/1000.);}
         // Never the frame's failure (2026-10-20 14:10: a refused vibration once ended VR): a fault shows the hands as they come.
         try{shownHands=hitStop.apply(shownHands,std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());}
         catch(const std::exception& e){hitStop.reset();static unsigned told=0;if(told++<5)logFile->event("hit_stop_failure","\"reason\":"+jsonString(e.what()));}}
        if(handHoldWanted)for(unsigned h=0;h<2;h++)if(handHold.state[h]!=handHoldSeen[h]){handHoldSeen[h]=handHold.state[h];
            static const char* const names[]{"none","tracked","estimated","held"};
            if(handHoldLogs++<400)logFile->event("hand_tracking_hold","\"frame\":"+std::to_string(liveFrame.frame)+",\"hand\":"+std::to_string(h)+",\"state\":\""+names[unsigned(handHold.state[h])]+
                "\",\"focused\":"+(liveFrame.hands.focused?"true":"false")+",\"grip_flags\":"+std::to_string(liveFrame.hands.hands[h].gripFlags)+",\"grip_active\":"+(liveFrame.hands.hands[h].gripActive?"true":"false"));}
        // A pad style's tilt (2026-10-25, playing lying down): the origin turned down about its right axis, every frame (the
        // setting can change in the game).
        if(game::padStyle.load()&&rootCamera::mode==2){const int tilt=game::padTiltDegrees.load();
            liveTrackingToAnchor=tilt?compose(Pose{game::padTilt(tilt),Vec3{0,0,0}},liveTrackingBase):liveTrackingBase;}
        inputPose::publish(liveFrame,liveTrackingToAnchor,liveOriginEpoch);
        ready=true;
    });
    return ready;
}
void closeLiveFrame(uintptr_t view){
    inputPose::close();
    withNativeQueues(view,[&](VkInstance,VkPhysicalDevice,VkDevice){
        auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        auto close=module?reinterpret_cast<diagnostic::CloseNativeXr>(GetProcAddress(module,"evrCloseNativeXr")):nullptr;
        if(!close||close())throw std::runtime_error("live native XR close failed");
    });
}
void submitLiveFrame(PairResources& pair){
    struct Done{~Done(){cpuTiming::pairDone();}} done;const cpuTiming::Span timed(cpuTiming::Submit);
    withNativeQueues(pair.originalView,[&](VkInstance,VkPhysicalDevice,VkDevice){
        auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        auto finish=module?reinterpret_cast<diagnostic::FinishNativeXr>(GetProcAddress(module,"evrFinishNativeXr")):nullptr;
        std::array<VkImage,2> images{};
        for(size_t eye=0;eye<2;++eye){
            const auto index=read<uint16_t>(reinterpret_cast<uintptr_t>(pair.targets[eye].data())+8);
            const auto texture=read<uintptr_t>(read<uintptr_t>(pair.graph+layout::graphTextures)+index*8);
            images[eye]=read<VkImage>(texture+0x30);
        }
        if(liveFrame.eyeMask!=3&&alternateEyes::stale.valid&&alternateEyes::stale.frame==liveFrame.frame){
            using StalePose=int(*)(uint64_t,uint32_t,const XrPosef*);
            const auto set=reinterpret_cast<StalePose>(GetProcAddress(module,"evrSetNativeXrStalePose"));
            if(set&&set(liveFrame.frame,alternateEyes::stale.eye,&alternateEyes::stale.pose)){
                static unsigned refused=0;if(refused++<3)logFile->event("alternate_eye_stale_pose_refused","\"frame\":"+std::to_string(liveFrame.frame));}
        }
        if(alternateEyes::worldMotion.valid&&alternateEyes::worldMotion.frame==liveFrame.frame){
            using WorldMotion=int(*)(uint64_t,const XrPosef*,int64_t);
            static const auto set=reinterpret_cast<WorldMotion>(GetProcAddress(module,"evrSetNativeXrWorldMotion"));
            if(set)set(liveFrame.frame,&alternateEyes::worldMotion.motion,alternateEyes::worldMotion.dt);
        }
        if(!finish||finish(liveFrame.frame,images.data()))throw std::runtime_error("live native XR submission failed");
        for(size_t eye=0;eye<2;++eye)if(liveFrame.eyeMask>>eye&1){alternateEyes::drawn[eye]=alternateEyes::pendingDrawn[eye];alternateEyes::pendingDrawn[eye]={};}
        using Captured=int(*)(uint64_t);
        const auto captured=reinterpret_cast<Captured>(GetProcAddress(module,"evrNativeFrameCaptured"));
        if(captured&&captured(liveFrame.frame)==1){
            for(size_t eye=0;eye<2;++eye){
                const auto p=reinterpret_cast<uintptr_t>(pair.parameters[eye].data());
                const auto position=read<WorldPositionQ32>(p+0x38);const auto orientation=read<Quat>(p+0x50);
                std::ostringstream f;f<<std::setprecision(10)<<"\"frame\":"<<liveFrame.frame<<",\"time\":"<<liveFrame.time
                    <<",\"serial\":"<<pair.serial<<",\"eye\":"<<eye<<",\"camera_q32\":["<<position.x<<','<<position.y<<','<<position.z
                    <<"],\"orientation\":["<<orientation.x<<','<<orientation.y<<','<<orientation.z<<','<<orientation.w
                    <<"],\"history_valid\":"<<unsigned(read<uint8_t>(pair.created[eye]+layout::viewHistoryValid))<<",\"pingpong\":"<<read<uint32_t>(pair.created[eye]+layout::viewPingPong);
                for(auto item:{std::pair<const char*,size_t>{"view_matrix",0x1000},{"projection_matrix",0x1080}}){ // 1.0 (0xa80, 0xb00 before)
                    auto m=read<std::array<float,16>>(pair.created[eye]+item.second);if(item.second==0x1000){m[3]=m[7]=m[11]=0;m[15]=1;}
                    f<<",\""<<item.first<<"\":[";for(size_t n=0;n<16;++n){if(n)f<<',';f<<m[n];}f<<']';}
                std::ostringstream descriptorHex;descriptorHex<<std::hex<<std::setfill('0');
                for(auto byte:read<std::array<uint8_t,80>>(pair.created[eye]+layout::viewProjection))descriptorHex<<std::setw(2)<<unsigned(byte);
                f<<",\"descriptor\":"<<jsonString(descriptorHex.str());
                logFile->event("captured_game_camera",f.str());
            }
        }
        // The right eye's DLSS feature handle (dlssEval::resetRightEvery, diagnostic).
        if(dlssEval::resetRightEvery>0)try{const auto ctx=read<uintptr_t>(pair.created[1]+layout::viewUpscaler+0x10);dlssEval::rightFeature=ctx?read<uintptr_t>(ctx+0xe8):0;}catch(...){dlssEval::rightFeature=0;}
        if(pair.pairedFrames==10||pair.pairedFrames==300){
            // Read-only upscaler ownership snapshot (2026-10-05 DLSS investigation): View+9b0 kind
            // (1 FSR3, 2 DLSS, 3 XeSS), +9b8 FSR object, +9c0 DLSS context, +9c8 XeSS context.
            // Executor 0xe1163c/0xe11c7d: context+0xe0 is the NGX parameter object, +0xe8 the feature handle.
            std::ostringstream o;o<<"\"pairs\":"<<pair.pairedFrames;
            const std::array<std::pair<const char*,uintptr_t>,3> views{{{"original",pair.originalView},{"left",pair.created[0]},{"right",pair.created[1]}}};
            for(const auto& [name,v]:views){
                const auto ctx=read<uintptr_t>(v+layout::viewUpscaler+0x10);
                o<<",\""<<name<<"\":{\"view\":"<<hex(v)<<",\"features\":"<<read<uint32_t>(v+0x10)<<",\"kind\":"<<unsigned(read<uint8_t>(v+layout::viewUpscaler))
                 <<",\"fsr\":"<<hex(read<uintptr_t>(v+layout::viewUpscaler+8))<<",\"dlss_context\":"<<hex(ctx)<<",\"dlss_params\":"<<hex(ctx?read<uintptr_t>(ctx+0xe0):0)
                 <<",\"dlss_feature\":"<<hex(ctx?read<uintptr_t>(ctx+0xe8):0)<<",\"xess\":"<<hex(read<uintptr_t>(v+layout::viewUpscaler+0x18))<<'}';
            }
            logFile->event("upscaler_context_snapshot",o.str());
        }
        if(controllerUiFixture::requested&&pair.pairedFrames==controllerFixtureArmPair){controllerUiFixture::armed=true;actionSequenceObserver::recording=true;logFile->event("controller_fixture_armed","\"submitted_pairs\":"+std::to_string(controllerFixtureArmPair));}
        logFile->event("live_native_pair_complete","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(pair.serial));
        if(pair.pairedFrames>=calibrationFrames||(worldSession&&sessionStop.load())){
            pair.sessionFinal=worldSession;
            inputPose::close();
            auto close=reinterpret_cast<diagnostic::CloseNativeXr>(GetProcAddress(module,"evrCloseNativeXr"));
            if(!close||close())throw std::runtime_error("live native XR close after final pair failed");
        }
    });
}

// early: the frame was begun early in this game frame (world_early_xr_frame) and is ended before the game's present -
// the game rendered no world for it (layer: evrSkipNativeXrEarly).
void skipLiveFrame(PairResources& pair,bool early=false){
    withNativeQueues(pair.originalView,[&](VkInstance,VkPhysicalDevice,VkDevice){
        auto module=GetModuleHandleW(L"evr_diagnostic.dll");
        auto skip=module?reinterpret_cast<diagnostic::SkipNativeXr>(GetProcAddress(module,early?"evrSkipNativeXrEarly":"evrSkipNativeXr")):nullptr;
        if(!skip||skip(liveFrame.frame))throw std::runtime_error(early?"live native XR early deferred frame failed":"live native XR deferred frame failed");
    });
    logFile->event("world_warmup_frame_ended","\"frame\":"+std::to_string(liveFrame.frame)+
        ",\"serial\":"+std::to_string(pair.serial)+",\"layers\":0,\"warmup_frames\":"+std::to_string(pair.warmupFrames));
}
// The right eye and the desktop refresh's original View draw with the left eye's View streaming object (2026-10-19;
// EVR_SHARE_EYE_STREAMING=0 for each View's own): the object's chunks become ready only through the terrain update, which
// one View a frame runs (worldSharedStages: the left eye, the original View on the refresh frames), so the other objects
// fell behind whenever the view moved fast - the right eye's fine detail radius (+0xc444, the distance to the nearest
// chunk not ready, 0x1e3f56) fell from 33 m to 4 m while falling (Mock game-20261019-055216) and the ruins below went
// see-through in the right eye on every picture and in the left on 4 of 6; lent to the right eye alone, both eyes lost
// the tower only on the picture right after a refresh (-060125, the original View's own object 30 frames old). Running
// the update for the right eye as well crashes (the brick jobs write through the closed frame arena, 0x13ab449 <-
// 0x5875b, -055635 and 10-18). With the left eye's object every View picks its bricks and detail from one state.
inline const bool shareStreaming=[]{char v[8]{};return GetEnvironmentVariableA("EVR_SHARE_EYE_STREAMING",v,sizeof v)!=1||v[0]!='0';}();
// With one eye drawn a frame (alternate eyes), the right eye drawn alone takes the left eye's objects as well - the
// streaming object, the many-lights shadows, the fog (viewLend) and the exposure (2026-10-23, the user after the headset:
// "the outlines still shake, even standing; the fog flickers fast"). Each was lent only on a frame that also drew the
// left eye, so with one eye a frame each eye kept its own fog, exposure, detail and point-light shadows - the left/right
// differences the lends had taken away came back, and every eye showed its own and the other eye's in turns. On a right
// eye's frame the left eye's objects are idle (the left View is not drawn), and each object is still updated once a
// frame. EVR_AFW_SHARE_LEFT=0: as before.
inline const bool afwShareLeft=[]{char v[8]{};return GetEnvironmentVariableA("EVR_AFW_SHARE_LEFT",v,sizeof v)!=1||v[0]!='0';}();
struct StreamingLend {uintptr_t view=0,own=0;bool lent=false;
    StreamingLend(uintptr_t to,uintptr_t left,bool want){if(!want||!to||!left)return;
        try{own=read<uintptr_t>(to+layout::viewStreaming);const auto l=read<uintptr_t>(left+layout::viewStreaming);
            if(own&&l&&own!=l){*reinterpret_cast<uintptr_t*>(to+layout::viewStreaming)=l;view=to;lent=true;}}catch(...){}}
    StreamingLend(const StreamingLend&)=delete;StreamingLend& operator=(const StreamingLend&)=delete;
    ~StreamingLend(){if(lent)*reinterpret_cast<uintptr_t*>(view+layout::viewStreaming)=own;}};
// The right eye draws with the left eye's many-lights shadow object (View+0xf28: made by 0xc8970 - "could not create
// many lights shadows view" -, updated by 0xca330 at 0x1367f1). Experiment, EVR_SHARE_MANY_LIGHT_SHADOWS=1 (2026-10-21,
// the user: "the shadows differ between the eyes when entering a building, and settle a little later"; RenderDoc
// game-20261021-105907 frame 791 in the user's torch-lit ruin: the 3072x2048 point-light shadow atlas of the two eyes
// differed in 4 of 384 tiles - a light's shadow in one eye's tile, the other's empty, or the same light in another tile -
// and the lighting came out 4 % darker in the left eye at the same points of the world, while GI and the sun's shadow
// were the same). Each eye keeps its own atlas and fills it on its own; with one object both eyes read one atlas.
// Lent (game-20261021-111156, the same frame): one atlas for both eyes, the lighting's left/right ratio 0.961 -> 0.998
// and after DLSS 0.974 -> 1.008 at the same points of the world. On by default; EVR_SHARE_MANY_LIGHT_SHADOWS=0: each its own.
constexpr uintptr_t viewManyLightShadows=0xf28;
inline const bool shareManyLightShadows=[]{char v[8]{};return !(GetEnvironmentVariableA("EVR_SHARE_MANY_LIGHT_SHADOWS",v,sizeof v)==1&&v[0]=='0');}();
struct FieldLend {uintptr_t view=0,own=0,offset=0;bool lent=false;
    FieldLend(uintptr_t to,uintptr_t left,uintptr_t at,bool want):offset(at){if(!want||!to||!left)return;
        try{own=read<uintptr_t>(to+at);const auto l=read<uintptr_t>(left+at);
            if(own&&l&&own!=l){*reinterpret_cast<uintptr_t*>(to+at)=l;view=to;lent=true;}}catch(...){}}
    FieldLend(const FieldLend&)=delete;FieldLend& operator=(const FieldLend&)=delete;
    ~FieldLend(){if(lent)*reinterpret_cast<uintptr_t*>(view+offset)=own;}};
// The many-lights shadow object is updated once a headset frame (2026-10-23; the user's reports "lights with parallax:
// a door's handle, metal parts, the night's lights", also offline). RenderDoc game-20261023-020815, pair 600 in the user's
// night village: with the object lent (FieldLend above) the right eye's frame updated it once more - 0xca3c0, called at
// 0x1368f1 with View+0xf28, the graph context and its parameters - and re-rendered 4 of the 384 tiles of the shared
// 3072x2048 point-light shadow atlas between the two eyes' lighting (13924..13956 and 19278..19310): a flickering
// fire's shadow, the leaves moved. The left eye lit with the first, the right with the second, and the fire's light on
// the leaves above it differed at the same points of the world. A second update of the same object within one headset
// frame is skipped now (its return value is not used at the call), so both eyes light with one atlas.
// EVR_SHADOW_UPDATE_ONCE=0: as before.
namespace shadowUpdateOnce {
using Update=void(__fastcall*)(void*,void*,void*,void*);
inline Update original=nullptr;inline bool enabled=false;
constexpr uintptr_t rva=0xca3c0,callerRva=0x1368f1;
constexpr const char* pattern="48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 B8 FC FF FF 48 81 EC 08 04 00 00";
inline std::mutex mutex;inline void* lastObject=nullptr;inline uint64_t lastFrame=0;inline std::atomic<uint64_t> updates{0},skipped{0};
__declspec(noinline) void __fastcall update(void* object,void* graph,void* parameters,void* fourth){
    bool skip=false;
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+callerRva+5){
        std::lock_guard<std::mutex> lock(mutex);const uint64_t frame=liveFrame.frame;
        if(frame&&object&&object==lastObject&&frame==lastFrame)skip=true;else{lastObject=object;lastFrame=frame;}}
    if(skip){const auto n=++skipped;
        if(n<=3||n%1800==0){const ObserverLastError preserve;try{logFile->event("shadow_update_skipped","\"frame\":"+std::to_string(liveFrame.frame)+",\"skipped\":"+std::to_string(n)+",\"updates\":"+std::to_string(updates.load())+",\"object\":"+hex(reinterpret_cast<uintptr_t>(object)));}catch(...){}}
        return;}
    ++updates;original(object,graph,parameters,fourth);
}
inline void install(){
    {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SHADOW_UPDATE_ONCE",v,sizeof v);if(n==1&&v[0]=='0'){logFile->event("shadow_update_once_off","\"reason\":\"EVR_SHADOW_UPDATE_ONCE=0\"");return;}}
    if(!shareManyLightShadows){logFile->event("shadow_update_once_off","\"reason\":\"the shadow object is not lent (EVR_SHARE_MANY_LIGHT_SHADOWS=0)\"");return;}
    const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva,call=baseAddress+callerRva;
    if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)
        throw std::runtime_error("shadow update signature or caller changed");
    if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("shadow update hook failed");
    enabled=true;logFile->event("shadow_update_once_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva));
}
inline void disable() noexcept {
    if(!enabled)return;
    const auto status=renderHookRetirement::disable(reinterpret_cast<void*>(baseAddress+rva));if(status==MH_OK)enabled=false;
    try{logFile->event("shadow_update_once_removed","\"status\":"+std::to_string(status)+",\"updates\":"+std::to_string(updates.load())+",\"skipped\":"+std::to_string(skipped.load()));}catch(...){}
}
}
// (A) one View state for both eyes (2026-10-21, the user: "the fixes feel ad hoc - is something more basic wrong?"): the two
// eyes are two whole Views, so everything the game keeps from frame to frame grows on its own in each eye. To find all of
// it at once, EVR_VIEW_COMPARE=1 writes, just before the right eye's render of the pairs viewCompare::serials, the three
// Views (left, right, the desktop's; layout::viewBytes each, view-compare-<serial>-<name>.bin) and, for every 8 bytes the
// eyes hold differently that look like pointers, the first 0x100 bytes each side points at (view-compare-<serial>.jsonl).
// Read-only. docs/analysis/view_state_20261021.
namespace viewCompare {
inline bool wanted(){static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_VIEW_COMPARE",v,sizeof v);return n==1&&v[0]=='1';}();return on;}
constexpr std::array<uint64_t,5> serials{600,601,602,603,1500};   // 603 since 2026-10-24: four pairs in a row (a field that alternates)
// the bytes taken at each differing pointer (EVR_VIEW_COMPARE_BYTES=<hex>, 0x100 by default; 2026-10-24 the water: what alternates
// frame by frame in the left eye's objects and not in the right eye's)
inline size_t pointedBytes(){static const size_t n=[]{char v[16]{};const auto k=GetEnvironmentVariableA("EVR_VIEW_COMPARE_BYTES",v,sizeof v);const auto x=k&&k<sizeof v?size_t(std::strtoull(v,nullptr,16)):0x100;return std::clamp<size_t>(x,0x100,0x10000);}();return n;}
inline std::vector<uint8_t> bytesAt(uintptr_t address,size_t n){
    std::vector<uint8_t> data(n);for(size_t at=0;at<n;at+=0x100){SIZE_T got=0;const auto k=std::min<size_t>(0x100,n-at);
        if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address+at),data.data()+at,k,&got))std::fill_n(data.data()+at,k,uint8_t(0));}
    return data;}
inline std::string hexOf(const std::vector<uint8_t>& b){static const char* d="0123456789abcdef";std::string s;s.reserve(b.size()*2);for(auto v:b){s+=d[v>>4];s+=d[v&15];}return s;}
// serial: the pairs counted from the first right eye's render this run (pair.serial counts from the game's start).
inline std::atomic<uint64_t> pairsSeen{0};
inline void dump(uint64_t,uintptr_t left,uintptr_t right,uintptr_t desktop) noexcept {
    if(!wanted())return;const auto serial=++pairsSeen;
    if(std::find(serials.begin(),serials.end(),serial)==serials.end())return;
    try{
        const std::array<std::pair<const char*,uintptr_t>,3> views{{{"left",left},{"right",right},{"desktop",desktop}}};
        std::array<std::vector<uint8_t>,3> data;
        for(size_t i=0;i<3;++i){data[i]=bytesAt(views[i].second,layout::viewBytes);
            std::ofstream(calibrationDirectory/("view-compare-"+std::to_string(serial)+"-"+views[i].first+".bin"),std::ios::binary).write(reinterpret_cast<const char*>(data[i].data()),std::streamsize(data[i].size()));}
        std::ofstream out(calibrationDirectory/("view-compare-"+std::to_string(serial)+".jsonl"));
        out<<"{\"serial\":"<<serial<<",\"left\":\""<<hex(left)<<"\",\"right\":\""<<hex(right)<<"\",\"desktop\":\""<<hex(desktop)<<"\",\"base\":\""<<hex(baseAddress)<<"\"}\n";
        unsigned pointers=0;
        for(size_t at=0;at+8<=layout::viewBytes;at+=8){
            uint64_t l=0,r=0,d=0;std::memcpy(&l,data[0].data()+at,8);std::memcpy(&r,data[1].data()+at,8);std::memcpy(&d,data[2].data()+at,8);
            if(l==r)continue;
            const auto pointerLike=[](uint64_t v){return v>=0x10000&&v<0x7fffffffffffull&&!(v&7);};
            if(!pointerLike(l)||!pointerLike(r))continue;
            ++pointers;
            out<<"{\"offset\":\""<<hex(at)<<"\",\"left\":\""<<hex(l)<<"\",\"right\":\""<<hex(r)<<"\",\"desktop\":\""<<hex(d)
               <<"\",\"left_bytes\":\""<<hexOf(bytesAt(l,pointedBytes()))<<"\",\"right_bytes\":\""<<hexOf(bytesAt(r,pointedBytes()))<<"\",\"desktop_bytes\":\""<<hexOf(bytesAt(d,pointedBytes()))<<"\"}\n";}
        logFile->event("view_compare_written","\"serial\":"+std::to_string(serial)+",\"pointers_differing\":"+std::to_string(pointers));
    }catch(...){}
}
}
// (A), the general lending: the right eye renders with the left eye's object for each listed View field, like the
// streaming and many-lights lends above (FieldLend: put in just before the right eye's render, put back after its CPU
// work). Experiment, EVR_LEND_VIEW_FIELDS="<hex>,<hex>;<frame>:<hex>,..." - groups apart by ';', a group with "<frame>:"
// starts at that XR frame (liveFrame.frame, the key script's numbers; 0 without) and stays. The fields that hold the eye's own
// owner, self pointers or camera (view-compare-600, game-20261021-170950: +0x0 the per-eye owner, +0x240/+0x3e0 point
// into the View itself, +0x8d0/+0xfc0 the camera, +0x8e0/+0xfd0, +0x80/+0x840/+0xf48 per-eye slots next to each other)
// are refused. Each group's lent and not lent fields are logged when it starts (view_lend_group).
// Mock 2026-10-21 (the user's save, lent from a frame on, no failures, the pictures right): +0xee0/+0xef0, +0xf78..+0xf90,
// +0x9b0..+0x9c0, +0xb40/+0xb48/+0xec8 (game-20261021-173451, -174628); +0x988/+0x990 (the sun's shadows) ends the series
// (shadow_projection_failure "owner topology": the cascades are shared there already, the right eye's setup taking the
// left eye's camera block and its map the left eye's copy, layer/shadow_share.hpp).
// The fog (+0xb40, layout::viewFog) is lent by default (EVR_SHARE_EYE_FOG=0: each eye its own): with the draw
// camera at report 161239 (the user's "shadows of another depth in the forest beyond the rock tunnel": a pond with mist
// before the forest) and the head turned twice, the mist over the water stood in one eye only - the left/right ratio of
// the pictures 0.974..0.984 and 6..12 % of the blocks more than 10 % apart (stereo_lr.py) in a turn of every run without
// the fog lent (game-20261021-181553 second turn, -182713 both turns with the water's +0xb48/+0xec8 lent), 0.993..0.999
// and 0.7..3 % in both turns with it (-183812). Each eye's fog volume grows from its own history; the copy of 10-17
// (layer/gi_reproject.hpp fogEnabled, off since 10-20) made one eye's mist dense at the feet. The user on the headset
// (19:01, 0.9.6): "smoother, more natural all over; the only problem is a reflection on the water in the left eye only"
// (bug report 190412 at the pond: bright streaks of the water's reflection along the left eye's left edge, fewer in the
// right eye; the eyes' water also of another tint) - the screen-space reflections (+0xf18/+0xf20, layout::viewSsr, two
// slots) were lent too (EVR_SHARE_EYE_SSR=1; off by default since 20:40): one reflection history for both eyes, as
// with the fog. The user on the headset (20:35): "the reflections feel different but it is not solved - they reflect
// something other than where the head moves; compare the reflections at the pond's edge". Mock 2026-10-21 game-20261021-201120 (the draw camera at the report's pond, three head turns: own, the
// fog, the fog and the reflections): no breakage, the eyes' left/right ratio and blocks the same in the second and third
// turns (0.994..1.015, 0..2.4 %); the Mock's camera stands lower than the user's and showed no one-eyed reflection in
// any turn, so the effect is for the headset to tell. Guess: the reflections, averaged over both eyes' frames, lose a
// little of their depth.
namespace viewLend {
struct Group {uint64_t from=0;std::vector<uintptr_t> fields;bool said=false;};
inline std::vector<Group> groups;
inline std::atomic<uint64_t> pairs{0},lends{0};
inline const bool parsed=[]{
    char v[1024]{};const auto n=GetEnvironmentVariableA("EVR_LEND_VIEW_FIELDS",v,sizeof v);if(n>=sizeof v)return false;
    if(!n){const auto off=[](const char* name){char f[8]{};const auto k=GetEnvironmentVariableA(name,f,sizeof f);return k==1&&f[0]=='0';};
        Group g{0,{}};if(!off("EVR_SHARE_EYE_FOG"))g.fields.push_back(layout::viewFog);
        {char f[8]{};const auto k=GetEnvironmentVariableA("EVR_SHARE_EYE_SSR",f,sizeof f);if(k==1&&f[0]=='1'){g.fields.push_back(layout::viewSsr);g.fields.push_back(layout::viewSsr+8);}}
        if(g.fields.empty())return false;groups.push_back(std::move(g));return true;}
    constexpr std::array<uintptr_t,11> refused{0x0,0x8,0x80,0x240,0x3e0,0x840,0x8d0,0x8e0,0xf48,0xfc0,0xfd0};
    std::string text(v,n);size_t at=0;
    while(at<=text.size()){
        const auto end=std::min(text.find(';',at),text.size());std::string part=text.substr(at,end-at);at=end+1;
        Group g;const auto colon=part.find(':');
        if(colon!=std::string::npos){g.from=std::strtoull(part.substr(0,colon).c_str(),nullptr,10);part=part.substr(colon+1);}
        size_t p=0;
        while(p<part.size()){const auto comma=std::min(part.find(',',p),part.size());const auto item=part.substr(p,comma-p);p=comma+1;
            if(item.empty())continue;const auto offset=uintptr_t(std::strtoull(item.c_str(),nullptr,16));
            if(offset%8||offset+8>layout::viewBytes||std::find(refused.begin(),refused.end(),offset)!=refused.end())continue;
            g.fields.push_back(offset);}
        if(!g.fields.empty())groups.push_back(std::move(g));
    }
    return !groups.empty();}();
struct Scope {
    std::vector<std::unique_ptr<FieldLend>> held;
    Scope(uintptr_t to,uintptr_t left,bool want){
        if(!parsed||!want||!to||!left)return;const auto pair=++pairs;const auto frame=liveFrame.frame;
        for(auto& g:groups){if(frame<g.from)continue;
            const auto first=held.size();
            for(const auto f:g.fields)held.push_back(std::make_unique<FieldLend>(to,left,f,true));
            if(!g.said){g.said=true;std::string lent,kept;
                for(size_t i=first;i<held.size();++i)(held[i]->lent?lent:kept)+=(std::string(held[i]->lent?(lent.empty()?"":","):(kept.empty()?"":","))+hex(held[i]->offset));
                try{logFile->event("view_lend_group","\"pair\":"+std::to_string(pair)+",\"frame\":"+std::to_string(frame)+",\"from\":"+std::to_string(g.from)+",\"lent\":["+lent+"],\"not_lent\":["+kept+"]");}catch(...){}}
        }
        size_t n=0;for(const auto& h:held)n+=h->lent;
        const auto k=++lends;if(k<=3||k%1800==0)try{logFile->event("view_lend","\"pair\":"+std::to_string(pair)+",\"lent\":"+std::to_string(n)+",\"listed\":"+std::to_string(held.size()));}catch(...){}
    }
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    ~Scope(){for(auto it=held.rbegin();it!=held.rend();++it)it->reset();}
};
}
void recordPair(PairResources &pair,void* context,void* parameters,void* target){
    const cpuTiming::Span timed(cpuTiming::Record);
    if(pair.pairPending)throw std::runtime_error("previous native eye pair still pending");
    pair.pairPending=true;
    if(pair.history.failed)throw std::runtime_error("culling history bridge rejected native state; ending VR diagnostic");
    if(pair.water&&pair.water->failed)throw std::runtime_error("water feedback bridge rejected native results; ending VR diagnostic");
    waterFeedback::Scope waterScope(pair.water.get());
    pair.frameReady=true;
    if(worldNativeDiagnostic)worldSharedDiagnostic::snapshot("pair_begin",pair.scene);
    // Does a change of the game's settings (the reflections) reach the eyes? Their Views copy the original View's features
    // and take SSR objects of their own once, when they are made (setupFeatures) - 2026-10-22, the user: "with the
    // reflections off, the reflection still differs". Told when the original View's features (+0x10) or the Renderer's
    // two SSR objects change from what they were then, and what the eyes' Views hold (settings_change_seen).
    {static uintptr_t view0=0;static uint32_t features0=0;static std::array<uintptr_t,2> ssr0{};static unsigned told=0;
     try{const auto renderer=read<uintptr_t>(pair.scene+layout::sceneRenderer);const auto features=read<uint32_t>(pair.originalView+0x10);
      const std::array<uintptr_t,2> ssr{read<uintptr_t>(renderer+layout::rendererSsr),read<uintptr_t>(renderer+layout::rendererSsr+8)};
      if(view0!=pair.originalView){view0=pair.originalView;features0=features;ssr0=ssr;}
      else if((features!=features0||ssr!=ssr0)&&told<50){++told;
       logFile->event("settings_change_seen","\"original_features\":["+std::to_string(features0)+","+std::to_string(features)+"],\"renderer_ssr_before\":["+hex(ssr0[0])+","+hex(ssr0[1])+
        "],\"renderer_ssr\":["+hex(ssr[0])+","+hex(ssr[1])+"],\"eye_features\":["+std::to_string(pair.created[0]?read<uint32_t>(pair.created[0]+0x10):0)+","+
        std::to_string(pair.created[1]?read<uint32_t>(pair.created[1]+0x10):0)+"],\"eye_ssr\":["+hex(pair.created[0]?read<uintptr_t>(pair.created[0]+layout::viewSsr):0)+","+
        hex(pair.created[1]?read<uintptr_t>(pair.created[1]+layout::viewSsr):0)+"],\"original_ssr\":["+hex(read<uintptr_t>(pair.originalView+layout::viewSsr))+","+hex(read<uintptr_t>(pair.originalView+layout::viewSsr+8))+"]");
       features0=features;ssr0=ssr;}}catch(...){}}
    // The eyes' own SSR objects against the Renderer's (2026-10-22, the user: "the water blurs, like a mosaic, the more the
    // nearer the view comes to the water's level; with VR off nothing of it"): the factory 0x17a3c0 makes 0x4b8 bytes and sets
    // no quality there, so whatever the game puts in later may reach only the Renderer's pair. EVR_SSR_COMPARE=1 (diagnostic):
    // at pairs 600 and 1800 the 32-bit words that differ, pointers on both sides left out (ssr_object_compare).
    {static const bool compare=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_SSR_COMPARE",v,sizeof v);return n==1&&v[0]=='1';}();
     static uint64_t pairs=0;const auto n=++pairs;
     if(compare&&(n==600||n==1800))try{
        const auto renderer=read<uintptr_t>(pair.scene+layout::sceneRenderer);
        const auto pointer=[](uint64_t v){return v>=0x10000&&v<0x800000000000ull;};
        for(unsigned eye=0;eye<2;eye++)for(unsigned i=0;i<2;i++){
            const auto original=read<uintptr_t>(renderer+layout::rendererSsr+i*8);
            const auto own=pair.created[eye]?read<uintptr_t>(pair.created[eye]+layout::viewSsr+i*8):0;
            std::ostringstream o;o<<"\"pair\":"<<n<<",\"eye\":"<<eye<<",\"slot\":"<<i<<",\"original\":"<<hex(original)<<",\"own\":"<<hex(own)<<",\"differ\":[";
            unsigned shown=0,differ=0;
            if(original&&own&&original!=own){
                const auto a=read<std::array<uint8_t,0x4b8>>(original),b=read<std::array<uint8_t,0x4b8>>(own);
                for(size_t at=0;at+4<=a.size();at+=4){
                    uint32_t x=0,y=0;std::memcpy(&x,a.data()+at,4);std::memcpy(&y,b.data()+at,4);if(x==y)continue;
                    const size_t q=at&~size_t(7);uint64_t qx=0,qy=0;std::memcpy(&qx,a.data()+q,8);std::memcpy(&qy,b.data()+q,8);
                    if(pointer(qx)&&pointer(qy))continue;
                    ++differ;if(shown++<120){float fx=0,fy=0;std::memcpy(&fx,&x,4);std::memcpy(&fy,&y,4);
                        o<<(shown>1?",":"")<<"["<<hex(at)<<","<<x<<","<<y<<","<<(std::isfinite(fx)?fx:0.f)<<","<<(std::isfinite(fy)?fy:0.f)<<"]";}
                }
            }
            o<<"],\"differing_words\":"<<differ;
            logFile->event("ssr_object_compare",o.str());
        }
     }catch(const std::exception& e){logFile->event("ssr_object_compare_failed","\"reason\":"+jsonString(e.what()));}}
    using TextureCreate=void* (__fastcall *)(void*,void*,const void*);
    using TextureImport=void* (__fastcall *)(void*,void*,void*,uint8_t,const uint64_t*);
    const auto source=reinterpret_cast<uintptr_t>(target);
    const auto currentGraph=read<uintptr_t>(source);
    if(pair.graph&&pair.graph!=currentGraph)throw std::runtime_error("calibration graph changed");
    pair.graph=currentGraph;const auto index=read<uint16_t>(source+8);
    const auto capacity=read<uint64_t>(pair.graph+layout::graphCapacity);
    if(!capacity||capacity>65535||index>=capacity||read<uint64_t>(pair.graph+layout::graphFreeCount)<6)throw std::runtime_error("invalid calibration target table");
    const auto descriptor=read<std::array<uint8_t,48>>(read<uintptr_t>(pair.graph+layout::graphDescriptors)+size_t(index)*48);
    if(descriptor[0x15]!=37||read<uint32_t>(source+0x18)!=0)throw std::runtime_error("unsupported calibration target");
    const auto input=reinterpret_cast<uintptr_t>(parameters);
    const auto originalParameters=read<std::array<uint8_t,layout::parameterBytes>>(input);
    vfxTransparent::originalCamera=read<WorldPositionQ32>(input+0x38);   // the owner of the effects on a refresh frame (vfx_transparent_bridge.hpp passRecord)
    if(read<uint8_t>(input+0x7c)!=0)throw std::runtime_error("calibration requires perspective game camera");
    const auto baseRotation=read<Quat>(input+0x50);
    game::StereoCameras runtimeCameras{};
    if(runtimeEyes()){
        const diagnostic::RecordedXrFrame frame=liveXr?diagnostic::RecordedXrFrame{liveFrame.frame,liveFrame.time,liveFrame.views}:recordedXr.frames.at(pair.pairedFrames);
        game::StableAnchor anchor;anchor.root=read<WorldPositionQ32>(input+0x38);anchor.orientation=baseRotation;bool anchorHeightCalibrated=false;
        if(rootCamera::mode){const auto sample=rootCamera::snapshot();
            const auto currentSerial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
            if(!sample.renderEntryMilliseconds||sample.renderSerial!=currentSerial)throw std::runtime_error("root snapshot does not belong to this render serial");
            rootCamera::logSample(sample,frame.frame);
            if(rootCamera::mode==2){anchor=rootCamera::renderAnchor(sample,anchor.root,anchor.orientation);anchorHeightCalibrated=rootCamera::calibratedEntity!=0;}}
        anchor=sceneryAnchor(anchor);
        runtimeCameras=game::stereoCameras(anchor,liveXr?liveTrackingToAnchor:recordedXr.trackingToAnchor,frame.views,3,frame.time,frame.frame,
                                         anchor.root,read<float>(input+0x70),read<float>(input+0x74));
        if(liveXr){alternateEyes::current=runtimeCameras;alternateEyes::nearZ=read<float>(input+0x70);alternateEyes::farZ=read<float>(input+0x74);alternateEyes::currentFrame=liveFrame.frame;}
        if(liveXr)targetMark::noteAnchor(anchor,liveTrackingToAnchor,read<WorldPositionQ32>(input+0x38),baseRotation,read<float>(input+0x78));   // the locked target's mark (target_mark.hpp)
        // A world camera's pose in the tracking space under an anchor (stereoCameras: world = anchor * trackingPose(trackingToAnchor * eye)).
        auto trackingOf=[&](const game::StableAnchor& a,const WorldPositionQ32& position,const Quat& orientation){
            const auto o=normalized(a.orientation);
            const auto root=cameraPosition(a.root,rotate(o,a.cameraOffsetGameUnits),1);
            const auto back=inverse(o);
            const Vec3 offset{float(relativeQ32(position.x,root.x)),float(relativeQ32(position.y,root.y)),float(relativeQ32(position.z,root.z))};
            return compose(inverse(liveTrackingToAnchor),game::trackingPose(Pose{normalized(multiply(back,orientation)),rotate(back,offset)}));
        };
        // The third-person pad style: the UI image ahead of the game camera - its reticle where the stick aims (pad_settings.hpp).
        if(liveXr&&game::padStyle.load()==2&&rootCamera::mode==2)padSettings::plate(trackingOf(anchor,read<WorldPositionQ32>(input+0x38),baseRotation));
        alternateEyes::worldMotion={};
        if(liveXr){
            static game::StableAnchor lastAnchor{};static bool lastValid=false;static uint64_t lastFrame=0,lastSession=0;static int64_t lastTime=0;
            if(lastValid&&lastSession==liveFrame.sessionGeneration&&liveFrame.frame==lastFrame+1&&anchor.worldUnitsPerMeter==1&&lastAnchor.worldUnitsPerMeter==1&&liveFrame.time>lastTime){
                const auto& eye=runtimeCameras.eyes[0];
                const auto motion=compose(trackingOf(anchor,eye.position,eye.orientation),inverse(trackingOf(lastAnchor,eye.position,eye.orientation)));
                if(finite(motion.position)&&length(motion.position)<5.f)alternateEyes::worldMotion={liveFrame.frame,true,motion,int64_t(liveFrame.time-lastTime)};
            }
            lastAnchor=anchor;lastValid=true;lastFrame=liveFrame.frame;lastSession=liveFrame.sessionGeneration;lastTime=liveFrame.time;
        }
        alternateEyes::stale={};
        if(liveXr&&liveFrame.eyeMask!=3){
            // stereoCameras: world = anchor * trackingPose(trackingToAnchor * eye). The eye left out was drawn with the
            // world camera `drawn`; the tracking pose that this frame's anchor turns into that camera is the one its
            // picture is shown at.
            const uint32_t eye=liveFrame.eyeMask==1?1u:0u;const auto& d=alternateEyes::drawn[eye];
            if(d.valid&&d.session==liveFrame.sessionGeneration&&anchor.worldUnitsPerMeter==1){
                const auto orientation=normalized(anchor.orientation);
                const auto rootCamera=cameraPosition(anchor.root,rotate(orientation,anchor.cameraOffsetGameUnits),1);
                const auto back=inverse(orientation);
                const Vec3 offset{float(relativeQ32(d.position.x,rootCamera.x)),float(relativeQ32(d.position.y,rootCamera.y)),float(relativeQ32(d.position.z,rootCamera.z))};
                const Pose local{normalized(multiply(back,d.orientation)),rotate(back,offset)};
                const auto pose=compose(inverse(liveTrackingToAnchor),game::trackingPose(local));
                if(finite(pose.position)&&length(offset)<50.f){alternateEyes::stale={liveFrame.frame,eye,true,pose};++alternateEyes::turned;}
            }
        }
        std::ostringstream fields;fields<<std::setprecision(9)<<"\"recorded_xr_frame\":"<<frame.frame<<",\"camera_q32\":["<<anchor.root.x<<','<<anchor.root.y<<','<<anchor.root.z
            <<"],\"orientation\":["<<anchor.orientation.x<<','<<anchor.orientation.y<<','<<anchor.orientation.z<<','<<anchor.orientation.w
            <<"],\"camera_offset\":["<<anchor.cameraOffsetGameUnits.x<<','<<anchor.cameraOffsetGameUnits.y<<','<<anchor.cameraOffsetGameUnits.z
            <<"],\"near\":"<<read<float>(input+0x70)<<",\"far\":"<<read<float>(input+0x74)<<",\"anchor_kind\":"
            <<jsonString(rootCamera::mode==2?(rootCamera::firstPerson(rootCamera::snapshot())?"stable_root_first_person_diagnostic":"native_event_or_special_camera_diagnostic"):"original_render_camera_diagnostic_only");
        const auto nativePosition=read<WorldPositionQ32>(input+0x38);
        fields<<",\"native_camera_q32\":["<<nativePosition.x<<','<<nativePosition.y<<','<<nativePosition.z
              <<"],\"native_orientation\":["<<baseRotation.x<<','<<baseRotation.y<<','<<baseRotation.z<<','<<baseRotation.w<<']';
        if(rootCamera::mode==2)fields<<",\"height_calibrated\":"<<(anchorHeightCalibrated?"true":"false");
        logFile->event("recorded_xr_anchor",fields.str());
    }
    const float phase=calibrationFrames>1?float(pair.pairedFrames)/float(calibrationFrames-1)*2*pi:0;
    const auto rotation=multiply(baseRotation,euler(.3f*std::sin(phase),.15f*std::sin(phase*2),.1f*std::sin(phase*3)));
    const auto headOffset=rotate(baseRotation,{.1f*std::sin(phase),.03f*std::sin(phase*2),.05f*(std::cos(phase)-1)});
    const auto right=rotate(rotation,{1,0,0});
    pair.serial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
    vfxSpringRebase::frameEyes={pair.serial,{runtimeCameras.eyes[0].position,runtimeCameras.eyes[1].position},runtimeEyes()};
    pair.renderThread=GetCurrentThreadId();
    pair.graphics=read<uintptr_t>(read<uintptr_t>(pair.scene+layout::sceneRenderer)+layout::rendererGraphics);
    struct WatchWindow{explicit WatchWindow(uint32_t pairs){objectWatch::renderThread=GetCurrentThreadId();objectWatch::sampling=objectWatch::enabled&&pairs>=300&&pairs<=302;}~WatchWindow(){objectWatch::sampling=false;}} watchWindow(pair.pairedFrames);
    const uint64_t sync=0;
    {   // Render preferences as f7c50 receives them (+2f8 sharpness -> NIS pass when >0.01, +2fc, +300); logged when they change.
        static std::array<float,3> lastPrefs{-1,-1,-1};
        const auto prefs=read<std::array<float,3>>(input+layout::parameterSharpness);
        if(prefs!=lastPrefs){lastPrefs=prefs;std::ostringstream f;f<<std::setprecision(6)<<"\"serial\":"<<pair.serial<<",\"sharpness\":"<<prefs[0]
            <<",\"pref_2fc\":"<<prefs[1]<<",\"pref_300\":"<<prefs[2];logFile->event("render_preferences_seen",f.str());}
        // +258..+287 feed the post pass block (106d9c..106dc2); expected to carry the post vignette colour/strength.
        static std::array<uint32_t,12> lastPost{};static uint32_t postLogs=0;
        const auto post=read<std::array<uint32_t,12>>(input+layout::parameterPost);
        if(post!=lastPost&&postLogs<60){lastPost=post;++postLogs;std::ostringstream f;f<<std::setprecision(5)<<"\"serial\":"<<pair.serial<<",\"floats\":[";
            for(size_t i=0;i<post.size();++i){float v;std::memcpy(&v,&post[i],4);f<<(i?",":"")<<(std::isfinite(v)?v:0.0f);}
            f<<"],\"words\":[";for(size_t i=0;i<post.size();++i)f<<(i?",":"")<<post[i];f<<"]";logFile->event("render_post_params_seen",f.str());}
    }
    uiHudBridge::apply();
    if(statusVignette::vrTint){
        // Low health from the game (+268 the vignette's colour - black while there is none -, +274 its pulsing alpha:
        // include/evr/status_rim.hpp) and the freezing rim; colours are the VR tint's own.
        const auto low=read<std::array<float,4>>(input+layout::parameterPost+0x10);
        const float lowA=evr::game::lowHealthRim(low)*edgeTintBridge::maxStrength;
        {static int lastRim=-1;static unsigned rimLogs=0;const int rim=lowA>0;
            if(rim!=lastRim&&rimLogs<200){lastRim=rim;++rimLogs;std::ostringstream f;f<<std::setprecision(5)<<"\"serial\":"<<pair.serial<<",\"on\":"<<(rim?"true":"false")
                <<",\"vignette\":["<<low[0]<<','<<low[1]<<','<<low[2]<<','<<low[3]<<"],\"strength\":"<<lowA;logFile->event("low_health_rim",f.str());}}
        const float frostA=std::clamp(statusVignette::frost.load(),0.0f,1.0f)*edgeTintBridge::maxStrength;
        const float sum=lowA+frostA;
        const auto& l=edgeTintBridge::lowHealth;const auto& f=edgeTintBridge::freezing;
        if(sum>0)edgeTintBridge::apply((lowA*l[0]+frostA*f[0])/sum,(lowA*l[1]+frostA*f[1])/sum,(lowA*l[2]+frostA*f[2])/sum,std::max(lowA,frostA));
        else edgeTintBridge::apply(1,1,1,0);
    }
    for(size_t eye=0;eye<2;eye++){
        if(liveXr&&!(liveFrame.eyeMask>>eye&1))continue;   // one eye a frame: the other keeps its picture
        pair.parameters[eye]=read<std::array<uint8_t,layout::parameterBytes>>(input);
        if(vrSharpness>=0)put<float>(pair.parameters[eye],layout::parameterSharpness,vrSharpness);
        // VR tint mode: the game's in-eye low health vignette (+274 = w of the post colour) and additive flash (+284) off.
        if(statusVignette::vrTint){put<float>(pair.parameters[eye],layout::parameterPost+0x1c,0.0f);put<float>(pair.parameters[eye],layout::parameterPost+0x2c,0.0f);}
        put<Quat>(pair.parameters[eye],0x50,rotation);
        // Synthetic 64 mm calibration baseline; not an OpenXR/HMD pose implementation.
        const double distance=eye?.032:-.032;
        const std::array<float,3> axis{right.x,right.y,right.z};
        const std::array<float,3> translation{headOffset.x,headOffset.y,headOffset.z};
        for(size_t n=0;n<3;n++){
            const auto originalPosition=read<int64_t>(input+0x38+n*8);
            put<int64_t>(pair.parameters[eye],0x38+n*8,originalPosition+int64_t(std::llround((translation[n]+distance*axis[n])*4294967296.0)));
        }
        if(runtimeEyes()){put<WorldPositionQ32>(pair.parameters[eye],0x38,runtimeCameras.eyes[eye].position);
            put<Quat>(pair.parameters[eye],0x50,runtimeCameras.eyes[eye].orientation);}
        if(matchCameraFov){
            const auto &fov=runtimeCameras.eyes[eye].fov;
            const float envelope=2*std::max(std::abs(fov.angleUp),std::abs(fov.angleDown));
            put<float>(pair.parameters[eye],0x78,envelope);
            logFile->event("native_auxiliary_camera_fov","\"eye\":"+std::to_string(eye)+",\"original\":"+std::to_string(read<float>(input+0x78))+",\"envelope\":"+std::to_string(envelope));
        }
        if(!read<uintptr_t>(reinterpret_cast<uintptr_t>(pair.targets[eye].data()))){
        auto eyeDescriptor=descriptor;
        if(runtimeEyes()){put<uint32_t>(eyeDescriptor,4,eyeWidth(eye));put<uint32_t>(eyeDescriptor,8,eyeHeight(eye));}
        reinterpret_cast<TextureCreate>(baseAddress+0x1012ce0)(pair.targets[eye].data(),reinterpret_cast<void*>(pair.graph),eyeDescriptor.data());
        const auto handle=reinterpret_cast<uintptr_t>(pair.targets[eye].data());
        if(read<uintptr_t>(handle)!=pair.graph)throw std::runtime_error("calibration target allocation failed");
        const auto ownedIndex=read<uint16_t>(handle+8);
        if(ownedIndex>=capacity)throw std::runtime_error("calibration owned index out of range");
        const auto texture=read<uintptr_t>(read<uintptr_t>(pair.graph+layout::graphTextures)+ownedIndex*8);
        const auto backend=read<uintptr_t>(pair.graphics+0x278);
        const auto device=read<VkDevice>(backend+layout::backendDevice);
        if(dlssEval::requested&&liveXr){dlssEval::diagDevice=device;dlssEval::registerDiagnostics=true;}
        if(dlssEval::depthNotes&&liveXr)dlssEval::diagDevice=device;   // the depth notes of the eye warp need the device only
        const auto vulkan=GetModuleHandleW(L"vulkan-1.dll");
        auto getDevice=vulkan?reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(vulkan,"vkGetDeviceProcAddr")):nullptr;
        auto nameImage=getDevice?reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(getDevice(device,"vkSetDebugUtilsObjectNameEXT")):nullptr;
        if(liveXr){
            auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
            using Track=int (*)(VkDevice,VkImage,uint32_t);
            auto track=layer?reinterpret_cast<Track>(GetProcAddress(layer,"evrTrackNativeEyeImage")):nullptr;
            if(!track||track(device,read<VkImage>(texture+0x30),uint32_t(eye)))throw std::runtime_error("native eye image registration failed");
        }else if(!nameImage)throw std::runtime_error("calibration requires debug object naming");
        const auto name=std::string("EVR calibration eye ")+std::to_string(eye);
        VkDebugUtilsObjectNameInfoEXT naming{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        naming.objectType=VK_OBJECT_TYPE_IMAGE;naming.objectHandle=read<uint64_t>(texture+0x30);naming.pObjectName=name.c_str();
        if(nameImage&&nameImage(device,&naming)!=VK_SUCCESS)throw std::runtime_error("calibration target naming failed");
        reinterpret_cast<TextureImport>(baseAddress+0x1012f90)(pair.aliases[eye].data(),reinterpret_cast<void*>(pair.graph),reinterpret_cast<void*>(texture),0,&sync);
        const auto alias=reinterpret_cast<uintptr_t>(pair.aliases[eye].data());
        if(read<uintptr_t>(alias)!=pair.graph||(read<uint32_t>(alias+8)&0x06000000)!=0x04000000)throw std::runtime_error("calibration import flags mismatch");
        std::memcpy(pair.targetArguments[eye].data(),pair.aliases[eye].data(),16);
        }
        const auto ownedIndex=read<uint16_t>(reinterpret_cast<uintptr_t>(pair.targets[eye].data())+8);
        const auto texture=read<uintptr_t>(read<uintptr_t>(pair.graph+layout::graphTextures)+ownedIndex*8);
        selectView(pair.scene,pair.created[eye]);
        logFile->event("calibration_eye_begin","\"eye\":"+std::to_string(eye)+",\"serial\":"+std::to_string(pair.serial)+",\"view\":"+hex(pair.created[eye])+",\"image\":"+hex(read<uintptr_t>(texture+0x30)));
        dataWatch::tick(pair.created[0],pair.created[1],liveFrame.frame);
        // Diagnostic (2026-10-18 21:10, EVR_FIND_TERRAIN_RADIUS=<XR frame>): which pointer of the eye Views leads to the object
        // whose +0x1780 holds 5, 1 / radius, 1023 (0x854a0 copies it into the terrain constants; View+0xed0 was a wrong guess).
        if(eye==1){static const uint64_t findAt=[]{char v[24]{};return GetEnvironmentVariableA("EVR_FIND_TERRAIN_RADIUS",v,sizeof v)?std::strtoull(v,nullptr,10):0ull;}();
            if(findAt&&liveFrame.frame==findAt){unsigned hits=0;
                for(int k=0;k<3;k++){const uintptr_t v=k<2?pair.created[k]:pair.originalView;if(!v)continue;
                    std::vector<uintptr_t> slots(0x8000/8);SIZE_T got=0;ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(v),slots.data(),slots.size()*8,&got);
                    for(size_t i=0;i<got/8;i++){const auto p=slots[i];if(p<0x10000||p>0x7fffffffffffull||(p&7))continue;
                        uint32_t t[3]{};SIZE_T g2=0;if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p+0x1780),t,12,&g2)||g2!=12)continue;
                        float f;std::memcpy(&f,&t[1],4);
                        if(t[0]==5&&t[2]==1023&&f>1e-4f&&f<10.f&&hits++<40)logFile->event("terrain_radius_owner","\"view\":"+std::to_string(k)+",\"view_offset\":"+hex(i*8)+",\"object\":"+hex(p)+",\"inv_radius\":"+std::to_string(f)+",\"radius_m\":"+std::to_string(1.f/f));}
                }
                logFile->event("terrain_radius_owner_search","\"frame\":"+std::to_string(liveFrame.frame)+",\"hits\":"+std::to_string(hits));
            }}
        // Diagnostic (2026-10-18 20:30, EVR_SCAN_TERRAIN_RADIUS=<XR frame>[,<XR frame>]): where the terrain's per-View detail
        // radius lives. The headset capture game-20261018-195604 (RenderDoc): the terrain draw's constants at 0x110 hold 5,
        // 1 / the radius of the fine displacement (left eye 0.0299 = 33.5 m, right eye 0.677 = 1.5 m; a Mock capture 0.0274
        // in both) and 1023 - the right eye's soil is flat beyond 1.5 m. On the right eye's begin of those frames every
        // writable page is searched for that triple; each hit is logged with its place relative to the Views.
        if(eye==1){static const std::vector<uint64_t> scanAt=[]{std::vector<uint64_t> v;char s[64]{};if(GetEnvironmentVariableA("EVR_SCAN_TERRAIN_RADIUS",s,sizeof s)){
                std::string t=s;size_t p=0;while(p<t.size()){const auto q=t.find(',',p);v.push_back(std::strtoull(t.substr(p,q-p).c_str(),nullptr,10));if(q==std::string::npos)break;p=q+1;}}return v;}();
            if(std::find(scanAt.begin(),scanAt.end(),liveFrame.frame)!=scanAt.end()){
                const auto started=GetTickCount64();std::vector<uint32_t> chunk(1u<<18);unsigned hits=0;uint64_t scanned=0;
                const uintptr_t views[3]={pair.created[0],pair.created[1],pair.originalView};
                MEMORY_BASIC_INFORMATION mbi{};uintptr_t at=0x10000;
                while(at<0x7ffffffe0000ull&&VirtualQuery(reinterpret_cast<void*>(at),&mbi,sizeof mbi)==sizeof mbi&&GetTickCount64()-started<30000){
                    const auto base=reinterpret_cast<uintptr_t>(mbi.BaseAddress);const auto size=mbi.RegionSize;at=base+size;
                    if(mbi.State!=MEM_COMMIT||mbi.Type==MEM_IMAGE||(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS|PAGE_WRITECOMBINE|PAGE_NOCACHE))||!(mbi.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))continue;
                    for(uintptr_t off=0;off<size;off+=chunk.size()*4-8){
                        const size_t want=size_t(std::min<uint64_t>(chunk.size()*4,size-off));SIZE_T got=0;
                        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+off),chunk.data(),want,&got)||got<12)break;scanned+=got;
                        for(size_t i=0;i+2<got/4;i++)if(chunk[i]==5&&chunk[i+2]==1023){float f;std::memcpy(&f,&chunk[i+1],4);
                            if(!(f>1e-4f&&f<4.f))continue;const auto addr=base+off+i*4;std::string where="\"none\"";
                            for(int k=0;k<3;k++)if(views[k]&&addr>=views[k]&&addr<views[k]+0x40000){where="{\"view\":"+std::to_string(k)+",\"offset\":"+hex(addr-views[k])+"}";break;}
                            if(hits++<400)logFile->event("terrain_radius_scan_hit","\"frame\":"+std::to_string(liveFrame.frame)+",\"address\":"+hex(addr)+",\"value\":"+std::to_string(f)+",\"radius_m\":"+std::to_string(1.f/f)+
                                ",\"region\":"+hex(base)+",\"region_bytes\":"+std::to_string(size)+",\"type\":"+std::to_string(mbi.Type)+",\"in\":"+where);}
                        if(want<chunk.size()*4)break;
                    }
                }
                logFile->event("terrain_radius_scan","\"frame\":"+std::to_string(liveFrame.frame)+",\"hits\":"+std::to_string(hits)+",\"scanned_mb\":"+std::to_string(scanned>>20)+",\"ms\":"+std::to_string(GetTickCount64()-started)+
                    ",\"views\":["+hex(views[0])+","+hex(views[1])+","+hex(views[2])+"]");
            }}
        if(worldNativeDiagnostic){
            const auto renderer=read<uintptr_t>(pair.scene+layout::sceneRenderer),pipeline=read<uintptr_t>(renderer+0x16e10);
            logFile->event("world_eye_readiness_before","\"eye\":"+std::to_string(eye)+
                ",\"scene_disabled\":"+std::to_string(read<uint8_t>(pair.scene+0x2c))+
                ",\"view_count\":"+std::to_string(read<uint32_t>(renderer+0x16c94))+
                ",\"view_limit\":"+std::to_string(read<uint32_t>(renderer+0x16c90))+
                ",\"renderer_service\":"+hex(read<uintptr_t>(renderer+0x14a80))+
                ",\"pipeline\":"+hex(pipeline)+",\"pipeline_state\":"+(pipeline?std::to_string(read<uint32_t>(pipeline+0xbc)):"null")+
                ",\"view_ready\":"+std::to_string(read<uint8_t>(pair.created[eye]+layout::viewReady)));
        }
        // From this point, graph/CPU/GPU work may retain all of these allocations.
        pair.recorded=true;
        const bool lightingReadyBefore=!worldNativeDiagnostic||read<uint8_t>(pair.created[eye]+layout::viewReady)!=0;
        const auto historyBefore=pair.history.substitutions[eye];
        const auto generatedBefore=pair.history.generatedCalls[eye+1];
        const auto detailBefore=detailBinding::substitutions[eye];
        const auto shadowBefore=shadowProjection::converted[eye];
        const auto fogBefore=fogProjection::converted[eye];
        fogProjection::Scope fogScope(pair.parameters[eye].data()+0x38,runtimeEyes()?&runtimeCameras.eyes[eye].descriptor:nullptr,unsigned(eye),liveFrame.frame,pair.serial);
        struct ProjectionScope{ProjectionScope(const game::ProjectionDescriptor* d){eyeProjection=d;projectionCalls=0;}~ProjectionScope(){eyeProjection=nullptr;}} projectionScope(runtimeEyes()?&runtimeCameras.eyes[eye].descriptor:nullptr);
        worldReadyDiagnostic::Scope readiness;
        stableFrustumBridge::Context eyeFrustum;eyeFrustum.serial=pair.serial;eyeFrustum.frame=liveFrame.frame;
        eyeFrustum.eye=int(eye);eyeFrustum.apply=stableFrustumBridge::stabilizeEyes;
        struct ReadyScope {ReadyScope(worldReadyDiagnostic::Scope* scope){worldReadyDiagnostic::current=scope;}~ReadyScope(){worldReadyDiagnostic::current=nullptr;}};
        // Kept until the eye's CPU tasks finished (finishEyeCpuTasks below), not just until f7c50 returns.
        std::optional<giShare::Scope> giScope;
        std::optional<StreamingLend> streamingLend;   // until the eye's CPU work is done (finishEyeCpuTasks)
        std::optional<FieldLend> shadowLend;          // the same: the many-lights shadow object (EVR_SHARE_MANY_LIGHT_SHADOWS=0: off)
        std::optional<viewLend::Scope> fieldLends;    // the same: the fields of EVR_LEND_VIEW_FIELDS (viewLend above)
        std::optional<giShare::BorrowScope> giBorrow;   // the right eye lit by the left eye's screen GI (borrow-screen form)
        std::optional<DofScope> dofScope; // also kept until the eye CPU tasks finished
        worldSharedStages::Context sharedStage;
        terrainReadyDiagnostic::Context terrainStage;
        downloadFeedbackDiagnostic::Context downloadStage;
        sharedPointLights::Context lightStage;lightStage.pair=&pair;lightStage.eye=int(eye);
        if(worldNativeDiagnostic){sharedStage.shared=read<uintptr_t>(pair.scene+0xa88);sharedStage.terrain=read<uintptr_t>(pair.scene+0xa80);sharedStage.serial=pair.serial;sharedStage.eye=int(eye);}
        {ReadyScope readyScope(worldNativeDiagnostic?&readiness:nullptr);
            terrainReadyDiagnostic::Scope terrainScope(worldNativeDiagnostic?&terrainStage:nullptr);
            downloadFeedbackDiagnostic::Scope downloadScope(worldNativeDiagnostic?&downloadStage:nullptr);
            worldSharedStages::Scope stageScope(worldNativeDiagnostic?&sharedStage:nullptr);
            cullingHistory::Scope cullingScope(cullingHistory::requested?&pair:nullptr,int(eye));
            detailBinding::Scope detailScope(pair,unsigned(eye));
            shadowProjection::Scope shadowScope(pair,unsigned(eye));
            sharedPointLights::Scope lightScope(lightStage);
            vfxTransparent::EyeScope transparentScope(pair,unsigned(eye));
            vfxModelBridge::EyeScope modelScope(pair,unsigned(eye));
            VfxTakeoverScope vfxTakeover(pair.scene,pair.originalView,pair.created[eye],desktopSkipVfx&&worldSharedStages::vfxOwnerEye>=0&&int(eye)==worldSharedStages::vfxOwnerEye);
            vfxTransparent::FrameScope vfxOwnerSource(&pair,pair.serial,pair.parameters[eye].data(),vfxTakeover.view!=0,pair.created[eye]);
            grassMovementBridge::EyeScope grassScope(pair.serial,int(eye),vfxTakeover.view!=0);
            shadowHistoryProbe::ViewScope shadowProbeScope(pair.created[eye]);
            // VFX lights/particles of this frame serve both eyes: cull them against both eye frusta.
            stableFrustumBridge::VfxWiden vfxWiden;bool widenVfx=false;
            if(vfxTakeover.view&&vfxCulling::enabled&&runtimeEyes()){
                try{vfxWiden.bounds=game::unionTangents(liveFrame.views[0],liveFrame.views[1],vfxUnionMinDistance,1000.f);
                    vfxWiden.eye=runtimeCameras.eyes[eye].descriptor;vfxWiden.wide=game::widenedDescriptor(vfxWiden.eye,vfxWiden.bounds);widenVfx=true;}
                catch(const std::exception& e){if(++vfxUnionFailures<=3)logFile->event("vfx_culling_union_failure","\"serial\":"+std::to_string(pair.serial)+",\"reason\":"+jsonString(e.what()));}
            }
            stableFrustumBridge::VfxScope vfxWidenScope(widenVfx?&vfxWiden:nullptr);
            struct VfxWidenReport{stableFrustumBridge::VfxWiden& w;bool used;uint64_t serial;
                ~VfxWidenReport(){if(!used)return;try{
                    const bool ok=!w.failed&&w.builds==1&&w.applied==1;
                    if(ok)++vfxUnionApplied;else ++vfxUnionFailures;
                    if((!ok&&vfxUnionFailures<=3)||vfxUnionApplied<=3||vfxUnionApplied%100==0){
                        std::ostringstream f;f<<std::setprecision(6)<<"\"serial\":"<<serial<<",\"applied\":"<<(ok?"true":"false")<<",\"builds\":"<<w.builds
                            <<",\"stage_calls\":"<<w.applied<<",\"reason\":"<<jsonString(w.reason)<<",\"union_tan\":["<<w.bounds.left<<','<<w.bounds.right<<','<<w.bounds.down<<','<<w.bounds.up
                            <<"],\"frames_applied\":"<<vfxUnionApplied<<",\"failures\":"<<vfxUnionFailures;
                        logFile->event("vfx_culling_union",f.str());}
                }catch(...){}}} vfxWidenReport{vfxWiden,widenVfx,pair.serial};
            // Right eye: GI probes come from the left eye's GI object (updated once this frame); only screen GI reruns.
            giScope.emplace(pair.graph,pair.created[0],pair.created[1],giShare::requested&&eye==1&&liveFrame.eyeMask==3&&pair.pairedFrames>=giShare::warmupPairs);
            giBorrow.emplace(pair.graph,pair.created[0],pair.created[1],giShare::borrowScreen&&!giShare::requested&&eye==1&&liveFrame.eyeMask==3&&pair.pairedFrames>=giShare::warmupPairs);
            if(giBorrow->on&&(giShare::borrowFrames<=3||giShare::borrowFrames%1000==0))
                logFile->event("gi_borrow_screen","\"serial\":"+std::to_string(pair.serial)+",\"frames\":"+std::to_string(giShare::borrowFrames)+",\"right_gi\":"+hex(giBorrow->rightGi)+",\"screen_gi_skipped\":"+std::to_string(giShare::skipped[20].load()));
            if(giScope->on&&(giShare::frames<=3||giShare::frames%100==0)){
                std::ostringstream f;f<<"\"serial\":"<<pair.serial<<",\"frames\":"<<giShare::frames<<",\"left_gi\":"<<hex(giScope->shared)<<",\"right_gi\":"<<hex(giScope->own)<<",\"skipped_before\":[";
                for(size_t i=0;i<giShare::skipped.size();++i)f<<(i?",":"")<<giShare::skipped[i].load();f<<']';
                logFile->event("gi_share",f.str());}
            if(vfxTakeover.view&&(desktopSkipVfxFrames<=3||desktopSkipVfxFrames%100==0))
                logFile->event("desktop_skip_vfx_owner","\"serial\":"+std::to_string(pair.serial)+",\"frames\":"+std::to_string(desktopSkipVfxFrames)+
                    ",\"vfx_view\":"+hex(read<uintptr_t>(pair.created[eye]+layout::viewVfx)));
            stableFrustumBridge::Scope eyeFrustumScope(stableFrustumBridge::observeEyes?&eyeFrustum:nullptr);
            struct WatchEye{explicit WatchEye(int e){objectWatch::currentEye=e;}~WatchEye(){objectWatch::currentEye=-1;}} watchEye(int(eye));
            dofScope.emplace(pair.created[eye],liveXr&&!keepDepthOfField);
            // One exposure for both eyes (eyeExposure above): the right eye's update is followed by the left eye's state.
            struct ExposurePair{explicit ExposurePair(uintptr_t from,uintptr_t to){eyeExposure::source=from;eyeExposure::target=to;}
                ~ExposurePair(){eyeExposure::source=0;eyeExposure::target=0;}};
            std::optional<ExposurePair> exposurePair;
            const bool leftShared=(liveFrame.eyeMask&1)||(afwShareLeft&&alternateEyes::requested&&liveFrame.eyeMask==2);
            if(eye==1&&eyeExposure::enabled&&leftShared)try{
                const auto from=read<uintptr_t>(pair.created[0]+layout::viewPost),to=read<uintptr_t>(pair.created[1]+layout::viewPost);
                if(from&&to&&from!=to)exposurePair.emplace(from,to);
            }catch(...){}
            if(eye==1)viewCompare::dump(pair.serial,pair.created[0],pair.created[1],pair.originalView);
            // The right eye draws with the left eye's View streaming object (StreamingLend above).
            {const cpuTiming::Span eyeTimed(eye?cpuTiming::Eye1:cpuTiming::Eye0);
             streamingLend.emplace(eye==1?pair.created[1]:0,pair.created[0],shareStreaming&&eye==1&&leftShared);
             shadowLend.emplace(eye==1?pair.created[1]:0,pair.created[0],viewManyLightShadows,shareManyLightShadows&&eye==1&&leftShared);
             {static std::atomic<uint64_t> shadowLends{0};if(shadowLend->lent){const auto n=++shadowLends;if(n<=3||n%1800==0)logFile->event("eye_many_light_shadows_lent","\"serial\":"+std::to_string(pair.serial)+
                 ",\"lends\":"+std::to_string(n)+",\"own\":"+hex(shadowLend->own)+",\"left\":"+hex(read<uintptr_t>(pair.created[1]+viewManyLightShadows)));}
              else if(shareManyLightShadows&&eye==1){static std::atomic<unsigned> notLent{0};if(notLent.fetch_add(1)<3)logFile->event("eye_many_light_shadows_not_lent","\"serial\":"+std::to_string(pair.serial)+
                 ",\"right\":"+hex(read<uintptr_t>(pair.created[1]+viewManyLightShadows))+",\"left\":"+hex(read<uintptr_t>(pair.created[0]+viewManyLightShadows)));}}
             {static std::atomic<uint64_t> lends{0};if(streamingLend->lent){const auto n=++lends;if(n<=3||n%1800==0)logFile->event("eye_streaming_lent","\"serial\":"+std::to_string(pair.serial)+
                 ",\"lends\":"+std::to_string(n)+",\"own\":"+hex(streamingLend->own)+",\"left\":"+hex(read<uintptr_t>(pair.created[1]+layout::viewStreaming)));}}
             fieldLends.emplace(eye==1?pair.created[1]:0,pair.created[0],eye==1&&leftShared);
             original(pair.results[eye].data(),context,reinterpret_cast<void*>(pair.created[eye]),pair.parameters[eye].data(),pair.targetArguments[eye].data());}}
        if(stableFrustumBridge::observeEyes&&(eyeFrustum.failed||(projectionCalls==1&&(eyeFrustum.calls!=2||eyeFrustum.mask!=3))))
            throw std::runtime_error("eye frustum diagnostic boundary differs");
        if(lightStage.failed)throw std::runtime_error("shared point lights rejected native input");
        if(detailBinding::failed)throw std::runtime_error("detail shader owner substitution failed");
        if(shadowProjection::failed)throw std::runtime_error("shadow projection conversion failed");
        if(fogProjection::failed)throw std::runtime_error("fog projection conversion failed");
        if(vfxTransparent::failed)throw std::runtime_error("transparent VFX observer rejected native input");
        if(bodyUpload::failed)throw std::runtime_error("body upload observer rejected native input");
        const auto historyCalls=pair.history.substitutions[eye]-historyBefore;
        if(cullingHistory::requested&&!cullingHistory::fillOnly)
            logFile->event("culling_history_eye","\"serial\":"+std::to_string(pair.serial)+",\"eye\":"+std::to_string(eye)+
                ",\"substitutions\":"+std::to_string(historyCalls));
        if(pair.history.failed)throw std::runtime_error("culling history rejected private culling output");
        logWorldColorState(pair.created[eye],int(eye),pair.serial,pair.parameters[eye].data(),pair.results[eye].data());
        if(worldNativeDiagnostic)terrainReadyDiagnostic::write(terrainStage,int(eye),pair.serial);
        if(worldNativeDiagnostic)downloadFeedbackDiagnostic::write(downloadStage,int(eye),pair.serial);
        if(worldNativeDiagnostic){
            const bool lightingReadyAfter=read<uint8_t>(pair.created[eye]+layout::viewReady)!=0;
            if(cullingHistory::requested&&worldSharedStages::once&&lightingReadyBefore&&lightingReadyAfter){
                if(pair.history.generatedCalls[eye+1]-generatedBefore!=1)
                    throw std::runtime_error("ready eye omitted shared generated instance count");
                ++pair.history.generatedAuditedEyes;
            }
            if(cullingHistory::requested&&!cullingHistory::fillOnly&&lightingReadyBefore&&lightingReadyAfter&&historyCalls!=1)
                throw std::runtime_error("ready eye omitted private visibility history");
            if(sharedPointLights::requested&&lightingReadyBefore&&lightingReadyAfter&&lightStage.calls!=1)
                throw std::runtime_error("ready eye omitted clustered light borrowing");
            detailBinding::auditEye(unsigned(eye),lightingReadyBefore,lightingReadyAfter,detailBefore);
            shadowProjection::auditEye(unsigned(eye),lightingReadyBefore,lightingReadyAfter,shadowBefore);
            fogProjection::audit(unsigned(eye),lightingReadyBefore,lightingReadyAfter,fogBefore);
            if(!lightingReadyBefore||!lightingReadyAfter)pair.frameReady=false;
            logFile->event("world_lighting_readiness","\"eye\":"+std::to_string(eye)+",\"serial\":"+std::to_string(pair.serial)+
                ",\"before\":"+(lightingReadyBefore?"true":"false")+",\"after\":"+(lightingReadyAfter?"true":"false"));
        }
        if(worldNativeDiagnostic)worldReadyDiagnostic::write(readiness,eye);
        if(worldNativeDiagnostic){worldSharedStages::write(sharedStage);if(sharedStage.rejected)throw std::runtime_error("shared world update lacks same-frame original proof");}
        if(worldNativeDiagnostic)worldSharedDiagnostic::snapshot(eye?"right_return":"left_return",pair.scene);
        eyeProjection=nullptr;
        if(liveXr&&runtimeEyes())alternateEyes::pendingDrawn[eye]={true,liveFrame.sessionGeneration,runtimeCameras.eyes[eye].position,runtimeCameras.eyes[eye].orientation,liveFrame.frame};
        if(runtimeEyes()){
            if(projectionCalls!=1){
                logFile->event("native_eye_projection_incomplete","\"eye\":"+std::to_string(eye)+",\"calls\":"+std::to_string(projectionCalls)+
                    ",\"view_serial\":"+std::to_string(read<uint64_t>(pair.created[eye]+layout::viewSerial))+
                    ",\"view_ready\":"+std::to_string(read<uint8_t>(pair.created[eye]+layout::viewReady))+
                    ",\"projection_tag\":"+std::to_string(read<uint32_t>(pair.created[eye]+layout::viewProjection)));
                // Native 966780 starts a per-View asynchronous preparation;
                // 9625b0 returns false until it completes. Never force ready.
                if(worldNativeDiagnostic&&projectionCalls==0&&readiness.count==5&&!readiness.overflow&&
                    readiness.observations[4].entry==4&&!readiness.observations[4].result&&
                    std::all_of(readiness.observations.begin(),readiness.observations.begin()+4,[](const auto& o){return o.result;})){
                    pair.frameReady=false;continue;
                }
                throw std::runtime_error("per-eye projection override count differs from one");
            }
            if(read<std::array<uint8_t,80>>(pair.created[eye]+layout::viewProjection)!=runtimeCameras.eyes[eye].descriptor.bytes)
                throw std::runtime_error("native saved per-eye descriptor mismatch");
            const auto& fov=runtimeCameras.eyes[eye].fov;
            std::ostringstream fields;fields<<std::setprecision(9)<<"\"eye\":"<<eye<<",\"serial\":"<<pair.serial<<",\"recorded_xr_frame\":"<<runtimeCameras.xrFrame
                <<",\"recorded_predicted_time\":"<<runtimeCameras.predictedDisplayTime<<",\"projection_calls\":"<<projectionCalls
                <<",\"fov\":["<<fov.angleLeft<<','<<fov.angleRight<<','<<fov.angleUp<<','<<fov.angleDown<<"],\"expected_projection\":[";
            for(size_t n=0;n<16;n++){if(n)fields<<',';fields<<runtimeCameras.eyes[eye].projection.values[n];}fields<<"],\"live_openxr_session\":"<<(liveXr?"true":"false");
            logFile->event("recorded_xr_eye_applied",fields.str());
        }
        // 1.0 (2026-10-16): the eye's own streaming or fog still busy after the bounded wait goes on (logged above as
        // incomplete). While a world streams in, a View's streaming work runs across frames, as for the game's own View;
        // ending the series there left the headset without the world.
        {const cpuTiming::Span tasksTimed(eye?cpuTiming::Tasks1:cpuTiming::Tasks0);
         finishEyeCpuTasks(pair.scene);}
        fieldLends.reset();streamingLend.reset();shadowLend.reset();giScope.reset();giBorrow.reset();dofScope.reset();
        worldSharedStages::detailRadius::finishEye(int(eye),pair.serial);   // the right eye's terrain detail radius (2026-10-18)
        // Diagnostic (2026-10-19, EVR_DUMP_STREAMING_AT=<frame>[,<frame>...]): at those live frames, after the right eye, the
        // View streaming objects ([View+0x978], 128 KB each - the terrain detail radius sits at +0xc444) of both eyes and of the
        // original View, and both eye Views (256 KB): which per-View values differ between the eyes while the right eye alone
        // shows the ruins see-through while falling (Mock game-20261019-053537: left eye and original View drew them).
        if(eye==1){static const std::vector<uint64_t> dumpAt=[]{std::vector<uint64_t> r;char v[256]{};const auto n=GetEnvironmentVariableA("EVR_DUMP_STREAMING_AT",v,sizeof v);
                if(n&&n<sizeof v){std::istringstream in(v);std::string t;while(std::getline(in,t,','))try{r.push_back(std::stoull(t));}catch(...){}}return r;}();
            static size_t dumped=0;
            if(dumped<dumpAt.size()&&liveFrame.frame>=dumpAt[dumped]){
                const auto tag=std::to_string(dumpAt[dumped++]);
                auto save=[&](const std::string& name,uintptr_t at,size_t bytes){if(!at)return;std::vector<uint8_t> b(bytes);SIZE_T got=0;
                    ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),b.data(),b.size(),&got);
                    std::ofstream out(calibrationDirectory/("streaming-"+tag+"-"+name+".bin"),std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(got));};
                const uintptr_t s0=read<uintptr_t>(pair.created[0]+layout::viewStreaming),s1=streamingLend&&streamingLend->lent?streamingLend->own:read<uintptr_t>(pair.created[1]+layout::viewStreaming),so=read<uintptr_t>(pair.originalView+layout::viewStreaming);
                save("eye0",s0,0x20000);save("eye1",s1,0x20000);save("orig",so,0x20000);save("view0",pair.created[0],0x40000);save("view1",pair.created[1],0x40000);
                logFile->event("streaming_dump","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(pair.serial)+",\"tag\":"+jsonString(tag)+
                    ",\"eye0\":"+hex(s0)+",\"eye1\":"+hex(s1)+",\"orig\":"+hex(so)+",\"radius\":["+std::to_string(s0?read<float>(s0+0xc444):0.f)+","+std::to_string(s1?read<float>(s1+0xc444):0.f)+","+std::to_string(so?read<float>(so+0xc444):0.f)+"]");}}
        // Diagnostic (2026-10-18 21:50, EVR_DUMP_VIEWS_ON_DIFF=1): both eye Views (256 KB each) on frames where the right eye's
        // terrain detail radius differed from the left eye's (the 1st, 30th and 150th such frame) and once where they agreed -
        // to find the per-View values behind it (the user: the trees' LOD distance differs between the eyes as well).
        if(eye==1){static const bool dumpWanted=[]{char v[8]{};return GetEnvironmentVariableA("EVR_DUMP_VIEWS_ON_DIFF",v,sizeof v)==1&&v[0]=='1';}();
            static unsigned diffFrames=0,sameDumps=0,diffDumps=0;
            if(dumpWanted){using namespace worldSharedStages::detailRadius;
                const bool diff=lastDiffSerial.load()==pair.serial,same=!diff&&lastSeenSerial.load()==pair.serial;if(diff)++diffFrames;
                if((diff&&(diffFrames==1||diffFrames==30||diffFrames==150))||(same&&sameDumps==0&&seen.load()>200)){
                    const std::string tag=diff?"diff"+std::to_string(diffDumps++):"same"+std::to_string(sameDumps++);
                    for(int k=0;k<2;k++){std::vector<uint8_t> bytes(0x40000);SIZE_T got=0;ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pair.created[k]),bytes.data(),bytes.size(),&got);
                        std::ofstream dump(calibrationDirectory/("views-"+tag+"-"+std::to_string(k)+".bin"),std::ios::binary);dump.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(got));}
                    logFile->event("views_dump_on_radius","\"tag\":"+jsonString(tag)+",\"serial\":"+std::to_string(pair.serial)+",\"left_inv\":"+std::to_string(lastLeft.load())+",\"right_inv\":"+std::to_string(lastRight.load()));}}}
        logFile->event("calibration_eye_recorded","\"eye\":"+std::to_string(eye)+",\"serial\":"+std::to_string(pair.serial)+",\"history_valid\":"+std::to_string(read<uint8_t>(pair.created[eye]+layout::viewHistoryValid))+",\"pingpong\":"+std::to_string(read<uint32_t>(pair.created[eye]+layout::viewPingPong))+",\"view_serial\":"+std::to_string(read<uint64_t>(pair.created[eye]+layout::viewSerial))+",\"gpu_verified\":false");
        std::ostringstream transforms;transforms<<std::setprecision(9)<<"\"eye\":"<<eye<<",\"serial\":"<<pair.serial;
        for(auto item:{std::pair<const char*,size_t>{"view_matrix",0x1000},{"projection_matrix",0x1080}}){ // 1.0 (0xa80, 0xb00 before)
            auto matrix=read<std::array<float,16>>(pair.created[eye]+item.second);transforms<<",\""<<item.first<<"\":[";
            // Native affine Matrix43 uses a 64-byte stride; its fourth column is padding.
            if(item.second==0x1000){matrix[3]=matrix[7]=matrix[11]=0;matrix[15]=1;}
            for(size_t n=0;n<16;n++){if(!std::isfinite(matrix[n]))throw std::runtime_error("nonfinite native calibration matrix");if(n)transforms<<',';transforms<<matrix[n];}transforms<<']';
        }
        transforms<<",\"camera_q32\":[";for(size_t n=0;n<3;n++){if(n)transforms<<',';transforms<<read<int64_t>(reinterpret_cast<uintptr_t>(pair.parameters[eye].data())+0x38+n*8);}transforms<<']';
        logFile->event("calibration_eye_transform",transforms.str());
    }
    selectView(pair.scene,pair.originalView);
    if(read<std::array<uint8_t,layout::parameterBytes>>(input)!=originalParameters)throw std::runtime_error("source render camera arguments changed during eye calls");
    bool heightWait=false;
    // (the third-person pad style shows the game camera: no first-person height to wait for, 2026-10-25)
    if(pair.frameReady&&!pair.pairedFrames&&rootCamera::mode==2&&!rootCamera::calibratedEntity&&game::padStyle.load()!=2){
        if(++pair.heightWaitFrames>heightWaitLimit)throw std::runtime_error("first-person height did not settle before the first live pair");
        heightWait=true;pair.frameReady=false;
        if(pair.heightWaitFrames==1||pair.heightWaitFrames%120==0)
            logFile->event("world_pair_waiting_height","\"serial\":"+std::to_string(pair.serial)+",\"frames\":"+std::to_string(pair.heightWaitFrames)+",\"limit\":"+std::to_string(heightWaitLimit));
    }
    if(!pair.frameReady){
        if(!heightWait&&++pair.warmupFrames>worldWarmupLimit)throw std::runtime_error("world View resource preparation exceeded 600 frames");
        logFile->event("world_pair_preparing","\"serial\":"+std::to_string(pair.serial)+",\"submitted\":false");return;
    }
    if(!pair.pairedFrames&&rootCamera::mode==2&&game::padStyle.load()!=2){
        // The first live pair must not freeze or follow a transitional native height (none in the third-person pad style).
        if(!rootCamera::calibratedEntity)throw std::runtime_error("first-person height did not settle before the first live pair");
        rootCamera::requireCalibrated=true;
        logFile->event("root_camera_height_required","\"serial\":"+std::to_string(pair.serial)+",\"provisional_anchors\":"+std::to_string(rootCamera::provisionalAnchors.load()));
    }
    ++pair.pairedFrames;
    logFile->event("calibration_pair_recorded","\"pair\":"+std::to_string(pair.pairedFrames)+",\"serial\":"+std::to_string(pair.serial));
    // Read-only diagnostic (world-gi-dump.txt, 2026-10-06): both eyes' GI objects (View+950, 0x1ce0 bytes),
    // the eye Views and the frame graph resource tables, on two consecutive pairs, for offline field classification.
    if(giDump&&(pair.pairedFrames==300||pair.pairedFrames==301))try{
        const auto dir=calibrationDirectory/"gi-dump";std::filesystem::create_directories(dir);
        const auto save=[&](const std::string& name,uintptr_t address,size_t bytes){
            std::vector<uint8_t> data(bytes);SIZE_T copied=0;
            if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),data.data(),bytes,&copied)||copied!=bytes)
                throw std::runtime_error("unreadable "+name);
            std::ofstream file(dir/(name+"-"+std::to_string(pair.pairedFrames)+".bin"),std::ios::binary);
            file.write(reinterpret_cast<const char*>(data.data()),std::streamsize(bytes));};
        std::ostringstream f;f<<"\"pair\":"<<pair.pairedFrames<<",\"serial\":"<<pair.serial<<",\"graph\":"<<hex(pair.graph);
        for(size_t eye=0;eye<2;++eye){
            const auto gi=read<uintptr_t>(pair.created[eye]+layout::viewGi);
            save("gi-eye"+std::to_string(eye),gi,0x1ce0);save("view-eye"+std::to_string(eye),pair.created[eye],layout::viewBytes);
            if(const auto fog=read<uintptr_t>(pair.created[eye]+layout::viewFog))save("fog-eye"+std::to_string(eye),fog,0x83e0); // volumetric fog (18a250 allocates 0x83e0)
            f<<",\"gi"<<eye<<"\":"<<hex(gi)<<",\"view"<<eye<<"\":"<<hex(pair.created[eye]);
        }
        const auto textures=read<uint64_t>(pair.graph+layout::graphCapacity),buffers=read<uint64_t>(pair.graph+0x118);
        if(textures&&textures<=65536)save("graph-texture-descriptors",read<uintptr_t>(pair.graph+layout::graphDescriptors),size_t(textures)*48);
        if(buffers&&buffers<=65536)save("graph-buffer-records",read<uintptr_t>(pair.graph+0x128),size_t(buffers)*16);
        save("graph-header",pair.graph,0x200);
        f<<",\"textures\":"<<textures<<",\"buffers\":"<<buffers;
        logFile->event("gi_dump",f.str());
    }catch(const std::exception& e){logFile->event("gi_dump_failure","\"reason\":"+jsonString(e.what()));}
}
// Back to the plate and on into VR again after a failure (2026-10-24 14:40; a player: "deep in the shroud, VR fell back to 2D -
// I have to restart the game for VR, and all the enemies I killed are back"; the user: "go back to the plate and return to VR").
// Before, one frame's failure ended the VR session for the rest of the start: the loader leaves its loop on a session's failure
// (loader.cpp vrWorker), and the same world cannot make a new pair while the failed one is kept (probe()). Now the frame's eyes are
// given up (the XR session is kept for the plate, as when a world is left: evrCloseNativeXr), the pair is marked sound again and
// the eyes begin again - at once after a first failure; while failures follow each other within 10 s, after 0.25, 0.5, 1, 2, 4,
// then every 5 s. Meanwhile the headset shows the game's own picture on the plate and the original View is drawn as without the
// mod. The reason stays in the log (calibration_series_failure, then world_frame_recovered). The hands' and weapons' models of
// the mod, when they were what failed, are tried again three times a session; after that VR goes on without them.
// EVR_RECOVER_FAILURES=0 in the game's environment: the session ends at the first failure, as before.
namespace failureRecovery {
inline const bool wanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_RECOVER_FAILURES",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<uint64_t> holdUntil{0};inline uint64_t lastFailure=0,count=0;inline unsigned streak=0,motionRetries=0;
inline bool held(){const auto until=holdUntil.load();return until&&GetTickCount64()<until;}
// Test only: EVR_TEST_FAIL_AT=<n>[-<last>],... fails the eyes' recording on those attempts (counted from 1 in the session: XR frame
// numbers start again with a new XR session, and a failing attempt does not make a pair); EVR_TEST_MOTION_FAIL_AT the same for the
// hands' and weapons' models (motionVisual).
inline uint64_t attempts=0,motionAttempts=0;   // motionAttempts: the models' preparations, counted from 1
inline std::vector<std::pair<uint64_t,uint64_t>> spans(const char* name){
    std::vector<std::pair<uint64_t,uint64_t>> out;char v[512]{};const auto n=GetEnvironmentVariableA(name,v,sizeof v);if(!n||n>=sizeof v)return out;
    std::stringstream in(v);std::string part;
    while(std::getline(in,part,',')){const auto dash=part.find('-');try{const uint64_t a=std::stoull(part.substr(0,dash)),b=dash==std::string::npos?a:std::stoull(part.substr(dash+1));if(b>=a)out.push_back({a,b});}catch(...){}}
    return out;
}
inline bool due(const std::vector<std::pair<uint64_t,uint64_t>>& at,uint64_t frame){for(const auto& s:at)if(frame>=s.first&&frame<=s.second)return true;return false;}
inline const auto failAt=spans("EVR_TEST_FAIL_AT");inline const auto motionFailAt=spans("EVR_TEST_MOTION_FAIL_AT");
}
bool recoverPair(PairResources& pair){
    if(!failureRecovery::wanted||!worldSession||sessionStop.load()||done.load())return false;
    selectView(pair.scene,pair.originalView);
    const auto now=GetTickCount64();
    auto& r=failureRecovery::streak;r=failureRecovery::lastFailure&&now-failureRecovery::lastFailure<=10000?r+1:1;
    failureRecovery::lastFailure=now;++failureRecovery::count;
    const uint64_t hold=r<=1?0:std::min<uint64_t>(5000,250ull<<std::min(r-2,5u));
    failureRecovery::holdUntil=now+hold;
    const bool motionAgain=motionVisual::failed&&motionVisual::validated&&failureRecovery::motionRetries<3;
    if(motionAgain){motionVisual::failed=false;++failureRecovery::motionRetries;}
    sharedRenderDeferredFrames=0;
    pair.failed=false;pair.pairPending=false;pair.frameReady=false;
    logFile->event("world_frame_recovered","\"frame\":"+std::to_string(liveFrame.frame)+",\"recoveries\":"+std::to_string(failureRecovery::count)+
        ",\"streak\":"+std::to_string(r)+",\"plate_ms\":"+std::to_string(hold)+",\"paired_frames\":"+std::to_string(pair.pairedFrames)+
        ",\"motion_visual_again\":"+(motionAgain?"true":"false")+",\"motion_visual_off\":"+(motionVisual::failed?"true":"false"));
    return true;
}
void finishPair() noexcept {
    auto* pointer=retainedPair.load(std::memory_order_acquire);
    if(!pointer)return;
    auto& pair=*pointer;
    if(!pair.pairPending&&!pair.failed)return;
    try{
        if(captureOpen){
            bool finish=true;
            if(captureInitialFogSdf){
                ++initialFogCaptureFrames;
                const auto fog=read<uintptr_t>(pair.created[0]+layout::viewFog);
                // Native View readiness may defer the upload even after the CPU
                // finishes. Include that transition and one further game frame
                // so asynchronous command recording is part of the capture.
                finish=initialFogUploadObserved||initialFogCaptureFrames>=8||pair.failed;
                if(fog&&!read<uint8_t>(fog+0x83a0)&&read<uint32_t>(fog+0x18)>0)initialFogUploadObserved=true;
            }
            if(finish){
                captureOpen=false;const auto success=captureApi->EndFrameCapture(nullptr,nullptr);
                if(!success)throw std::runtime_error("RenderDoc capture failed");
                if(captureLiveFinal)logFile->event("world_final_capture_ended","\"paired_frames\":"+std::to_string(pair.pairedFrames)+",\"serial\":"+std::to_string(pair.serial));
                if(captureGlide)logFile->event("world_glide_capture_ended","\"serial\":"+std::to_string(pair.serial));
                if(captureInitialFogSdf){
                    logFile->event("world_initial_fog_sdf_capture_ended","\"frames\":"+std::to_string(initialFogCaptureFrames)+
                        ",\"upload_cpu_transition_observed\":"+(initialFogUploadObserved?"true":"false"));
                    if(!initialFogUploadObserved)throw std::runtime_error("initial fog upload not observed within eight captured frames");
                }
            }
        }
        if(liveXr){if(pair.failed){closeLiveFrame(pair.originalView);if(recoverPair(pair))return;}else if(!pair.frameReady){skipLiveFrame(pair);pair.pairPending=false;return;}else submitLiveFrame(pair);}
        pair.pairPending=false;
        if(!pair.failed&&pair.pairedFrames<calibrationFrames&&!pair.sessionFinal)return;
        if(retainCalibration){
            selectView(pair.scene,pair.originalView);
            logFile->event(sceneCleanup?"calibration_retained_until_scene_disposal":"calibration_retained_until_process_exit","\"reason\":\"descriptor lifetime after mid-frame cleanup is unresolved\",\"views\":2,\"pairs\":"+std::to_string(pair.pairedFrames)+",\"openxr_connected\":"+(liveXr?"true":"false"));
            resultCode=pair.failed?1:0;done=true;return;
        }
        // The old engine-wide mid-frame retirement caused descriptor lifetime
        // corruption. Remove that call entirely, including from disabled modes.
        throw std::runtime_error("rendered native resources require Scene-lifetime retention");
    }catch(const std::exception& e){if(liveXr)try{closeLiveFrame(pair.originalView);}catch(...){}selectView(pair.scene,pair.originalView);logFile->event("calibration_pair_retained_on_failure","\"reason\":"+jsonString(e.what()));resultCode=1;}
    done=true;
}
void __fastcall destroyOwner(void* ownerAddress){
    const RenderCallbackScope callback;
    // Couple disposal to the engine's own Scene/owner lifetime, never to the next render call.
    uintptr_t restoreScene=0,restoreView=0;
    bool observedOwner=false,exitTrialSucceeded=false;uint32_t exitPairs=0;uint64_t exitFrame=0;
    bool leftDrawing=false,leftClean=false;
    try{
        auto* pair=(liveXr||done.load(std::memory_order_acquire))?retainedPair.load(std::memory_order_acquire):nullptr;
        if(pair&&read<uintptr_t>(reinterpret_cast<uintptr_t>(ownerAddress))==read<uintptr_t>(pair->originalView)){
            observedOwner=true;
            if(pair->renderThread!=GetCurrentThreadId())throw std::runtime_error("Scene disposal thread differs; keep bounded resources");
            const auto scene=pair->scene,previous=read<uintptr_t>(scene+layout::sceneView);
            if(previous!=pair->originalView)throw std::runtime_error("Scene disposal back-pointer differs");
            if(activeXrExitTrial){
                if(pair->failed||done.load()||pair->pairedFrames<30||pair->pairedFrames>=calibrationFrames)throw std::runtime_error("active XR exit requires healthy unfinished series with >=30 pairs");
                exitPairs=pair->pairedFrames;exitFrame=liveFrame.frame;
                logFile->event("world_active_xr_exit_begin","\"pairs\":"+std::to_string(exitPairs)+",\"frame\":"+std::to_string(exitFrame)+",\"limit\":"+std::to_string(calibrationFrames)+",\"pair_pending\":"+(pair->pairPending?"true":"false")+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"scene\":"+hex(scene));
            }
            // The series was still running: the player left the world while the eyes were drawn (no minute limit).
            const bool leftWhileDrawing=liveXr&&!activeXrExitTrial&&!done.load(std::memory_order_acquire);
            if(leftWhileDrawing)controllerUiFixture::worldLeft=true;
            leftDrawing=leftWhileDrawing;const bool pairFailed=pair->failed;
            // With the world left while drawing, the probe's thread is told only when this disposal is through (below):
            // it takes the other hooks off as soon as it sees `done`, and waits half a second for a running callback -
            // this one takes longer (game start 20261014-104758: result 4, "callback still running after hook disable").
            if(liveXr){closeLiveFrame(pair->originalView);if(!activeXrExitTrial&&!leftWhileDrawing)done=true;}
            restoreScene=scene;restoreView=previous;
            logFile->event("calibration_scene_disposal_begin","\"scene\":"+hex(scene)+",\"frame_idle_forced\":false,\"world_left_while_drawing\":"+(leftWhileDrawing?"true":"false"));
            if(worldNativeDiagnostic)worldSharedDiagnostic::snapshot("before_private_disposal",scene);
            using Release=void (__fastcall *)(void*,void*);
            for(auto& alias:pair->aliases)reinterpret_cast<Release>(baseAddress+0x1014dc0)(reinterpret_cast<void*>(pair->graph),alias.data());
            for(auto& handle:pair->targets)reinterpret_cast<Release>(baseAddress+0x1014dc0)(reinterpret_cast<void*>(pair->graph),handle.data());
            cullingHistory::release(pair->history);
            dispose(scene,pair->created[1],pair->privateSsr[1]);dispose(scene,pair->created[0],pair->privateSsr[0]);
            releasePrivateOwner(scene,pair->owners[1]);releasePrivateOwner(scene,pair->owners[0]);
            releaseGroup(scene,pair->groups[1]);releaseGroup(scene,pair->groups[0]);
            privateMemoryPools::release(scene,pair->memoryPools);selectView(scene,previous);
            if(pair->water){
                if(activeXrExitTrial)waterFeedback::defer(std::move(pair->water));
                else waterFeedback::release(*pair->water,leftWhileDrawing);
                if(pair->water&&pair->water->failed)throw std::runtime_error("water Scene cleanup failed");
            }
            motionShieldVisual::dispose(scene);
            motionArrowVisual::dispose(scene);
            motionOffhandVisual::dispose(scene);
            motionConsumableVisual::dispose(scene);
            motionModBow::dispose(scene);
            itemGallery::dispose(scene);
            onlineSync::dispose(scene);
            motionBodyIk::dispose(scene);
            motionHandVisual::dispose(scene);
            heldVfxBridge::report();
            motionVisual::dispose(scene);
            motionBowVisual::dispose(scene);
            if(worldNativeDiagnostic)worldSharedDiagnostic::snapshot("after_private_disposal",scene);
            restoreScene=0;
            retainedPair.store(nullptr,std::memory_order_release);delete pair;
            disableSharedResolutionHooks();
            logFile->event("calibration_scene_disposal_complete","\"views_destroyed\":2,\"owned_groups_cleared\":true,\"original_view_selected\":true");
            const auto status=MH_DisableHook(destroyOwnerTarget);destroyOwnerEnabled=status!=MH_OK;
            logFile->event("calibration_scene_hook_removed","\"status\":"+std::to_string(int(status)));
            exitTrialSucceeded=activeXrExitTrial&&status==MH_OK;
            leftClean=leftWhileDrawing&&!pairFailed&&status==MH_OK;
        }
    }catch(const std::exception& e){if(restoreScene)selectView(restoreScene,restoreView);try{logFile->event("calibration_scene_disposal_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
    originalDestroyOwner(ownerAddress);
    if(worldNativeDiagnostic&&observedOwner)worldSharedDiagnostic::disable();
    if(leftDrawing){
        // The session ends here: the world it drew is gone. Its result is that of this disposal.
        resultCode=leftClean?0u:1u;
        try{logFile->event("session_world_left","\"result\":"+std::to_string(resultCode.load())+",\"disposal_complete\":"+(leftClean?"true":"false")+",\"native_owner_returned\":true");}catch(...){}
        done.store(true,std::memory_order_release);
    }
    if(activeXrExitTrial&&observedOwner){
        resultCode=exitTrialSucceeded&&resultCode.load()!=1?0:1;
        logFile->event("world_active_xr_exit_complete","\"result\":"+std::to_string(resultCode.load())+",\"pairs\":"+std::to_string(exitPairs)+",\"frame\":"+std::to_string(exitFrame)+",\"native_owner_returned\":true,\"reads_destroyed_memory\":false");
        activeXrExitOwnerReturned=true;
        if(!exitTrialSucceeded||!activeXrExitReadbackRetained.load())done.store(true,std::memory_order_release);
    }
}
// EVR_LAYOUT_DUMP=1 in the game's environment: on the first probe the render objects' memory goes to layout-*.bin in
// the probe folder (raw bytes; unreadable pages are zeros), with their addresses in layout.json - to check the 1.0
// structure layout against the live game (tools/layout_dump_report.py). Read only.
namespace layoutDump {
inline bool wanted(){static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_LAYOUT_DUMP",v,sizeof v);return n==1&&v[0]=='1';}();return on;}
inline bool written=false;
inline void region(std::ostringstream& index,const char* name,uintptr_t address,size_t bytes){
    std::vector<uint8_t> data(bytes);
    for(size_t at=0;at<bytes;at+=0x100){SIZE_T got=0;const auto n=std::min<size_t>(0x100,bytes-at);
        if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address+at),data.data()+at,n,&got))std::fill_n(data.data()+at,n,uint8_t(0));}
    std::ofstream(calibrationDirectory/(std::string("layout-")+name+".bin"),std::ios::binary).write(reinterpret_cast<const char*>(data.data()),std::streamsize(bytes));
    index<<(index.tellp()>1?",":"")<<'"'<<name<<"\":{\"address\":"<<hex(address)<<",\"bytes\":"<<bytes<<'}';
}
inline uintptr_t peek(uintptr_t address){uintptr_t v=0;SIZE_T got=0;return address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&v,8,&got)?v:0;}
void write(uintptr_t view,uintptr_t context,uintptr_t parameters,uintptr_t target) noexcept {
    if(written||!wanted())return;written=true;
    try{std::ostringstream index;index<<'{';
        const auto scene=peek(view+8),owner=peek(view),renderer=peek(scene+layout::sceneRenderer),graphics=peek(renderer+layout::rendererGraphics);
        region(index,"view",view,layout::viewBytes);region(index,"owner",owner,0x100);region(index,"parameters",parameters,0x800);
        region(index,"scene",scene,0x1000);region(index,"renderer-head",renderer,0x400);region(index,"renderer-tail",renderer+0x16000,0x1000);
        region(index,"context",context,0x80);region(index,"context-frame",peek(context+8),0x80);region(index,"target",target,0x40);
        region(index,"graph",peek(target),0x280);region(index,"graphics",graphics,0x400);region(index,"backend",peek(graphics+0x278),0x2000);
        region(index,"platform",peek(peek(graphics+0x278)+layout::backendContext+0x18),0x1000);
        {uint16_t ix=0;SIZE_T got=0;ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(target+8),&ix,2,&got);
         region(index,"texture",peek(peek(peek(target)+layout::graphTextures)+size_t(ix)*8),0x100);}
        region(index,"vfx-scene",peek(scene+layout::sceneVfx),0x100);region(index,"fsr",peek(view+layout::viewUpscaler+8),0x480);
        region(index,"resolution",peek(owner+layout::ownerResolution),0x100);
        region(index,"memory-manager",peek(renderer+layout::rendererMemory),0x100);
        region(index,"instance-owner",peek(scene+0x948),0x200);   // the CPU instance owner (scene+0x900 before 1.0)
        region(index,"graph-buffers",peek(target)+0x250,0x180);   // the graph's buffer arrays (+0x100.. before 1.0)
        index<<'}';std::ofstream(calibrationDirectory/"layout.json")<<index.str();
        logFile->event("layout_dump_written","\"view\":"+hex(view)+",\"scene\":"+hex(scene)+",\"owner\":"+hex(owner)+",\"renderer\":"+hex(renderer));
    }catch(const std::exception& e){try{logFile->event("layout_dump_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
}
void probe(void *viewPointer,void* context,void* parameters,void* target) noexcept {
    const ObserverLastError preserve;
    const cpuTiming::Span hookTimed(cpuTiming::Hook);
    const auto view=reinterpret_cast<uintptr_t>(viewPointer);
    uintptr_t scene=0,previous=0;
    auto state=std::make_unique<PairResources>();
    auto& created=state->created;auto& groups=state->groups;auto& owners=state->owners;
    bool restore=false;
    try {
        if(detailFactoryProbe){runUnusedDetailFactoryProbe(view);resultCode=0;done=true;return;}
        if(preparedSessionProbe){testGameSession(view);resultCode=0;done=true;return;}
        layoutDump::write(view,reinterpret_cast<uintptr_t>(context),reinterpret_cast<uintptr_t>(parameters),reinterpret_cast<uintptr_t>(target));
        scene=read<uintptr_t>(view+8);
        if(liveXr&&!beginLiveFrame(view)){claimed=false;return;}
        previous=read<uintptr_t>(scene+layout::sceneView);
        const auto wrapper=read<uintptr_t>(view);
        if(worldResourceStage||worldNativeDiagnostic)logFile->event("world_feature_guard_observation",
            "\"view\":"+hex(view)+",\"scene\":"+hex(scene)+",\"features\":"+std::to_string(read<uint32_t>(view+0x10))+
            ",\"unknown_features\":"+std::to_string(read<uint32_t>(view+0x10)&~worldKnownFeatures)+
            ",\"wrapper_flags\":"+std::to_string(read<uint32_t>(wrapper+0x10)));
        if(worldNativeDiagnostic&&(!liveXr||!renderCalibration||(!worldSession&&calibrationFrames>(worldEndurance?diagnostic::maximumNativeDiagnosticFrames:300))||(calibrationFrames!=1&&!worldSharedStages::once)||scene!=provenWorldScene||view!=provenWorldView||
            !(read<uint32_t>(wrapper+0x10)&0x40u)))
            throw std::runtime_error("world native diagnostic requires bounded frames, shared-update policy and the proven world View");
        if(worldResourceStage&&(renderCalibration||liveXr||!(read<uint32_t>(wrapper+0x10)&0x40u)))
            throw std::runtime_error("unused world resource diagnostic requires a world detail owner and no rendering");
        if(initializeFeatures && (read<uint32_t>(view+0x10)&~0x202206u) &&
           ((!worldResourceStage&&!worldNativeDiagnostic)||(read<uint32_t>(view+0x10)&~worldKnownFeatures)))
            throw std::runtime_error("world feature ownership unresolved; allocation refused");
        if(initializeResolution && (read<uint32_t>(wrapper+0x10)&0x40u) && worldResourceStage!=2&&!worldNativeDiagnostic)
            throw std::runtime_error("world detail ownership unresolved; allocation refused");
        if(worldResourceStage)logFile->event("unused_world_resource_diagnostic","\"stage\":"+std::to_string(worldResourceStage)+
            ",\"original_features\":"+std::to_string(read<uint32_t>(view+0x10))+",\"drawn\":false");
        if(worldNativeDiagnostic)logFile->event(calibrationFrames==1?"world_native_single_pair_diagnostic":"world_native_series_diagnostic","\"proven_scene\":"+hex(scene)+
            ",\"proven_original_view\":"+hex(view)+",\"frames\":"+std::to_string(calibrationFrames)+",\"max_warmup_frames\":"+std::to_string(worldWarmupLimit)+",\"shared_updates_once\":"+(worldSharedStages::once?"true":"false")+",\"vfx_excluded\":true,\"playable_vr\":false");
        if(previous!=view || read<uintptr_t>(wrapper+layout::ownerScene)!=scene || read<uintptr_t>(wrapper+layout::ownerView)!=view)
            throw std::runtime_error("native scene/view/wrapper ownership mismatch");
        if(worldNativeDiagnostic)worldSharedDiagnostic::snapshot("before_private_creation",scene,true);
        // Do not replace the back-pointer while known old-view work is pending.
        // 1.0: the scene's world tasks are busy at this moment far more often than before (Mock runs game-20261016-075827
        // and -080708 ended here). They run on other threads: ask again for up to 0.2 s before refusing (once a session).
        {bool idle=false;
         for(unsigned attempt=0;attempt<100&&!(idle=reinterpret_cast<Wait>(baseAddress+0x1092e0)(reinterpret_cast<void *>(scene),0));++attempt)
             std::this_thread::sleep_for(std::chrono::milliseconds(2));
         if(!idle)throw std::runtime_error("existing scene tasks busy; no allocation attempted");}
        const auto before=read<std::array<uint8_t,layout::viewBytes>>(view);
        const auto ownerBefore=read<std::array<uint8_t,layout::ownerBytes>>(wrapper);
        if(privateWaterFeedback) {
            state->water=std::make_unique<WaterFeedbackState>();
            waterFeedback::create(*state->water,scene);
        }
        const auto originalResolution=read<uintptr_t>(wrapper+layout::ownerResolution);
        const auto originalDetail=originalResolution?read<uintptr_t>(originalResolution):0;
        std::array<uintptr_t,2> privateDetails{};
        const auto vfxScene=read<uintptr_t>(scene+layout::sceneVfx);
        // 1.0: the VFX scene's head (its View is embedded after it, at +0x80).
        const auto vfxBefore=read<std::array<uint8_t,layout::vfxSceneView>>(vfxScene);
        const auto oldVfx=read<uintptr_t>(view+layout::viewVfx);
        if(!oldVfx || vfxScene+layout::vfxSceneView!=oldVfx)throw std::runtime_error("unsupported VFX ownership topology");
        // The scene's clock (+0x8e0 before 1.0) is not located in 1.0; only the unused-View lifecycle test compares it.
        const auto sceneTime=read<std::array<uint8_t,24>>(scene+0x998);
        std::array<uint8_t,32> config{};
        std::memcpy(config.data(),&wrapper,8);
        std::memcpy(config.data()+8,before.data()+layout::viewConfig,20);
        restore=true;
        // 1.0 (2026-10-16): the per-eye culling history is a sharing optimization whose native owner (scene+0x900 before)
        // is not located yet; when its creation fails it is left out (both eyes keep the game's own history).
        if(cullingHistory::requested)try{cullingHistory::create(state->history,scene);}
            catch(const std::exception& e){try{cullingHistory::release(state->history);}catch(...){}
                state->history={};cullingHistory::requested=false;
                logFile->event("optional_hook_skipped","\"hook\":\"culling_history\",\"reason\":"+jsonString(e.what()));}
        for(size_t eye=0;eye<2;++eye) {
            if(initializeFeatures)privateMemoryPools::create(scene,wrapper,unsigned(eye),initializeResolution,state->memoryPools);
            uintptr_t selectedOwner=wrapper;
            if(initializeResolution) {
                owners[eye]=ownerBefore;
                put<uint32_t>(owners[eye],layout::ownerPools+8,state->memoryPools[eye][0]);
                put<uint32_t>(owners[eye],layout::ownerPools+12,state->memoryPools[eye][1]);
                // Borrow creation-time scene data; own only feature/resolution data.
                put<uint32_t>(owners[eye],0x10,eyeFeatures(read<uint32_t>(wrapper+0x10)));
                put<uintptr_t>(owners[eye],layout::ownerMarker,0);put<uint32_t>(owners[eye],layout::ownerFeatureGroup,0);
                put<uintptr_t>(owners[eye],layout::ownerResolution,0);put<uint32_t>(owners[eye],layout::ownerResolutionGroup,0);
                if(runtimeEyes()){put<uint32_t>(owners[eye],layout::ownerWidth,eyeWidth(eye));put<uint32_t>(owners[eye],layout::ownerHeight,eyeHeight(eye));
                    put<float>(owners[eye],layout::ownerRenderScale,eyeRenderScale);}
                selectedOwner=reinterpret_cast<uintptr_t>(owners[eye].data());
                put<uintptr_t>(config,0,selectedOwner);
                put<uintptr_t>(owners[eye],layout::ownerMarker,allocateFeatureMarker(scene));
            }
            const auto pointer=reinterpret_cast<Create>(baseAddress+0x1035e0)(reinterpret_cast<void *>(scene),config.data());
            created[eye]=reinterpret_cast<uintptr_t>(pointer);
            if(!created[eye] || created[eye]==view || (eye && created[0]==created[1]) ||
               read<uintptr_t>(scene+layout::sceneView)!=created[eye] || read<uintptr_t>(created[eye]+8)!=scene ||
               read<uintptr_t>(created[eye])!=selectedOwner || read<uintptr_t>(created[eye]+layout::viewSelf)!=created[eye]+layout::viewSelfTarget)
                throw std::runtime_error("created view ownership/self-pointer mismatch");
            logFile->event("native_view_created", "\"slot\":"+std::to_string(eye)+",\"view\":"+hex(created[eye])+
                ",\"original_view\":"+hex(view)+",\"scene\":"+hex(scene)+",\"rendered\":false,\"features\":"+
                std::to_string(read<uint32_t>(created[eye]+0x10)));
            // Bit 2 creates a singleton VFX view inside the shared VFX scene.
            // Never duplicate it. This probe does NOT draw or disable game VFX.
            // Opt-in eye de-aliasing: images the game binds while this eye's feature/resolution
            // resources are built get dedicated memory in the layer (no shared transient offsets).
            struct DealiasWindow{
                using Set=int(*)(uint32_t);Set set=nullptr;
                DealiasWindow(){if(!eyeDealias)return;const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");
                    set=layer?reinterpret_cast<Set>(GetProcAddress(layer,"evrSetImageDealiasWindow")):nullptr;
                    if(!set||set(1))throw std::runtime_error("eye de-alias window unavailable");}
                ~DealiasWindow(){if(set)set(0);}
            } dealiasWindow;
            if(initializeFeatures)setupFeatures(scene,selectedOwner,created[eye],eyeFeatures(read<uint32_t>(view+0x10)),state->memoryPools[eye][0],groups[eye],state->privateSsr[eye],state->privateSsr);
            if(initializeResolution) {
                put<uintptr_t>(owners[eye],layout::ownerView,created[eye]);put<uint32_t>(owners[eye],layout::ownerFeatureGroup,groups[eye]);
                try { privateMemoryPools::TransientScope transientScope(scene,state->memoryPools[eye][2],unsigned(eye));
                      setupResolution(owners[eye],state->privateSsr[eye]); }
                catch(...) {groups[eye]=read<uint32_t>(selectedOwner+layout::ownerFeatureGroup);throw;}
                // Native configure owns replacement semantics for this group.
                groups[eye]=read<uint32_t>(selectedOwner+layout::ownerFeatureGroup);
                // c02a0 allocates detail view (47730, 0x210 bytes) into the
                // PRIVATE resolution owner. c1b40 releases it through 49370.
                // Creation-time scene data remains borrowed, not destroyed.
                const auto resolution=read<uintptr_t>(selectedOwner+layout::ownerResolution);
                if(!resolution||resolution==originalResolution)
                    throw std::runtime_error("resolution owner aliases original");
                if(read<uint32_t>(selectedOwner+0x10)&0x40u){
                    privateDetails[eye]=read<uintptr_t>(resolution);
                    logFile->event("detail_scattering_owner_check", "\"eye\":"+std::to_string(eye)+
                        ",\"resolution\":"+hex(resolution)+",\"owner\":"+hex(privateDetails[eye])+
                        ",\"original\":"+hex(originalDetail)+",\"first_eye\":"+hex(privateDetails[0]));
                    if(!privateDetails[eye]||privateDetails[eye]==originalDetail||
                       (eye&&privateDetails[eye]==privateDetails[0]))
                        throw std::runtime_error("detail-scattering owner missing/aliased");
                    logFile->event("private_detail_scattering", "\"eye\":"+std::to_string(eye)+
                        ",\"owner\":"+hex(privateDetails[eye])+",\"original\":"+hex(originalDetail));
                }
                if(originalResolution&&read<uintptr_t>(originalResolution)!=originalDetail)
                    throw std::runtime_error("original detail-scattering owner changed");
                if(read<uint32_t>(view+0x10)&0x8000u){
                    const auto fsr=read<uintptr_t>(created[eye]+layout::viewUpscaler+8),oldFsr=read<uintptr_t>(view+layout::viewUpscaler+8);
                    const auto firstFsr=eye?read<uintptr_t>(created[0]+layout::viewUpscaler+8):0;
                    if(!fsr||!oldFsr||fsr==oldFsr||fsr==firstFsr||read<uint8_t>(created[eye]+layout::viewUpscaler)!=1)
                        throw std::runtime_error("private FSR3 context missing/aliased");
                    // +20 is a reference-counted shader program cache ID:
                    // 9bdef0 increments record+c; 9bf260 decrements it. Sharing
                    // that ID is expected. Temporal texture handles must differ. 1.0: the handles fill +28..+3d8 (59; the
                    // FidelityFX context moved from +28 to +3d8), before 1.0 +28..+3a8 (56).
                    const auto programId=read<uint32_t>(fsr+0x20),oldId=read<uint32_t>(oldFsr+0x20);
                    if(!programId)throw std::runtime_error("private FSR3 shader program missing");
                    unsigned imageHandles=0;
                    for(unsigned n=0;n<59;n++){
                        const auto graph=read<uintptr_t>(fsr+0x28+n*16);if(!graph)continue;
                        ++imageHandles;const auto index=read<uint16_t>(fsr+0x30+n*16);
                        for(const auto other:{oldFsr,firstFsr})if(other)for(unsigned m=0;m<59;m++)
                            if(read<uintptr_t>(other+0x28+m*16)==graph&&read<uint16_t>(other+0x30+m*16)==index)
                                throw std::runtime_error("private FSR3 temporal texture aliases another View");
                    }
                    if(!imageHandles)throw std::runtime_error("private FSR3 temporal textures missing");
                    logFile->event("private_fsr3_context","\"eye\":"+std::to_string(eye)+",\"object\":"+hex(fsr)+
                        ",\"original\":"+hex(oldFsr)+",\"program_id\":"+std::to_string(programId)+",\"original_program_id\":"+std::to_string(oldId)+
                        ",\"private_image_handles\":"+std::to_string(imageHandles));
                }
            }
        }
        if(renderCalibration){
            state->scene=scene;state->originalView=previous;
            recordPair(*state,context,parameters,target);
            logFile->event("calibration_pair_waiting_submission","\"serial\":"+std::to_string(state->serial));
            retainedPair.store(state.release(),std::memory_order_release);restore=false;return;
        }
        cullingHistory::release(state->history);
        dispose(scene,created[1],state->privateSsr[1]); dispose(scene,created[0],state->privateSsr[0]);
        if(initializeResolution) {releasePrivateOwner(scene,owners[1]);releasePrivateOwner(scene,owners[0]);}
        releaseGroup(scene,groups[1]);releaseGroup(scene,groups[0]);
        privateMemoryPools::release(scene,state->memoryPools);
        if(state->water)waterFeedback::release(*state->water);
        selectView(scene,previous); restore=false;
        if(read<std::array<uint8_t,layout::viewBytes>>(view)!=before || read<std::array<uint8_t,24>>(scene+0x998)!=sceneTime ||
           read<std::array<uint8_t,layout::vfxSceneView>>(vfxScene)!=vfxBefore || read<uintptr_t>(view+layout::viewVfx)!=oldVfx ||
           read<std::array<uint8_t,layout::ownerBytes>>(wrapper)!=ownerBefore || groups[0] || groups[1] ||
           read<uintptr_t>(scene+layout::sceneView)!=previous || read<uintptr_t>(wrapper+layout::ownerView)!=view)
            throw std::runtime_error("original view/scene time changed by unused view lifecycle");
        logFile->event("native_view_probe_complete", "\"two_views_destroyed\":true,\"original_view_unchanged\":true,\"scene_time_unchanged\":true,\"back_pointer_restored\":true");
        resultCode=0;
    } catch(const std::exception &error) {
        if(liveXr&&!state->recorded)try{closeLiveFrame(view);}catch(...){}
        if(state->recorded){selectView(scene,previous);state->failed=true;retainedPair.store(state.release(),std::memory_order_release);restore=false;}
        else if(state->graph){
            using Release=void (__fastcall *)(void*,void*);
            for(auto& alias:state->aliases)if(read<uintptr_t>(reinterpret_cast<uintptr_t>(alias.data()))==state->graph)reinterpret_cast<Release>(baseAddress+0x1014dc0)(reinterpret_cast<void*>(state->graph),alias.data());
            for(auto& handle:state->targets)if(read<uintptr_t>(reinterpret_cast<uintptr_t>(handle.data()))==state->graph)reinterpret_cast<Release>(baseAddress+0x1014dc0)(reinterpret_cast<void*>(state->graph),handle.data());
        }
        if(restore) { cullingHistory::release(state->history);dispose(scene,created[1],state->privateSsr[1]);dispose(scene,created[0],state->privateSsr[0]);
            if(initializeResolution) {releasePrivateOwner(scene,owners[1]);releasePrivateOwner(scene,owners[0]);}
            releaseGroup(scene,groups[1]);releaseGroup(scene,groups[0]);
            privateMemoryPools::release(scene,state->memoryPools);selectView(scene,previous); }
        if(state&&state->water&&!state->recorded)waterFeedback::release(*state->water);
        try { logFile->event("native_view_probe_failure","\"reason\":"+jsonString(error.what())); } catch(...) {}
        resultCode=1;
    }
    if(retainedPair.load(std::memory_order_acquire))return;
    if(captureOpen){captureOpen=false;captureApi->DiscardFrameCapture(nullptr,nullptr);}
    done.store(true,std::memory_order_release);
}
#include <tlhelp32.h>
#include "thread_stack_sampler.hpp"
// Diagnostic (2026-10-19, EVR_THREAD_CPU=1): every 5 s, how much of a core each of the game's threads used since the last
// time, the busiest 16 with their names. The game's own log says "[server] Bad Performance 22 ms (peak 197 ms)" every 5 s
// while the headset plays online - in the multiplayer session of 2026-10-19 00:00 the joined friend's hits and guards came
// late; it shows whether the server's thread is busy itself or waits for the others.
namespace threadCpu {
inline const bool wanted=[]{char v[8]{};return GetEnvironmentVariableA("EVR_THREAD_CPU",v,sizeof v)==1&&v[0]=='1';}();
inline std::unordered_map<DWORD,uint64_t> last;inline uint64_t lastMs=0;
using GetDescription=HRESULT(WINAPI*)(HANDLE,PWSTR*);
inline std::string utf8(const wchar_t* w){if(!w||!*w)return {};const int n=WideCharToMultiByte(CP_UTF8,0,w,-1,nullptr,0,nullptr,nullptr);if(n<=1)return {};
    std::string s(size_t(n-1),'\0');WideCharToMultiByte(CP_UTF8,0,w,-1,s.data(),n,nullptr,nullptr);return s;}
inline void sample(){
    if(!wanted)return;const auto now=GetTickCount64();if(lastMs&&now-lastMs<5000)return;
    const auto span=lastMs?now-lastMs:0;lastMs=now;
    static const auto getDescription=reinterpret_cast<GetDescription>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetThreadDescription"));
    const HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return;
    THREADENTRY32 entry{};entry.dwSize=sizeof entry;const DWORD pid=GetCurrentProcessId();
    struct Row{double pct;DWORD id;std::string name;};std::vector<Row> rows;std::unordered_map<DWORD,uint64_t> next;double total=0;
    for(BOOL ok=Thread32First(snap,&entry);ok;ok=Thread32Next(snap,&entry)){if(entry.th32OwnerProcessID!=pid)continue;
        const HANDLE h=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ThreadID);if(!h)continue;
        FILETIME c{},e{},k{},u{};
        if(GetThreadTimes(h,&c,&e,&k,&u)){const uint64_t cpu=((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime);
            next[entry.th32ThreadID]=cpu;const auto it=last.find(entry.th32ThreadID);
            if(span&&it!=last.end()&&cpu>=it->second){const double pct=double(cpu-it->second)/1e4/double(span)*100.0;total+=pct;
                std::string name;if(getDescription){PWSTR d=nullptr;if(SUCCEEDED(getDescription(h,&d))&&d){name=utf8(d);LocalFree(d);}}
                rows.push_back({pct,entry.th32ThreadID,name});}}
        CloseHandle(h);}
    CloseHandle(snap);last.swap(next);
    if(rows.empty())return;
    std::sort(rows.begin(),rows.end(),[](const Row& a,const Row& b){return a.pct>b.pct;});
    std::string top;char b[32];
    for(size_t i=0;i<rows.size()&&i<16;i++){std::snprintf(b,sizeof b,"%.1f",rows[i].pct);
        top+=std::string(i?",":"")+"{\"tid\":"+std::to_string(rows[i].id)+",\"name\":"+jsonString(rows[i].name)+",\"core_pct\":"+b+"}";}
    std::snprintf(b,sizeof b,"%.1f",total);
    try{logFile->event("thread_cpu","\"span_ms\":"+std::to_string(span)+",\"threads\":"+std::to_string(rows.size())+",\"total_core_pct\":"+b+
        ",\"render_thread\":"+std::to_string(GetCurrentThreadId())+",\"top\":["+top+"]");}catch(...){}
}
}
void * __fastcall render(void *out,void *context,void *view,void *parameters,void *target) {
    const RenderCallbackScope callback;
    threadCpu::sample();threadStacks::start();
    auto* pair=retainedPair.load(std::memory_order_acquire);
    rootCamera::FrameSnapshotScope rootFrame(rootCamera::mode&&active.load()&&!done.load()&&(!pair||reinterpret_cast<uintptr_t>(view)==pair->originalView),context,pair?pair->pairedFrames:0);
    headVisibility::FrameScope headFrame(headVisibility::requested&&active.load()&&!done.load()&&(!pair||reinterpret_cast<uintptr_t>(view)==pair->originalView));
    if(active.load(std::memory_order_acquire)&&renderCalibration){
        if(pair&&!done.load()&&reinterpret_cast<uintptr_t>(view)==pair->originalView &&
           read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18)!=pair->serial)finishPair();
        if(captureLiveFinal&&(!capturedLiveFinal||(captureOnRequest&&requestedCaptures<maxRequestedCaptures))&&pair&&!done.load()&&!captureOpen){
            bool trigger=!captureOnRequest&&pair->pairedFrames==configuredLiveFrames-1;
            // Diagnostic (2026-10-20, the right eye's light square on a puddle, gone without the desktop refreshes):
            // EVR_CAPTURE_ON_REFRESH=1 waits from that pair for the next frame that refreshes the original View (the same
            // count as the desktop skip below; a fall-held refresh is not foreseen).
            // =2: the frame after that refresh.
            {static const char onRefresh=[]{char v[4]{};return GetEnvironmentVariableA("EVR_CAPTURE_ON_REFRESH",v,sizeof v)==1&&(v[0]=='1'||v[0]=='2')?v[0]:'0';}();
                if(onRefresh!='0'&&!captureOnRequest)trigger=pair->pairedFrames+1>=configuredLiveFrames&&desktopSkipEvery&&desktopSkipCandidates&&
                    !((desktopSkipCandidates+(onRefresh=='1'?1:0))%desktopSkipEvery);}
            if(captureOnRequest&&reinterpret_cast<uintptr_t>(view)==pair->originalView&&pair->pairedFrames>30&&pair->pairedFrames%15==0){
                std::error_code ec;const auto request=calibrationDirectory/"world-capture.request";
                trigger=std::filesystem::exists(request,ec)&&std::filesystem::remove(request,ec);
            }
            if(trigger){
                // The three Views as they are at the captured frame (2026-10-18 20:50: where the terrain's per-View detail
                // radius lives - the capture's terrain constants at 0x114 are matched against these). 256 KB each.
                uintptr_t terrainObject=0;try{terrainObject=read<uintptr_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(view)+8)+0xa80);}catch(...){}
                for(int k=0;k<4;k++){const uintptr_t v=k<2?pair->created[k]:k==2?pair->originalView:terrainObject;if(!v)continue;
                    std::vector<uint8_t> bytes(0x40000);SIZE_T got=0;ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(v),bytes.data(),bytes.size(),&got);
                    std::ofstream dump(calibrationDirectory/("capture-view-"+(requestedCaptures?std::to_string(requestedCaptures)+"-":std::string())+std::to_string(k)+".bin"),std::ios::binary);dump.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(got));
                    logFile->event("capture_view_dump","\"view\":"+std::to_string(k)+",\"address\":"+hex(v)+",\"bytes\":"+std::to_string(got));}
                if(captureOnRequest&&requestedCaptures)captureApi->SetCaptureFilePathTemplate((calibrationDirectory/("world_final_"+std::to_string(requestedCaptures+1))).string().c_str());
                if(captureOnRequest)++requestedCaptures;
                captureApi->StartFrameCapture(nullptr,nullptr);captureOpen=true;capturedLiveFinal=true;
                logFile->event("world_final_capture_started","\"paired_frames\":"+std::to_string(pair->pairedFrames)+",\"manual_request\":"+(captureOnRequest?"true":"false")+",\"request\":"+std::to_string(requestedCaptures));
            }
        }
        if(captureGlide&&!capturedGlide&&!captureOpen&&pair&&!done.load()&&pair->pairedFrames>=30&&
           reinterpret_cast<uintptr_t>(view)==pair->originalView&&rootFrame.sample.number&&rootFrame.sample.cameraId==25){
            captureApi->StartFrameCapture(nullptr,nullptr);captureOpen=true;capturedGlide=true;
            logFile->event("world_glide_capture_started","\"paired_frames\":"+std::to_string(pair->pairedFrames)+",\"serial\":"+std::to_string(rootFrame.sample.renderSerial)+",\"root_sample\":"+std::to_string(rootFrame.sample.number));
        }
        if(captureInitialFogSdf&&!capturedInitialFogSdf&&pair&&!done.load()&&!captureOpen&&
           reinterpret_cast<uintptr_t>(view)==pair->originalView){
            const auto fog=read<uintptr_t>(pair->created[0]+layout::viewFog);
            // Capture the native upload after CPU collection. Do not alter task
            // flags, texture contents, or the engine's upload ordering.
            if(fog&&read<uint8_t>(fog+0x83a0)&&read<uint8_t>(fog+0xb91)&&read<uint16_t>(fog+0x8398)){
                captureApi->StartFrameCapture(nullptr,nullptr);captureOpen=true;capturedInitialFogSdf=true;
                logFile->event("world_initial_fog_sdf_capture_started","\"eye\":0,\"fog\":"+hex(fog)+
                    ",\"fog_frame\":"+std::to_string(read<uint32_t>(fog+0x18))+
                    ",\"pending_regions\":"+std::to_string(read<uint16_t>(fog+0x8398)));
            }
        }
        if(!liveXr&&!claimed.load()&&!captureOpen){captureApi->StartFrameCapture(nullptr,nullptr);captureOpen=true;}
        else if(!liveXr&&pair&&!done.load()&&calibrationFrames>1&&(pair->pairedFrames==calibrationFrames-1||pair->pairedFrames==calibrationFrames/4)&&!captureOpen){
            captureApi->SetCaptureFilePathTemplate((calibrationDirectory/(pair->pairedFrames==calibrationFrames-1?"native_series_final":"native_series_quarter")).string().c_str());
            captureApi->StartFrameCapture(nullptr,nullptr);captureOpen=true;
        }
    }
    // Establish the next runtime frame before shared world/VFX updates, once
    // the initial private View lifecycle has succeeded. This is opt-in while
    // validating native queue ordering; the first warmup retains bootstrap order.
    bool earlyPrepared=false;
    if(earlyWorldXr&&active.load()&&pair&&!pair->failed&&!done.load()&&!failureRecovery::held()&&
       reinterpret_cast<uintptr_t>(view)==pair->originalView&&
       read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18)!=pair->serial&&
       read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18)!=pair->xrAttemptSerial){
        try{
            pair->xrAttemptSerial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
            earlyPrepared=beginLiveFrame(pair->originalView,alternateEyesWanted(*pair));
            if(earlyPrepared)alternateEyesLog(pair->xrAttemptSerial);
            logFile->event(earlyPrepared?"world_early_xr_frame":"world_tracking_frame_deferred","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+
                std::to_string(pair->xrAttemptSerial));
        }catch(const std::exception& e){pair->failed=true;
            logFile->event("calibration_series_failure","\"reason\":"+jsonString(e.what()));}
    }
    if(desktopCompare&&active.load()&&pair&&!pair->failed&&!done.load()&&
       reinterpret_cast<uintptr_t>(view)==pair->originalView&&pair->pairedFrames%15==0){
        std::ifstream request(calibrationDirectory/"desktop-mode.txt");
        std::string magic,mode;unsigned sequence=0;request>>magic>>sequence>>mode>>std::ws;
        if(request&&request.eof()&&magic=="EVR_DESKTOP_MODE_1"&&sequence>desktopCompareSequence&&sequence<=6&&
           (mode=="render"||mode=="skip")){
            desktopCompareSequence=sequence;desktopCompareRender=mode=="render";
            logFile->event("desktop_compare_mode","\"sequence\":"+std::to_string(sequence)+
                ",\"paired_frames\":"+std::to_string(pair->pairedFrames)+",\"render\":"+(desktopCompareRender?"true":"false"));
        }
    }
    // Desktop skip: begin the runtime frame first; only a frame whose eye pair will render skips the original View.
    bool skipDesktop=false,refreshDesktop=false;
    // While the game loads (a fast travel, a respawn: cameras 1..3, PlayerCameraSpawning .. Respawning) the original
    // View is rendered by the game on every frame, as without the mod. Headset run game145015: after a fast travel
    // the loading screen stayed for 58 s, and was gone 0.4 s after the session's end took this hook away. Until then
    // the original View had been rendered on one frame in thirty only (the refresh of the skip). The same while the
    // player's root sample is missing or stale, which is how a loading world looks from here.
    bool loadingWorld=false;
    if(desktopSkip&&active.load()&&pair&&!pair->failed&&!done.load()&&reinterpret_cast<uintptr_t>(view)==pair->originalView){
        try{rootCamera::Sample root;loadingWorld=!rootCamera::trySnapshot(root)||root.cameraId<=3;}catch(const std::exception&){loadingWorld=true;}
        // The loading camera alone is not the end of it. After a respawn the game holds the player still until the
        // client reports itself ready (keen::ecs::PlayerInput.fromClientIsReadyForGameplay; the actor's state flag
        // IsReadyForGameplay, 50, follows it at 0x228ad0, and PlayerFlags UiCoverRequest, bit 2 of PlayerState+0x34,
        // stays up until then at 0x28d7b0) - and the client gets ready by the original View being rendered. A first
        // respawn loads for seconds on the respawn camera and is ready by its end; a later one is back on the
        // gameplay camera after one or two seconds, before the base's buildings are in, and with the skip resumed it
        // never got ready: headset runs game-20261013-105615 ("after the second respawn I could not move") and
        // game-20261013-141010 (movement_stalled at 8:00 and 25:32, both right after such a respawn, the stick in
        // the sent input, the flags 133 = with UiCoverRequest, the actor's state without bit 50; a fast travel - a
        // long load - ended it). So the original View is also rendered on every frame while the player is not ready.
        bool notReady=false;
        if(!loadingWorld){std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);const auto& s=inputHeadIdle::latest;const auto now=GetTickCount64();
            notReady=s.milliseconds&&now>=s.milliseconds&&now-s.milliseconds<=1000&&((s.flags&4u)!=0||(s.actorKnown&&!(s.actorState&(1ull<<50))));
            loadingWorld=notReady;}
        // EVR_DESKTOP_SKIP_LOADING=1 in the game's environment keeps the skip while the game loads, as it was before
        // 2026-10-12 16:45 (for a comparison in Mock; no launch flag sets it).
        static const bool skipWhileLoading=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_DESKTOP_SKIP_LOADING",value,sizeof value);return n&&n<sizeof value&&value[0]=='1';}();
        static bool loadingBefore=false;static unsigned loadingEvents=0;
        if(loadingWorld!=loadingBefore){loadingBefore=loadingWorld;
            if(loadingEvents++<64)logFile->event("desktop_skip_loading","\"loading\":"+std::string(loadingWorld?"true":"false")+",\"frame\":"+std::to_string(liveFrame.frame)+",\"paired_frames\":"+std::to_string(pair->pairedFrames)+",\"player_not_ready_for_gameplay\":"+(notReady?"true":"false")+",\"original_view_rendered_every_frame\":"+(loadingWorld&&!skipWhileLoading?"true":"false")+",\"skip_kept_for_comparison\":"+(skipWhileLoading?"true":"false"));}
        if(skipWhileLoading)loadingWorld=false;
    }
    if(desktopSkip&&!loadingWorld&&!desktopCompareRender&&!sharedRenderDeferredFrames&&active.load()&&pair&&!pair->failed&&!done.load()&&!failureRecovery::held()&&reinterpret_cast<uintptr_t>(view)==pair->originalView&&
       pair->pairedFrames>=desktopSkipWarmupPairs){
        const auto serial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
        const bool begunEarly=earlyPrepared&&serial==pair->xrAttemptSerial;
        if(serial!=pair->serial&&(begunEarly||serial!=pair->xrAttemptSerial)){
            ++desktopSkipCandidates;
            // In the air the refresh waits (2026-10-20; user 10-19: "falling from a height, buildings still vanish now and
            // then"): after the 0.9.1 lending, the one picture left with holes was the right eye's on the frame after a
            // refresh, and only while falling (10-19 06:50, Mock runs -060816 759 and -061432 730). While the local actor is
            // not grounded (its Grounded bit, as the air strokes read it) a due refresh is owed and done on the first frame
            // on the ground - at most 300 frames late, so that a long glide still refreshes the GI. EVR_FALL_REFRESH_HOLD=0: off.
            static const bool fallRefreshHold=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FALL_REFRESH_HOLD",v,sizeof v);return !(n==1&&v[0]=='0');}();
            static bool refreshOwed=false;static unsigned refreshPostponed=0,postponeLogs=0;
            bool refreshNow=desktopSkipEvery&&(!(desktopSkipCandidates%desktopSkipEvery)||refreshOwed);
            if(refreshNow&&fallRefreshHold){
                bool airborne=false;{std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);const auto& s=inputHeadIdle::latest;const auto now=GetTickCount64();
                    airborne=s.actorKnown&&s.milliseconds&&now>=s.milliseconds&&now-s.milliseconds<=500&&!(s.actorState&1ull);}
                if(airborne&&refreshPostponed<300){
                    if(!refreshOwed&&postponeLogs++<100)logFile->event("desktop_refresh_postponed","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(serial)+",\"airborne\":true");
                    refreshOwed=true;++refreshPostponed;refreshNow=false;}
            }
            if(refreshNow){if(refreshOwed&&postponeLogs++<100)logFile->event("desktop_refresh_done_late","\"frame\":"+std::to_string(liveFrame.frame)+",\"frames_late\":"+std::to_string(refreshPostponed));refreshOwed=false;refreshPostponed=0;}
            if(!refreshNow){
                try{
                    if(!begunEarly){pair->xrAttemptSerial=serial;earlyPrepared=beginLiveFrame(pair->originalView,alternateEyesWanted(*pair));if(earlyPrepared)alternateEyesLog(serial);}
                    skipDesktop=earlyPrepared;
                    if(!earlyPrepared)logFile->event("world_tracking_frame_deferred","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(serial)+",\"desktop_rendered\":true");
                }catch(const std::exception& e){pair->failed=true;
                    logFile->event("calibration_series_failure","\"reason\":"+jsonString(e.what()));}
            }else refreshDesktop=true;
        }
    }
    // The eye that is drawn first owns the shared updates: eye 0, or with one eye a frame the one that is drawn.
    // On a desktop-refresh frame the same eye owns the effects (worldSharedStages::vfxOwnerEye), when the eyes of this
    // frame are begun already. EVR_REFRESH_VFX_EYE=0: the original View keeps them on that frame (before 2026-10-15).
    static const bool refreshVfxEye=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REFRESH_VFX_EYE",v,sizeof v);return !(n==1&&v[0]=='0');}();
    const bool vfxToEye=refreshDesktop&&refreshVfxEye&&desktopSkipVfx&&earlyPrepared&&pair&&!pair->failed&&reinterpret_cast<uintptr_t>(view)==pair->originalView;
    // The shared world updates on a refresh frame as well (2026-10-17, headset report 10, user: "the far terrain's LOD changes
    // shape about twice a second - rocks bulge and sink"): the original View ran them with its own camera - the terrain
    // bricks around the desktop camera (0x5a8f0) - one frame in thirty, and the next frames the eye's camera chose again.
    // The user watched it stop in a Mock without refresh frames (-DesktopSkip 0, game-20261017-1051xx at the report's place).
    // Now the eye that owns the effects owns these too; the original View is still drawn (for GI) with the eye's bricks.
    // EVR_REFRESH_SHARED_EYE=0: the original View keeps them on that frame (before 2026-10-17).
    static const bool refreshSharedEye=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REFRESH_SHARED_EYE",v,sizeof v);return !(n==1&&v[0]=='0');}();
    const bool sharedToEye=vfxToEye&&refreshSharedEye;
    struct SharedOwnerScope {explicit SharedOwnerScope(bool eyes,bool vfx){const int e=liveFrame.eyeMask==2?1:0;if(eyes)worldSharedStages::ownerEye=e;if(eyes||vfx)worldSharedStages::vfxOwnerEye=e;}
        ~SharedOwnerScope(){worldSharedStages::ownerEye=-1;worldSharedStages::vfxOwnerEye=-1;}} sharedOwner(skipDesktop||sharedToEye,vfxToEye);
    {static std::atomic<unsigned> told{0};if(sharedToEye&&told.fetch_add(1)<3)logFile->event("desktop_refresh_shared_to_eye","\"frame\":"+std::to_string(liveFrame.frame)+",\"eye\":"+std::to_string(worldSharedStages::ownerEye));}
    worldSharedStages::Context sharedStage;
    terrainReadyDiagnostic::Context terrainStage;
    downloadFeedbackDiagnostic::Context downloadStage;
    if(worldNativeDiagnostic){sharedStage.shared=read<uintptr_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(view)+8)+0xa88);
        sharedStage.terrain=read<uintptr_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(view)+8)+0xa80);
        sharedStage.serial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);}
    game::SharedCullingCamera sharedCamera{};game::EyeCamera sharedGameCamera{};
    bool sharedCameraApplied=false,originalDeferred=false;unsigned sharedProjectionCalls=0;
    stableFrustumBridge::Context sharedFrustum;
    sharedFrustum.serial=sharedStage.serial;sharedFrustum.frame=liveFrame.frame;
    void* originalRenderParameters=parameters;
    std::array<uint8_t,layout::parameterBytes> preservedParameters{};
    if(sharedCullingCamera&&earlyPrepared&&!skipDesktop&&pair&&!pair->failed){
        try{
            const auto input=reinterpret_cast<uintptr_t>(parameters);
            preservedParameters=read<std::array<uint8_t,layout::parameterBytes>>(input);
            if(read<uint8_t>(input+0x7c)!=0)throw std::runtime_error("shared culling requires perspective camera");
            const auto sample=rootCamera::snapshot();
            if(sample.renderSerial!=sharedStage.serial||!sample.renderEntryMilliseconds)
                throw std::runtime_error("shared culling root snapshot serial mismatch");
            const auto anchor=sceneryAnchor(rootCamera::renderAnchor(sample,read<WorldPositionQ32>(input+0x38),read<Quat>(input+0x50)));
            // One tracking sample whose headset pose and eye poses did not belong together ended a headset session after
            // six minutes (game174405, "stereo frusta exceed one forward culling camera"). This helper camera only serves
            // shared simulation and culling: such a frame keeps the last good one instead of failing the series.
            static game::SharedCullingCamera heldSharedCamera{};static bool heldSharedCameraValid=false;static unsigned heldSharedCameraFrames=0;
            try{sharedCamera=game::sharedCullingCamera(liveFrame.head,liveFrame.views,read<float>(input+0x70),read<float>(input+0x74));heldSharedCamera=sharedCamera;heldSharedCameraValid=true;}
            catch(const std::invalid_argument& e){
                if(!heldSharedCameraValid)throw;
                sharedCamera=heldSharedCamera;
                if(heldSharedCameraFrames++<20)logFile->event("shared_culling_camera_held","\"frame\":"+std::to_string(liveFrame.frame)+",\"head_flags\":"+std::to_string(liveFrame.headFlags)+",\"reason\":"+jsonString(e.what()));
            }
            const std::array<XrView,2> helpers{sharedCamera.view,sharedCamera.view};
            sharedGameCamera=game::stereoCameras(anchor,liveTrackingToAnchor,helpers,3,liveFrame.time,liveFrame.frame,
                anchor.root,sharedCamera.nearZ,sharedCamera.farZ).eyes[0];
            pair->sharedParameters=preservedParameters;
            put<WorldPositionQ32>(pair->sharedParameters,0x38,sharedGameCamera.position);
            put<Quat>(pair->sharedParameters,0x50,sharedGameCamera.orientation);
            put<float>(pair->sharedParameters,0x70,sharedCamera.nearZ);
            put<float>(pair->sharedParameters,0x74,sharedCamera.farZ);
            put<float>(pair->sharedParameters,0x78,2*std::max(std::abs(sharedCamera.view.fov.angleUp),std::abs(sharedCamera.view.fov.angleDown)));
            originalRenderParameters=pair->sharedParameters.data();sharedCameraApplied=true;
        }catch(const std::exception& e){pair->failed=true;
            logFile->event("calibration_series_failure","\"reason\":"+jsonString(e.what()));}
    }
    heldVfxBridge::Scope heldEffectScope;
    if(game::padStyle.load()&&!motionVisual::requested&&earlyPrepared&&pair&&!pair->failed&&!done.load()){padOnline::frame(pair->scene,pair->originalView,sharedStage.serial);
        keyScript::frame(liveFrame.frame,keyScript::installed?menuPointer::gameWindow():nullptr);   // the Mock's keys (as in the motion branch below)
        padSettings::frame(menuPointer::screen.load());
        targetMark::frame(liveFrame.frame,GetTickCount64());   // the locked enemy's bar above it (target_mark.hpp)
        // the VR bug report (its button: the pad panel's left stick click), after the panel (it takes the overlay elsewhere)
        {const motion::GestureHead head{liveFrame.head,liveFrame.headFlags,liveFrame.frame,liveFrame.sessionGeneration,liveFrame.hands.originGeneration,liveFrame.time};
         bugReport::frame(liveFrame.hands,head,GetTickCount64());}}
    if(motionVisual::requested&&earlyPrepared&&pair&&!pair->failed&&!done.load()){
        try{const auto sample=rootCamera::snapshot();
            const auto input=reinterpret_cast<uintptr_t>(parameters);
            const auto anchor=sceneryAnchor(rootCamera::renderAnchor(sample,read<WorldPositionQ32>(input+0x38),read<Quat>(input+0x50)));
            if(motionVisual::validated&&!motionVisual::failed&&failureRecovery::due(failureRecovery::motionFailAt,++failureRecovery::motionAttempts))throw std::runtime_error("test failure (EVR_TEST_MOTION_FAIL_AT)");   // as a failure inside the models' preparation
            motionVisual::prepare(pair->scene,pair->originalView,anchor,sharedStage.serial);
            motionHandVisual::prepare(anchor,sharedStage.serial);
            motionBowVisual::prepare(sharedStage.serial);
            motionShieldVisual::prepare(anchor,sharedStage.serial);
            motionArrowVisual::prepare(anchor,sharedStage.serial);
            motionOffhandVisual::prepare(anchor,sharedStage.serial);
            motionOffhandVisual::fishInHand(anchor);
            motionConsumableVisual::prepare(anchor,sharedStage.serial);
            motionModBow::prepare(anchor,sharedStage.serial);
            itemGallery::tick(anchor,sharedStage.serial);
            liveThrowAim::prepare(anchor);
            motionFist::prepare(anchor);
            motionBodyIk::prepare(anchor,sharedStage.serial);
            onlineSync::frame(sharedStage.serial);
            motionVisual::verifyContextSource();
            heldVfxBridge::observe();
            vfxEmitterCensus::observe(motionVisual::scene,anchor);
            {rootCamera::Sample cam;{std::lock_guard<std::mutex> lock(rootCamera::mutex);cam=rootCamera::latest;}
             effectEntityCensus::observe(cam.world,liveTargets::types.world,anchor.root,liveTargets::guestWorld);}   // EVR_EFFECT_CENSUS=1, or lightly as a guest
            heldEffectScope.begin();
            const bool usesGesture=controllerUiFixture::gestureRequested||controllerUiFixture::combatGestureRequested||controllerUiFixture::liveRequested;
            motion::HeadGestureResult gesture;
            if(usesGesture){
                motion::GestureHead head{liveFrame.head,liveFrame.headFlags,liveFrame.frame,liveFrame.sessionGeneration,liveFrame.hands.originGeneration,liveFrame.time};
                // Entering the two-hand hold was hard to notice on the headset (user, 2026-10-11): a short pulse on
                // both hands on entry, a faint one on the left hand on leaving.
                {static bool wieldHeld=false;const bool wieldNow=motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::wieldPose.active;
                 if(wieldNow!=wieldHeld){wieldHeld=wieldNow;if(wieldNow)controllerUiFixture::requestBowHaptic({.4f,.4f},{80,80});else controllerUiFixture::requestBowHaptic({.15f,0},{40,0});}}
                // The measured-contact trial claims the right grip while it holds. With the VHVR entry the
                // hold needs the left grip only, so the right grip stays the quick wheel.
                gesture=controllerUiFixture::publishGesture(liveFrame.hands,head,GetTickCount64(),motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::wieldPose.active&&!motionVisual::wieldState.vhvrEntry(),
                    motionVisual::poseReady&&motionHandVisual::ready&&motion::bowDrawAsset(motionVisual::item,motionVisual::guid,motionVisual::rangedAnimationRequested&&controllerUiFixture::bowDrawRequested)?motionVisual::bowBinding(motionVisual::item,motionVisual::sourceHandle):
                    motionModBow::drawable(sharedStage.serial)?motionModBow::geometry.binding:0,
                    motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::bowGeometry.binding?motionVisual::bowGeometry:
                    motionModBow::drawable(sharedStage.serial)&&motionHandVisual::ready?motionModBow::geometry:motion::BowPhysicalGeometry{});
                menuPointer::frame(liveFrame.hands,head,GetTickCount64());
                bugReport::frame(liveFrame.hands,head,GetTickCount64());
                {const bool held=motionVisual::poseReady&&motionHandVisual::ready;
                 aimMark::frame(liveFrame.hands,held&&motionVisual::bowDrawHeld,held?motionVisual::bowGeometry:motion::BowPhysicalGeometry{},held?motionVisual::rangedItem:0,
                    held?motionVisual::rangedGeometry:motion::BowPhysicalGeometry{},held&&motionVisual::rangedTwoHanded,menuPointer::screen.load(),GetTickCount64());}
                targetMark::frame(liveFrame.frame,GetTickCount64());
                keyScript::frame(liveFrame.frame,keyScript::installed?menuPointer::gameWindow():nullptr);
                if(controllerUiFixture::liveGuardRequested){
                    // A shield on the left hand, else a melee weapon currently held with both hands.
                    motion::LiveGuardEquipment guard;
                    if(motionVisual::contextReady&&motionVisual::contextFrame==liveFrame.frame){
                        const auto root=rootCamera::snapshot();
                        const auto shield=attachmentObserver::shieldAppearance(root.entity,motionVisual::scene,motionVisual::view,root.world,GetTickCount64());
                        // The mod's bow in the left hand: the shield is put aside meanwhile (motion_shield_visual.hpp), no guard with it.
                        if(shield.entity&&shield.item&&!controllerUiFixture::modBowItem.load()){guard.binding=(uint64_t(shield.item)<<32)|shield.handle;guard.shield=true;}
                        else if(motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::meleeWeapon.binding&&motionVisual::meleeWeapon.twoHanded){
                            guard.binding=motionVisual::meleeWeapon.binding;guard.twoHandWeapon=true;guard.weaponForward=motionVisual::meleeWeapon.forward;}
                    }
                    controllerUiFixture::publishGuard(guard,liveFrame.hands.frame,GetTickCount64());
                }
                if(controllerUiFixture::liveHandUseRequested)
                    controllerUiFixture::publishRod(motionVisual::poseReady&&motionHandVisual::ready?motionVisual::heldRod:motion::LiveSwingRod{},GetTickCount64());
                if(controllerUiFixture::liveMeleeRequested){
                    // Bare hands: each hand is a fist (motion_fist.hpp); otherwise the weapon shown, and the left part of a pair.
                    const bool fists=motionFist::ready&&motionFist::frame==liveFrame.frame&&!(motionVisual::poseReady&&motionVisual::meleeWeapon.binding);
                    controllerUiFixture::publishMelee(fists?motionFist::right:motionVisual::poseReady&&motionHandVisual::ready?motionVisual::meleeWeapon:motion::LiveMeleeWeapon{},liveFrame.hands.frame,GetTickCount64());
                    controllerUiFixture::publishOffhandMelee(fists?motionFist::left:motionVisual::poseReady&&motionHandVisual::ready&&motionOffhandVisual::ready&&motionOffhandVisual::frame==liveFrame.frame?motionOffhandVisual::meleeWeapon:motion::LiveMeleeWeapon{},liveFrame.hands.frame,GetTickCount64());
                }
                if(controllerUiFixture::rangedAimRequested){
                    const bool shown=motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::rangedItem;
                    controllerUiFixture::publishRanged(shown?motionVisual::rangedGeometry:motion::BowPhysicalGeometry{},shown?motionVisual::rangedItem:0,shown&&motionVisual::rangedTwoHanded,GetTickCount64());
                }
                motion::Feedback feedback;{std::lock_guard<std::mutex> lock(controllerUiFixture::handMutex);feedback=controllerUiFixture::handFeedback;}
                auto command=controllerUiFixture::gestureFeedback.update(liveFrame.hands,gesture,feedback,GetTickCount64());
                if(controllerUiFixture::bowDrawRequested||controllerUiFixture::liveMeleeRequested||motionVisual::twoHandWieldRequested)controllerUiFixture::applyBowHaptic(command,liveFrame.hands,GetTickCount64());
                // The layer takes 0 < duration <= 100 ms and amplitudes 0..1 only, and a rejection fails the frame's motion
                // visuals (VR back to the flat plate - 2026-10-20 13:43/13:48, a 150 ms pulse at the fish's bite): kept in range here.
                for(unsigned h=0;h<2;h++){
                    if(!std::isfinite(command.amplitude[h])||command.amplitude[h]<0)command.amplitude[h]=0;else if(command.amplitude[h]>1)command.amplitude[h]=1;
                    if(command.amplitude[h]>0){if(command.duration[h]<=0)command.duration[h]=10000000;else if(command.duration[h]>100000000)command.duration[h]=100000000;}}
                // A rejected vibration is written down, not a failure of the frame (2026-10-20: it used to end VR for the session).
                if(command.fresh){const int rejected=nativeGestureHaptic(liveFrame.frame,liveFrame.sessionGeneration,command.amplitude.data(),command.duration.data());
                    static unsigned told=0;if(rejected&&told++<20)logFile->event("native_gesture_feedback_rejected","\"frame\":"+std::to_string(liveFrame.frame)+",\"result\":"+std::to_string(rejected)+
                        ",\"amplitude\":["+std::to_string(command.amplitude[0])+","+std::to_string(command.amplitude[1])+"],\"duration_ns\":["+std::to_string(command.duration[0])+","+std::to_string(command.duration[1])+"]");}
            }
            if(controllerUiFixture::motionGuardRequested){
                const bool ready=motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::updatedFrame==liveFrame.frame&&motionHandVisual::frame==liveFrame.frame;
                motion::GestureHead head{liveFrame.head,liveFrame.headFlags,liveFrame.frame,liveFrame.sessionGeneration,liveFrame.hands.originGeneration,liveFrame.time};
                controllerUiFixture::publishGuardHands(ready?liveFrame.hands:motion::HandFrame{},head,GetTickCount64());
            }
            // Publish the immutable hand and its policy in one critical section.
            if(controllerUiFixture::twoHandAttackRequested&&(!motionVisual::poseReady||!motionHandVisual::ready||motionVisual::updatedFrame!=liveFrame.frame||motionHandVisual::frame!=liveFrame.frame))
                controllerUiFixture::publishHands({},GetTickCount64());
            else controllerUiFixture::publishHands(liveFrame.hands,GetTickCount64(),usesGesture?&gesture:nullptr);
            // Publish the completed same-frame weapon and hand as one value.
            // Native event/query threads must never combine next weapon with
            // previous hands while the renderer is still preparing them.
            if(motionColliderChannel::requested){
                if(motionVisual::poseReady&&motionHandVisual::ready&&motionVisual::updatedFrame==liveFrame.frame&&motionHandVisual::frame==liveFrame.frame)
                    motionColliderChannel::publish(motionVisual::pendingColliderPose,liveFrame.hands,GetTickCount64());
                else motionColliderChannel::invalidate();
            }
            if(nativeMotionHaptic){
                motion::Feedback feedback;{std::lock_guard<std::mutex> lock(controllerUiFixture::handMutex);feedback=controllerUiFixture::handFeedback;}
                const auto amplitudes=motion::feedbackForFrame(feedback,liveFrame.hands,GetTickCount64());
                if(!usesGesture&&nativeMotionHaptic(liveFrame.frame,liveFrame.sessionGeneration,amplitudes.data()))throw std::runtime_error("native haptic bridge rejected frame");
                logFile->event("motion_feedback_requested","\"frame\":"+std::to_string(liveFrame.frame)+",\"source_frame\":"+std::to_string(feedback.frame)+",\"amplitude\":"+std::to_string(amplitudes[1]));
            }
        }catch(const std::exception& e){motionColliderChannel::invalidate();motionVisual::failed=true;motionVisual::poseReady=false;motionVisual::contextReady=false;motionHandVisual::ready=false;
            // 1.0 (2026-10-16): the model functions of the motion visuals are not all ported yet (motion_visual_bridge.hpp
            // validate()); a failure before the first validation leaves the hands out and the session goes on.
            if(motionVisual::validated)pair->failed=true;
            else{motionVisual::requested=false;logFile->event("optional_hook_skipped","\"hook\":\"motion_visual\",\"reason\":"+jsonString(e.what()));}
            logFile->event("motion_visual_failure","\"reason\":"+jsonString(e.what()));}
    }
    worldReadyDiagnostic::Scope originalReadiness;
    const auto originalScene=sharedCameraApplied?read<uintptr_t>(reinterpret_cast<uintptr_t>(view)+8):0;
    const auto originalSceneBlocked=originalScene?read<uint8_t>(originalScene+0x1c):0;
    void* result=nullptr;
    {struct OriginalReadyScope {
            worldReadyDiagnostic::Scope* previous=worldReadyDiagnostic::current;
            explicit OriginalReadyScope(worldReadyDiagnostic::Scope* value){worldReadyDiagnostic::current=value;}
            ~OriginalReadyScope(){worldReadyDiagnostic::current=previous;}
        } originalReadyScope(sharedCameraApplied?&originalReadiness:worldReadyDiagnostic::current);
        waterFeedback::Scope waterScope(pair?pair->water.get():nullptr);
        cullingHistory::Scope cullingScope(cullingHistory::requested?pair:nullptr,-1);
        worldSharedStages::Scope stageScope(worldNativeDiagnostic?&sharedStage:nullptr);
        terrainReadyDiagnostic::Scope terrainScope(worldNativeDiagnostic?&terrainStage:nullptr);
        downloadFeedbackDiagnostic::Scope downloadScope(worldNativeDiagnostic?&downloadStage:nullptr);
        struct SharedProjectionScope {
            const game::ProjectionDescriptor* previous=eyeProjection;unsigned calls=projectionCalls;
            explicit SharedProjectionScope(const game::ProjectionDescriptor* descriptor){if(descriptor){eyeProjection=descriptor;projectionCalls=0;}}
            ~SharedProjectionScope(){eyeProjection=previous;projectionCalls=calls;}
        } sharedProjection(sharedCameraApplied?&sharedGameCamera.descriptor:nullptr);
        stableFrustumBridge::Scope frustumScope(sharedCameraApplied?&sharedFrustum:nullptr);
        vfxTransparent::FrameScope vfxFrame(pair,sharedStage.serial,originalRenderParameters,
            !skipDesktop&&!vfxToEye&&active.load()&&!done.load()&&pair&&reinterpret_cast<uintptr_t>(view)==pair->originalView,pair?pair->originalView:0);
        grassMovementBridge::EyeScope grassScope(sharedStage.serial,-1,!skipDesktop&&!vfxToEye&&active.load()&&!done.load()&&pair&&reinterpret_cast<uintptr_t>(view)==pair->originalView);
        shadowHistoryProbe::ViewScope shadowProbeScope(reinterpret_cast<uintptr_t>(view));
        if(desktopRenderScale>0&&earlyPrepared&&pair&&!pair->failed&&!done.load()&&reinterpret_cast<uintptr_t>(view)==pair->originalView){
            const auto wrapper=read<uintptr_t>(reinterpret_cast<uintptr_t>(view));
            const auto current=read<float>(wrapper+layout::ownerRenderScale);
            if(current!=desktopRenderScale){
                if(desktopScaleWrites<8)logFile->event("desktop_render_scale_write","\"before\":"+std::to_string(current)+",\"after\":"+std::to_string(desktopRenderScale)+",\"serial\":"+std::to_string(sharedStage.serial));
                if(desktopScaleBefore<0){desktopScaleBefore=current;desktopScaleWrapper=wrapper;}
                *reinterpret_cast<float*>(wrapper+layout::ownerRenderScale)=desktopRenderScale;++desktopScaleWrites;
            }
        }
        if(skipDesktop){
            writeEmptyRenderResult(out);result=out;++desktopSkipped;
            if(desktopSkipped<=3||desktopSkipped%100==0)
                logFile->event("desktop_skip","\"serial\":"+std::to_string(sharedStage.serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
                    ",\"skipped\":"+std::to_string(desktopSkipped)+",\"rendered\":"+std::to_string(desktopRendered)+",\"candidates\":"+std::to_string(desktopSkipCandidates));
        }else{
            // The refresh's original View without its VfxView (f7c50 runs the effects for a View that has one): the
            // left eye, drawn next, runs them as on the other frames.
            struct VfxAside {uintptr_t view=0,vfx=0;~VfxAside(){if(view)*reinterpret_cast<uintptr_t*>(view+layout::viewVfx)=vfx;}} vfxAside;
            if(vfxToEye){vfxAside.vfx=read<uintptr_t>(pair->originalView+layout::viewVfx);
                if(vfxAside.vfx){vfxAside.view=pair->originalView;*reinterpret_cast<uintptr_t*>(vfxAside.view+layout::viewVfx)=0;++refreshVfxAside;
                    if(refreshVfxAside<=3||refreshVfxAside%500==0)logFile->event("desktop_refresh_vfx_to_eye","\"serial\":"+std::to_string(sharedStage.serial)+
                        ",\"frame\":"+std::to_string(liveFrame.frame)+",\"eye\":"+std::to_string(worldSharedStages::vfxOwnerEye)+",\"frames\":"+std::to_string(refreshVfxAside));}}
            // The original View drawn beside the eyes (the refresh, -DesktopRender) with the left eye's streaming object;
            // not while the world loads (the game's own original View then, as without the mod).
            const bool lendOriginal=shareStreaming&&!loadingWorld&&active.load()&&pair&&!pair->failed&&!done.load()&&
                reinterpret_cast<uintptr_t>(view)==pair->originalView&&pair->pairedFrames>=desktopSkipWarmupPairs;
            StreamingLend originalLend(lendOriginal?pair->originalView:0,lendOriginal?pair->created[0]:0,lendOriginal);
            struct UpdateSkip {bool was=worldSharedStages::detailRadius::streamingUpdate::skip;explicit UpdateSkip(bool on){if(on)worldSharedStages::detailRadius::streamingUpdate::skip=true;}
                ~UpdateSkip(){worldSharedStages::detailRadius::streamingUpdate::skip=was;}} updateSkip(originalLend.lent&&worldSharedStages::detailRadius::streamingUpdate::enabled);
            {static std::atomic<uint64_t> lends{0};if(originalLend.lent){const auto n=++lends;if(n<=3||n%300==0)logFile->event("original_streaming_lent","\"serial\":"+std::to_string(sharedStage.serial)+
                ",\"frame\":"+std::to_string(liveFrame.frame)+",\"lends\":"+std::to_string(n)+",\"own\":"+hex(originalLend.own));}}
            result=original(out,context,view,originalRenderParameters,target);
            if(desktopSkip&&pair&&reinterpret_cast<uintptr_t>(view)==pair->originalView)++desktopRendered;
            if(refreshDesktop&&desktopRefreshDiscard&&!desktopDiscard&&active.load()&&pair&&!pair->failed&&!done.load()){
                writeEmptyRenderResult(out);++desktopRefreshDiscarded;
                if(desktopRefreshDiscarded<=3||desktopRefreshDiscarded%100==0)
                    logFile->event("desktop_refresh_discard","\"serial\":"+std::to_string(sharedStage.serial)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"discarded\":"+std::to_string(desktopRefreshDiscarded)+",\"rendered\":"+std::to_string(desktopRendered));
            }
            if(desktopDiscard&&active.load()&&pair&&!pair->failed&&!done.load()&&reinterpret_cast<uintptr_t>(view)==pair->originalView&&
               pair->pairedFrames>=desktopSkipWarmupPairs){
                const bool hadImage=read<uint8_t>(reinterpret_cast<uintptr_t>(out))&1;
                writeEmptyRenderResult(out);++desktopDiscarded;
                if(desktopDiscarded<=3||desktopDiscarded%1000==0)
                    logFile->event("desktop_discard","\"serial\":"+std::to_string(sharedStage.serial)+",\"discarded\":"+std::to_string(desktopDiscarded)+
                        ",\"had_image\":"+(hadImage?"true":"false"));
            }
        }
        if(sharedCameraApplied)sharedProjectionCalls=projectionCalls;
    }
    if(sharedCameraApplied){
        try{
            const bool unchanged=read<std::array<uint8_t,layout::parameterBytes>>(reinterpret_cast<uintptr_t>(parameters))==preservedParameters;
            std::array<uint8_t,0x38> emptyResult{};writeEmptyRenderResult(emptyResult.data());
            const auto nativeResult=read<std::array<uint8_t,0x38>>(reinterpret_cast<uintptr_t>(out));
            // Headset run 2026-10-17 02:45 (the user changed graphics settings): the frame the game skipped while rebuilding
            // returned the empty result with byte 5 set ("00 00 00 00 00 01 ..."; no image: byte 0 bit 0 clear) and no camera
            // work - it ended the VR session as a mismatch. That byte alone is not a picture: with it cleared the result is
            // the exact empty one, and the frame is a wait as before.
            auto flagCleared=nativeResult;flagCleared[5]=0;
            const bool nativeEmpty=nativeResult==emptyResult||(!(nativeResult[0]&1)&&flagCleared==emptyResult);
            const auto boundary=game::sharedRenderBoundary(unchanged,nativeEmpty,
                sharedProjectionCalls,sharedFrustum.calls,sharedFrustum.mask,sharedFrustum.failed);
            if(boundary==game::SharedRenderBoundary::Deferred){
                if(++sharedRenderDeferredFrames>600)throw std::runtime_error("native original View did not resume within 600 frames");
                if(pair->pairPending)throw std::runtime_error("native deferred frame has a pending eye pair");
                pair->serial=sharedStage.serial;pair->frameReady=false;
                inputPose::channel.invalidate();skipLiveFrame(*pair,earlyPrepared);originalDeferred=true;
                logFile->event("world_original_render_deferred","\"frame\":"+std::to_string(liveFrame.frame)+
                    ",\"serial\":"+std::to_string(sharedStage.serial)+",\"consecutive_frames\":"+std::to_string(sharedRenderDeferredFrames)+
                    ",\"native_empty_exact\":true,\"source_parameters_unchanged\":true,\"projection_calls\":0,\"frustum_calls\":0,\"eyes_rendered\":false,\"layers\":0,\"scene_blocked\":"+std::to_string(originalSceneBlocked)+
                    ",\"frame_begun_early\":"+(earlyPrepared?"true":"false"));
            }else{
            if(boundary==game::SharedRenderBoundary::Rendered&&sharedRenderDeferredFrames){
                logFile->event("world_original_render_resumed","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(sharedStage.serial)+
                    ",\"deferred_frames\":"+std::to_string(sharedRenderDeferredFrames));sharedRenderDeferredFrames=0;
            }
            std::ostringstream fields;fields<<std::setprecision(9)<<"\"frame\":"<<liveFrame.frame<<",\"serial\":"<<sharedStage.serial
                <<",\"projection_calls\":"<<sharedProjectionCalls<<",\"source_parameters_unchanged\":"<<(unchanged?"true":"false")
                <<",\"stable_frustum_calls\":"<<sharedFrustum.calls<<",\"stable_frustum_mask\":"<<sharedFrustum.mask
                <<",\"stable_frustum_failed\":"<<(sharedFrustum.failed?"true":"false")
                <<",\"retreat_m\":"<<sharedCamera.retreat<<",\"near\":"<<sharedCamera.nearZ<<",\"far\":"<<sharedCamera.farZ
                <<",\"fov\":["<<sharedCamera.view.fov.angleLeft<<','<<sharedCamera.view.fov.angleRight<<','<<sharedCamera.view.fov.angleUp<<','<<sharedCamera.view.fov.angleDown
                <<"],\"camera_q32\":["<<sharedGameCamera.position.x<<','<<sharedGameCamera.position.y<<','<<sharedGameCamera.position.z
                <<"],\"orientation\":["<<sharedGameCamera.orientation.x<<','<<sharedGameCamera.orientation.y<<','<<sharedGameCamera.orientation.z<<','<<sharedGameCamera.orientation.w<<']';
            for(auto item:{std::pair<const char*,size_t>{"view_matrix",0x1000},{"projection_matrix",0x1080}}){ // 1.0 (0xa80, 0xb00 before)
                auto matrix=read<std::array<float,16>>(reinterpret_cast<uintptr_t>(view)+item.second);
                if(item.second==0x1000){matrix[3]=matrix[7]=matrix[11]=0;matrix[15]=1;}
                fields<<",\""<<item.first<<"\":[";
                for(size_t n=0;n<16;n++){if(!std::isfinite(matrix[n]))throw std::runtime_error("nonfinite shared culling matrix");if(n)fields<<',';fields<<matrix[n];}fields<<']';
            }
            logFile->event("world_shared_culling_camera",fields.str());
            if(sharedProjectionCalls!=1){
                std::ostringstream state;state<<"\"frame\":"<<liveFrame.frame<<",\"serial\":"<<sharedStage.serial
                    <<",\"scene_blocked\":"<<unsigned(originalSceneBlocked)<<",\"source_unchanged\":"<<(unchanged?"true":"false")
                    <<",\"output_hex\":\""<<std::hex<<std::setfill('0');
                for(auto value:read<std::array<uint8_t,0x38>>(reinterpret_cast<uintptr_t>(out)))state<<std::setw(2)<<unsigned(value);
                state<<"\",\"predicates\":["<<std::dec;
                for(size_t n=0;n<originalReadiness.count;n++){if(n)state<<',';const auto& o=originalReadiness.observations[n];
                    state<<"{\"entry\":"<<o.entry<<",\"result\":"<<(o.result?"true":"false")<<'}';}
                state<<"],\"overflow\":"<<(originalReadiness.overflow?"true":"false");
                logFile->event("original_render_incomplete",state.str());
            }
            if(!unchanged||sharedProjectionCalls!=1)throw std::runtime_error("shared culling camera application mismatch");
            if(sharedFrustum.failed||sharedFrustum.calls!=2||sharedFrustum.mask!=3)throw std::runtime_error("stable shared frustum application mismatch");
            }
        }catch(const std::exception& e){pair->failed=true;logFile->event("calibration_series_failure","\"reason\":"+jsonString(e.what()));}
    }
    logWorldColorState(reinterpret_cast<uintptr_t>(view),-1,sharedStage.serial,originalRenderParameters,out);
    if(worldNativeDiagnostic)terrainReadyDiagnostic::write(terrainStage,-1,sharedStage.serial);
    if(worldNativeDiagnostic)downloadFeedbackDiagnostic::write(downloadStage,-1,sharedStage.serial);
    if(worldNativeDiagnostic)worldSharedStages::write(sharedStage);
    pair=retainedPair.load(std::memory_order_acquire);
    if(active.load(std::memory_order_acquire) && !claimed.exchange(true))probe(view,context,parameters,target);
    else if(active.load()&&pair&&!pair->failed&&!done.load()&&!originalDeferred&&!failureRecovery::held()&&(calibrationFrames>1||worldNativeDiagnostic)&&reinterpret_cast<uintptr_t>(view)==pair->originalView&&
            read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18)!=pair->serial){
        try{
            bool ready=true;
            if(liveXr&&!earlyPrepared){
                const auto serial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
                if(pair->xrAttemptSerial==serial)ready=false;
                else{pair->xrAttemptSerial=serial;ready=beginLiveFrame(pair->originalView);
                    if(!ready)logFile->event("world_tracking_frame_deferred","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(serial));}
            }
            if(ready&&failureRecovery::due(failureRecovery::failAt,++failureRecovery::attempts))throw std::runtime_error("test failure (EVR_TEST_FAIL_AT)");
            if(ready)recordPair(*pair,context,parameters,target);
        }
        catch(const std::exception& e){pair->failed=true;selectView(pair->scene,pair->originalView);logFile->event("calibration_series_failure","\"reason\":"+jsonString(e.what()));}
    }
    if(!heldEffectScope.restore()&&pair)pair->failed=true;
    return result;
}
void require(MH_STATUS status,const char *name) { if(status!=MH_OK)throw std::runtime_error(std::string(name)+": "+MH_StatusToString(status)); }
void disableUnusedSceneHook() noexcept {
    if(destroyOwnerEnabled.load()&&!retainedPair.load(std::memory_order_acquire)){
        const auto status=MH_DisableHook(destroyOwnerTarget);destroyOwnerEnabled=status!=MH_OK;
        try{logFile->event("calibration_scene_hook_removed","\"status\":"+std::to_string(int(status))+",\"reason\":\"no_retained_pair\"");}catch(...){}
    }
}
bool quiesceDependentHooks() noexcept {
    if(awaitRenderCallbacks())return true;
    // Disabling a detour does not wait for a callback already using its
    // dependencies. Keep forwarding hooks and pinned trampolines until exit.
    try{logFile->event("view_resource_dependencies_retained","\"reason\":\"callback_still_running_after_hook_disable\",\"callbacks\":"+
        std::to_string(renderCallbacks.load()));}catch(...){}
    return false;
}
}
extern "C" void evr_observe_ui_state(uintptr_t rsp) noexcept {uiStateObserver::onDispatch(rsp);}
extern "C" void evr_observe_camera(uintptr_t rsp) noexcept {headVisibility::onCopy(rsp);}
extern "C" void evr_observe_body_upload(uintptr_t rsp,unsigned kind) noexcept {bodyUpload::onUpload(rsp,kind);}
extern "C" void evr_observe_vfx_gate(uintptr_t rsp,unsigned gate) noexcept {if(gate==15)vfxTransparent::onParticleConstants(rsp);else if(gate>=11)vfxGateProbe::onGate(rsp,gate-11);else if(gate>=3)vfxModelBridge::onGate(rsp,gate-3);else vfxTransparent::onGate(rsp,gate);}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeViewResources(void *directory) {
    if(!directory || started.exchange(true))return 2;
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ProbeViewResources),&pinned))return 3;
    void *target=nullptr;bool enabled=false,renderWasEnabled=false,renderRemovalLogged=false;
    auto recordRenderRemoval=[&]{
        if(!renderWasEnabled||renderRemovalLogged||!target)return;
        const auto signature=parsePattern(renderPattern);
        const bool restored=scan(static_cast<const uint8_t*>(target),signature.size(),signature)==std::vector<size_t>{0};
        logFile->event("view_resource_hook_removed","\"original_bytes_restored\":"+std::string(restored?"true":"false")+
            ",\"status\":0,\"callbacks\":"+std::to_string(renderCallbacks.load())+",\"trampoline_retained\":true");
        renderRemovalLogged=true;
        if(!restored)throw std::runtime_error("original render instruction restoration mismatch");
    };
    try {
        logFile=new Log((std::filesystem::path(static_cast<wchar_t *>(directory))/"view-resource-probe.jsonl").string(),true);
        calibrationDirectory=std::filesystem::path(static_cast<wchar_t*>(directory));
        if(std::filesystem::exists(calibrationDirectory/"world-culling-history.txt")){
            std::ifstream policy(calibrationDirectory/"world-culling-history.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_CULLING_HISTORY_1"||(!worldNativeDiagnostic&&worldResourceStage!=2))
                throw std::runtime_error("invalid private culling history policy");
            cullingHistory::requested=true;
            if(std::filesystem::exists(calibrationDirectory/"world-culling-fill-only.txt")){
                std::ifstream policy(calibrationDirectory/"world-culling-fill-only.txt");std::string magic;policy>>magic>>std::ws;
                if(!policy||!policy.eof()||magic!="EVR_CULLING_FILL_ONLY_1"||!worldNativeDiagnostic)throw std::runtime_error("invalid culling fill ablation policy");
                cullingHistory::fillOnly=true;
            }
        }
        if(std::filesystem::exists(calibrationDirectory/"world-root-camera.txt")){
            std::ifstream policy(calibrationDirectory/"world-root-camera.txt");std::string magic;policy>>magic>>rootCamera::mode>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_ROOT_CAMERA_1"||!worldNativeDiagnostic||!liveXr||rootCamera::mode<1||rootCamera::mode>2)
                throw std::runtime_error("invalid stable root camera diagnostic policy");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-dlss-eval-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-dlss-eval-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_DLSS_EVAL_OBSERVE_1"||!liveXr)throw std::runtime_error("DLSS evaluate observation requires live XR");
            dlssEval::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-camera-modes.txt")){
            std::ifstream policy(calibrationDirectory/"world-camera-modes.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_CAMERA_MODES_1"||!liveXr||rootCamera::mode!=2)
                throw std::runtime_error("camera mode policy requires live first-person");
            rootCamera::cameraModes=true;
        }
        // The pad styles (2026-10-25, user: "the play mode switchable: the full VR, the camera only in first person with the
        // Xbox pad played as usual, the third person view with the pad, the desktop"; include/evr/vr_settings.hpp Style; the
        // loader's world-pad-mode.txt, with the motion controllers' files left out): the real gamepad reaches the game as it
        // is (no controller-ui-fixture.txt). First person keeps the head-bob removal of the stable root (user); third person
        // shows the game's own camera (camera_mode.hpp cameraPresentation, root_camera_bridge.hpp renderAnchor).
        if(std::filesystem::exists(calibrationDirectory/"world-pad-mode.txt")){
            std::ifstream policy(calibrationDirectory/"world-pad-mode.txt");std::string magic,view;int lock=1,tilt=0;policy>>magic>>view>>lock>>tilt>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_PAD_MODE_1"||(view!="first"&&view!="third")||!liveXr||rootCamera::mode!=2||!rootCamera::cameraModes||
               std::filesystem::exists(calibrationDirectory/"controller-ui-fixture.txt"))throw std::runtime_error("invalid pad mode policy");
            game::padStyle=view=="third"?2:1;game::padPitchLock=lock!=0;game::padTiltDegrees=std::clamp(tilt,0,90);
            // As the live input (configureControllerFixture): in first person the head steers the walk, the jump and the attacks;
            // a full-screen UI holds the camera (it stops the game's camera update).
            if(game::padStyle==1)inputHeadIdle::vrLocomotion=true;
            rootCamera::uiHold=[]()->bool{const auto last=inputUi::lastNonHudMs.load();const auto now=GetTickCount64();return last&&now>=last&&now-last<=1500;};
            // Online as the mod without VR (padOnline), with the host's parts for the players whose mod confirms their own
            // actions (onlineFlat::prepareHostParts): their strokes, shots, parries reach this host's server.
            onlineSync::configure();
            if(onlineSync::requested){onlineSync::flat=true;
                nativeImpactComponents::requested=true;actionSequenceObserver::requested=true;
                sequenceColliderObserver::requested=true;impactHierarchyObserver::requested=true;attachmentObserver::requested=true;
                motionColliderApply::requested=true;motionColliderApply::live=true;liveTargets::requested=true;
                remoteMelee::nearestTarget=&liveTargets::nearestTo;controllerUiFixture::undoDamage=&liveTargets::undoDamage;controllerUiFixture::guardGiveBack=&liveTargets::guardGiveBack;
                controllerUiFixture::staminaOf=&liveTargets::staminaOf;controllerUiFixture::shotHitArrived=&liveTargets::publishShotHit;
                inputRanged::liveBowWatch=true;bowSpawnObserver::remoteAlways=true;}
            // The locked enemy's health bar and lock-on arrow above the enemy, not on the UI plate (2026-10-25, user: "in TPS and
            // FPS the enemy's lock is on the UI - the focus does not match, very hard to see; make it look as in VR"): the list of
            // creatures (kept on the event selection hook's ticks - installed for the pad styles below) and the mark's part of
            // motion VR (target_mark.hpp: the HUD image's part about the target goes to it, the layer's evrSetTargetHud).
            liveTargets::requested=true;targetMark::install();
            logFile->event("pad_mode_enabled","\"style\":\""+view+"\",\"pitch_lock\":"+(game::padPitchLock.load()?"true":"false")+",\"tilt_degrees\":"+std::to_string(game::padTiltDegrees.load())+
                ",\"head_steers\":"+(inputHeadIdle::vrLocomotion?"true":"false")+",\"online\":"+(onlineSync::requested?"true":"false"));
        }
        if(liveXr&&rootCamera::mode==2)rootCamera::readKnownHeight();
        if(std::filesystem::exists(calibrationDirectory/"world-camera-mode-fixture.txt")){
            std::ifstream policy(calibrationDirectory/"world-camera-mode-fixture.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_CAMERA_MODE_FIXTURE_1"||!rootCamera::cameraModes)
                throw std::runtime_error("camera mode fixture requires modes and at least 600 pairs");
            rootCamera::modeFixture=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-ui-state-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-ui-state-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_UI_STATE_OBSERVE_1"||!worldNativeDiagnostic)throw std::runtime_error("invalid scoped UI observer policy");
            uiStateObserver::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-input-observer.txt")){
            std::ifstream policy(calibrationDirectory/"world-input-observer.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_INPUT_OBSERVE_1"||!rootCamera::mode)throw std::runtime_error("invalid input observer policy");
            inputDirection::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-input-head-idle.txt")){
            std::ifstream policy(calibrationDirectory/"world-input-head-idle.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_INPUT_HEAD_IDLE_1"||!worldNativeDiagnostic||!liveXr||
               rootCamera::mode!=2||!inputDirection::requested)throw std::runtime_error("invalid idle head camera integration policy");
            inputHeadIdle::requested=true;inputShadow::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-input-head-walking.txt")){
            std::ifstream policy(calibrationDirectory/"world-input-head-walking.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_INPUT_HEAD_WALKING_1"||!worldNativeDiagnostic||!liveXr||
               rootCamera::mode!=2||!inputDirection::requested||inputHeadIdle::requested)
                throw std::runtime_error("invalid ordinary walking head camera integration policy");
            inputHeadIdle::requested=inputHeadIdle::walkingRequested=true;inputShadow::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-input-walk-fixture.txt")){
            std::ifstream policy(calibrationDirectory/"world-input-walk-fixture.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_INPUT_WALK_FIXTURE_1"||!inputHeadIdle::walkingRequested)
                throw std::runtime_error("walking fixture requires explicit ordinary walking policy");
            inputHeadIdle::fixtureRequested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-eye-private-transient.txt")){
            std::ifstream policy(calibrationDirectory/"world-eye-private-transient.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_EYE_PRIVATE_TRANSIENT_1"||!liveXr)throw std::runtime_error("eye private transient pool policy requires live XR");
            privateMemoryPools::privateTransient=true;
            logFile->event("eye_private_transient_policy","\"pool\":\"renderer+19c replaced per eye during c02a0\",\"released_after_views\":true");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-eye-dealias.txt")){
            std::ifstream policy(calibrationDirectory/"world-eye-dealias.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_EYE_DEALIAS_1"||!liveXr)throw std::runtime_error("eye de-alias policy requires live XR");
            eyeDealias=true;logFile->event("eye_dealias_policy","\"dedicated_memory\":true,\"scope\":\"eye feature/resolution setup on render thread\"");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-eye-render-scale.txt")){
            std::ifstream policy(calibrationDirectory/"world-eye-render-scale.txt");std::string magic;policy>>magic>>eyeRenderScale>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_EYE_RENDER_SCALE_1"||!liveXr||!std::isfinite(eyeRenderScale)||eyeRenderScale<.25f||eyeRenderScale>1.f)
                throw std::runtime_error("invalid eye render scale policy");
            logFile->event("eye_render_scale_policy","\"render_scale\":"+std::to_string(eyeRenderScale)+",\"upscaler\":\"game_setting\"");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-desktop-render-scale.txt")){
            std::ifstream policy(calibrationDirectory/"world-desktop-render-scale.txt");std::string magic;policy>>magic>>desktopRenderScale>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_DESKTOP_RENDER_SCALE_1"||!liveXr||!std::isfinite(desktopRenderScale)||desktopRenderScale<.25f||desktopRenderScale>1.f)
                throw std::runtime_error("invalid desktop render scale policy");
            logFile->event("desktop_render_scale_policy","\"render_scale\":"+std::to_string(desktopRenderScale)+",\"restored_on_exit\":true");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-endurance.txt")){
            std::ifstream policy(calibrationDirectory/"world-endurance.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_WORLD_ENDURANCE_1"||!worldNativeDiagnostic||!liveXr)
                throw std::runtime_error("invalid bounded endurance policy");
            worldEndurance=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-session.txt")){
            std::ifstream policy(calibrationDirectory/"world-session.txt");std::string magic;policy>>magic>>sessionMinutes>>std::ws;
            // 0 minutes (the start by itself, for play): no minute limit - the series ends with the world or the stop file.
            if(!policy||!policy.eof()||magic!="EVR_WORLD_SESSION_1"||!worldEndurance||!liveXr||sessionMinutes>240)
                throw std::runtime_error("invalid play session policy (needs endurance live XR and 0..240 minutes)");
            worldSession=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-active-xr-exit.txt")){
            std::ifstream policy(calibrationDirectory/"world-active-xr-exit.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_WORLD_ACTIVE_XR_EXIT_1"||!worldEndurance||!sceneCleanup)throw std::runtime_error("active XR exit needs explicit finite world Scene lifetime");
            activeXrExitTrial=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-active-contact-owner.txt")){
            std::ifstream policy(calibrationDirectory/"world-active-contact-owner.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_ACTIVE_CONTACT_OWNER_1"||!activeXrExitTrial)throw std::runtime_error("active contact owner requires explicit active Scene trial");
            activeContactOwnerTrial=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-render-hook-batch.txt")){
            std::ifstream f(calibrationDirectory/"world-render-hook-batch.txt");std::string magic;f>>magic>>std::ws;
            if(!f||!f.eof()||magic!="EVR_RENDER_HOOK_BATCH_1"||!activeContactOwnerTrial)throw std::runtime_error("render hook batch requires explicit active-owner trial");
            renderBatchRequested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-equipment-observer.txt")){
            std::ifstream policy(calibrationDirectory/"world-equipment-observer.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_EQUIPMENT_OBSERVE_1"||!rootCamera::mode)throw std::runtime_error("invalid equipment observer policy");
            equipmentObserver::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-body-upload-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-body-upload-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_BODY_UPLOAD_OBSERVE_1"||!worldNativeDiagnostic||!cullingHistory::requested)
                throw std::runtime_error("invalid body upload observation policy");
            bodyUpload::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-head-visibility.txt")){
            std::ifstream policy(calibrationDirectory/"world-head-visibility.txt");std::string magic;policy>>magic>>std::ws;
            // 1.0 (2026-10-16): the per-eye culling history is not ported yet (run_render_resource_probe.py leaves it out when
            // the unused proof skipped it). The head payload is written inside that private culling scope only, so without it
            // the head is not hidden (onCopy passes the native constants on); the rest of the policy (hands only, motion
            // visuals) goes on.
            if(!policy||!policy.eof()||magic!="EVR_HEAD_VISIBILITY_1"||rootCamera::mode!=2||!equipmentObserver::requested||cullingHistory::fillOnly)
                throw std::runtime_error("invalid live head visibility policy");
            if(!cullingHistory::requested)logFile->event("head_visibility_without_culling_history","\"head_hidden\":false,\"reason\":\"per-eye culling history not ported to 1.0\"");
            headVisibility::requested=true;equipmentObserver::continuous=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-held-vfx-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-held-vfx-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_HELD_VFX_OBSERVE_1")throw std::runtime_error("invalid held VFX observer policy");
            heldVfxObserver::requested=true;inputRanged::liveEquipmentWatch=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-held-vfx-mount.txt")){
            std::ifstream policy(calibrationDirectory/"world-held-vfx-mount.txt");std::string magic;policy>>magic>>std::ws;
            std::ifstream fixture(calibrationDirectory/"controller-ui-fixture.txt");std::string inputMagic;fixture>>inputMagic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_HELD_VFX_MOUNT_1"||!heldVfxObserver::requested||!fixture||!fixture.eof()||inputMagic!="EVR_CONTROLLER_LIVE_1")throw std::runtime_error("held VFX mount requires observed live weapon ownership");
            heldVfxBridge::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-attachments-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-attachments-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_ATTACHMENTS_OBSERVE_1"||!inputDirection::requested||!equipmentObserver::requested)throw std::runtime_error("invalid attachment observation policy");
            attachmentObserver::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-hands-only.txt")){
            std::ifstream policy(calibrationDirectory/"world-hands-only.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_HANDS_ONLY_1"||!headVisibility::requested||!rootCamera::cameraModes||std::filesystem::exists(calibrationDirectory/"world-body-clip.txt"))throw std::runtime_error("invalid hands-only policy");
            headVisibility::handsOnly=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-body-clip.txt")){
            std::ifstream policy(calibrationDirectory/"world-body-clip.txt");std::string magic;policy>>magic>>bodyUpload::clipDistance;
            unsigned part;while(policy>>part){if(part<4||part>23||part==15)throw std::runtime_error("body clip parts must be local body equipment");bodyUpload::parts.push_back(part);}
            if(!policy.eof()||magic!="EVR_BODY_CLIP_1"||!worldNativeDiagnostic||!headVisibility::requested||!std::isfinite(bodyUpload::clipDistance)||
               bodyUpload::clipDistance<.01f||bodyUpload::clipDistance>1.f||bodyUpload::parts.empty()||bodyUpload::parts.size()>20)
                throw std::runtime_error("invalid explicit body clip policy");
            bodyUpload::requested=bodyUpload::clipRequested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-body-glide-clip.txt")){
            std::ifstream policy(calibrationDirectory/"world-body-glide-clip.txt");std::string magic;policy>>magic>>bodyUpload::glideClipDistance>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_BODY_GLIDE_CLIP_1"||!bodyUpload::clipRequested||!rootCamera::cameraModes||
               !std::isfinite(bodyUpload::glideClipDistance)||bodyUpload::glideClipDistance<bodyUpload::clipDistance||bodyUpload::glideClipDistance>1.f)
                throw std::runtime_error("invalid explicit gliding body clip policy");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-body-motion-clip.txt")){
            std::ifstream policy(calibrationDirectory/"world-body-motion-clip.txt");std::string magic;policy>>magic>>bodyUpload::motionClipDistance>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_BODY_MOTION_CLIP_1"||!bodyUpload::clipRequested||!rootCamera::cameraModes||
               !inputHeadIdle::walkingRequested||bodyUpload::glideClipDistance>0||!std::isfinite(bodyUpload::motionClipDistance)||
               bodyUpload::motionClipDistance<bodyUpload::clipDistance||bodyUpload::motionClipDistance>1.f)
                throw std::runtime_error("invalid explicit motion body clip policy");
            inputActor::motionObserverRequested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-water-feedback.txt")){
            std::ifstream policy(calibrationDirectory/"world-water-feedback.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_WATER_FEEDBACK_1"||(!worldNativeDiagnostic&&worldResourceStage!=2))
                throw std::runtime_error("invalid private water feedback policy");
            privateWaterFeedback=true;
        }
        if(worldNativeDiagnostic&&std::filesystem::exists(calibrationDirectory/"world-shared-update-policy.txt")){
            std::ifstream policy(calibrationDirectory/"world-shared-update-policy.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_WORLD_SHARED_ONCE_1")throw std::runtime_error("invalid shared world update policy");
            worldSharedStages::once=true;
        }
        if(liveXr){
            std::ifstream config(calibrationDirectory/"live-xr-config.txt");std::string magic;
            config>>magic>>calibrationFrames>>liveRenderScale;
            if(!config||magic!="EVR_LIVE_XR_1"||!calibrationFrames||calibrationFrames>(worldEndurance?diagnostic::maximumNativeDiagnosticFrames:300)||
               !std::isfinite(liveRenderScale)||liveRenderScale<=0||liveRenderScale>2)throw std::runtime_error("invalid live XR diagnostic config");
            config>>std::ws;if(!config.eof())throw std::runtime_error("trailing live XR diagnostic config");
            configuredLiveFrames=calibrationFrames;
            if(worldSession){
                // Per-event-name limit of this log. The launcher's -LogNameCap (EVR_LOG_NAME_CAP in this
                // process) may raise it for long combined audits; play sessions keep 3000.
                unsigned nameCap=3000;
                if(const char* text=std::getenv("EVR_LOG_NAME_CAP")){char* end=nullptr;const auto requested=std::strtoul(text,&end,10);if(end&&!*end&&requested>=100&&requested<=20000)nameCap=unsigned(requested);}
                logFile->event("session_policy","\"minutes\":"+std::to_string(sessionMinutes)+",\"configured_pairs\":"+std::to_string(calibrationFrames)+
                ",\"pairs\":\"unlimited\",\"stop_file\":"+jsonString((calibrationDirectory/"stop.request").string())+",\"log_name_cap\":"+std::to_string(nameCap));
                calibrationFrames=sessionUnlimitedPairs;logFile->setNameCap(nameCap);}
        }
        if(std::filesystem::exists(calibrationDirectory/"world-share-vfx-lights.txt")){
            std::ifstream policy(calibrationDirectory/"world-share-vfx-lights.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_SHARE_VFX_LIGHTS_1"||!worldNativeDiagnostic||!liveXr||!worldSharedStages::once)
                throw std::runtime_error("invalid shared VFX lights diagnostic policy");
            sharedPointLights::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-share-vfx-models.txt")){
            // The eye that does not own the VFX culls the VFX models for itself (vfx_model_bridge.hpp).
            std::ifstream policy(calibrationDirectory/"world-share-vfx-models.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_SHARE_VFX_MODELS_1"||!sharedPointLights::requested)
                throw std::runtime_error("shared VFX models require the shared VFX lights");
            vfxModelBridge::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-share-vfx-decals.txt")){
            // The eye that does not own the VFX also borrows the owner's decal list (shared_point_lights.hpp).
            std::ifstream policy(calibrationDirectory/"world-share-vfx-decals.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_SHARE_VFX_DECALS_1"||!sharedPointLights::requested)
                throw std::runtime_error("shared VFX decals require the shared VFX lights");
            sharedPointLights::decalsRequested=true;logFile->event("shared_decals_enabled","\"view_buffer\":\"0x928\",\"view_counter\":\"0x8e4\",\"view_capacity\":\"0x80\",\"stride\":112");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-early-xr.txt")){
            std::ifstream policy(calibrationDirectory/"world-early-xr.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_WORLD_EARLY_XR_1"||!worldNativeDiagnostic||!liveXr||!worldSharedStages::once)
                throw std::runtime_error("invalid early XR world policy");
            earlyWorldXr=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-scenery-replay.txt")){
            std::ifstream policy(calibrationDirectory/"world-scenery-replay.txt");std::string magic;
            auto& a=sceneryReplayReference;auto& q=a.orientation;
            policy>>magic>>a.root.x>>a.root.y>>a.root.z>>q.x>>q.y>>q.z>>q.w>>std::ws;
            const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
            if(!policy||!policy.eof()||magic!="EVR_SCENERY_REPLAY_1"||!liveXr||!earlyWorldXr||rootCamera::mode!=2||
               !std::isfinite(norm)||std::abs(norm-1)>1e-4||std::abs(relativeQ32(a.root.x,0))>10000||
               std::abs(relativeQ32(a.root.y,0))>10000||std::abs(relativeQ32(a.root.z,0))>10000)
                throw std::runtime_error("invalid bounded Mock scenery camera reference");
            sceneryReplayRequested=true;
            logFile->event("scenery_replay_camera","\"draw_only\":true,\"entity_unchanged\":true");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-scenery-culling.txt")){
            std::ifstream policy(calibrationDirectory/"world-scenery-culling.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_SCENERY_CULLING_OBSERVE_1"||!liveXr||!headVisibility::requested||!cullingHistory::requested)
                throw std::runtime_error("scenery culling observation requires private live culling upload");
            sceneryCulling::requested=true;
            if(std::filesystem::exists(calibrationDirectory/"world-scenery-no-hzb.txt")){
                std::ifstream ablation(calibrationDirectory/"world-scenery-no-hzb.txt");std::string mode;ablation>>mode>>std::ws;
                if(!ablation||!ablation.eof()||mode!="EVR_SCENERY_NO_HZB_1")throw std::runtime_error("invalid scenery HZB diagnostic policy");
                sceneryCulling::noHzb=true;
            }
        }
        if(std::filesystem::exists(calibrationDirectory/"world-fog-projection.txt")){
            std::ifstream policy(calibrationDirectory/"world-fog-projection.txt");std::string magic,mode;policy>>magic>>mode>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_FOG_PROJECTION_1"||(mode!="observe"&&mode!="match"&&mode!="compare")||!liveXr||!worldNativeDiagnostic)
                throw std::runtime_error("invalid fog projection policy");
            fogProjection::policy=mode=="compare"?3:mode=="match"?2:1;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-shadow-projection.txt")){
            std::ifstream policy(calibrationDirectory/"world-shadow-projection.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_SHADOW_PROJECTION_1"||!liveXr||!worldNativeDiagnostic)
                throw std::runtime_error("invalid native shadow projection policy");
            shadowProjection::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-detail-bindings.txt")){
            std::ifstream policy(calibrationDirectory/"world-detail-bindings.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_DETAIL_BINDINGS_1"||!liveXr||!worldNativeDiagnostic)
                throw std::runtime_error("invalid private detail bindings policy");
            detailBinding::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-eye-frustum.txt")){
            std::ifstream policy(calibrationDirectory/"world-eye-frustum.txt");std::string magic,mode;policy>>magic>>mode>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_EYE_FRUSTUM_1"||(mode!="observe"&&mode!="stable")||!liveXr)
                throw std::runtime_error("invalid eye frustum diagnostic policy");
            stableFrustumBridge::observeEyes=true;stableFrustumBridge::stabilizeEyes=mode=="stable";
            logFile->event("eye_frustum_policy","\"mode\":"+jsonString(mode));
        }
        if(std::filesystem::exists(calibrationDirectory/"world-shared-culling-camera.txt")){
            std::ifstream policy(calibrationDirectory/"world-shared-culling-camera.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_SHARED_CULLING_CAMERA_1"||!earlyWorldXr||rootCamera::mode!=2||!sharedPointLights::requested)
                throw std::runtime_error("invalid shared culling camera policy");
            sharedCullingCamera=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-motion-visual.txt")){
            std::ifstream policy(calibrationDirectory/"world-motion-visual.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_MOTION_VISUAL_1"||!headVisibility::handsOnly||!attachmentObserver::requested||!earlyWorldXr||!sharedCullingCamera)
                throw std::runtime_error("invalid motion visual diagnostic policy");
            motionVisual::requested=true;
            if(std::filesystem::exists(calibrationDirectory/"world-motion-hands.txt")){
                std::ifstream hf(calibrationDirectory/"world-motion-hands.txt");std::string hm;hf>>hm>>std::ws;
                if(!hf||!hf.eof()||hm!="EVR_MOTION_HAND_VISUAL_1")throw std::runtime_error("invalid hand visual policy");
                motionHandVisual::requested=true;
                if(std::filesystem::exists(calibrationDirectory/"world-grip-preview.txt"))weaponGripPreview::load(calibrationDirectory);
                if(std::filesystem::exists(calibrationDirectory/"world-hand-curl.txt")){
                    std::ifstream cf(calibrationDirectory/"world-hand-curl.txt");std::string cm;float relaxed=-1;cf>>cm>>relaxed>>std::ws;
                    std::ifstream pf(calibrationDirectory/"hand-curl-closed-pose.bin",std::ios::binary|std::ios::ate);
                    if(!cf||!cf.eof()||cm!="EVR_HAND_CURL_1"||!std::isfinite(relaxed)||relaxed<0||relaxed>.9f||!pf||pf.tellg()!=102*48)
                        throw std::runtime_error("invalid hand curl policy/closed pose");
                    motionHandVisual::closedPose.resize(102);pf.seekg(0);pf.read(reinterpret_cast<char*>(motionHandVisual::closedPose.data()),102*48);
                    motionHandVisual::curlRelaxed=relaxed;
                    logFile->event("hand_curl_policy","\"relaxed\":"+std::to_string(relaxed)+",\"closed_joints\":102");
                }
                if(std::filesystem::exists(calibrationDirectory/"world-motion-equipment-switch.txt")){
                    std::ifstream ef(calibrationDirectory/"world-motion-equipment-switch.txt");std::string em;ef>>em>>std::ws;
                    if(!ef||!ef.eof()||em!="EVR_MOTION_EQUIPMENT_SWITCH_1"||!weaponGripPreview::requested||std::filesystem::exists(calibrationDirectory/"world-native-grip-observe.txt"))
                        throw std::runtime_error("equipment switch requires reviewed visual-only profile");
                    motionVisual::equipmentSwitchRequested=true;
                }
                if(std::filesystem::exists(calibrationDirectory/"world-native-grip-observe.txt")){
                    std::ifstream gf(calibrationDirectory/"world-native-grip-observe.txt");std::string gm;gf>>gm>>std::ws;
                    if(!gf||!gf.eof()||gm!="EVR_NATIVE_GRIP_OBSERVE_1")throw std::runtime_error("invalid grip observation policy");
                    nativeGripObserver::requested=true;
                    if(std::filesystem::exists(calibrationDirectory/"world-item-catalog.txt")){
                        std::ifstream cf(calibrationDirectory/"world-item-catalog.txt");std::string cm;cf>>cm>>std::ws;
                        if(!cf||!cf.eof()||cm!="EVR_ITEM_CATALOG_1")throw std::runtime_error("invalid item catalog policy");
                        itemCatalogObserver::requested=true;logFile->event("item_catalog_enabled","\"read_only\":true,\"maximum_items\":32768,\"maximum_selected\":4096");
                    }
                }
            }
        }
        if(std::filesystem::exists(calibrationDirectory/"world-vfx-transparent-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-vfx-transparent-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_VFX_TRANSPARENT_OBSERVE_1"||!sharedCullingCamera)
                throw std::runtime_error("invalid VFX transparent observation policy");
            vfxTransparent::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-vfx-transparent-render.txt")){
            std::ifstream policy(calibrationDirectory/"world-vfx-transparent-render.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_VFX_TRANSPARENT_RENDER_1"||!vfxTransparent::requested)
                throw std::runtime_error("invalid VFX transparent rendering policy");
            vfxTransparent::render=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-gi-share.txt")){
            std::ifstream policy(calibrationDirectory/"world-gi-share.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy.eof()){policy>>std::hex>>giShare::keepMask>>std::dec>>std::ws;}
            if(!policy||!policy.eof()||magic!="EVR_GI_SHARE_1"||!liveXr||!worldNativeDiagnostic||(giShare::keepMask>>giShare::targets.size()))throw std::runtime_error("GI share needs live world XR");
            giShare::requested=true;
            logFile->event("gi_share_policy","\"warmup_pairs\":"+std::to_string(giShare::warmupPairs)+",\"owner\":\"left_eye\",\"right_eye_keeps_mask\":"+std::to_string(giShare::keepMask));
        }
        if(std::filesystem::exists(calibrationDirectory/"world-keep-dof.txt")){
            std::ifstream policy(calibrationDirectory/"world-keep-dof.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_KEEP_DOF_1")throw std::runtime_error("invalid keep-DoF policy");
            keepDepthOfField=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-vr-sharpness.txt")){
            std::ifstream policy(calibrationDirectory/"world-vr-sharpness.txt");std::string magic;float value=-1;policy>>magic>>value>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_VR_SHARPNESS_1"||!(value>=0&&value<=1)||!liveXr)throw std::runtime_error("invalid VR sharpness policy");
            vrSharpness=value;
        }
        if(liveXr){std::ostringstream f;f<<std::setprecision(4)<<"\"depth_of_field\":"<<(keepDepthOfField?"\"game\"":"\"off\"")<<",\"sharpness\":";
            if(vrSharpness>=0)f<<vrSharpness;else f<<"\"game\"";logFile->event("vr_post_effects_policy",f.str());}
        if(std::filesystem::exists(calibrationDirectory/"world-controller-fixture-arm.txt")){
            std::ifstream policy(calibrationDirectory/"world-controller-fixture-arm.txt");std::string magic;uint32_t pairs=0;policy>>magic>>pairs>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_CONTROLLER_FIXTURE_ARM_1"||pairs<30||pairs>3000)throw std::runtime_error("invalid controller fixture arm policy");
            controllerFixtureArmPair=pairs;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-edge-tint.txt")){
            std::ifstream policy(calibrationDirectory/"world-edge-tint.txt");std::string magic;float strength=-1;policy>>magic>>strength>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_EDGE_TINT_1"||!(strength>=0&&strength<=1)||!liveXr)throw std::runtime_error("edge tint needs live world XR and strength 0..1");
            statusVignette::vrTint=true;edgeTintBridge::maxStrength=strength;
            std::ostringstream f;f<<std::setprecision(4)<<"\"low_health\":\"rim\",\"freezing\":\"rim\",\"blur\":\"off\",\"blend\":\"multiply\",\"max_strength\":"<<strength<<",\"band\":0.12,\"sides\":\"temporal_top_bottom\"";
            logFile->event("vr_status_tint_policy",f.str());
        }
        if(std::filesystem::exists(calibrationDirectory/"world-force-freezing.txt")){
            // Diagnostic (2026-10-06): force the freezing vignette amount so its VR presentation can be checked without a cold area.
            std::ifstream policy(calibrationDirectory/"world-force-freezing.txt");std::string magic;float value=-1;policy>>magic>>value>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_FORCE_FREEZING_1"||!(value>=0&&value<=4)||!liveXr)throw std::runtime_error("invalid forced freezing policy");
            statusVignette::forceFreezing=value;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-object-watch.txt")){
            std::ifstream policy(calibrationDirectory/"world-object-watch.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_OBJECT_WATCH_1"||!liveXr||!worldNativeDiagnostic)throw std::runtime_error("object watch needs live world XR");
            objectWatch::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-gi-dump.txt")){
            std::ifstream policy(calibrationDirectory/"world-gi-dump.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_GI_DUMP_1"||!liveXr||!worldNativeDiagnostic)throw std::runtime_error("GI dump needs live world XR");
            giDump=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-desktop-discard.txt")){
            std::ifstream policy(calibrationDirectory/"world-desktop-discard.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_DESKTOP_DISCARD_1"||!liveXr||!worldNativeDiagnostic)
                throw std::runtime_error("invalid desktop discard policy (live world XR)");
            desktopDiscard=true;
            logFile->event("desktop_discard_policy","\"warmup_pairs\":"+std::to_string(desktopSkipWarmupPairs)+",\"original_render\":\"executed_result_discarded\"");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-desktop-skip.txt")){
            std::ifstream policy(calibrationDirectory/"world-desktop-skip.txt");std::string magic;policy>>magic>>desktopSkipEvery>>std::ws;
            // EVR_DESKTOP_SKIP_EVERY=<0 or 2..60> (for a test from [Environment] of EnshroudedVR.ini, 2026-10-22): another interval
            // of the original View's refresh - 0 none (10-20: the right eye's bright water patch was mostly gone without it)
            {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_DESKTOP_SKIP_EVERY",v,sizeof v);
             char* end=nullptr;if(n&&n<sizeof v){const auto every=unsigned(std::strtoul(v,&end,10));if(end&&end!=v&&*end==0&&(every==0||(every>=2&&every<=60))){desktopSkipEvery=every;   // (review M1)
              logFile->event("desktop_skip_override","\"every\":"+std::to_string(every));}}}
            // On skipped frames eye 0 owns the shared updates and the water feedback consumer; borrowed VFX lights and
            // particles need eye 0 to own VFX too (world-desktop-skip-vfx.txt) so the other eye borrows from it. The
            // shared culling camera only applies to frames that still render the original View.
            const bool vfxOwner=std::filesystem::exists(calibrationDirectory/"world-desktop-skip-vfx.txt");
            if(!policy||!policy.eof()||magic!="EVR_DESKTOP_SKIP_1"||!liveXr||!worldNativeDiagnostic||!worldSharedStages::once||
               desktopSkipEvery==1||desktopSkipEvery>60||((sharedPointLights::requested||vfxTransparent::requested)&&!vfxOwner))
                throw std::runtime_error("invalid desktop skip policy (live XR, shared updates once, 0 or 2..60, VFX borrowing needs the eye VFX owner)");
            desktopSkip=true;
            {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_ALTERNATE_EYES",value,sizeof value);alternateEyes::requested=n==1&&value[0]=='1';
             logFile->event("alternate_eyes_policy","\"enabled\":"+std::string(alternateEyes::requested?"true":"false")+",\"warmup_pairs\":"+std::to_string(alternateEyes::warmupPairs)+
                ",\"eye_left_out\":\"its last picture at its own pose\",\"warp_by_the_mod\":"+std::string(dlssEval::depthNotes?"true":"false")+",\"eye_warp\":"+std::to_string([]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_EYE_WARP",v,sizeof v);return n==1?int(v[0]-'0'):0;}()));}
            {const auto ui=std::getenv("EVR_UI_QUAD");desktopRefreshDiscard=desktopSkipEvery&&ui&&!std::strcmp(ui,"1")&&!std::filesystem::exists(calibrationDirectory/"world-desktop-refresh-shown.txt");}
            logFile->event("desktop_skip_policy","\"refresh_result_discarded\":"+std::string(desktopRefreshDiscard?"true":"false")+",\"every\":"+std::to_string(desktopSkipEvery)+",\"warmup_pairs\":"+std::to_string(desktopSkipWarmupPairs)+
                ",\"shared_update_owner\":\"eye0_on_skipped_frames\"");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-desktop-compare.txt")){
            std::ifstream policy(calibrationDirectory/"world-desktop-compare.txt");std::string magic;policy>>magic>>std::ws;
            const char* mockNav=std::getenv("EVR_MOCK_NAV_FILE");
            if(!policy||!policy.eof()||magic!="EVR_DESKTOP_COMPARE_1"||!desktopSkip||!sceneryReplayRequested||!mockNav||!*mockNav)
                throw std::runtime_error("desktop comparison requires opted-in Mock navigation, fixed scenery and desktop skip");
            desktopCompare=true;logFile->event("desktop_compare_policy","\"initial_mode\":\"skip\",\"max_changes\":6");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-desktop-skip-vfx.txt")){
            std::ifstream policy(calibrationDirectory/"world-desktop-skip-vfx.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_DESKTOP_SKIP_VFX_1"||!desktopSkip)
                throw std::runtime_error("desktop skip VFX takeover requires desktop skip");
            desktopSkipVfx=true;
            logFile->event("desktop_skip_vfx_policy","\"vfx_owner\":\"eye0_on_skipped_frames\",\"eye1\":\"no_vfx\"");
        }
        auto base=reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));baseAddress=reinterpret_cast<uintptr_t>(base);
        const auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE)throw std::runtime_error("PE DOS signature");
        const auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 *>(base+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp || nt->OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)
            throw std::runtime_error("unsupported loaded executable build");
        const auto pattern=parsePattern(renderPattern);std::vector<uintptr_t> hits;
        const auto section=IMAGE_FIRST_SECTION(nt);
        for(uint32_t i=0;i<nt->FileHeader.NumberOfSections;++i) {
            if(!(section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
            const size_t rva=section[i].VirtualAddress,size=section[i].Misc.VirtualSize;
            if(rva+size>nt->OptionalHeader.SizeOfImage)throw std::runtime_error("invalid section range");
            for(auto offset:scan(base+rva,size,pattern))hits.push_back(rva+offset);
        }
        if(hits!=std::vector<uintptr_t>{renderRva})throw std::runtime_error("render signature missing/ambiguous");
        if(preparedSessionProbe||liveXr){const auto bytes=parsePattern("33 D2 41 B8 00 00 00 80 8B 01 90 85 C0 75");
            if(scan(base+0x8ae210,bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("native queue lock signature changed");}
        if(useRecordedXr){
            std::ifstream input(calibrationDirectory/"recorded-xr.txt");recordedXr=diagnostic::RecordedXr::read(input);
            calibrationFrames=uint32_t(recordedXr.frames.size());
            logFile->event("recorded_xr_loaded","\"frames\":"+std::to_string(calibrationFrames)+",\"render_scale\":"+std::to_string(recordedXr.renderScale)+",\"live_openxr_session\":false");
        }
        const std::pair<uintptr_t,const char *> lifecycle[]={
            {0x1035e0,"48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 60 48"},
            {0x107ca0,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 79 08 48 8B D9 48"},
            {0x1079b0,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B 91 28 11"},
            {0x1076f0,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B 69 08"},
            {0x1092e0,"48 89 5C 24 10 55 56 57 48 83 EC 50 48 8B 79"}};
        for(const auto &entry:lifecycle) { const auto bytes=parsePattern(entry.second);
            if(scan(base+entry.first,bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("view lifecycle signature changed at "+hex(entry.first)); }
        if(initializeFeatures) {
            const std::pair<uintptr_t,const char *> featureSites[]={
                {0x103ba0,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 50 48 8B 41 08 4C 8D 25 ? ? ? ? 4D 8B F1 4D 8B F8"},
                {0x17a3c0,"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 48 8B EC 48 83 EC 60 48 8B 01 48 8B FA 41 B1 08 C6 44 24 20 00 BA B8 04 00 00 41 B8 08 00"},
                {0x17b340,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 59 10 48 8D 91 70"},
                {0x11ccaf0,"48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ? ? ? ? 45 33 E4 48 8D 05 CF"},
                {0x11cd5f0,"48 89 5C 24 10 57 48 83 EC 40 48 89 11 48"},
                {0x11d1b10,"48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 30 48 8B DA 48 8B F9 48 83"},
                {0x11d2bb0,"40 53 57 48 83 EC 28 8B 1A 48 8B F9 44 8B C3"},
                {0x11d4480,"4C 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 55 53 57 41 56 48"},
                {0x11cd8d0,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20"},
                {0x11d3060,"48 8B C4 55 53 57 41 57 48 8D 68 A1"}};
            for(const auto &entry:featureSites) {const auto bytes=parsePattern(entry.second);
                if(scan(base+entry.first,bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("feature signature changed at "+hex(entry.first));}
        }
        if(initializeResolution) {
            for(const auto& entry:{std::pair<uintptr_t,const char*>{0x7fb20,"48 89 5C 24 20 55 56 57 41 56 41 57 48 8D 6C 24 C9 48 81 EC 00 01 00 00 48 8B 01 49"},
                    {0x80a20,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F1 48 8D 99 D8 03"}}){
                const auto bytes=parsePattern(entry.second);if(scan(base+entry.first,bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("FSR3 lifecycle signature changed");
            }
            const auto signature=parsePattern("48 8B C4 55 53 56 41 54 41 56 48 8D A8 98 FC");
            const auto cleanup=parsePattern("48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 8D 51 08 48 8B 8F 28 01 00");
            if(scan(base+0xf1cb0,signature.size(),signature)!=std::vector<size_t>{0} ||
               scan(base+0xf3970,cleanup.size(),cleanup)!=std::vector<size_t>{0})throw std::runtime_error("resolution lifecycle signature changed");
        }
        if(renderCalibration){
            const std::pair<uintptr_t,const char*> sites[]={{0x1012ce0,"40 53 55 56 41 56 48 83 EC 48 4D 8B F0 48"},{0x1012f90,"40 55 56 57 41 56 41 57 48 8D 6C 24 D1 48 81 EC"},{0x1014dc0,"48 8B C4 55 57 48 81 EC 88 00"}};
            for(const auto& entry:sites){auto bytes=parsePattern(entry.second);if(scan(base+entry.first,bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("calibration signature changed");}
            const auto captureRequest=calibrationDirectory/"world-final-capture.txt";
            if(std::filesystem::exists(captureRequest)){
                std::ifstream input(captureRequest);std::string magic;input>>magic;
                if(!worldNativeDiagnostic||!liveXr||configuredLiveFrames<2||magic!="EVR_WORLD_CAPTURE_1")throw std::runtime_error("invalid world final capture request");
                captureLiveFinal=true;
                if(std::filesystem::exists(calibrationDirectory/"world-capture-on-request.txt")){
                    std::ifstream requestMode(calibrationDirectory/"world-capture-on-request.txt");std::string requestMagic;requestMode>>requestMagic>>std::ws;
                    // Mock navigation, or (2026-10-18) the headset, where the VR bug report button asks (hooks/bug_report.hpp).
                    const char* mockNav=std::getenv("EVR_MOCK_NAV_FILE");const char* runtime=std::getenv("XR_RUNTIME_JSON");
                    const bool headset=runtime&&*runtime&&!std::strstr(runtime,"mock");
                    if(!requestMode||!requestMode.eof()||requestMagic!="EVR_WORLD_CAPTURE_REQUEST_1"||!((mockNav&&*mockNav)||headset))throw std::runtime_error("manual GPU capture requires opted-in Mock navigation or the headset");
                    captureOnRequest=true;
                }
            }
            const auto fogCaptureRequest=calibrationDirectory/"world-initial-fog-sdf-capture.txt";
            if(std::filesystem::exists(fogCaptureRequest)){
                std::ifstream input(fogCaptureRequest);std::string magic;input>>magic;
                if(!worldNativeDiagnostic||!liveXr||captureLiveFinal||magic!="EVR_WORLD_FOG_SDF_CAPTURE_1")throw std::runtime_error("invalid initial fog SDF capture request");
                captureInitialFogSdf=true;
            }
            const auto glideCaptureRequest=calibrationDirectory/"world-glide-capture.txt";
            if(std::filesystem::exists(glideCaptureRequest)){
                std::ifstream input(glideCaptureRequest);std::string magic;input>>magic>>std::ws;
                if(!input||!input.eof()||!worldNativeDiagnostic||!liveXr||rootCamera::mode!=2||!rootCamera::cameraModes||captureLiveFinal||captureInitialFogSdf||magic!="EVR_WORLD_GLIDE_CAPTURE_1")throw std::runtime_error("invalid native glide capture request");
                captureGlide=true;
            }
            if(!liveXr||captureLiveFinal||captureInitialFogSdf||captureGlide){auto module=GetModuleHandleW(L"renderdoc.dll");auto get=module?reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module,"RENDERDOC_GetAPI")):nullptr;
            if(!get||!get(eRENDERDOC_API_Version_1_6_0,reinterpret_cast<void**>(&captureApi))||captureApi->IsFrameCapturing())throw std::runtime_error("calibration requires idle RenderDoc API");
            captureApi->SetCaptureFilePathTemplate((std::filesystem::path(static_cast<wchar_t*>(directory))/(captureGlide?"world_glide":captureInitialFogSdf?"world_initial_fog_sdf":captureLiveFinal?"world_final":"native_pair")).string().c_str());}
        }
        target=base+renderRva;
        require(MH_Initialize(),"MH_Initialize");require(MH_CreateHook(target,reinterpret_cast<void *>(&render),reinterpret_cast<void **>(&original)),"MH_CreateHook");
        try{worldSharedStages::terrain::arena::install();}catch(...){}   // before the world (and its bricks) is made
        if(detailFactoryProbe)installDetailFactoryReserveHook();
        else if(!preparedSessionProbe)installSharedResolutionHooks();
        // 1.0 (2026-10-16): the detail factory (0x47730 in June) was inlined into the scene features' initialization
        // (0xf1b90); the trace is a record-only diagnostic (nothing reads it), so its loss is noted, not fatal.
        if(worldResourceStage==2||worldNativeDiagnostic)try{installNativeDetailFactoryTrace();}
            catch(const std::exception& e){logFile->event("native_detail_factory_trace_skipped","\"reason\":"+jsonString(e.what()));}
        // 1.0 (2026-10-16): the sharing of work between the eyes and the observers are optional; one whose check of the
        // new build fails is noted (optional_hook_skipped) and left out, instead of ending the session.
        auto optional=[&](const char* name,auto&& body,auto&& off){try{body();}catch(const std::exception& e){off();logFile->event("optional_hook_skipped","\"hook\":"+jsonString(name)+",\"reason\":"+jsonString(e.what()));}};
        if(privateWaterFeedback)optional("water_feedback",[&]{waterFeedback::install();},[&]{privateWaterFeedback=false;});
        if(waterFeedback::waterObserve::wanted()&&!waterFeedback::enabled)optional("water_feedback_observe",[&]{waterFeedback::waterObserve::install();},[&]{});
        if(cullingHistory::requested&&worldNativeDiagnostic)optional("culling_history",[&]{cullingHistory::install();},[&]{cullingHistory::requested=false;});
        if(detailBinding::requested)optional("detail_binding",[&]{detailBinding::install();},[&]{detailBinding::requested=false;});   // 1.0: skipped, not failed (the eye check below ends the session on failed)
        if(shadowProjection::requested)optional("shadow_projection",[&]{shadowProjection::install();},[&]{shadowProjection::requested=false;});
        if(headVisibility::requested)optional("head_visibility",[&]{headVisibility::install();},[&]{headVisibility::requested=false;headVisibility::failed=true;});
        if(bodyUpload::requested)optional("body_upload",[&]{bodyUpload::install();},[&]{bodyUpload::requested=false;});
        if(sharedPointLights::requested)optional("shared_point_lights",[&]{sharedPointLights::install();},[&]{sharedPointLights::requested=false;});
        if(liveXr)optional("eye_exposure",[&]{eyeExposure::install();},[&]{});
        if(liveXr)optional("combat_camera_override",[&]{combatCameraOverride::install();},[&]{});   // the fog dragon's camera kept off in VR
        if(sharedCullingCamera||desktopSkipVfx||stableFrustumBridge::observeEyes)optional("stable_frustum",[&]{stableFrustumBridge::install();},[&]{});
        if(desktopSkipVfx)optional("vfx_culling",[&]{vfxCulling::install();},[&]{});
        if(giShare::requested||giShare::borrowScreen)optional("gi_share",[&]{giShare::install();},[&]{giShare::requested=false;});
        if(objectWatch::requested)optional("object_watch",[&]{objectWatch::install();},[&]{objectWatch::requested=false;});
        if(statusVignette::forceFreezing>=0||statusVignette::vrTint)optional("status_vignette",[&]{statusVignette::install();},[&]{});
        if(vfxTransparent::requested)optional("vfx_transparent",[&]{vfxTransparent::install();},[&]{vfxTransparent::requested=false;vfxTransparent::render=false;});
        optional("vfx_cpu_draw_probe",[&]{vfxCpuDrawProbe::install();},[&]{});   // only with EVR_VFX_CPU_SKIP in the environment
        if(sharedPointLights::decalsRequested)optional("vfx_decals",[&]{vfxDecals::install();},[&]{sharedPointLights::decalsRequested=false;});   // after both: it uses the transparent writeback's allocation hook
        optional("grass_movement",[&]{grassMovementBridge::install();},[&]{});
        optional("snow_displacement",[&]{snowDisplacementBridge::install();},[&]{});
        optional("water_displacement",[&]{waterDisplacementBridge::install();},[&]{});
        optional("hit_event_intake",[&]{hitEventIntake::install();},[&]{});
        optional("shadow_update_once",[&]{shadowUpdateOnce::install();},[&]{});   // the shared point-light shadows updated once a frame (EVR_SHADOW_UPDATE_ONCE=0: off)
        optional("world_ray",[&]{worldRay::install();},[&]{});   // the guest's arrows against walls (EVR_ARROW_WALLS=0: off); EVR_RAYCAST_CENSUS=1: the census
        optional("shadow_history",[&]{shadowHistoryProbe::install();},[&]{});
        {void* const modelStubs[8]{reinterpret_cast<void*>(&evr_vfx_model_stub0),reinterpret_cast<void*>(&evr_vfx_model_stub1),reinterpret_cast<void*>(&evr_vfx_model_stub2),reinterpret_cast<void*>(&evr_vfx_model_stub3),
            reinterpret_cast<void*>(&evr_vfx_model_stub4),reinterpret_cast<void*>(&evr_vfx_model_stub5),reinterpret_cast<void*>(&evr_vfx_model_stub6),reinterpret_cast<void*>(&evr_vfx_model_stub7)};
         optional("vfx_model_bridge",[&]{vfxModelBridge::install(modelStubs,evr_vfx_model_trampolines);},[&]{});
         if(vfxModelBridge::installed)optional("vfx_spring_rebase",[&]{vfxSpringRebase::install();},[&]{});}
        {void* const extraStubs[4]{reinterpret_cast<void*>(&evr_vfx_extra_stub0),reinterpret_cast<void*>(&evr_vfx_extra_stub1),reinterpret_cast<void*>(&evr_vfx_extra_stub2),reinterpret_cast<void*>(&evr_vfx_extra_stub3)};
         optional("vfx_gate_probe",[&]{vfxGateProbe::install(extraStubs,evr_vfx_extra_trampolines);},[&]{});}
        if(rootCamera::modeFixture&&calibrationFrames<600)throw std::runtime_error("camera fixture needs at least 600 configured pairs");
        if(rootCamera::mode)rootCamera::install();
        if(std::filesystem::exists(calibrationDirectory/"world-bow-input-trial.txt")){
            std::ifstream bf(calibrationDirectory/"world-bow-input-trial.txt");std::string magic;bf>>magic>>std::ws;
            if(!bf||!bf.eof()||(magic!="EVR_BOW_INPUT_TRIAL_1"&&magic!="EVR_BOW_AIM_TRIAL_1"&&magic!="EVR_BOW_RELEASE_TRIAL_1"&&magic!="EVR_BOW_CANCEL_TRIAL_1")||!nativeGripObserver::requested)throw std::runtime_error("invalid bow input trial policy");
            controllerUiFixture::bowInputTrialRequested=true;
            const bool releaseFire=magic=="EVR_BOW_RELEASE_TRIAL_1";
            const bool cancelOnly=magic=="EVR_BOW_CANCEL_TRIAL_1";
            const bool aimed=magic=="EVR_BOW_AIM_TRIAL_1"||releaseFire||cancelOnly;
            controllerUiFixture::bowInputTrial=motion::BowInputTrial(aimed,releaseFire,cancelOnly);
            logFile->event("bow_input_trial_mode","\"aimed_triggers\":"+std::string(aimed?"true":"false")+",\"release_tap\":"+(releaseFire?"true":"false")+",\"cancel_only\":"+(cancelOnly?"true":"false"));
            logFile->event("bow_input_trial_enabled","\"maximum_presses\":1,\"native_button\":"+std::to_string(aimed?0:16384)+",\"native_state_write\":false,\"physical_draw\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-shield-mount.txt")){
            std::ifstream sf(calibrationDirectory/"world-shield-mount.txt");std::string magic;sf>>magic>>std::ws;
            if(!sf||!sf.eof()||magic!="EVR_SHIELD_MOUNT_1"||!nativeGripObserver::requested)throw std::runtime_error("invalid shield mount policy");
            motionShieldVisual::requested=true;logFile->event("motion_shield_enabled","\"measured_asset_only\":true,\"combat_authorized\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-animation.txt")){
            std::ifstream bf(calibrationDirectory/"world-bow-animation.txt");std::string magic;bf>>magic>>std::ws;
            if(!bf||!bf.eof()||magic!="EVR_BOW_ANIMATION_1"||!nativeGripObserver::requested)throw std::runtime_error("invalid bow animation policy");
            motionVisual::bowAnimationRequested=true;
            logFile->event("motion_bow_enabled","\"native_pose_copy\":true,\"physical_draw\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-ranged-animation.txt")){
            std::ifstream rf(calibrationDirectory/"world-ranged-animation.txt");std::string magic;rf>>magic>>std::ws;
            if(!rf||!rf.eof()||magic!="EVR_RANGED_ANIMATION_1"||!nativeGripObserver::requested||!motionVisual::bowAnimationRequested)throw std::runtime_error("invalid ranged animation policy");
            motionVisual::rangedAnimationRequested=true;
            logFile->event("motion_ranged_enabled","\"native_pose_copy\":true,\"tracked_projectile_aim\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-physical-cancel.txt")){
            std::ifstream pf(calibrationDirectory/"world-bow-physical-cancel.txt");std::string magic;pf>>magic>>std::ws;
            if(!pf||!pf.eof()||magic!="EVR_BOW_PHYSICAL_CANCEL_1"||!nativeGripObserver::requested||!motionVisual::bowAnimationRequested||controllerUiFixture::bowInputTrialRequested)throw std::runtime_error("invalid physical bow cancellation policy");
            controllerUiFixture::bowPhysicalTrialRequested=true;
            logFile->event("bow_physical_trial_enabled","\"known_bow_only\":true,\"firing_enabled\":false,\"native_power_write\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-physical-pose.txt")){
            std::ifstream pf(calibrationDirectory/"world-bow-physical-pose.txt");std::string magic;pf>>magic>>std::ws;
            if(!pf||!pf.eof()||magic!="EVR_BOW_PHYSICAL_POSE_1"||!controllerUiFixture::bowPhysicalTrialRequested)throw std::runtime_error("physical pose requires no-shot physical input trial");
            motionBowVisual::physicalPoseRequested=true;
            logFile->event("motion_bow_physical_enabled","\"known_bow_only\":true,\"private_pose_only\":true,\"native_power_write\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-physical-release.txt")){
            std::ifstream pf(calibrationDirectory/"world-bow-physical-release.txt");std::string magic;pf>>magic>>std::ws;
            if(!pf||!pf.eof()||magic!="EVR_BOW_PHYSICAL_RELEASE_1"||!controllerUiFixture::bowPhysicalTrialRequested||!motionBowVisual::physicalPoseRequested)throw std::runtime_error("physical release requires isolated physical pose trial");
            controllerUiFixture::bowPhysicalReleaseRequested=true;
            logFile->event("bow_physical_release_enabled","\"minimum_pull\":0.1,\"native_aim\":"+std::string(std::filesystem::exists(calibrationDirectory/"world-bow-tracked-aim.txt")?"false":"true")+",\"native_power_write\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-tracked-aim.txt")){
            std::ifstream af(calibrationDirectory/"world-bow-tracked-aim.txt");std::string magic;af>>magic>>std::ws;
            if(!af||!af.eof()||magic!="EVR_BOW_TRACKED_AIM_1"||!controllerUiFixture::bowPhysicalReleaseRequested||!std::filesystem::exists(calibrationDirectory/"world-bow-spawn-observe.txt"))throw std::runtime_error("tracked aim requires explicit physical release and spawn witness");
            controllerUiFixture::bowTrackedAimRequested=true;
            logFile->event("bow_tracked_aim_enabled","\"one_shot\":true,\"private_camera\":true,\"native_power_write\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-draw.txt")){
            std::ifstream df(calibrationDirectory/"world-bow-draw.txt");std::string magic;df>>magic>>std::ws;
            if(!df||!df.eof()||magic!="EVR_BOW_DRAW_1"||!controllerUiFixture::bowTrackedAimRequested)throw std::runtime_error("bow draw requires the tracked aim route");
            controllerUiFixture::bowDrawRequested=true;controllerUiFixture::clientNowFn=&nativeImpactComponents::clientNow;   // (review M2: the guest's bow tap without the body IK too)
            {char v[8]{};const auto m=GetEnvironmentVariableA("EVR_BOW_PRE_AIM",v,sizeof v);if(m==1&&v[0]=='0')controllerUiFixture::bowDraw.preAim(0);}   // include/evr/bow_draw.hpp preAimRange
            // The nocked arrow is drawn on the string of the tracked bow (VHVR), from the game's own arrow instance.
            motionArrowVisual::requested=motionBowVisual::physicalPoseRequested;
            logFile->event("bow_draw_enabled","\"repeatable\":true,\"two_hand_orientation\":true,\"attach_range\":0.2,\"full_draw_length\":0.6,\"minimum_pull\":0.1,\"shot_window_ms\":"+std::to_string(controllerUiFixture::bowDrawShotMs)+",\"native_power_write\":false");
        }
        configureControllerFixture(calibrationDirectory,60,true);
        if(std::filesystem::exists(calibrationDirectory/"world-live-melee.txt")){
            // Optional word "heavy=<1..2>": the factor on the swing speed that a heavy weapon needs (VHVR: 1.5, so 4.5 m/s).
            // On the headset at 25 frames a second no stroke of the great axe reached it in 18 seconds (game174405).
            std::ifstream mf(calibrationDirectory/"world-live-melee.txt");std::string magic,word;mf>>magic;float heavyFactor=1.5f,strokeTravel=0.f;bool knownWords=true,targetGate=false,heavyMotion=false;int heavyHold=int(controllerUiFixture::liveHeavyHoldMs);
            // "travel=<0..1>": metres a stroke has to cover above the required speed before it attacks (live_melee.hpp).
            while(mf>>word){try{if(word.rfind("heavy=",0)==0)heavyFactor=std::stof(word.substr(6));else if(word.rfind("travel=",0)==0)strokeTravel=std::stof(word.substr(7));else if(word.rfind("heavy_hold=",0)==0)heavyHold=std::stoi(word.substr(11));else if(word=="targets")targetGate=true;else if(word=="heavy_motion")heavyMotion=true;else knownWords=false;}catch(...){knownWords=false;}}
            // "heavy_hold=<ms>": a stroke that VHVR would take as the secondary attack holds the attack button that long
            // (live_secondary.hpp; default 400, 0 turns the heavy attack by motion off).
            if(heavyHold<0||heavyHold>3000)knownWords=false;
            if(magic!="EVR_LIVE_MELEE_1"||!knownWords||!(heavyFactor>=1.f&&heavyFactor<=2.f)||!(strokeTravel>=0.f&&strokeTravel<=1.f)||!controllerUiFixture::liveRequested||controllerUiFixture::motionAttackRequested)throw std::runtime_error("live melee requires the live controller input");
            // The heavy attack is a stroke with the right trigger held; "heavy_motion" adds VHVR's judgement by the motion.
            controllerUiFixture::liveMeleeRequested=true;controllerUiFixture::liveHeavyHoldMs=uint64_t(heavyHold);controllerUiFixture::liveHeavyByMotion=heavyMotion;
            {evr::motion::LiveMeleeSettings meleeSettings;meleeSettings.heavyFactor=heavyFactor;meleeSettings.strokeTravel=strokeTravel;controllerUiFixture::liveMelee=evr::motion::LiveMelee(meleeSettings);}
            // "targets": VHVR's rule that a stroke attacks only at a target (live_melee_targets.hpp). The list of targets is
            // kept on the ticks of the event selection hook, which the early Impact (world-live-melee-contact.txt) installs.
            aimMark::reachOf=&liveTargets::aimReach;   // the ground of the feet always; creatures when their list is kept
            if(targetGate){controllerUiFixture::liveMeleeTargetGate=true;liveTargets::requested=true;targetMark::install();
                {char v[4]{};if(GetEnvironmentVariableA("EVR_MELEE_TARGET_GATE",v,sizeof v)==1&&v[0]=='0')controllerUiFixture::liveMeleeTargetGate=false;}   // diagnostic: strokes attack without a target, the list kept
                controllerUiFixture::targetHealth=&liveTargets::healthNow;
                controllerUiFixture::hitWatchNear=[](const evr::motion::LiveMeleeWeapon& w,float radius,controllerUiFixture::HitNear* out,size_t maximum,uint64_t now){
                    std::array<liveTargets::NearBlade,16> nearby{};const auto n=liveTargets::nearBlade(w,radius,nearby.data(),std::min(maximum,nearby.size()),now);
                    for(size_t k=0;k<n;++k)out[k]={nearby[k].entity,nearby[k].distance,nearby[k].body,nearby[k].reach,nearby[k].state,nearby[k].character};
                    return n;};
                controllerUiFixture::hitWatchState=&liveTargets::stateNow;
                if(liveTargets::predictWanted){controllerUiFixture::hitPredict=&liveTargets::predictHit;controllerUiFixture::hitSettle=&liveTargets::settlePredictions;controllerUiFixture::hitLearn=&liveTargets::learnHit;
                 // the running action of the local player, for the stroke's attack modifier (live_melee_targets.hpp estimateStroke)
                 liveTargets::runningSequence=[]{actionSequenceObserver::Status a;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);a=actionSequenceObserver::latest;}
                  const auto now=GetTickCount64();return a.entity&&a.milliseconds&&now>=a.milliseconds&&now-a.milliseconds<=300&&a.active?a.sequence:0u;};
                 if(!actionSequenceObserver::requested){actionSequenceObserver::requested=true;actionSequenceObserver::recording=false;}}
                // A client's touches go to the host as hits (online_sync.hpp sendHits); the host lays the collider on them.
                controllerUiFixture::hitReport=[](uint32_t entity,bool character,uint64_t now){
                    if(!onlineSync::requested||!nativeImpactComponents::clientNow(now))return;std::array<int64_t,3> at{};
                    if(!liveTargets::entityPosition(entity,at,now))return;
                    std::lock_guard<std::mutex> lock(controllerUiFixture::hitReportMutex);auto* slot=&controllerUiFixture::hitReports[0];
                    for(auto& r:controllerUiFixture::hitReports)if(r.serial<slot->serial)slot=&r;
                    *slot={++controllerUiFixture::hitReportSerial,now,at,character,false,entity};};
                remoteMelee::nearestTarget=&liveTargets::nearestTo;controllerUiFixture::hitThing=&liveTargets::thingTouched;
                controllerUiFixture::creatureNear=&liveTargets::creatureNear;controllerUiFixture::undoDamage=&liveTargets::undoDamage;controllerUiFixture::guardGiveBack=&liveTargets::guardGiveBack;controllerUiFixture::staminaOf=&liveTargets::staminaOf;
                controllerUiFixture::localParryDecided=&parryObserver::localDecided;
                motionArrowVisual::flightHit=&liveTargets::segmentHit;controllerUiFixture::shotHitArrived=&liveTargets::publishShotHit;
                controllerUiFixture::shotSurfaceArrived=&liveTargets::publishSurfaceHit;
                // the world's surfaces for the guest's own arrow: the game's ray test (world_ray.hpp), when its hook is on
                motionArrowVisual::flightAsk=[](const float* from,const float* to)->uint64_t{return worldRay::enabled?worldRay::ask(from,to):0;};
                motionArrowVisual::flightAnswer=[](uint64_t& serial,bool& hit,std::array<int64_t,3>& at){const auto a=worldRay::answer();if(!a.serial)return false;
                    serial=a.serial;hit=a.hit;for(int k=0;k<3;k++)at[size_t(k)]=int64_t(double(a.at[k])*4294967296.);return true;};
                motionArrowVisual::flightSurface=[](const std::array<int64_t,3>& at,Vec3 dir){
                    // the wall or ground this guest's own arrow met, to the host (online_sync.hpp sendHits, kind 8)
                    if(!onlineSync::requested)return;
                    std::lock_guard<std::mutex> lock(controllerUiFixture::hitReportMutex);auto* slot=&controllerUiFixture::hitReports[0];
                    for(auto& r:controllerUiFixture::hitReports)if(r.serial<slot->serial)slot=&r;
                    *slot={++controllerUiFixture::hitReportSerial,GetTickCount64(),at,false,true,0,true,dir};};
                motionArrowVisual::flightReach=[](WorldPositionQ32 origin,Vec3 direction,uint64_t now){int by=0;const float r=liveTargets::aimReach(origin,direction,now,by);return by==1?r:1e9f;};
                motionArrowVisual::flightReport=[](const std::array<int64_t,3>& at,bool character,uint32_t entity){
                    // the hit of this guest's own arrow, to the host (online_sync.hpp sendHits, as a stroke's touch)
                    if(!onlineSync::requested)return;
                    std::lock_guard<std::mutex> lock(controllerUiFixture::hitReportMutex);auto* slot=&controllerUiFixture::hitReports[0];
                    for(auto& r:controllerUiFixture::hitReports)if(r.serial<slot->serial)slot=&r;
                    *slot={++controllerUiFixture::hitReportSerial,GetTickCount64(),at,character,true,entity};};
                controllerUiFixture::targetNear=[](const evr::motion::LiveMeleeWeapon& weapon,uint64_t now){const auto n=liveTargets::contact(weapon,now);return controllerUiFixture::TargetNear{n.reached,n.gated,n.entity,n.distance,n.cached,n.ahead,n.nearest};};
                // The actor's Grounded bit (0) off within the last half second: a stroke in the air attacks without a target.
                controllerUiFixture::actorAirborne=[](uint64_t now){std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);const auto& s=inputHeadIdle::latest;
                    return s.actorKnown&&now>=s.milliseconds&&now-s.milliseconds<=500&&!(s.actorState&1ull);};}
            logFile->event("live_melee_speed_policy","\"swing_m_per_s\":3,\"heavy_factor\":"+std::to_string(heavyFactor)+",\"stroke_travel_m\":"+std::to_string(strokeTravel)+",\"attacks_only_at_a_target\":"+(targetGate?"true":"false")+",\"air_strokes_free\":"+(controllerUiFixture::liveMeleeAirFree?"true":"false")+",\"heavy_by_right_trigger\":true,\"heavy_by_motion\":"+(heavyMotion?"true":"false")+",\"heavy_hold_ms\":"+std::to_string(heavyHold));
            // Value-only copy of the local action sequence for the logs; no full-record recording.
            if(!actionSequenceObserver::requested){actionSequenceObserver::requested=true;actionSequenceObserver::recording=false;}
            controllerUiFixture::observeMeleeNative=[](){
                actionSequenceObserver::Status a;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);a=actionSequenceObserver::latest;}
                const auto now=GetTickCount64();controllerUiFixture::MeleeNative out;
                out.valid=a.entity&&a.milliseconds&&now>=a.milliseconds&&now-a.milliseconds<=200;out.active=a.active;out.canPrimary=a.canPrimary;
                out.canBlock=a.canBlock;out.blocking=a.blocking;
                out.usedItem=a.usedItem;out.sequence=a.sequence;out.triggerTime=a.triggerTime;out.digital=a.digital;out.entity=a.entity;return out;};
            logFile->event("live_melee_enabled","\"swing_speed\":3,\"heavy_factor\":1.5,\"lever_m\":0.125,\"thrust_speed\":4,\"press_ms\":120,\"interval_ms\":400,\"native_button\":16384,\"direct_damage_writes\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-quick-slot-motion.txt")){
            std::ifstream qf(calibrationDirectory/"world-quick-slot-motion.txt");std::string magic;qf>>magic>>std::ws;
            if(!qf||!qf.eof()||magic!="EVR_QUICK_SLOT_MOTION_1"||!controllerUiFixture::liveRequested)throw std::runtime_error("quick slot by hand motion requires the live controller input");
            controllerUiFixture::quickSlotRequested=true;controllerUiFixture::liveMapper.motionQuickSlot(true);
            // A fallen player within 3 m: B holds the interaction (the revive) instead (2026-10-20, live_melee_targets.hpp).
            controllerUiFixture::fallenPlayerNear=[](uint64_t now){uint32_t who=0;double away=-1;return liveTargets::downedNear(3.,now,who,away);};
            logFile->event("quick_slot_motion_enabled","\"button\":\"right_secondary\",\"hold_ms\":250,\"select_m\":0.07,\"native_wheel\":true,\"right_grip_free\":true");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-live-guard.txt")){
            std::ifstream gf(calibrationDirectory/"world-live-guard.txt");std::string magic;gf>>magic>>std::ws;
            if(!gf||!gf.eof()||magic!="EVR_LIVE_GUARD_1"||!controllerUiFixture::liveRequested||controllerUiFixture::motionGuardRequested||controllerUiFixture::guardRequested)throw std::runtime_error("live guard requires the live controller input");
            controllerUiFixture::liveGuardRequested=true;parryObserver::install();
            // The grip rule (user 2026-10-14): the left grip held is the guard with a shield or a two-hand weapon; the press,
            // or the left controller swung fast, a parry. EVR_GUARD_GRIP=0 keeps the pose rule.
            {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUARD_GRIP",v,sizeof v);const bool grip=!(n==1&&v[0]=='0');
             evr::motion::LiveGuardSettings s;s.gripGuard=grip;
             {char w[16]{};const auto m=GetEnvironmentVariableA("EVR_SWING_PARRY_SPEED",w,sizeof w);if(m&&m<sizeof w){const float f=float(std::atof(w));if(f>=.5f&&f<=3.f)s.swingParrySpeed=f;}}   // 1.5: as before 2026-10-19
             controllerUiFixture::liveGuard=evr::motion::LiveGuard(s);
             logFile->event("live_guard_rule","\"grip\":"+std::string(grip?"true":"false")+",\"parry\":\"grip press or left hand over 1.5 m/s\",\"swing_parry_speed\":"+std::to_string(s.swingParrySpeed));}
            if(!actionSequenceObserver::requested){actionSequenceObserver::requested=true;actionSequenceObserver::recording=false;}
            if(!controllerUiFixture::observeMeleeNative)controllerUiFixture::observeMeleeNative=[](){
                actionSequenceObserver::Status a;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);a=actionSequenceObserver::latest;}
                const auto now=GetTickCount64();controllerUiFixture::MeleeNative out;
                out.valid=a.entity&&a.milliseconds&&now>=a.milliseconds&&now-a.milliseconds<=200;out.active=a.active;out.canPrimary=a.canPrimary;
                out.canBlock=a.canBlock;out.blocking=a.blocking;
                out.usedItem=a.usedItem;out.sequence=a.sequence;out.triggerTime=a.triggerTime;out.digital=a.digital;out.entity=a.entity;return out;};
            logFile->event("live_guard_enabled","\"shield_facing_dot\":0.5,\"raised_below_head_m\":0.55,\"ahead_m\":0.15,\"parry_speed\":1.5,\"parry_gap_ms\":70,\"native_button\":256,\"native_state_write\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-bow-spawn-observe.txt")){
            std::ifstream sf(calibrationDirectory/"world-bow-spawn-observe.txt");std::string magic;sf>>magic>>std::ws;
            if(!sf||!sf.eof()||magic!="EVR_BOW_SPAWN_OBSERVE_1"||!nativeGripObserver::requested)throw std::runtime_error("invalid spawn observer policy");
            bowSpawnObserver::install();
            if(std::filesystem::exists(calibrationDirectory/"world-ranged-survey.txt")){
                std::ifstream rf(calibrationDirectory/"world-ranged-survey.txt");std::string survey;rf>>survey>>std::ws;
                if(!rf||!rf.eof()||survey!="EVR_RANGED_SURVEY_1")throw std::runtime_error("invalid ranged survey policy");
                controllerUiFixture::rangedSurveyRequested=true;
                logFile->event("ranged_survey_enabled","\"left_primary_to_x\":true,\"crosshair_other_events\":true,\"read_only\":true");
            }
            if(std::filesystem::exists(calibrationDirectory/"world-ranged-aim.txt")){
                std::ifstream rf(calibrationDirectory/"world-ranged-aim.txt");std::string aim;rf>>aim>>std::ws;
                if(!rf||!rf.eof()||aim!="EVR_RANGED_AIM_1"||!motionVisual::rangedAnimationRequested)throw std::runtime_error("ranged aim requires the reviewed staff/wand display");
                controllerUiFixture::rangedAimRequested=true;
                logFile->event("ranged_aim_enabled","\"pointer_ray\":true,\"emitter_on_weapon_axis\":true,\"wand_trigger_to_attack\":true,\"items\":[1255239784,3851016100],\"native_power_write\":false");
            }
        }
        inputRanged::liveBowWatch=nativeGripObserver::requested&&motionVisual::liveEquipment();
        if(std::filesystem::exists(calibrationDirectory/"world-body-heading.txt")){
            // VHVR: the body follows the headset's yaw. Real-device feedback 2026-10-11: walking backward, the model
            // faced the way it walked and swung there. The local actor's look selection takes the camera (the head).
            std::ifstream bf(calibrationDirectory/"world-body-heading.txt");std::string magic;bf>>magic>>std::ws;
            if(!bf||!bf.eof()||magic!="EVR_BODY_HEADING_1"||!inputHeadIdle::vrLocomotion)throw std::runtime_error("body heading requires the live VR locomotion input");
            bodyHeading::requested=true;
            logFile->event("body_heading_enabled","\"look_mode_presented\":3,\"cameras\":[4,5,6,19,20,38],\"query_slot\":7,\"component_written\":false");
        }
        // While the live guard holds the block, the character looks where the head looks (2026-10-14, user: "the guard seems
        // to face where I moved - it holds when I had walked toward the enemy"; Mock game-20261014-214601: all through the
        // block the actor's look stayed where the body had last turned, 0 degrees, while the camera looked at 127). The game
        // blocks toward the actor's look; bodyHeading presents look mode 3 (the camera, which is the head's) only while
        // the block button is held - walking keeps the game's own modes (always on, it broke walking: 10-11 15:00).
        // EVR_GUARD_HEADING=0 leaves the look to the game.
        if(controllerUiFixture::liveGuardRequested&&!bodyHeading::requested&&inputHeadIdle::vrLocomotion){
            char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUARD_HEADING",v,sizeof v);
            if(!(n==1&&v[0]=='0')){bodyHeading::requested=true;bodyHeading::guardOnly=true;
                logFile->event("guard_heading_enabled","\"look_mode_presented\":3,\"only_while_the_block_is_held\":true,\"cameras\":[4,5,6,19,20,38]");}
        }
        if(std::filesystem::exists(calibrationDirectory/"world-ui-auto-hide.txt")){
            // User, 2026-10-12: the HUD hides (fading) five seconds after health, mana or another displayed thing last
            // changed; the game's use / pick-up pins stay. The layer does the look and the fade (hud_fade.hpp).
            std::ifstream uf(calibrationDirectory/"world-ui-auto-hide.txt");std::string magic;uf>>magic>>std::ws;
            if(!uf||!uf.eof()||magic!="EVR_UI_AUTO_HIDE_1"||(!controllerUiFixture::liveRequested&&!game::padStyle.load()))throw std::runtime_error("UI auto-hide requires the live controller input or a pad style");
            // The settings' [Picture] UiAutoHide=0 (the loader's EVR_UI_AUTO_HIDE=0; 2026-10-23): the HUD always shown.
            char hide[8]{};const auto hn=GetEnvironmentVariableA("EVR_UI_AUTO_HIDE",hide,sizeof hide);
            if(hn==1&&hide[0]=='0')logFile->event("ui_auto_hide_off","\"reason\":\"[Picture] UiAutoHide=0\"");else{
            uiHudBridge::requested=true;
            logFile->event("ui_auto_hide_enabled","\"hold_ms\":5000,\"fade_out_ms\":800,\"pins_always_shown\":[[420,84,1300,972],[0,280,1480,900]],\"compass_ignored\":true,\"ticking_numbers_ignored\":true");}
        }
        if(std::filesystem::exists(calibrationDirectory/"world-menu-pointer.txt")){
            // VHVR's pointer on the menus (menu_pointer.hpp): the right hand's ray is the mouse while a menu is open.
            std::ifstream mf(calibrationDirectory/"world-menu-pointer.txt");std::string magic;mf>>magic>>std::ws;
            if(!mf||!mf.eof()||magic!="EVR_MENU_POINTER_1"||!controllerUiFixture::liveRequested)throw std::runtime_error("the menu pointer requires the live controller input");
            menuPointer::requested=true;
        }
        if(std::filesystem::exists(calibrationDirectory/"world-live-held-items.txt")){
            // Real-device feedback 2026-10-11: the shield and the tools were not in the hands. Tools (item slot 3) join
            // the shown hand item and the swing; every shield of the model catalog takes the measured shield mount.
            std::ifstream hf(calibrationDirectory/"world-live-held-items.txt");std::string magic,word;bool follow=false,known=true;hf>>magic;
            while(hf>>word){if(word=="follow"&&!follow)follow=true;else known=false;}
            if(!hf.eof()||!known||magic!="EVR_LIVE_HELD_ITEMS_1"||!motionVisual::requested||!attachmentObserver::requested)throw std::runtime_error("live held items require the tracked weapon display");
            attachmentObserver::liveTools=true;motionShieldVisual::catalogShields=true;attachmentObserver::followRequested=follow;motionOffhandVisual::requested=true;motionVisual::offhandStrike=&motionOffhandVisual::strike;
            controllerUiFixture::handItem=&liveActionbars::hand;controllerUiFixture::liveHandUseRequested=true;
            motionFist::requested=true;motionEventSelection::bareHands=&motionFist::shown;
            // Placing by hand (VHVR): building mode and building pieces / tools aim with the right hand's pointer.
            inputHeadIdle::pointerAimRequested=true;buildingCursorRay::requested=true;
            // The fishing rod's cast flies as far as the swing was strong (VHVR; fishing_charge.hpp).
            fishingCharge::requested=true;
            // The HUD's compass looks where the head looks (compass_head.hpp; headset run 20261014-124515).
            compassHead::requested=true;
            // Everything of the category aims with the right hand's pointer - the tools that are swung (pickaxe, rake) too:
            // user decision 2026-10-12 ("the swung tools aim with the direction of the right hand"). The headset's ray for
            // them (mode 3, tried the same morning) stays available in pointer_aim_camera.hpp but is not used.
            inputHeadIdle::heldPiece=[](uint64_t now)->uint8_t{const auto held=liveActionbars::hand(now);return held.valid&&held.item&&held.category==6?uint8_t(1):uint8_t(0);};
            // The holsters (user, 2026-10-13): the grips at the shoulders and at the left hip. EVR_HOLSTERS=0 leaves them out.
            {char off[8]{};const auto m=GetEnvironmentVariableA("EVR_HOLSTERS",off,sizeof off);
             if(!(m&&m<sizeof off&&off[0]=='0')&&inputHeadIdle::requested&&inputShadow::requested&&controllerUiFixture::liveRequested){
                {std::lock_guard<std::mutex> lock(holsterInput::mutex);holsterInput::sender.seed(uint32_t(std::time(nullptr)));}
                holsterInput::requested=true;controllerUiFixture::holsterAct=&holsters::act;controllerUiFixture::holsterTick=&holsters::tick;controllerUiFixture::holsterRequested=true;
                // The consumable held from the quick wheel (input_head_idle.hpp consumableHoldInput): it empties the hand and brings the
                // slot back through the holsters' input, so it is on with them.
                consumableHoldInput::consumableAt=&liveActionbars::consumableAt;consumableHoldInput::selectedSlot=&liveActionbars::selectedSlot;
                // Its model in the right hand (motion_consumable_visual.hpp): with the tracked hands drawn. EVR_CONSUMABLE_MODEL=0: off.
                motionConsumableVisual::requested=motionConsumableVisual::wanted()&&motionVisual::requested&&motionHandVisual::requested;
                // The equipment's bow not on the action bar is shown by the mod (motion_mod_bow_visual.hpp), with the draw route. EVR_MOD_BOW=0: off.
                motionModBow::requested=holsters::modBowWanted()&&motionVisual::requested&&motionHandVisual::requested&&motionVisual::rangedAnimationRequested&&controllerUiFixture::bowDrawRequested;
                holsters::modBowEnabled=motionModBow::requested;
                itemGallery::configure();
                // keen::actor::StateFlag FloatingOnWater (43) / UnderWater (44) of the local actor within the last half second.
                controllerUiFixture::actorSwimming=[](uint64_t now){std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);const auto& s=inputHeadIdle::latest;
                    return s.actorKnown&&s.milliseconds&&now>=s.milliseconds&&now-s.milliseconds<=500&&(s.actorState&game::swimmingStates)!=0;};
                controllerUiFixture::actorWater=[](uint64_t now)->unsigned{std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);const auto& s=inputHeadIdle::latest;
                    return s.actorKnown&&s.milliseconds&&now>=s.milliseconds&&now-s.milliseconds<=500?unsigned((s.actorState>>43)&3):0u;};
                controllerUiFixture::handItem=&holsters::hand;
                const motion::HolsterZones z;
                std::ostringstream o;o<<"\"left_shoulder\":\"ranged_weapon_of_the_equipment\",\"right_shoulder\":\"weapon_in_the_hand\",\"left_hip\":\"shield\",\"shoulder_height\":["<<z.shoulderLow<<','<<z.shoulderHigh
                    <<"],\"hip_height\":["<<z.hipLow<<','<<z.hipHigh<<"],\"side_metres\":"<<z.side<<",\"shoulder_not_ahead_of\":"<<z.shoulderAhead<<",\"hip_behind\":"<<z.hipAhead<<",\"hip_slope\":"<<z.hipSlope
                    <<",\"vibration\":[0.3,200],\"bare_hands_selection_sent\":32,\"ranged_combat_bit\":50,\"shield_equipment_slot\":15,\"off_while_swimming\":true";
                logFile->event("holsters_enabled",o.str());}}
            // Sneaking by crouching and the view reset pose (user, 2026-10-18 morning; include/evr/room_sneak.hpp). The
            // runtime's own view reset (SteamVR's or Meta's system button held) is taken in beginLiveFrame either way.
            // EVR_ROOM_SNEAK=0 / EVR_RECENTER_POSE=0 leave them out.
            {auto on=[](const char* name){char v[8]{};const auto m=GetEnvironmentVariableA(name,v,sizeof v);return !(m&&m<sizeof v&&v[0]=='0');};
             if(inputHeadIdle::requested&&controllerUiFixture::liveRequested){
                // Real crouching can be switched on in the game's start menu (VR settings), so it is always there; the
                // settings file's [Body] (EVR_ROOM_SNEAK=0 off, EVR_CROUCH_DROP percent) says how it begins.
                controllerUiFixture::roomSneakRequested=true;controllerUiFixture::roomSneakOn=on("EVR_ROOM_SNEAK");controllerUiFixture::recenterPoseRequested=on("EVR_RECENTER_POSE");
                controllerUiFixture::holdTurn=[](uint64_t until){cameraControl::turnHeldUntil=until;};   // the fish's pull from the rod's tip is no turn
                {char v[8]{};const auto m=GetEnvironmentVariableA("EVR_CROUCH_DROP",v,sizeof v);if(m&&m<sizeof v){const int d=std::atoi(v);if(d>=evr::vrpanel::dropLow&&d<=evr::vrpanel::dropHigh)controllerUiFixture::crouchDrop=d;}}
                // The stroke's speed and swimming by the arms (2026-10-20; [Melee] SwingSpeed, [Body] ArmSwim), changeable there too.
                {char v[8]{};const auto m=GetEnvironmentVariableA("EVR_SWING_SPEED",v,sizeof v);if(m&&m<sizeof v){const int d=std::atoi(v);if(d>=evr::settings::swingSpeedLow&&d<=evr::settings::swingSpeedHigh)controllerUiFixture::swingPercent=evr::settings::clampSwingSpeed(d);}}
                controllerUiFixture::armSwimOn=on("EVR_ARM_SWIM");controllerUiFixture::hitStopOn=on("EVR_HIT_STOP");   // [Melee] HitStop (17:40)
                // keen::actor::StateFlag InSneakMode (21); no sneak by crouching while Dead 7, Sitting 9, Sleeping 11, Climbing 34,
                // ClimbingTransition 35, Attached 36, HangGliding 37, AttachedToHook 38, FloatingOnWater 43, UnderWater 44.
                // Sneak is keen::ecs::PlayerInputType 55 in 1.0: its toggleConfig bit says whether a press switches it.
                controllerUiFixture::sneakState=[](uint64_t now,bool& sneaking,bool& toggle,bool& allowed){
                    inputHeadIdle::State s;{std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);s=inputHeadIdle::latest;}
                    if(!(s.actorKnown&&s.milliseconds&&now>=s.milliseconds&&now-s.milliseconds<=500))return false;
                    constexpr uint64_t blocked=(1ull<<7)|(1ull<<9)|(1ull<<11)|(1ull<<34)|(1ull<<35)|(1ull<<36)|(1ull<<37)|(1ull<<38)|(1ull<<43)|(1ull<<44);
                    sneaking=(s.actorState>>21)&1;allowed=(s.actorState&blocked)==0;
                    toggle=!inputHeadIdle::toggleConfigKnown.load()||((inputHeadIdle::toggleConfig.load()>>55)&1);
                    return true;};
                if(controllerUiFixture::sneakView::dropM>0)rootCamera::viewDrop=[]{return controllerUiFixture::sneakView::current();};
                motion::RoomCrouchSettings c;c.enter=evr::vrpanel::enterRatio(controllerUiFixture::crouchDrop.load());const motion::RecenterPoseSettings r;
                std::ostringstream o;o<<std::setprecision(4)<<"\"room_sneak\":"<<(controllerUiFixture::roomSneakOn.load()?"true":"false")<<",\"crouch_drop_percent\":"<<controllerUiFixture::crouchDrop.load()
                    <<",\"swing_speed_percent\":"<<controllerUiFixture::swingPercent.load()<<",\"arm_swim\":"<<(controllerUiFixture::armSwimOn.load()?"true":"false")<<",\"hit_stop\":"<<(controllerUiFixture::hitStopOn.load()?"true":"false")<<",\"changeable_in_start_menu\":true,\"enter_ratio\":"<<c.enter<<",\"leave_ratio\":"<<c.enter*c.leave
                    <<",\"sneak_key\":\"L3\",\"recenter_pose\":"<<(controllerUiFixture::recenterPoseRequested?"true":"false")<<",\"pose_hands\":["<<r.side<<','<<r.ahead<<','<<r.radius<<"],\"pose_ms\":"<<r.hold
                    <<",\"runtime_reset\":true";
                logFile->event("room_sneak_enabled",o.str());}}
            if(follow)logFile->event("held_follow_enabled","\"attachment_slots\":[8,9],\"bow\":false,\"writes\":\"child_world_transform_after_attachment_system\",\"slot_attachment_unchanged\":true");
            logFile->event("live_held_items_enabled","\"tools_item_slots\":[2,3],\"swung_tools_attachment_slot\":9,\"tool_mount\":\"left_slot_group_mount\",\"catalog_shields\":"+std::to_string(motion::catalogShieldAssets.size())+",\"shield_shown\":"+(motionShieldVisual::requested?"true":"false"));
        }
        if(std::filesystem::exists(calibrationDirectory/"world-live-melee-contact.txt")){
            // Unlimited play (2026-10-11 real-device feedback: hit with the weapon in the hand). The attack collider of
            // the local player's own shown weapon takes the tracked weapon's transform at the native query boundary.
            // Optional words: "early" hands the attack's Impact to the game on the action's first tick (the hit volume
            // exists while the real swing is still under way); "observe" adds the read-only physics-query and hit
            // observers as Mock evidence; "still" (with early) withholds the attack's animation, turn and step, so that
            // a swing does not move the character.
            std::ifstream cf(calibrationDirectory/"world-live-melee-contact.txt");std::string magic,word;bool early=false,observe=false,still=false,known=true;cf>>magic;
            while(cf>>word){if(word=="early"&&!early)early=true;else if(word=="observe"&&!observe)observe=true;else if(word=="still"&&!still)still=true;else known=false;}
            const std::string mode=observe?"observe":"";
            if(controllerUiFixture::liveMeleeTargetGate&&!early)throw std::runtime_error("the melee target gate requires the early Impact (its hook keeps the list of targets)");
            if(magic!="EVR_LIVE_MELEE_CONTACT_1"||!known||(still&&!early)||!controllerUiFixture::liveMeleeRequested||!motionVisual::requested||motionEventSelection::requested||
               controllerUiFixture::motionAttackRequested||sequenceColliderObserver::requested||motionColliderChannel::requested)
                throw std::runtime_error("live melee contact requires live melee with the tracked weapon display, without the finite contact trials");
            nativeImpactComponents::requested=true;sequenceColliderObserver::requested=true;impactHierarchyObserver::requested=true;
            motionColliderChannel::requested=motionColliderChannel::live=true;motionColliderApply::requested=motionColliderApply::live=true;
            if(mode=="observe"){motionContactObserver::install();motionHitObserver::install();}
            if(early){controllerUiFixture::liveMeleeQuickRelease=true;motionEventSelection::requested=true;motionEventSelection::liveRetime=true;motionEventSelection::livePress=&controllerUiFixture::livePressSnapshot;motionEventSelection::liveStill=still;motionEventSelection::callerCensus=observe;motionEventSelection::hideAmmunition=controllerUiFixture::bowDrawRequested&&!motionArrowVisual::requested;motionEventSelection::install();meleeTriggerObserver::install();opportunityTakeObserver::install();sequenceRequestObserver::install();rangedSpecialProbe::install();controllerUiFixture::clearAirborneOpportunities=&motionEventSelection::shapeAirborneOpportunities;}
            logFile->event("live_melee_contact_enabled",std::string("\"collider_follows_tracked_weapon\":true,\"either_hand_slot\":true,\"grip_button_required\":false,\"cancellation\":false,\"ledger_entries\":16,\"observers\":")+(mode=="observe"?"true":"false")+",\"early_impact\":"+(early?"true":"false")+",\"attack_moves_character\":"+(still?"false":"true")+",\"ecs_written_by_mod\":false");
        }
        if(inputRanged::liveBowWatch)logFile->event("native_bow_survey_enabled","\"ranged_read_only\":true,\"maximum_samples\":2048,\"all_local_weapon_states\":true");
        if(std::filesystem::exists(calibrationDirectory/"world-motion-contact-observe.txt")){
            std::ifstream policy(calibrationDirectory/"world-motion-contact-observe.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_MOTION_CONTACT_OBSERVE_1"||!controllerUiFixture::motionAttackRequested)throw std::runtime_error("contact observation requires explicit one-swing diagnostic");
            nativeImpactComponents::requested=true;sequenceColliderObserver::requested=true;impactHierarchyObserver::requested=true;
            motionContactObserver::install();motionHitObserver::install();
        }
        if(std::filesystem::exists(calibrationDirectory/"world-motion-collider-shadow.txt")){
            std::ifstream policy(calibrationDirectory/"world-motion-collider-shadow.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_MOTION_COLLIDER_SHADOW_1"||!sequenceColliderObserver::requested||!motionVisual::requested||!controllerUiFixture::motionAttackRequested)throw std::runtime_error("collider shadow requires tracked one-swing contact diagnostic");
            motionColliderChannel::requested=true;
            logFile->event("motion_collider_shadow_enabled","\"applied\":false,\"ecs_written\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-motion-collider-apply.txt")){
            std::ifstream policy(calibrationDirectory/"world-motion-collider-apply.txt");std::string magic;policy>>magic>>std::ws;
            if(!policy||!policy.eof()||magic!="EVR_MOTION_COLLIDER_APPLY_1"||!motionColliderChannel::requested||sequenceColliderIdentity::requested)throw std::runtime_error("tracked collider requires validated explicit shadow geometry trial");
            if(std::filesystem::exists(calibrationDirectory/"world-motion-collider-cancel-after.txt")){
                std::ifstream cancel(calibrationDirectory/"world-motion-collider-cancel-after.txt");unsigned count=0;
                if(!(cancel>>count)||count<1||count>10)throw std::runtime_error("invalid bounded collider cancellation fixture");
                cancel>>std::ws;if(!cancel.eof())throw std::runtime_error("trailing cancellation fixture data");motionColliderApply::cancelAfter=count;
            }
            motionColliderApply::requested=true;motionColliderApply::install();logFile->event("motion_collider_apply_enabled",std::string("\"one_swing_only\":")+(controllerUiFixture::motionAttackLimit==1?"true":"false")+",\"expected_attacks\":"+std::to_string(controllerUiFixture::motionAttackLimit)+",\"native_hit_window_retained\":"+(std::filesystem::exists(calibrationDirectory/"world-motion-event-retime.txt")?"false":"true")+",\"ecs_written_by_mod\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-contact-lifetimes.txt")){
            std::ifstream f(calibrationDirectory/"world-contact-lifetimes.txt");std::string magic;f>>magic>>std::ws;
            if(!f||!f.eof()||magic!="EVR_CONTACT_LIFETIMES_1"||!controllerUiFixture::meleeSession||controllerUiFixture::motionAttackLimit!=2||!motionColliderApply::requested||!std::filesystem::exists(calibrationDirectory/"world-melee-session-contact.txt"))throw std::runtime_error("lifetime observation requires finite measured contact trial");
            contactLifetimeObserver::install();
            if(std::filesystem::exists(calibrationDirectory/"world-contact-quiescence.txt")){
                std::ifstream q(calibrationDirectory/"world-contact-quiescence.txt");std::string qm;q>>qm>>std::ws;
                if(!q||!q.eof()||qm!="EVR_CONTACT_QUIESCENCE_1")throw std::runtime_error("invalid read-only quiescence policy");
                if(std::filesystem::exists(calibrationDirectory/"world-contact-recycle.txt")){
                    std::ifstream c(calibrationDirectory/"world-contact-recycle.txt");std::string cm;unsigned limit=0;c>>cm>>limit>>std::ws;
                    if(!c||!c.eof()||cm!="EVR_CONTACT_RECYCLE_1"||limit!=1)throw std::runtime_error("invalid finite one-slot retirement policy");
                    contactQuiescenceObserver::configureRetirement();
                }
                contactQuiescenceObserver::install();
            }
        }
        if(std::filesystem::exists(calibrationDirectory/"world-contact-log-budget.txt")){
            std::ifstream f(calibrationDirectory/"world-contact-log-budget.txt");std::string magic;unsigned events=0,ready=0;f>>magic>>events>>ready>>std::ws;
            if(!f||!f.eof()||magic!="EVR_CONTACT_LOG_BUDGET_1"||events!=3||ready!=2||!controllerUiFixture::meleeSession||controllerUiFixture::motionAttackLimit!=2||!motionColliderApply::requested||!std::filesystem::exists(calibrationDirectory/"world-melee-session-contact.txt")||!std::filesystem::exists(calibrationDirectory/"world-motion-event-retime.txt"))throw std::runtime_error("recording budget witness requires bounded measured contact");
            motionEventSelection::recordingBudget.configure(events,true);sequenceColliderObserver::readyRecordingBudget.configure(ready,true);
            logFile->event("contact_recording_budget_enabled","\"event_limit\":3,\"ready_limit\":2,\"test_witness_stream\":true,\"processing_unlimited_by_recording\":true");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-motion-event-retime.txt")){
            std::ifstream f(calibrationDirectory/"world-motion-event-retime.txt");std::string magic;f>>magic>>std::ws;
            if(!f||!f.eof()||magic!="EVR_MOTION_EVENT_RETIME_1"||!motionColliderApply::requested||!controllerUiFixture::motionAttackRequested)throw std::runtime_error("early Impact requires tracked single-swing collider");
            if(std::filesystem::exists(calibrationDirectory/"world-motion-tracking-loss.txt")){
                std::ifstream loss(calibrationDirectory/"world-motion-tracking-loss.txt");std::string lm;loss>>lm>>std::ws;
                if(!loss||!loss.eof()||lm!="EVR_MOTION_TRACKING_LOSS_1")throw std::runtime_error("invalid tracking loss diagnostic");
                controllerUiFixture::trackingLossExpected=true;
                logFile->event("motion_tracking_loss_expected","\"runtime_tracking_flags\":true,\"no_forced_collider_counter\":true");
            }
            if(std::filesystem::exists(calibrationDirectory/"world-motion-offhand-release.txt")){
                std::ifstream loss(calibrationDirectory/"world-motion-offhand-release.txt");std::string lm;loss>>lm>>std::ws;
                if(!loss||!loss.eof()||lm!="EVR_MOTION_OFFHAND_RELEASE_1"||!controllerUiFixture::twoHandAttackRequested||controllerUiFixture::trackingLossExpected||motionColliderApply::cancelAfter)throw std::runtime_error("invalid offhand release diagnostic");
                controllerUiFixture::offhandReleaseExpected=true;
                logFile->event("motion_offhand_release_expected","\"runtime_grab_action\":true,\"no_forced_collider_counter\":true");
            }
            motionEventSelection::requested=motionEventSelection::shadowRequested=motionEventSelection::retimeRequested=true;motionColliderChannel::requireAuthority=true;motionEventSelection::install();
            if(std::filesystem::exists(calibrationDirectory/"world-blocked-event-recycle.txt")){
                std::ifstream b(calibrationDirectory/"world-blocked-event-recycle.txt");std::string bm;b>>bm>>std::ws;
                if(!b||!b.eof()||bm!="EVR_BLOCKED_EVENT_RECYCLE_1"||!controllerUiFixture::offhandReleaseExpected||controllerUiFixture::motionAttackLimit!=1||controllerUiFixture::meleeSession||contactQuiescenceObserver::enabled)throw std::runtime_error("blocked event recycling requires isolated pre-Impact offhand release trial");
                contactQuiescenceObserver::configureBlockedRetirement();contactQuiescenceObserver::install();
            }
            if(std::filesystem::exists(calibrationDirectory/"world-cancel-contact-recycle.txt")){
                std::ifstream c(calibrationDirectory/"world-cancel-contact-recycle.txt");std::string cm;c>>cm>>std::ws;
                if(!c||!c.eof()||cm!="EVR_CANCEL_CONTACT_RECYCLE_1"||!controllerUiFixture::offhandReleaseExpected||controllerUiFixture::motionAttackLimit!=1||controllerUiFixture::meleeSession||contactQuiescenceObserver::enabled||contactLifetimeObserver::enabled||motionColliderApply::cancelAfter||!motionColliderApply::requested)throw std::runtime_error("cancel contact retirement requires isolated post-Impact offhand trial");
                contactQuiescenceObserver::configureRetirement(contactQuiescenceObserver::RetirementTrial::PostImpactCancel);
                contactLifetimeObserver::install();contactQuiescenceObserver::install();
            }
        }
        if(std::filesystem::exists(calibrationDirectory/"world-combat-epoch-shadow.txt")){
            if(!controllerUiFixture::meleeSession||!contactQuiescenceObserver::retirementRequested||!motionEventSelection::retimeRequested)throw std::runtime_error("combat epoch shadow requires calibrated session/contact retirement trial");
            combatWorldBridge::install(calibrationDirectory);
        }
        if(std::filesystem::exists(calibrationDirectory/"world-combat-epoch-strict.txt")){
            const bool epochCancel=controllerUiFixture::offhandReleaseExpected&&controllerUiFixture::motionAttackLimit==1&&!controllerUiFixture::meleeSession&&std::filesystem::exists(calibrationDirectory/"world-cancel-contact-recycle.txt");
            if(combatWorldBridge::enabled||(!controllerUiFixture::meleeSession&&!epochCancel)||!contactQuiescenceObserver::retirementRequested||!motionEventSelection::retimeRequested)throw std::runtime_error("strict epochs require measured repeat or post-Impact cancellation trial");
            combatEpochGate::install(calibrationDirectory);
            controllerUiFixture::epochAllowsPoll=[](){try{return combatEpochGate::newAllowed(combatEpochGate::Stage::Poll);}catch(...){return false;}};
            controllerUiFixture::epochAllowsTicket=[](uint64_t world){return combatEpochGate::newAllowed(combatEpochGate::Stage::Ticket,world);};
            controllerUiFixture::epochContextFields=&combatEpochGate::contextFields;
            controllerUiFixture::observeEpoch=&combatEpochGate::inputState;
            if(std::filesystem::exists(calibrationDirectory/"world-combat-epoch-cancel.txt")){
                std::ifstream f(calibrationDirectory/"world-combat-epoch-cancel.txt");std::string magic;f>>magic>>std::ws;
                if(!f||!f.eof()||magic!="EVR_COMBAT_EPOCH_CANCEL_1"||!controllerUiFixture::meleeSession||controllerUiFixture::motionAttackLimit!=2||controllerUiFixture::offhandReleaseExpected||controllerUiFixture::trackingLossExpected)throw std::runtime_error("epoch cancellation requires isolated repeat coordinator trial");
                combatEpochGate::cancelTrial=true;controllerUiFixture::epochCloseExpected=true;contactQuiescenceObserver::expectedActions=1;
                logFile->event("combat_epoch_cancel_trial_enabled","\"expected_actions\":1,\"late_swing_still_present\":true,\"native_world_destroyed\":false");
            }
        }
        if(std::filesystem::exists(calibrationDirectory/"world-contact-end-retire.txt")){
            std::ifstream f(calibrationDirectory/"world-contact-end-retire.txt");std::string magic;f>>magic>>std::ws;
            if(!f||!f.eof()||magic!="EVR_CONTACT_END_RETIRE_1"||(activeXrExitTrial&&!activeContactOwnerTrial)||!combatEpochGate::cancelTrial||!contactQuiescenceObserver::retirementRequested||!contactLifetimeObserver::enabled||!motionColliderApply::requested||calibrationFrames!=(activeContactOwnerTrial?3000u:600u))throw std::runtime_error("terminal contact bridge requires explicit finite admission-cancel trial");
            if(std::filesystem::exists(calibrationDirectory/"world-interrupted-actor-owner.txt")){
                std::ifstream q(calibrationDirectory/"world-interrupted-actor-owner.txt");std::string magic;q>>magic;q>>std::ws;
                if(!q||!q.eof()||magic!="EVR_INTERRUPTED_ACTOR_OWNER_1"||!activeContactOwnerTrial)throw std::runtime_error("interrupted Actor trial requires explicit active contact owner");
                contactQuiescenceObserver::interruptedRequested=true;contactLifetimeObserver::actorReturnObserver=&contactQuiescenceObserver::observeInterruptedActorReturn;
            }
            contactQuiescenceObserver::terminalRequested=true;contactLifetimeObserver::actorOwnerRequested=true;contactLifetimeObserver::actorBoundaryObserver=&contactQuiescenceObserver::observeActorOwnerBoundary;
            logFile->event("contact_terminal_trial_enabled","\"actor_boundary_observation\":true,\"interrupted_actor_trial\":"+std::string(contactQuiescenceObserver::interruptedRequested?"true":"false")+",\"expected_actions\":1,\"active_renderer\":"+std::string(activeContactOwnerTrial?"true":"false")+",\"pairs_before_exit\":"+std::to_string(calibrationFrames)+",\"native_world_exit_required\":true,\"hold_after_normal_idle\":"+std::string(contactQuiescenceObserver::interruptedRequested?"false":"true")+",\"active_attack_exit_claim\":false");
        }
        if(std::filesystem::exists(calibrationDirectory/"world-contact-owner-exit.txt")){
            std::ifstream f(calibrationDirectory/"world-contact-owner-exit.txt");std::string magic;f>>magic>>std::ws;
            if(!f||!f.eof()||magic!="EVR_CONTACT_OWNER_EXIT_1"||!contactQuiescenceObserver::terminalRequested||!controllerUiFixture::motionAttackRequested||controllerUiFixture::installed.load()||(activeXrExitTrial&&!activeContactOwnerTrial))throw std::runtime_error("contact owner exit requires explicit completed-action terminal bridge before input installation");
            contactQuiescenceObserver::ownerHandoffRequested=true;
            controllerUiFixture::backendExitRequested=controllerUiFixture::holdConnectionOnEpochStop=true;
            controllerBackendExit::joinedFinalizer=[](){
                // The release/acquire readiness flag publishes the worker's
                // completed teardown facts; no World/Actor/backend dereference.
                if(!terminalOwnerReady.load(std::memory_order_acquire)||resultCode.load()!=0||(activeContactOwnerTrial&&(!activeXrExitOwnerReturned.load()||!done.load()))||retainedPair.load()||contactQuiescenceObserver::terminalRetired!=1||combatEpochGate::initialized||inputUi::enabled||inputUi::callbacks.load()||controllerUiFixture::installed.load()||!controllerUiFixture::backendExitReturned.load())return false;
                logFile->event("contact_owner_exit_complete","\"result\":0,\"terminal_retired\":1,\"scene_disposed\":true,\"world_leases_released\":true,\"input_owner_returned\":true,\"synchronous_owner_thread\":true,\"thread\":"+std::to_string(GetCurrentThreadId())+",\"reads_destroyed_memory\":false,\"active_attack_exit_claim\":false");
                logFile->event("view_resource_hook_complete","\"original_bytes_restored\":true,\"trampoline_retained\":true,\"result\":"+std::to_string(resultCode.load())+",\"native_input_owner_thread\":true");
                return resultCode.load()==0;
            };
            controllerBackendExit::install();
            logFile->event("contact_owner_exit_enabled","\"completed_action_only\":"+std::string(contactQuiescenceObserver::interruptedRequested?"false":"true")+",\"interrupted_actor_trial\":"+(contactQuiescenceObserver::interruptedRequested?"true":"false")+",\"active_renderer\":"+std::string(activeContactOwnerTrial?"true":"false")+",\"pairs_before_exit\":"+std::to_string(calibrationFrames)+",\"hold_neutral_until_native_owner\":true,\"active_attack_exit_claim\":false");
        }
        if(activeContactOwnerTrial&&(!contactQuiescenceObserver::ownerHandoffRequested||!controllerUiFixture::motionAttackRequested))throw std::runtime_error("active contact exit missing explicit input owner bridge");
        if(activeXrExitTrial&&controllerUiFixture::requested&&!activeContactOwnerTrial)throw std::runtime_error("initial active XR exit trial excludes controller/combat fixtures");
        if(inputDirection::requested)inputDirection::install();
        // A pad style's host parts (as onlineFlat::installHostParts): the event selection (the guests' strokes), the parry and
        // the shots' spawn; each optional.
        // The event selection also offline (2026-10-25): its ticks keep the creatures for the locked target's bar (liveTargets).
        if(game::padStyle.load()){
            const auto part=[](const char* name,void(*install)()){try{install();}catch(const std::exception& e){logFile->event("optional_hook_skipped","\"hook\":"+jsonString(name)+",\"reason\":"+jsonString(e.what()));}};
            motionEventSelection::hostOnly=true;part("motion_event_selection",[]{motionEventSelection::install();});
            if(onlineSync::requested){part("parry",[]{parryObserver::install();});part("bow_spawn",[]{bowSpawnObserver::install();});}
            logFile->event("pad_host_parts","\"online\":"+std::string(onlineSync::requested?"true":"false")+",\"event_selection\":"+(motionEventSelection::enabled?"true":"false")+
                ",\"parry\":"+(parryObserver::enabled?"true":"false"));}
        if(uiStateObserver::requested)uiStateObserver::install();
        if(equipmentObserver::requested)equipmentObserver::install();
        // 1.0: the readiness, shared-state, terrain and download diagnostics are read-only records; one whose check of the new
        // build fails is left out. The shared world stages (one eye updates the world) are not optional.
        if(worldNativeDiagnostic){optional("world_ready_diagnostic",[&]{worldReadyDiagnostic::install();},[&]{});
            optional("world_shared_diagnostic",[&]{worldSharedDiagnostic::install(calibrationDirectory);},[&]{});
            worldSharedStages::install();
            optional("terrain_ready_diagnostic",[&]{terrainReadyDiagnostic::install();},[&]{});
            optional("download_feedback_diagnostic",[&]{downloadFeedbackDiagnostic::install();},[&]{});}
        if(runtimeEyes()){
            const auto bytes=parsePattern("48 89 5C 24 10 57 48 83 EC 30 0F 29 74 24 20 48 8B FA 0F 28 F2 48 8B D9");
            const uint8_t call[]{0xe8,0x23,0xd3,0xfe,0xff};projectionTarget=base+0x118270;   // build 26082781: call 0x118270 at 0x12af48 (1.0: e8 43 d3 fe ff, call 0x1180f0 at 0x12ada8)
            fogProjection::verify();
            if(scan(base+0x118270,bytes.size(),bytes)!=std::vector<size_t>{0}||std::memcmp(base+0x12af4d-5,call,5))throw std::runtime_error("eye projection signature changed");
            require(MH_CreateHook(projectionTarget,reinterpret_cast<void*>(&convertEyeProjection),reinterpret_cast<void**>(&originalProjection)),"MH_CreateHook eye projection");
            require(MH_EnableHook(projectionTarget),"MH_EnableHook eye projection");projectionEnabled=true;
        }
        if(sceneCleanup){
            const auto bytes=parsePattern("40 53 48 83 EC 20 48 8B 19 48 85 DB 0F 84 4D 01");destroyOwnerTarget=base+0xf39d0;
            if(scan(base+0xf39d0,bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("Scene owner destruction signature changed");
            require(MH_CreateHook(destroyOwnerTarget,reinterpret_cast<void*>(&destroyOwner),reinterpret_cast<void**>(&originalDestroyOwner)),"MH_CreateHook scene lifecycle");
            require(MH_EnableHook(destroyOwnerTarget),"MH_EnableHook scene lifecycle");destroyOwnerEnabled=true;
            logFile->event("calibration_scene_hook_enabled","\"rva\":\"0xf39d0\",\"pattern\":\"40 53 48 83 EC 20 48 8B 19 48 85 DB\"");
        }
        if(dlssEval::requested||((dlssEval::depthNotes||dlssEval::resetRightEvery>0)&&liveXr))dlssEval::install();
        active=true; require(MH_EnableHook(target),"MH_EnableHook");enabled=true;renderWasEnabled=true;
        logFile->event("view_resource_hook_enabled","\"mode\":"+jsonString(detailFactoryProbe?"unused_detail_factory":renderCalibration?"native_retained_calibration":"unused_view_lifecycle_only")+",\"rva\":\"0x129fc0\"");
        const unsigned budgetSeconds=worldSession?(sessionMinutes?sessionMinutes*60:400u*86400u):worldEndurance?420:renderCalibration?60:preparedSessionProbe?40:10;
        logFile->event("view_resource_diagnostic_budget","\"seconds\":"+std::to_string(budgetSeconds)+",\"frames\":"+std::to_string(calibrationFrames)+",\"endurance\":"+(worldEndurance?"true":"false"));
        const auto sessionStart=std::chrono::steady_clock::now();
        const auto deadline=sessionStart+std::chrono::seconds(budgetSeconds)+std::chrono::seconds(worldSession?20:0);
        auto nextHeartbeat=sessionStart+std::chrono::seconds(30);uint64_t heartbeatPairs=0;
        while(!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now()<deadline){
            if(worldSession){
                const auto now=std::chrono::steady_clock::now();
                if(!sessionStop.load()&&(now>=sessionStart+std::chrono::seconds(budgetSeconds)||std::filesystem::exists(calibrationDirectory/"stop.request"))){
                    sessionStop=true;
                    logFile->event("session_stop_requested","\"reason\":"+jsonString(now>=sessionStart+std::chrono::seconds(budgetSeconds)?"minute_limit":"stop_file")+
                        ",\"elapsed_s\":"+std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now-sessionStart).count()));
                }
                if(now>=nextHeartbeat){
                    const auto* p=retainedPair.load(std::memory_order_acquire);const uint64_t pairs=p?p->pairedFrames:0;
                    logFile->event("session_heartbeat","\"elapsed_s\":"+std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now-sessionStart).count())+
                        ",\"pairs\":"+std::to_string(pairs)+",\"pairs_per_s\":"+std::to_string(double(pairs-heartbeatPairs)/30.0)+
                        ",\"failed\":"+(p&&p->failed?"true":"false")+","+logFile->suppressedSummary(8));
                    heartbeatPairs=pairs;nextHeartbeat=now+std::chrono::seconds(30);
                }
            }
            if(activeContactOwnerTrial&&!earlyInputStopped&&(contactQuiescenceObserver::interruptedRequested||contactQuiescenceObserver::terminalHeld.load())&&controllerUiFixture::epochStopped.load()){
                // The completed native action is already idle; close only input
                // consumers here. Native eye rendering/Scene ownership continues.
                inputDirection::disable(true,true);earlyInputStopped=true;
                logFile->event("active_contact_owner_exit_armed","\"input_readers_stopped\":true,\"completed_action_held\":"+std::string(contactQuiescenceObserver::terminalHeld.load()?"true":"false")+",\"interrupted_trial\":"+(contactQuiescenceObserver::interruptedRequested?"true":"false")+",\"render_still_active\":true,\"active_attack_exit_claim\":false,\"thread\":"+std::to_string(GetCurrentThreadId()));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const bool budgetExpired=!done.load(std::memory_order_acquire);
        // Give the desktop View its RenderScale back while the Scene (and wrapper) still exist.
        if(desktopScaleBefore>=0&&desktopScaleWrapper&&retainedPair.load(std::memory_order_acquire)){
            *reinterpret_cast<volatile float*>(desktopScaleWrapper+layout::ownerRenderScale)=desktopScaleBefore;
            logFile->event("desktop_render_scale_restored","\"render_scale\":"+std::to_string(desktopScaleBefore)+",\"writes\":"+std::to_string(desktopScaleWrites));
        }else if(desktopRenderScale>0)logFile->event("desktop_render_scale_not_restored","\"writes\":"+std::to_string(desktopScaleWrites)+",\"scene_retained\":"+(retainedPair.load()?"true":"false"));
        active=false;require(MH_DisableHook(target),"MH_DisableHook");enabled=false;recordRenderRemoval();
        if(!quiesceDependentHooks())return 4;
        if(budgetExpired)logFile->event("view_resource_budget_exhausted","\"seconds\":"+std::to_string(budgetSeconds)+",\"requested_pairs\":"+std::to_string(calibrationFrames)+",\"result\":"+std::to_string(resultCode.load()));
        // Finish finite input/render observers before inviting native exit.
        // Keep only consumed-event suppression and lifetime/fence hooks until
        // the exact server Ended proof. This avoids racing all hook teardown
        // against process termination and leaves MOD ledger ownership intact.
        if(!contactQuiescenceObserver::terminalRequested)motionEventSelection::disable();
        parryObserver::disable();targetMark::disable();meleeTriggerObserver::disable();opportunityTakeObserver::disable();sequenceRequestObserver::disable();rangedSpecialProbe::disable();steamLink::disableGameHooks();motionHitObserver::disable();motionContactObserver::disable();uiStateObserver::disable();bodyUpload::disable();headVisibility::disable();equipmentObserver::disable();if(earlyInputStopped)inputActor::disable();else inputDirection::disable(contactQuiescenceObserver::ownerHandoffRequested);
        if(!contactQuiescenceObserver::terminalRequested){contactQuiescenceObserver::disable();contactLifetimeObserver::disable();}
        bowSpawnObserver::disable();
        rootCamera::disable();
        dlssEval::disable();
        std::vector<void*> renderBatchTargets;std::vector<std::pair<uintptr_t,const char*>> renderBatchPatterns;
        uint64_t renderBatchBegin=0;
        if(renderBatchRequested){
            if(!activeContactOwnerTrial||!activeXrExitOwnerReturned.load()||retainedPair.load()||!done.load()||renderCallbacks.load())throw std::runtime_error("render batch requires finished Scene/GPU and drained render callbacks");
            auto add=[&](uintptr_t rva,const char* pattern,bool enabled){if(!enabled)throw std::runtime_error("render batch target is not owned/enabled");auto* p=reinterpret_cast<void*>(baseAddress+rva);if(read<uint8_t>(baseAddress+rva)!=0xe9)throw std::runtime_error("render batch detour bytes differ");renderBatchTargets.push_back(p);renderBatchPatterns.emplace_back(rva,pattern);};
            add(sharedPointLights::rva,sharedPointLights::pattern,sharedPointLights::enabled);
            add(stableFrustumBridge::rva,stableFrustumBridge::pattern,stableFrustumBridge::enabled);
            for(const auto& h:vfxTransparent::hooks)add(h.rva,h.pattern,h.enabled);
            add(vfxTransparent::rva,vfxTransparent::pattern,vfxTransparent::enabled);
            for(const auto& h:cullingHistory::hooks)add(h.rva,h.pattern,h.enabled);
            if(vfxCulling::enabled)add(vfxCulling::rva,vfxCulling::pattern,vfxCulling::enabled);
            if(giShare::enabled)for(const auto& t:giShare::targets)add(t.rva,t.pattern,t.enabled);
            if(renderBatchTargets.size()!=12u+(vfxCulling::enabled?1u:0u)+(giShare::enabled?unsigned(giShare::targets.size()):0u))throw std::runtime_error("render batch manifest differs");
            renderBatchBegin=GetTickCount64();require(renderHookRetirement::batch.prepare(renderBatchTargets),"MH_ApplyQueued render-only disable");
            std::ostringstream m;m<<"\"targets\":[";for(size_t i=0;i<renderBatchPatterns.size();++i){if(i)m<<',';m<<hex(renderBatchPatterns[i].first);}m<<"],\"count\":12,\"scene_returned\":true,\"render_callbacks\":0,\"preserves_combat_and_input_hooks\":true,\"thread\":"<<GetCurrentThreadId();
            logFile->event("render_hook_batch_applied",m.str());
        }
        sharedPointLights::disable();
        eyeExposure::disable();
        stableFrustumBridge::disable();
        vfxCulling::disable();
        giShare::uninstall();
        objectWatch::uninstall();
        statusVignette::uninstall();
        combatCameraOverride::uninstall();
        if(statusVignette::vrTint)try{edgeTintBridge::apply(1,1,1,0);}catch(...){}
        vfxModelBridge::disable();vfxGateProbe::disable();vfxSpringRebase::disable();vfxCpuDrawProbe::disable();vfxDecals::disable();grassMovementBridge::disable();snowDisplacementBridge::disable();waterDisplacementBridge::disable();hitEventIntake::disable();worldRay::disable();shadowUpdateOnce::disable();shadowHistoryProbe::disable();vfxTransparent::disable();
        shadowProjection::disable();
        detailBinding::disable();
        cullingHistory::disable();
        if(renderBatchRequested){
            bool restored=true;for(const auto& h:renderBatchPatterns){const auto bytes=parsePattern(h.second);restored&=scan(reinterpret_cast<const uint8_t*>(baseAddress+h.first),bytes.size(),bytes)==std::vector<size_t>{0};}
            if(!renderHookRetirement::batch.complete()||!restored)throw std::runtime_error("render batch receipts or original code incomplete");
            logFile->event("render_hook_batch_complete","\"count\":12,\"pending\":0,\"original_bytes_restored\":true,\"trampolines_retained\":true,\"elapsed_ms\":"+std::to_string(GetTickCount64()-renderBatchBegin)+",\"thread\":"+std::to_string(GetCurrentThreadId()));
        }
        waterFeedback::disable();waterFeedback::waterObserve::disable();
        if(worldNativeDiagnostic){worldReadyDiagnostic::disable();worldSharedStages::disable();terrainReadyDiagnostic::disable();downloadFeedbackDiagnostic::disable();}
        if(detailFactoryProbe||worldResourceStage==2||worldNativeDiagnostic)disableDetailFactoryReserveHook();
        if(projectionEnabled){require(MH_DisableHook(projectionTarget),"MH_DisableHook eye projection");projectionEnabled=false;
            logFile->event("recorded_xr_projection_hook_removed","\"status\":0");fogProjection::retired();}
        if(scan(static_cast<uint8_t *>(target),pattern.size(),pattern)!=std::vector<size_t>{0})throw std::runtime_error("original instruction restoration mismatch");
        if(!retainedPair.load(std::memory_order_acquire)){disableSharedResolutionHooks();disableUnusedSceneHook();worldSharedDiagnostic::disable();}
        contactQuiescenceObserver::finishTerminalWorld();
        combatWorldBridge::finish();combatEpochGate::finish();
        if(contactQuiescenceObserver::ownerHandoffRequested){
            logFile->event("contact_owner_exit_ready","\"terminal_retired\":"+std::to_string(contactQuiescenceObserver::terminalRetired)+",\"world_leases_released\":true,\"input_owner_pending\":true,\"reads_ecs\":false");
            terminalOwnerReady.store(true,std::memory_order_release);
            const auto ownerDeadline=GetTickCount64()+10000;
            while(!controllerBackendExit::completion.load()&&GetTickCount64()<ownerDeadline)std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if(controllerBackendExit::completion.load()!=2)throw std::runtime_error("joined native input owner did not finalize after World retirement");
            return resultCode.load(); // Final proof emitted synchronously by native owner.
        }
        logFile->event("view_resource_hook_complete","\"original_bytes_restored\":true,\"trampoline_retained\":true,\"result\":"+std::to_string(resultCode.load()));
        return resultCode.load();
    } catch(const std::exception &error) {
        if(logFile)try{logFile->event("view_resource_hook_failure_begin","\"reason\":"+jsonString(error.what()));}catch(...){}
        active=false;auto cleanup=enabled?MH_DisableHook(target):MH_OK;
        if(cleanup==MH_OK){enabled=false;try{recordRenderRemoval();}catch(...){}}
        if(cleanup==MH_OK&&quiesceDependentHooks()){
            try{motionEventSelection::disable();}catch(...){}
        try{motionHitObserver::disable();}catch(...){}
        parryObserver::disable();targetMark::disable();meleeTriggerObserver::disable();opportunityTakeObserver::disable();sequenceRequestObserver::disable();rangedSpecialProbe::disable();steamLink::disableGameHooks();
        try{motionContactObserver::disable();}catch(...){}
            try{uiStateObserver::disable();}catch(...){}
            try{bodyUpload::disable();}catch(...){}
            try{headVisibility::disable();}catch(...){}
            sharedPointLights::disable();
            eyeExposure::disable();
            stableFrustumBridge::disable();
            vfxCulling::disable();
            giShare::uninstall();
            objectWatch::uninstall();
            statusVignette::uninstall();
            combatCameraOverride::uninstall();
            if(statusVignette::vrTint)try{edgeTintBridge::apply(1,1,1,0);}catch(...){}
            vfxModelBridge::disable();vfxGateProbe::disable();vfxSpringRebase::disable();vfxCpuDrawProbe::disable();vfxDecals::disable();grassMovementBridge::disable();snowDisplacementBridge::disable();waterDisplacementBridge::disable();hitEventIntake::disable();worldRay::disable();shadowUpdateOnce::disable();shadowHistoryProbe::disable();vfxTransparent::disable();
            try{equipmentObserver::disable();}catch(...){}
            try{inputDirection::disable();}catch(...){}
            try{contactQuiescenceObserver::disable();}catch(...){}
            try{contactLifetimeObserver::disable();}catch(...){}
            try{rootCamera::disable();}catch(...){}
            try{bowSpawnObserver::disable();}catch(...){}
            try{dlssEval::disable();}catch(...){}
            try{shadowProjection::disable();}catch(...){}
            try{detailBinding::disable();}catch(...){}
            try{cullingHistory::disable();}catch(...){}
            waterFeedback::disable();waterFeedback::waterObserve::disable();
            if(worldNativeDiagnostic){worldReadyDiagnostic::disable();worldSharedStages::disable();terrainReadyDiagnostic::disable();downloadFeedbackDiagnostic::disable();}
            if(detailFactoryProbe||worldResourceStage==2||worldNativeDiagnostic)disableDetailFactoryReserveHook();
            if(projectionEnabled){const auto status=MH_DisableHook(projectionTarget);projectionEnabled=status!=MH_OK;}
            if(!retainedPair.load(std::memory_order_acquire)){disableSharedResolutionHooks();disableUnusedSceneHook();worldSharedDiagnostic::disable();}
        }
        if(logFile)try{logFile->event("view_resource_hook_failure","\"reason\":"+jsonString(error.what())+",\"disable_status\":"+jsonString(MH_StatusToString(cleanup)));}catch(...){}
        return 1;
    }
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) { if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);return TRUE; }
// Input/UI observation independent of eye resources and OpenXR frame limits.
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ObserveWorldLifetimes(void* directory){
 if(!directory||started.exchange(true))return 2;
 HMODULE pinned{};
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ObserveWorldLifetimes),&pinned))return 3;
 try{
  const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));logFile=new Log((path/"view-resource-probe.jsonl").string(),true);
  std::ifstream policy(path/"world-lifetimes-duration.txt");std::string magic;unsigned seconds=0;policy>>magic>>seconds>>std::ws;
  const bool xrTrial=magic=="EVR_WORLD_LIFETIMES_XR_1";
  const bool reentryTrial=magic=="EVR_WORLD_REENTRY_1";
  if(!policy||!policy.eof()||(!xrTrial&&!reentryTrial&&magic!="EVR_WORLD_LIFETIMES_1")||((xrTrial||reentryTrial)?(seconds<300||seconds>420):(seconds<60||seconds>180)))throw std::runtime_error("invalid finite World observation duration");
  baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);
  if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("World observer PE header");
  const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
  if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)throw std::runtime_error("unsupported loaded executable build");
  if(std::filesystem::exists(path/"world-epoch-shadow.txt")){
   std::ifstream f(path/"world-epoch-shadow.txt");std::string value;f>>value>>std::ws;
   if(!f||!f.eof()||value!="EVR_WORLD_EPOCH_SHADOW_1")throw std::runtime_error("invalid World epoch shadow policy");
   worldLifecycleObserver::epochShadow=true;worldLifecycleObserver::epochs.sealAdoption();
  }
  if(std::filesystem::exists(path/"world-epoch-broker.txt")){
   std::ifstream f(path/"world-epoch-broker.txt");std::string value;f>>value>>std::ws;
   if(!f||!f.eof()||value!="EVR_WORLD_EPOCH_BROKER_1"||!worldLifecycleObserver::epochShadow||!std::filesystem::exists(path/"world-lifetimes-exit.txt"))throw std::runtime_error("broker requires initial finite shadow/normal-exit policy");
   worldLifecycleObserver::brokerRequested=true;
  }
  const bool exitTrial=std::filesystem::exists(path/"world-lifetimes-exit.txt");
  if(exitTrial){std::ifstream f(path/"world-lifetimes-exit.txt");std::string value;f>>value>>std::ws;if(!f||!f.eof()||value!="EVR_WORLD_EXIT_OBSERVE_1"||!worldLifecycleObserver::epochShadow)throw std::runtime_error("invalid finite World exit trial");}
  if(std::filesystem::exists(path/"input-backend-exit.txt")){
   std::ifstream f(path/"input-backend-exit.txt");std::string value;f>>value>>std::ws;
   if(!f||!f.eof()||value!="EVR_INPUT_BACKEND_EXIT_1"||!exitTrial||xrTrial||!worldLifecycleObserver::brokerRequested)throw std::runtime_error("input backend exit requires isolated startup broker/normal-exit observation");
   worldLifecycleObserver::inputExitRequested=true;
  }
  if(reentryTrial){
   if(!exitTrial||!worldLifecycleObserver::brokerRequested||worldLifecycleObserver::inputExitRequested)throw std::runtime_error("reentry observation requires isolated two-generation broker and no input-owner finalizer");
   worldLifecycleObserver::reentryTrial=true;
  }
  if(xrTrial&&(!exitTrial||!worldLifecycleObserver::brokerRequested))throw std::runtime_error("XR World shadow needs broker and normal exit");
  if(std::filesystem::exists(path/"world-preclose-observe.txt")){
   std::ifstream f(path/"world-preclose-observe.txt");std::string value;f>>value>>std::ws;
   if(!f||!f.eof()||value!="EVR_WORLD_PRECLOSE_OBSERVE_1"||!exitTrial||!worldLifecycleObserver::brokerRequested)throw std::runtime_error("preclose needs bounded startup broker and normal exit");
   worldLifecycleObserver::precloseRequested=true;
  }
  if(std::filesystem::exists(path/"world-preclose-admission.txt")){
   std::ifstream f(path/"world-preclose-admission.txt");std::string value;f>>value>>std::ws;
   if(!f||!f.eof()||value!="EVR_WORLD_PRECLOSE_ADMISSION_1"||!worldLifecycleObserver::precloseRequested)throw std::runtime_error("early admission needs explicit preclose observer");
   worldLifecycleObserver::precloseAdmission=true;
  }
  if(std::filesystem::exists(path/"world-exit-progress.txt")){
   std::ifstream f(path/"world-exit-progress.txt");std::string value;f>>value>>std::ws;
   if(!f||!f.eof()||value!="EVR_WORLD_EXIT_PROGRESS_1"||!worldLifecycleObserver::precloseRequested||xrTrial)throw std::runtime_error("read-only exit progress requires non-XR preclose trial");
   worldLifecycleObserver::progress::requested=true;
  }
  require(MH_Initialize(),"World MH_Initialize");worldLifecycleObserver::install();
  logFile->event("world_lifecycle_observation_begin","\"seconds\":"+std::to_string(seconds)+",\"read_only\":true,\"assigns_gameplay_generations\":false,\"exit_trial\":"+(exitTrial?"true":"false")+",\"xr_trial\":"+(xrTrial?"true":"false")+",\"reentry_trial\":"+(reentryTrial?"true":"false")+",\"expected_generations\":"+(reentryTrial?"2":"1"));
  const auto deadline=GetTickCount64()+seconds*1000;while(GetTickCount64()<deadline){
   if(worldLifecycleObserver::inputExitRequested){if(worldLifecycleObserver::inputExitCompletion.load()>=2)break;}
   else if(exitTrial&&worldLifecycleObserver::exitReady())break;
   std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if(worldLifecycleObserver::inputExitRequested){
   worldLifecycleObserver::finishInputExit();
   const auto finalDeadline=GetTickCount64()+2500;
   while(worldLifecycleObserver::inputExitCompletion.load()==1&&GetTickCount64()<finalDeadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
   return worldLifecycleObserver::inputExitCompletion.load()==2?0:1;
  }
  if(exitTrial&&!worldLifecycleObserver::exitReady())throw std::runtime_error("finite World exit observation incomplete");
  worldLifecycleObserver::disable();logFile->event("world_lifecycle_observation_complete","\"result\":0");return 0;
 }catch(const std::exception& e){
  try{if(baseAddress)worldLifecycleObserver::disable();}catch(...){}
  if(logFile)try{logFile->event("world_lifecycle_observation_failure","\"reason\":"+jsonString(e.what()));}catch(...){}return 1;
 }
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ObserveControllerOwnerExit(void* directory){
 if(!directory||started.exchange(true))return 2;HMODULE pinned{};
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ObserveControllerOwnerExit),&pinned))return 3;
 try{
  const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));logFile=new Log((path/"view-resource-probe.jsonl").string(),true);
  std::ifstream f(path/"controller-owner-exit.txt");std::string magic;unsigned seconds=0;f>>magic>>seconds>>std::ws;
  if(!f||!f.eof()||magic!="EVR_CONTROLLER_OWNER_EXIT_1"||seconds<60||seconds>180)throw std::runtime_error("invalid finite controller owner exit policy");
  baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);
  if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("input owner PE header");
  const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
  if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)throw std::runtime_error("unsupported input owner executable");
  configureControllerFixture(path,10,false);
  if(!controllerUiFixture::requested||controllerUiFixture::neutralLabel()!="true")throw std::runtime_error("owner exit trial requires only neutral controller input");
  require(MH_Initialize(),"controller owner MH_Initialize");controllerUiFixture::backendExitRequested=true;
  controllerBackendExit::install();inputUi::verify();
  const auto deadline=GetTickCount64()+seconds*1000;
  while(!controllerUiFixture::backendWaitReady.load()&&!controllerUiFixture::failed.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
  if(!controllerUiFixture::backendWaitReady.load()||controllerUiFixture::failed.load())throw std::runtime_error("native neutral controller did not connect/poll");
  // Remove all World/UI readers before the user initiates normal shutdown.
  // The real native backend retains the synthetic neutral device until its
  // own destructor delivers disconnect and unregister on the owner thread.
  inputUi::disable(true);
  if(inputUi::failures.load())throw std::runtime_error("frontend observation failed before owner handoff");
  logFile->event("controller_owner_exit_wait","\"ui_hook_removed\":true,\"neutral_only\":true,\"native_connected_seen\":true,\"requires_normal_exit\":true,\"world_readers_active\":false,\"module_pinned_until_process_exit\":true");
  while(controllerBackendExit::completion.load()<2&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
  if(controllerBackendExit::completion.load()!=2)throw std::runtime_error("finite controller native owner exit incomplete");
  return 0;
 }catch(const std::exception& e){
  controllerUiFixture::stop=true;
  if(inputUi::enabled)try{inputUi::disable();}catch(...){}
  // Keep code and the owner callback pinned if the live table is still ours.
  // It can still restore safely at actual destruction; never write via a timer.
  if(!controllerUiFixture::installed.load())try{controllerBackendExit::disable();}catch(...){}
  if(logFile)try{logFile->event("controller_owner_exit_failure","\"reason\":"+jsonString(e.what())+",\"owned_table_retained\":"+(controllerUiFixture::installed.load()?"true":"false"));}catch(...){}return 1;
 }
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ObserveGameplayInput(void* directory){
    if(!directory||started.exchange(true))return 2;
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ObserveGameplayInput),&pinned))return 3;
    try{
        const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));
        logFile=new Log((path/"view-resource-probe.jsonl").string(),true);
        std::ifstream policy(path/"input-observer-duration.txt");std::string magic;unsigned duration=0;
        policy>>magic>>duration>>std::ws;
        if(!policy||!policy.eof()||magic!="EVR_INPUT_DURATION_1"||duration<5||duration>60)throw std::runtime_error("invalid bounded input observation duration");
        baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("input observer PE header");
        const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
        if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)
            throw std::runtime_error("unsupported loaded executable build");
        require(MH_Initialize(),"input MH_Initialize");rootCamera::mode=1;rootCamera::install();
        inputShadow::requested=std::filesystem::exists(path/"input-shadow-identity.txt");
        if(inputShadow::requested){
            std::ifstream flag(path/"input-shadow-identity.txt");std::string value;flag>>value>>std::ws;
            if(!flag||!flag.eof()||value!="EVR_INPUT_SHADOW_IDENTITY_1")throw std::runtime_error("invalid input identity policy");
        }
        actionSequenceObserver::requested=std::filesystem::exists(path/"action-sequence-observer.txt");
        if(actionSequenceObserver::requested){
            std::ifstream flag(path/"action-sequence-observer.txt");std::string value;flag>>value>>std::ws;
            if(!flag||!flag.eof()||value!="EVR_ACTION_SEQUENCE_OBSERVE_1")throw std::runtime_error("invalid action observation policy");
            logFile->event("action_sequence_enabled","\"bytes\":3104,\"max_samples\":4096,\"read_only\":true,\"time_unit_known\":false");
        }
        if(std::filesystem::exists(path/"attachment-candidates.txt")){
            std::ifstream flag(path/"attachment-candidates.txt");std::string value;flag>>value>>std::ws;
            if(!flag||!flag.eof()||value!="EVR_ATTACHMENT_CANDIDATES_1")throw std::runtime_error("invalid read-only attachment candidate policy");
            attachmentObserver::requested=attachmentObserver::allowCandidateScene=true;
        }
        configureControllerFixture(path,duration,false);
        if(std::filesystem::exists(path/"combat-target-observe.txt")){
            std::ifstream flag(path/"combat-target-observe.txt");std::string value;flag>>value>>std::ws;
            if(!flag||!flag.eof()||value!="EVR_TARGET_OBSERVE_1"||!actionSequenceObserver::requested||controllerUiFixture::requested)throw std::runtime_error("target observation requires action evidence and no input fixture");
            nativeImpactComponents::requested=true;sequenceColliderObserver::requested=true;impactHierarchyObserver::requested=true;motionTargets::install();
        }
        if(std::filesystem::exists(path/"combat-contact-observe.txt")){
            std::ifstream flag(path/"combat-contact-observe.txt");std::string value;flag>>value>>std::ws;
            if(!flag||!flag.eof()||value!="EVR_COMBAT_CONTACT_OBSERVE_1"||!actionSequenceObserver::requested||!controllerUiFixture::attackRequested||controllerUiFixture::attackButtons!=XINPUT_GAMEPAD_X)throw std::runtime_error("invalid standalone combat observer");
            nativeImpactComponents::requested=true;sequenceColliderObserver::requested=true;impactHierarchyObserver::requested=true;motionContactObserver::install();motionHitObserver::install();
            if(std::filesystem::exists(path/"combat-event-selection-observe.txt")){
                std::ifstream f(path/"combat-event-selection-observe.txt");std::string magic;f>>magic>>std::ws;
                if(!f||!f.eof()||magic!="EVR_EVENT_SELECTION_OBSERVE_1")throw std::runtime_error("invalid event selection diagnostic");motionEventSelection::requested=true;
                if(std::filesystem::exists(path/"combat-event-selection-shadow.txt")){
                    std::ifstream sf(path/"combat-event-selection-shadow.txt");std::string sm;sf>>sm>>std::ws;
                    if(!sf||!sf.eof()||sm!="EVR_EVENT_SELECTION_SHADOW_1")throw std::runtime_error("invalid event shadow diagnostic");motionEventSelection::shadowRequested=true;
                }
                if(std::filesystem::exists(path/"combat-event-retime.txt")){
                    std::ifstream rf(path/"combat-event-retime.txt");std::string rm;rf>>rm>>std::ws;
                    if(!rf||!rf.eof()||rm!="EVR_EVENT_RETIME_1"||!motionEventSelection::shadowRequested)throw std::runtime_error("invalid one-attack event retime diagnostic");motionEventSelection::retimeRequested=true;motionColliderChannel::requireAuthority=true;
                }
                motionEventSelection::install();
            }
            if(std::filesystem::exists(path/"combat-collider-identity.txt")){
                std::ifstream f(path/"combat-collider-identity.txt");std::string magic;f>>magic>>std::ws;if(!f||!f.eof()||magic!="EVR_COLLIDER_IDENTITY_1")throw std::runtime_error("invalid collider identity test");sequenceColliderIdentity::requested=true;
            }
        }
        inputDirection::movementInterval=32;inputDirection::senderInterval=16;inputDirection::install();
        logFile->event("input_observation_begin","\"seconds\":"+std::to_string(duration)+",\"read_only\":"+(controllerUiFixture::requested?"false":"true"));
        const auto deadline=GetTickCount64()+duration*1000;
        while(GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(20));
        motionTargets::disable();motionEventSelection::disable();motionHitObserver::disable();motionContactObserver::disable();inputDirection::disable();contactQuiescenceObserver::disable();contactLifetimeObserver::disable();rootCamera::disable();
        const auto result=inputDirection::failures.load()||inputPipeline::failures.load()||inputShadow::failures.load()||
            sequenceColliderObserver::failures.load()||impactHierarchyObserver::failures.load()||motionContactObserver::failures.load()||
            (sequenceColliderIdentity::requested&&(!sequenceColliderIdentity::opened.load()||sequenceColliderIdentity::failures.load()||sequenceColliderIdentity::outstanding.load()||sequenceColliderIdentity::opened.load()!=sequenceColliderIdentity::closed.load()))||
            inputActor::failures.load()||inputPicking::failures.load()||inputRanged::failures.load()||controllerUiFixture::failed.load()||
            (actionSequenceObserver::requested&&(!actionSequenceObserver::samples||actionSequenceObserver::failures))||
            (attachmentObserver::requested&&attachmentObserver::failures.load())||
            (inputShadow::requested&&!inputShadow::scopes.load())||!inputPipeline::samples.load()||
            !inputDirection::samples.load()||!inputDirection::copySamples.load()?1u:0u;
        logFile->event("input_observation_complete","\"result\":"+std::to_string(result));return result;
    }catch(const std::exception& e){
        try{motionTargets::disable();}catch(...){}
        try{motionEventSelection::disable();}catch(...){}
        try{motionHitObserver::disable();}catch(...){}
        parryObserver::disable();targetMark::disable();meleeTriggerObserver::disable();opportunityTakeObserver::disable();sequenceRequestObserver::disable();rangedSpecialProbe::disable();steamLink::disableGameHooks();
        try{motionContactObserver::disable();}catch(...){}
        try{inputDirection::disable();}catch(...){}
            try{contactQuiescenceObserver::disable();}catch(...){}
            try{contactLifetimeObserver::disable();}catch(...){}
        try{rootCamera::disable();}catch(...){}
        if(logFile)try{logFile->event("input_observation_failure","\"reason\":"+jsonString(e.what()));}catch(...){}
        return 1;
    }
}
// Disabled after the native update following feature disposal crashed at 17290c.
// A successful constructor/destructor return did not prove scene ownership safe.
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeViewFeatures(void *) { return 5; }
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeIsolatedViewFeatures(void *directory) { initializeFeatures=true;return EVR_ProbeViewResources(directory); }
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeViewResolution(void *directory) { initializeFeatures=true;initializeResolution=true;return EVR_ProbeViewResources(directory); }
// Mid-frame retirement caused a later vkCmdBindDescriptorSets crash. Disabled.
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeNativeCalibration(void *) {return 5;}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeRetainedCalibration(void *directory) { initializeFeatures=true;initializeResolution=true;renderCalibration=true;retainCalibration=true;return EVR_ProbeViewResources(directory); }
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeSceneLifetimeCalibration(void *directory) { sceneCleanup=true;return EVR_ProbeRetainedCalibration(directory); }
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeNativeSeries(void *directory) { calibrationFrames=300;return EVR_ProbeSceneLifetimeCalibration(directory); }
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeRecordedXr(void *directory) { useRecordedXr=true;return EVR_ProbeSceneLifetimeCalibration(directory); }
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbePreparedGameSession(void* directory){preparedSessionProbe=true;return EVR_ProbeViewResources(directory);}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeDetailFactory(void* directory){detailFactoryProbe=true;return EVR_ProbeViewResources(directory);}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeWorldViewFeatures(void* directory){worldResourceStage=1;initializeFeatures=true;return EVR_ProbeViewResources(directory);}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeWorldViewResolution(void* directory){worldResourceStage=2;initializeFeatures=true;initializeResolution=true;return EVR_ProbeViewResources(directory);}

extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeLiveNativeXr(void* directory){liveXr=true;initializeFeatures=true;initializeResolution=true;renderCalibration=true;retainCalibration=true;sceneCleanup=true;calibrationFrames=1;return EVR_ProbeViewResources(directory);}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeLiveNativeFov(void* directory){matchCameraFov=true;return EVR_ProbeLiveNativeXr(directory);}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ProbeWorldNativeXr(void* directory){
    std::ifstream proof(std::filesystem::path(static_cast<wchar_t*>(directory))/"world-resource-proof.txt");std::string magic;
    proof>>magic>>std::hex>>provenWorldScene>>provenWorldView;
    if(!proof||magic!="EVR_WORLD_RESOURCE_1"||!provenWorldScene||!provenWorldView)return 5;
    worldNativeDiagnostic=true;return EVR_ProbeLiveNativeXr(directory);
}

// The mod without VR (user, 2026-10-14: "with the VR display off the other players' movements are still seen as long
// as the mod is installed", "VR can be switched on and off"): the game runs and draws as it does by itself. Only the
// other players' VR bodies are driven (online_sync.hpp, remote_body.hpp) - at the same place of the frame as with VR:
// on the render thread, at the entry of the world View's render call, after the frame's CPU tasks are done.
// Sent: no body (there is none) - where the own character stands, once a second.
// The world Scene and View: with world-resource-proof.txt, the ones of the proof probe of the same launch, as for VR.
// Without it they are found here - the View whose render call comes by, that is the selected View of its Scene, has
// the world's detail owner (wrapper flag 0x40, as the proof probe asks) and no feature the world's Views do not have.
// Nothing is created for it, so no second hook library and no proof run are needed, and the mod may be started at the
// game's menu: it waits for a world, and takes the next one when the player leaves a session and enters another
// (the old Scene's poses and models went with it; nothing of them is touched again).
// online-flat.txt: EVR_ONLINE_FLAT_1 <minutes>; <directory>/stop.request ends it earlier.
// EVR_SYNC_FAKE_BODY=1 (with EVR_SYNC_SELF=1 or 2): a made-up body with waving arms goes out as the own one - a test
// for one player, seen on the own character in the game's third person view.
namespace onlineFlat {
Render nativeRender=nullptr;std::atomic<bool> stopRequested{false},retired{false};std::atomic<int> inFlight{0};std::atomic<uint64_t> frames{0},failures{0},skipped{0};
uint64_t serial=0;bool fakeBody=false,findWorld=false;unsigned logs=0,worlds=0;std::atomic<uint64_t> provenSeen{0};std::atomic<bool> cameraReady{false};
bool worldLike(uintptr_t view) noexcept {
    try{const auto wrapper=read<uintptr_t>(view),scene=read<uintptr_t>(view+8);
        return wrapper&&scene&&(read<uint32_t>(wrapper+0x10)&0x40u)&&!(read<uint32_t>(view+0x10)&~worldKnownFeatures)&&read<uintptr_t>(scene+layout::sceneView)==view;}
    catch(...){return false;}
}
// The camera observer wants a camera within three seconds of its installation; in a world that is still loading
// there is none yet. It is installed once and kept; later calls only look whether a camera has come by (taking it
// off and creating it again is refused by the hook library: it is created already).
struct LazyCamera {
    bool installed=false,ready=false;
    bool poll(){
        if(ready)return true;
        if(!installed){
            try{rootCamera::install();installed=true;ready=true;return true;}
            catch(const std::exception&){if(!rootCamera::enabled)throw;installed=true;}   // the hook is on, no camera yet
        }
        std::lock_guard<std::mutex> lock(rootCamera::mutex);ready=rootCamera::latest.number!=0;return ready;
    }
};
// The host's part for the other players' mods without VR (2026-10-22, user: "update it so that it works whether the friend
// plays in VR or not"). On the friend's server without VR the guest's decided hits, arrows, parries and guards were heard
// (online_sync_hit_heard) and never applied: the hooks that apply them came only with the live controller's policies of a
// VR start. Here the same parts, none of the VR input: the iterator observer (the server World's binding, the own Actor,
// the own bow's identity), the attack colliders' sweep with the guests' decided hits and weapons, the list of targets on
// the own Actor's ticks (the host's things near the guests, their arrows steered, their health given back), the server's
// parry and the spawn of shots. Flags and function pointers at the start; the hooks once the camera knows the local player.
// EVR_FLAT_HOST_PARTS=0: off (as 0.9.7).
bool hostPartsWanted=false,hostPartsTried=false,inputActorOn=false,selectionOn=false,bowOn=false;
void prepareHostParts(){
    {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FLAT_HOST_PARTS",v,sizeof v);hostPartsWanted=!(n==1&&v[0]=='0');}
    if(!hostPartsWanted){logFile->event("online_flat_host_parts","\"enabled\":false,\"reason\":\"EVR_FLAT_HOST_PARTS=0\"");return;}
    // per event name 300 lines (as a VR start: auto_start.hpp EVR_LOG_NAME_CAP), now that the observers record
    {unsigned cap=300;if(const char* text=std::getenv("EVR_LOG_NAME_CAP")){char* end=nullptr;const auto n=std::strtoul(text,&end,10);if(end&&!*end&&n>=100&&n<=20000)cap=unsigned(n);}logFile->setNameCap(cap);}
    nativeImpactComponents::requested=true;actionSequenceObserver::requested=true;   // recording stays on (no eye pairs to wait for)
    sequenceColliderObserver::requested=true;impactHierarchyObserver::requested=true;attachmentObserver::requested=true;
    motionColliderApply::requested=true;motionColliderApply::live=true;liveTargets::requested=true;
    remoteMelee::nearestTarget=&liveTargets::nearestTo;controllerUiFixture::undoDamage=&liveTargets::undoDamage;controllerUiFixture::guardGiveBack=&liveTargets::guardGiveBack;controllerUiFixture::staminaOf=&liveTargets::staminaOf;
    controllerUiFixture::shotHitArrived=&liveTargets::publishShotHit;
    inputRanged::liveBowWatch=true;bowSpawnObserver::remoteAlways=true;   // the own shots known as own (not a guest's)
}
void installHostParts(){
    if(!hostPartsWanted||hostPartsTried)return;hostPartsTried=true;
    const auto part=[](const char* name,void(*install)(),bool& on){
        try{install();on=true;}catch(const std::exception& e){on=false;logFile->event("optional_hook_skipped","\"hook\":"+jsonString(name)+",\"reason\":"+jsonString(e.what()));}};
    part("input_actor",[]{inputActor::install();},inputActorOn);
    // Without the iterator observer the own Actor and bow are not known: the parry and the shots would take the host's own for a guest's.
    if(inputActorOn){part("motion_event_selection",[]{motionEventSelection::install();},selectionOn);
        parryObserver::install();part("bow_spawn",[]{bowSpawnObserver::install();},bowOn);}
    logFile->event("online_flat_host_parts","\"enabled\":true,\"input_actor\":"+std::string(inputActorOn?"true":"false")+",\"targets\":"+(selectionOn?"true":"false")+
        ",\"parry\":"+(parryObserver::enabled?"true":"false")+",\"shots\":"+(bowOn?"true":"false")+",\"fishing_cast\":false,\"vr_input\":false");
}
void removeHostParts() noexcept {
    if(selectionOn)try{motionEventSelection::disable();}catch(...){}   // a session without a sample is reported, not fatal
    parryObserver::disable();
    if(bowOn)try{bowSpawnObserver::disable();}catch(...){}
    if(inputActorOn)try{inputActor::disable();}catch(...){}
    selectionOn=bowOn=inputActorOn=false;
}
// Another world than before (or the first): everything that belonged to the old Scene is forgotten, not undone.
void adopt(uintptr_t view) noexcept {
    try{
        // One library runs through every world of the game here (a VR start loads a fresh copy per world): what the
        // observers learnt of the old Worlds goes (the World binding keeps eight Worlds at most and is never cleared).
        {std::lock_guard<std::mutex> lock(nativeImpactComponents::mutex);nativeImpactComponents::bindings.clear();nativeImpactComponents::known.clear();}
        {std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);actionSequenceObserver::latest={};actionSequenceObserver::qualified.clear();}
        {std::lock_guard<std::mutex> lock(remoteMelee::mutex);remoteMelee::weapons={};remoteMelee::players={};remoteMelee::hits={};remoteMelee::peerEntities.clear();}
        remoteBody::bodies.clear();remoteBody::scene=0;motionVisual::scene=0;motionVisual::contextReady=false;
        motionHandVisual::rig.reset();motionHandVisual::poseResource=0;motionHandVisual::sourceHandle=0;
        {std::lock_guard<std::mutex> lock(equipmentObserver::otherMutex);equipmentObserver::others.clear();}
        {std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);equipmentObserver::latest={};}
        for(auto& p:onlineSync::peers.all())p.character=-1;
        onlineSync::flatPlaced=false;onlineSync::placed=false;
        provenWorldView=view;provenWorldScene=read<uintptr_t>(view+8);++worlds;
        if(logs++<400)logFile->event("online_flat_world","\"number\":"+std::to_string(worlds)+",\"scene\":"+hex(provenWorldScene)+",\"view\":"+hex(provenWorldView)+",\"features\":"+std::to_string(read<uint32_t>(view+0x10))+
            ",\"found_by\":\"render call of the selected View with the world's detail owner\"");
    }catch(...){provenWorldView=0;provenWorldScene=0;}
}
void frame(uintptr_t view) noexcept {
    const ObserverLastError preserve;
    try{
        const auto scene=read<uintptr_t>(view+8);
        if(scene!=provenWorldScene||read<uintptr_t>(scene+layout::sceneView)!=view){++skipped;return;}
        if(!finishEyeCpuTasks(scene,4)){++skipped;return;}   // 1.0: a busy View streaming skips the frame, without waiting long
        ++liveFrame.frame;++serial;++frames;
        motionVisual::scene=scene;motionVisual::view=view;motionVisual::contextReady=true;motionVisual::contextSerial=serial;motionVisual::contextFrame=liveFrame.frame;
        if(stopRequested.load()){onlineSync::dispose(scene);motionVisual::contextReady=false;retired=true;return;}
        if(!cameraReady.load())return;   // who the local player is, is not known yet (the camera observer comes once a world is drawn)
        equipmentObserver::Snapshot own;{std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);own=equipmentObserver::latest;}
        const auto local=rootCamera::snapshot().entity;const bool ownKnown=own.number&&own.entity==local&&own.scene==scene&&GetTickCount64()-own.milliseconds<=1500;
        if(ownKnown){
            std::array<double,3> at{};if(onlineSync::placeOf(own.handles,at)){onlineSync::flatPlace=sync::place(at);onlineSync::flatPlaced=true;}
            motionHandVisual::rigFromSource(own.handles[11]);
        }
        if(fakeBody&&ownKnown&&motionHandVisual::rig&&onlineSync::flatPlaced){
            // Arms that wave, on the own character's place and facing.
            uintptr_t anchor=0;for(unsigned part=0;part<20&&!anchor;part++)if(own.handles[part])anchor=motionVisual::currentObject(own.handles[part]);
            if(anchor){
                const auto transform=read<std::array<uint8_t,56>>(anchor+0x38);Quat modelQ;Vec3 modelScale;std::memcpy(&modelQ,transform.data()+24,16);std::memcpy(&modelScale,transform.data()+40,12);
                const auto& rig=*motionHandVisual::rig;const auto R=bodyik::globals(rig.neutral(),rig.parents());const bodyik::Settings settings;
                const float t=float(frames.load())*.12f;
                bodyik::Targets targets;targets.head={{0,0,0,1},R[17].position+settings.eyeInHead};
                targets.socket[0]={R[64].orientation,{-.28f,1.25f+.25f*std::sin(t),.32f}};targets.socket[1]={R[94].orientation,{.28f,1.25f+.25f*std::sin(t+pi),.32f}};
                std::array<hand::FingerCurl,2> curls{};for(auto& hand:curls)for(auto& v:hand.amount)v=.5f+.5f*std::sin(t*.7f);
                auto& out=motionBodyIk::published;out.body=sync::fromSolve(sync::Model{bodyik::uprightPart(normalized(modelQ)),modelScale},targets,R[1],curls);
                out.place=onlineSync::flatPlace;out.serial=serial;out.have=true;
            }
        }else motionBodyIk::published.have=false;
        onlineSync::frame(serial);
        // EVR_EFFECT_CENSUS=1 without VR as well (2026-10-21: the user's bow and bomb test ran without VR and its census was
        // never asked): the camera's World only - the client's, as a guest's copies of the host's arrows come.
        {rootCamera::Sample cam;{std::lock_guard<std::mutex> lock(rootCamera::mutex);cam=rootCamera::latest;}
         if(cam.number&&cam.world)effectEntityCensus::observe(cam.world,0,cam.stable.position,false);}
        if(frames.load()%600==1&&logs++<400)logFile->event("online_flat_frame","\"frames\":"+std::to_string(frames.load())+",\"skipped\":"+std::to_string(skipped.load())+",\"own_character_known\":"+(ownKnown?"true":"false")+
            ",\"rig\":"+(motionHandVisual::rig?"true":"false")+",\"fake_body\":"+(fakeBody?"true":"false")+",\"others_seen\":"+std::to_string([]{std::lock_guard<std::mutex> lock(equipmentObserver::otherMutex);return equipmentObserver::others.size();}()));
    }catch(const std::exception& e){if(failures.fetch_add(1)<20)try{logFile->event("online_flat_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
    catch(...){failures.fetch_add(1);}
}
void* __fastcall render(void* out,void* context,void* view,void* parameters,void* target){
    struct Flight{Flight(){inFlight.fetch_add(1);}~Flight(){inFlight.fetch_sub(1);}} flight;
    const auto v=reinterpret_cast<uintptr_t>(view);const auto now=GetTickCount64();
    if(!retired.load()){
        if(findWorld&&v!=provenWorldView&&(!provenWorldView||now-provenSeen.load()>1500)&&worldLike(v))adopt(v);
        if(v&&v==provenWorldView){provenSeen=now;frame(v);}
    }
    return nativeRender(out,context,view,parameters,target);
}
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_OnlineFlat(void* directory){
    if(!directory||started.exchange(true))return 2;
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_OnlineFlat),&pinned))return 3;
    void* target=nullptr;bool hooked=false;
    try{
        const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));calibrationDirectory=path;
        logFile=new Log((path/"view-resource-probe.jsonl").string(),true);
        std::ifstream policy(path/"online-flat.txt");std::string magic;unsigned minutes=0;policy>>magic>>minutes>>std::ws;
        if(!policy||!policy.eof()||magic!="EVR_ONLINE_FLAT_1"||minutes>720)throw std::runtime_error("invalid online flat policy");   // 0: no minute limit
        if(std::filesystem::exists(path/"world-resource-proof.txt")){
            std::ifstream proof(path/"world-resource-proof.txt");std::string proofMagic;proof>>proofMagic>>std::hex>>provenWorldScene>>provenWorldView;
            if(!proof||proofMagic!="EVR_WORLD_RESOURCE_1"||!provenWorldScene||!provenWorldView)throw std::runtime_error("world proof unreadable");
        }else onlineFlat::findWorld=true;
        baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("online flat PE header");
        const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
        if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)throw std::runtime_error("unsupported loaded executable build");
        const auto pattern=parsePattern(renderPattern);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+renderRva),pattern.size(),pattern)!=std::vector<size_t>{0})throw std::runtime_error("render signature differs");
        if(std::filesystem::exists(path/"world-hand-curl.txt")){
            std::ifstream cf(path/"world-hand-curl.txt");std::string cm;float relaxed=-1;cf>>cm>>relaxed>>std::ws;
            std::ifstream pf(path/"hand-curl-closed-pose.bin",std::ios::binary|std::ios::ate);
            if(!cf||!cf.eof()||cm!="EVR_HAND_CURL_1"||!std::isfinite(relaxed)||relaxed<0||relaxed>.9f||!pf||pf.tellg()!=102*48)throw std::runtime_error("invalid hand curl policy/closed pose");
            motionHandVisual::closedPose.resize(102);pf.seekg(0);pf.read(reinterpret_cast<char*>(motionHandVisual::closedPose.data()),102*48);motionHandVisual::curlRelaxed=relaxed;
        }
        {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SYNC_FAKE_BODY",value,sizeof value);onlineFlat::fakeBody=n&&n<sizeof value&&value[0]=='1';}
        worldNativeDiagnostic=true;   // finishEyeCpuTasks: the five collectors, not d5870 (which discards pending world SDF results)
        require(MH_Initialize(),"online flat MH_Initialize");
        // The camera observer (whose entity is the local player) wants a camera of a running world within three
        // seconds: at the menu it is installed later, from this thread, once a world is drawn.
        rootCamera::mode=1;bool cameraInstalled=false;
        if(!onlineFlat::findWorld){rootCamera::install();cameraInstalled=true;onlineFlat::cameraReady=true;}
        equipmentObserver::requested=true;equipmentObserver::continuous=true;equipmentObserver::otherWorldsPass=true;equipmentObserver::install();
        onlineSync::configure(true);onlineSync::flat=true;
        onlineFlat::prepareHostParts();
        if(cameraInstalled)onlineFlat::installHostParts();
        target=reinterpret_cast<void*>(baseAddress+renderRva);
        require(MH_CreateHook(target,reinterpret_cast<void*>(&onlineFlat::render),reinterpret_cast<void**>(&onlineFlat::nativeRender)),"online flat render hook");
        require(MH_EnableHook(target),"online flat render hook enable");hooked=true;
        logFile->event("online_flat_enabled","\"minutes\":"+std::to_string(minutes)+",\"scene\":"+hex(provenWorldScene)+",\"view\":"+hex(provenWorldView)+",\"finds_the_world_itself\":"+(onlineFlat::findWorld?"true":"false")+",\"vr\":false,\"self_test\":"+(onlineSync::selfTest?"true":"false")+
            ",\"fake_body\":"+(onlineFlat::fakeBody?"true":"false")+",\"finger_curl\":"+(motionHandVisual::closedPose.empty()?"false":"true"));
        const auto deadline=minutes?GetTickCount64()+uint64_t(minutes)*60000:~0ull;std::error_code ec;
        uint64_t cameraTried=0;unsigned cameraAttempts=0;onlineFlat::LazyCamera lazyCamera;bool cameraBroken=false;
        while(GetTickCount64()<deadline&&!std::filesystem::exists(path/"stop.request",ec)){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const auto now=GetTickCount64();
            if(!cameraInstalled&&!cameraBroken&&provenWorldView&&now-onlineFlat::provenSeen.load()<=500&&now-cameraTried>=1000){cameraTried=now;++cameraAttempts;
                try{if(lazyCamera.poll()){cameraInstalled=true;onlineFlat::cameraReady=true;logFile->event("online_flat_camera","\"installed\":true,\"attempts\":"+std::to_string(cameraAttempts));onlineFlat::installHostParts();}}
                catch(const std::exception& e){cameraBroken=true;try{rootCamera::disable();}catch(...){}
                    logFile->event("online_flat_camera","\"installed\":false,\"attempts\":"+std::to_string(cameraAttempts)+",\"reason\":"+jsonString(e.what()));}
            }
        }
        cameraInstalled=cameraInstalled||lazyCamera.installed;
        // The characters go back onto the game's poses on the render thread, at the same place of the frame.
        onlineFlat::stopRequested=true;
        // No world is drawn (the menu, or none was ever entered): nothing stands on the mod's poses in a living Scene.
        const bool worldDrawn=provenWorldView&&GetTickCount64()-onlineFlat::provenSeen.load()<=2000;
        for(unsigned n=0;worldDrawn&&n<300&&!onlineFlat::retired.load();n++)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const bool retiredInTime=!worldDrawn||onlineFlat::retired.load();onlineFlat::retired=true;
        const auto status=MH_DisableHook(target);hooked=false;
        for(unsigned n=0;n<200&&onlineFlat::inFlight.load();n++)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        onlineFlat::removeHostParts();
        equipmentObserver::disable();if(cameraInstalled)rootCamera::disable();
        const DWORD result=(onlineFlat::failures.load()||!retiredInTime||status!=MH_OK||(!onlineFlat::findWorld&&!onlineFlat::frames.load()))?1u:0u;
        logFile->event("online_flat_complete","\"result\":"+std::to_string(result)+",\"worlds\":"+std::to_string(onlineFlat::worlds)+",\"frames\":"+std::to_string(onlineFlat::frames.load())+",\"skipped\":"+std::to_string(onlineFlat::skipped.load())+
            ",\"failures\":"+std::to_string(onlineFlat::failures.load())+",\"characters_back_on_the_game_s_poses\":"+(retiredInTime?"true":"false")+",\"render_hook_removed\":"+(status==MH_OK?"true":"false")+
            ",\"bodies_sent\":"+std::to_string(onlineSync::bodiesSent)+",\"hellos_sent\":"+std::to_string(onlineSync::hellosSent)+",\"received\":"+std::to_string(steamLink::received.load())+",\"driven_frames\":"+std::to_string(onlineSync::drivenFrames));
        return result;
    }catch(const std::exception& e){
        onlineFlat::retired=true;
        if(hooked&&target)MH_DisableHook(target);
        onlineFlat::removeHostParts();
        try{equipmentObserver::disable();}catch(...){}
        try{rootCamera::disable();}catch(...){}
        if(logFile)try{logFile->event("online_flat_failure","\"reason\":"+jsonString(e.what()));}catch(...){}
        return 1;
    }
}

// The start by itself (user, 2026-10-14: "started from the Steam launcher as usual, the game starts with the settings
// of last time"): the game begins at its own menu, and the unused proof and the eyes need a world. This waits for one.
// Nothing is created or changed: the render call is watched until the View of a world (onlineFlat::worldLike) has been
// drawn without a gap for the asked seconds and the camera observer finds the local player's camera (it is taken off
// again at once). Then the hook is taken off and the Scene and View are written to world-found.txt.
// await-world.txt: EVR_AWAIT_WORLD_1 <steady seconds 1..120>; <directory>/stop.request ends the wait (result 6).
namespace awaitWorld {
Render nativeRender=nullptr;std::atomic<uintptr_t> view{0},scene{0};std::atomic<uint64_t> since{0},seen{0};std::atomic<int> inFlight{0};
void* __fastcall render(void* out,void* context,void* v,void* parameters,void* target){
    struct Flight{Flight(){inFlight.fetch_add(1);}~Flight(){inFlight.fetch_sub(1);}} flight;
    const auto candidate=reinterpret_cast<uintptr_t>(v);
    if(onlineFlat::worldLike(candidate)){
        const auto now=GetTickCount64();
        if(view.load()!=candidate||now-seen.load()>1000){uintptr_t s=0;try{s=read<uintptr_t>(candidate+8);}catch(...){}
            scene=s;view=candidate;since=now;}
        seen=now;
    }
    return nativeRender(out,context,v,parameters,target);
}
}
extern "C" __declspec(dllexport) DWORD WINAPI EVR_AwaitWorld(void* directory){
    if(!directory||started.exchange(true))return 2;
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_AwaitWorld),&pinned))return 3;
    void* target=nullptr;bool hooked=false;
    try{
        const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));calibrationDirectory=path;
        logFile=new Log((path/"view-resource-probe.jsonl").string(),true);
        std::ifstream policy(path/"await-world.txt");std::string magic;unsigned steady=0;policy>>magic>>steady>>std::ws;
        if(!policy||!policy.eof()||magic!="EVR_AWAIT_WORLD_1"||steady<1||steady>120)throw std::runtime_error("invalid await world policy");
        baseAddress=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto dos=read<IMAGE_DOS_HEADER>(baseAddress);
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)throw std::runtime_error("await world PE header");
        const auto nt=read<IMAGE_NT_HEADERS64>(baseAddress+dos.e_lfanew);
        if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.TimeDateStamp!=evr::gameBuild::timeDateStamp||nt.OptionalHeader.SizeOfImage!=evr::gameBuild::sizeOfImage)throw std::runtime_error("unsupported loaded executable build");
        const auto pattern=parsePattern(renderPattern);
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+renderRva),pattern.size(),pattern)!=std::vector<size_t>{0})throw std::runtime_error("render signature differs");
        require(MH_Initialize(),"await world MH_Initialize");
        target=reinterpret_cast<void*>(baseAddress+renderRva);
        require(MH_CreateHook(target,reinterpret_cast<void*>(&awaitWorld::render),reinterpret_cast<void**>(&awaitWorld::nativeRender)),"await world render hook");
        require(MH_EnableHook(target),"await world render hook enable");hooked=true;
        logFile->event("await_world_enabled","\"steady_seconds\":"+std::to_string(steady)+",\"creates_or_changes_anything\":false");
        std::error_code ec;uint64_t cameraTried=0;unsigned attempts=0;DWORD result=6;uintptr_t foundView=0,foundScene=0;const auto begun=GetTickCount64();
        rootCamera::mode=1;onlineFlat::LazyCamera lazyCamera;uint64_t cameraSince=0;
        while(!std::filesystem::exists(path/"stop.request",ec)){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const auto now=GetTickCount64();const auto v=awaitWorld::view.load();
            if(!v||now-awaitWorld::seen.load()>500||now-awaitWorld::since.load()<uint64_t(steady)*1000){cameraSince=0;continue;}
            if(now-cameraTried<500)continue;
            cameraTried=now;++attempts;
            if(!lazyCamera.poll()){cameraSince=0;
                if(attempts==1||attempts%40==0)logFile->event("await_world_camera","\"ready\":false,\"attempts\":"+std::to_string(attempts));
                continue;}
            // The world and the local player's camera, both for half a second more without a gap.
            if(!cameraSince){cameraSince=now;continue;}
            if(now-cameraSince<500)continue;
            foundView=v;foundScene=awaitWorld::scene.load();result=foundScene?0u:1u;break;
        }
        rootCamera::disable();
        const auto status=MH_DisableHook(target);hooked=false;
        for(unsigned n=0;n<200&&awaitWorld::inFlight.load();n++)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if(status!=MH_OK||awaitWorld::inFlight.load())result=1;
        if(result==0){std::ofstream found(path/"world-found.txt",std::ios::binary);found<<"EVR_WORLD_FOUND_1 0x"<<std::hex<<foundScene<<" 0x"<<foundView<<'\n';found.flush();if(!found)result=1;}
        logFile->event("await_world_complete","\"result\":"+std::to_string(result)+",\"scene\":"+hex(foundScene)+",\"view\":"+hex(foundView)+",\"waited_ms\":"+std::to_string(GetTickCount64()-begun)+
            ",\"camera_attempts\":"+std::to_string(attempts)+",\"render_hook_removed\":"+(status==MH_OK?"true":"false"));
        return result;
    }catch(const std::exception& e){
        if(hooked&&target)MH_DisableHook(target);
        try{rootCamera::disable();}catch(...){}
        if(logFile)try{logFile->event("await_world_failure","\"reason\":"+jsonString(e.what()));}catch(...){}
        return 1;
    }
}

#include "model_instance_probe.hpp"
#include "hand_pose_probe.hpp"
