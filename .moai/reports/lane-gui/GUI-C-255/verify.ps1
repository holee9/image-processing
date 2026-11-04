$ErrorActionPreference = 'Stop'
Set-Location D:/workspace-github/xpe-gui
$run = (Get-Content .moai/reports/lane-gui/GUI-C-255/current-run.txt -Raw).Trim()
$remaining = @()
foreach ($directory in Get-ChildItem $run -Directory | Where-Object Name -Like '*-uia3-*') {
    $text = Get-Content (Join-Path $directory.FullName flow.log) -Raw
    $match = [regex]::Match($text,'LAUNCH pid=(\d+)')
    $launchedPid = [int]$match.Groups[1].Value
    $alive = Get-Process -Id $launchedPid -ErrorAction SilentlyContinue
    if ($alive) { $remaining += $launchedPid }
}
Write-Output ('OWNED_GUI_REMAINING=' + $remaining.Count)
if ($remaining.Count) { Write-Output ('PIDS=' + ($remaining -join ',')); exit 1 }
Write-Output ('VERIFIED_AT=' + (Get-Date -Format o))
