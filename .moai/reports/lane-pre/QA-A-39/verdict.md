# QA-A-39 검증 보고서 — `xpe_calib_generate_offset` 에 `config_json_or_null` 추가

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 선행: `git merge origin/main` → `476419e` (merge commit, 충돌 없음)
- 커밋: `510ca1a` (ABI + 호출자 전수), `c459b0b` (테스트)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-39/`
- Refs #138 #97

---

## 1. 주장 (Claim)

| # | 주장 | 판정 |
|---|------|------|
| C1 | 공개 API 로 `{"method":"sigma_clip"}` 를 넘기면 결과가 Mean 과 다르다 | PASS |
| C2 | `NULL` 은 기존 기본(Mean)과 바이트 동일 — 호출자 동작 불변 | PASS |
| C3 | Median 도 도달 가능 — sigma_clip 전용 스위치가 아니다 | PASS |
| C4 | 잘못된 config 는 `XPE_ERR_CONFIG_INVALID`, 파일도 만들지 않는다 | PASS |
| C5 | N_min 마킹이 전역 defect 맵에 실제로 들어가고 되읽힌다 (A-38 Gap 1·2 해소) | PASS |
| C6 | 마킹이 0건이면 defect 맵을 만들지 않는다 | PASS |
| C7 | export 불변 (preprocess 50, common 16) | PASS |
| C8 | ci-preprocess / ci-common 재실측 회귀 없음 | PASS |

---

## 2. 증거 (Evidence)

### RED (`a39-red.log`)

구현 한 줄을 A-39 이전 동작으로 되돌린 상태 —
`parse_offset_generation_config(nullptr, &config)`:

```
[  FAILED  ] GenerateOffsetConfigTest.SigmaClipProducesADifferentMapThanMean (13 ms)
[  FAILED  ] GenerateOffsetConfigTest.MedianIsReachable (7 ms)
[  FAILED  ] GenerateOffsetConfigTest.MalformedConfigIsReported (4 ms)
[  FAILED  ] GenerateOffsetConfigTest.NMinMarksLandInTheGlobalDefectMap (1 ms)
[  PASSED  ] 2 tests.
```

통과한 2건은 NULL 동등성과 "마킹 없으면 맵 없음" 으로, 인자 배선과 무관하게
성립해야 하는 불변식이다 — 즉 이 스위트는 배선에만 반응한다.

프로브 자체에서 한 가지 측정된 사실: `/WX` 때문에 미사용 매개변수 경고
C4100 이 C2220 으로 승격되어 프로브가 빌드되지 않았고, `(void)config_json_or_null;`
를 넣어야 했다.

### GREEN (`a39-green.log`)

```
[  PASSED  ] 6 tests.
```

핵심 수치(모두 A-26 계열 `{100,110,105,108,500}`):

| 경로 | 픽셀 0 오프셋 |
|---|---|
| `NULL` / `{"method":"mean"}` | 184.6 |
| `{"method":"sigma_clip","sigma":1.0}` | 106.5 |
| `{"method":"median"}` | 108.0 |

`NMinMarksLandInTheGlobalDefectMap` 은 sigma_clip 실행 뒤
`xpe_calib_save(path, "defect", 0)` 가 `XPE_OK` 를 돌려주고 payload 4바이트가
모두 1임을 확인한다. 동시에 같은 실행의 오프셋 파일은 106.5 그대로다.

### 재실측

```
ctest --output-on-failure  (build/ci-preprocess)
100% tests passed, 0 tests failed out of 596

ctest --output-on-failure  (build/ci-common)
100% tests passed, 0 tests failed out of 69
```

### export 불변 (`a39-exports.txt`, `a39-export-diff.txt`)

```
          50 number of names      <- xpe_preprocess.dll
          16 number of names      <- xpe_common.dll
