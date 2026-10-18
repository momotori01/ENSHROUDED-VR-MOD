param(
    [string]$ShaderDirectory,
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$workspace=Split-Path $root -Parent
if(!$ShaderDirectory){$ShaderDirectory=Join-Path $root 'output/game-20260927-051956/view-resource-probe-20260927-052111/ui-raster'}
$ShaderDirectory=(Resolve-Path -LiteralPath $ShaderDirectory).Path
$cmake=Join-Path $root 'tools/python/cmake/data/bin/cmake.exe'
if(!$SkipBuild){
    & $cmake --build (Join-Path $root 'build') --config Release --target evr_ui_gpu_tests --parallel 8
    if($LASTEXITCODE){throw 'UI fixture build failed'}
}
$out=Join-Path $root ('output/ui-gpu-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
# Keep this folder empty. It replaces implicit layer discovery for this child only.
$implicit=Join-Path $root 'test_env/empty-implicit-layers'
New-Item -ItemType Directory -Force -Path $implicit | Out-Null
if(@(Get-ChildItem -LiteralPath $implicit -Force).Count){throw 'Isolated implicit-layer directory must be empty'}
$validation=Join-Path $workspace 'references/vulkan-sdk/portable/Bin'
$manifest=Get-Content -LiteralPath (Join-Path $workspace 'references/vulkan-sdk/portable-manifest.json') -Raw | ConvertFrom-Json
foreach($file in $manifest.files){
    if((Get-FileHash -LiteralPath (Join-Path $validation $file.name) -Algorithm SHA256).Hash -ne $file.sha256){throw 'Validation library hash mismatch'}
}
$settings=$out+'-vk-settings.txt'
@(
    'khronos_validation.validate_core = true'
    'khronos_validation.validate_sync = true'
    'khronos_validation.report_flags = error,warn'
) | Set-Content -LiteralPath $settings -Encoding ASCII
$keys=@('VK_LAYER_PATH','VK_IMPLICIT_LAYER_PATH','VK_INSTANCE_LAYERS','VK_LAYER_SETTINGS_PATH','VK_LOADER_LAYERS_ENABLE','VK_LOADER_LAYERS_DISABLE','VK_LOADER_LAYERS_ALLOW')
$prior=@{}
foreach($key in $keys){$prior[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
try{
    $env:VK_LAYER_PATH=$validation
    $env:VK_IMPLICIT_LAYER_PATH=$implicit
    $env:VK_LAYER_SETTINGS_PATH=$settings
    foreach($key in @('VK_INSTANCE_LAYERS','VK_LOADER_LAYERS_ENABLE','VK_LOADER_LAYERS_DISABLE','VK_LOADER_LAYERS_ALLOW')){
        Remove-Item -LiteralPath ('Env:' + $key) -ErrorAction SilentlyContinue
    }
    $binary=Join-Path $root 'build/bin/Release/evr_ui_gpu_tests.exe'
    & $binary $ShaderDirectory (Join-Path $root 'build/ui_fixture.vert.spv') $out *> ($out+'.log')
    $result=$LASTEXITCODE
    $record=@{
        scope='child-process environment only; no registry or host VR writes'
        shaderDirectory=$ShaderDirectory
        executable=$binary
        executableSha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
        validationManifest=$manifest
        validationSettings=$settings
        implicitLayerPath=$implicit
        exitCode=$result
        time=(Get-Date).ToString('o')
    }
    $record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath ($out+'-run.json') -Encoding UTF8
    Write-Output $out
    Get-Content -LiteralPath ($out+'.log') -Tail 8
    if($result){throw "UI GPU tests failed; evidence retained at $out"}
    foreach($name in @('0-mrt.spv','1-mrt.spv','0-additive-mrt.spv','1-additive-mrt.spv')){
        & (Join-Path $workspace 'references/vulkan-sdk/tools/Bin/spirv-val.exe') --target-env vulkan1.3 (Join-Path $out $name)
        if($LASTEXITCODE){throw "SPIR-V validation failed: $name"}
    }
    Write-Output 'PASS patched normal/font SPIR-V validation'
}finally{
    foreach($key in $keys){if($null -eq $prior[$key]){Remove-Item -LiteralPath ('Env:' + $key) -ErrorAction SilentlyContinue}else{[Environment]::SetEnvironmentVariable($key,$prior[$key],'Process')}}
}
