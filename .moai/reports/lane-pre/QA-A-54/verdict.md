# QA-A-54 — 검출 루프 3072² 실측과 1위 항목 한 건 (#144)

수정 파일
- `modules/preprocess/include/runtime_detection.h` (8값 중앙값 고속 경로)
- `modules/preprocess/tests/test_runtime_detection_median8_parity.cpp` (신규, 동일성 단언)
- `modules/preprocess/tools/xpe_detect_experiment.cpp` (`--decompose`)
- `modules/preprocess/CMakeLists.txt` (테스트 등록)

**알고리즘·규칙·임계는 한 줄도 바꾸지 않았다.** A-42~A-50 표의 유효성은 §2-4 에서 바이트 단위로 확인했다.

---

## §1. Claim — 주장

| # | 주장 |
|---|---|
| C1 | 3072² 를 **직접 측정**했다. 외삽이 아니다. 전체 1341.1 ms — 전역 σ 306.2 / memset 0.4 / 픽셀 루프 1034.5 |
| C2 | 합계 대조는 **구성상 정확**하도록 고쳤다. 첫 측정에서 합이 전체의 122%가 나왔고, 원인은 "복제 루프를 실측 루프로 착각한 것"이었다 |
| C3 | 1위 항목은 **픽셀당 두 번의 중앙값 선택** — 복제 루프의 **91.7%** |
| C4 | 8값 전용 정렬 네트워크로 교체. 3072² **1341.1 → 836.2 ms (1.60배)**, 픽셀 루프 **1034.5 → 466.0 ms (2.22배)**. SPEC 초과 **38.3배 → 23.9배** |
| C5 | 동작 무변화: A-50 의 `--regress` 표(7프레임 × 2설정 × TPR@5/6/8/10σ·FN·cleanFP·FPR·1%·enrich)가 **바이트 단위로 동일** |
| C6 | 픽셀 단위 동일성 단언 4건 추가. 단, **부호 있는 0 하나의 예외**를 측정해 숨기지 않고 명시적으로 고정했다 |
| C7 | 반증 2건. 첫 번째는 **발동하지 않았고 그 자체가 발견**(중복 비교-교환), 두 번째는 3/4 테스트를 실패시켰다 |
| C8 | 재측정 **609/609**(신규 4건 포함), 69/69, 경고 0 |

---

## §2. Evidence — 증거

### 2-1. 3072² 분해 (카드 1항) — 최적화 **전**

명령: `xpe_detect_experiment --decompose` (`a54-decompose-before.log`)

```
[decompose] 3072x3072  (9437184 pixels), best of 3 per row
  TOTAL xpe_defect_detect_runtime  [measured]    1341.1 ms   100.0%
    global sigma (Bm, shipped)     [measured]     306.2 ms    22.8%
    memset of the output map       [measured]       0.4 ms     0.0%
    per-pixel loop      [total - the two above]    1034.5 ms    77.1%
  -> the three rows above sum to the total by construction; nothing is unaccounted.

  replica loop, this TU     1283.1 ms  = 1.24x the in-situ loop
  (stage shares below are of the REPLICA, not of the total)
    gather only                                 99.1 ms     7.7%
    median  (delta)                            754.7 ms    58.8%
    copy + MAD  (delta)                        422.0 ms    32.9%
    threshold + map write  (delta)               7.4 ms     0.6%
      of which: the two selections            1176.7 ms    91.7%

  SPEC budget 35 ms -> over by 38.3x
```

**합계 대조에서 실제로 문제가 나왔고, 숫자가 아니라 방법을 고쳤다.** 첫 판에서는 복제 루프를
그대로 "픽셀 루프"로 적었고, 합이 전체의 **122.4%**(초과 −318.8 ms)가 됐다. 못 잰 구간이
아니라 **두 번 센 구간**이었다 — 복제 루프는 이 TU 에서 컴파일되므로 DLL 안의 루프와 인라이닝이
다르고, 3072² 에서 **1.24배 느리다**. 고친 형태는 위와 같다:

- 직접 잰 것은 **전체·전역 σ·memset** 셋뿐이고, 픽셀 루프는 **전체에서 그 둘을 뺀 값**이다
  → 합계는 구성상 정확하고 미계상은 0이다
- 복제 루프는 **단계 분해에만** 쓰고, 단계 비율은 전체가 아니라 **복제 자신**에 대해 적는다
- 복제/실측 배율(1.24배)을 같은 줄에 찍어 그 차이를 숨기지 않는다

