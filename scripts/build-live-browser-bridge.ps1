param([Parameter(Mandatory=$true)][string]$Output,[switch]$Test)
$ErrorActionPreference='Stop'
$framework=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$assemblies=Join-Path $env:WINDIR 'Microsoft.NET\assembly\GAC_MSIL'
$references=@('System.Windows.Forms.dll','System.Drawing.dll','System.Web.Extensions.dll')
foreach($name in @('UIAutomationClient','UIAutomationTypes','WindowsBase')) {
    $references += (Get-ChildItem -LiteralPath (Join-Path $assemblies $name) -Filter "$name.dll" -Recurse | Select-Object -First 1).FullName
}
New-Item -ItemType Directory -Force -Path (Split-Path $Output) | Out-Null
$compilerArgs=@('/nologo','/target:winexe','/platform:x64',"/out:$Output")
if($Test) {$compilerArgs[1]='/target:exe';$compilerArgs += '/main:BridgeTests'}
$compilerArgs += $references | ForEach-Object {"/r:$_"}
$compilerArgs += "$PSScriptRoot\..\tools\LiveBrowserBridge.cs"
if($Test) {$compilerArgs += @("$PSScriptRoot\..\tests\LiveBrowserBridgeTests.cs")}
& (Join-Path $framework 'csc.exe') @compilerArgs
if($LASTEXITCODE -ne 0) {throw 'Browser bridge compilation failed'}
