# QA-A-90 — shutdown 헤더와 코드의 불일치, 캐시 크기 복원 (#176)

**카드**: `.moai/lanes/pre/inbox/QA-A-90.md` · **브랜치**: `dev/preprocess` · **커밋**: `8eca1c4` (미푸시) · **병합 기준**: `d0737d8` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **초기화 전에 보정 맵을 로드할 수 있다** — 실행으로 확인(`XPE_OK`). 로더 소스 4곳에 초기화 검사가 없다 |
| C2 | **초기화 없는 shutdown 은 그 맵을 해제한다** — 실행으로 확인(`-16`). 대조군(shutdown 없음)에서는 맵이 남아 쓰인다(`XPE_OK`) |
| C3 | 따라서 **공개 헤더("no-op")가 틀렸고 코드가 맞다.** 헤더를 고쳤고 코드는 그대로다 |
| C4 | 반증: 코드를 헤더대로 바꾸면 **새 시험 1건만** 실패(기대 −16, 실제 0), 전체 기본 순서에서도 그 1건뿐 |
| C5 | 캐시 시험 픽스처 **세 곳** TearDown 에서 기본 크기 4 로 되돌린다. 크기 변경 뒤 기본 크기를 가정하는 시험은 **없다** |
| C6 | ci-preprocess **649/649**, ci-common **69/69**, 경고 0. 직접 실행 기본 순서 0 실패 |
| C7 | **셔플 시드 2 에서 이 변경과 무관한 새 순서 의존 1건**이 드러났다. 오염원 2개를 쌍 실행으로 확정했고, 고치지 않았다 |
| C8 | 도중에 **빌드에 없는 파일에 시험을 썼다가** "0 tests" 로 알아채고 옮겼다 |
| C9 | 커밋 메시지의 "로더 다섯 곳"은 부정확하다(`pipeline.cpp` 는 호출자). 확인한 것은 로더 4곳이다 |

---

## 2. 증거 (Evidence)

### C1 — 로더의 초기화 검사

```
grep -c 'is_initialized\|g_initialized'
  modules/preprocess/src/xpe_calib_load_offset.cpp       0
  modules/preprocess/src/xpe_calib_load_gain.cpp         0
  modules/preprocess/src/xpe_calib_load_defect_map.cpp   0
  modules/preprocess/src/calibration_cache.cpp           0
  modules/preprocess/src/pipeline.cpp                    0   ← 로더가 아니라 호출자
```

기존 증거도 같은 방향이었다: `EnduranceTest` 는 초기화 없이 `xpe_calib_load_*` 를 1000회 부르고 `ASSERT_EQ(..., XPE_OK)` 를 통과한다(QA-A-88·A-89).

### C2 — 실행

`test_xpe_preprocess_init.cpp` 에 `PreprocessLifecycleTest` 시험 2건을 추가했다(4×4 오프셋 맵, `fixtures/make_xcal.hpp` 의 `MakeOffsetXCal`).

```
[ RUN      ] PreprocessLifecycleTest.ShutdownWithoutInitReleasesMapsLoadedWithoutInit
[       OK ] PreprocessLifecycleTest.ShutdownWithoutInitReleasesMapsLoadedWithoutInit (4 ms)
[ RUN      ] PreprocessLifecycleTest.MapLoadedWithoutInitSurvivesInitWithoutShutdown
[       OK ] PreprocessLifecycleTest.MapLoadedWithoutInitSurvivesInitWithoutShutdown (6 ms)
```

| 시험 | 절차 | 단언 |
|---|---|---|
| 첫째 | 초기화 없이 로드 → 초기화 없이 shutdown → 초기화 → 보정 | 로드 `XPE_OK`(전제), 보정 `XPE_ERR_CALIB_NOT_LOADED` |
| 둘째(대조군) | 초기화 없이 로드 → 초기화 → 보정 | 보정 `XPE_OK` |

둘째가 통과하므로 "초기화가 맵을 지운다"는 대안 설명은 배제된다. 첫째의 −16 은 그 사이 shutdown 이 만든 것이다.

두 시험은 시작할 때 `init → shutdown` 으로 비운다 — 시험 대상인 "미초기화 shutdown" 동작에 기대지 않으려고.

