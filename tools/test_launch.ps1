# Starts the title update build in a 1600x900 window for testing, with a log in logs\test-<time>.log.
# Extra arguments are passed to the game (e.g. --some_flag=value).
$Root = Split-Path $PSScriptRoot
$exe = "$Root\out\build\win-amd64\RelWithDebInfo\wwe2k14_tu.exe"
$game = if ($env:GAME) { $env:GAME } elseif (Test-Path "$Root\userdata\launcher.json") {
    $j = Get-Content "$Root\userdata\launcher.json" -Raw | ConvertFrom-Json
    if ($j.GameFolder -and (Test-Path $j.GameFolder)) { $j.GameFolder } else { "$env:USERPROFILE\Downloads\ISO\WWE 2K14 (USA, Europe).iso" }
} else { "$env:USERPROFILE\Downloads\ISO\WWE 2K14 (USA, Europe).iso" }
$env:WWE2K14_WINDOW_MODE = "windowed"
$log = "$Root\logs\test-$(Get-Date -Format yyyyMMdd-HHmmss).log"
$argsList = @("`"$game`"", "--user_data_root=$Root\userdata", "--update_data_root=$Root\userdata\title_update",
  "--xmp_music_dir=$Root\userdata\music", "--no-enable_console", "--fullscreen=false", "--window_width=1600",
  "--window_height=900", "--log_file=$log") + $args
Start-Process -FilePath $exe -ArgumentList $argsList -WorkingDirectory $Root
$log
