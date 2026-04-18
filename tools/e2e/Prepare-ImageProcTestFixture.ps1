param(
    [string]$Configuration = 'Debug'
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
$projectRoot = Join-Path $repoRoot 'gui\ImageProcTest'
$repoFixtureRoot = Join-Path $projectRoot 'fixtures\gui-s0'
$manifestPath = Join-Path $repoFixtureRoot 'fixture-manifest.json'
$outputDir = Join-Path $projectRoot ("bin\{0}\net8.0-windows" -f $Configuration)
$runtimeFixtureRoot = Join-Path $outputDir 'fixtures\gui-s0'

Assert-Condition (Test-Path $manifestPath) "Fixture manifest not found: $manifestPath"

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json

$templateSourcePath = Join-Path $repoFixtureRoot $manifest.runtime.settingsTemplateRelativePath
$rawSourcePath = Join-Path $repoFixtureRoot $manifest.rawSample.relativePath
$outputRawPath = Join-Path $runtimeFixtureRoot $manifest.rawSample.relativePath
$settingsPath = Join-Path $outputDir $manifest.runtime.preparedSettingsFileName
$automationReportPath = Join-Path $outputDir $manifest.runtime.automationReportFileName
$prepReportPath = Join-Path $outputDir $manifest.runtime.prepReportFileName
$helpIndexPath = Join-Path $outputDir 'help\index.html'
$helpQuickStartPath = Join-Path $outputDir 'help\quick-start.html'
$helpScopePath = Join-Path $outputDir 'help\scope.html'

Assert-Condition (Test-Path $templateSourcePath) "Fixture settings template not found: $templateSourcePath"
Assert-Condition (Test-Path $rawSourcePath) "Fixture raw sample not found: $rawSourcePath"

New-Item -ItemType Directory -Force -Path $outputDir, $runtimeFixtureRoot | Out-Null
Copy-Item (Join-Path $repoFixtureRoot '*') -Destination $runtimeFixtureRoot -Recurse -Force

$offsetCalibrationDir = Join-Path $runtimeFixtureRoot $manifest.calibrationDirectories.offset
$gainCalibrationDir = Join-Path $runtimeFixtureRoot $manifest.calibrationDirectories.gain
$defectCalibrationDir = Join-Path $runtimeFixtureRoot $manifest.calibrationDirectories.defect
New-Item -ItemType Directory -Force -Path $offsetCalibrationDir, $gainCalibrationDir, $defectCalibrationDir | Out-Null

Assert-Condition (Test-Path $outputRawPath) "Prepared raw sample not found in runtime fixture: $outputRawPath"

$rawHash = (Get-FileHash $outputRawPath -Algorithm SHA256).Hash.ToUpperInvariant()
$expectedHash = [string]$manifest.rawSample.sha256
Assert-Condition ($rawHash -eq $expectedHash.ToUpperInvariant()) "Prepared raw sample hash mismatch. Expected $expectedHash, got $rawHash"
Assert-Condition (Test-Path $helpIndexPath) "Packaged help index not found: $helpIndexPath"
Assert-Condition (Test-Path $helpQuickStartPath) "Packaged quick-start help not found: $helpQuickStartPath"
Assert-Condition (Test-Path $helpScopePath) "Packaged scope help not found: $helpScopePath"

$settings = Get-Content $templateSourcePath -Raw | ConvertFrom-Json
$settings.backendMode = [string]$manifest.backendMode
$settings.rawWidth = [int]$manifest.rawSample.width
$settings.rawHeight = [int]$manifest.rawSample.height
$settings.rawPixelFormat = [string]$manifest.rawSample.pixelFormat
$settings.calibOffsetDir = $offsetCalibrationDir
$settings.calibGainDir = $gainCalibrationDir
$settings.calibDefectDir = $defectCalibrationDir
$settings.lastRawDir = ''
$settings | ConvertTo-Json -Depth 10 | Set-Content $settingsPath -Encoding utf8

$prepReport = [pscustomobject]@{
    FixtureId = [string]$manifest.fixtureId
    FixtureVersion = [string]$manifest.fixtureVersion
    RepositoryFixtureRoot = $repoFixtureRoot
    RuntimeFixtureRoot = $runtimeFixtureRoot
    RawPath = $outputRawPath
    RawSha256 = $rawHash
    SettingsPath = $settingsPath
    HelpIndexPath = $helpIndexPath
    HelpQuickStartPath = $helpQuickStartPath
    HelpScopePath = $helpScopePath
    AutomationReportPath = $automationReportPath
    OffsetCalibrationDirectory = $offsetCalibrationDir
    GainCalibrationDirectory = $gainCalibrationDir
    DefectCalibrationDirectory = $defectCalibrationDir
}
$prepReport | ConvertTo-Json -Depth 10 | Set-Content $prepReportPath -Encoding utf8

Write-Output 'ImageProcTest fixture prepared.'
Write-Output ("Fixture manifest: {0}" -f $manifestPath)
Write-Output ("Runtime fixture root: {0}" -f $runtimeFixtureRoot)
Write-Output ("Prepared settings: {0}" -f $settingsPath)
Write-Output ("Prepared raw fixture: {0}" -f $outputRawPath)
Write-Output ("Fixture prep report: {0}" -f $prepReportPath)
