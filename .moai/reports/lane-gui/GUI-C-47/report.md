# GUI-C-47 — 선택된 비교 모드를 밖에서 볼 수 있게 (#149 G-6)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-47 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `6a05690`
- 커밋 1건: `a2d08a6` — 미푸시
- **합격 판정 통과: C-46 에서 아무것도 못 잡던 약화가 이번엔 W-12 4건을 전부 무너뜨린다.**
- **결과: Native E2E 0/27/0/27 (1 m 58 s) · Mock 0/26/1/27 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. 노출 — 고른 방법과 버린 방법

`MainWindow.xaml`, 한 줄:

```xml
<views:ViewportShell AutomationProperties.AutomationId="ViewportShell"
                     AutomationProperties.HelpText="{Binding Settings.ComparisonMode}" />
```

| 후보 | 판정 | 이유 |
|---|---|---|
| **`AutomationProperties.HelpText` 바인딩** | **채택** | 렌더되지 않는 메타데이터 · **설정에 직접** 붙어 중간 기록자가 없다 · 한 줄 · 요소를 더하지 않는다 |
| 이름만 갖는 비가시 요소(`TextBlock` + `Visibility=Collapsed`) | 버림 | **접힌 요소는 UIA 트리에서 사라진다** — C-46 이 opacity 패널에서 실측한 그대로다. 읽으려면 보이게 해야 하고, 그 순간 UI 변경이 된다 |
| `Opacity=0` 인 `TextBlock` | 버림 | 트리에는 남지만 **레이아웃을 차지한다** — 배치가 바뀌면 "노출" 이 아니라 UI 변경이다 |
| 상태바에 모드 표시 | 버림 | 가장 읽기 쉽지만 **보이는 것이 바뀐다.** 카드 범위 밖이고, #149 G-1 과 함께 다룰 일이다 |
| `AutomationProperties.Name` 바인딩 | 버림 | 뷰포트의 접근성 이름을 모드 문자열로 덮어쓴다 — 스크린리더 사용자에게 **의미가 바뀐다.** HelpText 는 보조 설명 자리라 덮어쓰는 것이 없다 |

**중간 기록자가 없다는 성질이 핵심이다.** C-46 이 고른 opacity 슬라이더는 클릭 핸들러가
지역 변수로 **따로** 설정했고(`ViewportShell.xaml.cs:30-31`), 그래서 설정 대입을 지운 반증이
스위트를 통과시켰다. HelpText 는 바인딩 말고 쓰는 주체가 없다.

## 2. 시각 무변화 확인 — 첫 방법은 쓸 수 없었다

**픽셀 비교를 시도했고 폐기했다.** 변경 전후 창 캡처가 828 800 / 1 196 000 픽셀 달랐는데,
**같은 바이너리로 두 번 찍은 대조군이 이미 258 523 픽셀 달랐다.** 캡처 자체가 비결정적이므로
이 방법은 어떤 결론도 지지하지 못한다 — 828k 를 "변경 탓" 으로 귀속할 근거가 없다.

**안정적인 지표로 바꿨다**: UIA 요소 146개의 `ControlType|Name|창 기준 상대 좌표` 를 변경 전후로
덤프해 비교.

```
--- 차이 줄 수: 4
46c46
< List||X=-130,Y=-130,W=0,H=0
> List||X=-156,Y=-156,W=0,H=0
```

다른 4줄은 **전부 `W=0,H=0`**(미렌더 요소)이고, 좌표 차이는 창 위치 차이(-130 vs -156)를 그대로
따른다. **렌더되는 요소 중 움직인 것은 하나도 없다.**

첫 덤프는 **화면 절대 좌표**여서 288줄이 달랐다 — 창 위치가 달랐을 뿐이다. 상대 좌표로 바꾸고
나서야 답이 나왔다. 이것도 "재는 대상이 아니라 환경을 재고 있었다" 의 한 사례다.

한계는 명시한다: 이것은 **배치**가 같다는 증거이지 **색·글자**가 같다는 증거가 아니다.
HelpText 가 렌더 경로에 참여하지 않는다는 것은 구조적 사실이지 이 측정의 결과가 아니다.

## 3. W-12 — 버튼이 그 모드를 실제로 선택한다

```
W-12 Swipe:      viewport reports SwipeVertical      195 ms
W-12 Split:      viewport reports SplitLocked        211 ms
W-12 Overlay:    viewport reports OverlayOpacity     204 ms
W-12 Difference: viewport reports DifferenceHeatmap  196 ms
```

Mock·Native 양쪽에서 4건씩 돈다(Theory).

### 프라이밍을 단언하는 이유 — 첫 반증이 가르쳐 줬다

첫 판은 대상 버튼만 눌렀고, 반증에서 **3/4 만 실패했다.** Swipe 가 통과했는데
`SwipeVertical` 이 **기본값**이라, 클릭이 설정에 닿지 않아도 기대값이 이미 거기 있었기
때문이다 — 죽은 클릭과 산 클릭을 구분하지 못한다.

