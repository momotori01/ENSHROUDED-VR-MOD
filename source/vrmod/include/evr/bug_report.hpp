#pragma once
// The VR bug report (2026-10-15 evening, user: "before the 1.0 update brings a flood of bugs, put a bug report button
// into the start menu: after the menu is closed, 30 seconds of a detailed log and both eyes' screenshots, and the
// settings program's log zip with it, into a zip in the game's folder - the players send it with a short description").
// The pure parts: the texts in the player's language, the capture plan of the 30 seconds, and the rows of the Mock
// scripts (runtime/mock_actions.hpp, runtime/mock_runtime.cpp) the head and the hands of the window are written as, so
// that the player's own movements can be played again in a Mock session. The hooks' side is hooks/bug_report.hpp.
#include <evr/math.hpp>
#include <evr/motion_actions.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <string>
namespace evr::bugreport {
constexpr uint64_t windowMs=30000;   // "30 seconds" (the user): long enough to show a bug again on purpose
// The pictures of both eyes taken in the window (each one stops the frame: the user, 2026-10-18, "the 30 seconds after
// the press stutter"): one of full size at the start (small things far away), a run of six consecutive pairs early
// (flicker, one-frame faults; Mock game-20261018-132151: 8 frames a second for that half second, 52 elsewhere). The rest comes from the last seconds kept on the GPU (layer/native_xr.hpp, eye_history.hpp:
// two pairs a second, half size), written when the button is pressed - the 10 seconds before it - and when the window
// ends - its last 10 seconds. Until 2026-10-18 13:30 a single pair every 2.5 s (22 pairs, two of full size).
struct CaptureStep {uint64_t atMs;uint32_t pairs;bool full;};
constexpr std::array<CaptureStep,2> captureSteps{{{300,1,true},{1500,6,false}}};   // the late run of four went at 13:50: the last 10 s are written at the end
constexpr uint32_t plannedPairs(){uint32_t n=0;for(const auto& s:captureSteps)n+=s.pairs;return n;}

struct Texts {const wchar_t* title;const wchar_t* description;const wchar_t* armed;const wchar_t* recording;const wchar_t* making;const wchar_t* saved;const wchar_t* font;};
// Steam's game language (ISteamApps::GetCurrentGameLanguage: "japanese", "english", "schinese", ...). recording: one %d
// (the seconds left); saved: one %ls (the zip's name). English for a language not listed.
inline Texts texts(const std::string& language){
    if(language=="japanese")return {L"！VRバグ報告ボタン！",
        L"押す前の１０秒と、ゲームを再開した後の３０秒の\n両目のスクリーンショットと詳細なログが記録されます。\nenshrouded.exeの有るフォルダに、バグ報告.zipが生成されているので、\n\nバグの内容報告と一緒に送って下さい。\n\n※このログには個人情報は含まれません。\nユーザー名は<user><player-1>に変換されます。\nスクリーンショットに個人情報を含めないで下さい。",
        L"記録の準備ができました。\nメニューを閉じると３０秒間の記録が始まります。",L"● VRバグ報告：記録中　あと %d 秒",L"VRバグ報告：zip を作成中…",L"VRバグ報告を保存しました：%ls",L"Yu Gothic UI"};
    if(language=="german")return {L"! VR-FEHLERBERICHT !",
        L"Nach dem Drücken und dem Fortsetzen des Spiels werden 30 Sekunden lang\nScreenshots beider Augen und ein ausführliches Protokoll aufgezeichnet.\n\nIm Ordner von enshrouded.exe wird VR_bug_report_(Datum).zip erstellt.\nBitte sende diese Datei zusammen mit einer Beschreibung des Fehlers.\n\n※ Dieses Protokoll enthält keine persönlichen Daten.\nBenutzernamen werden in <user> <player-1> umgewandelt.\nBitte zeige in den Screenshots keine persönlichen Daten.",
        L"Bereit. Schließe das Menü, um die 30-Sekunden-Aufnahme zu starten.",L"● VR-Fehlerbericht: Aufnahme – noch %d s",L"VR-Fehlerbericht: ZIP wird erstellt ...",L"VR-Fehlerbericht gespeichert: %ls",L"Segoe UI"};
    if(language=="french")return {L"! RAPPORT DE BUG VR !",
        L"Après l'avoir pressé et repris la partie, pendant 30 secondes,\ndes captures des deux yeux et un journal détaillé sont enregistrés.\n\nLe fichier VR_bug_report_(date).zip est créé dans le dossier de enshrouded.exe.\nEnvoyez-le avec une description du bug.\n\n※ Ce journal ne contient aucune information personnelle.\nLes noms d'utilisateur sont convertis en <user> <player-1>.\nNe montrez pas d'informations personnelles dans les captures.",
        L"Prêt. Fermez le menu pour lancer l'enregistrement de 30 secondes.",L"● Rapport de bug VR : enregistrement – encore %d s",L"Rapport de bug VR : création du ZIP...",L"Rapport de bug VR enregistré : %ls",L"Segoe UI"};
    if(language=="spanish"||language=="latam")return {L"¡ INFORME DE ERRORES VR !",
        L"Tras pulsarlo y reanudar la partida, durante 30 segundos\nse graban capturas de ambos ojos y un registro detallado.\n\nSe creará VR_bug_report_(fecha).zip en la carpeta de enshrouded.exe.\nEnvíalo junto con una descripción del error.\n\n※ Este registro no contiene información personal.\nLos nombres de usuario se convierten en <user> <player-1>.\nNo incluyas información personal en las capturas.",
        L"Listo. Cierra el menú para empezar la grabación de 30 segundos.",L"● Informe de errores VR: grabando – quedan %d s",L"Informe de errores VR: creando el ZIP...",L"Informe de errores VR guardado: %ls",L"Segoe UI"};
    if(language=="italian")return {L"! SEGNALAZIONE BUG VR !",
        L"Dopo averlo premuto e aver ripreso il gioco, per 30 secondi\nvengono registrati screenshot di entrambi gli occhi e un registro dettagliato.\n\nNella cartella di enshrouded.exe viene creato VR_bug_report_(data).zip.\nInvialo insieme a una descrizione del bug.\n\n※ Questo registro non contiene informazioni personali.\nI nomi utente vengono convertiti in <user> <player-1>.\nNon includere informazioni personali negli screenshot.",
        L"Pronto. Chiudi il menu per avviare la registrazione di 30 secondi.",L"● Segnalazione bug VR: registrazione – mancano %d s",L"Segnalazione bug VR: creazione dello ZIP...",L"Segnalazione bug VR salvata: %ls",L"Segoe UI"};
    if(language=="polish")return {L"! ZGŁOŚ BŁĄD VR !",
        L"Po naciśnięciu i powrocie do gry przez 30 sekund\nzapisywane są zrzuty ekranu obu oczu i szczegółowy dziennik.\n\nW folderze enshrouded.exe zostanie utworzony plik VR_bug_report_(data).zip.\nWyślij go razem z opisem błędu.\n\n※ Ten dziennik nie zawiera danych osobowych.\nNazwy użytkowników są zamieniane na <user> <player-1>.\nNie umieszczaj danych osobowych na zrzutach ekranu.",
        L"Gotowe. Zamknij menu, aby rozpocząć 30-sekundowe nagranie.",L"● Zgłoszenie błędu VR: nagrywanie – zostało %d s",L"Zgłoszenie błędu VR: tworzenie pliku ZIP...",L"Zgłoszenie błędu VR zapisane: %ls",L"Segoe UI"};
    if(language=="brazilian"||language=="portuguese")return {L"! RELATAR BUG DE VR !",
        L"Depois de pressioná-lo e voltar ao jogo, durante 30 segundos\nsão gravadas capturas dos dois olhos e um registro detalhado.\n\nO arquivo VR_bug_report_(data).zip será criado na pasta do enshrouded.exe.\nEnvie-o junto com uma descrição do bug.\n\n※ Este registro não contém informações pessoais.\nOs nomes de usuário são convertidos em <user> <player-1>.\nNão inclua informações pessoais nas capturas de tela.",
        L"Pronto. Feche o menu para iniciar a gravação de 30 segundos.",L"● Relatório de bug de VR: gravando – faltam %d s",L"Relatório de bug de VR: criando o ZIP...",L"Relatório de bug de VR salvo: %ls",L"Segoe UI"};
    if(language=="russian")return {L"! ОТЧЁТ ОБ ОШИБКЕ VR !",
        L"После нажатия и возвращения в игру в течение 30 секунд\nзаписываются снимки экрана обоих глаз и подробный журнал.\n\nВ папке enshrouded.exe будет создан файл VR_bug_report_(дата).zip.\nОтправьте его вместе с описанием ошибки.\n\n※ Этот журнал не содержит личной информации.\nИмена пользователей преобразуются в <user> <player-1>.\nНе показывайте личную информацию на снимках экрана.",
        L"Готово. Закройте меню, чтобы начать 30-секундную запись.",L"● Отчёт об ошибке VR: запись — осталось %d с",L"Отчёт об ошибке VR: создание ZIP...",L"Отчёт об ошибке VR сохранён: %ls",L"Segoe UI"};
    if(language=="ukrainian")return {L"! ЗВІТ ПРО ПОМИЛКУ VR !",
        L"Після натискання та повернення до гри протягом 30 секунд\nзаписуються знімки екрана обох очей і докладний журнал.\n\nУ теці enshrouded.exe буде створено файл VR_bug_report_(дата).zip.\nНадішліть його разом з описом помилки.\n\n※ Цей журнал не містить особистої інформації.\nІмена користувачів перетворюються на <user> <player-1>.\nНе показуйте особисту інформацію на знімках екрана.",
        L"Готово. Закрийте меню, щоб почати 30-секундний запис.",L"● Звіт про помилку VR: запис — залишилось %d с",L"Звіт про помилку VR: створення ZIP...",L"Звіт про помилку VR збережено: %ls",L"Segoe UI"};
    if(language=="schinese")return {L"！VR 错误报告！",
        L"按下并返回游戏后，\n将记录 30 秒的双眼截图和详细日志。\n\nenshrouded.exe 所在文件夹中会生成 VR_bug_report_(日期).zip，\n请将其与错误说明一起发送。\n\n※ 此日志不包含个人信息。\n用户名会被转换为 <user> <player-1>。\n请勿在截图中包含个人信息。",
        L"已就绪。关闭菜单后开始 30 秒记录。",L"● VR 错误报告：记录中，剩余 %d 秒",L"VR 错误报告：正在生成 ZIP…",L"VR 错误报告已保存：%ls",L"Microsoft YaHei UI"};
    if(language=="tchinese")return {L"！VR 錯誤回報！",
        L"按下並返回遊戲後，\n將記錄 30 秒的雙眼截圖與詳細紀錄。\n\nenshrouded.exe 所在的資料夾中會產生 VR_bug_report_(日期).zip，\n請將其連同錯誤說明一起寄送。\n\n※ 此紀錄不包含個人資訊。\n使用者名稱會被轉換為 <user> <player-1>。\n請勿在截圖中包含個人資訊。",
        L"已就緒。關閉選單後開始 30 秒記錄。",L"● VR 錯誤回報：記錄中，剩餘 %d 秒",L"VR 錯誤回報：正在產生 ZIP…",L"VR 錯誤回報已儲存：%ls",L"Microsoft JhengHei UI"};
    if(language=="koreana")return {L"! VR 버그 신고 !",
        L"누른 뒤 게임으로 돌아가면 30초 동안\n양쪽 눈의 스크린샷과 자세한 로그가 기록됩니다.\n\nenshrouded.exe가 있는 폴더에 VR_bug_report_(날짜).zip이 생성됩니다.\n버그 설명과 함께 보내 주세요.\n\n※ 이 로그에는 개인정보가 포함되지 않습니다.\n사용자 이름은 <user> <player-1>로 변환됩니다.\n스크린샷에 개인정보가 포함되지 않도록 해 주세요.",
        L"준비되었습니다. 메뉴를 닫으면 30초 기록이 시작됩니다.",L"● VR 버그 신고: 기록 중 - %d초 남음",L"VR 버그 신고: ZIP 생성 중...",L"VR 버그 신고가 저장되었습니다: %ls",L"Malgun Gothic"};
    if(language=="turkish")return {L"! VR HATA BİLDİRİMİ !",
        L"Bastıktan ve oyuna döndükten sonra 30 saniye boyunca\nher iki gözün ekran görüntüleri ve ayrıntılı bir günlük kaydedilir.\n\nenshrouded.exe klasöründe VR_bug_report_(tarih).zip oluşturulur.\nHatanın açıklamasıyla birlikte gönderin.\n\n※ Bu günlük kişisel bilgi içermez.\nKullanıcı adları <user> <player-1> olarak dönüştürülür.\nEkran görüntülerinde kişisel bilgi göstermeyin.",
        L"Hazır. 30 saniyelik kaydı başlatmak için menüyü kapatın.",L"● VR hata bildirimi: kaydediliyor - %d sn kaldı",L"VR hata bildirimi: ZIP oluşturuluyor...",L"VR hata bildirimi kaydedildi: %ls",L"Segoe UI"};
    return {L"! VR BUG REPORT !",
        L"Screenshots of both eyes (the 10 seconds before you press this and 30 seconds\nafter you return to the game) and a detailed log are recorded.\nA file named VR_bug_report_(date).zip then appears in the folder of enshrouded.exe.\n\nPlease send it to us together with a short description of the bug.\n\n※ This log contains no personal information.\nUser names are replaced with <user> / <player-1>.\nPlease make sure the screenshots don't show any personal information.",
        L"Ready.\nThe 30-second recording starts when you close this menu.",L"● VR bug report: recording - %d s left",L"VR bug report: making the zip...",L"VR bug report saved: %ls",L"Segoe UI"};
}

// yaw, pitch, roll in degrees of q, inverse of math.hpp's euler (q = Ry(yaw) Rx(pitch) Rz(roll)).
inline std::array<float,3> eulerDegrees(Quat q){
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(!(n>0))return {0,0,0};
    q={q.x/n,q.y/n,q.z/n,q.w/n};
    // rotation matrix entries used: m02, m22, m12, m10, m11
    const float m02=2*(q.x*q.z+q.w*q.y),m22=1-2*(q.x*q.x+q.y*q.y),m12=2*(q.y*q.z-q.w*q.x),m10=2*(q.x*q.y+q.w*q.z),m11=1-2*(q.x*q.x+q.z*q.z);
    const float s=std::fmax(-1.f,std::fmin(1.f,-m12));constexpr float deg=57.29577951308232f;
    return {std::atan2(m02,m22)*deg,std::asin(s)*deg,std::atan2(m10,m11)*deg};
}
inline std::string number(float v){if(!std::isfinite(v))v=0;char b[32];std::snprintf(b,sizeof b,"%.5g",double(v));return b;}
// One row of the Mock hand script (runtime/mock_actions.hpp: per hand x y z yaw pitch roll, aim yaw pitch roll, trigger,
// grip, active, tracked; then focused, profile; sticks x y click; faces primary secondary). index from 1.
inline std::string mockHandRow(uint64_t index,const motion::HandFrame& f){
    std::string s=std::to_string(index);
    for(const auto& h:f.hands){
        const auto g=eulerDegrees(h.grip.orientation),a=eulerDegrees(h.aim.orientation);
        const float trigger=std::fmax(0.f,std::fmin(1.f,h.trigger));
        for(float v:{h.grip.position.x,h.grip.position.y,h.grip.position.z,g[0],g[1],g[2],a[0],a[1],a[2],trigger})s+=","+number(v);
        s+=std::string(",")+(h.grab?"1":"0")+","+(h.gripActive?"1":"0")+","+(h.tracked()?"1":"0");
    }
    s+=std::string(",")+(f.focused?"1":"0")+",0";
    for(const auto& h:f.hands)s+=","+number(std::fmax(-1.f,std::fmin(1.f,h.stick.x)))+","+number(std::fmax(-1.f,std::fmin(1.f,h.stick.y)))+","+(h.stickClick?"1":"0");
    for(const auto& h:f.hands)s+=std::string(",")+(h.primaryPressed?"1":"0")+","+(h.secondaryPressed?"1":"0");
    return s;
}
// One row of the Mock head script (runtime/mock_runtime.cpp: seconds, x y z, yaw pitch roll, ipd, tracked, connected,
// eye width, eye height[, the left and the right eye's fov: left right up down, radians]).
// 2026-10-18: the headset's own recommended size and eye fovs (before, 1963 and the Mock's optics): the terrain's detail
// depends on the picture's size - a replay at 1963 drew the field's soil alike in both eyes, the headset at 2804 did not.
inline std::string mockHeadRow(double seconds,const Pose& head,bool tracked,uint32_t width,uint32_t height,const std::array<float,8>* fov=nullptr){
    const auto e=eulerDegrees(head.orientation);char b[64];std::snprintf(b,sizeof b,"%.4f",seconds);
    std::string s=std::string(b)+","+number(head.position.x)+","+number(head.position.y)+","+number(head.position.z)+","+number(e[0])+","+number(e[1])+","+number(e[2])+
        ",0.064,"+(tracked?"1":"0")+",1,"+std::to_string(width?width:1963)+","+std::to_string(height?height:1963);
    if(fov){bool ok=true;
        for(int eye=0;eye<2;++eye){const float* f=fov->data()+eye*4;
            ok=ok&&std::isfinite(f[0])&&std::isfinite(f[1])&&std::isfinite(f[2])&&std::isfinite(f[3])&&f[0]<0&&f[1]>0&&f[2]>0&&f[3]<0&&f[0]>-1.5f&&f[1]<1.5f&&f[2]<1.5f&&f[3]>-1.5f;}
        if(ok)for(float f:*fov)s+=","+number(f);}
    return s;
}
}
