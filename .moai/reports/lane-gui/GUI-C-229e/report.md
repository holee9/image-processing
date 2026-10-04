# GUI-C-229e — 진단 조회가 판정을 막지 않게 (Codex #134)

앱 코드는 고치지 않았다. 바뀐 것은 E2E 프로젝트의 `RealInputScenarios.cs`, `Fixtures/KeyLossDiagnosis.cs`, 새 `Fixtures/KeystrokeStep.cs`, 새 `KeystrokeStepTests.cs` 이다.

## 구조

R02 가 "포커스 확인 뒤 4321 이어야 한다"까지 하는 단계를 `KeystrokeStep.Run` 으로 옮겼다. 읽기와 키 전송은 위임(`Seams`)으로 받으므로 입력도 앱도 없이 순서와 격리를 시험할 수 있다. 시나리오는 실제 창과 실제 키를 위임으로 넘긴다.

순서(원래 R02 와 같다): 입력 전 관측(격리) → **전경 확인(격리하지 않음)** → `SelectAll` + `Type("4321")` → 600 ms → Center 읽기(판정) → **시간 기록 출력과 판정 값 확정** → 진단 관측(격리) → 판정이 어긋났을 때만 1.5 s 재읽기(격리) → 단언.

- **진단은 판정에 끼어들지 않는다** (Codex 1·2): 판정 읽기의 시작·완료 기록은 Width·포커스·전경 조회보다 **먼저** 출력되고 Center 값은 먼저 확정된다. Width 찾기·읽기, 포커스, 전경, 1.5 s 재읽기, "앞에 무엇이 있는가" 설명이 각각 최선 노력 블록(`Reading.Take`, `Safely`) 안에서 일어나며, 예외면 FACTS 에 `diagnostic read failed: <예외 형식>: <메시지>` 가 그 자리를 대신한다. 판정과 기록은 그대로다.
- **입력 전 관측도 같은 방식으로 격리** (Codex 3): Width·포커스 조회가 실패해도 키 입력을 막지 않는다.
- **전경 확인 안전장치는 격리하지 않는다** (Codex 3): 앱이 전경이 아니거나, 전경을 확인하지 못해(예외 포함) 키를 보내지 않고 실패한다. 이 실패 메시지는 "could not be checked … NO key was sent" 또는 "was not the window in front … NO key was sent" 이다.
- 분류는 진단 읽기가 없으면 `Unclassified` 이다(판정 값 `Center` 는 진단이 아니라 항상 있다). 분류의 나머지(Width 입력 전/후, FACTS/READING, 전경은 사실만)는 229c 그대로다.

## 시험 (입력·앱 없이 위임으로)

`KeystrokeStepTests` 7건 + 기존 분류·기록 시험. 모두 통과(E2E 27/27, R02d·R02e 포함).

| 카드 항목 | 시험 | 단언 |
|-----------|------|------|
| (a) Width 조회 예외 + Center `4321` | `A_…` | 실패 없음(통과) + 시간 기록이 먼저 나옴 + FACTS 에 "diagnostic read failed: InvalidOperationException: …" + 기록이 진단 읽기보다 먼저 일어남(이벤트 순서) |
| (b) Width 조회 예외 + Center `43` | `B_…` | 원래 손실 실패 + 메시지에 기록 줄·"diagnostic read failed"·`Unclassified` |
| (c) 입력 전 Width 조회 예외 | `C_…` | 키는 보내짐(`keys-sent`), 실패 없음 |
| (d) 전경 확인 예외 | `D_…` | 키 안 보냄, "could not be checked"·"NO key was sent", 기록 없음. 앱이 전경이 아닐 때도 같은 시험 |
| 그 밖 | `TheReReadAndTheDescription…`, `ThePassingPath_…` | 재읽기·전경 설명 예외도 격리됨; 통과 경로의 이벤트 순서가 `observe-before, front-check, keys-sent, judgment, log, observe-after, log` |

반증 (넣은 채로 커밋하지 않음, 원본 복구 확인; `falsification_*.txt`):

| 팔 | 한 일 | 결과 |
|----|-------|------|
| (a) | 진단 읽기의 예외 처리를 뺌 | 4건 빨강 (a·b·c 와 재읽기 격리 시험) |
| (b) | 전경 확인의 예외를 "앱이 전경"으로 처리(안전장치를 격리) | (d) 예외 시험 빨강 |
| (c) | 시간 기록을 진단 읽기 뒤로 옮김 | (a) 시험과 통과 경로 순서 시험 빨강 |
| (d) | 입력 전 관측을 격리하지 않음 | (c) 시험 빨강 |

## R02d·R02e

R02d 는 이제 같은 `KeystrokeStep.Run` 을 실제 창으로 호출한다(키는 보내지 않는 `SendKeys` 와 항상 참인 전경 위임으로, 실제로 보낼 키가 없으므로 다른 창으로 갈 수 없다). 메시지의 모든 절이 있고 기록이 맨 앞에 나옴을 확인한다. R02e 는 229d 그대로 통과한다: 지연 없음 +600/+603 ms, 읽기 앞 800 ms 지연 +1405/+1406 ms, 읽기 안 500 ms 지연 +614/+1122 ms(`r02d_r02e_real_window.txt`).

## 새 R02 를 실제 키 입력으로 돌려 보았나

**돌리지 않았다.** R02d 가 이번에도 전경으로 PickerHost 의 "Windows 보안" 창(pid 47456)을 보고한다(같은 파일, `foreground pid 47456` 세 번). 새 R02 의 실제 키 경로 — 이번에 `KeystrokeStep.Run` 으로 옮긴 부분 — 는 컴파일되고 위임으로 시험되었지만 실제 키 입력으로는 한 번도 돌지 않았다.

## 관측한 사실과 추정

| 구분 | 내용 |
|------|------|
| 관측한 사실 | 위임으로 주입한 예외 4종에서 판정·기록·키 전송이 위 표대로 동작; 실제 창에서 R02d·R02e 통과; "Windows 보안" 창이 아직 전경 |
| 추정 | 실제 키 경로가 위임으로 시험한 것과 같이 동작한다는 것(실제 `GlobalInput`·UIA 와 연결된 상태로는 미실행) |

## 미검증 / 잔여 위험

- 시나리오가 `Seams` 를 만드는 부분(람다 연결)은 실제 입력으로는 실행되지 않았다. R02d 가 같은 연결을 키 없이 쓰므로 읽기 쪽 연결은 실제 창에서 확인되었고, `SendKeys`(`SelectAll`+`Type`) 연결과 전경 위임 연결은 확인되지 않았다.
- 진단 읽기를 격리하는 `catch (Exception)` 은 모든 예외를 삼키므로, 진단 코드의 버그가 메시지의 "diagnostic read failed" 로만 보인다. 판정에는 영향이 없다.
- 직전 관측(UIA 읽기)이 `Focus()` 와 Ctrl+A 사이에 들어가 있는 영향은 측정하지 않았다. 이 변경 뒤의 초록을 이전 초록과 같은 것으로 세지 말 것.
