<#
.SYNOPSIS
    BP-06..10 degraded-mode stress test driver.

.DESCRIPTION
    Stages built XPE DLLs and test executables, removes one optional DLL or all
    optional DLLs, then runs the common DegradedMode.* GTest cases. The common
    test executable links only xpe_common so missing optional DLLs cannot abort
    process startup before readiness degradation is verified.
#>

param(
    [Parameter(Mandatory)] [string] $Scenario,
    [string] $AbsentDll = '',
    [Parameter(Mandatory)] [string] $ExpectedReadiness,
    [Parameter(Mandatory)] [string] $BuildDir,
    [Parameter(Mandatory)] [string] $ResultsDir,
    [int] $ProcessTimeoutSec = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$OptionalDlls = @(
    'xpe_enhance_basic.dll',
    'gsvg.dll',
    'xpe_enhance_advanced.dll',
    'xpe_display.dll',
    'xpe_dicom.dll'
)

if ([string]::IsNullOrWhiteSpace($AbsentDll)) {
    if ($Scenario -eq 'all_optional_absent') {
        $AbsentDll = 'ALL_OPTIONAL'
    } else {
        throw '-AbsentDll is required unless -Scenario is all_optional_absent.'
    }
}

function Write-Result {
    param([bool]$Passed, [string]$Summary, [object]$Details)

    New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
    $result = @{
        scenario  = $Scenario
        passed    = $Passed
        summary   = $Summary
        timestamp = (Get-Date -Format 'o')
        details   = $Details
    }
    $result | ConvertTo-Json -Depth 5 | Set-Content "$ResultsDir/result.json"

    if ($Passed) {
        Write-Host "[PASS] $Scenario - $Summary"
    } else {
        Write-Error "[FAIL] $Scenario - $Summary"
    }
}

$dllSearchRoot = Resolve-Path $BuildDir
$binDir = Join-Path $dllSearchRoot 'bin'
if (-not (Test-Path $binDir)) {
    $directDlls = @(Get-ChildItem -Path $dllSearchRoot -Filter '*.dll' -File)
    $directExes = @(Get-ChildItem -Path $dllSearchRoot -Filter '*.exe' -File)
    if (($directDlls.Count + $directExes.Count) -eq 0) {
        Write-Result -Passed $false `
            -Summary 'Build output bin directory was not found' `
            -Details @{
                build_dir      = $dllSearchRoot.Path
                expected_bin   = $binDir
                harness_status = 'not_ready'
            }
        exit 1
    }
    $binDir = $dllSearchRoot.Path
}

$allBuiltDlls = Get-ChildItem -Path $binDir -Filter '*.dll' -File
$allBuiltExes = Get-ChildItem -Path $binDir -Filter '*.exe' -File

$stageDir = "$ResultsDir/staging"
if (Test-Path $stageDir) {
    Remove-Item $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

foreach ($dll in $allBuiltDlls) {
    Copy-Item $dll.FullName $stageDir -Force
}
foreach ($exe in $allBuiltExes) {
    Copy-Item $exe.FullName $stageDir -Force
}

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
        Write-Warning "DLL not found in build output: $AbsentDll"
    }
}

$smokeExe = Get-ChildItem -Path $stageDir -Filter 'test_xpe_common.exe' | Select-Object -First 1
if (-not $smokeExe) {
    Write-Result -Passed $false `
        -Summary 'test_xpe_common.exe with DegradedMode.* tests was not found' `
        -Details @{
            removed_dlls   = $removedDlls
            stage_dir      = $stageDir
            harness_status = 'ready'
            exe_status     = 'missing'
        }
    exit 1
}

$env:PATH = "$stageDir;$env:PATH"
$env:XPE_DEGRADED_STAGE_DIR = $stageDir
$env:XPE_DEGRADED_ABSENT_DLL = $AbsentDll

Write-Host "Running: $($smokeExe.FullName)"
$stdoutPath = Join-Path $ResultsDir 'stdout.txt'
$stderrPath = Join-Path $ResultsDir 'stderr.txt'
$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $smokeExe.FullName
$startInfo.Arguments = '--gtest_filter=DegradedMode.*'
$startInfo.WorkingDirectory = $stageDir
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::new()
$proc.StartInfo = $startInfo
[void] $proc.Start()

if (-not $proc.WaitForExit($ProcessTimeoutSec * 1000)) {
    $proc.Kill()
    $proc.WaitForExit()
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    if ($stdout) { Set-Content $stdoutPath $stdout }
    if ($stderr) { Set-Content $stderrPath $stderr }
    Write-Result -Passed $false `
        -Summary "Degraded-mode test process timed out after $ProcessTimeoutSec seconds" `
        -Details @{
            removed_dlls       = $removedDlls
            expected_readiness = $ExpectedReadiness
            exit_code          = 124
            smoke_exe          = $smokeExe.Name
            timeout_sec        = $ProcessTimeoutSec
        }
    exit 124
}

$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()
Set-Content $stdoutPath $stdout
Set-Content $stderrPath $stderr
$exitCode = $proc.ExitCode

Write-Host "Exit code: $exitCode"
if ($stdout) { Write-Host $stdout }
if ($stderr) { Write-Host $stderr }

$executedCount = 0
if ($stdout -and $stdout -match '\[\s+RUN\s+\]\s+DegradedMode\.') {
    $executedCount = ([regex]::Matches($stdout, '\[\s+RUN\s+\]\s+DegradedMode\.')).Count
}

$passedCount = 0
if ($stdout -and $stdout -match '\[\s+OK\s+\]\s+DegradedMode\.') {
    $passedCount = ([regex]::Matches($stdout, '\[\s+OK\s+\]\s+DegradedMode\.')).Count
}

$passed = ($exitCode -eq 0 -and $passedCount -gt 0)
Write-Result -Passed $passed `
    -Summary "$(if ($passed) { 'Graceful degradation verified' } else { 'Degradation test failed or no DegradedMode test passed' }) (exit $exitCode, passed $passedCount, executed $executedCount)" `
    -Details @{
        removed_dlls       = $removedDlls
        expected_readiness = $ExpectedReadiness
        exit_code          = $exitCode
        smoke_exe          = $smokeExe.Name
        executed_tests     = $executedCount
        passed_tests       = $passedCount
    }

if ($passed) { exit 0 }
exit 1
