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
    'cmake/Platform.cmake',
    'cmake/CompilerWarnings.cmake',
    'modules/common/CMakeLists.txt',
    'modules/common/include/xpe/common/xpe_common_api.h',
    'modules/common/include/xpe/common/xpe_error.h',
    'modules/common/include/xpe/common/xpe_memory.h',
    'modules/common/include/xpe/common/xpe_types.h',
    'modules/common/src/xpe_common.cpp',
    'modules/common/src/xpe_memory.cpp',
    'tests/CMakeLists.txt',
    'tests/common_smoke/CMakeLists.txt',
    'tests/common_smoke/test_common_smoke.cpp',
    'third_party/vcpkg.json',
    'third_party/common/vcpkg.json',
    'docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md',
    'docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md',
    '.github/ISSUE_TEMPLATE/epic.md',
    '.github/ISSUE_TEMPLATE/backlog-item.md',
    '.github/ISSUE_TEMPLATE/docs-sync.md',
    '.github/ISSUE_TEMPLATE/config.yml',
    '.github/dependabot.yml',
    '.github/workflows/ci.yml',
    '.github/workflows/codeql.yml',
    '.github/workflows/repository-guard.yml',
    '.github/workflows/windows-common-build.yml',
    '.github/workflows/delivery-bundle.yml',
    '.github/workflows/release-bundle.yml',
    'tools/ci/Validate-Repo.ps1',
    'tools/ci/Install-PinnedVcpkg.ps1',
    'tools/ci/Test-MarkdownLinks.ps1',
    'tools/ci/Test-TrackedTextFiles.ps1',
    'tools/ci/Use-MsvcDevShell.ps1',
    'tools/ci/New-ReleaseBundle.ps1'
)

foreach ($path in $requiredPaths) {
    Assert-PathExists $path
}

$parentPrd = Get-Content 'docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md' -Raw
$backlogPrd = Get-Content 'docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md' -Raw
$readme = Get-Content 'README.md' -Raw
$cmakeLists = Get-Content 'CMakeLists.txt' -Raw
$cmakePresets = Get-Content 'CMakePresets.json' -Raw
$typesHeader = Get-Content 'modules/common/include/xpe/common/xpe_types.h' -Raw
$fullManifest = Get-Content 'third_party/vcpkg.json' -Raw
$commonManifest = Get-Content 'third_party/common/vcpkg.json' -Raw
$guardWorkflow = Get-Content '.github/workflows/repository-guard.yml' -Raw
$buildWorkflow = Get-Content '.github/workflows/windows-common-build.yml' -Raw
$bundleWorkflow = Get-Content '.github/workflows/delivery-bundle.yml' -Raw
$releaseWorkflow = Get-Content '.github/workflows/release-bundle.yml' -Raw
$codeqlWorkflow = Get-Content '.github/workflows/codeql.yml' -Raw

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

if ($cmakePresets -notmatch '"name":\s*"ci-common"') {
    Add-RepoError 'CMakePresets.json is missing the ci-common preset.'
}

if ($cmakePresets -notmatch 'third_party/common') {
    Add-RepoError 'CMakePresets.json does not point ci-common to the lightweight common manifest.'
}

if ($cmakePresets -notmatch '"XPE_WARNINGS_AS_ERRORS"\s*:\s*"ON"') {
    Add-RepoError 'ci-common preset does not promote compiler warnings to errors.'
}

if ($fullManifest -notmatch '"builtin-baseline"\s*:\s*"[0-9a-fA-F]{40}"') {
    Add-RepoError 'third_party/vcpkg.json does not pin builtin-baseline to a full 40-character commit SHA.'
}

if ($commonManifest -notmatch '"builtin-baseline"\s*:\s*"[0-9a-fA-F]{40}"') {
    Add-RepoError 'third_party/common/vcpkg.json does not pin builtin-baseline to a full 40-character commit SHA.'
}

$flagMatches = [regex]::Matches($typesHeader, '#define\s+(XPE_FLAG_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+u)')
if ($flagMatches.Count -lt 10) {
    Add-RepoError 'xpe_types.h does not define the expected stable ABI flag set.'
}

$seenFlagValues = @{}
foreach ($match in $flagMatches) {
    $flagName = $match.Groups[1].Value
    $flagValue = $match.Groups[2].Value.ToLowerInvariant()
    if ($seenFlagValues.ContainsKey($flagValue)) {
        Add-RepoError "Duplicate XPE metadata flag value detected: $flagName and $($seenFlagValues[$flagValue]) both use $flagValue"
    } else {
        $seenFlagValues[$flagValue] = $flagName
    }
}

