param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$out = Join-Path $PSScriptRoot "output/game-math-$stamp"
New-Item -ItemType Directory -Path $out | Out-Null
$vectors = Join-Path $out 'adapter-vectors.json'
Push-Location $repo
try {
    & (Join-Path $PSScriptRoot 'build/bin/Release/evr_game_camera_tests.exe') --vectors $vectors |
        Tee-Object -FilePath (Join-Path $out 'invariants.txt')
    if ($LASTEXITCODE) { throw 'Camera adapter invariant tests failed' }
    $stereoVectors = Join-Path $out 'stereo-vectors.json'
    & (Join-Path $PSScriptRoot 'build/bin/Release/evr_game_stereo_tests.exe') --vectors $stereoVectors |
        Tee-Object -FilePath (Join-Path $out 'stereo-invariants.txt')
    if ($LASTEXITCODE) { throw 'Stereo camera invariant tests failed' }
    & $Python (Join-Path $PSScriptRoot 'tools/emulate_camera_math.py') --adapter-vectors $vectors --stereo-vectors $stereoVectors --output (Join-Path $out 'emulated.json') |
        Tee-Object -FilePath (Join-Path $out 'comparison.txt')
    if ($LASTEXITCODE) { throw 'Camera adapter differs from emulated game math' }
    & $Python (Join-Path $PSScriptRoot 'tools/emulate_frustum_math.py') --output (Join-Path $out 'frustum.json') |
        Tee-Object -FilePath (Join-Path $out 'frustum.txt')
    if ($LASTEXITCODE) { throw 'Asymmetric game frustum geometry failed' }
    Write-Output "PASS Offline camera math: $out"
} finally {
    Pop-Location
}
