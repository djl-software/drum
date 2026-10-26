param(
    [string]$BuildDirectory = "$PSScriptRoot\..\build",
    [string]$JuceSource = "",
    [switch]$Standalone
)
$ErrorActionPreference = 'Stop'
$drumCmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $drumCmake) { $drumCmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
if (-not (Test-Path -LiteralPath $drumCmake)) { throw 'Install CMake and Visual Studio C++ build tools.' }
$drumArguments = @('-S', "$PSScriptRoot\..", '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64')
$drumStandalone = if ($Standalone) { 'ON' } else { 'OFF' }
$drumArguments += "-DDRUM_BUILD_STANDALONE=$drumStandalone"
if ($JuceSource) { $drumArguments += "-DJUCE_SOURCE_DIR=$JuceSource" }
& $drumCmake @drumArguments
if ($LASTEXITCODE) { throw 'Configure failed' }
& $drumCmake --build $BuildDirectory --config Release --parallel 4
if ($LASTEXITCODE) { throw 'Build failed' }
& (Join-Path (Split-Path $drumCmake) 'ctest.exe') --test-dir $BuildDirectory -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }
