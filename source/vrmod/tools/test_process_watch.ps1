$ErrorActionPreference='Stop'
$mod=Split-Path $PSScriptRoot -Parent
$out=Join-Path $mod ('output/process-watch-fixture-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $out | Out-Null
$child=Join-Path $out 'child.ps1'
Set-Content -LiteralPath $child 'param([int]$Code) Start-Sleep -Seconds 4; exit $Code'
$executable=Join-Path $PSHOME 'pwsh.exe'
foreach($case in @(@{name='normal';code=0},@{name='nonzero';code=7},@{name='identity_rejected';code=0})){
    $directory=Join-Path $out $case.name
    New-Item -ItemType Directory -Path $directory | Out-Null
    $taskChild=Start-Process -FilePath $executable -ArgumentList @('-NoProfile','-File',('"'+$child+'"'),'-Code',$case.code) -WindowStyle Hidden -PassThru
    $creation=$taskChild.StartTime.ToUniversalTime().ToFileTimeUtc()
    if($case.name -eq 'identity_rejected'){$creation++}
    @{pid=$taskChild.Id;creation_filetime=$creation.ToString()} | ConvertTo-Json | Set-Content (Join-Path $directory 'launch.json')
    & (Join-Path $PSScriptRoot 'watch_diagnostic_process.ps1') -LaunchJson (Join-Path $directory 'launch.json') -ExpectedExecutable $executable -TimeoutMinutes 1
    $record=Get-Content (Join-Path $directory 'process-exit.json') -Raw | ConvertFrom-Json
    if($case.name -eq 'identity_rejected'){
        if($record.status -ne 'not_attached' -or $record.error -notmatch 'identity differs'){throw 'FAIL process identity was not rejected'}
    }else{
        if($record.status -ne 'exited' -or $record.exit_code -ne $case.code){throw ('FAIL exit code observation: '+($record | ConvertTo-Json -Compress))}
    }
    $taskChild.WaitForExit()
    $taskChild.Dispose()
    Write-Output "PASS $($case.name)"
}
Write-Output "Evidence: $out"
