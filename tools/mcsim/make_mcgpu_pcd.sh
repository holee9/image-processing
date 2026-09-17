#!/usr/bin/env bash
# Build the photon-counting fork MCGPUv1.3_PCD_scatterMode (QA-A-95, #180).
#
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/make_mcgpu_pcd.sh
#
# The fork tallies photon COUNTS per pixel, per energy bin, separately for
# non-scattered / Compton / Rayleigh / multiple-scatter photons. With fine
# energy bins any detector response (counting, energy integrating, scintillator
# absorption) can be applied afterwards (see analyze_pcd.py).
# Same build recipe as make_mcgpu.sh; the clone itself is not modified.
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
CUDA_HOME="${CUDA_HOME:-/usr/local/cuda-12.9}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCD_COMMIT="${PCD_COMMIT:-e57bd50a0c51}"
GPU_ARCH="${GPU_ARCH:-sm_89}"

export PATH="$CUDA_HOME/bin:$PATH"
mkdir -p "$WORK"
cd "$WORK"
[ -d MCGPUv1.3_PCD_scatterMode ] || git clone -q https://github.com/DIDSR/MCGPUv1.3_PCD_scatterMode.git
git -C MCGPUv1.3_PCD_scatterMode checkout -q "$PCD_COMMIT"
if [ ! -d cuda-samples ]; then
  git clone -q --depth 1 --filter=blob:none --sparse https://github.com/NVIDIA/cuda-samples.git
  git -C cuda-samples sparse-checkout set Common
fi

rm -rf build_pcd && mkdir build_pcd
cp MCGPUv1.3_PCD_scatterMode/MC-GPU_v1.3_PCD.cu MCGPUv1.3_PCD_scatterMode/MC-GPU_kernel_v1.3_PCD.cu \
   MCGPUv1.3_PCD_scatterMode/MC-GPU_v1.3_PCD.h build_pcd/
cd build_pcd
nvcc MC-GPU_v1.3_PCD.cu -o MC-GPU_v1.3_PCD.x -O3 -DUSING_CUDA \
     -I. -I"$HERE/shim" -I"$WORK/cuda-samples/Common" \
     --gpu-architecture="$GPU_ARCH" -lz > "$WORK/build_pcd.log" 2>&1 && rc=0 || rc=$?
echo "BUILD_EXIT=$rc"
exit $rc
