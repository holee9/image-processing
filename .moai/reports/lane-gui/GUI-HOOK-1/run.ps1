param([ValidateSet('before', 'after', 'format', 'format-export', 'build', 'commit')][string]$Step,
      [ValidateSet('commit.log', 'commit-evidence.log')][string]$CommitLog = 'commit.log')
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new()
$root = 'D:\workspace-github\xpe-gui'
Set-Location $root
$env:PATH = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:PATH
$env:CLAUDE_PROJECT_DIR = $root
if ($env:SKIP_MOAI_PRECOMMIT) { throw 'Bypass variable present; refusing execution.' }
$report = Join-Path $root '.moai/reports/lane-gui/GUI-HOOK-1'
$moai = Get-Command moai -CommandType Application -ErrorAction Stop
$dotnet = Get-Command dotnet -CommandType Application -ErrorAction Stop
$git = 'C:\Program Files\Git\cmd\git.exe'
Write-Output ('MOAI=' + $moai.Source)
Write-Output ('DOTNET=' + $dotnet.Source)
switch ($Step) {
    { $_ -in 'before', 'after' } {
        & $moai.Source gate 2>&1 | Tee-Object -FilePath (Join-Path $report ('gate-' + $Step + '.log'))
        $result = $LASTEXITCODE
    }
    'format' {
        & $dotnet.Source format whitespace clients/ImageProcTest/ImageProcTest.csproj --no-restore --include clients/ImageProcTest/Diagnostics/NativeDependencyLoader.cs clients/ImageProcTest/Services/NativePreprocessPreviewService.cs 2>&1 | Tee-Object -FilePath (Join-Path $report 'format.log')
        $result = $LASTEXITCODE
    }
    'build' {
        & $dotnet.Source build clients/ImageProcTest/ImageProcTest.csproj -c Release --no-restore 2>&1 | Tee-Object -FilePath (Join-Path $report 'build.log')
        $result = $LASTEXITCODE
    }
    'format-export' {
        & $dotnet.Source format whitespace clients/ImageProcTest/ImageProcTest.csproj --no-restore --include clients/ImageProcTest/Services/NativePresentationExportService.cs 2>&1 | Tee-Object -FilePath (Join-Path $report 'format-export.log')
        $result = $LASTEXITCODE
    }
    'commit' {
        & $git commit -F .moai/reports/lane-gui/GUI-HOOK-1/commit-message.txt -- clients/ImageProcTest/Diagnostics/NativeDependencyLoader.cs clients/ImageProcTest/Services/NativePreprocessPreviewService.cs clients/ImageProcTest/Services/NativePresentationExportService.cs .moai/reports/lane-gui/GUI-HOOK-1 2>&1 | Tee-Object -FilePath (Join-Path $report $CommitLog)
        $result = $LASTEXITCODE
    }
}
Write-Output ('EXIT=' + $result)
exit $result
