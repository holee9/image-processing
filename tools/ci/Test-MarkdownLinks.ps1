Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$patterns = @(
    'README.md',
    'docs/post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md',
    'docs/post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md',
    '.github/ISSUE_TEMPLATE/*.md',
    '.github/issue-drafts/*.md'
)

$files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
foreach ($pattern in $patterns) {
    Get-ChildItem -Path $pattern -File | ForEach-Object { $files.Add($_) }
}

$errors = New-Object System.Collections.Generic.List[string]

function Add-LinkError {
    param([string]$Message)
    $script:errors.Add($Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
}

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    $matches = [regex]::Matches($content, '\[[^\]]+\]\(([^)]+)\)')

    foreach ($match in $matches) {
        $rawTarget = $match.Groups[1].Value.Trim()
        if ([string]::IsNullOrWhiteSpace($rawTarget)) {
            continue
        }

        if ($rawTarget.StartsWith('<') -and $rawTarget.EndsWith('>')) {
            $rawTarget = $rawTarget.Substring(1, $rawTarget.Length - 2)
        }

        if ($rawTarget.StartsWith('#') -or
            $rawTarget.StartsWith('http://') -or
            $rawTarget.StartsWith('https://') -or
            $rawTarget.StartsWith('mailto:') -or
            $rawTarget.StartsWith('app://')) {
            continue
        }

        $relativeTarget = $rawTarget.Split('#')[0]
        if ([string]::IsNullOrWhiteSpace($relativeTarget)) {
            continue
        }

        $resolvedTarget = [System.IO.Path]::GetFullPath((Join-Path $file.DirectoryName $relativeTarget))
        if (-not (Test-Path $resolvedTarget)) {
            Add-LinkError "$($file.FullName) references a missing target: $rawTarget"
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host "Markdown link validation failed with $($errors.Count) error(s)." -ForegroundColor Red
    exit 1
}

Write-Host 'Markdown link validation passed.' -ForegroundColor Green
