# GUI-C-69 — CI 에서 Smoke 는 30 s 를 넘지 않습니다 (#165 후속)

> **읽은 CI 런**: **35071370089** · `merge(gui): GUI-C-67` · head `31a14352…` · conclusion **success**
> (2026-09-16T07:59:41Z). 아티팩트 `xpe-gui-e2e-smoke-results` / `xpe-gui-e2e-native-results`.

- 카드: GUI-C-69 · Refs #165 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **답: 넘지 않습니다.** Mock 25.5 s (85 %) · Native 22.9 s (76 %). **여유는 Mock 기준 4.5 s.**
- **다만 제가 가른 기준이 게이트가 키로 삼는 기준과 다릅니다**(§3). 이건 새 발견입니다.
- **결과: 통합 0/198/1/199 · Mock E2E 0/68/1/69 · slnx 0경고 0오류**

---

## 1. 할 일 1 — 어느 잡이 무엇을 도는가 (**카드의 잡 이름을 정정합니다**)

`ci.yml` 을 읽었습니다. **`gui-e2e-smoke` 라는 잡은 없습니다.** Mock 쪽은 별도 잡이 아니라
**`gui-automation` 잡 안의 한 스텝**입니다.

| 잡 | `timeout-minutes` | 이 스위트를 도는 스텝 | trx | 이 런의 잡 벽시계 |
|---|---|---|---|---|
| `gui-automation` | 15 | `Run FlaUI E2E smoke (Mock)` | `e2e-smoke.trx` | 08:00:01→08:02:22 = **141 s** |
| `gui-e2e-native` | 20 | `Run FlaUI E2E smoke (Native)` | `e2e-smoke-native.trx` | 08:05:02→08:08:45 = **223 s** |
| `dotnet-tests` | 30 | `Run C# integration tests` | — | E2E 아님 (IntegrationTests 199건) |

**둘 다 `--filter` 가 없습니다** — 프로젝트 전체 69건을 돕니다. 스위트를 나눠 도는 잡은 없습니다.

**`timeout-minutes` 는 게이트로 쓰지 않았습니다**(카드 지시). 위 표에 적은 것은 "잡이 무엇을
도는지" 를 확정하기 위한 것이고, §2 의 비율은 전부 §4.1·§4.2 성능 예산 대비입니다.

## 2. 할 일 2 — CI 스위트별 실측

trx 의 `UnitTest/@className` 으로 갈라 `UnitTestResult/@duration` 을 합산했습니다.

| 스위트 | **Mock** (`e2e-smoke.trx`) | **Native** (`e2e-smoke-native.trx`) | 게이트 | 소진 (Mock) |
|---|---|---|---|---|
| **Smoke** (§4.1) | **25.5 s** (21건) | **22.9 s** (21건) | **30 s** | **85 %** — 여유 **4.5 s** |
| **Workflow** (§4.2) | 30.0 s (25건) | 30.9 s (25건) | 180 s | 17 % — 여유 약 149 s |
| Rendering | 1.0 s (23건) | 0.9 s (23건) | 지정 없음 | — |
| (테스트 시작~종료 벽시계) | 58.4 s | 55.4 s | 지정 없음 | — |

**Mock 쪽이 정본입니다** — 계획서 §5.3(213–214행)이 두 게이트를 **PR 의 Mock 실행**에 겁니다.

**로컬과 CI 는 이번엔 갈리지 않았습니다**: 로컬 Smoke 22.9–23.2 s vs CI Mock 25.5 s
(+11 %). C-51 의 1.91배 선례는 이번 수치에는 해당하지 않습니다.

**C-68 §4 의 교정 생성 38 s 는 CI 어느 테스트에도 안 잡혔습니다.** 컬렉션 픽스처가 먼저
만들어졌다는 뜻이고, 그래서 CI 의 Smoke 합계는 61 s 쪽이 아닌 23–25 s 쪽입니다. **다만 그
귀속은 실행마다 갈리므로**(C-68 에서 관측) **CI 에서도 61 s 로 보이는 실행이 나올 수 있습니다.**

## 3. 그런데 **제 분류 기준이 게이트의 기준과 다릅니다**

계획서 262행은 게이트를 **트레이트**에 겁니다:

```
[Trait("Category", "Smoke")]        // < 30s gate
```

**저는 클래스의 네임스페이스(폴더)로 갈랐습니다. trx 는 트레이트를 기록하지 않기 때문입니다.**
둘을 코드에서 대조했습니다.

| 클래스 | 폴더 | `Category` 트레이트 | Mock 시간 |
|---|---|---|---|
| `CiProvenanceCompatibilityTests` | Smoke | **Smoke** | ─ |
| `NativeProvenanceTests` | Smoke | **Smoke** | ─ |
| `PanelToggleScenarios` | Smoke | **Smoke** | ─ |
| `WindowReacquireTests` | Smoke | **Smoke** | ─ |
| **`SmokeScenarios`** | **Smoke** | **없음** | **0.9 s** (5건) |
| `WorkflowScenarios` 외 3 | Workflows | **없음** (Workflow 트레이트는 저장소에 0건) | 30.0 s |
| `Rendering/*` 4개 | Rendering | Rendering | 1.0 s |

