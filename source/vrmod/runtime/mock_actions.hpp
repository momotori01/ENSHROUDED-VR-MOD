// Test-only OpenXR action subset. Included after the runtime handle helpers.
// CSV controls are process-local and never install an OS device or binding.
#include <map>
namespace {
struct MotionHand {Pose grip=identityPose(),aim=identityPose();float trigger=0;XrVector2f stick{};bool stickClick=false,primaryFace=false,secondaryFace=false;bool grab=false,active=true,tracked=true;};
struct MotionInput {std::array<MotionHand,2> hands{};bool focused=true;unsigned profile=0;};
struct MotionSet {std::string name;bool attached=false;};
struct MotionAction {MotionSet* owner=nullptr;std::string name;XrActionType type{};std::array<XrPath,2> subpaths{};};
std::set<MotionSet*> motionSets;
std::set<MotionAction*> motionActions;
std::vector<std::string> motionPaths{std::string()};
std::map<XrPath,std::vector<XrActionSuggestedBinding>> motionBindings;
std::vector<MotionInput> motionSamples;
MotionInput motionCurrent{},motionPrevious{};
bool motionAttached=false,motionSynced=false;
const char* motionProfiles[]{"/interaction_profiles/oculus/touch_controller","/interaction_profiles/valve/index_controller",
    "/interaction_profiles/htc/vive_controller","/interaction_profiles/microsoft/motion_controller"};
const MotionInput& motionInput(){
    static const MotionInput defaults{};static MotionInput overlay;
    const auto& base=motionSamples.empty()?defaults:motionSamples[std::min<size_t>(frame?frame-1:0,motionSamples.size()-1)];
    if(navigationPath.empty())return base;
    overlay=base;
    if(!navigationHandInput)for(auto& hand:overlay.hands){hand.stick={};hand.stickClick=hand.primaryFace=hand.secondaryFace=hand.grab=false;hand.trigger=0;}
    if(navigation.active(navigationClock())){
        overlay.hands[0].stick={navigation.command.leftX,navigation.command.leftY};
        if(!navigationHandInput||navigation.command.rightX!=0)overlay.hands[1].stick={navigation.command.rightX,0};
        if(!navigationHandInput||navigation.command.jump!=0)overlay.hands[1].primaryFace=navigation.command.jump!=0;
    }
    return overlay;
}
void resetMotionSession(){motionAttached=motionSynced=false;motionCurrent={};motionPrevious={};for(auto* set:motionSets)set->attached=false;}
XrPath motionPath(const std::string& name){for(size_t i=1;i<motionPaths.size();i++)if(motionPaths[i]==name)return XrPath(i);motionPaths.push_back(name);return XrPath(motionPaths.size()-1);}
int motionHand(XrPath p){if(p>=motionPaths.size())return -1;return motionPaths[size_t(p)]=="/user/hand/left"?0:motionPaths[size_t(p)]=="/user/hand/right"?1:-1;}
void loadMotionSamples(){
    motionSamples.clear();motionPaths={std::string()};motionBindings.clear();resetMotionSession();
    const char* name=std::getenv("EVR_MOCK_HAND_SEQUENCE");if(!name||!*name)return;
    std::ifstream in(name);if(!in)throw std::runtime_error("hand CSV absent");std::string line;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);uint64_t index=0;MotionInput value;
        if(!(row>>index)||index!=motionSamples.size()+1)throw std::runtime_error("hand CSV frame order");
        for(auto& h:value.hands){float yaw,pitch,roll,ay,ap,ar;int grip,active,tracked;
            if(!(row>>h.grip.position.x>>h.grip.position.y>>h.grip.position.z>>yaw>>pitch>>roll>>ay>>ap>>ar>>h.trigger>>grip>>active>>tracked))throw std::runtime_error("hand CSV fields");
            if(!finite(h.grip.position)||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(roll)||!std::isfinite(ay)||!std::isfinite(ap)||!std::isfinite(ar)||
                !std::isfinite(h.trigger)||h.trigger<0||h.trigger>1||grip<0||grip>1||active<0||active>1||tracked<0||tracked>1)throw std::runtime_error("hand CSV range");
            h.grip.orientation=euler(yaw*pi/180,pitch*pi/180,roll*pi/180);h.aim={euler(ay*pi/180,ap*pi/180,ar*pi/180),h.grip.position};
            h.grab=grip!=0;h.active=active!=0;h.tracked=tracked!=0;
        }
        int focus;if(!(row>>focus>>value.profile)||focus<0||focus>1||value.profile>=4)throw std::runtime_error("hand CSV focus/profile");value.focused=focus!=0;
        row>>std::ws;if(!row.eof())for(auto& h:value.hands){
            int pressed;if(!(row>>h.stick.x>>h.stick.y>>pressed)||!std::isfinite(h.stick.x)||!std::isfinite(h.stick.y)||
                std::abs(h.stick.x)>1||std::abs(h.stick.y)>1||pressed<0||pressed>1)throw std::runtime_error("hand CSV optional stick range/fields");
            h.stickClick=pressed!=0;
        }
        row>>std::ws;if(!row.eof())for(auto& h:value.hands){
            int primary,secondary;if(!(row>>primary>>secondary)||primary<0||primary>1||secondary<0||secondary>1)
                throw std::runtime_error("hand CSV optional face button range/fields");
            h.primaryFace=primary!=0;h.secondaryFace=secondary!=0;
        }
        std::string extra;if(row>>extra)throw std::runtime_error("hand CSV extra fields");motionSamples.push_back(value);
        if(motionSamples.size()>1000000)throw std::runtime_error("hand CSV diagnostic budget");
    }
    if(motionSamples.empty())throw std::runtime_error("hand CSV empty");
}
bool motionBound(MotionAction* a,int hand){
    auto profile=motionPath(motionProfiles[motionCurrent.profile]);auto found=motionBindings.find(profile);if(found==motionBindings.end())return false;
    const std::string prefix=hand==0?"/user/hand/left/":"/user/hand/right/";
    for(const auto& b:found->second)if(b.action==reinterpret_cast<XrAction>(a)&&b.binding<motionPaths.size()&&motionPaths[size_t(b.binding)].find(prefix)==0)return true;
    return false;
}
XrResult motionState(XrSession s,const XrActionStateGetInfo* info,XrActionType type,MotionAction*& action,int& hand){
    if(!good(s))return XR_ERROR_HANDLE_INVALID;if(!info||info->type!=XR_TYPE_ACTION_STATE_GET_INFO||info->next)return XR_ERROR_VALIDATION_FAILURE;
    action=reinterpret_cast<MotionAction*>(info->action);if(!motionActions.count(action))return XR_ERROR_HANDLE_INVALID;
    if(action->type!=type)return XR_ERROR_ACTION_TYPE_MISMATCH;if(!motionAttached||!action->owner->attached)return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    hand=motionHand(info->subactionPath);if(hand<0)return XR_ERROR_PATH_UNSUPPORTED;
    if(std::find(action->subpaths.begin(),action->subpaths.end(),info->subactionPath)==action->subpaths.end())return XR_ERROR_PATH_UNSUPPORTED;
    return XR_SUCCESS;
}
bool motionActive(MotionAction* a,int hand){return motionSynced&&motionCurrent.focused&&motionCurrent.hands[size_t(hand)].active&&motionBound(a,hand);}
Pose motionSpacePose(Space* space){auto* a=reinterpret_cast<MotionAction*>(space->action);int h=motionHand(space->subaction);if(h<0||!motionActions.count(a))return identityPose();
    return a->name=="aim_pose"?motionInput().hands[size_t(h)].aim:motionInput().hands[size_t(h)].grip;}
