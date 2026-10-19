#pragma once
// A keyboard script for Mock trials (diagnostic; nothing of it is active without EVR_KEY_SCRIPT in the game's
// environment). The user asked on 2026-10-13 to check, while they sleep, that changing the items of the graphics
// settings neither ends the session nor breaks anything; those pages are reached with the keyboard (ESC, down, Enter,
// Q / E for the tabs, the arrows for the items and their values - recorded from the user's own hands in
// game-20261013-025335/keyboard-operations.jsonl).
// The game takes its keyboard from raw input only: its window procedure (da6050) has no case for WM_KEYDOWN / WM_KEYUP;
// WM_INPUT goes to d9f3e0, which asks GetRawInputData twice (the size, then the data, header size 0x18) and hands a
// keyboard record (header type 1) to d9f160. So a key of the script is a WM_INPUT message posted to the game's own
// window with a handle of our own range, and the executable's import of GetRawInputData answers that handle with the
// record of the key (make code, E0 flag for the arrows, virtual key, down or up). Every other handle goes to the
// system's function unchanged. Nothing is injected system wide, no real key is pressed, other programs see nothing.
// EVR_KEY_SCRIPT=<path of a text file>: one step a line, "<XR frame> <KEY>" (a tap: down at that frame, up six frames
// later) or "<XR frame> <KEY> down|up"; '#' starts a comment. Keys: ESC ENTER TAB SPACE UP DOWN LEFT RIGHT SHIFT CTRL
// and the letters and digits.
// Since 2026-10-14 (user: "I record the walk again: keep the mouse's turning too; I press F8 where the faults show, and
// the self-test turns round there taking pictures") also:
//   "<XR frame> MOUSE <dx> <dy>"              a relative mouse movement as the game reads it from raw input (d9f3e0, record
//                                             type 0: it skips absolute ones and scales lLastX / lLastY by its own factor)
//   "<XR frame> RAWKEY <make> <flags> <vkey>" a keyboard record exactly as one was read (tools/save_route.py writes them)
//   "<XR frame> CAPTURE"                      asks the layer for one picture of both eyes (<game folder>/xr-capture.request,
//                                             taken on its next 15th pair)
//   "<XR frame> BURST <n>"                    asks for n (1..32) consecutive pictures of both eyes (<game folder>/
//                                             xr-burst.request; up to 4 a session) - a flicker of one frame in thirty
//   "<XR frame> STOP"                         ends the session as its minute limit does (<probe folder>/stop.request)
//   "<XR frame> TURN <yaw> <pitch>" / "TURN OFF"  turns the Mock head: yaw added to its pose script's (a positive yaw turns
//                                             LEFT - user watching a Mock, 2026-10-19: "TURN 130 looks left"), pitch instead
//                                             of it, degrees (the Mock runtime's EVR_MockHeadTurn; nothing on a headset)
//   "<XR frame> MARK <word>"                  only a line in the log (key_script_mark), to find a place of the script
//   "<XR frame> MBUTTON down|up"             the middle mouse button (2026-10-19, the game's combat target lock), as a
//                                             raw mouse record with no movement
// A step may be timed instead: "t<milliseconds> <step>" runs that long after the frame of "<XR frame> ORIGIN". The
// game moves by time, not by frames, and a replay runs at another frame rate than the walk it repeats (Mock replay
// game-20261014-184902: 24.5 ms a frame against the user's 20.5, 5 m away after 600 frames); tools/save_route.py
// writes the walks timed from the record's own times.
// EVR_INPUT_RECORD=1: every keyboard and mouse record that the game reads is written, by XR frame, to
// <game folder>/input-record.jsonl - the mouse summed per frame. The keys and the turning of a walk, for repeating it.
namespace keyScript {
enum class Kind:uint8_t{Key,Mouse,Capture,Stop,Turn,Mark,Origin,Burst};
struct Step {uint64_t frame=0;uint16_t make=0,vkey=0,flags=0;bool extended=false,down=true,rawFlags=false,timed=false,turnOn=false;Kind kind=Kind::Key;int32_t dx=0,dy=0;uint16_t buttons=0;float yaw=0,pitch=0;std::string name;};
inline std::vector<Step> steps,timedSteps;inline size_t next=0,nextTimed=0;inline bool installed=false;
inline const bool exclusive=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_KEY_SCRIPT_EXCLUSIVE",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline uint64_t originUs=0;   // steady clock, microseconds (as the probe log's "us"), when ORIGIN was reached
inline uint64_t steadyUs(){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
using HeadTurnFn=void(*)(int,float,float);inline HeadTurnFn headTurn=nullptr;inline bool headTurnLooked=false;
using RawFn=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
inline RawFn nativeRaw=nullptr;inline uintptr_t slot=0;
inline std::mutex mutex;inline std::array<RAWINPUT,256> ring{};
inline std::atomic<uint64_t> posted{0},served{0},captures{0};inline std::atomic<int> callbacks{0};
constexpr uintptr_t handleBase=0x4556520000ull;   // "EVR" and sixteen bits of index: not a handle the system gives out
struct CallbackScope{CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
// The record (EVR_INPUT_RECORD): what the game's window thread read since the last XR frame.
namespace record {
inline bool on=false;inline HANDLE file=INVALID_HANDLE_VALUE;inline std::mutex mutex;
inline int64_t mouseX=0,mouseY=0;inline uint32_t mouseReads=0,mouseButtons=0;inline std::vector<RAWKEYBOARD> keys;
inline uint64_t lastFrame=0,lines=0,mouseFrames=0,keyRecords=0;
inline void note(const RAWINPUT* in,UINT got){
    if(!in||got<sizeof(RAWINPUTHEADER))return;
    std::lock_guard<std::mutex> lock(mutex);
    if(in->header.dwType==RIM_TYPEMOUSE&&got>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE)){
        const auto& m=in->data.mouse;mouseButtons|=m.usButtonFlags;
        if(!(m.usFlags&MOUSE_MOVE_ABSOLUTE)){mouseX+=m.lLastX;mouseY+=m.lLastY;++mouseReads;}
    }else if(in->header.dwType==RIM_TYPEKEYBOARD&&got>=sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD)&&keys.size()<256)keys.push_back(in->data.keyboard);
}
}
inline UINT WINAPI raw(HRAWINPUT handle,UINT command,LPVOID data,PUINT size,UINT header){
    CallbackScope scope;const auto value=reinterpret_cast<uintptr_t>(handle);
    if((value&~uintptr_t(0xffff))==handleBase&&command==RID_INPUT&&size&&header==sizeof(RAWINPUTHEADER)){
        RAWINPUT input{};{std::lock_guard<std::mutex> lock(mutex);input=ring[(value&0xffff)%ring.size()];}
        const UINT need=input.header.dwSize;
        if(!data){*size=need;return 0;}
        if(*size<need){*size=need;return UINT(-1);}
        std::memcpy(data,&input,need);++served;
        if(record::on)record::note(&input,need);
        return need;
    }
    const UINT got=nativeRaw(handle,command,data,size,header);
    // While a script plays, the real mouse and keyboard do nothing (2026-10-14: the user touched the mouse before a
    // self-test and the walk started turned by 1.8 degrees). EVR_KEY_SCRIPT_EXCLUSIVE=0 lets them through.
    if(exclusive&&!steps.empty()&&command==RID_INPUT&&data&&got!=UINT(-1)&&got>=sizeof(RAWINPUTHEADER)){
        auto* in=static_cast<RAWINPUT*>(data);
        if(in->header.dwType==RIM_TYPEMOUSE&&got>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE)){in->data.mouse.lLastX=in->data.mouse.lLastY=0;in->data.mouse.usButtonFlags=0;}
        else if(in->header.dwType==RIM_TYPEKEYBOARD&&got>=sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD)){in->data.keyboard.MakeCode=0;in->data.keyboard.VKey=0xff;}
    }
    if(record::on&&command==RID_INPUT&&data&&got!=UINT(-1))record::note(static_cast<const RAWINPUT*>(data),got);
    return got;
}
// The executable's own import slot of a function (its import table; nothing outside the executable's image).
inline uintptr_t importSlot(const char* dll,const char* name){
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 0;
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE)return 0;
    const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!directory.VirtualAddress)return 0;
    for(auto d=reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base+directory.VirtualAddress);d->Name;++d){
        if(_stricmp(reinterpret_cast<const char*>(base+d->Name),dll)||!d->OriginalFirstThunk)continue;
        auto names=reinterpret_cast<const IMAGE_THUNK_DATA64*>(base+d->OriginalFirstThunk);auto slots=reinterpret_cast<const IMAGE_THUNK_DATA64*>(base+d->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
            if(!std::strcmp(reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData)->Name,name))return reinterpret_cast<uintptr_t>(&slots->u1.Function);
        }
    }
    return 0;
}
inline bool swap(uintptr_t location,uintptr_t expected,uintptr_t value){
    DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(location),8,PAGE_READWRITE,&old))return false;
    const bool done=InterlockedCompareExchangePointer(reinterpret_cast<void**>(location),reinterpret_cast<void*>(value),reinterpret_cast<void*>(expected))==reinterpret_cast<void*>(expected);
    DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(location),8,old,&ignored);return done;
}
inline bool key(const std::string& name,Step& s){
    struct Key{const char* name;uint16_t make,vkey;bool extended;};
    static const Key keys[]={{"ESC",0x01,VK_ESCAPE,false},{"ENTER",0x1c,VK_RETURN,false},{"TAB",0x0f,VK_TAB,false},{"SPACE",0x39,VK_SPACE,false},
        {"UP",0x48,VK_UP,true},{"DOWN",0x50,VK_DOWN,true},{"LEFT",0x4b,VK_LEFT,true},{"RIGHT",0x4d,VK_RIGHT,true},
        {"Q",0x10,'Q',false},{"W",0x11,'W',false},{"E",0x12,'E',false},{"R",0x13,'R',false},{"T",0x14,'T',false},{"Y",0x15,'Y',false},{"U",0x16,'U',false},{"I",0x17,'I',false},{"O",0x18,'O',false},{"P",0x19,'P',false},
        {"A",0x1e,'A',false},{"S",0x1f,'S',false},{"D",0x20,'D',false},{"F",0x21,'F',false},{"G",0x22,'G',false},{"H",0x23,'H',false},{"J",0x24,'J',false},{"K",0x25,'K',false},{"L",0x26,'L',false},
        {"Z",0x2c,'Z',false},{"X",0x2d,'X',false},{"C",0x2e,'C',false},{"V",0x2f,'V',false},{"B",0x30,'B',false},{"N",0x31,'N',false},{"M",0x32,'M',false},
        {"SHIFT",0x2a,VK_SHIFT,false},{"CTRL",0x1d,VK_CONTROL,false},
        {"1",0x02,'1',false},{"2",0x03,'2',false},{"3",0x04,'3',false},{"4",0x05,'4',false},{"5",0x06,'5',false},{"6",0x07,'6',false},{"7",0x08,'7',false},{"8",0x09,'8',false},{"9",0x0a,'9',false},{"0",0x0b,'0',false}};
    for(const auto& k:keys)if(name==k.name){s.make=k.make;s.vkey=k.vkey;s.extended=k.extended;s.name=name;return true;}
    return false;
}
// The game folder (the parent of the probe's own folder), where the layer looks for its request files.
inline std::filesystem::path gameFolder(){return calibrationDirectory.empty()?std::filesystem::path{}:calibrationDirectory.parent_path();}
inline void install(){
    char path[512]{};const auto n=GetEnvironmentVariableA("EVR_KEY_SCRIPT",path,sizeof path);
    const bool scripted=n&&n<sizeof path;
    char wanted[8]{};const auto r=GetEnvironmentVariableA("EVR_INPUT_RECORD",wanted,sizeof wanted);
    const bool recording=r&&r<sizeof wanted&&!std::strcmp(wanted,"1");
    if(!scripted&&!recording)return;
    if(recording){
        try{
            const auto folder=gameFolder();if(folder.empty())throw std::runtime_error("no game folder");
            record::file=CreateFileW((folder/"input-record.jsonl").wstring().c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(record::file==INVALID_HANDLE_VALUE)throw std::runtime_error("input record not opened");
            record::on=true;
        }catch(const std::exception& e){logFile->event("input_record_failure","\"reason\":"+jsonString(e.what()));}
    }
    if(!scripted){
        // Recording only: the import is wrapped all the same (it reads what the system gives the game).
        const auto user=GetModuleHandleW(L"user32.dll");const auto native=user?reinterpret_cast<uintptr_t>(GetProcAddress(user,"GetRawInputData")):0;
        slot=importSlot("user32.dll","GetRawInputData");
        bool wrapped=false;
        if(record::on&&slot&&native&&*reinterpret_cast<const uintptr_t*>(slot)==native){nativeRaw=reinterpret_cast<RawFn>(native);wrapped=swap(slot,native,reinterpret_cast<uintptr_t>(&raw));}
        if(!wrapped){
            if(record::on){record::on=false;CloseHandle(record::file);record::file=INVALID_HANDLE_VALUE;logFile->event("input_record_failure","\"reason\":\"the executable's import of GetRawInputData is not as expected\"");}
            slot=0;return;}
        installed=true;
        logFile->event("input_record_enabled","\"file\":\"input-record.jsonl\",\"script\":false,\"diagnostic\":true");
        return;
    }
    try{
        // Read with the system's own calls: this header is also compiled where <fstream> is not included.
        std::string text;
        {const HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
         if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("key script not readable");
         const DWORD size=GetFileSize(file,nullptr);DWORD got=0;
         if(size==INVALID_FILE_SIZE||size>16777216){CloseHandle(file);throw std::runtime_error("key script size");}
         text.resize(size);const BOOL ok=size?ReadFile(file,text.data(),size,&got,nullptr):TRUE;CloseHandle(file);
         if(!ok||got!=size)throw std::runtime_error("key script not read");}
        unsigned row=0;size_t at=0;
        while(at<text.size()){
            const auto end=std::min(text.find(char(10),at),text.size());std::string line=text.substr(at,end-at);at=end+1;
            ++row;if(const auto hash=line.find('#');hash!=std::string::npos)line.erase(hash);
            std::istringstream in(line);uint64_t frame=0;std::string name,how,when;
            if(!(in>>when))continue;
            Step s;
            try{size_t used=0;
                if(when[0]=='t'){s.timed=true;frame=std::stoull(when.substr(1),&used);++used;}else frame=std::stoull(when,&used);
                if(used!=when.size())throw std::runtime_error("");
            }catch(...){throw std::runtime_error("key script line "+std::to_string(row));}
            s.frame=frame;
            auto& list=s.timed?timedSteps:steps;
            if(!(in>>name)||(!frame&&!s.timed))throw std::runtime_error("key script line "+std::to_string(row));
            if(name=="ORIGIN"&&!s.timed){s.kind=Kind::Origin;s.name=name;list.push_back(s);continue;}
            if(name=="MARK"){s.kind=Kind::Mark;if(!(in>>s.name))throw std::runtime_error("key script line "+std::to_string(row));list.push_back(s);continue;}
            if(name=="TURN"){
                s.kind=Kind::Turn;s.name=name;std::string first;if(!(in>>first))throw std::runtime_error("key script line "+std::to_string(row));
                if(first!="OFF"){try{s.yaw=std::stof(first);}catch(...){throw std::runtime_error("key script line "+std::to_string(row));}
                    if(!(in>>s.pitch))throw std::runtime_error("key script line "+std::to_string(row));s.turnOn=true;}
                list.push_back(s);continue;}
            if(name=="MOUSE"){s.kind=Kind::Mouse;if(!(in>>s.dx>>s.dy))throw std::runtime_error("key script line "+std::to_string(row));s.name=name;list.push_back(s);continue;}
            if(name=="MBUTTON"){s.kind=Kind::Mouse;std::string how2;if(!(in>>how2)||(how2!="down"&&how2!="up"))throw std::runtime_error("key script line "+std::to_string(row));
                s.down=how2=="down";s.buttons=s.down?RI_MOUSE_MIDDLE_BUTTON_DOWN:RI_MOUSE_MIDDLE_BUTTON_UP;s.name=name;list.push_back(s);continue;}
            if(name=="CAPTURE"){s.kind=Kind::Capture;s.name=name;list.push_back(s);continue;}
            if(name=="BURST"){s.kind=Kind::Burst;s.name=name;if(!(in>>s.dx)||s.dx<1||s.dx>32)throw std::runtime_error("key script line "+std::to_string(row));list.push_back(s);continue;}
            if(name=="STOP"){s.kind=Kind::Stop;s.name=name;list.push_back(s);continue;}
            if(name=="RAWKEY"){
                unsigned make=0,flags=0,vkey=0;if(!(in>>make>>flags>>vkey)||make>0xffff||flags>0xffff||vkey>0xffff)throw std::runtime_error("key script line "+std::to_string(row));
                s.make=uint16_t(make);s.flags=uint16_t(flags);s.vkey=uint16_t(vkey);s.rawFlags=true;s.down=!(flags&RI_KEY_BREAK);s.name="VK_"+std::to_string(vkey);list.push_back(s);continue;}
            if(!key(name,s))throw std::runtime_error("key script line "+std::to_string(row));
            if(in>>how){if(how=="down")s.down=true;else if(how=="up")s.down=false;else throw std::runtime_error("key script line "+std::to_string(row));list.push_back(s);}
            else{s.down=true;list.push_back(s);s.down=false;s.frame=frame+(s.timed?150:6);list.push_back(s);}
        }
        if((steps.empty()&&timedSteps.empty())||steps.size()+timedSteps.size()>400000)throw std::runtime_error("key script empty or too long");
        if(!timedSteps.empty()&&std::none_of(steps.begin(),steps.end(),[](const Step& s){return s.kind==Kind::Origin;}))throw std::runtime_error("timed steps without ORIGIN");
        std::stable_sort(steps.begin(),steps.end(),[](const Step& a,const Step& b){return a.frame<b.frame;});
        std::stable_sort(timedSteps.begin(),timedSteps.end(),[](const Step& a,const Step& b){return a.frame<b.frame;});
        const auto user=GetModuleHandleW(L"user32.dll");const auto native=user?reinterpret_cast<uintptr_t>(GetProcAddress(user,"GetRawInputData")):0;
        slot=importSlot("user32.dll","GetRawInputData");
        if(!slot||!native||*reinterpret_cast<const uintptr_t*>(slot)!=native)throw std::runtime_error("the executable's import of GetRawInputData is not as expected");
        nativeRaw=reinterpret_cast<RawFn>(native);
        if(!swap(slot,native,reinterpret_cast<uintptr_t>(&raw))){slot=0;throw std::runtime_error("import slot not written");}
        installed=true;
        if(record::on)logFile->event("input_record_enabled","\"file\":\"input-record.jsonl\",\"script\":true,\"diagnostic\":true");
        logFile->event("key_script_enabled","\"steps\":"+std::to_string(steps.size())+",\"timed_steps\":"+std::to_string(timedSteps.size())+",\"first_frame\":"+std::to_string(steps.front().frame)+",\"last_frame\":"+std::to_string(steps.back().frame)+
            ",\"import_slot_rva\":"+hex(slot-reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)))+",\"messages_to_the_game_window_only\":true,\"system_input_injected\":false,\"diagnostic\":true");
    }catch(const std::exception& e){
        steps.clear();timedSteps.clear();logFile->event("key_script_failure","\"reason\":"+jsonString(e.what()));
        if(!installed&&record::on){record::on=false;CloseHandle(record::file);record::file=INVALID_HANDLE_VALUE;}
    }
}
// The name of a key of the table above, or VK_<n>.
inline std::string keyName(const RAWKEYBOARD& k){
    for(const char* n:{"ESC","ENTER","TAB","SPACE","UP","DOWN","LEFT","RIGHT","SHIFT","CTRL"}){Step s;if(key(n,s)&&s.vkey==k.VKey)return n;}
    if((k.VKey>='A'&&k.VKey<='Z')||(k.VKey>='0'&&k.VKey<='9'))return std::string(1,char(k.VKey));
    return "VK_"+std::to_string(k.VKey);
}
// What was read since the last XR frame, under that frame's number (a step of a script for frame F is posted at F and
// read before F + 1: so a record under F is posted again at F).
inline void flushRecord(uint64_t xrFrame){
    if(!record::on)return;
    int64_t x=0,y=0;uint32_t reads=0,buttons=0;std::vector<RAWKEYBOARD> keys;
    {std::lock_guard<std::mutex> lock(record::mutex);x=record::mouseX;y=record::mouseY;reads=record::mouseReads;buttons=record::mouseButtons;keys.swap(record::keys);
     record::mouseX=record::mouseY=0;record::mouseReads=record::mouseButtons=0;}
    const uint64_t frame=record::lastFrame?record::lastFrame:xrFrame;record::lastFrame=xrFrame;
    const std::string when="{\"frame\":"+std::to_string(frame)+",\"us\":"+std::to_string(steadyUs());   // the probe log's clock
    std::string text;
    if(x||y||buttons){
        text+=when+",\"mouse\":["+std::to_string(x)+","+std::to_string(y)+"],\"reads\":"+std::to_string(reads);
        if(buttons)text+=",\"buttons\":"+std::to_string(buttons);
        text+="}\n";++record::mouseFrames;
    }
    for(const auto& k:keys){
        text+=when+",\"key\":\""+keyName(k)+"\",\"make\":"+std::to_string(k.MakeCode)+",\"flags\":"+std::to_string(k.Flags)+
            ",\"vkey\":"+std::to_string(k.VKey)+",\"down\":"+((k.Flags&RI_KEY_BREAK)?"false":"true")+"}\n";++record::keyRecords;
    }
    if(text.empty())return;
    DWORD wrote=0;if(WriteFile(record::file,text.data(),DWORD(text.size()),&wrote,nullptr))++record::lines;
}
// Once per XR frame, on the thread that publishes the hands (next to menuPointer::frame).
inline std::string when(const Step& s){return s.timed?",\"scripted_ms\":"+std::to_string(s.frame):",\"scripted_frame\":"+std::to_string(s.frame);}
inline void perform(const Step& s,uint64_t xrFrame,HWND window){
        if(s.kind==Kind::Origin){
            originUs=steadyUs();
            logFile->event("key_script_origin","\"frame\":"+std::to_string(xrFrame)+",\"origin_us\":"+std::to_string(originUs)+",\"timed_steps\":"+std::to_string(timedSteps.size()));
            return;
        }
        if(s.kind==Kind::Mark){logFile->event("key_script_mark","\"frame\":"+std::to_string(xrFrame)+when(s)+",\"mark\":"+jsonString(s.name.c_str()));return;}
        if(s.kind==Kind::Turn){
            if(!headTurnLooked){headTurnLooked=true;
                if(const auto mock=GetModuleHandleW(L"evr_mock_runtime.dll"))headTurn=reinterpret_cast<HeadTurnFn>(GetProcAddress(mock,"EVR_MockHeadTurn"));}
            if(headTurn)headTurn(s.turnOn?1:0,s.yaw,s.pitch);
            logFile->event("key_script_turn","\"frame\":"+std::to_string(xrFrame)+when(s)+",\"on\":"+(s.turnOn?"true":"false")+",\"yaw_added\":"+std::to_string(s.yaw)+
                ",\"pitch\":"+std::to_string(s.pitch)+",\"mock_head\":"+(headTurn?"true":"false"));
            return;
        }
        if(s.kind==Kind::Capture){
            // One picture of both eyes: the layer takes <game folder>/xr-capture.request on its next 15th pair.
            const auto folder=gameFolder();bool asked=false;
            if(!folder.empty()){const HANDLE f=CreateFileW((folder/"xr-capture.request").wstring().c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(f!=INVALID_HANDLE_VALUE){CloseHandle(f);asked=true;++captures;}}
            logFile->event("key_script_capture","\"frame\":"+std::to_string(xrFrame)+when(s)+",\"asked\":"+(asked?"true":"false"));
            return;
        }
        if(s.kind==Kind::Burst){
            const auto folder=gameFolder();bool asked=false;
            if(!folder.empty()){const HANDLE f=CreateFileW((folder/"xr-burst.request").wstring().c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(f!=INVALID_HANDLE_VALUE){const auto text=std::to_string(s.dx)+" 4";DWORD written=0;
                    asked=WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size();CloseHandle(f);if(asked)++captures;}}
            logFile->event("key_script_burst","\"frame\":"+std::to_string(xrFrame)+when(s)+",\"count\":"+std::to_string(s.dx)+",\"asked\":"+(asked?"true":"false"));
            return;
        }
        if(s.kind==Kind::Stop){
            // The end of the script: the session closes the game normally, as at its minute limit.
            bool asked=false;
            if(!calibrationDirectory.empty()){const HANDLE f=CreateFileW((calibrationDirectory/"stop.request").wstring().c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(f!=INVALID_HANDLE_VALUE){CloseHandle(f);asked=true;}}
            logFile->event("key_script_stop","\"frame\":"+std::to_string(xrFrame)+when(s)+",\"asked\":"+(asked?"true":"false"));
            return;
        }
        const auto index=posted.fetch_add(1);RAWINPUT input{};
        if(s.kind==Kind::Mouse){
            input.header.dwType=RIM_TYPEMOUSE;input.header.dwSize=UINT(sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE));
            input.data.mouse.usFlags=MOUSE_MOVE_RELATIVE;input.data.mouse.lLastX=s.dx;input.data.mouse.lLastY=s.dy;input.data.mouse.usButtonFlags=s.buttons;
        }else{
            input.header.dwType=RIM_TYPEKEYBOARD;input.header.dwSize=UINT(sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD));
            auto& k=input.data.keyboard;k.MakeCode=s.make;k.Flags=s.rawFlags?s.flags:USHORT((s.down?RI_KEY_MAKE:RI_KEY_BREAK)|(s.extended?RI_KEY_E0:0));
            k.VKey=s.vkey;k.Message=s.down?WM_KEYDOWN:WM_KEYUP;
        }
        {std::lock_guard<std::mutex> lock(mutex);ring[index%ring.size()]=input;}
        const BOOL ok=PostMessageW(window,WM_INPUT,RIM_INPUT,LPARAM(handleBase+(index%ring.size())));
        if(s.kind==Kind::Mouse&&!s.buttons)return;   // one a frame while turning: the posts are counted in key_script_removed
        logFile->event("key_script_step","\"frame\":"+std::to_string(xrFrame)+when(s)+",\"key\":"+jsonString(s.name.c_str())+",\"down\":"+(s.down?"true":"false")+
            ",\"posted\":"+(ok?"true":"false")+",\"served\":"+std::to_string(served.load()));
}
inline void frame(uint64_t xrFrame,HWND window){
    if(!installed)return;
    flushRecord(xrFrame);
    if(!window)return;
    while(next<steps.size()&&steps[next].frame<=xrFrame)perform(steps[next++],xrFrame,window);
    if(!originUs)return;
    const uint64_t elapsedMs=(steadyUs()-originUs)/1000;
    while(nextTimed<timedSteps.size()&&timedSteps[nextTimed].frame<=elapsedMs)perform(timedSteps[nextTimed++],xrFrame,window);
}
inline void disable(){
    if(!installed)return;
    installed=false;
    const bool restored=slot&&swap(slot,reinterpret_cast<uintptr_t>(&raw),reinterpret_cast<uintptr_t>(nativeRaw));
    const auto deadline=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(record::on){flushRecord(record::lastFrame);record::on=false;CloseHandle(record::file);record::file=INVALID_HANDLE_VALUE;
        logFile->event("input_record_closed","\"lines\":"+std::to_string(record::lines)+",\"mouse_frames\":"+std::to_string(record::mouseFrames)+",\"key_records\":"+std::to_string(record::keyRecords));}
    logFile->event("key_script_removed","\"posted\":"+std::to_string(posted.load())+",\"served\":"+std::to_string(served.load())+",\"captures\":"+std::to_string(captures.load())+
        ",\"steps\":"+std::to_string(steps.size())+",\"timed_steps\":"+std::to_string(timedSteps.size())+",\"timed_done\":"+std::to_string(nextTimed)+",\"import_restored\":"+(restored?"true":"false"));
    if(headTurn)headTurn(0,0,0);
}
}