- **트레이트 기준 Smoke = 16건 24.6 s** / **폴더 기준 Smoke = 21건 25.5 s**. 차이는 0.9 s 라
  **결론(30 s 미만)은 어느 기준으로도 같습니다.**
- **그러나 `Category="Workflow"` 트레이트는 저장소에 하나도 없습니다.** §4.2 게이트를
  트레이트로 거는 도구가 있다면 **잴 대상이 0건**입니다.
- **그리고 §4.1 이 원래 재려던 다섯 건은 `SmokeScenarios` 하나뿐이고 CI 에서 0.9 s 입니다.**
  나머지 16건(트레이트 기준)은 C-29 이후 우리가 같은 폴더·같은 트레이트에 넣은 것들입니다.

**어느 기준이 맞는지 제가 정하지 않습니다.** §2 의 수치는 **폴더 기준**이고, 트레이트 기준
수치도 위에 같이 적었습니다.

## 4. 할 일 3 — 넘으면 무엇을 할지, **넘기 전에** (선택지 3 + 근거)

먼저 카드가 묻은 것: **`< 30s` 의 근거가 문서에 있는가.**

- **게이트 값 자체의 유도는 없습니다.** §4.1 제목에 `(Gate: < 30s)` 가 있을 뿐, 왜 30 인지는
  어디에도 적혀 있지 않습니다.
- **다만 무엇을 재려 했는지는 적혀 있습니다.** §4.1 의 표는 **S-01~S-05 다섯 건**이고, 같은
  절의 C-29 정정 노트가 **"스모크 5건 실측 합 584 ms"** 라고 적습니다. **30 s 는 0.58 s 짜리
  다섯 건에 대한 약 50배 여유입니다.** 지금 그 자리에 21건이 들어가 25.5 s 를 씁니다.

### 선택지 A — 스위트를 옮긴다 (§4.1 → §4.2)

- **근거**: §4.2 에 149 s 가 남습니다.
- **성격 판정**(카드 지시대로 여유가 아니라 성격을 봤습니다): S06~S10 은 **메뉴를 열고 토글을
  켜고 끄고 초기화하는** 여러 단계짜리 상호작용입니다. §4.1 의 S-01~S-05 는 **"창이 있나,
  메뉴 6개가 있나"** 식의 존재 확인이고 합이 0.58 s 입니다. **S06~S10 은 §4.2 쪽 성격에
  가깝습니다** — 옮기는 근거가 여유가 아니라 성격입니다.
- **비용**: 트레이트를 바꾸면 §3 의 어긋남을 건드립니다. `Category="Workflow"` 가 저장소에
  0건이므로 **새 값을 도입**하는 셈이고, 그 값을 읽는 도구가 아직 없습니다.

### 선택지 B — 시나리오를 싸게 만든다

- **근거**: C-68 §5 에서 S10 의 6.53 s 가 **대기가 아니라 상태 만들기**(메뉴 4회 여닫기,
  단계마다 300~600 ms)라고 쟀습니다. 상태를 UI 대신 설정 파일로 만들면 줄어듭니다.
- **비용**: **그러면 그 시나리오가 검증하는 것이 달라집니다.** S08·S10 은 "사용자가 메뉴로
  껐다 켰을 때 화면이 따라오는가" 를 봅니다. 상태를 뒤로 주입하면 **메뉴 경로가 빠집니다** —
  C-67 에서 S08 이 다른 테스트가 남긴 상태에 기댔다가 깨진 것과 같은 종류의 약화입니다.
- **깎지 않는 범위 안의 여지**: 고정 `Thread.Sleep` 을 **조건 폴링**으로 바꾸는 것은 검증을
  줄이지 않습니다. 재보지 않았으므로 **효과는 미검증**입니다.

### 선택지 C — 게이트를 다시 유도한다

- **근거**: 위에서 본 대로 **30 s 는 다섯 건 0.58 s 에 대한 값**이고, 그 다섯 건은 지금도
  0.9 s 입니다. **게이트는 그대로인데 대상이 4배로 늘었습니다** — C-68 에서 확인한 "게이트가
  잘못된 대상에 걸려 있다" 와 같은 형태이고, 이번엔 반대 방향입니다.
- **형태**: (i) §4.1 을 원래 다섯 건으로 되돌리고 나머지를 새 스위트로 분리하거나,
  (ii) 30 s 를 현재 대상에 맞게 다시 잡는 것.
- **비용**: 문서가 main 소유라 **제가 고칠 수 없습니다.** 그리고 **근거 없이 상한을 올리는
  것은 가장 나쁜 방향**입니다(C-68 카드의 지시).

**제 정리**: 지금은 게이트 안이므로 **당장 손대야 하는 것은 없습니다.** 다음 카드에서 Smoke 에
5 s 이상이 더 들어가면 넘습니다. **A 와 C 는 둘 다 §3 의 어긋남을 먼저 정리해야 합니다** —
어느 스위트에 있는지가 폴더와 트레이트 두 곳에 따로 적혀 있고 지금 두 값이 다릅니다.

