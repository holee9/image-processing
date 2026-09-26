# GUI-C-78 — 중단 보고: 메인 창 뷰포트는 영상을 받지 않습니다 (#171)

- 카드: GUI-C-78 · Refs #171 #78 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- **커밋 없음. 구현을 시작하지 않았습니다.** 작업 트리 clean(임시 계측·프로브 원복).
- **이유**: ②를 설계하다가 **메인 창의 워크벤치 뷰포트가 `LaneAImage`/`LaneBImage` 에 바인딩돼 있고,
  그 둘은 코드 어디서도 대입되지 않는다**는 것을 찾았고, **실행으로 확인했습니다 — 영상이 로드된
  상태에서 뷰포트의 두 영상 속성이 모두 `NULL`.**
- **그래서 ②·①·③ 을 어디에 붙일지가 정해지지 않습니다.** 라벨·표시기가 붙을 뷰포트가 영상을
  보여 주지 않습니다. 이것은 제가 정할 수 없는 설계 결정입니다(§3).
- **C-77 의 제 서술 하나를 정정합니다**(§2).
- `#171` 에 사실 코멘트: https://github.com/holee9/image-processing/issues/171#issuecomment-5706705374

---

## 1. 무엇을 찾았나

### 1.1 바인딩 (읽음)

```
gui/ImageProcTest/Views/ViewportShell.xaml:15-17
  <controls:ImageComparisonViewport x:Name="Viewport"
                                    AutomationProperties.AutomationId="WorkbenchViewport"
                                    SourceImage="{Binding LaneAImage}"
                                    ProcessedImage="{Binding LaneBImage}"
```

```
grep -rn "LaneAImage|LaneBImage" gui/ImageProcTest --include=*.cs --include=*.xaml   (obj 제외)
  MainWindowViewModel.cs:43,44     필드 선언
  MainWindowViewModel.cs:459-468   속성 선언 (private set)
  ViewportShell.xaml:16,17         바인딩
  → 대입 0건
```

### 1.2 실행 (측정)

뷰포트의 `ProcessedImage`·`SourceImage` 의존 속성을 `ElementName=Viewport` 바인딩으로 `ZoomPercentText`
의 `ItemStatus`·`HelpText` 에 임시로 내보냈습니다(`TargetNullValue=NULL`, `FallbackValue=UNRESOLVED`).

```
viewport.ProcessedImage = 'NULL'
viewport.SourceImage    = 'NULL'
ActiveImageSummary      = 'RAW 1024x1024, min=0, max=63426, bytes=2097152 | MOCK Calibr…'   ← 대조군
```

- **대조군**: 같은 실행에서 영상은 로드돼 있습니다(요약이 `RAW 1024x1024` 를 말함). "아무것도 안 불러와서
  NULL" 이 아닙니다.
- `UNRESOLVED` 가 아니라 `NULL` 이므로 **바인딩 경로는 해석됐고 값이 null** 입니다.

### 1.3 언제부터

```
git show 54a3ae7 -- gui/ImageProcTest/MainWindow.xaml
-   SourceImage="{Binding SourceImage}"
-   ProcessedImage="{Binding ProcessedImage}"
```

커밋 **`54a3ae7`**(2026-05-09) *"feat(workbench): evaluation workbench UI 전체 구현 (slice 1-12) (#78)"* —
메시지에 *"Slice 4: ViewportShell (… HUD, 2-lane 바인딩)"*. 이 커밋이 `SourceImage`/`ProcessedImage` 를
보던 뷰포트를 지우고 `LaneAImage`/`LaneBImage` 를 보는 `ViewportShell` 을 넣었습니다.

**설계 의도**(`docs/design/README.md` "Interactions & behavior", "State management"):

- Lane 드롭다운을 바꾸면 **그 lane 의 디스플레이 파이프라인을 다시 실행**한다 — `ApplyDisplayPipelineAsync`
  에 lane 인자를 주거나 둘을 병렬로 돌려 두 `ImageSource` 에 저장
- `LaneAImage` = **Reference render**(`LaneAAlgorithm`), `LaneBImage` = **Candidate render**(`LaneBAlgorithm`)

**그 파이프라인은 만들어지지 않았습니다** — `ApplyDisplayPipelineAsync` 에 lane 인자가 없고, 두 속성에
쓰는 코드가 없습니다.

