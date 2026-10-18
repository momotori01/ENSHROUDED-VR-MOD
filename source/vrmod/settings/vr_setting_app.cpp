// "Enshrouded VR setting.exe" (user, 2026-10-14: "make a VRsetting.exe as EDF6VR has; VR mode is switched on and off
// there" / "no Start the game: started from the Steam launcher as usual, the game starts with the settings of last
// time; this exe is only for the settings that cannot be changed in the game" / "give the exe the making of a log zip
// for bug reports, see EDF6").
// One window, after "EDF6 VR setting.exe" of the same author: each row has its name and a short note on the left and
// its controls on the right; a change is written at once into EnshroudedVR\EnshroudedVR.ini (only that value's line
// changes, include/evr/vr_settings.hpp). The mod's loader (dinput8.dll) reads the file when the game starts.
// Nothing is changed while the game runs. The program belongs into the game's folder, next to enshrouded.exe.
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>
#include <cctype>
#include <cstring>
#include <ctime>
#include <evr/vr_settings.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
namespace {
using namespace evr::settings;
HINSTANCE g_instance;HWND g_window;HFONT g_font,g_bold;int g_dpi=96,g_y=0;
std::wstring g_folder;                       // the game's folder (where this program is)
std::string g_ini;                           // EnshroudedVR.ini as read
bool g_running=false,g_loading=false,g_zipping=false,g_installed=false,g_updating=false;
void refreshUpdateButton();
std::vector<HWND> g_controls;                // disabled while the game runs
HWND g_rbVr,g_rbFlat,g_coSize,g_coDlss,g_coSharp,g_coEyes,g_btZip,g_btDefaults,g_status,g_last,g_stZip,g_btUpdate,g_stUpdate;
enum {ID_VR=100,ID_FLAT,ID_SIZE,ID_DLSS,ID_SHARP,ID_ZIP,ID_DEFAULTS,ID_EYES,ID_UPDATE};
constexpr UINT WM_APP_ZIP_DONE=WM_APP+1;     // wParam: exit code, lParam: new std::wstring (the zip's path)
constexpr UINT WM_APP_LATEST=WM_APP+2;       // lParam: new std::wstring (the newest release's version, or "" when it could not be checked)
constexpr UINT WM_APP_UPDATE_LINE=WM_APP+3;  // lParam: new std::wstring (a line the updater wrote)
constexpr UINT WM_APP_UPDATE_DONE=WM_APP+4;  // wParam: the updater's exit code
constexpr int kLeft=20,kRight=330,kWidth=700,kRowHeight=74;
struct Choice {const wchar_t* label;const char* value;};
const Choice kSizes[]{{L"Low 0.7  (faster)","0.7"},{L"Normal 0.8","0.8"},{L"High 1.0  (sharper, slower)","1.0"}};
const Choice kDlss[]{{L"Quality","Quality"},{L"Balanced","Balanced"},{L"Performance","Performance"},{L"DLAA  (sharpest, slower)","Dlaa"}};
const Choice kSharp[]{{L"Off","0"},{L"Soft 0.2","0.2"},{L"Strong 0.5","0.5"}};
const Choice kEyes[]{{L"Both eyes every frame","0"},{L"One eye a frame  (last picture, test)","1"},{L"One eye a frame, the other made from it  (PureDark's way, test)","2"}};

int S(int v){return MulDiv(v,g_dpi,96);}
std::wstring wide(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
std::string narrow(const std::wstring& s){if(s.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string o(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),o.data(),n,nullptr,nullptr);return o;}
std::wstring modPath(){return g_folder+L"\\EnshroudedVR";}
std::wstring iniPath(){return modPath()+L"\\EnshroudedVR.ini";}
std::wstring defaultsPath(){return modPath()+L"\\EnshroudedVR.defaults.ini";}
bool exists(const std::wstring& path){return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}
// Shared with a game that may be writing it, so a log in use still reads.
bool readFile(const std::wstring& path,std::string& out){
    out.clear();const HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size{};bool ok=GetFileSizeEx(h,&size)&&size.QuadPart<=(512ll<<20);
    if(ok){out.resize(size_t(size.QuadPart));size_t at=0;
        while(at<out.size()){DWORD got=0;if(!ReadFile(h,out.data()+at,DWORD(std::min<size_t>(out.size()-at,1u<<20)),&got,nullptr)||!got)break;at+=got;}
        out.resize(at);}
    CloseHandle(h);return ok;
}
bool writeBytes(const std::wstring& path,const std::string& bytes){std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)return false;f.write(bytes.data(),std::streamsize(bytes.size()));return bool(f);}
bool writeFile(const std::wstring& path,const std::string& bytes){
    const auto temporary=path+L".new";
    if(!writeBytes(temporary,bytes))return false;
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)!=0;
}
bool gameRunning(){
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return false;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;bool found=false;
    if(Process32FirstW(snapshot,&entry))do{if(!_wcsicmp(entry.szExeFile,L"enshrouded.exe")){found=true;break;}}while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return found;
}
HWND part(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0,HFONT font=nullptr){
    HWND hwnd=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,S(x),S(y),S(w),S(h),g_window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),g_instance,nullptr);
    SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(font?font:g_font),TRUE);return hwnd;
}
void row(const wchar_t* title,const wchar_t* note){part(L"STATIC",title,SS_LEFT,kLeft,g_y,296,20,0,g_bold);part(L"STATIC",note,SS_LEFT,kLeft,g_y+20,300,50);}
HWND control(HWND h){g_controls.push_back(h);return h;}
HWND combo(int id,const Choice* choices,size_t count,int w=250){
    HWND h=control(part(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,kRight,g_y+8,w,200,id));
    for(size_t i=0;i<count;i++)SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(choices[i].label));
    return h;
}
void pick(HWND h,const Choice* choices,size_t count,const std::string& value,size_t fallback){
    size_t at=fallback;for(size_t i=0;i<count;i++)if(sameText(choices[i].value,value)||(std::atof(value.c_str())>0&&std::abs(std::atof(choices[i].value)-std::atof(value.c_str()))<1e-6))at=i;
    SendMessageW(h,CB_SETCURSEL,at,0);
}
void check(HWND h,bool on){SendMessageW(h,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);}
void status(const wchar_t* text){SetWindowTextW(g_status,text);}
// What the loader wrote about the last start of the game (EnshroudedVR\logs\last-status.txt).
void showLast(){
    std::string last;std::wstring line=L"Last start of the game: (none yet)";
    if(readFile(modPath()+L"\\logs\\last-status.txt",last)&&!trim(last).empty())line=L"Last start of the game: "+wide(trim(last));
    wchar_t now[1024]{};GetWindowTextW(g_last,now,1024);if(line!=now)SetWindowTextW(g_last,line.c_str());
}
bool load(){
    g_installed=exists(g_folder+L"\\enshrouded.exe")&&exists(g_folder+L"\\dinput8.dll")&&exists(modPath()+L"\\bin\\evr_hooks.dll");
    if(!readFile(iniPath(),g_ini)){
        std::string defaults;if(!readFile(defaultsPath(),defaults)){status(L"EnshroudedVR\\EnshroudedVR.defaults.ini is missing. Extract everything in the mod's package into the game's folder, next to enshrouded.exe.");return false;}
        if(!writeFile(iniPath(),defaults)){status(L"Could not write EnshroudedVR\\EnshroudedVR.ini.");return false;}
        g_ini=defaults;}
    return true;
}
void show(){
    g_loading=true;const auto v=read(g_ini);
    check(g_rbVr,v.vr);check(g_rbFlat,!v.vr);
    pick(g_coSize,kSizes,std::size(kSizes),text(v.renderScale),1);pick(g_coDlss,kDlss,std::size(kDlss),v.dlss,0);
    pick(g_coSharp,kSharp,std::size(kSharp),text(v.sharpness),1);
    pick(g_coEyes,kEyes,std::size(kEyes),v.alternateEyes?(v.eyeWarp==2?"2":"1"):"0",0);
    for(HWND h:{g_coSize,g_coDlss,g_coSharp,g_coEyes})EnableWindow(h,!g_running&&v.vr);
    g_loading=false;
}
void refreshRunning(bool force=false){
    showLast();
    const bool now=gameRunning();refreshUpdateButton();if(now==g_running&&!force)return;g_running=now;refreshUpdateButton();
    if(g_updating)return;   // the update keeps everything off until it is done
    for(HWND h:g_controls)EnableWindow(h,!now);EnableWindow(g_btDefaults,!now);
    status(now?L"The game is running. Close the game to change settings.":
        !g_installed?L"The mod's files are not complete here. Extract everything in the mod's package into the game's folder, next to enshrouded.exe.\nenshrouded.exe と同じフォルダに、パッケージの中身を全部置いてください。":
        L"Saved. The game uses it the next time it starts - start it from Steam as usual.");
    if(!now)show();
}
// One value into the file (read again first: another program, or a hand, may have changed other lines).
void store(const char* section,const char* key,const std::string& value){
    if(g_loading||g_running)return;
    if(gameRunning()){refreshRunning();return;}
    std::string now;if(!readFile(iniPath(),now))now=g_ini;
    const auto changed=set(now,section,key,value);
    if(!writeFile(iniPath(),changed)){status(L"Could not write EnshroudedVR\\EnshroudedVR.ini.");return;}
    g_ini=changed;show();
}

