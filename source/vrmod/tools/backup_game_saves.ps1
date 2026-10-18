param([Parameter(Mandatory=$true)][string]$Source,[switch]$PassThru)
$ErrorActionPreference='Stop'
if (Get-Process enshrouded -ErrorAction SilentlyContinue) { throw 'Close the game before taking a consistent save backup.' }
$sourcePath=(Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
if ((Get-Item -LiteralPath $sourcePath).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Save root must not be a reparse point.' }
$mod=Split-Path $PSScriptRoot -Parent
$backupPath=Join-Path $mod ('output/save-backups/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $backupPath | Out-Null
$files=@(Get-ChildItem -LiteralPath $sourcePath -File -Recurse)
if (!$files.Count) { throw 'No save files found.' }
$manifest=@()
foreach ($file in $files) {
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Save file must not be a reparse point.' }
    if (!$file.FullName.StartsWith($sourcePath+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Save file escaped source root.' }
    $relative=$file.FullName.Substring($sourcePath.Length+1)
    $destination=Join-Path (Join-Path $backupPath 'remote') $relative
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    $before=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $file.FullName -Destination $destination
    $copied=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    $after=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ($before -ne $copied -or $before -ne $after) { throw 'Save changed during backup; snapshot is incomplete.' }
    $manifest+=@{relative=$relative;bytes=$file.Length;sha256=$copied}
}
$finalFiles=@(Get-ChildItem -LiteralPath $sourcePath -File -Recurse)
if($finalFiles.Count -ne $manifest.Count) { throw 'Save file set changed during backup.' }
foreach($entry in $manifest) {
    if((Get-FileHash -LiteralPath (Join-Path $sourcePath $entry.relative) -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw 'Save changed before snapshot completion.'
    }
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backupPath 'manifest.json')
@{source=$sourcePath;completed=(Get-Date).ToString('o');files=$manifest.Count;verified=$true} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backupPath 'backup.json')
if($PassThru) { [pscustomobject]@{path=$backupPath;source=$sourcePath;files=$manifest.Count;verified=$true} }
else { Write-Output "PASS verified $($manifest.Count) save files: $backupPath" }
