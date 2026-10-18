param([Parameter(Mandatory)][string]$Backup,[switch]$DryRun)
# Explicitly invoked only: preserve current full save, then restore the complete character ring.
# Does not delete files, alter the world ring, or change Steam/VR/OS settings.
$ErrorActionPreference='Stop'
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'Game must be closed before restoring a character snapshot.'}
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path.TrimEnd('\')
$backupRoot=(Resolve-Path -LiteralPath (Join-Path $taskRoot 'vrmod/output/save-backups')).Path.TrimEnd('\')
$reference=(Resolve-Path -LiteralPath $Backup).Path.TrimEnd('\')
if(-not $reference.StartsWith($backupRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Reference must be an existing managed save backup.'}
$metadata=Get-Content -LiteralPath (Join-Path $reference 'backup.json') -Raw | ConvertFrom-Json
$destination=(Resolve-Path -LiteralPath 'C:/Program Files (x86)/Steam/userdata/<steam-user-id>/1203620/remote').Path.TrimEnd('\')
if(!$metadata.verified -or $metadata.source.TrimEnd('\') -ne $destination){throw 'Reference is not a verified snapshot of this save root.'}
foreach($p in @($reference,$destination,(Join-Path $reference 'remote'))){if((Get-Item -LiteralPath $p).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'No reparse-point save roots.'}}
$manifest=@(Get-Content -LiteralPath (Join-Path $reference 'manifest.json') -Raw | ConvertFrom-Json)
$selected=@($manifest | Where-Object { $_.relative -match '^characters(?:-[1-9]|-index)?$' } | Sort-Object @{Expression={$_.relative -eq 'characters-index'}},relative)
if($selected.Count -ne 11 -or @($selected | Select-Object -ExpandProperty relative -Unique).Count -ne 11){throw 'Expected a complete 11-file character ring.'}
foreach($e in $selected){
 $src=Join-Path (Join-Path $reference 'remote') $e.relative;$dst=Join-Path $destination $e.relative
 if((Get-Item -LiteralPath $src).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'No reparse-point source files.'}
 if((Get-Item -LiteralPath $dst).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'No reparse-point destination files.'}
 if((Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash -ne $e.sha256){throw 'Reference character hash mismatch.'}
}
if($DryRun){[pscustomobject]@{reference=$reference;destination=$destination;files=@($selected.relative);world_files_changed=0;current_snapshot_required=$true} | ConvertTo-Json -Depth 5;exit}
$preserved=& (Join-Path $PSScriptRoot 'backup_game_saves.ps1') -Source $destination -PassThru
if(!$preserved.verified){throw 'Current save preservation did not verify.'}
$current=@(Get-Content -LiteralPath (Join-Path $preserved.path 'manifest.json') -Raw | ConvertFrom-Json)
$recordRoot=Join-Path $taskRoot ('vrmod/output/save-restores/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $recordRoot | Out-Null
$record=[ordered]@{authorization='User 2026-10-09 requested recovery of a burned-out torch using a save backup';reference=$reference;destination=$destination;preserved_current=$preserved.path;character_files=@($selected.relative);world_files_changed=0;started=(Get-Date -Format o);status='prepared';copied=@()}
try{
 if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'Game started during preparation.'}
 foreach($e in $current){if((Get-FileHash -LiteralPath (Join-Path $destination $e.relative) -Algorithm SHA256).Hash -ne $e.sha256){throw 'Current save changed after preservation.'}}
 $record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $recordRoot 'restore.json') -Encoding utf8
 foreach($e in $selected){
  $src=Join-Path (Join-Path $reference 'remote') $e.relative;$dst=Join-Path $destination $e.relative
  Copy-Item -LiteralPath $src -Destination $dst -Force
  if((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $e.sha256){throw 'Restored character hash mismatch.'}
  $record.copied+=@{relative=$e.relative;sha256=$e.sha256}
 }
 foreach($e in $current){if($e.relative -notin $selected.relative -and (Get-FileHash -LiteralPath (Join-Path $destination $e.relative) -Algorithm SHA256).Hash -ne $e.sha256){throw 'Unselected save file changed during restoration.'}}
 $record.status='verified';$record.completed=(Get-Date -Format o)
} catch {$record.status='failed';$record.error=$_.Exception.Message;throw}
finally{$record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $recordRoot 'restore.json') -Encoding utf8;Write-Output "restore record: $recordRoot/restore.json"}
Write-Output "PASS 11 character files restored; full current save preserved at $($preserved.path); world files unchanged."
