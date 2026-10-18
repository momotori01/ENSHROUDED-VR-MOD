param([string]$Out='')
# The mod without VR as a small folder for another PC (2026-10-14, user: "with the VR display off the other players'
# movements are still seen as long as the mod is installed"): the hook library of the last build, the two Python tools
# that put it into the running game, the game build's profile and the closed-hand pose for the fingers.
# Nothing here is a launcher: the game is started from Steam as usual, then start_online_flat.cmd is run.
# The folder's content goes into the game's folder on the other PC (it only adds a vrmod folder).
$ErrorActionPreference='Stop'
$mod=Split-Path $PSScriptRoot -Parent
$record=Join-Path $mod 'build/bin/Release/evr_view_resource_probe.json'
$dll=(Get-Content -LiteralPath $record -Raw | ConvertFrom-Json).library_path
if(!(Test-Path -LiteralPath $dll)){throw "hook library missing: $dll"}
if(!$Out){$Out=Join-Path $mod ('output/online-flat-package-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
if(Test-Path -LiteralPath $Out){throw "already there: $Out"}
$root=New-Item -ItemType Directory -Path $Out
foreach($d in 'vrmod/tools','vrmod/config/profiles','vrmod/build/bin/Release','vrmod/output/grip-preview-20261004-1530'){New-Item -ItemType Directory -Path (Join-Path $root $d) -Force | Out-Null}
Copy-Item -LiteralPath $dll -Destination (Join-Path $root 'vrmod/build/bin/Release/evr_view_resource_probe.dll')
foreach($f in 'run_online_flat.py','inject_camera_observer.py','read_game_memory.py','analyze_camera_dump.py'){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $f) -Destination (Join-Path $root 'vrmod/tools')}
# The injector reads the library's export table with pefile (MIT), kept beside the tools.
New-Item -ItemType Directory -Path (Join-Path $root 'vrmod/tools/python') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'python/pefile.py') -Destination (Join-Path $root 'vrmod/tools/python')
foreach($d in 'ordlookup','pefile-2024.8.26.dist-info'){Copy-Item -LiteralPath (Join-Path $PSScriptRoot "python/$d") -Destination (Join-Path $root 'vrmod/tools/python') -Recurse}
Copy-Item -LiteralPath (Join-Path $mod 'config/profiles/build23966345.json') -Destination (Join-Path $root 'vrmod/config/profiles')
$curl=Join-Path $mod 'output/grip-preview-20261004-1530'
foreach($f in 'profile.json','hand-pose.bin'){Copy-Item -LiteralPath (Join-Path $curl $f) -Destination (Join-Path $root 'vrmod/output/grip-preview-20261004-1530')}
$cmd=@'
@echo off
rem The mod without VR: other players' VR bodies are shown. Start the game from Steam first, then run this.
rem Ends after four hours, when the game is closed, or when a file named stop.request is put into the newest
rem vrmod\output\attach-...\online-flat-... folder.
cd /d "%~dp0"
python vrmod\tools\run_online_flat.py --attach --minutes 240 --hand-curl vrmod\output\grip-preview-20261004-1530
pause
'@
[IO.File]::WriteAllText((Join-Path $root 'start_online_flat.cmd'),$cmd,(New-Object Text.UTF8Encoding $false))
$readme=@'
Enshrouded VR MOD：VR なしで相手（VR の人）の動きを見るための一式

入れ方
1. このフォルダの中身（vrmod フォルダと start_online_flat.cmd）を、ゲームのフォルダ
   （enshrouded.exe のある所。例：...\steamapps\common\Enshrouded）へ写す。
2. Python 3.10 以降を入れておく（コマンド プロンプトで python と打って動く事）。

使い方
1. Steam からゲームを普通に起動して、セッションに入る（順番は後でも可。MOD はワールドが出るまで待つ）。
2. start_online_flat.cmd を実行する。黒い窓が開いたままになる（閉じると記録が取れないので、そのままに）。
3. 終わる時はゲームを普通に閉じる。4 時間たつと MOD は自動で外れる。

条件
- VR の人と Steam のフレンドで、同じセッションに居る事。
- ゲームの版が VR の人と同じ事（違うと「unsupported game hash」と出て何もしない）。
- ゲームを管理者として起動していない事。

何をするか
- ゲームの絵や操作は変えない。VR の人のキャラクターの頭・腕・指を、その人の動きのとおりに動かすだけ。
- 自分のキャラクターの位置を 1 秒に 1 回、MOD を入れているフレンドへ送る（近くに居る事を知らせるため）。
- 記録は vrmod\output\attach-...\online-flat-...\view-resource-probe.jsonl に残る。うまく見えない時はこのファイルを VR の人へ渡す。
'@
[IO.File]::WriteAllText((Join-Path $root 'README_ja.txt'),$readme,(New-Object Text.UTF8Encoding $true))
$files=Get-ChildItem -LiteralPath $root -Recurse -File
"package: $root"
"$($files.Count) files, $([Math]::Round(($files | Measure-Object Length -Sum).Sum/1MB,1)) MB; hook library sha256 $((Get-FileHash -LiteralPath (Join-Path $root 'vrmod/build/bin/Release/evr_view_resource_probe.dll')).Hash.Substring(0,16))"
