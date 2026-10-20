#pragma once
// The VR settings in the game's start menu (user, 2026-10-18 after the headset run of that morning: "the crouch's
// threshold should be changeable: add a VR settings item to the menu, where real crouching is switched on and off and
// its threshold changed, even during the game"; then "the crouch depth set freely with a slider"). A button beside the VR
// bug report's (hooks/bug_report.hpp draws both on the mod's own picture over the start menu and takes the menu
// pointer's presses); pressed, the place under the buttons shows the settings instead of the bug report's note. A change
// applies at once and is written into EnshroudedVR.ini ([Body] RealCrouch, CrouchDrop - include/evr/vr_settings.hpp),
// which the loader gives to the next start (EVR_ROOM_SNEAK, EVR_CROUCH_DROP).
// (From 11:00 to 12:40 the same day it also switched off, for comparing, what the right eye takes from the left eye's
// drawing - the shadow map, the indirect light, the fog. The user after trying it: "not borrowing did not solve the
// problems; the not-borrowing setting can go" - removed. A row for lock-on movement stood here from 15:30 to 18:10 on
// 2026-10-19 - removed with the option, include/evr/game_input.hpp.)
#include <evr/vr_settings.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
namespace evr::vrpanel {
// The crouch's threshold: how far the headset comes down, in percent of the standing eye height (the user's "30 %
// down"; include/evr/room_sneak.hpp enters under 1 - drop and leaves over that x 1.05). 10 .. 50, any whole percent.
// swingSpeed: the speed a stroke needs to attack, percent (include/evr/vr_settings.hpp, 50 .. 150 in steps of 5);
// armSwim: swimming by the arms (include/evr/swim_stroke.hpp). Both 2026-10-20.
struct Body {bool realCrouch=true;int crouchDrop=30;int swingSpeed=100;bool armSwim=true;};
constexpr int dropLow=10,dropHigh=50;
inline int clampDrop(int drop){return std::clamp(drop,dropLow,dropHigh);}
inline float enterRatio(int drop){return 1.f-float(clampDrop(drop))/100.f;}
// The values into the settings file's text (every other line stays).
inline std::string store(const std::string& ini,const Body& b){
    auto out=settings::set(settings::set(ini,"Body","RealCrouch",b.realCrouch?"1":"0"),"Body","CrouchDrop",std::to_string(clampDrop(b.crouchDrop)));
    out=settings::set(out,"Body","ArmSwim",b.armSwim?"1":"0");
    return settings::set(out,"Melee","SwingSpeed",std::to_string(settings::clampSwingSpeed(b.swingSpeed)));
}
// What a place on the picture is, and what pressing it does.
enum class Hit {None,BugReport,Settings,CrouchOn,CrouchOff,DropSlider,SwingSlider,SwimOn,SwimOff};
inline const char* name(Hit h){
    switch(h){case Hit::BugReport:return "bug_report";case Hit::Settings:return "settings";case Hit::CrouchOn:return "crouch_on";case Hit::CrouchOff:return "crouch_off";
        case Hit::DropSlider:return "slider";case Hit::SwingSlider:return "swing_slider";case Hit::SwimOn:return "swim_on";case Hit::SwimOff:return "swim_off";default:return "none";}
}
inline Hit hitNamed(const std::string& s){
    for(const auto h:{Hit::BugReport,Hit::Settings,Hit::CrouchOn,Hit::CrouchOff,Hit::DropSlider,Hit::SwingSlider,Hit::SwimOn,Hit::SwimOff})if(s==name(h))return h;
    return Hit::None;
}
inline bool isSlider(Hit h){return h==Hit::DropSlider||h==Hit::SwingSlider;}
inline Body apply(Body b,Hit h){
    if(h==Hit::CrouchOn)b.realCrouch=true;else if(h==Hit::CrouchOff)b.realCrouch=false;
    else if(h==Hit::SwimOn)b.armSwim=true;else if(h==Hit::SwimOff)b.armSwim=false;
    return b;
}
// The picture's places, in its own pixels (1200 x 450, drawn at about the UI's pixel size of 1920 x 1080). Redrawn
// 2026-10-20 (user: "the in-game settings' items are too big - make them neat and easy to read; only the bug report's
// button needs to stand out"): the bug report's button on top as before, the settings' button a small pill beside it,
// and under them, while open, four slim rows - a label on the left (a heading and a small note), the control on the
// right (two small pills, or a thin slider) - and the note at the bottom.
struct Box {Hit hit;int x0,y0,x1,y1;constexpr bool contains(int x,int y)const{return x>=x0&&x<x1&&y>=y0&&y<y1;}};
constexpr int width=1200,height=450,buttonHeight=84;
constexpr Box bugButton{Hit::BugReport,0,0,640,buttonHeight},settingsButton{Hit::Settings,664,20,868,64};
// the sheet under the buttons, and its rows
constexpr int sheetTop=96,rowTop=108,rowStep=60,rowHeight=52,labelX0=30,labelX1=660;
constexpr int rowY(int row){return rowTop+row*rowStep;}
constexpr Box pillOn(Hit h,int row){return {h,968,rowY(row)+10,1064,rowY(row)+44};}
constexpr Box pillOff(Hit h,int row){return {h,1074,rowY(row)+10,1170,rowY(row)+44};}
constexpr Box sliderBox(Hit h,int row){return {h,700,rowY(row)+4,1172,rowY(row)+48};}
constexpr Box crouchOn=pillOn(Hit::CrouchOn,0),crouchOff=pillOff(Hit::CrouchOff,0);
constexpr Box dropSlider=sliderBox(Hit::DropSlider,1),swingSlider=sliderBox(Hit::SwingSlider,2);
constexpr Box swimOn=pillOn(Hit::SwimOn,3),swimOff=pillOff(Hit::SwimOff,3);
constexpr int noteTop=rowY(4)+8;
// A slider: the press (and the trigger held while the hand moves) sets the value where the pointer is on its track; the
// ends of the box past the track give the lowest and the highest.
constexpr int trackInset=24;
constexpr int trackStart(const Box& b){return b.x0+trackInset;}
constexpr int trackEnd(const Box& b){return b.x1-trackInset;}
inline int sliderValue(const Box& b,int low,int high,int step,int x){
    const float t=std::clamp(float(x-trackStart(b))/float(trackEnd(b)-trackStart(b)),0.f,1.f);
    const int v=int(std::lround((float(low)+t*float(high-low))/float(step)))*step;return std::clamp(v,low,high);
}
inline int sliderPosition(const Box& b,int low,int high,int value){
    return trackStart(b)+int(std::lround(float(std::clamp(value,low,high)-low)/float(high-low)*float(trackEnd(b)-trackStart(b))));
}
constexpr int trackX0=trackStart(dropSlider),trackX1=trackEnd(dropSlider);
inline int sliderDrop(int x){return sliderValue(dropSlider,dropLow,dropHigh,1,x);}
inline int sliderX(int drop){return sliderPosition(dropSlider,dropLow,dropHigh,clampDrop(drop));}
inline int sliderSwing(int x){return sliderValue(swingSlider,settings::swingSpeedLow,settings::swingSpeedHigh,settings::swingSpeedStep,x);}
inline int swingX(int percent){return sliderPosition(swingSlider,settings::swingSpeedLow,settings::swingSpeedHigh,settings::clampSwingSpeed(percent));}
inline std::vector<Box> boxes(bool open){
    std::vector<Box> out{bugButton,settingsButton};
    if(open)for(const auto& b:{crouchOn,crouchOff,dropSlider,swingSlider,swimOn,swimOff})out.push_back(b);
    return out;
}
inline Hit hitAt(int x,int y,bool open){for(const auto& b:boxes(open))if(b.contains(x,y))return b.hit;return Hit::None;}
// The texts, in Steam's game language (English for any other). A row's label is its heading, a line break, and a smaller
// explanation (two blocks: a heading and its note are drawn apart, each fitting its own height).
struct Texts {const wchar_t* button;const wchar_t* crouch;const wchar_t* on;const wchar_t* off;const wchar_t* drop;const wchar_t* note;const wchar_t* unsaved;
    const wchar_t* swing;const wchar_t* swim;};   // 2026-10-20: the stroke's speed (a number, as drop) and swimming by the arms
// (2026-10-19, user: "the in-game settings in the game's language" - the languages of the VR bug report's button,
// include/evr/bug_report.hpp; English for any other.)
inline Texts texts(const std::string& language){
    if(language=="german")return {L"VR-Einstellungen",L"Echtes Ducken\nKopf senken zum Schleichen",L"Ein",L"Aus",L"Ducktiefe: %d%%\nWie weit unter der Augenhöhe im Stehen",
        L"Einstellungen gelten sofort und bleiben für den nächsten Start.\nAugenhöhe im Stehen = Höhe beim letzten Zurücksetzen der Sicht.",
        L"(Entwicklungsstart: gilt jetzt, wird nicht gespeichert)",
        L"Schwungtempo für Angriffe: %d%%\nNiedriger: schon ein leichterer Schwung greift an",L"Mit den Armen schwimmen\nIm Wasser mit offenen Händen paddeln (Daumen weg vom Stick)"};
    if(language=="french")return {L"Réglages VR",L"Accroupissement réel\nBaissez la tête pour vous faufiler",L"Oui",L"Non",L"Profondeur : %d%%\nDe combien sous la hauteur des yeux debout",
        L"Les réglages s'appliquent tout de suite et sont gardés au prochain lancement.\nHauteur des yeux debout = hauteur au dernier recentrage de la vue.",
        L"(lancement de développement : appliqué maintenant, non enregistré)",
        L"Vitesse du coup pour attaquer : %d%%\nPlus bas : un coup plus léger suffit",L"Nager avec les bras\nDans l'eau, brassez mains ouvertes (pouces hors des sticks)"};
    if(language=="spanish"||language=="latam")return {L"Ajustes de VR",L"Agacharse de verdad\nBaja la cabeza para ir en sigilo",L"Sí",L"No",L"Profundidad: %d%%\nCuánto por debajo de la altura de los ojos de pie",
        L"Los ajustes se aplican al instante y se guardan para el próximo inicio.\nAltura de los ojos de pie = la del último reinicio de la vista.",
        L"(inicio de desarrollo: se aplica ahora, no se guarda)",
        L"Velocidad del golpe para atacar: %d%%\nMenor: basta un golpe más suave",L"Nadar con los brazos\nEn el agua, brazadas con las manos abiertas (pulgares fuera del stick)"};
    if(language=="italian")return {L"Impostazioni VR",L"Accovacciamento reale\nAbbassa la testa per muoverti furtivo",L"Sì",L"No",L"Profondità: %d%%\nQuanto sotto l'altezza degli occhi in piedi",
        L"Le impostazioni valgono subito e restano al prossimo avvio.\nAltezza degli occhi in piedi = quella dell'ultimo ripristino della vista.",
        L"(avvio di sviluppo: vale ora, non viene salvato)",
        L"Velocità del colpo per attaccare: %d%%\nPiù bassa: basta un colpo più leggero",L"Nuotare con le braccia\nIn acqua, bracciate a mani aperte (pollici lontani dagli stick)"};
    if(language=="polish")return {L"Ustawienia VR",L"Prawdziwe kucanie\nOpuść głowę, aby się skradać",L"Wł.",L"Wył.",L"Głębokość: %d%%\nO ile poniżej wysokości oczu na stojąco",
        L"Ustawienia działają od razu i zostają na następne uruchomienie.\nWysokość oczu na stojąco = z ostatniego resetu widoku.",
        L"(uruchomienie deweloperskie: działa teraz, nie zapisano)",
        L"Szybkość zamachu do ataku: %d%%\nNiżej: wystarczy lżejszy zamach",L"Pływanie rękami\nW wodzie wiosłuj otwartymi dłońmi (kciuki z dala od drążków)"};
    if(language=="brazilian"||language=="portuguese")return {L"Configurações de VR",L"Agachar de verdade\nAbaixe a cabeça para ficar furtivo",L"Sim",L"Não",L"Profundidade: %d%%\nQuanto abaixo da altura dos olhos em pé",
        L"As configurações valem na hora e ficam para a próxima vez.\nAltura dos olhos em pé = a do último reajuste da visão.",
        L"(início de desenvolvimento: vale agora, não é salvo)",
        L"Velocidade do golpe para atacar: %d%%\nMenor: um golpe mais leve já ataca",L"Nadar com os braços\nNa água, braçadas com as mãos abertas (polegares fora do analógico)"};
    if(language=="russian")return {L"Настройки VR",L"Настоящее приседание\nОпустите голову, чтобы красться",L"Вкл",L"Выкл",L"Глубина: %d%%\nНасколько ниже высоты глаз стоя",
        L"Настройки действуют сразу и сохраняются до следующего запуска.\nВысота глаз стоя = при последнем сбросе вида.",
        L"(запуск для разработки: действует сейчас, не сохраняется)",
        L"Скорость взмаха для атаки: %d%%\nНиже: хватит и более лёгкого взмаха",L"Плавание руками\nВ воде гребите открытыми ладонями (большие пальцы не на стиках)"};
    if(language=="ukrainian")return {L"Налаштування VR",L"Справжнє присідання\nОпустіть голову, щоб скрадатися",L"Увімк",L"Вимк",L"Глибина: %d%%\nНаскільки нижче висоти очей стоячи",
        L"Налаштування діють одразу й зберігаються до наступного запуску.\nВисота очей стоячи = під час останнього скидання виду.",
        L"(запуск для розробки: діє зараз, не зберігається)",
        L"Швидкість замаху для атаки: %d%%\nНижче: вистачить і легшого замаху",L"Плавання руками\nУ воді гребіть відкритими долонями (великі пальці не на стиках)"};
    if(language=="schinese")return {L"VR 设置",L"真实下蹲\n低头即可潜行",L"开",L"关",L"下蹲深度：%d%%\n比站立时的眼睛高度低多少",
        L"设置立即生效，并保留到下次启动。\n站立时的眼睛高度 = 上次重置视角时的高度。",
        L"（开发用启动：仅本次生效，不保存）",
        L"攻击所需挥动速度：%d%%\n越低，越轻的挥动也能攻击",L"用手臂游泳\n在水中张开手划水即可前进（拇指离开摇杆）"};
    if(language=="tchinese")return {L"VR 設定",L"真實蹲下\n低頭即可潛行",L"開",L"關",L"蹲下深度：%d%%\n比站立時的眼睛高度低多少",
        L"設定立即生效，並保留到下次啟動。\n站立時的眼睛高度 = 上次重設視角時的高度。",
        L"（開發用啟動：僅本次生效，不儲存）",
        L"攻擊所需揮動速度：%d%%\n越低，越輕的揮動也能攻擊",L"用手臂游泳\n在水中張開手划水即可前進（拇指離開搖桿）"};
    if(language=="koreana")return {L"VR 설정",L"실제 웅크리기\n머리를 숙이면 은신합니다",L"켜기",L"끄기",L"웅크림 깊이: %d%%\n서 있을 때 눈높이보다 얼마나 낮을 때",
        L"설정은 즉시 적용되고 다음 실행에도 유지됩니다.\n서 있을 때 눈높이 = 마지막으로 시점을 재설정한 높이.",
        L"(개발용 실행: 이번에만 적용, 저장 안 됨)",
        L"공격에 필요한 휘두르기 속도: %d%%\n낮을수록 가볍게 휘둘러도 공격합니다",L"팔로 수영\n물속에서 손을 펴고 물을 저으면 나아갑니다 (엄지는 스틱에서 떼기)"};
    if(language=="turkish")return {L"VR ayarları",L"Gerçek eğilme\nGizlenmek için başını eğ",L"Açık",L"Kapalı",L"Eğilme derinliği: %d%%\nAyaktaki göz yüksekliğinin ne kadar altında",
        L"Ayarlar hemen uygulanır ve sonraki açılışta da kalır.\nAyaktaki göz yüksekliği = son görüş sıfırlamasındaki yükseklik.",
        L"(geliştirme başlatması: şimdi uygulanır, kaydedilmez)",
        L"Saldırı için savurma hızı: %d%%\nDüşük: daha hafif bir savuruş da saldırır",L"Kollarla yüzme\nSuda açık ellerle kulaç at (başparmaklar çubuktan uzakta)"};
    if(language=="japanese")return {L"VR設定",L"リアルしゃがみ\n頭を下げると隠密になります",L"入",L"切",L"しゃがむ深さ：%d%%\n立った時の目の高さから何%%下がったら隠密か",
        L"設定はすぐに反映され、次の起動にも残ります。\n立った時の目の高さ＝最後に視点をリセットした時の高さです。",
        L"（開発用の起動：この回だけ反映、保存はしません）",
        L"攻撃に要る振りの速さ：%d%%\n低いほど軽い振りで攻撃が出ます",L"腕で泳ぐ\n水の中で手を開いて水をかくと進みます（親指はスティックから離す）"};
    return {L"VR settings",L"Real crouch\nLower your head to sneak",L"On",L"Off",L"Crouch depth: %d%%\nHow far below your standing eye height",
        L"Settings apply at once and are kept for the next start.\nStanding eye height = the height at your last view reset.",
        L"(development start: applies now, not saved)",
        L"Swing speed to attack: %d%%\nLower: a lighter swing attacks",L"Swim with your arms\nIn water, stroke with open hands (thumbs off the sticks)"};
}
// Presses for a Mock trial (EVR_VR_SETTINGS_PRESSES): "frame:name;frame:name", in the order of their frames; a slider
// takes its value as "frame:slider=40" / "frame:swing_slider=80".
struct TestPress {unsigned long long frame;Hit hit;int value=-1;};
inline std::vector<TestPress> testPresses(const std::string& text){
    std::vector<TestPress> out;size_t at=0;
    while(at<text.size()){
        const auto end=std::min(text.find(';',at),text.size());const auto item=settings::trim(text.substr(at,end-at));at=end+1;
        const auto colon=item.find(':');if(colon==std::string::npos)continue;
        char* stop=nullptr;const auto frame=std::strtoull(item.c_str(),&stop,10);
        auto what=settings::trim(item.substr(colon+1));int value=-1;
        if(const auto eq=what.find('=');eq!=std::string::npos){char* e=nullptr;const auto v=std::strtol(what.c_str()+eq+1,&e,10);if(e&&!*e&&e!=what.c_str()+eq+1)value=int(v);what=what.substr(0,eq);}
        const auto hit=hitNamed(what);
        if(stop==item.c_str()+colon&&frame&&hit!=Hit::None&&(!isSlider(hit)||value>=0))out.push_back({frame,hit,value});
    }
    std::stable_sort(out.begin(),out.end(),[](const TestPress& a,const TestPress& b){return a.frame<b.frame;});
    return out;
}
}
