# Builds wwe2k14_tu.exe: WWE 2K14 with title update 2 (tu00000002). The vanilla game (wwe2k14.exe) is untouched.
# Needs your game\default.xex and the update installed by the launcher (Title update button, or
# "WWE2K14 Launcher.exe --install-tu <package>"), which extracts it to userdata\title_update.
#   tools\build-tu.ps1 [-UpdateFolder <extracted update>]
param([string]$UpdateFolder = "")
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot "env.ps1")
if (-not $UpdateFolder) { $UpdateFolder = "$Root\userdata\title_update" }
if (-not (Test-Path "$Root\game\default.xex")) { throw "game\default.xex missing: copy it from your own WWE 2K14 disc" }
if (-not (Test-Path "$UpdateFolder\default.xexp")) { throw "No title update in $UpdateFolder`: install it from the launcher first" }

$Sdk = Join-Path $Root "sdk\rexglue"
$rexglue = "$Sdk\out\install\win-amd64\bin\rexglue.exe"
if (-not (Test-Path $rexglue)) { throw "rexglue.exe not found: run tools\setup.ps1 first" }

try {
    # The codegen input: default.xex with default.xexp next to it (the runtime applies the patch on load).
    New-Item -ItemType Directory -Force "$Root\game_tu", "$Root\logs" | Out-Null
    Copy-Item "$Root\game\default.xex" "$Root\game_tu\default.xex" -Force
    Copy-Item "$UpdateFolder\default.xexp" "$Root\game_tu\default.xexp" -Force

    Write-Host "Translating the updated game code (a few minutes)..."
    $log = Join-Path $Root ("logs\codegen-tu-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
    Push-Location $Root
    try { cmd /c "`"$rexglue`" codegen wwe2k14_tu_config.toml > `"$log`" 2>&1" } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "codegen failed, see $log" }
    if (-not (Select-String -Path $log -Pattern "XEX patch applied successfully" -Quiet)) {
        throw "The title update did not apply to your default.xex (see $log). It must be WWE 2K14 title update 2."
    }

    Write-Host "Compiling wwe2k14_tu.exe (this takes a while)..."
    Push-Location $Root
    try {
        Invoke-Checked cmake --preset win-amd64
        Invoke-Checked cmake --build --preset relwithdebinfo --target wwe2k14_tu
    } finally { Pop-Location }
    Write-Host "`nDone: the launcher can now play with the title update." -ForegroundColor Green
} catch {
    Write-Host "`n$_" -ForegroundColor Red
    if ($Host.Name -eq "ConsoleHost") { Read-Host "Press Enter to close" | Out-Null }
    exit 1
}
if ($Host.Name -eq "ConsoleHost" -and [Environment]::UserInteractive -and $env:WWE2K14_NO_PAUSE -ne "1") {
    Read-Host "Press Enter to close" | Out-Null
}
