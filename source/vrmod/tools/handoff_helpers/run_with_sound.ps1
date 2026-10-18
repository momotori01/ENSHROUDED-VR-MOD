param([Parameter(Mandatory)][string]$Script,[string]$ScriptArgs='',[Parameter(Mandatory)][string]$Out)
# User rules: 2026-10-06 10:10 sounds at launch and after exit; 2026-10-09 "ゲーム起動時の音は不要" -> only the close sound.
Set-Location -LiteralPath 'S:\SteamLibrary\steamapps\common\Enshrouded'
$pw='pwsh'
# 2026-10-16 19:05 user: the launch sound again - they draw on this PC and stop when it sounds (the chat is not seen then).
& $pw -NoProfile -File ./vrmod/tools/notify_user_sound.ps1 -Kind GameStart *> "$Out.sound-start.txt"
& $pw -NoProfile -Command "& '$Script' $ScriptArgs" *> $Out
for($i=0;$i -lt 180 -and (Get-Process enshrouded -ErrorAction SilentlyContinue);$i++){Start-Sleep -Seconds 2}
if(Get-Process enshrouded -ErrorAction SilentlyContinue){"game still running - no close sound" | Add-Content $Out}
else{& $pw -NoProfile -File ./vrmod/tools/notify_user_sound.ps1 -Kind GameClosed *> "$Out.sound-closed.txt"}
