# GUI-C-229d — 판정 기준은 원래 R02 그대로, 시간은 기록만 (Codex #133)

앱 코드는 고치지 않았다. 바뀐 것은 E2E 프로젝트의 `RealInputScenarios.cs`, `Fixtures/KeyLossDiagnosis.cs`, `KeyLossDiagnosisTests.cs` 이다.

## 판정은 원래 R02 와 같다

- 타이밍 실패 게이트를 없앴다(`JudgeTiming`, 창 상수 `JudgmentSlackMs`·`JudgmentEarlyToleranceMs` 삭제). 시간으로 통과/실패를 바꾸는 코드가 없다. 시험 하나(`TheClassWorksWithNoTimingGate`)가 그 이름들이 다시 생기지 않음을 단언한다.
- 판정 기준: 직전 전경 확인(아니면 키를 보내지 않고 실패, 안전장치로 유지) → `SelectAll` → `Type("4321")` → 600 ms 대기 → Center 읽기 → `== "4321"`. 이 순서와 기준은 229c 와 같고 원래 R02 의 기준과 같다.
- **명시**: 판정 기준은 원래 R02 와 같다. 늦은 읽기가 손실을 가릴 수 있다는 한계(읽기가 늦게 시작하거나 늦게 끝나면 상자가 그동안 키를 처리했을 수 있다)는 원래 R02 에도 있던 것이며, 기록된 완료 시각으로 사후에 판별한다. 늦게 완료된 통과 실행은 기록으로 보인다.

## 시간은 기록만

- 판정 읽기의 **시작**과 **완료** 시각(마지막 키 기준 ms)을 매번 한 줄로 남긴다: `R02 judgment read: started +N ms, completed +M ms after the last key (nominal wait 600 ms; the verdict does not depend on these times)`. 통과한 실행에도 남고(`output.WriteLine`), 실패 메시지에도 같은 줄과 `FACTS` 안의 구간(`judgment read +N..+M ms`)이 들어간다.
- 완료 시각은 읽기가 값을 돌려준 **뒤** 에 잰다(`SafeText` 가 UIA 응답을 받은 뒤). Codex 재현(시작은 604 ms 인데 UIA 지연으로 1104 ms 에 완료)이 기록에 찍힌다.
- 이 수치는 통과/실패에 쓰이지 않는다.

## R02e 를 바꿨다

`R02e_ALateJudgmentRead_DoesNotChangeTheVerdict_ButTheRecordShowsTheLateCompletion`: R02 가 쓰는 `TakeJudgment` 에 키를 보내지 않고 세 경우를 넣는다. 같은 값이 든 상자를 읽으므로 판정에 쓰이는 값(`Center`)은 세 경우 모두 같다.

| 경우 | 기록 (실제 창에서 한 번 실행) |
|------|------------------------------|
| 지연 없음 (대조군) | started +613 ms, completed +616 ms |
| 읽기 앞 800 ms 지연 | started +1415 ms, completed +1416 ms |
| 읽기 **안** 500 ms 지연 (Codex 재현) | started +606 ms, completed +1123 ms |

단언: 세 값이 같다(판정 불변), 기록의 시작·완료가 지연만큼 늦게 찍힌다, 대조군은 제때라 비교가 의미를 가진다. 전문은 `r02d_r02e_real_window.txt`.

## 반증 (넣은 채로 커밋하지 않음, 원본 복구 확인; `falsification_*.txt`)

| 팔 | 한 일 | 결과 |
|----|-------|------|
| (a) | 기록 줄을 지움(`JudgmentLine` 이 빈 문자열) | 기록 시험 + R02e 빨강 (2건) |
| (b) | 완료 시각을 읽기 뒤에 재지 않음(완료 = 시작) | R02e 빨강 |
| (c) | 시작 시각을 읽기 앞 지연보다 앞에서 잼 | R02e 빨강 |

## 그대로인 것

분류(Width 입력 전/후 비교, `Unclassified`, FACTS/READING 분리, 전경은 사실만), 입력 직전 전경 확인과 거절, 판정 뒤 진단 관측, 실패 때 1.5 s 재읽기는 229c 그대로다. 단언의 엄격함도 같다.

## 새 R02 를 실제 키 입력으로 돌려 보았나

**돌리지 않았다.** R02d 메시지가 이번에도 전경으로 PickerHost 의 "Windows 보안" 창(pid 47456)을 보고한다(`r02d_r02e_real_window.txt`). 새 R02 의 실제 키 경로는 컴파일만 되었고 한 번도 실제 입력으로 돌지 않았다.

## 확인

| 항목 | 결과 |
|------|------|
| 분류·기록 시험 + R02d + R02e (E2E 프로젝트) | 통과 20/20 (R02 자체는 게이트로 건너뜀) |

## 관측한 사실과 추정

| 구분 | 내용 |
|------|------|
| 관측한 사실 | 입력 없이 호출한 `TakeJudgment` 의 기록: 지연 없음 +613/+616 ms, 읽기 앞 지연 +1415/+1416 ms, 읽기 안 지연 +606/+1123 ms; "Windows 보안" 창이 아직 전경 |
| 추정(근거 없음) | 이 기록으로 다음 간헐 실패가 "늦게 읽음"인지 사후에 가를 수 있다는 것. 실제 CI 실패에서 기록이 어떻게 찍힐지는 아직 모른다 |

## 미검증 / 잔여 위험

- 새 R02 의 실제 키 입력 경로는 처음 도는 것이 CI 일 가능성이 크다.
- 늦은 읽기가 손실을 가리는 한계는 그대로 남는다. 이 시험은 그것을 막지 않고 보이게만 한다.
- 직전 관측(UIA 읽기와 전경 확인)이 `Focus()` 와 Ctrl+A 사이에 들어가 있는 영향은 측정하지 않았다. 이 변경 뒤의 초록을 이전 초록과 같은 것으로 세지 말 것.
