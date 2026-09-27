# GUI-C-117 — 잡음 억제는 잇고, 선명화는 뺐다 (#173, #193)

## 1. 주장

1. `LaneBSharpeningSigma` 와 그 입력 요소를 제거했다 — 연쇄에 이을 단계가 없다(#193).
2. `LaneBDenoiseStrength` 를 `LaneBGsvgDenoiseK` 로 바꾸고, 가상 그리드의 `vg_denoise_k` 를 lane B 에서 덮어쓰게 이었다.
3. 이 값이 효과를 갖지 못하는 프리셋에서는 입력이 비활성이고 "미적용" 표시가 뜬다.
4. L-05 가 양쪽을 단언하며, 반증 **두 방향** 모두 터진다.
5. **합성 픽스처로는 이 연결을 측정할 수 없다는 것을 측정했다.** 이것이 이 카드에서 가장 값진 사실이다.
6. `#182` 조사가 이 값을 "복사되어 닿는" 것으로 인식한다.
7. 저장된 설정에 옛 키가 있어도 나머지 설정이 살아서 읽힌다.

## 2. 증거

### (a) 구현 전 빨강 — 이유까지 맞다

```
RED_EXIT=1   통과: 4   실패: 1
L05  LaneBGsvgDenoiseKInput is not in the tree, so the Candidate has no denoise k of its own.
L-01·L-02·L-03·L-04  초록
```

### (b) 합성 픽스처가 이 축을 가린다 — 측정

구현을 마친 뒤에도 L-05 가 빨강이었다. 값은 닿았는데 픽셀이 안 움직였다:

```
L-05 same k: A=4af3a42b5ae9317d B=4af3a42b5ae9317d
L-05 A: 4af3a42b5ae9317d -> 4af3a42b5ae9317d
L-05 B: 4af3a42b5ae9317d -> 4af3a42b5ae9317d
```

원인은 모듈에 있다 — `virtual_grid.cpp:614-618` 의 소프트 임계값은

```cpp
const double thr = k * Mad(b) / 0.6745;
```

즉 **최미세 대역의 MAD 가 0 이면 어떤 k 에서도 thr 이 0** 이다. 두 픽스처를 같은 방법(수평 2차 차분의 MAD)으로 재었다:

```
synthetic_1024x1024.raw : MAD 0.0      ← 어떤 k 도 구분되지 않는다
wrist_lat_3072x3072.raw : MAD 6.0
```

**즉 합성 프레임에서는 연결이 끊겨 있어도 L-05 가 통과했을 것이다.** 고칠 것은 단언이 아니라 픽스처였고, L-05 를 `LargeFrameApplicationFixture`(손목 영상) 로 옮겼다.

### (c) 초록

```
L-05 same k: A=0fe341c9b0ebebe1 B=0fe341c9b0ebebe1
L-05 A: 0fe341c9b0ebebe1 -> 0fe341c9b0ebebe1
L-05 B: 0fe341c9b0ebebe1 -> 3cca12fd2b628f34
```

### (d) 반증 — 두 방향

**방향 ① 후보가 자기 k 를 안 쓰게 되돌림** (`candidate.GsvgDenoiseK = …` 제거)

```
RED_EXIT=1   통과: 4   실패: 1
L05  Setting the Candidate's de-noise k to 8.0 did not reach its drawn pixels: still 0fe341c9b0ebebe1.
L-01~L-04  초록 (같은 실행)
```

**방향 ② 두 lane 이 한 파이프라인** (`LaneAImage = LaneBImage`)

```
RED_EXIT=1
L05  The Reference moved with a de-noise k set for the Candidate
     (0fe341c9b0ebebe1 -> 3cca12fd2b628f34) — the two lanes run one pipeline.
```

### (e) 조건부 표시가 화면에서 어떻게 보이는가 — U-02

```
U02 no-correction: enabled=False text='2'
U02 mark name='미적용' offscreen=False
    help='Lane B 가 가상 그리드를 돌고 피라미드 단계가 0 보다 클 때만 vg_denoise_k 가 전달됩니다 (#173).'
U02 virtual-grid: enabled=True        (표시 없음)
```

U-02 는 지우지 않고 **세 요소 → 조건부 양방향**으로 바꿨다. 선명화 입력이 트리에 없다는 단언도 같은 케이스에 넣었다.

### (f) `#182` 조사 + 옛 키

```
INT_EXIT=0   통과: 247, 건너뜀 1, 실패 0   (RetiredSettingKeyTests 1건 포함)
```

- `Unconnected` 에서 두 항목을 빼고 `LaneBGsvgDenoiseK` 를 `ConnectedViaCopy` 에 넣었다 — 조사가 "`GsvgDenoiseK` 로 복사되어 처리가 읽는다" 로 인식한다. 도구를 느슨하게 만들지 않았다.
- 바인딩 수 37 → 36.
- `RetiredSettingKeyTests`: `laneBSharpeningSigma`·`laneBDenoiseStrength` 가 든 설정 파일을 읽어도 `voiWindowCenter=1234`·`laneBAlgorithm` 이 그대로 살아 있다. `AppSettingsService.Load` 가 모든 예외를 삼키고 기본값으로 돌아가므로, **터지면 앱이 죽는 게 아니라 설정 전체가 조용히 사라진다** — 그래서 대조군(살아 있는 키)을 같이 단언했다.

### (g) 전체

```
BUILD_EXIT=0
Mock  E2E 전체 : 통과 114, 건너뜀 21, 실패 0 (4m 49s)
Native (LaneDenoise + TwoLane + Unapplied) : 통과 10, 실패 0
IntegrationTests : 통과 247, 건너뜀 1, 실패 0
```

## 3. baseline 귀속

- 반증 주입 전 `MainWindowViewModel.cs` 사본을 보관하고 그 파일로 복원. 복원 뒤 `FALSIFICATION` 문자열 0건.
- 네이티브는 `build/ci-3f520f8/bin` 스테이징으로 실행. 모든 수치는 이 워크트리·이 실행.

## 4. 미검증 (Gaps)

- **Mock 건너뜀이 20 → 21 로 늘었다.** L-05 가 네이티브 전용이기 때문이며, Mock 에서는 이 연결을 아무것도 검증하지 않는다.
- 손목 프레임의 MAD 6 은 **수평 2차 차분**으로 잰 근사치다. 모듈이 실제로 쓰는 것은 가우시안 피라미드의 최미세 라플라시안 대역이고, 그 대역의 MAD 는 재지 않았다. 두 픽스처의 **순서**(0 대 0 아님)만 근거로 삼았다.
- `k=8.0` 이라는 값의 임상적 타당성은 판단하지 않았다. 픽셀이 움직이는지만 봤다.
- 피라미드 단계를 0 으로 두었을 때의 비활성 경로는 코드에는 있으나(`LaneBDenoiseKApplies`) E2E 로 재지 않았다 — 프리셋 축만 쟀다.
- 선명화를 뺀 것이 다른 화면·보고서 JSON 에 미치는 영향은 grep 범위(`gui/**`, `clients/**`) 안에서만 확인했다.

## 5. 잔여 위험

- `LaneBGsvgDenoiseK` 의 기본값(2.0)이 `GsvgDenoiseK` 의 기본값과 같아야 두 lane 이 기본 상태에서 일치한다. 한쪽 기본값만 바뀌면 **모든 적용이 파이프라인을 두 번** 돌게 된다(W-23·W-26 이 잡는다). 두 값이 같아야 한다는 사실은 주석에 적었지만 시험으로 고정하지는 않았다.
- 옛 키가 무시된다는 것은 곧 **저장돼 있던 0.42 가 조용히 2.0 이 된다**는 뜻이다. C-115 의 프리셋 개명과 달리 대응표를 두지 않았다 — 옛 값이 가리키던 대상(존재하지 않는 denoise)이 없어서 옮길 곳이 없다.

🗿 MoAI