그래서 대상과 다른 모드로 **먼저 이동**하고, **그 프라이밍이 실제로 걸렸는지까지 단언**한다.
프라이밍만 넣고 검사하지 않았을 때도 여전히 3/4 였다(반증 아래서는 프라이밍도 죽으므로).
**검사까지 넣어야 4/4 가 된다.**

## 4. 합격 판정 — 반증 (빌드 결과 포함)

C-46 에서 **아무것도 잡지 못했던 바로 그 약화**:

```csharp
// vm.Settings.ComparisonMode = mode; (falsification GUI-C-47)
```

```
=== 빌드:
    경고 0개
    오류 0개                        ← 빌드 정상
=== 테스트 (W-12 만):
실패!  - 실패: 4, 통과: 0, 건너뜀: 0, 전체: 4

Priming with 'Difference' did not take effect (viewport reports 'SwipeVertical'),
so this case cannot tell a working click from a dead one.
Pressing 'Overlay' should select OverlayOpacity; the viewport reports 'SwipeVertical'. …
```

**4건 전부 실패한다.** C-46 에서는 같은 약화로 27건 중 0건이 실패했다. 관측점이 하류가
아니라는 뜻이고, 이것이 카드가 정한 합격 판정이다.

원복 후 Mock 27건 중 0 실패 재확인.

## 5. 시간과 게이트 여유

| 시점 | Native E2E | 게이트(3 min = 180 s) 대비 여유 |
|---|---|---|
| C-45 | 1 m 5 s (65 s) | **115 s** |
| C-46 | 1 m 55 s (115 s) | **65 s** |
| **C-47 (이번)** | **1 m 58 s (118 s)** | **62 s** |

이번 증가는 **3초**다(W-12 4건 ≈ 0.8 s + 프라이밍 대기). C-46 의 +50 s 와 성격이 다르다 —
그때는 렌더 테스트와 W-11 이 한꺼번에 붙었다.

**여유 62 s 는 30초 기준 위다.** 다만 추세는 한 방향이다: 115 → 65 → 62 s. 시나리오가 지금
속도로 더 붙으면 다음 두세 카드 안에 30초선에 닿는다. **Mock 은 14 s 로 여유가 크므로,
게이트 압박은 Native 전용 비용(앱 기동·네이티브 로딩)에서 온다.**

## 6. 실측 (verbatim)

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…   (앱 폴더 비움)
통과!  - 실패: 0, 통과: 27, 건너뜀: 0, 전체: 27 (1 m 58 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                    (Mock)
통과!  - 실패: 0, 통과: 26, 건너뜀: 1, 전체: 27 (14 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 는 C-46 시점 **23건** → **27건**(W-12 4건). Mock 의 건너뜀 1은 Native 전용
출처 가드(C-41). 통합 181 은 불변 — 이 카드는 통합을 건드리지 않았다.

## 7. 미검증 (Gaps)

- **색·글자가 같다는 것은 측정하지 않았다**(§2). 배치 불변까지가 이 측정의 범위다.
- **`SourceOnly`/`ProcessedOnly` 는 W-12 대상이 아니다** — 버튼이 없다(#149 G-3).
  HelpText 는 그 값도 보고하겠지만, 선택할 수단이 없어 시나리오로 확인하지 못했다.
- **HelpText 를 읽는 다른 소비자가 있는지 확인하지 않았다.** 스크린리더가 뷰포트의 보조
  설명으로 모드 문자열을 읽게 되는데, 그것이 적절한 문구인지는 접근성 관점의 판단이고
  이 카드에서 다루지 않았다(#149 에 함께 볼 만한 사항).
- **C-46 의 나머지 미검증은 그대로다** — Detach 창이 안 열리는 원인(카드가 곁가지 금지),
  `S01` 동반 실패 원인, 렌더 관측의 중앙 픽셀 1개 한계.
- 캡처 비결정성의 **원인**은 규명하지 않았다(§2) — 방법을 폐기하는 데 필요한 만큼만 쟀다.

## 8. 잔여 위험 (Residual risk)

- **HelpText 는 접근성 속성이다.** 자동화 관측을 위해 쓰고 있지만, 원래 용도는 사용자 보조
  설명이다. 접근성 스위트(§4.4, 다음 카드 계열)가 이 값을 "설명" 으로 검사하면 충돌할 수 있다.
- **관측점이 하나뿐이다.** 이 바인딩이 지워지면 W-12 는 실패로 드러나지만, 비교 모드의 외부
  가시성은 다시 0이 된다.
- **게이트 여유가 62 s 이고 줄어드는 방향이다**(§5). 시나리오를 더 붙이기 전에 Native 기동
  비용을 줄이는 쪽을 검토할 시점이 다가온다.
- 이 카드는 **보이는 것을 바꾸지 않았다** — 배치 실측이 그것을 뒷받침한다(§2).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
# 시각 무변화: 임시 프로브로 UIA 요소 덤프(창 기준 상대 좌표), 변경 전후 diff
dotnet test clients/ImageProcTest.E2ETests/… --filter "…TempCaptureProbe"   # 임시, 커밋 안 함
diff /tmp/geo-before.txt /tmp/geo-after.txt

# 반증
# gui/ImageProcTest/Views/ViewportShell.xaml.cs:30 을 주석 처리
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.E2ETests/… --filter "…W12"

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
