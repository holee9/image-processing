# GUI-C-192f — Codex #66 보류: 표시된 Disabled 가 세션 교체 뒤 남음, 게이트 밖 스냅샷, CI 초안

## 1. 높음 — Reset 없는 세션 교체 뒤 이미 표시된 Disabled 가 남음

**확인.** 192e 의 세션 대수(epoch)는 진행 중인 읽기의 완료 시점에만 비교됐다. 이미 화면에 있는 `Disabled` 는 대수가 움직여도 철회되지 않았고, `CheckFreshness` 가 `Disabled` 를 재읽기에서 빼 두었으므로 새 읽기가 예외로 끝나면 옛 끔 배너가 영구히 남았다.

**고침 — refresher 에서 한다(뷰모델 통지가 아니라).** 이유: 세션을 바꾸는 코드는 프레임 안(`GuiAiSession.InitCore`)이라 뷰모델이 모르고, 통지 경로를 새로 만들면 다음 경계를 또 빠뜨릴 수 있다. 표시된 답에 **표시 당시의 대수**(`_shownEpoch`)를 붙여 두고, 타이머 틱·`Request`·완료마다 현재 대수와 비교한다(`WithdrawIfTheSessionMoved`). 달라졌으면 즉시 `Unknown` 으로 철회하고(`Disabled`·`Active` 모두) 새 세션 기준으로 다시 읽는다. 새 읽기가 실패하거나 멈추면 192e 의 규칙으로 15초에 `NeverConfirmed` 알림이 되고, 옛 답은 돌아오지 않는다. 철회 횟수는 진단 줄 `sessionWithdrawals` 로 로그에 찍힌다.

**시험(표시된 상태 × 새 읽기, 경계 그대로).**
- 단위 이론 `ADisabledOrActiveAlreadyOnScreen_IsWithdrawn_WhenTheSessionIsReplacedWithNoReset` 6케이스: {Disabled, Active} × {새 읽기가 답함 / 실패 / 멈춤}. 철회는 새 읽기의 결과가 나오기 **전에** 일어나고, 실패·멈춤이면 17초 뒤 `NeverConfirmed` 알림이며 `Disabled` 는 한 번도 다시 안 뜬다.
- `ARequest_WithdrawsAnAnswerOfAReplacedSession_BeforeItReads`: 타이머가 아니라 `Request` 경로.
- **실제 뷰모델 SelfCheck 시나리오 9**: 끔 마크가 화면에 있음 → 에포크만 올리고(Reset 안 부름) 이후 읽기를 실패시킴 → 실제 `DispatcherTimer` 의 다음 틱에 마크가 사라지고(`worker=Unknown`) 1.5초 뒤에도 돌아오지 않음. 2.5초 걸리는 실시간 시험이다.

## 2. 중간 — 게이트 밖 읽기와 대수의 원자성

**고침.** `AiSessionTracker` 가 상태 필드 3개와 에포크를 따로 쓰던 것을 **불변 값 하나**(`SessionState(Started, Directory, Failure, Epoch)`)로 바꿨다. 쓰는 쪽(게이트 안, 한 번에 하나)이 다음 값을 만들어 참조 하나를 교체하고(`Publish`), 읽는 쪽(`OwnStatus`, `Epoch`, `Snapshot`)은 값 하나를 한 번만 읽는다. 읽는 쪽은 옛 값 전체나 새 값 전체만 본다.

**시험(장벽으로 결정적).** `BeforePublish` 시험용 훅(앱에서는 null)이 "다음 값을 만들고 교체하기 직전"에 읽기를 끼워 넣는다: `TheTrackerSnapshot_IsOldWhole_OrNewWhole_NeverAMixture` 는 `Stopped`·`InitSucceeded` 각각에서 그 순간의 스냅샷이 옛 상태+옛 대수 전체임을 단언한다. 추가로 실제 동시성 시험(`…KeepsItsInvariant_UnderConcurrentReads`): 쓰기 5만 회 × 읽는 스레드 3개에서 "시작됨 ⇔ 대수 홀수" 불변량 위반 0건. 이 마지막 시험은 비결정적이므로 증거는 위 결정적 시험이다.

## 3. 중간 — CI 초안

