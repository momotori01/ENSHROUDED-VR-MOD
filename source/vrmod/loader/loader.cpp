// The mod's loader: dinput8.dll beside enshrouded.exe (the game takes DirectInput8Create from it; the call goes on to
// the system's library). User, 2026-10-14: "started from the Steam launcher as usual, the game starts with the
// settings of last time", "the package works by dropping its contents into the game's folder", "VR is off by
// default and switched on in the exe", "the other VR players' movements are seen with VR on or off".
//
// At the game's start it reads EnshroudedVR\EnshroudedVR.ini and does, inside the game's process, what the
// development scripts did around it:
//   VR=1  the environment of a VR start (the Vulkan layer, OpenXR, the eye settings), the game's render settings for
//         VR, then on a thread: wait for a world (EVR_AwaitWorld) -> the unused proof (EVR_ProbeWorldViewResolution)
//         -> the eyes (EVR_ProbeWorldNativeXr) with the package's profile - once per world that is entered.
//   VR=0  the game as it is, with the other players' VR bodies (EVR_OnlineFlat) unless [Online] Sync=0.
// Each of these runs in its own copy of the hook library (a library does one run). Everything is written to
// EnshroudedVR\logs (loader.log and one folder per start), which the setting tool packs for a problem report.
// A launch by the development scripts (EVR_LAYER_OUTPUT is set already) is left alone, as is any other program.
// The values are tested in include/evr/auto_start.hpp; here are the files, the environment and the threads.
#include <windows.h>
#include <shlobj.h>
#include <evr/auto_start.hpp>
#include <evr/recommended_graphics.hpp>
#include <evr/game_build.hpp>
#include <algorithm>
#include <string>
#include <vector>
namespace {
using evr::settings::Values;
namespace start = evr::autostart;
struct State {
    start::Paths paths;std::wstring logs,ini,session;Values values;std::string iniText;
    bool vr=false,flat=false,runtimeGiven=false;std::wstring activeRuntime;
} g;
SRWLOCK g_logLock=SRWLOCK_INIT;
std::string g_note;   // why VR was asked for and is not running, for the status line

std::wstring wide(const std::string& s){
    if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    std::wstring out(size_t(n>0?n:0),L'\0');if(n>0)MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),out.data(),n);return out;
}
std::string narrow(const std::wstring& s){
    if(s.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(size_t(n>0?n:0),'\0');if(n>0)WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),out.data(),n,nullptr,nullptr);return out;
}
bool exists(const std::wstring& path){return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}
bool readFile(const std::wstring& path,std::string& out){
    out.clear();
    const HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size{};bool ok=GetFileSizeEx(h,&size)&&size.QuadPart<=(64ll<<20);
    if(ok){out.resize(size_t(size.QuadPart));size_t at=0;
        while(at<out.size()){DWORD got=0;if(!ReadFile(h,out.data()+at,DWORD(std::min<size_t>(out.size()-at,1u<<20)),&got,nullptr)||!got){ok=false;break;}at+=got;}}
    CloseHandle(h);if(!ok)out.clear();return ok;
}
bool writeFile(const std::wstring& path,const std::string& bytes,bool append=false){
    const HANDLE h=CreateFileW(path.c_str(),append?FILE_APPEND_DATA:GENERIC_WRITE,FILE_SHARE_READ,nullptr,append?OPEN_ALWAYS:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    size_t at=0;bool ok=true;
    while(at<bytes.size()){DWORD put=0;if(!WriteFile(h,bytes.data()+at,DWORD(std::min<size_t>(bytes.size()-at,1u<<20)),&put,nullptr)||!put){ok=false;break;}at+=put;}
    CloseHandle(h);return ok;
}
std::string stamp(bool forName){
    SYSTEMTIME t{};GetLocalTime(&t);char b[40]{};
    wsprintfA(b,forName?"%04u%02u%02u-%02u%02u%02u":"%04u-%02u-%02u %02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return b;
}
// One line into EnshroudedVR\logs\loader.log (every start, kept) and into this start's folder once there is one.
void say(const std::string& text){
    const std::string line=stamp(false)+"  "+text+"\r\n";
    AcquireSRWLockExclusive(&g_logLock);
    if(!g.logs.empty()){writeFile(g.logs+L"\\loader.log",line,true);if(!g.session.empty())writeFile(g.session+L"\\loader.log",line,true);}
    ReleaseSRWLockExclusive(&g_logLock);
}
// What the setting tool shows as "last start".
void status(const std::string& text){say("status: "+text);if(!g.logs.empty())writeFile(g.logs+L"\\last-status.txt",stamp(false)+"  "+text+"\r\n");}
std::string lastError(){return "error "+std::to_string(GetLastError());}

// A variable for everything that is loaded into this process later. The C runtime the mod's libraries share
// (ucrtbase.dll) holds its own copy of the environment from the moment it was initialised - before this library, most
// likely - so the variable is given to it as well; loading it here makes sure it is initialised.
using PutEnvironment=int(__cdecl*)(const wchar_t*,const wchar_t*);
PutEnvironment g_put=nullptr;bool g_putLooked=false;
void setEnvironment(const std::wstring& name,const std::wstring& value){
    SetEnvironmentVariableW(name.c_str(),value.empty()?nullptr:value.c_str());
    if(!g_putLooked){g_putLooked=true;
        wchar_t system[MAX_PATH]{};const UINT n=GetSystemDirectoryW(system,MAX_PATH);
        if(n&&n<MAX_PATH-16)if(const HMODULE crt=LoadLibraryW((std::wstring(system)+L"\\ucrtbase.dll").c_str()))g_put=reinterpret_cast<PutEnvironment>(GetProcAddress(crt,"_wputenv_s"));}
    if(g_put)g_put(name.c_str(),value.c_str());
}
bool environmentHas(const wchar_t* name){SetLastError(0);return GetEnvironmentVariableW(name,nullptr,0)!=0||GetLastError()!=ERROR_ENVVAR_NOT_FOUND;}

// The game build the mod's code addresses belong to (the hook library asks the same of the loaded image).
bool supportedBuild(){
    const auto base=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(!base||dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>4096)return false;
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    return nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.TimeDateStamp==evr::gameBuild::timeDateStamp&&nt->OptionalHeader.SizeOfImage==evr::gameBuild::sizeOfImage;
}
std::wstring registryText(HKEY root,const wchar_t* key,const wchar_t* name){
    wchar_t value[1024]{};DWORD bytes=sizeof value-sizeof(wchar_t);
    return RegGetValueW(root,key,name,RRF_RT_REG_SZ,nullptr,value,&bytes)==ERROR_SUCCESS?std::wstring(value):std::wstring();
}
bool onSearchPath(const wchar_t* name){wchar_t found[MAX_PATH]{};return SearchPathW(nullptr,name,nullptr,MAX_PATH,found,nullptr)!=0;}

// ---- the game's render settings for VR: changed at a VR start, put back at the next start without VR (the game
// writes the file when it closes, so not before). Since 1.0 (2026-10-16) the game reads them from Saved Games\keen
// games\Enshrouded\local_settings.json; the game folder's enshrouded_local.json (before 1.0) is used only where that
// file is not there (Mock run game-20261016-063118: DLSS written to the old file, the game drew FSR3) ----
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")
std::wstring settingsPath(){
    PWSTR saved=nullptr;std::wstring path;
    if(SHGetKnownFolderPath(FOLDERID_SavedGames,0,nullptr,&saved)==S_OK&&saved)path=std::wstring(saved)+L"\\keen games\\Enshrouded\\local_settings.json";
    CoTaskMemFree(saved);
    if(!path.empty()&&GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)return path;
    return g.paths.game+L"\\enshrouded_local.json";
}
std::wstring recordPath(){return g.paths.mod+L"\\render-settings-before-vr.txt";}
bool replaceFile(const std::wstring& path,const std::string& bytes){
    const std::wstring temporary=path+L".evr-new";
    if(!writeFile(temporary,bytes))return false;
    if(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING))return true;
    DeleteFileW(temporary.c_str());return false;
}
void applyRenderSettings(){
    std::string text,recordBytes,out,reason;start::RenderRecord earlier,record;
    if(!readFile(settingsPath(),text)){say("render settings: the game's settings file is not there yet (first start of the game?); HDR is not turned off for this start");return;}
    const bool hadRecord=readFile(recordPath(),recordBytes)&&start::parseRecord(recordBytes,earlier);
    // HDR off and the upscaler DLSS (2026-10-25, user: "the recommended button writes every item; HDR at each start" and
    // "nothing better than FSR 3 and DLSS? then set that by itself - Quality, Performance and so on stay free"): the DLSS /
    // FSR 3 mode and everything else stay the player's own; a card without DLSS gets FSR 3 from the game itself, with the
    // player's FSR 3 preset. (No AFW mode any more - AfwDlss is not set.)
    if(!start::applyVr(text,std::string(),hadRecord?&earlier:nullptr,out,record,reason,std::string(),true)){say("render settings: not changed - "+reason);return;}
    std::string list;for(const auto& c:record.changes)list+=" "+c.name+" "+c.original+"->"+c.applied;
    if(out==text&&hadRecord){say("render settings: already as VR wants them ("+list+" )");return;}
    if(!hadRecord)CopyFileW(settingsPath().c_str(),(g.paths.mod+L"\\enshrouded_local.before-vr.json").c_str(),FALSE);
    if(!writeFile(recordPath(),start::recordText(record))){say("render settings: the record could not be written, nothing changed");return;}
    if(out!=text&&!replaceFile(settingsPath(),out)){say("render settings: the game's settings file could not be written ("+lastError()+")");return;}
    say("render settings for VR:"+list+" (put back at the next start without VR)");
}
// The recommended graphics asked in the game's start menu (include/evr/recommended_graphics.hpp, 2026-10-20): written
// into the game's settings here, before the game reads them, once. The card's video memory came with the request (no
// DXGI here - this runs inside DllMain).
void applyRecommendedGraphics(){
    if(!evr::recommend::pending(g.iniText))return;
    const auto r=evr::recommend::recommend(evr::recommend::pendingMachine(g.iniText));
    std::string text;
    if(!readFile(settingsPath(),text)){say("recommended graphics: the game's settings file is not there; asked again at the next start");return;}
    evr::recommend::Applied applied;const auto out=evr::recommend::applyGame(text,r,applied);
    if(out!=text&&!replaceFile(settingsPath(),out)){say("recommended graphics: the game's settings file could not be written ("+lastError()+"); asked again at the next start");return;}
    // What is written here is the player's choice now (2026-10-25): an earlier VR start's record of it (an older version set
    // DLSS and GI at each VR start, include/evr/auto_start.hpp) must not take it back - neither at the VR start below nor
    // at a start without VR.
    {std::string bytes;start::RenderRecord kept;
     if(readFile(recordPath(),bytes)&&start::parseRecord(bytes,kept)&&evr::recommend::dropFromRecord(kept,r)&&!writeFile(recordPath(),start::recordText(kept)))
        say("recommended graphics: the record of the last VR start could not be written");}
    const auto ini=evr::recommend::setPending(g.iniText,false);
    if(writeFile(g.ini,ini))g.iniText=ini;else say("recommended graphics: the request could not be cleared in "+narrow(g.ini));
    std::string list;for(const auto& c:applied.changed)list+=" "+c;for(const auto& m:applied.missing)list+=" (not in the file: "+m+")";
    say("recommended graphics ("+r.tier+" of video memory) into the game's settings:"+(list.empty()?std::string(" already so"):list));
}
void restoreRenderSettings(){
    std::string recordBytes,text;start::RenderRecord record;
    if(!readFile(recordPath(),recordBytes))return;
    if(!start::parseRecord(recordBytes,record)){say("render settings: the record of the last VR start is unreadable, left as it is");return;}
    if(!readFile(settingsPath(),text)){say("render settings: the game's settings file could not be read, nothing put back");return;}
    std::vector<std::string> restored;const auto out=start::restore(text,record,&restored);
    if(out!=text&&!replaceFile(settingsPath(),out)){say("render settings: could not be put back ("+lastError()+")");return;}
    DeleteFileW(recordPath().c_str());
    std::string list;for(const auto& name:restored)list+=" "+name;
    say("render settings put back after VR:"+(list.empty()?std::string(" nothing (changed in the game since)"):list));
}

// ---- runs of the hook library ----
LONG g_copies=0;
// One export of a fresh copy of the hook library, with <directory> as its place for policies and logs.
DWORD run(const char* name,const std::wstring& directory){
    const LONG n=InterlockedIncrement(&g_copies);
    const std::wstring folder=g.paths.mod+L"\\run",copy=folder+L"\\evr_hooks_"+std::to_wstring(n)+L".dll";
    CreateDirectoryW(folder.c_str(),nullptr);
    if(!CopyFileW((g.paths.bin+L"\\evr_hooks.dll").c_str(),copy.c_str(),FALSE)){say(std::string(name)+": the hook library could not be copied ("+lastError()+")");return 0xfffffff0u;}
    const HMODULE library=LoadLibraryW(copy.c_str());
    if(!library){const DWORD code=GetLastError();
        say(std::string(name)+": the hook library could not be loaded (error "+std::to_string(code)+(code==ERROR_MOD_NOT_FOUND?": the Microsoft Visual C++ x64 runtime may be missing)":")"));return 0xfffffff1u;}
    const auto entry=reinterpret_cast<DWORD(WINAPI*)(void*)>(GetProcAddress(library,name));
    if(!entry){say(std::string(name)+": not in the hook library (old package?)");return 0xfffffff2u;}
    say(std::string(name)+" begins in "+narrow(directory.substr(g.logs.size()+1)));
    const DWORD result=entry(const_cast<wchar_t*>(directory.c_str()));
    say(std::string(name)+" returned "+std::to_string(result));
    return result;
}
std::wstring stage(const std::wstring& name){const std::wstring path=g.session+L"\\"+name;CreateDirectoryW(path.c_str(),nullptr);return path;}
bool put(const std::wstring& folder,const std::string& name,const std::string& bytes){return writeFile(folder+L"\\"+wide(name),bytes);}
// The package's profile: every policy file of the eyes that does not depend on the settings or on the world. A pad style
// (2026-10-25, [Mode] Style) leaves the motion controllers' files out (auto_start.hpp motionProfileFile).
bool copyProfile(const std::wstring& to){
    const bool pad=evr::settings::padStyle(g.values);
    const std::wstring from=g.paths.data+L"\\profile";WIN32_FIND_DATAW found{};unsigned files=0;bool ok=true;
    const HANDLE search=FindFirstFileW((from+L"\\*").c_str(),&found);if(search==INVALID_HANDLE_VALUE)return false;
    do{if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        if(pad&&start::motionProfileFile(narrow(found.cFileName)))continue;
        ok=CopyFileW((from+L"\\"+found.cFileName).c_str(),(to+L"\\"+found.cFileName).c_str(),FALSE)&&ok;++files;}while(FindNextFileW(search,&found));
    FindClose(search);return ok&&files>=20;
}
// The game reads the controller only while its window has the focus, and the player wears the headset.
BOOL CALLBACK findWindow(HWND window,LPARAM out){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId()||!IsWindowVisible(window)||GetWindow(window,GW_OWNER))return TRUE;
    *reinterpret_cast<HWND*>(out)=window;return FALSE;
}
DWORD WINAPI focusLater(void*){
    Sleep(12000);HWND window=nullptr;EnumWindows(findWindow,reinterpret_cast<LPARAM>(&window));
    if(!window){say("focus: the game's window was not found");return 0;}
    if(GetForegroundWindow()==window){say("focus: the game's window is in front");return 0;}
    if(!SetForegroundWindow(window)){
        // Windows lets only the foreground process hand the focus on; a released ALT tap satisfies the rule.
        keybd_event(VK_MENU,0,0,0);keybd_event(VK_MENU,0,KEYEVENTF_KEYUP,0);SetForegroundWindow(window);}
    Sleep(300);say(std::string("focus: the game's window was brought to the front: ")+(GetForegroundWindow()==window?"yes":"no (click the game's window once)"));
    return 0;
}
// Folders of earlier starts beyond the newest [Log] Keep go (0 keeps all); only what this loader made is touched:
// folders named yyyymmdd-hhmmss directly in EnshroudedVR\logs.
bool stampName(const wchar_t* n){for(int i=0;i<15;i++){if(i==8){if(n[i]!=L'-')return false;}else if(n[i]<L'0'||n[i]>L'9')return false;}return n[15]==0;}
void removeTree(const std::wstring& folder,int depth){
    if(depth>3)return;WIN32_FIND_DATAW found{};const HANDLE search=FindFirstFileW((folder+L"\\*").c_str(),&found);
    if(search!=INVALID_HANDLE_VALUE){
        do{const std::wstring name=found.cFileName;if(name==L"."||name==L"..")continue;const std::wstring path=folder+L"\\"+name;
            if(found.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)continue;
            if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)removeTree(path,depth+1);else DeleteFileW(path.c_str());}while(FindNextFileW(search,&found));
        FindClose(search);}
    RemoveDirectoryW(folder.c_str());
}
void pruneLogs(){
    if(g.values.keepLogs<=0)return;std::vector<std::wstring> names;WIN32_FIND_DATAW found{};
    const HANDLE search=FindFirstFileW((g.logs+L"\\*").c_str(),&found);if(search==INVALID_HANDLE_VALUE)return;
    do{if((found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(found.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&stampName(found.cFileName))names.push_back(found.cFileName);}while(FindNextFileW(search,&found));
    FindClose(search);std::sort(names.rbegin(),names.rend());
    for(size_t i=size_t(g.values.keepLogs);i<names.size();i++){removeTree(g.logs+L"\\"+names[i],0);say("log folder of an earlier start removed: "+narrow(names[i])+" ([Log] Keep="+std::to_string(g.values.keepLogs)+")");}
}

// The other players' VR bodies without VR, until the game closes.
void runFlat(const wchar_t* folder){
    const auto directory=stage(folder);std::string curl,pose;
    put(directory,"online-flat.txt","EVR_ONLINE_FLAT_1 0\n");
    if(readFile(g.paths.data+L"\\profile\\world-hand-curl.txt",curl)&&readFile(g.paths.data+L"\\profile\\hand-curl-closed-pose.bin",pose)){put(directory,"world-hand-curl.txt",curl);put(directory,"hand-curl-closed-pose.bin",pose);}
    status("Normal mode: the other players' VR bodies are shown"+(g_note.empty()?std::string():" - VR is not running: "+g_note));
    const DWORD result=run("EVR_OnlineFlat",directory);
    status(result==0?"Normal mode: ended":"Normal mode: the other players' VR bodies stopped (result "+std::to_string(result)+", see the log)");
}
DWORD WINAPI flatWorker(void*){
    pruneLogs();Sleep(5000);runFlat(L"flat");return 0;
}
DWORD WINAPI vrWorker(void*){
    pruneLogs();Sleep(5000);
    bool eyesRan=false;std::string why;
    // The layer prepares OpenXR when the game makes its Vulkan device, in the first seconds. Without a runtime or a
    // headset the game goes on as it is; that is said now, and the other players' bodies are shown instead.
    const char* hint=" - is the PC VR runtime (SteamVR) running and the headset connected? Start it first, then the game";
    for(unsigned n=0;n<90&&why.empty();n++){
        std::string layer,reason;readFile(g.session+L"\\vulkan.jsonl",layer);
        const int prepared=start::xrPrepared(layer,reason);
        if(prepared==1){say("OpenXR is prepared for the eyes");break;}
        if(prepared==0)why=reason+hint;else Sleep(1000);
    }
    for(unsigned world=1;why.empty()&&world<=12;world++){
        const std::wstring number=std::to_wstring(world);
        // 1. A world that is drawn, with the local player's camera.
        status(world==1?"VR: the game's picture is on the plate in the headset; waiting for a world (choose one in the menu with the controllers)":"VR: waiting for the next world");
        // The eyes start as soon as the world and the player's camera are there (user, 2026-10-14: "30 seconds after
        // entering is too late and takes the immersion away"). Until their first pair the headset shows the game's
        // picture on the plate (layer/screen_xr.hpp) - the menu, the loading, and the world's first seconds, in which
        // the eyes' Views warm up (300 frames of the game's own) and the player's height settles.
        const auto await=stage(L"world-"+number+L"-await");put(await,"await-world.txt","EVR_AWAIT_WORLD_1 2\n");
        if(const DWORD r=run("EVR_AwaitWorld",await)){why="waiting for a world failed (result "+std::to_string(r)+")";break;}
        std::string found,scene,view;
        if(!readFile(await+L"\\world-found.txt",found)||!start::worldFound(found,scene,view)){why="the world that was found could not be read";break;}
        {std::string layer,reason;readFile(g.session+L"\\vulkan.jsonl",layer);
            if(start::xrPrepared(layer,reason)!=1){why=(reason.empty()?std::string("the mod's Vulkan layer did not prepare OpenXR (see vulkan.jsonl)"):reason)+hint;break;}}
        // 2. The unused proof (two Views of this world made and destroyed, nothing drawn with them) is the development
        // sessions' step before the eyes; it takes some ten seconds with its pause. Here it runs only when asked for
        // ([Environment] EVR_AUTO_PROOF=1, for looking into a problem): the eyes' session makes the same things itself.
        if(environmentHas(L"EVR_AUTO_PROOF")){
            const auto proof=stage(L"world-"+number+L"-proof");
            put(proof,"world-culling-history.txt","EVR_CULLING_HISTORY_1\n");put(proof,"world-water-feedback.txt","EVR_WATER_FEEDBACK_1\n");
            if(const DWORD r=run("EVR_ProbeWorldViewResolution",proof)){why="the unused proof failed (result "+std::to_string(r)+")";break;}
            std::string log,reason;
            if(!readFile(proof+L"\\view-resource-probe.jsonl",log)||!start::proofAccepted(log,scene,view,reason)){why="the unused proof was not accepted: "+(reason.empty()?std::string("its log could not be read"):reason);break;}
            Sleep(6000);
        }
        // 3. The eyes, until this world is left.
        const auto eyes=stage(L"world-"+number+L"-vr");
        if(!copyProfile(eyes)){why="the package's profile (EnshroudedVR\\data\\profile) is incomplete";break;}
        bool written=true;for(const auto& file:start::sessionFiles(g.values,scene,view))written=put(eyes,file.first,file.second)&&written;
        if(!written){why="the session's files could not be written";break;}
        status("VR: running (world "+std::to_string(world)+")");
        if(const HANDLE focus=CreateThread(nullptr,0,focusLater,nullptr,0,nullptr))CloseHandle(focus);
        eyesRan=true;
        if(const DWORD r=run("EVR_ProbeWorldNativeXr",eyes)){why="the VR session ended with result "+std::to_string(r)+" (see world-"+std::to_string(world)+"-vr\\view-resource-probe.jsonl)";break;}
        status("VR: the world was left");
        Sleep(3000);
    }
    if(why.empty())why="the limit of worlds per start was reached; start the game again";
    status("VR: stopped - "+why);if(!eyesRan)g_note=why;
    // The others' bodies are still shown when VR never got to draw; after a session that ended badly nothing more is hooked.
    if(!eyesRan&&g.values.online)runFlat(L"flat-after-vr");
    return 0;
}

void begin(){
    wchar_t exe[MAX_PATH*2]{};const DWORD length=GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));
    if(!length||length>=std::size(exe))return;
    const std::wstring path=exe;const auto slash=path.find_last_of(L"\\/");if(slash==std::wstring::npos)return;
    if(_wcsicmp(path.c_str()+slash+1,L"enshrouded.exe"))return;                 // any other program: only the forwarding
    g.paths.game=path.substr(0,slash);g.paths.mod=g.paths.game+L"\\EnshroudedVR";g.paths.bin=g.paths.mod+L"\\bin";g.paths.data=g.paths.mod+L"\\data";
    if(!exists(g.paths.mod))return;                                             // the library alone, without the mod's folder
    if(environmentHas(L"EVR_LAYER_OUTPUT"))return;                              // a launch by the development scripts
    g.logs=g.paths.mod+L"\\logs";CreateDirectoryW(g.logs.c_str(),nullptr);
    {WIN32_FILE_ATTRIBUTE_DATA info{};const std::wstring log=g.logs+L"\\loader.log";   // loader.log starts over beyond 1 MB
        if(GetFileAttributesExW(log.c_str(),GetFileExInfoStandard,&info)&&(info.nFileSizeHigh||info.nFileSizeLow>(1u<<20)))MoveFileExW(log.c_str(),(g.logs+L"\\loader.old.log").c_str(),MOVEFILE_REPLACE_EXISTING);}
    std::string version;readFile(g.paths.mod+L"\\version.txt",version);
    say("---- game start; Enshrouded VR "+(version.empty()?std::string("(no version.txt)"):evr::settings::trim(version))+"; game folder "+narrow(g.paths.game));
    // The settings: EVR_AUTO_INI names another file (tests); a missing file is made from the package's defaults.
    wchar_t other[MAX_PATH*2]{};const DWORD given=GetEnvironmentVariableW(L"EVR_AUTO_INI",other,DWORD(std::size(other)));
    g.ini=given&&given<std::size(other)?std::wstring(other):g.paths.mod+L"\\EnshroudedVR.ini";
    if(!exists(g.ini)&&!given)CopyFileW((g.paths.mod+L"\\EnshroudedVR.defaults.ini").c_str(),g.ini.c_str(),TRUE);
    if(!readFile(g.ini,g.iniText))say("settings: "+narrow(g.ini)+" is not there; the defaults are used (VR off)");
    g.values=evr::settings::forStart(evr::settings::read(g.iniText));   // with the alternate eyes, their own picture (2026-10-23)
    say("settings: VR="+std::to_string(int(g.values.vr))+" Sync="+std::to_string(int(g.values.online))+" RenderScale="+evr::settings::text(g.values.renderScale)+" Dlss="+g.values.dlss+
        " Sharpness="+evr::settings::text(g.values.sharpness)+" GiQuality="+g.values.giQuality+" LogKeep="+std::to_string(g.values.keepLogs)+" RealCrouch="+std::to_string(int(g.values.realCrouch))+" CrouchDrop="+std::to_string(g.values.crouchDrop)+" SwingSpeed="+std::to_string(g.values.swingSpeed)+" ArmSwim="+std::to_string(int(g.values.armSwim))+" UiAutoHide="+std::to_string(int(g.values.uiAutoHide))+" AlternateEyes="+std::to_string(int(g.values.alternateEyes))+" AfwPacing="+std::to_string(g.values.afwPacing)+" FitView="+evr::settings::fitViewNames()[size_t(g.values.fitView)]+(given?" (from EVR_AUTO_INI)":""));
    say("settings: Style="+evr::settings::styleNames()[size_t(g.values.style)]+" PitchLock="+std::to_string(int(g.values.padPitchLock))+" Tilt="+std::to_string(g.values.padTilt));
    if(!supportedBuild()){restoreRenderSettings();status("Off: this version of the game is not the one the mod was made for (the game was updated?); the game runs without the mod");return;}
    applyRecommendedGraphics();
    const bool hooks=exists(g.paths.bin+L"\\evr_hooks.dll");
    const bool runtime=onSearchPath(L"vcruntime140.dll")&&onSearchPath(L"vcruntime140_1.dll")&&onSearchPath(L"msvcp140.dll");
    g.vr=g.values.vr;std::string whyNot;
    if(g.vr){
        const auto overrides=evr::settings::entries(g.iniText,"Environment");
        g.runtimeGiven=environmentHas(L"XR_RUNTIME_JSON");
        for(const auto& entry:overrides)if(entry.first=="XR_RUNTIME_JSON"&&!entry.second.empty())g.runtimeGiven=true;
        const auto active=registryText(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Khronos\\OpenXR\\1",L"ActiveRuntime");g.activeRuntime=active;
        if(!hooks||!exists(g.paths.bin+L"\\evr_diagnostic.dll")||!exists(g.paths.bin+L"\\evr_layer.json")||!exists(g.paths.bin+L"\\openxr_loader.dll")||!exists(g.paths.data+L"\\profile\\world-early-xr.txt"))
            whyNot="files of the package are missing in EnshroudedVR\\bin or EnshroudedVR\\data (extract the whole package again)";
        else if(!runtime)whyNot="the Microsoft Visual C++ x64 runtime is missing (install https://aka.ms/vs/17/release/vc_redist.x64.exe)";
        else if(!g.runtimeGiven&&active.empty())whyNot="no OpenXR runtime is set on this PC (set SteamVR as the OpenXR runtime in its settings)";
        else say("OpenXR runtime: "+(g.runtimeGiven?std::string("given by XR_RUNTIME_JSON"):narrow(active)));
        if(!whyNot.empty()){g.vr=false;say("VR is not started: "+whyNot);}
    }
    g.flat=!g.vr&&g.values.online&&hooks&&runtime;
    if(!g.vr)restoreRenderSettings();
    if(!g.vr&&!g.flat){status(!whyNot.empty()?"Off: VR could not be started - "+whyNot:g.values.online?"Off: the hook library or the Visual C++ runtime is missing":"Off: VR is off and [Online] Sync=0");return;}
    g.session=g.logs+L"\\"+wide(stamp(true));CreateDirectoryW(g.session.c_str(),nullptr);g.paths.log=g.session;
    say("this start's folder: logs\\"+narrow(g.session.substr(g.logs.size()+1)));
    g_note=whyNot;
    if(g.vr){
        applyRenderSettings();
        start::Shaders shaders;std::string list;
        if(readFile(g.paths.data+L"\\shaders.txt",list))for(const auto& line:evr::settings::lines(list)){const auto t=evr::settings::trim(line.text);const auto space=t.find(' ');if(space==std::string::npos)continue;
            const auto name=t.substr(0,space),hash=evr::settings::trim(t.substr(space+1));if(hash.size()!=64)continue;
            if(name=="shadow_receiver"&&exists(g.paths.data+L"\\shadow_receiver.spv"))shaders.shadowReceiver=wide(hash);
            if(name=="gi_probe"&&exists(g.paths.data+L"\\gi_probe.spv"))shaders.giProbe=wide(hash);}
        for(const auto& variable:start::vrEnvironment(g.paths,shaders,g.values.online,g.values.renderScale,g.values.alternateEyes,g.values.eyeWarp,g.values.afwPacing))setEnvironment(variable.first,variable.second);
        // Real crouching (2026-10-18): how it begins, and the file the game's start menu (VR settings) writes its changes into.
        setEnvironment(L"EVR_ROOM_SNEAK",g.values.realCrouch?L"":L"0");setEnvironment(L"EVR_CROUCH_DROP",std::to_wstring(g.values.crouchDrop));setEnvironment(L"EVR_SETTINGS_INI",g.ini);
        // The stroke's speed and swimming by the arms (2026-10-20), changeable there too.
        setEnvironment(L"EVR_SWING_SPEED",std::to_wstring(g.values.swingSpeed));setEnvironment(L"EVR_ARM_SWIM",g.values.armSwim?L"":L"0");setEnvironment(L"EVR_HIT_STOP",g.values.hitStop?L"":L"0");
        setEnvironment(L"EVR_UI_AUTO_HIDE",g.values.uiAutoHide?L"":L"0");   // [Picture] UiAutoHide (2026-10-23)
        // [Picture] FitView (2026-10-25): Fit 1, Normal 0, Auto none (the client finds ALVR itself)
        setEnvironment(L"EVR_FIT_RUNTIME_FOV",g.values.fitView==evr::settings::FitView::Fit?L"1":g.values.fitView==evr::settings::FitView::Normal?L"0":L"");
        // A pad style (2026-10-25): the native body shown but the head, nothing of the motion controllers.
        if(evr::settings::padStyle(g.values))for(const auto& variable:start::padEnvironment())setEnvironment(variable.first,variable.second);
        // The mod's Vulkan layer prepares OpenXR only with the runtime named by an absolute path (layer/xr_preflight.hpp).
        // Without XR_RUNTIME_JSON it is the PC's own active runtime, named here. First headset start from Steam,
        // 2026-10-14 12:39: "preflight requires explicit absolute runtime/loader paths" - the Mock tests had always
        // named their runtime.
        if(!g.runtimeGiven&&!g.activeRuntime.empty()){setEnvironment(L"XR_RUNTIME_JSON",g.activeRuntime);say("XR_RUNTIME_JSON set to the PC's active OpenXR runtime");}
        say(std::string("environment of a VR start set (shadow receiver shader ")+(shaders.shadowReceiver.empty()?"no":"yes")+", GI probe shader "+(shaders.giProbe.empty()?"no":"yes")+", C runtime copy "+(g_put?"yes":"no")+")");
    }
    // [Environment] of the settings file, on top (tests, looking into a problem).
    for(const auto& entry:evr::settings::entries(g.iniText,"Environment")){
        if(!start::overridable(entry.first)){say("[Environment] "+entry.first+" is not one of the mod's switches; left out");continue;}
        setEnvironment(wide(entry.first),start::expanded(wide(entry.second),g.paths));say("[Environment] "+entry.first+"="+entry.second);}
    const HANDLE thread=CreateThread(nullptr,0,g.vr?vrWorker:flatWorker,nullptr,0,nullptr);
    if(thread)CloseHandle(thread);else status("Off: the loader's thread could not be started ("+lastError()+")");
}
} // namespace

// The game's own import, handed on to the system's library.
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance,DWORD version,const IID& iid,void** out,IUnknown* outer){
    using Create=HRESULT(WINAPI*)(HINSTANCE,DWORD,const IID&,void**,IUnknown*);
    static Create real=nullptr;static SRWLOCK lock=SRWLOCK_INIT;
    AcquireSRWLockExclusive(&lock);
    if(!real){wchar_t system[MAX_PATH]{};const UINT n=GetSystemDirectoryW(system,MAX_PATH);
        if(n&&n<MAX_PATH-16)if(const HMODULE library=LoadLibraryW((std::wstring(system)+L"\\dinput8.dll").c_str()))real=reinterpret_cast<Create>(GetProcAddress(library,"DirectInput8Create"));}
    const Create call=real;ReleaseSRWLockExclusive(&lock);
    return call?call(instance,version,iid,out,outer):E_FAIL;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);begin();}
    return TRUE;
}
