# QA-A-61 — 호출자가 스레드 수를 지정한다 (#144)

수정 파일
- `modules/preprocess/include/runtime_detection.h` (`threadCount` 필드 + `ComputeGlobalSigmaThreaded` + `DetectFrame`)
- `modules/preprocess/tests/test_runtime_detection_thread_parity.cpp` (신규, 단언 4건)
- `modules/preprocess/tests/test_runtime_detection_performance_gate.cpp` (상한 10.00 확정 표기)
- `modules/preprocess/tools/xpe_detect_experiment.cpp` (`--shipped-threads` 측정)

**알고리즘·규칙·임계 무변경. 스레드 풀 없음. 공유 상태 없음. 내보내기 심볼 45개 그대로.**

---

## §1. Claim — 주장

| # | 주장 |
|---|---|
| C1 | 스레드 수는 `RuntimeDetectionConfig.threadCount` 이고 **기본 1**. 모듈은 하드웨어 병렬도를 읽지 않는다 |
| C2 | 기본 1은 기존 경로 그대로다 — `--regress diff` **바이트 단위 동일** |
| C3 | **N 스레드 결과가 1 스레드와 비트 단위로 같다** — σ와 결함 맵 양쪽, N ∈ {1,2,8,12}, 프레임 4형태 × 5종 |
| C4 | 재진입: `threadCount` 는 인자이고 모듈 상태가 아니다. 서로 다른 스레드 수의 동시 호출이 **각자 단일 스레드 답**을 받는다 |
| C5 | 반복 실행 결정성 8회 확인 |
| C6 | **반증 1(카드가 지정한 형태)은 발동하지 않았고, 그 자체가 발견이다** — 중복 처리는 이 설계에서 원리적으로 무해하다 |
| C7 | 반증 2·3(행 누락, 공유 스크래치 경쟁)은 발동한다 — 단언이 실제 분할 손상을 본다 |
| C8 | **출고 코드의 스레드 확장은 A-58 프로브보다 나쁘다** — σ 포화가 12→16으로 밀리고 배수가 5.52배→4.39배 |
| C9 | 게이트는 단일 스레드 그대로, 상한 10.00 확정 표기. 현재 비율 6.894 |
| C10 | 재실측 **622/622**(618+4), 69/69, 경고 0, 내보내기 45개 불변 |

---

## §2. Evidence — 증거

### 2-1. 형태 (카드 1항)

`RuntimeDetectionConfig` 에 `int32_t threadCount` 를 더하고 `RuntimeDetection_DefaultConfig()` 에서 **1**로 둔다.
1 이하는 1로 취급한다. 기존 공개 진입점 `xpe_defect_detect_runtime` 은 **한 줄도 바뀌지 않았고**,
그래서 기본 동작은 정의상 이전과 같다. `--regress diff` 로 확인:

```
diff .moai/reports/lane-pre/QA-A-50/a50-regress-bm.log  <현재 --regress 출력>
→ (차이 없음)
```

두 함수를 추가했다:
- `ComputeGlobalSigmaThreaded(img, threadCount)` — 차분 생성·히스토그램·절댓값 변환을 나누고,
  **스레드별 테이블을 합친다**. `threadCount == 1` 이면 기존 `ComputeGlobalSigma` 를 그대로 부른다
- `DetectFrame(img, config, map)` — **σ를 분할 전에 한 번** 계산한 뒤 행 구간을 나눈다

둘 다 인라인이다. 비인라인으로 두면 DLL 밖에서 링크되지 않아 테스트가 부를 수 없다
(이 레인이 `g_calib` 에서 두 번 겪은 LNK2001 과 같은 형태).

**공개 C ABI 는 건드리지 않았다** — `dumpbin /exports` 45개, `preprocess_api.h` 의 `XPE_API` 45개로 일치.
**스레드 수를 C ABI 로 노출하려면 내보내기가 바뀌므로 별도 판정이 필요하다**(§5-1).

### 2-2. 왜 동일성이 기대되는가 — 그리고 그것을 단언한다 (카드 2항)

우연이 아니라 **구성상** 그렇고, 단언은 그 구성이 실제로 출고됐는지 확인한다:

