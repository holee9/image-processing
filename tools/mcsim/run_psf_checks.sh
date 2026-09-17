#!/usr/bin/env bash
# Extra runs that check the PSF grid (QA-A-96, #180). Results go outside the
# grid directory so they never change the table.
#
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/run_psf_checks.sh
#
#  1. re-run the grid points flagged by check_kernels.py with 20 repeats and
#     different seeds -> $WORK/psf_rerun/t<T>_k<kVp>
#  2. direct broad-beam runs (30 x 30 cm at the detector, CsI response) for
#     10 cm / 100 kVp and 20 cm / 80 kVp -> $WORK/runs/QA96_broad_*
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="$WORK/venv/bin/python"
FLAGGED="${FLAGGED:-25:70 10:90 10:60 10:120 30:80 10:80}"

for p in $FLAGGED; do
  t="${p%%:*}"; k="${p##*:}"
  printf 'rerun t=%s k=%s ' "$t" "$k"
  "$PY" "$HERE/psf_case.py" --out "$WORK/psf_rerun/t${t}_k${k}" --thickness "$t" --kvp "$k" \
      --repeats 20 --seed0 987654321
done

for case in "10 100" "20 80"; do
  set -- $case
  REPEATS=10 SEED0=555000000 bash "$HERE/run_case.sh" "QA96_broad_t${1}_k${2}" pcd \
      --pcd 0 125000 50 --det-size 2 --pixels 10 --histories 1e8 --sdd 100 --air-gap 2 --al 2.5 \
      --thickness "$1" --kvp "$2" --field 30 --mat-dir "$WORK/mat150" | grep -E 'wrote|MCGPU_EXIT'
done
