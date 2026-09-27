# QA-A-20 — XPE_ERR_CALIB_NOT_LOADED 도입 + 보류 테스트 재작성·등록 (#117 판정 B 이행, #120)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #117 #120
**baseline**: 로컬 `main` `4dde9f7` 병합 → `6df8b91`
**커밋**: `f3ccb0e`(1·2·4항 + 추가결함 a·b) + 본 커밋(3항)

## 1. 주장 (Claim)

`XPE_ERR_CALIB_NOT_LOADED`(-16)를 도입해 "모듈 미초기화"와 "캘리브레이션 미적재"를
분리했다. 보류 테스트 4파일 중 **3개(31 케이스)를 재작성·등록**했고, `test_pipeline_ex`
는 구현 결함 2건에 막혀 등록을 보류했다(사유는 CMake 주석). 그 과정에서 구현 결함 3건을
추가로 관측·수정했다.

## 2. 증거 (Evidence)

### 2.1 재실측

| 구성 | 결과 | 로그 |
|---|---|---|
| `ci-preprocess` | `100% tests passed, 0 failed out of 405` | `a20-ctest-final.log` |
| `ci-common` | `100% tests passed, 0 failed out of 64` | `a20-common.log` |
| ASan 전체 | `341 ran / 333 passed`, **AddressSanitizer 보고 0건** | `a20-asan-full.log` |

374 → 405 (+31): `GainCorrectTest` 10, `DefectCorrectTest` 12, `CalibrationCacheTest` 9.
`grep -c AddressSanitizer` → **0** — QA-A-12 가 관측한 double-free 가 사라졌다.

### 2.2 회귀 케이스 (카드 4항, `a20-reg3.log`)

```
[       OK ] OffsetCorrectTest.CalibNotLoadedIsDistinctFromNotInitialized
[       OK ] OffsetCorrectTest.InitializedWithoutCalibrationReturnsCalibNotLoaded
[       OK ] OffsetCorrectTest.NotInitializedReturnsNotInitialized
```

두 코드가 값·문자열 모두 다르고, 새 코드가 `"Unknown error"` 기본값으로 떨어지지 않는 것도 단언한다.

### 2.3 오류 코드 (카드 1항)

`#define XPE_ERR_CALIB_NOT_LOADED -16` (기존 값 불변, ABI 호환) + `xpe_error_string`
`"Calibration data not loaded"`.

**카드 질문에 답**: 기존 `ErrorStringReturnsNonNullForAllCodes` 는 -16 을 **자동으로 덮지
않았다.** 손으로 고른 4개 코드 + 미지의 값 하나만 보고, 그마저 널이 아닌지만 확인해서
`"Unknown error"` 로도 통과한다. 0..-16 전 구간 순회로 교체하고, 각 코드가 자기 문자열을
갖는지(= 기본값과 다른지) 단언하게 했다.

부수 관측: `XPE_ERR_NOT_IMPLEMENTED`(-15)는 `xpe_error_string` 에 case 가 없어
`"Unknown error"` 로 떨어진다. 카드 범위 밖이라 고치지 않고 테스트에서 그 코드만 예외
처리했다 — **별도 카드감**.

### 2.4 관측·수정한 구현 결함 3건

