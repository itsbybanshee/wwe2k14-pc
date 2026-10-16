# Saves an image of the game window (even when other windows cover it) for automated tests.
#   tools\capture_window.ps1 [-Out logs\shot.png] [-Width 960]
param([string]$Out = "logs\shot.png", [int]$Width = 960)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT r);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$p = Get-Process wwe2k14_tu, wwe2k14 -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw "game window not found" }
$r = New-Object Win+RECT
[Win]::GetClientRect($p.MainWindowHandle, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[Win]::PrintWindow($p.MainWindowHandle, $hdc, 3) | Out-Null   # PW_CLIENTONLY | PW_RENDERFULLCONTENT
$g.ReleaseHdc($hdc)
$sh = [int]($h * $Width / $w)
$small = New-Object System.Drawing.Bitmap $bmp, $Width, $sh
$path = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path (Split-Path $PSScriptRoot) $Out }
$small.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
"$path ${w}x${h}"
