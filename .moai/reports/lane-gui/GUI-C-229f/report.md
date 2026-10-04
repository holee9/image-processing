# GUI-C-229f — PID 를 모르면 "전경" 이 아니다 (Codex #136)

앱 코드는 고치지 않았다. 바뀐 것은 E2E 프로젝트다: 새 `Fixtures/ProcessIdentity.cs`, 새 `ProcessIdentityTests.cs`, 그리고 `RealInputScenarios.cs`·`KeyLossDiagnosis.cs` 의 비교 몇 줄.

## 무엇이 틀렸었나

`AppIsForeground` 는 `GetWindowThreadProcessId` 가 쓴 전경 PID 와 앱의 UIA `ProcessId.ValueOrDefault` 를 `==` 로 비교했다. 읽기가 실패하면 둘 다 0 이고(`GetWindowThreadProcessId` 는 실패하면 0 을 돌려주고 PID 를 쓰지 않으며, UIA 의 기본값도 0), `0 == 0` 이 "앱이 앞"으로 읽혀 `SelectAll`·`Type("4321")` 이 다른 창으로 갈 수 있었다. 7건의 단계 시험은 전경 위임을 bool/예외로 대체해 이 PID 판정 자체를 시험하지 않았다.

## 순수 함수

`ProcessIdentity.ForegroundIsTheApplication(전경 핸들, 스레드 id, 전경 PID, 앱 PID)`: 앱 PID > 0, 전경 핸들 ≠ 0, 스레드 id ≠ 0, 전경 PID > 0, 그리고 전경 PID == 앱 PID 일 때만 참. 나머지는 모두 거짓이고 호출자는 키를 보내지 않고 실패한다. `AppIsForeground` 는 Win32 로 네 값을 읽어 이 함수에 넘기기만 한다. 전경 핸들이 없으면 `GetWindowThreadProcessId` 를 부르지 않는다.

`ProcessIdentity.SameKnownProcess(PID, 앱 PID)`: 둘 다 알려지고(> 0) 같을 때만 참. UIA 가 준 PID 를 앱 PID 와 비교하는 곳에 쓴다.

## 같은 비교를 쓰던 곳 (grep: `grep_process_id_comparisons.txt`)

| 위치 | 변경 |
|------|------|
| `RealInputScenarios.AppIsForeground` (R02 의 키 직전 확인, R01 의 클릭 전 확인 `MakeTheAppTheForegroundWindow`) | `ProcessIdentity.ForegroundIsTheApplication` 으로. R01 과 R02 는 같은 함수를 쓰므로 둘 다 바뀐다 |
| R02 의 Tab 직전 포커스 확인(`beforeTab.ProcessId == appProcess`)과 Tab 뒤 확인(`afterTab.ProcessId == appProcess`) | `SameKnownProcess` |
| `KeyLossDiagnosis` 의 전경·포커스 PID 비교 3곳(전경 사실 문장, 전경이 다른 프로세스인지, 포커스가 앱 안인지) | `SameKnownProcess`. 모르는 PID 는 "다른/모름"으로 보고 `Unclassified` |
| `MenuBehaviorScenarios.WindowsOfTheApp` 의 `e.ProcessId.ValueOrDefault == pid` | 바꾸지 않았다: 창 목록을 문자열로 보여 주는 진단 도우미이고 키·클릭·포커스 판단에 쓰이지 않으며, `pid` 는 `ProcessId.Value`(읽지 못하면 예외) 로 얻는다 |

위 grep 에 남은 `==` 비교는 `ProcessIdentity` 안의 두 줄과 위 `MenuBehaviorScenarios` 한 줄뿐이다.

## 시험

`ProcessIdentityTests`: 판정 11행 + 같은 알려진 프로세스 6행 + 진단 문장 1건. Codex 가 든 경우를 모두 담았다: 두 PID 0 / Win32 스레드 id 0 / 전경 핸들 0 / 앱 PID 0 + 전경 PID 정상 / 정상 일치 / 정상 불일치, 그리고 음수 앱 PID(`-1` 은 부호 없는 변환에서 `0xFFFFFFFF` 가 되어 단순 비교로는 일치로 읽힌다)도. 관련 묶음(`ProcessIdentity`·`KeystrokeStep`·`KeyLossDiagnosis`·R02 계열, R02 자체는 게이트로 건너뜀)은 `e2e_helper_tests.txt` 에 기록했다.

