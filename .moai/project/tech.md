# 기술 스택 (v2.0)

**Version**: 2.0.0 | **Updated**: 2026-04-17
**Changes from v1.0**: ONNX Runtime 1.20+, SBOM toolchain, SLSA L2/L3 provenance, CMake Presets v6, OpenTelemetry, IEC 81001-5-1 references. See `.moai/project/trend-survey-2026.md` for rationale.

## 프로그래밍 언어

| 언어 | 사용 목적 | 표준 |
|------|----------|----------|
| C++17 | 네이티브 DLL 모듈 (이미지 처리 알고리즘) | C++17 표준 |
| C# | 통합 테스트 GUI (ImageProcTest) | .NET 8, WPF |
| C11 | DLL ABI 경계 (P/Invoke 호출 가능) | C11 표준 |

## 빌드 시스템

| 도구 | 버전 | 목적 |
|------|------|------|
| CMake | >= 3.25 | C/C++ 빌드 시스템 |
| Ninja | 최신 | 빌드 생성기 (CMakePresets 통해) |
| vcpkg | 매니페스트 모드 | SOUP 의존성 관리 (고정 버전) |
| MSBuild | VS 2022 | C# 프로젝트 빌드 |

## 아키텍처 설계

### 3-Layer Anti-Spaghetti 설계
- **Layer 0**: 공통 타입/메모리 (xpe_common.dll)
- **Layer 1**: 알고리즘 DLLs (상호 의존 금지, Layer 0에만 의존)
- **Layer 1-G**: GSVG (독립 IEC 62304 패키지)
- **Layer 2**: C# GUI Orchestrator (P/Invoke로 모든 DLL 호출)

### ABI 설계 규칙
- **호출 규칙**: `__cdecl` (Windows C 기본값)
- **구조체 정렬**: `#pragma pack(push, 8)` — 8바이트 정렬
- **유니버스**: 순수 C 타입만 (`stdint.h`, `stddef.h`)
- **링크**: `extern "C"` 모든 내보내기 기호
- **내보내기 매크로**: `__declspec(dllexport)` / `__declspec(dllimport)`
- **반환 규칙**: 모든 오류 가능 함수는 `XpeErrorCode` 반환
- **스레드 안전성**: 모든 처리 함수 재진입 가능

## SOUP 의존성 (XPE)

| 구성 요소 | 버전 | 라이선스 | 사용 위치 | 목적 |
|-----------|------|---------|----------|------|
| OpenCV | 4.9.x | Apache 2.0 | preprocess, enhance_basic | 이중 필터, CLAHE, 이미지 연산 |
| DCMTK | 3.6.8 | BSD-3 | dicom | DICOM 파일/네트워크 I/O |
| Eigen | 3.4.x | MPL-2.0 | enhance_advanced | 행렬 연산, FFT |
| ONNX Runtime | **1.20.x+** (v2.0) | MIT | ai | DL 모델 추론. v2.0 업그레이드 근거: TensorRT EP 10.9, DirectML EP, CUDA EP 다중 백엔드 런타임 선택. 기존 U-Net, MobileNet-v3 + 신규 SSL/Diffusion 모델 |
| spdlog | 1.13.x | MIT | common | 비동기 로깅 |
| nlohmann/json | 3.11.x | MIT | common | JSON 설정 파싱 |
| fmt | 10.x | MIT | common | 문자열 서식 지정 |
| Google Test | 1.14.x | BSD-3 | tests | 단위 테스트 프레임워크 |

## SOUP 의존성 (GSVG, 독립)

| 구성 요소 | 버전 | 라이선스 | 목적 |
|-----------|------|---------|------|
| FFTW3 | 3.3.10 | GPL v2+ | DWT 분해 (동적 링크 필요) |
| OpenCV | 4.9.x | Apache 2.0 | 이미지 연산 |
| Eigen | 3.4.x | MPL-2.0 | 행렬 연산 |
| DCMTK | 3.6.8 | BSD-3 | DICOM 메타데이터 읽기 |
| nlohmann/json | 3.11.x | MIT | 설정 관리 |

## 전이 의존성

