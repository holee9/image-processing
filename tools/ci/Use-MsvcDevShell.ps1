param(
    [switch]$PersistForGitHubActions
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$programFilesX86 = ${env:ProgramFiles(x86)}
if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
    throw 'ProgramFiles(x86) is not set; cannot locate vswhere.'
}

$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found: $vswhere"
}

$installPath = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if ([string]::IsNullOrWhiteSpace($installPath)) {
    throw 'No Visual Studio installation with MSVC x64 tools was found.'
}

$devCmd = Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path $devCmd)) {
    throw "VsDevCmd.bat not found: $devCmd"
}

$cmd = "`"$devCmd`" -arch=amd64 -host_arch=amd64 >nul && set"
$environmentLines = cmd.exe /d /s /c $cmd

$persistNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    'PATH',
    'INCLUDE',
    'LIB',
    'LIBPATH',
    'DevEnvDir',
    'ExtensionSdkDir',
    'Framework40Version',
    'FrameworkDir',
    'FrameworkDir64',
    'FrameworkVersion',
    'FrameworkVersion64',
    'NETFXSDKDir',
    'UCRTVersion',
    'UniversalCRTSdkDir',
    'VCIDEInstallDir',
    'VCINSTALLDIR',
    'VCToolsInstallDir',
    'VCToolsRedistDir',
    'VCToolsVersion',
    'VisualStudioVersion',
    'VSINSTALLDIR',
    'WindowsLibPath',
    'WindowsSdkBinPath',
    'WindowsSdkDir',
    'WindowsSDKLibVersion',
    'WindowsSDKVersion'
) | ForEach-Object { [void]$persistNames.Add($_) }

foreach ($line in $environmentLines) {
    if ($line -notmatch '^([^=]+)=(.*)$') {
        continue
    }

    $name = $matches[1]
    $value = $matches[2]
    Set-Item -Path "Env:$name" -Value $value

    if ($PersistForGitHubActions -and $env:GITHUB_ENV -and $persistNames.Contains($name)) {
        Add-Content -LiteralPath $env:GITHUB_ENV -Value "$name=$value"
    }
}

Write-Host "MSVC developer environment configured: $installPath"
