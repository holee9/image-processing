#!/usr/bin/env bash
# QA-A-114 (#180): one-launch timing pilot for the 512^2 product-pitch phantom.
# Measures wall time for 1e8 histories so the launch count can be chosen from a
# measurement rather than a guess.
set -eu
T=/mnt/d/workspace-github/xpe-pre/tools/mcsim
PY=/root/mcsim/venv/bin/python
OUT=/root/mcsim/pilot512
mkdir -p "$OUT"

PIX=${PIX:-512}
DET=${DET:-7.168}          # 512 * 0.014 cm = 0.14 mm pitch
FIELD=${FIELD:-5.0}        # 5.0 / 0.014 = 357 pixels across, boundary inside the image
HIST=${HIST:-1e8}
KIND=${KIND:-step}

start=$(date +%s)
"$PY" "$T/phantom_images.py" --kind "$KIND" --launches 1 --histories "$HIST" \
    --pixels "$PIX" --det-size "$DET" --field "$FIELD" \
    --vox-x 0.1 --span 2.5 --step-width 0.5 \
    --out "$OUT" 2>&1 | tail -5
end=$(date +%s)
echo "[pilot] kind=$KIND pixels=$PIX det=$DET field=$FIELD histories=$HIST"
echo "[pilot] wall seconds: $((end - start))"
