# GUI-C-186c — Codex #28 (A) 보류 3건 수정 (#225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 카드: 메인 `.moai/lanes/gui/inbox/GUI-C-186c.md`, 감사 원문: 메인 `.moai/state/codex-archive/28.md` §(A). 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 이 위에 GUI-C-186b(`eec5204d`)와 GUI-C-187(`f6c32817`)이 있다.

## 1. 주장

1. **A1 (차단) 고침.** `ProcessingChainScenarios.cs` 446행의 `ceiling=` 뒤는 U+0001 바이트가 아니라 역참조 `\1`(두 글자: 백슬래시, 1)이다. 같은 결함이 다시 들어오지 못하게 시험 3건을 두었다.
2. **A2 (보통) 고침.** 상태 조회가 포기(`null`)하면 짧은 지연 뒤 다시 읽는다(최대 6회, 500 ms 간격). 더 새로운 갱신이 오면 낡은 재조회는 취소된다. 재조회·상한·취소는 가짜 시계로 시험했다.
3. **A3 (낮음) 고침.** init 가 일반 예외를 던지면 사유 문자열을 **한 번** 만들고(`InitFailureReason`), 추적기에 기록되는 것과 위로 올라가는 예외(`AiInitException`)가 그 같은 문자열을 쓴다. 체인의 단계 사유는 그 문자열을 그대로 품는다.
4. **네이티브 C-09 는 로컬에서 실행하지 못했다**(아래 §2-4, §4). CI 첫 관측이 판정 근거다.

## 2. 증거

### 2-1. A1 — 제어 문자

고치기 전(원문): `Assert.Matches(@"worker=Disabled; failures=(\d+); ceiling=<0x01>$", before);` — 바이트 검사에서 446행 `0x01` 1개(`ctrlscan_before_fix.txt`). 고친 뒤: `ceiling=` 다음이 바이트 `5c 31`(`\1`), U+0001 은 0개.

**왜 생겼나(관측과 추정을 가른다).** 이 세션에서 실험했다: 파이썬 소스로 보낸 `b'\\1'`(백슬래시 둘 + 1)이 `list(...)` 로 **`[1]`**(바이트 하나)로 도착했다. 백슬래시 하나를 쓴 `b'\1'` 도 `[1]`. 즉 이 도구 호출 경로에서는 `\\1` 이 파이썬에 닿기 전에 줄어들거나 파이썬이 `\1` 을 8진 이스케이프로 읽는다 — **어느 쪽인지는 가르지 못했다**(둘 다 같은 결과). 그래서 446행을 만든 편집이 이 경로였다는 것은 **추정**이다(그 편집의 기록은 남아 있지 않다). 고칠 때도 같은 일이 한 번 더 일어났다(내 첫 고침 시도가 `\\1` 을 써서 아무것도 바꾸지 않았다). 최종 고침은 이스케이프를 한 번도 적지 않고 `bytes([0x5c, 0x31])` 로 썼다.

**저장소 전체 바이트 검사**(`ctrlscan_before_fix.txt` → `ctrlscan_after_fix.txt`): 추적 중인 텍스트 파일 3480개(이진 38개는 확장자·NUL 로 제외)에서 TAB·LF·CR 이 아닌 제어 바이트를 센다.

| | 제어 바이트가 있는 파일 |
|---|---|
| 고치기 전 | 2: 이 파일 446행(`0x01`), `.moai/reports/lane-pre/QA-A-95/runs/iaea_table6_1.txt` 3행(`0x0c` 폼 피드) |
| 고친 뒤 | 1: lane-pre 의 그 파일 |

**대조군**: 고치기 전 상태에서 이 검사가 알려진 결함(446행)을 **잡았다**(위 표의 첫 줄). 검사가 파일을 읽는지의 대조는 3480개를 읽었다는 개수다.

lane-pre 의 `iaea_table6_1.txt` 는 **내 소유가 아니고**(`.moai/`) 손대지 않았다. 폼 피드가 표 내보내기에서 온 정상 자료인지 결함인지는 **확인하지 않았다**(미검증) — lead/pre 가 판단할 몫이다.

**재발 방지 시험 3건**(`AiStatusRefresherTests.cs`): C-09 정규식이 실패 수 = 상한일 때만 맞음(`TheC09Pattern_…`); E2E 소스가 그 패턴을 두 글자 `\1` 로 품음(`TheE2EAssertion_CarriesThatPattern_…`); `clients`·`gui` 의 `.cs`·`.xaml` 에 제어 바이트 없음 — 100개 넘게 읽었는지 단언하는 대조 포함(`NoSourceFileOfTheTwoClients_CarriesAControlByte`).

### 2-2. A2 — 조회 실패 뒤 재조회

