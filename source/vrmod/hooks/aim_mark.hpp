#pragma once
// view_resource_probe.cpp includes these at file scope first, like the other value headers: there this file sits
// inside its unnamed namespace.
#include <evr/aim_mark.hpp>
#include <evr/bow_physical_trial.hpp>
#include <evr/motion_actions.hpp>
#include <evr/ranged_aim.hpp>
// The mark of where the bow, the staff or the wand aims (include/evr/aim_mark.hpp). The game's reticle is a dot in the
// middle of the HUD - where the camera looks; here the shot leaves along the hands, so the user asked after the
// headset run of 2026-10-13 that the dot follow the right hand and be where the weapon aims.
// Each XR frame the aimed line is taken in the tracking space - the bow while its string is held: from the string
// hand through the arrow rest; a staff or wand in the hand: from its emitter along the hand's pointer ray, or along
// the weapon when both hands hold it - and the point 128 m along it (VHVR's distance for a weapon's mark) is told to
// the layer (evrSetAimMark). The layer draws a dot there into both eyes' images and leaves the middle of the HUD - the
// game's dot - out of the UI image meanwhile.
// VHVR shows a staff's mark only while both hands hold it, and the bow's path as a line (which the user did not
// want, 2026-10-11). Here the mark shows whenever one of the three aims, as the user asked.
// On with the bow's draw or the staff's aim by the hands (-BowDraw, -RangedAim); EVR_AIM_MARK=0 in the game's
// environment leaves the game's own dot (for a comparison).
namespace aimMark {
using MarkFn=int(*)(const float*,int);
inline MarkFn setMark=nullptr;inline bool enabled=false,shown=false;inline int lastKind=-1;inline unsigned logs=0;inline uint64_t lastLog=0;
// How far along the line the mark sits (include/evr/aim_mark.hpp markReach): asked with the line in the game's world
// (live_melee_targets.hpp aimReach, set by view_resource_probe.cpp - this file is read before the world is known).
// EVR_AIM_MARK_REACH=0 in the game's environment: always 128 m, as before 2026-10-13.
inline float (*reachOf)(evr::WorldPositionQ32,evr::Vec3,uint64_t,int&)=nullptr;inline bool reachWanted=true;
inline void install(bool requested){
    char value[8]{};const auto n=GetEnvironmentVariableA("EVR_AIM_MARK",value,sizeof value);
    if(!requested||(n&&n<sizeof value&&value[0]=='0'))return;
    if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))setMark=reinterpret_cast<MarkFn>(GetProcAddress(layer,"evrSetAimMark"));
    {char reach[8]{};const auto r=GetEnvironmentVariableA("EVR_AIM_MARK_REACH",reach,sizeof reach);reachWanted=!(r&&r<sizeof reach&&reach[0]=='0');}
    enabled=setMark!=nullptr;
    logFile->event("aim_mark_enabled",std::string("\"layer_draws\":")+(enabled?"true":"false")+",\"distance_m\":"+std::to_string(evr::aim::markDistance)+",\"game_dot_left_out_of_the_ui_image\":true");
}
// Once per XR frame, on the thread that publishes the hands (view_resource_probe.cpp, next to menuPointer::frame).
// bowHeld: a hand holds the bow's string this frame; bow / ranged: the weapon as shown (tracking space), empty when it
// is not; screen: the game's screen (1 is the HUD of play).
inline void frame(const evr::motion::HandFrame& f,bool bowHeld,const evr::motion::BowPhysicalGeometry& bow,uint32_t rangedItem,const evr::motion::BowPhysicalGeometry& ranged,bool rangedTwoHanded,uint8_t screen,uint64_t now){
    if(!enabled)return;
    try{
        evr::aim::Line line;int kind=0;const auto& right=f.hands[1];const evr::motion::BowPhysicalGeometry* shownWeapon=nullptr;
        const bool play=screen==1&&f.focused&&f.frame;
        if(play&&bowHeld&&bow.binding&&bow.frame==f.frame&&right.tracked()&&f.hands[0].tracked()){
            line=evr::aim::bowLine(bow.model,bow.scale,right.grip.position);kind=1;shownWeapon=&bow;
        }else if(play&&rangedItem&&ranged.binding&&ranged.frame==f.frame&&right.tracked()){
            const auto* weapon=evr::motion::rangedAimModel(rangedItem);
            if(weapon&&(rangedTwoHanded?f.hands[0].tracked():right.aimActive&&(right.aimFlags&15)==15)){
                line=evr::aim::rangedLine(ranged.model,ranged.scale,weapon->emitterY,rangedTwoHanded,right.aim);kind=rangedTwoHanded?3:2;shownWeapon=&ranged;}
        }
        if(!line.valid)kind=0;
        float reach=evr::aim::markDistance;int by=0;
        if(line.valid&&reachWanted&&reachOf&&shownWeapon&&shownWeapon->sourceWorld){
            // The same line in the game's world, as the shot takes it (ranged_aim.hpp).
            try{const auto world=evr::motion::rangedWorldAim(shownWeapon->model,shownWeapon->worldPosition,shownWeapon->worldOrientation,line.origin,line.direction);
                reach=reachOf(world.origin,world.direction,now,by);}
            catch(const std::exception&){reach=evr::aim::markDistance;by=0;}
        }
        if(line.valid){const auto p=evr::aim::markPoint(line,reach);const float v[3]={p.x,p.y,p.z};if(setMark(v,1)){line.valid=false;kind=0;}}
        if(!line.valid&&shown)setMark(nullptr,0);
        shown=line.valid;
        if((kind!=lastKind||(kind&&now>=lastLog+1000))&&logs<400){
            ++logs;lastKind=kind;lastLog=now;std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<f.frame<<",\"kind\":\""<<(kind==1?"bow":kind==2?"pointer":kind==3?"two_hands":"none")<<"\",\"shown\":"<<(shown?"true":"false")
                <<",\"origin\":["<<line.origin.x<<','<<line.origin.y<<','<<line.origin.z<<"],\"direction\":["<<line.direction.x<<','<<line.direction.y<<','<<line.direction.z<<"],\"reach_m\":"<<reach<<",\"ends_on\":\""<<(by==2?"creature":by==1?"ground":"far")<<"\",\"screen\":"<<unsigned(screen);
            logFile->event("aim_mark_state",o.str());
        }
    }catch(const std::exception& e){static std::atomic<unsigned> failures{0};if(failures.fetch_add(1)<3)logFile->event("aim_mark_failure","\"reason\":"+jsonString(e.what()));}
}
inline void disable(){
    if(!enabled)return;
    enabled=false;if(shown){setMark(nullptr,0);shown=false;}
    logFile->event("aim_mark_removed","\"records\":"+std::to_string(logs));
}
}
