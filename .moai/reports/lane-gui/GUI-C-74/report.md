# GUI-C-74 — 건너뛰기는 "통과로 세지 말자" 였지 "실패를 피하자" 가 아니었습니다 (#30011)

- 카드: GUI-C-74 · Refs #30011 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- **커밋 없음** — 설계 제안 카드입니다. **코드 무변경**(`git status` 에 `.bak` 2건만).
- **의도는 코드에 적혀 있었습니다**: skip 은 **pass 와 겨룬 선택**이지 fail 과 겨룬 선택이 아니었습니다(§1).
- **빈도 실측: 기동 100회 중 7회(7.0 %)** — 대조군 포함(§3).
- **후보 4개와 각각 잃는 것**(§4). **결정하지 않았습니다.**

---

## 1. 왜 실패가 아니라 건너뛰기인가 — 의도는 주석에 있습니다

`WindowReacquireTests.cs:74-79`, 커밋 **`6557c71`**(C-55, 2026-09-16):

```csharp
// The REAL defect can fire on this launch — GUI-C-55 caught it happening in 2 of 3 Native
// suite runs, the first time it has ever been seen outside a simulated trigger. When it does,
// this scenario has nothing to say: it asks what happens on a healthy acquire, and this was
// not one. Skipped with the note rather than passed, because "not measured" must not be
// counted as "measured and fine" (GUI-C-37).
```

**겨룬 상대는 `pass` 입니다.** "건강한 획득에서 노트가 비어 있다" 를 묻는 시나리오인데 그 기동이
건강하지 않았으니, 그대로 통과시키면 **재지 않은 것을 재고 괜찮았다로 세는** 것이 됩니다.
그래서 건너뛰었습니다. **`fail` 은 후보로 검토되지 않았습니다** — 그 자리에서는 결함이 이미
`UnreadableWindow_…` 로 보고되고 있다고 봤기 때문입니다.

**그리고 게이트는 그보다 먼저 있었습니다.** `ci.yml` 의 "Assert no skipped Native E2E cases" 는
**C-37 시절**(주석이 `2026-09-11` 관측을 인용) 것이고, **XCal generator 가 staged 인데도 W-02 가
skip 하는 상황을 잡으려고** 만든 것입니다. **두 결정은 서로를 모른 채 며칠 간격으로 들어왔고,
CI Native 기동에서 결함이 아직 안 터져서 만난 적이 없습니다.**

## 2. 게이트는 사유를 **인쇄하지만 구분하지는 않습니다**

`.github/workflows/ci.yml` (Native 잡):

```pwsh
$skipped = @($results | Where-Object { $_.outcome -ne 'Passed' -and $_.outcome -ne 'Failed' })
foreach ($s in $skipped) { Write-Host ("NOT RUN: {0} [{1}] {2}" -f $s.testName, $s.outcome, $s.Output.ErrorInfo.Message) }
if ($skipped.Count -gt 0 -or [int]$c.total -ne [int]$c.executed) { Write-Error (...); exit 1 }
```

- **사유는 로그에 남습니다.** 판정은 **사유를 보지 않습니다** — 건너뛴 게 하나라도 있으면 실패.
- **카드 §4-2 의 답**: `NativeRun_NamesTheBinariesItExercises` 와 이 건은 **게이트에서 구분되지
  않습니다.** 지금 부딪히지 않는 이유는 **정황**뿐입니다 — 그 케이스는
  `XPE_E2E_BACKEND != Native` 일 때만 건너뛰고, 게이트는 **Native 잡에만** 있습니다. 그래서
  그 조합은 일어날 수 없습니다. **누가 이 게이트를 Mock 잡(`gui-automation`)에 붙이면 설계상의
  건너뛰기로 즉시 빨간불이 됩니다.**
- **Mock 쪽에는 이런 게이트가 아예 없습니다.** Mock 의 건너뛰기는 판정에 영향이 없습니다.

## 3. 빈도 — 기동 100회, **7회(7.0 %)**. 대조군 포함

**세는 자리**: 픽스처는 기동마다 `XPE-C56-PROBE … framework=<Wpf|Win32>` 한 줄을 남깁니다
(`ProbeAcquisitionRoutes`, C-56). **`Win32` 가 실제 결함**입니다 — 시험용 이음매는 읽기 검사만
실패시키고 이 줄은 실제 값을 적습니다.

| 배치 | 기동 수 | `framework=Win32` | 비율 | `ReadableWindow_…` 건너뜀 | 대조군 `UnreadableWindow_…` |
|---|---|---|---|---|---|
| Mock ×30 | 60 | **5** | **8.3 %** | **1 / 30** | **30 / 30 통과** |
| Native ×20 | 40 | **2** | **5.0 %** | **0 / 20** | **20 / 20 통과** |
| 합계 | **100** | **7** | **7.0 %** | 1 / 50 | 50 / 50 통과 |

- **대조군이 붙어 있습니다**(카드 §5): 같은 실행에서 `UnreadableWindow_IsReplaced_AndTheRunSaysSo`
  가 **50회 전부 통과**했습니다. 0 건이 "아무것도 안 돌아서" 가 아닙니다.
