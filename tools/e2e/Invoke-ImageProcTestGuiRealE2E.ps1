# GUI-C-83 / #175 (2026-09-17): this is the REAL-backend E2E, so it asks for Native explicitly.
# Before this date it passed no --automation-backend and the requested mode came from whatever
# the settings file held. Since GUI-C-83 the automation report fails (Passed=False) when the
# requested and actual backends differ, so an explicit request is what makes that check mean
# 'Native actually loaded' rather than 'whatever the settings said was honoured'.
#
# SelfCheck: GUI-C-76 saw it fail ('VOI window center should default to Abdomen preset') because
# SelfCheck and the fixture template expected the HU window 40/400 while the app defaults to the
# raw-DN window 32768/65535. GUI-C-84 measured that VOI is applied to raw DN (modality identity),
# so the app was right; SelfCheck and the template were fixed in 08ae377.
#
# Still open: the Native body-part presets are HU values and flatten a raw image to one level
# (GUI-C-84). REQ-DISP-017 was amended to the DN domain; the code change is QA-B-82. This script
# does not exercise presets. It is not run by CI.

# GUI-C-233i: the backend-version assertion compares with the facts of the backend that was REQUESTED. It used to compare the Mock string (manifest) with a Native run, so the script could not
# pass as soon as it asked for Native (2026-09-17). Native: the report must say Native, must not carry a mock version, and must equal what the loaded DLL itself reports
# (xpe_display_version(), read here through P/Invoke from the same folder the app was given); Mock: the manifest's Mock value.
param(
    [ValidateSet('Native', 'Mock')]
    [string]$Backend = 'Native'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$mainExe = Join-Path $repoRoot 'gui\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.exe'
$selfCheckExe = Join-Path $repoRoot 'gui\ImageProcTest.SelfCheck\bin\Debug\net8.0-windows\ImageProcTest.SelfCheck.exe'
$prepareFixtureScript = Join-Path $repoRoot 'tools\e2e\Prepare-ImageProcTestFixture.ps1'
$manifestPath = Join-Path $repoRoot 'gui\ImageProcTest\fixtures\gui-s0\fixture-manifest.json'

Assert-Condition (Test-Path $mainExe) "ImageProcTest.exe not found: $mainExe"
Assert-Condition (Test-Path $selfCheckExe) "ImageProcTest.SelfCheck.exe not found: $selfCheckExe"
Assert-Condition (Test-Path $prepareFixtureScript) "Fixture preparation script not found: $prepareFixtureScript"
Assert-Condition (Test-Path $manifestPath) "Fixture manifest not found: $manifestPath"

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
& $prepareFixtureScript | Out-String | Write-Verbose

$settingsFile = Join-Path $repoRoot ('gui\ImageProcTest\bin\Debug\net8.0-windows\{0}' -f $manifest.runtime.preparedSettingsFileName)
$reportFile = Join-Path $repoRoot ('gui\ImageProcTest\bin\Debug\net8.0-windows\{0}' -f $manifest.runtime.automationReportFileName)
$prepReportFile = Join-Path $repoRoot ('gui\ImageProcTest\bin\Debug\net8.0-windows\{0}' -f $manifest.runtime.prepReportFileName)

Assert-Condition (Test-Path $settingsFile) "Prepared appsettings.json not found: $settingsFile"
Assert-Condition (Test-Path $prepReportFile) "Fixture prep report not found: $prepReportFile"

$prepReport = Get-Content $prepReportFile -Raw | ConvertFrom-Json
$rawFile = [string]$prepReport.RawPath
Assert-Condition (Test-Path $rawFile) "Prepared raw fixture not found: $rawFile"

$selfCheckOutput = & $selfCheckExe 2>&1 | Out-String
Assert-Condition ($LASTEXITCODE -eq 0) "Self-check failed.`n$selfCheckOutput"
Assert-Condition ($selfCheckOutput -like '*GUI-S0 self-check passed.*') 'Self-check success text missing.'

Remove-Item $reportFile -ErrorAction SilentlyContinue

$process = Start-Process -FilePath $mainExe `
    -WorkingDirectory (Split-Path -Parent $mainExe) `
    -ArgumentList @('--automation-raw', $rawFile, '--automation-report', $reportFile, '--automation-backend', $Backend) `
    -PassThru

Assert-Condition ($process.WaitForExit(30000)) 'ImageProcTest automation mode did not exit within 30 seconds.'
Assert-Condition (Test-Path $reportFile) "Automation report was not created: $reportFile"

$report = Get-Content $reportFile -Raw | ConvertFrom-Json

Assert-Condition ($report.Passed -eq $true) ("Automation report marked failure.`n{0}" -f (Get-Content $reportFile -Raw))
Assert-Condition ($report.ActualBackendMode -eq $Backend) ("The {0} backend was requested but the report says {1} ran (BackendMatchesRequest={2})." -f $Backend, $report.ActualBackendMode, $report.BackendMatchesRequest)
if ($Backend -eq 'Mock') {
    Assert-Condition ($report.BackendVersion -eq $manifest.expectedTelemetry.backendVersionMock) ("Unexpected Mock backend version in automation report: '{0}' (manifest: '{1}')." -f $report.BackendVersion, $manifest.expectedTelemetry.backendVersionMock)
}
else {
    Assert-Condition ($report.MockBackend -eq $false) 'Native was requested but the report says the mock backend ran.'
    Assert-Condition ($report.BackendVersion -notlike '*mock*') ("Native was requested but the reported backend version is a mock string: '{0}'." -f $report.BackendVersion)
    # the DLL's own answer, from the folder the app was pointed at (XPE_NATIVE_DIR), else the app's folder
    $nativeDir = if ($env:XPE_NATIVE_DIR) { $env:XPE_NATIVE_DIR } else { Split-Path -Parent $mainExe }
    Assert-Condition (Test-Path (Join-Path $nativeDir 'xpe_display.dll')) "xpe_display.dll not found in the native folder '$nativeDir'; the DLL's version cannot be read."
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class XpeDllVersionProbe {
    [DllImport("kernel32", SetLastError = true)] static extern bool SetDllDirectory(string path);
    [DllImport("xpe_display", EntryPoint = "xpe_display_version")] static extern IntPtr DisplayVersion();
    public static string Read(string dir) { SetDllDirectory(dir); return Marshal.PtrToStringAnsi(DisplayVersion()); }
}
"@
    $dllVersion = [XpeDllVersionProbe]::Read($nativeDir)
    Assert-Condition (-not [string]::IsNullOrWhiteSpace($dllVersion)) 'xpe_display_version() returned nothing.'
    Assert-Condition ($report.BackendVersion -eq ('xpe_display {0}' -f $dllVersion)) ("Backend version in the report ('{0}') is not the loaded DLL's ('xpe_display {1}', read from {2})." -f $report.BackendVersion, $dllVersion, $nativeDir)
    Assert-Condition ($report.DisplayVersion -eq $dllVersion) ("Display version in the report ('{0}') is not the DLL's ('{1}')." -f $report.DisplayVersion, $dllVersion)
}
Assert-Condition ($report.InitialLogCount -ge [int]$manifest.expectedTelemetry.initialLogCount) 'Initial log count is too low.'
$expectedAlerts = if ($Backend -eq 'Mock') { [int]$manifest.expectedTelemetry.initialAlertCountMock } else { [int]$manifest.expectedTelemetry.initialAlertCountNative }
Assert-Condition ($report.InitialAlertCount -eq $expectedAlerts) ("Initial alert count of the {0} backend must match the fixture manifest: report {1}, manifest {2}." -f $Backend, $report.InitialAlertCount, $expectedAlerts)
if ($Backend -eq 'Native') {
    # the one Native alert is named, so a different single alert cannot satisfy the count
    $runtimeLog = Get-Content -LiteralPath $report.RuntimeLogExportPath -Raw
    Assert-Condition ($runtimeLog -match ('ALERT INFO ' + [regex]::Escape($manifest.expectedTelemetry.initialAlertNative))) ("The Native start-up alert '{0}' is not in the exported runtime log." -f $manifest.expectedTelemetry.initialAlertNative)
}
Assert-Condition ($report.LogCountAfterLoad -gt $report.InitialLogCount) 'Load action did not increase log count.'
Assert-Condition ($report.ActiveImageSummary -like ("RAW {0}x{1}*" -f $manifest.rawSample.width, $manifest.rawSample.height)) 'Raw image summary was not updated.'
Assert-Condition ($report.LastRawDirPersisted -eq $true) 'lastRawDir was not persisted.'
Assert-Condition ($report.RequestedCalibrationEvaluationSummary -like '*Offset=Off*') 'Calibration evaluation summary did not record Offset=Off.'
Assert-Condition ($report.RequestedCalibrationEvaluationSummary -like '*Defect=On*') 'Calibration evaluation summary did not record Defect=On.'
Assert-Condition ($report.CalibrationEvaluationEvidenceExported -eq $true) 'Calibration evaluation state was not exported in menu-command evidence.'
Assert-Condition ($report.HelpWindowOpened -eq $true) 'Help window did not open during automation.'
Assert-Condition ($report.HelpDocumentLoaded -eq $true) 'Help document did not load during automation.'
Assert-Condition ($report.HelpWindowTitle -like '*Quick Start*') 'Unexpected help window title.'
Assert-Condition ($report.HelpDocumentPath -like '*quick-start.html') 'Unexpected help document path.'
Assert-Condition ($report.LogCountAfterClear -eq 0) 'Logs were not cleared.'
Assert-Condition ($report.AlertCountAfterClear -eq 0) 'Alerts were not cleared.'
Assert-Condition ($report.RuntimeStateAfterShutdown -eq 'Shutdown') 'Runtime did not enter Shutdown state.'

Write-Output 'GUI real automation E2E passed.'
Write-Output ("Automation report: {0}" -f $reportFile)
