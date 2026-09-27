# QA-A-109 (#179) — 성능 게이트를 3회 측정 최소값 판정으로

## 1. 주장

1. `Frame3072SquaredWithinMachineRatio` 는 이제 **같은 프로세스 안에서 기준 커널과 검출기를 한 쌍으로 3회 재고, 세 비율의 최소값**으로 판정한다. 한계 0.85 와 기준 커널은 변경하지 않았다.
2. **세 값이 모두 로그에 남는다.** 라운드별 검출기 ms·기준 커널 ms·비율, 마지막에 최소값과 세 값의 나열, 그리고 러너 프로필.
3. **반증이 작동한다.** 검출기에 지연을 주입하면 세 라운드가 모두 한계를 넘어 빨강(`RUN_EXIT=1`), 되돌리면 초록.
4. 실행 시간은 900 ms → 3642 ms 로 약 4배다. 전처리 ctest 전체가 통과하며(`CTEST_EXIT=0`) 잡 전체 시간에서 차지하는 몫은 무시할 수준이다.
5. 함께 요청받은 헤더 `@param` 중복 2건을 별도 커밋으로 고쳤고, 리더가 넣은 검사기가 **0건**을 찍는다.

## 2. 증거

### 2.1 3회 판정 (정상 트리, `BUILD_EXIT=0`, `RUN_EXIT=0`)

```
[perf-gate-machine] cpu="12th Gen Intel(R) Core(TM) i7-12700" logical=20 avx2=1 bandwidth=22.8 GB/s
[perf-gate-machine] numerator=xpe_preprocess(/arch:AVX2) denominator=in-test kernel(default arch)
[perf-gate] round 1/3 ... min of 5: 66.9 ms  (samples 68.6 / 68.2 / 67.5 / 70.6 / 66.9)
[perf-gate] round 1/3 reference kernel: 102.6 ms
[perf-gate-ratio] 3072 round=1 ratio=0.652 limit=0.850
[perf-gate] round 2/3 ... min of 5: 65.7 ms  (samples 69.5 / 67.7 / 65.7 / 68.1 / 67.4)
[perf-gate] round 2/3 reference kernel: 100.5 ms
[perf-gate-ratio] 3072 round=2 ratio=0.653 limit=0.850
[perf-gate] round 3/3 ... min of 5: 67.3 ms  (samples 76.2 / 67.3 / 69.5 / 111.2 / 92.4)
[perf-gate] round 3/3 reference kernel: 100.1 ms
[perf-gate-ratio] 3072 round=3 ratio=0.673 limit=0.850
[perf-gate-ratio] 3072 ratio=0.652 limit=0.850  (minimum of 3 rounds: 0.652 / 0.653 / 0.673)
```

3라운드 표본에 111.2 ms 순간 튐이 실제로 들어왔는데, min-of-5 가 흡수해 비율은 0.673 에 머물렀다. 이 실행이 그 자체로 설계 의도의 한 예다.

두 번째 실행(반증 되돌린 뒤): `minimum of 3 rounds: 0.656 / 0.662 / 0.674`.

**매 라운드가 기준 커널을 다시 잰다.** 한 번 재서 재사용하면 뒤 라운드는 더 이상 존재하지 않는 기계 상태와 비교하게 되어, 비율이 "지금 이 기계 대비 검출기" 라는 뜻을 잃는다.

### 2.2 반증 — 검출기에 지연 주입

`runtime_detection.cpp` 의 `xpe_defect_detect_runtime` 안에 입력 프레임을 추가로 훑는 패스를 넣었다(빌드 옵션·시험 코드는 그대로, `BUILD_EXIT=0`, warning 0).

| 주입량 | 라운드 비율 | 최소값 | 판정 |
|---|---|---|---|
| 없음 | 0.652 / 0.653 / 0.673 | 0.652 | 초록 |
| 2패스 (약 +17%) | 0.778 / 0.761 / 0.879 | 0.761 | **초록** |
| 8패스 (약 +60%) | 1.041 / 1.030 / 1.169 | 1.030 | **빨강** (`RUN_EXIT=1`) |
| 되돌림 | 0.656 / 0.662 / 0.674 | 0.656 | 초록 |

