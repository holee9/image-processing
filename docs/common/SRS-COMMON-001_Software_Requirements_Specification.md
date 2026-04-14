# SRS-COMMON-001: xpe_common.dll 소프트웨어 요구사항 명세서

**Document ID**: SRS-COMMON-001  
**Version**: 1.0.0  
**IEC 62304 Clause**: 5.2 — Software Requirements Specification  
**Safety Classification**: Class B  
**Date**: 2026-04-14  
**Normative Reference**: PRD (xpe-common-prd.md)

---

## 1. 개요

본 문서는 `xpe_common.dll` (Layer 0, 기반 인프라)의 소프트웨어 요구사항을 정의한다. 기능 요구사항 (FR), 안전 요구사항 (SAF), 성능 요구사항 (PERF)의 3가지 범주로 분류하며, 각 요구사항은 PRD의 7개 SWU와 추적 가능하다.

---

## 2. 기능 요구사항 (FR-CMN-*)

### 2.1 메모리 풀 (SWU-5.1)

**FR-CMN-100**: 메모리 풀 초기화
- 시작 시 float32 슬롯 4개 (각 37.7 MB) 할당
- 시작 시 uint16 슬롯 4개 (각 18.9 MB) 할당
- 총 226.4 MB 메모리 사전 할당
- 실패 시: `XPE_ERR_ALLOCATION_FAILED` 반환

**FR-CMN-101**: 메모리 할당 함수 (`xpe_mempool_alloc`)
- 입력: `width` (uint32_t), `height` (uint32_t), `format` (PixelFormat)
- 요청 크기와 슬롯 크기 일치 확인
- 일치 시: 사용 가능 슬롯 할당, 포인터 반환
- 불일치: `XPE_ERR_INVALID_SIZE` 반환
- 슬롯 부족: `XPE_ERR_POOL_EXHAUSTED` 반환

**FR-CMN-102**: 메모리 해제 함수 (`xpe_mempool_free`)
- 입력: `void* ptr` (할당된 슬롯)
- 참조 카운팅 감소
- 참조 카운트 = 0일 때: 슬롯을 사용 가능 상태로 변경
- 안전성: 이중 해제 감지, `XPE_ERR_INVALID_INPUT` 반환

**FR-CMN-103**: 풀 통계 조회 (`xpe_mempool_get_stats`)
- 반환: JSON 문자열
- 포함 정보: `{"allocated": 2, "available": 2, "peak": 4, "memory_bytes": 226400000}`
- 스레드 안전: mutex로 보호된 카운터 읽기

**FR-CMN-104**: 메모리 풀 정리 (`xpe_mempool_finalize`)
- 모든 슬롯 해제, 메모리 반환
- 프로세스 종료 시 호출
- 할당된 슬롯이 남아 있으면 경고

### 2.2 타입 정의 (SWU-5.2)

**FR-CMN-200**: XpeImage 구조체
- 크기: 정확히 240 바이트
- Pack=8 정렬 준수
- C# P/Invoke와 호환 가능
- static_assert로 오프셋 검증 필수

**FR-CMN-201**: XpeImageMetadata 구조체
- 크기: 정확히 192 바이트
- 필드: bodyPart, kVp, mAs, sdd, pixelSpacingMm, acquisitionTime, detectorId, temperature, reserved
- 모든 수치 필드 초기화 (0 또는 기본값)

**FR-CMN-202**: PixelFormat 열거형
- UINT16 = 0 (16-bit unsigned, 원본 ADC)
- FLOAT32 = 1 (32-bit float, 보정된 데이터)
- 다른 값 금지

**FR-CMN-203**: XPE_FLAG_* 비트마스크
- 10개 정의된 플래그 (0x0001 ~ 0x0200)
- 비트 연산만 허용 (|, &, ^)
- 다운스트림 DLL이 플래그 상태 확인 가능

**FR-CMN-204**: XpeRect 구조체 (선택)
- 필드: x, y, width, height (모두 uint32_t)
- 크기: 16 바이트 (Pack=8 호환)

### 2.3 에러 처리 (SWU-5.3)

**FR-CMN-300**: XpeError 열거형
- 50개 이상의 에러 코드 정의
- 범주별 분류: 입력(100~109), 메모리(110~119), 초기화(120~129), 캘리브(130~149), 파이프라인(150~159), 설정(160~169), 매개변수(170~179), I/O(180~189), AED(190~199)
- 성공: `XPE_OK = 0`

**FR-CMN-301**: 스레드-로컬 에러 컨텍스트
- 각 스레드가 자신의 에러 상태 유지
- 전역 변수 사용 금지 (__thread 또는 thread_local)
- 메시지, 파일명, 라인 번호, 타임스탬프 저장

**FR-CMN-302**: 에러 조회 함수 (`xpe_get_last_error`)
- 반환: 마지막 XpeError 코드
- 스레드 안전: 각 스레드의 TLS 읽기

