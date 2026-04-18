# XPE 모듈 상세 정보

**문서 ID**: XPE-CODEMAP-002  
**버전**: 1.0.0  
**날짜**: 2026-04-17  
**상태**: 작성 중  
**분류**: 내부 / 모듈 기준 문서

---

## 1. 모듈 개요

XPE 아키텍처는 총 8개의 DLL 모듈로 구성됩니다. 각 모듈은 명확한 책임 영역과 공개 인터페이스를 가지며, 모듈 간 의존성은 철저히 제어됩니다.

### 1.1 모듈 구성

| 계층 | 모듈명 | SWU 개수 | 기능 영역 | Phase |
|------|--------|----------|-----------|-------|
| Layer 0 | `xpe_common.dll` | 18개 | 공통 기능 | 항상 필요 |
| Layer 1 | `xpe_preprocess.dll` | 9개 | 전처리 | 항상 필요 |
| Layer 1 | `xpe_enhance_basic.dll` | 4개 | 기본 향상 | 항상 필요 |
| Layer 1 | `xpe_enhance_advanced.dll` | 4개 | 고급 향상 | 선택적 |
| Layer 1 | `xpe_ai.dll` | 4개 | AI 처리 | 선택적 |
| Layer 1 | `xpe_display.dll` | 4개 | 디스플레이 | 항상 필요 |
| Layer 1 | `xpe_dicom.dll` | 4개 | DICOM I/O | 항상 필요 |
| Layer 1-G | `gsvg.dll` | 4개 | 그리드 처리 | 독립 패키지 |

