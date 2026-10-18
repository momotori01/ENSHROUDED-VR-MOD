param(
    [ValidateSet('Backup','Verify')][string]$Action = 'Backup',
    [string]$Snapshot
)
$ErrorActionPreference = 'Stop'
$mod = Split-Path $PSScriptRoot -Parent
$registryPaths = @(
    'HKLM:\SOFTWARE\Khronos\OpenXR\1',
    'HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1',
    'HKCU:\SOFTWARE\Khronos\OpenXR\1'
)
function Read-RuntimeRegistry {
    foreach ($key in $registryPaths) {
        $value = Get-ItemProperty -LiteralPath $key -Name ActiveRuntime -ErrorAction SilentlyContinue
        @{key=$key;present=($null -ne $value);value=if($value){$value.ActiveRuntime}else{$null}}
    }
}
if ($Action -eq 'Backup') {
    if (!$Snapshot) { $Snapshot = Join-Path $mod ('output/host-backups/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
    New-Item -ItemType Directory -Path $Snapshot | Out-Null
    $pathsFile = Join-Path $env:LOCALAPPDATA 'openvr/openvrpaths.vrpath'
    $paths = @($pathsFile)
    if (Test-Path -LiteralPath $pathsFile) {
        $registration = Get-Content -LiteralPath $pathsFile -Raw | ConvertFrom-Json
        foreach ($config in $registration.config) {
            foreach ($name in @('steamvr.vrsettings','chaperone_info.vrchap','lighthouse/lighthousedb.json')) {
                $paths += Join-Path $config $name
            }
        }
    }
    $i = 0
    $files = foreach ($path in ($paths | Select-Object -Unique)) {
        $exists = Test-Path -LiteralPath $path -PathType Leaf
        $record = @{path=$path;exists=$exists;backup=$null;sha256=$null;lastWriteUtc=$null}
        if ($exists) {
            $record.backup = ('file-{0:D2}.backup' -f $i++)
            Copy-Item -LiteralPath $path -Destination (Join-Path $Snapshot $record.backup)
            $record.sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            $record.lastWriteUtc = (Get-Item -LiteralPath $path).LastWriteTimeUtc.ToString('o')
        }
        $record
    }
    $machineEnvironment = foreach ($name in @('XR_RUNTIME_JSON','VR_PATHREG_OVERRIDE','VR_CONFIG_PATH','VR_LOG_PATH','VK_INSTANCE_LAYERS','VK_LAYER_PATH')) {
        @{name=$name;user=[Environment]::GetEnvironmentVariable($name,'User');machine=[Environment]::GetEnvironmentVariable($name,'Machine')}
    }
    @{created=(Get-Date).ToString('o');user=[Security.Principal.WindowsIdentity]::GetCurrent().Name;files=@($files);registry=@(Read-RuntimeRegistry);persistentEnvironment=@($machineEnvironment);policy='Read-only baseline. Test uses only child process overrides.'} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Snapshot 'manifest.json')
    Write-Output $Snapshot
    exit 0
}
if (!$Snapshot) { throw 'Verify requires -Snapshot with a previously created backup directory.' }
$baseline = Get-Content -LiteralPath (Join-Path $Snapshot 'manifest.json') -Raw | ConvertFrom-Json
if ($baseline.user -ne [Security.Principal.WindowsIdentity]::GetCurrent().Name) { throw 'Run verification as the same Windows user who created the snapshot.' }
$checks = @()
foreach ($file in $baseline.files) {
    $exists = Test-Path -LiteralPath $file.path -PathType Leaf
    $same = $exists -eq $file.exists
    if ($same -and $exists) { $same = (Get-FileHash -LiteralPath $file.path -Algorithm SHA256).Hash -eq $file.sha256 }
    $checks += @{name=$file.path;pass=$same}
}
$current = @(Read-RuntimeRegistry)
foreach ($reg in $baseline.registry) {
    $now = $current | Where-Object { $_.key -eq $reg.key }
    $checks += @{name=$reg.key+'/ActiveRuntime';pass=($reg.present -eq $now.present -and $reg.value -eq $now.value)}
}
foreach ($variable in $baseline.persistentEnvironment) {
    $checks += @{name='Persistent environment '+$variable.name;pass=(
        [Environment]::GetEnvironmentVariable($variable.name,'User') -ceq $variable.user -and
        [Environment]::GetEnvironmentVariable($variable.name,'Machine') -ceq $variable.machine)}
}
$checks | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Snapshot 'verification.json')
foreach ($check in $checks) { Write-Output "$(if($check.pass){'PASS'}else{'FAIL'}) $($check.name)" }
if ($checks | Where-Object { !$_.pass }) { throw 'Host VR settings differ from backup. No file was overwritten; inspect changes before restoring user settings.' }
