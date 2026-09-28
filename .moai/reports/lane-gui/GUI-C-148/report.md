# GUI-C-148 (#208) — 인용 전수가 두 갈래를 갈랐다: `P01` 은 개명, 클래스명은 남긴다

상태: **전수 완료 · 머리말 정정 · `P01` 개명 · 반증 완료.**

`P01` 은 이름이 **두 가지**를 약속했는데 둘 다 틀렸다 — 3072 프레임도 아니고, **비용을 단언하지도 않는다**(§3).

---

## 1. 인용 전수 — 먼저 재고, 그 수로 갈랐다 (카드 §1)

카드가 파급 크기로 갈래를 정하라고 했다. 그래서 **개명 전에** 전수를 떴다(전 범위, `.git`·`bin`·`obj`·`build` 제외).

### 1.1 `P01_RenderCost_OnA3072Frame` — **적다 → 개명**

| 인용처 | 성격 |
|---|---|
| `GsvgLargeFrameScenarios.cs:35` (선언) | **결합** — 고쳐야 함 |
| `.moai/reports/lane-gui/GUI-C-138/measured-p10.txt:69` | 과거 관측 기록 |
| `.moai/reports/lane-gui/GUI-C-147/report.md:111` | 제가 이 결함을 보고한 자리 |

**결합 인용 1건.** 워크플로·`csproj`·다른 시험·SPEC 어디에도 없다. CI 가 이 이름으로 거르지 않는다.
나머지 `P01` 문자열 일치(`BP01_GainOffset_4k`, `BP01_OffsetNull…`, `BP-01-05-preprocess-manifest.md`)는 **다른 계보**다 — 접두사 충돌일 뿐 이 시험과 무관.

→ 카드 표의 **첫째 갈래: 개명하고 전부 고침.**

### 1.2 클래스명 `GsvgLargeFrameScenarios` — **많다 → 남긴다**

| 항목 | 실측 |
|---|---|
| 인용 총계 | **33건 / 14파일** (카드 착수 시점. 이 보고서를 쓴 뒤 다시 세면 49건인데 그중 16건이 이 보고서 자신이다 — 49 − 16 = 33 으로 확인) |
| 그중 보고서 | `GUI-C-102`·`103`·`104`·`105`·`106`·`134`·`137`·`138`·`147` — 12파일 |
| **`#200` 산출물** | `Fixtures/P10PreviewTileSignature.txt` 에 클래스명이 들어 있다 — **카드 §4 가 손대지 말라고 한 자리** |
| CI 워크플로 | **0건** |

→ 카드 표의 **둘째 갈래**에 해당한다. 개명하지 않고, **머리말에 이름이 무엇을 잘못 약속하는지 적었다.**
`#200` 의 기록 파일을 건드려야 완결되는 개명은 이 카드에서 할 수 없다. **리더 판단 사항**으로 남긴다(§6).

## 2. 머리말 — 긍정형으로 (카드 §2)

`:1` 과 `:16` 두 자리가 *"3072×3072 frame"* 이라고 적었다. **사실이 아니다.** `C-147` 에서 쓴 방식을 그대로 적용했다.

| 단락 | 내용 |
|---|---|
| **어느 프레임인가** | 파일은 3072², 앱이 싣는 것은 **첫 1024²**. 옛 문장이 무엇을 약속했는지도 한 줄 남겼다 |
| **왜 이 슬라이스인가(긍정형)** | 크기가 아니라 **화소 내용**. `GUI-C-117` 이 합성 프레임에서 **단계를 끊어도 통과하던 단언**을 발견해 이 픽스처로 옮겼다 |
| `REQ-GSVG-019` | 여기서 재지 않는다. 모듈 요구이고 post 의 `QA-B-102/103`(713–757 ms)로 충족. **3072 프레임의 GUI 종단 간 시간은 측정된 적이 없다** |

## 3. `P01` 이 실제로 무엇을 재는가 — **비용을 단언하지 않는다** (카드 §3)

카드의 의심이 맞았다. 이름은 `RenderCost` 인데, 이 시험의 **단 하나뿐인 단언**은 이것이다:

```csharp
Assert.True(second.TotalMs >= second.StageMs,
    "The stage cannot take longer than the render that contains it: …");
```

**포함 불변량**이다 — 단계는 렌더의 일부이므로 렌더보다 클 수 없다. 어떤 **상한과도 비교하지 않는다.**
밀리초 값들은 `output.WriteLine` 으로 **보고**될 뿐이다. 그 자체는 옳은 설계다(`GUI-C-90` 이 부하 걸린 기계가
이런 수치에 무엇을 하는지 쟀다) — **틀린 것은 이름이었다.**

실측이 그 간극을 보여 준다(§4.2):

| 모드 | stage | 렌더 총계 | GUI 몫 |
|---|---|---|---|
| `GsvgModeNone` | **0 ms** | 2290 ms | 2290 ms |
| `GsvgModeVirtualGrid` | **76 ms** | 2473 ms | 2397 ms |
| `GsvgModeGridSuppression` | **65 ms** | 2406 ms | 2341 ms |

