# QA-A-88 — 시험 사이 전역 상태: `calibration_mode:5` 의 출처와 순서 의존 시험 목록

**카드**: `.moai/lanes/pre/inbox/QA-A-88.md` · **브랜치**: `dev/preprocess` · **HEAD**: `3fdde65` · **코드 변경 없음** (임시 출력 1줄은 작업 트리에서만 쓰고 되돌림) · **기계**: Intel Core i7-12700

바이너리: `build/ci-preprocess/bin/xpe_preprocess_tests.exe` (시험 570건).

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **`calibration_mode:5` 는 `CalibModeTest` 에서 새어 나온 것이 맞다.** 기본 순서 5, `-CalibModeTest.*` 3, 단독 3 |
| C2 | **순서 의존 시험이 3건 확인됐다.** 각각 오염시키는 앞 시험을 필터·셔플로 고정했다 |
| C3 | **그중 1건은 기본 순서 그대로 이미 실패한다** — `CalibModeTest.QualityMeta_InitialState`. 바이너리를 직접 돌리면 570 중 1건 빨강 |
| C4 | **CI·로컬 ctest 는 원리상 이것을 볼 수 없다.** `gtest_discover_tests` 가 시험마다 프로세스를 따로 띄우고, CI 는 전부 `ctest` 다 |
| C5 | 전역은 **세 종류**다: 모드(`g_calib_mode`), 품질 메타(`g_quality_meta`), 로드된 교정(`g_calib`). `xpe_preprocess_init` 은 셋 다 지우지 않고, `shutdown` 은 `g_calib` 만 지운다 |
| C6 | **모드 누수가 영향을 주는 단언은 현재 없다.** 모드 값을 단언하는 시험은 `CalibModeTest` 뿐이고 그 픽스처가 매 시험 모드를 되돌린다 |
| C7 | 셔플 16회(시드 1~16)와 재현 실행의 시드를 모두 기록했다 |

**고치지 않았다** — 카드 지시대로 목록만.

---

## 2. 증거 (Evidence)

### C1 — 모드 누수의 출처

잔차 시험(`ResidualFieldsCarryTheHandComputedValues`)에 생성 파일 JSON 을 찍는 줄을 **작업 트리에만** 넣고 세 번 돌렸다.

| 실행 | 명령 | `calibration_mode` | 결과 |
|---|---|---|---|
| A1 | 인자 없음(기본 순서) | **5** (AUTO) | 569 통과 / 1 실패 |
| A2 | `--gtest_filter=-CalibModeTest.*` | **3** (MULTI_POINT_8, 기본값) | 562 통과 |
| A3 | `--gtest_filter=CalibQualityMetaTest.ResidualFieldsCarryTheHandComputedValues` | **3** | 1 통과 |

`CalibModeTest` 중 모드를 **AUTO 로 남기고 끝나는** 시험(`test_calib_mode.cpp`):

| 시험 | 마지막 `set_mode` |
|---|---|
| `SetGetRoundtrip_AllModes` | `XPE_CALIB_AUTO` (`:72`) |
| `GetMaxPoints_PerMode` | 루프 마지막 원소 `XPE_CALIB_AUTO` |
| `GetPolyDegree_PerMode` | 루프 마지막 원소 `XPE_CALIB_AUTO` |
| `MaxPoints_HardCap_10` | `XPE_CALIB_AUTO` (`:194`) |

픽스처(`:31-38`): `SetUp` 은 `xpe_calib_set_mode(XPE_CALIB_MULTI_POINT_8)`, `TearDown` 은 비어 있음. **들어올 때 되돌리고 나갈 때 되돌리지 않는다** — 자기 스위트 안에서는 안전하고, 다음 스위트로는 샌다.

`xpe_calib_set_mode` 를 부르는 시험 파일은 **`test_calib_mode.cpp` 하나뿐**이다(13건).

### C5 — 전역과 초기화

```
modules/preprocess/src/xpe_calib_mode.cpp:32   XpeCalibrationMode g_calib_mode = XPE_CALIB_MULTI_POINT_8;
modules/preprocess/src/xpe_calib_mode.cpp:35   XpeCalibQualityMeta g_quality_meta = []{ ... }
```

