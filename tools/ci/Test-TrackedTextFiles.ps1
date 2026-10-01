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

# Source and config files must not carry C0 control characters other than tab/LF/CR, nor DEL. A raw U+0001
# once replaced the regex back-reference `\1` in a C# test (GUI-C-186c) and nothing caught it. Plain-text
# artifacts (.txt) are left out on purpose: text extracted from PDFs legitimately keeps form feeds.
$extensionsWithControlCharCheck = @(
    '.c', '.cc', '.cpp', '.cxx',
    '.h', '.hh', '.hpp', '.hxx',
    '.cs', '.xaml', '.csproj', '.props', '.targets',
    '.ps1', '.psm1',
    '.yml', '.yaml',
    '.json',
    '.cmake'
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
    $checkControlChars = $extensionsWithControlCharCheck -contains $extension -or $fileName -eq 'CMakeLists.txt'

    # Read as UTF-8 explicitly. Windows PowerShell 5.1 otherwise decodes a BOM-less file with the ANSI code page
    # (cp949 here), where the bytes of a Korean character can swallow a following space, so a trailing space after
    # Korean text passed locally and failed in CI under PowerShell 7 (UTF-8 by default).
    $lines = @(Get-Content -LiteralPath $fullPath -Encoding UTF8)
    for ($index = 0; $index -lt $lines.Count; $index++) {
        $lineNumber = $index + 1
        $line = $lines[$index]

        if ($line -match '^<<<<<<< \S' -or $line -match '^={7}\s*$' -or $line -match '^>>>>>>> \S') {
            Add-TextError "${relativePath}:$lineNumber contains an unresolved merge conflict marker."
        }

        if ($checkTrailingWhitespace -and $line -match '[ \t]+$') {
            Add-TextError "${relativePath}:$lineNumber contains trailing whitespace."
        }

        if ($checkControlChars -and $line -match '[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]') {
            $code = [int][char]$Matches[0]
            Add-TextError ("${relativePath}:$lineNumber contains control character U+{0:X4}." -f $code)
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host "Tracked text file validation failed with $($errors.Count) error(s)." -ForegroundColor Red
    exit 1
}

Write-Host 'Tracked text file validation passed.' -ForegroundColor Green
