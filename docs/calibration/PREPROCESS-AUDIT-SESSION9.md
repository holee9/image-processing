# 전처리(xpe-pre) 정밀 감사 보고서 — 세션 9차

**일시**: 2026-04-28
**범위**: `modules/preprocess/` 전체 (소스 33개, 테스트 39개)
**방법**: 5개 독립 expert-backend 에이전트 교차검증 (Offset/Gain/Defect/Calibration/Pipeline)
**기준**: SPEC-XPE-P1A, IEC 62304 Class B

---

## 1. 검증 결과 요약

### 알고리즘 정상 구현 확인

| 알고리즘 | 상태 | 교차검증 에이전트 수 | 비고 |
|----------|:----:|:-------------------:|------|
| Gain Correction | 정상 | 3/3 | 곱셈 보정, uint16→float32 전환, NaN/Inf 검증 모두 정확 |
| Ghost Correction (3-Tier) | 정상 | 2/2 | LTI/NLCSC deconvolution, 시간 의존 감쇠 계산 정확 |
| Pipeline 순서 | 정상 | 3/3 | SPEC-XPE-P1A 스테이지 순서 일치 |
| Buffer 관리 | 정상 | 3/3 | uint16→float32 도메인 전환 정확 (Stage 4) |
| Bypass 플래그 | 정상 | 2/2 | 8개 bypass 모두 정상 동작 |
| SIMD Dispatch | 정상 | 2/2 | AVX-512→AVX2→NEON→Scalar 계층 구조 정상 |
| Binning Correction | 정상 | 1/1 | 1/mode^2 정규화 정확 |
| Temperature Compensation | 정상 | 2/2 | 물리 모델 정확, 스테이지 배치 적절 |
| XCal 포맷 | 정상 | 1/1 | Reader/Writer/Validator 일관성 확보 |
| SHA-256 무결성 | 정상 | 1/1 | FIPS 180-4 KAT 통과 |

### 발견된 이슈 (P0: 5건, P1: 8건, P2: 5건)

---

## 2. P0 — 즉시 조치 필요

### P0-OFF-01: Offset 이중 구현 — 라운딩 불일치

**발견**: 2/5 에이전트 교차확인

`xpe_offset_correct` 심볼이 두 파일에 정의되어 링커가 비결정적으로 하나를 선택합니다.

| 파일 | 라인 | 라운딩 |
|------|------|--------|
| `offset_correct.cpp` | L267 | `+0.5f` 반올림 → `uint16_t(v + 0.5f)` |
| `xpe_offset.cpp` | L72 | 절삭 → `uint16_t(corrected)` |

**영향**: 동일 입력에 대해 실행 환경에 따라 다른 결과 발생 가능

