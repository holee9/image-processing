# `#173` 전제 훑기 — **이슈 본문이 낡았다. 2-lane 은 이미 구현돼 있다**

> 카드 착수 전 전제 확인. **구현하지 않았다** — 이미 있는 것을 다시 만들지 않기 위한
> 측정이다.

## 1. 이슈가 말하는 전제

> `54a3ae7`(2026-05-09)이 뷰포트를 `LaneAImage`/`LaneBImage` 에 바인딩했지만
> **그 파이프라인은 만들어지지 않았고** …

**지금은 참이 아니다.** 파이프라인은 `MainWindowViewModel.RenderLanes` (:1860) 에 있고,
호출부는 :1221 이다.

## 2. 이슈가 "시작 전에 정의해야 할 것" 으로 든 셋 — 전부 있다

### (1) 각 lane 의 알고리즘 선택이 실제로 무엇을 바꾸는가

`AlgorithmPreset` 이 정의한다 — `No correction` / `Grid suppression` / `Virtual grid`
(+ `Preprocess + virtual grid`), 각각 `PreprocessInChain` 과 GSVG 모드로 내려간다.

```
MainWindowViewModel.cs:740  LaneAAlgorithm  → AlgorithmPreset.For(value).ApplyTo(Settings)
MainWindowViewModel.cs:753  LaneBAlgorithm
RenderLanes:1878            var candidate = inputs.Snapshot();
                            AlgorithmPreset.For(inputs.LaneBAlgorithm).ApplyTo(candidate);
```

후보 lane 은 **자기 설정 사본**으로 그려진다 — VOI 폭(`LaneBVoiWindowWidth`)과
`vg_denoise_k`(`LaneBGsvgDenoiseK`)도 lane 단위 재정의가 있다.

### (2) `#171` 의 통제를 lane 단위로

`LaneBIsStale` (:1922) 이 있고 화면에 붙어 있다:

```
AlgorithmBar.xaml:56  <TextBlock AutomationProperties.AutomationId="LaneBStaleMark" Text="오래됨"
AlgorithmBar.xaml:59        Visibility="{Binding LaneBIsStale, ...}"
AnalysisPanel.xaml:377  LaneBOverridesUnappliedMark  Text="미적용"
```

### (3) 뷰포트가 **실제로 받은 영상**을 보는 시험

`WorkbenchObservation.ReadLane` 이 lane 픽셀을 읽어 크기·평균·해시를 낸다.
`#172` 가 4개월 숨은 이유가 이 시험의 부재였고, 지금은 있다.

## 3. 측정 — 오늘, 두 백엔드에서

```
Mock   : 통과 3, 건너뜀 2 (L04·L05 는 네이티브 전용), 실패 0
Native : 통과 5, 건너뜀 0, 실패 0
```

| 시나리오 | 무엇을 단언하는가 | Native |
|---|---|---|
| L-01 | 두 lane 이 같은 원본을 그린다(해시 동일, 평균 > 0) | 통과 |
| L-02 | 후보 설정이 **후보만** 움직인다 | 통과 |
| L-03 | **후보만** 오래됨이 된다 | 통과 |
| L-04 | 후보 알고리즘이 **후보만** 움직인다 | 통과 |
| L-05 | 후보 `vg_denoise_k` 가 **후보만** 움직인다 | 통과 |

L-02 의 화소 증거:

```
L-02 A: 9e58cd6029598dfa -> 9e58cd6029598dfa (mean 127.008 -> 127.008)
L-02 B: 9e58cd6029598dfa -> ba2605349df50f19 (mean 127.008 -> 119.16)
```

**기준 lane 은 해시가 바이트 동일, 후보 lane 만 바뀌었다.** "두 lane 이 같은 영상을
그리게 되는" 실패 모드(이슈가 우려한 것)가 실제로 막혀 있다.

## 4. 설계와 남은 차이 — **하나 있고, 의도적이며 근거가 적혀 있다**

설계는 *"각각 lane 별 디스플레이 파이프라인으로"* 라고 한다. 실제로는 **기준 lane 을
따로 그리지 않는다** — 메인 뷰포트가 방금 그린 결과를 그대로 쓴다:

```
RenderLanes:1867   LaneAImage = reference;
```

주석이 이유를 적는다: 같은 설정으로 두 번 돌리는 것은 **이미 손에 있는 결과의 재계산**
이고, 두 실행은 서로 동의할 수밖에 없으니 **같은 주장도 아니다.** 그리고 호출 횟수를
세는 시험이 있다 — *"두 번째·세 번째 호출을 넣자 W-23·W-26 이 깨졌다"* (측정된 값).

후보 lane 도 `differs` 일 때만 그린다(:1873). 차이가 없으면 두 lane 은 같은 결과를
공유한다 — 이슈가 우려한 "같은 영상" 과 구별해야 한다: **설정이 같으면 같은 것이
정답**이고, L-01 이 그것을 대조군으로 쓴다.

**이 차이가 남길 것인지 메꿀 것인지는 설계 결정이고 내 몫이 아니다.**

## 5. 미검증 (Gaps)

- **`docs/design/README.md` 원문과 한 줄씩 대조하지 않았다.** 위 §4 의 인용은 이슈
  본문이 옮겨 적은 문장이다. 설계 문서는 lead 소유라 읽기만 가능하고, 이 보고는
  **이슈 본문 대비**로 한 것이다.
- **lane 정체 라벨("Reference"/"Candidate")이 뷰포트에 붙어 있는지 확인하지 않았다.**
  확인한 것은 알고리즘 선택기·오래됨·미적용 표시이고, lane 제목 텍스트는 보지 않았다.
- **3072² 손목 프레임에서 재지 않았다.** L-05 만 큰 프레임 픽스처를 쓴다.
- 이 훑기는 **현재 상태**만 본다. `#173` 이 열린 시점과 지금 사이에 어느 카드가
  무엇을 넣었는지 이력으로 추적하지 않았다.

🗿 MoAI
