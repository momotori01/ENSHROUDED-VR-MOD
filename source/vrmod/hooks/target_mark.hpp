#pragma once
// The mark of the locked combat target (2026-10-19; user after a headset run: "I cannot tell what is locked on - its health
// bar is on the HUD, not above the enemy's head"). VHVR (docs/analysis/vhvr_spec_20261010/ui_hud.md section 6) puts each
// enemy's HUD on a world space canvas at the character's top point, turned to the camera and scaled with the distance.
// The first step here: while the camera is one of a locked target (camera_mode.hpp targetLockCamera), a mark stands in
// both eyes' images just above the head of the player's combat target - its position and the height of its box from the
// player's SelectedTargets / PlayerRelevantEnemies (live_melee_targets.hpp combatTarget) - so that the locked enemy is
// the one marked (the layer draws it, evrSetTargetMark). The world point goes into the XR space by the anchor of the
// last eye render (game_stereo.hpp trackingPointFromWorld). The game's own bar stays on the HUD for now.
// EVR_TARGET_MARK=0 in the game's environment: no mark.
#include <evr/game_stereo.hpp>
namespace targetMark {
using MarkFn=int(*)(const float*,int);
// The chevron of the mod's own over the target: off since the headset run of 2026-10-19 15:22 (user: "the mark of our own is
// not needed any more - the game's own is enough"). EVR_TARGET_MARK_CHEVRON=1 draws it again.
inline bool chevron=false;
inline MarkFn setMark=nullptr,setHud=nullptr;inline bool enabled=false,shown=false,hudShown=false,fixture=false;inline unsigned logs=0,projectionLogs=0;inline uint32_t lastEntity=0;
inline std::mutex anchorMutex;inline game::StableAnchor anchor{};inline Pose trackingToAnchor=identityPose();inline uint64_t anchorMs=0;
// The game's own camera of the same render (its parameters before the eye cameras replace them: position +0x38, orientation
// +0x50, field of view +0x78 - 1.1345 rad, 65 degrees, in the Mock runs): the camera the game's spatial HUD (FbUiHUDSpatial:
// the lock-on arrow, the enemies' health bars and level badges, the interaction prompts) is projected with. Mock
// game-20261019-142512: the head alone turned in 30 degree steps and the prompt and the NPC's marker stayed put in the HUD
// image. The user after the headset run of 2026-10-19 14:00: "the enemy's health bar does not follow it; use the game's own
// together with the mark" - the HUD image's part about the locked target goes to the target (layer evrSetTargetHud).
inline WorldPositionQ32 nativePosition{};inline Quat nativeOrientation{0,0,0,1};inline float nativeFov=0;
// The height of the health bar's anchor above the target's base: the top of its box (no box: hudWithoutBox). Whether the
// game's field of view is the vertical one (EVR_SPATIAL_HUD_FOV=h: the horizontal one) - to be checked against the HUD image
// of a bug report taken while locked (target_mark_projection gives the anchor pixel the layer used, target_hud_part).
constexpr float hudWithoutBox=1.7f;
inline bool fovVertical(){static const bool v=[]{char x[4]{};return !(GetEnvironmentVariableA("EVR_SPATIAL_HUD_FOV",x,sizeof x)==1&&(x[0]=='h'||x[0]=='H'));}();return v;}
// How far above the box's top (or, without a box, above the target's position) the mark stands, metres.
constexpr float aboveTop=.35f,withoutBox=2.f;
// From the eye render of each frame (view_resource_probe.cpp, beside stereoCameras): the anchor its eye cameras were made with.
inline void noteAnchor(const game::StableAnchor& a,Pose t,WorldPositionQ32 camera,Quat orientation,float fov){std::lock_guard<std::mutex> lock(anchorMutex);anchor=a;trackingToAnchor=t;anchorMs=GetTickCount64();
    nativePosition=camera;nativeOrientation=orientation;nativeFov=fov;}
inline void install(){
    char value[8]{};const auto n=GetEnvironmentVariableA("EVR_TARGET_MARK",value,sizeof value);
    if(n==1&&value[0]=='0'){logFile->event("target_mark_off","\"reason\":\"EVR_TARGET_MARK=0\"");return;}
    if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll")){setMark=reinterpret_cast<MarkFn>(GetProcAddress(layer,"evrSetTargetMark"));setHud=reinterpret_cast<MarkFn>(GetProcAddress(layer,"evrSetTargetHud"));}
    // EVR_TARGET_HUD=0: the game's health bar stays in the HUD.
    if(GetEnvironmentVariableA("EVR_TARGET_HUD",value,sizeof value)==1&&value[0]=='0')setHud=nullptr;
    enabled=setMark!=nullptr;
    // EVR_TARGET_MARK_FIXTURE=1 (Mock runs without an enemy): with no locked target, the mark stands 3 m ahead of the root
    // camera along the anchor's facing and 0.3 m up - the world point goes the same way into the XR space and the layer draws it.
    fixture=GetEnvironmentVariableA("EVR_TARGET_MARK_FIXTURE",value,sizeof value)==1&&value[0]=='1';
    chevron=GetEnvironmentVariableA("EVR_TARGET_MARK_CHEVRON",value,sizeof value)==1&&value[0]=='1';
    logFile->event("target_mark_enabled",std::string("\"layer_draws\":")+(enabled?"true":"false")+",\"above_top_m\":"+std::to_string(aboveTop)+",\"lock_cameras\":[5,8,10,12,14,17],\"fixture\":"+(fixture?"true":"false")+",\"health_bar_moved\":"+(setHud?"true":"false")+",\"fov_vertical\":"+(fovVertical()?"true":"false"));
}
// Once per XR frame, on the thread that publishes the hands (view_resource_probe.cpp, next to aimMark::frame).
inline void frame(uint64_t xrFrame,uint64_t now){
    if(!enabled)return;
    try{
        // Whenever the game has a combat target (locked or not), not only while its camera is a lock camera: the headset run
        // of 2026-10-19 15:22 lost the lock camera a dozen times in 30 s (cameras 4, 6, 20, 25 for a moment, the target's
        // number 0 for a frame), and each time the health bar went back into the HUD (user: "it shows in the UI as well?
        // odd as it goes away"). The game draws the widget - the arrow, with the bar once locked - for its target all the
        // same. A target lost for a frame is bridged by the layer, which keeps the last part for a quarter of a second.
        rootCamera::Sample root;bool locked=rootCamera::trySnapshot(root)&&root.milliseconds&&now>=root.milliseconds&&now-root.milliseconds<=1000;
        // The combat target, else the game's hard-lock candidate (2026-10-19 19:00: the game shows the candidate's bar too).
        liveTargets::CombatTarget target;bool candidateOnly=false;if(locked&&(!liveTargets::hudTarget(target,candidateOnly,now)||!target.entity))locked=false;
        game::StableAnchor a;Pose t;uint64_t at=0;WorldPositionQ32 camera{};Quat cameraOrientation{0,0,0,1};float fov=0;
        {std::lock_guard<std::mutex> lock(anchorMutex);a=anchor;t=trackingToAnchor;at=anchorMs;camera=nativePosition;cameraOrientation=nativeOrientation;fov=nativeFov;}
        const bool anchored=at&&now>=at&&now-at<=500;
        if(!anchored)locked=false;
        // The bar after the target is lost (2026-10-19 18:45, user: "when the enemy leaves my view the health bar shows in
        // the UI"; headset run 20261019-183558: the game's combat target went for 1..1.5 s at a time - 7322..7455,
        // 7854..7940, 8282..8407 - while the game may still show the bar of the enemy it was): for up to 3 s the last
        // target, where it stands now among the creatures kept around the player (liveTargets::entityPosition), keeps
        // the bar's part moving with it. EVR_TARGET_HUD_LINGER_MS sets the time (0: none).
        static const uint64_t lingerMs=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_TARGET_HUD_LINGER_MS",v,sizeof v);return n&&n<sizeof v?std::strtoull(v,nullptr,10):3000ull;}();
        static liveTargets::CombatTarget lingerTarget{};static uint64_t lingerUntil=0;bool lingering=false;
        if(locked){lingerTarget=target;lingerUntil=now+lingerMs;}
        else if(anchored&&lingerTarget.entity&&now<lingerUntil){std::array<int64_t,3> p{};
            if(liveTargets::entityPosition(lingerTarget.entity,p,now)){target=lingerTarget;target.position=p;target.relevantPosition=p;locked=lingering=true;}}
        Vec3 point{};
        if(!locked&&fixture&&anchored){
            const auto orientation=normalized(a.orientation);
            const auto rootCamera=cameraPosition(a.root,rotate(orientation,a.cameraOffsetGameUnits),1);
            const auto ahead=rotate(orientation,game::trackingVector({0,.3f,-3.f}))*float(a.worldUnitsPerMeter);
            point=game::trackingPointFromWorld(a,t,cameraPosition(rootCamera,ahead,1));
            const float v[3]={point.x,point.y,point.z};
            // The health bar's route as well, for the fixture's point (the HUD image's part there moves to it).
            if(setHud&&std::isfinite(fov)&&fov>.2f&&fov<3.f){
                const auto world=cameraPosition(rootCamera,ahead,1);
                const Vec3 offset{float(relativeQ32(world.x,camera.x)),float(relativeQ32(world.y,camera.y)),float(relativeQ32(world.z,camera.z))};
                const Vec3 seen=rotate(inverse(normalized(cameraOrientation)),offset);
                if(finite(seen)&&seen.z>.3f){const float h[7]={point.x,point.y,point.z,seen.x/seen.z,seen.y/seen.z,std::tan(fov*.5f),fovVertical()?1.f:0.f};hudShown=setHud(h,1)==0;
                    if(xrFrame%60==0&&projectionLogs++<200)logFile->event("target_mark_projection","\"frame\":"+std::to_string(xrFrame)+",\"fixture\":true,\"slopes\":["+std::to_string(seen.x/seen.z)+','+std::to_string(seen.y/seen.z)+"],\"native_fov\":"+std::to_string(fov));}
            }
            if(!chevron||!setMark(v,1)){
                if(!shown&&logs++<400){std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<xrFrame<<",\"fixture\":true,\"point\":["<<point.x<<','<<point.y<<','<<point.z<<"]";logFile->event("target_mark_state",o.str());}
                shown=true;return;
            }
        }
        if(locked){
            const bool box=target.relevant&&target.boxValid&&std::isfinite(target.height)&&target.height>.2f&&target.height<20.f;
            const auto& base=target.relevant?target.relevantPosition:target.position;
            const WorldPositionQ32 top{base[0],addQ32(base[1],double(box?target.height+aboveTop:withoutBox)),base[2]};
            point=game::trackingPointFromWorld(a,t,top);
            const float v[3]={point.x,point.y,point.z};
            if(chevron&&setMark(v,1))locked=false;
            // The game's health bar: its anchor as the game's camera sees it (game camera space, +z ahead).
            bool hud=false;Vec3 hudPoint{};float slopes[2]{};
            if(locked&&setHud&&std::isfinite(fov)&&fov>.2f&&fov<3.f){
                const WorldPositionQ32 hudAnchor{base[0],addQ32(base[1],double(box?target.height:hudWithoutBox)),base[2]};
                const Vec3 offset{float(relativeQ32(hudAnchor.x,camera.x)),float(relativeQ32(hudAnchor.y,camera.y)),float(relativeQ32(hudAnchor.z,camera.z))};
                const Vec3 seen=rotate(inverse(normalized(cameraOrientation)),offset);
                if(finite(seen)&&seen.z>.3f&&length(offset)<200.f){
                    hudPoint=game::trackingPointFromWorld(a,t,hudAnchor);slopes[0]=seen.x/seen.z;slopes[1]=seen.y/seen.z;
                    const float h[7]={hudPoint.x,hudPoint.y,hudPoint.z,slopes[0],slopes[1],std::tan(fov*.5f),fovVertical()?1.f:0.f};
                    hud=setHud(h,1)==0;
                }
            }
            hudShown=hudShown||hud;
            // Every 15th frame while locked: the target's world point (its base, the box top) and the game's camera, to compare
            // with where the game's lock-on arrow stands in the HUD image (a bug report's ui-<n>.jpg, a Mock capture's ui-frame).
            if(xrFrame%15==0&&projectionLogs++<4000){std::ostringstream o;o<<std::setprecision(10)<<"\"frame\":"<<xrFrame<<",\"entity\":"<<target.entity<<",\"camera\":"<<unsigned(root.cameraId)
                <<",\"base_q32\":["<<base[0]<<','<<base[1]<<','<<base[2]<<"],\"box_height\":"<<(box?target.height:0.f)<<",\"targeting_offset\":[";for(size_t k=0;k<target.offset.size();++k)o<<(k?",":"")<<target.offset[k];
                o<<"],\"native_q32\":["<<camera.x<<','<<camera.y<<','<<camera.z<<"],\"native_orientation\":["<<cameraOrientation.x<<','<<cameraOrientation.y<<','<<cameraOrientation.z<<','<<cameraOrientation.w
                <<"],\"native_fov\":"<<fov<<",\"point\":["<<point.x<<','<<point.y<<','<<point.z<<"],\"health_bar\":"<<(hud?"true":"false")<<",\"slopes\":["<<slopes[0]<<','<<slopes[1]<<"],\"hud_point\":["<<hudPoint.x<<','<<hudPoint.y<<','<<hudPoint.z<<"]";
                logFile->event("target_mark_projection",o.str());}
        }
        if(!locked&&shown)setMark(nullptr,0);
        // (No explicit end for the health bar's part: the layer lets it go a quarter of a second after the last call.)
        static bool lastLingering=false;
        if(locked!=shown||(locked&&target.entity!=lastEntity)||lingering!=lastLingering){lastLingering=lingering;
            if(logs++<400){std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<xrFrame<<",\"shown\":"<<(locked?"true":"false")<<",\"lingering\":"<<(lingering?"true":"false")<<",\"candidate\":"<<(candidateOnly?"true":"false")<<",\"camera\":"<<unsigned(root.cameraId)
                <<",\"entity\":"<<(locked?target.entity:0)<<",\"box_height\":"<<(locked?target.height:0.f)<<",\"box_valid\":"<<(locked&&target.boxValid?"true":"false")
                <<",\"point\":["<<point.x<<','<<point.y<<','<<point.z<<"]";
                logFile->event("target_mark_state",o.str());}
            lastEntity=locked?target.entity:0;
        }
        shown=locked;
    }catch(const std::exception& e){static std::atomic<unsigned> failures{0};if(failures.fetch_add(1)<3)logFile->event("target_mark_failure","\"reason\":"+jsonString(e.what()));
        if(shown){setMark(nullptr,0);shown=false;}if(hudShown&&setHud){setHud(nullptr,0);hudShown=false;}}
}
inline void disable(){
    if(!enabled)return;
    enabled=false;if(shown){setMark(nullptr,0);shown=false;}if(hudShown&&setHud){setHud(nullptr,0);hudShown=false;}
    logFile->event("target_mark_removed","\"records\":"+std::to_string(logs));
}
}