1. 각 화소의 판정은 **입력 프레임과 config 만** 읽는다. 어떤 화소도 다른 화소의 판정을 읽지 않고,
   각자 자기 맵 바이트만 쓴다 → 행 분할이 값을 바꿀 수 없다
2. σ는 **분할 전에 프레임 전체에서 한 번** 계산된다. 그 자체의 병렬형은 **정수 히스토그램 개수를
   합산**하는데, 정수 덧셈은 정확하고 결합법칙이 성립하므로 병합 테이블이 단일 스레드와 같다

**부동소수 비결합성** — 스레드 수치가 어긋나는 통상적 이유 — 은 **들어오지 않는다.** 스레드 간에
float 를 더하는 곳이 한 군데도 없다.

단언 4건(전부 **raw 비트**, `EXPECT_FLOAT_EQ` 는 4 ulp 허용이라 틀린 단언):

| 테스트 | 내용 |
|---|---|
| `GlobalSigmaIsIdenticalAtEveryThreadCount` | 512²·640×480·129×257·3×1024 × 프레임 5종 × N∈{1,2,8,12}, 그리고 **0·−4 도 1처럼 동작** |
| `DefectMapIsIdenticalPixelForPixelAtEveryThreadCount` | 640×480(**비정사각**: 행 분할이 고르지 않게 떨어진다) × 5종, **화소마다** 비교. 실제 이상치를 심어 all-zero 로 자명하게 통과하지 않게 함 |
| `ConcurrentCallsWithDifferentThreadCountsAreIndependent` | 8스레드·3스레드 호출을 `std::async` 로 동시에, 각자 4회 반복 |
| `RepeatedThreadedRunsAreDeterministic` | 12스레드로 8회, 매번 첫 결과와 일치 |

결과: **4/4 통과.**

### 2-3. 반증 — 셋을 했고 첫 번째는 발동하지 않았다 (카드 반증 절)

**반증 1 — 카드가 지정한 형태: 경계 행을 두 워커가 중복 처리.**
빌드 `BUILD_EXIT=0`, `[15/15] Linking` 확인 → **4/4 통과, 발동하지 않음.**

카드는 "통과하면 단언이 분할을 보고 있지 않다는 뜻" 이라고 했다. 확인해 보니 **다른 이유**였다 —
**중복 처리가 이 설계에서 원리적으로 무해하다.** 화소 판정은 입력만 읽는 결정적 함수이고
결과는 `map[i] = 1` 이라 **멱등**이다. 같은 행을 두 번 돌리면 같은 값을 두 번 쓴다.
즉 이 조작은 **손상이 아니라서** 잡을 것이 없다. 단언의 결함이 아니다.

그래서 **실제로 손상을 만드는 두 가지**로 다시 시험했다.

**반증 2 — 행 누락**(워커마다 마지막 행을 빠뜨림). 빌드 exit 0, `[15/15] Linking`:
```
kind 0 threads 2: 3 pixels differ, first at index 153336 (row 239, col 376); 616 pixels flagged single-threaded
[  FAILED  ] ThreadParityTest.DefectMapIsIdenticalPixelForPixelAtEveryThreadCount
[  FAILED  ] ThreadParityTest.ConcurrentCallsWithDifferentThreadCountsAreIndependent
```
row 239 는 480행을 둘로 나눈 **정확히 그 경계**다. 진단이 좌표까지 짚는다.

**반증 3 — 공유 스크래치 버퍼**(모든 워커가 한 쌍의 벡터를 공유 = 진짜 데이터 경쟁).
빌드 exit 0, `[15/15] Linking`:
```
kind 0 threads 2: 30 pixels differ, first at index 44029 (row 68, col 509); ...
[  FAILED  ] DefectMapIsIdenticalPixelForPixelAtEveryThreadCount
[  FAILED  ] ConcurrentCallsWithDifferentThreadCountsAreIndependent
run 0 differs from the first
[  FAILED  ] RepeatedThreadedRunsAreDeterministic
```
**결정성 단언까지 깨진다** — 경계 안쪽(row 68)에서 터지는, 실행마다 달라지는 손상이다.
이것이 스레드 도입의 전형적 결함이고, 단언 셋이 모두 잡는다.

**복원 확인**: `FALSIFY` 문자열 0건, 빌드 exit 0, 4/4 통과.