수정 전 `RefreshAiWorkerStatus` 는 `null` 이면 그냥 돌아와, 이후 이벤트가 없으면 표시가 영영 낡았다. 지금은 순수 클래스 `AiStatusRefresher`(`AiBoneSuppressionStage.cs`, 이미 테스트에 링크됨)가 읽고, 포기하면 `schedule(500 ms, 다시 읽기)` 를 예약한다. 뷰모델은 `DispatcherTimer` 로 UI 스레드에서 지연을 실행한다(`Thread.Sleep`·`Task.Delay` 없음).

- **시험(가짜 읽기 + 가짜 시계)**: 첫 읽기 포기 → 예약 1건 → 실행하면 상태가 표시됨; 읽기가 되면 예약 없음; 영영 안 되면 `1 + MaxRetries` 번만 읽고 멈춤(표시는 그대로); 더 새로운 `Refresh` 가 낡은 재조회를 취소해 **마지막 사건의 상태가 이긴다**.
- **소스 시험**: 뷰모델이 이 클래스로 갱신하고 `DispatcherTimer` 를 쓰며 `Thread.Sleep`/`Task.Delay` 를 쓰지 않음.
- **측정 근거 없음(카드 지시대로 적는다)**: 250 ms(한정 대기), 500 ms(재시도 간격), 6회(상한) 모두 **고른 값**이다. 어떤 타이밍 측정에서도 유도하지 않았다.

### 2-3. A3 — init 예외의 한 사유 문자열

`InitFailureReason(ex)` = `xpe_ai_init threw <형식>: <메시지>.`(메시지가 마침표로 끝나면 중복하지 않음). `InitCore` 의 일반 `catch` 는 이것을 변수 하나에 만들어 **추적기에 기록하고** 같은 변수로 `AiInitException`(메시지 = 그 사유)을 던진다. 체인 경로의 `"<단계> threw: <메시지>"` 는 그 예외의 메시지를 그대로 품는다. DLL 없음·진입점 없음은 이전처럼 각자 문구로 `throw;`.

- **시험**: 같은 예외에서 추적기 상세 == 사유, 체인 단계 사유 == `ai_bone_suppress threw: <그 사유>`, 추적기 상세가 체인 사유에 들어 있음(`TheTrackersDetail_AndTheChainsStageReason_…`; 실제 `AiSessionTracker`·`ProcessingChainRunner`). 형식 시험 1건. 네이티브 `InitCore` 의 연결(변수 하나 → 기록 → 던짐 순서)은 소스 시험 1건.
- **동작 변화(알려 둔다)**: 일반 예외가 위로 올라갈 때 형식이 원래 예외에서 `AiInitException` 으로 바뀌고, Restart 실패 문구는 `AI session could not be restarted: xpe_ai_init threw <형식>: <메시지>.` 처럼 사유 전체를 담는다(전에는 메시지만). 이 예외의 원래 형식에 기대어 잡는 곳은 찾지 못했다. 검색 범위: `GuiAiRunner.cs`·`RealXpeBackend.cs`·`ProcessingChainRunner.cs`·`AiBoneSuppressionStage.cs`·`MainWindowViewModel.cs` 의 모든 `catch (` 와 `gui/ImageProcTest` 전체의 형식 지정 catch(`InvalidOperationException`·`AiInitException`·`ArgumentException`·`IOException`). 결과: AI init 경로를 받는 것은 형식 없는 `catch (Exception ex)`(체인 `ProcessingChainRunner.cs:59`, 재시작 `MainWindowViewModel.cs:613`)뿐이고, 형식 지정 `InvalidOperationException` catch 4곳(뷰모델 1530·1592·2880·2959행)은 모두 `FindRepositoryRoot` 만 감싼다(줄을 읽어 확인). 대조: 같은 검색이 `GuiAiRunner.cs` 의 알려진 `catch (Exception ex)` 를 찾는다.

### 2-4. 네이티브 C-09 — 로컬 실행 불가, 막는 것

실행하지 못했다. 막는 것(구체적으로):

