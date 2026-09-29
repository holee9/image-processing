# GUI-C-155 (#225) — 세 명령을 **단언**합니다. Native 에서만

상태: **완료.** 시험 4건 추가(전체 163 → **167**), Native **6/6 통과**, Mock **4통과 / 2건너뜀**(사유 기재).

그리고 **제 전제가 두 번 틀렸고, 시험이 두 번 다 잡았습니다**(§3).

---

## 1. 전제를 제가 확인했습니다 — 줄 번호는 달랐습니다

카드 §1 이 `ci.yml:550` 을 인용했는데, 실제 위치는 **`ci.yml:454-455`** 입니다:

```
# session-0 style CI desktop - this job IS that experiment. Mock backend,
# so no native DLLs are needed; the Native variant follows GUI-C-26
```

**내용은 카드 그대로입니다** — Mock 잡에 네이티브 DLL 이 없습니다. 인용은 그 줄로 적었습니다.

## 2. 무엇을 단언하는가

| 시험 | 단언 | Native | Mock |
|---|---|---|---|
| `R06_PInvokeSmoke_AnswersFromTheNativeLibraries` | 스모크가 답하고, **목이 아니다** | 단언 | **건너뜀** |
| `R11b_StopWithNothingRunning_SaysSoAndDiscardsNothing` | 빈 상태에서 정확한 문구, **두 번 눌러도 같음**, 로그에 폐기 없음 | 단언 | **단언**(양쪽) |
| `R12_StageTiming_ShowsTheValuesTheRenderAlreadyReported` | 표시값이 **`ChainStatusText` 를 포함** | 단언 | **단언**(양쪽) |
| `A03_TheWiredCommands_ReportWhatTheyDid` | 스모크 True + 버전이 목과 다름 · 진행 중 정지 + `StoppedRenderCount ≥ 1` · **빈 상태 문구** · 시간 보고에 `chain:` | 단언 | **건너뜀** |

### 2.1 행 6 을 **버전**으로 단언한 이유

`"passed"` 만 읽으면 **조용히 목으로 떨어진 실행도 통과**합니다. 목 디스플레이는 `v0.0.0-mock-display` 를,
실제 DLL 은 `1.0.0` 을 냅니다 — **그 차이가 네이티브가 돌았다는 증거**이고, 그래서 단언을 거기에 걸었습니다.

### 2.2 건너뛰기 사유를 `Skip.If` 에 적었습니다

> *"The Mock job runs with no native DLLs (ci.yml:454-455), so a red smoke is CORRECT there. Asserting it anyway would make 'the DLLs are absent' and 'the DLLs are broken' the same green."*

`A-03` 쪽은 한 겹 더 좁혔습니다 — 환경의 `XPE_NATIVE_DIR` 이 **실재하는 디렉터리**를 가리킬 때만 돕니다.
백엔드 이름이 아니라 **필요한 것이 실제로 있는지**로 가르는 편이 정확합니다.

### 2.3 **행 11 의 "비어 있을 때" 를 두 번 누릅니다**

카드가 특히 지목한 경로입니다 — `GUI-C-154` 의 첫 구현이 **끝난 렌더를 진행 중으로** 봤고,
원인이 취소 소스를 안 비운 것이었습니다. 한 번만 누르면 그 결함이 다시 들어와도 첫 응답은 맞을 수 있으므로
**연속 두 번**이 같은 문구를 내는지 단언합니다.

## 3. 제 전제가 두 번 틀렸습니다 — 둘 다 시험이 잡았습니다

### 3.1 로그를 패널 열지 않고 읽었습니다

첫 실행에서 `R-06` 이 실패했는데, 앱이 아니라 **제 시나리오**가 원인이었습니다:

```
Assert.Contains() Failure: Sub-string not found
String:    ""
Not found: "xpe_display_version"
```

로그 목록의 자동화 id 는 `LogList` 가 아니라 **`LogListBox`** 이고, 패널이 닫혀 있으면 비어 읽힙니다.
`ClearAlertsObservationScenarios` 의 **검증된 `OpenLogs`** 를 그대로 가져왔습니다 —
`GUI-C-139` 에서 이 절차를 새로 만들었다가 0줄을 읽은 적이 있어, 재발명하지 않았습니다.

### 3.2 **"이 픽스처는 2.3~2.5초 렌더" 가 틀렸습니다**

`R-11a`(진행 중 정지)를 UI 로 만들면서 `GUI-C-153` 의 P-01 수치를 근거로 삼았습니다. **틀렸습니다.**

| 무엇 | 실측 |
|---|---|
| P-01 이 보고한 `total=2247~2473 ms` | **시험의 왕복 시간** — 메뉴 호출부터 새 처리본이 나올 때까지 |
| 실제 배경 작업 | `work=20 ms` (상태바가 스스로 보고) |