### 2-4. 출고 코드의 스레드 확장 (카드 3항)

명령 `xpe_detect_experiment --shipped-threads` — **프로브가 아니라 `ComputeGlobalSigmaThreaded`·
`DetectFrame` 을 직접** 부른다.

```
[shipped-threads] 3072x3072, best of 5, SHIPPED code (not a probe)
   threads     sigma ms   speed-up     frame ms   speed-up    sigma value
         1        149.4      1.00x        695.1      1.00x      10.008017
         2         92.6      1.61x        430.9      1.61x      10.008017
         4         51.8      2.88x        254.0      2.74x      10.008017
         8         41.6      3.59x        183.2      3.79x      10.008017
        12         35.1      4.26x        129.7      5.36x      10.008017
        16         34.0      4.39x        102.3      6.80x      10.008017
        20         41.8      3.58x        102.8      6.76x      10.008017
```

**A-58 프로브와 다르다 — 카드가 이 확인을 요구한 이유가 여기 있다.**

| 전역 σ | A-58 프로브 | **A-61 출고 코드** |
|---|---|---|
| 1스레드 | 143.2 ms | 149.4 ms |
| 12스레드 | **26.0 ms (5.52배)** | **35.1 ms (4.26배)** |
| 포화 지점 | 12 | **16** (34.0 ms, 4.39배) |
| 20스레드 | 26.3 ms | 41.8 ms (**악화**) |

**출고 코드가 프로브보다 35% 느리고 배수도 낮다.** 프로브는 히스토그램 테이블을 타이밍 밖에서
한 번 잡았지만 출고본은 **호출마다 `T × 256 KB` 를 할당**하고 차분 생성에도 스레드를 띄운다.
A-58 이 "프로브는 풀 없이 잰 보수적 측정" 이라고 적었는데, **테이블 할당 쪽에서는 반대로
낙관적이었다.**

그리고 A-58 의 합산 추정(20스레드 60.2 + 26.3 = **86.5 ms**)에 대해, 출고 코드의 **실측 전체**는
16스레드에서 **102.3 ms** 다 — **18% 낙관적이었다.** A-58 이 "덧셈이지 측정이 아니다" 라고
명시해 둔 것이 그대로 확인됐다.

**σ 값은 모든 스레드 수에서 10.008017** 로 같다(표의 마지막 열). 여기서는 일관성 지표가 아니라
§2-2 의 비트 파리티 단언이 뒷받침한다.

### 2-5. 게이트 (카드 게이트 절)

**단일 스레드 그대로다.** 다중 스레드 게이트는 CI 러너 코어 수에 따라 흔들려 A-60 이 방금
없앤 문제를 다른 축으로 되돌린다. 상한 10.00 의 **잠정 표기를 떼고** 두 기계 측정치를 적었다
(개발 7.125~7.375 / CI 7.715 / 회귀 12.190~12.711). CI 값을 **아티팩트의 `Temporary/LastTest.log`
에서 읽는다는 것도 주석에 남겼다** — 고치지 말라는 지시와 이유까지.

현재: `[perf-gate-ratio] 3072 ratio=6.894 limit=10.000`, 2/2 통과.

### 2-6. 재진입 (카드 재진입 절)

`threadCount` 는 **config 필드 = 인자**다. 이 경로에 `static`·`thread_local`·호출 간 기억이
**없다**(grep 결과 유일한 일치는 그 사실을 적은 주석 한 줄).
`ConcurrentCallsWithDifferentThreadCountsAreIndependent` 가 서로 다른 스레드 수의 동시 호출에서
**각자 단일 스레드 답**을 받는 것을 확인한다. **공유 상태가 필요해진 지점은 없었다** —
필요했다면 배선하지 않고 판정으로 올렸을 것이다(카드 지시).

### 2-7. 재실측

```
100% tests passed, 0 tests failed out of 622     (build/ci-preprocess)   ← 618 + 신규 4
100% tests passed, 0 tests failed out of  69     (build/ci-common)
dumpbin /exports xpe_preprocess.dll  →  45 (preprocess_api.h 의 XPE_API 45개와 일치)
```
빌드 exit 0(양쪽), `grep -ci warning` = **0**(양쪽). `Test-TrackedTextFiles.ps1` 통과.

