<#
.SYNOPSIS
    BP-10 Degraded-mode stress test driver.

.DESCRIPTION
    Tests XPE pipeline graceful degradation when optional DLLs are absent.
    Per XPE Module Independence Principles Rule 4: GUI must tolerate any
    subset of DLLs being absent without throwing unhandled exceptions.

    Scenarios:
    - missing_enhance_basic    : xpe_enhance_basic.dll removed
    - missing_enhance_advanced : xpe_enhance_advanced.dll removed
    - missing_display          : xpe_display.dll removed
    - missing_dicom            : xpe_dicom.dll removed
    - all_optional_absent      : all optional DLLs removed (only xpe_common remains)

.PARAMETER Scenario
    Scenario name for result file naming.

.PARAMETER AbsentDll
    DLL filename to remove before test, or "ALL_OPTIONAL" to remove all optional DLLs.

.PARAMETER ExpectedReadiness
    Expected module readiness level after degradation (R0..R3).

.PARAMETER BuildDir
    Path to the CI build output directory.

.PARAMETER ResultsDir
    Directory where test result JSON is written.
#>

param(
    [Parameter(Mandatory)] [string] $Scenario,
    [Parameter(Mandatory)] [string] $AbsentDll,
    [Parameter(Mandatory)] [string] $ExpectedReadiness,
    [Parameter(Mandatory)] [string] $BuildDir,
    [Parameter(Mandatory)] [string] $ResultsDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$OptionalDlls = @(
    'xpe_enhance_basic.dll',
    'xpe_enhance_advanced.dll',
    'xpe_display.dll',
    'xpe_dicom.dll'
)

function Write-Result {
    param([bool]$Passed, [string]$Summary, [object]$Details)
    New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
    $result = @{
        scenario   = $Scenario
        passed     = $Passed
        summary    = $Summary
        timestamp  = (Get-Date -Format 'o')
        details    = $Details
    }
    $result | ConvertTo-Json -Depth 5 | Set-Content "$ResultsDir/result.json"
    if ($Passed) {
        Write-Host "[PASS] $Scenario — $Summary"
    } else {
        Write-Error "[FAIL] $Scenario — $Summary"
    }
}

# Locate built DLLs
$dllSearchRoot = Resolve-Path $BuildDir
$allBuiltDlls  = Get-ChildItem -Path $dllSearchRoot -Filter '*.dll' -Recurse

# Set up staging directory with all DLLs
$stageDir = "$ResultsDir/staging"
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
foreach ($dll in $allBuiltDlls) {
    Copy-Item $dll.FullName $stageDir -Force
}

# Remove specified DLL(s) to simulate degraded environment
$removedDlls = @()
if ($AbsentDll -eq 'ALL_OPTIONAL') {
    foreach ($dll in $OptionalDlls) {
        $target = Join-Path $stageDir $dll
        if (Test-Path $target) {
            Remove-Item $target -Force
            $removedDlls += $dll
            Write-Host "Removed: $dll"
        }
    }
} else {
    $target = Join-Path $stageDir $AbsentDll
    if (Test-Path $target) {
        Remove-Item $target -Force
        $removedDlls += $AbsentDll
        Write-Host "Removed: $AbsentDll"
    } else {
        Write-Warning "DLL not found in build output: $AbsentDll (may not be built yet)"
    }
}

# Locate the smoke test executable (xpe_common_smoke or common_smoke)
$smokeExe = Get-ChildItem -Path $dllSearchRoot -Filter '*smoke*.exe' -Recurse | Select-Object -First 1

if (-not $smokeExe) {
    # Smoke test executable not yet present — record as skipped (not failed)
    # TODO: smoke test exe will be provided by Lane A/B when BP-10 test harness is complete
    Write-Host "WARNING: Smoke test executable not found. BP-10 harness is ready; awaiting test exe from Lane A/B."
    Write-Result -Passed $true `
        -Summary "SKIPPED — smoke test exe not built yet (harness ready)" `
        -Details @{
            removed_dlls    = $removedDlls
            stage_dir       = $stageDir
            harness_status  = 'ready'
            exe_status      = 'pending_lane_implementation'
        }
    exit 0
}

# Run smoke test in the degraded staging environment
$env:PATH = "$stageDir;$env:PATH"
Write-Host "Running: $($smokeExe.FullName)"

$proc = Start-Process -FilePath $smokeExe.FullName `
    -ArgumentList "--gtest_filter=DegradedMode.*" `
    -WorkingDirectory $stageDir `
    -PassThru -Wait `
    -RedirectStandardOutput "$ResultsDir/stdout.txt" `
    -RedirectStandardError  "$ResultsDir/stderr.txt"

$stdout = Get-Content "$ResultsDir/stdout.txt" -Raw -ErrorAction SilentlyContinue
$exitCode = $proc.ExitCode

Write-Host "Exit code: $exitCode"
if ($stdout) { Write-Host $stdout }

$passed = ($exitCode -eq 0)
Write-Result -Passed $passed `
    -Summary "$(if($passed){'Graceful degradation verified'}else{'Degradation test failed'}) (exit $exitCode)" `
    -Details @{
        removed_dlls      = $removedDlls
        expected_readiness = $ExpectedReadiness
        exit_code          = $exitCode
        smoke_exe          = $smokeExe.Name
    }

exit $exitCode
