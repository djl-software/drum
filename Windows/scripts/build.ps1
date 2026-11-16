[CmdletBinding()]
param(
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$JuceSource = "",
    [string]$Generator = 'Visual Studio 17 2022',
    [ValidateRange(1,64)][int]$Parallel = 4,
    [switch]$Standalone
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'Run this script in Windows PowerShell, including inside a Windows VM.' }
$drumCmake = $null
$drumCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($drumCommand) { $drumCmake = $drumCommand.Source }
if (-not $drumCmake) {
    $drumVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $drumVswhere) {
        $drumInstallations = @(& $drumVswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
        foreach ($drumInstallation in $drumInstallations) {
            $drumCandidate = Join-Path $drumInstallation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $drumCandidate) { $drumCmake = $drumCandidate; break }
        }
    }
}
if (-not $drumCmake) { throw 'Install CMake 3.22+ and Visual Studio 2022 Desktop development with C++ (or Build Tools), including a Windows SDK. Put CMake on PATH or install the C++ CMake tools component.' }
$drumCtest = Join-Path (Split-Path $drumCmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $drumCtest)) { throw "CTest was not found next to CMake: $drumCtest" }
if ($JuceSource) {
    $JuceSource = [IO.Path]::GetFullPath($JuceSource)
    if (-not (Test-Path -LiteralPath (Join-Path $JuceSource 'CMakeLists.txt'))) { throw "JUCE_SOURCE_DIR must point to the JUCE source checkout: $JuceSource" }
} elseif (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Install Git for Windows so CMake can fetch pinned JUCE 8.0.12, or pass -JuceSource with an existing checkout.'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$drumArguments = @('-S', "$PSScriptRoot\..\..\Common", '-B', $BuildDirectory, '-G', $Generator, '-A', 'x64', '-DBUILD_TESTING=ON')
$drumStandalone = if ($Standalone) { 'ON' } else { 'OFF' }
$drumArguments += "-DDRUM_BUILD_STANDALONE=$drumStandalone"
if ($JuceSource) { $drumArguments += "-DJUCE_SOURCE_DIR=$JuceSource" }
& $drumCmake @drumArguments
if ($LASTEXITCODE -ne 0) { throw 'Configure failed. Check the Visual Studio generator, installed x64 C++ tools and Windows SDK.' }
& $drumCmake --build $BuildDirectory --config Release --parallel $Parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& $drumCtest --test-dir $BuildDirectory -C Release --output-on-failure --timeout 240
if ($LASTEXITCODE -ne 0) { throw 'Tests failed; the build is not validated.' }
Write-Output "Verified x64 VST3: $BuildDirectory\DJLDrum_artefacts\Release\VST3\DJL Drum.vst3"
