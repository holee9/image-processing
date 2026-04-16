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
    -ArgumentList @('--automation-raw', $rawFile, '--automation-report', $reportFile) `
    -PassThru

Assert-Condition ($process.WaitForExit(30000)) 'ImageProcTest automation mode did not exit within 30 seconds.'
Assert-Condition (Test-Path $reportFile) "Automation report was not created: $reportFile"

$report = Get-Content $reportFile -Raw | ConvertFrom-Json

Assert-Condition ($report.Passed -eq $true) ("Automation report marked failure.`n{0}" -f (Get-Content $reportFile -Raw))
Assert-Condition ($report.BackendVersion -eq $manifest.expectedTelemetry.backendVersion) 'Unexpected backend version in automation report.'
Assert-Condition ($report.InitialLogCount -ge [int]$manifest.expectedTelemetry.initialLogCount) 'Initial log count is too low.'
Assert-Condition ($report.InitialAlertCount -eq [int]$manifest.expectedTelemetry.initialAlertCount) 'Initial alert count must match fixture manifest.'
Assert-Condition ($report.LogCountAfterLoad -gt $report.InitialLogCount) 'Load action did not increase log count.'
Assert-Condition ($report.ActiveImageSummary -like ("RAW {0}x{1}*" -f $manifest.rawSample.width, $manifest.rawSample.height)) 'Raw image summary was not updated.'
Assert-Condition ($report.LastRawDirPersisted -eq $true) 'lastRawDir was not persisted.'
Assert-Condition ($report.CalibrationEvaluationSummary -like '*Offset=Off*') 'Calibration evaluation summary did not record Offset=Off.'
Assert-Condition ($report.CalibrationEvaluationSummary -like '*Defect=On*') 'Calibration evaluation summary did not record Defect=On.'
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
