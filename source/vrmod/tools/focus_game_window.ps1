param([Parameter(Mandatory)][string]$Launch)
# Bring the launched game's window to the foreground (2026-10-09 real-device run): the game reads the gamepad
# only while its window has focus, and the user wears the headset. Only the window of the recorded PID (checked
# against launch.json creation time) is touched; no setting is changed. Prints the foreground process afterwards.
$ErrorActionPreference='Stop'
$l=Get-Content -LiteralPath (Join-Path $Launch 'launch.json') -Raw | ConvertFrom-Json
$p=Get-Process -Id $l.pid
if($p.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -ne $l.creation_filetime){throw 'launched process identity mismatch'}
Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class EvrFocusWin32 {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    public static List<IntPtr> Top(uint pid){var r=new List<IntPtr>();EnumWindows((h,l)=>{uint o;GetWindowThreadProcessId(h,out o);if(o==pid&&IsWindowVisible(h)&&GetWindow(h,4)==IntPtr.Zero)r.Add(h);return true;},IntPtr.Zero);return r;}
}
'@
$windows=[EvrFocusWin32]::Top([uint32]$p.Id)
if($windows.Count -lt 1){throw 'no visible top-level game window'}
$h=$windows[0]
if([EvrFocusWin32]::IsIconic($h)){[EvrFocusWin32]::ShowWindow($h,9)|Out-Null}
if(-not [EvrFocusWin32]::SetForegroundWindow($h)){
    # Windows only lets the foreground process hand over focus; a released ALT tap satisfies the rule.
    [EvrFocusWin32]::keybd_event(0x12,0,0,[UIntPtr]::Zero);[EvrFocusWin32]::keybd_event(0x12,0,2,[UIntPtr]::Zero)
    [EvrFocusWin32]::SetForegroundWindow($h)|Out-Null
}
Start-Sleep -Milliseconds 300
$owner=0;[EvrFocusWin32]::GetWindowThreadProcessId([EvrFocusWin32]::GetForegroundWindow(),[ref]$owner)|Out-Null
"foreground: $((Get-Process -Id $owner).ProcessName) (game pid $($p.Id), focused=$($owner -eq $p.Id))"
