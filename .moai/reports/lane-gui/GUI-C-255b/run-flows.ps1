$ErrorActionPreference = 'Stop'
Set-Location D:/workspace-github/xpe-gui
$run = (Get-Content .moai/reports/lane-gui/GUI-C-255b/current-run.txt -Raw).Trim()
$runner = 'clients/ImageProcTest.GuiObservation/bin/RelWithDebInfo/net8.0-windows/ImageProcTest.GuiObservation.dll'
$dotnet = 'C:/Program Files/dotnet/dotnet.exe'
$attempt = Join-Path $run ('flows-' + (Get-Date -Format HHmmss))
New-Item -ItemType Directory -Path $attempt | Out-Null
try {
    foreach ($scale in @(100,150)) {
        & $dotnet $runner --display-settings (Join-Path $attempt ('display-'+$scale+'.json')) $scale
        if ($LASTEXITCODE -ne 0) { throw "UIA scale selection exit=$LASTEXITCODE" }
        $dpi = if ($scale -eq 100) { 96 } else { 144 }
        foreach ($set in @('s0_gf','s1_um300')) {
            & $dotnet $runner D:/workspace-github/xpe-gui $run $set $dpi hook > (Join-Path $attempt ($set+'-'+$scale+'.log')) 2>&1
            Write-Output ($set+'_'+$scale+'_FLOW_EXIT='+$LASTEXITCODE)
        }
    }
} finally {
    & $dotnet $runner --display-settings (Join-Path $attempt display-restored-100.json) 100
    Write-Output ('RESTORE_SCALE_EXIT='+$LASTEXITCODE)
}
Write-Output ('RUN='+$run)
