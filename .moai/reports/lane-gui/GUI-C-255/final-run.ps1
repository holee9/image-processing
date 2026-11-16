$ErrorActionPreference = 'Stop'
Set-Location D:/workspace-github/xpe-gui
$run = Join-Path $PWD ('.moai/reports/lane-gui/GUI-C-255/run-' + (Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Path $run | Out-Null
$run | Set-Content -Encoding UTF8 .moai/reports/lane-gui/GUI-C-255/current-run.txt
$runner = 'clients/ImageProcTest.GuiObservation/bin/RelWithDebInfo/net8.0-windows/ImageProcTest.GuiObservation.dll'
$dotnet = 'C:/Program Files/dotnet/dotnet.exe'
try {
    & $dotnet $runner --display-settings (Join-Path $run display-100.json) 100
    if ($LASTEXITCODE -ne 0) { throw "UIA 100% scale selection exit=$LASTEXITCODE" }
    foreach ($set in @('s0_gf','s1_um300')) {
        & $dotnet $runner D:/workspace-github/xpe-gui $run $set 96 > (Join-Path $run ($set+'-100-console.log')) 2>&1
        Write-Output ($set + '_100_FLOW_EXIT=' + $LASTEXITCODE)
    }
    & $dotnet $runner --display-settings (Join-Path $run display-150.json) 150
    if ($LASTEXITCODE -ne 0) { throw "UIA 150% scale selection exit=$LASTEXITCODE" }
    foreach ($set in @('s0_gf','s1_um300')) {
        & $dotnet $runner D:/workspace-github/xpe-gui $run $set 144 > (Join-Path $run ($set+'-150-console.log')) 2>&1
        Write-Output ($set + '_150_FLOW_EXIT=' + $LASTEXITCODE)
    }
} finally {
    & $dotnet $runner --display-settings (Join-Path $run display-restored-100.json) 100
    Write-Output ('RESTORE_SCALE_EXIT=' + $LASTEXITCODE)
}
Write-Output ('RUN=' + $run)
