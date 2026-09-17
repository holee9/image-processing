#!/usr/bin/env bash
# Build MC-GPU v1.3 (DIDSR/MCGPU) in WSL2 Ubuntu with CUDA 12.9 (QA-A-94, #180).
#
# Usage (from Windows):
#   wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/d/workspace-github/xpe-pre/tools/mcsim/make_mcgpu.sh
#
# Everything is built under $WORK (default /root/mcsim); nothing is written to
# the repository. External sources are cloned, never copied into the repo.
set -euo pipefail

WORK="${WORK:-/root/mcsim}"
CUDA_HOME="${CUDA_HOME:-/usr/local/cuda-12.9}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MCGPU_COMMIT="${MCGPU_COMMIT:-cb16a5f52661}"
GPU_ARCH="${GPU_ARCH:-sm_89}"          # RTX 4070 Ti, compute capability 8.9

export PATH="$CUDA_HOME/bin:$PATH"
mkdir -p "$WORK"
cd "$WORK"

[ -d MCGPU ] || git clone -q https://github.com/DIDSR/MCGPU.git
git -C MCGPU checkout -q "$MCGPU_COMMIT"

# helper_cuda.h / helper_functions.h (BSD-3-Clause, NVIDIA/cuda-samples, Common/)
if [ ! -d cuda-samples ]; then
  git clone -q --depth 1 --filter=blob:none --sparse https://github.com/NVIDIA/cuda-samples.git
  git -C cuda-samples sparse-checkout set Common
fi

# Build from a copy of the unmodified sources; the clone itself stays pristine.
rm -rf build && mkdir build
cp MCGPU/MC-GPU_v1.3.cu MCGPU/MC-GPU_kernel_v1.3.cu MCGPU/MC-GPU_v1.3.h build/
cd build

nvcc MC-GPU_v1.3.cu -o MC-GPU_v1.3.x -O3 -DUSING_CUDA \
     -I. -I"$HERE/shim" -I"$WORK/cuda-samples/Common" \
     --gpu-architecture="$GPU_ARCH" -lz > "$WORK/build.log" 2>&1 && rc=0 || rc=$?
echo "BUILD_EXIT=$rc"
exit $rc
