// include/evr/child_environment.hpp: what a program started from the game gets (2026-11-12, the Steam the game had
// started again carried the mod's variables to other games).
#include <evr/child_environment.hpp>
#include <cstdio>
#include <cstdlib>
using namespace evr::childenv;
static int failures=0;
#define ck(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);++failures;}}while(0)
static bool has(const std::vector<std::wstring>& list,const std::wstring& entry){for(const auto& e:list)if(e==entry)return true;return false;}
static bool hasName(const std::vector<std::wstring>& list,const std::wstring& name){for(const auto& e:list)if(sameName(nameOf(e),name))return true;return false;}
int main(){
    // names
    ck(nameOf(L"PATH=C:\\x")==L"PATH");ck(nameOf(L"=C:=C:\\game")==L"=C:");ck(nameOf(L"EMPTY=")==L"EMPTY");
    ck(modOwned(L"EVR_SCREEN_XR")&&modOwned(L"evr_x")&&!modOwned(L"EVRX")&&!modOwned(L"VK_LAYER_PATH")&&!modOwned(L"EV"));
    // a block's entries
    const wchar_t raw[]=L"=C:=C:\\g\0A=1\0EVR_X=2\0\0";
    const auto list=entries(raw);ck(list.size()==3&&list[0]==L"=C:=C:\\g"&&list[2]==L"EVR_X=2");
    const char rawAnsi[]="B=2\0EVR_Y=3\0\0";
    const auto ansi=entriesAnsi(rawAnsi);ck(ansi.size()==2&&ansi[0]==L"B=2"&&ansi[1]==L"EVR_Y=3");
    // The game's environment after the loader: the layer set, the runtime replaced, a user's own layer list changed.
    const std::vector<std::wstring> game{L"=C:=C:\\game",L"Path=C:\\Windows",L"SteamAppId=1203620",L"EVR_SCREEN_XR=1",L"evr_lower=1",
        L"VK_LAYER_PATH=S:\\game\\EnshroudedVR\\bin",L"VK_INSTANCE_LAYERS=VK_LAYER_EVR_diagnostic",L"XR_RUNTIME_JSON=S:\\mock.json",L"ENABLE_VULKAN_RENDERDOC_CAPTURE=1"};
    const std::vector<Original> originals{{L"VK_LAYER_PATH",false,L""},{L"VK_INSTANCE_LAYERS",true,L"VK_LAYER_users_own"},
        {L"XR_RUNTIME_JSON",true,L"C:\\SteamVR\\steamxr_win64.json"},{L"ENABLE_VULKAN_RENDERDOC_CAPTURE",false,L""},{L"XR_API_LAYER_PATH",true,L"C:\\layers"},{L"EVR_FROM_SHELL",true,L"1"}};
    size_t changed=0;const auto out=cleaned(game,originals,&changed);
    ck(has(out,L"=C:=C:\\game")&&has(out,L"Path=C:\\Windows")&&has(out,L"SteamAppId=1203620"));        // the rest as it is
    ck(!hasName(out,L"EVR_SCREEN_XR")&&!hasName(out,L"evr_lower")&&!hasName(out,L"EVR_FROM_SHELL"));    // nothing of the mod
    ck(!hasName(out,L"VK_LAYER_PATH")&&!hasName(out,L"ENABLE_VULKAN_RENDERDOC_CAPTURE"));               // not there before: left out
    ck(has(out,L"VK_INSTANCE_LAYERS=VK_LAYER_users_own")&&has(out,L"XR_RUNTIME_JSON=C:\\SteamVR\\steamxr_win64.json"));   // the old values
    ck(has(out,L"XR_API_LAYER_PATH=C:\\layers"));                                                        // taken away by the loader: back
    ck(changed==7);   // 2 EVR_, 2 left out, 2 old values, 1 put back
    // a variable the loader set to what it was: no change counted
    size_t same=99;const auto unchanged=cleaned({L"A=1",L"XR_RUNTIME_JSON=C:\\r.json"},{{L"xr_runtime_json",true,L"C:\\r.json"}},&same);
    ck(same==0&&unchanged.size()==2);
    // the block: sorted without regard to case, '=' names first, double NUL
    const auto b=block({L"b=2",L"A=1",L"=C:=x"});
    const std::wstring flat(b.begin(),b.end());
    ck(flat==std::wstring(L"=C:=x\0A=1\0b=2\0\0",15));
    const auto empty=block({});ck(empty.size()==2&&empty[0]==0&&empty[1]==0);
    // the program a call starts
    ck(programOf(L"C:\\a.exe",L"x y")==L"C:\\a.exe");
    ck(programOf(nullptr,L"\"C:\\Program Files (x86)\\Steam\\steam.exe\" -silent")==L"C:\\Program Files (x86)\\Steam\\steam.exe");
    ck(programOf(nullptr,L"  cmd.exe /c set")==L"cmd.exe");ck(programOf(nullptr,nullptr).empty());
    // inside the game's folder (the mod's own programs keep the environment)
    const std::wstring folder=L"S:\\SteamLibrary\\steamapps\\common\\Enshrouded";
    ck(insideFolder(L"S:\\SteamLibrary\\steamapps\\common\\Enshrouded\\Enshrouded VR setting.exe",folder));
    ck(insideFolder(L"s:/steamlibrary/steamapps/common/enshrouded/EnshroudedVR/bin/x.exe",folder+L"\\"));
    ck(!insideFolder(L"S:\\SteamLibrary\\steamapps\\common\\EnshroudedX\\a.exe",folder));          // a neighbour with a longer name
    ck(!insideFolder(L"S:\\SteamLibrary\\steamapps\\common\\EDF6\\EDF.exe",folder));
    ck(!insideFolder(L"C:\\Program Files (x86)\\Steam\\steam.exe",folder)&&!insideFolder(folder,folder)&&!insideFolder(L"steam.exe",L""));
    ck(fileName(L"C:\\Program Files (x86)\\Steam\\steam.exe")==L"steam.exe"&&fileName(L"cmd.exe")==L"cmd.exe");
    if(failures){std::printf("%d failure(s)\n",failures);return 1;}
    std::printf("child environment: all passed\n");return 0;
}