**Worktree**: xpe-pre (`dev/preprocess`) — Issue [#68](https://github.com/holee9/image-processing/issues/68)

### P0-OFF-02: Offset SIMD 커널 정의되었으나 미사용

**발견**: 2/5 에이전트 교차확인

`offset_correct.cpp`에 AVX-512(L57-86), AVX2(L98-127), NEON(L196-225) 커널이 정의되어 있으나, 공개 API(L263-268)는 스칼라 float 루프만 실행합니다.

**영향**: 3072x3072 기준 SIMD 최적화 미적용, <15ms 성능 목표 달성 불가

**Worktree**: Lane A (Pre)

### P0-CAL-01: Offset multi-method 미구현

**발견**: 1/5 에이전트 (calibration 전문)

`xpe_calib_generate_offset.cpp`는 mean 방식만 구현. `test_calib_generate_offset_multi.cpp`의 median, sigma_clip, winsor 테스트 20개가 구현 없이 작성됨.

**영향**: FUNC-032 무효, XpeCalibrationMode의 multi-method 옵션이 작동하지 않음

**Worktree**: xpe-pre (`dev/preprocess`) — Issue [#69](https://github.com/holee9/image-processing/issues/69)

### P0-CAL-02: Calibration Cache 스레드 안전성 위반

**발견**: 1/5 에이전트 (calibration 전문)

`calibration_cache.cpp`의 `CalibrationLRUCache`:
- `std::mutex` 없이 `lru_` 리스트와 `index_` 맵 수정
- `test_xpe_calib_endurance.cpp`가 4개 동시 스레드로 캐시 접근 테스트
- IEC 62304 Class B 의료기기 소프트웨어에서 데이터 레이스는 미정의 동작

**영향**: 힙 손상, 크래시, 캘리브레이션 데이터 오염 가능

**Worktree**: Lane B (Calib)

### P0-DEF-01: Defect bilinear 보간 구현 상태 불명확

**발견**: 1/5 에이전트 (defect 전문)

`defect_correct.cpp`에서 `xpe_interpolate_pixel()` 호출부 존재. 해당 함수의 실제 구현 여부와 호출부-구현부 일치 여부에 대해 에이전트 간 의견 차이가 있어 **직접 코드 확인이 필요**합니다.

**영향**: 결함 보정 핵심 알고리즘이 미작동일 가능성

**Worktree**: xpe-pre (`dev/preprocess`) — Issue [#70](https://github.com/holee9/image-processing/issues/70)

---

## 3. P1 — 후속 조치

### P1-OFF-03: Offset 테스트 2-arg API 호출

`test_offset_correct.cpp:52`가 2-arg API를 호출하나 현재 API는 3-arg.

### P1-GAIN-01: Gain 이중 구현 동작 분기

`gain_correct.cpp`는 null 게인맵 시 `XPE_ERR_NOT_INITIALIZED` 반환, `xpe_gain.cpp`는 pass-through 수행.

### P1-CAL-03: Defect map 만료 검증 우회

`xpe_calib_load_defect_map.cpp:33-36`이 `check_expiry=false`로 고정. offset/gain은 만료 검증 수행.

### P1-CAL-04: R² 품질 회귀 로깅 미구현

`xpe_calib_mode.cpp:288`에 TODO 코멘트만 존재.

### P1-CAL-05: Polynomial gain 생성 검증 불충분

테스트가 `XPE_OK`와 `XPE_ERR_IO_FAILED` 모두 허용.

### P1-DEF-02: Reflect padding 공식 오류 가능성

`xpe_defect_gen.cpp:179` 하단 반사 공식이 첫 행을 건너뛸 가능성.

### P1-DEF-03: Hampel 5-sigma 검출 구현 불명확

`runtime_detection.cpp:170`의 `DetectDefectivePixel()` 구현 상태 불명확, 테스트도 skip 처리.

### P1-OFF-04: AVX2 parity 테스트 비활성화

Offset/Gain/Defect 모두 `GTEST_SKIP()` 처리되어 SIMD-scalar 패리티가 CI에서 검증되지 않음.

---

## 4. P2 — 개선 권장

| ID | 항목 | 파일 |
|----|------|------|
| P2-DEF-04 | BPM merge bitwise OR → explicit 매핑 | `xpe_defect_gen.cpp:365` |
| P2-DEF-05 | 전체 결함 이웃 시 0.0f 반환 대안 마련 | `defect_correct.cpp:95` |
| P2-CAL-06 | 미사용 파라미터 정리 (integration_time_ms, temperature_c) | `xpe_calib_generate_offset.cpp:38-39` |
| P2-PIPE-01 | Nonlinearity correction stub → 백로그 등록 | `nonlinearity_correct.cpp:42-46` |
| P2-PIPE-02 | REQ-P1A-XXX 플레이스홀더 교체 | `xpe_verify_metrics.cpp:147,250,360,445` |

---

## 5. Worktree 분류

모든 이슈는 `modules/preprocess/`에 해당하므로 **xpe-pre** worktree에서 작업합니다.

| Issue | Worktree | Branch | 작업 범위 | P0 | P1 |
|:-----:|:--------:|--------|----------|:--:|:--:|
| [#68](https://github.com/holee9/image-processing/issues/68) | **xpe-pre** | `dev/preprocess` | Offset 이중구현 제거, SIMD dispatch 활성화, 테스트 3-arg 전환, AVX2 parity 활성화, Gain 동작 통일 | 2 | 3 |
| [#69](https://github.com/holee9/image-processing/issues/69) | **xpe-pre** | `dev/preprocess` | Multi-method 구현, Cache mutex, Defect map 만료 정책, R² 로깅, Polynomial 검증 | 2 | 3 |
| [#70](https://github.com/holee9/image-processing/issues/70) | **xpe-pre** | `dev/preprocess` | Bilinear 보간 검증, Reflect padding 수정, Hampel 검출 확인, BPM merge 개선 | 1 | 2 |

---

## 6. 검증 방법론

각 에이전트는 독립적으로 동일한 소스 파일을 읽고 분석했습니다. 교차검증 기준:

- **2/5 에이전트 이상 확인**: 높은 신뢰도로 판단
- **1/5 에이전트만 확인**: 추가 검증 필요로 표시 (P0-DEF-01)
- **0/5 에이전트 반박**: 해당 항목은 정상으로 확정

---

*Version: 1.0.0*
*Classification: Informational*
*Source: 세션 9차 전처리 정밀 감사 (2026-04-28)*
