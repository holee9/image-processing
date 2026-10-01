# GUI-C-186 — GUI-C-185 미검증 §5-4(모델 경로 기준)·§5-5(재시작 시 설정 값) 확인

레인: gui · 이슈: `#225` 행 10, `#130` · 조사 카드. 인용은 이 트리(`dev/gui` = `6a080ad5` 위, `modules/ai` 는 main `b659829b` 의 것) 기준이다. 푸시 없음. **결함 두 개를 확인해 별도 커밋(`8d905621`)으로 고쳤다.** 이 보고서는 그 위의 커밋이다.

## 결론

1. **§5-4 — 기본값 `data/models` 는 "그때 그 프로세스의 작업 디렉터리" 기준으로 풀리며, GUI 와 모듈은 같은 기준을 쓴다. 단 두 가지가 어긋났다**: ① 모델 디렉터리를 바꿔도 모듈은 옛 디렉터리를 계속 쓴다(**결함, 수정함**), ② 상대 경로는 호출 시점에 따라 다른 곳일 수 있고 문구는 상대 경로를 그대로 찍었다(**수정함**). 작업 디렉터리가 실제로 어디인지(바로 가기·`dotnet run`)는 **확인하지 못했다**.
2. **§5-5 — `Restart AI` 는 입력칸에 입력된 값(적용 전 값)을 읽는다.** 렌더가 쓰는 값은 렌더 시작 시점의 스냅샷이다. 둘이 다를 수 있고, 그때 사용자가 보는 결과는 §3 에 적었다.

## 1. §5-4 — 모델 디렉터리가 풀리는 기준

| 사실 | 근거 |
|---|---|
| 기본값은 상대 문자열 `data/models` 이고 `Path.GetFullPath` 없이 그대로 쓰였다(수정 전) | `AppSettings.cs:68`(`_aiModelDirectory = "data/models"`), 수정 전 `AiBoneSuppressionStage.cs:88`(`6a080ad5` 기준, `Path.Combine(modelDirectory, …)`) |
| 앱의 다른 디렉터리 설정도 같은 방식: 상대 값을 그대로 `Path.Combine` + `File.Exists` 에 넣는다 | `GuiPreprocessRunner.cs:46-50` (보정 디렉터리 `data/calibration/…`) — 즉 이 앱의 규약은 "상대 경로는 작업 디렉터리 기준" |
| 모듈은 `<modelDir>/bone_suppress.onnx` 를 **문자열 이어붙이기**로 만들고 그대로 연다(절대화 없음) | `ai.cpp:887-889`, 워커 쪽 `ai_worker_main.cpp:451-453` |
| 워커는 **호스트의 작업 디렉터리를 상속**한다: `CreateProcessA` 의 현재 디렉터리 인자가 `nullptr` | `ai_worker_supervisor.cpp:222-223` |
| 모델 디렉터리는 INIT 메시지로 **쓴 그대로** 워커에 간다 | `ai_worker_supervisor.cpp:269`, 워커가 받는 곳 `ai_worker_main.cpp:336-337` |
| 워커는 **필요할 때** 시작된다(첫 호출, 또는 죽은 뒤 다음 호출) | `ai_worker_supervisor.cpp:287-300`(`EnsureRunningLocked` → `StartLocked`) |
| E2E 런처는 앱을 **exe 가 있는 폴더**를 작업 디렉터리로 띄운다 | `ApplicationFixture.cs:134-137` |
| CI 는 모델 디렉터리를 만들거나 스테이징하지 않는다: `ci.yml` 에 `data/models` 언급 0건 | `grep -c 'data/models' .github/workflows/ci.yml` → 0 (Native E2E 스텝 `ci.yml:844-851`) |

**읽기.** 같은 프로세스 안에서 GUI 의 `File.Exists`, 모듈의 in-process 경로는 호출 시점의 작업 디렉터리를 쓰고, 워커는 **자기가 시작된 시점**의 것을 쓴다. 그 사이에 작업 디렉터리가 바뀌면 GUI 의 사전 확인과 워커가 서로 다른 곳을 보게 된다. 어긋나는 모양은 둘이다:
- 사전 확인은 "없다"인데 워커의 옛 디렉터리에는 모델이 있다 → **불필요한 "not attempted"**(모듈을 부르지 않으므로 성공할 수 있던 호출이 막힘).
- 사전 확인은 "있다"인데 워커의 디렉터리에는 없다 → 모듈이 `code -9` 를 돌려주고 **실패 횟수 1을 소모**한다(사전 확인이 막으려던 바로 그 경우).

