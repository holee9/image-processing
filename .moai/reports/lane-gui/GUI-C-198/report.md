# GUI-C-198 — "Run AI Bone Suppression" 활성 규칙 (MENU-001 §8)

증거: 이 폴더의 `measurements_before_fix.txt`(고치기 전 실측), `after_fix_e2e.txt`(고친 뒤 실행 결과), `falsification_arms.txt`(반증).

## 1. 실측 — 누르면 무슨 일이 났나 (고치기 전, UI Automation 으로만)

항목은 모든 상태에서 **활성**이었다(`item enabled=True`). 누른 결과:

| 상태 | 누른 뒤 | 조용한 무동작? | 예외? | 그림 픽셀 | "체인에 AI" 설정 |
|---|---|---|---|---|---|
| Mock, 초기화됨, 영상 있음 | 상태줄·체인에 `ai_bone_suppress=RequestedNotApplied … requires the native backend (xpe_ai.dll)` | 아니오(말함) | 없음 | 그대로 | False → **True** |
| Native, xpe_ai.dll 없음(기본 모델 경로) | `… not attempted: no model at …\data\models\bone_suppress.onnx` | 아니오 | 없음 | 그대로 | False → True |
| Native, 모델 파일만 있고 xpe_ai.dll 없음 | `… not started: xpe_ai.dll was not found beside the other native modules` | 아니오 | 없음 | 그대로 | False → True |
| Native, 백엔드 **종료** 뒤 | 같은 `no model at …` 줄(종료된 백엔드인데도 체인이 돌았다) | 아니오 | 없음 | 그대로 | False → True |
| Mock, 백엔드 종료 뒤 | Mock 의 거절 줄 | 아니오 | 없음 | 그대로 | False → True |

관찰:

- 조용한 무동작도 예외도 **없었다**. 거절은 매번 상태줄에 이유가 나왔다(카드 3번의 "막을 것"은 없음).
- 문제는 카드가 말한 그대로다. 호출할 수 없는 곳(Mock, DLL 없음, 종료된 백엔드)에서도 항목이 켜져 있다.
- 부작용: 누르면 `AiBoneSuppressionInChain` 이 **True 로 남는다**(실패해도). 이어지는 평범한 Apply 도 매번 AI 단계를 다시 시도한다(`chain after a plain Apply` 에 같은 거절이 반복). 설계 주석("Run Preprocessing 처럼 단계를 켠다")대로의 동작이라 이 카드에서는 바꾸지 않았다. 하지만 이제 항목이 쓸 수 없는 곳에서는 눌리지 않으므로 이 경로로 설정이 켜지는 일 자체가 줄어든다.

## 2. 변경

| 파일 | 내용 |
|---|---|
| `Services/AiBoneSuppressionAvailability.cs` (새, WPF·네이티브 없음) | 규칙 한 곳. 입력은 사실 네 개: 백엔드에 AI 세션이 있는가(`IAiSessionBackend` — Mock 은 없음), 초기화됨(`RuntimeInfo.State`), 시작·종료 진행 중 아님, `xpe_ai.dll` 이 로더와 같은 탐색(`NativeModuleLibraryLocator`)으로 보임. 이유 문장도 돌려준다 |
| `ViewModels/MainWindowViewModel.cs` | `RunAiBoneSuppressionCommand` 에 `canExecute`(= 규칙). 핸들러도 같은 규칙을 먼저 묻고 거절 이유를 상태줄에 남긴다(메뉴 없이 명령이 실행되는 경우). `AiBoneSuppressionAvailability`·`AiBoneSuppressionMenuToolTip` 속성. `RuntimeInfo` 가 바뀔 때·종료 시작·종료 끝에 `RefreshAiBoneSuppressionAvailability()` 로 메뉴에 다시 물으라고 알림 |
| `MainWindow.xaml` | 항목에 `IsEnabled="{Binding AiBoneSuppressionAvailability.CanRun}"`(Baseline 항목과 같은 방식) + 동적 `ToolTip`(꺼져 있으면 `Disabled now: <이유>` 가 붙음). `Command` 는 그대로 |
| `gui/ImageProcTest.E2E/Program.cs` | 쉘 E2E 의 "항상 켜져 있어야 한다" 단언을 "규칙과 같고, 꺼졌으면 툴팁에 이유가 있다" 로 |
| `BackendLifecycleTests.cs` | 새 읽기 전용 속성을 멤버 표에 등록 |
| `IntegrationTests.csproj` | 새 규칙 파일 링크 |

IsEnabled 바인딩을 같이 둔 이유(실측): 쉘 E2E(창이 떠 있지만 메뉴가 한 번도 안 열려 `IsLoaded=False`)에서 `Command.CanExecute=False` 인데 항목은 `IsEnabled=True` 로 남았다. 로드되지 않은 메뉴 항목은 명령의 CanExecute 를 아직 읽지 않는다. 사용자가 보는 열린 메뉴에서는 둘 다 맞는다(UIA 시험이 그 경로).

## 3. 시험