`g_calib_mode`·`g_quality_meta` 를 쓰는 곳은 `xpe_calib_mode.cpp` 안의 `set_mode`(`:122`), `record_quality_meta`(`:199-210`), `apply_quality_meta_json`(`:255-256`) 뿐이다. 정적 초기화 외에 되돌리는 코드가 없다.

`preprocess.cpp`:

```
xpe_preprocess_init      (:44)  g_initialized 만 true 로. g_calib 을 건드리지 않음
xpe_preprocess_shutdown  (:71)  g_initialized = false; g_calib = CalibrationData{};
```

`init`/`shutdown` 이 있는 파일 4개(`preprocess.cpp`, `offset_correct.cpp`, `gain_correct.cpp`, `defect_correct.cpp`)에서 `calib_mode|set_mode|MULTI_POINT_8` 계수 0.

### C2 · C3 — 순서 의존 시험 3건

#### (가) `CalibModeTest.QualityMeta_InitialState` — 품질 메타 누수

기본 순서(A1) 실패 원문:

```
test_calib_mode.cpp(158): error: Expected equality of these values:
  meta.calibration_mode   Which is: '\x3' (3)    0
test_calib_mode.cpp(159): ...
  meta.polynomial_degree  Which is: '\x2' (2)    0
test_calib_mode.cpp(160): ...
  meta.num_points         Which is: '\x3' (3)    0
```

이 시험은 `g_quality_meta` 가 **처음 상태**(전부 0, `previous_r_squared` = −1)라고 단언한다. 그 전역은 생성기가 호출될 때마다 채워지고 아무도 비우지 않는다.

기본 순서에서 이 시험 앞에 있는 스위트(목록 `a88-list.log`): `GenerateGainTest`(517행) → `CalibModeTest`(554행).

| 실행 | 명령 | 종료 |
|---|---|---|
| P0 | `--gtest_filter=CalibModeTest.QualityMeta_InitialState` | **0** (통과) |
| P1 | `--gtest_filter=GenerateGainTest.*:CalibModeTest.QualityMeta_InitialState` | **1** (실패, 7 통과 / 1 실패) |
| P2 | `--gtest_filter=*:-GenerateGainTest.*:CalibQualityMetaTest.*:GainPolyLoadTest.*` | **0** (540 통과) |

**오염원**: `GenerateGainTest` 는 P1 로 확인. `CalibQualityMetaTest`·`GainPolyLoadTest` 는 **생성기를 호출한다는 코드 사실**(아래 계수)로만 후보에 넣었고, 단독 쌍 실행은 하지 않았다.

생성기·로더 호출 시험 파일 계수(`xpe_calib_generate_gain(` / `..._polynomial(` / `xpe_calib_load_gain(`):

```
test_calib_gain_poly_load.cpp        1 | 2 | 9
test_calib_generate_gain.cpp         5 | 2 | 0
test_calib_quality_meta.cpp          0 | 2 | 6
(그 외 13개 파일)                     0 | 0 | 1~6
```

생성기는 매 호출 `record_quality_meta` 로 전역을 쓴다. 로더는 파일 JSON 에 FUNC-033 키가 있을 때만 쓴다(`apply_quality_meta_json` 이 키가 하나도 없으면 `false` 반환). 생성기를 부르는 파일은 위 세 개뿐이다.

셔플에서 이 시험이 실패한 시드: **1, 3, 4, 5, 7, 8, 10, 12, 13, 15, 16** (16회 중 11회).

#### (나) `PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded` — 로드된 결함 맵 누수

시드 1 실패 원문:

```
test_pipeline_stages.cpp(198): error: Expected equality of these values:
  -16
  rc
    Which is: -7
an enabled stage with no map must stop the pipeline, like offset and gain
```

`-16` = `XPE_ERR_CALIB_NOT_LOADED`, `-7` = `XPE_ERR_UNSUPPORTED_FORMAT` (`xpe_error.h:54`, `:63`). **맵이 없다가 아니라, 다른 크기의 맵이 있다**는 응답이다.