## 5. 부수 — 다섯 속성에 주석 한 줄

`gui/ImageProcTest/ViewModels/MainWindowViewModel.cs:363` 위:

```csharp
// No readers since GUI-C-68: ShowRuntimePanel, ShowRawSettingsPanel, ShowImageSummaryPanel,
// ShowMetadataPanel and ShowAlertsPanel name panels that do not exist — their menu items were
// removed (C-65) and the automation report stopped emitting them (C-68). Removal is a separate
// card; ShowCalibrationPanel and ShowLogsPanel below are still read and stay.
```

**제거는 하지 않았습니다**(카드가 별도 카드로 지정).

## 6. 실측 (verbatim)

```
gh run view 35071370089 --json jobs
  gui-automation   success  2026-09-16T08:00:01Z  2026-09-16T08:02:22Z
  gui-e2e-native   success  2026-09-16T08:05:02Z  2026-09-16T08:08:45Z
  dotnet-tests     success  2026-09-16T08:05:00Z  2026-09-16T08:06:14Z

e2e-smoke.trx         (Mock,   gui-automation)  Smoke 25.5 / Workflow 30.0 / Rendering 1.0
e2e-smoke-native.trx  (Native, gui-e2e-native)  Smoke 22.9 / Workflow 30.9 / Rendering 0.9
  트레이트 기준 Smoke: Mock 24.6 (16건) · Native 22.5 (16건)
  SmokeScenarios(S-01~S-05)만: Mock 0.9 · Native 0.4

grep -rn 'Trait(' clients/ImageProcTest.E2ETests --include=*.cs
  Rendering/*.cs ×4        Category=Rendering
  Scenarios/Smoke/*.cs ×4  Category=Smoke     ← SmokeScenarios.cs 는 없음
  (Category=Workflow: 0건)

docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md:102  ### 4.1 Smoke Test Suite (Gate: < 30s)
docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md:114  ### 4.2 Workflow Suite (Gate: < 3min)
docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md:262  [Trait("Category", "Smoke")] // < 30s gate

dotnet build clients/ImageProcTest.slnx -c Debug   경고 0개 / 오류 0개
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 198, 건너뜀 1, 전체 199
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 68,  건너뜀 1, 전체 69 (48 s)
```

**시나리오를 깎지도 지우지도 않았습니다**(카드 지시). E2E 69건 그대로입니다.

## 7. 미검증 (Gaps)

- **CI 를 새로 돌리지 않았습니다.** 수치는 **C-67 병합 런 35071370089** 의 아티팩트를 읽은
  것이고, 그 런에는 이 카드의 커밋이 없습니다(주석 한 줄이라 시간에 영향은 없습니다).
- **런 1회입니다.** 로컬은 3회 배치로 편차를 봤지만 **CI 의 편차는 모릅니다.** 25.5 s 가
  평균인지 상단인지 하단인지 말할 수 없습니다.
- **트레이트로 실제로 필터해 돌려 보지 않았습니다.** §3 은 코드의 트레이트 선언과 trx 의
  클래스명을 대조한 것이고, `--filter "Category=Smoke"` 를 실행한 결과가 아닙니다.
- **게이트를 읽어 실패시키는 도구가 있는지 확인하지 않았습니다.** `ci.yml` 의 두 스텝에는
  없습니다. 다른 곳(스크립트·대시보드)은 보지 않았습니다 — **"없다" 가 아니라 "ci.yml 에는
  없다" 입니다.**
- **선택지 B 의 "폴링으로 바꾸면 준다" 는 재지 않았습니다.**
- **주석 한 줄이 무엇도 바꾸지 않는다는 것**은 빌드 0경고·테스트 동일로만 확인했습니다.

## 8. 잔여 위험 (Residual risk)

- **여유가 4.5 s 입니다.** S10 한 건(6.5 s)이면 넘습니다 — **다음 Smoke 시나리오 한 건이
  게이트를 넘길 수 있습니다.**
- **교정 생성 38 s 가 CI 에서 Smoke 로 귀속되는 실행이 나오면 61 s 로 보입니다**(C-68 §4).
  그 실행에서는 게이트 밖으로 읽힙니다.
- **분류가 두 곳에 따로 적혀 있고 값이 다릅니다**(§3). 어느 쪽을 정본으로 할지 정하지 않으면
  "어느 스위트인가" 에 답이 두 개입니다.
- **`Category="Workflow"` 가 0건**이라, 트레이트로 §4.2 를 거는 순간 대상이 비어 통과합니다.

## 부록 — 사용한 명령

```bash
gh run view 35071370089 --json displayTitle,headSha,conclusion,jobs
gh run download 35071370089 -n xpe-gui-e2e-smoke-results -n xpe-gui-e2e-native-results
# trx 를 className 으로 갈라 duration 합산 (폴더 기준 + 트레이트 기준 두 벌)
grep -rn 'Trait(' clients/ImageProcTest.E2ETests --include=*.cs
sed -n '352,368p;443,463p' .github/workflows/ci.yml   # (image-processing 체크아웃)
```
