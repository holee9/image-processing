# GUI-C-75 — 토큰을 달았고, 게이트를 로컬에서 직접 돌려 확인했습니다 (#30011)

- 카드: GUI-C-75 · Refs #30011 #160 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시. **변경은 파일 하나, 5줄**(주석 3 + 메시지 2).
- **반증까지 로컬에서 실제로 돌렸습니다** — `:30011` 초록(exit 0) / `:99999` **빨강(exit 1)**(§2).
- **C(카운터) 경로도 확인했습니다** — 잡 요약 출력을 실제로 받아 봤습니다(§3).
- `NativeRun_…` 에는 **토큰을 달지 않았습니다**(카드 지시).
- **결과: Mock 0/71/1/72 · Native 0/72/0/72 · 통합 0/200/1/201 · slnx 0경고 0오류**

---

## 1. 단 것 — 사람 읽는 문장은 그대로, 기계 읽는 토큰을 덧붙임

`clients/ImageProcTest.E2ETests/Scenarios/Smoke/WindowReacquireTests.cs`

```csharp
// The token is for the CI gate, the sentence is for the person reading the log; both stay.
// GUI-C-74 measured this at ~7% per launch, and GUI-C-75 chose to account for it rather
// than fail on it — prose drifts silently, so the gate keys on the token alone.
Skip.If(
    !string.IsNullOrEmpty(fixture.ReacquiredNote),
    $"Not measured: the real re-acquire fired on this launch — {fixture.ReacquiredNote} " +
    "XPE-SKIP-ALLOWED:30011");
```

**기존 문구는 한 글자도 안 바꿨습니다.** 뒤에 토큰만 붙였습니다.

## 2. 반증 — **게이트를 실제로 돌렸습니다**

로컬에서 CI 게이트를 그대로 실행했습니다. `ci.yml` 은 **읽기만** 했고, 493–541행의 스크립트
본문을 그대로 뽑아 trx 경로만 제 디렉터리로 바꿔 돌렸습니다(`.github/workflows/` 무변경).

**건너뛰기를 결정적으로 만들었습니다**: `ReadableWindow_…` 의 픽스처를 잠시
`simulateUnreadableChecks: 1` 로 무장해 노트가 반드시 생기게 했습니다(측정 후 원복).

| 토큰 | 게이트 출력 | exit |
|---|---|---|
| `XPE-SKIP-ALLOWED:30011` | `SKIPPED (accounted, #30011): …ReadableWindow_IsKept_AndLeavesNoNote - window re-acquisition flake…` | **0 (초록)** |
| `XPE-SKIP-ALLOWED:99999` | `NOT RUN (unexplained): …` + `1 E2E case(s) did not run … with no accounted reason` | **1 (빨강)** |
| 건너뛰기 없음(Native 전체 실행) | `total=72 executed=72 … allow-list entries: 1` | **0 (초록)** |

**게이트는 토큰을 봅니다.** 허용 목록에 없는 번호는 빨강이 됩니다.

**카운터 검사도 통과합니다.** 건너뛴 판에서 `total=2 executed=1` → `gap=1`, `accounted=1` 로
일치했습니다. C-37 주석이 경고한 `notExecuted=0`(실제로 0으로 남습니다)은 이 검사에 쓰이지
않으므로 문제가 되지 않았습니다 — **`executed` 는 제대로 줄어듭니다.**

## 3. C(카운터) 경로 — 잡 요약이 실제로 찍힙니다

`GITHUB_STEP_SUMMARY` 를 파일로 지정하고 같은 스크립트를 돌렸습니다.

```
### Native E2E skips
- accounted: 1
- unexplained: 0
  - #30011 ImageProcTest.E2ETests.Scenarios.Smoke.WindowReacquireTests.ReadableWindow_IsKept_AndLeavesNoNote
```

**카드 §4-4 가 물은 "잡 요약에 `accounted` 줄이 찍히는지" 는 이것으로 확인됩니다** — 다만
**CI 에서 찍히는 것은 아직 못 봤습니다**(§5).

## 4. 관측 하나 — 로그에서 em dash 가 깨집니다

게이트가 인쇄한 사유 문자열에서 `—`(em dash)가 `?` 로 나왔습니다.

```
NOT RUN (unexplained): …ReadableWindow… [NotExecuted] Not measured: the real re-acquire fired
on this launch ? The launched window did not support AutomationId (framework=Wpf); …
```

**토큰에는 영향이 없습니다**(ASCII 라서 그대로 읽힙니다) — **사람이 읽는 절반만 상합니다.**
**이것이 제 콘솔 인코딩 탓인지 CI 에서도 같은지는 확인하지 못했습니다.** 문구를 바꾸지
않았습니다(카드가 "지금 문구는 유지" 라고 했고, 고칠지는 리더 판단입니다).

## 5. 실측 (verbatim)

