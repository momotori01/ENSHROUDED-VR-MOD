#pragma once
#include <evr/game_stereo.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace evr::game {
// The 1.0 build (25936191, 2026-10-16) renumbered CameraId (its reflection table: name, length, value). The policies
// below keep the June values; a value read from the game goes through this first. New cameras take their nearest June
// kind: SprintingTarget -> Sprinting, MotherFlame -> Altar, the building fly modes -> BuildingThirdPerson, the wand's
// free aim -> the staff's (default, sprinting -> Sprinting, sneaking). 255 (no camera) and unknown values stay out of
// range (Unknown).
inline uint8_t legacyCameraId(uint8_t id){
    static constexpr uint8_t map[46]{
        0,1,2,3,4,5,          // EntityCamera .. DefaultTarget
        6,6,                  // SprintingTarget (new), Sprinting
        7,9,11,13,            // RangedBowDefault, RangedBowSneaking, RangedStaffDefault, RangedStaffSneaking
        15,16,17,18,19,20,    // Hipfire, HipfireStaffChanneling, ...Target, Execute, Idle, Sneaking
        21,22,23,24,25,26,27, // Sitting, Cooking, Climbing, HookShot, Glider, Swimming, Diving
        28,29,30,31,          // HeadDivingStart, HeadDivingDefault, Dialogue, Altar
        31,                   // MotherFlame (new)
        32,33,34,35,36,37,38, // Obelisk, ShroudForge, BuildingAltar, BuildingAreaDepositProp, BuildingFirstPerson(+Sneak), ThirdPerson
        38,38,                // BuildingFlySparkMode, ...Sprinting (new)
        39,40,41,42,          // Fishing, FishingHardFocusFloat, FishingSoftFocusFloat, FishingVictory
        11,6,13};             // FreeAimWandDefault, FreeAimWandSprinting, FreeAimWandSneaking (new)
    return id<46?map[id]:id==255?255:uint8_t(45);
}
// The pad styles (2026-10-25, include/evr/vr_settings.hpp Style; the session's world-pad-mode.txt): 0 none, 1 first person
// played with the gamepad, 2 the game's own third-person camera in the headset. padPitchLock: the third person's view stays
// level (the game camera's pitch left out); padTiltDegrees: the pad styles' view tilted down, 0 .. 90, for playing lying down.
inline std::atomic<int> padStyle{0};inline std::atomic<bool> padPitchLock{true};inline std::atomic<int> padTiltDegrees{0};
// The tilt as a rotation of the anchor's space: about its right axis (x), down by the degrees (a face turned up to the
// ceiling at 90 looks ahead).
inline Quat padTilt(int degrees){const float a=-float(std::clamp(degrees,0,90))*3.14159265f/180.f;return {std::sin(a/2),0,0,std::cos(a/2)};}
// The view reset of the pad panel (2026-10-25 evening, user: "on the view reset the slider moves to the head's angle now"):
// the head's pitch above level, in whole degrees 0 .. 90 (looking down counts 0), measured in the plane the head's right side
// leaves upright - lying on the back with the face to the ceiling is 90. padTiltFromHead: the next reset takes it.
inline std::atomic<bool> padTiltFromHead{false};
inline int padTiltOf(Quat head){
    const auto right=rotate(normalized(head),Vec3{1,0,0}),forward=rotate(normalized(head),Vec3{0,0,-1});
    const float yaw=right.x*right.x+right.z*right.z>1e-6f?std::atan2(-right.z,right.x):0.f;
    const auto level=rotate(euler(-yaw,0,0),forward);   // the forward with the right side turned onto +x
    const float pitch=std::atan2(level.y,-level.z)*180.f/3.14159265f;
    return std::clamp(int(std::lround(pitch)),0,90);
}
// Values validated against CameraId reflection for build23966345. This is a
// presentation policy, not a claim that every named state is a cutscene.
enum class CameraPresentation { FirstPerson, NativeEvent, NativeSpecial, Unknown };
inline CameraPresentation cameraPresentation(uint8_t id){
    // The third-person pad style: every camera of the game is shown as the game has it (a native camera: the headset's
    // view anchored at the game camera, root_camera_bridge.hpp renderAnchor).
    if(padStyle.load()==2)return id==255?CameraPresentation::Unknown:CameraPresentation::NativeEvent;
    // Every camera of the game is first person (2026-10-24 15:55, user: "the altar's, the obelisk's, the shroud forge's cameras -
    // and the others too: make every camera first person"). Until then the event cameras kept the game's own view in the headset:
    // 0 EntityCamera, 1 / 2 the spawning, 3 the respawning (also the teleports and being revived), 31 the altar, 32 the obelisk,
    // 33 the shroud forge; 30 the dialogue was made first person an hour before (user: "talking to someone turns it third person";
    // log 20261024-151525: 4 -> 30 on each talk). Cameras of a later build (45, legacyCameraId) as well. Only 255 - no camera of
    // the player yet - is not known. The game keeps steering these cameras itself: headingRealignCamera leaves them alone.
    if(id==255)return CameraPresentation::Unknown;
    // 18 (PlayerCameraExecute) is the sneak attack from behind - the game's camera of the stab, about 2 m behind the
    // player - kept the game's own until 2026-10-21 (user: "a backstab from a sneak attack switches to the third-person
    // camera"). It is the player's own attack: first person, as VHVR attacks.
    // 38 is the camera the game uses while a pickaxe is in hand (Mock run game103654: 19 -> 38 on selecting
    // it, back to 4 on the sword). It is ordinary play, and the native third-person view in the headset was
    // reported on the first headset run (game100334), so it is first person like the other play cameras.
    // 39..41 are the cameras of fishing (Mock run game065355: 4 -> 39 while the rod is cast, back to 4): the headset
    // showed the player from behind for the length of the cast. Fishing is ordinary play (VHVR fishes in first
    // person). The victory camera (42, the catch shown to the player) was the game's own until 2026-10-19 (headset run
    // 20261019-185327, user: "after the catch it turns third person"): first person too.
    // 36 / 37 (PlayerCameraBuildingFirstPerson, ...Sneak) are what 38 (BuildingThirdPerson) becomes when the player
    // toggles the game's "first person mode" (right stick click with a building tool in hand, in a flame altar's
    // area; the HUD names it). Headset run game145015: that toggle, hit by accident, left the player with the game's
    // camera, without hands, tool or pointer aim until it was toggled back ("after summoning I cannot move, the
    // camera is odd, the staff is not in my hand"). Both are the same play as 38 and are first person here.
    // 34 / 35 (PlayerCameraBuildingAltar, ...BuildingAreaDepositProp) are the cameras of a flame altar or an area prop in
    // the hand, ready to be placed - kept the game's own (third person, about 6 m behind) until 2026-10-19. Headset run
    // 20261019-104950 (the user: "placing an altar the camera went mad; back at the menu and in again, VR did not start
    // - the altar is in my hand"): the headset jumped to that camera, and with the altar still in hand at the next world
    // the first-person anchor was never asked for, so the height never became ready (world_pair_waiting_height). They
    // are placing like the building tool's 36..38: first person, the placement following the pointer (building_cursor_ray).
    return CameraPresentation::FirstPerson;
}
// Body heading (VHVR: the body follows the headset's yaw). The game picks the local actor's look direction in
// 2873a0 (branch at 289115): a mode byte of 1 decides by state (movement direction, or the camera while a bow or
// staff is in hand or the block is held), 3 and 4 always take the camera. Live play presents 3 during ordinary
// ground play; every other camera (climbing, swimming, gliding, sitting, dialogue, building ...) keeps the game's choice.
constexpr uint8_t lookModeByState=1,lookModeCamera=3;
inline bool bodyHeadingCamera(uint8_t id){
    return id==4||id==5||id==6||id==19||id==20||id==38; // Default, DefaultTarget, Sprinting, Idle, Sneaking, tool in hand
}
inline const char* cameraPresentationName(CameraPresentation p){
    switch(p){case CameraPresentation::FirstPerson:return "first_person";
    case CameraPresentation::NativeEvent:return "native_event";
    case CameraPresentation::NativeSpecial:return "native_special";
    default:return "unknown_native";}
}
// Explicit render-only diagnostic sequence; never modifies CameraId in ECS.
inline uint8_t cameraModeFixture(uint32_t completedPairs,uint8_t nativeId){
    if(completedPairs<100)return nativeId;
    if(completedPairs<400)return 30; // event camera
    if(completedPairs<450)return 34; // native building special (the altar's building camera)
    if(completedPairs<500)return 255; // future/unknown state
    return nativeId;
}
// The absolute accumulator belongs to the native update observer, which adds
// only weighted user orbit input during first-person gameplay. Rendering may
// sample it repeatedly; it must never integrate an input a second time.
class StableHeading {
    bool calibrated_=false;uint32_t entity_=0;
    double originTurn_=0;float originYaw_=0;
public:
    bool calibrated()const{return calibrated_;}
    // The native camera's heading plus the accumulated turn: constant for as long as only the player turns the camera.
    bool reference(uint32_t entity,double& value)const{
        if(!calibrated_||entity!=entity_)return false;
        value=double(originYaw_)+originTurn_;return true;
    }
    void reset(){*this=StableHeading{};}
    float value(uint32_t entity,Quat initialCamera,double turn){
        if(!entity||!std::isfinite(turn))throw std::invalid_argument("invalid stable heading identity/input");
        if(calibrated_&&entity!=entity_)throw std::invalid_argument("stable heading entity changed; reset required");
        if(!calibrated_){originYaw_=heading(normalized(initialCamera),0);originTurn_=turn;entity_=entity;calibrated_=true;}
        // Native camera/animation rotations are deliberately not sampled again.
        // The accumulated orbit input turns the native camera the opposite way to heading()'s angle: in the first
        // real-device runs (2026-10-09, game-20261009-103959) every right-stick span moved the native camera heading by
        // -d while an added turn moved this anchor by +d, so the VR view turned left on a right turn (and jumps, which
        // move along the native camera, went sideways the wrong way). Subtract the accumulated input.
        const double angle=double(originYaw_)-(turn-originTurn_);
        if(!std::isfinite(angle))throw std::invalid_argument("stable heading overflow");
        return float(std::remainder(angle,2*double(pi)));
    }
};
// The game also turns its camera by itself (run game-20261013-185120, keyboard and mouse in the Mock runtime: 17 degrees
// with no input on leaving the glider camera, 26 more with the grappling hook). The VR view keeps the player's own
// turns only, so from then on the native camera pointed elsewhere, and the game still steers by it wherever the head
// camera is not sent (keyboard play, swimming until 2026-10-13): "after the grappling hook the direction input was
// off". The difference is fed back into the game's own orbit input, a part per camera update, until the native camera
// stands on the VR heading again. The VR view does not turn: the amount is kept out of the accumulated turn.
// Cameras the game steers by itself for as long as they last are left alone; the difference is taken out afterwards.
inline bool headingRealignCamera(uint8_t id){
    if(id==5||id==8||id==10||id==12||id==14||id==17)return false; // the ...Target cameras: the game keeps them on the locked target
    if(id<=3||(id>=30&&id<=33)||id>=45)return false;   // the event cameras the game steers (above), and those of a later build
    return cameraPresentation(id)==CameraPresentation::FirstPerson&&id!=23&&id!=24&&id!=25&&id!=28&&id!=29; // Climbing, HookShot, Glider, HeadDiving
}
// The cameras of a locked combat target (June ids): DefaultTarget 5 and the bow's / staff's / hipfire's ...Target ones.
inline bool targetLockCamera(uint8_t id){return id==5||id==8||id==10||id==12||id==14||id==17;}
struct HeadingRealign {float gain=.3f,limit=.15f,dead=.002f;};
// Native heading minus VR heading, in (-pi, pi]. reference: StableHeading::reference.
inline float headingError(float nativeYaw,double turn,double reference){
    if(!std::isfinite(nativeYaw)||!std::isfinite(turn)||!std::isfinite(reference))throw std::invalid_argument("heading error input");
    return float(std::remainder(double(nativeYaw)+turn-reference,2*double(pi)));
}
// What to add to the weighted orbit input of the next camera update: the native heading turns by the opposite of it.
inline float headingCorrection(float error,const HeadingRealign& s={}){
    if(!std::isfinite(error)||std::abs(error)<=s.dead)return 0;
    return std::clamp(error*s.gain,-s.limit,s.limit);
}
// The correction's keeper: when a difference appeared, how far it went, and whether the correction works.
// A difference that grows while it is corrected is the game steering the camera just then (a blend out of the glider's
// camera did that for a third of a second) or a camera that answers the orbit input the other way: the correction
// rests for a while and tries again; `rests` such rests without the difference ever going away end it for the run.
// One that does not go away in `held` corrected updates is a camera the game holds: no correction until the camera
// changes. The growth is judged from the difference the correction (re)starts at: a camera that is left alone may
// have added to it since the difference was first seen - a ladder turned the camera by 119 degrees, and the first
// version, judging from the first sight of the difference, ended the realignment at the foot of the ladder (Mock
// session game-20261014-004300: "forwards and backwards are swapped now - I think it is the ladder").
class HeadingRealignWatch {
public:
    enum class Event {None,Begin,End,Rest,Held,Stopped};
    struct Step {float correction=0;Event event=Event::None;};
    struct Limits {float begin=.01f,end=.004f,grow=.5f;unsigned held=600,rest=90,rests=8;};
    bool stopped()const{return stopped_;}
    bool active()const{return active_;}
    float first()const{return first_;}
    float peak()const{return peak_;}
    unsigned corrected()const{return corrected_;}
    Step update(float error,uint8_t camera,bool wanted,const HeadingRealign& s={},const Limits& l={}){
        Step out;
        if(!std::isfinite(error)){correcting_=false;return out;}
        if(held_>=0&&held_!=int(camera))held_=-1;
        if(resting_)--resting_;
        const bool may=wanted&&!stopped_&&held_<0&&!resting_&&headingRealignCamera(camera);
        if(!active_&&std::abs(error)>l.begin){active_=true;first_=peak_=error;corrected_=0;correcting_=false;out.event=Event::Begin;}
        if(active_){
            if(std::abs(error)>std::abs(peak_))peak_=error;
            if(std::abs(error)<=l.end){active_=false;correcting_=false;rests_=0;out.event=Event::End;}
        }
        if(!may){correcting_=false;return out;}
        if(active_){
            if(!correcting_){correcting_=true;start_=error;samples_=0;}
            else if(std::abs(error)>std::abs(start_)+l.grow){
                correcting_=false;
                if(++rests_>=l.rests){stopped_=true;out.event=Event::Stopped;}else{resting_=l.rest;out.event=Event::Rest;}
                return out;
            }
            if(++samples_>=l.held){held_=int(camera);correcting_=false;out.event=Event::Held;return out;}
            ++corrected_;
        }
        out.correction=headingCorrection(error,s);
        return out;
    }
private:
    bool active_=false,correcting_=false,stopped_=false;
    float first_=0,peak_=0,start_=0;
    unsigned corrected_=0,samples_=0,resting_=0,rests_=0;int held_=-1;
};
inline StableAnchor nativeCameraAnchor(WorldPositionQ32 p,Quat q){
    StableAnchor a;a.root=p;a.orientation=normalized(q);return a;
}
} // namespace evr::game
