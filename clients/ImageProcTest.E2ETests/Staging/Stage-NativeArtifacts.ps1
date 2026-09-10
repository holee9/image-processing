<#
.SYNOPSIS
    Stages native DLLs from a CI run and records WHERE THEY CAME FROM.

.DESCRIPTION
    #98 keeps this lane from building native DLLs, so a Native E2E run always exercises binaries
    produced elsewhere. GUI-C-40 measured what that costs: the suite reported "Native 9/9" while the
    staged xpe_preprocess.dll predated the six-argument xpe_calib_generate_offset it was supposed to
    exercise. Nothing was wrong with the run — the word "Native" simply does not say WHICH native.

    So this script writes provenance.json beside the DLLs, naming the CI run, that run's head SHA,
    and each file's md5. NativeProvenanceTests refuses to pass a Native run without it and prints the
    provenance when it is there.

    The file is the SAME shape ci.yml writes (21c1e92) so one reader serves both producers; the only
    difference is "source", which is "lane" here and "ci" there.

    Copying uses -Force deliberately: GUI-C-31 measured a stale DLL surviving a copy that skipped
    existing files, and a silent no-op is the exact failure this whole card is about.

.PARAMETER RunId
    The GitHub Actions run id the artifacts come from.

.PARAMETER Destination
    Directory the DLLs are staged into — the same path XPE_NATIVE_DIR points at.

.PARAMETER Artifacts
    Artifact names to download. Defaults to the three the GUI suite consumes.

.EXAMPLE
    ./Stage-NativeArtifacts.ps1 -RunId 34528935791 -Destination ./build/ci-common/bin
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RunId,
    [Parameter(Mandatory = $true)][string]$Destination,
    [string[]]$Artifacts = @(
        'xpe-ci-common-binaries',
        'xpe-ci-preprocess-binaries',
        'xpe-ci-post-binaries'
    )
)

$ErrorActionPreference = 'Stop'

function Get-Md5([string]$Path) {
    # Lower-case hex, matching what ci.yml writes — the reader compares these strings.
    (Get-FileHash -Path $Path -Algorithm MD5).Hash.ToLower()
}

# The head SHA is the load-bearing field: it is what a reader compares against the commit under
# test. Without it the manifest would only say "some run", which is what we already had.
Write-Host "Reading run $RunId ..."
$runJson = gh run view $RunId --json headSha,headBranch,workflowName,createdAt | ConvertFrom-Json
if (-not $runJson.headSha) {
    throw "gh run view $RunId returned no headSha. Cannot record provenance, so nothing was staged."
}

$staging = Join-Path ([System.IO.Path]::GetTempPath()) "xpe-artifacts-$RunId"
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Path $staging -Force | Out-Null

foreach ($artifact in $Artifacts) {
    Write-Host "Downloading $artifact ..."
    gh run download $RunId -n $artifact -D (Join-Path $staging $artifact)
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null

# Drop any existing record BEFORE the first copy. A copy can fail part-way (a locked DLL, a full
# disk, a name collision), and the state that leaves behind — new binaries beside a record naming
# the PREVIOUS run — is worse than no record at all: the guard would pass and attribute these files
# to a run that never produced them. Measured on this script before the removal was added: eight
# files staged, the copy failed, and provenance.json still named the earlier run.
# With the record gone first, a mid-copy failure leaves a directory the guard REFUSES.
$provenancePath = Join-Path $Destination 'provenance.json'
if (Test-Path $provenancePath) { Remove-Item -Force $provenancePath }

$staged = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($file in Get-ChildItem -Path $staging -Recurse -Include '*.dll', '*.exe') {
    $target = Join-Path $Destination $file.Name
    # -Force, never skip-if-exists: a copy that quietly leaves the old file in place is the defect
    # GUI-C-31 measured.
    Copy-Item -Path $file.FullName -Destination $target -Force
    [void]$staged.Add($file.Name)
}

if ($staged.Count -eq 0) {
    throw "No .dll or .exe was found in the downloaded artifacts. Nothing staged, no provenance written."
}

# Hash AFTER every copy, once per destination file — not as each copy lands. Several artifacts ship
# the same file name (xpe_common.dll appears in two), so the last copy wins on disk; hashing during
# the loop recorded the md5 of a file that was then overwritten, and the manifest would have named a
# hash matching nothing on disk. A provenance record that is wrong is worse than none.
$records = @()
foreach ($name in ($staged | Sort-Object)) {
    $target = Join-Path $Destination $name
    $records += [pscustomobject]@{
        name   = $name
        md5    = Get-Md5 $target
        length = (Get-Item $target).Length
    }
}

# Shape fixed by ci.yml (21c1e92). "lane" distinguishes this local staging from the CI writer.
$provenance = [pscustomobject]@{
    source      = 'lane'
    runId       = "$RunId"
    headSha     = $runJson.headSha
    headBranch  = $runJson.headBranch
    workflow    = $runJson.workflowName
    runCreated  = $runJson.createdAt
    stagedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
    files       = $records
}

$provenance | ConvertTo-Json -Depth 5 | Set-Content -Path $provenancePath -Encoding utf8

Write-Host ""
Write-Host "Staged $($records.Count) file(s) into $Destination"
Write-Host "  run    $RunId  ($($runJson.workflowName), $($runJson.headBranch))"
Write-Host "  head   $($runJson.headSha)"
foreach ($r in $records) { Write-Host "  $($r.md5)  $($r.name)" }
Write-Host "Provenance: $provenancePath"