```

`diff a37-old-names.txt a39-new-names.txt` → 빈 출력 (exit 0).
**이름은 그대로지만 시그니처가 바뀌었다** — dumpbin 의 이름 목록은 C 함수의
인자 개수를 반영하지 않는다. §5 잔여 위험 1 참조.

### 변경 규모

커밋 1 — ABI (`a39-diff-commit1.txt`):

```
 include/xpe/preprocess_api.h                     | 23 ++++-
 src/xpe_calib_generate_offset.cpp                | 15 +++--
 tests/test_calibration_manager.cpp               |  6 +--
 tests/test_calibration_roundtrip.cpp             | 12 ++---
 tests/test_error_precedence.cpp                  |  4 +-
 tests/test_sigma_clip_nmin.cpp                   |  2 +-
 tests/test_xpe_calib_generate_offset.cpp         | 24 ++++----
 tests/test_xpe_preprocess_calibration.cpp        | 10 ++---
 tools/xpe_calib_fixture_gen.cpp                  |  2 +-
 9 files changed, 61 insertions(+), 37 deletions(-)
```

커밋 2 — 테스트 (`a39-diff-commit2.txt`):

```
 CMakeLists.txt                             |   2 +
 tests/test_calib_generate_offset_config.cpp| 211 +++++++++++++
 2 files changed, 213 insertions(+)
