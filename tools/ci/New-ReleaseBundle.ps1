param(
    [Parameter(Mandatory = $true)]
    [string]$OutDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$resolvedOutDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutDir))
if (Test-Path $resolvedOutDir) {
    Remove-Item -LiteralPath $resolvedOutDir -Recurse -Force
}

New-Item -ItemType Directory -Path $resolvedOutDir | Out-Null

$bundlePaths = @(
    'README.md',
    'third_party/vcpkg.json',
    'docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md',
    'docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md',
    '.github/ISSUE_TEMPLATE',
    '.github/issue-drafts',
    '.github/workflows',
    'tools/ci'
)

foreach ($path in $bundlePaths) {
    if (Test-Path $path) {
        $destination = Join-Path $resolvedOutDir $path
        $parent = Split-Path $destination -Parent
        if (-not (Test-Path $parent)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Copy-Item -LiteralPath $path -Destination $destination -Recurse -Force
    }
}

$generatedAtUtc = (Get-Date).ToUniversalTime().ToString('o')

$metadata = @"
Repository: $env:GITHUB_REPOSITORY
Ref: $env:GITHUB_REF
SHA: $env:GITHUB_SHA
RunId: $env:GITHUB_RUN_ID
GeneratedAtUtc: $generatedAtUtc
"@

Set-Content -LiteralPath (Join-Path $resolvedOutDir 'BUNDLE-METADATA.txt') -Value $metadata -Encoding UTF8

Write-Host "Release bundle written to $resolvedOutDir"
