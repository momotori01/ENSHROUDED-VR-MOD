param([switch]$Virtual)
$ErrorActionPreference='Stop'
$runtime='C:\Program Files (x86)\Steam\steamapps\common\SteamVR'
if(!(Test-Path -LiteralPath (Join-Path $runtime 'steamxr_win64.json'))){throw 'SteamVR runtime not found'}
$test=Join-Path $PSScriptRoot 'test_env/steamvr'
$config=Join-Path $test 'config'
$logs=Join-Path $test 'logs'
New-Item -ItemType Directory -Force $config,$logs | Out-Null
Remove-Item -LiteralPath (Join-Path $test 'shutdown.request') -ErrorAction SilentlyContinue
$drivers=@()
if($Virtual){$drivers=@((Join-Path $PSScriptRoot 'test_env/evr_virtual'))}
@{jsonid='vrpathreg';version=1;runtime=@($runtime);config=@($config);log=@($logs);external_drivers=$drivers} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $test 'openvrpaths.vrpath')
$driverName=if($Virtual){'evr_virtual'}else{'null'}
if(!(Test-Path -LiteralPath (Join-Path $test 'pose.txt'))){'pose 0 1.65 0 0 0 0 0.064 1 1' | Set-Content -LiteralPath (Join-Path $test 'pose.txt')}
$settings=@{
 steamvr=@{forcedDriver=$driverName;requireHmd=$false;activateMultipleDrivers=$false;enableHomeApp=$false;showMirrorView=$false;startMonitorFromAppLaunch=$false;supersampleScale=1.0}
 power=@{pauseCompositorOnStandby=$false;turnOffScreensTimeout=0.0}
 dashboard=@{enableDashboard=$false}
 driver_evr_virtual=@{enable=[bool]$Virtual;renderWidth=640;renderHeight=720;poseFile=(Join-Path $test 'pose.txt')}
 driver_null=@{enable=$true;serialNumber='EVR-Test-Null';modelNumber='EVR Null Test';windowWidth=1280;windowHeight=720;renderWidth=640;renderHeight=720;displayFrequency=90.0;secondsFromVsyncToPhotons=0.01111111}
}
# Installed always-active addons can otherwise load even with forcedDriver.
# Disable them only in this isolated configuration, preserving every real setting.
foreach ($directory in (Get-ChildItem (Join-Path $runtime 'drivers') -Directory)) {
    $manifest = Join-Path $directory.FullName 'driver.vrdrivermanifest'
    if (Test-Path -LiteralPath $manifest) {
        $name = (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).name
        if ($name -and $name -ne 'null') { $settings['driver_'+$name] = @{enable=$false} }
    }
}
$settings | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $config 'steamvr.vrsettings')
$chaperone = Join-Path $config 'chaperone_info.vrchap'
& {
    # Synthetic 3m square, identity origins. Never copy the user's physical room geometry.
    $bounds = @(
        @(@(-1.5,0,-1.5),@(-1.5,2.5,-1.5),@(1.5,2.5,-1.5),@(1.5,0,-1.5)),
        @(@(1.5,0,-1.5),@(1.5,2.5,-1.5),@(1.5,2.5,1.5),@(1.5,0,1.5)),
        @(@(1.5,0,1.5),@(1.5,2.5,1.5),@(-1.5,2.5,1.5),@(-1.5,0,1.5)),
        @(@(-1.5,0,1.5),@(-1.5,2.5,1.5),@(-1.5,2.5,-1.5),@(-1.5,0,-1.5))
    )
    $universes = foreach ($universe in @('170000001','2')) { @{universeID=$universe;time='2026-09-12T00:00:00Z';
        standing=@{translation=@(0,0,0);yaw=0};seated=@{translation=@(0,0,0);yaw=0};
        play_area=@(3,3);collision_bounds=$bounds} }
    @{jsonid='chaperone_info';version=5;universes=@($universes)} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $chaperone
}
Write-Output "Isolated SteamVR test configuration: $test"
