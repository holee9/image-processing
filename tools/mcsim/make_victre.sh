#!/usr/bin/env bash
# Build VICTRE MC-GPU v1.5b (DIDSR/VICTRE_MCGPU) for the anti-scatter grid
# tables (QA-A-99, #180). Same recipe as make_mcgpu.sh; the clone is not modified.
#
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/make_victre.sh
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
CUDA_HOME="${CUDA_HOME:-/usr/local/cuda-12.9}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VICTRE_COMMIT="${VICTRE_COMMIT:-30b5e66cf547}"
GPU_ARCH="${GPU_ARCH:-sm_89}"

export PATH="$CUDA_HOME/bin:$PATH"
mkdir -p "$WORK"
cd "$WORK"
[ -d VICTRE_MCGPU ] || git clone -q https://github.com/DIDSR/VICTRE_MCGPU.git
git -C VICTRE_MCGPU checkout -q "$VICTRE_COMMIT"
if [ ! -d cuda-samples ]; then
  git clone -q --depth 1 --filter=blob:none --sparse https://github.com/NVIDIA/cuda-samples.git
  git -C cuda-samples sparse-checkout set Common
fi

rm -rf build_victre && mkdir build_victre
cp VICTRE_MCGPU/MC-GPU_v1.5b.cu VICTRE_MCGPU/MC-GPU_kernel_v1.5b.cu VICTRE_MCGPU/MC-GPU_v1.5b.h \
   VICTRE_MCGPU/load_voxels_binary_VICTRE_v1.5b.c build_victre/
cd build_victre
nvcc MC-GPU_v1.5b.cu -o MC-GPU_v1.5b.x -O3 \
     -I. -I"$HERE/shim" -I"$WORK/cuda-samples/Common" \
     --gpu-architecture="$GPU_ARCH" -lz > "$WORK/build_victre.log" 2>&1 && rc=0 || rc=$?
echo "BUILD_EXIT=$rc"
exit $rc
