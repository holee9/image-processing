# QA-A-43 검증 보고서 — 전역 σ 하한 도입으로 1% 상한 복구

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 커밋: `bbddf2d` (α 하한 + 테스트 갱신 + CI 공백 수정)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-43/`
- Refs #143 #144

> **결과 한 줄: clean 입력 오검출이 1.72% → 0.0102% 로 내려가 1% 상한이 복구됐다.**
> SPEC 알고리즘 절(3×3 중심 제외)은 그대로 유지된다. 대가는 TPR@5σ 9% 손실
> (0.6108 → 0.5536)과 시간 +19%.
>
> **남는 것:** FPR 요구(1e-5)는 여전히 미달이다 — 1.02e-4 로 한 자릿수 위.
> λ=5 vs TPR@5σ≥0.999 모순도 이 카드가 손대지 않았고 사용자 결정 대기다.

---

## 1. 3열 비교표 (A-42 §2-6 과 같은 시드·형식)

조건: 1024×1024, 961 주입, σ=10 ADU, 시드 `20260911`, RelWithDebInfo.
로그: `a43-rates-raw.log`, `a43-time.log`, A-42 의 `a42-rates-after.log`.

| 지표 | **A-41 기준**<br>(5×5, 중심 포함) | **A-42**<br>(3×3 SPEC 규칙) | **A-43**<br>(+ 전역 σ 하한 α=0.8) |
|---|---|---|---|
| TPR @ 5σ | 0.500520 | 0.610822 | **0.553590** |
| TPR @ 6σ | 0.711759 | 0.761707 | 0.753382 |
| TPR @ 8σ | 0.955255 | 0.927159 | **0.927159** (불변) |
| TPR @ 10σ | 0.998959 | 0.986472 | **0.986472** (불변) |
| clean 오검출 픽셀 | 496 | 18,011 | **107** |
| clean FPR | 4.730e-4 | 1.718e-2 | **1.020e-4** |
| **REQ 1% 상한 준수** | **준수** (0.047%) | **위반** (1.72%) | **준수** (0.0102%) |
| REQ FPR < 1e-5 | 미달 (47배) | 미달 (1,718배) | **미달 (10배)** |
| 1024² 시간 (best of 3) | — (단발 1340 ms) | 111.1 ms | **130.7 ms** |
| 3072² 시간 (best of 3) | ~12,100 ms (환산) | 1,004.3 ms | **1,191.3 ms** |
| SPEC 성능 35 ms 대비 | ~345배 | 28.7배 | **34.0배** |

읽는 법 세 가지:

1. **상한 칸이 이 카드의 목적이다.** 위반 → 준수로 돌아왔고, 여유도 크다 —
   상한(10,485 픽셀)의 **1/98** 이다.
2. **8σ 이상에서 TPR 손실이 0 이다.** 하한을 적용해도 그만큼 넘는 신호는
   임계를 넘는다. 손실은 임계 근처(5~6σ)에만 발생한다.
3. **A-41 열의 1024² 시간은 단발 측정(1340 ms)이라 best-of-3 인 나머지 두 열과
   직접 비교하면 안 된다.** 3072² 환산치도 A-41 시점의 추정이다. 자릿수
   비교로만 읽어야 한다.

### 후보 하네스 재실행 (`a43-sweep.log`, 시드 3개 평균)

하네스의 `(b) 3x3 floor a=0.8` 행이 이번에 출하 경로가 된 규칙이다:
TPR@5σ 0.580992, FPR 1.062e-4, 160.8 ms. 출하 경로의 단일 시드 측정
(0.553590 / 1.020e-4)과 시드가 달라 소폭 차이가 나며, 두 값 모두 같은
결론을 가리킨다. 하네스의 `baseline 3x3` 행은 하한이 0 인 config 를 쓰므로
A-42 상태(1.719e-2)를 그대로 보여준다 — 비교 기준으로 남겨 둔 것이다.

---

## 2. 증거 (Evidence)

### 2-1. RED → GREEN

RED (`a43-red-ctest.log`): 하한 적용 직후 **6건 FAIL / 605**

```
487 - RuntimeDetectionFunctionalTest.KnownDivergence_GaussianNoiseExceedsTheOnePercentCeiling (Failed)
599 - RuntimeDetectionRatesTest.KnownDivergence_FprOnCleanFramesExceedsTheRequirement (Failed)
602 - RuntimeDetectionRatesTest.KnownDivergence_CleanFrameBreachesTheOnePercentCeiling (Failed)
603 - BufferReuseParityTest.IdenticalOnAGaussianFrame (Failed)
604 - BufferReuseParityTest.IdenticalWithInjectedTransients (Failed)
606 - BufferReuseParityTest.IdenticalOnANarrowFrame (Failed)
```

성격이 두 가지로 갈린다:

- **앞 3건 = 뒤집힌 KnownDivergence.** A-42 가 "상한을 깬다" 로 고정해 둔
  케이스들이 이제 상한을 지키므로 실패한다. **의도된 실패**다.
- **뒤 3건 = 기준 config 누락.** 진입점만 하한을 채우고, 비교 기준으로 쓰는
  `DetectDefectivePixel` 직접 호출은 기본 config(하한 0)를 쓰고 있었다.
  버퍼 재사용과 무관한 사유로 갈라진 것이라 기준 쪽에 같은 하한을 채웠다.

GREEN (`a43-green-ctest.log`): `100% tests passed, 0 tests failed out of 605`,
전체 55.62초. ci-common (`a43-common.log`): `69/69`. 빌드 경고 0건.

### 2-2. 갱신한 케이스 3건 — "틀렸던 게 아니라 대체됐다"

| 케이스 | A-42 상태 | A-43 상태 | 주석에 남긴 것 |
|---|---|---|---|
| `KnownDivergence_CleanFrameBreachesTheOnePercentCeiling` → `CleanFrameStaysUnderTheOnePercentCeiling` | 18,011 (1.72%) 위반 고정 | 107 (0.0102%) 준수 | 496 → 18,011 → 107 세 측정 이력 |
| `KnownDivergence_GaussianNoiseExceedsTheOnePercentCeiling` → `GaussianNoiseKeepsFalsePositivesLow` | 90/4096 (2.2%) 위반 고정 | 0/4096 | 이름이 두 번 바뀐 이유와 각각이 대체(supersede)였다는 점 |
| `BufferReuseParityTest` 3건 | — | 기준 config 에 하한 주입 | 왜 기준이 진입점과 같은 config 를 써야 하는지 |

A-42 의 기대가 **작성 시점에는 옳았다.** 그 커밋의 트리를 정확히 서술했고,
이번 커밋이 트리를 바꿨기 때문에 교체된 것이다. 주석에 그렇게 적었다.

### 2-3. 반증 실험 (카드 4항)

**반증 1 — 하한 제거(α=0.0)** (`a43-probe-floor-off.log`):

```
99% tests passed, 3 tests failed out of 605
  487 - RuntimeDetectionFunctionalTest.GaussianNoiseKeepsFalsePositivesLow (Failed)
  599 - RuntimeDetectionRatesTest.KnownDivergence_FprOnCleanFramesExceedsTheRequirement (Failed)
  602 - RuntimeDetectionRatesTest.CleanFrameStaysUnderTheOnePercentCeiling (Failed)
