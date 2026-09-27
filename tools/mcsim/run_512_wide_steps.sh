#!/usr/bin/env bash
# #191 blind spot 1: does the boundary error converge to the wedge level once you
# are far enough from an edge?
#
# run_512_phantoms.sh answers everything EXCEPT distance. Its staircase is 10
# steps of 0.5 cm across a 5 cm field, so the farthest any pixel gets from a
# thickness boundary is half a step. The #180 analysis could show the error
# falling with distance but could not show where it lands, because the phantom
# has no pixels out at the distance where the answer would be.
#
# This run changes ONE parameter. Everything that was validated in
# run_512_phantoms.sh -- 0.14 mm product pitch, 5 cm field, the 77-pixel margin
# on each side where the scatter-only region lives, vox-x, launch count -- is
# kept byte-identical, so a difference between the two runs is the step width
# and nothing else.
#
#   --step-width 2.5 (was 0.5)
#     2*span/step = 2 steps, so one internal boundary at the field centre with
#     T_LO=5 cm on one side and T_HI=30 cm on the other. Distance from that
#     boundary now runs to 2.5 cm in phantom coordinates -- about 25 mm at the
#     detector -- against 2.5 mm before. The 4.5 mm blind spot is covered by a
#     factor of five, with room to see whether the curve flattens or keeps
#     falling.
#
# WHAT THIS RUN DOES NOT ANSWER, and must not be read as answering:
#   - Thickness dependence. Two levels (5 and 30 cm) instead of ten. The
#     10-step run stays the reference for how the error varies WITH thickness;
#     this one varies distance and holds thickness coarse. Reporting a
#     thickness trend from two points would be inventing one.
#   - Blind spots 2-4 of #191 (the sigma-1 explanation, REQ-GSVG-011
#     independence, REQ-GSVG-025 protection). None of them are about distance
#     and none are touched here.
#
# `air` and `wedge` are NOT re-run. `air` has no phantom in it, and `wedge` is
# unchanged by the step width -- both are reused from the phantoms512 export.
# Re-running them would spend an hour to reproduce two identical files.
#
# Cost: 300 launches x 21 s = about 1 h 45 m for the one `step` kind, measured
# from the same pilot the 10-step run was sized against (pilot_512.sh).
#
# After the run: analyze_512.py against the reused air/wedge, then report the
# error-vs-distance curve WITH the maximum boundary distance the run actually
# achieved. The point of the run is that number, so a report that omits it has
# not closed the blind spot.
set -eu
T=/mnt/d/workspace-github/xpe-pre/tools/mcsim
PY=/root/mcsim/venv/bin/python
OUT=/root/mcsim/phantoms512_wide
REUSE=/root/mcsim/phantoms512
mkdir -p "$OUT"

STEP_LAUNCHES=${STEP_LAUNCHES:-300}

# Fail early and loudly rather than after two hours: the reused inputs must be
# there before the long run starts, not discovered missing by the analysis.
for f in air_80kVp_512_primary.f32 air_80kVp_512_total.f32; do
    if [ ! -f "$REUSE/$f" ]; then
        echo "missing reusable input: $REUSE/$f" >&2
        echo "run run_512_phantoms.sh first, or point REUSE at its export" >&2
        exit 1
    fi
done

t0=$(date +%s)
"$PY" "$T/phantom_images.py" --kind step --launches "$STEP_LAUNCHES" \
    --histories 1e8 --pixels 512 --det-size 7.168 --field 5.0 \
    --vox-x 0.1 --span 2.5 --step-width 2.5 --out "$OUT" > "$OUT/step.log" 2>&1
t1=$(date +%s)
echo "[512-wide] step: $STEP_LAUNCHES launches, $((t1 - t0)) s"

cp "$REUSE"/air_80kVp_512_*.f32 "$OUT"/
echo "[512-wide] air reused from $REUSE"
echo "[512-wide] done -- next: analyze_512.py, and report the max boundary distance reached"
