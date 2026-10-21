param(
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version='0.1.0',
    [switch]$Install,         # also put the package into this game folder (the game must be closed)
    # The GitHub repository whose releases the update button looks at (EnshroudedVR\Update-EnshroudedVR.ps1 reads it from
    # the package's manifest). -ReleaseZip: also EnshroudedVR-<Version>.zip in vrmod\release, the asset of the release with the
    # tag EnshroudedVR-<Version> (user, 2026-10-19: release files go into the work folder, not onto the desktop).
    [ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository='momotori01/ENSHROUDED-VR-MOD',
    [switch]$ReleaseZip
)
# The package for other players (user, 2026-10-14: "distributed by git", "works by dropping all its contents into
# the game's folder", "VR is off by default and switched on in the exe").
#   <package>\dinput8.dll                      the loader (loader/loader.cpp)
#   <package>\Enshrouded VR setting.exe        VR on/off and the log zip (settings/vr_setting_app.cpp)
#   <package>\EnshroudedVR\bin                 the hook library, the Vulkan layer with its manifest, the OpenXR loader
#   <package>\EnshroudedVR\data                the two replacement shaders, and data\profile: the policy files of a VR
#                                              session, written by the same tool the development sessions use
#   <package>\EnshroudedVR\EnshroudedVR.defaults.ini, version.txt, README.txt, licenses
# Nothing in it names this PC's folders, and it needs no Python on the player's PC (the update button and the log zip use
# the Windows PowerShell every Windows has; <package>\EnshroudedVR\Update-EnshroudedVR.ps1).
# The package is a new folder vrmod\dist\package-<time>; nothing is published from here.
$ErrorActionPreference='Stop'
$mod=Split-Path $PSScriptRoot -Parent;$game=Split-Path $mod -Parent
Set-Location -LiteralPath $game
$pw=(Get-Process -Id $PID).Path
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$package=Join-Path $mod "dist/package-$stamp";$evr=Join-Path $package 'EnshroudedVR'
foreach($d in @($package,$evr,"$evr/bin","$evr/data","$evr/licenses")){New-Item -ItemType Directory -Path $d | Out-Null}
$release=Join-Path $mod 'build/bin/Release'
$hook=(Get-Content -LiteralPath "$release/evr_view_resource_probe.json" -Raw | ConvertFrom-Json).library_path
$layer=(Get-Content -LiteralPath "$release/evr_layer.json" -Raw | ConvertFrom-Json).layer.library_path
foreach($f in @($hook,$layer,"$release/openxr_loader.dll","$mod/dist/dinput8.dll","$mod/dist/Enshrouded VR setting.exe")){if(!(Test-Path -LiteralPath $f)){throw "missing: $f (build first)"}}
Copy-Item -LiteralPath "$mod/dist/dinput8.dll" -Destination $package
Copy-Item -LiteralPath "$mod/dist/Enshrouded VR setting.exe" -Destination $package
Copy-Item -LiteralPath $hook -Destination "$evr/bin/evr_hooks.dll"
Copy-Item -LiteralPath $layer -Destination "$evr/bin/evr_diagnostic.dll"      # the hook library looks for it by this name
Copy-Item -LiteralPath "$release/openxr_loader.dll" -Destination "$evr/bin"
# The layer's manifest names its library beside itself.
$manifest=Get-Content -LiteralPath "$release/evr_layer.json" -Raw | ConvertFrom-Json
$manifest.layer.library_path='.\evr_diagnostic.dll';$manifest.layer.description='Enshrouded VR MOD'
[IO.File]::WriteAllText("$evr/bin/evr_layer.json",($manifest | ConvertTo-Json -Depth 6),(New-Object Text.UTF8Encoding $false))
# The two replacement shaders of the VR sessions, with the hashes the layer checks.
$shaders=@()
foreach($s in @(@{name='shadow_receiver';folder='output/shadow-receiver-live-20261016-1-0';file='shadow_receiver.spv'},@{name='gi_probe';folder='output/gi-probe-fallback-20261010';file='gi_probe.spv'})){
    $m=Get-Content -LiteralPath (Join-Path $mod "$($s.folder)/manifest.json") -Raw | ConvertFrom-Json
    $source=Join-Path $mod "$($s.folder)/$($s.file)"
    $hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLower()
    if($hash -ne $m.shader_sha256){throw "$($s.name): the shader does not match its manifest"}
    Copy-Item -LiteralPath $source -Destination "$evr/data/$($s.file)";$shaders+="$($s.name) $hash"
}
[IO.File]::WriteAllText("$evr/data/shaders.txt",(($shaders -join "`r`n")+"`r`n"),(New-Object Text.UTF8Encoding $false))
# The profile: the policy files of the real-headset play session with everything on (docs/real_device_plan_20261012.md),
# from run_play_session.ps1 -> run_render_resource_probe.py --write-profile. The eye size, the sharpening, the minute
# limit (none) and the world proof are written by the loader for each session.
$session=@('-RealVr','-LiveInput','-Minutes','240','-UiQuad','-RenderScale','0.8','-EyeFrustum','stable','-SymmetricFov','-DetailBindings','-ShadowProjection',
    '-ShadowReceiverPlane','./vrmod/output/shadow-receiver-live-20261016-1-0','-GiProbeFallback','./vrmod/output/gi-probe-fallback-20261010','-FogProjection','match',
    '-LiveMelee','-LiveGuard','-TwoHandWieldVhvr','-QuickSlotMotion','-RealVrWeaponTrial','-WeaponMountObserve','-RangedAnimation','-ShieldMount','-BowDraw','-RangedAim','-BodyIk',
    '-LiveMeleeContact','-LiveMeleeEarly','-LiveMeleeStill','-LiveMeleeTargets','-LiveMeleeTravel','0.25','-LiveHeldItems','-LiveHeldFollow','-UiAutoHide','-ShareVfxModels','-ShareDecals','-MenuPointer','-OnlineSync',
    '-WriteProfile',"$evr/data/profile")
$out=& $pw -NoProfile -File (Join-Path $PSScriptRoot 'run_play_session.ps1') @session 2>&1
if($LASTEXITCODE -ne 0){$out | Select-Object -Last 12;throw 'the profile could not be written'}
($out | Where-Object { "$_" -like 'PROFILE *' } | Select-Object -Last 1)
$profileFiles=@(Get-ChildItem -LiteralPath "$evr/data/profile" -File)
foreach($need in @('world-early-xr.txt','controller-ui-fixture.txt','world-body-ik.txt','world-live-melee.txt','world-hand-curl.txt','hand-curl-closed-pose.bin','world-shared-update-policy.txt','world-eye-private-transient.txt')){
    if($profileFiles.Name -notcontains $need){throw "the profile lacks $need"}}
foreach($never in @('world-resource-proof.txt','world-resource-provenance.json','live-xr-config.txt','world-session.txt','world-vr-sharpness.txt','launch.json')){
    if($profileFiles.Name -contains $never){throw "the profile must not hold $never"}}
Copy-Item -LiteralPath "$mod/config/EnshroudedVR.defaults.ini" -Destination $evr
Copy-Item -LiteralPath "$mod/package/README.txt" -Destination $evr
Copy-Item -LiteralPath "$mod/package/Update-EnshroudedVR.ps1" -Destination $evr   # the update button's (settings/vr_setting_app.cpp)
Copy-Item -LiteralPath (Join-Path $game 'references/OpenXR-SDK/LICENSE') -Destination "$evr/licenses/OpenXR-SDK-LICENSE.txt"
Copy-Item -LiteralPath (Join-Path $game 'references/minhook/LICENSE.txt') -Destination "$evr/licenses/MinHook-LICENSE.txt"
$build=(Get-Content -LiteralPath "$mod/config/profiles/build26082781.json" -Raw | ConvertFrom-Json)
[IO.File]::WriteAllText("$evr/version.txt","$Version (packed $stamp; for the game build of enshrouded.exe $($build.exe_sha256.Substring(0,12)))`r`n",(New-Object Text.UTF8Encoding $false))
# Nothing of this PC in the package's text files.
$here=@($game,$env:USERPROFILE,$env:USERNAME) | Where-Object { $_ }
foreach($f in (Get-ChildItem -LiteralPath $package -Recurse -File | Where-Object { $_.Extension -in '.txt','.json','.ini','.ps1' })){
    # The GitHub account of the releases (the updater's repository) is public by intent; the check is for this PC's names.
    $text=[IO.File]::ReadAllText($f.FullName).Replace($Repository.Split('/')[0],'')
    foreach($h in $here){if($text.IndexOf($h,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or $text.IndexOf($h.Replace('\','/'),[StringComparison]::OrdinalIgnoreCase) -ge 0 -or $text.IndexOf($h.Replace('\','\\'),[StringComparison]::OrdinalIgnoreCase) -ge 0){throw "this PC's folder or user name stands in $($f.FullName)"}}
}
$files=@(Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object { [ordered]@{path=$_.FullName.Substring($package.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower()} })
[IO.File]::WriteAllText("$evr/PACKAGE_MANIFEST.json",([ordered]@{name='Enshrouded VR MOD';version=$Version;packed=$stamp;repository=$Repository;game_exe_sha256=$build.exe_sha256;files=$files} | ConvertTo-Json -Depth 5),(New-Object Text.UTF8Encoding $false))
"PACKAGE $package : $($files.Count+1) files, $([math]::Round((($files | ForEach-Object { $_.bytes } | Measure-Object -Sum).Sum)/1MB,1)) MB, profile $($profileFiles.Count) files"
if($ReleaseZip){
    $releaseFolder=Join-Path $mod 'release';New-Item -ItemType Directory -Force -Path $releaseFolder | Out-Null
    $zip=Join-Path $releaseFolder "EnshroudedVR-$Version.zip"
    if(Test-Path -LiteralPath $zip){throw "$zip exists already - give the release a new -Version"}
    Compress-Archive -Path (Join-Path $package '*') -DestinationPath $zip
    "RELEASE $zip ($([math]::Round((Get-Item -LiteralPath $zip).Length/1MB,1)) MB): tag EnshroudedVR-$Version, asset EnshroudedVR-$Version.zip, repository $Repository"
}
if($Install){
    if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'The game is running; close it before -Install.'}
    # As a player would: everything into the game's folder. The player's own settings, logs and run copies are not in the package.
    Copy-Item -Path (Join-Path $package '*') -Destination $game -Recurse -Force
    "INSTALLED into $game"
}