```

**상한 관련 케이스만 실패한다.** 602·487 은 1% 상한 두 건이고, 599 는 FPR
브래킷의 상한(`< 0.001`)이 1.7e-2 에 깨진 것이다. 검출 규칙·경계·결정성·버퍼
재사용 등 나머지 602건은 하한과 무관하게 통과한다 — 하한이 **한 가지 성질만
건드린다**는 증거다.

**반증 2 — α=1.0** (`a43-probe-alpha10.log`): 실패 2건.

```
598 - KnownDivergence_TprAtFiveSigmaIsBelowTheRequirement (Failed)   TPR 0.391259 < 0.40 브래킷
599 - KnownDivergence_FprOnCleanFramesExceedsTheRequirement (Failed) FPR 9.54e-7 < 1e-5
```

두 번째가 중요하다: **α=1.0 이면 FPR 이 요구치(1e-5)를 실제로 만족한다**
(9.54e-7, clean FP 1픽셀). 즉 α 는 TPR 과 FPR-요구 사이의 교환 손잡이이고,
1.0 은 FPR 요구를 닫는다 — 대신 TPR@5σ 가 0.5536 → 0.3913 으로 29% 더 떨어진다.
리더가 0.8 을 고른 근거(A-42 표)는 유효하지만, **이 사실은 α 선택을 다시 볼
근거가 될 수 있어 §4 에 남긴다.**

### 2-4. 변경 규모 (`a43-diff.txt`)

```
 include/runtime_detection.h                       | 63 ++++++++++++++++--
 src/runtime_detection.cpp                         |  9 ++-
 tests/test_runtime_detection_buffer_reuse.cpp     | 13 +++-
 tests/test_runtime_detection_functional.cpp       | 29 +++++----
 tests/test_runtime_detection_rates.cpp            | 54 +++++++--------
 tools/xpe_detect_experiment.cpp                   |  2 +-   (CI 공백 1줄)
 6 files changed, 125 insertions(+), 45 deletions(-)