- **기동 결함률(7 %)과 이 케이스의 건너뜀률(2 %)은 다른 수입니다.** 결함은 아무 픽스처에서나
  터지는데, 건너뛰는 것은 **이 케이스 자신의 기동**에서 터졌을 때뿐입니다.
- **선례와 같은 자리**: C-50 이 60 기동 중 3회(5 %)로 쟀습니다. **7 % 는 그와 같은 범위**입니다.
- **주의 — 순진한 grep 은 못 씁니다.** trx 전체에서 `re-located` 를 세면 **시험용 이음매가 만든
  노트까지** 셉니다(Mock 30회에서 `framework=Wpf` 55줄). **`XPE-C56-PROBE … framework=Win32`
  만이 실제 결함의 자리**입니다.

**전체 스위트 1회 기준**으로는 기동이 4회(Mock)/4회(Native)이므로, **한 번의 CI Native 실행이
결함을 만날 확률은 대략 1-(1-0.07)^4 ≈ 25 %** 입니다 — **이 추정은 기동이 서로 독립이라는
가정에 기댑니다. 재지 않았습니다.**

## 4. 후보 — 넷, 각각 잃는 것과 함께

### A. 결함 전용 케이스를 두고, **그것만** 결함을 보고한다 (건너뛰기 → 실패)

`ReadableWindow_…` 의 `Skip.If` 를 `Assert.False` 로 바꾸거나, 같은 조건을 보는 **새 케이스**를
두어 실패시킵니다.

- **얻는 것**: 결함이 **사유가 붙은 상시 신호**를 갖습니다. 게이트에서 분리됩니다.
- **잃는 것**: **CI Native 실행의 약 1/4 이 빨개집니다**(§3 추정). 고쳐지지 않은 환경 결함으로
  머지가 막히고, 이 저장소가 기록한 **"읽는 사람 없는 빨간불"** 이 정확히 그 상태입니다.
  빨강이 일상이 되면 **다음 진짜 빨강이 안 읽힙니다.**

### B. 게이트가 **사유로** 구분하게 한다 (`.github/workflows/` — 리더 소유)

게이트가 이미 인쇄하는 사유 문자열을 판정에도 쓰게 합니다. 예: 메시지가
`Not measured: the real re-acquire fired on this launch` 로 시작하는 건너뛰기 **하나만** 허용하고,
**그 외 모든 건너뛰기는 지금처럼 실패.**

- **얻는 것**: 게이트의 원래 일(빠진 DLL·staging 문제)을 **그대로** 지키면서 섞인 신호를
  가릅니다. **테스트 코드 변경 0.** 오늘 바로 가능합니다.
- **잃는 것**: ① **사유 문자열이 게이트의 계약**이 됩니다 — 문구가 바뀌면 조용히 안 맞고,
  그때는 빨강으로 떨어지므로 **안전한 방향**이지만 원인 찾기가 한 겹 멀어집니다.
  ② **결함은 여전히 세어지지 않습니다.** 허용될 뿐, 빈도·추세는 아무 데도 안 남습니다.
  ③ allow-list 는 **나중에 넓히기 쉽습니다** — 초록이 급한 사람에게 열려 있는 문입니다.

### C. **센다** — 기동마다의 `framework` 를 집계해 기록한다

trx 의 `XPE-C56-PROBE` 줄에서 `Win32` 수를 세어 **잡 요약 또는 아티팩트**에 적습니다
(`N/M 기동`). 원하면 **비율 상한**을 두고 그때만 실패시킬 수 있습니다.

- **얻는 것**: **빈도와 추세**가 생깁니다 — "고쳐졌는지" 를 물을 수 있는 유일한 후보입니다.
  그리고 **이 케이스 하나가 아니라 모든 컬렉션의 기동**을 덮습니다(노트는 이미 7개 클래스의
  출력에 실립니다).
- **잃는 것**: ① **아무것도 게이트하지 않습니다** — 누가 읽어야 하고, 이 저장소에는 **이틀간
  아무도 안 읽은 빨간 워크플로** 선례가 있습니다. ② 상한을 두려면 **CI 기준선이 필요한데
  아직 없습니다**(제 수치는 이 기계 것입니다). ③ 세는 자리가 **문자열**이라 프로브 줄의
  형식이 바뀌면 조용히 0이 됩니다 — **부재가 성공으로 보이는** 바로 그 형태입니다.

### D. 테스트가 **다시 기동한다** (회피가 아니라 재시도)

`ReadableWindow_…` 가 건강한 획득을 얻을 때까지 최대 N회(예: 3) 새로 기동하고, **몇 번째에
얻었는지 출력**합니다. N회 모두 터지면 그때는 건너뛰거나 실패합니다.

- **얻는 것**: 기동당 7 % 이면 **3회 재시도 후 미측정 확률은 약 0.03 %** — 건강 경로 단언이
  **거의 항상 실제로 돌아갑니다.** 그리고 **시도 횟수 자체가 빈도 신호**입니다(C 를 공짜로 얻음).