**작업 디렉터리가 바뀌는 경로**: `Load Raw Image` 는 `Microsoft.Win32.OpenFileDialog` 를 `Title/Filter/CheckFileExists` 만 주고 연다(`MainWindowViewModel.cs:2062-2067`, `RestoreDirectory` 미설정). 그 대화상자의 기본값은 `RestoreDirectory = false` 이다. 대화상자가 확인된 뒤 프로세스 작업 디렉터리를 선택한 파일의 폴더로 **바꾸는지는 문서상의 기대일 뿐, 이 트리에서 관측하지 않았다**(대화상자는 사용자 입력이 있어야 열린다).

**바로 가기·`dotnet run` 의 작업 디렉터리**: 코드와 워크플로로는 알 수 없다(바로 가기의 "시작 위치", `dotnet run` 의 동작은 이 저장소 밖의 사실). **미검증.**

### 확인된 결함과 수정 (`8d905621`)

**D1 — 디렉터리를 바꿔도 모듈은 옛 디렉터리를 쓴다.** `xpe_ai_init` 는 이미 초기화된 뒤에는 **인자를 무시하고 OK** 를 돌려준다(`ai.cpp:501-504`: "ignoring"). GUI 는 렌더마다 설정의 경로로 `Init` 을 부르므로(`RealXpeBackend.cs:344` → `GuiAiRunner.Run`), 입력칸을 바꿔도 init 은 성공처럼 보이고 아무것도 바뀌지 않았다. 그런데 GUI 의 사전 확인은 **새** 디렉터리를 봤다. 결과: 새 디렉터리에 모델이 있고 옛 곳에 없으면 사전 확인은 통과하고 모듈은 `-9`; 반대면 사전 확인이 막는다. (헤더 `ai_api.h:296` 가 말하는 "다른 디렉터리로 init 하면 다시 만든다"는 이 코드에서는 shutdown 을 거친 뒤에만 일어난다 — 같은 파일 `ai.cpp:891` 의 "reload when init pointed somewhere else" 도 `modelDirPath` 가 init 에서만 바뀌므로 shutdown 없이는 닿지 않는다.)
수정: `GuiAiSession` 이 시작한 디렉터리를 기억하고, 다른 디렉터리를 요청받으면 **같은 `WithLock` 안에서 shutdown 뒤 init**(= 새 세션, 실패 횟수 초기화). 같은 곳을 다르게 쓴 것(끝 구분자·대소문자·`..`)은 변경이 아니다.

**D2 — 상대 경로의 모호함.** 위 읽기대로 상대 경로는 시점마다 다른 곳일 수 있고, `no model at <경로>` 문구도 입력한 상대 문자열을 그대로 찍어 "어디를 봤는지"를 알려 주지 못했다. 수정: 모듈에는 **절대 경로**(`NormalizeDirectory`)를 주고, 문구도 절대 경로로 찍는다. 이로써 세션은 시작 시점에 정한 절대 경로를 끝까지 쓰므로, 이후 작업 디렉터리가 바뀌어도 GUI 의 사전 확인과 워커가 같은 곳을 본다.

시험·반증은 §4.

## 2. §5-5 — `Restart AI` 가 읽는 모델 디렉터리

- `RestartAiSession` 은 `var directory = Settings.AiModelDirectory;` 를 읽는다(`MainWindowViewModel.cs:577`). `Settings` 는 **화면에 묶인 살아 있는 설정**이다: 입력칸은 `Text="{Binding Settings.AiModelDirectory, UpdateSourceTrigger=PropertyChanged}"` 로, 글자를 칠 때마다 값이 바뀐다(`AnalysisPanel.xaml:244-245`).
- 렌더는 시작 시점에 **스냅샷**을 뜬다: `var inputs = Settings.Snapshot();` (`MainWindowViewModel.cs:2134`) 그리고 AI 단계는 그 스냅샷의 값을 쓴다(`RealXpeBackend.cs:344`).
- 따라서 **Restart 는 입력칸에 친 값(적용 전)**, 마지막 렌더는 렌더 시작 때의 값이다. 다를 수 있다.

