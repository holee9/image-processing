# RTM-COMMON-001: 요구사항 추적 행렬 (Requirements Traceability Matrix)

**Document ID**: RTM-COMMON-001  
**Version**: 1.1.0  
**IEC 62304 Clause**: 5.1.1(c) — Requirements Traceability  
**Date**: 2026-10-03  
**Normative References**: SRS-COMMON-001, SAD-COMMON-001, SHA-COMMON-001

> **상태 메모 (2026-10-03, QA-A-231, #253)**: 이 RTM 은 SRS-COMMON-001 이 상정한 설계(메모리 풀, JSON 설정, 이벤트 시스템, 파라미터 검증기)를 기준으로 작성되었으며 현재 구현(`modules/common`)과 대응하지 않는다. §2~§4 의 52행이 인용한 시험·벤치마크 이름 52개 중 `modules/common/tests` 의 gtest 이름(81개)에 있는 것은 0개이고, `modules/common` 어디에도 문자열로 나오지 않는다. 인용한 구현 이름 `MemoryPool`·`JsonConfig`·`EventSystem`·`ErrorHandler` 도 소스에 없다(`ParameterValidator` 는 `xpe_common.cpp` 의 TODO 주석 한 줄에만 나온다). 구현된 공용 모듈은 `xpe_alloc_image`/`xpe_free_image`/`xpe_copy_image`, 알림 큐, `xpe_configure`, `xpe_get_param_range`, 로깅이다.
>
> 그래서 이 문서의 ✓ 를 모두 **△** 로 바꿨다. **△ = 인용한 시험 ID 를 소스에서 찾을 수 없음; 동작 자체는 다시 판정하지 않음.** △ 는 요구가 충족되지 않았다는 판정이 아니다 — 근거로 인용된 시험이 없다는 뜻이다. 구현된 기능에 맞춰 행을 다시 잇는 일(실제 시험 이름과 그 시험이 단언하는 내용을 행마다 적는 것)은 #253 에서 다룬다.
>
> | Version | Date | Description |
> |---------|------------|-------------|
> | 1.0.0 | 2026-04-14 | 초기 작성 |
> | 1.1.0 | 2026-10-03 | QA-A-231 / #253: 인용 시험이 소스에 없음을 확인 — §2~§4·§6·§7.1·§7.3·§8 의 ✓ 72개를 △ 로 변경, 상태 메모 추가. 요구·행은 삭제하지 않음 |

---

## 1. 목적

본 RTM은 SRS (Software Requirements Specification)의 요구사항을 SAD (Software Architecture Design)의 소프트웨어 단위(SWU), 위험 분석(SHA)의 위험 제어, 그리고 테스트 케이스와 양방향으로 추적한다.

### 1.1 추적성 방향

```
SRS 요구사항
    ↓ (분해)
SAD 소프트웨어 단위 (SWU)
    ↓ (구현)
테스트 케이스 (Test)
    ↓ (검증)
테스트 결과 (Pass/Fail)

역방향:
테스트 결과
    ↑ (검증)
SHA 위험 제어
    ↑ (마이그레이션)
SRS 안전 요구사항
```

---

## 2. 기능 요구사항 추적성 (FR-CMN-*)

| SRS ID | 제목 | SWU ID | 구현 | 테스트 케이스 | 위험 제어 | 상태 |
|--------|------|--------|------|-------------|---------|------|
| FR-CMN-100 | 메모리 풀 초기화 | 5.1 | MemoryPool::init() | test_mempool_alloc_free | HAZ-003 | △ |
| FR-CMN-101 | 메모리 할당 함수 | 5.1 | MemoryPool::alloc() | test_mempool_slot_reuse | HAZ-002 | △ |
| FR-CMN-102 | 메모리 해제 함수 | 5.1 | MemoryPool::free() | test_mempool_double_free | HAZ-002 | △ |
| FR-CMN-103 | 풀 통계 조회 | 5.1 | MemoryPool::get_stats() | test_mempool_stats | — | △ |
| FR-CMN-104 | 메모리 풀 정리 | 5.1 | MemoryPool::finalize() | test_mempool_cleanup | — | △ |
| FR-CMN-200 | XpeImage 구조체 | 5.2 | xpe_common.h | test_struct_size_pack8 | HAZ-001 | △ |
| FR-CMN-201 | XpeImageMetadata 구조체 | 5.2 | xpe_common.h | test_metadata_size | HAZ-001 | △ |
| FR-CMN-202 | PixelFormat 열거형 | 5.2 | xpe_common.h | test_pixelformat_enum | — | △ |
| FR-CMN-203 | XPE_FLAG_* 비트마스크 | 5.2 | xpe_common.h | test_flag_bitwise | HAZ-005 | △ |
| FR-CMN-204 | XpeRect 구조체 | 5.2 | xpe_common.h | test_rect_struct | — | △ |
| FR-CMN-300 | XpeError 열거형 | 5.3 | xpe_error.h | test_error_enum_coverage | — | △ |
| FR-CMN-301 | 스레드-로컬 에러 컨텍스트 | 5.3 | ErrorHandler::_xpe_error_context | test_error_thread_local | — | △ |
| FR-CMN-302 | 에러 조회 함수 | 5.3 | ErrorHandler::get_last_error() | test_error_get | — | △ |
| FR-CMN-303 | 에러 상세 정보 함수 | 5.3 | ErrorHandler::get_last_error_detail() | test_error_detail | — | △ |
| FR-CMN-304 | 에러 문자열 변환 | 5.3 | ErrorHandler::error_to_string() | test_error_string | — | △ |
| FR-CMN-400 | AlertType 열거형 | 5.4 | event_system.h | test_alert_type_enum | — | △ |
| FR-CMN-401 | Event System 초기화 | 5.4 | EventSystem::init() | test_event_init | HAZ-006 | △ |
| FR-CMN-402 | 콜백 등록 함수 | 5.4 | EventSystem::register_callback() | test_event_callback_register | HAZ-007 | △ |
| FR-CMN-403 | 알림 발송 함수 | 5.4 | EventSystem::emit_alert() | test_event_emit | HAZ-006 | △ |
| FR-CMN-404 | 큐 오버플로우 처리 | 5.4 | EventSystem::emit_alert() | test_event_overflow | HAZ-006 | △ |
| FR-CMN-405 | Event System 통계 조회 | 5.4 | EventSystem::get_queue_stats() | test_event_stats | — | △ |
| FR-CMN-500 | 설정 로드 함수 | 5.5 | JsonConfig::load() | test_config_load | HAZ-004 | △ |
| FR-CMN-501 | 기본 경로 우선순위 | 5.5 | JsonConfig::load() | test_config_path_priority | — | △ |
| FR-CMN-502 | 설정 스키마 검증 | 5.5 | JsonConfig::validate_schema() | test_config_schema | HAZ-004 | △ |
| FR-CMN-503 | 설정 조회 함수 | 5.5 | JsonConfig::get_*() | test_config_get | — | △ |
| FR-CMN-504 | 설정 쓰기 함수 | 5.5 | JsonConfig::set_*() | test_config_set | — | △ |
| FR-CMN-505 | 핫-리로드 | 5.5 | JsonConfig::reload() | test_config_reload | HAZ-004 | △ |
| FR-CMN-600 | 이미지 매개변수 검증 | 5.6 | ParameterValidator::validate_image_params() | test_param_validate | — | △ |
| FR-CMN-601 | kVp 검증 | 5.6 | ParameterValidator | test_param_kvp_range | — | △ |
| FR-CMN-602 | mAs 검증 | 5.6 | ParameterValidator | test_param_mas_range | — | △ |
| FR-CMN-603 | sdd 검증 | 5.6 | ParameterValidator | test_param_sdd_range | — | △ |
| FR-CMN-604 | 온도 검증 | 5.6 | ParameterValidator | test_param_temp_range | — | △ |
| FR-CMN-605 | 픽셀 간격 검증 | 5.6 | ParameterValidator | test_param_pixel_range | — | △ |
| FR-CMN-606 | 파이프라인 설정 검증 | 5.6 | ParameterValidator::validate_pipeline_config() | test_param_pipeline_config | — | △ |
| FR-CMN-700 | C 호출 규약 | 5.7 | extern "C" declarations | test_pinvoke_cdecl | — | △ |
| FR-CMN-701 | Pack=8 마샬링 | 5.7 | StructLayout(Pack=8) | test_pinvoke_pack8 | HAZ-001 | △ |
| FR-CMN-702 | 콜백 마샬링 | 5.7 | delegate definitions | test_pinvoke_callback | HAZ-007 | △ |

---

## 3. 안전 요구사항 추적성 (SAF-CMN-*)

| SRS ID | 제목 | 관련 위험 | 위험 제어 (SAD 참조) | 테스트 | 상태 |
|--------|------|---------|-------------------|--------|------|
| SAF-CMN-100 | 이중 해제 방지 | HAZ-002 | 포인터 검증 (SAD §3.1) | test_mempool_invalid_free | △ |
| SAF-CMN-110 | Pack=8 정렬 검증 | HAZ-001 | static_assert (SAD §3.2) | test_pack8_alignment | △ |
| SAF-CMN-120 | XPE_FLAG 사용 규칙 | HAZ-005 | 플래그 설정 규칙 (SAD §3.2) | test_flag_lifecycle | △ |
| SAF-CMN-130 | Null 포인터 방지 | — | 모든 함수 입력 검증 (SAD §3.*) | test_null_ptr_checks | △ |
| SAF-CMN-140 | 설정 핫-리로드 안전성 | HAZ-004 | 원자적 교체 (SAD §3.5) | test_config_atomic_reload | △ |
| SAF-CMN-150 | Event Queue 오버플로우 처리 | HAZ-006 | FIFO 대체 (SAD §3.4) | test_event_queue_overflow | △ |
| SAF-CMN-160 | 스레드 안전성 | — | mutex, rwlock, TLS (SAD §3.*) | test_thread_safety | △ |

---

## 4. 성능 요구사항 추적성 (PERF-CMN-*)

| SRS ID | 제목 | SWU | 목표 | 테스트 | 합격 기준 | 상태 |
|--------|------|-----|------|--------|----------|------|
| PERF-CMN-100 | 메모리 할당 응답 | 5.1 | < 1 ms | benchmark_mempool_alloc | ≤ 1ms | △ |
| PERF-CMN-101 | 풀 초기화 시간 | 5.1 | < 100 ms | benchmark_mempool_init | ≤ 100ms | △ |
| PERF-CMN-110 | 설정 로드 시간 | 5.5 | < 20 ms | benchmark_config_load | ≤ 20ms | △ |
| PERF-CMN-111 | 핫-리로드 시간 | 5.5 | < 50 ms | benchmark_config_reload | ≤ 50ms | △ |
| PERF-CMN-120 | 매개변수 검증 | 5.6 | < 5 ms | benchmark_param_validate | ≤ 5ms | △ |
| PERF-CMN-130 | Event 발송 시간 | 5.4 | < 1 ms | benchmark_event_emit | ≤ 1ms | △ |
| PERF-CMN-140 | 에러 조회 시간 | 5.3 | < 0.1 ms | benchmark_error_get | ≤ 0.1ms | △ |
| PERF-CMN-150 | 메모리 제한 | 5.1 | ≤ 226.4 MB | test_mempool_size | ≤ 226.4MB | △ |

---

## 5. 테스트 케이스 - SWU별 맵핑

### 5.1 SWU-5.1: MemoryPool

#### 단위 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| UT-MEM-001 | test_mempool_init | 초기화 성공, 8개 슬롯 확인 | FR-CMN-100 |
| UT-MEM-002 | test_mempool_alloc_float32 | float32 슬롯 할당 | FR-CMN-101 |
| UT-MEM-003 | test_mempool_alloc_uint16 | uint16 슬롯 할당 | FR-CMN-101 |
| UT-MEM-004 | test_mempool_free_valid | 유효 포인터 해제 | FR-CMN-102 |
| UT-MEM-005 | test_mempool_double_free | 이중 해제 감지 | SAF-CMN-100 |
| UT-MEM-006 | test_mempool_null_free | NULL 포인터 해제 오류 | SAF-CMN-130 |
| UT-MEM-007 | test_mempool_exhausted | 슬롯 고갈 시 XPE_ERR_POOL_EXHAUSTED | FR-CMN-101 |
| UT-MEM-008 | test_mempool_invalid_size | 잘못된 크기 요청 | FR-CMN-101 |
| UT-MEM-009 | test_mempool_stats | 통계 JSON 반환 | FR-CMN-103 |
| UT-MEM-010 | test_mempool_cleanup | 종료 시 리소스 해제 | FR-CMN-104 |

#### 통합 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| IT-MEM-001 | test_mempool_slot_reuse | 해제 후 재할당 | FR-CMN-101 |
| IT-MEM-002 | test_mempool_refcount | 참조 카운팅 정확성 | FR-CMN-102 |
| IT-MEM-003 | test_mempool_zero_copy | 포인터 일관성 | SAD §3.1 |

### 5.2 SWU-5.2: TypeDefinitions

#### 단위 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| UT-TYPE-001 | test_struct_size_xpeimage | sizeof(XpeImage) == 240 | FR-CMN-200, SAF-CMN-110 |
| UT-TYPE-002 | test_struct_size_metadata | sizeof(XpeImageMetadata) == 192 | FR-CMN-201 |
| UT-TYPE-003 | test_struct_alignment | 모든 필드 offset 검증 (static_assert) | SAF-CMN-110 |
| UT-TYPE-004 | test_pixelformat_values | UINT16=0, FLOAT32=1 | FR-CMN-202 |
| UT-TYPE-005 | test_flag_values | 10개 플래그 값 검증 | FR-CMN-203 |
| UT-TYPE-006 | test_flag_bitwise_ops | OR, AND 연산 검증 | FR-CMN-203 |

#### 통합 테스트 (P/Invoke)

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| IT-TYPE-001 | test_pinvoke_pack8 | C#-C++ struct 레이아웃 일치 | FR-CMN-701 |
| IT-TYPE-002 | test_pinvoke_field_offsets | 모든 필드 오프셋 일치 | HAZ-001 |

### 5.3 SWU-5.3: ErrorHandler

#### 단위 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| UT-ERR-001 | test_error_set_and_get | 에러 설정 및 조회 | FR-CMN-302 |
| UT-ERR-002 | test_error_detail | 상세 정보 반환 | FR-CMN-303 |
| UT-ERR-003 | test_error_string | 에러 코드 → 문자열 | FR-CMN-304 |
| UT-ERR-004 | test_error_thread_local | 스레드별 격리 | FR-CMN-301 |
| UT-ERR-005 | test_error_clear | 에러 상태 초기화 | FR-CMN-301 |

#### 스레드 안전 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| IT-ERR-001 | test_error_multithread | 다중 스레드 에러 격리 | SAF-CMN-160 |

### 5.4 SWU-5.4: NotificationSystem

#### 단위 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| UT-EVT-001 | test_event_init | Event System 초기화 | FR-CMN-401 |
| UT-EVT-002 | test_event_register_callback | 콜백 등록 | FR-CMN-402 |
| UT-EVT-003 | test_event_emit_alert | 알림 발송 (논블로킹) | FR-CMN-403 |
| UT-EVT-004 | test_event_queue_capacity | 256개 용량 확인 | FR-CMN-403 |
| UT-EVT-005 | test_event_overflow | 오버플로우 처리 | FR-CMN-404 |
| UT-EVT-006 | test_event_stats | 통계 JSON 반환 | FR-CMN-405 |

#### 통합 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| IT-EVT-001 | test_event_callback_delivery | 콜백 호출 검증 | FR-CMN-402 |
| IT-EVT-002 | test_event_multithread | 다중 생산자 + 소비자 | SAF-CMN-160 |

### 5.5 SWU-5.5: JsonConfig

#### 단위 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| UT-CFG-001 | test_config_load_valid | 유효 JSON 로드 | FR-CMN-500 |
| UT-CFG-002 | test_config_load_missing | 파일 없음 → XPE_ERR_FILE_NOT_FOUND | FR-CMN-500 |
| UT-CFG-003 | test_config_parse_error | 잘못된 JSON | FR-CMN-500 |
| UT-CFG-004 | test_config_schema_validate | 필수 키 검증 | FR-CMN-502 |
| UT-CFG-005 | test_config_get_string | 문자열 조회 | FR-CMN-503 |
| UT-CFG-006 | test_config_get_float | 실수 조회 | FR-CMN-503 |
| UT-CFG-007 | test_config_set_string | 문자열 설정 | FR-CMN-504 |
| UT-CFG-008 | test_config_reload | 핫-리로드 | FR-CMN-505 |

#### 통합 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| IT-CFG-001 | test_config_path_priority | 경로 우선순위 (env > default) | FR-CMN-501 |
| IT-CFG-002 | test_config_atomic_reload | 원자적 업데이트 | SAF-CMN-140 |

### 5.6 SWU-5.6: ParameterValidator

#### 단위 테스트

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| UT-VAL-001 | test_param_kvp_valid | kVp ∈ [40, 150] | FR-CMN-601 |
| UT-VAL-002 | test_param_kvp_low | kVp < 40 → 오류 | FR-CMN-601 |
| UT-VAL-003 | test_param_kvp_high | kVp > 150 → 오류 | FR-CMN-601 |
| UT-VAL-004 | test_param_mas_valid | mAs ∈ [0.1, 500] | FR-CMN-602 |
| UT-VAL-005 | test_param_sdd_valid | sdd ∈ [400, 1500] | FR-CMN-603 |
| UT-VAL-006 | test_param_temp_valid | temp ∈ [-10, 85] | FR-CMN-604 |
| UT-VAL-007 | test_param_pixel_valid | pixelSpacing ∈ [0.1, 0.5] | FR-CMN-605 |
| UT-VAL-008 | test_param_image_null | NULL 이미지 → 오류 | SAF-CMN-130 |
| UT-VAL-009 | test_param_dimensions | 해상도 ∈ [512, 4096] | FR-CMN-600 |

### 5.7 SWU-5.7: PipelineOrchestrator

#### 통합 테스트 (P/Invoke)

| 테스트 ID | 이름 | 검증 | SRS 추적 |
|----------|------|------|---------|
| IT-PINVOKE-001 | test_pinvoke_cdecl | C 호출 규약 | FR-CMN-700 |
| IT-PINVOKE-002 | test_pinvoke_pack8_struct | struct 마샬링 | FR-CMN-701 |
| IT-PINVOKE-003 | test_pinvoke_callback | 콜백 마샬링 | FR-CMN-702 |
| IT-PINVOKE-004 | test_pinvoke_error_handling | P/Invoke 오류 처리 | SAF-CMN-130 |

---

## 6. 위험 제어 - 테스트 맵핑

| 위험 ID | 테스트 | 확인 | 상태 |
|--------|--------|------|------|
| HAZ-001 | test_pack8_alignment, test_pinvoke_pack8 | struct 정렬 검증 | △ |
| HAZ-002 | test_mempool_double_free, test_mempool_invalid_free | 포인터 검증 | △ |
| HAZ-003 | test_mempool_exhausted, IT-MEM-003 | 흐름 제어 | △ |
| HAZ-004 | test_config_atomic_reload, test_config_schema | 핫-리로드 안전 | △ |
| HAZ-005 | test_flag_lifecycle, test_flag_bitwise_ops | 플래그 규칙 | △ |
| HAZ-006 | test_event_overflow, IT-EVT-002 | 큐 오버플로우 | △ |
| HAZ-007 | test_pinvoke_callback, IT-EVT-001 | 콜백 안전 | △ |

---

## 7. 양방향 추적성 검증

### 7.1 SRS → 테스트 (Forward Traceability)

```
모든 SRS 요구사항 (42개)
  ├─ 테스트 케이스 할당 (40개 ≥ 42개 × 90% = 37.8개) △
  └─ 미할당: 2개 (선택사항 또는 통합)
```

### 7.2 테스트 → SRS (Backward Traceability)

```
모든 테스트 케이스 (40개)
  ├─ SRS 요구사항에 추적됨 (38개)
  ├─ 보너스 테스트 (2개) - 추가 검증
  └─ 불필요한 테스트: 0개
```

### 7.3 GAP 분석

| 요구사항 | 테스트 | 상태 |
|---------|--------|------|
| FR-CMN-100~106 (MemoryPool) | UT-MEM-001~010, IT-MEM-001~003 | △ |
| FR-CMN-200~204 (TypeDef) | UT-TYPE-001~006, IT-TYPE-001~002 | △ |
| FR-CMN-300~304 (ErrorHandler) | UT-ERR-001~005, IT-ERR-001 | △ |
| FR-CMN-400~405 (XPE Event System) | UT-EVT-001~006, IT-EVT-001~002 | △ |
| FR-CMN-500~505 (JsonConfig) | UT-CFG-001~008, IT-CFG-001~002 | △ |
| FR-CMN-600~606 (ParamValidator) | UT-VAL-001~009 | △ |
| FR-CMN-700~702 (P/Invoke) | IT-PINVOKE-001~004 | △ |

---

## 8. 추적성 메트릭

| 메트릭 | 값 | 목표 | 상태 |
|--------|-----|------|------|
| **요구사항 커버리지** | 42/42 (100%) | ≥ 95% | △ |
| **테스트 케이스 수** | 40개 | ≥ 30개 | △ |
| **추적된 테스트** | 40/40 (100%) | ≥ 95% | △ |
| **위험 제어 커버리지** | 7/7 (100%) | ≥ 100% | △ |
| **양방향 추적성** | 100% | ≥ 100% | △ |

---

## 9. 추적성 유지 방법

### 9.1 변경 관리

1. **요구사항 변경**: SRS 수정 → RTM 업데이트 → 테스트 추가
2. **테스트 추가**: 테스트 케이스 작성 → SRS 추적 추가
3. **위험 변경**: SHA 수정 → 테스트 업데이트 → RTM 반영

### 9.2 정기 검증

- **월별**: RTM 검토 (요구사항 누락 확인)
- **분기별**: 테스트 케이스 재평가 (커버리지 확인)
- **연간**: 전체 추적성 감사

---

**RTM-COMMON-001 v1.1.0 끝**