- 기능(Functional) `AiBoneSuppressionAvailabilityTests` 9건: 규칙 표(활성은 네 사실이 모두 참일 때뿐), 꺼진 상태마다 서로 다른 이유 문장, 명령이 `canExecute` 로 규칙을 쓰고 핸들러가 스스로 거절하고 규칙이 백엔드 자체에서 읽히고(요청 모드가 아님) 상태가 움직일 때 메뉴에 알림, XAML 이 명령·IsEnabled·툴팁 바인딩이고 "아직 구현 안 됨" 목록에 없음.
- E2E(UIA 패턴만, 전역 입력 없음) `AiMenuAvailabilityScenarios`: **E01** 항목 활성 여부 = (런타임 줄이 `mode=Native` + `src=` 이고 고정한 디렉터리에 `xpe_ai.dll` 이 있음) — 앱의 규칙이 아니라 사용자가 보는 줄과 파일에서 독립 유도; 꺼졌으면 툴팁에 `Disabled now:`. **E02** 백엔드 종료 → 항목 비활성, 초기화 → 기대 상태로 복귀.
- `ProcessingChainScenarios` C08(AI 거절 경로): 항목이 꺼진 상태(Mock·DLL 없음)에서는 같은 단계를 "체인에 AI" 설정으로 요청(같은 단계·같은 거절), 켜진 상태면 메뉴로 누른다. C09(여섯 번 반복 실패)는 메뉴 누름이 필수이므로 항목이 꺼져 있으면 이유를 말하고 실패한다.
- 결과(`after_fix_e2e.txt`): Functional **527 통과 / 0 실패 / 5 건너뜀**. E2E Mock(자동화 보고서+체인+AI 메뉴+기준선) **25 통과 / 0 실패 / 10 건너뜀**. Native, xpe_ai.dll 없음: E01·E02·C08 **3 통과**. Native, xpe_ai.dll 있음: E01·E02·C08 **3 통과**(E01 은 "활성"을 기대하고 활성). 쉘 E2E·SelfCheck(16 시나리오) 통과.
- 반증(`falsification_arms.txt`): canExecute 제거 → 기능 시험 빨강(E2E Mock 은 IsEnabled 바인딩이 같은 규칙을 이중으로 나르므로 초록 — 둘 다 제거하면 E2E 도 빨강). DLL 조건 무시 → 기능+E2E(Native DLL 없음) 빨강. 알림 제거 → 기능+E2E(Native DLL 있음, 종료 뒤 항목이 켜진 채) 빨강. 핸들러 가드 제거 → 기능 빨강. 전부 바이트 동일 복원 확인.

## 4. 겹치는 파일 (M-시리즈)

이 카드가 건드린 파일 중 GUI-C-196 M1~M7 이 건드린 것: `MainWindowViewModel.cs`, `ImageProcTest.IntegrationTests.csproj`, `BackendLifecycleTests.cs`(멤버 표). 모두 다른 줄이라 의미 충돌은 없을 것으로 본다(M-시리즈는 Baseline 명령 쪽, 이 카드는 AI 명령 쪽). 196 병합 뒤 리베이스한다.

## 5. 미검증·한계

1. **C09**: 이 기계에 있는 `xpe_ai.dll` 빌드는 `xpe_ai_worker_state` 를 내보내지 않는 옛 빌드이고(기본 빌드는 `xpe_ai_init` 도 없음) C09 가 성립하지 않는다. 항목이 켜진 상태에서 첫 시도까지 누르는 것은 관찰했지만(`not started: xpe_ai.dll does not export …`, 이 DLL 때문이고 이 변경과 무관), 여섯 번 반복·표시 확인은 CI Native 잡 몫이다. CI 에서 xpe_ai.dll 이 고정 디렉터리에 놓이는지는 이 기계에서 확인하지 못했다. 안 놓이면 C09 는 이유를 말하며 실패한다(조용히 건너뛰지 않음).
2. E01 의 독립 기대값은 고정한 `XPE_NATIVE_DIR` 의 `xpe_ai.dll` 만 본다. 앱 폴더에 같은 DLL 이 있으면(이 저장소의 빌드에는 없음) 기대와 어긋난다.
3. 로컬 `xpe_ai.dll` 이 없는 것과 있지만 못 쓰는 것(옛 빌드, 모델 없음)은 구분하지 않는다: 항목은 DLL 이 보이면 켜지고, 그 뒤의 실패는 상태줄이 말한다. 설계 의도(MENU-001 §8 은 "owner DLL 존재·초기화됨")이고 카드와 같다.
4. 파일 존재 확인은 메뉴가 열릴 때가 아니라 `RuntimeInfo`·종료 알림 시점에 다시 읽힌다. 백엔드 상태가 그대로인 채 DLL 파일이 중간에 생기거나 사라지면 다음 알림 전까지 반영되지 않는다.
5. 위 실행은 모두 Debug 빌드. Release/출하 빌드에서는 돌리지 않았다.
6. 고치기 전 실측(`measurements_before_fix.txt`)에서 Native "종료 뒤" 시나리오는 첫 실행(위 표의 값)은 통과했고, 파일로 남긴 두 번째 실행은 실패(6 초, 원인을 읽지 않음)했다. 표의 그 행은 첫 실행의 관찰이다. 이 실측 시나리오는 고친 뒤 asserting 시나리오(E01·E02)로 바뀌어 저장소에 남지 않는다(측정용 소스는 커밋하지 않음).
