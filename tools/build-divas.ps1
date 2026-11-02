# Builds wwe2k14_divas.exe: WWE 2K14 with the Divas Evolution v1.0.0 mod's code. The vanilla game (wwe2k14.exe) is
# untouched. Needs the mod's folder (the one with its default.xex: the launcher's game folder once the mod is
# installed with the Total Conversion button) and the vanilla game built once (generated\, tools\codegen.ps1).
#   tools\build-divas.ps1 -ModFolder <mod folder>
param([Parameter(Mandatory = $true)][string]$ModFolder)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot "env.ps1")
if (-not (Test-Path "$ModFolder\default.xex")) { throw "No default.xex in $ModFolder" }
if (-not (Test-Path "$Root\generated\sources.cmake")) { throw "Build the vanilla game first (tools\codegen.ps1, tools\build.ps1)" }

$Sdk = Join-Path $Root "sdk\rexglue"
$rexglue = "$Sdk\out\install\win-amd64\bin\rexglue.exe"
if (-not (Test-Path $rexglue)) { throw "rexglue.exe not found: run tools\setup.ps1 first" }

# tools\prepare_divas.py needs Python 3: the py launcher, python on PATH, or the default per-user install.
$python = @(
    (Get-Command py -ErrorAction SilentlyContinue).Source
    (Get-Command python -ErrorAction SilentlyContinue | Where-Object { $_.Source -notlike "*WindowsApps*" }).Source
    (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending).FullName
) | Where-Object { $_ } | Select-Object -First 1
if (-not $python) { throw "Python 3 not found: install it (winget install Python.Python.3.12)" }

try {
    New-Item -ItemType Directory -Force "$Root\logs" | Out-Null
    Write-Host "Preparing the mod's code for translation..."
    Invoke-Checked $python -I "$Root\tools\prepare_divas.py" "$ModFolder\default.xex" "$Root\game_divas\default.xex"

    Write-Host "Translating the mod's game code (a few minutes)..."
    $log = Join-Path $Root ("logs\codegen-divas-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
    Push-Location $Root
    try { cmd /c "`"$rexglue`" codegen wwe2k14_divas_config.toml > `"$log`" 2>&1" } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "codegen failed, see $log" }

    Write-Host "Compiling wwe2k14_divas.exe (this takes a while)..."
    Push-Location $Root
    try {
        Invoke-Checked cmake --preset win-amd64
        Invoke-Checked cmake --build --preset relwithdebinfo --target wwe2k14_divas
    } finally { Pop-Location }
    Write-Host "`nDone: the launcher can now play Divas Evolution." -ForegroundColor Green
} catch {
    Write-Host "`n$_" -ForegroundColor Red
    if ($Host.Name -eq "ConsoleHost") { Read-Host "Press Enter to close" | Out-Null }
    exit 1
}
if ($Host.Name -eq "ConsoleHost" -and [Environment]::UserInteractive -and $env:WWE2K14_NO_PAUSE -ne "1") {
    Read-Host "Press Enter to close" | Out-Null
}
