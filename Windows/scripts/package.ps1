param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [string]$OutputDirectory = "$PSScriptRoot\..\dist",
    [switch]$Standalone
)
$ErrorActionPreference = 'Stop'
$drumBundle = Join-Path $BuildDirectory 'DJLDrum_artefacts\Release\VST3\DJL Drum.vst3'
$drumApp = Join-Path $BuildDirectory 'DJLDrum_artefacts\Release\Standalone\DJL Drum.exe'
$drumFiles = @($drumBundle)
if ($Standalone) { $drumFiles += $drumApp }
foreach ($drumFile in $drumFiles) {
    if (-not (Test-Path -LiteralPath $drumFile)) { throw "Missing build output: $drumFile" }
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
# Copy bundle contents explicitly so repeated packaging does not nest the bundle.
$drumDestination = Join-Path $OutputDirectory 'DJL Drum.vst3'
New-Item -ItemType Directory -Force -Path $drumDestination | Out-Null
Copy-Item -Path (Join-Path $drumBundle '*') -Destination $drumDestination -Recurse -Force
if ($Standalone) { Copy-Item -LiteralPath $drumApp -Destination $OutputDirectory -Force }
# The tracked handover remains in Common; include a distribution copy there too.
$drumHandoverDestination = Join-Path $OutputDirectory 'Common\handover'
New-Item -ItemType Directory -Force -Path $drumHandoverDestination | Out-Null
Copy-Item -Path "$PSScriptRoot\..\..\Common\handover\*" -Destination $drumHandoverDestination -Recurse -Force
Copy-Item -LiteralPath "$PSScriptRoot\..\..\AGENTS.md" -Destination $OutputDirectory -Force
@'
# DJL Drum for Windows

Build instructions and platform handover: [Common/handover/WINDOWS.md](Common/handover/WINDOWS.md).
Live status and source migration register: [Common/handover/LIVE.md](Common/handover/LIVE.md).
This is a development package; copy the entire DJL Drum.vst3 bundle for DAW testing.
'@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'README.md') -Encoding UTF8
$drumCommonReadme = Get-Content -LiteralPath "$PSScriptRoot\..\..\Common\README.md" -Raw
$drumCommonReadme.Replace('(handover/', '(Common/handover/').Replace('(../AGENTS.md)', '(AGENTS.md)') | Set-Content -LiteralPath (Join-Path $OutputDirectory 'README-common.md') -Encoding UTF8
$drumDocsDestination = Join-Path $OutputDirectory 'docs'
New-Item -ItemType Directory -Force -Path $drumDocsDestination | Out-Null
Copy-Item -Path "$PSScriptRoot\..\..\Common\docs\*" -Destination $drumDocsDestination -Recurse -Force
Write-Output "Packaged development build: $OutputDirectory"

$drumCompanionDestination = Join-Path $OutputDirectory 'companion'
New-Item -ItemType Directory -Force -Path $drumCompanionDestination | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot\..\..\Common\companion\DJL_Drum_Link" -Destination $drumCompanionDestination -Recurse -Force

Copy-Item -LiteralPath "$PSScriptRoot\..\..\Common\companion\DJL Drum Link.adg" -Destination $drumCompanionDestination -Force

$drumUpdate = "$PSScriptRoot\..\..\Common\output\pdf\DJL-Drum-Manual-Update-0.17.6.pdf"
if (Test-Path -LiteralPath $drumUpdate) { Copy-Item -LiteralPath $drumUpdate -Destination $OutputDirectory -Force }
