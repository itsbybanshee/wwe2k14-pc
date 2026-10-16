# Fetch, patch, build and install the ReXGlue SDK into sdk\rexglue (native Windows, clang + MSVC ABI).
#
# The SDK source is cloned at a fixed base commit, then patches\sdk\*.patch (this project's SDK changes) are
# applied on top. See THIRD_PARTY_NOTICES.md for the SDK's licences.
param([int]$Jobs = 0)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot
$Sdk = Join-Path $Root "sdk\rexglue"
$BaseCommit = "987593c0fc6612850a2e1b247b2b03aa5281e663"
. (Join-Path $PSScriptRoot "env.ps1")

# Without Windows developer mode git checks symlinks out as small text files holding the target
# path (e.g. libmspack's cabextract/mspack/lzxd.c). Replace each one with a copy of its target.
function Repair-GitSymlinks([string]$Repo) {
    $repos = @($Repo) + @(git -C $Repo submodule foreach --recursive --quiet 'echo $displaypath' |
        Where-Object { $_ -notlike "*moltenvk*" } | ForEach-Object { Join-Path $Repo $_ })
    foreach ($pass in 1..3) {  # a link can point at another link
        foreach ($r in $repos) {
            foreach ($entry in (git -C $r ls-files -s | Where-Object { $_ -like "120000*" })) {
                $rel = ($entry -split "`t", 2)[1]
                $link = Join-Path $r $rel
                $dest = ((git -C $r cat-file -p ($entry -split " ")[1]) -join "`n").Trim()
                if (-not (Test-Path $link -PathType Leaf) -or (Get-Content $link -Raw).Trim() -ne $dest) { continue }
                $target = Join-Path (Split-Path $link) $dest
                if (-not (Test-Path $target)) {
                    if ($pass -eq 3) { Write-Warning "symlink target missing: $r/$rel" }
                    continue
                }
                Remove-Item $link -Force
                Copy-Item $target $link -Recurse -Force
                git -C $r update-index --assume-unchanged $rel
            }
        }
    }
}

if (-not (Test-Path "$Sdk\.git")) {
    # autocrlf=false everywhere: the patches carry exact line endings.
    Invoke-Checked git -c core.autocrlf=false clone --no-checkout https://github.com/HollywoodAkeem/rexglue-sdk-yukes.git $Sdk
    Invoke-Checked git -C $Sdk config core.autocrlf false
    Invoke-Checked git -C $Sdk checkout -q -b wwe2k14 $BaseCommit
    foreach ($patch in (Get-ChildItem "$Root\patches\sdk\*.patch" | Sort-Object Name)) {
        Invoke-Checked git -C $Sdk apply --whitespace=nowarn $patch.FullName
    }
    Invoke-Checked git -C $Sdk add -A
    Invoke-Checked git -C $Sdk -c user.name="WWE2K14 PC" -c user.email="wwe2k14@local" commit -q -m "WWE 2K14 PC: SDK changes"
    Invoke-Checked git -C $Sdk -c core.autocrlf=false submodule update --init --recursive
    Invoke-Checked git -C $Sdk remote set-url --push origin DISABLED-no-push  # local only
}
Repair-GitSymlinks $Sdk

$Build = Join-Path $Sdk "out\build\win-amd64"
if (-not (Test-Path "$Build\CMakeCache.txt")) {
    Invoke-Checked cmake -S $Sdk -B $Build -G "Ninja Multi-Config" `
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ `
        "-DCMAKE_C_FLAGS=-march=x86-64-v3" "-DCMAKE_CXX_FLAGS=-march=x86-64-v3" `
        -DCMAKE_CXX_STANDARD=23 "-DCMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo" `
        "-DCMAKE_INSTALL_PREFIX=$Sdk\out\install\win-amd64" `
        -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_FIDELITYFX=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF `
        -DREXGLUE_ENABLE_SANITIZERS=OFF -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_FFMPEG_SHARED=ON `
        -DREXGLUE_PROFILE_GUEST_FUNCTIONS=OFF -DREXGLUE_USE_VULKAN=ON
}
$jobArgs = if ($Jobs -gt 0) { @("-j", $Jobs) } else { @() }
Invoke-Checked cmake --build $Build --config RelWithDebInfo --target install @jobArgs
Write-Host "ReXGlue SDK built and installed to $Sdk\out\install\win-amd64"
