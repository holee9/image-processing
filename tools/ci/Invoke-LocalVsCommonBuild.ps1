param(
    [string]$BuildDir = 'build/local-vs2022-common',
    [switch]$PostBenchmark,
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$vsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vsWhere)) {
    throw "vswhere.exe not found: $vsWhere"
}

$vsInstall = & $vsWhere -products * -version '[17.0,18.0)' -latest -property installationPath
if ([string]::IsNullOrWhiteSpace($vsInstall)) {
    throw 'Visual Studio 2022 installation not found.'
}

$vsDevCmd = Join-Path $vsInstall 'Common7\Tools\VsDevCmd.bat'
$cmake = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$ninja = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$vcpkgRoot = Join-Path $vsInstall 'VC\vcpkg'
$toolchainFile = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$gitCmdPath = 'C:\Program Files\Git\cmd'
$pwshCommand = Get-Command pwsh -ErrorAction SilentlyContinue

$requiredPaths = @($vsDevCmd, $cmake, $ctest, $ninja, $vcpkgRoot, $toolchainFile, $gitCmdPath)
foreach ($path in $requiredPaths) {
    if (-not (Test-Path $path)) {
        throw "Required tool path not found: $path"
    }
}

$envDump = cmd.exe /c "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
foreach ($line in $envDump) {
    if ($line -match '^(.*?)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}

$cacheRoot = Join-Path $repoRoot '.cache\localappdata'
$appDataRoot = Join-Path $repoRoot '.cache\appdata'
$downloadsRoot = Join-Path $repoRoot '.cache\vcpkg-downloads'
$binaryCacheRoot = Join-Path $repoRoot '.cache\vcpkg-bincache'
$registriesRoot = Join-Path $cacheRoot 'vcpkg\registries'

New-Item -ItemType Directory -Force -Path $cacheRoot, $appDataRoot, $downloadsRoot, $binaryCacheRoot, $registriesRoot | Out-Null

[System.Environment]::SetEnvironmentVariable('LOCALAPPDATA', $cacheRoot, 'Process')
[System.Environment]::SetEnvironmentVariable('APPDATA', $appDataRoot, 'Process')
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $vcpkgRoot, 'Process')
[System.Environment]::SetEnvironmentVariable('VCPKG_DISABLE_METRICS', '1', 'Process')
[System.Environment]::SetEnvironmentVariable('VCPKG_DOWNLOADS', $downloadsRoot, 'Process')
[System.Environment]::SetEnvironmentVariable('VCPKG_DEFAULT_BINARY_CACHE', $binaryCacheRoot, 'Process')
[System.Environment]::SetEnvironmentVariable('X_VCPKG_REGISTRIES_CACHE', $registriesRoot, 'Process')

$pathEntries = @($gitCmdPath, (Split-Path $cmake), (Split-Path $ninja))
if ($pwshCommand) {
    [System.Environment]::SetEnvironmentVariable('VCPKG_FORCE_SYSTEM_BINARIES', '1', 'Process')
    $pathEntries += Split-Path $pwshCommand.Source
} else {
    [System.Environment]::SetEnvironmentVariable('VCPKG_FORCE_SYSTEM_BINARIES', $null, 'Process')
}

$env:PATH = (($pathEntries -join ';') + ';' + $env:PATH)

$resolvedBuildDir = Join-Path $repoRoot $BuildDir
if ($Clean -and (Test-Path $resolvedBuildDir)) {
    if (-not $resolvedBuildDir.StartsWith($repoRoot.Path, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean outside the repository root: $resolvedBuildDir"
    }

    cmd.exe /c "if exist `"$resolvedBuildDir`" rmdir /s /q `"$resolvedBuildDir`""
    if (Test-Path $resolvedBuildDir) {
        throw "Failed to clean build directory: $resolvedBuildDir"
    }
}

New-Item -ItemType Directory -Force -Path $resolvedBuildDir | Out-Null

$configureArgs = @(
    '-S', '.',
    '-B', $resolvedBuildDir,
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    '-DCMAKE_C_COMPILER=cl.exe',
    '-DCMAKE_CXX_COMPILER=cl.exe',
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DVCPKG_MANIFEST_DIR=$repoRoot\third_party\common",
    '-DVCPKG_TARGET_TRIPLET=x64-windows',
    '-DBUILD_TESTS=ON',
    '-DBUILD_TESTING=ON',
    '-DXPE_WARNINGS_AS_ERRORS=ON'
)

if ($PostBenchmark) {
    $configureArgs += @(
        '-DBUILD_GSVG=ON',
        '-DBUILD_PREPROCESS=OFF',
        '-DBUILD_ENHANCE_BASIC=ON',
        '-DBUILD_ENHANCE_ADVANCED=ON',
        '-DBUILD_AI=OFF',
        '-DBUILD_DISPLAY=ON',
        '-DBUILD_DICOM=OFF'
    )
} else {
    $configureArgs += '-DBUILD_GSVG=ON'
}

Write-Host "Visual Studio: $vsInstall"
Write-Host "Build directory: $resolvedBuildDir"
Write-Host 'Configuring...'
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

Write-Host 'Building...'
& $cmake --build $resolvedBuildDir --parallel
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE"
}

Write-Host 'Running tests...'
& $ctest --test-dir $resolvedBuildDir --build-config RelWithDebInfo --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "CTest failed with exit code $LASTEXITCODE"
}

Write-Host 'Local VS2022 common build completed successfully.' -ForegroundColor Green