| 구성 요소 | 경로 | 라이선스 | 참고 사항 |
|-----------|------|---------|-----------|
| OpenSSL | DCMTK | Apache 2.0 | DICOM 네트워크 TLS (C-STORE/C-FIND) |

## 타겟 플랫폼

| OS | 아키텍처 | SIMD |
|----|----------|------|
| Windows 11 (주요) | x86-64 | AVX2 |
| Ubuntu 24.04 (지연) | x86-64 | AVX2 |
| ARM64 (지연) | aarch64 | NEON |

## 개발 환경

### 필수 도구
- **Visual Studio 2022**: C++ 및 C# 개발 (Intel C++ Compiler 옵션)
- **vcpkg**: 의존성 관리 (매니페스트 모드)
- **CMake**: 빌드 시스템 (Ninja 백엔드)
- **Git**: 버전 관리
- **MoAI 프레임워크**: 자동화된 개발 워크플로우

### 빌드 구성
```bash
# 개발 환경 설정
cmake --preset x86-windows-developer
cmake --build --preset x86-windows-developer

# 릴리즈 빌드
cmake --preset x86-windows-release
cmake --build --preset x86-windows-release

# 테스트 실행
ctest --preset x86-windows-developer
```

### 테스트 프레임워크
- **Google Test**: C++ 단위/통합 테스트
- **xUnit**: C# GUI 테스트
- **CTest**: 테스트 자동화 및 리포팅
- **MoAI TDD**: 테스트 주도 개발 워크플로우

### 품질 보장
- **TRUST 5**: Tested (85%+), Readable, Unified, Secured, Trackable
- **IEC 62304 Class B**: 의료 기기 소프트웨어 품질 표준
- **MX 태그 시스템**: 코드 수준 주석 및 경고
- **자동화된 검증**: MoAI 품질 게이트웨이

## HW/SW Development Strategy

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, Section 1.2)

| 전략 | Research ID | 해당 SWU | 구현 방침 |
|------|-------------|----------|-----------|
| HW-only (FPGA) | PRE-01 | SWU-1.9 ReadoutArtifactValidator | FPGA 구현은 HW팀 담당. SW는 post-readout validation만 수행 |
| SW-first → FPGA | PRE-02, PRE-03, PRE-06, PRE-08, PRE-09 | SWU-1.1, 1.2, 1.3, 1.7, 1.8 | Host PC 우선 개발. Fluoroscopy 고프레임 시 FPGA 이관 가능 구조 유지 |
| SW-first → MCU | PRE-07 | SWU-1.6 TempCompensator | 온도 센서 LUT 보간. 임베디드 MCU 이관 가능 구조로 구현 |
| SW-only | PRE-04, PRE-05 | SWU-1.4 GhostCorrector | 복잡한 NLCSC 비선형 모델. Host PC 전용 |

## 개발 전략

### FPGA 이관 설계 규칙 (SW-first → HW SWUs)

- **프로그래밍 모델**: `xpe_preprocess_configure()` JSON 스키마에 `"hw_mode"` 필드 예비 (미래 FPGA bypass용)
- **알고리즘 설계**: 순수 함수(stateless) 선호 → FPGA 포팅 용이
- **메모리 레이아웃**: `#pragma pack(push, 8)` / Pack=8 레이아웃 유지 → FPGA DMA 호환

### 기술 우선순위 분류

**Phase 1 — Foundation (필수 기술)**:
- 전처리 전체(PRE-01~09), Display LUT(POST-12), 기본 후처리(POST-01~04)
- 기본 Collimation/ROI 기반 워크플로우(POST-07 기본 tier), DICOM(SUP-04)
- Support 기술 전체(SUP-01~05)

**Phase 2 — Differentiator (차별화 기술)**:
- PRE-04 NLCSC Lag Correction (14-50x 업계 우위)
- PRE-06 ML Defect (14.2x NMSE)
- POST-05 MFP/FMP (MUSICA-class)
- POST-07 AI Collimation, POST-11 Virtual Grid (CNR 2-3x)

**Phase 3 — Intelligence (AI 고도화)**:
- POST-09 DL Bone Suppression (폐결절 민감도 16.8% 향상)
- POST-02 DL Denoising, 고급 추론 최적화