시드 1 에서 바로 앞에 돈 시험: `EnduranceTest.*` 5건. 쌍 실행(`EnduranceTest.<X>:PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded`, 등록 순서상 Endurance 가 먼저):

| 앞 시험 | 종료 |
|---|---|
| (없음, 단독) | 0 |
| `EnduranceTest.*` 전체 | **1** |
| `FourThreadsConcurrentLoadOffset_NoCrash` | 0 |
| `FourThreadsConcurrentLoadGain_NoCrash` | 0 |
| `ThousandCycles_NocrashAllOk` | **1** |
| `ThousandCycles_MemoryGrowthUnderOneMB` | **1** |
| `FourThreadsMixedLoad_NoCrash` | **1** |

**결함 맵을 로드하는 세 시험만 오염시킨다**(`test_xpe_calib_endurance.cpp:77, :101, :109, :199` 의 `xpe_calib_load_defect_map`). `EnduranceTest` 픽스처에는 `SetUpTestSuite`/`TearDownTestSuite`(픽스처 파일 생성·삭제)만 있고 **시험마다 `init`/`shutdown` 이 없다** — 로드된 맵이 다음 `shutdown` 까지 남는다. `PipelineStageTest` 의 `SetUp` 은 `xpe_preprocess_init` 인데 `init` 은 `g_calib` 을 지우지 않는다.

시드 1 재실행(`a88-S1b.log`)에서 같은 두 시험이 다시 실패했다 — **결정적 재현**.

기본 순서에서는 이 시험이 실패하지 않는다. 등록 순서상 `EnduranceTest`(목록 175행)와 `PipelineStageTest`(282행) 사이에 `shutdown` 을 부르는 시험이 있을 것으로 보이지만 **확인하지 않았다**.

셔플에서 실패한 시드: **1** (16회 중 1회).

#### (다) `XpePreprocessEndurance.NoMemoryLeakAfter1000Frames` — 로드된 오프셋 맵 누수(추정)

시드 9 실패 원문:

```
test_xpe_preprocess_memleak.cpp(112): error: Value of: rc == 0 || rc == -6 || rc == -16
  Actual: false
Expected: true
[XpePreprocessEndurance] baseline PrivateUsage = 5912 KB, after 1000 frames = 5912 KB, delta = 0 KB (budget 2048 KB ...)
```

**메모리 단언이 아니다** — 증가 0 KB. 실패한 것은 `run_one_frame` 안의 `xpe_offset_correct` 반환 코드 단언(`:111-112`)이다. **실제 `rc` 값은 출력되지 않아 모른다.** 첫 프레임에서만 실패하는 것으로 보인다(오류 줄 1개; 프레임마다 끝에 `shutdown` 이 있다).

| 실행 | 종료 |
|---|---|
| 단독 × 3 | 0, 0, 0 |
| 전체 셔플 시드 9 × 3 (`S9`, `S9-r1`, `S9-r2`) | 1, 1, 1 |

시드 9 에서 바로 앞 시험: `Median8ParityTest.*` 4건, 그 앞 `CalibCacheConcurrencyTest.LoadUnderConcurrentEvictionNeverReportsSpuriousFailure`.

두 시험만 걸러 셔플(`--gtest_filter=CalibCacheConcurrencyTest.LoadUnderConcurrentEvictionNeverReportsSpuriousFailure:XpePreprocessEndurance.NoMemoryLeakAfter1000Frames --gtest_shuffle --gtest_random_seed=S`):

| 시드 | 실행 순서 | 결과 |
|---|---|---|
| 1 | Cache → Endurance | **실패** |
| 2 | Cache → Endurance | **실패** |
| 3 | Cache → Endurance | **실패** |
| 4 | Endurance → Cache | 통과 |
| 5 | Cache → Endurance | **실패** |
| 6 | Endurance → Cache | 통과 |

**순서와 결과가 6/6 일치한다.** `CalibCacheConcurrencyTest` 가 무엇을 남기는지(오프셋 맵으로 추정)는 코드로 확인하지 않았다.