### C3 — 판단

| 근거 | 방향 |
|---|---|
| 로더가 초기화를 요구하지 않는다(C1) | 초기화 전 `g_calib` 이 비어 있지 않을 수 있다 → 지우는 것이 관측 가능하다 |
| 헤더 첫 줄 "Shutdown preprocessing module and **release resources**", REQ-P1A-031 "No memory leak after shutdown" | 미초기화 상태에서 로드한 맵을 남기면 성립하지 않는다 |
| 같은 파일 픽스처 `SetUp`: 초기화 전 `xpe_preprocess_shutdown()` 에 "Ensure clean state before each test" | 코드 쪽 동작에 이미 기대는 사용처 |
| A-89 수정 전 여러 시험의 "init 전 shutdown 으로 정리" 패턴 | 같음 |

카드가 제시한 두 갈래 중 "요구하지 않는다 → 헤더가 틀렸다" 쪽이다.

**고친 헤더** (`preprocess_api.h`, `xpe_preprocess_shutdown`):

```diff
- * Safe to call multiple times. If module is not initialized, this is a no-op.
- * After shutdown, module returns to uninitialized state and can be re-initialized.
+ * Safe to call multiple times. After shutdown, the module is in the
+ * uninitialized state and can be re-initialized.
+ *
+ * Releases the loaded offset map, gain map or polynomial gain coefficients,
+ * and defect map, whether or not the module was initialized. The calibration
+ * loaders do not require initialization, so maps may be loaded before
+ * xpe_preprocess_init(); a shutdown at that point releases them too (it is
+ * not a no-op).
+ *
+ * Does not reset the calibration mode set by xpe_calib_set_mode() or the
+ * quality metadata returned by xpe_calib_get_quality_meta(); both keep their
+ * values across shutdown and re-initialization.
```

- 해제 대상은 `CalibrationData`(`xpe_preprocess_internal.h`)의 `offset_map`, `gain_map`, `gain_poly_coeffs`, `defect_map` 과 대조했다.
- 모드·품질 메타 유지는 QA-A-88 실행(시험 사이에 shutdown 이 있었는데도 모드 5, 품질 메타 값이 남음)과 `xpe_calib_mode.cpp:32`, `:35` 에 근거한다.
- 캐시가 shutdown 에서 비워지는지는 **확인하지 않아서 적지 않았다.**
- Doxygen 경고를 피하려고 `#` 와 특수 기호를 쓰지 않았다. **로컬에 doxygen 이 없어 실행하지 못했다.**

### C4 — 반증

`preprocess.cpp` 의 `xpe_preprocess_shutdown` 에 임시로:

```cpp
if (!g_initialized.load(std::memory_order_acquire)) return; /* FALSIFY-A90 */
```

```
BUILD_EXIT_a90f=0
[1/4] Building CXX object ...xpe_preprocess.dir\src\preprocess.cpp.obj
[2/4] Linking CXX shared library bin\xpe_preprocess.dll

PreprocessLifecycleTest.*   : [  PASSED  ] 19 tests. [  FAILED  ] 1 test
  test_xpe_preprocess_init.cpp(440): error: Expected equality of these values:
    -16
    CorrectOneSmallFrame()
      Which is: 0
전체 기본 순서               : [  PASSED  ] 571 tests. [  FAILED  ] 1 test  (같은 시험)
```

원복: `git diff -- modules/preprocess/src` 출력 없음, `FALSIFY` 계수 0.

**전체 기본 순서에서도 1건뿐**이라는 것은, 기본 순서에서는 다른 시험이 "미초기화 shutdown 이 정리한다"에 기대어 판정이 바뀌지 않는다는 뜻이다. `PreprocessLifecycleTest` 픽스처의 초기화 전 shutdown 이 실제로 정리할 것이 있었던 경우가 있는지는 확인하지 않았다. 셔플 순서에서 같은 반증은 돌리지 않았다.

### C5 — 캐시 크기

기본값 `calibration_cache.cpp:205` `maxSize_{4}`.

