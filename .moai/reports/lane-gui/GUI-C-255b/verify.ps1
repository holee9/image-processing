$ErrorActionPreference = 'Stop'
Set-Location D:/workspace-github/xpe-gui
$run = (Get-Content .moai/reports/lane-gui/GUI-C-255b/current-run.txt -Raw).Trim()
$summary = Get-Content (Join-Path $run final-evidence-summary.json) -Raw | ConvertFrom-Json
$remaining = @()
foreach ($flow in $summary.flows) {
    $alive = Get-Process -Id $flow.ownPid -ErrorAction SilentlyContinue
    if ($alive) { $remaining += $flow.ownPid }
}
Write-Output ('OWNED_GUI_REMAINING=' + $remaining.Count)
Write-Output ('VERIFIED_AT=' + (Get-Date -Format o))
if ($remaining.Count) { Write-Output ('PIDS=' + ($remaining -join ',')); exit 1 }