**FR-CMN-303**: 에러 상세 정보 함수 (`xpe_get_last_error_detail`)
- 반환: XpeErrorDetail 구조체 포인터
- 포함: code, message, filename, lineNumber, timestamp, context
- 유효 기간: 다음 에러까지

**FR-CMN-304**: 에러 문자열 변환 (`xpe_error_to_string`)
- 입력: XpeError 코드
- 출력: 사람이 읽을 수 있는 영문 메시지
- 범위 밖 코드: "Unknown error"

### 2.4 알림 시스템 (SWU-5.4)

**FR-CMN-400**: AlertType 열거형
- INFO = 0, WARNING = 1, ERROR = 2, CALIBRATION_NEEDED = 3, AI_UNAVAILABLE = 4

**FR-CMN-401**: 비동기 이벤트 디스패처 (AED) 초기화
- 원형 버퍼 (circular buffer) 크기: 256 알림
- 전용 알림 스레드 생성
- 스레드 안전: mutex로 보호된 큐

**FR-CMN-402**: 콜백 등록 함수 (`xpe_aed_register_callback`)
- 입력: 함수 포인터, 사용자 데이터
- 저장: 최대 8개 콜백 동시 등록 (또는 제한 없음)
- 반환: 콜백 ID 또는 성공 코드

**FR-CMN-403**: 알림 발송 함수 (`xpe_aed_emit_alert`)
- 입력: AlertType, 메시지 문자열
- 동작: 큐에 추가 (논블로킹)
- 반환: 즉시 (큐 추가 성공/실패)
- 실패: `XPE_ERR_AED_QUEUE_FULL`

**FR-CMN-404**: 큐 오버플로우 처리
- 256개 이상의 알림 시: 가장 오래된 항목 제거 (FIFO 대체)
- 삭제 카운트 메트릭 기록
- C#에 경고 알림 발송

**FR-CMN-405**: AED 통계 조회 (`xpe_aed_get_queue_stats`)
- 반환: JSON, `{"pending": 5, "max_capacity": 256, "discarded": 2}`

### 2.5 JSON 설정 (SWU-5.5)

**FR-CMN-500**: 설정 로드 함수 (`xpe_config_load`)
- 입력: 파일 경로
- 동작: JSON 파일 읽기, 파싱, 검증
- 성공: 설정 메모리에 저장
- 실패: `XPE_ERR_CONFIG_INVALID` 또는 `XPE_ERR_FILE_NOT_FOUND`

**FR-CMN-501**: 기본 경로 우선순위
- 1차: 환경 변수 `XPE_CONFIG_PATH`
- 2차: `./config/xpe_config.json`
- 3차: 하드코딩된 기본값

**FR-CMN-502**: 설정 스키마 검증
- 필수 키: `pipeline`, `pipeline.flags_enabled`
- 선택 키: `ai`, `display`, `calibration`
- 미지의 키: 무시 (미래 호환성)

**FR-CMN-503**: 설정 조회 함수
- `xpe_config_get_string(key)` → const char*
- `xpe_config_get_float(key)` → float
- `xpe_config_get_int(key)` → int
- 키 없음: NULL 또는 기본값 반환

**FR-CMN-504**: 설정 쓰기 함수 (`xpe_config_set_*`)
- 메모리의 설정 변경 (파일 미변경)
- 반환: `XPE_OK` 또는 `XPE_ERR_INVALID_INPUT`

**FR-CMN-505**: 핫-리로드 (`xpe_config_reload`)
- 디스크에서 파일 재읽기
- JSON 재파싱 및 검증
- 실패 시: 이전 설정 유지 (자동 롤백)

### 2.6 매개변수 검증 (SWU-5.6)

**FR-CMN-600**: 이미지 매개변수 검증 (`xpe_validate_image_params`)
- 입력: XpeImage 포인터
- 검사: 모든 필드 범위 검증
- 반환: `XPE_OK` 또는 `XPE_ERR_PARAM_OUT_OF_RANGE`

**FR-CMN-601**: kVp 검증 [40, 150] kV

**FR-CMN-602**: mAs 검증 [0.1, 500] mAs

**FR-CMN-603**: sdd 검증 [400, 1500] mm

**FR-CMN-604**: 검출기 온도 검증 [-10, 85] °C

**FR-CMN-605**: 픽셀 간격 검증 [0.1, 0.5] mm

**FR-CMN-606**: 파이프라인 설정 검증 (`xpe_validate_pipeline_config`)
- 입력: JSON 문자열
- 검사: 모든 구성 값 유효 범위 확인
- 반환: `XPE_OK` 또는 `XPE_ERR_CONFIG_INVALID`

### 2.7 P/Invoke 브리지 (SWU-5.7)

**FR-CMN-700**: 모든 C 함수 C 호출 규약 (Cdecl)
- `extern "C"` 선언
- Windows DLL export
- C#에서 `CallingConvention.Cdecl` 호환

