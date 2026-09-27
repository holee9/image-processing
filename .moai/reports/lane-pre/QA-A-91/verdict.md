# QA-A-91 — 시드 2 순서 의존: 저장·로드 시험이 남긴 보정 맵 (#176)

**카드**: `.moai/lanes/pre/inbox/QA-A-91.md` · **브랜치**: `dev/preprocess` · **커밋**: `b35c0a3` (미푸시) · **병합 기준**: `180acc2` · **기계**: Intel Core i7-12700

바뀐 파일은 시험 4개뿐이다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | 수정 전 트리에서 **재현했다**: 시드 2 전체 실패, 오염원 두 개와의 쌍 실행 둘 다 실패, 단독 통과 |
| C2 | 오염원 두 픽스처(`CalibLoadTest`, `CalibSaveTest`)와, 같은 형태인 캐시 픽스처 두 곳의 `TearDown` 에서 보정 맵을 해제한다 |
| C3 | **반증: 오염원 하나씩 정리를 빼면 그 쌍만 빨강**이다. 의존 쪽을 고치지 않았으므로 A-89 와 달리 하나씩 드러난다 |
| C4 | **시드 1~32 전부 실패 0**, 매 실행 **580건** 실행. **실패한 시드는 없다** |
| C5 | 기본 순서 실패 0, ci-preprocess 649/649, ci-common 69/69, 빌드 경고 0 |

---

## 2. 증거 (Evidence)

### C1 — 재현 (수정 전)

병합 후 빌드는 `ninja: no work to do` 였다 — 병합된 커밋들이 preprocess 소스를 건드리지 않아 바이너리가 A-90 수정본 그대로다.

| 실행 | 실행 수 | 결과 |
|---|---|---|
| `--gtest_shuffle --gtest_random_seed=2` | 580 | 571 통과 / **1 실패** (`GenerateOffsetConfigTest.SigmaClipProducesADifferentMapThanMean`) |
| `--gtest_filter=CalibLoadTest.*:<대상>` | 20 | 19 통과 / **1 실패** (대상) |
| `--gtest_filter=CalibSaveTest.*:<대상>` | 10 | 8 통과 / **1 실패** (대상) |
| `--gtest_filter=<대상>` | 1 | 1 통과 |

### C2 — 수정

네 픽스처 `TearDown` 맨 앞(또는 캐시 정리 뒤)에:

```cpp
(void)xpe_preprocess_init(nullptr);
xpe_preprocess_shutdown();
```

| 픽스처 | 파일 | 이유 |
|---|---|---|
| `CalibLoadTest` | `test_xpe_calib_load.cpp` | 오염원 — offset/gain/defect 맵을 로드하고 정리 없음 |
| `CalibSaveTest` | `test_xpe_calib_save.cpp` | 오염원 — 같음 |
| `CalibCacheOwnershipTest` | `test_calib_cache_ownership.cpp` | 캐시 미스가 활성 맵을 설치하는데 정리 없음 |
| `CalibrationCacheTest` | `test_calibration_cache.cpp` | 같음 |

- `init` 을 먼저 부르는 이유는 A-89·A-90 과 같다 — `shutdown` 을 초기화된 모듈에서 부르기 위해.
- 캐시 픽스처 두 곳은 **이번 순서에서 드러나지 않았다.** 카드 지시("드러나는 순서가 지금 없더라도 고칩니다")대로 같이 고쳤다. 이 두 곳을 되돌려 드러나는 순서를 찾는 반증은 하지 않았다.
- **의존 쪽(`GenerateOffsetConfigTest`)은 고치지 않았다.**

수정 후 빌드: 네 파일 재컴파일(`[1/5]`~`[4/5] Building CXX object ...`), `BUILD_EXIT_f91=0`.

| 실행 | 실행 수 | 결과 |
|---|---|---|
| `CalibLoadTest.*` + 대상 | 20 | 20 통과 |
| `CalibSaveTest.*` + 대상 | 10 | 9 통과, 실패 0 |
| 시드 2 | 580 | 572 통과, 실패 0 |

`CalibSaveTest` 쌍의 10 실행 중 9 통과는 수정 전에도 8 + 1 = 9 였다 — 1건은 원래 건너뛰는 시험이다.

### C3 — 반증

수정본을 스크래치패드에 보관하고, 한 파일만 `git show HEAD:<file>` 로 되돌려 재빌드(해당 `.obj` 재컴파일 1건 확인) → 두 쌍 실행 → 수정본 복원.

| 되돌린 파일 | `CalibLoadTest` 쌍 | `CalibSaveTest` 쌍 |
|---|---|---|
| `test_xpe_calib_load.cpp` | 19 통과 / **1 실패** (대상) | 9 통과 / 실패 0 |
| `test_xpe_calib_save.cpp` | 20 통과 / 실패 0 | 8 통과 / **1 실패** (대상) |

