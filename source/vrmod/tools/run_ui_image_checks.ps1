param(
    [Parameter(Mandatory=$true)][string]$Launch,
    [switch]$RequireCleanup,
    [switch]$Previews,
    [string]$ActiveContactOwnerProbe,
    [string]$PythonExecutable
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$PythonExecutable){$PythonExecutable=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'}
if(!(Test-Path -LiteralPath $PythonExecutable)){throw 'Supply -PythonExecutable for isolated Python 3.12 with numpy 2.3.5 and Pillow 12.3.0. See docs/vr_test_environment.md.'}
$Launch=(Resolve-Path -LiteralPath $Launch).Path
$prior=[Environment]::GetEnvironmentVariable('PYTHONPATH','Process')
try{
    # Existing reverse-engineering dependencies use Python 3.10 extensions.
    # Do not put them on this Python 3.12 interpreter's search path.
    Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
    $versions=& $PythonExecutable -c 'import sys,json,numpy,PIL; print(json.dumps(dict(python=sys.version,numpy=numpy.__version__,Pillow=PIL.__version__)))'
    if($LASTEXITCODE){throw 'Image-check Python dependencies unavailable'}
    $versions | Set-Content -LiteralPath (Join-Path $Launch 'ui-image-check-dependencies.json')
    $arguments=@((Join-Path $PSScriptRoot 'check_ui_xr.py'),$Launch)
    if($RequireCleanup){$arguments+='--require-cleanup'}
    if($Previews){$arguments+='--previews'}
    if($ActiveContactOwnerProbe){$arguments+=@('--active-contact-owner-probe',$ActiveContactOwnerProbe)}
    & $PythonExecutable @arguments
    if($LASTEXITCODE){throw 'UI XR evidence checks failed; report retained in native-xr/ui-xr-checks.json'}
}finally{
    if($null -eq $prior){Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue}else{[Environment]::SetEnvironmentVariable('PYTHONPATH',$prior,'Process')}
}