- **적용 가능한 diff**: `gui_shipped_build.patch`(실제 `ci.yml` 를 기준으로 `gui-shell-runners` 뒤·`gui-automation` 앞에 새 잡 `gui-shipped-build` 를 넣고, `tools/check_shipped_build.ps1` 를 새 파일로 만든다). `git apply --check` 종료 코드 0, `--stat` 은 두 파일 94줄 추가(`git_apply_check.txt`). 대조군: 옛 초안(`GUI-C-192e/draft_ci_job.diff`)은 같은 명령에서 128. 적용한 `ci.yml` 이 YAML 로 읽히고 잡 키·단계가 맞음도 확인했다(PyYAML).
- **스크립트**(`draft_check_shipped_build.ps1`): `Release + -p:XpeTestFaults=true` 가 비영 종료**이고** 출력에 `XPE0001` 이 있어야 통과한다(다른 이유로 실패하면 "게이트 입증 안 됨"). 반증: 오류 코드를 `XPE0002` 로 바꾸면 스크립트가 "failed, but NOT with XPE0001" 로 실패(아래 반증 4). 이 트리에서 실행해 종료 코드 0(대조군 12개 문자열 발견 → Release 없음 → XPE0001 거부 → 출하 exe 가 4개 스위치에 종료 코드 2).
- **publish·설치 패키지 경로 탐색**(`publish_search.txt`): 범위 = 추적 파일 4676개의 이름, 그리고 `.github`, `tools`, `gui`, `clients`, 루트 `CMakeLists.txt`·`Makefile`, `docs/project` 의 내용을 `dotnet publish`, `PublishSingleFile`, `.pubxml`, `publishprofile`, NSIS/Inno/WiX/MSIX/ClickOnce/Squirrel/CPack/chocolatey/nupkg/installer 용어로 검색. **결과: GUI 앱의 publish·패키징 경로는 없다.** 걸린 것은 VS 설치 경로(`vswhere.exe` 의 `Visual Studio\\Installer`), 낱말 조각 일치(`nsis` 가 "co**nsis**tent" 안에서 걸림: 설치기가 아님), 네이티브 모듈의 CMake `install(TARGETS …)`(GUI 앱 아님), 문서의 "installer / IT" 표(저장소 밖 설치기를 가리킴). 이 저장소 밖에서 만들어지는 설치 패키지는 **볼 수 없다.** 다른 브랜치·외부 저장소는 검색하지 않았다.
- **`--no-build` 게시의 범위**: 가드(`XpeTestFaultsRefusedInRelease`)는 `BeforeTargets="CoreCompile"` 이라 **컴파일이 일어나는 빌드에서만** 돈다. `dotnet publish --no-build` 는 컴파일을 건너뛰므로 가드가 다시 실행되지 않는다. 그 경우 게시물이 어떤 빌드에서 왔는지는 가드가 보증하지 않으니, 최종 산출물 폴더에 대한 스캔이 유일한 방어다. 스크립트에 `-ArtifactDir` 매개변수(게시·패키지 산출물 폴더를 같은 스캔에 넣음)를 넣었다. 지금은 해당 경로가 없어 CI 잡은 이 인자를 쓰지 않는다 — 경로가 생기면 그 단계에서 인자를 줘야 한다.

## 4. 반증 (`falsification_arms.txt`, 규칙마다 제거 → 가드 빨강, 원본 바이트 동일 복원, 복원 뒤 Functional 407/0·SelfCheck 0·Release 깨끗이 재빌드)

| 지운 것 | 빨강 |
|---|---|
| 표시된 답의 대수 철회 | 단위 7건 + **SelfCheck 시나리오 9**(마크가 안 사라짐) |
| `Request` 의 대수 확인 | 단위 1건 |
| 대수·상태를 한 값으로 | 단위 2건(결정적 + 동시성) |
| XPE0001 오류 코드 | 소스 시험 + **스크립트가 "NOT with XPE0001" 로 실패** |

## 5. 실행 결과

Functional 407 통과 / 0 실패 / 1 건너뜀, SelfCheck 종료 0(시나리오 9 포함), E2E M01~M03 통과(진단 줄 `sessionWithdrawals=0 maxUiGapMs=1029` 등), Release dll·pdb·xml 에 시험용 이름 없음(Debug 대조군 발견).

## 6. 남은 공백 (Gaps)

- 이전 보고서의 공백이 그대로다: 첫 AI 추론의 지연 로딩이 15초를 넘으면 정상인데 알림이 뜰 수 있고, 15초·5초·재읽기의 Native 영향은 실측하지 못했다(#98). 다음 Native C-09 의 `noticesNeverConfirmed`·`maxReadMs`·`maxUiGapMs`·(신규) `sessionWithdrawals` 로 판정한다. 프레임의 폴더 변경이 정상 사용에서 대수를 올린다면 `sessionWithdrawals` 가 0 이 아닌 것은 의도된 동작이고, 시험 중 불필요하게 오르면 그 경계가 너무 잦다는 신호다.
- 대수 철회는 타이머 틱(1초)·요청·완료 시점에 일어난다. 대수가 움직인 순간부터 철회까지 UI 가 스케줄될 때까지의 지연이 있다(192e 의 UI 정체 설명과 같다).
- 게이트 밖 자체 답의 동작 시험은 네이티브가 필요해 소스를 읽는 시험으로만 지킨다.
- `gui-shipped-build` 잡은 CI 에서 돌려 보지 않았다(로컬 스크립트 실행·`git apply --check` 만).
- 이 저장소 밖에서 만들어지는 출하 패키지는 검사할 수 없다.

## 7. 잔여 위험 (Residual-risk)

- `BeforePublish` 는 앱 코드에 있는 시험용 훅이다(null 검사 한 줄). 쓰는 쪽이 둘 이상이 되면 읽고-바꾸고-쓰기가 경합할 수 있다: 지금은 모든 쓰기가 같은 게이트 안에서 일어난다는 전제다(`_state with {…}` 읽기-수정이 그 전제에 의존).
- `Failure` 만 바꾸는 `InitFailed` 는 대수를 올리지 않는다(직전 `Stopped` 가 올렸다).
