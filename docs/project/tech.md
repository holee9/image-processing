# 기술 스택

## 언어

| 언어 | 용도 | 표준 |
|----------|-------|----------|
| C/C++ | Native DLL 모듈 (영상 처리 알고리즘) | C++17 |
| C# | 통합 테스트 GUI (ImageProcTest) | .NET 8, WPF |
| C | DLL ABI 경계 (P/Invoke 호환 export) | C11 |

## 빌드 시스템

| 도구 | 버전 | 목적 |
|------|---------|---------|
| CMake | >= 3.25 | C/C++ 빌드 시스템 |
| Ninja | latest | 빌드 생성기 (via CMakePresets) |
| vcpkg | manifest mode | SOUP 의존성 관리 (고정 버전) |
| MSBuild | VS 2022 | C# 프로젝트 빌드 |

## SOUP 의존성 (XPE)

| 컴포넌트 | 버전 | 라이선스 | 사용처 | 목적 |
|-----------|---------|---------|---------|---------|
| OpenCV | 4.9.x | Apache 2.0 | preprocess, enhance_basic | Bilateral filter, CLAHE, image ops |
| DCMTK | 3.6.8 | BSD-3 | dicom | DICOM file/network I/O |
| Eigen | 3.4.x | MPL-2.0 | enhance_advanced | Matrix ops, FFT |
| ONNX Runtime | 1.17.x | MIT | ai | DL 모델 추론 (U-Net, MobileNet-v3) |
| spdlog | 1.13.x | MIT | common | 비동기 로깅 |
| nlohmann/json | 3.11.x | MIT | common | JSON 구성 파싱 |
| fmt | 10.x | MIT | common | 문자열 포맷팅 |
| Google Test | 1.14.x | BSD-3 | tests | 단위 테스트 프레임워크 |

## SOUP 의존성 (GSVG, 독립)

| 컴포넌트 | 버전 | 라이선스 | 목적 |
|-----------|---------|---------|---------|
| FFTW3 | 3.3.10 | GPL v2+ | DWT 분해 (동적 링크 필요) |
| OpenCV | 4.9.x | Apache 2.0 | 영상 처리 |
| Eigen | 3.4.x | MPL-2.0 | 행렬 연산 |
| DCMTK | 3.6.8 | BSD-3 | DICOM 메타데이터 읽기 |
| nlohmann/json | 3.11.x | MIT | 구성 |

## 전이적 의존성

| 컴포넌트 | 경유 | 라이선스 | 참고 |
|-----------|-----|---------|-------|
| OpenSSL | DCMTK | Apache 2.0 | DICOM 네트워크 TLS (C-STORE/C-FIND) |

## 대상 플랫폼

| OS | 아키텍처 | SIMD |
|----|-------------|------|
| Windows 11 (주요) | x86-64 | AVX2 |
| Ubuntu 24.04 (보류) | x86-64 | AVX2 |
| ARM64 (보류) | aarch64 | NEON |

## HW/SW 개발 전략

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

## 필수 기술 vs 차별화 기술

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, Section 2)

Phase별 기술 우선순위:

**Phase 1 — Foundation (필수 기술)**:
- 전처리 전체(PRE-01~09), Display LUT(POST-12), 기본 후처리(POST-01~04), 기본 Collimation/ROI 기반 워크플로우(POST-07 기본 tier), DICOM(SUP-04), Support 전체(SUP-01~05)

**Phase 2 — Differentiator**:
- PRE-04 NLCSC Lag Correction (14-50x 업계 우위), PRE-06 ML Defect (14.2x NMSE), POST-05 MFP/FMP (MUSICA-class), POST-07 AI Collimation, POST-11 Virtual Grid (CNR 2-3x)

**Phase 3 — Intelligence**:
- POST-09 DL Bone Suppression (폐결절 민감도 16.8% 향상), POST-02 DL Denoising

## C ABI 설계

- Struct packing: `#pragma pack(push, 8)` / `[StructLayout(Pack = 8)]`
- 메모리 소유권: Caller가 xpe_common을 통해 할당, caller가 해제
- Configuration: JSON 문자열 (`const char*`) at boundary
- 오류 처리: `int32_t` 반환 코드 + `char*` 오류 버퍼
- 스레드 안전성: 모든 export 함수는 독립 버퍼로 재진입 가능

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

## 테스팅

| 프레임워크 | 언어 | 범위 |
|-----------|---------|-------|
| Google Test + CTest | C++ | 단위 테스트 (SWU 레벨), 통합 테스트 |
| xUnit | C# | ImageProcTest GUI 테스트 |

## 품질 프레임워크

- IEC 62304 Class B 규정 준수
- TRUST 5: Tested (85%+), Readable, Unified, Secured, Trackable
- TDD 방법론 (RED-GREEN-REFACTOR)
- SWU-DLL-테스트 추적성 (IEC 62304 5.4.1)

## 라이선스 위험

| 위험 | 컴포넌트 | 완화 방법 |
|------|-----------|------------|
| GPL 오염 | FFTW3 (GSVG only) | 동적 링크만 사용, GSVG를 별도 DLL로 분리 |
| OpenSSL 수출 제한 | DCMTK 전이적 | 관할권별 요구사항 검토 |