- **잃는 것**: ① 불운한 실행에서 **기동 2~3회분(약 2.5 s/회)** 이 붙습니다 — Smoke 예산이
  CI Mock 에서 **26.4 s / 30 s** 입니다. ② **결함이 흔해지는 회귀를 가릴 수 있습니다** — N회
  모두 실패할 때만 신호가 나므로, 7 % → 30 % 가 돼도 초록입니다(시도 횟수를 기록하고 상한을
  두면 막히지만, 그러면 C 의 기준선 문제를 그대로 물려받습니다).

### 조합에 대한 관측 (결정 아님)

- **B + C** 는 서로의 빈 자리를 메웁니다 — B 가 게이트를 지키고, C 가 없는 카운터를 만듭니다.
- **D + C** 도 같습니다 — D 의 시도 횟수가 곧 C 의 카운터입니다. 이 경우 **B 는 필요 없어질
  수 있습니다**(건너뛰기가 사실상 사라지므로). 다만 **"사실상" 은 게이트가 받아 주는 단어가
  아닙니다** — 0.03 % 라도 나면 빨간불입니다.
- **A 단독**은 게이트 문제는 풀지만 빨간불 문제를 만듭니다.

## 5. 실측 (verbatim)

```
# 30 × Mock, 20 × Native — 각 실행이 WindowReacquireTests 2건(= 기동 2회)
for i in $(seq 1 30); do dotnet test … --filter "FullyQualifiedName~WindowReacquireTests" \
  --logger "trx;LogFileName=mock-$i.trx" --results-directory build/c74; done

probe framework per launch (Mock):   {'Wpf': 55, 'Win32': 5}   total launches = 60
outcomes (Mock):  UnreadableWindow Passed 30 / ReadableWindow Passed 29, NotExecuted 1
probe framework per launch (Native): {'Wpf': 38, 'Win32': 2}   total launches = 40
outcomes (Native): UnreadableWindow Passed 20 / ReadableWindow Passed 20, NotExecuted 0

# 전체 스위트 1회에서도 같은 자리
build/e2e-c73/c73-mock2.trx    probe: Wpf 4                      (건너뜀 1 = NativeRun…, 설계대로)
build/e2e-c73n2/run-1.trx      probe: Win32 1, Wpf 3             (건너뜀 1 = ReadableWindow…)

# 순진한 grep 이 세는 것 (쓰면 안 되는 이유)
cat build/c74/mock-*.trx | grep -o "framework=[A-Za-z0-9]*" | sort | uniq -c
  11 framework=Win32      ← 프로브 5 + 건너뛰기 메시지 + 노트 인쇄가 섞임
  81 framework=Wpf

git status --short   →  .claude/settings.json.bak-hook / .bak-rmask 2건만
```

## 6. 미검증 (Gaps)

- **CI 에서의 빈도는 재지 않았습니다.** 7 % 는 **이 기계**의 것이고, 이 저장소에는 로컬 대 CI 가
  1.91배 갈린 선례가 있습니다. **CI Native 기동에서 이 결함이 몇 %인지는 모릅니다.**
- **"CI Native 실행 1회가 결함을 만날 확률 약 25 %"** 는 **기동 독립 가정**에 기댄 추정입니다.
  기동들이 같은 머신·같은 세션에서 연달아 일어나므로 독립이 아닐 수 있습니다.
- **Native 배치가 20회(40 기동)로 작습니다.** 2/40 은 신뢰구간이 넓습니다.
- **후보 A~D 를 구현해 보지 않았습니다.** 숫자(재시도 3회 → 0.03 %)는 7 % 를 기동 독립으로
  둔 **계산이지 측정이 아닙니다.**
- **B 의 문자열 계약이 실제로 안 맞는 상황**을 만들어 보지 않았습니다.
- **`Counters.notExecuted` 가 0 으로 남는 xUnit 런타임 skip 의 성질**은 게이트 주석이 적은 것을
  그대로 읽었고, 이번에 다시 재지 않았습니다.
- **#30011 의 원인**은 이 카드에서 건드리지 않았습니다 — C-50/C-56 이 좁힌 "요소 생성 시점"
  그대로입니다.

## 7. 잔여 위험 (Residual risk)

- **지금 이 순간에도 결함은 세어지지 않습니다.** 어느 후보도 채택되기 전까지, CI Native 는
  **결함을 만나면 빨강, 안 만나면 초록**이고 그 둘이 같은 줄에서 나옵니다.
- **Mock 쪽은 게이트가 없어 건너뛰기가 판정에 안 잡힙니다** — 같은 결함이 Mock 에서는 조용히
  지나갑니다(§2).
- **C 나 D 를 문자열/형식에 의존해 만들면**, 형식이 바뀌는 날 **0 이 성공처럼 보입니다.**

## 부록 — 사용한 명령

```bash
git log -L '70,86:clients/ImageProcTest.E2ETests/Scenarios/Smoke/WindowReacquireTests.cs'
git show 6557c71 -- clients/ImageProcTest.E2ETests/Scenarios/Smoke/WindowReacquireTests.cs
sed -n '455,482p' .github/workflows/ci.yml            # (image-processing 체크아웃)
# trx 에서 프로브 줄만 골라 framework 집계 + 케이스별 outcome 집계
```
