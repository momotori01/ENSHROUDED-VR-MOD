param(
    [Parameter(Mandatory=$true)][ValidateSet('Apply','Restore','RestoreNow','SelfTest')][string]$Action,
    [string]$Settings,                  # the game's enshrouded_local.json (Apply)
    [string]$Record,                    # <launch>/vr-render-settings.json
    [string]$LaunchJson,                # Restore: wait for this managed process to exit
    [string]$ExpectedExecutable,
    [string]$DlssQuality='',            # Apply: optional DLSS quality (e.g. Performance); empty keeps the user's value
    [ValidateRange(1,720)][int]$TimeoutMinutes=720
)
# VR render settings (2026-10-06, user decision: VR launches use DLSS; the flat-screen settings stay the user's).
# Apply: change only graphics.render.antiAliasing (and dlss when -DlssQuality is given) in enshrouded_local.json,
#   keep the text otherwise byte-identical, and record the original values.
# Sharpness is not changed here: with DLSS the game does not use renderPreferences.sharpness for its own sharpen pass;
#   the VR sharpness is written into the eye render parameters by the probe (launch_diagnostics -VrSharpness).
# Restore: wait for the managed game process to exit (identity checked like the lifetime watcher), then put back
#   only the fields that still hold the values written by Apply. Other changes the user made in game are kept.
# RestoreNow: same restore without waiting (launch failed before the game started).
$ErrorActionPreference='Stop'
function Read-Field([string]$text,[string]$name){ $m=[regex]::Match($text,'"'+[regex]::Escape($name)+'":\s*"([^"]*)"'); if($m.Success){$m.Groups[1].Value}else{$null} }
function Set-Field([string]$text,[string]$name,[string]$value){
    $pattern='("'+[regex]::Escape($name)+'":\s*")([^"]*)(")'
    if(([regex]::Matches($text,$pattern)).Count -ne 1){throw "settings field '$name' not found exactly once"}
    [regex]::Replace($text,$pattern,{param($m) $m.Groups[1].Value+$value+$m.Groups[3].Value})
}
# A true/false field (graphics.vsync), read and written as the text true / false.
function Read-Flag([string]$text,[string]$name){ $m=[regex]::Match($text,'"'+[regex]::Escape($name)+'":\s*(true|false)'); if($m.Success){$m.Groups[1].Value}else{$null} }
function Set-Flag([string]$text,[string]$name,[string]$value){
    $pattern='("'+[regex]::Escape($name)+'":\s*)(true|false)'
    if(([regex]::Matches($text,$pattern)).Count -ne 1){throw "settings flag '$name' not found exactly once"}
    [regex]::Replace($text,$pattern,{param($m) $m.Groups[1].Value+$value})
}
function Write-Text([string]$path,[string]$text){ [IO.File]::WriteAllText($path,$text,(New-Object Text.UTF8Encoding $false)) }
function Apply([string]$settings,[string]$record,[string]$quality){
    $path=(Resolve-Path -LiteralPath $settings).Path
    $text=[IO.File]::ReadAllText($path)
    $original=@{antiAliasing=(Read-Field $text 'antiAliasing');dlss=(Read-Field $text 'dlss')}
    if(!$original.antiAliasing -or !$original.dlss){throw 'antiAliasing/dlss settings missing'}
    # DLAA belongs to RenderAntiAliasingSetting, not RenderDlssPresetSetting.
    # Preserve the DLSS upscaling preset so returning to DLSS keeps the user's choice.
    $applied=@{antiAliasing=if($quality -eq 'Dlaa'){'Dlaa'}else{'Dlss'};dlss=if($quality -and $quality -ne 'Dlaa'){$quality}else{$original.dlss}}
    $new=Set-Field $text 'antiAliasing' $applied.antiAliasing
    $new=Set-Field $new 'dlss' $applied.dlss
    # Diagnostic (2026-10-14, the user: "is the flicker of the shadows of moving things the game's own, from shadow
    # quality Low?"): EVR_VR_SHADOWS=Low|Medium|High|Ultra in the launcher's environment also sets
    # graphics.render.shadows for this launch. It is recorded and put back with the other two fields.
    if($env:EVR_VR_SHADOWS){
        if($env:EVR_VR_SHADOWS -notin 'Low','Medium','High','Ultra'){throw 'EVR_VR_SHADOWS: Low, Medium, High or Ultra'}
        $original.shadows=(Read-Field $text 'shadows');if(!$original.shadows){throw 'shadows setting missing'}
        $applied.shadows=$env:EVR_VR_SHADOWS;$new=Set-Field $new 'shadows' $applied.shadows
    }
    # Measurement (2026-10-14, the user: "I want more FPS"): EVR_VR_VSYNC=Off|On in the launcher's environment also sets
    # graphics.vsync for this launch - the game presents its desktop window every frame, and with vertical sync that
    # present waits for a monitor that is slower than the headset. Recorded and put back like the other fields.
    # ... and EVR_VR_RENDER="name=value,name=value" any of the quoted graphics.render fields (dynamicGi=Performance,
    # fog=Low, contactShadows=Disabled ...), for telling which setting carries how much of the frame's time.
    if($env:EVR_VR_RENDER){
        foreach($pair in ($env:EVR_VR_RENDER -split ',')){
            $name,$value=$pair.Trim() -split '=',2
            if($name -notmatch '^[A-Za-z0-9]+$' -or $value -notmatch '^[A-Za-z0-9]+$' -or $name -in 'antiAliasing','dlss','shadows','vsync'){throw "EVR_VR_RENDER: bad entry '$pair'"}
            $original[$name]=(Read-Field $text $name);if(!$original[$name]){throw "render setting '$name' missing"}
            $applied[$name]=$value;$new=Set-Field $new $name $value
        }
    }
    if($env:EVR_VR_VSYNC){
        if($env:EVR_VR_VSYNC -notin 'Off','On'){throw 'EVR_VR_VSYNC: Off or On'}
        $original.vsync=(Read-Flag $text 'vsync');if(!$original.vsync){throw 'vsync setting missing'}
        $applied.vsync=if($env:EVR_VR_VSYNC -eq 'On'){'true'}else{'false'};$new=Set-Flag $new 'vsync' $applied.vsync
    }
    $entry=[ordered]@{settings=$path;original=$original;applied=$applied;sha256_before=(Get-FileHash $path).Hash;changed=($new -ne $text);applied_at=(Get-Date).ToString('o')}
    if($entry.changed){
        $backup=Join-Path (Split-Path -Parent $record) 'enshrouded_local.before-vr.json'
        Copy-Item -LiteralPath $path -Destination $backup;$entry.backup=$backup
        Write-Text $path $new
    }
    $entry.sha256_after=(Get-FileHash $path).Hash
    $entry | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $record -Encoding utf8
    $entry
}
function Restore([string]$record){
    $entry=Get-Content -LiteralPath $record -Raw | ConvertFrom-Json
    $path=$entry.settings;$text=[IO.File]::ReadAllText($path);$new=$text;$restored=@()
    foreach($name in @($entry.applied.PSObject.Properties.Name | Where-Object { $_ -ne 'vsync' })){
        if($null -eq $entry.applied.$name){continue}
        $now=Read-Field $new $name
        if($now -eq $entry.applied.$name -and $now -ne $entry.original.$name){$new=Set-Field $new $name $entry.original.$name;$restored+=$name}
    }
    if($null -ne $entry.applied.vsync){$now=Read-Flag $new 'vsync'
        if($now -eq $entry.applied.vsync -and $now -ne $entry.original.vsync){$new=Set-Flag $new 'vsync' $entry.original.vsync;$restored+='vsync'}}
    if($new -ne $text){Write-Text $path $new}
    $result=[ordered]@{restored_fields=$restored;restored_at=(Get-Date).ToString('o');sha256_final=(Get-FileHash $path).Hash;
        note='Only fields still holding the VR values were reverted; other in-game changes are kept.'}
    ($entry | Add-Member -NotePropertyName restore -NotePropertyValue $result -PassThru -Force) | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $record -Encoding utf8
    $result
}
switch($Action){
    'Apply' { Apply $Settings $Record $DlssQuality | ConvertTo-Json -Depth 4 }
    'RestoreNow' { Restore $Record | ConvertTo-Json -Depth 4 }
    'Restore' {
        $launch=Get-Content -LiteralPath $LaunchJson -Raw | ConvertFrom-Json
        $exe=(Resolve-Path -LiteralPath $ExpectedExecutable).Path
        $deadline=(Get-Date).AddMinutes($TimeoutMinutes)
        $process=$null
        try{
            $process=[Diagnostics.Process]::GetProcessById([int]$launch.pid);$null=$process.Handle
            if($process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -eq [string]$launch.creation_filetime){
                while(!$process.WaitForExit(1000)){ if((Get-Date) -ge $deadline){throw 'game still running at the restore deadline; settings left as applied'} }
            }
        }catch [ArgumentException]{ } # already exited
        finally{ if($process){$process.Dispose()} }
        # The managed pid is gone (or reused). The game writes its settings on exit, so also wait for any other
        # instance of the same executable before restoring.
        while(Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($exe)) -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }){
            if((Get-Date) -ge $deadline){throw 'game still running at the restore deadline; settings left as applied'}
            Start-Sleep -Seconds 1
        }
        Restore $Record | ConvertTo-Json -Depth 4
    }
    'SelfTest' {
        $dir=Join-Path ([IO.Path]::GetTempPath()) ('evr-vr-settings-'+[guid]::NewGuid().ToString('n'));New-Item -ItemType Directory $dir | Out-Null
        try{
            $sample="{`r`n`t`"graphics`": {`r`n`t`t`"render`": {`r`n`t`t`t`"preset`": `"Custom`",`r`n`t`t`t`"antiAliasing`": `"Fsr3`",`r`n`t`t`t`"fsr3`": `"Native`",`r`n`t`t`t`"dlss`": `"Balanced`",`r`n`t`t`t`"shadows`": `"Low`"`r`n`t`t}`r`n`t}`r`n}`r`n"
            $s=Join-Path $dir 'enshrouded_local.json';$r=Join-Path $dir 'record.json';$ok=$true
            Write-Text $s $sample
            $null=Apply $s $r ''
            $t=[IO.File]::ReadAllText($s);$ok=$ok -and (Read-Field $t 'antiAliasing') -eq 'Dlss' -and (Read-Field $t 'dlss') -eq 'Balanced' -and $t.Replace('"Dlss"','"Fsr3"') -eq $sample
            $null=Restore $r;$ok=$ok -and ([IO.File]::ReadAllText($s) -eq $sample)
            "case byte-identical round trip: $ok"
            Write-Text $s $sample;$null=Apply $s $r 'Dlaa'
            $t=[IO.File]::ReadAllText($s)
            $dlaa=(Read-Field $t 'antiAliasing') -eq 'Dlaa' -and (Read-Field $t 'dlss') -eq 'Balanced'
            $null=Restore $r;$dlaa=$dlaa -and ([IO.File]::ReadAllText($s) -eq $sample)
            "case DLAA separate mode and byte-identical restore: $dlaa";$ok=$ok -and $dlaa
            # The user changes another setting in game during VR: that change survives the restore.
            Write-Text $s $sample;$null=Apply $s $r 'Performance'
            Write-Text $s ((Set-Field ([IO.File]::ReadAllText($s)) 'shadows' 'High'))
            $null=Restore $r;$t=[IO.File]::ReadAllText($s)
            $keep=(Read-Field $t 'antiAliasing') -eq 'Fsr3' -and (Read-Field $t 'dlss') -eq 'Balanced' -and (Read-Field $t 'shadows') -eq 'High'
            "case in-game change kept: $keep";$ok=$ok -and $keep
            # The user deliberately switched the upscaler in game: it is not reverted.
            Write-Text $s $sample;$null=Apply $s $r ''
            Write-Text $s ((Set-Field ([IO.File]::ReadAllText($s)) 'antiAliasing' 'Xess'))
            $null=Restore $r;$user=(Read-Field ([IO.File]::ReadAllText($s)) 'antiAliasing') -eq 'Xess'
            "case user upscaler choice kept: $user";$ok=$ok -and $user
            # Already DLSS: nothing written, nothing restored.
            Write-Text $s ($sample.Replace('"Fsr3"','"Dlss"'));$before=(Get-FileHash $s).Hash;$e=Apply $s $r ''
            $same=-not $e.changed -and (Get-FileHash $s).Hash -eq $before;$res=Restore $r;$same=$same -and $res.restored_fields.Count -eq 0
            "case already DLSS untouched: $same";$ok=$ok -and $same
            "self-test $(if($ok){'PASS'}else{'FAIL'})";if(!$ok){exit 1}
        }finally{
            $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
            $resolvedTest=[IO.Path]::GetFullPath($dir)
            if(!$resolvedTest.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolvedTest) -notmatch '^evr-vr-settings-[a-f0-9]{32}$'){throw 'Unexpected temporary self-test path; preserved.'}
            Remove-Item -LiteralPath $resolvedTest -Recurse -Force
        }
    }
}
