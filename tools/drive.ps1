# Drives a running wwe2k14.exe (started with --mnk_mode=true) for automated testing: focuses its window,
# holds keys long enough for the game to see them, and saves a small screenshot (window capture) to
# work\drive\shot.png. Keys are .NET Keys names: Escape = Start, Space = A, ShiftKey = B, Up/Down/Left/Right = D-pad.
#   tools\drive.ps1 -Keys Escape,Space -Shot
#   tools\drive.ps1 -Keys Down*3,Space -Wait 2 -Shot      (Down*3 = three presses)
param([string[]]$Keys = @(), [int]$HoldMs = 120, [double]$Wait = 1.0, [switch]$Shot, [int]$Width = 640)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Drive {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  public struct RECT { public int L, T, R, B; }
}
"@
$p = Get-Process wwe2k14, wwe2k14_tu -EA 0 | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1
if (-not $p) { "game not running"; exit 1 }
$h = $p.MainWindowHandle
[Drive]::ShowWindow($h, 9) | Out-Null
[Drive]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 150
foreach ($k in $Keys) {
    $name, $count = $k -split '\*'
    $n = if ($count) { [int]$count } else { 1 }
    $vk = [byte][System.Windows.Forms.Keys]$name
    for ($i = 0; $i -lt $n; $i++) {
        [Drive]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds $HoldMs
        [Drive]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 350
    }
}
Start-Sleep -Milliseconds ([int]($Wait * 1000))
if ($Shot) {
    $r = New-Object Drive+RECT
    [Drive]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $out = Join-Path (Split-Path $PSScriptRoot) "work\drive"
    New-Item -ItemType Directory -Force $out | Out-Null
    $sh = [int]($hh * $Width / $w)
    $small = New-Object System.Drawing.Bitmap $bmp, $Width, $sh
    $small.Save("$out\shot.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose(); $small.Dispose()
    "shot: $out\shot.png"
}
