#pragma once
// The pad styles' VR settings in the game's start menu (2026-10-25, user: "the third person's pitch lock switchable in the
// game and in the exe", "the view's default tilt, 0 .. 90, for playing lying down"; evening, after the headset: "the tilt as
// a slider, the D-pad's left and right a degree each - this screen does not use them; the view reset moves the slider to the
// head's angle now; RT switches TPS and FPS in the game; LT the view reset; the right stick's click the pitch lock; the left
// stick's click the bug report"). Without motion controllers there is no pointer for the mod's own buttons
// (bug_report.hpp), so a small panel of the mod's is drawn over the start menu (the layer's evrSetUiOverlay, on the right half
// where the start menu draws nothing; hidden as the bug report's button when the game's settings page covers it). The pad is
// read here by itself (XInputGetState, the game reads it as well; nothing is held back from the game). A change applies at
// once and is written into the settings file the loader read (EVR_SETTINGS_INI: [Mode] Style, [Pad] PitchLock / Tilt) when
// it has rested a second or the menu closes; a development start has none - the change holds for the session.
// The layer keeps one overlay picture: in a pad style the bug report draws nothing over the start menu (this panel shows
// its line) and only its recording line elsewhere.
// Third person (2026-10-25 evening, user: "TPS: the UI follows the head - head aim - but the character's shots do not; stick
// aim is fine for TPS"): the UI image stands ahead of the game camera's own direction (evrSetUiPlate, every eye pair: plate())
// instead of ahead of the head - its middle, the game's reticle, is where the right stick aims and the shots go.
// Included after bug_report.hpp (its text picture: bugReport::render, gameLanguage).
namespace padSettings {
using GetStateFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
using PlateFn=int(*)(int,const float*);
inline GetStateFn getState=nullptr;inline bugReport::OverlayFn setOverlay=nullptr;inline PlateFn setPlate=nullptr;
inline bool tried=false,ready=false,japanese=false,drawn=false,plateSet=false;
inline std::wstring ini;inline std::string drawnLook,savedLook;inline uint64_t changedAt=0;inline unsigned changes=0,plateLogs=0;
// LT, RT, R3, L3, D-pad left, D-pad right
enum Button {LT,RT,R3,L3,Left,Right,Buttons};
inline bool held[Buttons]{};inline uint64_t heldSince[Buttons]{},repeatAt[Buttons]{};
constexpr float u0=.50f,v0=.33f;constexpr uint32_t width=880,height=272;
constexpr LONG sliderX0=430,sliderX1=830;
// EVR_PAD_TEST_PRESSES (Mock trials): "<XR frame>:<LT|RT|R3|L3|LEFT|RIGHT>[*n]/..." - presses while the start menu is open.
struct TestPress {uint64_t frame=0;Button button=LT;int count=1;};
inline std::vector<TestPress> tests;inline size_t nextTest=0;
inline std::vector<TestPress> parseTests(const std::string& text){
    // "/" or ";" as well: the trial's -Environment splits at ",", and its launcher's command line at ";"
    std::string list=text;for(char& c:list)if(c==';'||c=='/')c=',';
    std::vector<TestPress> out;std::stringstream in(list);std::string item;
    while(std::getline(in,item,',')){const auto colon=item.find(':');if(colon==std::string::npos)continue;
        TestPress t;t.frame=std::strtoull(item.substr(0,colon).c_str(),nullptr,10);std::string name=item.substr(colon+1);
        if(const auto star=name.find('*');star!=std::string::npos){t.count=std::clamp(std::atoi(name.c_str()+star+1),1,90);name.resize(star);}
        static const std::pair<const char*,Button> names[]{{"LT",LT},{"RT",RT},{"R3",R3},{"L3",L3},{"LEFT",Left},{"RIGHT",Right}};
        bool known=false;for(const auto& n:names)if(name==n.first){t.button=n.second;known=true;}
        if(known&&t.frame)out.push_back(t);}
    return out;
}
inline void prepare(){
    tried=true;
    for(const wchar_t* name:{L"xinput1_4.dll",L"xinput9_1_0.dll",L"xinput1_3.dll"})if(const auto m=LoadLibraryW(name)){getState=reinterpret_cast<GetStateFn>(GetProcAddress(m,"XInputGetState"));if(getState)break;}
    if(const auto layer=GetModuleHandleW(L"evr_diagnostic.dll")){setOverlay=reinterpret_cast<bugReport::OverlayFn>(GetProcAddress(layer,"evrSetUiOverlay"));setPlate=reinterpret_cast<PlateFn>(GetProcAddress(layer,"evrSetUiPlate"));}
    {wchar_t path[MAX_PATH*2]{};const DWORD n=GetEnvironmentVariableW(L"EVR_SETTINGS_INI",path,DWORD(std::size(path)));if(n&&n<std::size(path))ini=path;}
    {char v[512]{};const auto n=GetEnvironmentVariableA("EVR_PAD_TEST_PRESSES",v,sizeof v);if(n&&n<sizeof v)tests=parseTests(v);}
    japanese=bugReport::gameLanguage()=="japanese";
    ready=getState&&setOverlay;
    savedLook=std::to_string(game::padStyle.load())+"/"+std::to_string(int(game::padPitchLock.load()))+"/"+std::to_string(game::padTiltDegrees.load());
    logFile->event("pad_settings_enabled","\"xinput\":"+std::string(getState?"true":"false")+",\"overlay\":"+(setOverlay?"true":"false")+",\"plate\":"+(setPlate?"true":"false")+
        ",\"settings_file\":"+(ini.empty()?std::string("false"):std::string("true"))+",\"japanese\":"+(japanese?"true":"false")+",\"test_presses\":"+std::to_string(tests.size()));
}
inline int reportPhase(){return bugReport::installed?bugReport::phase.load():-1;}
inline std::string settingsLook(){return std::to_string(game::padStyle.load())+"/"+std::to_string(int(game::padPitchLock.load()))+"/"+std::to_string(game::padTiltDegrees.load());}
inline std::string look(){return settingsLook()+"/"+std::to_string(reportPhase());}
inline void draw(){
    using bugReport::Block;
    const COLORREF gold=RGB(200,174,116),ink=RGB(230,222,206),dim=RGB(150,142,128),red=RGB(236,96,84),track=RGB(70,64,56);
    const bool third=game::padStyle.load()==2;const int tilt=game::padTiltDegrees.load();const bool lock=game::padPitchLock.load();const int report=reportPhase();
    std::vector<Block> b;
    const auto line=[&](LONG top,const std::wstring& text,COLORREF color,LONG right=LONG(width)-16){
        b.push_back({{16,top,right,top+36},text,22,FW_SEMIBOLD,color,0,0,0,0,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS,1});};
    b.push_back({{0,0,LONG(width),LONG(height)},L"",0,0,0,RGB(15,12,10),222,RGB(74,66,56),1,0,16});
    b.push_back({{16,8,LONG(width)-16,46},japanese?L"コントローラーVR":L"controller VR",26,FW_BOLD,gold,0,0,0,0,DT_LEFT|DT_VCENTER|DT_SINGLELINE,1});
    line(54,japanese?std::wstring(L"RT　視点：")+(third?L"三人称（TPS）　→ 一人称に切り替え":L"一人称（FPS）　→ 三人称に切り替え"):
                     std::wstring(L"RT   View: ")+(third?L"third person (TPS) - switch to first person":L"first person (FPS) - switch to third person"),ink);
    line(96,japanese?L"十字キー ←→　視点の傾き："+std::to_wstring(tilt)+L"°":L"D-pad left/right   View tilt: "+std::to_wstring(tilt)+L"°",ink,sliderX0-14);
    {   // the slider: 0 .. 90 degrees
        const LONG y=114,x=sliderX0+LONG(std::lround(double(sliderX1-sliderX0)*tilt/90.0));
        b.push_back({{sliderX0,y-4,sliderX1,y+4},L"",0,0,0,track,255,0,0,0,4});
        if(x>sliderX0+2)b.push_back({{sliderX0,y-4,x,y+4},L"",0,0,0,gold,255,0,0,0,4});
        b.push_back({{x-11,y-11,x+11,y+11},L"",0,0,0,gold,255,RGB(240,226,190),2,0,11});
    }
    line(138,japanese?L"LT　視点のリセット（今の頭の向きを正面に。傾きも合わせる）":L"LT   Reset the view (where your head looks now is ahead, tilt included)",ink);
    line(180,japanese?std::wstring(L"右スティック押し込み　上下の傾きの固定（三人称）：")+(lock?L"入":L"切"):std::wstring(L"R3   Pitch lock (third person): ")+(lock?L"On":L"Off"),third?ink:dim);
    std::wstring reportLine;COLORREF reportInk=ink;
    if(report==int(bugReport::Phase::Armed)){reportLine=japanese?L"VRバグ報告：メニューを閉じると 30 秒の記録が始まります":L"VR bug report: close the menu to start the 30-second recording";reportInk=red;}
    else if(report==int(bugReport::Phase::Recording)||report==int(bugReport::Phase::Making)){reportLine=japanese?L"VRバグ報告：記録中":L"VR bug report: recording";reportInk=red;}
    else if(report>=0)reportLine=japanese?L"左スティック押し込み　VRバグ報告（メニューを閉じると 30 秒記録）":L"L3   VR bug report (records 30 seconds after the menu closes)";
    else{reportLine=japanese?L"VRバグ報告：使えません":L"VR bug report: not available";reportInk=dim;}
    line(222,reportLine,reportInk);
    const auto rgba=bugReport::render(width,height,b);
    setOverlay(rgba.data(),width,height,u0,v0,u0+width/1920.f,v0+height/1080.f,2);drawn=true;drawnLook=look();
}
inline void save(){
    savedLook=settingsLook();changedAt=0;
    if(ini.empty())return;
    std::string text;{std::ifstream in(std::filesystem::path(ini),std::ios::binary);if(in)text.assign(std::istreambuf_iterator<char>(in),{});}
    using evr::settings::set;
    const auto out=set(set(set(text,"Mode","Style",game::padStyle.load()==2?"PadThird":"PadFirst"),"Pad","PitchLock",game::padPitchLock.load()?"1":"0"),"Pad","Tilt",std::to_string(game::padTiltDegrees.load()));
    const auto fresh=ini+L".new";
    {std::ofstream o(std::filesystem::path(fresh),std::ios::binary|std::ios::trunc);if(!o)return;o<<out;if(!o)return;}
    MoveFileExW(fresh.c_str(),ini.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
// First and third person in play (RT). Into first person the VR heading starts again from the game camera's heading now
// (rootCamera's stable heading, as at the world's start; the realignment of the game camera rests until it is taken), and
// the head steers the walk and the attacks again; into third person the game camera is shown and the head steers nothing.
inline void switchStyle(){
    const int to=game::padStyle.load()==1?2:1;
    if(to==1){{std::lock_guard<std::mutex> lock(rootCamera::headingMutex);rootCamera::headingReferenceEntity=0;}rootCamera::stableHeading.reset();}
    game::padStyle=to;inputHeadIdle::vrLocomotion=to==1;
    if(to==1&&plateSet&&setPlate){setPlate(0,nullptr);plateSet=false;}
}
inline void press(Button button,const char* by){
    const char* what=nullptr;
    switch(button){
    case RT:switchStyle();what="style";break;
    case LT:game::padTiltFromHead=true;++controllerUiFixture::recenterRequests;what="view_reset";break;
    case R3:game::padPitchLock=!game::padPitchLock.load();what="pitch_lock";break;
    case L3:if(bugReport::installed){std::lock_guard<std::mutex> lock(bugReport::mutex);bugReport::pressHit(bugReport::Hit::BugReport,by);what="bug_report";}break;
    case Left:case Right:{const int t=game::padTiltDegrees.load();const int n=std::clamp(t+(button==Right?1:-1),evr::settings::padTiltLow,evr::settings::padTiltHigh);
        if(n!=t){game::padTiltDegrees=n;what="tilt";}break;}
    default:break;}
    if(what&&changes++<400)logFile->event("pad_settings_change","\"what\":\""+std::string(what)+"\",\"by\":\""+by+"\",\"style\":\""+(game::padStyle.load()==2?"third":"first")+
        "\",\"pitch_lock\":"+(game::padPitchLock.load()?"true":"false")+",\"tilt\":"+std::to_string(game::padTiltDegrees.load())+",\"frame\":"+std::to_string(liveFrame.frame));
}
// Once per eye pair of a pad style (view_resource_probe.cpp, before bugReport::frame); screen: the game's (2 the start menu).
inline void frame(uint8_t screen) noexcept {
    try{
        if(!game::padStyle.load())return;
        if(!tried)prepare();
        if(!ready)return;
        const auto now=GetTickCount64();
        // the settings file: once a change has rested a second (the slider's steps), or when the menu closes
        if(settingsLook()!=savedLook){if(!changedAt)changedAt=now;if(screen!=2||now-changedAt>=1000)save();}else changedAt=0;
        if(screen!=2){if(drawn){setOverlay(nullptr,0,0,0,0,0,0,0);drawn=false;}for(bool& h:held)h=true;return;}   // a press held into the menu does not count
        while(nextTest<tests.size()&&liveFrame.frame>=tests[nextTest].frame){for(int i=0;i<tests[nextTest].count;++i)press(tests[nextTest].button,"test");++nextTest;}
        XINPUT_STATE state{};bool connected=false;for(DWORD port=0;port<4&&!connected;++port)connected=getState(port,&state)==ERROR_SUCCESS;
        const auto button=[&](WORD b){return connected&&(state.Gamepad.wButtons&b)!=0;};
        const bool down[Buttons]{connected&&state.Gamepad.bLeftTrigger>160,connected&&state.Gamepad.bRightTrigger>160,button(XINPUT_GAMEPAD_RIGHT_THUMB),button(XINPUT_GAMEPAD_LEFT_THUMB),
            button(XINPUT_GAMEPAD_DPAD_LEFT),button(XINPUT_GAMEPAD_DPAD_RIGHT)};
        for(int i=0;i<Buttons;++i){
            if(down[i]&&!held[i]){heldSince[i]=now;repeatAt[i]=now+400;press(Button(i),"pad");}
            else if(down[i]&&(i==Left||i==Right)&&now>=repeatAt[i]){repeatAt[i]=now+50;press(Button(i),"pad");}   // held: on after 0.4 s, 20 a second
            held[i]=down[i];
        }
        if(!drawn||drawnLook!=look())draw();
        else setOverlay(nullptr,0,0,u0,v0,u0+width/1920.f,v0+height/1080.f,2);   // kept shown (the layer drops a place not renewed for a second)
    }catch(const std::exception& e){static unsigned logs=0;if(logs++<5)try{logFile->event("pad_settings_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
    catch(...){}
}
// Third person: the UI image ahead of the game camera (`camera`: its pose in the tracking space, each eye pair).
inline void plate(const Pose& camera) noexcept {
    try{
        if(game::padStyle.load()!=2||!setPlate)return;
        if(!finite(camera.position)||!std::isfinite(camera.orientation.w))return;
        const auto q=normalized(camera.orientation);
        const float pose[7]{q.x,q.y,q.z,q.w,camera.position.x,camera.position.y,camera.position.z};
        // 2 while playing: the plate stays ahead of the camera and the layer still takes the locked enemy's bar out of it to the
        // enemy (as with the plate ahead of the head); 1 in a menu (as the motion VR's fixed menu plate - nothing taken out)
        const int r=setPlate(controllerUiFixture::liveScreenGameplay.load()?2:1,pose);if(!r)plateSet=true;
        if(plateLogs<3||(r&&plateLogs<20)){++plateLogs;std::ostringstream o;o<<std::setprecision(6)<<"\"result\":"<<r<<",\"orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"],\"position\":["
            <<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<"],\"frame\":"<<liveFrame.frame;logFile->event("pad_third_person_plate",o.str());}
    }catch(...){}
}
}
