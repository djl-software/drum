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
Copy-Item -LiteralPath "$PSScriptRoot\..\README.md" -Destination $OutputDirectory -Force
$drumDocsDestination = Join-Path $OutputDirectory 'docs'
New-Item -ItemType Directory -Force -Path $drumDocsDestination | Out-Null
Copy-Item -Path "$PSScriptRoot\..\docs\*" -Destination $drumDocsDestination -Recurse -Force
$drumManual = "$PSScriptRoot\..\output\pdf\DJL-Drum-User-Manual-0.11.0-Windows.pdf"
if (Test-Path -LiteralPath $drumManual) { Copy-Item -LiteralPath $drumManual -Destination $OutputDirectory -Force }
Write-Output "Packaged development build: $OutputDirectory"
