#include <evr/melee_session_fixture.hpp>
#include <evr/epoch_attack_packet.hpp>
#include <evr/combat_epoch_client.hpp>
#include <evr/motion_guard_fixture.hpp>
#include <evr/controller_guard_fixture.hpp>
#include <evr/controller_equipment_fixture.hpp>
#include <evr/head_gesture_feedback.hpp>
#include <evr/vertical_stick_mapping.hpp>
#include <evr/quick_wheel_schedule.hpp>
#include <evr/grip_router.hpp>
#include <evr/holster.hpp>
#include <evr/room_sneak.hpp>
#include <evr/swim_stroke.hpp>
#include <evr/vr_settings_panel.hpp>
#include <evr/equip_planner.hpp>
#include <evr/motion_event_retime.hpp>
#include <evr/live_contact.hpp>
#include <evr/motion_feedback.hpp>
#include <evr/motion_attack_fixture.hpp>
#include <evr/controller_attack_fixture.hpp>
#include <evr/controller_motion_fixture.hpp>
#include <evr/controller_menu_fixture.hpp>
#include <evr/live_pad.hpp>
#include <evr/bow_input_trial.hpp>
#include <evr/bow_physical_trial.hpp>
#include <evr/bow_physical_release.hpp>
#include <evr/bow_tracked_shot.hpp>
#include <evr/bow_draw.hpp>
#include <evr/ranged_aim.hpp>
#include <evr/live_melee.hpp>
#include <evr/live_throw.hpp>
#include <evr/live_swing_cast.hpp>
#include <evr/live_secondary.hpp>
#include <evr/throwable_catalog.hpp>
#include <evr/live_guard.hpp>
#include <evr/quick_slot.hpp>
#include "menu_pointer.hpp"
#include "aim_mark.hpp"
#include "key_script.hpp"
// Explicit TEST ONLY: neutral XInput connect/disconnect inside the managed game.
// The owned backend table is restored on the frontend thread. No OS driver,
// system DLL detour or physical vibration. Neutral mode has zero input;
// menu mode uses bounded START pulses through the original input consumer.
namespace controllerUiFixture {
using StateFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
using CapsFn=DWORD(WINAPI*)(DWORD,DWORD,XINPUT_CAPABILITIES*);
using RumbleFn=DWORD(WINAPI*)(DWORD,XINPUT_VIBRATION*);
// Explicit finite gesture/menu diagnostic. Render thread publishes values;
// frontend thread alone publishes the existing owned native input packet.
inline bool guardRequested=false,guardCompleted=false;
inline evr::game::ControllerGuardFixture guard;
inline evr::game::GuardObservation(*observeGuard)()=nullptr;
inline int lastGuardBits=-1;
inline bool motionGuardRequested=false;
inline std::unique_ptr<evr::game::MotionGuardFixture> motionGuard;
inline std::unique_ptr<evr::motion::GuardBodyTracker> guardBodyTracker;
inline std::mutex guardMutex;inline evr::motion::HandFrame guardHands;inline evr::motion::GuardBody guardBody;
inline uint64_t guardReceipt=0,lastGuardFrame=0;
inline void publishGuardHands(const evr::motion::HandFrame& f,const evr::motion::GestureHead& h,uint64_t now){
 if(!motionGuardRequested)return;
 const auto b=guardBodyTracker->update(f,h);std::lock_guard<std::mutex> lock(guardMutex);guardHands=f;guardBody=b;guardReceipt=now;
}

inline bool equipmentRequested=false,equipmentCompleted=false;
inline evr::game::ControllerEquipmentFixture equipment;
inline evr::game::EquipmentObservation(*observeEquipment)()=nullptr;
inline uint32_t lastEquipmentItem=0;
inline bool gestureRequested=false,gestureCompleted=false,gestureSawMenu=false,combatGestureRequested=false;
inline bool gestureOpenCapture=false,gestureSelectionCapture=false,gestureClosedCapture=false;
inline bool wheelRequested=false;inline evr::motion::QuickWheelProgress wheelProgress;
// Explicit EVR_GRIP_ROUTER_WHEEL_1: RB comes from GripRouter only. Equip requests are logged,
// never executed. Synthetic body/zone calibration comes from world-grip-router.txt.
inline bool routerRequested=false;inline std::unique_ptr<evr::motion::GripRouter> gripRouter;
inline std::unique_ptr<evr::motion::GuardBodyTracker> routerBody;inline evr::motion::GripRoute lastRoute{};
inline uint64_t lastRouterFrame=0;inline unsigned routerEquipRequests=0;
// Explicit EVR_GRIP_ROUTER_EQUIP_1: router equip requests are executed only through the native
// gamepad wheel (RB + right stick) toward a planned weapon slot; never a consumable slot.
inline bool routerEquipRequested=false;inline evr::motion::ActionbarSnapshot(*observeActionbar)()=nullptr;
inline evr::motion::WheelSelect wheelSelect{evr::motion::WheelSelectConfig{300000000,330000000,300000000,150000000,1500000000}};
inline evr::motion::WheelSelect::Step selectStep{};inline uint8_t preferredMelee=255;inline unsigned equipPlans=0,equipCompleted=0;
inline std::mutex wheelMutex;inline XINPUT_STATE wheelState{};inline std::atomic<DWORD> wheelObserved{0};
inline void publishWheel(uint16_t buttons,float x,float y){
 std::lock_guard<std::mutex> lock(wheelMutex);
 const auto rx=SHORT(std::lround(std::clamp(x,-1.f,1.f)*32767)),ry=SHORT(std::lround(std::clamp(y,-1.f,1.f)*32767));
 if(wheelState.Gamepad.wButtons==buttons&&wheelState.Gamepad.sThumbRX==rx&&wheelState.Gamepad.sThumbRY==ry&&wheelState.dwPacketNumber)return;
 ++wheelState.dwPacketNumber;wheelState.Gamepad.wButtons=buttons;wheelState.Gamepad.sThumbRX=rx;wheelState.Gamepad.sThumbRY=ry;
}
// Explicit EVR_CONTROLLER_LIVE_1 (2026-10-09, real-device phase 1): continuous play input from the VR
// controllers (live_pad.hpp). The render thread publishes hands/gesture (publishGesture); only this
// frontend thread maps and publishes the pad; XInput polls copy it. No time or frame limit: it ends with
// the session drain (neutral pad polled, synthetic disconnect, table restored). Stale or unavailable
// frames publish a neutral pad instead of failing the session.
inline bool liveRequested=false;inline evr::motion::LivePadMapper liveMapper;
inline std::atomic<bool> liveWieldBusy{true};
inline bool bowInputTrialRequested=false;inline evr::motion::BowInputTrial bowInputTrial;
inline bool bowPhysicalTrialRequested=false;inline evr::motion::BowPhysicalTrial bowPhysicalTrial;
inline bool bowPhysicalReleaseRequested=false;inline evr::motion::BowPhysicalRelease bowPhysicalRelease;
inline bool bowTrackedAimRequested=false;inline std::mutex bowShotMutex;inline evr::motion::BowTrackedShot bowTrackedShot;
// VHVR-style repeatable draw (bow_draw.hpp). Replaces the one-shot release trial on the same route.
inline bool bowDrawRequested=false;inline evr::motion::BowDraw bowDraw;
constexpr uint64_t bowDrawShotMs=500,bowDrawShotFrames=30;
// Mock survey only: left primary face button -> native X (MainhandAction) in gameplay, and the
// Crosshair witness also logs event types other than the bow's spawn event.
inline bool rangedSurveyRequested=false;
// VHVR-style staff and wand (ranged_aim.hpp): shots of the reviewed models leave the
// weapon's emitter along the hand's pointer ray. The render thread publishes the weapon
// pose with the same frame's hands; the native Crosshair call reads the newest value.
// The wand's native attack is X (MainhandAction); the right trigger becomes that press.
inline bool rangedAimRequested=false;
inline evr::motion::BowPhysicalGeometry gestureRangedGeometry;inline uint32_t gestureRangedItem=0;inline uint64_t gestureRangedMs=0;
inline bool gestureRangedTwoHanded=false; // the staff is held with both hands on this frame: it aims along itself (VHVR)
// Live melee (live_melee.hpp): a swing of the shown melee weapon becomes one native attack
// press (X, MainhandAction). The render thread publishes the weapon with its hand frame.
// MeleeNative is read-only evidence of the game's own action sequence for the logs.
inline bool liveMeleeRequested=false;inline evr::motion::LiveMelee liveMelee,liveMeleeLeft;
// The cooldown between attacks (include/evr/live_melee.hpp LiveCooldown; VHVR melee.md 3.6 with the user's 80 % rule of
// 2026-10-12). One for both hands. Its length comes from the action that the stroke started
// (motion_event_selection_observer.hpp tells it here); the hand that struck vibrates while the next stroke would not
// attack yet. EVR_MELEE_COOLDOWN=0 in the game's environment gives the fixed 0.4 s between presses back (for a
// comparison; no launch flag).
inline evr::motion::LiveCooldown liveCooldown;
inline bool liveCooldownEnabled(){static const bool on=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_COOLDOWN",value,sizeof value);return !(n&&n<sizeof value&&value[0]=='0');}();return on;}
struct CooldownMeasure {uint64_t serial=0,ms=0;uint32_t sequence=0;};
inline std::mutex liveCooldownMutex;inline CooldownMeasure liveCooldownMeasured;
inline void measureLiveCooldown(uint64_t serial,uint64_t ms,uint32_t sequence){std::lock_guard<std::mutex> lock(liveCooldownMutex);liveCooldownMeasured={serial,ms,sequence};}
// The press (LiveAttackPress::serial) whose attack the game has begun (motion_event_selection_observer.hpp tells it
// when it takes the action over). A press that never gets here cools down for LiveCooldown's grace time only.
inline std::atomic<uint64_t> liveAttackBegan{0};
// For the render thread, which sends the vibration: until when (GetTickCount64) and which hands (bit 0 left, bit 1 right).
inline std::atomic<uint64_t> cooldownHapticUntil{0};inline std::atomic<unsigned> cooldownHapticHands{0};
constexpr float cooldownHapticAmplitude=.2f;
// The fish tiring (2026-10-19, user: "while I am bringing the fish's endurance gauge down well, let the controller
// vibrate"): live_equipment.hpp sees the fish's endurance (NetworkFishing +0x1c) fall between two reads and keeps the rod's
// hand (the right) vibrating until then. EVR_FISH_GAIN_HAPTIC=0: off.
inline std::atomic<uint64_t> fishGainHapticUntil{0};
// As a pull on the line (user, 18:50: "a pulse as if being pulled - bzz-bzz, bzz-bzz"): two 60 ms buzzes 40 ms apart,
// then 240 ms quiet - every 0.4 s.
constexpr float fishGainHapticAmplitude=.5f;
inline uint64_t fishPulseLeft(uint64_t now){const auto p=now%400;return p<60?60-p:(p>=100&&p<160)?160-p:0;}   // ms of the buzz left now
// VHVR's secondary attack by motion (live_secondary.hpp) is judged for every stroke and written to the record. A stroke
// judged so holds the attack button: the game starts a weapon's heavy attack when the main-hand button is held (Mock
// run game121730: the one-hand sword's sequence 2522099871, the "Heavy" entry of its item data, began 291 ms after
// the press; the short press of an ordinary stroke stays the first attack). 400 ms leaves a margin over that; the
// policy word heavy_hold=<ms> changes it, 0 keeps every stroke a short press.
inline uint64_t liveHeavyHoldMs=400;
// Which stroke is the heavy one (user decision 2026-10-12, after coming home: "hold the right trigger while
// swinging for the heavy attack"): a stroke made with the right trigger held. With a melee weapon in the hand the
// trigger had no meaning of the mod's own - it only passed through as the game's RT - so it is taken away from the
// game there. VHVR's judgement by the motion alone (strong thrust, hook ...) is still worked out and put on record,
// but acts only with the policy word heavy_motion.
inline bool liveHeavyByTrigger=true,liveHeavyByMotion=false;
// What the selected action bar slot holds (hooks/live_actionbar_pages.hpp), also for items that bring no model for the
// hand (bombs, building pieces) and for the empty slot (item 0: bare hands).
struct HandItem {bool valid=false;uint32_t item=0;unsigned count=0;uint8_t category=255,itemSlot=255,weaponType=255;bool typeKnown=false;};
inline HandItem (*handItem)(uint64_t)=nullptr;
// When the player's keen::ecs::NetworkFishing last named a float (live_equipment.hpp reads it about thirty times a
// second): the float is out for as long as this is fresh.
inline std::atomic<uint64_t> fishingFloatMs{0};
// keen::fishing::State of the player's NetworkFishing (live_equipment.hpp, read about 30 times a second) and when it was read:
// 22 AdvancedMinigame .. 30 FinalHook are the fight with a hooked fish (28 DecideOutbreak, 29 Outbreak: the fish pulls aside).
inline std::atomic<int> fishingState{-1};inline std::atomic<uint64_t> fishingStateMs{0};
// The same component's outbreakDirection (0 none, 1 left, 2 right) and minigameInputState (0 none, 1 the steering is the
// right way, 2 the wrong way) - the game's own judgment of the steering (live_equipment.hpp, 2026-10-19).
inline std::atomic<int> fishingDirection{0},fishingInputState{0};
inline std::atomic<uint32_t> fishingFishId{0};
// keen::ecs::NetworkFishing's component id and its World (live_equipment.hpp): other players' fishing is read by it (remote_body.hpp).
inline std::atomic<int> fishingComponentType{-1};inline std::atomic<uintptr_t> fishingComponentWorld{0};   // NetworkFishing +0x34: the hooked / caught fish's entity (live_equipment.hpp)
inline std::atomic<uint64_t> castTapAt{0};   // when the trigger's release last tapped the cast (the rod's flick waits a second after it)
// Set at install (view_resource_probe.cpp): until then the game's orbit input is no turn of the view (camera_control_observer.hpp).
inline void (*holdTurn)(uint64_t until)=nullptr;
inline bool fishFight(uint64_t now){const auto at=fishingStateMs.load();const int s=fishingState.load();return at&&now>=at&&now-at<=300&&s>=22&&s<=30;}
inline bool fishingFloatOut(uint64_t now){const auto at=fishingFloatMs.load();return at&&now>=at&&now-at<=150;}
// The holsters (include/evr/holster.hpp, hooks/holster_actions.hpp): a grip pressed at its own shoulder or at the left
// hip. holsterAct: true when the game was asked for something - only then the hand vibrates (VHVR Pose.cs:823-832:
// 0.2 s at 0.3; user: "a weak vibration when the grip has put something on or off, none for only reaching in").
inline bool holsterRequested=false;inline bool (*holsterAct)(unsigned,uint64_t)=nullptr;inline void (*holsterTick)(uint64_t)=nullptr;
// In the water no holster press is looked for (VHVR Pose.cs:42-49: swimming skips the gesture detection; 2026-10-18).
inline bool (*actorSwimming)(uint64_t)=nullptr;
// Which: 1 FloatingOnWater (43), 2 UnderWater (44), 3 both, 0 neither or not known (2026-10-20, diving by the strokes).
inline unsigned (*actorWater)(uint64_t)=nullptr;
// A fallen player within reach of a revive (live_melee_targets.hpp downedNear): the right B then holds the game's
// interaction for as long as it is held instead of tapping it or opening the quick wheel (include/evr/quick_slot.hpp).
inline bool (*fallenPlayerNear)(uint64_t)=nullptr;
// While a holster acts the block is let go: the game does not change the action bar's selection while the player
// blocks, and takes the request as done (headset run 20261014-162900: the sword could not be put away at the right
// shoulder six times in a row while the shield was held up; holster_act frames 6603..6869).
inline std::atomic<uint64_t> holsterGuardYieldUntil{0};
inline evr::motion::HolsterGrips holsterGrips;
// Sneaking by crouching and the view reset pose (user, 2026-10-18 morning; VHVR locomotion_input.md 3.6 and 2.9,
// include/evr/room_sneak.hpp). The render thread tells the head's height at the view's origin and the standing eye
// height with each new origin (publishOrigin, view_resource_probe.cpp beginLiveFrame); a held pose asks it for a new
// origin (recenterRequests). sneakState (set by the policy): the actor's InSneakMode, whether the player's settings
// switch the sneak with each press, and whether the character can sneak now (not swimming, sitting, climbing ...).
inline bool roomSneakRequested=false,recenterPoseRequested=false;
// Real crouching on / off and its threshold (percent of the standing eye height the headset comes down), changed by the
// VR settings in the start menu while the game runs (hooks/bug_report.hpp; 2026-10-18).
inline std::atomic<bool> roomSneakOn{true};inline std::atomic<int> crouchDrop{30};
// The speed a stroke needs to attack, percent of the mod's own (include/evr/vr_settings.hpp swingSpeed), and swimming by
// the arms on / off (include/evr/swim_stroke.hpp); both changed in the start menu's VR settings too (2026-10-20).
inline std::atomic<int> swingPercent{100};inline std::atomic<bool> armSwimOn{true};
// When the last stroke pressed the attack (GetTickCount64; motion_collider_apply.hpp replayCurrent, 2026-10-20).
inline std::atomic<uint64_t> strokeMs{0};
inline evr::motion::SwimStroke swimStroke;
inline std::atomic<uint64_t> recenterRequests{0},originEpoch{0};inline std::atomic<float> originHeadY{0},originReference{1.65f};
inline void publishOrigin(float headY,float reference,uint64_t epoch){originHeadY.store(headY);originReference.store(reference);originEpoch.store(epoch);}
inline bool (*sneakState)(uint64_t now,bool& sneaking,bool& toggle,bool& allowed)=nullptr;
inline evr::motion::RoomCrouch roomCrouch;inline evr::motion::SneakPresser sneakPresser;inline evr::motion::RecenterPose recenterPose;
// Live held items: the right trigger is the game's use button for what is used that way (hand tools of the right hand
// slot, building pieces; VHVR uses tools and building with the trigger), and a throwable is thrown by the hand's
// movement (VHVR ThrowableManager, live_throw.hpp).
inline bool liveHandUseRequested=false;inline evr::motion::LiveThrow liveThrow;
// The throw's guide from the left hand (2026-10-18, include/evr/live_throw.hpp ThrowGuide), drawn by the layer (evrSetThrowGuide).
inline evr::motion::ThrowGuide throwGuide;using ThrowGuideFn=int(*)(const float*,uint32_t,int);inline ThrowGuideFn setThrowGuide=nullptr;inline bool throwGuideShown=false;
// A throw waits in tracking space for the render thread to put it into the world; that world line waits for the game
// to spawn the thrown item (bow_spawn_observer.hpp).
struct ThrowRequest {uint64_t serial=0,milliseconds=0,frame=0;evr::Vec3 origin{},direction{};float speed=0;};
struct ThrowWorldAim {uint64_t serial=0,milliseconds=0;evr::motion::BowWorldAim aim;bool used=false;};
inline std::mutex throwMutex;inline ThrowRequest throwRequest;inline ThrowWorldAim throwWorldAim;
// The hand of the latest stroke that became an attack (1 right, 0 left) and when: the attack's hit volume belongs to
// the weapon of that hand (motion_visual_bridge.hpp).
inline std::atomic<uint64_t> liveStrikeLeftMs{0},liveStrikeRightMs{0};
// HUD auto-hide: whether the game shows the HUD of ordinary play (not a menu, the quick wheel or building mode), and when
// that was seen last.
inline std::atomic<bool> liveScreenGameplay{false};inline std::atomic<uint64_t> liveScreenAt{0};
// VHVR: a stroke attacks only when the weapon reaches a target. targetNear (live_melee_targets.hpp, set by the policy
// word "targets") says whether the blade is near something that has Health; a tool needs no target.
struct TargetNear {bool reached=true,gated=false;uint32_t entity=0;float distance=0;unsigned cached=0;bool ahead=false;float nearest=-1;bool airborne=false;};
inline bool liveMeleeTargetGate=false;inline TargetNear(*targetNear)(const evr::motion::LiveMeleeWeapon&,uint64_t)=nullptr;
// A stroke in the air (jumping, falling) attacks without reaching a target (2026-10-15, user: "a swing of the sword while
// jumping gives no attack"; Mock game-20261015-040910: the game starts the attack in the air as on the ground, so what
// kept it out on the headset is this gate - the blade above the enemies' bodies). VHVR strikes in the air as well. The
// airborne state is the actor's (Grounded bit off, input_head_idle.hpp's latest). EVR_MELEE_AIR_FREE=0 keeps the gate.
inline bool (*actorAirborne)(uint64_t)=nullptr;
// The ordinary stroke in the air: the skilled Primary follow-ups (the jump attacks) are emptied so that the press
// attacks the ordinary way (motion_event_selection_observer.hpp, clearSkilledPrimaryOpportunities; 2026-10-15).
// The airborne press just made and the kind its stroke asks for (+1 the rising slash, -1 the crash-down): read by
// sequence_request_observer.hpp within 250 ms, where the game's request of the attack's sequence is turned to that kind.
namespace airborneRequest {inline std::atomic<int> kind{0};inline std::atomic<uint8_t> weaponType{255};inline std::atomic<uint64_t> at{0};}
inline unsigned (*clearAirborneOpportunities)(int kind,uint8_t weaponType)=nullptr;   // shapes the airborne attack slots: +1 the rising slash, -1 the crash-down, 0 the game's choice
inline const bool liveMeleeAirFree=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_MELEE_AIR_FREE",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline bool airborneNow(uint64_t now){return liveMeleeAirFree&&actorAirborne&&actorAirborne(now);}
inline evr::motion::LiveMeleeWeapon gestureMeleeWeapon,gestureOffhandWeapon;inline uint64_t gestureMeleeFrame=0,gestureMeleeMs=0,gestureOffhandFrame=0,gestureOffhandMs=0;
struct MeleeNative {bool valid=false,active=false,canPrimary=false,canBlock=false,blocking=false;uint32_t usedItem=0,sequence=0,entity=0;int64_t triggerTime=0;uint64_t digital=0;};
inline MeleeNative(*observeMeleeNative)()=nullptr;
// The swing that pressed the attack button: identifies the action it starts for the early Impact of live play.
inline std::mutex livePressMutex;inline evr::motion::LiveAttackPress liveAttackPress;
inline evr::motion::LiveAttackPress livePressSnapshot(){std::lock_guard<std::mutex> lock(livePressMutex);return liveAttackPress;}
// Live guard (live_guard.hpp): a raised shield, or a two-hand weapon held across, holds the
// native block button (LB). The render thread publishes what is equipped with its hand frame.
inline bool liveGuardRequested=false;inline evr::motion::LiveGuard liveGuard;
// Quick slot by hand motion (quick_slot.hpp): right B held opens the native wheel, the right hand points, release takes.
inline bool quickSlotRequested=false;inline evr::motion::QuickSlot quickSlot;
inline evr::motion::LiveGuardEquipment gestureGuardEquipment;inline uint64_t gestureGuardFrame=0,gestureGuardMs=0;
// VHVR bow pulses: full draw = bow hand 0.2 s at 0.3; release = arrow hand 0.1 s at 0.9
// and bow hand 0.2 s at 0.3. The frontend requests, the render thread sends.
struct BowHaptic {uint64_t sequence=0;std::array<float,2> amplitude{};std::array<uint32_t,2> milliseconds{};};
inline std::mutex bowHapticMutex;inline BowHaptic bowHaptic;
inline void requestBowHaptic(std::array<float,2> amplitude,std::array<uint32_t,2> milliseconds){
 std::lock_guard<std::mutex> lock(bowHapticMutex);bowHaptic={bowHaptic.sequence+1,amplitude,milliseconds};
}
inline void applyBowHaptic(evr::motion::GestureFeedbackResult& command,const evr::motion::HandFrame& hands,uint64_t now){
 static uint64_t seen=0;static std::array<uint64_t,2> until{};static std::array<float,2> amplitude{};
 BowHaptic request;{std::lock_guard<std::mutex> lock(bowHapticMutex);request=bowHaptic;}
 if(request.sequence!=seen){seen=request.sequence;
  for(unsigned h=0;h<2;h++)if(request.amplitude[h]>0&&request.milliseconds[h]){until[h]=now+request.milliseconds[h];amplitude[h]=request.amplitude[h];}}
 if(!command.fresh)return;
 for(unsigned h=0;h<2;h++){
  if(until[h]<=now||!hands.focused||!hands.hands[h].tracked()){until[h]=0;continue;}
  if(!command.amplitude[h])command.duration[h]=XrDuration(std::min<uint64_t>(until[h]-now,100))*1000000;
  command.amplitude[h]=std::max(command.amplitude[h],amplitude[h]);
 }
 // The cooldown of a stroke: a steady, weaker vibration of the hand that struck until the next stroke can attack.
 const auto cooling=cooldownHapticUntil.load();const auto which=cooldownHapticHands.load();
 if(cooling>now)for(unsigned h=0;h<2;h++){
  if(!((which>>h)&1)||!hands.focused||!hands.hands[h].tracked())continue;
  if(!command.amplitude[h])command.duration[h]=XrDuration(std::min<uint64_t>(cooling-now,100))*1000000;
  command.amplitude[h]=std::max(command.amplitude[h],cooldownHapticAmplitude);
 }
 const auto tiring=fishGainHapticUntil.load();
 if(tiring>now&&hands.focused&&hands.hands[1].tracked()){
  if(const auto left=fishPulseLeft(now)){
   if(!command.amplitude[1])command.duration[1]=XrDuration(std::min<uint64_t>(left,tiring-now))*1000000;
   command.amplitude[1]=std::max(command.amplitude[1],fishGainHapticAmplitude);}
 }
}
inline evr::motion::BowPhysicalGeometry gestureBowGeometry;
struct BowPhysicalPublished {uint64_t binding=0,frame=0,milliseconds=0;bool held=false;};
inline std::mutex bowPhysicalMutex;inline BowPhysicalPublished bowPhysicalPublished;
inline std::mutex liveMutex;inline XINPUT_STATE liveState{};inline std::atomic<DWORD> liveObserved{0};
inline uint64_t liveLastFrame=0,liveSamples=0,liveStaleMs=0;inline int liveLastScreen=-1;inline bool liveConnectionWarned=false;
inline evr::motion::LivePadState liveLogged{};
inline void publishLive(const evr::motion::LivePadState& p){
 std::lock_guard<std::mutex> lock(liveMutex);auto& g=liveState.Gamepad;
 const evr::motion::LivePadState current{g.wButtons,g.sThumbLX,g.sThumbLY,g.sThumbRX,g.sThumbRY,g.bLeftTrigger,g.bRightTrigger};
 if(current==p&&liveState.dwPacketNumber)return;
 ++liveState.dwPacketNumber;g.wButtons=p.buttons;g.sThumbLX=p.leftX;g.sThumbLY=p.leftY;g.sThumbRX=p.rightX;g.sThumbRY=p.rightY;
 g.bLeftTrigger=p.leftTrigger;g.bRightTrigger=p.rightTrigger;
}
// Real-device run game-20261009-113310: at the session end the game window was not in front (the user had opened
// SteamVR's VR view), and the game polls XInput only while focused, so the neutral pad could not be polled. When the
// game has not polled for 500 ms it is not reading input at all; the table is then restored without a polled neutral
// (the game reads the real, disconnected port once it is focused again) and the reason is logged.
inline std::atomic<uint64_t> liveLastPollMs{0};inline bool liveReleasedUnpolled=false;
// Early Impact (live melee contact): the game starts the attack when the button comes up again, so the tap ends as
// soon as the game has read it pressed (meleeTapPolls reads) instead of after the fixed 120 ms.
inline bool liveMeleeQuickRelease=false;inline std::atomic<uint64_t> liveAttackPolls{0};inline unsigned meleeTapPolls=1;
inline bool liveNeutralPolled(){std::lock_guard<std::mutex> lock(liveMutex);return liveObserved.load()==liveState.dwPacketNumber;}
inline bool verticalRequested=false;inline evr::motion::VerticalStickMapping verticalMapper;inline uint64_t lastVerticalFrame=0;
inline evr::motion::HandFrame gestureHands;
inline evr::motion::HeadGesture gestureMapper;inline evr::motion::GestureFeedback gestureFeedback;
inline std::mutex gestureMutex;inline evr::motion::HeadGestureResult gestureResult;inline uint64_t gestureMs=0;
inline evr::motion::GestureHead gestureHead;
inline bool gestureWieldGrip=false;inline uint64_t gestureBowBinding=0;
inline void publishGuard(const evr::motion::LiveGuardEquipment& equipment,uint64_t frame,uint64_t now){
 std::lock_guard<std::mutex> lock(gestureMutex);gestureGuardEquipment=equipment;gestureGuardFrame=frame;gestureGuardMs=equipment.binding?now:0;
}
// Where the fishing rod's cast goes (VHVR FishingManager: the direction of the hand's movement). The game sends the
// float along the camera of the sent input: server_player_fishing (31db90) takes the forward of the PlayerInput
// camera in 30faa0 (the quaternion at fromClient+38) and the distance from the charge. In the headset that camera is
// the head, so the float went where the player looked, not where the rod was swung (headset run game145015: "it does
// cast, but not in the direction of the swing"). For a short while after a cast by a swing the camera of the sent
// input looks along the swing instead (input_head_idle.hpp). direction: tracking space, unit, as LiveSwingCast gives it.
struct FishingAim {uint64_t until=0,serial=0;evr::Vec3 direction{};};
// The guard's direction while the live guard holds the block (tracking space, level): input_head_idle.hpp sends the
// camera this way, and bodyHeading (guard only) makes the character look along it. Under fishingAimMutex.
inline FishingAim guardAim;
inline FishingAim fishingAim;inline std::mutex fishingAimMutex;
// The bow's shot as the sent camera (2026-10-19): from the release for sentMs the input this player's game sends carries
// the camera the shot needs (bow_aim.hpp bowAimCameraKnownMuzzle) - the server that makes the arrow, this PC's or the
// host's, takes the arrow's line from it. Under fishingAimMutex.
struct BowAim {uint64_t until=0,serial=0;std::array<uint8_t,40> camera{};};inline BowAim bowAim;
// The strength of that cast (the stroke's peak speed of the rod's tip / 5 m/s, VHVR FULL_THROW_SPEED) for the game's
// charge, which decides how far the float flies (fishing_charge.hpp). Told for every cast by a swing, also when the
// swing does not tell the direction. Under fishingAimMutex.
struct FishingSwing {uint64_t until=0,serial=0;float strength=0;};
inline FishingSwing fishingSwing;
// The fishing rod shown with this frame's hands (live_swing_cast.hpp); empty when there is none.
inline evr::motion::LiveSwingRod gestureRod;inline uint64_t gestureRodMs=0;
inline void publishRod(const evr::motion::LiveSwingRod& rod,uint64_t now){
 std::lock_guard<std::mutex> lock(gestureMutex);gestureRod=rod;gestureRodMs=rod.binding?now:0;
}
inline void publishMelee(const evr::motion::LiveMeleeWeapon& weapon,uint64_t frame,uint64_t now){
 std::lock_guard<std::mutex> lock(gestureMutex);gestureMeleeWeapon=weapon;gestureMeleeFrame=frame;gestureMeleeMs=weapon.binding?now:0;
}
// The left-hand part of a weapon pair (hand 0), with that same frame's pose; empty when there is none.
inline void publishOffhandMelee(const evr::motion::LiveMeleeWeapon& weapon,uint64_t frame,uint64_t now){
 std::lock_guard<std::mutex> lock(gestureMutex);gestureOffhandWeapon=weapon;gestureOffhandFrame=frame;gestureOffhandMs=weapon.binding?now:0;
}
// Called by the render thread right after publishGesture, with that same frame's weapon pose.
inline void publishRanged(const evr::motion::BowPhysicalGeometry& geometry,uint32_t item,bool twoHanded,uint64_t now){
 std::lock_guard<std::mutex> lock(gestureMutex);gestureRangedGeometry=geometry;gestureRangedItem=item;gestureRangedTwoHanded=item&&twoHanded;gestureRangedMs=item?now:0;
}
inline evr::motion::HeadGestureResult publishGesture(const evr::motion::HandFrame& f,const evr::motion::GestureHead& head,uint64_t now,bool wieldGrip=false,uint64_t bowBinding=0,evr::motion::BowPhysicalGeometry bowGeometry={}){
 evr::motion::GesturePad pad;
 for(unsigned h=0;h<2;h++){
  const auto& s=f.hands[h];if(s.stickActive)(h?pad.right:pad.left)=s.stick;
  if(s.stickClickActive&&s.stickClick)pad.buttons|=h?0x80:0x40;
  (h?pad.rightTrigger:pad.leftTrigger)=s.trigger;
 }
 auto guarded=head;
 if(!f.hands[0].stickActive||!f.hands[1].stickActive||!f.hands[0].stickClickActive||!f.hands[1].stickClickActive)guarded.flags=0;
 auto value=gestureMapper.update(f,guarded,pad);
 if(value.pad.left.x||value.pad.left.y||(!wheelRequested&&value.pad.right.x)||(!verticalRequested&&!wheelRequested&&value.pad.right.y)||value.pad.leftTrigger||value.pad.rightTrigger||(value.pad.buttons&~uint16_t(0x3f)))
  if(!liveRequested)throw std::runtime_error("bounded gesture fixture rejects movement/attack/non-menu controls");
 {std::lock_guard<std::mutex> lock(gestureMutex);gestureResult=value;gestureHands=f;gestureHead=head;gestureMs=now;gestureWieldGrip=wieldGrip;gestureBowBinding=bowBinding;gestureBowGeometry=bowGeometry;}
 std::ostringstream fields;fields<<std::setprecision(10)<<"\"frame\":"<<f.frame<<",\"time\":"<<f.predictedTime
  <<",\"session\":"<<f.sessionGeneration<<",\"origin\":"<<f.originGeneration<<",\"active\":"<<(value.active?"true":"false")
  <<",\"entered\":"<<(value.entered?"true":"false")<<",\"left\":"<<(value.left?"true":"false")<<",\"valid\":"<<(value.valid?"true":"false")
  <<",\"reset_swing\":"<<(value.resetSwing?"true":"false")<<",\"mapped_buttons\":"<<value.mappedButtons<<",\"buttons\":"<<value.pad.buttons<<",\"raw_buttons\":"<<pad.buttons
  <<",\"raw_right\":["<<pad.right.x<<','<<pad.right.y<<"],\"hand_in_head\":["<<value.handInHead.x<<','<<value.handInHead.y<<','<<value.handInHead.z
  <<"],\"left_hand_in_head\":["<<value.leftHandInHead.x<<','<<value.leftHandInHead.y<<','<<value.leftHandInHead.z
  <<"],\"left_tracked\":"<<(value.leftTracked?"true":"false")<<",\"active_hands\":"<<unsigned(value.activeHands)<<",\"entered_hand\":"<<value.enteredHand
  <<",\"pulse\":"<<value.pulseAmplitude<<",\"pulse_duration\":"<<value.pulseDuration;
 logFile->event("head_gesture_sample",fields.str());return value;
}
inline bool requested=false,menuRequested=false,tabsRequested=false,altarRequested=false,motionRequested=false,doubleJumpRequested=false,glideRequested=false;
inline std::atomic<bool> liveGuardHeld{false};
// Building mode: the game's HudScreenType BlueprintMultiTool - 28 until June, 29 in 1.0 (LogicEditor came in at 20; headset
// report 19 of 2026-10-17, "the quick slot in building mode works differently": the building controls never applied).
constexpr uint8_t buildingScreen=29;
// The body turned to the head when a stroke attacks (2026-10-17, headset report 6: "the airborne attack moves toward where I
// last walked, not where the head looks"; Mock game-20261017-032221: the jump attack went the way the body faced, and the
// body faces the last walk, look mode 1): bodyHeading (input_actor_observer.hpp) presents the camera's mode (the head's
// look) from the stroke's press for this long, and all through the air with a melee weapon. Off by default: it did not
// turn the attack (Mock game-20261017-033212, from the press only: the body 0 -> -14 degrees with the head at -90;
// game-20261017-033753, through the jump as well: the body did not turn in the air at all, then -14 at the attack's start).
// EVR_ATTACK_FACING_MS=<ms> turns the experiment on.
inline const uint64_t attackFacingMs=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_ATTACK_FACING_MS",v,sizeof v);return n&&n<sizeof v?uint64_t(std::strtoull(v,nullptr,10)):0ull;}();
inline std::atomic<uint64_t> attackFacingUntil{0};
// The airborne attack goes the way of the left stick at its start, and the body turns there at once (Mock
// game-20261017-043317: the stick forward with the head turned 90 degrees - the jump attack went toward the head, -90 degrees,
// the body with it; game-20261017-033212 without the stick: the way the body had last faced). The headset's stick is turned
// by the head (head-based walking), so from an airborne stroke's press an idle stick is pushed fully forward for this long:
// the attack goes where the head looks (headset report 6 of 2026-10-17). A stick that is held keeps its own way.
// EVR_AIR_ATTACK_STICK_MS=0 leaves the stick alone.
inline const uint64_t airAttackStickMs=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_AIR_ATTACK_STICK_MS",v,sizeof v);return n&&n<sizeof v?uint64_t(std::strtoull(v,nullptr,10)):250ull;}();
inline std::atomic<uint64_t> airAttackStickUntil{0};
inline const bool airDownHeavy=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_AIR_DOWN_HEAVY",v,sizeof v);return !(n==1&&v[0]=='0');}();
// A skill attack on the ground goes where the head looks as well (1.0 attack spec, user 2026-10-17: "the direction of the
// move is the head's or the swing's"): when a swing has started one (motion_event_selection_observer.hpp, skillAction) the
// idle stick is pushed forward for the same time. EVR_SKILL_FACING=0 leaves it alone.
inline const bool skillFacing=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SKILL_FACING",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline void skillBegan(uint64_t now){if(skillFacing&&airAttackStickMs&&airAttackStickUntil.load()<now+airAttackStickMs)airAttackStickUntil=now+airAttackStickMs;}
// The local player's effective actor state and when it was read (input_actor_observer.hpp): the live guard's parry
// waits for its ParryOportunity (bit 4).
// The local player's BaseActor (input_actor_observer.hpp) and the time of the last swing parry of the live guard
// (live_guard.hpp swingParry), for the game's parry (parry_observer.hpp).
inline std::atomic<uintptr_t> playerBaseActor{0};inline std::atomic<uint64_t> swingParryAt{0},guardRepressAt{0};
// wandNextShotMs: no swing of the wand shoots before it (motion_event_selection_observer.hpp sets it when a swing's action starts).
inline std::atomic<uint64_t> wandNextShotMs{0};
inline std::atomic<uint64_t> playerActorState{0},playerActorStateMs{0},blockOpportunityMs{0};   // blockOpportunityMs: the last tick the player's BlockOpportunity (5) was up (parry_observer.hpp relaxed parry)   // the block button is held by the live guard (input_actor_observer.hpp records the actor's look then)
inline evr::game::ControllerMotionFixture motion;
inline evr::game::MotionObservation(*observeMotion)()=nullptr;
inline bool attackRequested=false,attackCompleted=false;
inline uint16_t attackButtons=0;
inline bool motionAttackRequested=false,motionAttackCompleted=false;
inline bool twoHandAttackRequested=false;
inline evr::motion::Weapon twoHandSupportWeapon; // Immutable after setup, before hooks start.
inline bool twoHandSupport(const evr::motion::HandFrame& h){return twoHandAttackRequested&&evr::motion::twoHandSupportValid(h,twoHandSupportWeapon); }
inline unsigned motionAttackLimit=1;
inline bool trackingLossExpected=false,trackingLossObserved=false;
inline bool offhandReleaseExpected=false,offhandReleaseObserved=false;
inline bool epochCloseExpected=false;
inline std::atomic<bool> epochCloseObserved{false},epochStopped{false};
inline bool epochCoordinatorStopped=false;
inline evr::motion::CombatEpochState(*observeEpoch)()=nullptr;
inline unsigned expectedNativeActions(){return epochCloseExpected?1:motionAttackLimit;}
inline bool expectedMotionCancellation(){return (trackingLossExpected&&trackingLossObserved)||(offhandReleaseExpected&&offhandReleaseObserved)||(epochCloseExpected&&epochCloseObserved.load());}
inline evr::game::MotionAttackFixture swing{{2.f,.3f,.6f,2,100000000,12.f,30.f}};
inline std::unique_ptr<evr::game::MeleeSessionFixture> meleeSession;
inline evr::game::MotionCombatObservation(*observeCombat)()=nullptr;
inline evr::motion::Feedback handFeedback;
inline std::mutex handMutex;inline evr::motion::HandFrame handFrame;inline evr::motion::HeadGestureResult handGesture;inline uint64_t handMs=0,lastLoggedHand=0;
inline void publishHands(const evr::motion::HandFrame& f,uint64_t now,const evr::motion::HeadGestureResult* gesture=nullptr){if(!motionAttackRequested)return;std::lock_guard<std::mutex> lock(handMutex);
 if(!handMs||handFrame.hands[1].gripFlags!=f.hands[1].gripFlags||handFrame.hands[1].gripActive!=f.hands[1].gripActive||handFrame.focused!=f.focused)
  logFile->event("motion_tracking_sample","\"frame\":"+std::to_string(f.frame)+",\"session\":"+std::to_string(f.sessionGeneration)+",\"origin\":"+std::to_string(f.originGeneration)+",\"grip_flags\":"+std::to_string(f.hands[1].gripFlags)+",\"grip_active\":"+(f.hands[1].gripActive?"true":"false")+",\"tracked\":"+(f.hands[1].tracked()?"true":"false")+",\"focused\":"+(f.focused?"true":"false"));
 if(twoHandAttackRequested&&(!handMs||handFrame.hands[0].grab!=f.hands[0].grab||handFrame.hands[0].grabActive!=f.hands[0].grabActive||handFrame.hands[0].gripFlags!=f.hands[0].gripFlags)){
  std::ostringstream out;out<<"\"frame\":"<<f.frame<<",\"left_grab\":"<<(f.hands[0].grab?"true":"false")<<",\"left_grab_active\":"<<(f.hands[0].grabActive?"true":"false")<<",\"left_tracked\":"<<(f.hands[0].tracked()?"true":"false")<<",\"right_tracked\":"<<(f.hands[1].tracked()?"true":"false")<<",\"focused\":"<<(f.focused?"true":"false");logFile->event("motion_support_sample",out.str());
 }
 handFrame=f;handMs=now;handGesture=gesture?*gesture:evr::motion::HeadGestureResult{};}

inline evr::game::ControllerAttackFixture attack;
inline std::mutex attackTicketMutex;inline evr::motion::NativeAttackTicket attackTicket;inline uint64_t attackTicketSerial=0,attackTicketRequest=0;
inline evr::motion::NativeAttackTicket ticketSnapshot(){std::lock_guard<std::mutex> lock(attackTicketMutex);return attackTicket;}
inline void revokeTicket(){std::lock_guard<std::mutex> lock(attackTicketMutex);attackTicket.revoked=true;}
inline void invalidateTicket(){std::lock_guard<std::mutex> lock(attackTicketMutex);attackTicket.valid=false;}

inline evr::game::AttackObservation(*observeAttack)()=nullptr;
inline bool(*epochAllowsPoll)()=nullptr;
inline bool(*epochAllowsTicket)(uint64_t)=nullptr;
inline std::string(*epochContextFields)(const evr::motion::CombatWorlds&)=nullptr;
inline evr::motion::EpochAttackPacket attackPacket;
inline void publishAttack(uint8_t trigger){attackPacket.publish(trigger);}
inline bool motionCompleted=false;inline uint64_t lastMotionRoot=0;
struct MotionActor {uint32_t entity=0;uint64_t state=0,milliseconds=0,digital=0;};
inline std::mutex motionActorMutex;inline MotionActor motionActor;
inline void publishMotionActor(uint32_t entity,uint64_t state,uint64_t digital=0){std::lock_guard<std::mutex> lock(motionActorMutex);motionActor={entity,state,GetTickCount64(),digital};}
inline int(*observeCamera)()=nullptr;inline int lastCamera=-1;
inline std::atomic<bool> armed{true};
using CaptureFn=int(*)(uint32_t);
inline CaptureFn requestXrCapture=nullptr,requestXrBurst=nullptr;
inline uint32_t motionBurstFrames=0;
inline evr::game::ControllerMenuFixture menu;
inline std::atomic<uint64_t> packetButtons{1ull<<16},observedPacket{0};
inline bool menuCompleted=false;
inline unsigned emittedTransitions=0;
inline void publishButtons(uint16_t buttons){
    const auto previous=packetButtons.load();
    if(uint16_t(previous)==buttons)return;
    packetButtons.store((((previous>>16)+1)<<16)|buttons);++emittedTransitions;
}
inline std::string neutralLabel(){return liveRequested||menuRequested||motionRequested||attackRequested||motionAttackRequested||gestureRequested||equipmentRequested||guardRequested?"false":"true";}
// Substituted only by the standalone test executable; no runtime configuration.
inline auto resolveExport=&GetProcAddress;
inline auto clockNow=&GetTickCount64;
using RescanFn=bool(__fastcall*)(void*,const void*);
inline RescanFn nativeDeviceEvent=nullptr;
inline bool nativeConnectedSeen=false,nativeDisconnectedSeen=false;
inline int lastNativeConnection=-1;
inline std::atomic<bool> failureReported{false};
inline std::atomic<bool> active{false},connected{false},stop{false},done{false},failed{false},installed{false};
inline std::atomic<unsigned> callbacks{0},states{0},capabilities{0},rumbles{0},disconnects{0};
inline uintptr_t backend=0,manager=0;inline HMODULE module=nullptr;
inline StateFn nativeState=nullptr;inline CapsFn nativeCaps=nullptr;inline RumbleFn nativeRumble=nullptr;
inline DWORD port=0;inline uint64_t begin=0,disconnectAt=0,releaseAt=0;
// Explicit owner-exit adapter. The native hook must call begin only at the
// verified dac6c0 entry and finish only after that same original call returns.
// Explicit isolated or completed-action terminal-owner trials only.
// Active XR/combat exit remains disabled until the other consumers can drain.
inline bool backendExitRequested=false,inputModulePinned=false,holdConnectionOnEpochStop=false;
inline std::atomic<bool> backendWaitReady{false},readersStopped{false};
inline auto pinInputModule=&GetModuleHandleExW;
inline std::atomic<bool> backendExitBegun{false},backendExitReturned{false};
inline bool backendExitTableRestored=false;
inline uint64_t ownerSerial=0,nextOwnerSerial=0;inline DWORD ownerThread=0;
struct BackendExitToken {uint64_t serial=0;uintptr_t backend=0,manager=0,module=0;DWORD thread=0;
 bool valid()const{return serial&&backend&&manager&&module&&thread;}
 bool operator==(const BackendExitToken& x)const{return serial==x.serial&&backend==x.backend&&manager==x.manager&&module==x.module&&thread==x.thread;}
};
inline BackendExitToken pendingBackendExit;

struct Callback {Callback(){++callbacks;}~Callback(){--callbacks;}};
// Only MOD-owned values change here. XInput poll and frontend owner may both
// reach it. No native Actor/World pointer is read, no ledger is cleared.
inline void checkEpochStop(){
 // Once latched, the input output stays neutral even after the owning broker
 // leases are released. Never sample a destroyed gameplay context to stop twice.
 if(readersStopped.load()||epochStopped.load()||!motionAttackRequested||!observeEpoch)return;
 auto value=evr::motion::CombatEpochState::Unavailable;
 try{value=observeEpoch();}catch(...){}
 if(value==evr::motion::CombatEpochState::Open)return;
 const auto packet=attackPacket.stop();revokeTicket();stop=true;
 {std::lock_guard<std::mutex> lock(handMutex);handFeedback={};}
 if(value==evr::motion::CombatEpochState::Unavailable)failed=true;
 if(!epochStopped.exchange(true)){
  const bool expected=epochCloseExpected&&value==evr::motion::CombatEpochState::Closed;
  epochCloseObserved=expected;if(!expected)failed=true;
  try{logFile->event("controller_world_epoch_stopped","\"expected\":"+std::string(expected?"true":"false")+",\"state\":"+std::to_string(unsigned(value))+",\"neutral_packet\":"+std::to_string(packet>>8)+",\"ticket_revoked\":true,\"ledgers_retained\":true,\"thread\":"+std::to_string(GetCurrentThreadId()));}catch(...){failed=true;}
 }
}
// A real pad plugged in (2026-10-19, a player's logs EnshroudedVR-logs-20261019-001026: "with an Xbox controller connected
// the buttons did nothing" - controller_fixture_failure "neutral fixture needs all ports disconnected" on every start with
// it, and the fixture connected once it was unplugged): the mod's pad was only put in while all four ports were empty, and
// a pad arriving later stopped it. Now, while the mod's pad is in, the game sees port 0 as the mod's pad and the other ports
// as empty - real pads are hidden from the game (the motion controllers are the input in VR; Steam Input or a real pad
// may sit on any port). EVR_HIDE_PHYSICAL_PADS=0: as before (a real pad stops the mod's pad).
inline const bool hidePhysicalPads=[]{char v[4]{};return !(GetEnvironmentVariableA("EVR_HIDE_PHYSICAL_PADS",v,sizeof v)==1&&v[0]=='0');}();
inline std::atomic<unsigned> physicalHidden{0};
// Test (Mock has no real pad): EVR_TEST_PHYSICAL_PAD=<0..3> makes that port answer as a connected pad with nothing pressed.
inline const int testPhysicalPort=[]{char v[4]{};return GetEnvironmentVariableA("EVR_TEST_PHYSICAL_PAD",v,sizeof v)==1&&v[0]>='0'&&v[0]<='3'?v[0]-'0':-1;}();
template<class T> inline DWORD testPhysical(DWORD user,DWORD result,T* output){
    if(int(user)!=testPhysicalPort||result!=ERROR_DEVICE_NOT_CONNECTED)return result;if(output)*output={};return ERROR_SUCCESS;}
inline DWORD WINAPI state(DWORD user,XINPUT_STATE* output){
    const Callback scope;
    const auto result=testPhysical(user,nativeState(user,output),output);
    const ObserverLastError preserve;
    if(!active.load())return result;
    if(user!=port){if(!hidePhysicalPads||result==ERROR_DEVICE_NOT_CONNECTED)return result;++physicalHidden;if(output)*output={};return ERROR_DEVICE_NOT_CONNECTED;}
    if(result!=ERROR_DEVICE_NOT_CONNECTED){if(!hidePhysicalPads){failed=true;active=false;stop=true;return result;}++physicalHidden;}
    ++states;if(!connected.load()){++disconnects;if(hidePhysicalPads&&result!=ERROR_DEVICE_NOT_CONNECTED){if(output)*output={};return ERROR_DEVICE_NOT_CONNECTED;}return result;}   // leaving: a hidden real pad stays hidden, so the game sees the mod's pad go
    if(!output)return ERROR_BAD_ARGUMENTS;
    if(liveRequested){
        if(stop.load())publishLive({});
        {std::lock_guard<std::mutex> lock(liveMutex);*output=liveState;}
        if(output->Gamepad.wButtons&XINPUT_GAMEPAD_X)++liveAttackPolls;
        liveObserved=output->dwPacketNumber;liveLastPollMs=clockNow();return ERROR_SUCCESS;
    }
    if(wheelRequested){
        if(stop.load())publishWheel(0,0,0);
        {std::lock_guard<std::mutex> lock(wheelMutex);*output=wheelState;}
        if(wheelObserved.exchange(output->dwPacketNumber)!=output->dwPacketNumber){
            try{logFile->event("quick_wheel_packet_polled","\"packet\":"+std::to_string(output->dwPacketNumber)+",\"buttons\":"+std::to_string(output->Gamepad.wButtons)+",\"right_x\":"+std::to_string(output->Gamepad.sThumbRX)+",\"right_y\":"+std::to_string(output->Gamepad.sThumbRY));}catch(...){}
        }return ERROR_SUCCESS;
    }
    if(attackRequested||motionAttackRequested){
        checkEpochStop();*output={};auto packet=attackPacket.load();
        if(uint8_t(packet)&&epochAllowsPoll&&!epochAllowsPoll()){const auto old=packet;packet=attackPacket.stop();revokeTicket();try{logFile->event("controller_attack_epoch_stopped","\"old_packet\":"+std::to_string(old>>8)+",\"neutral_packet\":"+std::to_string(packet>>8)+",\"latched\":true");}catch(...){} }
        output->dwPacketNumber=DWORD(packet>>8);if(attackButtons)output->Gamepad.wButtons=uint8_t(packet)?attackButtons:0;else output->Gamepad.bRightTrigger=uint8_t(packet);
        if(observedPacket.exchange(packet)!=packet)try{logFile->event("controller_attack_packet_polled","\"packet\":"+std::to_string(packet>>8)+",\"right_trigger\":"+std::to_string(output->Gamepad.bRightTrigger)+",\"buttons\":"+std::to_string(output->Gamepad.wButtons));}catch(...){}
        return ERROR_SUCCESS;
    }
    *output={};const auto packet=packetButtons.load();output->dwPacketNumber=DWORD(packet>>16);output->Gamepad.wButtons=uint16_t(packet);
    if(observedPacket.exchange(packet)!=packet)try{logFile->event("controller_fixture_packet_polled","\"packet\":"+std::to_string(packet>>16)+",\"buttons\":"+std::to_string(uint16_t(packet))+",\"thread\":"+std::to_string(GetCurrentThreadId()));}catch(...){}
    return ERROR_SUCCESS;
}
inline DWORD WINAPI caps(DWORD user,DWORD flags,XINPUT_CAPABILITIES* output){
    const Callback scope;const auto result=testPhysical(user,nativeCaps(user,flags,output),output);const ObserverLastError preserve;
    if(!active.load())return result;
    if(user!=port){if(!hidePhysicalPads||result==ERROR_DEVICE_NOT_CONNECTED)return result;if(output)*output={};return ERROR_DEVICE_NOT_CONNECTED;}
    if(result!=ERROR_DEVICE_NOT_CONNECTED&&!hidePhysicalPads){failed=true;active=false;stop=true;return result;}
    ++capabilities;if(!connected.load()){if(hidePhysicalPads&&result!=ERROR_DEVICE_NOT_CONNECTED){if(output)*output={};return ERROR_DEVICE_NOT_CONNECTED;}return result;}if(!output)return ERROR_BAD_ARGUMENTS;
    *output={};output->Type=XINPUT_DEVTYPE_GAMEPAD;output->SubType=XINPUT_DEVSUBTYPE_GAMEPAD;
    output->Gamepad.wButtons=0xf3ff;output->Gamepad.bLeftTrigger=output->Gamepad.bRightTrigger=255;
    output->Gamepad.sThumbLX=output->Gamepad.sThumbLY=output->Gamepad.sThumbRX=output->Gamepad.sThumbRY=32767;
    output->Vibration.wLeftMotorSpeed=output->Vibration.wRightMotorSpeed=65535;return ERROR_SUCCESS;
}
inline DWORD WINAPI rumble(DWORD user,XINPUT_VIBRATION* value){
    const Callback scope;
    if(active.load()&&user==port){++rumbles;return connected.load()?ERROR_SUCCESS:ERROR_DEVICE_NOT_CONNECTED;}
    if(active.load()&&hidePhysicalPads)return ERROR_DEVICE_NOT_CONNECTED;   // a hidden real pad does not shake
    return nativeRumble(user,value);
}
inline bool exchange(uintptr_t location,uintptr_t expected,uintptr_t value){
    return reinterpret_cast<uintptr_t>(InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(location),reinterpret_cast<void*>(value),reinterpret_cast<void*>(expected)))==expected;
}
inline bool restoreEntry(uintptr_t location,uintptr_t ours,uintptr_t original){
    // Never overwrite a third party's newer entry. Already restored is safe.
    const auto previous=reinterpret_cast<uintptr_t>(InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(location),reinterpret_cast<void*>(original),reinterpret_cast<void*>(ours)));
    return previous==ours||previous==original;
}
inline void restore(){
    if(backendExitBegun.load())return; // Native owner handoff already owns cleanup.
    {std::lock_guard<std::mutex> lock(handMutex);handFeedback={};}
    publishAttack(0);publishButtons(0);active=false;connected=false;
    bool ok=true;
    ok=restoreEntry(backend+0x10,reinterpret_cast<uintptr_t>(&state),reinterpret_cast<uintptr_t>(nativeState))&&ok;
    ok=restoreEntry(backend+0x18,reinterpret_cast<uintptr_t>(&rumble),reinterpret_cast<uintptr_t>(nativeRumble))&&ok;
    ok=restoreEntry(backend+0x20,reinterpret_cast<uintptr_t>(&caps),reinterpret_cast<uintptr_t>(nativeCaps))&&ok;
    const auto pending=read<uint8_t>(backend+0x28),nativeConnected=read<uint8_t>(backend+0x50);
    if(pending||nativeConnected)failed=true;
    installed=false;if(!ok)failed=true;done=true;
    logFile->event("controller_fixture_restored","\"pointers_restored\":"+std::string(ok?"true":"false")+",\"states\":"+std::to_string(states.load())+
        ",\"capabilities\":"+std::to_string(capabilities.load())+",\"rumble_requests_suppressed\":"+std::to_string(rumbles.load())+
        ",\"rescan_pending\":"+std::to_string(pending)+",\"native_connected\":"+std::to_string(nativeConnected)+
        ",\"native_connection_seen\":"+(nativeConnectedSeen?"true":"false")+",\"native_disconnection_seen\":"+(nativeDisconnectedSeen?"true":"false")+
        ",\"disconnect_results\":"+std::to_string(disconnects.load())+",\"physical_pad_polls_hidden\":"+std::to_string(physicalHidden.load())+",\"neutral_only\":"+neutralLabel()+",\"system_settings_unchanged\":true");
}
// The frontend's input manager (frontend+0x4b0 in June). 1.0 moved the frontend's fields (ui +0x490 -> +0x5b0, device
// +0x4d0 -> +0x5f8) and this one is not pinned down from the code yet: it is found once by the identity the checks below
// rely on (manager+0x48 -> backend, backend+0 -> manager, backend+0x48 -> backend+8) and logged.
inline size_t frontendInputOffset=0;
inline uintptr_t frontendInput(uintptr_t frontend){
    if(!frontendInputOffset)for(size_t o=0x400;o<0x900;o+=8){
        try{const auto in=read<uintptr_t>(frontend+o);if(!in||in&7)continue;const auto cur=read<uintptr_t>(in+0x48);
            if(cur&&!(cur&7)&&read<uintptr_t>(cur)==in&&read<uintptr_t>(cur+0x48)==cur+8){frontendInputOffset=o;
                logFile->event("controller_fixture_input_offset","\"frontend_offset\":"+std::to_string(o)+",\"june\":1200");break;}}catch(...){}
    }
    return frontendInputOffset?read<uintptr_t>(frontend+frontendInputOffset):0;
}
inline void onScope(uintptr_t frontend,bool normalHud,uint8_t screen=1,uint8_t auxiliary=0,bool valid=true,bool nativeBlocked=false) noexcept {
    if(!requested||done.load()||backendExitBegun.load()||readersStopped.load())return;
    bool verifiedOwner=false;
    try{
        const auto input=frontendInput(frontend),current=input?read<uintptr_t>(input+0x48):0;
        if(!current||read<uintptr_t>(current)!=input)throw std::runtime_error("fixture input owner unavailable");
        if(!installed.load()){
            if(stop.load()){done=true;return;}if(!armed.load()||!normalHud)return;
            if(guardRequested&&(!observeGuard||!observeGuard().valid))return;
            if(equipmentRequested&&(!observeEquipment||!observeEquipment().valid))return;
            if(motionRequested&&(!observeMotion||!observeMotion().valid))return;
            if(attackRequested&&(!observeAttack||!observeAttack().valid))return;
            if(motionAttackRequested&&(!observeCombat||!observeCombat().valid))return;
            const auto dll=read<HMODULE>(current+8);const auto get=read<uintptr_t>(current+0x10),set=read<uintptr_t>(current+0x18),cap=read<uintptr_t>(current+0x20);
            if(!dll||!get||!set||!cap||(get!=reinterpret_cast<uintptr_t>(resolveExport(dll,MAKEINTRESOURCEA(100)))&&get!=reinterpret_cast<uintptr_t>(resolveExport(dll,"XInputGetState")))||
                set!=reinterpret_cast<uintptr_t>(resolveExport(dll,"XInputSetState"))||cap!=reinterpret_cast<uintptr_t>(resolveExport(dll,"XInputGetCapabilities")))throw std::runtime_error("fixture API table differs");
            // A real pad on any port: refused before 2026-10-19; now hidden from the game while the mod's pad is in
            // (hidePhysicalPads above) - the game may have bound it already, and the rescan below binds port 0 again.
            unsigned physicalPorts=0;
            for(DWORD user=0;user<4;user++){
                XINPUT_STATE s{};XINPUT_CAPABILITIES c{};
                if(testPhysical(user,reinterpret_cast<StateFn>(get)(user,&s),&s)!=ERROR_DEVICE_NOT_CONNECTED||testPhysical(user,reinterpret_cast<CapsFn>(cap)(user,0,&c),&c)!=ERROR_DEVICE_NOT_CONNECTED)physicalPorts|=1u<<user;
            }
            if(physicalPorts&&!hidePhysicalPads)throw std::runtime_error("neutral fixture needs all ports disconnected");
            const auto boundPort=read<uint32_t>(current+0x40);const auto boundConnected=read<uint8_t>(current+0x50);
            if(physicalPorts)logFile->event("controller_fixture_physical_pads_hidden","\"ports\":"+std::to_string(physicalPorts)+",\"game_bound_port\":"+std::to_string(boundPort)+
                ",\"game_connected\":"+std::to_string(boundConnected)+",\"mod_port\":"+std::to_string(port));
            if(!nativeDeviceEvent||read<uintptr_t>(current+0x48)!=current+8||(!physicalPorts&&(boundPort!=0||boundConnected)))
                throw std::runtime_error("fixture native port/rescan binding differs");
            backend=current;manager=input;module=dll;nativeState=reinterpret_cast<StateFn>(get);nativeRumble=reinterpret_cast<RumbleFn>(set);nativeCaps=reinterpret_cast<CapsFn>(cap);
            ownerThread=GetCurrentThreadId();ownerSerial=++nextOwnerSerial;
            if(backendExitRequested){
                HMODULE retained=nullptr;
                if(!pinInputModule(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(get),&retained)||retained!=dll)throw std::runtime_error("owner-exit cannot pin exact XInput module");
                inputModulePinned=true; // Process lifetime only; no OS/driver change.
            }
            // Install inactive wrappers first; partially installed tables still pass through.
            bool first=exchange(current+0x10,get,reinterpret_cast<uintptr_t>(&state));
            bool second=first&&exchange(current+0x18,set,reinterpret_cast<uintptr_t>(&rumble));
            bool third=second&&exchange(current+0x20,cap,reinterpret_cast<uintptr_t>(&caps));
            if(!third){
                bool rolledBack=true;
                if(second)rolledBack=restoreEntry(current+0x18,reinterpret_cast<uintptr_t>(&rumble),set)&&rolledBack;
                if(first)rolledBack=restoreEntry(current+0x10,reinterpret_cast<uintptr_t>(&state),get)&&rolledBack;
                logFile->event("controller_fixture_install_rollback","\"pointers_restored\":"+std::string(rolledBack?"true":"false"));
                throw std::runtime_error("fixture table installation raced");
            }
            installed=true;begin=clockNow();connected=true;active=true;
            if(motionAttackRequested)logFile->event("motion_attack_fixture_enabled",std::string("\"one_swing_only\":")+(motionAttackLimit==1?"true":"false")+",\"expected_attacks\":"+std::to_string(motionAttackLimit)+",\"native_button\":16384,\"direct_damage_writes\":false");
            if(attackRequested)logFile->event("controller_attack_fixture_enabled","\"single_rt_pulse\":"+std::string(attackButtons?"false":"true")+",\"single_button_pulse\":"+(attackButtons?"true":"false")+",\"stable_melee_appearance_required\":true,\"direct_damage_writes\":false");
            // Native internal hotplug event20 only sets backend+28 pending scan.
            // The next normal input update performs enumeration and device events.
            std::array<uint8_t,64> event{};event[0]=20;
            if(!nativeDeviceEvent(reinterpret_cast<void*>(manager),event.data()))throw std::runtime_error("native rescan event rejected");
            logFile->event("controller_fixture_connected","\"backend\":"+hex(backend)+",\"port\":0,\"synthetic\":true,\"neutral_only\":"+neutralLabel()+",\"all_native_ports_disconnected\":"+std::string(physicalPorts?"false":"true")+",\"navigation_sequence\":"+std::string(tabsRequested?"true":"false")+",\"altar_sequence\":"+std::string(altarRequested?"true":"false")+",\"motion_sequence\":"+std::string(motionRequested?"true":"false")+",\"double_jump\":"+std::string(doubleJumpRequested?"true":"false")+",\"glide_sequence\":"+std::string(glideRequested?"true":"false")+",\"thread\":"+std::to_string(GetCurrentThreadId()));
        }else{
            if(current!=backend||input!=manager||read<HMODULE>(current+8)!=module)throw std::runtime_error("fixture owner changed");
            verifiedOwner=true;
            checkEpochStop();
            if(epochStopped.load()&&!epochCoordinatorStopped){
                epochCoordinatorStopped=true;
                if(meleeSession){meleeSession->closeEpoch();const auto& command=meleeSession->lastCommand;
                    logFile->event("melee_epoch_cancelled","\"retired\":"+std::to_string(command.retired)+",\"revoked\":"+(command.revoked?"true":"false")+",\"reason\":"+std::to_string(unsigned(command.reason))+",\"completed_attacks\":0");}
                invalidateTicket();
            }
            const auto nativeConnected=read<uint8_t>(backend+0x50);
            if(nativeConnected>1)throw std::runtime_error("native connection flag invalid");
            if(nativeConnected)nativeConnectedSeen=true;
            if(!nativeConnected&&disconnectAt&&nativeConnectedSeen)nativeDisconnectedSeen=true;
            if(lastNativeConnection!=nativeConnected){
                lastNativeConnection=nativeConnected;
                logFile->event("controller_fixture_native_connection","\"connected\":"+std::string(nativeConnected?"true":"false")+
                    ",\"rescan_pending\":"+std::to_string(read<uint8_t>(backend+0x28))+",\"state_calls\":"+std::to_string(states.load()));
            }
            if(failed.load()){
                if((liveRequested||gestureRequested||equipmentRequested||guardRequested||motionAttackRequested||attackRequested)&&active.load()){
                    // A UI/test failure still drains our neutral packet and real
                    // disconnect. Physical arrival/owner loss disables active
                    // immediately and must never resume synthetic input.
                    const auto now=clockNow();publishButtons(0);if(liveRequested)publishLive({});
                    if(!disconnectAt){
                        if(!releaseAt)releaseAt=now;
                        const auto neutral=(motionAttackRequested||attackRequested)?attackPacket.load():packetButtons.load();
                        if((liveRequested?!liveNeutralPolled():observedPacket.load()!=neutral)&&now-releaseAt<=500)return;
                        connected=false;disconnectAt=now;
                        logFile->event("controller_fixture_disconnected","\"failed_drain\":true,\"neutral_polled\":"+std::string(observedPacket.load()==((motionAttackRequested||attackRequested)?attackPacket.load():packetButtons.load())?"true":"false"));
                    }
                    if(now-disconnectAt<1500)return;
                }
                restore();return;
            }
            if(backendExitRequested&&nativeConnected&&states.load())backendWaitReady=true;
            // Explicit terminal-owner trial only: after the real epoch stop,
            // keep a NEUTRAL connection for its native owner. No new gameplay
            // observation and no synthetic disconnect/connection-flag write.
            if(holdConnectionOnEpochStop&&epochStopped.load()&&!failed.load()){
                if(!backendExitRequested||!inputModulePinned)throw std::runtime_error("terminal owner hold lacks pinned native backend");
                attackPacket.stop();return;
            }
            const auto now=clockNow();
            if(guardRequested&&!disconnectAt&&!stop.load()){
                const auto sample=observeGuard?observeGuard():evr::game::GuardObservation{};
                const auto previous=motionGuardRequested?motionGuard->phase():guard.phase();
                evr::game::ControllerGuardFixture::Result command{previous,0};unsigned holds=0;
                if(motionGuardRequested){
                    evr::motion::HandFrame hands;evr::motion::GuardBody body;uint64_t receipt;
                    {std::lock_guard<std::mutex> lock(guardMutex);hands=guardHands;body=guardBody;receipt=guardReceipt;}
                    const auto result=motionGuard->update(now,nativeConnected!=0,valid&&normalHud&&screen==1&&!auxiliary,sample,hands,receipt,body);
                    command={result.phase,result.buttons};holds=motionGuard->holds();
                    if(hands.frame!=lastGuardFrame||previous!=result.phase){lastGuardFrame=hands.frame;
                        std::ostringstream g;g<<std::setprecision(10)<<"\"frame\":"<<hands.frame<<",\"time\":"<<hands.predictedTime<<",\"session\":"<<hands.sessionGeneration<<",\"origin\":"<<hands.originGeneration
                         <<",\"receipt\":"<<receipt<<",\"milliseconds\":"<<now<<",\"binding\":"<<sample.binding<<",\"buttons\":"<<result.buttons
                         <<",\"phase\":"<<unsigned(result.phase)<<",\"posture\":"<<(result.input.intent.posture?"true":"false")<<",\"edge\":"<<(result.input.edge?"true":"false")
                         <<",\"fresh\":"<<(result.input.fresh?"true":"false")<<",\"blocked\":"<<unsigned(result.input.intent.blocked)<<",\"can_block\":"<<(sample.canBlock?"true":"false")
                         <<",\"forward\":"<<result.input.intent.forward<<",\"above_navel\":"<<result.input.intent.aboveNavel
                         <<",\"navel\":["<<body.navel.x<<','<<body.navel.y<<','<<body.navel.z<<"],\"body_forward\":["<<body.forward.x<<','<<body.forward.y<<','<<body.forward.z<<']';
                        logFile->event("motion_guard_sample",g.str());
                    }
                }else {command=guard.update(now,nativeConnected!=0,valid&&normalHud&&screen==1&&!auxiliary,sample);holds=guard.holds();}
                publishButtons(command.buttons);
                const int bits=(sample.canBlock?1:0)|(sample.blocking?2:0)|((sample.digital&(1ull<<30))?4:0);
                if(previous!=command.phase||lastGuardBits!=bits){
                    lastGuardBits=bits;
                    logFile->event("guard_fixture_phase","\"phase\":"+std::to_string(unsigned(command.phase))+",\"milliseconds\":"+std::to_string(now)+",\"item\":"+std::to_string(sample.item)+",\"root\":"+std::to_string(sample.root)+",\"buttons\":"+std::to_string(command.buttons)+",\"observation_valid\":"+(sample.valid?"true":"false")+",\"screen\":"+std::to_string(screen)+",\"can_block\":"+(sample.canBlock?"true":"false")+",\"blocking\":"+(sample.blocking?"true":"false")+",\"digital\":"+std::to_string(sample.digital)+",\"holds\":"+std::to_string(holds));
                }
                if(command.phase==evr::game::ControllerGuardFixture::Phase::Complete){guardCompleted=true;stop=true;}
                if(command.phase==evr::game::ControllerGuardFixture::Phase::Aborted){publishButtons(0);failed=true;stop=true;}
            }
            if(equipmentRequested&&!disconnectAt&&!stop.load()){
                const auto sample=observeEquipment?observeEquipment():evr::game::EquipmentObservation{};
                const auto previous=equipment.phase();
                const auto command=equipment.update(now,nativeConnected!=0,valid&&normalHud&&screen==1&&!auxiliary,sample);
                publishButtons(command.buttons);
                if(previous!=command.phase||lastEquipmentItem!=sample.item){
                    lastEquipmentItem=sample.item;
                    logFile->event("equipment_fixture_phase","\"phase\":"+std::to_string(unsigned(command.phase))+",\"milliseconds\":"+std::to_string(now)+",\"item\":"+std::to_string(sample.item)+",\"root\":"+std::to_string(sample.root)+",\"child\":"+std::to_string(sample.child)+",\"source\":"+std::to_string(sample.handle)+",\"buttons\":"+std::to_string(command.buttons)+",\"observation_valid\":"+(sample.valid?"true":"false")+",\"screen\":"+std::to_string(screen));
                }
                if(command.phase==evr::game::ControllerEquipmentFixture::Phase::Complete){equipmentCompleted=true;stop=true;}
                if(command.phase==evr::game::ControllerEquipmentFixture::Phase::Aborted){publishButtons(0);failed=true;stop=true;}
            }
            if(liveRequested&&!disconnectAt&&!stop.load()){
                evr::motion::HeadGestureResult value;evr::motion::HandFrame hands;uint64_t received,bowBinding=0;bool wieldGrip=false;evr::motion::BowPhysicalGeometry bowGeometry;
                {std::lock_guard<std::mutex> lock(gestureMutex);value=gestureResult;hands=gestureHands;received=gestureMs;wieldGrip=gestureWieldGrip;bowBinding=gestureBowBinding;bowGeometry=gestureBowGeometry;}
                // Native ranged aim (bow, staff) keeps the gameplay screen but switches the auxiliary HUD to 3
                // (surveys game081611, game233325). That is still gameplay: without this the aim was
                // mapped as a menu and a held attack button dropped out every few frames.
                // The death screen keeps the HUD screen 1 but raises the player's UiCoverRequest: it is a menu
                // (menuPointer::uiCover; user 2026-10-19 "when I die I cannot respawn").
                const bool uiCover=valid&&screen==1&&menuPointer::uiCover(now);
                auto screenKind=uiCover?evr::motion::LiveScreen::Ui:valid&&screen==1&&auxiliary==3&&!nativeBlocked?evr::motion::LiveScreen::Gameplay:evr::motion::liveScreen(valid,normalHud,auxiliary);
                // The building modes (29, the screwdriver's 33) and the summoning staff's adjustment (37) aim at the world: the
                // table the building controls are made on (include/evr/live_pad.hpp WorldUi; menu_pointer.hpp leaves them too).
                if(screenKind==evr::motion::LiveScreen::Ui&&!uiCover&&(screen==29||screen==33||screen==37))screenKind=evr::motion::LiveScreen::WorldUi;
                {static const bool menuLeftY=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_LT_Y",v,sizeof v);return !(n==1&&v[0]=='0');}();liveMapper.menuLeftTriggerY(menuLeftY);}
                // A building hammer or a blueprint in the hand (the game's item category 10, Blueprints - 1.0 data
                // docs/analysis/held_1_0_20261017: Tool_Quickbuilder_Hammer 693164685, _Improved 1468357474 and the walls and
                // roofs): the left trigger is the game's LT, which enters building mode (include/evr/live_pad.hpp).
                {bool blueprint=false;if(handItem&&screenKind==evr::motion::LiveScreen::Gameplay){const auto held=handItem(now);blueprint=held.valid&&held.item&&held.category==10;}
                 liveMapper.leftTriggerGame(blueprint);
                 static bool wasBlueprint=false;if(blueprint!=wasBlueprint){wasBlueprint=blueprint;logFile->event("left_trigger_game_lt","\"on\":"+std::string(blueprint?"true":"false")+",\"milliseconds\":"+std::to_string(now));}}
                {static bool was=false;if(uiCover!=was){was=uiCover;logFile->event("live_ui_cover","\"on\":"+std::to_string(int(uiCover))+",\"flags\":"+std::to_string(menuPointer::playerFlags.load())+",\"milliseconds\":"+std::to_string(now));}}
                const bool fresh=received&&now>=received&&now-received<=200;
                liveScreenGameplay=screenKind==evr::motion::LiveScreen::Gameplay;liveScreenAt=now;
                // The stick mapper checks age/identity itself and resets on stale or unavailable input.
                const auto mapped=verticalMapper.update(now,received,ownerSerial,fresh?evr::motion::liveStickMode(screenKind):evr::motion::VerticalStickMode::Unavailable,hands,value);
                auto pad=liveMapper.update(fresh?screenKind:evr::motion::LiveScreen::Unavailable,mapped,hands,wieldGrip&&!value.active);
                // The left trigger's pull also sends AreaLoot for 150 ms (include/evr/holster.hpp): a killed enemy's loot is
                // taken too. Off since 12:20 the same day (user: "gathering everything stays the default - the button held"): the
                // left trigger is the game's Y, held as long as it is pulled, and the game's own long press gathers.
                // EVR_LT_AREA_LOOT=1: the pulse again.
                {static const bool areaLoot=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_LT_AREA_LOOT",v,sizeof v);return n==1&&v[0]=='1';}();
                 static bool was=false;const bool on=liveMapper.leftInteract();
                 if(areaLoot&&on&&!was){evr::game::areaLootUntilMs.store(now+150);static unsigned lines=0;if(lines++<200)logFile->event("left_trigger_area_loot","\"milliseconds\":"+std::to_string(now));}
                 was=on;}
                if(rangedSurveyRequested&&fresh&&screenKind==evr::motion::LiveScreen::Gameplay&&hands.hands[0].primaryActive&&hands.hands[0].primaryPressed)pad.buttons|=evr::motion::LivePadMapper::X;
                if(quickSlotRequested){
                    evr::motion::GestureHead quickHead;{std::lock_guard<std::mutex> lock(gestureMutex);quickHead=gestureHead;}
                    // Gameplay, or the wheel overlay this gesture is holding open; menus and the head gesture own their input.
                    const bool allowed=fresh&&mapped.valid&&!value.active&&(quickHead.flags&15)==15&&(screenKind==evr::motion::LiveScreen::Gameplay||
                        (quickSlot.busy()&&screenKind==evr::motion::LiveScreen::Overlay));
                    const bool revive=fallenPlayerNear&&fallenPlayerNear(now);
                    const auto slot=quickSlot.update(hands,quickHead.pose,allowed,now,revive);
                    pad.buttons|=slot.buttons;
                    // Only while the game's wheel is up (auxiliary 1): fishing does not open it, and the hand's way went to the
                    // camera's turn instead (headset run 20261019-160912, user: "pressing to pull up while fishing turns me round" -
                    // the right B held, RB with the right stick at 0.92 / -0.40 sent for seconds, the view turning).
                    if(slot.steer&&auxiliary==1){pad.rightX=int16_t(std::lround(std::clamp(slot.x,-1.f,1.f)*32767));pad.rightY=int16_t(std::lround(std::clamp(slot.y,-1.f,1.f)*32767));}
                    // The dial's click (2026-10-18, roll selection): a short light pulse each time the stick enters another
                    // eighth of the wheel (its slots), and once when it leaves the centre.
                    {static int lastSector=-1;int sector=-1;
                     if(quickSlot.open()&&(slot.x!=0||slot.y!=0)){float a=std::atan2(slot.x,slot.y);if(a<0)a+=6.2831853f;sector=int(std::lround(a/0.7853982f))%8;}
                     if(sector!=lastSector){if(sector>=0)requestBowHaptic({0,.15f},{0,25});
                        static unsigned dialLogs=0;if(sector>=0&&dialLogs++<300)logFile->event("quick_slot_dial","\"frame\":"+std::to_string(hands.frame)+",\"sector\":"+std::to_string(sector)+
                            ",\"twist_deg\":"+std::to_string(int(std::lround(quickSlot.twist()*57.29578f))));
                        lastSector=sector;}}
                    if(slot.opened||slot.chosen||slot.cancelled||slot.tapped||slot.held){
                        std::ostringstream s;s<<std::setprecision(6)<<"\"frame\":"<<hands.frame<<",\"kind\":\""<<(slot.opened?"opened":slot.chosen?"chosen":slot.tapped?"tapped":slot.held?"interaction_held_for_a_fallen_player":"cancelled")
                         <<"\",\"stick\":["<<slot.x<<','<<slot.y<<"],\"buttons\":"<<slot.buttons<<",\"screen\":"<<unsigned(screenKind)<<",\"by_roll\":"<<(quickSlot.settings().roll?"true":"false")<<",\"twist_deg\":"<<quickSlot.twist()*57.29578f;
                        logFile->event("quick_slot_state",s.str());}
                }
                if(holsterRequested&&holsterAct){
                    evr::motion::GestureHead holsterHead;{std::lock_guard<std::mutex> lock(gestureMutex);holsterHead=gestureHead;}
                    static uint64_t holsterFrame=0;
                    if(hands.frame!=holsterFrame){holsterFrame=hands.frame;
                        // Gameplay only; menus, the quick wheel and the head gesture own their input.
                        const bool allowed=fresh&&mapped.valid&&!value.active&&(holsterHead.flags&15)==15&&screenKind==evr::motion::LiveScreen::Gameplay&&!(pad.buttons&evr::motion::LivePadMapper::RB)&&!(actorSwimming&&actorSwimming(now));
                        const auto press=holsterGrips.update(hands,holsterHead.pose,allowed);
                        if(press.act!=evr::motion::HolsterAct::None){
                            holsterGuardYieldUntil.store(now+800);
                            const bool asked=holsterAct(unsigned(press.act),now);
                            if(asked){if(press.hand)requestBowHaptic({0,.3f},{0,200});else requestBowHaptic({.3f,0},{200,0});}
                            using namespace evr;const Vec3 d=hands.hands[press.hand].grip.position-holsterHead.pose.position;
                            std::ostringstream s;s<<std::setprecision(5)<<"\"frame\":"<<hands.frame<<",\"hand\":"<<press.hand<<",\"place\":"<<unsigned(press.place)<<",\"act\":"<<unsigned(press.act)
                             <<",\"asked\":"<<(asked?"true":"false")<<",\"vibration\":"<<(asked?"[0.3,200]":"null")<<",\"hand_from_head\":["<<d.x<<','<<d.y<<','<<d.z<<"]";
                            logFile->event("holster_press",s.str());
                        }
                        if(holsterTick)holsterTick(now);
                    }
                }
                // The action bar's other page: the game swaps it with the right stick's click while the wheel's button is
                // held (HUD: RB + R). Here the wheel is held open with the right hand's B, and that thumb cannot click its
                // stick as well. User, 2026-10-12: "with the right stick's click I cannot do it with one hand; change it
                // to the left stick's click". While the wheel's button is down the left stick's click is the game's
                // right one (and no left click of its own); the right stick's click still works as before.
                if(fresh&&mapped.valid&&!value.active&&(pad.buttons&evr::motion::LivePadMapper::RB)&&screenKind!=evr::motion::LiveScreen::Ui&&screenKind!=evr::motion::LiveScreen::WorldUi){
                    const bool leftClick=hands.hands[0].stickClickActive&&hands.hands[0].stickClick;static bool leftBefore=false;
                    pad.buttons&=~uint16_t(0x40);if(leftClick)pad.buttons|=0x80;
                    if(leftClick&&!leftBefore)logFile->event("quick_slot_page","\"frame\":"+std::to_string(hands.frame)+",\"left_stick_click\":true,\"native_button\":128,\"screen\":"+std::to_string(unsigned(screenKind)));
                    leftBefore=leftClick;
                }
                if(liveMeleeRequested){
                    evr::motion::LiveMeleeWeapon weapon,offhand;uint64_t weaponFrame=0,weaponMs=0,offhandFrame=0,offhandMs=0;
                    {std::lock_guard<std::mutex> lock(gestureMutex);weapon=gestureMeleeWeapon;weaponFrame=gestureMeleeFrame;weaponMs=gestureMeleeMs;
                     offhand=gestureOffhandWeapon;offhandFrame=gestureOffhandFrame;offhandMs=gestureOffhandMs;}
                    // Gameplay only; the quick wheel grip and the head gesture own their input.
                    // A bare hand strikes only while its grip is held (VHVR FistCollision: an empty hand must be clenched).
                    const bool play=fresh&&mapped.valid&&screenKind==evr::motion::LiveScreen::Gameplay&&!value.active&&!(pad.buttons&evr::motion::LivePadMapper::RB);
                    // (A grip that was pressed in a holster is the holster's until it is let go: no fist.)
                    const auto clenched=[&](unsigned h){return hands.hands[h].grabActive&&hands.hands[h].grab&&!(holsterRequested&&holsterGrips.owns(h));};
                    const bool allowed=play&&weapon.binding&&weaponFrame==hands.frame&&weaponMs&&now>=weaponMs&&now-weaponMs<=200&&(!weapon.fist||clenched(1));
                    // How the hands and the middle of each blade moved (VHVR PhysicsEstimator), once per frame of the hands.
                    static evr::motion::MotionHistory secondaryHands[2],secondaryHits[2];const auto rightWeapon=weapon,leftWeapon=offhand;
                    {using namespace evr;static uint64_t historyFrame=0;   // Vec3 is OpenXR's type: its operators live in evr
                     if(hands.frame!=historyFrame){historyFrame=hands.frame;
                        for(unsigned h=0;h<2;++h){const auto& hand=hands.hands[h];
                            if(!play||!hands.focused||!hand.tracked()||!evr::finite(hand.grip.position)){secondaryHands[h].reset();secondaryHits[h].reset();continue;}
                            secondaryHands[h].add(hands.predictedTime,hand.grip.position);
                            const auto& carried=h?rightWeapon:leftWeapon;const float axis=evr::length(carried.forward);
                            if(carried.binding&&carried.hand==h&&(h?weaponFrame:offhandFrame)==hands.frame&&!carried.fist&&carried.reach>.05f&&axis>1e-4f)
                                secondaryHits[h].add(hands.predictedTime,hand.grip.position+carried.forward*(carried.reach*.5f/axis));
                            else secondaryHits[h].reset();}}}
                    // The right trigger with a melee weapon (not a tool, not the bare hand) is the heavy attack's modifier: the
                    // game does not get it as its RT.
                    const bool heavyWeapon=allowed&&!weapon.fist&&weapon.weaponType!=255;
                    // In the air with a melee weapon the body faces the head already: the jump attack starts a few frames after
                    // the press and the game stops the body's turn there (Mock game-20261017-033212: from the press alone the
                    // body turned from 0 to only -14 degrees and the attack went that way).
                    if(heavyWeapon&&attackFacingMs&&airborneNow(now)&&attackFacingUntil.load()<now+150)attackFacingUntil=now+150;
                    static bool heavyTrigger=false;
                    if(heavyWeapon&&liveHeavyByTrigger&&hands.hands[1].triggerActive&&std::isfinite(hands.hands[1].trigger)){
                        const float t=hands.hands[1].trigger;if(t>=.8f)heavyTrigger=true;else if(t<=.2f)heavyTrigger=false;
                        pad.rightTrigger=0;
                    }else heavyTrigger=false;
                    TargetNear target;if(liveMeleeTargetGate&&weapon.needsTarget&&targetNear)target=targetNear(weapon,now);
                    if(target.gated&&!target.reached&&airborneNow(now)){target.reached=true;target.airborne=true;}
                    const evr::motion::LiveCooldown* cooldown=liveCooldownEnabled()?&liveCooldown:nullptr;
                    // In the air a stroke up needs a quarter more speed (2026-10-20, user: "the rising slash of a jump goes off too
                    // easily - lifting the sword for a downward blow sets it off"). EVR_AIR_UP_FACTOR=<1..3>: another factor.
                    static const float airUpFactor=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_AIR_UP_FACTOR",v,sizeof v);const float f=n&&n<sizeof v?float(std::atof(v)):1.25f;return f>=1.f&&f<=3.f?f:1.25f;}();
                    const float upFactor=airborneNow(now)?airUpFactor:1.f;
                    // The player's own share of the speed a stroke needs (VR settings, [Melee] SwingSpeed; 2026-10-20).
                    const float swingScale=float(evr::settings::clampSwingSpeed(swingPercent.load()))/100.f;
                    const auto scaled=[swingScale](evr::motion::LiveMeleeWeapon w){w.speedFactor=std::clamp(w.speedFactor*swingScale,.05f,2.f);return w;};
                    auto swing=liveMelee.update(hands,scaled(weapon),allowed,now,target.reached,cooldown,target.ahead,upFactor);   // ahead: reached by the look-ahead only - the blade is still coming
                    if(swing.fired&&cooldown)liveCooldown.start(now);
                    bool cooledStroke=swing.cooling;
                    // The other weapon of a pair (VHVR: either hand strikes). Its stroke presses the same attack button.
                    bool leftStruck=false;
                    {const bool offhandAllowed=(offhand.fist?play&&clenched(0):allowed)&&offhand.binding&&offhand.hand==0&&offhandFrame==hands.frame&&offhandMs&&now>=offhandMs&&now-offhandMs<=200;
                     TargetNear leftTarget;if(offhandAllowed&&liveMeleeTargetGate&&offhand.needsTarget&&targetNear)leftTarget=targetNear(offhand,now);
                     if(leftTarget.gated&&!leftTarget.reached&&airborneNow(now)){leftTarget.reached=true;leftTarget.airborne=true;}
                     const auto left=liveMeleeLeft.update(hands,scaled(offhand),offhandAllowed,now,leftTarget.reached,cooldown,leftTarget.ahead,upFactor);
                     if(left.fired&&cooldown)liveCooldown.start(now);
                     cooledStroke=cooledStroke||left.cooling;
                     if(left.fired){leftStruck=true;target=leftTarget;const bool press=swing.press||left.press;swing=left;swing.press=press;weapon=offhand;liveStrikeLeftMs.store(now);}
                     else{if(swing.fired)liveStrikeRightMs.store(now);swing.press=swing.press||left.press;}}
                    {static bool air=false;static unsigned airLogged=0;const bool fast=swing.sampled&&(swing.thrust||swing.speed>swing.required)&&target.gated&&!target.reached;
                     if(fast&&!air&&airLogged++<200)logFile->event("live_melee_air_stroke","\"frame\":"+std::to_string(hands.frame)+",\"speed\":"+std::to_string(swing.speed)+",\"required\":"+std::to_string(swing.required)+",\"targets_known\":"+std::to_string(target.cached)+",\"nearest_metres\":"+std::to_string(target.nearest)+",\"attack\":false");
                     air=fast;}
                    static uint64_t tapPolls=0,tapStarted=0;static bool tapReleased=false;
                    if(swing.fired){tapPolls=liveAttackPolls.load();tapStarted=now;tapReleased=false;}
                    if(liveMeleeQuickRelease&&swing.press&&!tapReleased&&liveAttackPolls.load()-tapPolls>=meleeTapPolls){tapReleased=true;
                        logFile->event("live_melee_tap_released","\"frame\":"+std::to_string(hands.frame)+",\"held_ms\":"+std::to_string(now-tapStarted)+",\"polls\":"+std::to_string(liveAttackPolls.load()-tapPolls));}
                    if(swing.press&&!(liveMeleeQuickRelease&&tapReleased))pad.buttons|=evr::motion::LivePadMapper::X;
                    // VHVR's secondary attack by motion, judged at the moment the stroke attacks. `weapon` is the one that struck.
                    evr::motion::SecondaryResult second;auto secondStyle=evr::motion::SecondaryStyle::None;static uint64_t heavyUntil=0;
                    bool heavyByTrigger=false,heavyStroke=false,airborneStroke=false;unsigned airCleared=0;int airKind=0;
                    if(swing.fired){
                        const unsigned main=leftStruck?0u:1u,other=1u-main;const auto& otherWeapon=leftStruck?rightWeapon:leftWeapon;
                        const bool pair=rightWeapon.binding&&leftWeapon.binding&&!rightWeapon.fist&&!leftWeapon.fist&&rightWeapon.hand==1&&leftWeapon.hand==0;
                        secondStyle=evr::motion::secondaryStyle(weapon.weaponType,pair);
                        evr::motion::SecondaryMotion m;m.twoHanded=weapon.twoHanded;m.forward=weapon.forward;m.otherForward=pair?otherWeapon.forward:evr::Vec3{};
                        m.mainVelocity=swing.handVelocity;m.otherVelocity=swing.otherVelocity;
                        m.mainAverage=secondaryHands[main].averageVelocity();m.otherAverage=secondaryHands[other].averageVelocity();
                        m.mainLongest=secondaryHands[main].longestLocomotion(1.f);m.hitLongest=secondaryHits[main].longestLocomotion(.5f);
                        m.mainPosition=hands.hands[main].grip.position;m.otherPosition=hands.hands[other].grip.position;
                        {evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}m.player={head.pose.position.x,0,head.pose.position.z};}
                        second=evr::motion::secondaryAttack(secondStyle,m);
                        // Heavy: the trigger was held when the stroke attacked (any melee weapon, either hand's stroke), or -
                        // only on request - VHVR's judgement by the motion.
                        heavyByTrigger=liveHeavyByTrigger&&heavyTrigger&&!weapon.fist&&weapon.weaponType!=255;
                        heavyStroke=heavyByTrigger||(liveHeavyByMotion&&second.secondary);
                        // In the air (user 2026-10-15 afternoon: "swing up for the rising attack, down for the crash-down"): every
                        // stroke is a short press - the jump's Primary follow-ups (the jump attacks, with their skills) take it; a held
                        // button starts nothing in the air (no item has a Heavy row for the air). Before the press the follow-up slots
                        // are shaped by the hand's direction: going up (over 1 m/s) keeps the rising slash, going down (or the trigger
                        // held) the crash-down, level leaves the game its choice (shapeAirborneOpportunities).
                        airborneStroke=airborneNow(now);
                        airKind=swing.handVelocity.y>1.f?1:swing.handVelocity.y<-1.f?-1:0;
                        if(airborneStroke){airborneRequest::kind=airKind;airborneRequest::weaponType=weapon.weaponType;airborneRequest::at=now;}
                        if(airborneStroke&&clearAirborneOpportunities)airCleared=clearAirborneOpportunities(airKind,weapon.weaponType);
                        if(heavyStroke&&liveHeavyHoldMs&&!airborneStroke)heavyUntil=now+liveHeavyHoldMs;
                        // 1.0 (2026-10-17): the airborne attacks are not in the jump's follow-up slots any more (Mock
                        // game-20261017-093735: none of the 428 components of the player's entity held a jump attack's number
                        // at the press) - the item's own rows choose: Primary while rising with the slash-up skill, Heavy while
                        // falling above the ground with the crash-down skill (docs/analysis/attacks_1_0_20261017/item-actions.json).
                        // A downward stroke in the air is therefore sent as the heavy one (the held button), an upward or level
                        // one stays the short press. EVR_AIR_DOWN_HEAVY=0: every airborne stroke a short press, as before.
                        // Mock game-20261017-094444 (the jump 14 frames before each stroke): the stroke up gave the slash-up
                        // (2251110514), the stroke down the crash-down (3198880426) 14 frames later, its shock wave spawned.
                        // The game's own rule stays: the heavy is taken after about 0.25 s of holding, and the crash-down
                        // needs the character falling then (an earlier try with the jump 30 frames before: landed by then, the
                        // ground's heavy attack came, game-20261017-094136).
                        else if(airborneStroke&&airKind<0&&airDownHeavy&&liveHeavyHoldMs)heavyUntil=now+liveHeavyHoldMs;
                    }
                    if(heavyUntil){if(play&&now<heavyUntil)pad.buttons|=evr::motion::LivePadMapper::X;else heavyUntil=0;}
                    if(swing.fired){
                        strokeMs.store(now);   // the hit volume's sweep starts here (motion_collider_apply.hpp replayCurrent)
                        if(observeMeleeNative){const auto running=observeMeleeNative();std::lock_guard<std::mutex> lock(livePressMutex);
                            liveAttackPress={running.valid?running.entity:0u,uint32_t(weapon.binding>>32),running.triggerTime,now,liveAttackPress.serial+1};
                            if(cooldown)liveCooldown.expect(liveAttackPress.serial,heavyUntil?liveHeavyHoldMs:0);}
                        // VHVR: the weapon hand, 0.2 s at 0.5, when the attack starts - which here is at a target, so it is the
                        // hit's feel. Stronger since the headset run 20261014-162900 (user: "the hit's vibration may well be stronger").
                        if(leftStruck)requestBowHaptic({.9f,0},{250,0});else requestBowHaptic({0,.9f},{0,250});
                        std::ostringstream s;s<<std::setprecision(8)<<"\"frame\":"<<hands.frame<<",\"presses\":"<<liveMelee.presses()+liveMeleeLeft.presses()<<",\"speed\":"<<swing.speed<<",\"required\":"<<swing.required
                         <<",\"thrust\":"<<(swing.thrust?"true":"false")<<",\"hand\":"<<(leftStruck?0:1)<<",\"fist\":"<<(weapon.fist?"true":"false")<<",\"heavy\":"<<(weapon.heavy?"true":"false")<<",\"two_handed\":"<<(weapon.twoHanded?"true":"false")
                         <<",\"binding\":"<<weapon.binding<<",\"velocity\":["<<swing.velocity.x<<','<<swing.velocity.y<<','<<swing.velocity.z<<"],\"forward\":["<<weapon.forward.x<<','<<weapon.forward.y<<','<<weapon.forward.z<<"],\"native_button\":16384,\"target_gate\":"<<(target.gated?"true":"false")<<",\"target\":"<<target.entity<<",\"target_metres\":"<<target.distance<<",\"target_ahead\":"<<(target.ahead?"true":"false")<<",\"targets_known\":"<<target.cached;
                        s<<",\"secondary\":"<<(second.secondary?"true":"false")<<",\"secondary_style\":\""<<evr::motion::secondaryStyleName(secondStyle)<<"\",\"strong_thrust\":"<<(second.strongThrust?"true":"false")
                         <<",\"strong_swing\":"<<(second.strongSwing?"true":"false")<<",\"hook\":"<<(second.hook?"true":"false")<<",\"both_hands\":"<<(second.bothHands?"true":"false")
                         <<",\"weapon_type\":"<<unsigned(weapon.weaponType)<<",\"trigger_held\":"<<(heavyTrigger?"true":"false")<<",\"heavy_attack\":"<<(heavyStroke?"true":"false")
                         <<",\"heavy_by\":\""<<(heavyByTrigger?"trigger":heavyStroke?"motion":"none")<<"\",\"heavy_hold_ms\":"<<(heavyUntil?liveHeavyHoldMs:0)<<",\"airborne_stroke\":"<<(airborneStroke?"true":"false")<<",\"air_kind\":"<<airKind<<",\"air_opportunities_cleared\":"<<airCleared
                         <<",\"cooldown\":"<<(cooldown?"true":"false")<<",\"cooldown_wait_ms\":"<<(cooldown?liveCooldown.waitMs():0)
                         <<",\"airborne\":"<<(target.airborne?"true":"false");
                        logFile->event("live_melee_press",s.str());
                        if(attackFacingMs)attackFacingUntil=now+attackFacingMs;
                        if(airborneStroke&&airAttackStickMs)airAttackStickUntil=now+airAttackStickMs;
                    }
                    if(cooldown){
                        // The action that the stroke started says how long its cooldown is; the hand vibrates while it waits.
                        static uint64_t measured=0,measuredMs=0,readyLogged=0,lostLogged=0;static unsigned hapticHands=0,refusedLogged=0;static bool refusing=false;
                        if(swing.fired){const bool pair=rightWeapon.binding&&leftWeapon.binding&&rightWeapon.hand==1&&leftWeapon.hand==0;hapticHands=pair?3u:(leftStruck?1u:2u);}
                        CooldownMeasure m;{std::lock_guard<std::mutex> lock(liveCooldownMutex);m=liveCooldownMeasured;}
                        if(m.serial&&(m.serial!=measured||m.ms!=measuredMs)&&liveCooldown.measure(m.serial,m.ms)){measured=m.serial;measuredMs=m.ms;
                            logFile->event("live_melee_cooldown","\"frame\":"+std::to_string(hands.frame)+",\"serial\":"+std::to_string(m.serial)+",\"sequence\":"+std::to_string(m.sequence)+",\"next_attack_ms\":"+std::to_string(m.ms)+
                                ",\"cooldown_ms\":"+std::to_string(liveCooldown.lengthMs)+",\"share\":"+std::to_string(liveCooldown.share)+",\"wait_ms\":"+std::to_string(liveCooldown.waitMs())+",\"hands\":"+std::to_string(hapticHands)+",\"from_the_action\":true");}
                        if(!liveCooldown.begun)liveCooldown.begin(liveAttackBegan.load());
                        // The hum tells "an attack is on its way out, the next one has to wait": only for a press whose
                        // attack began. A press that started nothing leaves the stroke's own short pulse and silence.
                        const bool cooling=play&&liveCooldown.cooling(now);
                        cooldownHapticHands=hapticHands;cooldownHapticUntil=cooling&&liveCooldown.begun?liveCooldown.readyAt():0;
                        if(liveCooldown.lost(now)&&lostLogged!=liveCooldown.from){lostLogged=liveCooldown.from;
                            logFile->event("live_melee_press_lost","\"frame\":"+std::to_string(hands.frame)+",\"serial\":"+std::to_string(liveCooldown.serial)+",\"waited_ms\":"+std::to_string(now-liveCooldown.from)+",\"grace_ms\":"+std::to_string(liveCooldown.grace)+",\"cooldown_ended\":true");}
                        if(!cooling&&liveCooldown.begun&&liveCooldown.from&&readyLogged!=liveCooldown.from&&now>=liveCooldown.readyAt()){readyLogged=liveCooldown.from;
                            logFile->event("live_melee_cooldown_over","\"frame\":"+std::to_string(hands.frame)+",\"waited_ms\":"+std::to_string(now-liveCooldown.from)+",\"cooldown_ms\":"+std::to_string(liveCooldown.lengthMs)+",\"measured\":"+(measured&&measured==liveCooldown.serial?"true":"false"));}
                        if(cooledStroke&&!refusing&&refusedLogged++<200)logFile->event("live_melee_cooling_stroke","\"frame\":"+std::to_string(hands.frame)+",\"since_attack_ms\":"+std::to_string(now-liveCooldown.from)+",\"wait_ms\":"+std::to_string(liveCooldown.waitMs())+",\"attack\":false");
                        refusing=cooledStroke;
                    }else{cooldownHapticUntil=0;}
                    if(observeMeleeNative){
                        static MeleeNative seen{};const auto native=observeMeleeNative();
                        if(native.valid&&(native.active!=seen.active||native.sequence!=seen.sequence||native.triggerTime!=seen.triggerTime)){seen=native;
                            logFile->event("live_melee_native","\"frame\":"+std::to_string(hands.frame)+",\"active\":"+(native.active?"true":"false")+",\"used_item\":"+std::to_string(native.usedItem)+
                                ",\"sequence\":"+std::to_string(native.sequence)+",\"trigger_time\":"+std::to_string(native.triggerTime)+",\"can_primary\":"+(native.canPrimary?"true":"false")+
                                ",\"main_bits\":"+std::to_string(native.digital&15)+",\"read_only\":true");}
                    }
                }
                if(liveHandUseRequested&&handItem){
                    const auto held=handItem(now);
                    const bool gameplay=fresh&&mapped.valid&&screenKind==evr::motion::LiveScreen::Gameplay&&!value.active&&!(pad.buttons&evr::motion::LivePadMapper::RB);
                    // With a building tool in hand, in a flame altar's area, the right stick's click toggles the game's
                    // "first person mode": camera 38 (BuildingThirdPerson) becomes 36 / 37 (BuildingFirstPerson, ...Sneak),
                    // and the game then draws its own bare arms and neither the player's model nor the tool (Mock run
                    // game160101; headset run game145015, entered by accident: "the staff is not in my hand, the camera
                    // is odd, I cannot aim with the hand"). The headset is first person already, so the click does not
                    // reach the game while camera 38 is shown, and a mode that is on anyway is clicked off again.
                    // EVR_FIRST_PERSON_MODE in the game's environment, for a trial only: 1 lets the click through (the mode is
                    // then clicked off by the rule), 2 leaves a mode that is on alone, 3 both (the game's own behaviour).
                    {static uint64_t leaveUntil=0,leaveNext=0;static unsigned leaves=0,withheld=0;const int camera=observeCamera?observeCamera():-1;constexpr uint16_t rightStickClick=0x80;
                     static const unsigned keep=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_FIRST_PERSON_MODE",value,sizeof value);return n&&n<sizeof value?unsigned(std::strtoul(value,nullptr,10))&3:0u;}();
                     if(fresh&&mapped.valid&&screenKind==evr::motion::LiveScreen::Gameplay&&!value.active){
                      if(!(keep&1)&&camera==38&&(pad.buttons&rightStickClick)&&!(pad.buttons&evr::motion::LivePadMapper::RB)){pad.buttons&=~rightStickClick;
                       if(withheld++<20)logFile->event("live_first_person_mode","\"frame\":"+std::to_string(hands.frame)+",\"camera_id\":38,\"click_withheld\":true");}
                      if(!(keep&2)&&(camera==36||camera==37)&&now>=leaveNext){leaveUntil=now+120;leaveNext=now+1500;
                       if(leaves++<20)logFile->event("live_first_person_mode","\"frame\":"+std::to_string(hands.frame)+",\"camera_id\":"+std::to_string(camera)+",\"clicked_off\":true");}
                      if(now<leaveUntil)pad.buttons|=rightStickClick;else if(!(keep&2)&&(camera==36||camera==37))pad.buttons&=~rightStickClick;}}
                    uint32_t swung=0;{std::lock_guard<std::mutex> lock(gestureMutex);if(gestureMeleeWeapon.binding&&gestureMeleeMs&&now>=gestureMeleeMs&&now-gestureMeleeMs<=200)swung=uint32_t(gestureMeleeWeapon.binding>>32);}
                    // Everything that is not a weapon and is not swung (felling axe, pickaxe, rake): the trigger is its use button.
                    static bool usePressed=false;static uint32_t useItem=0;
                    const bool use=gameplay&&held.valid&&held.item&&held.category!=2&&held.item!=swung&&hands.hands[1].triggerActive&&std::isfinite(hands.hands[1].trigger);
                    const bool before=usePressed;
                    if(!use)usePressed=false;
                    else{const float t=hands.hands[1].trigger;if(t>=.8f)usePressed=true;else if(t<=.2f)usePressed=false;
                        pad.rightTrigger=0;if(usePressed)pad.buttons|=evr::motion::LivePadMapper::X;useItem=held.item;}
                    // The fishing rod (item category 5), live_swing_cast.hpp: the trigger is the game's "charge and cast" button,
                    // and the rod casts when the trigger is let go after a swing of the rod (user decision 2026-10-12; VHVR's
                    // rule of the staff). The button follows the trigger, so the game casts on the release in any case; the
                    // swing is judged for the record and the pulse of the hand. (The first form - the swing itself lets the
                    // button go, VHVR's rule of the rod - is still in the class and would act through letGo.)
                    // VHVR casts only while no float is out (with a float the trigger reels in): the game's cameras 40..42
                    // (the two that follow the float, and the catch) stand for "a float is out" - not yet seen at water.
                    {static evr::motion::LiveSwingCast swingCast;static bool letGo=false;
                     const int fishingCamera=observeCamera?observeCamera():-1;const bool floatOut=(fishingCamera>=40&&fishingCamera<=42)||fishingFloatOut(now);
                     evr::motion::LiveSwingRod rod;{std::lock_guard<std::mutex> lock(gestureMutex);if(gestureRod.binding&&gestureRodMs&&now>=gestureRodMs&&now-gestureRodMs<=200&&uint32_t(gestureRod.binding>>32)==held.item)rod=gestureRod;}
                     const auto cast=swingCast.update(hands,rod,use&&held.category==5&&rod.binding&&!floatOut);
                     if(cast.letGo&&usePressed)pad.buttons&=uint16_t(~evr::motion::LivePadMapper::X);
                     // The cast's distance from the swing alone (2026-10-19 18:45, user: "not from how long the trigger is held,
                     // only from the swing's speed"): fishing_charge.hpp gives the game's cast the swing's strength as its charge,
                     // whatever the hold charged (none: the shortest cast). The trigger holds the game's button as before and the
                     // float flies on its release (18:55 to 19:05 the hold gave nothing and the release tapped the button for
                     // 150 ms - user: "the float does not fly the moment I let go, the old way was better": EVR_FISH_CAST_TAP=1).
                     // A trigger still held from reeling in when the float comes back gives nothing until it is let go - no
                     // cast on that release.
                     {static const bool castTap=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_CAST_TAP",v,sizeof v);return n==1&&v[0]=='1';}();
                      static bool castArmed=false;static uint64_t castTapUntil=0;static unsigned tapLogs=0;
                      static bool heldFromReeling=false;
                      if(held.category==5&&rod.binding&&gameplay){
                         if(floatOut){castArmed=false;heldFromReeling=usePressed;
                            // While the line is reeled in (31 StartReelIn .. 34 FloatReturned) a trigger held from before is not
                            // the game's button already (2026-10-20, a guest's report: the host's server saw it still down when
                            // the float came back, began the next AimAndCharge, and the release threw the float 3 m - AbortPreHooked,
                            // reeled in again - seven times; the mask on the float's return came a round trip too late there).
                            const int reelState=fishingState.load();const auto reelMs=fishingStateMs.load();
                            if(usePressed&&reelMs&&now>=reelMs&&now-reelMs<=300&&reelState>=31&&reelState<=34)pad.buttons&=uint16_t(~evr::motion::LivePadMapper::X);}
                         else{
                            if(!usePressed)heldFromReeling=false;
                            if(heldFromReeling)pad.buttons&=uint16_t(~evr::motion::LivePadMapper::X);
                            if(usePressed&&!before&&!heldFromReeling)castArmed=true;
                            if(castTap&&castArmed&&usePressed)pad.buttons&=uint16_t(~evr::motion::LivePadMapper::X);
                            if(castArmed&&before&&!usePressed){castArmed=false;castTapAt=now;if(castTap)castTapUntil=now+150;
                               if(tapLogs++<200)logFile->event("live_fishing_cast_release","\"frame\":"+std::to_string(hands.frame)+",\"swing_cast\":"+(cast.cast?"true":"false")+",\"strength\":"+std::to_string(cast.strength)+",\"tap\":"+(castTap?"true":"false"));}
                         }
                      }else castArmed=heldFromReeling=false;
                      if(now<castTapUntil&&gameplay&&held.category==5)pad.buttons|=evr::motion::LivePadMapper::X;}
                     if(cast.cast){requestBowHaptic({0,.5f},{0,200});
                        // 800 ms: the release reaches the game on its next frame, the cast starts on the next tick and spawns the float 167 ms into it.
                        // The swing tells the direction when it has a level part and does not point behind the player (more
                        // than 100 degrees from where the head looks, level): a wind-up taken for the cast would throw the
                        // float backwards. Otherwise the game casts where the head looks, as it does without a swing.
                        const float level=std::sqrt(cast.direction.x*cast.direction.x+cast.direction.z*cast.direction.z);float ahead=1;
                        {evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}
                         const auto look=evr::rotate(head.pose.orientation,{0,0,-1});const float lookLevel=std::sqrt(look.x*look.x+look.z*look.z);
                         if((head.flags&15)==15&&lookLevel>.2f&&level>1e-3f)ahead=(cast.direction.x*look.x+cast.direction.z*look.z)/(level*lookLevel);}
                        const bool aimed=evr::finite(cast.direction)&&level>.3f&&ahead>-.17f;
                        // Without a direction of the swing the float goes where the head looks: also told here, since the
                        // game's camera is not the head's while the rod is charged (headset run 20261014-162900).
                        evr::Vec3 lookAim{};{evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}
                         if((head.flags&15)==15){const auto l=evr::rotate(head.pose.orientation,{0,0,-1});const float n=std::sqrt(l.x*l.x+l.z*l.z);if(n>.2f)lookAim={l.x/n,0,l.z/n};}}
                        {std::lock_guard<std::mutex> lock(fishingAimMutex);fishingSwing={now+800,fishingSwing.serial+1,cast.strength};
                         if(aimed)fishingAim={now+800,fishingAim.serial+1,cast.direction};
                         else if(lookAim.x!=0||lookAim.z!=0)fishingAim={now+800,fishingAim.serial+1,lookAim};}
                        std::ostringstream s;s<<std::setprecision(8)<<"\"frame\":"<<hands.frame<<",\"item\":"<<held.item<<",\"tip_speed\":"<<cast.speed<<",\"peak\":"<<cast.peak<<",\"strength\":"<<cast.strength
                         <<",\"direction_from_the_swing\":"<<(aimed?"true":"false")<<",\"ahead_of_the_head\":"<<ahead
                         <<",\"tip_m\":"<<rod.tip<<",\"direction\":["<<cast.direction.x<<','<<cast.direction.y<<','<<cast.direction.z<<"],\"casts\":"<<swingCast.casts()<<",\"camera_id\":"<<fishingCamera<<",\"on_trigger_release\":"<<(cast.released?"true":"false")<<",\"native_button_released\":16384";
                        logFile->event("live_fishing_cast",s.str());}
                     if(cast.letGo!=letGo){letGo=cast.letGo;if(!letGo)logFile->event("live_fishing_cast_rearmed","\"frame\":"+std::to_string(hands.frame));}
                     // The fish's pull (2026-10-19, user: "the fish's left / right action - the right stick's left and right - by
                     // leaning the rod left and right"; then "the rod tip's left and right is what counts"): while a hooked fish
                     // fights (fishFight), where the rod's tip stands left or right of the head's level forward is the left
                     // stick's x - the tip at the right hand's grip plus the rod's axis times its length, 6 degrees free, all the
                     // way at 26. The player's own stick, pushed further, goes first. EVR_FISH_ROD_TILT=0: off.
                     // The left stick, not the right (2026-10-19 18:40, the user fishing with keyboard and mouse: "pressing A / D
                     // against the fish's movement brings its endurance down, and at zero a click catches it - A while it
                     // tries to go right, D while it tries to go left"): A / D are MovementLeft / Right, the pad's left stick.
                     // Until then the lean went to the right stick - the camera (headset run 20261019-175224: every fight
                     // ended Outbreak 29 -> ReelIn 32). The tip leaned against the fish (left while it runs right) is A.
                     // EVR_FISH_STEER_STICK=right: the right stick as before (that stick then is no turn of the view,
                     // cameraControl::turnHeldUntil).
                     {static const bool rodTilt=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_ROD_TILT",v,sizeof v);return !(n==1&&v[0]=='0');}();
                      static float lastLean=0;static unsigned tiltLogs=0;
                      static const bool steerLeft=[]{char v[16]{};GetEnvironmentVariableA("EVR_FISH_STEER_STICK",v,sizeof v);return v[0]!='r';}();
                      if(rodTilt&&gameplay&&held.category==5&&rod.binding&&fishFight(now)){
                         evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}
                         const auto r=evr::rotate(head.pose.orientation,{1,0,0}),f=evr::rotate(head.pose.orientation,{0,0,-1});
                         const float n=std::sqrt(r.x*r.x+r.z*r.z),m=std::sqrt(f.x*f.x+f.z*f.z);
                         const auto& g=hands.hands[1].grip.position;const auto& e=head.pose.position;
                         const evr::Vec3 to{g.x+rod.forward.x*rod.tip-e.x,g.y+rod.forward.y*rod.tip-e.y,g.z+rod.forward.z*rod.tip-e.z};
                         if((head.flags&15)==15&&n>.3f&&m>.3f&&rod.tip>.3f&&evr::finite(to)){
                            const float side=(to.x*r.x+to.z*r.z)/n,ahead=(to.x*f.x+to.z*f.z)/m;
                            const float lean=std::atan2(side,std::max(ahead,.1f));   // the tip's angle from the head's forward, + to the right
                            const float a=std::abs(lean),x=std::copysign(std::clamp((a-.10f)/(.45f-.10f),0.f,1.f),lean);
                            if(steerLeft){if(std::abs(x)>std::abs(pad.leftX/32767.f))pad.leftX=int16_t(std::lround(x*32767));}
                            else{if(std::abs(x)>std::abs(pad.rightX/32767.f)){pad.rightX=int16_t(std::lround(x*32767));pad.rightY=0;}
                               if(holdTurn)holdTurn(now+150);}
                            if((std::abs(lean-lastLean)>.1f||tiltLogs<4)&&tiltLogs<600){++tiltLogs;lastLean=lean;
                               logFile->event("live_fishing_rod_tilt","\"frame\":"+std::to_string(hands.frame)+",\"state\":"+std::to_string(fishingState.load())+",\"lean\":"+std::to_string(lean)+",\"stick\":\""+(steerLeft?"left":"right")+"\",\"stick_x\":"+std::to_string((steerLeft?pad.leftX:pad.rightX)/32767.f)+
                                  ",\"direction\":"+std::to_string(fishingDirection.load())+",\"input_state\":"+std::to_string(fishingInputState.load()));}
                         }
                      }}
                     // The click by a flick of the rod (2026-10-19 18:35, user: "let the click (the trigger) also be done by pulling
                     // the rod up with a jerk - a flick as if lifting the fish out"; then "a flick may reel in", "a sensitivity that
                     // does not go off by mistake in the fight"): while a float is out - waiting (the click reels in), the bite
                     // (21 WaitHookInput), the tired fish (30 FinalHook) - but not in the fight itself (22..29, where the rod is
                     // leaned left and right), the rod's tip rising 7 cm within 0.25 s, at 1.6 m/s or more (12 cm / 2.2 m/s until 19:25:
                     // user "the flick needs too long a way, a little shorter is fine"), nearly straight up -
                     // within 25 degrees of up to the head's left or right (the user: "only nearly straight up, 30 degrees left and
                     // right" -> "25 degrees"; towards and away from the player is free, a lift of the rod swings the tip back) -
                     // presses the game's button (X, the trigger's) for 0.15 s with a short pulse of the hand. EVR_FISH_FLICK=0: off.
                     // (Headset run 20261019-183558: no press at all - the press, the 0.8 s pause and the pulse had gone into a
                     // comment of the line before; and the speed from GetTickCount64, 16 ms steps, jumped to 20..55 m/s. The speed
                     // is now the tip's rise over the last 60 ms or more, timed by the steady clock.)
                     {static const bool flickOn=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_FLICK",v,sizeof v);return !(n==1&&v[0]=='0');}();
                      struct TipAt {int64_t us=0;evr::Vec3 p{};};
                      static uint64_t flickUntil=0,flickNext=0,flickGapUntil=0;static std::array<TipAt,64> tips{};static size_t tipAt=0;static unsigned flickLogs=0;
                      // A click needs the button to go down (2026-10-20, a guest's report "the fish's endurance goes down, but at
                      // zero it cannot be landed": the trigger pressed at the bite (21) and held through the whole fight, so at
                      // FinalHook 30 the button was already down - the flick's press went into it and the release at 30 was no
                      // click; three fights ended 30 -> ReelIn 32, frames 40435, 41361, 47315 of 20261020-002320). With the
                      // button held, a click lets it go for 80 ms and presses it again; FinalHook entered with the trigger held is
                      // such a click by itself - holding the trigger reels the tired fish in.
                      static int lastFishState=-1;static unsigned finalLogs=0;
                      const int64_t us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                      const int fstate=fishingState.load();const auto fstateMs=fishingStateMs.load();
                      // From the wait for a fish on (16 WaitForFish .. 21), not while the float flies and lands (10..15: the cast's
                      // follow-through lifts the rod - headset run 20261019-183558 took one at 10, 3 frames after the cast), and
                      // not within a second of the cast's tap (castTapAt).
                      const bool waits=fstateMs&&now>=fstateMs&&now-fstateMs<=300&&((fstate>=16&&fstate<=21)||fstate==30)&&now>=castTapAt.load()+1000;
                      if(flickOn&&gameplay&&held.category==5&&rod.binding&&rod.tip>.3f){
                         const auto& g=hands.hands[1].grip.position;const evr::Vec3 tip{g.x+rod.forward.x*rod.tip,g.y+rod.forward.y*rod.tip,g.z+rod.forward.z*rod.tip};
                         if(evr::finite(tip)){
                            tips[tipAt%tips.size()]={us,tip};++tipAt;
                            // the lowest point of the last 0.25 s, and the newest sample at least 60 ms old (for the speed)
                            TipAt low{us,tip},back{};for(const auto& t:tips){if(!t.us||t.us>us||us-t.us>250000)continue;if(t.p.y<low.p.y)low=t;if(us-t.us>=60000&&(!back.us||t.us>back.us))back=t;}
                            const float speed=back.us&&us>back.us?(tip.y-back.p.y)*1e6f/float(us-back.us):0.f;
                            evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}
                            const auto r=evr::rotate(head.pose.orientation,{1,0,0});const float rn=std::sqrt(r.x*r.x+r.z*r.z);
                            const float rise=tip.y-low.p.y,sideways=rn>.3f?std::abs(((tip.x-low.p.x)*r.x+(tip.z-low.p.z)*r.z)/rn):0.f;
                            if(waits&&now>=flickNext&&rise>=.07f&&speed>=1.6f&&sideways<=rise*.4663f){   // 7 cm, 1.6 m/s since 19:25 ("the flick needs too long a way")
                                  // .4663: tan 25 degrees
                               const bool down=(pad.buttons&evr::motion::LivePadMapper::X)!=0;
                               flickGapUntil=down?now+80:0;flickUntil=(down?now+80:now)+150;flickNext=now+800;requestBowHaptic({0,.4f},{0,60});
                               if(flickLogs++<100)logFile->event("live_fishing_flick","\"frame\":"+std::to_string(hands.frame)+",\"state\":"+std::to_string(fstate)+",\"rise_m\":"+std::to_string(rise)+
                                  ",\"sideways_m\":"+std::to_string(sideways)+",\"speed_mps\":"+std::to_string(speed)+",\"native_button\":16384");}
                         }
                      }else tipAt=0,tips={};
                      {const bool fresh=fstateMs&&now>=fstateMs&&now-fstateMs<=300;
                       if(fresh&&fstate==30&&lastFishState!=30&&lastFishState>=22&&gameplay&&held.category==5&&usePressed&&now>=flickUntil){
                          flickGapUntil=now+80;flickUntil=now+230;
                          if(finalLogs++<100)logFile->event("live_fishing_final_hook_click","\"frame\":"+std::to_string(hands.frame)+",\"from_state\":"+std::to_string(lastFishState)+",\"trigger_held\":true,\"gap_ms\":80,\"press_ms\":150");}
                       if(fresh)lastFishState=fstate;}
                      if(now<flickGapUntil&&gameplay&&held.category==5)pad.buttons&=uint16_t(~evr::motion::LivePadMapper::X);
                      else if(now<flickUntil&&gameplay&&held.category==5)pad.buttons|=evr::motion::LivePadMapper::X;}}
                    if(usePressed!=before)logFile->event("live_use_trigger","\"frame\":"+std::to_string(hands.frame)+",\"pressed\":"+(usePressed?"true":"false")+",\"item\":"+std::to_string(useItem)+
                        ",\"category\":"+std::to_string(held.category)+",\"item_slot\":"+std::to_string(held.itemSlot)+",\"native_button\":16384");
                    // A throwable: wind up with the trigger, let go while the hand moves.
                    // EVR_THROW_TEST_ANY_ITEM=1 (Mock trials only; no save of the tests has a throwable): whatever is held counts as one,
                    // for the guide and the direction - the press then is that item's own attack.
                    static const bool throwTestAny=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_THROW_TEST_ANY_ITEM",v,sizeof v);return n==1&&v[0]=='1';}();
                    const bool throwable=gameplay&&((held.valid&&held.category==2&&held.typeKnown&&held.weaponType==12&&evr::motion::catalogThrowable(held.item))||throwTestAny);
                    // The guide: the left hand held forward while the throw is got ready shows where it goes and decides it
                    // (user 2026-10-18; ThrowGuide). Asked before the throw's own update, with the wind-up as it stood.
                    evr::motion::ThrowGuideResult guide;
                    {evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}
                     guide=throwGuide.update(hands,head.pose,(head.flags&15)==15,throwable&&liveThrow.aiming());}
                    const auto thrown=liveThrow.update(hands,throwable,now);
                    {static bool looked=false;if(!looked){looked=true;if(const auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))setThrowGuide=reinterpret_cast<ThrowGuideFn>(GetProcAddress(layer,"evrSetThrowGuide"));}
                     evr::Vec3 dots[16];const unsigned k=thrown.thrown?0u:throwGuide.dots(guide,dots,16);
                     if(setThrowGuide&&k){float v[48];for(unsigned i=0;i<k;++i){v[i*3]=dots[i].x;v[i*3+1]=dots[i].y;v[i*3+2]=dots[i].z;}setThrowGuide(v,k,1);}
                     else if(setThrowGuide&&throwGuideShown)setThrowGuide(nullptr,0,0);
                     static unsigned guideLogs=0;
                     if((k>0)!=throwGuideShown&&guideLogs<400){++guideLogs;std::ostringstream s;s<<std::setprecision(6)<<"\"frame\":"<<hands.frame<<",\"shown\":"<<(k?"true":"false")<<",\"kept\":"<<(guide.kept?"true":"false")
                         <<",\"from\":["<<guide.from.x<<','<<guide.from.y<<','<<guide.from.z<<"],\"direction\":["<<guide.direction.x<<','<<guide.direction.y<<','<<guide.direction.z<<"],\"dots\":"<<k<<",\"layer\":"<<(setThrowGuide?"true":"false");
                         logFile->event("throw_guide",s.str());}
                     throwGuideShown=k>0;}
                    if(throwable)pad.rightTrigger=0;
                    if(thrown.thrown){
                        // along the guide when the left hand gave one in this wind-up, otherwise along the swing
                        const bool byGuide=guide.kept;const auto direction=byGuide?guide.direction:thrown.direction;throwGuide.reset();
                        ThrowRequest r;{std::lock_guard<std::mutex> lock(throwMutex);r.serial=throwRequest.serial+1;r.milliseconds=now;r.frame=hands.frame;r.origin=thrown.origin;r.direction=direction;r.speed=thrown.speed;throwRequest=r;}
                        requestBowHaptic({0,.5f},{0,200});
                        std::ostringstream s;s<<std::setprecision(8)<<"\"frame\":"<<hands.frame<<",\"serial\":"<<r.serial<<",\"item\":"<<held.item<<",\"count\":"<<held.count<<",\"origin\":["<<r.origin.x<<','<<r.origin.y<<','<<r.origin.z
                         <<"],\"direction\":["<<r.direction.x<<','<<r.direction.y<<','<<r.direction.z<<"],\"by_guide\":"<<(byGuide?"true":"false")<<",\"swing_direction\":["<<thrown.direction.x<<','<<thrown.direction.y<<','<<thrown.direction.z
                         <<"],\"hand_speed\":"<<thrown.speed<<",\"travel\":"<<thrown.travel<<",\"throws\":"<<liveThrow.throws()<<",\"native_button\":16384";
                        logFile->event("live_throw",s.str());
                    }
                    if(thrown.press)pad.buttons|=evr::motion::LivePadMapper::X;
                    // The second action of what the trigger uses: the game's LB (its HUD: "LB refill" with the watering can,
                    // run game065355). The upper left button, which has no other meaning during play.
                    {static bool secondPressed=false;const bool second=use&&hands.hands[0].secondaryActive&&hands.hands[0].secondaryPressed;
                     if(second)pad.buttons|=evr::motion::LivePadMapper::LB;
                     if(second!=secondPressed){secondPressed=second;logFile->event("live_use_second","\"frame\":"+std::to_string(hands.frame)+",\"pressed\":"+(second?"true":"false")+",\"item\":"+std::to_string(held.item)+",\"native_button\":256");}}
                    // A building piece or building tool in the hand (category BuildTools). The game's HUD offers RT + d-pad to
                    // turn the piece by 90 degrees (run game065355), and the trigger is already the use button here: as in
                    // building mode below (VHVR: the right grip is the modifier and takes the stick away from turning the
                    // player), the right grip is the game's RT and the right stick with it the d-pad. The grip is free only
                    // with the quick slot by hand motion; otherwise it is the wheel and this does not apply.
                    {static unsigned pieceLogged=~0u;unsigned state=0;
                     if(gameplay&&held.valid&&held.category==6&&hands.hands[1].grabActive&&hands.hands[1].grab){
                        const auto stick=hands.hands[1].stick;uint16_t direction=0;
                        if(hands.hands[1].stickActive&&std::isfinite(stick.x)&&std::isfinite(stick.y)&&std::max(std::abs(stick.x),std::abs(stick.y))>=.6f)
                            direction=std::abs(stick.x)>std::abs(stick.y)?uint16_t(stick.x>0?0x0008:0x0004):uint16_t(stick.y>0?0x0001:0x0002);
                        pad.buttons&=uint16_t(~mapped.mappedButtons);pad.buttons|=direction;pad.rightX=pad.rightY=0;pad.rightTrigger=255;state=1u|(unsigned(direction)<<4);
                     }
                     if(state!=pieceLogged){pieceLogged=state;logFile->event("live_piece_modifier","\"frame\":"+std::to_string(hands.frame)+",\"held\":"+(state?"true":"false")+",\"direction\":"+std::to_string(state>>4)+
                        ",\"item\":"+std::to_string(held.item)+",\"buttons\":"+std::to_string(pad.buttons)+",\"right_trigger\":"+std::to_string(unsigned(pad.rightTrigger)));}}
                }
                // Building mode: the game's screen 28 (left trigger with the building hammer). Its HUD names the buttons
                // (run game083455): X build, LT leave, RB other shapes, RT + d-pad nearer / farther / turn by 90 degrees,
                // RT + B undo, RT + A materials, L3 remove. After VHVR's building controls (vhvr_spec_20261010/
                // equipment_tools.md 5.3): the trigger of the main hand builds; the right grip is the modifier (the game's
                // RT), and while it is held the right stick turns and moves the piece instead of turning the player; the
                // upper right button opens the piece menu. Everything else keeps the menu meaning it has on that screen.
                if(liveHandUseRequested&&fresh&&mapped.valid&&valid&&screen==buildingScreen&&!nativeBlocked&&!value.active){
                    const auto& r=hands.hands[1];static bool buildPressed=false;static unsigned buildLogged=~0u;
                    if(r.triggerActive&&std::isfinite(r.trigger)){if(r.trigger>=.8f)buildPressed=true;else if(r.trigger<=.2f)buildPressed=false;}else buildPressed=false;
                    pad.rightTrigger=0;
                    const bool modifier=r.grabActive&&r.grab;uint16_t direction=0;
                    // The right stick as the d-pad (one direction at a time) instead of turning the player.
                    const auto stickAsDpad=[&]{
                        const float x=pad.rightX/32767.f,y=pad.rightY/32767.f;
                        if(std::max(std::abs(x),std::abs(y))>=.6f)direction=std::abs(x)>std::abs(y)?uint16_t(x>0?0x0008:0x0004):uint16_t(y>0?0x0001:0x0002);
                        pad.buttons|=direction;pad.rightX=pad.rightY=0;};
                    // The piece menu is held open by the upper button of the LEFT hand, so that the right thumb is free for
                    // the stick that chooses in it (user after the headset run game-20261013-105615: "the building quick slot
                    // opens with the right hand and asks for the right stick - it cannot be operated. The left hand's B opens
                    // the quick slot; while the piece selection is open the right stick is the d-pad"). The upper right
                    // button opens it as before. On this screen the left button would be the pad's Y, which the building
                    // HUD does not use; it is not sent while it holds the menu.
                    const bool leftMenu=hands.hands[0].secondaryActive&&hands.hands[0].secondaryPressed;
                    if(modifier){pad.buttons&=uint16_t(~evr::motion::LivePadMapper::RB);pad.rightTrigger=255;stickAsDpad();}
                    else if((pad.buttons&evr::motion::LivePadMapper::B)||leftMenu){
                        // The piece menu is open while the game's RB is held (run game095858: categories up and down, shapes
                        // left and right, taken on release); the stick chooses in it.
                        pad.buttons&=uint16_t(~(evr::motion::LivePadMapper::B|(leftMenu?evr::motion::LivePadMapper::Y:uint16_t(0))));pad.buttons|=evr::motion::LivePadMapper::RB;stickAsDpad();}
                    if(buildPressed)pad.buttons|=evr::motion::LivePadMapper::X;
                    const unsigned state=(buildPressed?1u:0u)|(modifier?2u:0u)|((pad.buttons&evr::motion::LivePadMapper::RB)?4u:0u)|(unsigned(direction)<<4);
                    if(state!=buildLogged){buildLogged=state;logFile->event("live_build_input","\"frame\":"+std::to_string(hands.frame)+",\"build\":"+(buildPressed?"true":"false")+",\"modifier\":"+(modifier?"true":"false")+
                        ",\"direction\":"+std::to_string(direction)+",\"piece_menu\":"+((pad.buttons&evr::motion::LivePadMapper::RB)?"true":"false")+",\"by_the_left_hand\":"+(leftMenu&&!modifier?"true":"false")+",\"buttons\":"+std::to_string(pad.buttons)+",\"right_trigger\":"+std::to_string(unsigned(pad.rightTrigger))+",\"screen\":28");}
                }
                if(liveGuardRequested){
                    evr::motion::LiveGuardEquipment equipment;uint64_t guardFrame=0,guardMs=0;evr::motion::GestureHead head;
                    {std::lock_guard<std::mutex> lock(gestureMutex);equipment=gestureGuardEquipment;guardFrame=gestureGuardFrame;guardMs=gestureGuardMs;head=gestureHead;}
                    const bool allowed=fresh&&mapped.valid&&screenKind==evr::motion::LiveScreen::Gameplay&&!value.active&&guardFrame==hands.frame&&guardMs&&now>=guardMs&&now-guardMs<=200&&
                        head.frame==hands.frame&&head.session==hands.sessionGeneration;
                    // The melee press of this tick has priority over the block (user rule: attack first).
                    const auto guard=liveGuard.update(hands,head.pose,(head.flags&15)==15,equipment,allowed,bool(pad.buttons&evr::motion::LivePadMapper::X)||now<holsterGuardYieldUntil.load(),now);
                    if(guard.pressed)pad.buttons|=evr::motion::LivePadMapper::LB;
                    liveGuardHeld=guard.pressed;
                    if(guard.pressed&&(guard.direction.x!=0||guard.direction.z!=0)){std::lock_guard<std::mutex> lock(fishingAimMutex);guardAim={now+200,guardAim.serial+(guardAim.until<now),guard.direction};}
                    if(guard.repress)guardRepressAt=now;
                    if(guard.swingParry){swingParryAt=now;
                        logFile->event("live_guard_swing_parry","\"frame\":"+std::to_string(hands.frame)+",\"speed\":"+std::to_string(guard.speed)+",\"presses\":"+std::to_string(liveGuard.presses()));}
                    static bool guardPressed=false,guardPosture=false;
                    if(guard.pressed!=guardPressed||guard.posture!=guardPosture||guard.parry){guardPressed=guard.pressed;guardPosture=guard.posture;
                        std::ostringstream s;s<<std::setprecision(8)<<"\"frame\":"<<hands.frame<<",\"pressed\":"<<(guard.pressed?"true":"false")<<",\"posture\":"<<(guard.posture?"true":"false")
                         <<",\"parry\":"<<(guard.parry?"true":"false")<<",\"facing\":"<<guard.facing<<",\"speed\":"<<guard.speed<<",\"shield\":"<<(equipment.shield?"true":"false")
                         <<",\"two_hand_weapon\":"<<(equipment.twoHandWeapon?"true":"false")<<",\"binding\":"<<equipment.binding<<",\"presses\":"<<liveGuard.presses()<<",\"native_button\":256";
                        logFile->event("live_guard_state",s.str());}
                    if(observeMeleeNative){
                        static int seenBlocking=-1;const auto native=observeMeleeNative();
                        if(native.valid&&int(native.blocking)!=seenBlocking){seenBlocking=native.blocking;
                            logFile->event("live_guard_native","\"frame\":"+std::to_string(hands.frame)+",\"blocking\":"+(native.blocking?"true":"false")+",\"can_block\":"+(native.canBlock?"true":"false")+
                                ",\"block_bit\":"+std::to_string(native.digital>>28&1)+",\"read_only\":true");}
                    }
                }
                if(rangedAimRequested){
                    // Wand in hand: the right trigger is the native attack press. Staff keeps native RT (hold to cast).
                    uint32_t rangedItem=0;uint64_t rangedMs=0;evr::motion::BowPhysicalGeometry rangedGeometry;
                    {std::lock_guard<std::mutex> lock(gestureMutex);rangedItem=gestureRangedItem;rangedMs=gestureRangedMs;rangedGeometry=gestureRangedGeometry;}
                    static bool wandPressed=false;
                    const bool wandHeld=evr::motion::rangedAimWand(rangedItem)&&rangedMs&&now>=rangedMs&&now-rangedMs<=200&&fresh&&mapped.valid&&
                        evr::motion::bowTrialHud(valid,normalHud,screen,auxiliary,nativeBlocked)&&!value.active;
                    const bool wand=wandHeld&&hands.hands[1].triggerActive;
                    if(!wand)wandPressed=false;
                    else{const float t=hands.hands[1].trigger;if(t>=.8f)wandPressed=true;else if(t<=.2f)wandPressed=false;
                        pad.rightTrigger=0;if(wandPressed)pad.buttons|=evr::motion::LivePadMapper::X;}
                    // The wand's attack by a swing as well (user 2026-10-19: "the wand's normal attack also by a swing - the
                    // trigger or a swing; locked on, it flies to the target anyway"): the melee rule for one hand (VHVR's
                    // 3 m/s, the wand's turn counted 12.5 cm ahead of the hand, one tap of the attack button a stroke, the
                    // stroke slowing to half before the next one), with no target gate - the shot goes where the game aims it.
                    // EVR_WAND_SWING=0: the trigger only.
                    {static const bool wandSwing=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_WAND_SWING",v,sizeof v);return !(n==1&&v[0]=='0');}();
                     // Less eager than a weapon (user after the headset run of 2026-10-19 14:50: "the magic comes out too easily -
                     // make it need a bigger swing; unlike a weapon it fires without hitting anything, so it should come when I
                     // mean to swing"): 4.5 m/s at the lever point instead of 3, held over 15 cm of the hand's way
                     // (EVR_WAND_SWING_SPEED, EVR_WAND_SWING_TRAVEL in metres). The device log before it: 27 swing shots at
                     // 3.0..5.3 m/s, most of them 3.2..3.4.
                     // Then (headset run 20261019-152240, user: "far too dull now - I kept swinging; shots should come for
                     // swings like those"): bug report 152647's hand script, 30 s of swinging about once a second, run through
                     // this rule offline (lever point 12.5 cm, the logged shot 4.71 m/s reproduced as 4.68): 4.5 / 0.15 m gave
                     // 1 shot (the log: 1), 3.0 / 0 gave 31, 3.2 / 0.10 gave 22, 3.4 / 0.10 gave 19. The idle wiggles peak at
                     // 3.0..3.3 m/s over 0.09 m: 3.3 m/s over 0.10 m.
                     static evr::motion::LiveMelee wandStrokes([]{evr::motion::LiveMeleeSettings w;w.swingSpeed=3.3f;w.strokeTravel=.10f;
                        char v[16]{};auto n=GetEnvironmentVariableA("EVR_WAND_SWING_SPEED",v,sizeof v);if(n&&n<sizeof v){const float x=std::strtof(v,nullptr);if(std::isfinite(x)&&x>=2&&x<=12)w.swingSpeed=x;}
                        n=GetEnvironmentVariableA("EVR_WAND_SWING_TRAVEL",v,sizeof v);if(n&&n<sizeof v){const float x=std::strtof(v,nullptr);if(std::isfinite(x)&&x>=0&&x<=1)w.strokeTravel=x;}
                        return w;}());
                     evr::motion::LiveMeleeWeapon stroke;
                     // The shot itself comes on the action's first tick (LiveAttackPress::spawnsOnly, motion_event_selection_observer.hpp),
                     // and the game's cooldown stays: no swing shoots before the last swing's action lets the next attack begin.
                     const bool cooled=now>=wandNextShotMs.load();
                     const bool swingAllowed=wandSwing&&wandHeld&&cooled&&screenKind==evr::motion::LiveScreen::Gameplay&&!(pad.buttons&evr::motion::LivePadMapper::RB)&&rangedGeometry.binding;
                     if(swingAllowed){stroke.binding=rangedGeometry.binding;stroke.needsTarget=false;stroke.hand=1;
                        stroke.forward=evr::rotate(rangedGeometry.model.orientation,{0,1,0});stroke.reach=.5228f;}
                     const auto swing=wandStrokes.update(hands,stroke,swingAllowed,now);
                     if(swing.press)pad.buttons|=evr::motion::LivePadMapper::X;
                     if(swing.fired){
                        wandNextShotMs=now+600;   // until the action tells its own time (the normal attack's is 600 ms)
                        if(observeMeleeNative){const auto running=observeMeleeNative();std::lock_guard<std::mutex> lock(livePressMutex);
                            liveAttackPress={running.valid?running.entity:0u,rangedItem,running.triggerTime,now,liveAttackPress.serial+1,true};}}
                     if(swing.fired)logFile->event("wand_swing_attack","\"frame\":"+std::to_string(hands.frame)+",\"speed\":"+std::to_string(swing.speed)+
                        ",\"required\":"+std::to_string(swing.required)+",\"presses\":"+std::to_string(wandStrokes.presses())+",\"item\":"+std::to_string(rangedItem)+",\"trigger_held\":"+(wandPressed?"true":"false"));}
                }
                if(bowInputTrialRequested){
                    const auto before=bowInputTrial.phase();
                    pad=bowInputTrial.update(pad,hands,bowBinding,fresh&&mapped.valid&&evr::motion::bowTrialHud(valid,normalHud,screen,auxiliary,nativeBlocked)&&!value.active,now);
                    if(before!=bowInputTrial.phase())logFile->event("bow_input_trial_phase","\"frame\":"+std::to_string(value.frame)+",\"phase\":"+std::to_string(unsigned(bowInputTrial.phase()))+
                        ",\"binding\":"+std::to_string(bowBinding)+",\"buttons\":"+std::to_string(pad.buttons)+",\"screen\":"+std::to_string(screen)+",\"auxiliary\":"+std::to_string(auxiliary)+",\"native_state_write\":false");
                    if(bowInputTrial.phase()==evr::motion::BowInputTrial::Aborted&&!bowInputTrial.cancellationTrial())throw std::runtime_error("bow input trial scope lost; cancellation not established");
                }
                if(bowPhysicalTrialRequested){
                    const auto before=bowDrawRequested?unsigned(bowDraw.phase()):bowPhysicalReleaseRequested?bowPhysicalRelease.phase():unsigned(bowPhysicalTrial.phase());
                    const bool hud=fresh&&mapped.valid&&evr::motion::bowTrialHud(valid,normalHud,screen,auxiliary,nativeBlocked);
                    const bool allowed=hud&&!value.active;
                    // Menus and other native screens own their triggers: the draw lets go and the pad is left as mapped.
                    if(bowDrawRequested&&!hud)bowDraw.abort();
                    else pad=bowDrawRequested?bowDraw.update(pad,hands,bowGeometry,allowed,now):
                        bowPhysicalReleaseRequested?bowPhysicalRelease.update(pad,hands,bowGeometry,allowed,now):bowPhysicalTrial.update(pad,hands,bowGeometry,allowed);
                    auto phase=bowDrawRequested?unsigned(bowDraw.phase()):bowPhysicalReleaseRequested?bowPhysicalRelease.phase():unsigned(bowPhysicalTrial.phase());
                    if(bowTrackedAimRequested){
                        std::lock_guard<std::mutex> lock(bowShotMutex);
                        if(phase==5&&before!=5){
                            if(bowDrawRequested)bowTrackedShot={}; // one token per shot
                            // The draw route aims along the line between both hands whatever the rest mount angle is.
                            const bool armed=bowTrackedShot.arm(hands,bowGeometry,now,bowDrawRequested?-1.f:.5f);
                            if(!armed){if(bowDrawRequested)bowDraw.abort();else bowPhysicalRelease.abort();phase=4;pad.leftTrigger=pad.rightTrigger=0;}
                            const auto& shot=bowTrackedShot.value();std::ostringstream s;s<<std::setprecision(10)
                             <<"\"armed\":"<<(armed?"true":"false")<<",\"frame\":"<<hands.frame<<",\"binding\":"<<bowGeometry.binding<<",\"entity\":"<<bowGeometry.entity
                             <<",\"source_world\":"<<hex(bowGeometry.sourceWorld)<<",\"milliseconds\":"<<now
                             <<",\"model_q32\":["<<bowGeometry.worldPosition.x<<','<<bowGeometry.worldPosition.y<<','<<bowGeometry.worldPosition.z<<']'
                             <<",\"model_q\":["<<bowGeometry.worldOrientation.x<<','<<bowGeometry.worldOrientation.y<<','<<bowGeometry.worldOrientation.z<<','<<bowGeometry.worldOrientation.w<<']'
                             <<",\"scale\":["<<bowGeometry.scale.x<<','<<bowGeometry.scale.y<<','<<bowGeometry.scale.z<<']'
                             <<",\"nock\":["<<shot.nock.x<<','<<shot.nock.y<<','<<shot.nock.z<<']'
                             <<",\"aim_q32\":["<<shot.aim.origin.x<<','<<shot.aim.origin.y<<','<<shot.aim.origin.z<<']'
                             <<",\"aim_q\":["<<shot.aim.orientation.x<<','<<shot.aim.orientation.y<<','<<shot.aim.orientation.z<<','<<shot.aim.orientation.w<<']';
                            logFile->event("bow_tracked_shot_armed",s.str());
                            if(armed)try{const auto camera=evr::motion::bowAimCameraKnownMuzzle(shot.aim);std::lock_guard<std::mutex> aimLock(fishingAimMutex);
                                bowAim.until=now+600;++bowAim.serial;std::memcpy(bowAim.camera.data(),camera.data(),40);}catch(...){}
                        }
                        if(phase!=5||!allowed)bowTrackedShot.invalidate();
                    }
                    if(bowDrawRequested){
                        const bool released=phase==5&&before!=5,full=!released&&bowDraw.takeFullDraw(bowGeometry);
                        if(released)requestBowHaptic({.3f,.9f},{200,100});else if(full)requestBowHaptic({.3f,0},{200,0});
                        if(released||full)logFile->event("bow_draw_haptic","\"frame\":"+std::to_string(hands.frame)+",\"kind\":\""+(released?"release":"full_draw")+"\",\"shots\":"+std::to_string(bowDraw.shots()));
                    }
                    {std::lock_guard<std::mutex> lock(bowPhysicalMutex);bowPhysicalPublished={bowGeometry.binding,hands.frame,now,phase==2};}
                    const auto& m=bowDrawRequested?bowDraw.metrics():bowPhysicalReleaseRequested?bowPhysicalRelease.metrics():bowPhysicalTrial.metrics();static uint64_t lastPhysicalFrame=0;
                    if(before!=phase||hands.frame!=lastPhysicalFrame){
                        lastPhysicalFrame=hands.frame;std::ostringstream out;out<<std::setprecision(10)
                         <<"\"frame\":"<<hands.frame<<",\"phase\":"<<phase<<",\"binding\":"<<bowGeometry.binding
                         <<",\"geometry_frame\":"<<bowGeometry.frame<<",\"valid\":"<<(m.valid?"true":"false")<<",\"near\":"<<(m.withinRange?"true":"false")
                         <<",\"distance\":"<<m.distance<<",\"pull\":"<<m.pull<<",\"lateral\":"<<m.lateral
                         <<",\"nock\":["<<m.nock.x<<','<<m.nock.y<<','<<m.nock.z<<"],\"triggers\":["<<unsigned(pad.leftTrigger)<<','<<unsigned(pad.rightTrigger)
                         <<"],\"raw_trigger\":"<<hands.hands[1].trigger<<",\"native_state_write\":false,\"firing_enabled\":"<<(bowPhysicalReleaseRequested?"true":"false")
                         <<",\"release_pull\":"<<(bowDrawRequested?bowDraw.releasePull():bowPhysicalRelease.releasePull());
                        if(bowDrawRequested)out<<",\"draw\":true,\"shots\":"<<bowDraw.shots()<<",\"pull_percentage\":"<<bowDraw.pullPercentage(bowGeometry)<<",\"hud\":"<<(hud?"true":"false");
                        // The repeatable route runs for a whole session: log changes, and held frames only.
                        if(!bowDrawRequested||before!=phase||phase==2)logFile->event("bow_physical_trial_sample",out.str());
                    }
                }
                // The hand with the menu pointer: while the ray is on a menu its trigger is the mouse's left button and
                // its B the right one (VHVR; B is the pad's "back" again when the hand points away from the menu).
                if(menuPointer::trigger.load())pad.rightTrigger=0;
                if(menuPointer::secondary.load())pad.buttons&=~evr::motion::LivePadMapper::B;   // the left A: the right click
                if(menuPointer::primary.load())pad.buttons&=~evr::motion::LivePadMapper::A;     // the right A: the left click
                if(auto stickUntil=airAttackStickUntil.load()){
                    static unsigned pushes=0;
                    if(now<stickUntil&&screenKind==evr::motion::LiveScreen::Gameplay&&std::abs(int(pad.leftX))<8000&&std::abs(int(pad.leftY))<8000){
                        pad.leftX=0;pad.leftY=32767;
                        if(pushes++<40)logFile->event("live_air_attack_stick","\"frame\":"+std::to_string(hands.frame)+",\"until_ms\":"+std::to_string(stickUntil-now)+",\"forward\":true");
                    }else if(now>=stickUntil)airAttackStickUntil.compare_exchange_strong(stickUntil,0);
                }
                // Sneaking by crouching, and the view reset pose (2026-10-18). The game's sneak is its L3 (0x40; checked
                // with the right stick's L3 on 2026-10-05: one press in, one out).
                if(roomSneakRequested||recenterPoseRequested){
                    evr::motion::GestureHead roomHead;{std::lock_guard<std::mutex> lock(gestureMutex);roomHead=gestureHead;}
                    const bool headOk=fresh&&(roomHead.flags&15)==15;
                    static uint64_t poseFrame=0;
                    if(recenterPoseRequested&&hands.frame!=poseFrame){poseFrame=hands.frame;
                        // Not while the head gesture holds a hand at the temple (its own input).
                        if(recenterPose.update(hands,roomHead.pose,headOk&&!value.active,now)){
                            const auto n=recenterRequests.fetch_add(1)+1;
                            logFile->event("recenter_pose","\"frame\":"+std::to_string(hands.frame)+",\"requests\":"+std::to_string(n)+",\"held_ms\":"+std::to_string(recenterPose.settings.hold));}
                    }
                    if(roomSneakRequested){
                        static uint64_t crouchEpoch=0;const auto epoch=originEpoch.load();
                        const bool on=roomSneakOn.load();
                        // switched off: not crouched (a sneak the crouch began is ended by the presser below)
                        if(epoch!=crouchEpoch||!on){crouchEpoch=epoch;roomCrouch.reset();}
                        const bool before=roomCrouch.crouched();float drop=0;
                        if(on&&headOk&&epoch&&roomHead.origin==epoch){drop=roomHead.pose.position.y-originHeadY.load();
                            evr::motion::RoomCrouchSettings c;c.reference=originReference.load();c.enter=evr::vrpanel::enterRatio(crouchDrop.load());roomCrouch.update(drop,c);}
                        bool sneaking=false,toggle=true,able=false;
                        const bool known=sneakState&&sneakState(now,sneaking,toggle,able);
                        const bool allowed=known&&able&&fresh&&mapped.valid&&screenKind==evr::motion::LiveScreen::Gameplay&&!value.active&&!(pad.buttons&evr::motion::LivePadMapper::RB);
                        const auto r=sneakPresser.update(roomCrouch.crouched(),sneaking,toggle,allowed,now);
                        if(r.hold)pad.buttons|=0x40;
                        static unsigned lines=0;
                        if((r.pressed||before!=roomCrouch.crouched())&&lines<400){++lines;
                            std::ostringstream s;s<<std::setprecision(4)<<"\"frame\":"<<hands.frame<<",\"crouched\":"<<(roomCrouch.crouched()?"true":"false")<<",\"drop_m\":"<<drop
                             <<",\"standing_m\":"<<originReference.load()<<",\"real_crouch\":"<<(on?"true":"false")<<",\"crouch_drop_percent\":"<<crouchDrop.load()<<",\"sneaking\":"<<(sneaking?"true":"false")<<",\"switches\":"<<(toggle?"true":"false")
                             <<",\"allowed\":"<<(allowed?"true":"false")<<",\"pressed\":"<<(r.pressed?"true":"false")<<",\"to_sneak\":"<<(r.entering?"true":"false")<<",\"by_the_body\":"<<(sneakPresser.bodySneak()?"true":"false");
                            logFile->event("room_sneak",s.str());}
                    }
                }
                // Swimming by the arms (include/evr/swim_stroke.hpp; VR settings, [Body] ArmSwim): the strokes' velocity is
                // added to the left stick while the character is in the water.
                {static uint64_t swimFrame=0;static evr::motion::SwimStrokeResult swim;static bool wasMoving=false;static unsigned lines=0,strokeFrames=0;
                 const bool inWater=armSwimOn.load()&&actorSwimming&&actorSwimming(now)&&fresh&&mapped.valid&&screenKind==evr::motion::LiveScreen::Gameplay&&!value.active;
                 if(hands.frame!=swimFrame||!inWater){swimFrame=hands.frame;
                    evr::motion::GestureHead swimHead;{std::lock_guard<std::mutex> lock(gestureMutex);swimHead=gestureHead;}
                    static const evr::motion::SwimStrokeSettings swimSettings=[]{evr::motion::SwimStrokeSettings c;char v[16]{};const auto n=GetEnvironmentVariableA("EVR_SWIM_STICK_SCALE",v,sizeof v);
                        if(n&&n<sizeof v){const float x=float(std::atof(v));if(x>=.05f&&x<=2.f)c.metresPerSecondPerStick=x;}return c;}();
                    swim=swimStroke.update(hands,swimHead.pose,inWater&&(swimHead.flags&15)==15,swimSettings);}
                 if(swim.moving){
                    pad.leftX=int16_t(std::clamp<long>(long(pad.leftX)+std::lround(swim.stickX*32767),-32767,32767));
                    pad.leftY=int16_t(std::clamp<long>(long(pad.leftY)+std::lround(swim.stickY*32767),-32767,32767));}
                 // Up and down (include/evr/swim_stroke.hpp up, include/evr/holster.hpp swim inputs): under the water a stroke
                 // that lifts presses the game's DiveAscend and one that sinks DiveDescend, as long as it carries (30 % of a
                 // full throw); on the surface one that sinks presses SwimStartDiving. EVR_SWIM_DIVE=0: off.
                 {static const bool dive=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SWIM_DIVE",v,sizeof v);return !(n==1&&v[0]=='0');}();
                  static uint64_t lastBits=0;static unsigned diveLines=0;
                  uint64_t bits=0;const unsigned water=inWater&&actorWater?actorWater(now):0u;
                  if(dive&&inWater){
                    if(water&2){if(swim.upStick>=.3f)bits=evr::game::inputDiveAscend;else if(swim.upStick<=-.3f)bits=evr::game::inputDiveDescend;}
                    else if(water&1){if(swim.upStick<=-.3f)bits=evr::game::inputSwimStartDiving;}}
                  evr::game::swimInputBits.store(bits);if(bits)evr::game::swimInputUntilMs.store(now+120);
                  if(bits!=lastBits&&diveLines++<300){std::ostringstream s;s<<std::setprecision(3)<<"\"frame\":"<<hands.frame<<",\"water\":"<<water<<",\"up_m_s\":"<<swim.up<<",\"up_stick\":"<<swim.upStick
                     <<",\"start_diving\":"<<((bits&evr::game::inputSwimStartDiving)?"true":"false")<<",\"ascend\":"<<((bits&evr::game::inputDiveAscend)?"true":"false")<<",\"descend\":"<<((bits&evr::game::inputDiveDescend)?"true":"false");
                     logFile->event("swim_dive",s.str());}
                  lastBits=bits;}
                 const bool sample=swim.stroking&&strokeFrames++%30==0;
                 if((swim.moving!=wasMoving||sample)&&lines<600){++lines;
                    std::ostringstream s;s<<std::setprecision(3)<<"\"frame\":"<<hands.frame<<",\"moving\":"<<(swim.moving?"true":"false")<<",\"stroking\":"<<(swim.stroking?"true":"false")
                     <<",\"hands\":["<<swim.counted[0]<<','<<swim.counted[1]<<"],\"velocity\":["<<swim.velocity.x<<','<<swim.velocity.z<<"],\"wanted\":["<<swim.wanted.x<<','<<swim.wanted.z<<"],\"stick\":["<<swim.stickX<<','<<swim.stickY<<']';
                    logFile->event("swim_stroke",s.str());}
                 wasMoving=swim.moving;}
                publishLive(pad);
                liveWieldBusy.store(!fresh||screenKind!=evr::motion::LiveScreen::Gameplay||value.active||bool(pad.buttons&evr::motion::LivePadMapper::RB));
                static uint64_t lastScope=0;if(!fresh&&lastScope&&now>lastScope)liveStaleMs+=std::min<uint64_t>(now-lastScope,1000);lastScope=now;
                if(!nativeConnected&&!liveConnectionWarned&&now-begin>5000){liveConnectionWarned=true;
                    logFile->event("controller_live_not_enumerated","\"milliseconds\":"+std::to_string(now-begin)+",\"state_calls\":"+std::to_string(states.load()));}
                const bool frameChanged=fresh&&value.frame!=liveLastFrame;
                if(frameChanged){liveLastFrame=value.frame;++liveSamples;}
                // Raw face/grip changes are logged too: a press that maps to nothing leaves the pad unchanged (check_live_input).
                const auto& rr=hands.hands[1];const auto& ll=hands.hands[0];
                const unsigned raw=(rr.primaryPressed?1:0)|(rr.secondaryPressed?2:0)|(ll.primaryPressed?4:0)|(ll.secondaryPressed?8:0)|(ll.grab?16:0)|(rr.grab?32:0);
                static unsigned liveLoggedRaw=0;
                if(pad!=liveLogged||int(screenKind)!=liveLastScreen||(fresh&&raw!=liveLoggedRaw)||(frameChanged&&liveSamples%90==1)){
                    liveLogged=pad;liveLastScreen=int(screenKind);if(fresh)liveLoggedRaw=raw;
                    DWORD packet;{std::lock_guard<std::mutex> lock(liveMutex);packet=liveState.dwPacketNumber;}
                    const auto& r=hands.hands[1];const auto& l=hands.hands[0];
                    std::ostringstream out;out<<"\"frame\":"<<value.frame<<",\"fresh\":"<<(fresh?"true":"false")<<",\"age_ms\":"<<(received&&now>=received?now-received:UINT64_MAX)
                     <<",\"screen\":"<<unsigned(screen)<<",\"auxiliary\":"<<unsigned(auxiliary)<<",\"live_screen\":"<<unsigned(screenKind)<<",\"stick_mode\":"<<unsigned(mapped.mode)
                     <<",\"mapping_valid\":"<<(mapped.valid?"true":"false")<<",\"needs_neutral\":"<<(mapped.needsNeutral?"true":"false")<<",\"gesture\":"<<(value.active?"true":"false")
                     <<",\"buttons\":"<<pad.buttons<<",\"left\":["<<pad.leftX<<','<<pad.leftY<<"],\"right\":["<<pad.rightX<<','<<pad.rightY<<"],\"triggers\":["<<unsigned(pad.leftTrigger)<<','<<unsigned(pad.rightTrigger)<<']'
                     <<",\"wield_grip\":"<<(wieldGrip?"true":"false")<<",\"raw_faces\":["<<(r.primaryPressed?1:0)<<','<<(r.secondaryPressed?1:0)<<','<<(l.primaryPressed?1:0)<<','<<(l.secondaryPressed?1:0)<<"],\"raw_grips\":["<<(l.grab?1:0)<<','<<(r.grab?1:0)<<']'
                     <<",\"tracked\":["<<(l.tracked()?1:0)<<','<<(r.tracked()?1:0)<<"],\"packet\":"<<packet<<",\"native_connected\":"<<unsigned(nativeConnected)<<",\"samples\":"<<liveSamples;
                    logFile->event("controller_live_sample",out.str());
                }
            }
            if(wheelRequested&&!disconnectAt&&!stop.load()){
                evr::motion::HeadGestureResult value;evr::motion::HandFrame hands;uint64_t received;
                {std::lock_guard<std::mutex> lock(gestureMutex);value=gestureResult;hands=gestureHands;received=gestureMs;}
                if(!valid||screen!=1||auxiliary>1||now-begin>60000)throw std::runtime_error("quick wheel scope/time invalid");
                if(received&&now>=received&&now-received<=200){
                    const auto observed=observeMotion?observeMotion():evr::game::MotionObservation{};
                    if(value.frame>=440&&!nativeConnected)throw std::runtime_error("quick wheel input backend disconnected");
                    const bool overlay=auxiliary==1;
                    const auto mode=overlay?evr::motion::VerticalStickMode::NativeOverlay:normalHud&&observed.valid?evr::motion::VerticalStickMode::Gameplay:evr::motion::VerticalStickMode::Unavailable;
                    auto mapped=verticalMapper.update(now,received,ownerSerial,mode,hands,value);
                    if(mapped.mappedButtons||mapped.pad.buttons||mapped.pad.left.x||mapped.pad.left.y||mapped.pad.leftTrigger||mapped.pad.rightTrigger)throw std::runtime_error("wheel generated gameplay command");
                    bool grip=hands.hands[1].grabActive&&hands.hands[1].grab;
                    if(routerRequested){
                        if(value.frame!=lastRouterFrame){lastRouterFrame=value.frame;
                            evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(gestureMutex);head=gestureHead;}
                            const auto body=routerBody->update(hands,head);
                            evr::motion::GripAuthority authority;authority.enabled=true;authority.known=authority.local=authority.alive=observed.valid;
                            authority.uiOrEvent=!(valid&&screen==1&&(normalHud||overlay));authority.gestureActive=value.active;authority.wheelOverlay=overlay;
                            lastRoute=gripRouter->update(hands,body,authority);
                            if(lastRoute.equip!=evr::motion::EquipRequest::None){++routerEquipRequests;
                                logFile->event("grip_router_equip_request","\"frame\":"+std::to_string(value.frame)+",\"request\":"+std::to_string(unsigned(lastRoute.equip))+
                                    ",\"hand\":"+std::to_string(lastRoute.equipHand)+",\"zone\":"+std::to_string(unsigned(lastRoute.hands[lastRoute.equipHand].pressZone))+
                                    ",\"count\":"+std::to_string(routerEquipRequests)+",\"executed\":"+(routerEquipRequested?"true":"false")+",\"native_writes\":false"+
                                    ",\"gesture_overlap\":"+(lastRoute.gestureOverlap?"true":"false")+",\"gesture_hands\":"+std::to_string(unsigned(value.activeHands)));}
                            if(routerEquipRequested){
                                const auto bar=observeActionbar?observeActionbar():evr::motion::ActionbarSnapshot{};
                                const bool fresh=bar.valid&&now>=bar.milliseconds&&now-bar.milliseconds<=500;
                                if(fresh&&preferredMelee==255&&bar.selected<8&&evr::motion::equipSlotMatches(bar.slots[bar.selected],evr::motion::EquipRequest::MainWeapon))preferredMelee=bar.selected;
                                if(lastRoute.equip!=evr::motion::EquipRequest::None&&!wheelSelect.active()){
                                    const auto plan=evr::motion::planEquip(fresh?bar:evr::motion::ActionbarSnapshot{},lastRoute.equip,preferredMelee);++equipPlans;
                                    std::string slots;for(size_t k=0;k<8;k++){const auto& s=bar.slots[k];slots+=std::string(k?",":"")+"["+std::to_string(s.item)+","+std::to_string(s.count)+","+std::to_string(s.entity)+","+std::to_string(s.category)+","+std::to_string(s.equipmentSlot)+"]";}
                                    logFile->event("equip_plan","\"frame\":"+std::to_string(value.frame)+",\"request\":"+std::to_string(unsigned(lastRoute.equip))+",\"result\":"+std::to_string(unsigned(plan.result))+
                                        ",\"slot\":"+std::to_string(plan.slot)+",\"selected\":"+std::to_string(bar.selected)+",\"fresh\":"+(fresh?"true":"false")+",\"preferred_melee\":"+std::to_string(preferredMelee)+
                                        ",\"bar_entity\":"+std::to_string(bar.barEntity)+",\"snapshot\":"+std::to_string(bar.sequence)+",\"slots\":["+slots+"]");
                                    if(plan.result==evr::motion::EquipPlanResult::Target)wheelSelect.start(plan.slot,hands.predictedTime);
                                }
                                if(wheelSelect.active()){
                                    selectStep=wheelSelect.update(hands.predictedTime,overlay,fresh?bar.selected:255);
                                    if(!wheelSelect.active()){
                                        const bool done=wheelSelect.phase()==evr::motion::WheelSelect::Phase::Done;equipCompleted+=done;
                                        logFile->event("equip_wheel_result","\"frame\":"+std::to_string(value.frame)+",\"target\":"+std::to_string(wheelSelect.target())+",\"done\":"+(done?"true":"false")+
                                            ",\"error\":"+jsonString(selectStep.error?selectStep.error:"")+",\"selected\":"+std::to_string(fresh?bar.selected:255));
                                        if(!done)throw std::runtime_error(std::string("equip wheel selection failed: ")+(selectStep.error?selectStep.error:"unknown"));
                                        if(evr::motion::equipSlotMatches(bar.slots[wheelSelect.target()],evr::motion::EquipRequest::MainWeapon))preferredMelee=wheelSelect.target();
                                    }
                                } else selectStep={};
                            }
                        }
                        grip=lastRoute.valid&&lastRoute.wheelHeld;
                    }
                    const bool selecting=routerEquipRequested&&(selectStep.buttons||selectStep.stick.x||selectStep.stick.y);
                    if(selecting){publishWheel(selectStep.buttons,selectStep.stick.x,selectStep.stick.y);grip=selectStep.buttons!=0;}
                    else publishWheel(mapped.valid&&grip?0x0200:0,mapped.pad.right.x,mapped.pad.right.y);
                    const auto progress=wheelProgress.update(value.frame,overlay,!auxiliary&&!grip&&(selecting?!(selectStep.stick.x||selectStep.stick.y):(!mapped.pad.right.x&&!mapped.pad.right.y)));
                    if(value.frame!=lastVerticalFrame){lastVerticalFrame=value.frame;
                        std::ostringstream fields;fields<<std::setprecision(10)<<"\"frame\":"<<value.frame<<",\"time\":"<<value.time<<",\"owner\":"<<ownerSerial<<",\"age_ms\":"<<(now-received)
                         <<",\"screen\":"<<unsigned(screen)<<",\"auxiliary\":"<<unsigned(auxiliary)<<",\"mode\":"<<unsigned(mapped.mode)
                         <<",\"valid\":"<<(mapped.valid?"true":"false")<<",\"neutral_required\":"<<(mapped.needsNeutral?"true":"false")
                         <<",\"raw_right\":["<<hands.hands[1].stick.x<<','<<hands.hands[1].stick.y<<"],\"right\":["<<mapped.pad.right.x<<','<<mapped.pad.right.y<<']'
                         <<",\"mapped_buttons\":"<<mapped.mappedButtons<<",\"grip\":"<<(grip?"true":"false")<<",\"closed_cycles\":"<<wheelProgress.closedCycles();
                        if(routerRequested)fields<<",\"router_valid\":"<<(lastRoute.valid?"true":"false")<<",\"router_blocked\":"<<unsigned(lastRoute.blocked)
                         <<",\"router_wheel\":"<<(lastRoute.wheelHeld?"true":"false")<<",\"router_right_held\":"<<(lastRoute.hands[1].held?"true":"false")
                         <<",\"router_right_zone\":"<<unsigned(lastRoute.hands[1].zone)<<",\"router_right_consumed\":"<<(lastRoute.hands[1].consumed?"true":"false")
                         <<",\"router_rearm\":"<<(lastRoute.attackRearmRequired[1]?"true":"false")<<",\"raw_right_grip\":"<<((hands.hands[1].grabActive&&hands.hands[1].grab)?"true":"false")
                         <<",\"router_gesture_overlap\":"<<(lastRoute.gestureOverlap?"true":"false")<<",\"gesture_hands\":"<<unsigned(value.activeHands);
                        if(routerEquipRequested)fields<<",\"select_phase\":"<<unsigned(wheelSelect.phase())<<",\"select_rb\":"<<selectStep.buttons<<",\"select_stick\":["<<selectStep.stick.x<<','<<selectStep.stick.y<<']';
                        logFile->event("quick_wheel_frontend",fields.str());
                    }
                    if(progress.verdict==evr::motion::QuickWheelProgress::Verdict::Fail)throw std::runtime_error(progress.error);
                    if(progress.captureTag){if(!requestXrCapture||requestXrCapture(progress.captureTag))throw std::runtime_error("wheel capture unavailable");}
                    if(progress.verdict==evr::motion::QuickWheelProgress::Verdict::Complete){gestureCompleted=true;stop=true;}
                }else{publishWheel(0,0,0);if(now-begin>5000)throw std::runtime_error("quick wheel tracking stale");}
            }
            if(verticalRequested&&!disconnectAt&&!stop.load()){
                evr::motion::HeadGestureResult value;evr::motion::HandFrame hands;uint64_t received;
                {std::lock_guard<std::mutex> lock(gestureMutex);value=gestureResult;hands=gestureHands;received=gestureMs;}
                if(!valid||!normalHud||auxiliary||screen!=1||now-begin>60000)throw std::runtime_error("vertical stick scope/time invalid");
                if(received&&now>=received&&now-received<=200){
                    const auto observed=observeMotion?observeMotion():evr::game::MotionObservation{};
                    if(value.frame>=440&&!nativeConnected)throw std::runtime_error("vertical stick input backend disconnected");
                    auto mapped=verticalMapper.update(now,received,ownerSerial,observed.valid?evr::motion::VerticalStickMode::Gameplay:evr::motion::VerticalStickMode::Unavailable,hands,value);
                    if((observed.valid&&!mapped.valid)||mapped.pad.right.x||mapped.pad.right.y||mapped.pad.left.x||mapped.pad.left.y||mapped.pad.leftTrigger||mapped.pad.rightTrigger||
                       (mapped.pad.buttons&~uint16_t(0x2040))||value.active)throw std::runtime_error("vertical stick fixture unexpected control");
                    publishButtons(mapped.pad.buttons);
                    if(value.frame!=lastVerticalFrame){lastVerticalFrame=value.frame;
                        MotionActor actor;{std::lock_guard<std::mutex> lock(motionActorMutex);actor=motionActor;}
                        std::ostringstream out;out<<std::setprecision(14)<<"\"frame\":"<<value.frame<<",\"time\":"<<value.time<<",\"owner\":"<<ownerSerial
                         <<",\"age_ms\":"<<now-received<<",\"buttons\":"<<mapped.pad.buttons<<",\"mapped\":"<<mapped.mappedButtons<<",\"right_y\":"<<hands.hands[1].stick.y
                         <<",\"screen\":"<<unsigned(screen)<<",\"mode\":"<<unsigned(mapped.mode)<<",\"neutral_required\":"<<(mapped.needsNeutral?"true":"false")
                         <<",\"actor_valid\":"<<(observed.valid?"true":"false")<<",\"entity\":"<<observed.entity<<",\"actor_entity\":"<<actor.entity
                         <<",\"actor_age_ms\":"<<(now>=actor.milliseconds?now-actor.milliseconds:UINT64_MAX)<<",\"actor_state\":"<<actor.state<<",\"digital\":"<<actor.digital
                         <<",\"root_age_ms\":"<<observed.rootAgeMs<<",\"mapping_valid\":"<<(mapped.valid?"true":"false")<<",\"camera_id\":"<<unsigned(observed.camera)<<",\"root\":["<<observed.x<<','<<observed.y<<','<<observed.z<<']';
                        logFile->event("vertical_stick_frontend",out.str());
                    }
                    if(value.frame>=750&&observed.valid&&mapped.valid){if(mapped.pad.buttons)throw std::runtime_error("vertical stick did not release");gestureCompleted=true;stop=true;}
                }else {publishButtons(0);if(now-begin>5000)throw std::runtime_error("vertical stick pose stale");}
            }
            if(gestureRequested&&!verticalRequested&&!wheelRequested&&!disconnectAt&&!stop.load()){
                evr::motion::HeadGestureResult value;uint64_t received;
                {std::lock_guard<std::mutex> lock(gestureMutex);value=gestureResult;received=gestureMs;}
                if(!valid||auxiliary||(screen!=1&&screen!=4)||now-begin>60000)throw std::runtime_error("gesture menu scope/time invalid");
                if(received&&now>=received&&now-received<=300){
                    if(value.frame>=440&&!nativeConnected)throw std::runtime_error("gesture controller not enumerated before input");
                    publishButtons(value.pad.buttons);gestureSawMenu|=screen==4;
                    logFile->event("head_gesture_frontend","\"frame\":"+std::to_string(value.frame)+",\"screen\":"+std::to_string(screen)+",\"buttons\":"+std::to_string(value.pad.buttons)+",\"packet\":"+std::to_string(packetButtons.load()>>16)+",\"age_ms\":"+std::to_string(now-received));
                    uint32_t capture=0;
                    if(screen==4&&value.frame>=470&&!gestureOpenCapture){gestureOpenCapture=true;capture=1;}
                    if(screen==4&&value.frame>=530&&!gestureSelectionCapture){gestureSelectionCapture=true;capture=3;}
                    if(screen==1&&value.frame>=610&&gestureSawMenu&&!gestureClosedCapture){gestureClosedCapture=true;capture=2;}
                    if(capture){
                        if(!requestXrCapture||requestXrCapture(capture))throw std::runtime_error("gesture UI capture rejected");
                        logFile->event("head_gesture_capture_requested","\"tag\":"+std::to_string(capture)+",\"screen\":"+std::to_string(screen)+",\"frame\":"+std::to_string(value.frame));
                    }
                    if(value.frame>=650){
                        if(screen!=1||value.active||value.pad.buttons||!gestureSawMenu||!gestureClosedCapture)throw std::runtime_error("gesture menu did not return to HUD");
                        gestureCompleted=true;stop=true;
                    }
                }else {publishButtons(0);if(now-begin>5000)throw std::runtime_error("gesture input stale");}
            }
            if(menuRequested&&!disconnectAt){
                const int camera=observeCamera?observeCamera():-1;
                if(camera!=lastCamera){lastCamera=camera;logFile->event("controller_fixture_camera","\"camera_id\":"+std::to_string(camera)+",\"screen\":"+std::to_string(screen));}
                const auto previous=menu.phase();const auto command=menu.update(now,nativeConnected!=0,screen,auxiliary,valid);
                publishButtons(command.buttons);
                if(command.phase!=previous)logFile->event("controller_menu_phase","\"phase\":"+std::to_string(unsigned(command.phase))+
                    ",\"screen\":"+std::to_string(screen)+",\"buttons\":"+std::to_string(command.buttons)+",\"packet\":"+std::to_string(packetButtons.load()>>16));
                if(command.captureTag&&requestXrCapture){
                    if(requestXrCapture(command.captureTag))throw std::runtime_error("controller XR capture request rejected");
                    logFile->event("controller_menu_capture_requested","\"tag\":"+std::to_string(command.captureTag)+",\"screen\":"+std::to_string(screen));
                }
                if(command.phase==evr::game::ControllerMenuFixture::Phase::Complete){menuCompleted=true;stop=true;}
                if(command.phase==evr::game::ControllerMenuFixture::Phase::Aborted){publishButtons(0);failed=true;stop=true;}
            }
            if(motionRequested&&!disconnectAt){
                if(!observeMotion)throw std::runtime_error("motion fixture observer absent");
                auto observed=observeMotion();observed.valid=observed.valid&&valid;
                if(observed.rootSample!=lastMotionRoot){
                    lastMotionRoot=observed.rootSample;std::ostringstream out;out<<std::setprecision(14)<<"\"sample\":"<<observed.rootSample<<",\"camera_id\":"<<unsigned(observed.camera)<<",\"entity\":"<<observed.entity<<",\"actor_state\":"<<observed.actorState<<",\"root\":["<<observed.x<<','<<observed.y<<','<<observed.z<<"],\"valid\":"<<(observed.valid?"true":"false")<<",\"root_age_ms\":"<<observed.rootAgeMs<<",\"actor_age_ms\":"<<observed.actorAgeMs;logFile->event("controller_motion_sample",out.str());
                }
                const auto previous=motion.phase();const auto command=motion.update(now,nativeConnected!=0,screen,auxiliary,observed);
                publishButtons(command.buttons);
                if(command.phase!=previous){std::ostringstream out;out<<std::setprecision(12)<<"\"phase\":"<<unsigned(command.phase)<<",\"screen\":"<<unsigned(screen)<<",\"buttons\":"<<command.buttons<<",\"camera_id\":"<<unsigned(observed.camera)<<",\"entity\":"<<observed.entity<<",\"actor_state\":"<<observed.actorState<<",\"root\":["<<observed.x<<','<<observed.y<<','<<observed.z<<']';logFile->event("controller_motion_phase",out.str());}
                if(command.phase!=previous&&command.phase==evr::game::ControllerMotionFixture::Phase::PressFirst&&motionBurstFrames){
                    if(!requestXrBurst||requestXrBurst(motionBurstFrames))throw std::runtime_error("motion XR burst request rejected");
                    logFile->event("controller_motion_burst_requested","\"count\":"+std::to_string(motionBurstFrames));
                }
                if(command.captureTag&&requestXrCapture){
                    if(requestXrCapture(command.captureTag))throw std::runtime_error("motion XR capture request rejected");
                    logFile->event("controller_motion_capture_requested","\"tag\":"+std::to_string(command.captureTag)+",\"camera_id\":"+std::to_string(observed.camera));
                }
                if(command.phase==evr::game::ControllerMotionFixture::Phase::Complete){motionCompleted=true;stop=true;}
                if(command.phase==evr::game::ControllerMotionFixture::Phase::Aborted){publishButtons(0);failed=true;stop=true;}
            }
            if(motionAttackRequested&&!disconnectAt&&!stop.load()){
                if(!observeCombat)throw std::runtime_error("motion combat observer absent");
                auto s=observeCombat();evr::motion::HandFrame hands;evr::motion::HeadGestureResult gesture;uint64_t stamp;
                {std::lock_guard<std::mutex> lock(handMutex);hands=handFrame;gesture=handGesture;stamp=handMs;}
                const auto previous=meleeSession?meleeSession->phase():swing.phase();
                const auto c=meleeSession?meleeSession->update(now,nativeConnected!=0,normalHud&&valid,hands,stamp,s,combatGestureRequested?&gesture:nullptr):swing.update(now,nativeConnected!=0,normalHud&&valid,hands,stamp,s,combatGestureRequested?&gesture:nullptr);
                if(meleeSession&&(hands.frame!=lastLoggedHand||previous!=c.phase||meleeSession->lastCommand.issued||meleeSession->lastCommand.accepted||meleeSession->lastCommand.retired||meleeSession->lastCommand.revoked)){
                    const auto& command=meleeSession->lastCommand;
                    logFile->event("melee_session_command","\"frame\":"+std::to_string(hands.frame)+",\"issued\":"+std::to_string(command.issued)+",\"accepted\":"+std::to_string(command.accepted)+",\"retired\":"+std::to_string(command.retired)+",\"current\":"+std::to_string(command.current)+",\"revoked\":"+(command.revoked?"true":"false")+",\"reason\":"+std::to_string(unsigned(command.reason))+",\"world\":"+hex(s.world)+",\"can_primary\":"+(s.canPrimary?"true":"false")+",\"native_ms\":"+std::to_string(s.nativeMs));
                }
                {std::lock_guard<std::mutex> lock(handMutex);handFeedback={hands.frame,hands.sessionGeneration,hands.originGeneration,now,{0,0}};
                 if(c.phase!=evr::game::MotionAttackFixture::Phase::Aborted&&c.phase!=evr::game::MotionAttackFixture::Phase::Complete&&c.total>0&&normalHud&&valid){handFeedback.amplitude[1]=.6f*float(c.remaining/c.total);if(twoHandAttackRequested)handFeedback.amplitude[0]=handFeedback.amplitude[1];}}
                if(twoHandAttackRequested&&lastLoggedHand!=hands.frame){
                    logFile->event("two_hand_attack_support","\"frame\":"+std::to_string(hands.frame)+",\"left_grab\":"+(hands.hands[0].grab?"true":"false")+",\"left_tracked\":"+(hands.hands[0].tracked()?"true":"false")+",\"blocked\":"+std::to_string(unsigned(c.gesture.blocked))+",\"request\":"+std::to_string(c.gesture.request));
                }
                if(meleeSession&&meleeSession->lastCommand.retired){
                    std::lock_guard<std::mutex> lock(attackTicketMutex);const auto& command=meleeSession->lastCommand;
                    if(!attackTicket.valid||attackTicketRequest!=command.retired)throw std::runtime_error("session retirement does not own contact ticket");
                    attackTicket.valid=false;attackTicket.revoked=command.revoked;
                    logFile->event("melee_ticket_retired","\"frame\":"+std::to_string(hands.frame)+",\"request\":"+std::to_string(command.retired)+",\"serial\":"+std::to_string(attackTicket.serial)+",\"valid\":false,\"revoked\":"+(command.revoked?"true":"false"));
                }
                if(c.gesture.request){std::lock_guard<std::mutex> lock(attackTicketMutex);if(meleeSession&&attackTicket.valid)throw std::runtime_error("prior melee ticket still live");attackTicketRequest=c.gesture.request;attackTicket={s.entity,s.item,s.triggerTime,now,++attackTicketSerial,true,true,s.binding,hands.frame,hands.sessionGeneration,hands.originGeneration};attackTicket.worlds=s.worlds;
                    if(epochAllowsTicket&&!epochAllowsTicket(s.world)){attackTicket.valid=false;attackTicket.revoked=true;}
                    logFile->event("native_attack_ticket","\"entity\":"+std::to_string(s.entity)+",\"item\":"+std::to_string(s.item)+",\"previous_trigger\":"+std::to_string(s.triggerTime)+",\"issued_ms\":"+std::to_string(now)+",\"serial\":"+std::to_string(attackTicketSerial)+",\"motion\":true,\"request\":"+std::to_string(c.gesture.request)+",\"frame\":"+std::to_string(hands.frame)+",\"session\":"+std::to_string(hands.sessionGeneration)+",\"origin\":"+std::to_string(hands.originGeneration)+(epochContextFields?epochContextFields(attackTicket.worlds):std::string{}));}
                publishAttack(c.pulseLevel);
                if(previous!=c.phase||lastLoggedHand!=hands.frame){lastLoggedHand=hands.frame;
                    std::ostringstream o;o<<std::setprecision(10)<<"\"phase\":"<<unsigned(c.phase)<<",\"frame\":"<<hands.frame<<",\"time\":"<<hands.predictedTime
                    <<",\"completed_attacks\":"<<c.completed<<",\"request\":"<<c.request<<",\"gesture_request\":"<<c.gesture.request<<",\"speed\":"<<c.gesture.speed<<",\"blocked\":"<<unsigned(c.gesture.blocked)
                    <<",\"gesture_guard\":"<<(combatGestureRequested?"true":"false")<<",\"gesture_frame\":"<<gesture.frame<<",\"gesture_time\":"<<gesture.time
                    <<",\"gesture_active\":"<<(gesture.active?"true":"false")<<",\"gesture_reset\":"<<(gesture.resetSwing?"true":"false")
                    <<",\"speed_basis\":\"controller_translation_v2_right_two_hand\",\"velocity_previous_frame\":"<<c.gesture.velocityPreviousFrame
                    <<",\"controller_speed\":["<<evr::length(c.gesture.controllerVelocity[0])<<","<<evr::length(c.gesture.controllerVelocity[1])<<"]"
                    <<",\"buttons\":"<<(c.pulseLevel?attackButtons:0)<<",\"accepted\":"<<(c.accepted?"true":"false")
                    <<",\"item\":"<<s.item<<",\"used_item\":"<<s.usedItem<<",\"entity\":"<<s.entity<<",\"binding\":"<<s.binding
                    <<",\"sequence\":"<<s.sequence<<",\"trigger_time\":"<<s.triggerTime<<",\"played_time\":"<<s.playedTime<<",\"current_time\":"<<s.currentTime<<",\"length\":"<<s.length
                    <<",\"active\":"<<(s.active?"true":"false")<<",\"remaining\":"<<c.remaining<<",\"total\":"<<c.total<<",\"haptic_diagnostic\":"<<c.gesture.haptic[1]
                    <<",\"observation_valid\":"<<(s.valid?"true":"false")<<",\"normal_ui\":"<<(normalHud?"true":"false");logFile->event("motion_attack_sample",o.str());}
                if(c.phase==evr::game::MotionAttackFixture::Phase::Complete){motionAttackCompleted=true;invalidateTicket();stop=true;}
                if(c.phase==evr::game::MotionAttackFixture::Phase::Aborted){
                    revokeTicket();publishAttack(0);
                    const bool fresh=c.request==1&&hands.focused&&normalHud&&valid&&s.valid&&s.alive&&s.ready&&stamp&&now>=stamp&&now-stamp<=200;
                    trackingLossObserved=fresh&&trackingLossExpected&&!hands.hands[1].tracked();
                    offhandReleaseObserved=fresh&&offhandReleaseExpected&&twoHandAttackRequested&&hands.hands[0].tracked()&&hands.hands[1].tracked()&&hands.hands[0].grabActive&&!hands.hands[0].grab;
                    const bool expected=trackingLossObserved||offhandReleaseObserved;failed=!expected;stop=true;
                    logFile->event(offhandReleaseExpected?"motion_offhand_release_abort":"motion_tracking_loss_abort",std::string("\"expected\":")+(expected?"true":"false")+",\"frame\":"+std::to_string(hands.frame)+",\"request\":"+std::to_string(c.request)+",\"left_grab\":"+(hands.hands[0].grab?"true":"false")+",\"left_tracked\":"+(hands.hands[0].tracked()?"true":"false")+",\"right_tracked\":"+(hands.hands[1].tracked()?"true":"false")+",\"ticket_revoked\":true,\"pulse_released\":true");
                }
            }
            if(attackRequested&&!disconnectAt&&!stop.load()){
                if(!observeAttack)throw std::runtime_error("attack observer absent");
                auto s=observeAttack();s.valid=s.valid&&valid;
                const auto previous=attack.phase();const auto c=attack.update(now,nativeConnected!=0,screen,auxiliary,s);
                if(previous!=c.phase&&c.pulseLevel){std::lock_guard<std::mutex> lock(attackTicketMutex);attackTicket={s.entity,s.item,s.triggerTime,now,++attackTicketSerial,true};
                    logFile->event("native_attack_ticket","\"entity\":"+std::to_string(s.entity)+",\"item\":"+std::to_string(s.item)+",\"previous_trigger\":"+std::to_string(s.triggerTime)+",\"issued_ms\":"+std::to_string(now)+",\"serial\":"+std::to_string(attackTicketSerial));}
                publishAttack(c.pulseLevel);
                if(previous!=c.phase)logFile->event("controller_attack_phase","\"phase\":"+std::to_string(unsigned(c.phase))+
                    ",\"item\":"+std::to_string(s.item)+",\"entity\":"+std::to_string(s.entity)+",\"right_trigger\":"+std::to_string(attackButtons?0:c.pulseLevel)+",\"buttons\":"+std::to_string(attackButtons&&c.pulseLevel?attackButtons:0)+
                    ",\"active_samples\":"+std::to_string(s.activeSamples)+",\"active\":"+(s.active?"true":"false")+",\"screen\":"+std::to_string(screen)+",\"auxiliary\":"+std::to_string(auxiliary)+",\"observation_valid\":"+(s.valid?"true":"false"));
                if(c.phase==evr::game::ControllerAttackFixture::Phase::Complete){attackCompleted=true;invalidateTicket();stop=true;}
                if(c.phase==evr::game::ControllerAttackFixture::Phase::Aborted){invalidateTicket();publishAttack(0);failed=true;stop=true;}
            }
            if(!disconnectAt&&(stop.load()||(!liveRequested&&!menuRequested&&!motionRequested&&!attackRequested&&!motionAttackRequested&&!gestureRequested&&!equipmentRequested&&!guardRequested&&!backendExitRequested&&now-begin>=2500))){
                publishAttack(0);publishButtons(0);
                if(liveRequested){
                    publishLive({});
                    if(!liveNeutralPolled()){
                        if(!releaseAt)releaseAt=now;
                        const auto last=liveLastPollMs.load();const bool polling=last&&now>=last&&now-last<=500;
                        if(polling&&now-releaseAt<=2000)return;
                        if(polling)throw std::runtime_error("live neutral pad not polled");
                        if(now-releaseAt<=600)return; // let a just-refocused game poll once
                        liveReleasedUnpolled=true;
                        logFile->event("controller_live_release_unpolled","\"reason\":\"game not polling input (window not in front)\",\"last_poll_age_ms\":"+std::to_string(last&&now>=last?now-last:UINT64_MAX)+",\"samples\":"+std::to_string(liveSamples));
                    }else logFile->event("controller_live_release_confirmed","\"samples\":"+std::to_string(liveSamples)+",\"stale_ms\":"+std::to_string(liveStaleMs));
                }
                if(wheelRequested){
                    publishWheel(0,0,0);std::lock_guard<std::mutex> lock(wheelMutex);
                    if(wheelObserved.load()!=wheelState.dwPacketNumber){if(now-begin>65000)throw std::runtime_error("wheel neutral packet not polled");return;}
                }
                if(!wheelRequested&&(gestureRequested||equipmentRequested||guardRequested)&&observedPacket.load()!=packetButtons.load()){
                    if(now-begin>65000)throw std::runtime_error("gesture neutral packet not polled");return;
                }
                if((attackRequested||motionAttackRequested)&&observedPacket.load()!=attackPacket.load()){
                    if(!releaseAt)releaseAt=now;
                    if(now>=releaseAt&&now-releaseAt<=500)return; // One actual neutral poll before disconnect.
                    throw std::runtime_error("neutral attack release was not polled before disconnect");
                }
                if(attackRequested||motionAttackRequested)logFile->event("controller_attack_release_confirmed","\"neutral_packet\":"+std::to_string(attackPacket.load()>>8)+",\"observed_packet\":"+std::to_string(observedPacket.load()>>8)+",\"release_before_disconnect\":true");
                connected=false;disconnectAt=now;
                logFile->event("controller_fixture_disconnected","\"states\":"+std::to_string(states.load())+",\"neutral_only\":"+neutralLabel()+"");
            }
            if(disconnectAt&&now-disconnectAt>=1500)restore();
        }
    }catch(const std::exception& e){
        publishAttack(0);publishButtons(0);stop=true;failed=true;
        if(liveRequested)publishLive({});
        if(!((liveRequested||gestureRequested||equipmentRequested||guardRequested)&&verifiedOwner&&installed.load()&&active.load())){active=false;connected=false;}
        if(!installed.load())done=true;
        if(!failureReported.exchange(true))try{logFile->event("controller_fixture_failure","\"reason\":"+jsonString(e.what()));}catch(...){}
    }
}
// Worker-side request: MOD values only, no frontend/backend/World pointer
// reads or table writes. The caller must then disable and drain its UI hook.
// This is deliberately NOT completion: only native polling or owner destruction
// may finish the connection. The original XInput module was pinned at install.
inline void stopGameplayReaders(){
 if(!backendExitRequested||!inputModulePinned||!backendWaitReady.load()||!installed.load()||failed.load())throw std::runtime_error("reader stop needs a proven pinned native input owner");
 if(menuRequested||motionRequested||attackRequested||gestureRequested||equipmentRequested||guardRequested)throw std::runtime_error("reader handoff supports neutral or motion combat input only");
 if(readersStopped.load())return;
 const auto stoppedAttack=attackPacket.stop();stop=true;revokeTicket();
 const auto packet=motionAttackRequested?stoppedAttack:packetButtons.load();const unsigned packetShift=motionAttackRequested?8:16;
 {std::lock_guard<std::mutex> lock(handMutex);handFeedback={};}
 if(readersStopped.exchange(true))return;
 logFile->event("controller_gameplay_readers_stopped","\"neutral_packet\":"+std::to_string(packet>>packetShift)+",\"motion_input\":"+(motionAttackRequested?"true":"false")+",\"ticket_revoked\":true,\"reads_native_memory\":false,\"writes_native_memory\":false,\"claims_neutral_poll\":false,\"claims_completion\":false,\"thread\":"+std::to_string(GetCurrentThreadId()));
}
// The incoming native callback supplies the live backend. Saved pointer values
// serve only as identity checks; this routine is never called by a timer/worker.
inline BackendExitToken beginBackendExit(uintptr_t nativeBackend,uintptr_t caller){
 if(!backendExitRequested||!requested||!installed.load()||done.load()||nativeBackend!=backend)return {};
 if(caller!=0xf97414||GetCurrentThreadId()!=ownerThread||!ownerSerial||!inputModulePinned||backendExitBegun.load())throw std::runtime_error("input owner destruction scope differs");
 if(read<uintptr_t>(nativeBackend)!=manager||read<uintptr_t>(manager+0x48)!=nativeBackend||read<HMODULE>(nativeBackend+8)!=module)throw std::runtime_error("input owner destruction identity differs");
 pendingBackendExit={ownerSerial,nativeBackend,manager,reinterpret_cast<uintptr_t>(module),ownerThread};
 backendExitBegun=true;stop=true;active=false;connected=false;
 const auto stoppedAttack=attackPacket.stop();publishButtons(0);const bool attackKind=attackRequested||motionAttackRequested;const auto neutral=attackKind?stoppedAttack:packetButtons.load();const unsigned packetShift=attackKind?8:16;revokeTicket();invalidateTicket();
 {std::lock_guard<std::mutex> lock(handMutex);handFeedback={};}
 if(meleeSession&&!epochCoordinatorStopped){meleeSession->closeEpoch();epochCoordinatorStopped=true;}
 const bool stateRestored=restoreEntry(nativeBackend+0x10,reinterpret_cast<uintptr_t>(&state),reinterpret_cast<uintptr_t>(nativeState));
 const bool rumbleRestored=restoreEntry(nativeBackend+0x18,reinterpret_cast<uintptr_t>(&rumble),reinterpret_cast<uintptr_t>(nativeRumble));
 const bool capsRestored=restoreEntry(nativeBackend+0x20,reinterpret_cast<uintptr_t>(&caps),reinterpret_cast<uintptr_t>(nativeCaps));
 backendExitTableRestored=stateRestored&&rumbleRestored&&capsRestored;
 installed=false;if(!backendExitTableRestored)failed=true;
 logFile->event("controller_backend_exit_handoff","\"owner_serial\":"+std::to_string(ownerSerial)+",\"backend\":"+hex(nativeBackend)+",\"manager\":"+hex(manager)+",\"module\":"+hex(reinterpret_cast<uintptr_t>(module))+",\"thread\":"+std::to_string(ownerThread)+",\"caller\":"+hex(caller)+",\"pointers_restored\":"+(backendExitTableRestored?"true":"false")+",\"original_api\":["+hex(reinterpret_cast<uintptr_t>(nativeState))+","+hex(reinterpret_cast<uintptr_t>(nativeRumble))+","+hex(reinterpret_cast<uintptr_t>(nativeCaps))+"]"+",\"restored_api\":["+hex(read<uintptr_t>(nativeBackend+0x10))+","+hex(read<uintptr_t>(nativeBackend+0x18))+","+hex(read<uintptr_t>(nativeBackend+0x20))+"]"+",\"module_pinned\":true,\"neutral_packet\":"+std::to_string(neutral>>packetShift)+",\"packet_kind\":"+jsonString(attackKind?"attack":"buttons")+",\"log_schema\":2,\"neutral_was_polled\":"+(observedPacket.load()==neutral?"true":"false")+",\"callbacks\":"+std::to_string(callbacks.load())+",\"native_connected_before\":"+std::to_string(read<uint8_t>(nativeBackend+0x50))+",\"ticket_revoked\":true,\"writes_connection_flag\":false,\"reads_world\":false,\"native_original_not_called_yet\":true");
 return pendingBackendExit;
}
inline bool finishBackendExit(const BackendExitToken& token){
 // No native pointer read, including manager/backend/module, after destruction.
 if(!token.valid()||!backendExitBegun.load()||backendExitReturned.load()||!(token==pendingBackendExit)||token.serial!=ownerSerial||GetCurrentThreadId()!=token.thread)return false;
 backendExitReturned=true;done=true;
 if(!backendExitTableRestored||callbacks.load())failed=true;
 logFile->event("controller_backend_exit_returned","\"owner_serial\":"+std::to_string(token.serial)+",\"backend\":"+hex(token.backend)+",\"thread\":"+std::to_string(GetCurrentThreadId())+",\"callbacks\":"+std::to_string(callbacks.load())+",\"pointers_restored_before_original\":"+(backendExitTableRestored?"true":"false")+",\"reads_destroyed_memory\":false,\"writes_destroyed_memory\":false,\"claims_neutral_poll\":false,\"claims_polled_disconnect\":false,\"native_original_returned\":true");
 return !failed.load();
}
// Called ONLY from the verified original frontend predicate's owner scope,
// before any World/UI read. Finishing the already owned input connection does
// not require an Actor, a camera, a current HUD, or a fresh tracked hand sample.
// This still requires real native polling/disconnection; it cannot manufacture
// completion once the backend itself is gone.
inline bool serviceStoppedFrontend(uintptr_t frontend){
 if(backendExitBegun.load()||readersStopped.load())return true;
 if(!requested||!motionAttackRequested||!observeEpoch)return false;
 checkEpochStop();if(!epochStopped.load())return false;
 if(done.load())return true;
 if(!installed.load()){done=true;return true;}
 if(menuRequested||motionRequested||attackRequested||gestureRequested||equipmentRequested||guardRequested){failed=true;return true;}
 onScope(frontend,false,255,255,false);return true;
}
// The world was left while the pad was in use (the start by itself, 2026-10-14: back to the menu, or the game closed,
// with the eyes still drawn): the frontend scope that ends the connection in its order does not come any more (game
// start 20261014-103739: "controller fixture restoration/callbacks incomplete", and the mod's pad stayed in the game's
// table). The pad then reports itself as taken away - the game's own polling sees that, as when a real pad is
// unplugged - and the three table entries go back from this thread, each only if it still is the mod's.
inline std::atomic<bool> worldLeft{false},restoredAfterWorldLeft{false};
inline void restoreAfterWorldLeft(){
    publishAttack(0);publishButtons(0);if(liveRequested)publishLive({});
    const auto statesBefore=states.load(),seen=disconnects.load();
    connected=false;const auto until=GetTickCount64()+1500;
    while(disconnects.load()<seen+3&&GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    active=false;bool ok=true,mine=false;
    try{
        // The backend's table is read before anything is written: after the world it may be another object.
        mine=read<uintptr_t>(backend+0x10)==reinterpret_cast<uintptr_t>(&state)&&read<uintptr_t>(backend+0x18)==reinterpret_cast<uintptr_t>(&rumble)&&read<uintptr_t>(backend+0x20)==reinterpret_cast<uintptr_t>(&caps);
        if(mine){
            ok=restoreEntry(backend+0x10,reinterpret_cast<uintptr_t>(&state),reinterpret_cast<uintptr_t>(nativeState))&&ok;
            ok=restoreEntry(backend+0x18,reinterpret_cast<uintptr_t>(&rumble),reinterpret_cast<uintptr_t>(nativeRumble))&&ok;
            ok=restoreEntry(backend+0x20,reinterpret_cast<uintptr_t>(&caps),reinterpret_cast<uintptr_t>(nativeCaps))&&ok;
        }
    }catch(...){mine=false;}
    const auto drained=GetTickCount64()+1000;
    while(callbacks.load()&&GetTickCount64()<drained)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    installed=false;done=true;if(!ok)failed=true;restoredAfterWorldLeft=true;
    logFile->event("controller_fixture_restored_after_world_left","\"table_was_the_mod_s\":"+std::string(mine?"true":"false")+",\"pointers_restored\":"+(mine&&ok?"true":"false")+
        ",\"states\":"+std::to_string(states.load())+",\"polled_while_leaving\":"+std::to_string(states.load()-statesBefore)+",\"disconnect_results\":"+std::to_string(disconnects.load())+
        ",\"callbacks\":"+std::to_string(callbacks.load())+",\"thread\":"+std::to_string(GetCurrentThreadId()));
}
inline void drain(){
    if(!requested)return;publishAttack(0);stop=true;const auto deadline=GetTickCount64()+(worldLeft.load()?1000:5000);
    while((!done.load()||callbacks.load())&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if(!done.load()&&worldLeft.load()&&installed.load()&&!backendExitBegun.load())restoreAfterWorldLeft();
    active=false;connected=false;
    if(!done.load()||installed.load()||callbacks.load()){failed=true;throw std::runtime_error("controller fixture restoration/callbacks incomplete");}
    if(restoredAfterWorldLeft.load()&&!failed.load())return;   // the ordered ending checked below belongs to a world that is still there
    if(backendExitRequested&&backendExitReturned.load()){
        if(!backendExitTableRestored||!inputModulePinned||failed.load())throw std::runtime_error("native input owner handoff failed");
        return; // Distinct native destruction completion, never a fabricated poll.
    }
    if(motionAttackRequested&&!motionAttackCompleted&&!expectedMotionCancellation()){failed=true;throw std::runtime_error("native motion attack diagnostic incomplete");}
    if(attackRequested&&!attackCompleted){failed=true;throw std::runtime_error("native controller attack sequence incomplete");}
    if(motionRequested&&!motionCompleted){failed=true;throw std::runtime_error("native controller motion sequence incomplete");}
    if(guardRequested&&!guardCompleted){failed=true;throw std::runtime_error("native guard diagnostic incomplete");}
    if(equipmentRequested&&!equipmentCompleted){failed=true;throw std::runtime_error("native equipment switch diagnostic incomplete");}
    if(gestureRequested&&!gestureCompleted){failed=true;throw std::runtime_error("native gesture menu diagnostic incomplete");}
    if(menuRequested&&!menuCompleted){failed=true;throw std::runtime_error("native controller menu sequence incomplete");}
    if(liveRequested&&liveReleasedUnpolled&&states.load()&&nativeConnectedSeen&&!failed.load())return; // see liveLastPollMs
    if(!states.load()||!disconnects.load()||!nativeConnectedSeen||!nativeDisconnectedSeen){failed=true;throw std::runtime_error("controller fixture connection/disconnection not polled");}
}
}
