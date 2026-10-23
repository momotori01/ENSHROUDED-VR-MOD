// The VR bug report (2026-10-15 evening, user: "for the flood of bugs the 1.0 update will bring: a red '! VR bug report !'
// button in the start menu; pressed during play, it records, from the moment the start menu closes, 30 seconds of a
// detailed log and pictures of both eyes, and the zip of the settings program's logs goes with it, into a zip in the
// folder of enshrouded.exe that the players send with a short description"). The texts, the capture plan and the rows of
// the Mock scripts are include/evr/bug_report.hpp.
//   - The button and its note are a picture of the mod's own (GDI, in the player's language: Steam's game language) laid
//     over the right half of the start menu's UI image (layer evrSetUiOverlay), only where the game draws nothing (the
//     settings page that opens on the same screen hides it). The menu pointer's press on it comes here and is not given
//     to the game (menu_pointer.hpp overlayButtonAt / overlayButtonPress).
//   - Pressed: armed. When the menu is closed (the HUD again, screen 1), the 30 seconds begin: the log's per-name budget
//     is opened again (every kind of record is written afresh), the state goes on record twice a second (bug_report_state),
//     every hand and head sample is written as the Mock's hand and head scripts (bug-report-*/hands.csv, head.csv: the
//     player's movements can be played again in a Mock session), and pictures of both eyes are asked for, half size
//     (evrRequestBugCapture; 22 pairs, a run of 6 early and 4 late). A small red line on the plate counts the seconds.
//   - Then report.json, and "Enshrouded VR setting.exe --bug-report <folder> <start> <end>" makes the zip: the settings
//     program's own log zip contents, the folder, and the pictures as JPEG pairs (settings/vr_setting_app.cpp).
//   - A crash in the 30 seconds (user 2026-10-15): report.json is written at the window's start ("status": "recording")
//     and again every second with the hand and head rows, and the settings program waits beside the game
//     (--watch-bug-report): when the game goes first, it makes the zip at once, with the game's crash dump. Its
//     "Make zip" finishes a report that even that missed.
// Nothing of the game is written; the press is never posted. EVR_BUG_REPORT=0 leaves it out.
// The VR settings (2026-10-18; include/evr/vr_settings_panel.hpp): a second button beside it on the same picture. Pressed,
// the note's place shows real crouching's on / off and its threshold; a press there changes the live values at once
// (controllerUiFixture::roomSneakOn, crouchDrop) and writes them into the settings file the loader read (EVR_SETTINGS_INI;
// a development start has none: the change holds for the session only). EVR_VR_SETTINGS_PRESSES (Mock trials):
// "frame:name;..." presses while the start menu is open.
namespace bugReport {
using OverlayFn=int(*)(const uint8_t*,uint32_t,uint32_t,float,float,float,float,int);
using ShownFn=int(*)();using CaptureFn=int(*)(uint32_t);using HalfFn=int(*)(int);
inline OverlayFn setOverlay=nullptr;inline ShownFn overlayShown=nullptr;inline CaptureFn requestCapture=nullptr;inline HalfFn captureHalf=nullptr;
// The last seconds of both eyes (layer evrSaveEyeHistory, 2026-10-18; the user: "when I see it I open the menu and press
// the button - keep the 10 seconds before"): written when the button is pressed and when the window ends; the zip is made
// once the second is written (or 5 s later).
using SaveHistoryFn=int(*)();using HistorySavedFn=uint64_t(*)();inline SaveHistoryFn saveHistory=nullptr;inline HistorySavedFn historySaved=nullptr;
inline uint64_t makerWaitSaves=0,makerWaitUntil=0;
// A newer mod is out (layer evrModUpdateState, layer/mod_update_check.hpp; user 2026-10-19: "where the bug report is in
// the left stick's menu too"): "MOD Update" in white beside the buttons, blinking slowly (include/evr/mod_update.hpp).
using UpdateStateFn=int(*)();inline UpdateStateFn updateState=nullptr;
inline bool noticeOn(){return updateState&&evr::modupdate::noticeShown(updateState(),GetTickCount64());}
constexpr RECT noticeBox{1000,18,1200,66};   // right of the settings' button (980), within the button row (84)
inline bool installed=false;
enum class Phase {Idle,Armed,Recording,Making,Done};
inline std::atomic<int> phase{int(Phase::Idle)};
inline std::mutex mutex;
inline std::string language;inline evr::bugreport::Texts texts;
// The panel over the start menu: its place on the UI image and its picture's size (the picture is drawn at about
// the UI's pixel size of 1920 x 1080); the button is its top part.
constexpr float panelU0=.30f,panelV0=.47f;constexpr uint32_t panelW=1200,panelH=570,buttonW=640,buttonH=84;   // wide: the note's lines as written, none wrapped
constexpr float panelU1=panelU0+panelW/1920.f,panelV1=panelV0+panelH/1080.f,buttonU1=panelU0+buttonW/1920.f,buttonV1=panelV0+buttonH/1080.f;
// The line on the plate while recording / making / done.
constexpr float lineU0=.015f,lineV0=.012f;constexpr uint32_t lineW=760,lineH=52;
constexpr float lineU1=lineU0+lineW/1920.f,lineV1=lineV0+lineH/1080.f;
inline int drawnKind=-1;inline int drawnSeconds=-1;inline uint64_t drawnLook=~0ull;
// The VR settings: what the pointer is on (set by the pointer's thread), whether the settings are open, the settings file.
using Hit=evr::vrpanel::Hit;
inline std::atomic<int> hoverHit{int(Hit::None)},hoverX{-1};inline std::atomic<bool> settingsOpen{false};
// A slider (the crouch depth, user 2026-10-18: "set freely with a slider"; the stroke's speed, 2026-10-20): the one held
// from its press until the trigger is let go (Hit::None: none).
inline std::atomic<int> sliderHeld{int(Hit::None)};
// The recommended graphics asked from this panel in this session (its button then says so; done at the next start).
inline std::atomic<bool> recommendAsked{false};
// The alternate eyes' pacing (2026-10-23, include/evr/afw_pacing.hpp; the layer's evrSetAfwPacing): the value shown, whether
// the alternate eyes are on (without them the row is dim - a press still writes the value for their next start), and "fill"
// chosen in a session not started with it (it waits for the next start; the note says so).
inline std::atomic<int> afwPacing{0};inline bool afwOn=false;inline std::atomic<bool> pacingWaits{false};
using PacingFn=int(*)(int);inline PacingFn setAfwPacing=nullptr;
inline evr::vrpanel::Texts panelTexts;inline std::wstring settingsIni;inline std::vector<evr::vrpanel::TestPress> testPresses;inline size_t nextTestPress=0;
static_assert(panelW==uint32_t(evr::vrpanel::width)&&panelH==uint32_t(evr::vrpanel::height)&&buttonW==uint32_t(evr::vrpanel::bugButton.x1)&&buttonH==uint32_t(evr::vrpanel::buttonHeight));
inline std::vector<uint8_t> panel,line;
// The window.
inline uint64_t armedAt=0,startMs=0,startFrame=0,doneAt=0;inline int64_t startUnix=0,endUnix=0;
// frames.csv (2026-10-23): each frame's XR number, predicted display time (ns), a precise clock (ms) and the eyes drawn
// (3 both, 1 or 2 one eye a frame) - the head script's time is GetTickCount's, 15.6 ms steps, too coarse to tell how many
// refreshes of the headset each frame took (the alternate-eye warp's "shakes while walking" against the frame rate).
inline std::filesystem::path folder;inline std::ofstream handsCsv,headCsv,framesCsv;inline uint64_t rows=0,lastState=0;inline size_t nextStep=0;
inline std::vector<std::string> captureResults;inline uint8_t lastScreen=255;
inline HANDLE maker=nullptr;inline std::wstring zipName;inline unsigned long makerExit=0;

inline std::wstring wideOf(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
inline std::string narrowOf(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
// Steam's game language, or the system's for the common ones.
inline std::string gameLanguage(){
    if(const auto steam=GetModuleHandleW(L"steam_api64.dll")){
        using AppsFn=void*(*)();using LanguageFn=const char*(*)(void*);
        auto apps=reinterpret_cast<AppsFn>(GetProcAddress(steam,"SteamAPI_SteamApps_v009"));if(!apps)apps=reinterpret_cast<AppsFn>(GetProcAddress(steam,"SteamAPI_SteamApps_v008"));/* 1.0: v009 */const auto current=reinterpret_cast<LanguageFn>(GetProcAddress(steam,"SteamAPI_ISteamApps_GetCurrentGameLanguage"));
        if(apps&&current)try{if(void* a=apps())if(const char* l=current(a);l&&*l)return l;}catch(...){}
    }
    switch(PRIMARYLANGID(GetUserDefaultUILanguage())){
        case LANG_JAPANESE:return "japanese";case LANG_GERMAN:return "german";case LANG_FRENCH:return "french";case LANG_SPANISH:return "spanish";case LANG_ITALIAN:return "italian";
        case LANG_POLISH:return "polish";case LANG_PORTUGUESE:return "brazilian";case LANG_RUSSIAN:return "russian";case LANG_UKRAINIAN:return "ukrainian";case LANG_KOREAN:return "koreana";
        case LANG_TURKISH:return "turkish";case LANG_CHINESE:return SUBLANGID(GetUserDefaultUILanguage())==SUBLANG_CHINESE_TRADITIONAL?"tchinese":"schinese";default:return "english";
    }
}
// A picture of text blocks (GDI, grey-scale antialiased): RGBA8, display values, straight alpha.
// radius: rounded corners (2026-10-20, the VR settings' pills and the slider's knob), edges smoothed; 0 square.
struct Block {RECT box;std::wstring text;int pixels;int weight;COLORREF ink;COLORREF fill;uint8_t fillAlpha;COLORREF frame;int frameWidth;UINT format;int radius=0;};
inline std::vector<uint8_t> render(uint32_t w,uint32_t h,const std::vector<Block>& blocks){
    std::vector<uint8_t> out(size_t(w)*h*4,0);
    const HDC dc=CreateCompatibleDC(nullptr);if(!dc)return out;
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof bi.bmiHeader;bi.bmiHeader.biWidth=LONG(w);bi.bmiHeader.biHeight=-LONG(h);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;const HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bmp||!bits){DeleteDC(dc);return out;}
    const auto old=SelectObject(dc,bmp);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    auto put=[&](uint32_t x,uint32_t y,COLORREF c,float a){   // c over the pixel, straight alpha
        if(x>=w||y>=h||a<=0)return;auto* p=&out[(size_t(y)*w+x)*4];const float da=p[3]/255.f,oa=a+da*(1-a);if(oa<=0)return;
        const float r=(GetRValue(c)*a+p[0]*da*(1-a))/oa,g=(GetGValue(c)*a+p[1]*da*(1-a))/oa,b=(GetBValue(c)*a+p[2]*da*(1-a))/oa;
        p[0]=uint8_t(std::min(255.f,r+.5f));p[1]=uint8_t(std::min(255.f,g+.5f));p[2]=uint8_t(std::min(255.f,b+.5f));p[3]=uint8_t(std::min(255.f,oa*255.f+.5f));};
    for(const auto& b:blocks){
        if(b.radius>0){   // how far a pixel's middle is outside the rounded box (negative inside), as the coverage's edge
            const float r=float(std::min<LONG>({LONG(b.radius),(b.box.right-b.box.left)/2,(b.box.bottom-b.box.top)/2}));
            for(LONG y=std::max(0L,b.box.top);y<std::min(LONG(h),b.box.bottom);++y)for(LONG x=std::max(0L,b.box.left);x<std::min(LONG(w),b.box.right);++x){
                const float px=float(x)+.5f,py=float(y)+.5f;
                const float cx=std::clamp(px,float(b.box.left)+r,float(b.box.right)-r),cy=std::clamp(py,float(b.box.top)+r,float(b.box.bottom)-r);
                const float d=std::hypot(px-cx,py-cy)-r;
                const float outer=std::clamp(.5f-d,0.f,1.f),inner=b.frameWidth>0?std::clamp(.5f-(d+float(b.frameWidth)),0.f,1.f):outer;
                if(b.fillAlpha&&inner>0)put(uint32_t(x),uint32_t(y),b.fill,b.fillAlpha/255.f*inner);
                if(b.frameWidth>0&&outer>inner)put(uint32_t(x),uint32_t(y),b.frame,outer-inner);}
        }
        else for(LONG y=std::max(0L,b.box.top);y<std::min(LONG(h),b.box.bottom);++y)for(LONG x=std::max(0L,b.box.left);x<std::min(LONG(w),b.box.right);++x){
            const bool edge=b.frameWidth>0&&(x<b.box.left+b.frameWidth||x>=b.box.right-b.frameWidth||y<b.box.top+b.frameWidth||y>=b.box.bottom-b.frameWidth);
            if(edge)put(uint32_t(x),uint32_t(y),b.frame,1.f);else if(b.fillAlpha)put(uint32_t(x),uint32_t(y),b.fill,b.fillAlpha/255.f);}
        if(b.text.empty())continue;
        std::memset(bits,0,size_t(w)*h*4);
        const HFONT font=CreateFontW(-b.pixels,0,0,0,b.weight,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,texts.font);
        const auto oldFont=SelectObject(dc,font);RECT r=b.box;InflateRect(&r,-(b.frameWidth+(b.radius>0?4:10)),-(b.frameWidth+(b.radius>0?0:6)));
        DrawTextW(dc,b.text.c_str(),int(b.text.size()),&r,b.format|DT_NOPREFIX);GdiFlush();
        SelectObject(dc,oldFont);DeleteObject(font);
        const auto* px=static_cast<const uint8_t*>(bits);
        for(LONG y=std::max(0L,b.box.top);y<std::min(LONG(h),b.box.bottom);++y)for(LONG x=std::max(0L,b.box.left);x<std::min(LONG(w),b.box.right);++x){
            const auto* q=px+(size_t(y)*w+size_t(x))*4;const float cover=std::max({q[0],q[1],q[2]})/255.f;if(cover>0)put(uint32_t(x),uint32_t(y),b.ink,cover);}
    }
    SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);return out;
}
// What the panel shows, as one number: a change of any part draws it again.
inline uint64_t look(int kind){
    return uint64_t(kind&3)|uint64_t(hoverHit.load()&15)<<2|uint64_t(settingsOpen.load()?1:0)<<6|uint64_t(controllerUiFixture::roomSneakOn.load()?1:0)<<7|uint64_t(controllerUiFixture::crouchDrop.load()&255)<<8|
        uint64_t(sliderHeld.load()&15)<<16|uint64_t(noticeOn()?1:0)<<21|uint64_t(controllerUiFixture::swingPercent.load()&255)<<22|uint64_t(controllerUiFixture::armSwimOn.load()?1:0)<<30|uint64_t(controllerUiFixture::hitStopOn.load()?1:0)<<31|uint64_t(recommendAsked.load()?1:0)<<32|
        uint64_t(afwPacing.load()&3)<<33|uint64_t(pacingWaits.load()?1:0)<<35;
}
// The VR settings' values now.
inline evr::vrpanel::Body currentBody(){
    return {controllerUiFixture::roomSneakOn.load(),controllerUiFixture::crouchDrop.load(),controllerUiFixture::swingPercent.load(),controllerUiFixture::armSwimOn.load(),controllerUiFixture::hitStopOn.load()};
}
inline RECT rectOf(const evr::vrpanel::Box& b){return {LONG(b.x0),LONG(b.y0),LONG(b.x1),LONG(b.y1)};}
// kind 0 the button with its note, 1 armed; the VR settings' button beside it, and its rows instead of the note while open
inline void drawPanel(int kind){
    const COLORREF red=RGB(168,64,52),dark=RGB(14,11,10);   // a dull, quiet red (user 2026-10-15: "too loud for the game's mood")
    const COLORREF gold=RGB(200,174,116);   // the VR settings' accent (2026-10-20), a muted brass
    const auto hit=Hit(hoverHit.load());const bool open=settingsOpen.load();
    std::vector<Block> b;
    const bool lit=hit==Hit::BugReport;
    b.push_back({{0,0,LONG(buttonW),LONG(buttonH)},texts.title,46,FW_BOLD,lit?RGB(228,200,184):red,lit?RGB(92,32,26):dark,uint8_t(lit?235:215),red,4,DT_CENTER|DT_VCENTER|DT_SINGLELINE});
    {const bool on=hit==Hit::Settings||open;   // the settings' button: a small quiet pill (2026-10-20), lit while pointed at or open
     b.push_back({rectOf(evr::vrpanel::settingsButton),panelTexts.button,24,FW_SEMIBOLD,on?RGB(246,238,220):RGB(206,196,176),on?RGB(52,46,38):RGB(18,15,13),uint8_t(on?235:205),
         on?gold:RGB(112,102,88),hit==Hit::Settings?2:1,DT_CENTER|DT_VCENTER|DT_SINGLELINE,22});}
    if(noticeOn())b.push_back({noticeBox,evr::modupdate::wideNotice,28,FW_SEMIBOLD,RGB(245,245,245),RGB(18,18,18),200,0,0,DT_CENTER|DT_VCENTER|DT_SINGLELINE});
    if(open){
        using namespace evr::vrpanel;
        const auto body=currentBody();const int held=sliderHeld.load();
        // the sheet: dark, a thin line round it, rounded
        b.push_back({{0,LONG(sheetTop),LONG(panelW),LONG(panelH)},L"",0,0,0,RGB(15,12,10),222,RGB(74,66,56),1,0,16});
        // a row's label: its heading (up to the line break) and, small under it, its explanation; a thin line under the row
        auto label=[&](int row,const std::wstring& text,bool dim){const auto cut=text.find(L'\n');const LONG y=LONG(rowY(row));
            b.push_back({{LONG(labelX0),y+2,LONG(labelX1),y+30},text.substr(0,cut),23,FW_SEMIBOLD,dim?RGB(118,112,102):RGB(230,222,206),0,0,0,0,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS,1});
            if(cut!=std::wstring::npos)b.push_back({{LONG(labelX0),y+30,LONG(labelX1),y+LONG(rowHeight)},text.substr(cut+1),17,FW_NORMAL,dim?RGB(100,95,88):RGB(150,142,128),0,0,0,0,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS,1});
            if(row<rows-1)b.push_back({{LONG(labelX0),y+LONG(rowStep)-5,LONG(panelW)-30,y+LONG(rowStep)-4},L"",0,0,0,RGB(50,45,39),255,0,0,0});};
        // on / off: two small pills, the chosen one filled
        auto pill=[&](const Box& box,const wchar_t* text,bool selected){const bool pointed=hit==box.hit;
            b.push_back({rectOf(box),text,19,selected?FW_BOLD:FW_SEMIBOLD,selected?RGB(26,21,16):pointed?RGB(246,238,220):RGB(168,160,146),selected?gold:pointed?RGB(52,46,38):RGB(26,22,19),uint8_t(selected?240:215),
                pointed?RGB(246,238,220):selected?gold:RGB(86,78,68),pointed?2:1,DT_CENTER|DT_VCENTER|DT_SINGLELINE,17});};
        // a slider: a thin track, the part up to the value, a round knob (bigger while pointed at or held)
        auto slider=[&](const Box& box,int knobX,bool on){const LONG mid=LONG((box.y0+box.y1)/2),knob=LONG(knobX);const bool pointed=hit==box.hit||held==int(box.hit);
            b.push_back({{LONG(trackStart(box)),mid-2,LONG(trackEnd(box)),mid+2},L"",0,0,0,RGB(64,58,50),255,0,0,0,2});
            b.push_back({{LONG(trackStart(box)),mid-2,knob,mid+2},L"",0,0,0,on?gold:RGB(104,98,90),255,0,0,0,2});
            const LONG r=pointed?13:10;
            b.push_back({{knob-r,mid-r,knob+r,mid+r},L"",0,0,0,pointed?RGB(250,244,230):RGB(226,218,202),255,pointed?gold:RGB(40,35,30),pointed?3:1,0,int(r)});};
        wchar_t depth[256]{};std::swprintf(depth,256,panelTexts.drop,body.crouchDrop);
        wchar_t swing[256]{};std::swprintf(swing,256,panelTexts.swing,evr::settings::clampSwingSpeed(body.swingSpeed));
        label(0,panelTexts.crouch,false);pill(crouchOn,panelTexts.on,body.realCrouch);pill(crouchOff,panelTexts.off,!body.realCrouch);
        label(1,depth,!body.realCrouch);slider(dropSlider,sliderX(body.crouchDrop),body.realCrouch);
        label(2,swing,false);slider(swingSlider,swingX(body.swingSpeed),true);
        label(3,panelTexts.swim,false);pill(swimOn,panelTexts.on,body.armSwim);pill(swimOff,panelTexts.off,!body.armSwim);
        label(4,panelTexts.hitStop,false);pill(hitStopOn,panelTexts.on,body.hitStop);pill(hitStopOff,panelTexts.off,!body.hitStop);
        label(5,panelTexts.recommend,false);pill(recommendButton,recommendAsked.load()?panelTexts.recommendDone:panelTexts.recommendSet,recommendAsked.load());
        {const int pacing=afwPacing.load();label(6,panelTexts.pacing,!afwOn);
         pill(pacingMax,panelTexts.pacingMax,pacing==0);pill(pacingHalf,panelTexts.pacingHalf,pacing==1);pill(pacingFill,panelTexts.pacingFill,pacing==2);}
        std::wstring note=panelTexts.note;if(settingsIni.empty())note+=std::wstring(L"\n")+panelTexts.unsaved;
        if(pacingWaits.load())note+=std::wstring(L"\n")+panelTexts.pacingNext;
        b.push_back({{LONG(labelX0),LONG(noteTop),LONG(panelW)-30,LONG(panelH)-6},note,17,FW_NORMAL,RGB(140,133,120),0,0,0,0,DT_LEFT|DT_WORDBREAK|DT_EDITCONTROL});
    }
    else if(kind==0)b.push_back({{0,LONG(buttonH)+10,LONG(panelW),450},texts.description,26,FW_NORMAL,RGB(214,206,192),dark,190,0,0,DT_LEFT|DT_WORDBREAK|DT_EDITCONTROL});
    else b.push_back({{0,LONG(buttonH)+10,LONG(panelW),LONG(buttonH)+10+120},texts.armed,28,FW_SEMIBOLD,RGB(150,186,132),dark,200,0,0,DT_LEFT|DT_WORDBREAK|DT_EDITCONTROL});
    panel=render(panelW,panelH,b);drawnKind=kind;drawnLook=look(kind);
}
inline void drawLine(const std::wstring& text){
    line=render(lineW,lineH,{{{0,0,LONG(lineW),LONG(lineH)},text,28,FW_SEMIBOLD,RGB(184,78,64),RGB(12,9,8),200,RGB(150,58,48),2,DT_LEFT|DT_VCENTER|DT_SINGLELINE}});
}
inline bool buttonAt(float u,float v,uint8_t screen){
    const int p=phase.load();
    const bool shown=screen==2&&(p==int(Phase::Idle)||p==int(Phase::Armed)||p==int(Phase::Done))&&overlayShown&&overlayShown();
    // the panel's own pixels (it is drawn at the UI's pixel size: 1920 x 1080 of the UI image)
    const int x=int(std::floor((u-panelU0)*1920.f));
    const auto hit=shown?evr::vrpanel::hitAt(x,int(std::floor((v-panelV0)*1080.f)),settingsOpen.load()):Hit::None;
    hoverHit=int(hit);hoverX=x;return hit!=Hit::None;
}
// The VR settings' values into the settings file (read again first: only their own lines change).
inline bool saveBody(const evr::vrpanel::Body& body,std::string& why){
    if(settingsIni.empty()){why="no settings file (a development start)";return false;}
    std::string text;{std::ifstream in(std::filesystem::path(settingsIni),std::ios::binary);if(in)text.assign(std::istreambuf_iterator<char>(in),{});}
    const auto out=evr::vrpanel::store(text,body);const auto fresh=settingsIni+L".new";
    {std::ofstream o(std::filesystem::path(fresh),std::ios::binary|std::ios::trunc);if(!o){why="could not write";return false;}o<<out;if(!o){why="could not write";return false;}}
    if(!MoveFileExW(fresh.c_str(),settingsIni.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){why="could not replace ("+std::to_string(GetLastError())+")";return false;}
    return true;
}
// The values now (live at once), into the settings file when `save`, and on record (a slider's drag: up to 300).
inline void changeBody(const evr::vrpanel::Body& before,const evr::vrpanel::Body& after,const char* press,const char* by,bool save){
    controllerUiFixture::roomSneakOn=after.realCrouch;controllerUiFixture::crouchDrop=after.crouchDrop;
    controllerUiFixture::swingPercent=evr::settings::clampSwingSpeed(after.swingSpeed);controllerUiFixture::armSwimOn=after.armSwim;controllerUiFixture::hitStopOn=after.hitStop;
    std::string why;const bool saved=save&&saveBody(after,why);
    static unsigned drags=0;if(!save&&drags++>=300)return;
    logFile->event("vr_settings_changed","\"press\":\""+std::string(press)+"\",\"by\":\""+by+"\",\"real_crouch\":"+(after.realCrouch?"true":"false")+",\"crouch_drop_percent\":"+std::to_string(after.crouchDrop)+
        ",\"before_percent\":"+std::to_string(before.crouchDrop)+",\"swing_speed_percent\":"+std::to_string(after.swingSpeed)+",\"swing_before_percent\":"+std::to_string(before.swingSpeed)+",\"arm_swim\":"+(after.armSwim?"true":"false")+",\"hit_stop\":"+(after.hitStop?"true":"false")+",\"enter_ratio\":"+std::to_string(evr::vrpanel::enterRatio(after.crouchDrop))+",\"saved\":"+(saved?"true":"false")+(why.empty()?"":",\"not_saved\":"+jsonString(why))+
        ",\"frame\":"+std::to_string(liveFrame.frame));
}
// A press on the panel (the mutex held). testValue: the slider's value of a Mock's press (-1 from the pointer).
inline void pressHit(Hit hit,const char* by,int testValue=-1){
    if(hit==Hit::BugReport){
        const int p=phase.load();if(p!=int(Phase::Idle)&&p!=int(Phase::Done))return;
        phase=int(Phase::Armed);armedAt=GetTickCount64();settingsOpen=false;
        if(saveHistory)saveHistory();   // the 10 seconds before the press
        logFile->event("bug_report_armed","\"frame\":"+std::to_string(liveFrame.frame)+",\"language\":"+jsonString(language)+",\"history_asked\":"+(saveHistory?"true":"false"));
        return;
    }
    if(hit==Hit::Settings){settingsOpen=!settingsOpen.load();logFile->event("vr_settings_panel","\"open\":"+std::string(settingsOpen.load()?"true":"false")+",\"by\":\""+by+"\",\"frame\":"+std::to_string(liveFrame.frame));return;}
    if(hit==Hit::None||!settingsOpen.load())return;
    // The recommended graphics (include/evr/recommended_graphics.hpp): the mod's [Picture] values now, the game's own
    // settings by the loader at the next start (the game holds them in memory and writes them back when it closes); the
    // card's video memory goes with the request (the loader runs in DllMain, where DXGI must not be asked).
    if(hit==Hit::Recommend){
        const auto machine=evr::recommend::detectMachine();const auto r=evr::recommend::recommend(machine);
        std::string why="no settings file (a development start)";bool saved=false;std::vector<std::string> changed;
        if(!settingsIni.empty()){std::string text;{std::ifstream in(std::filesystem::path(settingsIni),std::ios::binary);if(in)text.assign(std::istreambuf_iterator<char>(in),{});}
            const auto out=evr::recommend::setPending(evr::recommend::applyIni(text,r,&changed),true,machine);const auto fresh=settingsIni+L".new";
            {std::ofstream o(std::filesystem::path(fresh),std::ios::binary|std::ios::trunc);if(o)o<<out;saved=bool(o);}
            saved=saved&&MoveFileExW(fresh.c_str(),settingsIni.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);if(!saved)why="could not write";}
        if(saved)recommendAsked=true;
        std::string list;for(const auto& c:changed)list+=std::string(list.empty()?"":",")+jsonString(c);
        logFile->event("vr_settings_recommended","\"by\":\""+std::string(by)+"\",\"saved\":"+(saved?"true":"false")+(saved?std::string():",\"why\":"+jsonString(why))+",\"gpu\":"+jsonString(machine.gpu)+
            ",\"vendor\":"+std::to_string(machine.vendor)+",\"video_memory_mb\":"+std::to_string(machine.videoMemory/(1024ull*1024ull))+",\"tier\":"+jsonString(r.tier)+
            ",\"render_scale\":"+jsonString(r.renderScale)+",\"dlss\":"+jsonString(r.dlss)+",\"gi\":"+jsonString(r.giQuality)+",\"ini_changed\":["+list+"],\"game_settings\":\"at the next start\",\"frame\":"+std::to_string(liveFrame.frame));
        return;}
    // The alternate eyes' pacing: live at once (the layer), and into [Picture] AfwPacing for the next start.
    if(const int pacing=evr::vrpanel::pacingOf(hit);pacing>=0){
        const int live=afwOn&&setAfwPacing?setAfwPacing(pacing):-1;   // 0 taken, 1 "fill" not prepared in this session
        afwPacing=pacing;pacingWaits=afwOn&&pacing==2&&live!=0;
        std::string why="no settings file (a development start)";bool saved=false;
        if(!settingsIni.empty()){std::string text;{std::ifstream in(std::filesystem::path(settingsIni),std::ios::binary);if(in)text.assign(std::istreambuf_iterator<char>(in),{});}
            const auto fresh=settingsIni+L".new";{std::ofstream o(std::filesystem::path(fresh),std::ios::binary|std::ios::trunc);if(o)o<<evr::vrpanel::storePacing(text,pacing);saved=bool(o);}
            saved=saved&&MoveFileExW(fresh.c_str(),settingsIni.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);if(!saved)why="could not write";}
        logFile->event("vr_settings_pacing","\"by\":\""+std::string(by)+"\",\"pacing\":"+std::to_string(pacing)+",\"alternate_eyes\":"+(afwOn?"true":"false")+",\"live\":"+std::to_string(live)+
            ",\"waits_for_next_start\":"+(pacingWaits.load()?"true":"false")+",\"saved\":"+(saved?"true":"false")+(saved?std::string():",\"why\":"+jsonString(why))+",\"frame\":"+std::to_string(liveFrame.frame));
        return;}
    const auto before=currentBody();
    auto after=evr::vrpanel::apply(before,hit);
    // a slider: the value where it was pressed (a Mock's press gives its value), then followed while held; saved when let go
    bool save=true;
    if(evr::vrpanel::isSlider(hit)){
        if(hit==Hit::DropSlider)after.crouchDrop=testValue>=0?evr::vrpanel::clampDrop(testValue):evr::vrpanel::sliderDrop(hoverX.load());
        else after.swingSpeed=testValue>=0?evr::settings::clampSwingSpeed(testValue):evr::vrpanel::sliderSwing(hoverX.load());
        if(testValue<0){sliderHeld=int(hit);save=false;}}
    changeBody(before,after,evr::vrpanel::name(hit),by,save);
}
// While a slider is held: its value follows the pointer (every frame the trigger is down; menu_pointer.hpp).
inline void drag(float u,float){
    std::lock_guard<std::mutex> lock(mutex);const auto held=Hit(sliderHeld.load());if(!evr::vrpanel::isSlider(held))return;
    const auto before=currentBody();auto after=before;const int x=int(std::floor((u-panelU0)*1920.f));
    if(held==Hit::DropSlider)after.crouchDrop=evr::vrpanel::sliderDrop(x);else after.swingSpeed=evr::vrpanel::sliderSwing(x);
    if(after.crouchDrop!=before.crouchDrop||after.swingSpeed!=before.swingSpeed)changeBody(before,after,held==Hit::DropSlider?"slider_drag":"swing_slider_drag","pointer",false);
}
// The trigger let go: a held slider's value goes into the settings file.
inline void release(){
    std::lock_guard<std::mutex> lock(mutex);const auto held=Hit(sliderHeld.exchange(int(Hit::None)));if(!evr::vrpanel::isSlider(held))return;
    const auto now=currentBody();
    changeBody(now,now,held==Hit::DropSlider?"slider_release":"swing_slider_release","pointer",true);
}
// The menu pointer's press (its own thread).
inline void press(){std::lock_guard<std::mutex> lock(mutex);pressHit(Hit(hoverHit.load()),"pointer");}
inline void install(){
    char value[8]{};const auto n=GetEnvironmentVariableA("EVR_BUG_REPORT",value,sizeof value);if(n==1&&value[0]=='0')return;
    const auto layer=GetModuleHandleW(L"evr_diagnostic.dll");if(!layer)return;
    setOverlay=reinterpret_cast<OverlayFn>(GetProcAddress(layer,"evrSetUiOverlay"));overlayShown=reinterpret_cast<ShownFn>(GetProcAddress(layer,"evrUiOverlayShown"));
    requestCapture=reinterpret_cast<CaptureFn>(GetProcAddress(layer,"evrRequestBugCapture"));
    updateState=reinterpret_cast<UpdateStateFn>(GetProcAddress(layer,"evrModUpdateState"));
    saveHistory=reinterpret_cast<SaveHistoryFn>(GetProcAddress(layer,"evrSaveEyeHistory"));historySaved=reinterpret_cast<HistorySavedFn>(GetProcAddress(layer,"evrEyeHistorySaved"));captureHalf=reinterpret_cast<HalfFn>(GetProcAddress(layer,"evrSetCaptureHalf"));
    if(!setOverlay||!overlayShown){logFile->event("bug_report_unavailable","\"reason\":\"the layer has no overlay\"");return;}
    language=gameLanguage();texts=evr::bugreport::texts(language);panelTexts=evr::vrpanel::texts(language);
    {wchar_t ini[MAX_PATH*2]{};const DWORD n=GetEnvironmentVariableW(L"EVR_SETTINGS_INI",ini,DWORD(std::size(ini)));if(n&&n<std::size(ini))settingsIni=ini;}
    {char v[512]{};const auto n=GetEnvironmentVariableA("EVR_VR_SETTINGS_PRESSES",v,sizeof v);if(n&&n<sizeof v)testPresses=evr::vrpanel::testPresses(v);}
    // the pacing shown: this session's (the layer's) with the alternate eyes, else the settings file's for their next start
    {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ALTERNATE_EYES",v,sizeof v);afwOn=n==1&&v[0]=='1';
     setAfwPacing=reinterpret_cast<PacingFn>(GetProcAddress(layer,"evrSetAfwPacing"));
     int shown=0;if(const auto now=reinterpret_cast<int(*)()>(GetProcAddress(layer,"evrAfwPacing"));now&&afwOn)shown=now()&3;
     else if(!settingsIni.empty()){std::string text;std::ifstream in(std::filesystem::path(settingsIni),std::ios::binary);if(in){text.assign(std::istreambuf_iterator<char>(in),{});shown=evr::settings::read(text).afwPacing;}}
     afwPacing=shown;}
    menuPointer::overlayButtonAt=&buttonAt;menuPointer::overlayButtonPress=&press;menuPointer::overlayButtonDrag=&drag;menuPointer::overlayButtonRelease=&release;installed=true;
    logFile->event("bug_report_enabled","\"language\":"+jsonString(language)+",\"window_ms\":"+std::to_string(evr::bugreport::windowMs)+",\"pairs\":"+std::to_string(evr::bugreport::plannedPairs())+
        ",\"captures\":"+std::string(requestCapture?"true":"false")+",\"vr_settings\":true,\"settings_file\":"+(settingsIni.empty()?std::string("null"):jsonString(narrowOf(settingsIni)))+
        ",\"test_presses\":"+std::to_string(testPresses.size())+",\"panel\":["+std::to_string(panelU0)+','+std::to_string(panelV0)+','+std::to_string(panelU1)+','+std::to_string(panelV1)+"]");
}
inline std::string stampNow(){SYSTEMTIME t{};GetLocalTime(&t);char b[32];std::snprintf(b,sizeof b,"%04u%02u%02u-%02u%02u%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return b;}
inline int64_t unixNow(){return int64_t(std::time(nullptr));}
// report.json: "status" recording (written at the window's start; a crash leaves it so - the settings program's "Make zip"
// then finishes the report: settings/vr_setting_app.cpp findUnfinishedReport) or complete.
inline uint64_t lastFlush=0;
inline void writeReport(uint64_t now,const char* status,const char* why){
    {std::ofstream report(folder/"report.json.new",std::ios::trunc);
    report<<"{\n \"what\": \"VR bug report (hooks/bug_report.hpp)\",\n \"status\": \""<<status<<"\",\n \"language\": "<<jsonString(language)<<",\n \"start_unix\": "<<startUnix<<",\n \"end_unix\": "<<endUnix<<
        ",\n \"start_frame\": "<<startFrame<<",\n \"end_frame\": "<<liveFrame.frame<<",\n \"window_ms\": "<<(now-startMs)<<",\n \"ended_by\": "<<jsonString(why)<<",\n \"hand_and_head_rows\": "<<rows<<
        ",\n \"captures\": [";
    for(size_t i=0;i<captureResults.size();++i)report<<(i?",":"")<<captureResults[i];
    report<<"],\n \"replay\": \"Mock: EVR_MOCK_HAND_SEQUENCE=hands.csv, -Pose head.csv (frames from 1 at the window's start)\"\n}\n";}
    std::error_code ec;std::filesystem::rename(folder/"report.json.new",folder/"report.json",ec);   // whole or not at all: a crash mid-write leaves the last one
}
// The settings program waits for the game beside the 30 seconds ("--watch-bug-report <folder> <pid>"): if the game goes
// before report.json says complete (a crash), it makes the zip at once (settings/vr_setting_app.cpp watchBugReport).
// Out of the game's job where that is allowed, so that it outlives the game.
inline void launchWatcher(){
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);const auto game=std::filesystem::path(exe).parent_path();
    const auto setting=game/L"Enshrouded VR setting.exe";if(!std::filesystem::exists(setting)){logFile->event("bug_report_watcher","\"started\":false,\"reason\":\"no settings program\"");return;}
    std::wstring command=L"\""+setting.wstring()+L"\" --watch-bug-report \""+folder.wstring()+L"\" "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};bool breakaway=true;
    BOOL ok=CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB,nullptr,game.wstring().c_str(),&si,&pi);
    if(!ok){breakaway=false;ok=CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,game.wstring().c_str(),&si,&pi);}
    if(ok){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
    logFile->event("bug_report_watcher","\"started\":"+std::string(ok?"true":"false")+",\"out_of_job\":"+(breakaway?"true":"false")+(ok?"":",\"error\":"+std::to_string(GetLastError())));
}
inline void begin(uint64_t now){
    const auto stamp=stampNow();folder=calibrationDirectory/("bug-report-"+stamp);std::error_code ec;std::filesystem::create_directories(folder,ec);
    handsCsv.open(folder/"hands.csv",std::ios::trunc);headCsv.open(folder/"head.csv",std::ios::trunc);framesCsv.open(folder/"frames.csv",std::ios::trunc);
    framesCsv<<"# xr_frame,predicted_display_time_ns,clock_ms,eyes_drawn\n";
    handsCsv<<"# The player's hands of the VR bug report "<<stamp<<" (Mock hand script, EVR_MOCK_HAND_SEQUENCE; include/evr/bug_report.hpp)\n";
    headCsv<<"# The player's head of the VR bug report "<<stamp<<" (Mock pose script, EVR_POSE_SEQUENCE)\n";
    rows=0;lastState=0;nextStep=0;captureResults.clear();startMs=now;startFrame=liveFrame.frame;startUnix=unixNow();endUnix=0;lastFlush=now;
    if(captureHalf)captureHalf(1);
    if(const auto cap=logFile->nameCap())logFile->setNameCap(cap);   // every kind of record is written afresh in the window
    phase=int(Phase::Recording);
    writeReport(now,"recording","");   // a crash in the window leaves this one: the settings program finishes the report from it
    launchWatcher();
    char env[64]{};std::string eyes="0",warp="0";if(GetEnvironmentVariableA("EVR_ALTERNATE_EYES",env,sizeof env))eyes=env;if(GetEnvironmentVariableA("EVR_EYE_WARP",env,sizeof env))warp=env;
    logFile->event("bug_report_window_begin","\"frame\":"+std::to_string(startFrame)+",\"unix\":"+std::to_string(startUnix)+",\"folder\":"+jsonString(folder.string())+",\"language\":"+jsonString(language)+
        ",\"armed_ms_before\":"+std::to_string(now-armedAt)+",\"alternate_eyes\":"+jsonString(eyes)+",\"eye_warp\":"+jsonString(warp)+",\"log_name_cap\":"+std::to_string(logFile->nameCap()));
    // A development headset session with a RenderDoc capture on request (run_play_session.ps1 -RealVr -CaptureWorldFinal
    // -CaptureOnRequest; 2026-10-18 19:50, the field's soil flat in the right eye on the headset only): the press also asks
    // for that one capture of the frame (render() takes the request within 15 pairs).
    if(captureLiveFinal&&captureOnRequest&&requestedCaptures<maxRequestedCaptures){std::error_code ec;
        const HANDLE f=CreateFileW((calibrationDirectory/"world-capture.request").wstring().c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        const bool asked=f!=INVALID_HANDLE_VALUE;if(asked)CloseHandle(f);
        logFile->event("bug_report_renderdoc_request","\"asked\":"+std::string(asked?"true":"false"));}
}
inline void launchMaker(){
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);const auto game=std::filesystem::path(exe).parent_path();
    const auto setting=game/L"Enshrouded VR setting.exe";
    std::wstring command=L"\""+setting.wstring()+L"\" --bug-report \""+folder.wstring()+L"\" "+std::to_wstring(startUnix)+L" "+std::to_wstring(endUnix)+L" "+wideOf(language);
    STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
    if(std::filesystem::exists(setting)&&CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,game.wstring().c_str(),&si,&pi)){
        CloseHandle(pi.hThread);maker=pi.hProcess;phase=int(Phase::Making);
        logFile->event("bug_report_zip_started","\"program\":"+jsonString(setting.string()));
    }else{
        phase=int(Phase::Done);doneAt=GetTickCount64();zipName=L"(" + folder.wstring() + L")";makerExit=GetLastError();
        logFile->event("bug_report_zip_failed","\"reason\":\"the settings program could not be started\",\"error\":"+std::to_string(makerExit)+",\"program\":"+jsonString(setting.string()));
    }
}
inline void finish(uint64_t now,const char* why){
    if(captureHalf)captureHalf(-1);
    endUnix=unixNow();handsCsv.close();headCsv.close();framesCsv.close();
    writeReport(now,"complete",why);
    logFile->event("bug_report_window_end","\"frame\":"+std::to_string(liveFrame.frame)+",\"window_ms\":"+std::to_string(now-startMs)+",\"rows\":"+std::to_string(rows)+",\"captures\":"+std::to_string(captureResults.size())+",\"ended_by\":"+jsonString(why));
    // the window's last 10 seconds, then the zip (frame() starts the maker once they are written)
    if(saveHistory&&historySaved&&std::string(why)!="session end"){makerWaitSaves=historySaved()+1;saveHistory();makerWaitUntil=now+5000;phase=int(Phase::Making);}
    else launchMaker();
}
inline void sample(const evr::motion::HandFrame& f,const evr::motion::GestureHead& head,uint64_t now){
    ++rows;handsCsv<<evr::bugreport::mockHandRow(rows,f)<<'\n';
    // the headset's recommended size (the swapchain over the render scale, as the Mock makes it) and its eyes' fovs
    const auto recommended=[](uint32_t v){return liveRenderScale>0&&v?uint32_t(std::lround(double(v)/liveRenderScale)):0u;};
    std::array<float,8> fov{};
    for(size_t eye=0;eye<2;++eye){const auto& v=liveFrame.views[eye].fov;fov[eye*4]=v.angleLeft;fov[eye*4+1]=v.angleRight;fov[eye*4+2]=v.angleUp;fov[eye*4+3]=v.angleDown;}
    {LARGE_INTEGER c{},q{};QueryPerformanceCounter(&c);QueryPerformanceFrequency(&q);
     framesCsv<<liveFrame.frame<<','<<liveFrame.time<<','<<std::fixed<<std::setprecision(3)<<(q.QuadPart?double(c.QuadPart)*1000.0/double(q.QuadPart):0.0)<<std::defaultfloat<<','<<unsigned(liveFrame.eyeMask)<<'\n';}
    headCsv<<evr::bugreport::mockHeadRow(double(now-startMs)/1000.0,head.pose,(head.flags&15)==15,recommended(liveFrame.width[0]),recommended(liveFrame.height[0]),&fov)<<'\n';
    if(now>=lastFlush+1000){lastFlush=now;handsCsv.flush();headCsv.flush();framesCsv.flush();writeReport(now,"recording","");}   // a crash loses at most a second
    // EVR_BUG_REPORT_CRASH_AT_MS=<ms> (Mock trials only): the game crashes that long into the window, to try the crash case.
    {static const uint64_t crashAt=[]{char v[24]{};const auto n=GetEnvironmentVariableA("EVR_BUG_REPORT_CRASH_AT_MS",v,sizeof v);return n&&n<sizeof v?std::strtoull(v,nullptr,10):0ull;}();
     if(crashAt&&now-startMs>=crashAt){logFile->event("bug_report_test_crash","\"t_ms\":"+std::to_string(now-startMs));*static_cast<volatile int*>(nullptr)=1;}}
    if(now>=lastState+500){lastState=now;
        std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<f.frame<<",\"t_ms\":"<<(now-startMs)<<",\"screen\":"<<unsigned(menuPointer::screen.load())<<",\"paused\":"<<(pauseClock::paused.load()?"true":"false")
            <<",\"focused\":"<<(f.focused?"true":"false")<<",\"head\":["<<head.pose.position.x<<','<<head.pose.position.y<<','<<head.pose.position.z<<"],\"head_tracked\":"<<((head.flags&15)==15?"true":"false");
        for(unsigned i=0;i<2;++i){const auto& h=f.hands[i];o<<",\""<<(i?"right":"left")<<"\":{\"grip\":["<<h.grip.position.x<<','<<h.grip.position.y<<','<<h.grip.position.z<<"],\"tracked\":"<<(h.tracked()?"true":"false")
            <<",\"trigger\":"<<h.trigger<<",\"grab\":"<<(h.grab?"true":"false")<<",\"stick\":["<<h.stick.x<<','<<h.stick.y<<"],\"a\":"<<(h.primaryPressed?"true":"false")<<",\"b\":"<<(h.secondaryPressed?"true":"false")<<"}";}
        o<<",\"airborne\":"<<(controllerUiFixture::airborneNow(now)?"true":"false");
        logFile->event("bug_report_state",o.str());
    }
    while(nextStep<evr::bugreport::captureSteps.size()&&now-startMs>=evr::bugreport::captureSteps[nextStep].atMs){
        const auto& s=evr::bugreport::captureSteps[nextStep];if(captureHalf)captureHalf(s.full?0:1);const int r=requestCapture?requestCapture(s.pairs):-1;
        if(r==1&&now-startMs<evr::bugreport::captureSteps[nextStep].atMs+600)break;   // the earlier pictures are still being taken: again on the next frame
        // one line a step: taken (0), or given up after 0.6 s (1: the pictures before it were not taken - no eyes drawn)
        captureResults.push_back("{\"t_ms\":"+std::to_string(now-startMs)+",\"frame\":"+std::to_string(f.frame)+",\"pairs\":"+std::to_string(s.pairs)+",\"full_size\":"+(s.full?"true":"false")+",\"result\":"+std::to_string(r)+"}");
        ++nextStep;
    }
}
// Once per XR frame (view_resource_probe.cpp, after the menu pointer).
inline void disable();
inline void frame(const evr::motion::HandFrame& f,const evr::motion::GestureHead& head,uint64_t now){
    static bool tried=false;if(!tried&&menuPointer::installed){tried=true;install();if(installed)menuPointer::onDisable=&disable;}
    if(!installed)return;
    try{
        std::lock_guard<std::mutex> lock(mutex);
        const auto screen=menuPointer::screen.load();
        // EVR_BUG_REPORT_PRESS_AT=<XR frame> (Mock trials): the button counts as pressed at that frame while the start menu is open.
        {static const uint64_t pressAt=[]{char v[24]{};const auto n=GetEnvironmentVariableA("EVR_BUG_REPORT_PRESS_AT",v,sizeof v);return n&&n<sizeof v?std::strtoull(v,nullptr,10):0ull;}();
         static bool pressed=false;if(pressAt&&!pressed&&f.frame>=pressAt&&screen==2&&phase.load()==int(Phase::Idle)){pressed=true;pressHit(Hit::BugReport,"test");}}
        while(nextTestPress<testPresses.size()&&f.frame>=testPresses[nextTestPress].frame&&screen==2){pressHit(testPresses[nextTestPress].hit,"test",testPresses[nextTestPress].value);++nextTestPress;}
        const auto p=Phase(phase.load());
        if(p==Phase::Armed&&screen==1&&lastScreen!=1)begin(now);
        else if(p==Phase::Recording){sample(f,head,now);if(now-startMs>=evr::bugreport::windowMs)finish(now,"time");}
        else if(p==Phase::Making&&!maker&&makerWaitUntil){   // waiting for the window's last seconds to be written
            if(historySaved()>=makerWaitSaves||now>=makerWaitUntil){makerWaitUntil=0;launchMaker();}
        }
        else if(p==Phase::Making&&maker&&WaitForSingleObject(maker,0)==WAIT_OBJECT_0){
            GetExitCodeProcess(maker,&makerExit);CloseHandle(maker);maker=nullptr;
            std::ifstream z(folder/"zip.txt");std::string name;std::getline(z,name);zipName=name.empty()?folder.wstring():wideOf(name);
            phase=int(Phase::Done);doneAt=now;
            logFile->event("bug_report_zip_done","\"exit\":"+std::to_string(makerExit)+",\"zip\":"+jsonString(narrowOf(zipName)));
        }
        if(Phase(phase.load())==Phase::Done&&now-doneAt>15000)phase=int(Phase::Idle);
        lastScreen=screen;
        // What the plate shows.
        const auto q=Phase(phase.load());
        if(screen==2&&(q==Phase::Idle||q==Phase::Armed||q==Phase::Done)){
            const int kind=q==Phase::Armed?1:0;
            // over whatever the game shows there (user 2026-10-15: "show it even over other things - this first")
            // hidden while the game covers much of the place (the settings page, user 2026-10-17: "the bug report button stays
            // over the graphics settings - hide it once the settings are open"), still over small things (2026-10-15)
            if(kind!=drawnKind||look(kind)!=drawnLook){drawPanel(kind);setOverlay(panel.data(),panelW,panelH,panelU0,panelV0,panelU1,panelV1,2);}
            else setOverlay(nullptr,0,0,panelU0,panelV0,panelU1,panelV1,2);
        }else if(q==Phase::Recording||q==Phase::Making||q==Phase::Done){
            const int seconds=q==Phase::Recording?int((evr::bugreport::windowMs-std::min(evr::bugreport::windowMs,now-startMs)+999)/1000):q==Phase::Making?-2:-3;
            if(seconds!=drawnSeconds||drawnKind!=2){
                wchar_t text[512]{};
                if(q==Phase::Recording)std::swprintf(text,512,texts.recording,seconds);
                else if(q==Phase::Making)std::swprintf(text,512,L"%ls",texts.making);
                else std::swprintf(text,512,texts.saved,std::filesystem::path(zipName).filename().wstring().c_str());
                drawLine(text);drawnSeconds=seconds;drawnKind=2;
                setOverlay(line.data(),lineW,lineH,lineU0,lineV0,lineU1,lineV1,0);
            }else setOverlay(nullptr,0,0,lineU0,lineV0,lineU1,lineV1,0);
        }else if(drawnKind!=-1){setOverlay(nullptr,0,0,0,0,0,0,0);drawnKind=-1;drawnSeconds=-1;}
        if(screen!=2){settingsOpen=false;if(sliderHeld.load()){mutex.unlock();release();mutex.lock();}}   // closed with the start menu
    }catch(const std::exception& e){static std::atomic<unsigned> failures{0};if(failures.fetch_add(1)<3)logFile->event("bug_report_failure","\"reason\":"+jsonString(e.what()));}
}
// At the session's end: a window under way is closed and its zip made with what there is.
inline void disable(){
    if(!installed)return;
    try{std::lock_guard<std::mutex> lock(mutex);
        if(Phase(phase.load())==Phase::Recording)finish(GetTickCount64(),"session end");
        installed=false;menuPointer::overlayButtonAt=nullptr;menuPointer::overlayButtonPress=nullptr;menuPointer::overlayButtonDrag=nullptr;menuPointer::overlayButtonRelease=nullptr;if(setOverlay)setOverlay(nullptr,0,0,0,0,0,0,0);
        if(captureHalf)captureHalf(-1);
        logFile->event("bug_report_removed","\"phase\":"+std::to_string(phase.load()));
    }catch(...){}
}
}