| 픽스처 | SetUp | 시험 안 변경 | TearDown 추가 |
|---|---|---|---|
| `CalibCacheConcurrencyTest` | 4 | 1 (`:165`) | `xpe_calib_cache_set_max_size(4)` |
| `CalibCacheOwnershipTest` | 4 | 1 (`:156`), 1 (`:187`) | 같음 |
| `CalibrationCacheTest` | 4 | 1 (`:204`), 0 (`:221`) | 같음 |

카드는 "캐시 시험"(concurrency)만 지목했지만 같은 형태가 세 픽스처에 있어 셋 다 고쳤다.

**드러날 짝 탐색**:

```
modules/preprocess/src 에서 g_calibCache / _cached( 사용 (calibration_cache.cpp 제외): 0건
_cached( | cache_set_max_size | cache_clear 를 부르는 시험 파일:
  test_calib_cache_concurrency.cpp
  test_calib_cache_ownership.cpp
  test_calibration_cache.cpp
  test_error_precedence.cpp        ← 널 포인터 인자 4건(:154, :157, :159, :161)뿐
```

- 캐시는 자기 공개 함수로만 쓰인다.
- 위 세 픽스처는 모두 SetUp 에서 4 로 맞춘다.
- `test_error_precedence.cpp` 는 캐시 동작을 보지 않는다.
- 크기를 읽는 공개 getter 가 없다(`preprocess_api.h:656`, `:665` 는 clear 와 set 뿐).

**결과: 드러날 시험이 없다.** 짝 실행은 대상이 없어 하지 않았다.

### C6 — 검증

```
PRE_BUILD_EXIT=0   warn=0   (빌드 로그 10534 B, [1/92] 부터 재컴파일 — 공개 헤더 변경)
PRE_CTEST_EXIT=0   100% tests passed, 0 tests failed out of 649
COM_BUILD_EXIT=0   warn=0
COM_CTEST_EXIT=0   100% tests passed, 0 tests failed out of 69
ctest 로그에서 새 시험 이름 4줄 확인

직접 실행 기본 순서: [  PASSED  ] 572 tests. fails=[]
셔플 시드 1~16    : 시드 2 를 제외한 15회 572 통과 / 0 실패
```

### C7 — 시드 2 의 새 순서 의존

```
[ RUN      ] GenerateOffsetConfigTest.SigmaClipProducesADifferentMapThanMean
test_calib_generate_offset_config.cpp(129): error: Expected equality of these values:
  0
  generate(series, "clip.xcal", "{\"method\":\"sigma_clip\",\"sigma\":1.0}")
    Which is: -4
```

| 실행 | 결과 |
|---|---|
| 시드 2 재실행 × 2 | 같은 1건 실패, 2/2 |
| 대상 단독 × 3 | 3/3 통과 |
| `MultiOffsetTest.*` + 대상 | 통과 |
| `SigmaClipConformanceTest.*` + 대상 | 통과 |
| **`CalibLoadTest.*` + 대상** | **대상 실패** |
| **`CalibSaveTest.*` + 대상** | **대상 실패** |

- `-4` = `XPE_ERR_CONFIG_INVALID`. sigma_clip 경로는 결함 표시를 `g_calib.defect_map` 에 합치는데, `xpe_calib_generate_offset.cpp:61-64` 가 **이미 있는 결함 맵의 크기가 다르면** 이 값을 낸다. (나)와 같은 계열이다.
- `GenerateOffsetConfigTest` 픽스처 `SetUp` 은 `xpe_preprocess_init` 만 부른다.
- `test_xpe_calib_load.cpp`, `test_xpe_calib_save.cpp` 의 `shutdown` 호출: 각 0건.
- 후보 선정: 시드 2 에서 대상 앞에 돈 64개 스위트 중 결함 맵을 로드·생성하는 시험 파일에 속한 스위트 16개(스위트-파일 쌍 기준) → 파일의 `shutdown` 호출이 0건인 4개 스위트부터 쌍 실행.

**왜 지금 드러났나** — 시드 2 는 A-88·A-89 에서 실패 0 이었다. 이번에 시험이 2건 늘어(570 → 572) **같은 시드의 실행 순서가 달라졌다.** 이 변경이 결함을 만든 것이 아니다(두 오염원과 대상 모두 이번에 건드리지 않았다).

