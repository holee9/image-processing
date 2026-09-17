#!/usr/bin/env bash
# Why is the PSF-sum SPR lower than a direct broad beam? (QA-A-97, #180)
#
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/run_psf_vs_broad.sh [step...]
#
# Steps (default: all), each changes one thing against the A-96 setup
# (SDD 100 cm, air gap 2 cm, 2.5 mm Al, slab 60 cm, CsI 600 um, 5-150 keV tables):
#   sdd      divergence: PSF sum and direct broad beam at SDD 100/200/400/1000/3000 cm
#   radius   truncation: PSF sum on a 100 cm coarse detector
#   slab     slab edge:  direct broad beam and PSF with a 120 cm slab
#   pixel    pixel integration: PSF sum with 2.5 mm coarse pixels
#   offaxis  off-axis pencils tilted to hit the detector 0-21.2 cm from the axis (OFFAXIS_D)
# Results: $WORK/qa97/<step>/<case>/ (psf.json or summary.json).
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="$WORK/venv/bin/python"
OUT="$WORK/qa97"
STEPS="${*:-sdd radius slab pixel offaxis}"
CASES=("10 100" "20 80")
MIN_FREE_MB=1500

wait_mem() {
  local free
  free=$(powershell.exe -NoProfile -Command \
    "[int]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024)" 2>/dev/null | tr -dc '0-9')
  free=${free:-0}
  while [ "$free" -lt "$MIN_FREE_MB" ]; do
    echo "waiting: Windows free ${free} MB"; sleep 30
    free=$(powershell.exe -NoProfile -Command \
      "[int]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024)" 2>/dev/null | tr -dc '0-9')
    free=${free:-0}
  done
  printf '[win_free_mb=%s] ' "$free"
}

psf() {   # psf <dir> <T> <kVp> [psf_case args...]
  local d="$1" t="$2" k="$3"; shift 3
  wait_mem; printf 'psf %s ' "$d"
  "$PY" "$HERE/psf_case.py" --out "$OUT/$d" --thickness "$t" --kvp "$k" --repeats 5 --no-fine "$@"
}

broad() { # broad <dir> <T> <kVp> [gen_slab_input args...]
  local d="$1" t="$2" k="$3"; shift 3
  wait_mem; echo "broad $d"
  WORK="$WORK" REPEATS=10 SEED0=555000000 bash "$HERE/run_case.sh" "../qa97/$d" pcd \
      --pcd 0 125000 50 --det-size 2 --pixels 10 --histories 1e8 --air-gap 2 --al 2.5 \
      --thickness "$t" --kvp "$k" --field 30 --mat-dir "$WORK/mat150" "$@" | grep -E 'MCGPU_EXIT'
}

for step in $STEPS; do
  for c in "${CASES[@]}"; do
    set -- $c; t="$1"; k="$2"; tag="t${t}_k${k}"
    case "$step" in
      sdd)
        for s in 100 200 400 1000 3000; do
          psf   "sdd/${tag}_sdd${s}_psf"   "$t" "$k" --sdd "$s"
          broad "sdd/${tag}_sdd${s}_broad" "$t" "$k" --sdd "$s"
        done ;;
      radius)
        psf "radius/${tag}_det100" "$t" "$k" --coarse-size 100 --coarse-pixels 200 ;;
      slab)
        psf   "slab/${tag}_slab120_psf"   "$t" "$k" --slab-xz 120
        broad "slab/${tag}_slab120_broad" "$t" "$k" --slab-xz 120 ;;
      pixel)
        psf "pixel/${tag}_px2.5mm" "$t" "$k" --coarse-pixels 240 ;;
      offaxis)
        # tilt in x so the pencil meets the (tilted) detector centre; sin = d / sqrt(d^2 + SDD^2)
        for d in ${OFFAXIS_D:-0 2.5 5 7.5 10 12.5 15 17.5 21.2}; do
          sx=$(awk -v d="$d" 'BEGIN{printf "%.8f", d / sqrt(d*d + 100*100)}')
          psf "offaxis/${tag}_d${d}" "$t" "$k" --source-dir "$sx" 0
        done ;;
      *) echo "unknown step $step"; exit 2 ;;
    esac
  done
done