GSVG 단계를 켜도 마찬가지였습니다(2차 시도도 `Stop: no render is in flight.`).
**UI 로는 이길 수 없는 경주**입니다 — 메뉴 여는 시간이 렌더보다 깁니다.

그래서 그 단언을 **결정적인 자리로 옮겼습니다**: `AutomationReportBackendTests.A03`.
자동화 실행은 같은 명령을 인프로세스로 몰고, `ApplyDisplayPipelineAsync` 가 **첫 `await` 이전에**
취소 소스를 만들므로 다음 줄의 정지가 **항상** 진행 중에 닿습니다.

**시나리오 머리말의 틀린 수치도 그 자리에서 정정했습니다** — 주석이 코드를 거짓으로 말하게 두지 않습니다.

### 3.3 두 번 다 "느슨하게" 하지 않았습니다

`R-11a` 를 *"둘 중 하나면 통과"* 로 바꾸면 **정지가 동작하는 경우와 렌더가 이미 끝난 경우가 같은 초록**이 됩니다.
옮기거나 지우는 것이 맞고, 옮겼습니다.

## 4. 실측

### 4.1 새 시험 (Native)

```
통과 R12_StageTiming_ShowsTheValuesTheRenderAlreadyReported [7 s]
     chain='chain: preprocess=NotRequested, gsvg=NotRequested; times: preprocess=0 ms, gsvg=0 ms; display input=raw'
     status='Stage timing: work=20 ms; vm=1 ms; display: … || chain: … (같은 문자열 포함)'
통과 R11b_StopWithNothingRunning_SaysSoAndDiscardsNothing [7 s]
     first/second: 'Stop: no render is in flight.'
통과 R06_PInvokeSmoke_AnswersFromTheNativeLibraries [2 s]
     status: 'P/Invoke smoke: 3 of 3 probes answered.'
통과 A03_TheWiredCommands_ReportWhatTheyDid [11 s]
통과 A01 / A02 (기존)
→ 통과: 6, 실패 0
```

### 4.2 Mock

```
통과 R12, R11b, A01, A02
건너뜀 R06, A03   ← 사유 기재
→ 통과: 4, 건너뜀: 2, 실패 0
```

### 4.3 전체 회귀

| | 전체 | 통과 | 실패 |
|---|---|---|---|
| `C-154` 시점 | 163 | 162 | 1 (`P-10`) |
| **이 카드 뒤** | **167** | **166** | **1 — 같은 `P-10`, 같은 해시 `36fc547e253b07f1`** |

**새 시험 4건이 그대로 늘었고 다른 변화는 없습니다.**

## 5. 바꾼 것

| 파일 | 변경 |
|---|---|
| `Scenarios/Workflows/MenuCommandScenarios.cs` | **신규** — R-06 · R-11b · R-12 |
| `Scenarios/Smoke/AutomationReportBackendTests.cs` | `A03` 추가 + `NativeDirectoryOrSkip` 헬퍼 |

**앱은 건드리지 않았습니다** — 이 카드는 단언을 붙이는 카드입니다.

## 6. 행 15·16 — 재귀 설계를 다음에 보고합니다

카드 §3 대로 §1·§2 를 끝냈습니다. 재귀 설계는 별도 보고로 냅니다 — 지금 초안 수준의 생각은
*"러너를 부르는 경로에 표식을 두고, 표식이 있으면 앱이 러너를 띄우지 않는다"* 이지만,
**환경 변수인지 인자인지, CI 의 어느 잡에서 무엇이 보이는지 확인하지 않았습니다.** 확인 전에는 설계가 아닙니다.

## 7. 미검증 / 잔여 위험

- **CI 에서 안 돌았습니다** — 전부 로컬. **로컬 실측, CI 상한 추정**
- ~~`A-03` 이 CI 에서 건너뛸 수 있다~~ → **확인했습니다: 건너뛰지 않습니다.** `ci.yml:611-613` 이
  `gui-e2e-native` 에 `XPE_E2E_BACKEND: Native` · `XPE_NATIVE_DIR: …/build/e2e-native-dlls` ·
  `XPE_NATIVE_DIR_EXCLUSIVE: "1"` 을 겁니다. 그 잡에서 `A-03` 과 `R-06` 은 **단언으로 돕니다.**
  (처음엔 미검증으로 적었다가 그 자리에서 확인했습니다 — `#205` 형태가 될 뻔한 자리였습니다)
- `R-11b` 의 *"로그에 폐기 없음"* 은 **마지막 6줄**만 봅니다. 그 앞에 있으면 놓칩니다
- 행 12 단언은 `ChainStatusText` **포함**만 봅니다. `work=` 쪽 수치가 상태바의 다른 표시와 같은지는 대조하지 않았습니다
- `P-10` 로컬 전용 판정은 리더의 CI 관측 인용입니다

---

Refs #225