**총 SWU**: 38개 (C/C++ 36개, C# 2개)
**총 API 함수**: 83개 (전처리 파이프라인 통합으로 +1)

---

## 2. 상세 모듈 정보

### 2.1 Layer 0: xpe_common.dll

**역할**: 라이브러리 라이프사이클, 메모리 관리, 구성, 파라미터 범위, 알릿 폴링, 로깅

| 함수 개수 | 18개 | 의존성 | 없음 |
|-----------|------|--------|------|
| **핵심 API** | 설명 |
| `xpe_init()` | XPE 서브시스템 초기화 |
| `xpe_shutdown()` | 모든 XPE 리소스 해제 |
| `xpe_alloc_image()` | 이미지 버퍼 할당 |
| `xpe_free_image()` | 이미지 버퍼 해제 |
| `xpe_configure()` | 런타임 구성 업데이트 |
| `xpe_get_pending_alert()` | 대기 중인 알릿 조회 |
| `xpe_aed_configure()` | 자동 노출 감지 구성 |
| **공개 타입** | `XpeImageBuffer`, `XpeImageMetadata`, `XpeErrorCode`, `XpePixelFormat` |

### 2.2 Layer 1: xpe_preprocess.dll

**역할**: 오프라인 보정(오프셋/게인/결함 맵), 런타임 보정, 고스트 아티팩트 보정, 전체 파이프라인 통합

| 함수 개수 | 19개 | 의존성 | `xpe_common.dll` |
|-----------|------|--------|------------------|
| **주요 기능** | 설명 |
| `xpe_offset_correct()` | 오프셋 보정 적용 |
| `xpe_gain_correct()` | 게인 보정 적용 |
| `xpe_defect_correct()` | 결함 픽셀 보정 |
| `xpe_ghost_correct()` | 고스트/지연 보정 (Tier 1/2/3) |
| `xpe_calib_load_*()` | 보정 데이터 로드 |
| `xpe_temp_compensate()` | 온도 보상 |
| `xpe_nonlinearity_correct()` | 비선형 보정 |
| `xpe_binning_correct()` | 빈닝 보정 |
| `xpe_validate_readout_artifact()` | 읽기 아티팩트 검증 |
| `xpe_preprocess_pipeline()` | **새로 추가**: 전처리 파이프라인 통합 (REQ-P1A-041~049) |
| **SWU 매핑** | PRE-01~09 (9개) |
| **처리 순서** | (0.5)→(1)→(1.5)→(2)→(2.5)→(3)→(4) |

### 2.3 Layer 1: xpe_enhance_basic.dll

**역할**: 기본 이미지 향상 연산: 로그 변환, 노이즈 감소, 대비, 엣지 향상

| 함수 개수 | 6개 | 의존성 | `xpe_common.dll` |
|-----------|------|--------|------------------|
| **주요 기능** | 설명 |
| `xpe_log_transform()` | 로그 변환 적용 |
| `xpe_noise_reduce()` | 노이즈 감소 |
| `xpe_contrast_enhance()` | 대비 향상 (CLAHE) |
| `xpe_edge_enhance()` | 엣지 향상 |
| `xpe_log_inverse()` | 역 로그 변환 |
| `xpe_noise_estimate_sigma()` | 노이즈 추정 |
| **SWU 매핑** | POST-01~04, POST-07 (4개) |
| **입력 형식** | `uint16` → `float32` |

### 2.4 Layer 1: xpe_enhance_advanced.dll

**역할**: 다중 스케일 주파수 처리, 분수 미분 향상, 콜리메이션 검출, 노출 인덱스 계산

| 함수 개수 | 4개 | 의존성 | `xpe_common.dll` |
|-----------|------|--------|------------------|
| **주요 기능** | 설명 |
| `xpe_multiscale_process()` | 다중 스케일 처리 |
| `xpe_fractional_process()` | 분수 미분 처리 |
| `xpe_detect_collimation()` | 콜리메이션 경계 검출 |
| `xpe_calc_exposure_index()` | 노출 인덱스 계산 |
| **SWU 매핑** | POST-05~06, POST-08~09 (4개) |
| **입력 형식** | `float32` |
| **특징** | 로그 변환 후 실행 필요 |

### 2.5 Layer 1: xpe_ai.dll

**역할**: 딥러닝 추론: 신체 부위 인식, 스티칭, 뼈 억제, DL 기반 노이즈 감소

| 함수 개수 | 7개 | 의존성 | `xpe_common.dll` + `xpe_ai_worker.exe` |
|-----------|------|--------|-----------------------------------------|
| **실행 모델** | 설명 |
| `xpe_ai_init()` | AI 워커 프로세스 시작 |
| `xpe_ai_shutdown()` | AI 워커 종료 |
| `xpe_bodypart_recognize()` | 신체 부위 분류 |
| `xpe_stitch_images()` | 이미지 스티칭 |
| `xpe_bone_suppress()` | 뼈 구조 억제 |
| `xpe_dl_denoise()` | 딥러닝 노이즈 감소 |
| **SWU 매핑** | POST-02, POST-07 AI (4개) |
| **특징** | 샌박스된 워커 프로세스 |
| **GPU 지원** | ONNX Runtime/TensorRT |

### 2.6 Layer 1: xpe_display.dll

**역할**: DICOM 표준 LUT 파이프라인: 모달리티 LUT, VOI LUT, 프레젠테이션 LUT, 프리셋 관리

| 함수 개수 | 11개 | 의존성 | `xpe_common.dll` |
|-----------|------|--------|------------------|
| **주요 기능** | 설명 |
| `xpe_modality_lut_apply()` | 모달리티 LUT 적용 |
| `xpe_voi_lut_apply()` | VOI LUT 적용 |
| `xpe_presentation_lut_apply()` | 프레젠테이션 LUT 적용 |
| `xpe_lut_*()` | LUT 프리셋 관리 |
| `xpe_presentation_lut_check_display()` | 디스플레이 GSDF 준수 확인 |
| **SWU 매핑** | POST-12 (4개) |
| **출력 형식** | `uint16` |

### 2.7 Layer 1: xpe_dicom.dll

**역할**: DICOM 파일 I/O, 태그 조작, GSPS 주석, 네트워크 서비스(C-STORE/C-FIND)

| 함수 개수 | 10개 | 의존성 | `xpe_common.dll` |
|-----------|------|--------|------------------|
| **주요 기능** | 설명 |
| `xpe_dicom_read()` | DICOM 파일 읽기 |
| `xpe_dicom_write()` | DICOM 파일 쓰기 |
| `xpe_dicom_*()` | DICOM 태그 조작 |
| `xpe_gsps_*()` | GSPS 프레젠테이션 상태 |
| `xpe_dicom_cstore()` | 원격 C-STORE 전송 |
| **SWU 매핑** | SUP-04 (4개) |
| **네트워크** | C-STORE, C-FIND MWL 지원 |

### 2.8 Layer 1-G: gsvg.dll

**역할**: 반산란 그리드 검출 및 가상 그리드 억제. 독립 IEC 62304 패키지

| 함수 개수 | 8개 | 의존성 | 없음 (독립) |
|-----------|------|--------|-------------|
| **주요 기능** | 설명 |
| `gsvg_process()` | 그리드 억제 처리 |
| `gsvg_detect_grid()` | 그리드 검출 |
| `gsvg_suppress_grid()` | 그리드 억제 |
| `gsvg_virtual_grid()` | 가상 그리드 생성 |
| **특징** | 독립된 타입 정의 |
| **오류 코드** | `GsvgErrorCode` 별도 정의 |
| **SWU 매핑** | GSVG-01~04 (4개) |

---

## 3. SWU-모듈 매핑

### 3.1 전체 SWU 분배

| SWU 범위 | SWU 개수 | 모듈 | 설명 |
|----------|----------|------|------|
| PRE-01~09 | 9개 | `xpe_preprocess.dll` | 전처리 단계 |
| POST-01~04 | 4개 | `xpe_enhance_basic.dll` | 기본 향상 |
| POST-05~06 | 4개 | `xpe_enhance_advanced.dll` | 고급 향상 |
| POST-07 | 1개 | `xpe_enhance_basic.dll` | 기본 노이즈 감소 |
| POST-07 AI | 1개 | `xpe_ai.dll` | AI 노이즈 감소 |
| POST-08~09 | 4개 | `xpe_enhance_advanced.dll` | 고급 처리 |
| POST-12 | 4개 | `xpe_display.dll` | 디스플레이 처리 |
| SUP-04 | 4개 | `xpe_dicom.dll` | DICOM 지원 |
| GSVG-01~04 | 4개 | `gsvg.dll` | 그리드 처리 |

### 3.2 SWU 상세 분류

#### 전처리 (PRE-01~09)
- **PRE-01**: 읽기 아티팩트 검증 (`xpe_validate_readout_artifact`)
- **PRE-07**: 온도 보상 (`xpe_temp_compensate`)
- **PRE-08**: 비선형성 보정 (`xpe_nonlinearity_correct`)
- **PRE-09**: 빈닝 보정 (`xpe_binning_correct`)
- **PRE-02/03/06**: 오프셋/게인/결함 보정
- **PRE-04**: 고스트 보정 (Tier 1/2/3 LTI/NLCSC) (`xpe_ghost_correct`)

#### 향상 (POST-01~12)
- **POST-01**: 로그 변환 (`xpe_log_transform`)
- **POST-02**: 노이즈 감소 (`xpe_noise_reduce`)
- **POST-03**: 대비 향상 (`xpe_contrast_enhance`)
- **POST-04**: 엣지 향상 (`xpe_edge_enhance`)
- **POST-05/06**: 다중 스케일/분수 처리 (`xpe_*_advanced`)
- **POST-07**: 노이즈 감소 (기본/AI)
- **POST-08/09**: 콜리메이션/노출 인덱스 (`xpe_*_advanced`)
- **POST-12**: 디스플레이 LUT (`xpe_display.dll`)

---

## 4. 내부 모듈 구성

### 4.1 모듈 내부 구조

```
xpe_common.dll/
├── include/xpe/common/
│   ├── xpe_types.h      # 공통 타입 정의
│   ├── xpe_error.h      # 오류 코드
│   ├── xpe_memory.h     # 메모리 관리
│   └── xpe_common_api.h # 공개 API
├── src/
│   ├── lifecycle.cpp     # 라이프사이클
│   ├── memory.cpp       # 메모리 관리
│   ├── alert.cpp        # 알릿 시스템
│   ├── logging.cpp      # 로깅
│   └── aed.cpp          # 자동 노출 감지

xpe_preprocess.dll/
├── include/xpe/preprocess/
│   ├── xpe_preprocess.h
│   ├── xpe_calibration.h
│   ├── xpe_ghost.h
│   └── xpe_pipeline.h  # **새로 추가**: 파이프라인 통합 API
├── src/
│   ├── offset_correct.cpp
│   ├── gain_correct.cpp
│   ├── defect_correct.cpp
│   ├── ghost_correct.cpp     # Tier 1/2/3 구현 (LTI/NLCSC)
│   ├── pipeline.cpp          # **새로 추가**: 전처리 파이프라인 (REQ-P1A-041~049)
│   ├── calibration_io.cpp
│   ├── temp_compensate.cpp    # **새로 추가**: 온도 보상
│   ├── nonlinearity.cpp      # **새로 추가**: 비선형 보정
│   ├── binning.cpp           # **새로 추가**: 빈닝 보정
│   └── readout_validate.cpp   # **새로 추가**: 읽기 아티팩트 검증
```

### 4.2 파일 구성 규칙

- **include/**: 공개 헤더 파일 (ABI 안정성 보장)
- **src/**: 구현 파일 (내부 변경 자유)
- **test/**: 단위 테스트 코드
- **docs/**: 모듈 문서
- **examples/**: 사용 예제

---

## 5. 모듈 간 통신 규칙

### 5.1 데이터 전달 규칙

```c
// 1. 안정적인 C ABI 사용
typedef struct XpeImageBuffer {
    uint32_t width;
    uint32_t height;
    XpePixelFormat format;
    void* data;
    size_t dataSize;
} XpeImageBuffer;

// 2. 호출자 할당 메모리 모델
XpeImageBuffer input;
xpe_alloc_image(width, height, format, &input);
// ... 처리 ...
xpe_free_image(&input);
```

### 5.2 오류 처리 규칙

```c
// 1. 모든 함수는 XpeErrorCode 반환
XpeErrorCode result = xpe_process(&img);
if (result != XPE_OK) {
    const char* error = xpe_error_string(result);
    // 오류 처리
}

// 2. 확장 오류 정보는 out-parameters 사용
char errorMsg[256];
xpe_get_pending_alert(0, errorMsg, sizeof(errorMsg), &severity);
```

### 5.3 스레드 안전성

- **재진입 가능**: 모든 함수는 재진입 가능
- **무상태 설계**: 모듈 내부 상태 없음
- **독립 버퍼**: 각 스레드별 버퍼 사용
- **원자성 연산**: 알릿 크기 등 원자적 접근

---

## 6. 모듈 확장 가이드

### 6.1 새 모듈 추가 절차

1. **ABI 설계**: 공개 API 정의
2. **의존성 확인**: Layer 0만 의존해야 함
3. **메모리 관리**: 호출자 할당 모델 따름
4. **테스트 작성**: Google Test 기반 단위 테스트
5. **문서화**: 모듈 문서 작성
6. **통합**: ImageProcTest에 통합

### 6.2 기존 모듈 확장

- **기능 추가**: 새 함수는 기존 ABI와 호환해야 함
- **구조 확장**: `XpeImageMetadata` 등 확장 시 하위 호환성 유지
- **구성 추가**: JSON 기반 구성으로 확장

### 6.3 버전 관리

- **주 버전**: ABI 변경 시
- **부 버전**: 기능 추가 시
- **수정 버전**: 버그 수정 시
- **호환성**: 이전 버전과 ABI 호환성 유지

---

## 7. 검증 및 테스트

### 7.1 모듈별 테스트 전략

| 모듈 | 테스트 방법 | 검증 항목 |
|------|-------------|----------|
| `xpe_common.dll` | 단위 테스트 | 메모리 누수, 스레드 안전성 |
| `xpe_preprocess.dll` | 단위 테스트 + 표준 데이터셋 | 보정 정확도, 성능 |
| `xpe_enhance_*.dll` | 단위 테스트 + 이미지 QA | 품질 향상 효과 |
| `xpe_ai.dll` | 단위 테스트 + 모델 검증 | 추론 정확도, 워커 안정성 |
| `xpe_display.dll` | 단위 테스트 + GSDF 검증 | 디스플레이 정확도 |
| `xpe_dicom.dll` | DICOM 유효성 테스트 | 표준 준수성 |

### 7.2 통합 테스트

- **ImageProcTest**: 모든 DLL 통합 테스트
- **파이프라인 테스트**: End-to-end 처리 검증
- **성능 테스트**: 실시간 처리 성능 검증
- **규제 테스트**: IEC 62304 요구사항 검증

---

## 8. 디버깅 및 모니터링

### 8.1 모듈별 디버깅 특징

| 모듈 | 디버깅 특징 | 모니터링 항목 |
|------|-------------|-------------|
| `xpe_common.dll` | 로깅, 알릿 시스템 | 메모리 사용, 알릿 큐 |
| `xpe_preprocess.dll` | 보정 데이터 검증 | 보정 정확도, 성능 |
| `xpe_enhance_*.dll` | 이미지 품질 검증 | 향상 효과, 파라미터 |
| `xpe_ai.dll` | 워커 프로세스 모니터링 | GPU 사용, 추론 지연 |
| `xpe_display.dll` | LUT 적용 검증 | 디스플레이 정확도 |
| `xpe_dicom.dll` | DICOM 태그 검증 | 네트워크 지연, 오류율 |

### 8.2 진단 정보

- **로그 레벨**: TRACE, DEBUG, INFO, WARN, ERROR
- **알릿 시스템**: 경고, 오류, 디버그 정보
- **성능 모니터링**: 각 스테이지별 처리 시간
- **메모리 모니터링**: 메모리 사용량 추적

---

## 9. 모듈 배포

### 9.1 배포 단위

- **개별 DLL**: 각 모듈별 배포 가능
- **Phase 묶음**: Phase 1/2/3 단위로 배포
- **전체 패키지**: 모든 모듈 통합 배포

### 9.2 버전 관리

- **의존성 버전**: ABI 호환성 유지
- **배포 버전**:의존성과 함께 관리
- **롤백 지원**: 이전 버전으로 롤백 가능

---

## 10. 참고 문서

- `.moai/project/api-spec.md` - C ABI 참조 문서
- `.moai/project/pipeline-spec.md` - 파이프라인 명세
- `.moai/specs/xpe-algorithm-spec-deepsync.md` - 알고리즘 명세

---

*최종 업데이트: 2026-04-17*