---

## §3. Baseline-attribution — 무엇에 대고 쟀나

- **비트 파리티**: 같은 프로세스에서 1스레드 결과를 먼저 만들고 N스레드 결과와 비교. 두 값 모두
  이번 실행의 것이며 기록된 수치를 옮겨 쓰지 않았다
- **스레드 확장표**: 이번 턴 `--shipped-threads` 실행값(σ best of 5, frame best of 3)
- **A-58 대조**: A-58 로그 `a58-sigma-threads.log` 의 값. 보고서 본문에서 옮기지 않았다
- **CI 비율 7.715**: 리더가 CI 런 35042910764 아티팩트에서 읽어 전달한 값. 내가 재지 않았다
- **내보내기 45**: `dumpbin /exports` 실행 결과와 헤더의 `XPE_API` 개수를 각각 세어 대조
- **테스트 수**: 이번 트리 ctest 전체. 618 → 622 증가분이 신규 4건

---

## §4. Gaps — 관측하지 않은 것

1. **공개 C ABI 로는 스레드 수를 지정할 수 없다.** `xpe_defect_detect_runtime` 은 config 인자가
   없고, 추가하면 내보내기가 바뀐다 — 이 카드는 그 판정을 받지 않았다(§5-1)
2. **비트 파리티를 3072² 에서 확인하지 않았다.** 최대 640×480 이다 — 큰 프레임에서만 나타나는
   분할 오류(예: 32비트 인덱스 오버플로)는 걸리지 않는다
3. **스레드 살균 도구(TSan 등)를 돌리지 않았다.** 경쟁을 잡은 근거는 **결과 비교**이지
   도구 검출이 아니다. 아주 드물게만 나타나는 경쟁은 8회 반복으로 못 볼 수 있다
4. **16스레드 포화의 원인을 규명하지 않았다.** 테이블 할당(`T × 256 KB`)과 스레드 스폰으로
   **추정**했을 뿐 계측하지 않았다. 20에서 악화되는 것도 마찬가지다
5. **호출마다 테이블을 새로 할당하는 비용을 따로 재지 않았다.** A-58 프로브와의 35% 차이를
   설명하는 가설이지 측정이 아니다
6. **`DetectFrame` 은 맵을 0으로 채우지 않는다**(호출자 책임). 공개 진입점은 `memset` 을 하지만
   `DetectFrame` 은 하지 않는다 — 주석에 적었으나 **오용 시 조용히 틀린다**
7. E/P 코어 배치 미고정, 합성 프레임, 이 기계 한 대

---

## §5. Residual-risk — 남는 위험

1. **가장 큰 위험은 Gap 1 이다.** "호출자가 스레드 수를 지정한다" 가 **모듈 내부에서만** 참이다.
   실제 파이프라인이 DLL 경계 너머에 있다면 이 기능에 손이 닿지 않는다. **ABI 를 열 것인지,
   내부 호출자만으로 충분한지는 판정 사항**이라 배선하지 않고 올린다
2. **스레드 확장이 A-58 예측보다 나쁘다**(86.5 → 102.3 ms, 18%). 스레드 정책 결정이 A-58
   숫자를 근거로 내려졌다면 그 근거가 약해진다 — 다만 결론("알고리즘 몫이 남는다")은 더 강해진다
3. **비트 동일성 논증은 "float 를 스레드 간에 더하지 않는다" 에 의존한다.** 앞으로 누구든
   σ 계산에 부동소수 누산을 넣으면 그 전제가 조용히 깨지고, **파리티 테스트가 그때 잡는다** —
   잡히도록 테스트를 남겨 둔 것이 이 카드의 실질 산출이다
4. **반증 1이 발동하지 않은 이유(멱등)는 지금 설계에 한해 참이다.** 맵 쓰기가 누적(`|=`, `+=`)
   으로 바뀌면 중복 처리가 곧바로 손상이 되고, 그때는 반증 1이 유효한 시험이 된다
5. 20배 초과는 그대로다 — 이 카드는 성능 목표를 좁히지 않았고, 단일 스레드 기본값에서
   **1 ms 도 바뀌지 않았다**

---

Refs #144 #143
