[CmdletBinding()]
# Brings an installed Enshrouded VR MOD up to the latest GitHub release (user, 2026-10-19: "an update button in the VR
# setting exe, as EDF6 has"; after EDF6VR's Update-EDF6VR.ps1). Started by "Enshrouded VR setting.exe" (Update now:
# -Yes; on its start: -CheckOnly). Lives in <game>\EnshroudedVR\.
# The release: tag "EnshroudedVR-<x.y.z>", asset "EnshroudedVR-<x.y.z>.zip" (the package's contents at its root:
# dinput8.dll, "Enshrouded VR setting.exe", EnshroudedVR\...), uploaded to GitHub so that GitHub records its SHA-256.
# Nothing is copied before the download and every file in it match: the ZIP its SHA-256, each file its line of the
# package's manifest (EnshroudedVR\PACKAGE_MANIFEST.json). Each file it replaces is saved in EnshroudedVR\backup first.
# The player's settings (EnshroudedVR\EnshroudedVR.ini) and logs are not in the package and stay as they are.
param([switch]$CheckOnly, [switch]$Yes, [switch]$AllowDevelopmentFolder)
$ErrorActionPreference = 'Stop'

$DefaultRepository = 'momotori01/ENSHROUDED-VR-MOD'
$BackupsKept = 3

# The installed package's record: its version and the repository its releases come from.
function Get-InstalledEnshroudedVR {
    param([Parameter(Mandatory)][string]$GameDirectory)
    $result = [pscustomobject]@{ Version = $null; Repository = $DefaultRepository }
    $manifest = Join-Path $GameDirectory 'EnshroudedVR\PACKAGE_MANIFEST.json'
    if (Test-Path -LiteralPath $manifest -PathType Leaf) {
        try {
            $m = Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$m.version -match '^\d+\.\d+\.\d+$') { $result.Version = [string]$m.version }
            if ([string]$m.repository -match '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { $result.Repository = [string]$m.repository }
        } catch {}
    }
    return $result
}

# The latest release and its ZIP, with the SHA-256 GitHub records for it.
function Get-LatestEnshroudedVRRelease {
    param([Parameter(Mandatory)][string]$Repository)
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repository/releases/latest" `
        -Headers @{ 'User-Agent' = 'EnshroudedVR-Updater'; 'Accept' = 'application/vnd.github+json' }
    $tag = [string]$release.tag_name
    if ($tag -notmatch '^EnshroudedVR-(\d+\.\d+\.\d+)$') { throw "Unexpected release name '$tag'." }
    $version = $Matches[1]
    $asset = @($release.assets | Where-Object { $_.name -eq "EnshroudedVR-$version.zip" })[0]
    if (!$asset) { throw "The release $tag has no EnshroudedVR-$version.zip." }
    if ([string]$asset.digest -notmatch '^sha256:([0-9a-fA-F]{64})$') {
        throw 'GitHub gives no SHA-256 for this download, so it cannot be checked. Nothing was installed.'
    }
    [pscustomobject]@{
        Version = $version; Url = [string]$asset.browser_download_url; Size = [long]$asset.size
        Sha256 = $Matches[1].ToUpperInvariant(); Notes = [string]$release.body
    }
}

