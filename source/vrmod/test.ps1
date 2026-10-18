param([switch]$Long,[string]$Python='python')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$previous = @{}
foreach ($key in @('XR_RUNTIME_JSON','EVR_MOCK_LOG','EVR_POSE_SEQUENCE','EVR_MOCK_REALTIME')) { $previous[$key] = [Environment]::GetEnvironmentVariable($key,'Process') }
Push-Location $repo
try {
    & $Python vrmod/tools/pose_sequences.py
    if ($LASTEXITCODE) { throw 'Pose generation failed' }
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
    $out=Join-Path $PSScriptRoot "output/test-$stamp"
    New-Item -ItemType Directory -Force $out | Out-Null
    $env:XR_RUNTIME_JSON=(Resolve-Path vrmod/build/bin/Release/evr_mock_runtime.json).Path
    $env:EVR_MOCK_LOG=Join-Path $out 'mock_runtime.jsonl'
    $env:EVR_POSE_SEQUENCE=(Resolve-Path vrmod/config/poses/suite.csv).Path
    $suiteFrames=(Get-Content -LiteralPath $env:EVR_POSE_SEQUENCE).Count
    Remove-Item Env:EVR_MOCK_REALTIME -ErrorAction SilentlyContinue
    & vrmod/build/bin/Release/evr_core_tests.exe | Tee-Object -FilePath (Join-Path $out 'core.txt')
    if ($LASTEXITCODE) { throw 'Core tests failed' }
    & vrmod/build/bin/Release/evr_xr_probe.exe --frames $suiteFrames --cycles 2 --capture-every 180 --output $out | Tee-Object -FilePath (Join-Path $out 'probe.txt')
    if ($LASTEXITCODE) { throw 'OpenXR test failed' }
    & $Python vrmod/tools/check_capture.py $out --expect-motion | Tee-Object -FilePath (Join-Path $out 'images.txt')
    if ($LASTEXITCODE) { throw 'Image checks failed' }
    if ($Long) {
        $env:EVR_POSE_SEQUENCE=(Resolve-Path vrmod/config/poses/13_continuous.csv).Path
        $env:EVR_MOCK_REALTIME='1'
        & vrmod/build/bin/Release/evr_xr_probe.exe --frames 16200 --capture-every 2700 --output (Join-Path $out 'long')
        if ($LASTEXITCODE) { throw 'Long test failed' }
    }
    Write-Output "Results: $out"
} finally { Pop-Location; foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key,$previous[$key],'Process') } }
