$ErrorActionPreference = 'Stop'
Set-Location D:/workspace-github/xpe-gui
$run = (Get-Content .moai/reports/lane-gui/GUI-C-255/current-run.txt -Raw).Trim()
$runner = 'clients/ImageProcTest.GuiObservation/bin/RelWithDebInfo/net8.0-windows/ImageProcTest.GuiObservation.dll'
$dotnet = 'C:/Program Files/dotnet/dotnet.exe'
$scaleRun = Join-Path $run ('dpi-' + (Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Path $scaleRun | Out-Null
try {
    & $dotnet $runner --display-settings (Join-Path $scaleRun display-150.json) 150
    if ($LASTEXITCODE -ne 0) { throw "UIA scale selection exit=$LASTEXITCODE" }
    foreach ($set in @('s0_gf','s1_um300')) {
        & $dotnet $runner D:/workspace-github/xpe-gui $run $set 144 > (Join-Path $scaleRun ($set+'-150-console.log')) 2>&1
        Write-Output ($set + '_150_FLOW_EXIT=' + $LASTEXITCODE)
    }
} finally {
    & $dotnet $runner --display-settings (Join-Path $scaleRun display-restored-100.json) 100
    Write-Output ('RESTORE_SCALE_EXIT=' + $LASTEXITCODE)
}