# Checks the ZIP and every file in it before anything is copied, saves each file it is about to replace, then copies
# the package over the game folder. Returns the number of files changed and where the old ones were saved.
function Install-EnshroudedVRPackage {
    param([Parameter(Mandatory)][string]$GameDirectory, [Parameter(Mandatory)][string]$ZipPath,
          [Parameter(Mandatory)][string]$Sha256, [string]$FromVersion, [switch]$AllowDevelopmentFolder)
    $root = (Resolve-Path -LiteralPath $GameDirectory).Path
    if (!(Test-Path -LiteralPath (Join-Path $root 'enshrouded.exe') -PathType Leaf)) {
        throw 'enshrouded.exe is missing. The mod must be in the game folder, next to enshrouded.exe.'
    }
    if (!$AllowDevelopmentFolder -and (Test-Path -LiteralPath (Join-Path $root 'vrmod'))) {
        throw 'This is the development folder (vrmod). Not updating it from a release.'
    }
    if (Get-Process -Name enshrouded -ErrorAction SilentlyContinue) {
        throw 'Close the game before updating. No files were changed.'
    }
    $actual = (Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash
    if ($actual -ne $Sha256.ToUpperInvariant()) {
        throw "The download is damaged (SHA-256 $actual, expected $Sha256). No files were changed."
    }
    $stage = Join-Path ([IO.Path]::GetTempPath()) ('EnshroudedVR-update-' + [Guid]::NewGuid().ToString('N'))
    $changed = 0
    $backup = $null
    try {
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $stage
        $manifestPath = Join-Path $stage 'EnshroudedVR\PACKAGE_MANIFEST.json'
        if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'The download has no package manifest. No files were changed.' }
        $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $listed = @($manifest.files)
        if (!$listed.Count) { throw 'The package manifest lists no files. No files were changed.' }
        foreach ($entry in $listed) {
            $name = [string]$entry.path
            if (!$name -or $name -match '(^|/)\.\.(/|$)' -or $name -match '^[/\\]' -or $name -match ':') {
                throw "The package lists an unsafe path ($name). No files were changed."
            }
            $file = Join-Path $stage ($name -replace '/', '\')
            if (!(Test-Path -LiteralPath $file -PathType Leaf) -or
                (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne ([string]$entry.sha256).ToUpperInvariant()) {
                throw "$name in the download does not match its manifest. No files were changed."
            }
        }
        $label = if ($FromVersion) { $FromVersion } else { 'unknown' }
        $backup = Join-Path $root ('EnshroudedVR\backup\' + $label + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
        $names = @($listed | ForEach-Object { [string]$_.path }) + @('EnshroudedVR/PACKAGE_MANIFEST.json')
        foreach ($name in $names) {
            $source = Join-Path $stage ($name -replace '/', '\')
            $target = Join-Path $root ($name -replace '/', '\')
            if (Test-Path -LiteralPath $target -PathType Leaf) {
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash) { continue }
                $saved = Join-Path $backup ($name -replace '/', '\')
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saved) | Out-Null
                Copy-Item -LiteralPath $target -Destination $saved -Force
            }
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            try { Copy-Item -LiteralPath $source -Destination $target -Force }
            catch { throw "Could not write $name ($($_.Exception.Message)). The files replaced so far are saved in $backup." }
            $changed++
        }
        # Only the latest few backups are kept.
        $backups = Join-Path $root 'EnshroudedVR\backup'
        if (Test-Path -LiteralPath $backups) {
            @(Get-ChildItem -LiteralPath $backups -Directory | Sort-Object LastWriteTime -Descending) |
                Select-Object -Skip $BackupsKept | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse -Force }
        }
    } finally {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (!(Test-Path -LiteralPath $backup)) { $backup = $null }
    return [pscustomobject]@{ Changed = $changed; Backup = $backup }
}

# Dot-sourcing exposes only the functions (the offline test).
if ($MyInvocation.InvocationName -ne '.') {
    try {
        $gameRoot = Split-Path -Parent $PSScriptRoot
        $installed = Get-InstalledEnshroudedVR -GameDirectory $gameRoot
        Write-Host ('Installed: EnshroudedVR {0}' -f $(if ($installed.Version) { $installed.Version } else { '(unknown)' }))
        $latest = Get-LatestEnshroudedVRRelease -Repository $installed.Repository
        Write-Host ('Latest:    EnshroudedVR {0} ({1:N1} MB)' -f $latest.Version, ($latest.Size / 1MB))
        if ($installed.Version -and [version]$installed.Version -ge [version]$latest.Version) {
            Write-Host 'Already up to date. Nothing was changed.'
            exit 0
        }
        if ($CheckOnly) { exit 0 }
        if (!$Yes) {
            $answer = Read-Host 'Download and install it? Y/N'
            if ($answer -notmatch '^[Yy]') { Write-Host 'Nothing was changed.'; exit 0 }
        }
        $zip = Join-Path ([IO.Path]::GetTempPath()) ('EnshroudedVR-{0}-{1}.zip' -f $latest.Version, [Guid]::NewGuid().ToString('N'))
        try {
            Write-Host 'Downloading...'
            $ProgressPreference = 'SilentlyContinue'
            Invoke-WebRequest -Uri $latest.Url -OutFile $zip -UseBasicParsing
            Write-Host 'Checking and installing...'
            $result = Install-EnshroudedVRPackage -GameDirectory $gameRoot -ZipPath $zip -Sha256 $latest.Sha256 -FromVersion $installed.Version -AllowDevelopmentFolder:$AllowDevelopmentFolder
        } finally {
            Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue
        }
        Write-Host ('Updated to EnshroudedVR {0}: {1} file(s) replaced.' -f $latest.Version, $result.Changed) -ForegroundColor Green
        if ($result.Backup) { Write-Host ('The replaced files are saved in {0}' -f $result.Backup) }
        Write-Host 'Your settings (EnshroudedVR\EnshroudedVR.ini) were kept.'
        if ($latest.Notes) { Write-Host ''; Write-Host $latest.Notes.Trim() }
    } catch {
        Write-Host ('ERROR: {0}' -f $_.Exception.Message) -ForegroundColor Red
        exit 1
    }
}
