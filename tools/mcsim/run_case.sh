#!/usr/bin/env bash
# Run one water-slab MC-GPU case end to end (QA-A-94, #180).
#
#   wsl.exe -d Ubuntu-24.04 -u root -- env REPEATS=10 bash \
#       /mnt/d/workspace-github/xpe-pre/tools/mcsim/run_case.sh \
#       <name> <broad|pencil|pcd> [gen_slab_input.py options...]
#
# mode pcd runs the photon-counting fork (make_mcgpu_pcd.sh); pass
# --pcd EMIN EMAX NBIN and a small detector (the whole detector is the ROI).
#
# Output stays in $WORK/runs/<name>: run.in, slab.vox, beam.spc,
# image_<i>.dat, mcgpu_<i>.log, gpu.csv, summary.json, time.txt.
#
# One MC-GPU launch is one CUDA kernel call. This GPU also drives a display,
# so a long call can trip the Windows GPU watchdog: keep --histories per
# launch small (1e8 finished in well under a second here) and use REPEATS
# with fresh seeds for more statistics.
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="$WORK/venv/bin/python"
BIN="$WORK/build/MC-GPU_v1.3.x"
PCD_BIN="$WORK/build_pcd/MC-GPU_v1.3_PCD.x"
CSI_TABLE="$WORK/MCGPU/materials/CesiumIodide__5-120keV.mcgpu.gz"
REPEATS="${REPEATS:-1}"
SEED0="${SEED0:-1234567890}"

name="$1"; mode="$2"; shift 2
[ "$mode" = pcd ] && BIN="$PCD_BIN"
out="$WORK/runs/$name"
rm -rf "$out"; mkdir -p "$out"

# GPU sampling is bounded from outside (timeout) and stopped on every exit path.
timeout 3600 nvidia-smi --query-gpu=timestamp,utilization.gpu,memory.used \
    --format=csv,noheader -lms 500 > "$out/gpu.csv" 2>/dev/null &
sampler=$!
trap 'kill "$sampler" 2>/dev/null || true' EXIT

start=$(date +%s.%N)
for i in $(seq 1 "$REPEATS"); do
  "$PY" "$HERE/gen_slab_input.py" --out "$out" --seed $((SEED0 + i)) "$@" > "$out/gen.txt"
  ( cd "$out" && "$BIN" run.in > "mcgpu_$i.log" 2>&1 ) && rc=0 || rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "MCGPU_EXIT=$rc (repeat $i)"; tail -20 "$out/mcgpu_$i.log"; exit "$rc"
  fi
  if [ "$mode" = pcd ]; then mv "$out/output" "$out/out_$i"; else mv "$out/image.dat" "$out/image_$i.dat"; fi
done
end=$(date +%s.%N)
kill "$sampler" 2>/dev/null || true

cat "$out/gen.txt"
echo "MCGPU_EXIT=0 repeats=$REPEATS"
awk -v s="$start" -v e="$end" 'BEGIN{printf "wall_s=%.1f\n", e-s}' | tee "$out/time.txt"
awk -F', ' '{gsub(/ %/,"",$2); gsub(/ MiB/,"",$3); if($2+0>u)u=$2+0; if($3+0>m)m=$3+0; n++}
            END{printf "gpu_samples=%d max_util_pct=%s max_mem_mib=%s\n", n, u, m}' \
    "$out/gpu.csv" | tee -a "$out/time.txt"
grep -h "Speed \[x-rays/s\]" "$out"/mcgpu_*.log | awk '{s+=$NF; n++} END{printf "mean_speed_xrays_per_s=%.3g over %d launches\n", s/n, n}' | tee -a "$out/time.txt"

if [ "$mode" = pcd ]; then
  "$PY" "$HERE/analyze_pcd.py" --csi-table "$CSI_TABLE" --csi-um "${CSI_UM:-600}" "$out"/out_* > "$out/summary.json"
else
  "$PY" "$HERE/analyze_image.py" "$out"/image_*.dat --mode "$mode" > "$out/summary.json"
fi
echo "summary: $out/summary.json"