## C ABI 인터페이스 설계

### 핵심 설계 원칙
- **구조체 정렬**: `#pragma pack(push, 8)` / `[StructLayout(Pack = 8)]`
- **메모리 소유권**: 호출자가 `xpe_common`으로 할당, 호출자가 해제
- **설정 전달**: JSON 문자열 (`const char*`) 경계에서
- **오류 처리**: `int32_t` 반환 코드 + `char*` 오류 버퍼
- **스레드 안전성**: 모든 내보내기 함수 독립 버퍼로 재진입 가능

### XpeImageMetadata Flags 확장

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

## 테스트 프레임워크

| 프레임워크 | 언어 | 범위 |
|-----------|------|------|
| Google Test + CTest | C++ | 단위 테스트 (SWU 레벨), 통합 테스트 |
| xUnit | C# | ImageProcTest GUI 테스트 |

## 품질 프레임워크

- **IEC 62304 Class B**: 의료 기기 소프트웨어 품질 표준 준수
- **TRUST 5**: Tested (85%+), Readable, Unified, Secured, Trackable
- **TDD 방법론**: RED-GREEN-REFACTOR 개발 사이클
- **추적성**: SWU-to-DLL-to-test 추적 관리 (IEC 62304 5.4.1)
- **MoAI 워크플로우**: 자동화된 SPEC 관리 및 구현

## 라이선스 리스크

| 리스크 | 구성 요소 | 완화 방안 |
|------|-----------|------------|
| GPL 오염 | FFTW3 (GSVG 전용) | 동적 링크만, GSVG 별도 DLL로 분리 |
| OpenSSL 수출 제한 | DCMTK 전이적 의존 | 관할권별 요구사항 검토 |

## 성능 최적화

### SIMD 지원
- **AVX2**: Windows 및 Ubuntu x86-64 기본 지원
- **NEON**: ARM64 대상 지연 계획
- **벡터화**: 이미지 처리 알고리즘 자동 벡터화

## 공급망 보안 및 빌드 검증 (v2.0 신규)

### SBOM (Software Bill of Materials)

FDA §524B 법적 의무 대응. SPDX 3.0 + CycloneDX 1.6 이중 포맷 지원.

| 도구 | 목적 | 배포 위치 |
|------|------|----------|
| **syft** (Anchore) | SBOM 생성 (SPDX/CycloneDX) | CI 자동 실행 |
| **cyclonedx-cpp-maker** | C/C++ 전용 CycloneDX | 백업 생성기 |
| **Grype** | SBOM 취약점 스캔 | CI 자동 실행 |
| **OSV-Scanner** (Google) | 취약점 데이터베이스 스캔 | CI 자동 실행 |
| **CycloneDX VEX / OpenVEX / CSAF 2.0** | 취약점 대응 문서 | 취약점 보고 시 자동 생성 |