**고치지 않았다** — 카드 범위 밖이고, A-88 처럼 목록부터다. A-89 의 (나)와 같은 수정(의존 쪽 `init → shutdown → init`, 오염원 쪽 TearDown)이 들어맞을 것으로 보이지만 검증하지 않았다.

### C8 — 빌드에 없는 파일

처음에 두 시험을 `test_xpe_preprocess.cpp` 에 썼다.

```
BUILD_EXIT_a90probe=0      빌드 로그: ninja: no work to do.
--gtest_filter=PreprocessShutdownContract.*  →  [  PASSED  ] 0 tests.
```

`modules/preprocess/CMakeLists.txt:336`: `# tests/test_xpe_preprocess.cpp` — QA-A-21 사유로 빌드에서 빠진 파일이다. 쓴 시험은 **컴파일조차 되지 않는 코드**였다. `git checkout --` 로 그 파일을 되돌리고, 빌드되는 `test_xpe_preprocess_init.cpp` 로 옮겼다. 옮긴 뒤에는 **시험 이름이 실행 목록에 나타나는지**까지 확인했다.

"0 tests" 는 `fails=[]` 와 같은 모양의 초록이다 — 실패가 없다는 것이지 통과했다는 것이 아니다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 반증과 C2 실행은 `d0737d8` + 이 카드 변경으로 빌드한 바이너리다(반증은 `preprocess.cpp` 임시 변경 포함, 원복 확인).
- 전체 검증은 원복 후 두 프리셋 전체 재빌드분이다.
- 시드 2 조사는 전체 검증과 같은 바이너리다.
- 로그: `a89-a90pair.log`, `a89-a90fpair.log`, `a89-a90fall.log`, `a90-pre/com-*.log`, `a89-v90-*.log`, `a89-s2r1/2.log`, `a89-alone1~3.log`, `a89-pair-*.log` — 스크래치패드, 요약 줄 확인.

---

## 4. 미검증 (Gaps)

- **Doxygen 미실행.** 공개 헤더 주석을 바꿨다 — CI Documentation Generation 결과가 유일한 증거가 된다.
- **gain·defect 로더의 초기화 전 로드는 실행하지 않았다.** 실행한 것은 offset 로더다. 나머지는 코드 계수(초기화 검사 0건)와 `EnduranceTest` 의 기존 통과에 근거한다.
- **캐시가 shutdown 에서 비워지는지** 확인하지 않았다(헤더에도 적지 않음).
- **시드 2 의 두 오염원 중 어느 시험이 정확히 남기는지**(시험 단위)는 좁히지 않았다. 스위트 단위까지다.
- **CalibLoadTest·CalibSaveTest 외의 오염원**이 더 있을 수 있다. `shutdown` 을 부르는 파일에 속한 나머지 12개 후보 스위트는 쌍 실행하지 않았다(시험 중간에만 부르고 끝에 안 부를 수 있다).
- **`CalibCacheOwnershipTest`, `CalibrationCacheTest` 도 캐시 로더로 활성 맵을 남길 수 있다**(캐시 미스가 활성 맵을 설치 — A-89). TearDown 에 정리가 없다. 이번 카드 범위가 아니라 고치지 않았고, 셔플에서 드러나지도 않았다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **헤더가 이제 동작을 약속한다.** "초기화 여부와 무관하게 해제", "모드·품질 메타는 유지" — 나중에 둘 중 하나를 바꾸면 헤더와 새 시험(첫째 약속)을 같이 바꿔야 한다. 둘째 약속(모드·품질 메타 유지)을 지키는 시험은 없다.
- **시드 2 실패는 그대로다.** CI 는 기본 순서와 시드 1·9 만 돌리므로 CI 에서는 드러나지 않는다. 시험 수가 또 바뀌면 다른 시드에서 나타날 수 있다.
- **"0 tests" 초록** — CI 의 필터 실행에도 같은 함정이 있다. 필터가 아무것도 안 잡으면 통과처럼 보인다.
- **`8eca1c4` 는 미푸시다.**

---

Refs #176