셔플에서 실패한 시드: **9, 12** (16회 중 2회).

### 셔플 전체 (시드 1~16, 전체 바이너리)

명령: `xpe_preprocess_tests.exe --gtest_shuffle --gtest_random_seed=<S>`. 각 로그에 `Note: Randomizing tests' orders with a seed of <S> .` 가 찍혀 있음을 확인했다.

| 시드 | 로그 바이트 | 실패 |
|---|---|---|
| 1 | 100046 | (가), (나) |
| 2 | 98532 | — |
| 3 | 99912 | (가) |
| 4 | 99730 | (가) |
| 5 | 99726 | (가) |
| 6 | 98530 | — |
| 7 | 99724 | (가) |
| 8 | 99720 | (가) |
| 9 | 98818 | (다) |
| 10 | 99749 | (가) |
| 11 | 98548 | — |
| 12 | 99994 | (가), (다) |
| 13 | 99553 | (가) |
| 14 | 98538 | — |
| 15 | 99723 | (가) |
| 16 | 99727 | (가) |

16회 중 12회 실패, 실패 시험은 위 3건 외에 없음.

### C4 — 왜 지금까지 안 보였나

```
modules/preprocess/CMakeLists.txt:461   gtest_discover_tests(xpe_preprocess_tests ...
.github/workflows/ci.yml:154            run: ctest --test-dir build/ci-preprocess ...
```

`gtest_discover_tests` 는 gtest 케이스마다 ctest 항목 하나를 만들고, 각 항목은 `--gtest_filter=<그 케이스>` 로 **별도 프로세스**를 띄운다. 전역이 프로세스와 함께 사라지므로 **ctest 는 순서 의존을 볼 수 없다.** CI 의 preprocess·common·post 단계와 벤치마크·커버리지 모두 `ctest` 다. 바이너리를 직접 돌리는 경로는 저장소에 없다(`.github/workflows/*.yml` 에서 `xpe_preprocess_tests`·`gtest_shuffle`·`gtest_filter` 0건).

### C6 — 모드 누수가 영향을 주는 단언

- 모드 값을 단언하는 시험: `CalibModeTest.*` 뿐. 그 픽스처 `SetUp` 이 매번 `MULTI_POINT_8` 로 되돌린다.
- `calibration_mode` 를 읽는 다른 시험(`CalibQualityMetaTest`, `GainPolyLoadTest`): QA-A-86 에서 확인한 대로 **값을 단언하지 않는다**(키 존재만).
- `xpe_calib_get_max_points`/`get_poly_degree` 를 부르는 시험: `CalibModeTest` 뿐.

그러므로 **지금은** 모드 누수로 바뀌는 판정이 없다. 셔플 16회에서도 모드 때문에 실패한 시험은 없다(3건 모두 다른 전역).

### 원복

```
A88 markers: 0
git status --short:  ?? .claude/settings.json.bak-hook / ?? .claude/settings.json.bak-rmask
REBUILD_EXIT=0
[1/2] Building CXX object ...tests\test_calib_quality_meta.cpp.obj
[2/2] Linking CXX executable bin\xpe_preprocess_tests.exe
```

---

## 3. baseline 귀속 (Baseline-attribution)

- 모든 실행은 `3fdde65` + 임시 출력 1줄로 빌드한 같은 바이너리(`BUILD_EXIT=0`, 해당 파일 재컴파일 로그 확인)다. (나)·(다) 의 쌍 실행과 셔플도 같은 바이너리다.
- 로그: `a88-A1-default.log`(99860 B), `a88-A2-nomode.log`(97532), `a88-A3-alone.log`(835), `a88-P0/P1/P2.log`, `a88-Q*.log`, `a88-S1~S16.log`, `a88-S1b.log`, `a88-S9-r1/r2.log`, `a88-M-alone1~3.log`, `a88-R1~R6.log`, `a88-list.log`(21852). 모두 스크래치패드에 있고 크기를 확인했다.
- 임시 출력 줄은 잔차 시험의 단언 앞에서 JSON 을 찍을 뿐 단언을 바꾸지 않는다. 다른 시험의 판정에는 영향이 없다.