```

### 2-5. CI 게이트 수정 (동승)

`tools/ci/Test-TrackedTextFiles.ps1` 이 `xpe_detect_experiment.cpp:276` 의 줄 끝
공백으로 main CI 를 막고 있었다(A-41 이 넣은 파일, `modules/**` 는 레인 소유).
`git merge origin/main` 후 해당 줄만 정리했고 로컬에서 같은 스크립트로 확인했다:

```
Tracked text file validation passed.
```

---

## 3. Baseline 귀속

- **테스트 총수**: A-42 시점 605 → **605** (신규 0건, 이름 변경 3건).
- **빌드 프리셋**: `ci-preprocess` / `ci-common` 동일, `/WX` 유지, 경고 0.
- **반증 실험**: 같은 트리에서 상수 한 줄만 바꿔 재빌드하고 같은 명령으로
  ctest 를 돌렸다. 원복 후 GREEN 을 다시 측정했다.
- **α=0.8 의 출처**: 리더 판정이며, 그 판정의 근거는 A-42 §2-6 표(레인 실측)다.
  이 카드가 새로 고른 값이 아니다.
- **시간 측정**: `--time` 은 best-of-3 이므로 낙관적인 쪽이다. A-42 와 같은
  방식이라 두 열은 서로 비교 가능하다.

---

## 4. 미검증 (Gaps)

1. **α 를 0.8·1.0·0.0 세 점에서만 확인했다.** 0.9 나 0.6~0.7 은 재지 않았다.
   §2-3 이 보여주듯 0.8 → 1.0 사이에서 FPR 이 1.02e-4 → 9.54e-7 로 두 자릿수
   움직인다. **요구 1e-5 를 만족하면서 TPR 손실이 0.39 보다 작은 α 가 그 사이에
   있을 가능성이 높으나 탐색하지 않았다.**
2. **전역 σ 가 프레임 단위라는 가정을 시험하지 않았다.** 영역별 잡음이 크게
   다른 실제 프레임에서는 저잡음 영역의 하한이 과도해져 그 영역의 검출을
   죽인다. 합성 가우시안에서는 드러나지 않는 실패 모드다.
3. **전역 σ 계산이 프레임 사본을 뜬다.** 3072² 에서 36 MB 를 추가로 잡는다.
   메모리 압박 환경에서의 동작은 재지 않았다.
4. **3072² 에서 TPR 을 재지 않았다.** 시간과 flagged 비율(0.010%)만 쟀다.
5. **`globalSigmaFloor` 를 외부에서 설정하는 경로가 없다.** 진입점이 항상
   `RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR` 를 쓴다. 튜닝하려면 재컴파일이 필요하다.
6. **A-41/42 가 남긴 Gap 은 그대로다** — 클러스터 결함 미시험, 합성 잡음 한계,
   시드 3개, DLL/인라인 2배 시간 차 미해명.

---

## 5. 잔여 위험 (Residual risk)

1. **FPR 요구는 여전히 미달이다 (1.02e-4 vs 1e-5).** 상한은 복구했지만 요구는
   못 맞췄다. `KnownDivergence_FprOnCleanFramesExceedsTheRequirement` 가 그대로
   남아 있고, §2-3 의 α=1.0 결과가 그 격차를 닫을 수 있음을 보여준다 —
   **α 선택이 열려 있는 문제라는 뜻이다.**
2. **λ=5 vs TPR@5σ≥0.999 모순은 이 카드가 손대지 않았다.** A-43 이후에도
   TPR@5σ 는 0.5536 으로 요구의 55% 다. 사용자 결정 대기 항목이며 카드 지시대로
   고치려 하지 않았다.
3. **하한이 검출을 조용히 죽이는 방향으로 작동한다.** 잘못된 α 는 오검출이 아니라
   **미검출**을 만들고, 미검출은 로그에도 알림에도 남지 않는다. α 를 바꿀 때는
   FPR 뿐 아니라 TPR 을 반드시 함께 재야 한다 — 이 보고서의 3열 표가 그 형식이다.
4. **성능 목표에서 한 걸음 멀어졌다.** 28.7배 → 34.0배 초과. 전역 σ 2패스가
   +19% 다. 필요하면 서브샘플링(예: 16픽셀마다 1개)으로 대부분 회수할 수 있으나
   추정 정확도와의 교환이라 이 카드에서 시도하지 않았다.
5. **하한은 프레임마다 다시 계산된다.** 연속 촬영에서 프레임 간 잡음이 흔들리면
   임계도 함께 흔들린다. 프레임 간 임계 안정성은 측정하지 않았다.
