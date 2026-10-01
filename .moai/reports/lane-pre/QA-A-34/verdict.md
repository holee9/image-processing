# QA-A-34 검증 보고서 — 죽은 코드 삭제 + 가짜 게이트 정리 + defect 스테이지 전파 통일 (#120)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-34 (`.moai/lanes/pre/inbox/QA-A-34.md`, 4항 추가 포함)
- 정본: `docs/project/api-spec.md` §6 규칙 3 (decision #120, main `00880b8`)
- 커밋 4건: `c1453aa`(1항) → `6456537`(2항) → `fe0ad03`(3항) → `0978ea3`(4항)

## 1. 주장 (Claim)

1. **1항** — `xpe::calib::mode` 네임스페이스와 그것만이 쓰던 헬퍼 4개·상수 2개를 삭제했다. 삭제 전에 카드가 요구한 export 정의 확인을 실측했고, 의존 방향이 **반대**임을 확인했다(§2).
2. **2항** — `test_calib_mode.cpp` 의 R² 게이트 테스트 **4건**을 삭제했다. 카드는 2건(`R2QualityGate_Pass/Fail`)을 지목했지만 같은 형태가 **4건**이었다(§3).
3. **3항** — defect 스테이지를 offset·gain 과 같은 계약으로 통일했다. A-32 가 고정한 "조용한 건너뜀" 케이스를 RED 로 뒤집고 구현을 고쳐 GREEN 으로 만들었다(§4).
4. **4항** — 죽은 파서 `ParseWindowSize` / `ParseSigmaThreshold` 를 삭제했다(§5).
5. 총 **307줄 삭제 / 92줄 추가**. 최종 ci-preprocess **554/554 PASS**, ci-common **69/69 PASS**. `xpe_preprocess.dll` export **50 names, 목록 diff 빈 출력**.

## 2. 1항 — export 정의 확인 (삭제 전 필수 절차)

카드: "공개 export `xpe_calib_get_poly_degree`/`xpe_calib_get_max_points` 는 **다른 정의**여야 한다 … 의존하면 멈추고 보고."

실측 (`a34-export-grep.txt`):

```
# 공개 export 의 정의 위치
modules/preprocess/src/xpe_calib_mode.cpp:230:uint32_t xpe_calib_get_max_points(void) {
modules/preprocess/src/xpe_calib_mode.cpp:242:uint32_t xpe_calib_get_poly_degree(void) {

# 죽은 네임스페이스 호출자 (정의 파일 제외)
grep_exit=1            ← 일치 0

# 죽은 함수가 공개 export 를 부르는 방향 (역의존)
317:    uint32_t max_points = xpe_calib_get_max_points();
333:    return xpe_calib_get_poly_degree();
```

**의존 방향이 반대였다.** 공개 export 는 230/242 의 독립된 정의이고, 죽은 래퍼(317/333)가 그것을 호출하고 있었다. 즉 죽은 쪽이 사라져도 공개 함수는 그대로다 — 멈출 이유가 없었다.

삭제 대상과 사유:

| 삭제한 것 | 사유 |
|---|---|
| `xpe::calib::mode::{init_metadata, update_metadata, get_max_points, get_poly_degree}` | 호출자 0, 헤더 선언 0 |
| `init_quality_meta`, `copy_cstr`, `log_quality_regression`, `update_quality_meta` | 위 네임스페이스만이 호출 |
| `R_SQUARED_QUALITY_GATE`, `MAX_POINTS_HARD_CAP` | 위 헬퍼만이 사용 |

**남긴 것**: `get_mode_params`(92) 의 무효 모드 폴백(97). 이 함수는 살아 있는 export 230/242 가 호출하므로 함수째 지울 수 없다. 폴백 분기는 방어적 코드로 남으며 계속 미커버다.

export 불변 (`a34-dumpbin.log`, `a34-export-diff.txt`):

```
          50 number of functions
          50 number of names
```
QA-A-01 baseline 과의 이름 목록 `diff` — **빈 출력, exit 0**.

## 3. 2항 — 가짜 게이트 테스트, 지목된 2건이 아니라 4건

카드는 `R2QualityGate_Pass/Fail` 2건을 지목했다. 같은 파일을 읽어 보니 **같은 형태가 4건**이었다:

| 케이스 | 실제 단언 |
|---|---|
| `R2QualityGate_Pass_WhenAboveThreshold` | `constexpr double r_squared_good = 0.9995; EXPECT_GE(r_squared_good, 0.999);` |
| `R2QualityGate_Fail_WhenBelowThreshold` | `constexpr double r_squared_bad = 0.998; EXPECT_LT(r_squared_bad, 0.999);` |
| `PreviousCalibration_Comparison_Regression` | 지역 double 두 개를 선언해 `EXPECT_LT(new, previous - 0.01)` |
| `PreviousCalibration_Comparison_Stable` | 같은 형태로 `EXPECT_GE` |

네 건 모두 **프로덕션 코드를 호출하지 않는다.** 각 비교 위의 `xpe_calib_get_quality_meta(&meta)` 는 메타를 읽고 그대로 버린다 — 단언이 `meta` 를 건드리지 않는다. 안전 게이트 이름을 단 초록 행 네 개가 산술을 검증하고 있었다.

**살아 있는 게이트는 없다.** 이들이 덮는다고 주장한 R² 게이트는 1항에서 삭제한 `update_metadata` 안에 있었다. 카드의 "없으면 삭제 + 사유" 에 해당하므로 네 건 모두 삭제하고, 그 자리에 사유를 주석으로 남겼다 — 게이트가 도달 가능한 API 로 돌아오면 테스트는 여기 와야 하고 반드시 그것을 호출해야 한다는 조건까지.

## 4. 3항 — defect 스테이지 전파 (RED → GREEN)

정본 (`api-spec.md` §6 규칙 3, 인용):

> **Pipeline stage propagation (decision #120 / QA-A-34).** `xpe_preprocess_pipeline` treats the three correction stages alike: when a stage is not bypassed … and its map is not loaded, the pipeline stops with `XPE_ERR_CALIB_NOT_LOADED`. A silently skipped defect stage (observed by QA-A-32: `XPE_OK` with no correction) is a defect, not a contract … Skipping is only ever the result of an explicit bypass flag.

### RED (`a34-item3-red.log`, exit=8)

A-32 가 `DefectStageIsSkippedRatherThanFailingWithoutAMap` 로 고정해 둔 케이스를 `DefectStageFailsWhenItsMapIsNotLoaded` 로 뒤집었다:

```
321/555 Test #321: PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded ........................***Failed    0.28 sec
99% tests passed, 1 tests failed out of 554
```

### 구현 변경

맵 존재를 **게이트가 아니라 전제**로 바꿨다:

```cpp
if (!cfg.bypassDefect) {
    bool defectAvailable = false;
    { std::lock_guard<std::mutex> calibLock(g_calib_mutex);
      defectAvailable = (g_calib.defect_map != nullptr); }
    if (!defectAvailable) return XPE_ERR_CALIB_NOT_LOADED;
}
```

### GREEN (`a34-item3-green.log`, exit=0)

```
100% tests passed, 0 tests failed out of 554
```

bypass 경로 무회귀는 추측하지 않고 `BypassDefectStillSkipsTheStageCleanly` 로 **별도 단언**했다 — 명시적 bypass 는 맵 없이도 `XPE_OK` 여야 한다.

## 5. 4항 — 죽은 파서

실측 (`a34-parser-grep.txt`): 호출자는 `runtime_detection.cpp:156-157` 두 줄뿐이고 **둘 다 리터럴 `nullptr`** 을 넘긴다. 파서는 받은 기본값을 그대로 돌려줄 뿐, JSON 을 훑는 본문이 실행된 적이 없다.

SPEC 요구 확인: `windowSize` / `sigmaThreshold` 를 외부에서 지정해야 한다는 요구는 없다. `grep` 의 유일한 일치는 `SPEC-XPE-P2-ADV` 의 Hough 피크 검출(`findPeaks(A, threshold, windowSize)`)로 이 모듈과 무관하다. 카드의 "있으면 멈추고 보고" 조건에 해당하지 않아 삭제했다.

진입점 Doxygen 의 `@param configJsonOrNull Optional JSON config with windowSize and sigmaThreshold` 도 **존재하지 않는 인자를 서술**하고 있어 함께 정정했다. A-33 이 남긴 테스트 파일 머리말의 참조도 갱신했다.

## 6. 증거 (Evidence)

| 항 | 결과 | 로그 |
|---|---|---|
| 1항 삭제 후 | 557/557 PASS | `a34-item1.log` (exit=0) |
| 1항 export | 50/50, diff 빈 출력 | `a34-dumpbin.log`, `a34-export-diff.txt` |
| 2항 삭제 후 | 557 → **553/553 PASS** (−4 = 삭제한 케이스) | `a34-item2.log` (exit=0) |
| 3항 RED | 1건 실패 / 554 | `a34-item3-red.log` (exit=8) |
| 3항 GREEN | **554/554 PASS** | `a34-item3-green.log` (exit=0) |
| 4항 삭제 후 | **554/554 PASS** | `a34-item4b.log` (exit=0) |
| ci-common | **69/69 PASS** | `a34-common.log` (exit=0) |

변경 규모 (`a34-diffstat.txt`): **6 files changed, 92 insertions(+), 307 deletions(-)**.

## 7. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 557/557 PASS (QA-A-33, `../QA-A-33/a33-run2.log`) | **554/554 PASS** (`a34-item4b.log`) | −4(가짜 게이트 삭제) +1(bypass 무회귀 케이스 추가) = −3 |
| ci-common | 69/69 PASS (QA-A-32, `../QA-A-32/a32-common.log`) | **69/69 PASS** (`a34-common.log`) | 변화 없음 |
| `xpe_preprocess.dll` export | 50 names (QA-A-01 `abi.log:132`) | **50 names, 목록 동일** | 변화 없음 |

## 8. 미검증 (Gaps)

- **커버리지 수치를 측정하지 못했다.** 1항이 겨냥한 52줄이 실제로 사라져 분모가 줄었는지, 3항·4항이 비율을 얼마나 움직였는지 모른다. 로컬 OpenCppCoverage 가 32비트 전용이라 A-32·A-33 과 같은 이유로 측정 불가 — leader dispatch 필요.
- **`ValidateConfig` 는 남겨 두었다.** 4항으로 config 가 항상 기본값이 되었으므로 이 검증 함수도 실질적으로 죽은 코드에 가깝다. 카드가 지목하지 않았고, 내부 `DetectDefectivePixel` 경로에서는 여전히 의미가 있을 수 있어 손대지 않았다.
- **defect 전파가 다른 호출자에 미치는 영향을 전수 확인하지 않았다.** `xpe_preprocess_pipeline_ex` / `_batch` 가 같은 `pipeline_core` 를 쓰는지는 코드로 확인했으나, 그 두 진입점을 defect 활성 상태로 실행해 보지는 않았다. 기존 케이스가 통과했다는 사실이 유일한 근거다.
- **C# 소비자 미확인.** defect 스테이지가 이제 오류를 반환하므로, 파이프라인을 defect 활성으로 호출하던 GUI 코드가 있다면 동작이 바뀐다. Lane C 소유라 확인하지 않았다 — §9 참조.
- **ASan 재측정 없음.**
- **삭제한 죽은 코드의 의도는 추적하지 않았다.** `update_metadata` 의 R² 회귀 경고나 10점 하드캡이 어떤 요구에서 왔는지 확인하지 않았다. 요구가 살아 있다면 삭제가 아니라 배선이 맞았을 수 있다.

## 9. 잔여 위험 (Residual risk)

- **3항은 동작을 바꾸는 변경이다.** 이전에는 defect 맵 없이도 파이프라인이 성공했다. 그 동작에 의존하던 호출자(맵을 안 올리고 defect 를 켠 채 돌리던 코드)는 이제 `XPE_ERR_CALIB_NOT_LOADED` 를 받는다. **의도된 변경이고 안전 방향**이지만, 조용히 통과하던 경로가 갑자기 실패하는 형태라 통합 지점에서 드러날 수 있다.
- **`get_mode_params` 의 무효 모드 폴백은 여전히 도달 불가**다. `xpe_calib_set_mode` 가 무효 모드를 거부하므로 이 분기는 커버되지 않고, 커버리지 분모에 계속 남는다.
- **R² 품질 게이트가 이 모듈에서 완전히 사라졌다.** 1항 삭제로 계산 코드가, 2항 삭제로 (가짜였지만) 그 이름을 달고 있던 테스트가 없어졌다. FUNC-033 이 여전히 게이트를 요구한다면 지금은 구현도 검증도 없는 상태다 — 요구 확인이 필요하다.
- **파서 삭제로 창/시그마 튜닝 경로가 코드상으로도 사라졌다.** 이전에도 동작하지 않았으므로 기능 손실은 아니지만, 나중에 설정 가능성이 요구되면 파서를 다시 쓰게 된다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| 1항 export 확인 + 죽은 네임스페이스 삭제 + dumpbin diff | QA-A-34 |
| 2항 가짜 R² 게이트 4건 삭제 | QA-A-34 |
| 3항 defect 전파 RED→GREEN | QA-A-34 |
| 4항 죽은 파서 삭제 | QA-A-34 |
| 커버리지 수치 | leader dispatch 필요 |
| **FUNC-033 R² 게이트 요구 존재 여부 확인** | 신규 카드 필요 |
| defect 전파에 대한 C# 소비자 영향 | Lane C 확인 필요 |
| `ValidateConfig` 처분 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a34-export-grep.txt` | 1항 삭제 전 export 정의·의존 방향 확인 |
| `a34-dumpbin.log`, `a34-export-diff.txt` | export 50/50, baseline 대비 diff 빈 출력 |
| `a34-item1.log` | 1항 후 557/557 PASS |
| `a34-item2.log` | 2항 후 553/553 PASS |
| `a34-item3-red.log` | 3항 RED — 1건 실패 (exit=8) |
| `a34-item3-green.log` | 3항 GREEN — 554/554 PASS |
| `a34-parser-grep.txt` | 4항 호출자·SPEC 요구 확인 |
| `a34-item4b.log` | 4항 후 554/554 PASS |
| `a34-common.log` | ci-common 69/69 PASS |
| `a34-diffstat.txt` | 커밋 4건 합계 `git diff --stat` |
