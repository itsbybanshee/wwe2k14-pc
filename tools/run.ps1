# Launch the recompiled game against your extracted WWE 2K14 disc folder.
# Saves/settings go to userdata\, the log to logs\run-<time>.log.
param(
    [string]$GameDir = "",
    [ValidateSet("debug", "relwithdebinfo", "release")] [string]$Config = "relwithdebinfo",
    [string]$LogLevel = "info"
)
if ([string]::IsNullOrEmpty($GameDir)) {
    $GameDir = if ($env:WWE2K14_GAME) { $env:WWE2K14_GAME } else { Join-Path $env:USERPROFILE "Downloads\xiso\WWE 2K14 (USA, Europe)" }
}
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot
$cfgDir = @{ debug = "Debug"; relwithdebinfo = "RelWithDebInfo"; release = "Release" }[$Config]
$exe = Join-Path $Root "out\build\win-amd64\$cfgDir\wwe2k14.exe"
if (-not (Test-Path $exe)) { throw "$exe not built: run tools\build.ps1 -Config $Config" }
if (-not (Test-Path (Join-Path $GameDir "default.xex"))) { throw "no default.xex in $GameDir" }

$userData = Join-Path $Root "userdata"
New-Item -ItemType Directory -Force $userData, (Join-Path $Root "logs") | Out-Null
$log = Join-Path $Root ("logs\run-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
Write-Host "Starting $exe`n  game: $GameDir`n  log:  $log"
& $exe $GameDir "--user_data_root=$userData" "--log_file=$log" "--log_level=$LogLevel"
