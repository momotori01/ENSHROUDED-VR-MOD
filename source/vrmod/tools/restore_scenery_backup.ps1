param([Parameter(Mandatory)][string]$Backup,[switch]$DryRun,[switch]$KeepExtraFiles)
# User-authorized scenery comparison: preserve the full current save, then restore one complete reference.
# Copies only known flat save files; no deletion and no Steam/VR/OS settings changes.
$ErrorActionPreference='Stop'
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'Game must be closed before restoring a scenery reference.'}
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path.TrimEnd('\')
$backupRoot=(Resolve-Path -LiteralPath (Join-Path $taskRoot 'vrmod/output/save-backups')).Path.TrimEnd('\')
$reference=(Resolve-Path -LiteralPath $Backup).Path.TrimEnd('\')
if(-not $reference.StartsWith($backupRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Reference must be an existing managed save backup.'}
$metadata=Get-Content -LiteralPath (Join-Path $reference 'backup.json') -Raw | ConvertFrom-Json
$destination=(Resolve-Path -LiteralPath 'C:/Program Files (x86)/Steam/userdata/<steam-user-id>/1203620/remote').Path.TrimEnd('\')
if(!$metadata.verified -or $metadata.source.TrimEnd('\') -ne $destination){throw 'Reference is not a verified snapshot of this save root.'}
foreach($p in @($reference,$destination,(Join-Path $reference 'remote'))){if((Get-Item -LiteralPath $p).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'No reparse-point save roots.'}}
$manifest=@(Get-Content -LiteralPath (Join-Path $reference 'manifest.json') -Raw | ConvertFrom-Json)
# 2026-10-16 (the 1.0 update): the saves are world<n>.ksd, character<n>.ksd, global.ksd and user_settings.json, and the game keeps
# its own backup\ and migrated\ folders beside them; only the flat files are restored, the folders stay as they are.
$manifest=@($manifest | Where-Object {$_.relative -notmatch '[\\/]'})
$selected=@($manifest | Sort-Object @{Expression={$_.relative -match '-index$'}},relative)
if(!$selected.Count -or $selected.Count -gt $metadata.files -or @($selected.relative | Select-Object -Unique).Count -ne $selected.Count){throw 'Incomplete or duplicate reference manifest.'}
foreach($e in $selected){if($e.relative -notmatch '^[A-Za-z0-9_-]+(\.(ksd|json))?$'){throw 'Only reviewed flat save filenames are allowed.'}}
$existing=@(Get-ChildItem -LiteralPath $destination -File | Select-Object -ExpandProperty Name)
$extraFiles=@($existing | Where-Object {$_ -notin $selected.relative})
$missingFiles=@($selected.relative | Where-Object {$_ -notin $existing})
if($missingFiles.Count -or ($extraFiles.Count -and !$KeepExtraFiles)){throw 'Save file set differs; preserve new files and choose a matching reference.'}
# Explicit subset restore: retain every extra file, verify its hash afterwards.
# Old index files determine active revisions; never remove newly generated revisions.
foreach($e in $selected){
 $src=Join-Path (Join-Path $reference 'remote') $e.relative;$dst=Join-Path $destination $e.relative
 if((Get-Item -LiteralPath $src).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'No reparse-point source files.'}
 if((Get-Item -LiteralPath $dst).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'No reparse-point destination files.'}
 if((Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash -ne $e.sha256){throw 'Reference save hash mismatch.'}
}
if($DryRun){[pscustomobject]@{reference=$reference;destination=$destination;files=@($selected.relative);world_files_restored=@($selected | Where-Object {$_.relative -notmatch '^characters'}).Count;current_snapshot_required=$true;extra_files_retained=$extraFiles} | ConvertTo-Json -Depth 5;exit}
$preserved=& (Join-Path $PSScriptRoot 'backup_game_saves.ps1') -Source $destination -PassThru
if(!$preserved.verified){throw 'Current save preservation did not verify.'}
$current=@(Get-Content -LiteralPath (Join-Path $preserved.path 'manifest.json') -Raw | ConvertFrom-Json)
$recordRoot=Join-Path $taskRoot ('vrmod/output/save-restores/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $recordRoot | Out-Null
$record=[ordered]@{authorization='User 2026-10-09 requested restoring the same save backup before every scenery comparison to align time and world state';reference=$reference;destination=$destination;preserved_current=$preserved.path;save_files=@($selected.relative);world_files_restored=@($selected | Where-Object {$_.relative -notmatch '^characters'}).Count;started=(Get-Date -Format o);status='prepared';extra_files_retained=$extraFiles;subset_restore=[bool]$KeepExtraFiles;copied=@()}
try{
 if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'Game started during preparation.'}
 foreach($e in $current){if((Get-FileHash -LiteralPath (Join-Path $destination $e.relative) -Algorithm SHA256).Hash -ne $e.sha256){throw 'Current save changed after preservation.'}}
 $record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $recordRoot 'restore.json') -Encoding utf8
 foreach($e in $selected){
  $src=Join-Path (Join-Path $reference 'remote') $e.relative;$dst=Join-Path $destination $e.relative
  Copy-Item -LiteralPath $src -Destination $dst -Force
  if((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $e.sha256){throw 'Restored save hash mismatch.'}
  $record.copied+=@{relative=$e.relative;sha256=$e.sha256}
 }
 foreach($e in $current){if($e.relative -notin $selected.relative -and (Get-FileHash -LiteralPath (Join-Path $destination $e.relative) -Algorithm SHA256).Hash -ne $e.sha256){throw 'Unselected save file changed during restoration.'}}
 $record.status='verified';$record.completed=(Get-Date -Format o)
} catch {$record.status='failed';$record.error=$_.Exception.Message;throw}
finally{$record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $recordRoot 'restore.json') -Encoding utf8;Write-Output "restore record: $recordRoot/restore.json"}
Write-Output "PASS $($selected.Count) reference save files restored; full previous save preserved at $($preserved.path)."