**각 오염원의 정리가 자기 쌍을 막는다는 것이 따로 보인다.** A-89 의 (나)(다)는 의존 쪽과 오염원 쪽을 둘 다 고쳐서 "하나만 빼면 초록"이었지만, 이번에는 의존 쪽을 고치지 않아 "하나만 빼면 그 쌍이 빨강"이다.

복원 후 재빌드 `BUILD_EXIT_f91b=0`(`test_xpe_calib_save.cpp.obj` 재컴파일), `git diff --stat`: `4 files changed, 28 insertions(+)`.

### C4 — 시드 1~32

명령: `build/ci-preprocess/bin/xpe_preprocess_tests.exe --gtest_shuffle --gtest_random_seed=N`, N = 1 … 32.

| 시드 | 실행 수 | 통과 | 실패 |
|---|---|---|---|
| 1 | 580 | 572 | 0 |
| 2 | 580 | 572 | 0 |
| 3 | 580 | 572 | 0 |
| 4 | 580 | 572 | 0 |
| 5 | 580 | 572 | 0 |
| 6 | 580 | 572 | 0 |
| 7 | 580 | 572 | 0 |
| 8 | 580 | 572 | 0 |
| 9 | 580 | 572 | 0 |
| 10 | 580 | 572 | 0 |
| 11 | 580 | 572 | 0 |
| 12 | 580 | 572 | 0 |
| 13 | 580 | 572 | 0 |
| 14 | 580 | 572 | 0 |
| 15 | 580 | 572 | 0 |
| 16 | 580 | 572 | 0 |
| 17 | 580 | 572 | 0 |
| 18 | 580 | 572 | 0 |
| 19 | 580 | 572 | 0 |
| 20 | 580 | 572 | 0 |
| 21 | 580 | 572 | 0 |
| 22 | 580 | 572 | 0 |
| 23 | 580 | 572 | 0 |
| 24 | 580 | 572 | 0 |
| 25 | 580 | 572 | 0 |
| 26 | 580 | 572 | 0 |
| 27 | 580 | 572 | 0 |
| 28 | 580 | 572 | 0 |
| 29 | 580 | 572 | 0 |
| 30 | 580 | 572 | 0 |
| 31 | 580 | 572 | 0 |
| 32 | 580 | 572 | 0 |

- 32개 로그 모두 `Note: Randomizing tests' orders with a seed of N .` 와 요약 줄(`tests from … test suites ran`)이 있다. 요약 줄이 없는 로그는 무효로 표시하도록 판독 함수를 짰고, 무효는 0건이다.
- **실행 수 580 은 32회 모두 같다.** 580 = 572 통과 + 원래 건너뛰는 8건.
- **실패한 시드: 없음.**

### C5 — 전체

```
기본 순서(인자 없음): ran=580  572 통과  실패 0
PRE_BUILD_EXIT=0  warn=0
PRE_CTEST_EXIT=0  100% tests passed, 0 tests failed out of 649
COM_BUILD_EXIT=0  warn=0
COM_CTEST_EXIT=0  100% tests passed, 0 tests failed out of 69
```

### 코멘트

```
https://github.com/holee9/image-processing/issues/176#issuecomment-5708133983
check: n=4 sha=true
```

---

## 3. baseline 귀속 (Baseline-attribution)

- 재현은 병합 기준 `180acc2` 의 바이너리(A-90 수정 포함, 이 카드 수정 전)다.
- 수정 후·시드 32개·ctest 는 `180acc2` + 이 카드 변경 4파일로 빌드한 바이너리다.
- 실행 수 580 은 A-90 의 572 통과 + 8 건너뜀과 같은 구성이다(시험 수 변화 없음) — 그래서 **시드 1~16 의 실행 순서는 A-90 과 같다.** A-90 에서 실패하던 시드 2 가 이번에 초록이 된 것은 순서가 바뀌어서가 아니라 수정 때문이다.

---

## 4. 미검증 (Gaps)

- **캐시 픽스처 두 곳의 수정을 반증하지 않았다** — 드러나는 순서가 없어 빨강을 만들 짝이 없다.
- **시드 33 이상**은 돌리지 않았다.
- **셔플 실행 소요 시간**은 재지 않았다.
- **`ci-common`·`ci-post` 바이너리의 순서 의존**은 보지 않았다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **의존 쪽(`GenerateOffsetConfigTest`)은 여전히 "결함 맵이 없다"를 스스로 보장하지 않는다.** 결함 맵을 남기는 새 시험이 생기면 같은 실패가 돌아온다. 오염원을 막는 방식만으로는 앞으로 생길 오염원을 막지 못한다.
- **시험 수가 바뀌면 같은 시드의 순서가 바뀐다.** CI 에 고정할 시드의 의미는 시험 수와 함께만 유지된다. 이번 측정은 **실행 580건 기준**이다.
- **`b35c0a3` 은 미푸시다.**

---

Refs #176
