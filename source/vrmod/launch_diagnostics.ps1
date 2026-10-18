param([ValidateSet('Dlss','Keep')][string]$VrUpscaler='Dlss',[ValidateSet('','Dlaa','UltraQuality','Quality','Balanced','Performance','UltraPerformance')][string]$VrDlssQuality='',[ValidatePattern('^(Keep|0(\.\d+)?|1(\.0+)?)$')][string]$VrSharpness='0.5',[switch]$NoFovCrop,[switch]$SymmetricFov,[ValidateRange(-1,10)][double]$FovCropMarginDeg=-1,[ValidateRange(0,1)][double]$MockVisibilityMask=0,[int]$EyeLumaEvery=0,[int]$LogNameCap=0,[switch]$DesktopMirror,[switch]$NoDesktopMirror,[int]$GpuTimingEvery=0,[switch]$EyeImageSweep,[int]$QueueTraceLive=0,[switch]$InteractiveWindow, [switch]$CaptureShaders, [switch]$PrepareVr, [switch]$PrepareMock, [switch]$PrepareRealVr, [ValidatePattern('^(none|-?\d+(\.\d+)?,-?\d+(\.\d+)?,-?\d+(\.\d+)?)$')][string]$GripTrim='', [switch]$SessionProbe, [switch]$LiveStereo, [string]$MockPoseSequence, [switch]$MockNavigate, [switch]$MockNavigateHands, [switch]$DirectPlay, [string]$SaveBackupSource, [int]$RenderCpuArenaMiB=0, [switch]$RenderDoc, [ValidateSet('None','Core','Sync')][string]$VulkanValidation='None', [string]$ShadowReceiverPlane='', [string]$GiProbeFallback='', [switch]$HeadVisibilityShader, [switch]$VfxTransparentShader, [switch]$UiPipelineProbe, [switch]$UiCapture, [switch]$UiQuad, [switch]$UiPartial, [switch]$PrepareBodyClip, [switch]$BodyClipShader, [switch]$HandsOnly, [switch]$MotionHands, [string]$MockHandSequence, [switch]$XrAsyncCompletion, [switch]$LogBuffered)
$ErrorActionPreference='Stop'
if($MotionHands -and !$LiveStereo){throw 'MotionHands requires LiveStereo.'}
if($XrAsyncCompletion -and !$LiveStereo){throw 'XrAsyncCompletion requires LiveStereo.'}
if($MockHandSequence -and (!$MotionHands -or !$PrepareMock)){throw 'MockHandSequence requires MotionHands and PrepareMock.'}
if($MockNavigate -and (!$PrepareMock -or !$MotionHands -or !$MockHandSequence)){throw 'MockNavigate requires the Mock runtime and hand input.'}
if($MockNavigateHands -and !$MockNavigate){throw 'MockNavigateHands requires MockNavigate.'}
if($HandsOnly){$HeadVisibilityShader=$true;$BodyClipShader=$true}
if($BodyClipShader){if(!$LiveStereo){throw 'BodyClipShader requires live stereo diagnostics.'};$PrepareBodyClip=$true}
if($PrepareBodyClip -and !$LiveStereo){throw 'PrepareBodyClip requires live stereo diagnostics.'}
if($UiQuad -and !$LiveStereo){throw 'UiQuad requires explicit live stereo diagnostics'}
if($UiQuad){$UiCapture=$true}
if($RenderCpuArenaMiB -ne 0 -and ($RenderCpuArenaMiB -lt 512 -or $RenderCpuArenaMiB -gt 2048)){throw 'RenderCpuArenaMiB must be 0 (native default) or 512..2048.'}
if (@(@($PrepareVr,$PrepareMock,$PrepareRealVr) | Where-Object { $_ }).Count -gt 1) { throw 'Choose one Runtime.' }
if (($SessionProbe -or $LiveStereo) -and !($PrepareVr -or $PrepareMock -or $PrepareRealVr)) { throw 'Session probe requires explicit graphics preparation.' }
if ($PrepareRealVr -and ($MockPoseSequence -or $MockHandSequence -or $MockVisibilityMask -gt 0)) { throw 'Mock pose/hand/mask inputs cannot be used with the real SteamVR runtime.' }
if ($MockPoseSequence -and !$PrepareMock) { throw 'Mock pose sequence requires PrepareMock.' }
$shadowReceiver=$null
if($ShadowReceiverPlane){
    if(!$LiveStereo){throw 'ShadowReceiverPlane requires live stereo diagnostics.'}
    $shadowReceiver=Get-Content -LiteralPath (Join-Path $ShadowReceiverPlane 'manifest.json') -Raw | ConvertFrom-Json
    if($shadowReceiver.source_sha256 -ne 'bb450d00434dca964a841dcc9995582282ce69bf49260e6d5e153ba87483177b' -or $shadowReceiver.history_reset -or $shadowReceiver.control){throw 'Unreviewed or offline-only shadow receiver manifest.'}
    if((Get-FileHash -LiteralPath $shadowReceiver.shader -Algorithm SHA256).Hash.ToLower() -ne $shadowReceiver.shader_sha256){throw 'Shadow receiver shader hash mismatch.'}
}
$giProbe=$null
if($GiProbeFallback){
    if(!$LiveStereo){throw 'GiProbeFallback requires live stereo diagnostics.'}
    $giProbe=Get-Content -LiteralPath (Join-Path $GiProbeFallback 'manifest.json') -Raw | ConvertFrom-Json
    if($giProbe.source_sha256 -ne '5d79570aa82b8a4247a11985893317fe908d1105e32a63244880e984957fd01c' -or $giProbe.history_reset -ne $false -or $giProbe.control -ne $false -or $giProbe.fallback_only -ne $true){throw 'Unreviewed or offline-only GI probe manifest.'}
    if((Get-FileHash -LiteralPath $giProbe.shader -Algorithm SHA256).Hash.ToLower() -ne $giProbe.shader_sha256){throw 'GI probe shader hash mismatch.'}
}
if ($HeadVisibilityShader -and !$LiveStereo) { throw 'HeadVisibilityShader requires explicit live stereo diagnostics.' }
if ($VfxTransparentShader -and !$LiveStereo) { throw 'VfxTransparentShader requires explicit live stereo diagnostics.' }
$gameDir=Split-Path $PSScriptRoot -Parent
if (Get-Process enshrouded -ErrorAction SilentlyContinue) { throw 'Enshrouded is already running; diagnostic launch skipped.' }
$bin=Join-Path $PSScriptRoot 'build/bin/Release'
$manifestPath=Join-Path $bin 'evr_layer.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'Build the diagnostic layer first.' }
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if($RenderDoc){
    $renderDocPath=Join-Path $gameDir 'references/renderdoc/1.46/RenderDoc_1.46_64'
    foreach($name in @('renderdoc.dll','renderdoc.json')){if(!(Test-Path -LiteralPath (Join-Path $renderDocPath $name))){throw 'Portable RenderDoc is missing.'}}
}
if (!(Test-Path -LiteralPath $manifest.layer.library_path)) { throw 'Diagnostic DLL in manifest is missing.' }
if($VulkanValidation -ne 'None'){
    $validationRoot=Join-Path $gameDir 'references/vulkan-sdk'
    $validationPath=Join-Path $validationRoot 'portable/Bin'
    $validationManifest=Get-Content -LiteralPath (Join-Path $validationRoot 'portable-manifest.json') -Raw | ConvertFrom-Json
    foreach($file in $validationManifest.files){
        if($file.name -notin @('VkLayer_khronos_validation.dll','VkLayer_khronos_validation.json')){throw 'Unexpected validation layer manifest entry.'}
        if((Get-FileHash -LiteralPath (Join-Path $validationPath $file.name) -Algorithm SHA256).Hash -ne $file.sha256){throw 'Portable validation layer hash changed.'}
    }
    if($validationManifest.files.Count -ne 2){throw 'Portable validation layer manifest incomplete.'}
}
$isolated = Join-Path $PSScriptRoot 'test_env/steamvr'
if ($PrepareVr -or $PrepareMock -or $PrepareRealVr -or $DirectPlay -or $RenderCpuArenaMiB -or $UiCapture) {
    if ((Get-FileHash -LiteralPath (Join-Path $gameDir 'enshrouded.exe') -Algorithm SHA256).Hash -ne '82E5020A8736348CD0374F004B2F5E5EF456B5BD27AA5420DF34C7DD241CFEEB') { throw 'Unsupported game build; VR preparation refused.' }
}
if ($PrepareVr) {
    $sessionPath = Join-Path $isolated 'active-session.json'
    if (!(Test-Path -LiteralPath $sessionPath)) { throw 'VR preparation requires an already-owned isolated virtual session.' }
    $session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
    $serverRecords = @($session.processes | Where-Object { [IO.Path]::GetFileName($_.path) -eq 'vrserver.exe' })
    if ($serverRecords.Count -ne 1) { throw 'Isolated server identity missing.' }
    $record = $serverRecords[0]
    $servers = @(Get-Process vrserver -ErrorAction SilentlyContinue)
    if ($servers.Count -ne 1 -or $servers[0].Id -ne $record.pid -or $servers[0].Path -ne $record.path -or $servers[0].StartTime.ToUniversalTime().Ticks -ne ([datetime]$record.startUtc).ToUniversalTime().Ticks) { throw 'Owned virtual server is not the active SteamVR server.' }
    $registration = Get-Content -LiteralPath (Join-Path $isolated 'openvrpaths.vrpath') -Raw | ConvertFrom-Json
    $runtimeManifest = Join-Path $registration.runtime[0] 'steamxr_win64.json'
}
# Real device (2026-10-09): the SteamVR session the user started. Read-only checks; the runtime is chosen for
# the game process only (XR_RUNTIME_JSON). No OpenXR/SteamVR/OS registration, setting or driver is changed, and
# SteamVR is never started or stopped from here.
$realVr=$null
if ($PrepareRealVr) {
    $registrationPath = Join-Path $env:LOCALAPPDATA 'openvr/openvrpaths.vrpath'
    if (!(Test-Path -LiteralPath $registrationPath)) { throw 'SteamVR registration (openvrpaths.vrpath) not found.' }
    $userRegistration = Get-Content -LiteralPath $registrationPath -Raw | ConvertFrom-Json
    $runtimeRoot = [string]@($userRegistration.runtime)[0]
    $realManifest = Join-Path $runtimeRoot 'steamxr_win64.json'
    $expectedServer = Join-Path $runtimeRoot 'bin/win64/vrserver.exe'
    if (!(Test-Path -LiteralPath $realManifest) -or !(Test-Path -LiteralPath $expectedServer)) { throw 'Registered SteamVR runtime files are missing.' }
    $servers = @(Get-Process vrserver -ErrorAction SilentlyContinue)
    if ($servers.Count -ne 1) { throw 'Start SteamVR (exactly one vrserver) with the headset before a real-device launch.' }
    $server = $servers[0]
    if (!$server.Path -or (Resolve-Path -LiteralPath $server.Path).Path -ne (Resolve-Path -LiteralPath $expectedServer).Path) { throw 'The running vrserver is not the registered SteamVR runtime.' }
    $sessionPath = Join-Path $isolated 'active-session.json'
    if (Test-Path -LiteralPath $sessionPath) {
        $isolatedSession = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
        if (@($isolatedSession.processes | Where-Object { $_.pid -eq $server.Id }).Count) { throw 'The running vrserver belongs to the isolated virtual test session; use -PrepareVr for it.' }
    }
    $realVr = [ordered]@{runtime_root=$runtimeRoot;manifest=(Resolve-Path -LiteralPath $realManifest).Path;manifest_sha256=(Get-FileHash -LiteralPath $realManifest -Algorithm SHA256).Hash
        vrserver_pid=$server.Id;vrserver_path=$server.Path;vrserver_start_utc=$server.StartTime.ToUniversalTime().ToString('o');scope='game process environment only'}
    Write-Output "Real SteamVR: vrserver PID $($server.Id), runtime $($realVr.manifest)"
}
$saveBackup=$null
if($DirectPlay) {
    if(!$SaveBackupSource) {
        $backupRoot=Join-Path $PSScriptRoot 'output/save-backups'
        foreach($candidate in (Get-ChildItem -LiteralPath $backupRoot -Directory | Sort-Object Name -Descending)) {
            $recordPath=Join-Path $candidate.FullName 'backup.json'
            if(!(Test-Path -LiteralPath $recordPath)){continue}
            $record=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
            if($record.verified -and $record.source -match '[\\/]userdata[\\/][0-9]+[\\/]1203620[\\/]remote$') {
                $SaveBackupSource=$record.source
                break
            }
        }
    }
    if(!$SaveBackupSource) { throw 'DirectPlay requires SaveBackupSource or a prior verified Steam save backup.' }
    $saveBackup=& (Join-Path $PSScriptRoot 'tools/backup_game_saves.ps1') -Source $SaveBackupSource -PassThru
    if(!$saveBackup.verified){throw 'DirectPlay save backup did not complete.'}
    Write-Output "PASS automatic DirectPlay save backup: $($saveBackup.path)"
}
$out=Join-Path $PSScriptRoot ('output/game-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item -LiteralPath (Join-Path $gameDir 'enshrouded.log') -Destination (Join-Path $out 'previous_game.log') -ErrorAction SilentlyContinue
$previous = @{}
foreach($key in @('EVR_NATIVE_MOTION_HANDS','EVR_MOCK_HAND_SEQUENCE','EVR_MOCK_NAV_FILE','EVR_MOCK_NAV_HAND_INPUT')){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
$previous['EVR_HANDS_ONLY'] = [Environment]::GetEnvironmentVariable('EVR_HANDS_ONLY','Process')
$previous['EVR_GRIP_TRIM'] = [Environment]::GetEnvironmentVariable('EVR_GRIP_TRIM','Process')
$previous['EVR_SYMMETRIC_FOV'] = [Environment]::GetEnvironmentVariable('EVR_SYMMETRIC_FOV','Process')
$previous['EVR_XR_ASYNC_COMPLETION'] = [Environment]::GetEnvironmentVariable('EVR_XR_ASYNC_COMPLETION','Process')
$previous['EVR_EYE_LUMA_EVERY'] = [Environment]::GetEnvironmentVariable('EVR_EYE_LUMA_EVERY','Process')
$previous['EVR_LOG_NAME_CAP'] = [Environment]::GetEnvironmentVariable('EVR_LOG_NAME_CAP','Process')
$previous['EVR_DESKTOP_MIRROR'] = [Environment]::GetEnvironmentVariable('EVR_DESKTOP_MIRROR','Process')
foreach($key in @('EVR_FOV_CROP','EVR_FOV_CROP_MARGIN_DEG','EVR_MOCK_VISIBILITY_MASK')){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
$previous['EVR_GPU_TIMING_EVERY'] = [Environment]::GetEnvironmentVariable('EVR_GPU_TIMING_EVERY','Process')
$previous['EVR_EYE_IMAGE_SWEEP'] = [Environment]::GetEnvironmentVariable('EVR_EYE_IMAGE_SWEEP','Process')
$previous['EVR_QUEUE_TRACE_LIVE'] = [Environment]::GetEnvironmentVariable('EVR_QUEUE_TRACE_LIVE','Process')
$previous['EVR_LOG_BUFFERED'] = [Environment]::GetEnvironmentVariable('EVR_LOG_BUFFERED','Process')
$previous['EVR_BODY_CLIP_SHADER'] = [Environment]::GetEnvironmentVariable('EVR_BODY_CLIP_SHADER','Process')
$previous['EVR_BODY_CLIP_PREPARE'] = [Environment]::GetEnvironmentVariable('EVR_BODY_CLIP_PREPARE','Process')
$previous['EVR_UI_QUAD'] = [Environment]::GetEnvironmentVariable('EVR_UI_QUAD','Process')
$previous['EVR_UI_CAPTURE'] = [Environment]::GetEnvironmentVariable('EVR_UI_CAPTURE','Process')
$previous['EVR_UI_PIPELINE_PROBE'] = [Environment]::GetEnvironmentVariable('EVR_UI_PIPELINE_PROBE','Process')
$previous['EVR_UI_PARTIAL'] = [Environment]::GetEnvironmentVariable('EVR_UI_PARTIAL','Process')
foreach($key in @('EVR_SHADOW_RECEIVER_SHADER','EVR_SHADOW_RECEIVER_SHADER_SHA256')){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
foreach($key in @('EVR_GI_PROBE_SHADER','EVR_GI_PROBE_SHADER_SHA256')){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
$previous['EVR_HEAD_VISIBILITY_SHADER'] = [Environment]::GetEnvironmentVariable('EVR_HEAD_VISIBILITY_SHADER','Process')
$previous['EVR_VFX_TRANSPARENT_SHADER'] = [Environment]::GetEnvironmentVariable('EVR_VFX_TRANSPARENT_SHADER','Process')
foreach($key in @('ENABLE_VULKAN_RENDERDOC_CAPTURE','DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46','VK_LAYER_SETTINGS_PATH')){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
$previous['EVR_RENDER_CPU_ARENA_MIB'] = [Environment]::GetEnvironmentVariable('EVR_RENDER_CPU_ARENA_MIB', 'Process')
$previous['EVR_LIVE_NATIVE_XR'] = [Environment]::GetEnvironmentVariable('EVR_LIVE_NATIVE_XR', 'Process')
$previous['EVR_PREPARE_SESSION_FIXTURE'] = [Environment]::GetEnvironmentVariable('EVR_PREPARE_SESSION_FIXTURE', 'Process')
$previous['EVR_PREPARE_GAME_SESSION'] = [Environment]::GetEnvironmentVariable('EVR_PREPARE_GAME_SESSION', 'Process')
$previous['EVR_MOCK_LOG'] = [Environment]::GetEnvironmentVariable('EVR_MOCK_LOG', 'Process')
$previous['EVR_POSE_SEQUENCE'] = [Environment]::GetEnvironmentVariable('EVR_POSE_SEQUENCE', 'Process')
foreach ($key in @('VK_LAYER_PATH','VK_INSTANCE_LAYERS','EVR_LAYER_OUTPUT','SteamAppId','SteamGameId','VK_LOADER_DEBUG','EVR_CAPTURE_SHADERS','EVR_PREPARE_OPENXR','EVR_PREPARE_LOADER','EVR_PREPARE_FIXTURE','EVR_PREPARE_FAIL_DEVICE','XR_RUNTIME_JSON','VR_PATHREG_OVERRIDE','VR_CONFIG_PATH','VR_LOG_PATH')) { $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }
try {
    $env:EVR_NATIVE_MOTION_HANDS=if($MotionHands){'1'}else{'0'}
    $env:EVR_MOCK_HAND_SEQUENCE=''
    $env:EVR_MOCK_NAV_FILE=''
    $env:EVR_MOCK_NAV_HAND_INPUT=if($MockNavigateHands){'1'}else{''}
    if($MockNavigate){
        $env:EVR_MOCK_NAV_FILE=Join-Path $out 'mock-navigation.txt'
        [IO.File]::WriteAllText($env:EVR_MOCK_NAV_FILE,'')
    }
    if($MockHandSequence){
        $handSource=(Resolve-Path -LiteralPath $MockHandSequence).Path
        $handCopy=Join-Path $out 'mock-hands.csv'
        Copy-Item -LiteralPath $handSource -Destination $handCopy
        $handHash=(Get-FileHash -LiteralPath $handSource -Algorithm SHA256).Hash
        if((Get-FileHash -LiteralPath $handCopy -Algorithm SHA256).Hash -ne $handHash){throw 'Hand sequence snapshot hash differs.'}
        $env:EVR_MOCK_HAND_SEQUENCE=$handCopy
        @{source=$handSource;sha256=$handHash;runtime_input=$handCopy;scope='child process only'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'mock-hands-input.json')
    }
    $env:EVR_HANDS_ONLY=if($HandsOnly){'1'}else{'0'}
    # Grip orientation trim "yaw,pitch,roll" degrees (2026-10-09): real controllers default to the user's EDF6VR hand angle
    # (4.47,-59.27,3.61; left hand mirrored in the layer). The Mock fixtures were made without it. 'none' = identity.
    $gripTrimValue=if($GripTrim){$GripTrim}elseif($PrepareRealVr){'4.47,-59.27,3.61'}else{'none'}
    if($gripTrimValue -eq 'none'){Remove-Item Env:EVR_GRIP_TRIM -ErrorAction SilentlyContinue}else{$env:EVR_GRIP_TRIM=$gripTrimValue}
    # Opt-in queue-ordered XR release (no per-frame device idle); child process only.
    $env:EVR_SYMMETRIC_FOV=if($SymmetricFov){'1'}else{'0'}
    $env:EVR_XR_ASYNC_COMPLETION=if($XrAsyncCompletion){'1'}else{'0'}
    $env:EVR_EYE_LUMA_EVERY=if($EyeLumaEvery -gt 0){[string]$EyeLumaEvery}else{'0'}
    $env:EVR_LOG_NAME_CAP=if($LogNameCap -gt 0){[string]$LogNameCap}else{'0'}
    # Right-eye desktop mirror: default for VR (-LiveStereo) launches since 2026-10-06, because the desktop View
    # render is skipped by default; -NoDesktopMirror turns it off.
    $desktopMirrorOn=($DesktopMirror -or $LiveStereo) -and -not $NoDesktopMirror
    $env:EVR_DESKTOP_MIRROR=if($desktopMirrorOn){'1'}else{'0'}
    # Visibility-mask field of view crop is on by default when the runtime offers the mask (fov_crop.hpp).
    $env:EVR_FOV_CROP=if($NoFovCrop){'0'}else{'1'}
    if($FovCropMarginDeg -ge 0){$env:EVR_FOV_CROP_MARGIN_DEG=[string]$FovCropMarginDeg}else{Remove-Item Env:EVR_FOV_CROP_MARGIN_DEG -ErrorAction SilentlyContinue}
    # Mock runtime only: offer a test visibility mask (ellipse at this fraction of the fov tangents).
    if($MockVisibilityMask -gt 0){if(!$PrepareMock){throw 'MockVisibilityMask requires PrepareMock.'};$env:EVR_MOCK_VISIBILITY_MASK=[string]$MockVisibilityMask}else{Remove-Item Env:EVR_MOCK_VISIBILITY_MASK -ErrorAction SilentlyContinue}
    $env:EVR_GPU_TIMING_EVERY=if($GpuTimingEvery -gt 0){[string]$GpuTimingEvery}else{'0'}
    $env:EVR_EYE_IMAGE_SWEEP=if($EyeImageSweep){'1'}else{'0'}
    $env:EVR_QUEUE_TRACE_LIVE=if($QueueTraceLive -gt 0){[string]$QueueTraceLive}else{'0'}
    # Performance measurement only: buffered diagnostic logs (tail may be lost on a crash).
    $env:EVR_LOG_BUFFERED=if($LogBuffered){'1'}else{'0'}
    $env:EVR_BODY_CLIP_SHADER=if($BodyClipShader){'1'}else{'0'}
    $env:EVR_BODY_CLIP_PREPARE=if($PrepareBodyClip){'1'}else{'0'}
    $env:EVR_UI_QUAD=if($UiQuad){'1'}else{'0'}
    $env:EVR_UI_CAPTURE=if($UiCapture){'1'}else{'0'}
    $env:EVR_UI_PIPELINE_PROBE=if($UiPipelineProbe -or $UiCapture){'1'}else{'0'}
    $env:EVR_UI_PARTIAL=if($UiPartial){'1'}else{'0'}  # diagnostic: keep UI frames with unreviewed draws (map tab, 2026-10-06)
    if($shadowReceiver){$env:EVR_SHADOW_RECEIVER_SHADER=$shadowReceiver.shader;$env:EVR_SHADOW_RECEIVER_SHADER_SHA256=$shadowReceiver.shader_sha256}
    else{Remove-Item Env:EVR_SHADOW_RECEIVER_SHADER,Env:EVR_SHADOW_RECEIVER_SHADER_SHA256 -ErrorAction SilentlyContinue}
    if($giProbe){$env:EVR_GI_PROBE_SHADER=$giProbe.shader;$env:EVR_GI_PROBE_SHADER_SHA256=$giProbe.shader_sha256}
    else{Remove-Item Env:EVR_GI_PROBE_SHADER,Env:EVR_GI_PROBE_SHADER_SHA256 -ErrorAction SilentlyContinue}
    $env:EVR_HEAD_VISIBILITY_SHADER=if($HeadVisibilityShader){'1'}else{'0'}
    $env:EVR_VFX_TRANSPARENT_SHADER=if($VfxTransparentShader){'1'}else{'0'}
    $env:EVR_RENDER_CPU_ARENA_MIB=$RenderCpuArenaMiB.ToString()
    $env:VK_LAYER_PATH=$bin
    $env:VK_INSTANCE_LAYERS='VK_LAYER_EVR_diagnostic'
    if($RenderDoc){
        $env:VK_LAYER_PATH=$renderDocPath+';'+$bin
        # Observe the same wrapped image/device handles returned to the game.
        # If RenderDoc is above EVR, its wrapper handles cannot match EVR's map.
        $env:VK_INSTANCE_LAYERS='VK_LAYER_EVR_diagnostic;VK_LAYER_RENDERDOC_Capture'
        $env:ENABLE_VULKAN_RENDERDOC_CAPTURE='1'
        Remove-Item Env:DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46 -ErrorAction SilentlyContinue
    }
    if($VulkanValidation -ne 'None'){
        $env:VK_LAYER_PATH+=';'+$validationPath
        $env:VK_INSTANCE_LAYERS+=';VK_LAYER_KHRONOS_validation'
        $env:VK_LAYER_SETTINGS_PATH=Join-Path $out 'vk_layer_settings.txt'
        $validationSettings=@(
            'khronos_validation.validate_core = true'
            ('khronos_validation.validate_sync = '+($VulkanValidation -eq 'Sync').ToString().ToLower())
            'khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG'
            ('khronos_validation.log_filename = '+(Join-Path $out 'validation.log'))
            'khronos_validation.report_flags = error,warn'
            'khronos_validation.duplicate_message_limit = 20'
        )
        $validationSettings | Set-Content -LiteralPath $env:VK_LAYER_SETTINGS_PATH
        @{mode=$VulkanValidation;library=$validationManifest;layer_path=$env:VK_LAYER_PATH;layers=$env:VK_INSTANCE_LAYERS;settings_path=$env:VK_LAYER_SETTINGS_PATH;scope='child process only; no installer or registry writes'} |
            ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'validation-config.json')
    }
    $env:EVR_LAYER_OUTPUT=$out
    $env:SteamAppId='1203620'
    $env:SteamGameId='1203620'
    $env:VK_LOADER_DEBUG='error,warn,layer'
    $env:EVR_PREPARE_OPENXR=if ($PrepareVr -or $PrepareMock -or $PrepareRealVr) {'1'} else {'0'}
    $env:EVR_PREPARE_FIXTURE='0'
    $env:EVR_PREPARE_FAIL_DEVICE='0'
    $env:EVR_PREPARE_SESSION_FIXTURE='0'
    $env:EVR_LIVE_NATIVE_XR=if($LiveStereo){'1'}else{'0'}
    $env:EVR_PREPARE_GAME_SESSION=if($SessionProbe){'1'}else{'0'}
    if ($PrepareMock) {
        $env:EVR_PREPARE_LOADER=(Resolve-Path -LiteralPath (Join-Path $bin 'openxr_loader.dll')).Path
        $env:XR_RUNTIME_JSON=(Resolve-Path -LiteralPath (Join-Path $bin 'evr_mock_runtime.json')).Path
        $env:EVR_MOCK_LOG=Join-Path $out 'mock.jsonl'
        $mockPoseSource=$null
        $mockPoseHash=$null
        if($MockPoseSequence){
            $mockPoseSource=(Resolve-Path -LiteralPath $MockPoseSequence).Path
            $mockPoseCopy=Join-Path $out 'mock-pose.csv'
            Copy-Item -LiteralPath $mockPoseSource -Destination $mockPoseCopy
            $mockPoseHash=(Get-FileHash -LiteralPath $mockPoseSource -Algorithm SHA256).Hash
            if((Get-FileHash -LiteralPath $mockPoseCopy -Algorithm SHA256).Hash -ne $mockPoseHash){throw 'Mock pose snapshot hash differs.'}
            $env:EVR_POSE_SEQUENCE=$mockPoseCopy
        }else{$env:EVR_POSE_SEQUENCE=''}
        @{mode=if($MockPoseSequence){'csv_sequence'}else{'static_default'};source=$mockPoseSource;sha256=$mockPoseHash;runtime_input=$env:EVR_POSE_SEQUENCE} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'mock-pose-input.json')
    }
    if ($PrepareVr) {
        $env:EVR_PREPARE_LOADER=(Resolve-Path -LiteralPath (Join-Path $bin 'openxr_loader.dll')).Path
        $env:XR_RUNTIME_JSON=(Resolve-Path -LiteralPath $runtimeManifest).Path
        $env:VR_PATHREG_OVERRIDE=Join-Path $isolated 'openvrpaths.vrpath'
        $env:VR_CONFIG_PATH=Join-Path $isolated 'config'
        $env:VR_LOG_PATH=Join-Path $isolated 'logs'
    }
    if ($PrepareRealVr) {
        $env:EVR_PREPARE_LOADER=(Resolve-Path -LiteralPath (Join-Path $bin 'openxr_loader.dll')).Path
        $env:XR_RUNTIME_JSON=$realVr.manifest
        # The user's own SteamVR paths/config/logs: no isolated overrides in the game process.
        foreach($key in @('VR_PATHREG_OVERRIDE','VR_CONFIG_PATH','VR_LOG_PATH','EVR_POSE_SEQUENCE','EVR_MOCK_LOG')){Remove-Item "Env:$key" -ErrorAction SilentlyContinue}
    }
    if($CaptureShaders){$env:EVR_CAPTURE_SHADERS='1'}else{Remove-Item Env:EVR_CAPTURE_SHADERS -ErrorAction SilentlyContinue}
    $arguments=@{}
    if ($DirectPlay) { $arguments.ArgumentList='--play' }
    # Interactive tests need an actual visible game surface; helper processes remain hidden.
    $gameWindowStyle=if($InteractiveWindow){'Normal'}else{'Hidden'}
    # VR launches render the eyes with DLSS (user decision 2026-10-06); only antiAliasing (and dlss with
    # -VrDlssQuality) change, and a helper puts them back after the game exits so the flat-screen settings stay.
    # Sharpness (user request 2026-10-06 "くっきりしてる方が見栄え良い"): -VrSharpness (0..1, default 0.5 = the NIS
    # midpoint; 0 = off; Keep = the game's value) is recorded in launch.json and the probe writes it into the eye render
    # parameters (with DLSS the game passes 0 to its own sharpen pass). enshrouded_local.json is not touched for it.
    $vrSharpnessValue=if($VrSharpness -eq 'Keep'){''}else{$VrSharpness}
    if($LiveStereo -and $VrUpscaler -eq 'Dlss'){
        $vrSettingsRecord=Join-Path $out 'vr-render-settings.json'
        # 1.0 (2026-10-16): the machine settings moved to Saved Games\keen games\Enshrouded\local_settings.json (the game
        # folder's enshrouded_local.json is not read any more: Mock run game-20261016-063118 asked for DLSS and drew FSR3).
        $machineSettings=Join-Path $env:USERPROFILE 'Saved Games\keen games\Enshrouded\local_settings.json'
        if(!(Test-Path -LiteralPath $machineSettings)){$machineSettings=Join-Path $gameDir 'enshrouded_local.json'}
        $vrSettings=& (Join-Path $PSScriptRoot 'tools/vr_render_settings.ps1') -Action Apply -Settings $machineSettings -Record $vrSettingsRecord -DlssQuality $VrDlssQuality | ConvertFrom-Json
        Write-Output "VR render settings: antiAliasing $($vrSettings.original.antiAliasing) -> $($vrSettings.applied.antiAliasing), dlss $($vrSettings.applied.dlss), eye sharpness $(if($vrSharpnessValue){$vrSharpnessValue}else{'game'}) (changed=$($vrSettings.changed))"
    }
    $process=Start-Process -FilePath (Join-Path $gameDir 'enshrouded.exe') -WorkingDirectory $gameDir -WindowStyle $gameWindowStyle -PassThru -RedirectStandardError (Join-Path $out 'loader_stderr.txt') @arguments
    @{pid=$process.Id;output=$out;interactive_window=[bool]$InteractiveWindow;mode=if($PrepareVr -or $PrepareMock -or $PrepareRealVr){'openxr_graphics_preparation'}else{'diagnostic_only'};grip_trim=$gripTrimValue;runtime=if($PrepareRealVr){'real_steamvr'}elseif($PrepareVr){'isolated_steamvr'}elseif($PrepareMock){'mock'}else{'none'};real_vr=$realVr;render_cpu_arena_mib=$RenderCpuArenaMiB;renderdoc=[bool]$RenderDoc;vulkan_validation=$VulkanValidation;shadow_receiver_shader=$shadowReceiver;gi_probe_shader=$giProbe;head_visibility_shader=[bool]$HeadVisibilityShader;hands_only=[bool]$HandsOnly;xr_async_completion=[bool]$XrAsyncCompletion;symmetric_fov=[bool]$SymmetricFov;eye_luma_every=$EyeLumaEvery;vr_upscaler=if($vrSettingsRecord){$VrUpscaler}else{'unchanged'};vr_sharpness=if($LiveStereo -and $vrSharpnessValue){$vrSharpnessValue}else{'game'};vr_render_settings=$vrSettingsRecord;log_name_cap=$LogNameCap;desktop_mirror=[bool]$desktopMirrorOn;fov_crop=![bool]$NoFovCrop;fov_crop_margin_deg=$FovCropMarginDeg;mock_visibility_mask=$MockVisibilityMask;gpu_timing_every=$GpuTimingEvery;eye_image_sweep=[bool]$EyeImageSweep;queue_trace_live=$QueueTraceLive;log_buffered=[bool]$LogBuffered;mock_navigation=[bool]$MockNavigate;mock_navigation_hand_input=[bool]$MockNavigateHands;motion_hands=[bool]$MotionHands;vfx_transparent_shader=[bool]$VfxTransparentShader;ui_pipeline_probe=[bool]($UiPipelineProbe -or $UiCapture);ui_capture=[bool]$UiCapture;ui_quad=[bool]$UiQuad;body_clip_prepare=[bool]$PrepareBodyClip;body_clip_shader=[bool]$BodyClipShader;direct_play=[bool]$DirectPlay;save_backup=$saveBackup;session_probe=[bool]$SessionProbe;live_stereo=[bool]$LiveStereo;started=(Get-Date).ToString('o');creation_filetime=$process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString()} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'launch.json')
    Write-Output "Launched PID $($process.Id); diagnostics: $out"
} finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
    # The game never started: put the VR render settings back now.
    if($vrSettingsRecord -and !$process -and (Test-Path -LiteralPath $vrSettingsRecord)){& (Join-Path $PSScriptRoot 'tools/vr_render_settings.ps1') -Action RestoreNow -Record $vrSettingsRecord | Out-Null}
}
# Start after restoring this shell's environment. The helper does not need Vulkan
# layers or a Runtime override, and uses its process handle only to observe lifetime.
$watcher=Join-Path $PSScriptRoot 'tools/watch_diagnostic_process.ps1'
$watchArgs=@('-NoProfile','-File',('"'+$watcher+'"'),'-LaunchJson',('"'+(Join-Path $out 'launch.json')+'"'),'-ExpectedExecutable',('"'+(Join-Path $gameDir 'enshrouded.exe')+'"'))
Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $watchArgs -WindowStyle Hidden -WorkingDirectory $gameDir -RedirectStandardError (Join-Path $out 'process-watch-stderr.txt') | Out-Null
if($vrSettingsRecord){
    $restoreArgs=@('-NoProfile','-File',('"'+(Join-Path $PSScriptRoot 'tools/vr_render_settings.ps1')+'"'),'-Action','Restore','-Record',('"'+$vrSettingsRecord+'"'),'-LaunchJson',('"'+(Join-Path $out 'launch.json')+'"'),'-ExpectedExecutable',('"'+(Join-Path $gameDir 'enshrouded.exe')+'"'))
    Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $restoreArgs -WindowStyle Hidden -WorkingDirectory $gameDir -RedirectStandardOutput (Join-Path $out 'vr-render-settings-restore.txt') -RedirectStandardError (Join-Path $out 'vr-render-settings-restore-stderr.txt') | Out-Null
}