bool motionSpaceTracked(Space* space){auto* a=reinterpret_cast<MotionAction*>(space->action);int h=motionHand(space->subaction);
    return h>=0&&motionActions.count(a)&&motionActive(a,h)&&motionInput().hands[size_t(h)].tracked;}
}
XR_FN(m_xrStringToPath)(XrInstance i,const char* str,XrPath* out){
    if(!good(i))return XR_ERROR_HANDLE_INVALID;if(!str||!out)return XR_ERROR_VALIDATION_FAILURE;
    const std::string text(str);if(text.empty()||text[0]!='/'||text.size()>=XR_MAX_PATH_LENGTH||text.back()=='/'||text.find("//")!=std::string::npos)return XR_ERROR_PATH_FORMAT_INVALID;
    for(auto c:text)if(!(c=='/'||c=='_'||c=='-'||c=='.'||(c>='a'&&c<='z')||(c>='0'&&c<='9')))return XR_ERROR_PATH_FORMAT_INVALID;
    *out=motionPath(text);return XR_SUCCESS;
}
XR_FN(m_xrPathToString)(XrInstance i,XrPath path,uint32_t capacity,uint32_t* count,char* out){
    if(!good(i))return XR_ERROR_HANDLE_INVALID;if(!path||path>=motionPaths.size())return XR_ERROR_PATH_INVALID;
    const auto& s=motionPaths[size_t(path)];std::vector<char> chars(s.begin(),s.end());chars.push_back(0);return enumerate(capacity,count,out,chars);
}
XR_FN(m_xrCreateActionSet)(XrInstance i,const XrActionSetCreateInfo* info,XrActionSet* out){
    if(!good(i))return XR_ERROR_HANDLE_INVALID;if(!info||!out||info->type!=XR_TYPE_ACTION_SET_CREATE_INFO||info->next||!info->actionSetName[0]||!info->localizedActionSetName[0])return XR_ERROR_VALIDATION_FAILURE;
    for(auto* s:motionSets)if(s->name==info->actionSetName)return XR_ERROR_NAME_DUPLICATED;
    auto* s=new MotionSet{info->actionSetName};motionSets.insert(s);*out=reinterpret_cast<XrActionSet>(s);note("motion_set_created");return XR_SUCCESS;
}
XR_FN(m_xrDestroyActionSet)(XrActionSet set){
    auto* s=reinterpret_cast<MotionSet*>(set);if(!motionSets.count(s))return XR_ERROR_HANDLE_INVALID;
    for(auto* space:spaces)if(space->action){auto* a=reinterpret_cast<MotionAction*>(space->action);if(motionActions.count(a)&&a->owner==s)return XR_ERROR_CALL_ORDER_INVALID;}
    for(auto it=motionActions.begin();it!=motionActions.end();){auto* a=*it;if(a->owner!=s){++it;continue;}
        for(auto& p:motionBindings){auto& v=p.second;v.erase(std::remove_if(v.begin(),v.end(),[&](const auto& b){return b.action==reinterpret_cast<XrAction>(a);}),v.end());}
        it=motionActions.erase(it);delete a;}
    motionSets.erase(s);delete s;note("motion_set_destroyed");return XR_SUCCESS;
}
XR_FN(m_xrCreateAction)(XrActionSet set,const XrActionCreateInfo* info,XrAction* out){
    auto* s=reinterpret_cast<MotionSet*>(set);if(!motionSets.count(s))return XR_ERROR_HANDLE_INVALID;if(s->attached)return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    if(!info||!out||info->type!=XR_TYPE_ACTION_CREATE_INFO||info->next||info->countSubactionPaths!=2||!info->subactionPaths||!info->actionName[0]||!info->localizedActionName[0])return XR_ERROR_VALIDATION_FAILURE;
    if(info->actionType!=XR_ACTION_TYPE_POSE_INPUT&&info->actionType!=XR_ACTION_TYPE_BOOLEAN_INPUT&&info->actionType!=XR_ACTION_TYPE_FLOAT_INPUT&&info->actionType!=XR_ACTION_TYPE_VECTOR2F_INPUT&&info->actionType!=XR_ACTION_TYPE_VIBRATION_OUTPUT)return XR_ERROR_VALIDATION_FAILURE;
    if(motionHand(info->subactionPaths[0])!=0||motionHand(info->subactionPaths[1])!=1)return XR_ERROR_PATH_UNSUPPORTED;
    for(auto* a:motionActions)if(a->owner==s&&a->name==info->actionName)return XR_ERROR_NAME_DUPLICATED;
    auto* a=new MotionAction{s,info->actionName,info->actionType,{info->subactionPaths[0],info->subactionPaths[1]}};motionActions.insert(a);*out=reinterpret_cast<XrAction>(a);return XR_SUCCESS;
}
XR_FN(m_xrSuggestInteractionProfileBindings)(XrInstance i,const XrInteractionProfileSuggestedBinding* info){
    if(!good(i))return XR_ERROR_HANDLE_INVALID;if(!info||info->type!=XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING||info->next||!info->suggestedBindings||!info->countSuggestedBindings)return XR_ERROR_VALIDATION_FAILURE;
    if(motionAttached)return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    if(!info->interactionProfile||info->interactionProfile>=motionPaths.size())return XR_ERROR_PATH_UNSUPPORTED;
    const auto& profile=motionPaths[size_t(info->interactionProfile)];if(std::find(std::begin(motionProfiles),std::end(motionProfiles),profile)==std::end(motionProfiles))return XR_ERROR_PATH_UNSUPPORTED;
    const bool click=profile.find("htc/")!=std::string::npos||profile.find("microsoft/")!=std::string::npos;
    const std::string stick=profile.find("htc/")!=std::string::npos?"/input/trackpad":"/input/thumbstick";
    std::vector<XrActionSuggestedBinding> copy;
    for(unsigned k=0;k<info->countSuggestedBindings;k++){
        const auto& b=info->suggestedBindings[k];auto* a=reinterpret_cast<MotionAction*>(b.action);if(!motionActions.count(a))return XR_ERROR_HANDLE_INVALID;
        if(!b.binding||b.binding>=motionPaths.size())return XR_ERROR_PATH_UNSUPPORTED;const auto& name=motionPaths[size_t(b.binding)];auto pos=name.find('/',11);
        if(name.find("/user/hand/left/")!=0&&name.find("/user/hand/right/")!=0)return XR_ERROR_PATH_UNSUPPORTED;
        const auto suffix=name.substr(pos);
        const bool left=name.find("/user/hand/left/")==0;
        const bool faceSupported=profile==motionProfiles[0]||profile==motionProfiles[1];
        const bool touchLeft=profile==motionProfiles[0]&&left;
        const bool face=faceSupported&&(suffix==(touchLeft?"/input/x/click":"/input/a/click")||suffix==(touchLeft?"/input/y/click":"/input/b/click"));
        const bool stickTouch=a->type==XR_ACTION_TYPE_BOOLEAN_INPUT&&suffix==stick+"/touch"&&profile!=motionProfiles[3];
        bool valid=stickTouch||(a->type==XR_ACTION_TYPE_POSE_INPUT&&(suffix=="/input/grip/pose"||suffix=="/input/aim/pose"))||
            (a->type==XR_ACTION_TYPE_FLOAT_INPUT&&(suffix=="/input/trigger/value"||suffix==(click?"/input/squeeze/click":"/input/squeeze/value")||
                (profile==motionProfiles[1]&&(suffix=="/input/squeeze/force"||suffix=="/input/trackpad/force"))))||
            (a->type==XR_ACTION_TYPE_BOOLEAN_INPUT&&(suffix==(click?"/input/squeeze/click":"/input/squeeze/value")||suffix==stick+"/click"||face))||
            (a->type==XR_ACTION_TYPE_VECTOR2F_INPUT&&suffix==stick)||
            (a->type==XR_ACTION_TYPE_VIBRATION_OUTPUT&&suffix=="/output/haptic");if(!valid)return XR_ERROR_PATH_UNSUPPORTED;copy.push_back(b);
    }
    for(const auto& b:copy){auto* a=reinterpret_cast<MotionAction*>(b.action);note("motion_binding","\"profile\":"+jsonString(profile)+",\"action\":"+jsonString(a->name)+",\"path\":"+jsonString(motionPaths[size_t(b.binding)]));}
    motionBindings[info->interactionProfile]=copy;note("motion_bindings","\"profile\":"+jsonString(profile)+",\"count\":"+std::to_string(copy.size()));return XR_SUCCESS;
}
XR_FN(m_xrAttachSessionActionSets)(XrSession s,const XrSessionActionSetsAttachInfo* info){
    if(!good(s))return XR_ERROR_HANDLE_INVALID;if(motionAttached)return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    if(!info||info->type!=XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO||info->next||info->countActionSets!=1||!info->actionSets)return XR_ERROR_VALIDATION_FAILURE;
    auto* set=reinterpret_cast<MotionSet*>(info->actionSets[0]);if(!motionSets.count(set))return XR_ERROR_HANDLE_INVALID;
    set->attached=true;motionAttached=true;note("motion_set_attached");return XR_SUCCESS;
}
XR_FN(m_xrCreateActionSpace)(XrSession s,const XrActionSpaceCreateInfo* info,XrSpace* out){
    if(!good(s))return XR_ERROR_HANDLE_INVALID;if(!info||!out||info->type!=XR_TYPE_ACTION_SPACE_CREATE_INFO||info->next)return XR_ERROR_VALIDATION_FAILURE;
    auto* a=reinterpret_cast<MotionAction*>(info->action);if(!motionActions.count(a))return XR_ERROR_HANDLE_INVALID;
    if(a->type!=XR_ACTION_TYPE_POSE_INPUT)return XR_ERROR_ACTION_TYPE_MISMATCH;if(motionHand(info->subactionPath)<0)return XR_ERROR_PATH_UNSUPPORTED;
    auto* space=new Space{XR_REFERENCE_SPACE_TYPE_LOCAL,info->poseInActionSpace,info->action,info->subactionPath};spaces.insert(space);*out=reinterpret_cast<XrSpace>(space);return XR_SUCCESS;
}
XR_FN(m_xrSyncActions)(XrSession s,const XrActionsSyncInfo* info){
    if(!good(s))return XR_ERROR_HANDLE_INVALID;if(!info||info->type!=XR_TYPE_ACTIONS_SYNC_INFO||info->next||info->countActiveActionSets!=1||!info->activeActionSets)return XR_ERROR_VALIDATION_FAILURE;
    auto* set=reinterpret_cast<MotionSet*>(info->activeActionSets[0].actionSet);if(!motionSets.count(set))return XR_ERROR_HANDLE_INVALID;
    if(!motionAttached||!set->attached)return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    if(info->activeActionSets[0].subactionPath!=XR_NULL_PATH)return XR_ERROR_PATH_UNSUPPORTED;
    motionPrevious=motionCurrent;motionCurrent=motionInput();motionCurrent.focused=begun&&state==XR_SESSION_STATE_FOCUSED&&motionCurrent.focused;motionSynced=true;
    note("motion_sync","\"frame\":"+std::to_string(frame)+",\"focused\":"+(motionCurrent.focused?"true":"false")+",\"profile\":"+std::to_string(motionCurrent.profile));
    return motionCurrent.focused?XR_SUCCESS:XR_SESSION_NOT_FOCUSED;
}
XR_FN(m_xrGetCurrentInteractionProfile)(XrSession s,XrPath hand,XrInteractionProfileState* out){
    if(!good(s))return XR_ERROR_HANDLE_INVALID;int h=motionHand(hand);if(h<0)return XR_ERROR_PATH_UNSUPPORTED;
    if(!out||out->type!=XR_TYPE_INTERACTION_PROFILE_STATE||out->next)return XR_ERROR_VALIDATION_FAILURE;
    if(!motionAttached)return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    out->interactionProfile=motionCurrent.hands[size_t(h)].active?motionPath(motionProfiles[motionCurrent.profile]):XR_NULL_PATH;return XR_SUCCESS;
}
XR_FN(m_xrGetActionStatePose)(XrSession s,const XrActionStateGetInfo* info,XrActionStatePose* out){
    MotionAction* a;int h;auto r=motionState(s,info,XR_ACTION_TYPE_POSE_INPUT,a,h);if(XR_FAILED(r))return r;
    if(!out||out->type!=XR_TYPE_ACTION_STATE_POSE||out->next)return XR_ERROR_VALIDATION_FAILURE;
    out->isActive=motionActive(a,h);return XR_SUCCESS;
}
XR_FN(m_xrGetActionStateFloat)(XrSession s,const XrActionStateGetInfo* info,XrActionStateFloat* out){
    MotionAction* a;int h;auto r=motionState(s,info,XR_ACTION_TYPE_FLOAT_INPUT,a,h);if(XR_FAILED(r))return r;
    if(!out||out->type!=XR_TYPE_ACTION_STATE_FLOAT||out->next)return XR_ERROR_VALIDATION_FAILURE;
    // Grip float actions (value/click, Index force) report the CSV grip column as 0 or 1.
    const bool squeeze=a->name=="grab"||a->name=="grab_force";
    // The Index trackpad's force (pad_force): the CSV has no column for it - never pressed.
    auto value=[&](const MotionHand& v){return a->name=="pad_force"?0.f:squeeze?(v.grab?1.f:0.f):v.trigger;};
    out->isActive=motionActive(a,h);out->currentState=out->isActive?value(motionCurrent.hands[size_t(h)]):0;
    out->changedSinceLastSync=out->isActive&&value(motionCurrent.hands[size_t(h)])!=value(motionPrevious.hands[size_t(h)]);out->lastChangeTime=XrTime(frame)*11111111;return XR_SUCCESS;
}
XR_FN(m_xrGetActionStateBoolean)(XrSession s,const XrActionStateGetInfo* info,XrActionStateBoolean* out){
    MotionAction* a;int h;auto r=motionState(s,info,XR_ACTION_TYPE_BOOLEAN_INPUT,a,h);if(XR_FAILED(r))return r;
    if(!out||out->type!=XR_TYPE_ACTION_STATE_BOOLEAN||out->next)return XR_ERROR_VALIDATION_FAILURE;
    // stick_touch: the CSV has no touch column; a moved or clicked stick counts as the thumb on it.
    auto value=[&](const MotionHand& v){return a->name=="stick_click"?v.stickClick:a->name=="primary_face"?v.primaryFace:a->name=="secondary_face"?v.secondaryFace:
        a->name=="stick_touch"?(v.stickClick||v.stick.x!=0||v.stick.y!=0):v.grab;};
    const bool current=value(motionCurrent.hands[size_t(h)]),previous=value(motionPrevious.hands[size_t(h)]);
    out->isActive=motionActive(a,h);out->currentState=out->isActive&&current;
    out->changedSinceLastSync=out->isActive&&current!=previous;out->lastChangeTime=XrTime(frame)*11111111;return XR_SUCCESS;
}
XR_FN(m_xrGetActionStateVector2f)(XrSession s,const XrActionStateGetInfo* info,XrActionStateVector2f* out){
    MotionAction* a;int h;auto r=motionState(s,info,XR_ACTION_TYPE_VECTOR2F_INPUT,a,h);if(XR_FAILED(r))return r;
    if(!out||out->type!=XR_TYPE_ACTION_STATE_VECTOR2F||out->next)return XR_ERROR_VALIDATION_FAILURE;
    const auto current=motionCurrent.hands[size_t(h)].stick,previous=motionPrevious.hands[size_t(h)].stick;
    out->isActive=motionActive(a,h);out->currentState=out->isActive?current:XrVector2f{};
    out->changedSinceLastSync=out->isActive&&(current.x!=previous.x||current.y!=previous.y);out->lastChangeTime=XrTime(frame)*11111111;return XR_SUCCESS;
}
XR_FN(m_xrApplyHapticFeedback)(XrSession s,const XrHapticActionInfo* info,const XrHapticBaseHeader* effect){
    if(!info||info->type!=XR_TYPE_HAPTIC_ACTION_INFO||info->next||!effect||effect->type!=XR_TYPE_HAPTIC_VIBRATION||effect->next)return XR_ERROR_VALIDATION_FAILURE;
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.action=info->action;gi.subactionPath=info->subactionPath;MotionAction* a;int h;
    auto r=motionState(s,&gi,XR_ACTION_TYPE_VIBRATION_OUTPUT,a,h);if(XR_FAILED(r))return r;if(!motionCurrent.focused)return XR_SESSION_NOT_FOCUSED;
    const auto* v=reinterpret_cast<const XrHapticVibration*>(effect);if(v->duration<=0||!std::isfinite(v->amplitude)||v->amplitude<0||v->amplitude>1||!std::isfinite(v->frequency)||v->frequency<0)return XR_ERROR_VALIDATION_FAILURE;
    note("motion_haptic","\"frame\":"+std::to_string(frame)+",\"hand\":"+std::to_string(h)+",\"amplitude\":"+std::to_string(v->amplitude)+",\"duration\":"+std::to_string(v->duration));return XR_SUCCESS;
}
XR_FN(m_xrStopHapticFeedback)(XrSession s,const XrHapticActionInfo* info){
    if(!info||info->type!=XR_TYPE_HAPTIC_ACTION_INFO||info->next)return XR_ERROR_VALIDATION_FAILURE;
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.action=info->action;gi.subactionPath=info->subactionPath;MotionAction* a;int h;
    auto r=motionState(s,&gi,XR_ACTION_TYPE_VIBRATION_OUTPUT,a,h);if(XR_FAILED(r))return r;
    note("motion_haptic_stop","\"frame\":"+std::to_string(frame)+",\"hand\":"+std::to_string(h));return XR_SUCCESS;
}
#define EVR_MOCK_ACTION_MAP(M) M(xrStringToPath) M(xrPathToString) M(xrCreateActionSet) M(xrDestroyActionSet) M(xrCreateAction) \
 M(xrSuggestInteractionProfileBindings) M(xrAttachSessionActionSets) M(xrCreateActionSpace) M(xrSyncActions) \
 M(xrGetCurrentInteractionProfile) M(xrGetActionStatePose) M(xrGetActionStateFloat) M(xrGetActionStateBoolean) M(xrGetActionStateVector2f) \
 M(xrApplyHapticFeedback) M(xrStopHapticFeedback)
