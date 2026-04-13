Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$errors = New-Object System.Collections.Generic.List[string]

function Add-RepoError {
    param([string]$Message)
    $script:errors.Add($Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
}

function Assert-PathExists {
    param([string]$RelativePath)
    if (-not (Test-Path $RelativePath)) {
        Add-RepoError "Missing required path: $RelativePath"
    } else {
        Write-Host "OK: $RelativePath"
    }
}

$requiredPaths = @(
    'README.md',
    'CMakeLists.txt',
    'CMakePresets.json',
    'third_party/vcpkg.json',
    'docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md',
    'docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md',
    '.github/ISSUE_TEMPLATE/epic.md',
    '.github/ISSUE_TEMPLATE/backlog-item.md',
    '.github/ISSUE_TEMPLATE/docs-sync.md',
    '.github/ISSUE_TEMPLATE/config.yml',
    'tools/ci/Validate-Repo.ps1',
    'tools/ci/New-ReleaseBundle.ps1'
)

foreach ($path in $requiredPaths) {
    Assert-PathExists $path
}

$parentPrd = Get-Content 'docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md' -Raw
$backlogPrd = Get-Content 'docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md' -Raw
$readme = Get-Content 'README.md' -Raw
$cmakeLists = Get-Content 'CMakeLists.txt' -Raw

if ($parentPrd -notmatch 'XPE-PRD-003_PRD_Decomposition_and_Backlog\.md') {
    Add-RepoError 'Parent PRD does not reference the backlog decomposition document.'
}

if ($readme -notmatch 'XPE-PRD-002_Detailed_Project_Execution_PRD\.md') {
    Add-RepoError 'README does not reference the detailed execution PRD.'
}

if ($readme -notmatch 'XPE-PRD-003_PRD_Decomposition_and_Backlog\.md') {
    Add-RepoError 'README does not reference the backlog decomposition PRD.'
}

if ($cmakeLists -notmatch 'function\(xpe_add_optional_subdirectory') {
    Add-RepoError 'Top-level CMakeLists.txt is missing the optional subdirectory helper.'
}

$backlogMatches = [regex]::Matches($backlogPrd, 'BI-\d{2}\.\d{2}\.\d{2}')
if ($backlogMatches.Count -eq 0) {
    Add-RepoError 'No backlog identifiers were found in PRD-003.'
}

$draftFiles = Get-ChildItem '.github/issue-drafts' -File -Filter '*.md' -ErrorAction SilentlyContinue
foreach ($draft in $draftFiles) {
    $draftId = [System.IO.Path]::GetFileNameWithoutExtension($draft.Name)
    if ($backlogPrd -notmatch [regex]::Escape($draftId)) {
        Add-RepoError "Issue draft has no matching backlog item in PRD-003: $draftId"
    }
}

$requiredDraftIds = @('BI-00.01.01', 'BI-01.01.01', 'BI-03.01.01')
foreach ($draftId in $requiredDraftIds) {
    if (-not (Test-Path ".github/issue-drafts/$draftId.md")) {
        Add-RepoError "Missing required initial issue draft: $draftId"
    }
}

if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host "Repository validation failed with $($errors.Count) error(s)." -ForegroundColor Red
    exit 1
}

Write-Host ''
Write-Host 'Repository validation passed.' -ForegroundColor Green