**FR-CMN-701**: Pack=8 struct 마샬링
- C# StructLayout(Pack=8)으로 정의
- MarshalAs 속성으로 필드 마샬링
- 바이트-정확한 정렬 보장

**FR-CMN-702**: 콜백 마샬링
- C# delegate 정의
- `xpe_aed_register_callback`에 전달
- 호출 규약: Cdecl

---

## 3. 안전 요구사항 (SAF-CMN-*)

**SAF-CMN-100**: 메모리 풀 이중 해제 방지
- `xpe_mempool_free(ptr)` 호출 시 포인터 유효성 검증
- 유효하지 않은 포인터: `XPE_ERR_INVALID_INPUT` 반환
- 힙 손상 방지

**SAF-CMN-110**: Pack=8 정렬 검증
- 모든 struct에 static_assert 추가
- 빌드 시 오프셋/크기 검증
- P/Invoke 호환성 보장

**SAF-CMN-120**: XPE_FLAG 사용 규칙
- 플래그는 OR 연산으로만 설정 (|=)
- 다운스트림이 우회된 단계를 식별 가능
- 우회된 단계의 플래그는 설정하지 않음

**SAF-CMN-130**: Null 포인터 방지
- 모든 함수 입력: NULL 확인
- 반환: `XPE_ERR_NULL_POINTER`
- 특히 XpeImage.data 포인터 검증

**SAF-CMN-140**: 설정 핫-리로드 안전성
- 파일 읽기 중 메모리 설정 불변성 보장
- 검증 실패 시: 이전 설정 유지 (원자적 교체)
- 경합 조건 방지: mutex 사용

**SAF-CMN-150**: AED 큐 오버플로우 처리
- 256개 이상 시: 가장 오래된 항목 제거
- 안전한 메모리 해제 (메모리 누수 금지)
- C#에 경고 알림 발송

**SAF-CMN-160**: 스레드 안전성
- 에러 컨텍스트: 스레드-로컬
- AED 큐: mutex로 보호
- 설정 접근: rwlock 또는 원자적 포인터

---

## 4. 성능 요구사항 (PERF-CMN-*)

**PERF-CMN-100**: 메모리 풀 할당 응답 시간
- 목표: < 1 ms
- 구현: O(1) 시간에 사용 가능 슬롯 찾기 (비트마스크 또는 리스트)

**PERF-CMN-101**: 메모리 풀 초기화 소요 시간
- 목표: < 100 ms (226.4 MB 할당)
- 시작 시 일회만 실행

**PERF-CMN-110**: 설정 로드 소요 시간
- 목표: < 20 ms
- JSON 파싱 라이브러리: 경량 (예: cJSON, nlohmann/json)

**PERF-CMN-111**: 설정 핫-리로드 소요 시간
- 목표: < 50 ms
- 검증 실패 시: 이전 설정 빠르게 복구

**PERF-CMN-120**: 매개변수 검증 소요 시간
- 목표: < 5 ms
- 구현: 간단한 범위 검사 (조건문)

**PERF-CMN-130**: AED 알림 발송 (xpe_aed_emit_alert)
- 목표: < 1 ms
- 동작: 큐 쓰기만 (논블로킹)
- 콜백 호출은 별도 스레드에서 비동기 수행

**PERF-CMN-140**: 에러 조회 응답 시간
- 목표: < 0.1 ms
- 구현: TLS 메모리 읽기만

**PERF-CMN-150**: 메모리 사용량 제한
- 총 피크: 226.4 MB (메모리 풀)
- 설정 JSON: < 1 MB
- 에러 컨텍스트: < 2 KB / 스레드
- TLS 오버헤드 최소화

---

## 5. 요구사항 추적성 테이블

| 요구사항 ID | SWU | 제목 | 상태 | 검증 방법 |
|-----------|-----|------|------|---------|
| FR-CMN-100 | 5.1 | 메모리 풀 초기화 | ACTIVE | Unit test |
| FR-CMN-101 | 5.1 | 할당 함수 | ACTIVE | Unit test |
| FR-CMN-200 | 5.2 | XpeImage 구조체 | ACTIVE | static_assert |
| FR-CMN-300 | 5.3 | XpeError 열거형 | ACTIVE | Header check |
| FR-CMN-400 | 5.4 | AED 초기화 | ACTIVE | Unit test |
| FR-CMN-500 | 5.5 | 설정 로드 | ACTIVE | Unit test |
| FR-CMN-600 | 5.6 | 매개변수 검증 | ACTIVE | Unit test |
| FR-CMN-700 | 5.7 | P/Invoke 호환성 | ACTIVE | P/Invoke test |
| SAF-CMN-100 | 5.1 | 이중 해제 방지 | ACTIVE | Integration test |
| SAF-CMN-110 | 5.2 | Pack=8 검증 | ACTIVE | static_assert |
| PERF-CMN-100 | 5.1 | 할당 응답 시간 | ACTIVE | Benchmarking |

---

**SRS-COMMON-001 v1.0.0 끝**