R02f(실제 창, 키 없음): 전경 확인이 독립적으로 읽은 전경 프로세스와 일치한다(`전경 PID > 0 이고 앱 PID 와 같을 때만 참`). 이 환경에서는 앱 pid 27404, 전경 pid 47456("Windows 보안" 창)이고 `AppIsForeground=False` 였다(`r02f_real_window.txt`). 앱이 전경일 때의 일치는 이 환경에서 관측하지 못했다.

## 반증 (넣은 채로 커밋하지 않음, 원본 복구 확인; `falsification_*.txt`)

| 팔 | 한 일 | 결과 |
|----|-------|------|
| (a) | "앱 PID > 0" 조건을 뺌 | 빨강 1건(`-1` / `0xFFFFFFFF` 행) |
| (b) | "스레드 id ≠ 0" 조건을 뺌 | 빨강 1건 |
| (c) | "전경 핸들 ≠ 0" 조건을 뺌 | 빨강 1건 |
| (d) | "전경 PID > 0" 조건만 뺌 | **초록** — 이 조건은 "앱 PID > 0 + 두 PID 일치"가 이미 함의한다 |
| (d2) | (a)와 (d)의 두 PID 조건을 함께 뺌 | 빨강 2건 — 두 조건은 각각으로는 서로 가려지고 함께 빼야 보인다 |
| (e) | `SameKnownProcess` 를 단순 비교로 되돌림 | 빨강 3건 |

(d) 가 초록인 것은 시험이 약해서가 아니라 조건이 논리적으로 중복이기 때문이다. 카드가 적은 5개 조건을 읽기 쉽도록 모두 남겼다.

## 새 R02 를 실제 키 입력으로 돌려 보았나

**돌리지 않았다.** R02f 가 이번에도 전경으로 PickerHost 의 "Windows 보안" 창(pid 47456)을 보고한다. 새 R02 의 실제 키 경로는 위임으로만 시험되었고 실제 입력으로는 한 번도 돌지 않았다. 앱이 전경인 상태에서 `ForegroundIsTheApplication` 이 참을 돌려주는 실제 경로는 이 환경에서 관측하지 못했다.

## 관측한 사실과 추정

| 구분 | 내용 |
|------|------|
| 관측한 사실 | 순수 함수 표가 위와 같이 동작; (d) 가 초록, (d2) 가 빨강; 실제 창에서 `AppIsForeground=False` 와 독립 읽기가 일치(앱 27404, 전경 47456) |
| 추정 | 앱이 전경일 때 실제 Win32 값 네 개가 모두 알려져 참이 된다는 것(이 환경에서는 관측 못 함) |

## 곁에서 본 것: 오래된 바이너리 가드가 한 번 빨갛게 되었다

`R02` 필터가 레거시 `LegacyPreprocessReadinessScenarios.R02_AllThreeTabsBlockPreprocess…` 도 골라 돌렸고, 그 시험이 `AssertFresh`(레거시 앱 dll 이 모든 소스보다 새로워야 함)에서 빨갛게 나왔다. 두 번 다시 돌려도 같았다. 원인은 이번 변경이 아니라 GUI-C-228f 의 반증이 링크된 앱 소스 `clients/ImageProcTest/Diagnostics/XpePreprocessOracleProcess.cs` 를 잠깐 고쳤다가 같은 바이트로 되돌린 것이다: 내용은 같지만 수정 시각이 dll 보다 새로워 가드가 걸렸다(`git status` 에 앱 소스 변경 없음). 앱 소스는 바꾸지 않고 `dotnet build clients/ImageProcTest/ImageProcTest.csproj -c Debug` 로 dll 을 다시 만든 뒤 같은 묶음이 통과한다(`e2e_helper_tests.txt`: 통과 47, 건너뜀 2 = R01·R02 의 전역 입력 게이트).

## 미검증 / 잔여 위험

- 앱이 실제로 전경일 때의 참 경로, 그리고 새 R02 의 실제 키 입력 경로는 CI 가 처음 돌 가능성이 크다.
- `GetWindowThreadProcessId` 가 반환하는 스레드 id 가 0 이 아닌 정상 값으로 돌아오는 것은 실제 창에서 거짓 경로만 관측했다.
- 이 변경 뒤의 초록을 이전 초록과 같은 것으로 세지 말 것(직전 UIA 읽기 영향은 측정하지 않았다).