1024² 에서는 복제/실측이 0.99배라 이 문제가 보이지 않았다. **3072² 를 직접 재지 않았다면
방법의 결함도 드러나지 않았을 것이다.**

### 2-2. 1위 항목과 그 처방

두 번의 중앙값 선택이 복제 루프의 **91.7%**다(gather 7.7%, 임계·기록 0.6%). `ComputeMedian` 은
`std::nth_element` + `std::max_element` 두 패스이고, MSVC 는 이 크기에서 삽입 정렬로 떨어진다.
`ComputeMAD` 가 내부에서 같은 함수를 다시 부르므로 픽셀당 두 번이다.

처방은 **8값 전용 Batcher odd-even 정렬 네트워크**(19 compare-exchange, 데이터 의존 분기 없음,
전부 레지스터). 3×3 창의 내부 화소는 이웃이 정확히 8개라 프레임 대부분이 이 경로를 탄다.
경계 화소(3·5개)는 기존 경로 그대로다.

```cpp
inline float ComputeMedian(std::vector<float>& values) {
    if (values.size() == 8u) return MedianOfEight(values.data());
    return ComputeMedianGeneric(values);   // 기존 본문, 이름만 바뀜
}
```

**근사가 아니다.** 네트워크는 8값을 완전히 정렬하므로 `v[3]`·`v[4]` 는 기존 경로가 고르는 것과
**같은 두 순서통계량**이고(기존: `values[mid]` 와 하위 절반의 최댓값), 반환식도 같은 합에 같은
상수다. 규칙·창 크기·임계·σ 바닥은 건드리지 않았다.

### 2-3. 최적화 **후**

`a54-decompose-after.log`:

| | 1024² 전 | 1024² 후 | 3072² 전 | 3072² 후 |
|---|---|---|---|---|
| TOTAL | 143.8 ms | **88.8 ms** | 1341.1 ms | **836.2 ms** |
| 전역 σ (Bm) | 29.9 | 29.6 | 306.2 | 369.8 |
| 픽셀 루프(실측) | 113.8 | **59.2** | 1034.5 | **466.0** |
| SPEC 초과 | 4.1배 | **2.5배** | 38.3배 | **23.9배** |

픽셀 루프 **2.22배**(3072²), 전체 **1.60배**. 전역 σ 는 손대지 않았으므로 변하지 않아야 하고,
1024² 에서 29.9 → 29.6 ms 로 실제로 변하지 않았다. 3072² 의 306.2 → 369.8 ms 는 **측정 산포**로
본다 — 같은 코드이고 best-of-3 이며, 이 값의 변동 자체가 §4 의 미검증 항목이다.

**전역 σ 가 이제 1위다**(3072² 전체의 44.2%). 다음 카드의 대상은 거기다.

**SIMD 가 유효할 지점**(카드 3항, 한 줄): `ComputeGlobalSigma` 의 전프레임 차분 두 패스 —
940만 원소를 순차로 읽어 빼고 절댓값을 취하는 구간은 폭 8 AVX2 로 그대로 벡터화되지만,
픽셀당 8값 네트워크는 이미 레지스터 안에 있어 이득이 작다.

### 2-4. 동작 무변화 — 자릿수가 아니라 바이트 (카드 4항)

```
diff .moai/reports/lane-pre/QA-A-50/a50-regress-bm.log \
     .moai/reports/lane-pre/QA-A-54/a54-regress-after.log
→ (차이 없음)  IDENTICAL to QA-A-50 baseline (byte for byte)
```

A-50 의 `--regress` 출력 전체 — 7프레임 × 2설정 × {σ_g, TPR@5σ, TPR@6σ, TPR@8σ, TPR@10σ,
FN@10σ, cleanFP, cleanFPR, 1% 상한, enrichment} — 가 **한 바이트도 다르지 않다.**
TPR 0.554631, FPR 1.030e-04 같은 값이 소수점 여섯 자리까지 그대로다. A-42~A-50 의 모든 표가
그대로 유효하다는 뜻이다.

### 2-5. 픽셀 단위 동일성 단언 (카드 2항)

`test_runtime_detection_median8_parity.cpp`, `BufferReuseParityTest` 형식:

| 테스트 | 내용 |
|---|---|
| `FastPathMatchesGenericBitForBit` | 무작위 8값 20만 건(3분의 1은 동률이 잦은 소정수) + 극단 7케이스, **비트 단위** 비교 |
| `ZeroSignDifferenceCannotChangeADecision` | 아래 예외를 고정 |
| `MadMatchesGenericBitForBit` | MAD 5만 건 비트 단위 |
| `DetectionMapIsIdenticalPixelForPixel` | 256² 프레임, 실제 이상치 포함, 출고 경로 대 강제 generic 경로의 맵을 **화소마다** 비교 |

`EXPECT_FLOAT_EQ` 는 4 ulp 를 허용하므로 이 주장에는 틀린 단언이다. 1 ulp 차이도 경계에서
Hampel 판정을 뒤집을 수 있으므로 **raw 비트**를 비교한다.

**측정된 예외 1건 — 숨기지 않고 고정했다.** 처음 작성한 테스트가 실패했고, 원인은
`{-0.0, +0.0, ...}` 입력이었다. 두 순서통계량이 **부호가 반대인 0**이면 `-0.0 < 0.0` 이 거짓이라
두 경로가 서로 다른 쪽을 고를 수 있고, 합이 한쪽은 `-0.0`, 다른 쪽은 `+0.0` 이 된다
(측정: fast `0x00000000`, generic `0x80000000`, `equalValue=1`).

**수치는 같고 부호 비트만 다르며, 판정을 바꿀 수 없다** — 중앙값은 `|중심 − 중앙값|` 과
`|x − 중앙값|` 로만 규칙에 들어가고 `std::abs` 가 두 0을 모두 `+0.0` 으로 만든다.
전용 테스트가 그 등식을 직접 단언하고, 화소 단위 테스트가 end-to-end 로 보인다.
단언은 **"비트가 같거나, 둘 다 0"** 으로 좁혔다 — 0이 아닌 값의 1 ulp 차이는 여전히 실패한다.
허용오차로 풀지 않았다.

NaN 은 범위 밖이다. NaN 은 `<` 를 비일관으로 만들어 `std::nth_element` 의 전제(strict weak
ordering)를 깨므로, **기존 경로가 정의되지 않는다** — 같아질 "옛 동작"이 없다.

### 2-6. 반증 (카드 5항)

**반증 1 — 발동하지 않았고, 그것이 발견이다.**
네트워크 마지막 층의 `CE(a3,a4)` 를 제거했다.

빌드 결과 1차: `BUILD_EXIT=-1`, 로그가 `[11/12]` 에서 끊겼다 — **12단계(테스트 링크)가 끝나지
않았다.** 그 상태로 돌린 테스트는 통과했지만 **낡은 바이너리**였다. 규약대로 빌드 결과를 읽어
잡았고, 다시 빌드했다:
```
[1/1] Linking CXX executable bin\xpe_preprocess_tests.exe
BUILD_EXIT=0
→ 4 tests PASSED
```
**새 바이너리로도 통과한다.** 이유는 명확하다 — 중앙값은 `(a3 + a4) * 0.5f` 이고 **덧셈은
교환법칙이 성립**하므로, a3 와 a4 의 순서는 결과에 영향이 없다. 즉 19개 중 **1개는 이 용도에
대해 중복**이다. 그대로 두었다: 주석이 "완전 정렬"을 주장하고 있고 그 성질을 유지하는 편이
읽기 쉽다. 18-CE 로 줄이는 것은 별도 결정이다.

**반증 2 — 발동한다.** 실제로 기여하는 `CE(a2,a4)` 를 제거했다.
```
[6/6] Linking CXX executable bin\xpe_preprocess_tests.exe
BUILD_EXIT=0
[  FAILED  ] Median8ParityTest.FastPathMatchesGenericBitForBit
[  FAILED  ] Median8ParityTest.MadMatchesGenericBitForBit
[  FAILED  ] Median8ParityTest.DetectionMapIsIdenticalPixelForPixel
 3 FAILED TESTS
```
화소 단위 맵 비교까지 실패했다 — 단언이 실제로 깨진 네트워크를 잡는다.

**복원 확인**: `a54-restore-build.log` exit 0, CE 19개 유지(`grep -o` 로 확인), 파리티 4/4 통과.

### 2-7. 재측정