```

---

## 3. 호출자 전수 목록 (합격 조건)

전체 grep 결과는 `a39-callers.txt` (58행). 갱신한 30곳:

| 파일 | 건수 | 비고 |
|---|---|---|
| `tools/xpe_calib_fixture_gen.cpp` | 1 | `nullptr` — 픽스처는 Mean 유지, SHA 불변 |
| `tests/test_calibration_manager.cpp` | 3 | |
| `tests/test_calibration_roundtrip.cpp` | 6 | |
| `tests/test_error_precedence.cpp` | 2 | |
| `tests/test_sigma_clip_nmin.cpp` | 1 | |
| `tests/test_xpe_calib_generate_offset.cpp` | 12 | |
| `tests/test_xpe_preprocess_calibration.cpp` | 5 | |

**갱신하지 않은 것 3종:**

1. `clients/ImageProcTest.IntegrationTests/PInvoke/XpePreprocessNative.cs:41`
   `CalibGenerateOffsetDelegate` 가 **5인자 P/Invoke 선언**이다. C ABI 가
   6인자가 됐으므로 이 델리게이트로 호출하면 callee 가 6번째를 스택/레지스터
   잔여값으로 읽는다. `clients/` 는 Lane C 소유라 이 레인에서 고치지 않는다.
   같은 파일을 쓰는 `P1AReady/PreprocessCorrectionChainSmokeTests.cs:204` 이
   실제 호출자다. **리더 통보 필요 — 이 카드가 만든 유일한 레인 밖 파손.**
2. `clients/ImageProcTest/Diagnostics/XpePreprocessReadinessProbe.cs:22` 은
   export 이름 문자열 목록뿐이라 영향 없음(카드의 예상과 일치).
3. `tests/test_xpe_preprocess.cpp` 의 3곳은 CMake 에 미등록이라 컴파일되지
   않는다(QA-A-21 이 기록한 구조체 필드 불일치 때문). 건드리지 않았다 —
   등록 시점에 6인자로 함께 고쳐야 한다.

로컬 shim 1건은 대상이 아니다: `tests/test_calib_generate_offset_multi.cpp:38`
의 동명 함수는 `(frames, n, XpeImageBuffer*, config)` 시그니처를 가진 파일
내부 래퍼로, 공개 API 가 아니다.

---

## 4. Baseline 귀속

- **테스트 총수**: QA-A-38 측정 590 → 596 (+6 = 신규 `GenerateOffsetConfigTest`).
  기존 케이스는 인자 하나가 붙었을 뿐 기대값이 바뀐 것이 없다(전부 `nullptr` =
  기존 동작).
- **빌드 프리셋**: `ci-preprocess`, `ci-common` 동일, `/WX` 유지. 최종 빌드 경고 0건.
- **export baseline**: QA-A-35 의 `a35-dumpbin.log` 50개 이름. A-37·A-38 과 동일 파일.
- **RED 기준선**: 같은 트리에서 배선 한 줄만 되돌려 재빌드해 측정했고, 원복 후
  같은 명령으로 GREEN 을 다시 측정했다.

---

## 5. 미검증 (Gaps)

1. **C# 쪽 6인자 호출을 실행해 보지 않았다.** 위 §3-1 의 파손은 코드를 읽어
   확인한 것이지, `PreprocessCorrectionChainSmokeTests` 를 돌려 실제 크래시나
   오동작을 관측한 것은 아니다. Lane C 소유 파일이라 이 레인에서 실행 환경을
   갖추지 않았다.
2. **`sigma`/`max_iter`/`percentile` 키의 정상 경로는 부분만 확인했다.**
   `sigma` 는 1.0 을 넘겨 동작을 확인했고, `max_iter`·`lower_percentile`·
   `upper_percentile` 은 **거부 경로만** 테스트했다(잘못된 값 → CONFIG_INVALID).
   winsor 방식 자체는 이 스위트에서 다루지 않았다.
3. **`xpe_calib_fixture_gen` 의 산출 SHA 재측정은 하지 않았다.** `nullptr` 를
   넘겨 Mean 을 유지하므로 바이트가 같아야 하지만, A-36 의 결정성 테스트가
   ci-preprocess 596건 안에서 통과했다는 사실이 간접 증거이고 SHA 를 직접
   대조하지는 않았다.
4. **api-spec §6 문서는 리더가 갱신 중이다.** 이 레인은 헤더 Doxygen 만
   작성했고 api-spec 은 건드리지 않았다(문서 정본 원칙).
5. **다른 DLL 의 호출자는 조사하지 않았다.** grep 범위는 저장소 전체였으나,
   빌드되지 않는 언어/생성 코드(예: 바인딩 생성기 출력)가 있다면 놓쳤을 수 있다.

---

## 6. 잔여 위험 (Residual risk)

1. **export 이름 diff 는 이 변경을 잡아내지 못한다.** dumpbin 의 이름 목록은
   C 함수의 인자 개수를 담지 않으므로 50/50 그대로다. 즉 "export diff 0" 은
   이 카드에서 **ABI 안전의 증거가 아니다** — 실제 계약은 바뀌었고, 그것을
   잡아낸 것은 컴파일러(호출자 30곳)와 코드 리뷰(C# 1곳)뿐이다. 자동 게이트가
   없다는 뜻이며, 앞으로 같은 종류의 변경도 같은 사각지대를 지난다.
2. **NULL 동등성은 현재 기본값에 묶여 있다.** `OffsetGenerationConfig` 의
   기본 method 가 언젠가 바뀌면 "NULL = 기존 동작" 약속이 조용히 깨진다.
   `NullConfigIsTheSameAsExplicitMean` 은 NULL 과 `{"method":"mean"}` 의
   동등성만 고정하지, "기본이 mean 이다" 를 고정하지는 않는다.
3. **sigma_clip 이 이제 프로덕션에서 도달 가능해졌다** — A-38 §5-1 의 위험이
   실현 가능 상태가 됐다. 이 방식을 켠 호출자는 결함 맵이 커지는 것을 감수해야
   하고, OR 로 누적된 비트는 이 경로로는 지워지지 않는다.
4. **부분 실패 시 상태.** config 파싱 실패는 파일 생성 전에 반환되므로 부작용이
   없지만, 마스크 병합이 실패하면(차원 불일치) 오프셋 파일도 쓰이지 않는다 —
   생성 자체가 실패한 것으로 보인다. 호출자가 두 실패를 구분할 방법은 반환
   코드뿐(`CONFIG_INVALID`)이며, 이는 config 오류와 같은 코드다.
