# QA-A-15 — 배선 누락 테스트 등록 (#120, Class B)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #120
**baseline**: `origin/main` `0247b51` 병합 (`0 0` 동기)

## 1. 주장 (Claim)

디스크에 있으나 `XPE_TEST_SOURCES` 에 없던 테스트 5파일(58 TEST) 중
**`test_xcal_compression.cpp` 1개(14 TEST)만 등록**했다. 나머지 4개는 컴파일 또는
런타임에서 실패하며, 실패의 뿌리가 **QA-A-12(#117)가 판정할 SPEC-구현 갈라짐**이라
그 판정 뒤로 미뤘다. 이유는 CMake 주석에 남겼다.

## 2. 증거 (Evidence)

### 2.1 배선 누락 실측 (leader 관측 재확인)

| 파일 | 디스크 | 등록 상태(수정 전) | TEST |
|---|---|---|---|
| `test_pipeline_ex.cpp` | 있음 | 없음 | 14 |
| `test_xcal_compression.cpp` | 있음 | 없음 | 14 |
| `test_calibration_cache.cpp` | 있음 | 없음 | 9 |
| `test_gain_correct.cpp` | 있음 | 주석(126행) | 10 |
| `test_defect_correct.cpp` | 있음 | 주석(129행) | 11 |

### 2.2 5개 전부 등록 → 컴파일 36건 실패 (`a15-build1.log`)

```
20  test_gain_correct.cpp
 6  test_pipeline_ex.cpp
 6  test_calibration_cache.cpp
 4  test_defect_correct.cpp
    (test_xcal_compression.cpp — 오류 0)
```

`error C2660` 16건 + `C2737` 18건 + `C2664` 2건.

### 2.3 CMake 주석이 말한 "구 API 인자 불일치"는 정확하지 않다

주석은 *"uses old 2-arg API"* 라고만 적혀 있었으나, 실제로는 **함수가 하는 일이 바뀌었다.**

| 호출 | 구(테스트가 가정) | 신(현재 API) | 성격 |
|---|---|---|---|
| `xpe_gain_correct` | `(img, gainMap)` — 맵을 인자로, in-place 소유권 이전 | `(input, output, metadata)` — 전역 `g_calib` 맵 사용 | 의미 변경 |
| `xpe_calib_save` | `(buf, path, expiry, ...)` — 임의 버퍼를 파일로 | `(filepath, calib_type)` — 로드된 캘리브레이션 저장 | 의미 변경 |
| `xpe_defect_detect_runtime` | `(image, out, nullptr)` | `(image, metadata, out)` | 순서만 |
| `xpe_defect_correct` | `(img, defectMap, nullptr)` | `(input, output, metadata)` | **타입이 같아 컴파일은 통과**, 런타임 실패 |

`xpe_defect_correct` 가 가장 위험하다 — 인자 2가 둘 다 `XpeImageBuffer*` 라 컴파일러가
침묵하고, 실행에서 5건이 `-1`(INVALID_INPUT)로 죽는다.

### 2.4 마이그레이션한 것

- `test_defect_correct.cpp` — `xpe_defect_detect_runtime` 인자 순서 3곳 교체
- `test_pipeline_ex.cpp` / `test_calibration_cache.cpp` — 픽스처가 은퇴한 4인자
  `xpe_calib_save` 로 .xcal 을 만들고 있었다. `test_offset_correct.cpp` 의 방식대로
  `write_xcal_file` 로 직접 쓰는 헬퍼(`writeXCalFixture`)로 교체(각 3곳)

이 과정에서 **내 픽스처 버그를 하나 만들었고 실측으로 잡았다**: OFFSET 을 `XCAL_FMT_UINT16`
으로 썼다가 `write_xcal_file` 이후 로드가 `-4`(CONFIG_INVALID)로 거부됐다.
`xcal_validator.cpp:78` 이 OFFSET→FLOAT32 를 강제한다. FLOAT32 로 고쳤다.

### 2.5 등록 후 런타임 결과 (`a15-suites2.log`, `pex.log`, `pass2b.log`)

| 스위트 | 결과 |
|---|---|
| `RleCodecTest` + XCal 압축 (14) | **전부 통과** |
| `CalibrationCacheTest` (9) | 실패 — 아래 (a) |
| `PipelineExTest` (14) | 실패 2건 + **프로세스 중단** — 아래 (b) |
| `DefectCorrectTest` (11) | 실패 5건 — §2.3 의미 불일치 |

**(a) CalibrationCache** — OFFSET 맵의 원소 타입이 어긋난다. 테스트는
`data[0] == 100u` 로 **uint16** 을 기대하는데 XCal v1 은 OFFSET 을 FLOAT32 로만 허용한다
(`xcal_validator.cpp:78`). float `100.0f` 의 하위 16비트를 uint16 으로 읽으면 `0` 이다.
어느 쪽이 옳은지는 이 카드가 정할 문제가 아니다.

**(b) PipelineEx** — `xpe_calib_state_load(&state, dir)` 가 `XPE_OK` 를 돌려주지만
`state.offsetMap.data` 는 `NULL`, `state.offsetMap.height` 는 `0` 이다.
`pipeline.cpp:326-342` 는 맵을 **전역 `g_calib` 에만** 싣고 구조체 필드는 채우지 않는다
(주석에도 *"other maps use g_calib"*). 파일명은 내 픽스처와 일치한다
(`offset.xcal`/`gain.xcal`/`defect.xcal`) — 픽스처 문제가 아니다.
그 다음 `PipelineExWithState` 에서 **테스트 바이너리가 중단된다**(요약 줄 없이 exit 127).

**공통 뿌리**: 테스트 4개는 "맵을 데이터로 주고받는" 설계를 가정하고, 구현은 전역
`g_calib` 을 쓴다. leader 가 A-12 카드에 적은 *"SPEC-XPE-P1A:131/145/160 은 맵을 인자로
받는 시그니처를 요구한다"* 와 같은 갈래다.

### 2.6 최종 등록 + 재실측 (`ctest-a15.log`)

`test_xcal_compression.cpp` 만 등록:

```
100% tests passed, 0 tests failed out of 369
Total Test time (real) =  26.97 sec
```

355 → **369** (+14). `rle_codec.cpp`(176줄, 0%)가 처음으로 실행된다.

## 3. baseline 귀속

`origin/main` `0247b51` 병합 트리. 모든 수치는 이번 실행 관측.
컴파일 오류 36건 → `a15-build1.log`, 런타임 결과 → `a15-suites2.log`/`pex.log`/`pass2b.log`,
최종 → `ctest-a15.log`.

## 4. Gaps (미검증)

- **커버리지 수치를 재지 않았다.** 로컬 x64 OpenCppCoverage 유무를 확인하지 않았고
  `coverage` 프리셋도 돌리지 않았다. leader 의 "셋만 덮이면 0.80" 산술도 검증하지 않았다.
  이번에 실제로 등록된 것은 셋 중 하나뿐이므로 그 산술은 그대로 성립하지 않는다.
- **`xcal_reader.cpp` 46% 미덮 경로 목록(카드 3항)을 작성하지 못했다.** 커버리지 실측
  없이 목록을 쓰면 추측이 된다.
- **PipelineEx 중단의 원인을 규명하지 않았다.** 중단 지점만 관측했고 디버거·ASan 을
  붙이지 않았다. "크래시가 있다"는 관측이고, 어디가 왜인지는 미확인이다.
- 등록 보류한 4파일의 마이그레이션 코드는 **컴파일까지만 확인**됐다(gain 은 그것도 못 함).

## 5. 잔여 위험

- 보류한 4파일(44 TEST)은 계속 실행되지 않는다. A-17 의 gain/defect 가드도 여전히 미실행이다.
- 픽스처를 `write_xcal_file` 로 옮기면서 **테스트가 XCal 포맷 규칙에 직접 묶였다.**
  포맷이 바뀌면 이 픽스처들이 같이 깨진다 — 은퇴한 `xpe_calib_save` 가 흡수하던 부분이다.
- `test_xcal_compression` 은 이번 실행에서 통과했으나 **CI 에서 처음 도는 스위트**다.
  환경 의존 실패 가능성은 CI 가 판정한다.
