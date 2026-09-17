#!/usr/bin/env bash
# Fetch MC-GPU v1.3 material files for water and air (QA-A-94, #180).
#
# DIDSR/MCGPU ships no water/air files and generating them needs the PENELOPE
# 2006 database. DIDSR/MCGPUv1.3_PCD (CC0-1.0, same MC-GPU v1.3 format) ships
# both; they are downloaded into $WORK/mat, never into the repository.
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
PCD_REPO="https://github.com/DIDSR/MCGPUv1.3_PCD"
PCD_COMMIT="${PCD_COMMIT:-}"

mkdir -p "$WORK/mat"
cd "$WORK/mat"
if [ -z "$PCD_COMMIT" ]; then
  PCD_COMMIT="$(git ls-remote "$PCD_REPO" HEAD | cut -f1)"
fi
echo "PCD_COMMIT=$PCD_COMMIT"
for f in water air; do
  wget -q "https://raw.githubusercontent.com/DIDSR/MCGPUv1.3_PCD/$PCD_COMMIT/Sample_Fan_Beam/inputs/$f.mcgpu" -O "$f.mcgpu"
  printf '%s lines=%s sha256=%s\n' "$f.mcgpu" "$(wc -l < "$f.mcgpu")" "$(sha256sum "$f.mcgpu" | cut -c1-16)"
done
