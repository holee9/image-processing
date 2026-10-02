# GUI-C-192e — Codex #62 보류 5건 + 193 출하 게이트

## 0. 먼저 정정할 것 (내 오류)

- **192d 보고서 §2-2 는 틀렸다.** "AI 미요청이면 모듈에 묻기 전에 즉시 답한다"고 `GuiAiRunner.cs` 161~165행을 인용했지만, 그 코드는 `WithLock` **안**이었다. 게이트가 잡혀 있으면 "시작 안 함"도 기다렸다(Codex #62 중간 5). 이번에 게이트 밖으로 옮겼다(§5).
- **192d §5 의 공백**("읽기가 예외로 끝나 `null` 을 돌려주는 경우는 알림 대상이 아니다")이 Codex 높음 2 와 같은 것이다. 보고서의 공백을 카드로 올리는 일이 내게 빠져 있었다. 이번 메시지 본문에 §6 으로 적는다.

## 1. 높음 1 — 재시작 전 Disabled 가 새 세션에 뜰 수 있음

**원인(확인함).** `RestartAiSession` 은 `AiStatus.Reset()` 을 부르지 않았다. 같은 백엔드·같은 세대라서, 재시작 전에 시작된 읽기의 "switched off" 답이 192b 의 "같은 세대 Disabled 는 항상 적용" 규칙으로 새 세션에 표시됐다.

**고침.**
1. `RestartAiSession` 이 **시작하자마자** `AiStatus.Reset()` (세대를 올리고 `Unknown`) — `await` 전에.
2. 세션 대수(`AiSessionTracker.Epoch`, `IAiSessionBackend.AiSessionEpoch`): 세션이 끝날 때(`Stopped`: 종료·재시작·다른 폴더)와 없다가 시작할 때 올라간다. 읽기 시작 시 대수를 기억하고 완료 시 달라졌으면 답을 버리고 다시 읽는다. `Reset` 을 부르지 않는 경계(프레임 안의 재초기화)도 막기 위한 것이다. 같은 세션을 가리키는 반복 `init` 은 세지 않는다(모듈이 무시하고, 세면 읽기가 계속 버려진다).

**경계 전수**(세션이 바뀌는 곳, 실제 배선에서 `Reset`/대수가 닿는지):

| 경계 | 코드 | Reset | 대수 |
|---|---|---|---|
| 백엔드 교체 | `InitializeBackend` (`AiStatus.Reset()`) | 있음 | (새 백엔드: 객체 동일성) |
| 종료 | `BeginShutdown` (`AiStatus.Reset()`) | 있음 | `Stopped` 가 올림 |
| **Restart AI** | `RestartAiSession` | **없었음 → 추가** | `Shutdown`→`Stopped`, `Init`→시작 |
| 프레임의 폴더 변경 재초기화 | `GuiAiSession.InitCore` → `Shutdown()` | 없음(VM 이 모름) | **대수가 막음** |
| init 실패 뒤 재시도 | 같은 `InitCore` | 없음 | 실패는 상태를 안 바꿈, 직전 `Stopped` 가 올렸음 |

**시험(실제 뷰모델 명령).** SelfCheck 시나리오 8(`RestartAiSessionCommand` 를 실행 → 지연된 옛 Disabled 를 그 뒤에 풀어 줌 → 화면에 Disabled 가 한 번도 안 뜸), 8b(대수만 올리고 `Reset` 없이 → 같은 결과). 단위 `ADisabledAnswer_ReadUnderAnEarlierSession_IsDropped_EvenWithNoReset`. SelfCheck 종료 코드 0.

## 2. 높음 2 — 첫 읽기가 실패하면 알림·재시도가 멈춤

`null`(예외로 끝난 읽기)을 **미확인 시도**로 기록한다(`_lastReadFailed`, 측정 `failedReads`). 정상으로 돌아온 `Unknown`(AI 미요청)은 답이라 재시도·알림 대상이 아니다. 실패한 읽기는 상한의 1/3(5초)마다 다시 읽고(폭주 없음), 첫 미답 시작부터 센 15초에 `NeverConfirmed` 알림이 뜬다(재시도마다 새로 세지 않음). 시험 `AFirstReadThatFails_IsRetried_AndRaisesTheNoticeWhenTheFailuresGoOn`(재시도 간격, 15초 시점, 읽기 복구 시 해제, 답한 `Unknown` 은 재시도 안 함).

## 3. 높음 3 — 기본 시계 long 곱셈 넘침

`MonotonicClock`: 시작 시점의 카운터를 기억하고 **차이만** 변환한다(`차이 / 주파수 × TicksPerSecond + 나머지 × TicksPerSecond / 주파수`). 카운터 자체에 곱하지 않고, 영점을 가정하지 않는다. 시험은 카운터를 옛 식의 한계(922,337,203,685, 10 MHz 에서 약 25.6시간) 4초 아래에서 시작해 20초를 1초씩 올린다: 1초 단위 정확한 경과, 단조 증가, 반초 단위, 나누어떨어지지 않는 주파수. 시험 자체가 옛 식이 그 지점에서 음수가 됨을 단언한다(그렇지 않으면 시험이 아무것도 보여 주지 못한다). 5초 재읽기·15초 알림 판정도 경계 양쪽에서 시험(`TheDecisions_AreRightAcrossTheCounterBoundary`).

**같은 패턴 전수**(`census.txt`, 대조군 포함): `GetTimestamp` 사용은 gui·clients 에서 3곳. 이 시계(수정됨) 외에 `ImageComparisonViewport.cs:258,383` 는 `(차이) * 1000.0 / Frequency` — **차이에 double 곱셈**이라 넘치지 않는다. 대조군: 같은 검색이 시험 파일의 의도된 `TicksPerSecond` 곱셈을 찾는다.

## 4. 중간 4 — 15초는 UI 정체 시 표시 상한이 아니다

맞다. 판정은 1초 `DispatcherTimer`(Background)에서 돌아 UI 가 스케줄될 때만 평가된다. 15초는 **답의 나이에 대한 기준**이지 알림이 화면에 닿는 시각의 보장이 아니다. 코드 주석(`CheckFreshness`, VM 타이머)과 이 보고서를 그렇게 고쳤다. 측정: 진단 줄에 `maxUiGapMs`(두 번의 확인 사이 가장 긴 간격 = UI 정체)를 추가했다(시험 `TheLongestGapBetweenChecks_IsMeasured`, M02 로그 `maxUiGapMs=1036`: Mock 정상 실행의 최대 간격).

**UI 를 15초 넘게 막는 경로**: 앱 코드의 동기 대기(`Wait`·`.Result`·`GetResult`·`Thread.Sleep`·`WaitOne`)를 검색했다(`census.txt`): 시험용 결함의 백그라운드 읽기 하나뿐, UI 스레드 대기는 없다. **이것은 UI 스레드의 긴 동기 계산이나 네이티브 호출까지 없다는 증거가 아니다** — 그것은 검색하지 않았고 모른다. 그래서 측정(`maxUiGapMs`)을 C-09 판독 항목으로 둔다.

## 5. 중간 5 — 첫 지연 로딩의 거짓 NeverConfirmed

- `QueryWorkerState` 가 `OwnStatus()` 를 **게이트 밖에서 먼저** 본다(그리고 게이트 안에서 다시 확인). 잠금 순서 변경의 안전성: 추적기 필드(`_started`, `_initFailure`, `_startedDirectory`)는 게이트 안에서만 쓰이고 `volatile` 로 바꿨다(게이트 밖 읽기가 쓰기를 본다). 게이트 밖에서 "아직 시작 안 함"을 읽는 순간에 프레임이 막 시작을 끝냈어도 결과는 "아직 안 함"의 과거 사실이고, 다음 읽기가 바로잡는다. 진단 문구의 `_initDiagnostics` 도 `volatile`.
- **이것으로 끝나지 않는다(미해결, 정직하게).** 세션이 시작된 뒤에는 `_started` 가 참이라 모듈에 물어야 하고, 첫 추론(모델이 첫 호출에서 지연 로딩, `ai.cpp` 101행)이 게이트를 15초 넘게 쥐면 읽기는 여전히 기다린다. 이 코드만으로는 느린 로딩과 무응답을 구별할 수 없다. 그래서 **문구를 관측 사실로 바꿨다.**
- **문구 변경(레인 간 계약 — 알림 문자열).** 192c·192d 둘 다:
  - 마지막 답이 낡음: `AI worker status check delayed: the last answer is over 15 s old. The AI may be busy; AI results may not be applied. Use Restart AI if this stays.`
  - 시작·재시작 뒤 한 번도 답 없음: `AI worker status check delayed: no answer since the AI session started or was restarted (over 15 s). The AI may be busy; AI results may not be applied. Use Restart AI if this stays.`
  옛 문구("status unknown … not confirmed")는 워커 장애를 암시했다. 새 문구는 "확인이 늦어짐"과 "AI 가 바쁠 수 있음"을 말한다. 이 문자열을 앵커로 쓰는 다른 레인의 시험이 있는지는 이 저장소 검색(`clients/`)에서만 확인했다(내 시험·E2E 뿐, 갱신함). 다른 레인 저장소는 보지 않았다 — **리더가 확인해 달라.**

## 6. 193 출하 게이트

(a) `gui/XpeTestFaults.props` 에 `Target XpeTestFaultsRefusedInRelease`(`BeforeTargets="CoreCompile"`, 조건 `XpeTestFaults == true And Configuration == Release`)와 `<Error Code="XPE0001">` 추가. 관측(`release_gate.txt`):

| 구성 | 결과 |
|---|---|
| Release + `-p:XpeTestFaults=true` (앱) | 빌드 실패, 오류 `XPE0001` |
| Release 기본 | 성공 |
| Debug + `-p:XpeTestFaults=true` | 성공 |
| SelfCheck Release + `true` | 실패 |

소스 시험 `TheBuildSymbol_IsDefinedOnlyForDebug…` 가 타깃·조건·오류 코드를 단언한다.

(b) **ci.yml diff 초안**(`draft_ci_job.diff`, 새 잡 `gui-shipped-build`: Debug 대조군 빌드 → Release 빌드 → dll·pdb·xml 에 금지 문자열 없음 → Release+`true` 가 거부됨 → 출하 exe 가 각 스위치에 종료 코드 2). **검사 스크립트 초안**(`draft_check_shipped_build.ps1`, `tools/check_shipped_build.ps1` 로 둘 것)을 이 트리에서 실제로 돌려 종료 코드 0 을 확인했다(`draft_check_run.txt`): 대조군 "Debug 가 12개 문자열을 가진다"가 먼저 통과해야 Release 의 "없음"이 의미를 갖는다. 스위치 4개(`ai-worker-disabled`, `ai-worker-silent`, `ai-worker-silent:0`, `display-pipeline-after:2`) 모두 출하 exe 가 종료 코드 2 로 거부한다. 이 잡이 **CI 에서** 돌아 보지는 못했다.

## 7. 반증 (`falsification_arms.txt`, 규칙마다 제거 → 가드 빨강, 원본 바이트 동일 복원, 복원 뒤 Functional 398/0·SelfCheck 0)

| 지운 것 | 빨강 |
|---|---|
| 재시작의 `Reset` | **SelfCheck 시나리오 8** (옛 Disabled 가 화면에 뜸). 단위는 초록 — 실제 배선은 SelfCheck 만 본다 |
| 대수 확인 | 단위 1건 + **SelfCheck 8b** |
| 실패한 읽기 추적 | 단위 2건 |
| 시계를 옛 곱셈으로 | 단위 2건 |
| 게이트 밖 자체 답 | 소스 시험 `TheRestart_IsOneStepUnderTheLock` (**처음엔 초록이었다**: 순서만 보던 시험이 `false &&` 로 꺼진 검사를 못 잡았다. `if (early is not null)` 존재까지 단언하도록 고친 뒤 빨강. 이 가드는 **소스를 읽는 시험이지 동작 시험이 아니다**: 네이티브가 필요해 로컬에서 동작을 못 본다) |
| UI 정체 측정 | 단위 1건 |
| 출하 게이트(props 타깃) | 소스 시험 + **초안 스크립트(Release+true 가 성공해 거부되지 않음)** |

## 8. 실행 결과

Functional 398 통과 / 0 실패 / 1 건너뜀, SelfCheck 종료 0(시나리오 8·8b 포함), E2E M01·M02·M03 통과(새 문구로; M02 진단 줄 `refresher: reads=2 … failedReads=0 retries=0 maxUiGapMs=1036 …`), Release dll·pdb·xml 에 시험용 이름 없음(Debug 대조군 발견).

## 9. 남은 공백 (Gaps)

- **첫 추론의 지연 로딩이 15초를 넘으면 알림이 뜬다**(정상 동작인데). 문구는 이제 "느릴 수 있음"이라 거짓말은 아니지만, 사용자에게는 경고다. 다음 Native CI 의 C-09 로그 `noticesNeverConfirmed`·`maxReadMs` 가 이를 판정한다. Native 로 실측하지 못했다(#98).
- 15초·5초 값은 여전히 추정이다.
- UI 스레드의 긴 동기 계산·네이티브 호출은 검색하지 않았다(§4).
- 게이트 밖 자체 답의 동작은 소스 시험으로만 지킨다.
- CI 잡(`gui-shipped-build`)은 초안이며 CI 에서 돌려 보지 않았다.
- 다른 레인 저장소의 알림 문구 앵커는 보지 않았다.

## 10. 잔여 위험 (Residual-risk)

- 재읽기 5초는 `Active` 인 동안만 돈다. 실패한 읽기의 재시도도 5초 간격.
- 대수는 같은 디렉터리로의 반복 `init` 을 세지 않으므로, 모듈이 안에서 세션을 몰래 갈아 끼우는 경우(이 코드가 아닌 곳)는 못 본다.
- `Disabled` 우선 표시 규칙(192b)은 같은 세대·같은 백엔드·같은 대수 안에서만 성립한다.
