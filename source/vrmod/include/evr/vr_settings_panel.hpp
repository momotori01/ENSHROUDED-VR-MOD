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
// armSwim: swimming by the arms (include/evr/swim_stroke.hpp). Both 2026-10-20. hitStop: the hit stop of a melee stroke
// (include/evr/hit_stop.hpp; 2026-10-20 17:40, user: "make the hit stop switchable in the game's settings").
struct Body {bool realCrouch=true;int crouchDrop=30;int swingSpeed=100;bool armSwim=true;bool hitStop=true;};
constexpr int dropLow=10,dropHigh=50;
inline int clampDrop(int drop){return std::clamp(drop,dropLow,dropHigh);}
inline float enterRatio(int drop){return 1.f-float(clampDrop(drop))/100.f;}
// The values into the settings file's text (every other line stays).
inline std::string store(const std::string& ini,const Body& b){
    auto out=settings::set(settings::set(ini,"Body","RealCrouch",b.realCrouch?"1":"0"),"Body","CrouchDrop",std::to_string(clampDrop(b.crouchDrop)));
    out=settings::set(out,"Body","ArmSwim",b.armSwim?"1":"0");
    out=settings::set(out,"Melee","SwingSpeed",std::to_string(settings::clampSwingSpeed(b.swingSpeed)));
    return settings::set(out,"Melee","HitStop",b.hitStop?"1":"0");
}
// What a place on the picture is, and what pressing it does.
// PacingMax / PacingHalf / PacingFill (2026-10-23): the alternate eyes' pacing 0 / 1 / 2 (include/evr/afw_pacing.hpp).
enum class Hit {None,BugReport,Settings,CrouchOn,CrouchOff,DropSlider,SwingSlider,SwimOn,SwimOff,HitStopOn,HitStopOff,Recommend,PacingMax,PacingHalf,PacingFill};
inline const char* name(Hit h){
    switch(h){case Hit::BugReport:return "bug_report";case Hit::Settings:return "settings";case Hit::CrouchOn:return "crouch_on";case Hit::CrouchOff:return "crouch_off";
        case Hit::DropSlider:return "slider";case Hit::SwingSlider:return "swing_slider";case Hit::SwimOn:return "swim_on";case Hit::SwimOff:return "swim_off";
        case Hit::HitStopOn:return "hit_stop_on";case Hit::HitStopOff:return "hit_stop_off";case Hit::Recommend:return "recommend";
        case Hit::PacingMax:return "pacing_0";case Hit::PacingHalf:return "pacing_1";case Hit::PacingFill:return "pacing_2";default:return "none";}
}
inline Hit hitNamed(const std::string& s){
    for(const auto h:{Hit::BugReport,Hit::Settings,Hit::CrouchOn,Hit::CrouchOff,Hit::DropSlider,Hit::SwingSlider,Hit::SwimOn,Hit::SwimOff,Hit::HitStopOn,Hit::HitStopOff,Hit::Recommend,
                      Hit::PacingMax,Hit::PacingHalf,Hit::PacingFill})if(s==name(h))return h;
    return Hit::None;
}
inline bool isSlider(Hit h){return h==Hit::DropSlider||h==Hit::SwingSlider;}
// The pacing a press chooses (-1: not a pacing press), and the settings file with it ([Picture] AfwPacing; the Body's lines
// are not written with it - the pacing is not part of the Body, whose values a press writes all together).
inline int pacingOf(Hit h){return h==Hit::PacingMax?0:h==Hit::PacingHalf?1:h==Hit::PacingFill?2:-1;}
inline std::string storePacing(const std::string& ini,int pacing){return settings::set(ini,"Picture","AfwPacing",std::to_string(std::clamp(pacing,0,2)));}
inline Body apply(Body b,Hit h){
    if(h==Hit::CrouchOn)b.realCrouch=true;else if(h==Hit::CrouchOff)b.realCrouch=false;
    else if(h==Hit::SwimOn)b.armSwim=true;else if(h==Hit::SwimOff)b.armSwim=false;
    else if(h==Hit::HitStopOn)b.hitStop=true;else if(h==Hit::HitStopOff)b.hitStop=false;
    return b;
}
// The picture's places, in its own pixels (1200 x 450, drawn at about the UI's pixel size of 1920 x 1080). Redrawn
// 2026-10-20 (user: "the in-game settings' items are too big - make them neat and easy to read; only the bug report's
// button needs to stand out"): the bug report's button on top as before, the settings' button a small pill beside it,
// and under them, while open, four slim rows - a label on the left (a heading and a small note), the control on the
// right (two small pills, or a thin slider) - and the note at the bottom. 17:40: a fifth row (the hit stop), the picture
// 60 pixels taller (510; still inside the UI image: from 0.47 x 1080 = 508 to 1018). The sixth row (the recommended graphics,
// a button - include/evr/recommended_graphics.hpp) another 60: 570, to 1078. 2026-10-23: a seventh row (the alternate eyes'
// pacing, three pills) in the same 570: the rows 52 pixels apart instead of 60, their pills and sliders a little lower.
struct Box {Hit hit;int x0,y0,x1,y1;constexpr bool contains(int x,int y)const{return x>=x0&&x<x1&&y>=y0&&y<y1;}};
constexpr int width=1200,height=570,buttonHeight=84;
constexpr Box bugButton{Hit::BugReport,0,0,640,buttonHeight},settingsButton{Hit::Settings,664,20,868,64};
// the sheet under the buttons, and its rows
constexpr int sheetTop=96,rowTop=108,rowStep=52,rowHeight=46,labelX0=30,labelX1=660;
constexpr int rowY(int row){return rowTop+row*rowStep;}
constexpr Box pillOn(Hit h,int row){return {h,968,rowY(row)+7,1064,rowY(row)+39};}
constexpr Box pillOff(Hit h,int row){return {h,1074,rowY(row)+7,1170,rowY(row)+39};}
constexpr Box sliderBox(Hit h,int row){return {h,700,rowY(row)+1,1172,rowY(row)+45};}
// three pills a little wider (some languages' words for them are long)
constexpr Box pacingPill(Hit h,int row,int i){return {h,776+i*132,rowY(row)+7,902+i*132,rowY(row)+39};}
constexpr Box crouchOn=pillOn(Hit::CrouchOn,0),crouchOff=pillOff(Hit::CrouchOff,0);
constexpr Box dropSlider=sliderBox(Hit::DropSlider,1),swingSlider=sliderBox(Hit::SwingSlider,2);
constexpr Box swimOn=pillOn(Hit::SwimOn,3),swimOff=pillOff(Hit::SwimOff,3);
constexpr Box hitStopOn=pillOn(Hit::HitStopOn,4),hitStopOff=pillOff(Hit::HitStopOff,4);
constexpr Box recommendButton{Hit::Recommend,968,rowY(5)+7,1170,rowY(5)+39};   // one button the width of the two pills
constexpr Box pacingMax=pacingPill(Hit::PacingMax,6,0),pacingHalf=pacingPill(Hit::PacingHalf,6,1),pacingFill=pacingPill(Hit::PacingFill,6,2);
constexpr int rows=7,noteTop=rowY(rows)+8;
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
    if(open)for(const auto& b:{crouchOn,crouchOff,dropSlider,swingSlider,swimOn,swimOff,hitStopOn,hitStopOff,recommendButton,pacingMax,pacingHalf,pacingFill})out.push_back(b);
    return out;
}
inline Hit hitAt(int x,int y,bool open){for(const auto& b:boxes(open))if(b.contains(x,y))return b.hit;return Hit::None;}
// The texts, in Steam's game language (English for any other). A row's label is its heading, a line break, and a smaller
// explanation (two blocks: a heading and its note are drawn apart, each fitting its own height).
struct Texts {const wchar_t* button;const wchar_t* crouch;const wchar_t* on;const wchar_t* off;const wchar_t* drop;const wchar_t* note;const wchar_t* unsaved;
    const wchar_t* swing;const wchar_t* swim;   // 2026-10-20: the stroke's speed (a number, as drop) and swimming by the arms
    const wchar_t* hitStop;                     // 17:40: the hit stop
    const wchar_t* recommend;const wchar_t* recommendSet;const wchar_t* recommendDone;   // 18:50: the recommended graphics
    // 2026-10-23: the alternate eyes' pacing - its label, its three pills, and the note when "fill" waits for the next start
    const wchar_t* pacing;const wchar_t* pacingMax;const wchar_t* pacingHalf;const wchar_t* pacingFill;const wchar_t* pacingNext;};