근거: [FDA 2025-06 Cybersecurity Final Guidance](https://www.federalregister.gov/documents/2025/06/27/2025-11669/cybersecurity-in-medical-devices-quality-system-considerations-and-content-of-premarket-submissions), [SPDX 3.0](https://spdx.dev/), [CycloneDX 1.6](https://cyclonedx.org/)

### 공급망 무결성 (SLSA)

SLSA (Supply-chain Levels for Software Artifacts) 기반 빌드 증명:

| 레벨 | 목표 | 구현 |
|------|------|------|
| L1 | Provenance 존재 | 기본 CI 로그 |
| L2 | 서명된 provenance | GitHub Actions SLSA generator (v1.x) |
| **L3** | 격리된 빌드 환경 | 본 프로젝트 Phase 2 목표. Reusable workflow 사용 |

attestation: in-toto v1.0 spec

근거: [SLSA Specification](https://slsa.dev/spec/v1.0/levels)

### 재현 가능한 빌드 (Reproducible Builds)

- **SOURCE_DATE_EPOCH** 환경변수 준수
- **CMake/Ninja**: `__DATE__`/`__TIME__` 매크로 프로덕션 빌드 배제
- **Windows**: MSVC `/Brepro` 링커 플래그
- **검증**: `scripts/verify_reproducible.sh` (2회 빌드 byte-identical 비교)

근거: [reproducible-builds.org](https://reproducible-builds.org/)

## 관측성 (Observability, v2.0 신규)

### OpenTelemetry 통합 (Should, opt-in)

| 요소 | 버전 | 용도 |
|------|------|------|
| OpenTelemetry C++ SDK | 1.x+ | xpe_common OTEL Tracer/Meter/Logger API |
| OTLP 내보내기 | HTTP/protobuf 및 gRPC | 수집 endpoint |
| Semantic Conventions | 표준 | `xpe.stage.name`, `xpe.swu.id` |
| **Profiling Signal** | 2024 도입 | 지속적 프로파일링 지원 |

엔진 기본값: OFF. 활성화 시 site configuration으로 설정.

근거: [OpenTelemetry 2025 roadmap](https://opentelemetry.io/blog/2025/)

### Drift Detection (AI 모듈)

AI 모듈은 추론 시 input fingerprint 생성, 하기 검출기 지원:

- Kolmogorov-Smirnov 테스트 (per-feature)
- Maximum Mean Discrepancy (MMD, deep kernel)
- Classifier-based drift detector

근거: [Nature Digital Medicine "Distribution shift detection"](https://www.nature.com/articles/s41746-024-01085-w), [Nature Communications "Empirical data drift"](https://www.nature.com/articles/s41467-024-46142-w)

## 규제 표준 매핑 (v2.0 신규)

XPE 프로젝트가 준수하는 표준 및 가이던스:

| 표준/가이던스 | 분류 | 역할 | 대응 SPEC |
|--------------|:----:|------|----------|
| IEC 62304:2006+A1:2015 | Normative | SW 라이프사이클 (Class B) | 기존 |
| **IEC 81001-5-1:2021** | Normative | 보안 SW 라이프사이클 (64 요구사항) | SPEC-XPE-SEC |
| ISO 13485:2016 | Normative | QMS | QMSR 2026 통합 |
| ISO 14971:2019 | Normative | 리스크 관리 | 기존 |
| **ISO/IEC 42001:2023** | Normative | AI Management System | SPEC-XPE-REG |
| **FDA §524B** | Law | Cyber Device 사이버보안 | SPEC-XPE-SEC |
| **FDA PCCP Final 2024-12** | Guidance | AI-DSF 사전승인 변경 | SPEC-XPE-REG |
| **FDA AI-DSF Lifecycle Draft 2025-01** | Guidance | TPLC AI 접근 | SPEC-XPE-REG |
| **FDA Transparency 2024-06** | Guidance | ML-MD 투명성 | SPEC-XPE-REG |
| **IMDRF GMLP N88 Final 2025-01** | Guidance | 10대 원칙 | SPEC-XPE-REG |
| **EU Regulation 2024/1689 (AI Act)** | Law | High-Risk AI | SPEC-XPE-REG |
| EU Regulation 2017/745 (MDR) | Law | 의료기기 | 기존 |
| DICOM PS 3.1-3.20 | Normative | 상호운용성 | SPEC-XPE-IOP |
| HL7 FHIR R5 | Normative | 의료 정보 교환 | SPEC-XPE-IOP |
| IHE RAD TF Rev 23.0 (2025-08) | Normative | 프로필 | SPEC-XPE-IOP |

## 빌드 시스템 업그레이드 (v2.0 신규)

- **CMake Presets v6**: workflow presets + configure/build/test 조합
- **vcpkg**: 매니페스트 모드 (기존) + 커스텀 registry (Phase 2)
- **GitHub Actions**: Reusable workflows for SBOM/SLSA
- **cibuildwheel/toolchain**: 결정적 빌드 환경

### 메모리 관리
- **메모리 풀**: xpe_common.dll에서 제공
- **버퍼 재사용**: 스레드 로컬 버퍤 풀링
- **페이지 관리**: 대용량 이미지 효율적 처리

### 멀티 스레딩
- **스레드 풀**: 작업 병렬 처리
- **무상태 설계**: 스레드 간 상태 공유 없음
- **스케일링**: 코어 수에 따른 자동 스레드 수 조절
