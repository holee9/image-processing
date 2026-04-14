# SOUP 목록 & 분석

**Document ID:** XPE-SOUP-001 v1.0  
**IEC 62304 Clause:** 5.3.3, 5.3.4, 7.4.1 — 7.4.3  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. 목적

XPE에 사용되는 모든 SOUP(Software of Unknown Provenance)를 식별하고, 기능/성능 요구사항, 시스템 요구사항, 위험 분석을 문서화한다.

## 2. SOUP 인벤토리

| ID | 이름 | 버전 | 목적 | 라이선스 | SW 항목 | 안전 등급 |
|----|------|---------|---------|---------|---------|:----------:|
| SOUP-001 | OpenCV | 4.9.x | 이미지 처리 프리미티브 | Apache 2.0 | SWI-2 | B |
| SOUP-002 | dcmtk | 3.6.8 | DICOM 읽기/쓰기/네트워크 | BSD-3 | SWI-4 | B |
| SOUP-003 | ONNX Runtime | 1.17.x | DL 모델 추론 | MIT | SWI-2 | B |
| SOUP-004 | spdlog | 1.13.x | 로깅 프레임워크 | MIT | SWI-5 | A |
| SOUP-005 | nlohmann/json | 3.11.x | JSON 설정 파싱 | MIT | SWI-5 | A |
| SOUP-006 | Google Test | 1.14.x | 유닛 테스팅 (개발 전용) | BSD-3 | — | N/A |
| SOUP-007 | fmt | 10.x | 문자열 포매팅 | MIT | SWI-5 | A |
| SOUP-008 | Eigen | 3.4.x | 행렬 연산 | MPL-2.0 | SWI-2 | B |

## 3. 기능 & 성능 요구사항 (5.3.3)

| SOUP ID | 기능 요구사항 | 성능 요구사항 |
|---------|------------------------|------------------------|
| SOUP-001 | cv::bilateralFilter(정확한 edge-preserving), cv::CLAHE(block histogram+clip+redistribute), cv::pyrDown/pyrUp(Laplacian pyramid), cv::resize(bilinear/bicubic) | 3072×3072 bilateral ≤ 200ms, CLAHE ≤ 150ms, pyrDown 12-level ≤ 300ms |
| SOUP-002 | DcmFileFormat 읽기/쓰기, DcmDataset 태그 조작, DcmSCU C-STORE/C-FIND, JPEG 2000 무손실 codec (OpenJPEG 백엔드), 모든 DX IOD Type 1/2 태그 지원 | DICOM 파일 쓰기 ≤ 1s (비압축 3072×3072), J2K 인코딩 ≤ 3s |
| SOUP-003 | ONNX 모델 로드 (Residual U-Net ~50M params), CreateSession, 추론 실행, GPU 공급자 (CUDA EP) + CPU fallback, float32 입출력 | 추론 ≤ 2s (RTX 3060), ≤ 10s (CPU만), 모델 로드 ≤ 5s |
| SOUP-004 | spdlog::info/warn/error, rotating file sink, async logger | 로그 쓰기 ≤ 1μs (async 모드) |
| SOUP-005 | JSON 파싱/직렬화, 중첩 객체, 배열 | 1MB 설정 파싱 ≤ 10ms |
| SOUP-007 | fmt::format 문자열 포매팅 | 무시할 수 있는 오버헤드 |
| SOUP-008 | Eigen::MatrixXf 연산, FFT (unsupported 모듈을 통해), 분해 | 3072×3072 행렬 곱셈 ≤ 500ms |

## 4. 시스템 하드웨어 & 소프트웨어 요구사항 (5.3.4)

| SOUP ID | OS | CPU 아키텍처 | GPU | 의존성 | 디스크 |
|---------|----|---------:|-----|-------------|------|
| SOUP-001 | Windows 11, Ubuntu 24.04 | x86-64 (AVX2), ARM64 (NEON) | — (CPU만) | — | ~50MB |
| SOUP-002 | Windows 11, Ubuntu 24.04 | x86-64, ARM64 | — | OpenSSL ≥ 1.1 (TLS) | ~30MB |
| SOUP-003 | Windows 11, Ubuntu 24.04 | x86-64, ARM64 | NVIDIA CUDA 12+ (선택사항) | CUDA Toolkit 12.x (선택사항) | ~200MB |
| SOUP-004 | 크로스 플랫폼 | 모든 | — | — | ~1MB |
| SOUP-005 | 크로스 플랫폼 | 모든 | — | — | 헤더만 |
| SOUP-007 | 크로스 플랫폼 | 모든 | — | — | ~1MB |
| SOUP-008 | 크로스 플랫폼 | 모든 (SIMD 자동 감지) | — | — | 헤더만 |

## 5. SOUP 리스크 분석 (7.4)

### 5.1 발표된 이상 목록

| SOUP ID | 이상 출처 | 알려진 관련 이슈 | 완화 |
|---------|---------------|----------------------|-----------|
| SOUP-001 | github.com/opencv/opencv/issues | CLAHE 경계 artifact (4.8+ 고정) | 버전 ≥ 4.9 사용 |
| SOUP-002 | github.com/DCMTK/dcmtk/issues | J2K codec edge case (드물음) | DVTk 준수 테스트 |
| SOUP-003 | github.com/microsoft/onnxruntime/issues | CUDA EP 메모리 누수 (1.16+ 고정) | 버전 ≥ 1.17 사용 |
| SOUP-008 | gitlab.com/libeigen/eigen/-/issues | 극도의 규모에서 수치 정밀도 | 조건 수 확인 |

### 5.2 실패 모드 분석

| SOUP ID | 실패 모드 | XPE에 대한 영향 | 심각도 | 확률 | 리스크 | 제어 |
|---------|-------------|---------------|:--------:|:----:|:----:|---------|
| SOUP-001 | bilateralFilter: 잘못된 출력 | 노이즈 표시 또는 세부 손실 | 경미 | 원격 | 낮음 | 참조 기준 PSNR ≥ 30dB |
| SOUP-001 | CLAHE: 메모리 손상 | SW 충돌 | 경미 | 불가능 | 낮음 | CI의 ASan, 예외 처리 |
| SOUP-002 | 잘못된 DICOM 태그 값 | 비준수 출력 | 심각 | 원격 | 낮음 | 릴리스당 DVTk 검증 |
| SOUP-002 | C-STORE 연결 실패 | 이미지가 PACS에 전송되지 않음 | 경미 | 가끔 | 낮음 | 큐 + 재시도 (3×) + 알림 |
| SOUP-003 | 추론 NaN/Inf | AI 출력 무의미 | 경미 | 원격 | 낮음 | 출력 범위 확인 [0,1], fallback |
| SOUP-003 | CUDA OOM | 추론 실패 | 경미 | 원격 | 낮음 | CPU fallback, 메모리 사전 확인 |
| SOUP-008 | 수치적 불안정성 | MFP artifact (밴딩) | 경미 | 원격 | 낮음 | 행렬 조건 확인, fallback |

### 5.3 SOUP 모니터링 계획

| 활동 | 빈도 | 담당 |
|----------|-----------|-------------|
| CVE 스캔 (SOUP 의존성) | 분기별 | DevOps |
| SOUP 버전 업데이트 평가 | SOUP 릴리스당 (주요) | SW 담당자 |
| SOUP 회귀 테스트 | SOUP 업데이트당 | CI 파이프라인 |

---

## 개정 이력

| 개정판 | 날짜 | 작성자 | 설명 |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | 초기 릴리스 |

---

*문서 끝 — XPE-SOUP-001 v1.0*