1. **필요한 네이티브 산출물이 로컬에 없다.** `build/`·`gui/`·`clients/` 에서 `xpe_ai.dll`·`xpe_ai_worker.exe` 를 찾았으나(`find`, 대조군: 같은 도구가 `ImageProcTest.dll` 은 찾음) **없음**. 있는 것은 CI 산출물 폴더(`build/c134-*`)의 `xpe_common.dll`·`gsvg.dll` 뿐이다. 스테이징 목록(`ci.yml` "Verify staged DLLs" 단계)은 `xpe_preprocess`·`xpe_display` 도 요구한다.
2. **이 레인은 로컬 네이티브 빌드를 하지 않는다**(#98, CI 몫). 만든다 해도 `ci-ai` 프리셋은 ONNX Runtime 경로(`-DONNXRUNTIME_ROOT`, `ci.yml:470`)가 필요하고 로컬에 있는지는 **확인하지 않았다**.
3. 그래서 Mock 백엔드로 돌린 결과는 **C-09 가 올바른 사유로 건너뛰었음**("The AI session exists only on the native backend.")뿐이다. Disabled → HelpText 갱신 → 재시작 후 `worker=Active; failures=0` 의 관측은 **CI 첫 관측**이 한다.

### 2-5. 반증 8팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구, 복구 후 11/11)

| 팔 | 빨강이 된 시험 |
|---|---|
| 읽기가 포기해도 재조회를 예약하지 않음 | 3건(재조회·상한·최신 우선) |
| 낡은 갱신을 취소하지 않음 | 1건 |
| 재시도 상한 없음 | 1건 |
| 사유에서 메시지가 빠짐 | 형식 시험 1건 |
| init 가 원래 예외를 `throw;` 로 올림 | 소스 시험 1건 |
| 추적기가 자기 문구를 기록 | 같은 소스 시험 1건 |
| 뷰모델이 재조회를 예약하지 않음(빈 예약자) | 소스 시험 1건 |
| E2E 정규식의 `\1` 자리에 `0x01` | 2건(두 글자 시험·제어 바이트 시험) |

(`falsification_arms.txt`. 시작과 끝의 소스 해시 `c9d872aad0c9 f51c49c4c36e 3b83a6639bb7 ad99704fba28` 이 같다.)

### 2-6. 로컬 실행 (`local_runs.txt`)

`gui/ImageProcTest.slnx`·`clients/ImageProcTest.slnx` 빌드 0 오류. `Category=Functional` **324 통과 · 건너뜀 1(기존) · 실패 0**(신규 11 포함). E2E(Mock, UI Automation 패턴만): `ProcessingChainScenarios` C-01·C-03·C-05·C-08 통과, C-09 건너뜀(네이티브 전용), A04·A05·A07·A10·A11(세 축 개수)·A12·A13·A15~A17·A02·A09 통과, A03 건너뜀(기존). 빌드 경고 12+1 개는 내가 바꾼 줄에서 나온 것이 아니다(뷰모델 110~114·371~377행의 기존 문서 주석, E2E 322행의 기존 xUnit2031).

## 3. 기준 귀속 (측정 대상)

모든 숫자는 이 트리(`dev/gui` 의 `f6c32817` 위 작업 트리)에서 이 실행으로 얻은 것이다. 바이트 검사는 `git ls-files` 의 추적 파일만 본다.

## 4. 미검증

1. **네이티브 C-09 는 실행되지 않았다**(§2-4). Disabled 표시·HelpText·재시작 후 `Active` 는 CI 첫 관측이 근거다.
2. **`DispatcherTimer` 로 실제 재조회가 화면까지 가는 것은 실행하지 못했다.** 순수 클래스는 가짜 시계로, 뷰모델 연결은 소스 시험으로만 확인했다. 앱을 띄워 첫 조회가 실패하는 장면을 만들지 않았다(Native 잡에서 프레임은 하나씩 돌아 E2E 로는 만들 수 없다).
3. **446행을 만든 편집이 정말 같은 이스케이프 경로였는지**는 추정이다(§2-1).
4. **"같은 경로로 쓴 다른 파일"을 열거하지 못했다.** 검사는 추적 파일 전부를 보지만 추적되지 않은 파일(작업 폴더의 임시 도구 등)은 보지 않았다.
5. lane-pre 의 `iaea_table6_1.txt` 폼 피드가 결함인지 정상 자료인지.
6. 재조회 상한·간격(6회, 500 ms)과 한정 대기 250 ms 는 측정 근거가 없다.

## 5. 잔여 위험

- **재조회가 포기한 뒤에도 낡을 수 있다.** 모듈의 시간 예산이 기본 5 초인데 재조회 창은 읽기 대기까지 합쳐 대략 3 초에서 4.75 초다. 조용한 워커 앞에서 프레임이 그보다 길면 표시는 다음 렌더 끝까지 낡은 채로 있다.
- **재조회의 읽기는 UI 스레드에서 최대 250 ms 막는다**(지연 대기는 막지 않는다). 최악에는 한 번의 갱신이 UI 스레드를 합쳐서 1.75 초 가까이 쓴다.
- `AiInitException` 으로 감싸면서 Restart 실패 문구가 길어졌다(§2-3). 문구를 읽는 시험은 없었다.

## 증거 파일

`ctrlscan_before_fix.txt` · `ctrlscan_after_fix.txt` · `falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