// ---- Problem report: every log and settings file a report needs, into one zip beside this program ----
std::string stampNow(const char* format){SYSTEMTIME t{};GetLocalTime(&t);char b[40]{};wsprintfA(b,format,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return b;}
std::wstring registryText(const wchar_t* key,const wchar_t* name){
    wchar_t value[1024]{};DWORD bytes=sizeof value-sizeof(wchar_t);
    return RegGetValueW(HKEY_LOCAL_MACHINE,key,name,RRF_RT_REG_SZ,nullptr,value,&bytes)==ERROR_SUCCESS?std::wstring(value):std::wstring();
}
// Personal names out of the zip (user 2026-10-15: "nothing personal in it?"): the game's own log names the player and,
// in company, the other players ("Player 'name' logged in", "Character Savegame 'name'") and holds paths with the
// Windows user's name (C:/Users/<name>/...). Every text file that goes into a zip has those names replaced by
// <player-1>, <player-2>, ... and <user>. The mod's own records name players by the last four digits of their Steam ID only.
std::vector<std::pair<std::string,std::string>> g_hidden;
void collectPrivate(){
    g_hidden.clear();
    auto add=[&](const std::string& name,const std::string& as){
        if(name.size()<3)return;for(const auto& h:g_hidden)if(_stricmp(h.first.c_str(),name.c_str())==0)return;g_hidden.push_back({name,as});};
    wchar_t value[260]{};
    if(GetEnvironmentVariableW(L"USERNAME",value,260))add(narrow(value),"<user>");
    if(GetEnvironmentVariableW(L"USERPROFILE",value,260)){std::wstring p=value;const auto s=p.find_last_of(L'\\');if(s!=std::wstring::npos)add(narrow(p.substr(s+1)),"<user>");}
    if(GetEnvironmentVariableW(L"COMPUTERNAME",value,260))add(narrow(value),"<computer>");   // in the game's log and its crash dumps
    std::string log;readFile(g_folder+L"\\enshrouded.log",log);unsigned players=0;
    for(const char* key:{"Player '","Savegame '","player '"}){
        for(size_t at=log.find(key);at!=std::string::npos;at=log.find(key,at+1)){
            const size_t begin=at+std::strlen(key),end=log.find('\'',begin);
            if(end==std::string::npos||end-begin>64||end==begin)continue;
            const std::string name=log.substr(begin,end-begin);bool known=false;for(const auto& h:g_hidden)if(_stricmp(h.first.c_str(),name.c_str())==0)known=true;
            if(!known)add(name,"<player-"+std::to_string(++players)+">");
        }
    }
    std::sort(g_hidden.begin(),g_hidden.end(),[](const auto& a,const auto& b){return a.first.size()>b.first.size();});
}
std::string hide(std::string s){
    for(const auto& [name,as]:g_hidden){
        std::string lower=s;for(auto& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));
        std::string key=name;for(auto& c:key)c=char(std::tolower(static_cast<unsigned char>(c)));
        std::string out;out.reserve(s.size());size_t at=0;
        for(size_t found=lower.find(key);found!=std::string::npos;found=lower.find(key,found+key.size())){out.append(s,at,found-at);out+=as;at=found+key.size();}
        out.append(s,at,std::string::npos);s.swap(out);
    }
    return s;
}
bool textFile(const std::wstring& ext){for(const wchar_t* e:{L".log",L".txt",L".json",L".jsonl",L".ini",L".csv",L".info"})if(!_wcsicmp(ext.c_str(),e))return true;return false;}
// A file into the zip's folder: text with the names replaced, anything else as it is.
void copyHidden(const std::wstring& from,const std::wstring& to,const std::wstring& ext){
    std::string bytes;if(textFile(ext)&&readFile(from,bytes)){writeBytes(to,hide(bytes));return;}
    CopyFileW(from.c_str(),to.c_str(),FALSE);
}
// The VR bug report (makeBugReport): a long session's record is cut to its beginning (the start, the hooks) and its end
// (the 30 seconds were the last thing written), so that the zip stays small enough to send (Flat2VR's Discord takes 20 MB).
bool g_trimLogs=false,g_gameStopped=false;
void copyTrimmed(const std::wstring& from,const std::wstring& to,unsigned long long size){
    const HANDLE h=CreateFileW(from.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return;
    constexpr unsigned long long head=8ull<<20,tail=24ull<<20;std::string a(size_t(head),char(0)),b(size_t(tail),char(0));DWORD got=0;size_t at=0;
    while(at<a.size()&&ReadFile(h,a.data()+at,DWORD(std::min<size_t>(a.size()-at,1u<<20)),&got,nullptr)&&got)at+=got;a.resize(at);
    LARGE_INTEGER to2{};to2.QuadPart=LONGLONG(size-tail);SetFilePointerEx(h,to2,nullptr,FILE_BEGIN);at=0;
    while(at<b.size()&&ReadFile(h,b.data()+at,DWORD(std::min<size_t>(b.size()-at,1u<<20)),&got,nullptr)&&got)at+=got;b.resize(at);CloseHandle(h);
    constexpr char newline=char(10);
    const auto cutA=a.find_last_of(newline),cutB=b.find(newline);if(cutA!=std::string::npos)a.resize(cutA+1);if(cutB!=std::string::npos)b.erase(0,cutB+1);
    writeBytes(to,hide(a+"{\"event\":\"bug_report_log_cut\",\"cut_bytes\":"+std::to_string(size-a.size()-b.size())+",\"kept\":\"the first 8 MB and the last 24 MB\"}"+std::string(1,newline)+b));
}
// ---- The game's own crash dump (crash_dumps\enshrouded_<UTC time>\: enshrouded.dmp, .info, .log; about 3 MB together)
// goes along when it is recent (user 2026-10-15: "a crash in the 30 seconds must still give a report"). The dump holds
// the memory of the game's threads at the crash (and what it points to): the names of g_hidden in it (the Windows user's
// in the paths of the loaded programs, the computer's, the players') become x's of the same length, and so do internet
// addresses (a.b.c.d, as text and as their four bytes) and Steam IDs (as text and as their eight bytes); the dump stays
// readable for a debugger. The .info's computer name goes as in every text file, its user_id_hash becomes <hidden>.
long long unixOf(const FILETIME& f){ULARGE_INTEGER t{};t.LowPart=f.dwLowDateTime;t.HighPart=f.dwHighDateTime;return static_cast<long long>(t.QuadPart/10000000ull)-11644473600ll;}
std::string lowered(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
struct Scrubbed {unsigned names=0,addresses=0,steamIds=0;};
Scrubbed scrubDump(std::string& b){
    Scrubbed n;std::string lower=lowered(b);
    auto wipe=[&](const std::string& key,bool sixteen){unsigned hits=0;if(key.size()<3)return hits;
        for(size_t at=lower.find(key);at!=std::string::npos;at=lower.find(key,at+key.size())){
            for(size_t i=0;i<key.size();++i){const char c=sixteen&&(i&1)?char(0):'x';b[at+i]=c;lower[at+i]=c;}++hits;}
        return hits;};
    for(const auto& h:g_hidden){
        std::string sixteen;for(const wchar_t ch:wide(h.first)){sixteen+=char(ch&0xff);sixteen+=char((ch>>8)&0xff);}
        n.names+=wipe(lowered(h.first),false)+wipe(lowered(sixteen),true);}
    // Text: a.b.c.d and 7656119.......... (8-bit and 16-bit characters).
    std::vector<uint32_t> binary;
    for(const size_t step:{size_t(1),size_t(2)}){
        auto at_=[&](size_t i)->int{if(i>=b.size()||(step==2&&(i+1>=b.size()||b[i+1]!=0)))return -1;return static_cast<unsigned char>(b[i]);};
        auto digit=[](int c){return c>='0'&&c<='9';};
        for(size_t at=0;at<b.size();++at){
            if(!digit(at_(at)))continue;
            if(at>=step){const int p=at_(at-step);if(digit(p)||p=='.')continue;}
            size_t end=at;std::string text;
            for(int c=at_(end);(digit(c)||c=='.')&&text.size()<=40;c=at_(end)){text+=char(c);end+=step;}
            bool wipeIt=false;
            if(text.size()==17&&text.rfind("7656119",0)==0){wipeIt=true;++n.steamIds;}
            else if(text.size()>=7&&text.size()<=15){
                unsigned parts[4]{};int count=0;bool ok=true;size_t s=0;
                for(;;){const size_t dot=text.find('.',s);const std::string part=text.substr(s,dot==std::string::npos?std::string::npos:dot-s);
                    if(part.empty()||part.size()>3||count==4){ok=false;break;}
                    parts[count++]=unsigned(std::stoul(part));if(parts[count-1]>255){ok=false;break;}
                    if(dot==std::string::npos)break;s=dot+1;}
                if(ok&&count==4&&parts[0]!=127&&(parts[0]|parts[1]|parts[2]|parts[3])){wipeIt=true;++n.addresses;
                    if(parts[0]>=10&&parts[1]&&parts[2]&&parts[3])binary.push_back(parts[0]|parts[1]<<8|parts[2]<<16|parts[3]<<24);}
            }
            if(wipeIt)for(size_t i=at;i<end;i+=step)if(digit(at_(i)))b[i]='x';
            at=end;
        }
    }
    // Their bytes: the addresses found as text (four bytes, none 0) and every Steam ID of a player (0x01100001 above).
    std::sort(binary.begin(),binary.end());binary.erase(std::unique(binary.begin(),binary.end()),binary.end());
    for(const uint32_t ip:binary){const char k[4]{char(ip&0xff),char((ip>>8)&0xff),char((ip>>16)&0xff),char((ip>>24)&0xff)};const std::string key(k,4);
        for(size_t at=b.find(key);at!=std::string::npos;at=b.find(key,at+4)){std::memset(&b[at],0,4);++n.addresses;}}
    for(size_t at=0;at+8<=b.size();++at)
        if(b[at+4]==char(1)&&b[at+5]==char(0)&&b[at+6]==char(0x10)&&b[at+7]==char(1)&&(b[at]|b[at+1]|b[at+2]|b[at+3])){std::memset(&b[at],0,4);++n.steamIds;}
    return n;
}
std::string hideInfoFields(std::string s){
    const auto at=s.find("\"user_id_hash\"");if(at==std::string::npos)return s;
    const auto info=s.find("\"info\"",at);if(info==std::string::npos||info-at>200)return s;
    const auto colon=s.find(':',info+6);const auto open=colon==std::string::npos?colon:s.find('"',colon);const auto close=open==std::string::npos?open:s.find('"',open+1);
    if(close!=std::string::npos&&close-open<80)s.replace(open+1,close-open-1,"<hidden>");
    return s;
}
// The newest crash dump folder written since `since` (unix time): its name, time and size.
bool newestDump(long long since,std::wstring& name,long long& time,unsigned long long& bytes){
    const std::wstring dumps=g_folder+L"\\crash_dumps";name.clear();time=since-1;bytes=0;
    WIN32_FIND_DATAW found{};const HANDLE search=FindFirstFileW((dumps+L"\\*").c_str(),&found);if(search==INVALID_HANDLE_VALUE)return false;
    do{const std::wstring n=found.cFileName;if(!(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||n==L"."||n==L"..")continue;
        long long t=0;unsigned long long total=0;WIN32_FIND_DATAW f{};const HANDLE s=FindFirstFileW((dumps+L"\\"+n+L"\\*").c_str(),&f);
        if(s!=INVALID_HANDLE_VALUE){do{if(!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){t=std::max(t,unixOf(f.ftLastWriteTime));total+=(static_cast<unsigned long long>(f.nFileSizeHigh)<<32)|f.nFileSizeLow;}}while(FindNextFileW(s,&f));FindClose(s);}
        if(total&&t>time){time=t;name=n;bytes=total;}
    }while(FindNextFileW(search,&found));
    FindClose(search);return !name.empty();
}
void includeCrashDump(const std::wstring& to,long long since,std::string& listing){
    std::wstring name;long long time=0;unsigned long long bytes=0;if(!newestDump(since,name,time,bytes))return;
    const std::wstring from=g_folder+L"\\crash_dumps\\"+name,dir=to+L"\\crash_dump";CreateDirectoryW(dir.c_str(),nullptr);
    std::string note;
    WIN32_FIND_DATAW f{};const HANDLE s=FindFirstFileW((from+L"\\*").c_str(),&f);
    if(s!=INVALID_HANDLE_VALUE){do{if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        const std::wstring n=f.cFileName;const auto dot=n.find_last_of(L'.');const std::wstring ext=dot==std::wstring::npos?L"":n.substr(dot);
        const auto size=(static_cast<unsigned long long>(f.nFileSizeHigh)<<32)|f.nFileSizeLow;std::string bytes2;
        if(!_wcsicmp(ext.c_str(),L".info")){if(readFile(from+L"\\"+n,bytes2))writeBytes(dir+L"\\"+n,hide(hideInfoFields(bytes2)));}
        else if(textFile(ext))copyHidden(from+L"\\"+n,dir+L"\\"+n,ext);
        else if(size<=(64ull<<20)&&readFile(from+L"\\"+n,bytes2)){const auto c=scrubDump(bytes2);writeBytes(dir+L"\\"+n,bytes2);
            note+="  "+narrow(n)+": "+std::to_string(c.names)+" names, "+std::to_string(c.addresses)+" internet addresses, "+std::to_string(c.steamIds)+" Steam IDs made x's\r\n";}
        else note+="  "+narrow(n)+" left out ("+std::to_string(size>>20)+" MB)\r\n";
    }while(FindNextFileW(s,&f));FindClose(s);}
    char when[64]{};const time_t t=time_t(time);tm local{};localtime_s(&local,&t);std::strftime(when,sizeof when,"%Y-%m-%d %H:%M:%S",&local);
    listing+="\r\nThe game's crash dump in this zip (crash_dump\\): crash_dumps\\"+narrow(name)+", written "+when+"\r\n"+note;
}
// The files of a folder as lines for info.txt, and (with `to`) copied there; folders below go along up to `depth`.
void gather(const std::wstring& from,const std::wstring& to,int depth,std::string& listing,const std::wstring& shown){
    WIN32_FIND_DATAW found{};const HANDLE search=FindFirstFileW((from+L"\\*").c_str(),&found);if(search==INVALID_HANDLE_VALUE)return;
    if(!to.empty())CreateDirectoryW(to.c_str(),nullptr);
    do{const std::wstring name=found.cFileName;if(name==L"."||name==L".."||(found.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
        if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(depth>0)gather(from+L"\\"+name,to.empty()?to:to+L"\\"+name,depth-1,listing,shown+L"\\"+name);continue;}
        const auto size=(static_cast<unsigned long long>(found.nFileSizeHigh)<<32)|found.nFileSizeLow;
        SYSTEMTIME t{};FILETIME local{};FileTimeToLocalFileTime(&found.ftLastWriteTime,&local);FileTimeToSystemTime(&local,&t);char line[700]{};
        std::snprintf(line,sizeof line,"  %-64s %12llu  %04u-%02u-%02u %02u:%02u\r\n",narrow(shown+L"\\"+name).c_str(),size,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute);listing+=line;
        const auto dot=name.find_last_of(L'.');const std::wstring ext=dot==std::wstring::npos?L"":name.substr(dot);
        // Programs and pictures stay out; a log that grew very large too (its size is in the list).
        if(!to.empty()&&_wcsicmp(ext.c_str(),L".dll")&&_wcsicmp(ext.c_str(),L".exe")&&_wcsicmp(ext.c_str(),L".ppm")&&_wcsicmp(ext.c_str(),L".rgba8")&&size<=(200ull<<20))
            {if(g_trimLogs&&size>(32ull<<20)&&!_wcsicmp(ext.c_str(),L".jsonl"))copyTrimmed(from+L"\\"+name,to+L"\\"+name,size);else copyHidden(from+L"\\"+name,to+L"\\"+name,ext);}
    }while(FindNextFileW(search,&found));
    FindClose(search);
}
// Everything a report needs (the settings, versions, the mod's logs, the game's settings and log) into folder, with info.txt.
// keepStart: a start folder of EnshroudedVR\logs that goes along whether or not it is among the newest (a VR bug report's);
// the newest crash dump written since dumpsSince goes along (by default: of the last three days).
void gatherReport(const std::wstring& folder,const std::string& title,size_t newestStarts=3,const std::wstring& keepStart=L"",long long dumpsSince=-1){
    collectPrivate();
    std::string version,last,info=title+"\r\nMade: "+stampNow("%04u-%02u-%02u %02u:%02u:%02u")+"\r\n";
    readFile(modPath()+L"\\version.txt",version);readFile(modPath()+L"\\logs\\last-status.txt",last);
    std::string ini;if(!readFile(iniPath(),ini))ini=g_ini;   // as it is on disk (the window-less runs have not read it)
    const auto v=read(ini);
    info+="Mod version: "+(trim(version).empty()?std::string("unknown"):trim(version))+"\r\n";
    info+="Settings: VR="+std::to_string(int(v.vr))+" Sync="+std::to_string(int(v.online))+" RenderScale="+text(v.renderScale)+" Dlss="+v.dlss+" Sharpness="+text(v.sharpness)+" AlternateEyes="+std::to_string(int(v.alternateEyes))+" EyeWarp="+std::to_string(v.eyeWarp)+"\r\n";
    // (a game that crashed can stay in the process list a while: a program holding its handle - Steam, a launcher)
    info+="Game running: "+std::string(g_gameStopped?"no (it stopped before the 30 seconds ended)":gameRunning()?"yes":"no")+"\r\nLast start: "+trim(last)+"\r\n";
    const wchar_t* windows=L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    info+="Windows: "+narrow(registryText(windows,L"ProductName"))+" "+narrow(registryText(windows,L"DisplayVersion"))+" build "+narrow(registryText(windows,L"CurrentBuild"))+"\r\n";
    info+="OpenXR runtime: "+narrow(registryText(L"SOFTWARE\\Khronos\\OpenXR\\1",L"ActiveRuntime"))+"\r\n";
    std::string listing;
    {WIN32_FILE_ATTRIBUTE_DATA a{};for(const wchar_t* name:{L"enshrouded.exe",L"dinput8.dll",L"Enshrouded VR setting.exe"}){char line[300]{};
        if(GetFileAttributesExW((g_folder+L"\\"+name).c_str(),GetFileExInfoStandard,&a)){SYSTEMTIME t{};FILETIME local{};FileTimeToLocalFileTime(&a.ftLastWriteTime,&local);FileTimeToSystemTime(&local,&t);
            std::snprintf(line,sizeof line,"  %-64s %12llu  %04u-%02u-%02u %02u:%02u\r\n",narrow(name).c_str(),(static_cast<unsigned long long>(a.nFileSizeHigh)<<32)|a.nFileSizeLow,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute);}
        else std::snprintf(line,sizeof line,"  %-64s      missing\r\n",narrow(name).c_str());
        listing+=line;}}
    // The mod's own files (listed), its settings and loader log, the newest three start folders, the game's settings and log.
    gather(modPath(),folder,0,listing,L"EnshroudedVR");
    gather(modPath()+L"\\bin",L"",0,listing,L"EnshroudedVR\\bin");gather(modPath()+L"\\data",L"",1,listing,L"EnshroudedVR\\data");
    {std::vector<std::wstring> starts;WIN32_FIND_DATAW found{};const HANDLE search=FindFirstFileW((modPath()+L"\\logs\\*").c_str(),&found);
        if(search!=INVALID_HANDLE_VALUE){do{if((found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&found.cFileName[0]!=L'.')starts.push_back(found.cFileName);}while(FindNextFileW(search,&found));FindClose(search);}
        std::sort(starts.rbegin(),starts.rend());
        CreateDirectoryW((folder+L"\\logs").c_str(),nullptr);gather(modPath()+L"\\logs",folder+L"\\logs",0,listing,L"EnshroudedVR\\logs");
        for(size_t i=0;i<starts.size();i++)gather(modPath()+L"\\logs\\"+starts[i],i<newestStarts||starts[i]==keepStart?folder+L"\\logs\\"+starts[i]:std::wstring(),3,listing,L"EnshroudedVR\\logs\\"+starts[i]);}
    for(const wchar_t* name:{L"enshrouded_local.json",L"enshrouded.log"})copyHidden(g_folder+L"\\"+name,folder+L"\\"+name,std::wstring(name).substr(std::wstring(name).find_last_of(L'.')));
    {   // 1.0 (2026-10-16): the game's machine settings are in Saved Games\keen games\Enshrouded\local_settings.json
        PWSTR saved=nullptr;if(SHGetKnownFolderPath(FOLDERID_SavedGames,0,nullptr,&saved)==S_OK&&saved)
            copyHidden(std::wstring(saved)+L"\\keen games\\Enshrouded\\local_settings.json",folder+L"\\local_settings.json",L".json");
        CoTaskMemFree(saved);}
    {std::string dumps;gather(g_folder+L"\\crash_dumps",L"",1,dumps,L"crash_dumps");if(!dumps.empty())listing+="\r\nThe game's crash dumps (a recent one is in crash_dump\\):\r\n"+dumps;}
    includeCrashDump(folder,dumpsSince>=0?dumpsSince:static_cast<long long>(std::time(nullptr))-3*86400,listing);
    info+="Personal names replaced in the text files: "+std::to_string(g_hidden.size())+" (the Windows user, the players of the game's log)\r\n";
    {   // what a path problem needs of the hidden user name: not the name, only whether it holds other than plain letters
        wchar_t user[260]{};const DWORD n=GetEnvironmentVariableW(L"USERNAME",user,260);bool plain=true;
        for(DWORD i=0;i<n;++i)if(user[i]>126||user[i]<33)plain=false;
        info+="Windows user name: "+std::to_string(n)+" characters, "+(plain?"plain ASCII":"has spaces or non-ASCII characters (Japanese etc.)")+"\r\n";}
    writeBytes(folder+L"\\info.txt",hide(info+"\r\nFiles:\r\n"+listing));
}
// A work folder in the temporary files goes again (only what was copied into it a moment ago).
void removeTree(const std::wstring& f,int depth){
    WIN32_FIND_DATAW d{};const HANDLE s=FindFirstFileW((f+L"\\*").c_str(),&d);
    if(s!=INVALID_HANDLE_VALUE){do{const std::wstring n=d.cFileName;if(n==L"."||n==L"..")continue;
        if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(depth<6&&!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))removeTree(f+L"\\"+n,depth+1);}else DeleteFileW((f+L"\\"+n).c_str());}while(FindNextFileW(s,&d));FindClose(s);}
    RemoveDirectoryW(f.c_str());
}
DWORD compressFolder(const std::wstring& folder,const std::wstring& zip){
    wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);
    auto quote=[](std::wstring s){for(size_t i=s.find(L'\'');i!=std::wstring::npos;i=s.find(L'\'',i+2))s.insert(i,1,L'\'');return L"'"+s+L"'";};
    std::wstring command=L"\""+std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe\" -NoLogo -NoProfile -ExecutionPolicy Bypass -Command \"Compress-Archive -Path "+quote(folder+L"\\*")+L" -DestinationPath "+quote(zip)+L" -Force\"";
    STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION process{};DWORD code=1;
    if(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)){
        WaitForSingleObject(process.hProcess,INFINITE);GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);}
    return code;
}
// A VR bug report whose 30 seconds did not end (the game crashed or was ended in them, and the watcher below could not
// finish it either: report.json still says "recording", and there is no zip.txt), of the last three days.
struct Unfinished {std::wstring folder;long long start=0,end=0;std::wstring language;};
bool findUnfinished(const std::wstring& root,int depth,Unfinished& best);
int makeBugReport(const std::wstring& bugFolder,long long start,long long end,const std::wstring& language,bool unfinished=false,std::wstring* zipOut=nullptr);
int makeLogsZip(std::wstring& zip){
    const std::string stamp=stampNow("%04u%02u%02u-%02u%02u%02u");
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    const std::wstring folder=std::wstring(temp)+L"EnshroudedVR-logs-"+wide(stamp);zip=g_folder+L"\\EnshroudedVR-logs-"+wide(stamp)+L".zip";
    if(!CreateDirectoryW(folder.c_str(),nullptr))return 2;
    gatherReport(folder,"Enshrouded VR setting - problem report");
    const DWORD code=compressFolder(folder,zip);removeTree(folder,0);
    return code==0&&exists(zip)?0:3;
}
// "Make zip": the logs into EnshroudedVR-logs-<time>.zip - or, first, a VR bug report that a crash cut short into its zip.
void startZip(){
    if(g_zipping||g_updating)return;
    Unfinished u;const bool cut=findUnfinished(modPath()+L"\\logs",2,u);
    g_zipping=true;EnableWindow(g_btZip,FALSE);SetWindowTextW(g_stZip,cut?L"Finishing the VR bug report that was cut short...":L"Making the zip...");
    std::thread([cut,u]{
        std::wstring zip;const int code=cut?makeBugReport(u.folder,u.start,u.end,u.language,true,&zip):makeLogsZip(zip);
        PostMessageW(g_window,WM_APP_ZIP_DONE,WPARAM(code),reinterpret_cast<LPARAM>(new std::wstring(zip)));
    }).detach();
}
// ---- The VR bug report's zip (hooks/bug_report.hpp starts "--bug-report <folder> <start> <end>" when its 30 seconds
// are over): the problem report above, the report's folder (report.json, the Mock scripts of the player's hands and
// head), and the pictures of both eyes taken in the window as JPEG pairs (left | right), into VR_bug_report_<time>.zip in
// the game's folder. The PPM pictures that were turned into JPEG are deleted (they are the report's own). No window.
struct Picture {uint32_t width=0,height=0;std::vector<uint8_t> rgb;};
bool readPpm(const std::wstring& path,Picture& out){
    std::string bytes;if(!readFile(path,bytes)||bytes.size()<16||bytes[0]!='P'||bytes[1]!='6')return false;
    size_t at=2;
    auto field=[&](unsigned long& v){while(at<bytes.size()&&std::isspace(static_cast<unsigned char>(bytes[at])))++at;const size_t b=at;
        while(at<bytes.size()&&std::isdigit(static_cast<unsigned char>(bytes[at])))++at;if(b==at)return false;v=std::stoul(bytes.substr(b,at-b));return true;};
    unsigned long w=0,h=0,m=0;if(!field(w)||!field(h)||!field(m)||m!=255||!w||!h||w>16384||h>16384)return false;++at;
    if(bytes.size()-at<size_t(w)*h*3)return false;
    out.width=uint32_t(w);out.height=uint32_t(h);out.rgb.assign(bytes.begin()+std::ptrdiff_t(at),bytes.begin()+std::ptrdiff_t(at+size_t(w)*h*3));return true;
}
bool writeJpeg(IWICImagingFactory* factory,const std::wstring& path,const std::vector<uint8_t>& bgr,uint32_t width,uint32_t height,uint32_t stride){
    IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;IPropertyBag2* bag=nullptr;bool ok=false;
    if(SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))&&
       SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatJpeg,nullptr,&encoder))&&SUCCEEDED(encoder->Initialize(stream,WICBitmapEncoderNoCache))&&
       SUCCEEDED(encoder->CreateNewFrame(&frame,&bag))){
        PROPBAG2 option{};option.pstrName=const_cast<LPOLESTR>(L"ImageQuality");VARIANT value;VariantInit(&value);value.vt=VT_R4;value.fltVal=.85f;bag->Write(1,&option,&value);
        WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;
        ok=SUCCEEDED(frame->Initialize(bag))&&SUCCEEDED(frame->SetSize(width,height))&&SUCCEEDED(frame->SetPixelFormat(&format))&&IsEqualGUID(format,GUID_WICPixelFormat24bppBGR)&&
           SUCCEEDED(frame->WritePixels(height,stride,UINT(bgr.size()),const_cast<BYTE*>(bgr.data())))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit());
    }
    if(bag)bag->Release();if(frame)frame->Release();if(encoder)encoder->Release();if(stream)stream->Release();
    return ok;
}
// The pairs of the window: native-xr/frame-<n>-eye-<0|1>.ppm written between start - 600 s and end + 20 s (since 2026-10-18
// the 10 seconds before the press are written at the press, in the menu - possibly minutes before the window starts).
unsigned convertPictures(const std::wstring& nativeXr,const std::wstring& to,long long start,long long end,std::string& listing){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return 0;
    IWICImagingFactory* factory=nullptr;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))){CoUninitialize();return 0;}
    std::vector<unsigned long> frames;WIN32_FIND_DATAW found{};const HANDLE search=FindFirstFileW((nativeXr+L"\\frame-*-eye-0.ppm").c_str(),&found);
    if(search!=INVALID_HANDLE_VALUE){
        do{ULARGE_INTEGER t{};t.LowPart=found.ftLastWriteTime.dwLowDateTime;t.HighPart=found.ftLastWriteTime.dwHighDateTime;
            const long long unixTime=static_cast<long long>(t.QuadPart/10000000ull)-11644473600ll;
            if(unixTime>=start-600&&unixTime<=end+20)frames.push_back(std::wcstoul(found.cFileName+6,nullptr,10));}while(FindNextFileW(search,&found));
        FindClose(search);}
    std::sort(frames.begin(),frames.end());CreateDirectoryW(to.c_str(),nullptr);unsigned made=0;
    for(const auto n:frames){
        const std::wstring left=nativeXr+L"\\frame-"+std::to_wstring(n)+L"-eye-0.ppm",right=nativeXr+L"\\frame-"+std::to_wstring(n)+L"-eye-1.ppm";
        Picture a,b;if(!readPpm(left,a))continue;const bool pair=readPpm(right,b);
        const uint32_t gap=pair?8:0,width=a.width+gap+(pair?b.width:0),height=std::max(a.height,pair?b.height:0u),stride=(width*3+3)&~3u;
        std::vector<uint8_t> bgr(size_t(stride)*height,255);
        auto put=[&](const Picture& p,uint32_t x0){for(uint32_t y=0;y<p.height;++y)for(uint32_t x=0;x<p.width;++x){
            const uint8_t* s=&p.rgb[(size_t(y)*p.width+x)*3];uint8_t* d=&bgr[size_t(y)*stride+size_t(x0+x)*3];d[0]=s[2];d[1]=s[1];d[2]=s[0];}};
        put(a,0);if(pair)put(b,a.width+gap);
        wchar_t name[64]{};swprintf_s(name,L"pair-%06lu.jpg",n);
        if(writeJpeg(factory,to+L"\\"+name,bgr,width,height,stride)){
            ++made;DeleteFileW(left.c_str());if(pair)DeleteFileW(right.c_str());
            listing+="  bug-report\\screenshots\\"+narrow(name)+(pair?"  (left | right)\r\n":"  (left eye only)\r\n");}
    }
    factory->Release();CoUninitialize();return made;
}
// unfinished: the 30 seconds did not end (the watcher or "Make zip" finishes it; end is the last time the game wrote report.json).
int makeBugReport(const std::wstring& bugFolder,long long start,long long end,const std::wstring& language,bool unfinished,std::wstring* zipOut){
    const std::string stamp=stampNow("%04u%02u%02u-%02u%02u%02u");
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    const std::wstring work=std::wstring(temp)+L"EnshroudedVR-bug-report-"+wide(stamp),zip=g_folder+(language==L"japanese"?L"\\バグ報告_":L"\\VR_bug_report_")+wide(stamp)+L".zip";   // the name the button's note gives (Japanese: "バグ報告.zip")
    if(!CreateDirectoryW(work.c_str(),nullptr))return 2;
    g_trimLogs=true;g_gameStopped=unfinished;
    // The session's folder: the probe's folder holds the report's folder; the pictures are beside the probe's folder.
    const auto slash=bugFolder.find_last_of(L'\\');const std::wstring probe=slash==std::wstring::npos?bugFolder:bugFolder.substr(0,slash);
    const auto slash2=probe.find_last_of(L'\\');const std::wstring session=slash2==std::wstring::npos?probe:probe.substr(0,slash2);
    const bool inLogs=_wcsnicmp(probe.c_str(),(modPath()+L"\\logs\\").c_str(),modPath().size()+6)==0;
    const auto slash3=session.find_last_of(L'\\');const std::wstring sessionName=slash3==std::wstring::npos?session:session.substr(slash3+1);
    // Of the mod's start folders only the report's own (it need not be the newest when the game was started again after
    // a crash); a development run's session is gathered below, with the newest start of the mod's logs.
    gatherReport(work,"Enshrouded VR - VR bug report (the start menu's button; hooks/bug_report.hpp)",inLogs?0:1,inLogs?sessionName:std::wstring(),start-60);   // small enough to send (Flat2VR's Discord: 20 MB)
    std::string listing="\r\nThe report's folder: "+narrow(bugFolder)+"\r\n";
    if(unfinished){char when[64]{};const time_t t=time_t(end);tm local{};localtime_s(&local,&t);std::strftime(when,sizeof when,"%Y-%m-%d %H:%M:%S",&local);
        listing+="The 30 seconds did not end: the game stopped at about "+std::string(when)+" ("+std::to_string(end-start)+" s in; a crash, or the game was ended).\r\nThis zip was made afterwards by the settings program.\r\n";}
    gather(bugFolder,work+L"\\bug-report",0,listing,L"bug-report");
    if(!inLogs)gather(probe,work+L"\\session",0,listing,L"session");   // a session outside the mod's logs (a development run)
    const auto pairs=convertPictures(session+L"\\native-xr",work+L"\\bug-report\\screenshots",start,end,listing);
    listing+="Pictures: "+std::to_string(pairs)+" pairs\r\n";
    std::string info;readFile(work+L"\\info.txt",info);writeBytes(work+L"\\info.txt",hide(info+listing));
    const DWORD code=compressFolder(work,zip);
    removeTree(work,0);
    const bool made=code==0&&exists(zip);
    writeBytes(bugFolder+L"\\zip.txt",made?narrow(zip):std::string());
    if(zipOut)*zipOut=zip;
    return made?0:3;
}
long long jsonNumber(const std::string& s,const char* key){const std::string k=std::string("\"")+key+"\":";const auto at=s.find(k);return at==std::string::npos?0:std::strtoll(s.c_str()+at+k.size(),nullptr,10);}
std::string jsonText(const std::string& s,const char* key){
    const std::string k=std::string("\"")+key+"\": \"";const auto at=s.find(k);if(at==std::string::npos)return {};
    const auto end=s.find('"',at+k.size());return end==std::string::npos?std::string():s.substr(at+k.size(),end-at-k.size());}
