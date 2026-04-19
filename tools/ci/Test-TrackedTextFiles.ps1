Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$trackedFiles = git ls-files
$errors = New-Object System.Collections.Generic.List[string]

$extensionsWithWhitespaceCheck = @(
    '.c', '.cc', '.cpp', '.cxx',
    '.h', '.hh', '.hpp', '.hxx',
    '.ps1', '.psm1',
    '.yml', '.yaml',
    '.json',
    '.txt'
)

function Add-TextError {
    param([string]$Message)
    $script:errors.Add($Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
}

foreach ($relativePath in $trackedFiles) {
    if ([string]::IsNullOrWhiteSpace($relativePath)) {
        continue
    }

    $fullPath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path $fullPath)) {
        continue
    }

    $extension = [System.IO.Path]::GetExtension($fullPath).ToLowerInvariant()
    $fileName = [System.IO.Path]::GetFileName($fullPath)
    $checkTrailingWhitespace = $extensionsWithWhitespaceCheck -contains $extension -or $fileName -eq 'CMakeLists.txt'

    $lines = @(Get-Content -LiteralPath $fullPath)
    for ($index = 0; $index -lt $lines.Count; $index++) {
        $lineNumber = $index + 1
        $line = $lines[$index]

        if ($line -match '^<<<<<<< \S' -or $line -match '^={7}\s*$' -or $line -match '^>>>>>>> \S') {
            Add-TextError "${relativePath}:$lineNumber contains an unresolved merge conflict marker."
        }

        if ($checkTrailingWhitespace -and $line -match '[ \t]+$') {
            Add-TextError "${relativePath}:$lineNumber contains trailing whitespace."
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host "Tracked text file validation failed with $($errors.Count) error(s)." -ForegroundColor Red
    exit 1
}

Write-Host 'Tracked text file validation passed.' -ForegroundColor Green