```
# 게이트를 ci.yml 493-541 행에서 그대로 뽑아 trx 경로만 바꿔 실행
sed -n '493,541p' .github/workflows/ci.yml | sed 's|build/e2e-native-results|build/c75|' > gate.ps1

[토큰 30011]
total=2 executed=1 passed=1 failed=0 notExecuted=0
allow-list entries: 1
SKIPPED (accounted, #30011): …ReadableWindow_IsKept_AndLeavesNoNote - window re-acquisition flake…
exit=0

[토큰 99999]
NOT RUN (unexplained): …ReadableWindow… XPE-SKIP-ALLOWED:99999
1 E2E case(s) did not run in the Native job with no accounted reason …
exit=1

[Native 전체 실행 trx (건너뛰기 0건)]
total=72 executed=72 passed=72 failed=0 notExecuted=0
allow-list entries: 1
exit=0

[GITHUB_STEP_SUMMARY 지정 시]
### Native E2E skips / - accounted: 1 / - unexplained: 0 / - #30011 …

dotnet build clients/ImageProcTest.slnx -c Debug     경고 0개 / 오류 0개
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 71,  건너뜀 1, 전체 72 (1 m)
Repeat-E2E.ps1 -Times 1 -Backend Native    runs with a failure: 0 / 1 — total=72 executed=72
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 200, 건너뜀 1, 전체 201
```

**Mock 의 건너뜀 1건은 `NativeRun_NamesTheBinariesItExercises`**(설계상, Mock 이라서)이고
**토큰을 달지 않았습니다** — Mock 게이트가 없으니 쓰이지 않는 장치가 됩니다(카드 §4-3).

**이번 Native 실행에서는 재획득 결함이 안 터졌습니다**(건너뜀 0). C-74 실측 기준 한 실행이
결함을 만날 확률이 약 25 % 이므로 **안 터진 것이 정상 범위**이고, 그래서 **결정적 재현을
따로 만들어** 게이트를 시험했습니다(§2).

## 6. 미검증 (Gaps)

- **CI 에서 돌려 보지 못했습니다.** 게이트는 **로컬에서 같은 스크립트**를 같은 형식의 trx 에
  돌린 것입니다. CI 러너의 pwsh 버전·인코딩·`Get-ChildItem` 경로 해석까지 같다는 보장은
  없습니다. **`accounted` 줄이 실제 CI 잡 요약에 찍히는 것은 아직 못 봤습니다.**
- **결정적 재현은 시험용 이음매로 만든 것**입니다(`simulateUnreadableChecks: 1`). 그 판의
  노트는 `framework=Wpf` 로, **실제 결함이 만드는 `Win32` 노트와 문자열이 다릅니다.**
  토큰은 두 경우 모두 같은 자리에 붙으므로 게이트 판정에는 차이가 없지만, **실제 결함이 터진
  trx 로 게이트를 돌려 본 것은 아닙니다.**
- **em dash 깨짐이 CI 에서도 나는지** 모릅니다(§4).
- **다른 건너뛰기가 생겼을 때** 게이트가 그것을 빨강으로 잡는지는 `:99999` 판으로만
  확인했습니다 — 실제로 다른 케이스가 건너뛰는 상황은 만들지 않았습니다.
- **CI 에서의 결함 빈도**는 여전히 모릅니다. C 의 카운터가 쌓여야 답이 나옵니다.
- **#30011 의 원인**은 이 카드도 안 건드립니다.

## 7. 잔여 위험 (Residual risk)

- **토큰이 붙은 건너뛰기는 이제 초록입니다.** 그 케이스가 **다른 이유로** 건너뛰게 되면 —
  예를 들어 `IsAvailable` 이 거짓이라 첫 `Skip.If` 가 걸리면 — **그 메시지에는 토큰이 없으므로
  빨강**입니다. 확인했습니다: 토큰은 두 번째 `Skip.If` 에만 붙였습니다.
- **Mock 잡에는 여전히 건너뛰기 게이트가 없습니다.** 같은 결함이 Mock 에서는 조용합니다.
- **`allow-list` 는 한 항목뿐이고 매 실행 그 수가 인쇄됩니다.** 늘어나면 보입니다 — **보는
  사람이 있어야 한다는 조건은 그대로입니다.**

## 부록 — 사용한 명령

```bash
sed -n '493,541p' .github/workflows/ci.yml | sed 's|build/e2e-native-results|build/c75|' > gate.ps1
dotnet test …E2ETests… --filter "FullyQualifiedName~WindowReacquireTests" \
  --logger "trx;LogFileName=g.trx" --results-directory build/c75
powershell -NoProfile -File gate.ps1                 # 토큰 30011 / 99999 두 판
powershell -NoProfile -Command "$env:GITHUB_STEP_SUMMARY='…'; & gate.ps1"
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 -Times 1 -Backend Native …
```
