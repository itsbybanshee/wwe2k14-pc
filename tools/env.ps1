# Dot-source this (". $PSScriptRoot\env.ps1") to get MSVC/Windows SDK + LLVM 20+ in the current session.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not $env:VSINSTALLDIR -and (Test-Path $vswhere)) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
        $env:PATH = "$(Split-Path $vswhere);$env:PATH"
        Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
        Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
    }
}

# Visual Studio's bundled clang is too old for ReXGlue (needs 20+): put the standalone LLVM first.
$llvm = "$env:ProgramFiles\LLVM\bin"
if (Test-Path "$llvm\clang.exe") { $env:PATH = "$llvm;$env:PATH" }

$clangVersion = (& clang --version | Select-Object -First 1)
if ($clangVersion -notmatch "clang version (\d+)" -or [int]$Matches[1] -lt 20) {
    throw "Clang 20+ required (found: $clangVersion). Install LLVM: winget install LLVM.LLVM"
}

# PowerShell 5.1 ignores native exit codes even with ErrorActionPreference=Stop.
function Invoke-Checked {
    $exe, $rest = $args
    & $exe @rest
    if ($LASTEXITCODE -ne 0) { throw "failed ($LASTEXITCODE): $($args -join ' ')" }
}
