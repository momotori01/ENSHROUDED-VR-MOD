#pragma once
#include <evr/math.hpp>
#include <cstring>
#include <string>
#include <vector>

namespace evr::motion {
struct HandSample {
    Pose grip=identityPose(),aim=identityPose();
    XrSpaceLocationFlags gripFlags=0,aimFlags=0;
    XrPath profile=XR_NULL_PATH;
    float trigger=0;XrVector2f stick{};bool stickActive=false,stickClickActive=false,stickClick=false;
    bool gripActive=false,aimActive=false,triggerActive=false,grabActive=false,grab=false;
    bool primaryActive=false,primaryPressed=false,secondaryActive=false,secondaryPressed=false;
    // Finger curl inputs (2026-10-09, EDF6VR-style hands): the grip value used for the press (force where present) and
    // the thumb resting on the stick. ABI v3.
    float squeeze=0;bool thumbTouchActive=false,thumbTouch=false;
    bool tracked()const {return gripActive&&(gripFlags&15)==15;}
};
struct HandFrame {
    uint64_t frame=0,sessionGeneration=0,originGeneration=0;
    XrTime predictedTime=0;
    bool focused=false;
    std::array<HandSample,2> hands{};
};
// Explicitly owned by one session and serialized on its XR thread. No system
// bindings, input component, player transform, or global dispatch is modified.
class Actions {
    XrInstance instance=XR_NULL_HANDLE;XrSession session=XR_NULL_HANDLE;
    XrActionSet set=XR_NULL_HANDLE;
    XrAction gripPose=XR_NULL_HANDLE,aimPose=XR_NULL_HANDLE,trigger=XR_NULL_HANDLE,grab=XR_NULL_HANDLE,grabForce=XR_NULL_HANDLE,stickTouch=XR_NULL_HANDLE,haptic=XR_NULL_HANDLE,stick=XR_NULL_HANDLE,stickClick=XR_NULL_HANDLE,primaryFace=XR_NULL_HANDLE,secondaryFace=XR_NULL_HANDLE;
    std::array<XrPath,2> hands{};
    std::array<XrSpace,2> gripSpaces{},aimSpaces{};
    uint64_t generation=0,lastFrame=0;XrTime lastTime=0;
    bool attached=false,focused=false;
    std::array<bool,2> canHaptic{};
    // Grip press with hysteresis (2026-10-09 real device: Index squeeze/value rises from a hand merely resting on the
    // grip, so the old boolean binding fired on its own). As in the user's EDF6VR: the Index force sensor decides where
    // it exists (press 0.30 / release 0.10), otherwise squeeze value or click (press 0.75 / release 0.45).
    std::array<bool,2> gripHeld{};
    // Optional grip orientation trim (2026-10-09 real device): a natural hold of a real controller points the grip -Z
    // (pinky->thumb tube) almost straight up, while the hand/weapon calibration and the Mock fixtures treat a forward
    // tube as the natural hold, so hands and weapons looked ~60-90 degrees off. The user's EDF6VR settles the same with
    // yaw 4.47 / pitch -59.27 / roll 3.61 degrees on the right hand (left: yaw and roll mirrored), applied in the grip's
    // own frame. Orientation only; the grip position (palm centre) is unchanged. Identity unless set.
    std::array<Quat,2> gripTrim{{{0,0,0,1},{0,0,0,1}}};
public:
    void setGripTrim(float yawDegrees,float pitchDegrees,float rollDegrees){
        for(float v:{yawDegrees,pitchDegrees,rollDegrees})if(!std::isfinite(v)||std::abs(v)>180)throw std::invalid_argument("grip trim must be finite degrees");
        constexpr float d=3.14159265358979f/180;
        gripTrim[1]=normalized(euler(yawDegrees*d,pitchDegrees*d,rollDegrees*d));
        gripTrim[0]=normalized(euler(-yawDegrees*d,pitchDegrees*d,-rollDegrees*d));
    }
    Quat gripTrimFor(unsigned hand)const{return gripTrim.at(hand);}
    static constexpr float gripForcePress=.30f,gripForceRelease=.10f,gripPress=.75f,gripRelease=.45f;
    static bool gripStep(bool held,float value,bool force){
        return held?value>(force?gripForceRelease:gripRelease):value>(force?gripForcePress:gripPress);
    }
private:
#define EVR_MOTION_FUNCTIONS(X) \
 X(xrStringToPath) X(xrPathToString) X(xrCreateActionSet) X(xrDestroyActionSet) X(xrCreateAction) \
 X(xrSuggestInteractionProfileBindings) X(xrAttachSessionActionSets) X(xrCreateActionSpace) X(xrDestroySpace) \
 X(xrSyncActions) X(xrGetActionStatePose) X(xrGetActionStateFloat) X(xrGetActionStateBoolean) X(xrGetActionStateVector2f) \
 X(xrGetCurrentInteractionProfile) X(xrLocateSpace) X(xrApplyHapticFeedback) X(xrStopHapticFeedback)
#define DECLARE(n) PFN_##n n=nullptr;
    EVR_MOTION_FUNCTIONS(DECLARE)
#undef DECLARE
    static void check(XrResult result,const char* name){if(XR_FAILED(result))throw std::runtime_error(std::string(name)+" XrResult="+std::to_string(result));}
    XrPath path(const char* text){XrPath p=XR_NULL_PATH;check(xrStringToPath(instance,text,&p),"motion path");return p;}
    XrAction create(const char* name,const char* label,XrActionType type){
        XrActionCreateInfo ci{XR_TYPE_ACTION_CREATE_INFO};std::strcpy(ci.actionName,name);std::strcpy(ci.localizedActionName,label);
        ci.actionType=type;ci.countSubactionPaths=2;ci.subactionPaths=hands.data();XrAction a=XR_NULL_HANDLE;
        check(xrCreateAction(set,&ci,&a),"motion create action");return a;
    }
    void pose(XrAction action,XrSpace space,unsigned hand,XrSpace base,XrTime time,Pose& result,XrSpaceLocationFlags& flags,bool& active){
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.action=action;gi.subactionPath=hands[hand];
        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};check(xrGetActionStatePose(session,&gi,&state),"motion pose state");
        active=state.isActive!=0;flags=0;result=identityPose();if(!active)return;
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};check(xrLocateSpace(space,base,time,&location),"motion locate space");
        flags=location.locationFlags;
        if((flags&3)==3){if(!finite(location.pose.position))throw std::runtime_error("nonfinite hand position");
            result={normalized(location.pose.orientation),location.pose.position};}
    }
