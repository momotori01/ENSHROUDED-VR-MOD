// Local, bounded exception capture. No hardware breakpoints, context writes,
// software breakpoint patches, registry changes or remote symbol requests.
#include <evr/log.hpp>
#include <windows.h>
#include <dbghelp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <intrin.h>
using namespace evr;
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
static uint64_t creationTime(HANDLE process){
    FILETIME created{},ended{},kernel{},user{};
    if(!GetProcessTimes(process,&created,&ended,&kernel,&user))throw std::runtime_error("GetProcessTimes failed");
    return (uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
}
static void handledFixtureException(){
    __try{RaiseException(0xe0425652,0,0,nullptr);}
    __except(GetExceptionCode()==0xe0425652?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){}
}
static int fixture(const std::string& mode,const std::filesystem::path& directory){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    std::cout<<GetCurrentProcessId()<<' '<<creationTime(GetCurrentProcess())<<std::endl;
    bool ran=false;
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(25);
    while(std::chrono::steady_clock::now()<end){
        if(!ran&&std::filesystem::exists(directory/"go")){
            ran=true;
            if(mode=="fastfail")__fastfail(FAST_FAIL_FATAL_APP_EXIT);
            if(mode=="handled"){handledFixtureException();std::ofstream(directory/"handled")<<"yes";}
        }
        if(std::filesystem::exists(directory/"stop"))return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
struct Attachment {
    DWORD pid=0;bool active=false;
    ~Attachment(){if(active)DebugActiveProcessStop(pid);}
};
int main(int argc,char** argv){
    try {
        if(argc==4&&std::string(argv[1])=="--fixture")return fixture(argv[2],argv[3]);
        DWORD pid=0;uint64_t created=0;unsigned seconds=300;
        std::filesystem::path expected,out;
        for(int i=1;i<argc;++i){
            const std::string a=argv[i];
            if(a=="--pid"&&i+1<argc)pid=std::stoul(argv[++i]);
            else if(a=="--created"&&i+1<argc)created=std::stoull(argv[++i]);
            else if(a=="--expected-path"&&i+1<argc)expected=argv[++i];
            else if(a=="--output"&&i+1<argc)out=argv[++i];
            else if(a=="--seconds"&&i+1<argc)seconds=std::stoul(argv[++i]);
            else throw std::runtime_error("invalid argument");
        }
        if(!pid||!created||expected.empty()||out.empty()||seconds<1||seconds>900)
            throw std::runtime_error("target identity and bounded duration required");
        Handle process{OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|SYNCHRONIZE,FALSE,pid)};
        if(!process.value)throw std::runtime_error("OpenProcess failed "+std::to_string(GetLastError()));
        wchar_t actual[32768]{};DWORD length=32768;
        if(!QueryFullProcessImageNameW(process.value,0,actual,&length)||
           !std::filesystem::equivalent(expected,std::filesystem::path(actual))||creationTime(process.value)!=created)
            throw std::runtime_error("target path or creation time differs");
        BOOL debugged=FALSE;
        if(!CheckRemoteDebuggerPresent(process.value,&debugged)||debugged)throw std::runtime_error("existing debugger refused");
        if(std::filesystem::exists(out))throw std::runtime_error("output must be a new directory");
        std::filesystem::create_directories(out);
        Log log((out/"exceptions.jsonl").string(),true);
        log.event("watch_identity","\"pid\":"+std::to_string(pid)+",\"creation_filetime\":"+std::to_string(created));
        Attachment attachment{pid,false};
        if(!DebugActiveProcess(pid))throw std::runtime_error("DebugActiveProcess failed "+std::to_string(GetLastError()));
        attachment.active=true;
        if(!DebugSetProcessKillOnExit(FALSE))throw std::runtime_error("unable to disable debugger kill-on-exit");
        log.event("watch_attached","\"kill_on_exit\":false,\"breakpoint_patches\":false,\"context_writes\":false");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        bool initialBreak=true,exited=false;unsigned dumps=0;
        while(std::chrono::steady_clock::now()<deadline&&!std::filesystem::exists(out/"stop.request")){
            DEBUG_EVENT event{};
            if(!WaitForDebugEvent(&event,100)){
                if(GetLastError()!=ERROR_SEM_TIMEOUT)throw std::runtime_error("WaitForDebugEvent failed "+std::to_string(GetLastError()));
                continue;
            }
            DWORD status=DBG_CONTINUE;
            try {
                if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT&&event.u.CreateProcessInfo.hFile)
                    CloseHandle(event.u.CreateProcessInfo.hFile);
                else if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT&&event.u.LoadDll.hFile)
                    CloseHandle(event.u.LoadDll.hFile);
                else if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){
                    log.event("watch_process_exit","\"exit_code\":"+std::to_string(event.u.ExitProcess.dwExitCode));exited=true;
                }else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
                    auto record=event.u.Exception.ExceptionRecord;
                    if(initialBreak&&record.ExceptionCode==EXCEPTION_BREAKPOINT){
                        initialBreak=false;log.event("watch_ready");
                    }else{
                        status=DBG_EXCEPTION_NOT_HANDLED;
                        const bool fatal=!event.u.Exception.dwFirstChance||record.ExceptionCode==0xc0000409;
                        if(fatal||record.ExceptionCode==0xc0000005||record.ExceptionCode==0xe0425652){
                            std::ostringstream f;f<<"\"thread\":"<<event.dwThreadId<<",\"first_chance\":"<<event.u.Exception.dwFirstChance
                                <<",\"code\":"<<record.ExceptionCode<<",\"address\":"<<reinterpret_cast<uintptr_t>(record.ExceptionAddress)<<",\"parameters\":[";
                            for(DWORD i=0;i<std::min<DWORD>(record.NumberParameters,EXCEPTION_MAXIMUM_PARAMETERS);++i){if(i)f<<',';f<<record.ExceptionInformation[i];}
                            f<<']';log.event("watch_exception",f.str());
                        }
                        if(fatal&&dumps<3){
                            Handle thread{OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,event.dwThreadId)};
                            CONTEXT context{};context.ContextFlags=CONTEXT_ALL;
                            const bool contextValid=thread.value&&GetThreadContext(thread.value,&context);
                            record.ExceptionRecord=nullptr; // chained pointer belongs to target; don't dereference locally
                            EXCEPTION_POINTERS pointers{&record,&context};
                            MINIDUMP_EXCEPTION_INFORMATION info{event.dwThreadId,&pointers,FALSE};
                            const auto path=out/("exception_"+std::to_string(++dumps)+".dmp");
                            Handle file{CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
                            const bool saved=file.value!=INVALID_HANDLE_VALUE&&contextValid&&MiniDumpWriteDump(process.value,pid,file.value,
                                MINIDUMP_TYPE(MiniDumpNormal|MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules|MiniDumpWithIndirectlyReferencedMemory),&info,nullptr,nullptr);
                            const auto error=saved?0:GetLastError();
                            log.event("watch_dump","\"saved\":"+std::string(saved?"true":"false")+",\"error\":"+std::to_string(error)+
                                ",\"context_valid\":"+(contextValid?"true":"false")+",\"rip\":"+std::to_string(context.Rip)+",\"rsp\":"+std::to_string(context.Rsp));
                        }
                    }
                }
            }catch(...){ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status);throw;}
            if(!ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status))throw std::runtime_error("ContinueDebugEvent failed");
            if(exited){attachment.active=false;break;}
        }
        if(!exited){
            const bool detached=DebugActiveProcessStop(pid)!=FALSE;
            if(detached)attachment.active=false;
            log.event("watch_detached","\"success\":"+std::string(detached?"true":"false"));
            if(!detached)throw std::runtime_error("detach failed");
        }
        std::cout<<"PASS exception watch ended; dumps="<<dumps<<" exited="<<exited<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
