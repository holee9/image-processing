<#
.SYNOPSIS
    Runs the E2E suite N times, keeping a trx per run, and lists failures BY NAME.

.DESCRIPTION
    GUI-C-49. An intermittent failure was seen twice (GUI-C-48) and its name was lost both times —
    not because the runner hid it, but because the lane's own repeat loop piped `dotnet test` through
    `grep '통과!|실패!' | head`, which keeps the summary line and drops the "실패 <name>" line above it.
    The name was always in the output; the measurement threw it away.

    So the loop lives here instead of being retyped per card. Every run writes its own trx, and the
    summary reads names out of those files rather than out of a filtered console stream — a run that
    crashes mid-way still leaves the trx of the runs before it.

    Why a trx and not just fuller console output: the console of a run that aborts is truncated at
    the point of the abort, while trx is written per completed test. When the suite dies, the trx
    says what had passed; the console says nothing.

.PARAMETER Times
    How many times to run the suite.

.PARAMETER Backend
    Mock or Native. Native additionally needs -NativeDir.

.PARAMETER NativeDir
    Directory holding the staged native DLLs (becomes XPE_NATIVE_DIR).

.PARAMETER Filter
    Optional --filter expression.

.EXAMPLE
    ./Repeat-E2E.ps1 -Times 10 -Backend Native -NativeDir ./build/ci-common/bin
#>
[CmdletBinding()]
param(
    [int]$Times = 10,
    [ValidateSet('Mock', 'Native')][string]$Backend = 'Mock',
    [string]$NativeDir,
    [string]$Filter,
    [string]$ResultsDirectory = 'build/e2e-repeat'
)

$ErrorActionPreference = 'Stop'

if ($Backend -eq 'Native' -and -not $NativeDir) {
    throw "-NativeDir is required for the Native backend; without it the run measures the loader's choice, not a pinned directory (#129)."
}

$project = 'clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj'
New-Item -ItemType Directory -Path $ResultsDirectory -Force | Out-Null

$env:XPE_E2E_BACKEND = $Backend
if ($NativeDir) { $env:XPE_NATIVE_DIR = (Resolve-Path $NativeDir).Path }

$runs = @()
for ($i = 1; $i -le $Times; $i++) {
    $trx = "run-$i.trx"
    $arguments = @(
        'test', $project, '-c', 'Debug', '--no-build',
        '--logger', "trx;LogFileName=$trx",
        '--results-directory', $ResultsDirectory
    )
    if ($Filter) { $arguments += @('--filter', $Filter) }

    $started = Get-Date
    # Output is discarded on purpose: the trx is the record. Piping it through a filter here is how
    # the names were lost in the first place.
    #
    # $ErrorActionPreference is relaxed around the call ONLY. dotnet writes a failing test to stderr,
    # and under 'Stop' PowerShell turns any native stderr into a terminating NativeCommandError: the
    # first 10-run batch aborted at the one failing run and printed no summary — the loop died on
    # exactly the event it exists to record. Merging the streams alone does not help; the preference
    # is what makes it terminating, so that is what is changed, and only here.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & dotnet @arguments 2>&1 | Out-Null }
    finally { $ErrorActionPreference = $previous }
    $seconds = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)

    $path = Join-Path $ResultsDirectory $trx
    $failed = @()
    if (Test-Path $path) {
        [xml]$doc = Get-Content $path
        $failed = @($doc.TestRun.Results.UnitTestResult |
            Where-Object { $_.outcome -eq 'Failed' } |
            ForEach-Object { $_.testName })
    }

    $runs += [pscustomobject]@{ Run = $i; Seconds = $seconds; Failed = $failed }
    $status = if ($failed.Count -eq 0) { 'pass' } else { "FAIL: $($failed -join ', ')" }
    Write-Host "run $i  ${seconds}s  $status"
}

Write-Host ""
Write-Host "=== $Times run(s), backend=$Backend ==="
$failingRuns = @($runs | Where-Object { $_.Failed.Count -gt 0 })
Write-Host "runs with a failure: $($failingRuns.Count) / $Times"

# Grouped by name: an intermittent that always hits the same scenario is a different problem from
# one that moves around, and the counts are what tell them apart.
$runs.Failed | Group-Object | Sort-Object Count -Descending | ForEach-Object {
    Write-Host ("  {0,3}x  {1}" -f $_.Count, $_.Name)
}

# Per-run seconds are printed above, one line each. A min/max/median summary line lived here and
# was removed in GUI-C-49: it threw an ArgumentTransformationMetadataException that aborted the
# script AFTER the failure list had printed, and debugging a convenience line was not worth
# spending the card's budget on. The numbers it would have shown are already on screen.