bool findUnfinished(const std::wstring& root,int depth,Unfinished& best){
    WIN32_FIND_DATAW found{};const HANDLE search=FindFirstFileW((root+L"\\*").c_str(),&found);
    if(search!=INVALID_HANDLE_VALUE){
        const long long now=static_cast<long long>(std::time(nullptr));
        do{const std::wstring name=found.cFileName;
            if(!(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(found.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||name==L"."||name==L"..")continue;
            const std::wstring path=root+L"\\"+name;
            if(name.rfind(L"bug-report-",0)!=0){if(depth>0)findUnfinished(path,depth-1,best);continue;}
            std::string report;WIN32_FILE_ATTRIBUTE_DATA a{};
            if(exists(path+L"\\zip.txt")||!readFile(path+L"\\report.json",report)||jsonText(report,"status")!="recording")continue;
            if(!GetFileAttributesExW((path+L"\\report.json").c_str(),GetFileExInfoStandard,&a))continue;
            const long long written=unixOf(a.ftLastWriteTime);
            if(now-written>3*86400)continue;                 // an old one stays as it is
            if(now-written<15&&gameRunning())continue;       // its 30 seconds are under way
            const long long start=jsonNumber(report,"start_unix");
            if(start>best.start){best.folder=path;best.start=start;best.end=written;best.language=wide(jsonText(report,"language"));}
        }while(FindNextFileW(search,&found));
        FindClose(search);}
    return !best.folder.empty();
}
// "--watch-bug-report <folder> <the game's process id>": hooks/bug_report.hpp starts it with the 30 seconds. When the
// report is complete, the game makes its zip itself and this ends; when the game is gone first (a crash, or it was
// ended), this finishes the report at once - with the game's crash dump once that is written - so that the zip is in
// the game's folder as the button's note says, with nothing more to do for the player.
int watchBugReport(const std::wstring& folder,DWORD pid){
    const HANDLE game=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!game)return 5;
    auto status=[&]{std::string r;readFile(folder+L"\\report.json",r);return jsonText(r,"status");};
    bool gone=false;
    while(!gone){if(status()=="complete"){CloseHandle(game);return 0;}gone=WaitForSingleObject(game,500)==WAIT_OBJECT_0;}
    CloseHandle(game);
    Sleep(1000);
    std::string report;if(!readFile(folder+L"\\report.json",report)||jsonText(report,"status")!="recording"||exists(folder+L"\\zip.txt"))return 0;
    const long long start=jsonNumber(report,"start_unix");
    // The crash dump: up to 20 s for one written since the start, until its files stop growing.
    unsigned long long lastBytes=0;
    for(int i=0;i<20;++i){std::wstring name;long long t=0;unsigned long long bytes=0;
        if(newestDump(start-60,name,t,bytes)){if(bytes==lastBytes)break;lastBytes=bytes;}
        Sleep(1000);}
    WIN32_FILE_ATTRIBUTE_DATA a{};const long long end=GetFileAttributesExW((folder+L"\\report.json").c_str(),GetFileExInfoStandard,&a)?unixOf(a.ftLastWriteTime):start;
    return makeBugReport(folder,start,end,wide(jsonText(report,"language")),true);
}
// ---- Update (user, 2026-10-19: "an automatic update button in the VR setting exe, after EDF6") --------------------
// As EDF6 VR setting.exe does: on start the newest GitHub release is looked up in the background (the updater with
// -CheckOnly) and shown beside the installed version; "Update now" runs EnshroudedVR\Update-EnshroudedVR.ps1 -Yes, which
// downloads the release, checks it, saves the files it replaces and copies the new ones (the player's settings stay).
// The update may bring a new copy of this program: a running program can be renamed but not overwritten, so it steps
// aside as "<exe>.old" first; when a new one came, it is started and this one closes.
bool g_latestChecked=false,g_renamedForUpdate=false;
std::wstring g_installedVersion,g_latestVersion,g_updateNote,g_updateResult,g_updateError;bool g_updateNoteBad=false;
std::wstring selfPath(){wchar_t self[MAX_PATH]{};GetModuleFileNameW(nullptr,self,MAX_PATH);return self;}
std::wstring updaterPath(){return modPath()+L"\\Update-EnshroudedVR.ps1";}
std::wstring powerShell(){wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);return std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";}
std::wstring oem(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_OEMCP,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_OEMCP,0,s.data(),int(s.size()),w.data(),n);return w;}
// The installed package's version (EnshroudedVR\PACKAGE_MANIFEST.json "version").
std::wstring installedVersion(){
    std::string m;if(!readFile(modPath()+L"\\PACKAGE_MANIFEST.json",m))return {};
    const std::string key="\"version\"";const auto at=m.find(key);if(at==std::string::npos)return {};
    const auto open=m.find('"',m.find(':',at)+1);const auto close=open==std::string::npos?open:m.find('"',open+1);
    return close==std::string::npos?std::wstring():wide(m.substr(open+1,close-open-1));
}
std::vector<int> versionParts(const std::wstring& v){std::vector<int> p;std::wstringstream s(v);std::wstring part;while(std::getline(s,part,L'.'))p.push_back(_wtoi(part.c_str()));return p;}
bool upToDate(){return g_latestChecked&&!g_installedVersion.empty()&&!g_latestVersion.empty()&&versionParts(g_installedVersion)>=versionParts(g_latestVersion);}
// A hidden command whose output is read line by line (the updater), in the game's folder. Returns its exit code.
DWORD runCaptured(std::wstring command,const std::function<void(const std::string&)>& line){
    SECURITY_ATTRIBUTES inherit{sizeof inherit,nullptr,TRUE};HANDLE readPipe=nullptr,writePipe=nullptr;
    if(!CreatePipe(&readPipe,&writePipe,&inherit,0))return 0xFFFFFFFF;
    SetHandleInformation(readPipe,HANDLE_FLAG_INHERIT,0);
    const HANDLE nothing=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,0,nullptr);
    STARTUPINFOW start{};start.cb=sizeof start;start.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;start.wShowWindow=SW_HIDE;
    start.hStdInput=nothing;start.hStdOutput=writePipe;start.hStdError=writePipe;
    PROCESS_INFORMATION process{};
    const BOOL started=CreateProcessW(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,g_folder.c_str(),&start,&process);
    CloseHandle(writePipe);if(nothing!=INVALID_HANDLE_VALUE)CloseHandle(nothing);
    if(!started){CloseHandle(readPipe);line("ERROR: could not start Windows PowerShell.");return 0xFFFFFFFF;}
    CloseHandle(process.hThread);
    char buffer[4096];DWORD got=0;std::string pending;
    while(ReadFile(readPipe,buffer,sizeof buffer,&got,nullptr)&&got)
        for(DWORD i=0;i<got;i++){const char c=buffer[i];if(c=='\r'||c=='\n'){if(!pending.empty())line(pending);pending.clear();}else pending+=c;}
    if(!pending.empty())line(pending);
    CloseHandle(readPipe);WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    return code;
}
std::wstring updaterCommand(const wchar_t* argument){return L"\""+powerShell()+L"\" -NoLogo -NoProfile -ExecutionPolicy Bypass -File \""+updaterPath()+L"\" "+argument;}
void post(UINT message,WPARAM w,std::wstring text){PostMessageW(g_window,message,w,reinterpret_cast<LPARAM>(new std::wstring(std::move(text))));}
// Two lines: what is installed and the newest release; a third with what the last update said.
void showVersions(){
    std::wstring text=L"Installed: "+(g_installedVersion.empty()?std::wstring(L"unknown"):g_installedVersion);
    if(!exists(updaterPath()))text+=L"\nNewest: EnshroudedVR\\Update-EnshroudedVR.ps1 is missing";
    else if(!g_latestChecked)text+=L"\nNewest: checking...";
    else if(g_latestVersion.empty())text+=L"\nNewest: could not check (no internet?)";
    else text+=L"\nNewest: "+g_latestVersion+(upToDate()?L"  (you have it / 最新です)":L"");
    if(!g_updateNote.empty())text+=L"\n"+g_updateNote;
    SetWindowTextW(g_stUpdate,text.c_str());
}
void refreshUpdateButton(){EnableWindow(g_btUpdate,!g_running&&!g_zipping&&!g_updating&&exists(updaterPath())&&!upToDate());}
void startLatestCheck(){
    if(!exists(updaterPath())){g_latestChecked=true;showVersions();return;}
    const std::wstring command=updaterCommand(L"-CheckOnly");
    std::thread([command]{std::wstring latest;
        runCaptured(command,[&](const std::string& s){const auto w=oem(trim(s));const std::wstring key=L"Latest:";
            if(w.rfind(key,0)==0){std::wstringstream in(w.substr(key.size()));std::wstring name,version;in>>name>>version;latest=version;}});
        post(WM_APP_LATEST,0,latest);}).detach();
}
void startUpdate(){
    auto note=[](const wchar_t* text,bool bad){g_updateNote=text;g_updateNoteBad=bad;showVersions();};
    if(gameRunning()){note(L"Close the game first. / ゲームを閉じてください",true);return;}
    if(!exists(updaterPath())){note(L"EnshroudedVR\\Update-EnshroudedVR.ps1 is missing. Extract the mod again.",true);return;}
    const std::wstring old=selfPath()+L".old";DeleteFileW(old.c_str());
    g_renamedForUpdate=MoveFileW(selfPath().c_str(),old.c_str())!=0;
    g_updating=true;g_updateResult.clear();g_updateError.clear();
    for(HWND h:g_controls)EnableWindow(h,FALSE);EnableWindow(g_btDefaults,FALSE);EnableWindow(g_btZip,FALSE);refreshUpdateButton();
    note(L"Updating... please wait. / 更新中です",false);
    const std::wstring command=updaterCommand(L"-Yes");
    std::thread([command]{
        const DWORD code=runCaptured(command,[](const std::string& s){const auto w=oem(trim(s));if(!w.empty())post(WM_APP_UPDATE_LINE,0,w);});
        PostMessageW(g_window,WM_APP_UPDATE_DONE,code,0);}).detach();
}
void updateLine(const std::wstring& line){
    // Each step as the updater says it; its result line or its error is what stays shown.
    const bool bad=line.rfind(L"ERROR:",0)==0;
    if(bad)g_updateError=line.substr(std::min<size_t>(line.size(),7));
    const bool result=line.rfind(L"Updated to EnshroudedVR",0)==0||line.rfind(L"Already up to date",0)==0;
    if(result)g_updateResult=line;
    if(result||bad||g_updateResult.empty()){g_updateNote=bad?g_updateError:line;g_updateNoteBad=bad;showVersions();}
}
void updateDone(DWORD code){
    g_updating=false;
    const std::wstring self=selfPath(),old=self+L".old";const bool fresh=exists(self);
    if(g_renamedForUpdate&&!fresh)MoveFileW(old.c_str(),self.c_str());   // nothing new came: put it back
    if(code==0&&g_renamedForUpdate&&fresh){
        g_updateNote=L"Updated. Starting the new version... / 更新しました";g_updateNoteBad=false;showVersions();
        ShellExecuteW(nullptr,L"open",self.c_str(),nullptr,g_folder.c_str(),SW_SHOWNORMAL);DestroyWindow(g_window);return;}
    g_renamedForUpdate=false;
    if(code==0){g_updateNote=g_updateResult.empty()?L"Done.":g_updateResult;g_updateNoteBad=false;}
    else{g_updateNote=g_updateError.empty()?L"The update did not finish. Nothing was changed.":g_updateError;g_updateNoteBad=true;}
    g_installedVersion=installedVersion();showVersions();
    EnableWindow(g_btZip,!g_zipping);refreshRunning(true);
}
void build(){
    g_y=18;
    row(L"Update",L"Gets the newest version of this mod.\nYour settings are kept.\n最新版に更新します（設定はそのまま）");
    g_btUpdate=part(L"BUTTON",L"Update now",BS_PUSHBUTTON|WS_TABSTOP|WS_GROUP,kRight,g_y+6,120,30,ID_UPDATE);
    g_stUpdate=part(L"STATIC",L"",SS_LEFT|SS_NOPREFIX,kRight+134,g_y+4,kWidth-kRight-134-kLeft,64);
    g_y+=kRowHeight+4;
    row(L"Play mode",L"VR: play with the headset.\nNormal: play on your monitor, without VR.\nOther VR players' movements show in both.");
    g_rbVr=control(part(L"BUTTON",L"VR",BS_AUTORADIOBUTTON|WS_TABSTOP|WS_GROUP,kRight,g_y+8,120,22,ID_VR));
    g_rbFlat=control(part(L"BUTTON",L"Normal (no VR)",BS_AUTORADIOBUTTON|WS_TABSTOP,kRight+130,g_y+8,170,22,ID_FLAT));
    g_y+=kRowHeight;
    row(L"Picture size (VR)",L"Bigger is sharper, but gives lower FPS.");g_coSize=combo(ID_SIZE,kSizes,std::size(kSizes));g_y+=kRowHeight-16;
    row(L"DLSS (VR)",L"How the picture is made from a smaller one.");g_coDlss=combo(ID_DLSS,kDlss,std::size(kDlss));g_y+=kRowHeight-16;
    row(L"Sharpening (VR)",L"Makes edges crisper.");g_coSharp=combo(ID_SHARP,kSharp,std::size(kSharp));g_y+=kRowHeight-16;
    row(L"Frame mode (VR)",L"One eye a frame: the other shows its last picture, or is made from the drawn one.\n片目ずつ交互に描く（試験中・速い）。PureDark 方式は描いた目からもう片方を作る");g_coEyes=combo(ID_EYES,kEyes,std::size(kEyes));g_y+=kRowHeight-8;
    row(L"Problem report",L"The logs in one zip, next to this program.\nAlso finishes a VR bug report a crash cut.\n報告用zip。途切れたVRバグ報告も仕上げます");
    g_btZip=part(L"BUTTON",L"Make zip",BS_PUSHBUTTON|WS_TABSTOP|WS_GROUP,kRight,g_y+6,120,30,ID_ZIP);
    g_stZip=part(L"STATIC",L"",SS_LEFT,kRight+134,g_y+4,kWidth-kRight-134-kLeft,56);
    g_y+=kRowHeight+4;
    g_btDefaults=part(L"BUTTON",L"Put settings back",BS_PUSHBUTTON|WS_TABSTOP,kLeft,g_y,150,30,ID_DEFAULTS);
    g_y+=42;
    g_status=part(L"STATIC",L"",SS_LEFT,kLeft,g_y,kWidth-2*kLeft,52);g_y+=56;
    g_last=part(L"STATIC",L"",SS_LEFT|SS_NOPREFIX,kLeft,g_y,kWidth-2*kLeft,52);g_y+=58;
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){
    switch(message){
    case WM_COMMAND:{const int id=LOWORD(wParam),code=HIWORD(wParam);
        if(code==BN_CLICKED)switch(id){
            case ID_VR:store("Mode","VR","1");break;case ID_FLAT:store("Mode","VR","0");break;
            case ID_ZIP:startZip();break;
            case ID_UPDATE:startUpdate();break;
            case ID_DEFAULTS:if(!gameRunning()&&MessageBoxW(hwnd,L"Put all settings back to how they came?",L"Enshrouded VR setting",MB_YESNO|MB_ICONQUESTION)==IDYES){
                std::string defaults;if(readFile(defaultsPath(),defaults)&&writeFile(iniPath(),defaults)){g_ini=defaults;show();}else status(L"EnshroudedVR\\EnshroudedVR.defaults.ini is missing.");}break;}
        if(code==CBN_SELCHANGE){const auto at=size_t(SendMessageW(reinterpret_cast<HWND>(lParam),CB_GETCURSEL,0,0));
            if(id==ID_SIZE&&at<std::size(kSizes))store("Picture","RenderScale",kSizes[at].value);
            if(id==ID_DLSS&&at<std::size(kDlss))store("Picture","Dlss",kDlss[at].value);
            if(id==ID_SHARP&&at<std::size(kSharp))store("Picture","Sharpness",kSharp[at].value);
            if(id==ID_EYES&&at<std::size(kEyes)){store("Picture","AlternateEyes",at?"1":"0");store("Picture","EyeWarp",at==2?"2":"0");}}
        return 0;}
    case WM_APP_ZIP_DONE:{
        std::unique_ptr<std::wstring> zip(reinterpret_cast<std::wstring*>(lParam));g_zipping=false;EnableWindow(g_btZip,TRUE);
        if(wParam==0&&exists(*zip)){SetWindowTextW(g_stZip,(L"Saved in the game folder:\n"+zip->substr(zip->find_last_of(L'\\')+1)).c_str());
            if(!GetEnvironmentVariableW(L"EVR_SETTING_NO_EXPLORER",nullptr,0))ShellExecuteW(nullptr,L"open",L"explorer.exe",(L"/select,\""+*zip+L"\"").c_str(),nullptr,SW_SHOWNORMAL);}
        else SetWindowTextW(g_stZip,L"Could not make the zip.");
        return 0;}
    case WM_APP_LATEST:{std::unique_ptr<std::wstring> v(reinterpret_cast<std::wstring*>(lParam));g_latestVersion=*v;g_latestChecked=true;showVersions();refreshUpdateButton();return 0;}
    case WM_APP_UPDATE_LINE:{std::unique_ptr<std::wstring> line(reinterpret_cast<std::wstring*>(lParam));updateLine(*line);return 0;}
    case WM_APP_UPDATE_DONE:updateDone(static_cast<DWORD>(wParam));return 0;
    case WM_CLOSE:if(g_updating){MessageBoxW(hwnd,L"Please wait until the update is done.\n\n更新が終わるまでお待ちください。",L"Enshrouded VR setting",MB_ICONINFORMATION);return 0;}break;
    case WM_TIMER:refreshRunning();return 0;
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(wParam),TRANSPARENT);return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_DESTROY:PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    g_instance=instance;
    INITCOMMONCONTROLSEX controls{sizeof controls,ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    wchar_t self[MAX_PATH]{};GetModuleFileNameW(nullptr,self,MAX_PATH);g_folder=self;g_folder.erase(g_folder.find_last_of(L'\\'));
    // "--bug-report <folder> <start> <end>" (hooks/bug_report.hpp): the VR bug report's zip, without a window.
    {int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
     if(argv&&argc>=5&&std::wstring(argv[1])==L"--bug-report"){const std::wstring bugFolder=argv[2];const long long start=_wtoi64(argv[3]),end=_wtoi64(argv[4]);const std::wstring language=argc>=6?argv[5]:L"";LocalFree(argv);return makeBugReport(bugFolder,start,end,language);}
     // "--watch-bug-report <folder> <pid>" (hooks/bug_report.hpp, with the 30 seconds): finishes the report if the game goes first.
     if(argv&&argc>=4&&std::wstring(argv[1])==L"--watch-bug-report"){const std::wstring bugFolder=argv[2];const DWORD pid=DWORD(_wtoi64(argv[3]));LocalFree(argv);return watchBugReport(bugFolder,pid);}
     // "--problem-report [root]" (tests): what "Make zip" does, without a window; root is where cut reports are looked for.
     if(argv&&argc>=2&&std::wstring(argv[1])==L"--problem-report"){const std::wstring root=argc>=3?argv[2]:modPath()+L"\\logs";LocalFree(argv);
        Unfinished u;std::wstring zip;return findUnfinished(root,2,u)?makeBugReport(u.folder,u.start,u.end,u.language,true,&zip):makeLogsZip(zip);}
     if(argv)LocalFree(argv);}
    {const HDC screen=GetDC(nullptr);g_dpi=GetDeviceCaps(screen,LOGPIXELSX);ReleaseDC(nullptr,screen);}
    NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof metrics;SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof metrics,&metrics,0);
    g_font=CreateFontIndirectW(&metrics.lfMessageFont);metrics.lfMessageFont.lfWeight=FW_SEMIBOLD;g_bold=CreateFontIndirectW(&metrics.lfMessageFont);
    WNDCLASSEXW windowClass{};windowClass.cbSize=sizeof windowClass;windowClass.lpfnWndProc=windowProc;windowClass.hInstance=instance;windowClass.lpszClassName=L"EnshroudedVRSetting";
    windowClass.hCursor=LoadCursorW(nullptr,IDC_ARROW);windowClass.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    windowClass.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));windowClass.hIconSm=windowClass.hIcon;
    if(!RegisterClassExW(&windowClass))return 1;
    const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    g_window=CreateWindowExW(WS_EX_CONTROLPARENT,windowClass.lpszClassName,L"Enshrouded VR setting",style,CW_USEDEFAULT,CW_USEDEFAULT,S(kWidth),S(600),nullptr,nullptr,instance,nullptr);
    if(!g_window)return 1;
    build();
    RECT wanted{0,0,S(kWidth),S(g_y)};AdjustWindowRectEx(&wanted,style,FALSE,WS_EX_CONTROLPARENT);
    SetWindowPos(g_window,nullptr,0,0,wanted.right-wanted.left,wanted.bottom-wanted.top,SWP_NOMOVE|SWP_NOZORDER);
    // Left behind by an update that replaced this program.
    DeleteFileW((selfPath()+L".old").c_str());
    if(load()){::show();refreshRunning(true);}else for(HWND h:g_controls)EnableWindow(h,FALSE);
    g_installedVersion=installedVersion();showVersions();startLatestCheck();refreshUpdateButton();
    SetTimer(g_window,1,1000,nullptr);
    ShowWindow(g_window,show);UpdateWindow(g_window);
    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(g_window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    return 0;
}