### 1.4 왜 아무 시험도 못 잡았나 (읽음)

| 점검 | 무엇을 보나 | 뷰포트의 영상을 보나 |
|---|---|---|
| E2E **W-01** "viewport carries it" | `ViewportShell` 존재 + 사각형 + 상태 표시줄 + 로그 | **아니오** |
| 자동화 `ComparisonViewportDetected` | **상수 `true`**(C-77 census) | 아니오 |
| 자동화 `ComparisonSourcePreserved` | `ReferenceEquals(viewModel.SourceImage, …)` — **뷰모델** 속성 | 아니오 |
| Rendering 스위트 | 컨트롤을 **직접** 만들어 영상을 넣고 그림 | 앱의 바인딩을 안 거침 |
| 픽셀 캡처(C-72) | 메인 창도 하얗게 나와 **눈먼 도구로 판정** | — |

**C-72 의 "메인 창도 하얗다" 는 도구가 눈멀었다는 근거로 썼습니다.** 뷰포트가 실제로 비어 있었다는
사실이 그 흰 화면의 일부를 설명할 수 있습니다. **다만 메뉴·패널까지 흰 것은 이것으로 설명되지 않으므로
캡처가 눈멀었다는 판정은 유지합니다** — 두 가지가 함께 참일 수 있습니다.

## 2. C-77 정정

C-77 보고서 §1.4 와 `#171` 코멘트에서 제 계측을 **"화면에 넘겨진 바로 그 비트맵의 지문"** 이라고
적었습니다. **틀렸습니다.** 그것은 뷰모델 속성 **`ProcessedImage` 에 대입된 비트맵의 지문**이고,
§1 에 따라 **메인 창 뷰포트는 그 속성을 표시하지 않습니다.**

