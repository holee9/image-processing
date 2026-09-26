# GUI-C-68 — 180초 게이트의 출처 (#165 후속)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-68 · Refs #165 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **게이트는 저장소에 있습니다.** 그리고 **제가 그것을 잘못된 대상에 적용해 왔습니다**(§2).
- **결과: Native 3회 0 실패(90.8 s) · Mock 0/68/1/69 · 통합 0/198/1/199 · slnx 0경고 0오류**

---

## 1. 출처 — 찾았습니다

```
docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md
  102: ### 4.1 Smoke Test Suite (Gate: < 30s)
  114: ### 4.2 Workflow Suite (Gate: < 3min)      ← 180 s 의 출처
```

**숫자 `180` 으로는 잡히지 않습니다** — 문서가 `< 3min` 으로 적고 있습니다. 카드가
"`ci.yml`·E2E 프로젝트·`XPE-GUI-E2E-001` 어디에도 없다" 고 한 것은 **그 표기 때문**이고,
**검색어가 결론을 정한 네 번째 사례**입니다(링커 스캔 · grep 범위 · 부재 단언 · 이번).

**전파 경로**도 추적했습니다:

| 자리 | 내용 |
|---|---|
| `XPE-GUI-E2E-001` §4.2 제목 | `Gate: < 3min` — **원본** |
| `.moai/reports/lane-gui/GUI-C-43/report.md:88` | "스위트 전체 Native 1 m 4 s … **§4.2 게이트(3 min) 안이다**" — **제가 처음 인용** |
| `GUI-C-46`·`GUI-C-47` 보고서 | "게이트(3 min)" · "게이트(3 min = 180 s)" — 제가 숫자로 환산 |
| 카드 `GUI-C-48` (리더) | "여유 115 s → 65 s → 62 s (**게이트 180 s**)" — 제 보고서에서 카드로 |

**출처는 실재하고, 저는 §4.2 라고 정확히 인용했습니다.** 문제는 다음 절입니다.

## 2. 제 사용법이 틀렸습니다 — 범위와 대상 둘 다

**§4.2 의 게이트는 Workflow 스위트에 걸립니다.** 저는 **Native 실행 전체**(Smoke + Workflow +
Rendering + 픽스처 기동)를 그 게이트에 비교해 왔습니다. C-43 의 그 문장부터 그렇습니다 —
"스위트 전체 Native 1 m 4 s … §4.2 게이트 안이다".

**그리고 §4.1 에 30초 게이트가 하나 더 있는데 한 번도 보지 않았습니다.**

## 3. 스위트별 실측

`--logger trx` 의 테스트별 duration 을 클래스 경로로 묶었습니다(C-67 배치 3회).

| 스위트 | 실측 | 게이트 | 비율 |
|---|---|---|---|
| **Workflow** (§4.2) | **26.4 · 27.0 · 27.1 s** | **180 s** | **15 %** — 여유 약 **153 s** |
| **Smoke** (§4.1) | **22.9 · 23.2 s** (그리고 60.6 s — §4) | **30 s** | **77 %** — 여유 약 **7 s** |
| Rendering | 0.6 s | 지정 없음 | — |
| (전체 벽시계) | 90.5 s | 지정 없음 | — |

**제가 걱정하던 쪽은 여유가 많고, 보지 않던 쪽이 빠듯합니다.** C-65~C-67 에서 추가한 S06~S10 이
**전부 Smoke** 에 들어갔기 때문입니다.

### 추세 (스위트별)

| 카드 | Smoke(테스트 시간) | Workflow |
|---|---|---|
| C-61 | 41.5 s | 27.5 s |
| C-63 | 39.9 s | 26.8 s |
| C-65 | 11.8 s | 26.5 s |
| C-66 | 13.7 s | 26.8 s |
| C-67 | 23.2 s* | 27.1 s |

**Workflow 는 다섯 카드 동안 26.4–27.5 s 로 평평합니다.** 움직인 것은 Smoke 뿐입니다.

## 4. 그런데 Smoke 수치에 함정이 있습니다 — 교정 생성 38 s

`ReadableWindow_IsKept_AndLeavesNoNote` 의 duration 이 **0.59 s 또는 38.2 s** 로 갈립니다.

```
c65b: 0.59 / 38.28 / 38.26     c66: 0.59 / 0.58 / 0.59
c67:  38.27 / 0.61 / 0.59      c68: 38.25 / 38.23 / 38.08   (이번 배치는 3회 모두)
```

**그 테스트의 비용이 아닙니다.** `SharedCalibrationSet` 은 프로세스당 한 번 XCal 을
생성(`Lazy`)하고, **그 비용은 가장 먼저 만들어진 픽스처가 냅니다.** 이 테스트는 **자기 픽스처를
테스트 본문 안에서** 만들기 때문에(컬렉션 픽스처와 달리) 그 차례가 오면 duration 에 잡힙니다.
컬렉션 픽스처가 먼저면 **어느 테스트에도 안 잡힙니다**(`dotnet test` 의 duration 은 픽스처 생성을
제외합니다 — C-48 에서 측정).

