[CmdletBinding()]
param([Parameter(Mandatory)][string]$Launch,[Parameter(Mandatory)][string]$Unused)
$ErrorActionPreference='Stop'
& 'S:/stable/Python31011/python.exe' vrmod/tools/run_render_resource_probe.py --launch $Launch --world-live-xr $Unused --live-frames 3000 --world-active-xr-exit --world-shared-updates-once --world-share-vfx-lights --world-early-xr --world-shared-culling-camera --world-vfx-transparent-observe --world-vfx-transparent-render --world-water-feedback --world-culling-history --world-root-camera first-person --world-camera-modes --world-input-observe --world-input-head-walking --world-endurance --world-equipment-observe --world-head-visibility --world-attachments-observe --world-hands-only --world-motion-hands --world-motion-visual --world-grip-preview vrmod/output/grip-preview-20261004-1530
if($LASTEXITCODE -ne 0){throw 'Active XR exit trial failed; retain raw evidence and normally close game if still open.'}
