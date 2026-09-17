#!/usr/bin/env bash
# Pencil-beam PSF grid: water thickness x kVp (QA-A-96, #180).
#
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/run_psf_grid.sh "5 10" "60 70 80"
#
# Arguments: thickness list (cm), kVp list. Defaults are the full grid.
# Each case goes to $WORK/psf/t<T>_k<kVp>/ (psf.npz, psf.json). Before every
# case the Windows free memory is read through WSL interop; below 1.5 GB the
# script waits.
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="$WORK/venv/bin/python"
THICK="${1:-5 10 15 20 25 30}"
KVPS="${2:-60 70 80 90 100 110 120}"
REPEATS="${REPEATS:-5}"
MIN_FREE_MB=1500

# 5-150 keV water/air tables from the PCD fork (CC0): the 5-120 keV tables of
# fetch_materials.sh reject a 120 kVp spectrum.
PCDMAT="$WORK/MCGPUv1.3_PCD_scatterMode/materialFiles/MCGPUFiles"
mkdir -p "$WORK/mat150"
ln -sf "$PCDMAT/waterMIF_5_150_keV.mcgpu" "$WORK/mat150/water.mcgpu"
ln -sf "$PCDMAT/air_5_150_keV.mcgpu" "$WORK/mat150/air.mcgpu"

win_free_mb() {
  powershell.exe -NoProfile -Command \
    "[int]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024)" 2>/dev/null | tr -dc '0-9'
}

for t in $THICK; do
  for k in $KVPS; do
    free=$(win_free_mb); free=${free:-0}
    while [ "$free" -lt "$MIN_FREE_MB" ]; do
      echo "waiting: Windows free ${free} MB < ${MIN_FREE_MB} MB"; sleep 30
      free=$(win_free_mb); free=${free:-0}
    done
    printf 't=%s k=%s win_free_mb=%s ' "$t" "$k" "$free"
    "$PY" "$HERE/psf_case.py" --out "$WORK/psf/t${t}_k${k}" --thickness "$t" --kvp "$k" --repeats "$REPEATS"
  done
done
