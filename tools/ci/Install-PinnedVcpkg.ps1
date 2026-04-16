param(
    [string]$ManifestPath = 'third_party/common/vcpkg.json'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
    throw 'VCPKG_ROOT is not set.'
}

if (-not (Test-Path $ManifestPath)) {
    throw "Missing vcpkg manifest: $ManifestPath"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$baseline = $manifest.'builtin-baseline'
if ([string]::IsNullOrWhiteSpace($baseline) -or $baseline -notmatch '^[0-9a-fA-F]{40}$') {
    throw "Manifest does not contain a valid 40-character builtin-baseline: $ManifestPath"
}

$vcpkgRoot = $env:VCPKG_ROOT
$workspace = $env:GITHUB_WORKSPACE
if ([string]::IsNullOrWhiteSpace($workspace)) {
    throw 'GITHUB_WORKSPACE is required for safe vcpkg installation.'
}

$workspacePath = (Resolve-Path $workspace).Path.TrimEnd('\')
$vcpkgParent = Split-Path -Parent $vcpkgRoot
if (-not (Test-Path $vcpkgParent)) {
    New-Item -ItemType Directory -Path $vcpkgParent -Force | Out-Null
}

$resolvedParent = (Resolve-Path $vcpkgParent).Path.TrimEnd('\')
if (-not $resolvedParent.StartsWith($workspacePath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to manage VCPKG_ROOT outside GITHUB_WORKSPACE: $vcpkgRoot"
}

$bootstrap = Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat'
if (-not (Test-Path $bootstrap)) {
    if (Test-Path $vcpkgRoot) {
        Remove-Item -LiteralPath $vcpkgRoot -Recurse -Force
    }
    git clone https://github.com/microsoft/vcpkg $vcpkgRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to clone vcpkg.'
    }
}

git -C $vcpkgRoot checkout --quiet $baseline
if ($LASTEXITCODE -ne 0) {
    git -C $vcpkgRoot fetch origin $baseline
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to fetch vcpkg baseline $baseline"
    }

    git -C $vcpkgRoot checkout --quiet FETCH_HEAD
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to checkout vcpkg baseline $baseline"
    }
}

& $bootstrap -disableMetrics
if ($LASTEXITCODE -ne 0) {
    throw 'vcpkg bootstrap failed.'
}

Write-Host "vcpkg pinned to baseline $baseline"
