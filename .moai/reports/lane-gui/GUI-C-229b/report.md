# GUI-C-229b — 입력은 원래대로, 관측만 앞뒤에 (Codex #131)

앱 코드는 고치지 않았다. 바뀐 것은 E2E 프로젝트의 `RealInputScenarios.cs`, `Fixtures/KeyLossDiagnosis.cs`, `KeyLossDiagnosisTests.cs` 이다.

## 무엇을 바꿨나

1. **입력을 원래대로** (Codex 2): 글자별 입력과 키 사이 전경 확인을 뺐다. `GlobalInput.SelectAll()` 다음 `GlobalInput.Type("4321")` 한 번 호출이다. 키 사이에는 아무것도 읽지 않는다.
2. **전경 확인은 입력 직전 한 번, 아니면 실패로 끝** (Codex 1): 입력 전 값을 읽은 뒤 `AppIsForeground` 를 한 번 확인한다. 앱이 전경이 아니면 키를 하나도 보내지 않고 `Assert.Fail`(건너뜀 아님)로 끝낸다. 메시지에 전경 창 설명과 입력 전 관측이 들어간다. 이 확인은 `SelectAll` 앞이므로 Ctrl+A 와 숫자 넷이 모두 보호된다.
3. **관측은 입력 앞뒤에만**: 입력 직전, 입력 직후, 600 ms 뒤(단언이 쓰는 값), 실패 때만 1.5 s 뒤. 각 시점에 Center·Width 값, 키보드 포커스(자동화 id·pid), 전경 pid 를 읽는다(`Observation`).
4. **단언은 그대로 엄격하다**: 마지막 키 600 ms 뒤 Center 가 `4321` 이어야 한다. 입력 직후 관측에 걸린 시간만큼 `Thread.Sleep` 을 줄여 600 ms 뒤 읽기는 마지막 키 600 ms 뒤에 이뤄지게 했다.
5. **분류를 다시 짰다** (Codex 1·3):
   - 키를 안 보냈으면 항상 `NotSentForegroundWasNotTheApp`(상자가 우연히 4321 이어도). 실제 시나리오에서는 그 경우 입력 전에 이미 실패로 끝나므로 이 분기는 방어선이다.
   - Width 는 **입력 전** 값과 비교한다. 늘어난 꼬리가 사라진 글자와 정확히 같을 때만 `KeysWentToAnotherBox`. 입력 전부터 Width 가 그 꼬리로 끝나면(예: `21`) Width 로 말할 수 있는 설명은 주장하지 않는다.
   - "덮임"과 "버려짐"은 이 관측으로 못 가르므로 한 범주 `KeysNotInBoxFocusStayed` 로 합치고, 포커스가 상자를 떠났으면 `FocusLeftTheBoxStayedInApp`(키가 어디 갔는지는 관측 안 됨).
   - 관측이 두 개 이상의 설명에 맞거나 하나도 안 맞으면 `Unclassified`.
6. **사실과 추정을 나눠 보인다**: 실패 메시지는 `FACTS (observed): …` 다음 `READING (an inference from the facts above, not an observation): <이름> - <설명>` 순서다.

## 판단: SelectAll·Tab 에도 같은 확인이 필요한가 (Codex 5)

- **SelectAll**: 넣었다. 입력 직전 확인 한 번이 Ctrl+A 와 `Type` 앞에 있다. Ctrl+A 와 `Type` 사이에는 아무것도 없다(둘 사이 간격은 그대로).
- **Tab**: 이미 있다. 기존 코드가 Tab 직전에 "키보드 포커스가 이 앱의 center 상자에 있음"을 UIA 로 단언하고 아니면 보내지 않고 실패한다(`beforeTab` 단언). 같은 형태의 확인이므로 더 넣지 않았다.
- 타이밍 영향: 확인과 관측이 Ctrl+A 앞에 한 번 들어간 것이 유일한 간격 변화다. 키 사이 간격은 달라지지 않았다. 다만 `Focus()` 와 Ctrl+A 사이가 이전보다 UIA 읽기 시간만큼 길어진다(읽기 시간은 측정하지 않았다). 입력 직후 관측도 UIA 읽기라 앱 UI 스레드가 그 시각에 일을 하게 만든다. 간헐 실패의 조건이 이 두 읽기로 달라질 수 있다는 위험을 아래에 적었다.

