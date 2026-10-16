# Records the game window for review: logs\watch\frame-NNN.png plus logs\watch\cursor.txt (frame, time, cursor
# position in 1280x720 UI coordinates, mouse buttons held).
#   tools\watch_window.ps1 [-Seconds 90] [-Interval 0.5] [-Width 640]
param([int]$Seconds = 90, [double]$Interval = 0.5, [int]$Width = 640)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int k);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
"@
$root = Split-Path $PSScriptRoot
$dir = Join-Path $root "logs\watch"
Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $dir | Out-Null
$p = Get-Process wwe2k14_tu, wwe2k14 -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw "game window not found" }
$h = $p.MainWindowHandle
$n = 0
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
  $r = New-Object W+RECT; [W]::GetClientRect($h, [ref]$r) | Out-Null
  $w = $r.R - $r.L; $hh = $r.B - $r.T
  if ($w -gt 0 -and $hh -gt 0) {
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [W]::PrintWindow($h, $dc, 3) | Out-Null; $g.ReleaseHdc($dc)
    $pt = New-Object W+POINT; [W]::GetCursorPos([ref]$pt) | Out-Null; [W]::ScreenToClient($h, [ref]$pt) | Out-Null
    $s = [Math]::Min($w / 1280.0, $hh / 720.0)
    $ux = ($pt.X - ($w - 1280 * $s) / 2) / $s; $uy = ($pt.Y - ($hh - 720 * $s) / 2) / $s
    # Draw the cursor position on the frame.
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Lime), 3
    $g.DrawEllipse($pen, $pt.X - 8, $pt.Y - 8, 16, 16)
    $btn = ""
    if ([W]::GetAsyncKeyState(1) -band 0x8000) { $btn += "L" }
    if ([W]::GetAsyncKeyState(2) -band 0x8000) { $btn += "R" }
    if ([W]::GetAsyncKeyState(4) -band 0x8000) { $btn += "M" }
    $small = New-Object System.Drawing.Bitmap $bmp, $Width, ([int]($hh * $Width / $w))
    $name = "frame-{0:D3}.png" -f $n
    $small.Save((Join-Path $dir $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose(); $small.Dispose()
    Add-Content (Join-Path $dir "cursor.txt") ("{0:D3} {1:HH:mm:ss.f} ui=({2:F0},{3:F0}) buttons={4}" -f $n, (Get-Date), $ux, $uy, $btn)
    $n++
  }
  Start-Sleep -Milliseconds ([int]($Interval * 1000))
}
"$n frames in $dir"