**사용자가 보는 결과** (수정 전 → 후):
| 순서 | 수정 전 | 수정 후 |
|---|---|---|
| 새 경로 B 를 치고 `Restart AI` | init(B) — 모듈이 B 를 씀. 마지막 렌더는 A 로 한 것이라 화면의 AI 결과는 A 기준(이미 "미반영" 표시: `ChainInputsDiffer` 에 `AiModelDirectory` 포함) | 같음 |
| B 를 치고 **렌더만**(Restart 없이) | init(B) 이 **무시**돼 모듈은 A 를 계속 씀(D1) | init(B) 이 새 세션을 시작해 모듈이 B 를 씀 |
| 없는 경로를 치고 `Restart AI` | init 은 스텁/ONNX 모두 경로를 열지 않아 성공 → 표시가 사라짐 → 다음 렌더에서 "no model at <절대 경로>" | 같음, 경로가 절대로 찍힘 |

즉 §5-5 의 우려(적용 전 값으로 init 되는가)는 **그렇다**. 이것 자체는 결함으로 보지 않았다: Restart 는 "지금 화면에 적힌 값으로 새 세션"이라는 자연스러운 뜻이고, 다음 렌더가 같은 값을 쓰게 되며(스냅샷이 그 값을 담으므로) D1 수정 뒤에는 둘이 갈라진 채 남지 않는다. **다른 판단(적용된 값만 쓰기)을 원하면** `RestartAiSession` 이 `_renderedInputs` 를 읽도록 바꾸는 한 줄이다 — 리더 결정.

## 3. 수정이 바꾸는 동작 (사용자 눈에)

- 모델 디렉터리를 바꾸면 **AI 실패 횟수가 0 으로 돌아간다**(새 세션). "3회 실패로 중단" 표시가 떠 있는 상태에서 디렉터리를 바꾸고 렌더하면 새 세션이 시작돼 표시가 사라질 수 있다 — 의도한 결과지만 **관측하지는 않았다**(네이티브 미실행).

## 4. 시험·반증

- `AiBoneSuppressionStageTests`: 새 디렉터리/같은 디렉터리(끝 구분자·대소문자·`..`)/상대 vs 절대 판정, 기본값 정규화, 문구가 절대 경로, `GuiAiSession.Init` 이 이 규칙을 쓰고 shutdown 을 부르며 모듈에 절대 경로를 주는지(소스 시험, 이 트리 텍스트만 본다). 로컬 `Functional` **294 통과 · 건너뜀 1(기존) · 실패 0**, 이 클래스 47건.
- **반증 6팔**(실제 소스, 빌드 성공, 소스 바이트 동일 복구, 복구 후 47/47): "바뀌어도 새 세션이 아님" 3건 빨강 · "항상 새 세션" 4건 · 표기 그대로 비교 2건 · 문구가 상대 경로로 돌아감 1건 · init 이 모듈에 친 문자열을 줌 1건 · init 이 옛 세션을 안 닫음 1건 (`falsification_arms.txt`).

## 5. 미검증 · 한계

1. **네이티브 shutdown→init 은 로컬에서 실행되지 않았다.** D1 은 **소스 읽기로 확인**했다(`ai.cpp:501-504`, 같은 파일 `xpe_ai_shutdown` 이 세션·워커를 해제함). 모듈을 로컬에서 빌드·실행해 "두 번째 init 이 무시됨"을 관측하지는 않았다(네이티브 빌드 실행은 CI 몫). 수정은 컴파일과 소스 시험으로만 검증했다.
2. `OpenFileDialog` 가 작업 디렉터리를 바꾸는지, 바로 가기·`dotnet run` 의 작업 디렉터리가 어디인지는 **관측하지 못했다**(§1).
3. **기본값 `data/models` 가 배포 때 어디를 가리켜야 하는지**는 이 조사로 정해지지 않는다. 절대 경로화로 "그 순간의 작업 디렉터리 기준"이 고정될 뿐, 그 기준이 의도한 위치인지는 모른다. 설치 위치 기준(앱 폴더 옆)이 맞다면 별도 결정이다.
4. 수정은 **같은 프로세스의 같은 `GuiAiSession`** 이 시작한 세션만 안다. 다른 곳에서 `xpe_ai_init` 을 부르는 코드가 생기면 이 규칙을 비껴간다(지금은 소스 시험이 호출 위치를 한 곳으로 고정한다 — C-185 의 시험).
5. §5-5 의 "Restart 가 적용 전 값을 읽는다"는 코드로 확정했지만 **화면에서 그 순서로 눌러 본 것은 아니다**.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
