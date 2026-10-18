param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$references = Join-Path $repo 'references'
$manifest = Get-Content (Join-Path $references 'manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest) {
    $destination = Join-Path $references $entry.name
    if (!(Test-Path -LiteralPath $destination)) {
        & git init $destination
        if ($LASTEXITCODE) { throw "git init failed: $($entry.name)" }
        & git -C $destination remote add origin $entry.url
        & git -C $destination fetch --depth 1 origin $entry.commit
        if ($LASTEXITCODE) { throw "Download failed: $($entry.name)" }
        & git -C $destination checkout --detach FETCH_HEAD
        if ($LASTEXITCODE) { throw "Checkout failed: $($entry.name)" }
    }
    $actual = & git -C $destination rev-parse HEAD
    if ($LASTEXITCODE -or $actual -ne $entry.commit) {
        throw "Reference revision differs: $($entry.name). Existing sources were preserved."
    }
}
& $Python -m pip install --target (Join-Path $PSScriptRoot 'python') -r (Join-Path $PSScriptRoot 'requirements.txt')
if ($LASTEXITCODE) { throw 'Python dependency installation failed' }
Write-Output 'Dependencies ready. MSVC x64 and a Windows SDK must also be installed.'
