#!/usr/bin/env bash
# QA-A-114 (#180): cross-check the 512^2 phantom set against the [wet] curve and
# export it next to the 80^2 set. The suffix keeps the two geometries apart.
set -eu
T=/mnt/d/workspace-github/xpe-pre/tools/mcsim
PY=/root/mcsim/venv/bin/python
"$PY" "$T/export_phantoms.py" /root/mcsim/phantoms512 \
    "$T/tables/wet_water_csi600.csv" --name-suffix _512 --export "$T/phantoms"
