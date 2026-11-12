// A check of the loader's hook on process creation (loader/loader.cpp guardChildren, 2026-11-12) without the game:
// this program is copied as "enshrouded.exe" into a folder with an "EnshroudedVR" folder and the loader's dinput8.dll,
// and started with EVR_LAYER_OUTPUT set (the development launch's path, where the loader puts the hook on first).
// It loads the loader, then starts cmd.exe four ways; each writes the environment it got ("set") into <folder>\out:
//   inherited.txt  CreateProcessW, inherited environment        -> no EVR_*, no VK_INSTANCE_LAYERS / XR_RUNTIME_JSON
//   block.txt      CreateProcessW, the own environment as block -> the same
//   shell.txt      ShellExecuteExW                              -> the same
//   own.txt        a copy of cmd.exe inside the folder (owncmd.exe; the mod's own programs) -> everything kept
// Not a ctest: tools/check_child_environment.sh prepares the folder and reads the files.
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")
int wmain(){
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
    std::wstring folder(exe);folder=folder.substr(0,folder.find_last_of(L"\\/"));
    if(!LoadLibraryW((folder+L"\\dinput8.dll").c_str())){std::printf("the loader did not load (%lu)\n",GetLastError());return 2;}
    const std::wstring out=folder+L"\\out";CreateDirectoryW(out.c_str(),nullptr);
    wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);const std::wstring cmd=std::wstring(system)+L"\\cmd.exe";
    int failures=0;
    const auto run=[&](const std::wstring& program,const wchar_t* name,bool block){
        std::wstring line=L"\""+program+L"\" /d /c set > \""+out+L"\\"+name+L"\"";
        std::vector<wchar_t> env;
        if(block){if(wchar_t* own=GetEnvironmentStringsW()){for(const wchar_t* p=own;*p;p+=wcslen(p)+1)env.insert(env.end(),p,p+wcslen(p)+1);FreeEnvironmentStringsW(own);}
            env.push_back(L'\0');env.push_back(L'\0');}
        STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
        if(!CreateProcessW(nullptr,line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|(block?CREATE_UNICODE_ENVIRONMENT:0),block?env.data():nullptr,nullptr,&si,&pi)){
            std::printf("%ls: not started (%lu)\n",name,GetLastError());++failures;return;}
        WaitForSingleObject(pi.hProcess,10000);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);std::printf("%ls: written\n",name);
    };
    run(cmd,L"inherited.txt",false);
    run(cmd,L"block.txt",true);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    const std::wstring parameters=L"/d /c set > \""+out+L"\\shell.txt\"";
    SHELLEXECUTEINFOW info{};info.cbSize=sizeof info;info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI|SEE_MASK_NOASYNC;info.lpFile=cmd.c_str();info.lpParameters=parameters.c_str();info.nShow=SW_HIDE;
    if(ShellExecuteExW(&info)&&info.hProcess){WaitForSingleObject(info.hProcess,10000);CloseHandle(info.hProcess);std::printf("shell.txt: written\n");}
    else{std::printf("shell.txt: not started (%lu)\n",GetLastError());++failures;}
    run(folder+L"\\owncmd.exe",L"own.txt",false);
    return failures?1:0;
}