if ($guardWorkflow -notmatch 'actions/checkout@v6') {
    Add-RepoError 'Repository Guard is not pinned to actions/checkout@v6.'
}

if ($guardWorkflow -notmatch 'schedule:') {
    Add-RepoError 'Repository Guard is missing scheduled re-validation.'
}

if ($guardWorkflow -notmatch 'Test-TrackedTextFiles\.ps1') {
    Add-RepoError 'Repository Guard does not validate tracked text files.'
}

if ($buildWorkflow -notmatch 'actions/checkout@v6') {
    Add-RepoError 'Windows Common Build is not pinned to actions/checkout@v6.'
}

if ($buildWorkflow -notmatch 'actions/cache@v5') {
    Add-RepoError 'Windows Common Build is not pinned to actions/cache@v5.'
}

if ($buildWorkflow -notmatch 'actions/upload-artifact@v6') {
    Add-RepoError 'Windows Common Build is not pinned to actions/upload-artifact@v6.'
}

if ($buildWorkflow -notmatch 'ctest --test-dir') {
    Add-RepoError 'Windows Common Build does not run ctest smoke validation.'
}

if ($buildWorkflow -notmatch 'Install-PinnedVcpkg\.ps1') {
    Add-RepoError 'Windows Common Build does not pin vcpkg via Install-PinnedVcpkg.ps1.'
}

if ($buildWorkflow -notmatch 'Use-MsvcDevShell\.ps1') {
    Add-RepoError 'Windows Common Build does not configure the MSVC developer shell explicitly.'
}

if ($buildWorkflow -notmatch 'VCPKG_FORCE_SYSTEM_BINARIES') {
    Add-RepoError 'Windows Common Build does not force vcpkg to use runner-provided system build tools.'
}

if ($buildWorkflow -match 'git clone --depth 1 https://github.com/microsoft/vcpkg') {
    Add-RepoError 'Windows Common Build uses a shallow vcpkg clone that can break builtin-baseline resolution.'
}

if ($buildWorkflow -notmatch 'schedule:') {
    Add-RepoError 'Windows Common Build is missing scheduled cross-validation.'
}

if ($buildWorkflow -match 'if \(-not \(Test-Path \$env:VCPKG_ROOT\)\)') {
    Add-RepoError 'Windows Common Build still checks only the vcpkg root directory instead of bootstrap-vcpkg.bat presence.'
}

if ($bundleWorkflow -notmatch 'actions/checkout@v6') {
    Add-RepoError 'Delivery Bundle is not pinned to actions/checkout@v6.'
}

if ($bundleWorkflow -notmatch 'actions/upload-artifact@v6') {
    Add-RepoError 'Delivery Bundle is not pinned to actions/upload-artifact@v6.'
}

if ($releaseWorkflow -notmatch 'gh release create' -or $releaseWorkflow -notmatch 'gh release upload') {
    Add-RepoError 'Release Bundle workflow is missing GitHub Release publish/update logic.'
}

if ($codeqlWorkflow -notmatch 'github/codeql-action/init@v4') {
    Add-RepoError 'CodeQL workflow is missing github/codeql-action/init@v4.'
}

if ($codeqlWorkflow -notmatch 'github/codeql-action/analyze@v4') {
    Add-RepoError 'CodeQL workflow is missing github/codeql-action/analyze@v4.'
}

if ($codeqlWorkflow -notmatch 'security-events:\s*write') {
    Add-RepoError 'CodeQL workflow is missing security-events: write permission.'
}

if ($codeqlWorkflow -notmatch 'schedule:') {
    Add-RepoError 'CodeQL workflow is missing scheduled analysis.'
}

if ($codeqlWorkflow -notmatch 'Install-PinnedVcpkg\.ps1') {
    Add-RepoError 'CodeQL workflow does not pin vcpkg via Install-PinnedVcpkg.ps1.'
}

if ($codeqlWorkflow -notmatch 'Use-MsvcDevShell\.ps1') {
    Add-RepoError 'CodeQL workflow does not configure the MSVC developer shell explicitly.'
}

if ($codeqlWorkflow -notmatch 'VCPKG_FORCE_SYSTEM_BINARIES') {
    Add-RepoError 'CodeQL workflow does not force vcpkg to use runner-provided system build tools.'
}

if ($codeqlWorkflow -match 'if \(-not \(Test-Path \$env:VCPKG_ROOT\)\)') {
    Add-RepoError 'CodeQL workflow still checks only the vcpkg root directory instead of bootstrap-vcpkg.bat presence.'
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