## 확인 (입력 없이 가능한 것)

| 항목 | 결과 |
|------|------|
| 분류·거절 시험 14건(Codex 재현 포함) | 통과 14/14 |
| Codex 재현 (1): `Sent=false`(`keysSent=false`) + 상자가 `4321` → 실패 경로 | `KeysNotSent_IsNotSent_EvenWhenTheBoxHoldsTheText` |
| Codex 재현 (3): Width 가 입력 전부터 `21` → `Unclassified` | `AWidthThatAlreadyEndedInTheTailBeforeTheKeys_SaysNothing_…`, 꼬리로 끝나는 `100021` 변형도 |
| 입력 직전 전경이 앱이 아니면 키를 안 보내고 실패 | `WhenTheAppIsNotInFront_TheRefusalIsAMessage_AndNothingIsSent` (`RefuseIfNotInFront`). 시나리오에서 그 결과로 `Assert.Fail` 하는 줄은 컴파일만 됐다 |
| R02d: 실제 창에서 키 없이 메시지 생성 | 통과, 메시지 전문은 `r02d_message_from_real_window.txt` |

반증 (넣은 채로 커밋하지 않음, 원본 복구 확인): (a) 키 미전송 분기를 지우면 (1) 재현 시험 빨강, (b) Width 입력 전 값을 무시하면 (3) 재현 둘 빨강, (c) 첫 일치를 반환하면 둘 이상 설명 시험 빨강, (d) 거절이 거절하지 않게 하면 거절 시험 빨강. 파일 `falsification_*.txt`.

## 새 R02 를 실제 키 입력으로 돌려 보았나 (Codex 6)

**돌리지 않았다.** 조건("Windows 보안" 같은 전경 탈취가 있으면 멈춘다)에 걸렸다. R02d 가 만든 메시지의 사실에 따르면 지금 이 데스크톱의 전경은 **PickerHost 프로세스의 "Windows 보안" 창(pid 47456, hwnd 0xFFF1CEE)** 이고 키보드 포커스도 그 창의 'Popup Window' 이며, 전에 GUI-C-229 에서 본 것과 같은 hwnd 라 오래 떠 있는 창이다. 앱의 전경 설정이 이기더라도 확인과 전송 사이의 틈에 이 창이 전경을 도로 가져가면 키가 그 창으로 간다. 그 창이 무엇인지(자격 증명이나 패스키 요청 등)는 열어 보지 않았고, 닫는 일도 하지 않았다. 사용자나 리더가 그 창을 정리한 뒤 알려 주면 그때 한 번 돌리겠다.

## 관측한 사실과 추정 (보고서 분리)

| 구분 | 내용 |
|------|------|
| 관측한 사실 | 최근 14회 중 R02 1회 실패(`Actual "43"`); 실패한 실행과 직전 통과 실행 사이 앱·E2E·모듈·워크플로 변경 0; 실행 순서 동일; R02·R01 시간은 통과 실행들의 범위 안; 이 데스크톱에 "Windows 보안" 창이 오래 전경에 있음 |
| 추정(근거 없음) | 간헐이라는 것, 원인이 (a)(b)(c) 중 하나라는 것. 지금 코드는 다음 실패에서 사실을 남기도록 했을 뿐 원인을 말하지 않는다 |

## 미검증 / 잔여 위험

- 새 R02 경로(입력 직전 확인, `Observe`, `DescribeLostKeys` 의 실패 경로)는 컴파일만 됐고 실제 키 입력으로는 한 번도 돌지 않았다. 처음 도는 것은 CI 일 가능성이 크다.
- 입력 앞뒤에 UIA 읽기가 들어갔으므로 간헐 실패의 조건이 달라질 수 있다. 읽기가 UI 스레드 일을 만들어 실패를 가리거나, `Focus()`~Ctrl+A 간격이 길어져 조건이 바뀔 수 있다. 그 영향은 측정하지 않았다. 다음 초록이 원인 해소인지 이 변화인지 이 시험만으로는 못 가른다 — 이전 R02 의 초록 14회와 이 변경 뒤의 초록을 같은 것으로 세지 말 것.
- 전경 확인과 첫 키 사이의 틈은 없애지 못했다(마이크로초 단위로 좁음).
- 분류는 관측에서 나온 읽기일 뿐 판정이 아니다. 단언은 읽기와 무관하게 실패한다.
