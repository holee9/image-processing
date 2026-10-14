<#
.SYNOPSIS
  Stages the native module DLLs of a green main CI run into build\e2e-native-dlls, the way the
  gui-e2e-native job does, and writes provenance.json so Start-ImageProcTestNative.ps1 can say MATCH.
  (Milestone 3 U1, leader.)

.DESCRIPTION
  Same steps and order as .github/workflows/ci.yml job gui-e2e-native:
    1. downloads xpe-ci-common-binaries, xpe-ci-preprocess-binaries, xpe-ci-post-binaries (merged);
    2. adds xpe-ci-dicom-binaries without replacing a name already staged;
    3. copies the virtual-grid table modules/gsvg/data/vg_table_water_csi600_victre.csv beside the DLLs;
    4. writes provenance.json {source, runId, headSha, files[name, md5, length]} for xpe_*.dll.
  Requires the GitHub CLI (gh) logged in. The run must be a completed, successful push run on main.

.PARAMETER RunId   CI run to stage from. Default: the newest successful "XPE CI Pipeline" push run on main.
.PARAMETER Repo    Default holee9/image-processing.
.PARAMETER Dest    Default <repo>\build\e2e-native-dlls. Its old contents are removed first.
#>
[CmdletBinding()]
param(
    [string]$RunId,
    [string]$Repo = 'holee9/image-processing',
    [string]$Dest
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Dest) { $Dest = Join-Path $root 'build\e2e-native-dlls' }
# Prefer the installed GitHub CLI: a 'gh' shim earlier on PATH may not pass output through.
$gh = 'C:\Program Files\GitHub CLI\gh.exe'
if (-not (Test-Path -LiteralPath $gh)) { $gh = (Get-Command gh -ErrorAction SilentlyContinue).Source }
if (-not (Test-Path -LiteralPath $gh)) { throw 'GitHub CLI (gh) not found' }

if (-not $RunId) {
    # Filter by workflow file: a --jq expression with inner double quotes is mangled by Windows PowerShell 5 argument passing.
    $RunId = & $gh run list -R $Repo --workflow ci.yml --branch main --event push --status success --limit 1 --json databaseId --jq '.[0].databaseId'
    if (-not $RunId) { throw 'no successful XPE CI Pipeline push run on main' }
}
$run = & $gh run view $RunId -R $Repo --json headSha,headBranch,event,conclusion | ConvertFrom-Json
if ($run.conclusion -ne 'success' -or $run.headBranch -ne 'main' -or $run.event -ne 'push') {
    throw ("run {0} is {1}/{2}/{3}; need a successful push run on main" -f $RunId, $run.event, $run.headBranch, $run.conclusion)
}

$work = Join-Path ([IO.Path]::GetTempPath()) ("xpe-ci-dlls-" + $RunId)
if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null
foreach ($n in 'xpe-ci-common-binaries', 'xpe-ci-preprocess-binaries', 'xpe-ci-post-binaries', 'xpe-ci-dicom-binaries') {
    & $gh run download $RunId -R $Repo -n $n -D (Join-Path $work $n)
    if ($LASTEXITCODE -ne 0) { throw "download failed: $n" }
}

if (Test-Path -LiteralPath $Dest) { Remove-Item -LiteralPath $Dest -Recurse -Force }
New-Item -ItemType Directory -Path $Dest | Out-Null
# 1. merged, later artifacts overwrite (download-artifact merge-multiple)
foreach ($n in 'xpe-ci-common-binaries', 'xpe-ci-preprocess-binaries', 'xpe-ci-post-binaries') {
    Get-ChildItem -LiteralPath (Join-Path $work $n) -Recurse -File | ForEach-Object { Copy-Item $_.FullName (Join-Path $Dest $_.Name) -Force }
}
# 2. dicom, never replace
Get-ChildItem -LiteralPath (Join-Path $work 'xpe-ci-dicom-binaries') -Recurse -Filter *.dll | Sort-Object FullName | ForEach-Object {
    $target = Join-Path $Dest $_.Name
    if (-not (Test-Path -LiteralPath $target)) { Copy-Item $_.FullName $target }
}
# 3. virtual-grid table
$table = Join-Path $root 'modules\gsvg\data\vg_table_water_csi600_victre.csv'
if (-not (Test-Path -LiteralPath $table)) { throw "missing product table: $table" }
Copy-Item $table (Join-Path $Dest (Split-Path $table -Leaf)) -Force
# 4. provenance
$files = Get-ChildItem -LiteralPath $Dest -Filter xpe_*.dll | ForEach-Object {
    [pscustomobject]@{ name = $_.Name; md5 = (Get-FileHash $_.FullName -Algorithm MD5).Hash.ToLower(); length = $_.Length }
}
[pscustomobject]@{ source = 'ci'; runId = "$RunId"; headSha = $run.headSha; files = $files } |
    ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Dest 'provenance.json') -Encoding utf8
Remove-Item -LiteralPath $work -Recurse -Force

foreach ($n in 'xpe_common.dll', 'xpe_preprocess.dll', 'xpe_display.dll', 'gsvg.dll', 'xpe_ai.dll', 'xpe_enhance_basic.dll', 'xpe_dicom.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $Dest $n))) { throw "missing $n in $Dest" }
}
Write-Host ("staged run {0} (main {1}) into {2}" -f $RunId, $run.headSha.Substring(0, 8), $Dest)