`ProcessedImage` 를 바인딩하는 곳은 **분리 뷰어 하나**입니다(`OpenDetachedComparisonViewerCore`,
`BindingMode.OneWay`, #166). **분리 뷰어 안의 실제 값은 재지 않았습니다.**

**그대로 남는 것**:

- VOI 직접 입력이 파이프라인을 다시 돌리지 않는다는 **코드 경로**(1092행)와 `ProcessedImage` 가
  갱신되지 않는다는 **측정**은 유효합니다.
- 메인 창 HUD 가 `Settings` 에 바인딩돼 있다는 것도 유효합니다 — **다만 그 HUD 옆의 뷰포트는 비어 있습니다.**

`#171` 에 정정 사실을 적었습니다(§5 항).

## 3. 왜 멈췄나 — 판단이 필요한 두 갈래

카드의 ②(라벨을 영상을 만든 값에)·①(오래됨 표시)·③(`IsPreviewStale`)은 **메인 창 뷰포트가 영상을 보여
준다는 전제** 위에 있습니다. 그 전제가 없으므로 **무엇에 붙일지**가 먼저 정해져야 합니다.

| 갈래 | 내용 | 그러면 ②①③ 은 |
|---|---|---|
| **A. 설계대로 2-lane 을 구현** | Lane A/B 를 알고리즘별 파이프라인으로 그린다(`docs/design/README.md`) | **lane 마다** 영상을 만든 값·오래됨이 따로 생깁니다. 라벨·표시기가 lane 단위가 됩니다 |
| **B. 뷰포트를 `SourceImage`/`ProcessedImage` 로 되돌림** | `54a3ae7` 이전 바인딩 | 카드 설계 그대로 적용 가능 |

**어느 쪽도 레인이 정할 일이 아닙니다.** A 는 기능 구현(알고리즘 선택이 실제로 무엇을 바꾸는지 정의 필요),
B 는 워크벤치 설계를 되돌리는 결정입니다.

**B 를 고르면 바로 진행할 수 있는 순서**: ② → ①(오래됨 표시 방식; 즉시 재렌더는 현재 파이프라인에 취소·순서
보장이 없어 입력 중간값의 결과가 늦게 도착해 **새로운 오래된 영상 경로**를 만들 수 있다는 것을 코드에서 읽었습니다
— 실행 안 함) → ③ + E2E.

**③ 시험에 대한 관측 하나**(A/B 무관): **UI 로는 디스플레이 파이프라인 실패를 일으킬 수 없습니다.**
Mock(`CreateDisplayPreview`)과 Real(`RealXpeBackend.cs:138,151`) 모두 폭·기울기를 보정하고, 기울기·절편은
Analysis 패널에서 편집할 수도 없습니다. 전처리 거절도 Mock 에서는 메뉴가 비활성(`CanRunPreprocessing`),
Native 픽스처는 교정 세트를 줍니다. **실패 경로를 E2E 로 재려면 명령줄 전용의 좁은 결함 주입이 필요합니다**
(예: `--automation-fault display-pipeline-after:<N>` 을 `AutomationArgs` 에 추가, 모르는 값은 거절,
켜지면 로그에 크게 남김). **제품 코드에 시험 장치를 넣는 결정이라 함께 판단을 구합니다.**

## 4. 실측 (verbatim)

```
[임시 계측 — ViewportShell.xaml, ZoomPercentText 에
   ItemStatus = {Binding ElementName=Viewport, Path=ProcessedImage, TargetNullValue=NULL, FallbackValue=UNRESOLVED}
   HelpText   = {Binding ElementName=Viewport, Path=SourceImage,    TargetNullValue=NULL, FallbackValue=UNRESOLVED} ]
dotnet test …E2ETests… --filter "FullyQualifiedName~LaneProbe"
 viewport.ProcessedImage='NULL'
 viewport.SourceImage='NULL'
 ActiveImageSummary='RAW 1024x1024, min=0, max=63426, bytes=2097152 | MOCK Calibr'

git log -S"LaneBImage" -- gui/ImageProcTest            → 54a3ae7 2026-05-09 feat(workbench): … (#78)
git show 54a3ae7 -- gui/ImageProcTest/MainWindow.xaml → -SourceImage="{Binding SourceImage}"
                                                        -ProcessedImage="{Binding ProcessedImage}"

원복 후: git status --short → .bak 2건만 / git diff --stat → (비어 있음)
dotnet build clients/ImageProcTest.slnx -c Debug → 경고 0개 / 오류 0개
```

## 5. 미검증 (Gaps)

- **Native 에서 뷰포트 값을 재지 않았습니다.** 바인딩과 대입 부재는 백엔드와 무관하게 읽히지만 실행은 Mock 뿐입니다.
- **사람 눈에 메인 뷰포트가 비어 보이는지**는 보지 않았습니다(캡처 도구가 눈멀었음). 의존 속성 값이 null
  이라는 것까지입니다. `ImageComparisonViewport` 가 영상이 null 일 때 무엇을 그리는지는 코드로도 보지 않았습니다.
- **분리 뷰어가 받는 영상 값**은 재지 않았습니다(바인딩만 읽음).
- **`54a3ae7` 이후 누가 메인 뷰포트에서 영상을 봤다고 기록한 곳이 있는지** 찾지 않았습니다.
- 즉시 재렌더의 경쟁 조건은 **코드를 읽은 것**입니다.

## 6. 잔여 위험 (Residual risk)

- **메인 창에서 처리 영상을 볼 수 없는 상태가 2026-05-09 부터 계속됐을 수 있습니다.** 그동안의 모든 점검이
  뷰포트의 영상이 아니라 다른 것을 봤습니다(§1.4).
- **HUD 는 빈 뷰포트 옆에서 `Settings` 값을 표시합니다.**
- **`#171` 의 사용자 결정이 "메인 창에 오래된 영상이 보인다" 는 전제 위에서 내려졌습니다.** 그 영상은
  **분리 뷰어**에만 보입니다(바인딩 기준, 값은 미측정).

## Card Cross-Check

| 항목 | 상태 |
|---|---|
| ② 라벨 정정 | **중단** — 붙일 뷰포트가 영상을 표시하지 않음 |
| ① 오래됨 표시/재렌더 | **중단** — 같은 이유 |
| ③ `IsPreviewStale` | **중단** — 같은 이유 + 실패 경로 E2E 에 결함 주입 필요 여부 결정 대기 |
| E2E 시험 · 반증 · Native 1회 | 미착수 |
| `#171` 사실 코멘트 | 완료 (뷰포트 바인딩·실측·이력·C-77 정정) |
| 메인 뷰포트 영상 부재 | **새 결정 필요** — 갈래 A(2-lane 구현) / B(바인딩 복원) |
