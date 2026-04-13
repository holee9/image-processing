# Technology Stack

## Languages

| Language | Usage | Standard |
|----------|-------|----------|
| C/C++ | Native DLL modules (image processing algorithms) | C++17 |
| C# | Integration test GUI (ImageProcTest) | .NET 8, WPF |
| C | DLL ABI boundary (P/Invoke compatible exports) | C11 |

## Build System

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | >= 3.25 | C/C++ build system |
| Ninja | latest | Build generator (via CMakePresets) |
| vcpkg | manifest mode | SOUP dependency management (pinned versions) |
| MSBuild | VS 2022 | C# project build |

## SOUP Dependencies (XPE)

| Component | Version | License | Used By | Purpose |
|-----------|---------|---------|---------|---------|
| OpenCV | 4.9.x | Apache 2.0 | preprocess, enhance_basic | Bilateral filter, CLAHE, image ops |
| DCMTK | 3.6.8 | BSD-3 | dicom | DICOM file/network I/O |
| Eigen | 3.4.x | MPL-2.0 | enhance_advanced | Matrix ops, FFT |
| ONNX Runtime | 1.17.x | MIT | ai | DL model inference (U-Net, MobileNet-v3) |
| spdlog | 1.13.x | MIT | common | Async logging |
| nlohmann/json | 3.11.x | MIT | common | JSON config parsing |
| fmt | 10.x | MIT | common | String formatting |
| Google Test | 1.14.x | BSD-3 | tests | Unit testing framework |

## SOUP Dependencies (GSVG, independent)

| Component | Version | License | Purpose |
|-----------|---------|---------|---------|
| FFTW3 | 3.3.10 | GPL v2+ | DWT decomposition (dynamic linking required) |
| OpenCV | 4.9.x | Apache 2.0 | Image operations |
| Eigen | 3.4.x | MPL-2.0 | Matrix operations |
| DCMTK | 3.6.8 | BSD-3 | DICOM metadata reading |
| nlohmann/json | 3.11.x | MIT | Configuration |

## Transitive Dependencies

| Component | Via | License | Notes |
|-----------|-----|---------|-------|
| OpenSSL | DCMTK | Apache 2.0 | TLS for DICOM network (C-STORE/C-FIND) |

## Target Platform

| OS | Architecture | SIMD |
|----|-------------|------|
| Windows 11 (primary) | x86-64 | AVX2 |
| Ubuntu 24.04 (deferred) | x86-64 | AVX2 |
| ARM64 (deferred) | aarch64 | NEON |

## HW/SW Development Strategy

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, Section 1.2)

| 전략 | Research ID | 해당 SWU | 구현 방침 |
|------|-------------|----------|-----------|
| HW-only (FPGA) | PRE-01 | SWU-1.9 ReadoutArtifactValidator | FPGA 구현은 HW팀 담당. SW는 post-readout validation만 수행 |
| SW-first → FPGA | PRE-02, PRE-03, PRE-06, PRE-08, PRE-09 | SWU-1.1, 1.2, 1.3, 1.7, 1.8 | Host PC 우선 개발. Fluoroscopy 고프레임 시 FPGA 이관 가능 구조 유지 |
| SW-first → MCU | PRE-07 | SWU-1.6 TempCompensator | 온도 센서 LUT 보간. 임베디드 MCU 이관 가능 구조로 구현 |
| SW-only | PRE-04, PRE-05 | SWU-1.4 GhostCorrector | 복잡한 NLCSC 비선형 모델. Host PC 전용 |

### FPGA 이관 설계 규칙 (SW-first → HW SWUs)

- `xpe_preprocess_configure()` JSON 스키마에 `"hw_mode"` 필드 예비 (미래 FPGA bypass용)
- 알고리즘 로직 순수 함수(stateless) 선호 → FPGA 포팅 용이
- `#pragma pack(push, 8)` / Pack=8 레이아웃 유지 → FPGA DMA 호환

## Must-Have vs Differentiator

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, Section 2)

Phase별 기술 우선순위:

**Phase 1 — Foundation (필수 기술)**:
- 전처리 전체(PRE-01~09), Display LUT(POST-12), 기본 후처리(POST-01~04), 기본 Collimation/ROI 기반 워크플로우(POST-07 기본 tier), DICOM(SUP-04), Support 전체(SUP-01~05)

**Phase 2 — Differentiator**:
- PRE-04 NLCSC Lag Correction (14-50x 업계 우위), PRE-06 ML Defect (14.2x NMSE), POST-05 MFP/FMP (MUSICA-class), POST-07 AI Collimation, POST-11 Virtual Grid (CNR 2-3x)

**Phase 3 — Intelligence**:
- POST-09 DL Bone Suppression (폐결절 민감도 16.8% 향상), POST-02 DL Denoising

## C ABI Design

- Struct packing: `#pragma pack(push, 8)` / `[StructLayout(Pack = 8)]`
- Memory ownership: Caller allocates via xpe_common, caller frees
- Configuration: JSON string (`const char*`) at boundary
- Error handling: `int32_t` return codes + `char*` error buffer
- Thread safety: All exported functions reentrant with independent buffers

### XpeImageMetadata Flags (확장)

```c
// XpeImageMetadata.flags 비트 정의
#define XPE_FLAG_GHOST_CORRECTED         0x00000001u  // PRE-04/05 완료
#define XPE_FLAG_AI_PROCESSED            0x00000002u  // AI 모듈 처리 완료
#define XPE_FLAG_DEFECT_CORRECTED        0x00000004u  // PRE-06 defect correction 완료
#define XPE_FLAG_GAIN_CORRECTED          0x00000008u  // PRE-03 gain correction 완료
#define XPE_FLAG_READOUT_VALIDATED       0x00000010u  // PRE-01 readout artifact validation 완료
#define XPE_FLAG_TEMP_COMPENSATED        0x00000020u  // PRE-07 온도 보정 완료
#define XPE_FLAG_NONLINEARITY_CORRECTED  0x00000040u  // PRE-08 비선형성 보정 완료
#define XPE_FLAG_BINNING_CORRECTED       0x00000080u  // PRE-09 binning correction 완료
#define XPE_FLAG_AED_TRIGGERED           0x00000100u  // SUP-02 AED 이벤트로 획득
#define XPE_FLAG_COLLIMATION_DETECTED    0x00000200u  // POST-07 ROI 검출 완료
#define XPE_FLAG_STITCHED                0x00000400u  // POST-08 stitching 완료
#define XPE_FLAG_BONE_SUPPRESSED         0x00000800u  // POST-09 bone suppression 완료
#define XPE_FLAG_GSVG_SKIPPED            0x00001000u  // GSVG SAFE-003 fallback 발생
```

## Testing

| Framework | Language | Scope |
|-----------|---------|-------|
| Google Test + CTest | C++ | Unit tests (SWU-level), integration tests |
| xUnit | C# | ImageProcTest GUI tests |

## Quality Framework

- IEC 62304 Class B compliance
- TRUST 5: Tested (85%+), Readable, Unified, Secured, Trackable
- TDD methodology (RED-GREEN-REFACTOR)
- SWU-to-DLL-to-test traceability (IEC 62304 5.4.1)

## License Risks

| Risk | Component | Mitigation |
|------|-----------|------------|
| GPL contamination | FFTW3 (GSVG only) | Dynamic linking only, GSVG as separate DLL |
| OpenSSL export restrictions | DCMTK transitive | Review per-jurisdiction requirements |
