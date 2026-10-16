# Build the game executable from generated\ (run tools\codegen.ps1 first).
param([ValidateSet("debug", "relwithdebinfo", "release")] [string]$Config = "relwithdebinfo")
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot "env.ps1")
if (-not (Test-Path "$Root\generated\sources.cmake")) { throw "generated\ is empty: run tools\codegen.ps1 first" }
Push-Location $Root
try {
    Invoke-Checked cmake --preset win-amd64
    Invoke-Checked cmake --build --preset $Config
} finally { Pop-Location }