**따라서 Smoke 합계는 23 s 또는 61 s** 이고, 차이는 스위트의 일이 아니라 **귀속 위치**입니다.
전체 벽시계는 두 경우 모두 90 s 대로 같습니다.

**30 s 게이트를 어느 쪽으로 읽을지는 제가 정하지 않습니다** — "스위트 자신의 일" 이면 23 s,
"그 스위트에 귀속된 전부" 면 61 s 입니다. **문서는 그 구분을 적지 않습니다.**

## 5. +9.5 s 는 어디서 왔나

전체 벽시계 기준(배치 내 편차가 ±0.4 s 로 작아 비교 가능):

| 배치 | 벽시계 | 차이 |
|---|---|---|
| C-66 | 81.4 · 81.4 · 82.2 s | — |
| C-67 | 90.6 · 90.6 · 90.4 s | **+9 s** |

테스트별로는 **S09 2.90 s + S10 6.53 s = 9.43 s** 로 일치합니다.

**S10 이 더 비싼 이유**는 대기가 아니라 **상태 만들기**입니다 — 로그 탭 선택 → 토글 켜기 →
끄기 → 초기화 → 다시 끄기. 메뉴를 네 번 여닫고 각 단계에 300~600 ms 를 기다립니다.
**같은 형태의 시나리오는 같은 값을 냅니다.**

## 6. `visiblePanels` — 다섯 제거 (판정대로)

```csharp
visiblePanels = new { calibration = …, display = …, logs = … }
```

`runtime` · `rawSettings` · `imageSummary` · `metadata` · `alerts` 를 지웠습니다. 남긴 셋은 실제
상태를 말합니다(logs 는 로그 영역, 나머지 둘은 비활성 항목의 체크).

**부수 결과 하나**: 이제 `ShowRuntimePanel` · `ShowRawSettingsPanel` · `ShowImageSummaryPanel` ·
`ShowMetadataPanel` · `ShowAlertsPanel` **다섯 속성을 읽는 곳이 하나도 없습니다.** 정의만 남은
상태이고, **제거는 이 카드가 요청한 범위 밖이라 하지 않았습니다.**

## 7. 실측 (verbatim)

```
docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md:102  ### 4.1 Smoke Test Suite (Gate: < 30s)
docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md:114  ### 4.2 Workflow Suite (Gate: < 3min)

build/e2e-c67/run-2.trx   Smoke   23.2  Workflow   27.0  Rendering   0.6
build/e2e-c67/run-3.trx   Smoke   22.9  Workflow   26.4  Rendering   0.6
build/e2e-c67/run-1.trx   Smoke   60.6  Workflow   27.1  Rendering   0.6   ← 교정 38 s 귀속

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 68, 건너뜀: 1, 전체: 69 (50 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 198, 건너뜀: 1, 전체: 199

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 91s · run 2 91.5s · run 3 90s
```

**시나리오를 깎지 않았습니다**(카드 지시). E2E 69건 그대로입니다.

## 8. 미검증 (Gaps)

- **CI 에서의 스위트별 시간은 재지 않았습니다.** 여기 숫자는 이 기계의 것이고, C-51 이 기록한
  "게이트를 잘못된 기계에서 잡았다"(로컬 702 ms → CI 1340 ms, 1.91배)가 그대로 해당합니다.
  **CI 에서 Smoke 가 30 s 안인지는 모릅니다.**
- **잡 단위 `timeout-minutes` 대비 비율은 계산하지 않았습니다** — `ci.yml` 은 main 소유라 읽기만
  했고, 어느 잡이 이 스위트를 도는지 확정하지 못했습니다. 필요하면 잡 이름을 주시면 재겠습니다.
- **§4.1/§4.2 게이트가 "테스트 시간" 인지 "벽시계" 인지 문서가 말하지 않습니다**(§4).
- **Rendering 스위트에는 지정된 게이트가 없습니다.** 0.6 s 라 지금은 문제가 아닙니다.
- **교정 생성 38 s 의 귀속이 왜 배치마다 다른지**는 재지 않았습니다 — 컬렉션 순서로 보이지만
  확정하지 않았습니다.

## 9. 잔여 위험 (Residual risk)

- **Smoke 가 30 s 게이트의 77 % 입니다**(테스트 시간 기준). 같은 자리에 시나리오를 더 넣으면
  넘습니다 — **어느 스위트에 넣을지가 이제 선택 사항입니다.**
- **교정 38 s 가 Smoke 에 귀속되는 실행에서는 61 s 로 보입니다.** 게이트를 벽시계로 읽는
  도구가 있으면 이미 넘은 것으로 읽습니다.
- **다섯 속성이 읽는 곳 없이 남아 있습니다**(§6).
- **제 보고 네 장이 잘못된 대상에 게이트를 적용했습니다**(C-43·C-46·C-47·C-67 및 그 사이
  보고서들의 "여유" 수치). **그 수치들은 폐기해야 합니다** — 스위트별 수치는 §3 입니다.

## 부록 — 사용한 명령

```bash
grep -n "Gate:" docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md
grep -rn "3 min\|3분" .moai/reports/lane-gui/*/report.md
# trx 의 testName 경로로 스위트를 가르고 duration 합산
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c68
```