public:
    std::vector<std::string> unsupportedProfiles;
    Actions()=default;Actions(const Actions&)=delete;Actions& operator=(const Actions&)=delete;
    ~Actions(){close();}
    void open(PFN_xrGetInstanceProcAddr get,XrInstance i,XrSession s,uint64_t sessionGeneration){
        if(instance||!get||!i||!s||!sessionGeneration)throw std::invalid_argument("motion actions need a fresh valid session");
        instance=i;session=s;generation=sessionGeneration;
        try {
#define LOAD(n) {PFN_xrVoidFunction fn=nullptr;check(get(instance,#n,&fn),#n);if(!fn)throw std::runtime_error("null " #n);n=reinterpret_cast<PFN_##n>(fn);}
            EVR_MOTION_FUNCTIONS(LOAD)
#undef LOAD
            hands={path("/user/hand/left"),path("/user/hand/right")};
            XrActionSetCreateInfo ci{XR_TYPE_ACTION_SET_CREATE_INFO};std::strcpy(ci.actionSetName,"evr_motion");std::strcpy(ci.localizedActionSetName,"Enshrouded VR motion");
            check(xrCreateActionSet(instance,&ci,&set),"motion action set");
            gripPose=create("grip_pose","Hand grip pose",XR_ACTION_TYPE_POSE_INPUT);
            aimPose=create("aim_pose","Hand pointer pose",XR_ACTION_TYPE_POSE_INPUT);
            trigger=create("trigger","Weapon trigger",XR_ACTION_TYPE_FLOAT_INPUT);
            grab=create("grab","Hold weapon",XR_ACTION_TYPE_FLOAT_INPUT);
            grabForce=create("grab_force","Grip force",XR_ACTION_TYPE_FLOAT_INPUT);
            stickTouch=create("stick_touch","Thumb on stick",XR_ACTION_TYPE_BOOLEAN_INPUT);
            haptic=create("haptic","Weapon feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT);
            stick=create("stick","Move and turn stick",XR_ACTION_TYPE_VECTOR2F_INPUT);
            stickClick=create("stick_click","Stick press",XR_ACTION_TYPE_BOOLEAN_INPUT);
            primaryFace=create("primary_face","Primary face button",XR_ACTION_TYPE_BOOLEAN_INPUT);
            secondaryFace=create("secondary_face","Secondary face button",XR_ACTION_TYPE_BOOLEAN_INPUT);
            // Standard core profiles, independent of the HMD manufacturer.
            struct Profile{const char* name;const char* squeeze;const char* stick;};
            const Profile profiles[]{
                {"/interaction_profiles/oculus/touch_controller","/input/squeeze/value","/input/thumbstick"},
                {"/interaction_profiles/valve/index_controller","/input/squeeze/value","/input/thumbstick"},
                {"/interaction_profiles/htc/vive_controller","/input/squeeze/click","/input/trackpad"},
                {"/interaction_profiles/microsoft/motion_controller","/input/squeeze/click","/input/thumbstick"}};
            for(const auto& profile:profiles){
                std::vector<XrActionSuggestedBinding> bindings;
                for(const auto* hand:{"/user/hand/left","/user/hand/right"})
                    for(const auto& b:std::array<std::pair<XrAction,const char*>,5>{{{gripPose,"/input/grip/pose"},{aimPose,"/input/aim/pose"},{trigger,"/input/trigger/value"},{grab,profile.squeeze},{haptic,"/output/haptic"}}})
                        bindings.push_back({b.first,path((std::string(hand)+b.second).c_str())});
                for(const auto* hand:{"/user/hand/left","/user/hand/right"}){
                    const std::string source=std::string(hand)+profile.stick;
                    bindings.push_back({stick,path(source.c_str())});bindings.push_back({stickClick,path((source+"/click").c_str())});
                }
                // Touch: left X/Y, right A/B. Index: A/B on either hand.
                // Controllers without two face buttons leave these actions unbound;
                // never consume a runtime system/menu button as a substitute.
                const bool touch=std::strcmp(profile.name,"/interaction_profiles/oculus/touch_controller")==0;
                const bool index=std::strcmp(profile.name,"/interaction_profiles/valve/index_controller")==0;
                if(index)for(const auto* hand:{"/user/hand/left","/user/hand/right"})
                    bindings.push_back({grabForce,path((std::string(hand)+"/input/squeeze/force").c_str())});
                // Thumb rest: stick touch on Touch/Index, trackpad touch on Vive; WMR has no stick touch.
                if(std::strcmp(profile.name,"/interaction_profiles/microsoft/motion_controller")!=0)
                    for(const auto* hand:{"/user/hand/left","/user/hand/right"})
                        bindings.push_back({stickTouch,path((std::string(hand)+profile.stick+"/touch").c_str())});
                if(touch||index)for(unsigned h=0;h<2;h++){
                    const std::string hand=h?"/user/hand/right":"/user/hand/left";
                    bindings.push_back({primaryFace,path((hand+(touch&&h==0?"/input/x/click":"/input/a/click")).c_str())});
                    bindings.push_back({secondaryFace,path((hand+(touch&&h==0?"/input/y/click":"/input/b/click")).c_str())});
                }
                XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                suggested.interactionProfile=path(profile.name);suggested.countSuggestedBindings=uint32_t(bindings.size());suggested.suggestedBindings=bindings.data();
                const auto r=xrSuggestInteractionProfileBindings(instance,&suggested);
                if(r==XR_ERROR_PATH_UNSUPPORTED)unsupportedProfiles.push_back(profile.name);else check(r,"motion suggest bindings");
            }
            XrSessionActionSetsAttachInfo ai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};ai.countActionSets=1;ai.actionSets=&set;
            check(xrAttachSessionActionSets(session,&ai),"motion attach action set");attached=true;
            for(unsigned h=0;h<2;h++)for(unsigned type=0;type<2;type++){
                XrActionSpaceCreateInfo si{XR_TYPE_ACTION_SPACE_CREATE_INFO};si.action=type?aimPose:gripPose;si.subactionPath=hands[h];si.poseInActionSpace=identityPose();
                check(xrCreateActionSpace(session,&si,type?&aimSpaces[h]:&gripSpaces[h]),"motion create action space");
            }
        }catch(...){close();throw;}
    }
    HandFrame sample(XrSpace base,XrTime predictedTime,uint64_t frame,uint64_t originGeneration){
        if(!attached||!base||predictedTime<=0||!frame||!originGeneration||frame<=lastFrame||predictedTime<=lastTime)
            throw std::invalid_argument("invalid or repeated motion sample epoch/frame/time");
        lastFrame=frame;lastTime=predictedTime;focused=false;canHaptic={};
        HandFrame out;out.frame=frame;out.predictedTime=predictedTime;out.sessionGeneration=generation;out.originGeneration=originGeneration;
        XrActiveActionSet active{set,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
        const auto r=xrSyncActions(session,&sync);if(r==XR_SESSION_NOT_FOCUSED){stop(0);stop(1);gripHeld={};return out;}check(r,"motion sync actions");
        if(r!=XR_SUCCESS)throw std::runtime_error("unexpected motion sync status");
        out.focused=true;
        for(unsigned h=0;h<2;h++){
            auto& v=out.hands[h];XrInteractionProfileState p{XR_TYPE_INTERACTION_PROFILE_STATE};
            check(xrGetCurrentInteractionProfile(session,hands[h],&p),"motion current profile");v.profile=p.interactionProfile;
            pose(gripPose,gripSpaces[h],h,base,predictedTime,v.grip,v.gripFlags,v.gripActive);
            if((v.gripFlags&1)!=0)v.grip.orientation=normalized(multiply(v.grip.orientation,gripTrim[h]));
            pose(aimPose,aimSpaces[h],h,base,predictedTime,v.aim,v.aimFlags,v.aimActive);
            XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.subactionPath=hands[h];gi.action=trigger;
            XrActionStateFloat t{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&gi,&t),"motion trigger state");
            v.triggerActive=t.isActive!=0;if(v.triggerActive){if(!std::isfinite(t.currentState)||t.currentState<0||t.currentState>1)throw std::runtime_error("invalid motion trigger value");v.trigger=t.currentState;}
            gi.action=grab;XrActionStateFloat g{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&gi,&g),"motion grab state");
            gi.action=grabForce;XrActionStateFloat gf{XR_TYPE_ACTION_STATE_FLOAT};check(xrGetActionStateFloat(session,&gi,&gf),"motion grab force state");
            const bool force=gf.isActive!=0;const float squeeze=force?gf.currentState:g.currentState;
            v.grabActive=g.isActive!=0||force;
            if(v.grabActive&&(!std::isfinite(squeeze)||squeeze<0||squeeze>1))throw std::runtime_error("invalid motion grip value");
            gripHeld[h]=v.grabActive&&gripStep(gripHeld[h],squeeze,force);v.grab=gripHeld[h];v.squeeze=v.grabActive?squeeze:0;
            gi.action=stickTouch;XrActionStateBoolean touch{XR_TYPE_ACTION_STATE_BOOLEAN};check(xrGetActionStateBoolean(session,&gi,&touch),"motion stick touch state");
            v.thumbTouchActive=touch.isActive!=0;v.thumbTouch=v.thumbTouchActive&&touch.currentState;
            gi.action=stick;XrActionStateVector2f axis{XR_TYPE_ACTION_STATE_VECTOR2F};check(xrGetActionStateVector2f(session,&gi,&axis),"motion stick state");
            v.stickActive=axis.isActive!=0;if(v.stickActive){
                if(!std::isfinite(axis.currentState.x)||!std::isfinite(axis.currentState.y)||std::abs(axis.currentState.x)>1||std::abs(axis.currentState.y)>1)
                    throw std::runtime_error("invalid motion stick value");v.stick=axis.currentState;
            }
            gi.action=stickClick;XrActionStateBoolean click{XR_TYPE_ACTION_STATE_BOOLEAN};check(xrGetActionStateBoolean(session,&gi,&click),"motion stick click state");
            v.stickClickActive=click.isActive!=0;v.stickClick=v.stickClickActive&&click.currentState;
            gi.action=primaryFace;XrActionStateBoolean primary{XR_TYPE_ACTION_STATE_BOOLEAN};
            check(xrGetActionStateBoolean(session,&gi,&primary),"motion primary face state");
            v.primaryActive=primary.isActive!=0;v.primaryPressed=v.primaryActive&&primary.currentState;
            gi.action=secondaryFace;XrActionStateBoolean secondary{XR_TYPE_ACTION_STATE_BOOLEAN};
            check(xrGetActionStateBoolean(session,&gi,&secondary),"motion secondary face state");
            v.secondaryActive=secondary.isActive!=0;v.secondaryPressed=v.secondaryActive&&secondary.currentState;
        }
        focused=true;for(unsigned h=0;h<2;h++){canHaptic[h]=out.hands[h].tracked();if(!canHaptic[h])stop(h);}return out;
    }
    std::string profileName(XrPath p)const{
        if(!p)return {};uint32_t n=0;check(xrPathToString(instance,p,0,&n,nullptr),"motion profile length");
        if(!n||n>XR_MAX_PATH_LENGTH)throw std::runtime_error("invalid motion profile length");std::vector<char> result(n);
        check(xrPathToString(instance,p,n,&n,result.data()),"motion profile text");return result.data();
    }
    bool vibrate(unsigned hand,float amplitude,XrDuration duration){
        if(hand>=2||!std::isfinite(amplitude)||amplitude<0||amplitude>1||duration<=0||duration>100000000)
            throw std::invalid_argument("motion haptic must be a bounded finite pulse <=100ms");
        if(!attached||!focused||!canHaptic[hand])return false;
        XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=hands[hand];
        XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION};v.duration=duration;v.frequency=XR_FREQUENCY_UNSPECIFIED;v.amplitude=amplitude;
        auto r=xrApplyHapticFeedback(session,&hi,reinterpret_cast<XrHapticBaseHeader*>(&v));
        if(r==XR_SESSION_NOT_FOCUSED){focused=false;canHaptic={};return false;}check(r,"motion haptic");return r==XR_SUCCESS;
    }
    bool stop(unsigned hand)noexcept{
        if(hand>=2||!attached||!xrStopHapticFeedback)return false;
        XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=hands[hand];
        return XR_SUCCEEDED(xrStopHapticFeedback(session,&hi));
    }
    bool close()noexcept{
        bool ok=true;if(attached)for(unsigned h=0;h<2;h++)ok=stop(h)&&ok;
        if(xrDestroySpace)for(auto* collection:{&gripSpaces,&aimSpaces})for(auto& space:*collection)if(space){ok=XR_SUCCEEDED(xrDestroySpace(space))&&ok;space=XR_NULL_HANDLE;}
        if(set&&xrDestroyActionSet){ok=XR_SUCCEEDED(xrDestroyActionSet(set))&&ok;set=XR_NULL_HANDLE;}
        instance=XR_NULL_HANDLE;session=XR_NULL_HANDLE;attached=focused=false;canHaptic={};gripHeld={};lastFrame=0;lastTime=0;unsupportedProfiles.clear();return ok;
    }
};
#undef EVR_MOTION_FUNCTIONS
} // namespace evr::motion