```
100% tests passed, 0 tests failed out of 609     (build/ci-preprocess)   ← 605 + 신규 4
100% tests passed, 0 tests failed out of  69     (build/ci-common)
```
빌드 exit 0, `grep -ci warning` = **0**. `Test-TrackedTextFiles.ps1` 통과.

---

## §3. Baseline-attribution — 무엇에 대고 쟀나

- **시간**: 같은 실행·같은 기계·best-of-3. 전후 비교는 **같은 바이너리 형상의 같은 명령**
  (`--decompose`)을 최적화 전후로 돌린 것이며, A-43 시절의 1,191 ms 를 옮겨 적지 않았다
  (이번 트리의 전 측정값은 1341.1 ms 다 — 기계 상태가 다르므로 그 숫자로 비교한다)
- **동작 무변화**: A-50 이 남긴 `a50-regress-bm.log` 파일 자체와 `diff`. 요약이 아니라 파일 비교
- **테스트 수**: 이번 트리에서 ctest 전체 실행. 605 → 609 의 증가분은 신규 4건
- **예외 사례**: `{-0.0,+0.0,...}` 의 비트값을 별도 프로브로 직접 출력해 확인(추론 아님)

---

## §4. Gaps — 관측하지 않은 것

1. **전역 σ 3072² 값이 전후로 306.2 → 369.8 ms 로 흔들렸다.** 코드가 같으므로 산포로 보지만,
   **반복 측정으로 확인하지 않았다.** 20% 산포는 이 보고서의 다른 비교에도 그대로 얹힌다
2. **복제 루프가 실측 루프보다 1.24배 느린 이유를 규명하지 않았다.** 인라이닝 차이로 추정할
   뿐, 디스어셈블리를 보지 않았다. 단계 비율은 그래서 **복제의 비율**이지 실측 루프의 비율이 아니다
3. **경계 화소는 여전히 기존 경로다.** 3072² 에서 경계는 약 0.13%라 무시할 만하지만, 측정하지 않았다
4. **`ComputeMedianGeneric` 의 성능을 따로 재지 않았다.** 고속 경로가 얼마나 빠른지는 루프
   전체의 차이로만 알고, 함수 단위 마이크로벤치는 없다
5. **다른 컴파일러·다른 CPU 에서 이득이 같은지 모른다.** 정렬 네트워크의 이점은 MSVC 가
   `vminss/vmaxss` 로 낮추는 데 의존하고, 이 워크트리의 x64 RelWithDebInfo `/arch:AVX2` 한 조합만 쟀다
6. **19개 중 1개가 중복**임을 알았지만 제거하지 않았고, 나머지 18개가 전부 필요한지도 확인하지
   않았다(반증 2건만 해봤다)
7. **NaN 입력에서의 동작은 두 경로 모두 정의되지 않는다.** 실프레임이 NaN 을 담을 수 있는지
   확인하지 않았다
8. SIMD 는 카드 지시대로 하지 않았다. 기본값·판정 기준 변경 없음

---

## §5. Residual-risk — 남는 위험

1. **"동작이 같다"의 근거는 두 층뿐이다** — 무작위 25만 건의 비트 비교와, A-50 표의 바이트 동일성.
   둘 다 강하지만 **입력 분포에 의존**한다. 실프레임의 값 분포(정수 격자 위의 uint16)는
   테스트가 쓴 연속 분포와 다르고, 동률이 훨씬 잦다. 동률을 늘린 세 번째 무작위 갈래를 넣어
   완화했지만, 실프레임 자체로는 확인하지 못했다(#151 대기)
2. **부호 있는 0 예외가 다른 경로로 새어 나갈 수 있다.** 지금은 `abs` 가 삼킨다는 것을 보였지만,
   중앙값을 **다른 용도로** 쓰는 코드가 생기면 그 전제가 깨진다. 전제를 주석과 테스트에 적어 두었다
3. **전역 σ 가 1위가 되면서 #144 의 성격이 바뀌었다.** 픽셀 루프를 더 깎아도 전체는 44% 밖에
   못 줄인다. 35 ms 까지는 **한 항목의 최적화로 도달할 수 없고**, 알고리즘 또는 SIMD 결정이 필요하다
4. **23.9배는 여전히 예산 밖이다.** 이 카드는 격차를 38.3배에서 줄였을 뿐 닫지 않았다
5. 측정은 이 기계 한 대의 값이다. CI 기계에서의 절대값은 다를 수 있고, **비율만 이식 가능**하다고 본다

---

Refs #144 #143
