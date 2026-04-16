# SOUP List & Analysis

**Document ID:** XPE-SOUP-001 v1.0  
**IEC 62304 Clause:** 5.3.3, 5.3.4, 7.4.1 — 7.4.3  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

XPE에 사용되는 모든 SOUP(Software of Unknown Provenance)를 식별하고, 기능/성능 요구사항, 시스템 요구사항, 위험 분석을 문서화한다.

## 2. SOUP Inventory

| ID | Name | Version | Purpose | License | SW Item | Safety Class |
|----|------|---------|---------|---------|---------|:----------:|
| SOUP-001 | OpenCV | 4.9.x | Image processing primitives | Apache 2.0 | SWI-2 | B |
| SOUP-002 | dcmtk | 3.6.8 | DICOM read/write/network | BSD-3 | SWI-4 | B |
| SOUP-003 | ONNX Runtime | 1.17.x | DL model inference | MIT | SWI-2 | B |
| SOUP-004 | spdlog | 1.13.x | Logging framework | MIT | SWI-5 | A |
| SOUP-005 | nlohmann/json | 3.11.x | JSON config parsing | MIT | SWI-5 | A |
| SOUP-006 | Google Test | 1.14.x | Unit testing (dev only) | BSD-3 | — | N/A |
| SOUP-007 | fmt | 10.x | String formatting | MIT | SWI-5 | A |
| SOUP-008 | Eigen | 3.4.x | Matrix operations | MPL-2.0 | SWI-2 | B |

## 3. Functional & Performance Requirements (5.3.3)

| SOUP ID | Functional Requirements | Performance Requirements |
|---------|------------------------|------------------------|
| SOUP-001 | cv::bilateralFilter(정확한 edge-preserving), cv::CLAHE(block histogram+clip+redistribute), cv::pyrDown/pyrUp(Laplacian pyramid), cv::resize(bilinear/bicubic) | 3072×3072 bilateral ≤ 200ms, CLAHE ≤ 150ms, pyrDown 12-level ≤ 300ms |
| SOUP-002 | DcmFileFormat read/write, DcmDataset tag manipulation, DcmSCU C-STORE/C-FIND, JPEG 2000 lossless codec (OpenJPEG backend), 모든 DX IOD Type 1/2 tag 지원 | DICOM file write ≤ 1s (uncompressed 3072×3072), J2K encode ≤ 3s |
| SOUP-003 | ONNX model load (Residual U-Net ~50M params), CreateSession, Run inference, GPU provider (CUDA EP) + CPU fallback, float32 input/output | Inference ≤ 2s (RTX 3060), ≤ 10s (CPU-only), model load ≤ 5s |
| SOUP-004 | spdlog::info/warn/error, rotating file sink, async logger | Log write ≤ 1μs (async mode) |
| SOUP-005 | JSON parse/serialize, nested object, array | Parse 1MB config ≤ 10ms |
| SOUP-007 | fmt::format string formatting | Negligible overhead |
| SOUP-008 | Eigen::MatrixXf operations, FFT (via unsupported module), decomposition | 3072×3072 matrix multiply ≤ 500ms |

## 4. System Hardware & Software Requirements (5.3.4)

| SOUP ID | OS | CPU Arch | GPU | Dependencies | Disk |
|---------|----|---------:|-----|-------------|------|
| SOUP-001 | Windows 11, Ubuntu 24.04 | x86-64 (AVX2), ARM64 (NEON) | — (CPU only) | — | ~50MB |
| SOUP-002 | Windows 11, Ubuntu 24.04 | x86-64, ARM64 | — | OpenSSL ≥ 1.1 (TLS) | ~30MB |
| SOUP-003 | Windows 11, Ubuntu 24.04 | x86-64, ARM64 | NVIDIA CUDA 12+ (optional) | CUDA Toolkit 12.x (optional) | ~200MB |
| SOUP-004 | Cross-platform | Any | — | — | ~1MB |
| SOUP-005 | Cross-platform | Any | — | — | Header-only |
| SOUP-007 | Cross-platform | Any | — | — | ~1MB |
| SOUP-008 | Cross-platform | Any (SIMD auto-detect) | — | — | Header-only |

## 5. SOUP Risk Analysis (7.4)

### 5.1 Published Anomaly Lists

| SOUP ID | Anomaly Source | Known Relevant Issues | Mitigation |
|---------|---------------|----------------------|-----------|
| SOUP-001 | github.com/opencv/opencv/issues | CLAHE boundary artifact (fixed 4.8+) | Version ≥ 4.9 사용 |
| SOUP-002 | github.com/DCMTK/dcmtk/issues | J2K codec edge case (rare) | DVTk conformance test |
| SOUP-003 | github.com/microsoft/onnxruntime/issues | CUDA EP memory leak (fixed 1.16+) | Version ≥ 1.17 사용 |
| SOUP-008 | gitlab.com/libeigen/eigen/-/issues | Numerical precision at extreme scale | Condition number check |

### 5.2 Failure Mode Analysis

| SOUP ID | Failure Mode | Effect on XPE | Severity | Prob | Risk | Control |
|---------|-------------|---------------|:--------:|:----:|:----:|---------|
| SOUP-001 | bilateralFilter: incorrect output | Noise visible or detail lost | Minor | Remote | Low | Output PSNR vs reference ≥ 30dB |
| SOUP-001 | CLAHE: memory corruption | SW crash | Minor | Improbable | Low | ASan in CI, exception handling |
| SOUP-002 | Wrong DICOM tag value | Non-conformant output | Serious | Remote | Low | DVTk validation per release |
| SOUP-002 | C-STORE association failure | Image not sent to PACS | Minor | Occasional | Low | Queue + retry (3×) + alert |
| SOUP-003 | Inference NaN/Inf | AI output meaningless | Minor | Remote | Low | Output range check [0,1], fallback |
| SOUP-003 | CUDA OOM | Inference failure | Minor | Remote | Low | CPU fallback, memory pre-check |
| SOUP-008 | Numerical instability | MFP artifact (banding) | Minor | Remote | Low | Matrix condition check, fallback |

### 5.3 SOUP Monitoring Plan

| Activity | Frequency | Responsible |
|----------|-----------|-------------|
| CVE scan (SOUP dependencies) | Quarterly | DevOps |
| SOUP version update evaluation | Per SOUP release (major) | SW Lead |
| SOUP regression test | Per SOUP update | CI pipeline |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SOUP-001 v1.0*