// (2026-10-19, user: "the in-game settings in the game's language" - the languages of the VR bug report's button,
// include/evr/bug_report.hpp; English for any other.)
inline Texts texts(const std::string& language){
    if(language=="german")return {L"VR-Einstellungen",L"Echtes Ducken\nKopf senken zum Schleichen",L"Ein",L"Aus",L"Ducktiefe: %d%%\nWie weit unter der Augenhöhe im Stehen",
        L"Einstellungen gelten sofort und bleiben für den nächsten Start.\nAugenhöhe im Stehen = Höhe beim letzten Zurücksetzen der Sicht.",
        L"(Entwicklungsstart: gilt jetzt, wird nicht gespeichert)",
        L"Schwungtempo für Angriffe: %d%%\nNiedriger: schon ein leichterer Schwung greift an",L"Mit den Armen schwimmen\nIm Wasser mit offenen Händen paddeln (Daumen weg vom Stick)",L"Trefferstopp\nHand und Waffe halten beim Treffer kurz an",L"Empfohlene Grafik\nGrafik für VR auf diesem PC (ab dem nächsten Start)",L"Setzen",L"Vorgemerkt",L"AFW-Bildtakt\nNur AFW. Halb: feste 37,5 fps bei 75 Hz, kein Wechsel",L"Max",L"Halb",L"Füllen",L"(AFW-Bildtakt „Füllen“: ab dem nächsten Start)"};
    if(language=="french")return {L"Réglages VR",L"Accroupissement réel\nBaissez la tête pour vous faufiler",L"Oui",L"Non",L"Profondeur : %d%%\nDe combien sous la hauteur des yeux debout",
        L"Les réglages s'appliquent tout de suite et sont gardés au prochain lancement.\nHauteur des yeux debout = hauteur au dernier recentrage de la vue.",
        L"(lancement de développement : appliqué maintenant, non enregistré)",
        L"Vitesse du coup pour attaquer : %d%%\nPlus bas : un coup plus léger suffit",L"Nager avec les bras\nDans l'eau, brassez mains ouvertes (pouces hors des sticks)",L"Arrêt à l'impact\nLa main et l'arme s'arrêtent un instant au coup",L"Graphismes recommandés\nRéglages pour la VR sur ce PC (au prochain lancement)",L"Appliquer",L"Prévu",L"Cadence AFW\nAFW seulement. Moitié : 37,5 i/s fixes à 75 Hz, sans alternance",L"Max",L"Moitié",L"Combler",L"(Cadence AFW « Combler » : au prochain lancement)"};
    if(language=="spanish"||language=="latam")return {L"Ajustes de VR",L"Agacharse de verdad\nBaja la cabeza para ir en sigilo",L"Sí",L"No",L"Profundidad: %d%%\nCuánto por debajo de la altura de los ojos de pie",
        L"Los ajustes se aplican al instante y se guardan para el próximo inicio.\nAltura de los ojos de pie = la del último reinicio de la vista.",
        L"(inicio de desarrollo: se aplica ahora, no se guarda)",
        L"Velocidad del golpe para atacar: %d%%\nMenor: basta un golpe más suave",L"Nadar con los brazos\nEn el agua, brazadas con las manos abiertas (pulgares fuera del stick)",L"Pausa al impactar\nLa mano y el arma se detienen un instante al golpear",L"Gráficos recomendados\nAjustes para VR en este PC (desde el próximo inicio)",L"Aplicar",L"Listo",L"Ritmo de AFW\nSolo AFW. Mitad: 37,5 fps fijos a 75 Hz, sin cambios",L"Máx.",L"Mitad",L"Rellenar",L"(Ritmo de AFW «Rellenar»: desde el próximo inicio)"};
    if(language=="italian")return {L"Impostazioni VR",L"Accovacciamento reale\nAbbassa la testa per muoverti furtivo",L"Sì",L"No",L"Profondità: %d%%\nQuanto sotto l'altezza degli occhi in piedi",
        L"Le impostazioni valgono subito e restano al prossimo avvio.\nAltezza degli occhi in piedi = quella dell'ultimo ripristino della vista.",
        L"(avvio di sviluppo: vale ora, non viene salvato)",
        L"Velocità del colpo per attaccare: %d%%\nPiù bassa: basta un colpo più leggero",L"Nuotare con le braccia\nIn acqua, bracciate a mani aperte (pollici lontani dagli stick)",L"Pausa all'impatto\nMano e arma si fermano un attimo quando colpisci",L"Grafica consigliata\nImpostazioni per la VR su questo PC (dal prossimo avvio)",L"Applica",L"Pronto",L"Cadenza AFW\nSolo AFW. Metà: 37,5 fps fissi a 75 Hz, senza alternanza",L"Max",L"Metà",L"Riempi",L"(Cadenza AFW «Riempi»: dal prossimo avvio)"};
    if(language=="polish")return {L"Ustawienia VR",L"Prawdziwe kucanie\nOpuść głowę, aby się skradać",L"Wł.",L"Wył.",L"Głębokość: %d%%\nO ile poniżej wysokości oczu na stojąco",
        L"Ustawienia działają od razu i zostają na następne uruchomienie.\nWysokość oczu na stojąco = z ostatniego resetu widoku.",
        L"(uruchomienie deweloperskie: działa teraz, nie zapisano)",
        L"Szybkość zamachu do ataku: %d%%\nNiżej: wystarczy lżejszy zamach",L"Pływanie rękami\nW wodzie wiosłuj otwartymi dłońmi (kciuki z dala od drążków)",L"Zatrzymanie przy trafieniu\nRęka i broń na chwilę stają przy trafieniu",L"Zalecana grafika\nUstawienia dla VR na tym PC (od następnego uruchomienia)",L"Ustaw",L"Gotowe",L"Tempo klatek AFW\nTylko AFW. Połowa: stałe 37,5 kl./s przy 75 Hz, bez przeskoków",L"Maks.",L"Połowa",L"Wypełnij",L"(Tempo AFW „Wypełnij”: od następnego uruchomienia)"};
    if(language=="brazilian"||language=="portuguese")return {L"Configurações de VR",L"Agachar de verdade\nAbaixe a cabeça para ficar furtivo",L"Sim",L"Não",L"Profundidade: %d%%\nQuanto abaixo da altura dos olhos em pé",
        L"As configurações valem na hora e ficam para a próxima vez.\nAltura dos olhos em pé = a do último reajuste da visão.",
        L"(início de desenvolvimento: vale agora, não é salvo)",
        L"Velocidade do golpe para atacar: %d%%\nMenor: um golpe mais leve já ataca",L"Nadar com os braços\nNa água, braçadas com as mãos abertas (polegares fora do analógico)",L"Pausa no impacto\nA mão e a arma param um instante ao acertar",L"Gráficos recomendados\nAjustes para VR neste PC (a partir do próximo início)",L"Aplicar",L"Pronto",L"Ritmo do AFW\nSó AFW. Metade: 37,5 fps fixos a 75 Hz, sem alternar",L"Máx.",L"Metade",L"Preencher",L"(Ritmo do AFW \"Preencher\": a partir do próximo início)"};
    if(language=="russian")return {L"Настройки VR",L"Настоящее приседание\nОпустите голову, чтобы красться",L"Вкл",L"Выкл",L"Глубина: %d%%\nНасколько ниже высоты глаз стоя",
        L"Настройки действуют сразу и сохраняются до следующего запуска.\nВысота глаз стоя = при последнем сбросе вида.",
        L"(запуск для разработки: действует сейчас, не сохраняется)",
        L"Скорость взмаха для атаки: %d%%\nНиже: хватит и более лёгкого взмаха",L"Плавание руками\nВ воде гребите открытыми ладонями (большие пальцы не на стиках)",L"Стоп при ударе\nРука и оружие на миг замирают при попадании",L"Рекомендуемая графика\nНастройки для VR на этом ПК (со следующего запуска)",L"Задать",L"Готово",L"Темп кадров AFW\nТолько AFW. Половина: ровно 37,5 к/с при 75 Гц, без скачков",L"Макс",L"Половина",L"Интерп.",L"(Темп AFW «Интерп.»: со следующего запуска)"};
    if(language=="ukrainian")return {L"Налаштування VR",L"Справжнє присідання\nОпустіть голову, щоб скрадатися",L"Увімк",L"Вимк",L"Глибина: %d%%\nНаскільки нижче висоти очей стоячи",
        L"Налаштування діють одразу й зберігаються до наступного запуску.\nВисота очей стоячи = під час останнього скидання виду.",
        L"(запуск для розробки: діє зараз, не зберігається)",
        L"Швидкість замаху для атаки: %d%%\nНижче: вистачить і легшого замаху",L"Плавання руками\nУ воді гребіть відкритими долонями (великі пальці не на стиках)",L"Зупинка при ударі\nРука й зброя на мить завмирають при влучанні",L"Рекомендована графіка\nНалаштування для VR на цьому ПК (з наступного запуску)",L"Задати",L"Готово",L"Темп кадрів AFW\nЛише AFW. Половина: рівно 37,5 к/с при 75 Гц, без стрибків",L"Макс",L"Половина",L"Інтерп.",L"(Темп AFW «Інтерп.»: з наступного запуску)"};
    if(language=="schinese")return {L"VR 设置",L"真实下蹲\n低头即可潜行",L"开",L"关",L"下蹲深度：%d%%\n比站立时的眼睛高度低多少",
        L"设置立即生效，并保留到下次启动。\n站立时的眼睛高度 = 上次重置视角时的高度。",
        L"（开发用启动：仅本次生效，不保存）",
        L"攻击所需挥动速度：%d%%\n越低，越轻的挥动也能攻击",L"用手臂游泳\n在水中张开手划水即可前进（拇指离开摇杆）",L"命中停顿\n命中时手和武器短暂停住",L"推荐画质设置\n按本机为 VR 设置（下次启动时生效）",L"应用",L"已预约",L"AFW 帧节奏\n仅 AFW。减半：75 Hz 时固定 37.5 帧，不来回切换",L"最大",L"减半",L"补帧",L"（AFW 帧节奏“补帧”：下次启动时生效）"};
    if(language=="tchinese")return {L"VR 設定",L"真實蹲下\n低頭即可潛行",L"開",L"關",L"蹲下深度：%d%%\n比站立時的眼睛高度低多少",
        L"設定立即生效，並保留到下次啟動。\n站立時的眼睛高度 = 上次重設視角時的高度。",
        L"（開發用啟動：僅本次生效，不儲存）",
        L"攻擊所需揮動速度：%d%%\n越低，越輕的揮動也能攻擊",L"用手臂游泳\n在水中張開手划水即可前進（拇指離開搖桿）",L"命中停頓\n命中時手和武器短暫停住",L"推薦畫質設定\n依本機為 VR 設定（下次啟動時生效）",L"套用",L"已預約",L"AFW 影格節奏\n僅 AFW。減半：75 Hz 時固定 37.5 幀，不來回切換",L"最大",L"減半",L"補幀",L"（AFW 影格節奏「補幀」：下次啟動時生效）"};
    if(language=="koreana")return {L"VR 설정",L"실제 웅크리기\n머리를 숙이면 은신합니다",L"켜기",L"끄기",L"웅크림 깊이: %d%%\n서 있을 때 눈높이보다 얼마나 낮을 때",
        L"설정은 즉시 적용되고 다음 실행에도 유지됩니다.\n서 있을 때 눈높이 = 마지막으로 시점을 재설정한 높이.",
        L"(개발용 실행: 이번에만 적용, 저장 안 됨)",
        L"공격에 필요한 휘두르기 속도: %d%%\n낮을수록 가볍게 휘둘러도 공격합니다",L"팔로 수영\n물속에서 손을 펴고 물을 저으면 나아갑니다 (엄지는 스틱에서 떼기)",L"히트 스톱\n명중하면 손과 무기가 잠깐 멈춥니다",L"권장 그래픽 설정\n이 PC의 VR에 맞게 변경 (다음 실행부터)",L"적용",L"예약됨",L"AFW 프레임 간격\nAFW 전용. 절반: 75 Hz에서 37.5 fps 고정 (전환 없음)",L"최대",L"절반",L"보간",L"(AFW 프레임 간격 \"보간\": 다음 실행부터)"};
    if(language=="turkish")return {L"VR ayarları",L"Gerçek eğilme\nGizlenmek için başını eğ",L"Açık",L"Kapalı",L"Eğilme derinliği: %d%%\nAyaktaki göz yüksekliğinin ne kadar altında",
        L"Ayarlar hemen uygulanır ve sonraki açılışta da kalır.\nAyaktaki göz yüksekliği = son görüş sıfırlamasındaki yükseklik.",
        L"(geliştirme başlatması: şimdi uygulanır, kaydedilmez)",
        L"Saldırı için savurma hızı: %d%%\nDüşük: daha hafif bir savuruş da saldırır",L"Kollarla yüzme\nSuda açık ellerle kulaç at (başparmaklar çubuktan uzakta)",L"Vuruş duraklaması\nİsabet anında el ve silah bir an durur",L"Önerilen grafik\nBu PC'de VR için ayarlar (sonraki açılıştan itibaren)",L"Uygula",L"Hazır",L"AFW kare hızı\nYalnızca AFW. Yarım: 75 Hz'de sabit 37,5 fps, geçiş yok",L"Maks.",L"Yarım",L"Doldur",L"(AFW kare hızı \"Doldur\": sonraki açılıştan itibaren)"};
    if(language=="japanese")return {L"VR設定",L"リアルしゃがみ\n頭を下げると隠密になります",L"入",L"切",L"しゃがむ深さ：%d%%\n立った時の目の高さから何%%下がったら隠密か",
        L"設定はすぐに反映され、次の起動にも残ります。\n立った時の目の高さ＝最後に視点をリセットした時の高さです。",
        L"（開発用の起動：この回だけ反映、保存はしません）",
        L"攻撃に要る振りの速さ：%d%%\n低いほど軽い振りで攻撃が出ます",L"腕で泳ぐ\n水の中で手を開いて水をかくと進みます（親指はスティックから離す）",L"ヒットストップ\n当たった瞬間に手と武器が一瞬止まります",L"推奨グラフィック設定\nこの PC の VR 向けに変更（次の起動で反映）",L"変更",L"予約済み",L"AFW の表示間隔\nAFW の時だけ。半分＝75 Hz で 37.5 fps に固定（切り替わりなし）",L"最大",L"半分",L"補間",L"（AFW の表示間隔「補間」は次の起動から）"};
    return {L"VR settings",L"Real crouch\nLower your head to sneak",L"On",L"Off",L"Crouch depth: %d%%\nHow far below your standing eye height",
        L"Settings apply at once and are kept for the next start.\nStanding eye height = the height at your last view reset.",
        L"(development start: applies now, not saved)",
        L"Swing speed to attack: %d%%\nLower: a lighter swing attacks",L"Swim with your arms\nIn water, stroke with open hands (thumbs off the sticks)",L"Hit stop\nYour hand and weapon pause for a moment on a hit",L"Recommended graphics\nSet the game's graphics for VR on this PC (from the next start)",L"Set",L"Done",L"AFW frame pacing\nAFW only. Half: a steady 37.5 fps at 75 Hz, no switching",L"Max",L"Half",L"Fill",L"(AFW frame pacing \"Fill\": from the next start)"};
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
