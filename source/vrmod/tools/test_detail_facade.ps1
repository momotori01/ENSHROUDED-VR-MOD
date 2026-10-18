$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskWorkspace=Split-Path $taskRoot -Parent
$taskMsvc='C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133'
$taskSdk='C:/Program Files (x86)/Windows Kits/10'
$taskOutput=Join-Path $taskWorkspace 'docs/analysis/detail_facade_tests'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskIncludeBefore=$env:INCLUDE
$taskLibBefore=$env:LIB
try {
    $env:INCLUDE="$taskMsvc/include;$taskSdk/Include/10.0.19041.0/ucrt;$taskSdk/Include/10.0.19041.0/shared;$taskSdk/Include/10.0.19041.0/um"
    $env:LIB="$taskMsvc/lib/x64;$taskSdk/Lib/10.0.19041.0/ucrt/x64;$taskSdk/Lib/10.0.19041.0/um/x64"
    & "$taskMsvc/bin/Hostx64/x64/cl.exe" /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /Gy /MT "/I$taskRoot/include" "/I$taskWorkspace/references/minhook/include" "/Fo$taskOutput/detail_facade_tests.obj" "/Fe$taskOutput/detail_facade_tests.exe" "$PSScriptRoot/test_detail_facade.cpp" /link /INCREMENTAL:NO /OPT:REF *> "$taskOutput/build.log"
    if($LASTEXITCODE){Get-Content "$taskOutput/build.log";throw 'Offline facade test build failed'}
    & "$taskOutput/detail_facade_tests.exe" | Tee-Object -FilePath "$taskOutput/results.txt"
    if($LASTEXITCODE){throw 'Offline facade tests failed'}
} finally {
    $env:INCLUDE=$taskIncludeBefore
    $env:LIB=$taskLibBefore
}