단계는 65–84 ms, 렌더는 2.3–2.5 s — **비용의 97% 가 GUI 쪽**이다. 이름이 약속한 "render cost" 를
게이트로 읽으면 이 시험이 그것을 지키고 있다고 오독하게 된다. **아무 상한도 지키지 않는다.**

**새 이름**: `P01_TheStageFitsInsideTheRender_OnTheWristSlice`

- `TheStageFitsInsideTheRender` — **실제 단언**
- `OnTheWristSlice` — **실제 프레임**

`P01` 접두사는 유지했다 — 시나리오 번호는 보고서 전체가 쓰는 주소다.
doc-comment 에 옛 이름과 개명 사유, 그리고 **"수치는 보고이지 게이트가 아니다"** 를 적었다.

`#207`(`AC-SIMD-003` 개명)·`#212`(시험이 실제로 무엇을 단언하는지 읽기) 와 같은 형태다.

## 4. 반증 (카드 §5)

### 4.1 빌드 · 회귀

`dotnet build` **오류 0개**.

```
Native, GsvgLargeFrameScenarios 전체: 실패 1, 통과 13, 건너뜀 0, 전체 14 (3 m 19 s)
  실패 = P10_ThePreviewChange_KeptTheDrawnPixels
```

이 `P-10` 빨강은 **리더가 CI 로 로컬 전용 판정한 건**이다(카드 §6: 런 `03b0028` 의 `gui-e2e-native` success,
로그에 `P10`·`P11` 둘 다 실행). 제 `build/ci-common/bin` 이 `2026-09-26` 자 옛 산출물이다. **손대지 않았다.**

### 4.2 개명한 시험을 **이름으로 관측**했다

이름이 바뀌었으니 "통과 수가 같다" 로는 부족하다 — 새 이름이 실제로 실행되는지를 봐야 한다.

```
통과 …GsvgLargeFrameScenarios.P01_TheStageFitsInsideTheRender_OnTheWristSlice(radioId: "GsvgModeNone") [9 s]
통과 …GsvgLargeFrameScenarios.P01_TheStageFitsInsideTheRender_OnTheWristSlice(radioId: "GsvgModeVirtualGrid") [8 s]
통과 …GsvgLargeFrameScenarios.P01_TheStageFitsInsideTheRender_OnTheWristSlice(radioId: "GsvgModeGridSuppression") [8 s]
통과: 3
```

세 `InlineData` 케이스가 **새 이름으로** 전부 실행·통과했다. 위 §3 표의 수치가 이 실행의 출력이다.

### 4.3 잔여 인용 0건

옛 이름 `P01_RenderCost_OnA3072Frame` 이 남은 자리는 **새 doc-comment 의 역사 기술 1곳**뿐이다.
코드·설정·워크플로에 결합 인용 **0건**. **대조군**으로 새 이름이 검색에 1건 잡힌다.

파일에 남은 `3072` 언급 6곳은 전부 **사실 기술**이다(파일이 3072², 요구가 3072², 옛 문장이 그랬다는 기록).

## 5. 바꾼 것

| 자리 | 변경 |
|---|---|
| `GsvgLargeFrameScenarios.cs:1` | 머리 주석의 3072 약속 제거 + 실재 한 줄 |
| `:16` 클래스 summary | 3072 문장 → 네 단락(어느 프레임 · 왜 이 슬라이스 · `REQ-GSVG-019` 아님 · 종단 간 미측정) |
| `:35` 시험 | `P01_RenderCost_OnA3072Frame` → `P01_TheStageFitsInsideTheRender_OnTheWristSlice` + 개명 사유 doc |

**단언·문턱·픽스처 크기·`P-10`·`P-11`** — 전부 손대지 않았다(카드 §4).

## 6. 리더 판단이 필요한 것

- **클래스명 `GsvgLargeFrameScenarios`**: 33건/14파일이고 그중 하나가 `#200` 의 `P10PreviewTileSignature.txt` 다.
  개명하려면 그 파일을 고쳐야 하는데 카드 §4 가 막는다. 개명할지, 머리말 기술로 둘지 **지시가 필요하다**
- 보고서 문서 안의 옛 시험명(`GUI-C-138/measured-p10.txt` 등)은 **소급 수정하지 않았다** — 그때의 관측 기록이다

## 7. 미검증 / 잔여 위험

- **CI 에서 안 돌았다.** 수치는 전부 로컬 — **로컬 실측, CI 상한 추정**
- **Mock 에서 돌려 보지 않았다.** 이 컬렉션은 Native 전용이고(`C-147` 에서 Mock 17건 전부 건너뜀 관측),
  이 카드는 시험을 **추가하지 않고 이름만 바꿨으므로** 전제 단정이 새로 생기지 않았다 — 그래도 CI 의 Mock 잡
  결과는 확인하지 않았다
- `P-10` 빨강은 리더의 CI 판정(로컬 전용)을 **인용**한 것이고 제가 CI 로그를 직접 읽지는 않았다
- `P02`~`P09` 의 이름이 각자의 단언과 맞는지는 **보지 않았다** — 카드가 `P01` 로 범위를 못 박았다.
  같은 형태가 더 있을 수 있다

---

Refs #208