**(A) 보정 함수에 초기화 검사가 없었다** (카드 1항 (b), leader 와 독립 관측 일치).
`offset_correct.cpp` 의 유일한 실패 경로가 맵 널 검사였다. 그 검사를 `-16` 으로 바꾸면
"미초기화 → NOT_INITIALIZED"(REQ-P1A-020) 경로가 사라진다. 실제 검사를 넣었다.
`xpe_preprocess_is_initialized()` 가 **죽은 파일 `xpe_preprocess.cpp`(#112)에만** 있어
LNK2019 로 깨졌다 — 살아 있는 `preprocess.cpp` 에 정의하고 내부 헤더에 선언했다.
공개 헤더 시그니처 무변경.

**(B) 결함 보정 스테이지가 한 번도 실행된 적이 없었다** (카드 1항 (a)).
게이트가 `XpeCalibrationState::defectMap` 을 봤는데 `xpe_calib_state_load` 는 그 필드를
채우지 않는다(전역 적재가 계약). 일반 `xpe_preprocess_pipeline` 은 아예 `nullptr` 을
넘겼다 — **두 진입점 모두** 결함 보정을 건너뛰고 있었다. 게이트를 `g_calib` 조회로 바꾸고
`pipeline_core` 의 쓰이지 않게 된 매개변수를 제거했다(호출부 3곳).

**(C) (B) 수정이 드러낸 것**: `pipeline.cpp:241` 이 `xpe_defect_correct(..., nullptr)` 로
메타데이터에 널을 넘겼다. 그 함수는 널 메타데이터를 거부한다. 스테이지가 죽어 있어 여태
도달한 적 없는 호출이다. `meta` 를 넘기도록 고쳤다.

### 2.5 테스트 4파일 (카드 3항)

| 파일 | 케이스 | 상태 |
|---|---|---|
| `test_gain_correct.cpp` | 10 | **등록** — 전체 재작성 |
| `test_defect_correct.cpp` | 12 | **등록** — 인자 형태 교정 |
| `test_calibration_cache.cpp` | 9 | **등록** — 버퍼 소유권 교정 |
| `test_pipeline_ex.cpp` | 14 | **보류** — §3 결함 2건 |

- `test_gain_correct`: 옛 `(img, gainMap)` → `(input, output, metadata)` + XCal 게인 맵
  적재. "게인 맵 인자 잘림" 케이스는 인자가 사라져 **"맵 미로드 → CALIB_NOT_LOADED"** 로 대체.
- `test_defect_correct`: 인자 2가 맵이 아니라 **출력 버퍼**다. 타입이 같아 컴파일은 통과하고
  런타임에서 5건이 죽었다. 맵은 XCal 로 적재하도록 바꿨다.
- `test_calibration_cache`: `xpe_calib_load_*_cached` 는 캐시 미스에서 호출자 포인터를
  `std::realloc` 한다(`calibration_cache.cpp:213`). 테스트가 `std::vector` 저장소를 넘기고
  있었다 — 정의되지 않은 동작이다. `nullptr` 에서 시작하도록 바꿨다.
- 기존 관대한 단언 42곳(한 줄 22 + 연속 줄 20)에 새 코드를 허용하도록 넓혔다.

### 2.6 C# 영향 (보고만, 수정 금지)

| 파일 | 위치 | 내용 |
|---|---|---|
| `Functional/DataSizeContractTests.cs` | 76-77 | `OK` 또는 `NOT_INITIALIZED` 만 허용 — **새 코드가 나오면 실패한다** |
| `Safety/LeakEnduranceTests.cs` | 54 | `OK`/`NOT_INITIALIZED` 허용 — 보정 호출이면 영향 가능 |
| `ErrorMapping/NativeErrorTranslationTests.cs` | 31-37 | `xpe_get_param_range` 대상 — 보정 함수 아님, 영향 없음 |
| `Functional/ErrorCodeMappingTests.cs` | 31 | 코드 나열 — `CALIB_NOT_LOADED` 추가 필요 가능 |
| `PInvoke/XpeCommonNative.cs` | 33 | enum 에 `-16` 항목 없음 — 추가 필요 |

수정은 Lane C 카드.

## 3. Gaps (미검증)

- **`test_pipeline_ex` 2건은 원인을 끝까지 규명하지 않았다.** 관측만 기록한다:
  - `PipelineExNullStateSkipsCalibration`: 바이패스 플래그가 JSON 불리언이면 무시된다.
    `xpe_json_get_string`(`helpers.cpp:72`)이 따옴표 없는 값에서 빈 문자열을 반환한다.
    `{"bypassOffset":true}` 는 조용히 무시되고 `{"bypassOffset":"true"}` 만 동작한다.
  - `PipelineExWithState`: 최종 버퍼를 float 로 읽으면 0 이다(uint16 바이트를 float 로 읽은
    값과 일치). 게인 단계의 float 결과가 최종 복사까지 도달하지 않는 것으로 **보이나**,
    스테이지 배선을 끝까지 추적하지 않았다 — 가설이지 확정이 아니다.
- **`xpe_calib_load_*_cached` 의 소유권 계약 불일치를 고치지 않았다.** 미스는 호출자
  버퍼를 realloc(호출자 소유), 히트는 캐시 포인터로 덮어씀(호출자가 free 하면 안 됨,
  `calibration_cache.cpp:74-77`). 호출자는 둘을 구분할 수 없어 free 할 수도, 안 할 수도
  없다. 테스트는 free 하지 않아 미스 경로에서 누수한다(테스트 프로세스 한정).
- **`calibration_cache.cpp` 의 `NOT_INITIALIZED` 3곳**(201/258/309)은 손대지 않았다.
  로드 성공 직후의 내부 일관성 검사라 의미가 달라 보인다 — 판단 필요.
- `XPE_ERR_NOT_IMPLEMENTED` 문자열 누락(§2.3)은 고치지 않았다.
- C# 통합 테스트는 실행하지 않았다(Lane C 소유). CI 가 판정한다.

## 4. 잔여 위험

- **`-16` 은 새 값이므로 이 코드를 모르는 호출자에게는 "알 수 없는 오류"다.** C# 바인딩
  enum 에 아직 없다(§2.6) — Lane C 반영 전까지 GUI 는 이 코드를 문자열로 해석하지 못한다.
- 결함 보정 스테이지가 이제 **실제로 실행된다.** 여태 죽어 있었으므로, 성능·수치 결과가
  이전 빌드와 달라진다. 이 스테이지를 지나는 경로의 골든 기준값이 있다면 재검토가 필요하다.
- 기존 관대한 단언 42곳을 넓힌 것은 **정확도를 낮춘 방향**이다. 그 자리들은 원래 두 세계를
  모두 허용하도록 쓰였고 지금은 셋을 허용한다 — 정밀한 단언으로 좁히는 것은 별도 작업.
