#!/usr/bin/env bash
# One-variable-at-a-time SPR sensitivity study (QA-A-95, #180).
#
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/run_sensitivity.sh
#
# Needs make_mcgpu.sh, make_mcgpu_pcd.sh and fetch_materials.sh to have run.
# Every case uses the photon-counting fork with 1 keV bins on a 2 x 2 cm
# central detector, so one run yields SPR for three detector responses
# (ideal energy, ideal counting, CsI 600 um). Nothing here changes a default.
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MAT="$WORK/mat"
PCDMAT="$WORK/MCGPUv1.3_PCD_scatterMode/materialFiles/MCGPUFiles"

# Extra slab materials shipped with the PCD fork (CC0-1.0).
ln -sf "$PCDMAT/waterMIF_5_150_keV.mcgpu" "$MAT/waterMIF.mcgpu"
ln -sf "$PCDMAT/luciteMIF_5_150_keV.mcgpu" "$MAT/lucite.mcgpu"

COMMON=(--pcd 0 125000 125 --det-size 2 --pixels 10 --histories 1e8 --sdd 100)

run() {  # run <name> <repeats> <args...>
  local name="$1" reps="$2"; shift 2
  local free_mb
  free_mb=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
  echo "== $name (repeats=$reps, WSL MemAvailable=${free_mb} MB)"
  REPEATS="$reps" bash "$HERE/run_case.sh" "$name" pcd "${COMMON[@]}" "$@" | grep -E 'wrote|MCGPU_EXIT|wall_s'
}

variants() {  # variants <prefix> <repeats> <base args...>
  local p="$1" r="$2"; shift 2
  run "${p}_base"       "$r" "$@"
  run "${p}_gap0"       "$r" "$@" --air-gap 0
  run "${p}_gap10"      "$r" "$@" --air-gap 10
  run "${p}_fieldEntr"  "$r" "$@" --field-at entrance
  run "${p}_al1.5"      "$r" "$@" --al 1.5
  run "${p}_al4.0"      "$r" "$@" --al 4.0
  run "${p}_waterMIF"   "$r" "$@" --mat waterMIF.mcgpu
  run "${p}_rho1.03"    "$r" "$@" --density 1.03
  run "${p}_pmma"       "$r" "$@" --mat lucite.mcgpu --density 1.19
  run "${p}_slab30"     "$r" "$@" --slab-xz 30
}

# B: A-94 comparison case. Later "--air-gap"/"--al"/... override these values.
variants B 5  --thickness 10 --kvp 104 --field 30 --air-gap 2 --al 2.5
# A: same variables at 20 cm / 80 kVp (direction check).
variants A 10 --thickness 20 --kvp 80  --field 30 --air-gap 2 --al 2.5

# IAEA Diagnostic Radiology Physics (2014) Table 6.1: 30 x 30 cm PMMA, 20 cm,
# 80 kV, 3 mm Al total, FID 100 cm, OID 5 cm, 30 x 30 cm field at receptor.
run IAEA_pmma20_80kV 10 --thickness 20 --kvp 80 --field 30 --air-gap 5 --al 3.0 \
    --slab-xz 30 --mat lucite.mcgpu --density 1.19

# Combination check (not a default): every variable above that raised SPR in
# series B, applied together -- air gap 0, field at slab entrance, 1.03 g/cm^3.
# Read the counting / CsI columns for the detector part.
run B_combo_up 5 --thickness 10 --kvp 104 --field 30 --air-gap 0 --al 2.5 \
    --field-at entrance --density 1.03
