#pragma once
// No <windows.h> here: the motion headers that use this are also built into the plain tests, where its near/far
// macros break the code. ntdll's own export, declared by no SDK header.
#pragma comment(lib, "ntdll.lib")
extern "C" __declspec(dllimport) unsigned char __stdcall RtlDllShutdownInProgress();

namespace evr
{
// True while Windows ends the process (ExitProcess: the DLLs' static destructors run after the other threads are gone).
// A call into the OpenXR runtime then can crash: a player's log of 2026-10-22 (SteamVR, 0.9.9) crashed in
// vrclient_x64.dll from the native XR client's xrDestroySession in a static destructor, after the game's own
// "Caught shutdown crash in VulkanGraphicsDevice::destroyDevice". The process is going away; its handles go with it.
inline bool processExiting() noexcept { return RtlDllShutdownInProgress() != 0; }
} // namespace evr