2패스 행은 통과라서 더 값진 결과다 — 한계 0.85 는 가장 나쁜 정상 실행 위 30.6% 로 잡혀 있으므로, 17% 회귀가 통과하는 것은 설계대로다. 게이트가 잡도록 설계된 크기(가장 가까운 회귀 1.289)는 8패스 쪽이고, 그 경우 **세 라운드가 모두** 넘는다. 즉 최소값 판정이 회귀를 가리지 않는다.

주입 코드는 되돌렸고 `git diff` 가 비어 있음을 확인했다.

### 2.3 실행 시간

| | 이전 | 이후 |
|---|---|---|
| `Frame3072SquaredWithinMachineRatio` | 약 900 ms | 3642 ms |

기준 커널 3회(약 100 ms × (1 준비 + 5 측정) × 3)와 검출기 3회(약 67 ms × 6 × 3)가 더해진 값이다. 전처리 ctest 전체는 `CTEST_EXIT=0` 으로 통과했고, 늘어난 2.7초는 잡 전체(빌드 포함 수 분)에서 문제가 되지 않는다.

### 2.4 헤더 `@param` 중복 (리더 요청, 별도 커밋 `8da27b2`)

`preprocess_api.h` 의 `xpe_preprocess_pipeline` 주석에서 `@warning` 앞에 있던 `img`·`meta` 두 줄이 아래 목록과 중복이었다. 아래 목록(`calibPath`·`ghostHandle`·`configJsonOrNull` 과 함께 있는 쪽)을 남기고 앞의 두 줄을 지웠다.

```
$ python tools/docs/check_header_docs.py
-- 20 headers, 0 declarations skipped as unparseable, 0 findings
CHECK_EXIT=0
```

## 3. 기준 귀속
- 모든 값은 이 워크트리의 `build/ci-preprocess` RelWithDebInfo 빌드, i7-12700 에서 위 로그 원문으로 관측했다.
- 헤더 검사는 리더가 main `e10e0bd` 로 넣은 `tools/docs/check_header_docs.py` 를 병합한 뒤 그대로 실행했다.
- 커밋: `267add8`(게이트), `8da27b2`(헤더 주석). push 하지 않았다.

## 4. 미검증
- **CI 에서 3회 판정을 아직 돌려 보지 않았다.** 라운드 간 편차가 CI 러너에서 어떤 폭인지는 다음 실행 로그를 봐야 안다.
- 러너 프로필이 CI 에서 실제로 무엇을 찍는지 아직 모른다(카드 2항). 다음 실행 로그를 읽고 A-108 의 대역폭 가설을 판정하겠다.
- 반증은 한 기계에서만 했다. CI 러너에서 같은 주입이 같은 크기로 나타나는지는 재지 않았다.
- 기준 커널을 `/arch:AVX2` 로 맞췄을 때의 영향은 카드 지시대로 재지 않았다.

## 5. 잔여 위험
- **최소값 판정은 라운드에 국한된 변동만 걸러낸다.** 실패 실행 시도2 처럼 러너의 느린 상태가 잡 전체(약 11초) 동안 지속되면 세 라운드가 모두 느려 빨강이 된다. 주입 8패스의 비율(1.030–1.169)이 관측된 CI 실패(0.972·1.172)와 비슷한 범위라는 점이 그 한계를 그대로 보여 준다 — 이 게이트는 "잡 내내 지속되는 기계 열화" 와 "코드 회귀" 를 구분하지 못한다. 러너 프로필의 대역폭 값이 그 구분을 줄 수 있는 유일한 단서이고, 다음 CI 실행에서 확인된다.
- 리더 관측대로 `2d08cdc` 실행에서는 게이트가 통과했다. 간헐 실패이므로 3회 판정의 효과는 여러 실행을 모아야 판정된다.
