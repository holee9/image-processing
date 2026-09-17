/*
 * vector_types.h -- compatibility shim for building MC-GPU v1.3 (DIDSR/MCGPU)
 * with CUDA 12.9 (QA-A-94, #180).
 *
 * MC-GPU includes <vector_types.h> from the CUDA 5.0 samples "shared/inc"
 * directory, which no longer ships. Written for this project, not copied from
 * NVIDIA. The small vector structs (int2, float3, ...) are already provided by
 * the CUDA 12.9 headers that <cuda_runtime.h> pulls in, so this shim only adds
 * the __align__ attribute macro MC-GPU_v1.3.h uses on its structs.
 */
#ifndef XPE_MCSIM_VECTOR_TYPES_SHIM_H
#define XPE_MCSIM_VECTOR_TYPES_SHIM_H

#ifndef __align__
#define __align__(n) __attribute__((aligned(n)))
#endif

#endif
