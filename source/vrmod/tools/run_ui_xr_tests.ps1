param([switch]$SkipBuild,[switch]$IncludeScreen,[switch]$IncludeMotion,[string]$BuildDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$workspace=Split-Path $root -Parent
if(!$BuildDirectory){$BuildDirectory=Join-Path $root 'build'}
$BuildDirectory=(Resolve-Path -LiteralPath $BuildDirectory).Path
$testNames=@('ui_xr','ui_transfer')
if($IncludeScreen){$testNames+=@('screen_xr')}
if($IncludeMotion){$testNames+=@('motion_xr')}
$targets=@('evr_mock_runtime')+@($testNames | ForEach-Object {'evr_'+$_+'_tests'})
if(!$SkipBuild){& (Join-Path $root 'tools/python/cmake/data/bin/cmake.exe') --build $BuildDirectory --config Release --target @targets --parallel 8;if($LASTEXITCODE){throw 'UI XR build failed'}}
$out=Join-Path $root ('output/ui-xr-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $out | Out-Null
$implicit=Join-Path $root 'test_env/empty-implicit-layers'
New-Item -ItemType Directory -Force -Path $implicit | Out-Null
if(@(Get-ChildItem -LiteralPath $implicit -Force).Count){throw 'Implicit layer isolation is not empty'}
$validation=Join-Path $workspace 'references/vulkan-sdk/portable/Bin'
$manifest=Get-Content -LiteralPath (Join-Path $workspace 'references/vulkan-sdk/portable-manifest.json') -Raw | ConvertFrom-Json
foreach($file in $manifest.files){if((Get-FileHash -LiteralPath (Join-Path $validation $file.name) -Algorithm SHA256).Hash -ne $file.sha256){throw 'Validation hash mismatch'}}
$settings=Join-Path $out 'vk_layer_settings.txt'
@('khronos_validation.validate_core = true','khronos_validation.validate_sync = true','khronos_validation.report_flags = error,warn','khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG',('khronos_validation.log_filename = '+(Join-Path $out 'validation.log'))) | Set-Content -LiteralPath $settings -Encoding ascii
$keys=@('VK_LAYER_PATH','VK_IMPLICIT_LAYER_PATH','VK_INSTANCE_LAYERS','VK_LAYER_SETTINGS_PATH','VK_LOADER_LAYERS_ENABLE','VK_LOADER_LAYERS_DISABLE','VK_LOADER_LAYERS_ALLOW','XR_RUNTIME_JSON','EVR_MOCK_LOG','EVR_POSE_SEQUENCE','EVR_MOCK_HAND_SEQUENCE')
$prior=@{}
foreach($key in $keys){$prior[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
try{
    $env:VK_LAYER_PATH=$validation;$env:VK_IMPLICIT_LAYER_PATH=$implicit;$env:VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation';$env:VK_LAYER_SETTINGS_PATH=$settings
    $env:XR_RUNTIME_JSON=Join-Path $BuildDirectory 'bin/Release/evr_mock_runtime.json'
    foreach($key in @('VK_LOADER_LAYERS_ENABLE','VK_LOADER_LAYERS_DISABLE','VK_LOADER_LAYERS_ALLOW','EVR_POSE_SEQUENCE')){Remove-Item -LiteralPath ('Env:'+$key) -ErrorAction SilentlyContinue}
    Push-Location $out
    try{
        foreach($test in $testNames){
            if($test -eq 'screen_xr'){$env:EVR_POSE_SEQUENCE=Join-Path $root 'config/poses/13_continuous.csv'}else{Remove-Item Env:EVR_POSE_SEQUENCE -ErrorAction SilentlyContinue}
            if($test -eq 'motion_xr'){$env:EVR_MOCK_HAND_SEQUENCE=Join-Path $root 'test_env/motion-hands.csv'}else{Remove-Item Env:EVR_MOCK_HAND_SEQUENCE -ErrorAction SilentlyContinue}
            $env:EVR_MOCK_LOG=Join-Path $out ($test+'-mock.jsonl')
            $binary=Join-Path $BuildDirectory ('bin/Release/evr_'+$test+'_tests.exe')
            & $binary (Join-Path $out 'gpu-images') *> (Join-Path $out ($test+'.log'))
            $result=$LASTEXITCODE
            @{executable=$binary;sha256=(Get-FileHash -LiteralPath $binary).Hash;exitCode=$result;scope='process-only Vulkan layers and Mock Runtime';validation=$manifest} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out ($test+'-run.json'))
            Get-Content -LiteralPath (Join-Path $out ($test+'.log')) -Tail 2
            if($result){throw "UI XR test failed: $test"}
        }
    }finally{Pop-Location}
    $validationLog=Join-Path $out 'validation.log'
    if((Test-Path -LiteralPath $validationLog) -and (Get-Item -LiteralPath $validationLog).Length){throw "Vulkan validation messages: $validationLog"}
    Write-Output "PASS Core+Sync validation: 0 warnings/errors; evidence $out"
}finally{
    foreach($key in $keys){if($null -eq $prior[$key]){Remove-Item -LiteralPath ('Env:'+$key) -ErrorAction SilentlyContinue}else{[Environment]::SetEnvironmentVariable($key,$prior[$key],'Process')}}
}
