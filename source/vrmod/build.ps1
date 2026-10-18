param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$cmake = Join-Path $root 'tools/python/cmake/data/bin/cmake.exe'
if (!(Test-Path -LiteralPath $cmake)) { throw 'Run tools/bootstrap.ps1 first.' }
$generation = Join-Path $root ('build/diagnostic-generations/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
if(Test-Path -LiteralPath $generation){throw 'Diagnostic generation already exists; refusing to overwrite its evidence.'}
$configureOptions = @("-DEVR_DIAGNOSTIC_OUTPUT_DIR=$generation")
Write-Output "Building a new diagnostic generation: $generation"
& $cmake -S $root -B (Join-Path $root 'build') -G 'Visual Studio 16 2019' -A x64 @configureOptions
if ($LASTEXITCODE) { throw 'CMake configuration failed' }
& $cmake --build (Join-Path $root 'build') --config Release --parallel 8
if ($LASTEXITCODE) { throw 'Build failed' }
$evidence=@{builtAt=(Get-Date).ToString('o');tests='not_run';binaries=@()}
foreach($file in (Get-ChildItem -LiteralPath (Join-Path $generation 'Release') -Filter '*.dll')){
    $evidence.binaries+=@{path=$file.FullName;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash;bytes=$file.Length}
}
$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $generation 'build-evidence.json') -Encoding UTF8
if(!$SkipTests){
    & (Join-Path $root 'tools/python/cmake/data/bin/ctest.exe') --test-dir (Join-Path $root 'build') -C Release --output-on-failure
    $evidence.tests=if($LASTEXITCODE){'failed'}else{'passed'}
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $generation 'build-evidence.json') -Encoding UTF8
    if($evidence.tests -eq 'failed'){throw 'Tests failed'}
}
