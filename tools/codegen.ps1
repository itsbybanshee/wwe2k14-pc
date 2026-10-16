# Translate game\default.xex (PPC) to C++ in generated\. Writes a timestamped log in logs\ and prints
# a summary of unresolved calls / branches, the input for tools\find_functions.py.
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot
$Sdk = Join-Path $Root "sdk\rexglue"
. (Join-Path $PSScriptRoot "env.ps1")

$rexglue = @("$Sdk\out\install\win-amd64\bin\rexglue.exe") + @(Get-ChildItem "$Sdk\out" -Recurse -Filter rexglue.exe -ErrorAction SilentlyContinue | ForEach-Object FullName) |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $rexglue) { throw "rexglue.exe not found: run tools\setup.ps1 first" }
if (-not (Test-Path "$Root\game\default.xex")) { throw "game\default.xex missing: copy it from your own WWE 2K14 disc" }

# generated\rexglue.cmake is SDK-managed: render it through a throwaway `rexglue init` project.
if (-not (Test-Path "$Root\generated\rexglue.cmake")) {
    $tmp = Join-Path ([IO.Path]::GetTempPath()) "wwe2k14-init-$PID"
    try {
        Invoke-Checked $rexglue init --app_name wwe2k14 --app_root "$tmp\wwe2k14"
        New-Item -ItemType Directory -Force "$Root\generated" | Out-Null
        Copy-Item "$tmp\wwe2k14\generated\rexglue.cmake" "$Root\generated\rexglue.cmake"
    } finally { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
}

New-Item -ItemType Directory -Force "$Root\logs" | Out-Null
$log = Join-Path $Root ("logs\codegen-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
Push-Location $Root
try {
    cmd /c "`"$rexglue`" codegen wwe2k14_config.toml > `"$log`" 2>&1"
    $code = $LASTEXITCODE
} finally { Pop-Location }

$unresolvedCalls = @(Select-String -Path $log -Pattern "UnresolvedCall|Unresolved call" )
$unresolvedBranches = @(Select-String -Path $log -Pattern "Unresolved (conditional )?branch")
$errors = @(Select-String -Path $log -Pattern "\[error\]|error:" )
Write-Host "codegen exit $code; log: $log"
Write-Host ("  {0} unresolved calls, {1} unresolved branches, {2} error lines" -f
    $unresolvedCalls.Count, $unresolvedBranches.Count, $errors.Count)
exit $code