---

## 4. 미검증 (Gaps)

- **(가) 의 오염원 중 `CalibQualityMetaTest`·`GainPolyLoadTest` 는 쌍 실행으로 확인하지 않았다.** 생성기 호출이라는 코드 사실뿐이다.
- **(나) 가 기본 순서에서 왜 통과하는지**(중간에 `shutdown` 하는 시험) 확인하지 않았다.
- **(다) 의 `rc` 실제 값과, `CalibCacheConcurrencyTest` 가 남기는 상태를 코드로 확인하지 않았다.** 순서-결과 6/6 일치까지다.
- **셔플은 16개 시드뿐이다.** 드문 조합에서만 드러나는 순서 의존이 더 있을 수 있다. 시드 1~16 에서 (나)는 1회, (다)는 2회만 나왔다.
- **`--gtest_repeat` 로 같은 프로세스 안 반복**은 돌리지 않았다(자기 자신에게 오염되는 시험).
- **`ci-common`·`ci-post` 바이너리는 보지 않았다.** 이 카드는 preprocess 바이너리만이다.
- **다른 전역**(알림 큐, 캐시, 스레드 설정 등)을 전수하지 않았다. 셔플에서 드러난 셋만이다.

---

## 5. 잔여 위험 (Residual-risk)

- **바이너리를 직접 돌리면 기본 순서에서 이미 1건 빨강이다.** 개발자가 디버거나 필터 없이 바이너리를 실행하면 "원래 깨져 있던 시험"으로 읽힐 수 있다 — ctest 는 초록이라 두 결과가 어긋난다.
- **ctest 의 프로세스 분리가 누수를 숨긴다.** 반대로 말하면 **분리를 믿고 전역을 되돌리지 않는 시험이 계속 늘 수 있다.** 이번 3건이 그렇게 생겼을 것이다.
- **"이유 없이 통과"하는 방향**: 이번 3건은 모두 오염되면 실패하는 쪽이다. 오염되면 **통과하는** 단언(예: 앞 시험이 로드한 맵 덕분에 OK 가 나는 경우)은 셔플로도 잘 드러나지 않는다 — 단독 실행에서 실패해야 보인다. 전 시험 단독 실행은 사실상 ctest 가 하고 있고 초록이므로, 그런 단언은 **현재는 없다**고 볼 근거가 있다.
- **모드 누수는 지금은 무해하지만**, `calibration_mode` 값을 단언하는 시험이 `CalibModeTest` 밖에 생기는 순간 (가)와 같은 형태가 된다.

---

## 6. 목록 요약 (다음 카드 재료)

| # | 순서 의존 시험 | 새는 전역 | 오염원(확인됨) | 재현 시드 |
|---|---|---|---|---|
| (가) | `CalibModeTest.QualityMeta_InitialState` | `g_quality_meta` | `GenerateGainTest` (후보: `CalibQualityMetaTest`, `GainPolyLoadTest`) | 기본 순서, 또는 1·3·4·5·7·8·10·12·13·15·16 |
| (나) | `PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded` | `g_calib` (결함 맵) | `EnduranceTest` 의 `ThousandCycles_NocrashAllOk`, `ThousandCycles_MemoryGrowthUnderOneMB`, `FourThreadsMixedLoad_NoCrash` | 1 |
| (다) | `XpePreprocessEndurance.NoMemoryLeakAfter1000Frames` | `g_calib` 로 추정 | `CalibCacheConcurrencyTest.LoadUnderConcurrentEvictionNeverReportsSpuriousFailure` (순서-결과 6/6) | 9, 12 |
| — | (판정 영향 없음) | `g_calib_mode` | `CalibModeTest` 의 4건 (AUTO 로 끝남) | 기본 순서 |

---

관련: QA-A-87 보고의 관찰 ② (#140 작업 중 발견). 이 주제의 이슈는 아직 없다 — 카드 지시대로 이슈에 달지 않았다.
