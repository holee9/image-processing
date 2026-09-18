#!/usr/bin/env bash
# QA-A-114 (#180): the 512^2 product-pitch phantom set.
#
# Geometry, and why each number:
#   pixels 512, det 7.168 cm  -> 0.14 mm pitch, the pitch the user decided on.
#                                A 4 mm pitch at 512^2 would be a 2 m object.
#   field 5.0 cm              -> 357 pixels across (>= the 320 the post lane's
#                                calculation asks for), and the boundary sits
#                                inside the image with 77 pixels of margin on
#                                each side, which is where the scatter-only
#                                region the card asks for lives.
#   span 2.5, step 0.5        -> the QA-A-98 staircase re-scaled to fit inside
#                                the field: 10 steps of 5..30 cm water. At the
#                                original 5 cm step width every step would fall
#                                outside a 5 cm field.
#   vox-x 0.1                 -> 5 voxels per step; the 0.5 cm voxel of QA-A-98
#                                would make one voxel per step.
#
# Launch counts are a measurement, not a guess: one launch of 1e8 histories
# takes 21 s at this geometry (measured, pilot_512.sh), and the per-pixel
# primary count at 1e8 has a median of 8 in the field -- 35% noise. 300 launches
# brings that to about 2%, which is the level QA-A-98 reported. `air` needs
# fewer because nothing attenuates it.
set -eu
T=/mnt/d/workspace-github/xpe-pre/tools/mcsim
PY=/root/mcsim/venv/bin/python
OUT=/root/mcsim/phantoms512
mkdir -p "$OUT"

STEP_LAUNCHES=${STEP_LAUNCHES:-300}
AIR_LAUNCHES=${AIR_LAUNCHES:-100}

run_one() {
    local kind=$1 launches=$2
    local t0 t1
    t0=$(date +%s)
    "$PY" "$T/phantom_images.py" --kind "$kind" --launches "$launches" \
        --histories 1e8 --pixels 512 --det-size 7.168 --field 5.0 \
        --vox-x 0.1 --span 2.5 --step-width 0.5 --out "$OUT" > "$OUT/$kind.log" 2>&1
    t1=$(date +%s)
    echo "[512] $kind: $launches launches, $((t1 - t0)) s"
}

run_one step "$STEP_LAUNCHES"
run_one wedge "$STEP_LAUNCHES"
run_one air "$AIR_LAUNCHES"
echo "[512] done"
